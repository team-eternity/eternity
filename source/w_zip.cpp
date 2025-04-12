//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: ZIP archive loading.
// Authors: James Haley
//

#include "z_auto.h"

#include "i_system.h"
#include "m_buffer.h"
#include "m_compare.h"
#include "m_qstr.h"
#include "m_structio.h"
#include "m_swap.h"
#include "w_wad.h"
#include "w_zip.h"

#include "../zlib/zlib.h"

// Internal ZIP file structures

#define ZIP_LOCAL_FILE_SIG  "PK\x3\x4"
#define ZIP_LOCAL_FILE_SIZE 30

struct ZIPLocalFileHeader
{
   uint32_t signature;    // Must be PK\x3\x4
   uint16_t extrVersion;  // Version needed to extract
   uint16_t gpFlags;      // General purpose flags
   uint16_t method;       // Compression method
   uint16_t fileTime;     // DOS file modification time
   uint16_t fileDate;     // DOS file modification date
   uint32_t crc32;        // Checksum
   uint32_t compressed;   // Compressed file size
   uint32_t uncompressed; // Uncompressed file size
   uint16_t nameLength;   // Length of file name following struct
   uint16_t extraLength;  // Length of "extra" field following name

   // Following structure:
   // const char *filename;
   // const char *extra;
};

using ZLFH_t = ZIPLocalFileHeader;

static MStructReader<ZLFH_t> localFileReader([] (MStructReader<ZLFH_t> &obj) {
   obj.addField(&ZLFH_t::signature   );
   obj.addField(&ZLFH_t::extrVersion );
   obj.addField(&ZLFH_t::gpFlags     );
   obj.addField(&ZLFH_t::method      );
   obj.addField(&ZLFH_t::fileTime    );
   obj.addField(&ZLFH_t::fileDate    );
   obj.addField(&ZLFH_t::crc32       );
   obj.addField(&ZLFH_t::compressed  );
   obj.addField(&ZLFH_t::uncompressed);
   obj.addField(&ZLFH_t::nameLength  );
   obj.addField(&ZLFH_t::extraLength );
});

#define ZIP_CENTRAL_DIR_SIG  "PK\x1\x2"
#define ZIP_CENTRAL_DIR_SIZE 46

struct ZIPCentralDirEntry
{
   uint32_t signature;     // Must be "PK\x1\x2"
   uint16_t madeByVersion; // Version "made by"
   uint16_t extrVersion;   // Version needed to extract
   uint16_t gpFlags;       // General purpose flags
   uint16_t method;        // Compression method
   uint16_t fileTime;      // DOS file modification time
   uint16_t fileDate;      // DOS file modification date
   uint32_t crc32;         // Checksum
   uint32_t compressed;    // Compressed file size
   uint32_t uncompressed;  // Uncompressed file size
   uint16_t nameLength;    // Length of name following structure
   uint16_t extraLength;   // Length of extra field following name
   uint16_t commentLength; // Length of comment following extra
   uint16_t diskStartNum;  // Starting disk # for file
   uint16_t intAttribs;    // Internal file attribute bitflags
   uint32_t extAttribs;    // External file attribute bitflags
   uint32_t localOffset;   // Offset to ZIPLocalFileHeader

   // Following structure:
   // const char *filename;
   // const char *extra;
   // const char *comment;
};

using ZCDE_t = ZIPCentralDirEntry;

static MStructReader<ZCDE_t> centralDirReader([] (MStructReader<ZCDE_t> &obj) {
   obj.addField(&ZCDE_t::signature    );
   obj.addField(&ZCDE_t::madeByVersion);
   obj.addField(&ZCDE_t::extrVersion  );
   obj.addField(&ZCDE_t::gpFlags      );
   obj.addField(&ZCDE_t::method       );
   obj.addField(&ZCDE_t::fileTime     );
   obj.addField(&ZCDE_t::fileDate     );
   obj.addField(&ZCDE_t::crc32        );
   obj.addField(&ZCDE_t::compressed   );
   obj.addField(&ZCDE_t::uncompressed );
   obj.addField(&ZCDE_t::nameLength   );
   obj.addField(&ZCDE_t::extraLength  );
   obj.addField(&ZCDE_t::commentLength);
   obj.addField(&ZCDE_t::diskStartNum );
   obj.addField(&ZCDE_t::intAttribs   );
   obj.addField(&ZCDE_t::extAttribs   );
   obj.addField(&ZCDE_t::localOffset  );
});

#define ZIP_END_OF_DIR_SIG  "PK\x5\x6"
#define ZIP_END_OF_DIR_SIZE 22

struct ZIPEndOfCentralDir
{
   uint32_t signature;        // Must be "PK\x5\x6"
   uint16_t diskNum;          // Disk number (NB: multi-partite zips are NOT supported)
   uint16_t centralDirDiskNo; // Disk number containing the central directory
   uint16_t numEntriesOnDisk; // Number of entries on this disk
   uint16_t numEntriesTotal;  // Total entries in the central directory
   uint32_t centralDirSize;   // Central directory size in bytes
   uint32_t centralDirOffset; // Offset of central directory
   uint16_t zipCommentLength; // Length of following zip file comment

   // Following structure:
   // const char *comment;
};

using ZECD_t = ZIPEndOfCentralDir;

static MStructReader<ZECD_t> endCentralDirReader([] (MStructReader<ZECD_t> &obj) {
   obj.addField(&ZECD_t::signature       );
   obj.addField(&ZECD_t::diskNum         );
   obj.addField(&ZECD_t::centralDirDiskNo);
   obj.addField(&ZECD_t::numEntriesOnDisk);
   obj.addField(&ZECD_t::numEntriesTotal );
   obj.addField(&ZECD_t::centralDirSize  );
   obj.addField(&ZECD_t::centralDirOffset);
   obj.addField(&ZECD_t::zipCommentLength);
});

#define ZF_ENCRYPTED   0x01
#define BUFREADCOMMENT 0x400

//
// ZIP_FindEndOfCentralDir
//
// Derived from ZDoom, which derived it from Quake 3 unzip.c, where it is
// named unzlocal_SearchCentralDir and is derived from unzip.c by Gilles 
// Vollant, under the following BSD-style license (which ZDoom does NOT 
// properly preserve in its own code):
//
// unzip.h -- IO for uncompress .zip files using zlib
// Version 0.15 beta, Mar 19th, 1998,
//
// Copyright (C) 1998 Gilles Vollant
//
// I WAIT FEEDBACK at mail info@winimage.com
// Visit also http://www.winimage.com/zLibDll/unzip.htm for evolution
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
// 
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
static bool ZIP_FindEndOfCentralDir(InBuffer &fin, long &position)
{
   uint8_t buf[BUFREADCOMMENT + 4];
   long    FileSize  = 0;  // haleyjd: file offsets should be long
   long    uBackRead = 0;
   long    uMaxBack  = 0;
   long    uPosFound = 0;

   if(fin.seek(0, SEEK_END))
      return false;

   FileSize  = fin.tell();
   uMaxBack  = emin<long>(0xffff, FileSize);
   uBackRead = 4;

   while(uBackRead < uMaxBack)
   {
      long uReadSize, uReadPos;

      if(uBackRead + BUFREADCOMMENT > uMaxBack)
         uBackRead = uMaxBack;
      else
         uBackRead += BUFREADCOMMENT;

      uReadPos  = FileSize - uBackRead;
      uReadSize = emin<long>(BUFREADCOMMENT + 4, FileSize - uReadPos);

      if(fin.seek(uReadPos, SEEK_SET))
         return false;

      size_t sizeReadSize = static_cast<size_t>(uReadSize);
      if(fin.read(buf, sizeReadSize) != sizeReadSize)
         return false;

      for(long i = uReadSize - 3; (i--) > 0; )
      {
         if(buf[i] == 'P' && buf[i+1] == 'K' && buf[i+2] == 5 && buf[i+3] == 6)
         {
            uPosFound = uReadPos + i;
            break;
         }
      }

      if(uPosFound != 0)
         break;
   }

   position = uPosFound;
   return true;
}

//=============================================================================
//
// ZipFile Class
//

//
// Destructor
//
ZipFile::~ZipFile()
{
   // free the directory
   if(lumps && numLumps)
   {
      // free lump names
      for(int i = 0; i < numLumps; i++)
      {
         if(lumps[i].name)
            efree(lumps[i].name);
      }

      // free the lump directory
      efree(lumps);
      lumps    = nullptr;
      numLumps = 0;
   }

   // free zipwads
   if(wads)
   {
      DLListItem<ZipWad> *rover = wads;
      while(rover)
      {
         ZipWad &zw = **rover;
         rover = rover->dllNext;

         efree(zw.buffer); // free the in-memory wad file
         efree(&zw);       // free the ZipWad structure
      }
      wads = nullptr;
   }

   // close the disk file if it is open
   if(file)
   {
      fclose(file);
      file = nullptr;
   }
}

//
// ZipFile::readEndOfCentralDir
//
// Protected method. Read the end-of-central-directory data structure.
//
bool ZipFile::readEndOfCentralDir(InBuffer &fin, ZIPEndOfCentralDir &zcd)
{
   long centralDirEnd;

   // Locate the central directory
   if(!ZIP_FindEndOfCentralDir(fin, centralDirEnd) || !centralDirEnd)
      return false; 

   if(fin.seek(centralDirEnd, SEEK_SET))
      return false;

   if(!endCentralDirReader.readFields(zcd, fin))
      return false;

   // Basic sanity checks

   // Multi-disk zips aren't supported
   if(zcd.numEntriesTotal != zcd.numEntriesOnDisk ||
      zcd.diskNum != 0 || zcd.centralDirDiskNo != 0)
      return false;

   // Allocate directory
   numLumps = zcd.numEntriesTotal;
   lumps    = ecalloc(ZipLump *, numLumps + 1, sizeof(ZipLump));

   return true;
}

//
// ZipFile::readCentralDirEntry
//
// Protected method.
// Read a single entry from the central directory. "skip" will be set
// to true if, on a true return value, the lump should not be exposed to
// the world (ie., it's a directory, it's encrypted, etc.). If false is
// returned, an IO or format verification error occurred and loading
// should be aborted.
//
bool ZipFile::readCentralDirEntry(InBuffer &fin, ZipLump &lump, bool &skip)
{
   qstring namestr;
   edefstructvar(ZIPCentralDirEntry, entry);

   if(!centralDirReader.readFields(entry, fin))
      return false;

   // verify signature
   if(memcmp(&entry.signature, ZIP_CENTRAL_DIR_SIG, 4))
      return false;

   // Read out the name, entry, and comment together.
   // This will position the InBuffer at the next directory entry.
   size_t totalStrLen = 
      entry.nameLength + entry.extraLength + entry.commentLength;
   ZAutoBuffer nameBuffer(totalStrLen + 1, true);
   char *name = nameBuffer.getAs<char *>();

   if(fin.read(name, totalStrLen) != totalStrLen)
      return false;

   // skip bogus unnamed entries and directories
   if(!entry.nameLength ||
      (name[entry.nameLength - 1] == '/' && entry.uncompressed == 0))
   {
      skip = true;
      return true;
   }

   // check for supported compression method
   // TODO: support some additional methods
   switch(entry.method)
   {
   case METHOD_STORED:
   case METHOD_DEFLATE:
      break;
   default:
      skip = true; // unsupported method
      return true;
   }

   // check against encryption (bit 0 of general purpose flags == 1)
   if(entry.gpFlags & ZF_ENCRYPTED)
   {
      skip = true;
      return true;
   }
   
   // Save and normalize the name
   namestr.copy(name, entry.nameLength);
   namestr.toLower();
   namestr.replace("\\", '/');

   // Save important directory information
   lump.name       = namestr.duplicate();
   lump.gpFlags    = entry.gpFlags;
   lump.method     = entry.method;
   lump.compressed = entry.compressed;
   lump.size       = entry.uncompressed;
   lump.offset     = entry.localOffset;

   // Lump will need true offset to file data calculated the first time it is
   // read from the file.
   lump.flags |= LF_CALCOFFSET;

   // Remember our parent ZipFile
   lump.file = this;

   // Is this lump an embedded wad file?
   const char *dotpos = strrchr(lump.name, '.');
   // Must block macOS archiving artifacts
   if(dotpos && strncmp(lump.name, "__macosx/", 9) && !strncmp(dotpos, ".wad", 4))
      lump.flags |= LF_ISEMBEDDEDWAD;

   return true;
}

//
// ZipFile::readCentralDirectory
//
// Protected method.
// Read out the central directory.
//
bool ZipFile::readCentralDirectory(InBuffer &fin, long offset, uint32_t size)
{
   int lumpidx = 0; // current index into lumps[]

   // seek to start of directory
   if(fin.seek(offset, SEEK_SET))
      return false;
  
   for(int i = 0; i < numLumps; i++)
   {
      ZipLump &lump    = lumps[lumpidx];
      bool     skipped = false;
      
      if(readCentralDirEntry(fin, lump, skipped))
      {
         if(!skipped)
            ++lumpidx; // advance if not skipped
      }
      else
         return false; // an error occurred
   }

   // Adjust numLumps to omit skipped lumps
   numLumps = lumpidx;

   return true;
}

//
// ZIP_LumpSortCB
//
// qsort callback; sort the lumps using strcmp.
//
static int ZIP_LumpSortCB(const void *va, const void *vb)
{
   const ZipLump *lumpA = static_cast<const ZipLump *>(va);
   const ZipLump *lumpB = static_cast<const ZipLump *>(vb);

   return strcmp(lumpA->name, lumpB->name);
}

//
// ZipFile::readFromFile
//
// Extracts the directory from a physical ZIP file.
//
bool ZipFile::readFromFile(FILE *f)
{
   InBuffer reader;
   edefstructvar(ZIPEndOfCentralDir, zcd);

   // remember our disk file
   file = f;

   reader.openExisting(f, InBuffer::LENDIAN);

   // read in the end-of-central-directory structure
   if(!readEndOfCentralDir(reader, zcd))
      return false;

   // read in the directory
   if(!readCentralDirectory(reader, zcd.centralDirOffset, zcd.centralDirSize))
      return false;

   // sort the directory
   if(numLumps > 1)
      qsort(lumps, numLumps, sizeof(ZipLump), ZIP_LumpSortCB);

   return true;
}

//
// ZipFile::checkForWadFiles
//
// Find all lumps that were marked as LF_ISEMBEDDEDWAD, load them into memory,
// and then add them to the same directory to which this zip file belongs.
//
void ZipFile::checkForWadFiles(WadDirectory &parentDir)
{
   for(int i = 0; i < numLumps; i++)
   {
      if(!(lumps[i].flags & LF_ISEMBEDDEDWAD))
         continue;

      // will not consider any lump less than 28 in size 
      // (valid wad header, plus at least one lump in the directory)
      if(lumps[i].size < 28)
         continue;

      ZipWad *zipwad = estructalloc(ZipWad, 1);

      zipwad->size   = static_cast<size_t>(lumps[i].size);
      zipwad->buffer = Z_Malloc(zipwad->size, PU_STATIC, nullptr);

      lumps[i].read(zipwad->buffer);

      parentDir.addInMemoryWad(zipwad->buffer, zipwad->size);

      // remember this zipwad
      zipwad->links.insert(zipwad, &wads);
   }
}

//
// ZipFile::linkTo
//
// Link the ZipFile instance into a wad directory's list of zips.
//
void ZipFile::linkTo(DLListItem<ZipFile> **head)
{
   links.insert(this, head);
}

//
// ZipFile::getLump
//
// Range-checking accessor function returning a reference to a given lump
// structure. Fatal if the index is out of range.
//
ZipLump &ZipFile::getLump(int lumpNum)
{
   if(lumpNum < 0 || lumpNum >= numLumps)
      I_Error("ZipFile::getLump: %d >= numLumps\n", lumpNum);

   return lumps[lumpNum];
}

//
// ZipFile::findLump
//
// For use in a pinch only; does a linear search on the loaded directory to
// find a given lump, if it exists. Returns the highest matching lump number,
// or -1 if not found. WadDirectory takes care of hashing at runtime, so this 
// class doesn't bother.
//
int ZipFile::findLump(const char *name) const
{
   int lumpnum = -1;

   for(int i = 0; i < numLumps; i++)
   {
      if(!strcasecmp(lumps[i].name, name))
         lumpnum = i;
   }

   return lumpnum;
}

//=============================================================================
//
// ZipLump Methods
//
// The structure is a POD, but supports various methods.
//

//
// Lump Readers
//

//
// ZIP_ReadStored
//
// Read a stored zip file (stored == uncompressed, flat data)
//
static void ZIP_ReadStored(InBuffer &fin, void *buffer, uint32_t len)
{
   if(fin.read(buffer, len) != len)
      I_Error("ZIP_ReadStored: failed to read stored file\n");
}

#define DEFLATE_BUFF_SIZE 4096

//
// ZIPDeflateReader
//
// This class wraps up all zlib functionality into a nice little package
//
class ZIPDeflateReader
{
protected:
   InBuffer &fin;       // input buffered file
   z_stream  zlStream;  // zlib data structure
   bool      atEOF;     // hit EOF in InBuffer::Read
   
   byte deflateBuffer[DEFLATE_BUFF_SIZE]; // buffer for input to zlib

   void buffer()
   {
      size_t bytesRead;

      bytesRead = fin.read(deflateBuffer, DEFLATE_BUFF_SIZE);

      if(bytesRead != DEFLATE_BUFF_SIZE)
         atEOF = true;

      zlStream.next_in  = deflateBuffer;
      zlStream.avail_in = uInt(bytesRead);
   }

public:
   ZIPDeflateReader(InBuffer &pFin) 
      : fin(pFin), zlStream(), atEOF(false)
   {
      int code;
      
      zlStream.zalloc = nullptr;
      zlStream.zfree  = nullptr;

      buffer();
      
      if((code = inflateInit2(&zlStream, -MAX_WBITS)) != Z_OK)
         I_Error("ZIPDeflateReader: inflateInit2 failed with code %d\n", code);
   }

   ~ZIPDeflateReader()
   {
      inflateEnd(&zlStream);
   }

   void read(void *outbuffer, size_t len)
   {
      int code;

      zlStream.next_out  = static_cast<Bytef *>(outbuffer);
      zlStream.avail_out = static_cast<uInt>(len);

      do
      {
         code = inflate(&zlStream, Z_SYNC_FLUSH);
         if(zlStream.avail_in == 0 && !atEOF)
            buffer();
      }
      while(code == Z_OK && zlStream.avail_out);

      if(code != Z_OK && code != Z_STREAM_END)
         I_Error("ZIPDeflateReader::read: invalid deflate stream\n");

      if(zlStream.avail_out != 0)
         I_Error("ZIPDeflateReader::read: truncated deflate stream\n");
   }
};

//
// ZIP_ReadDeflated
//
// Read a deflated file (deflate == zlib compression algorithm)
//
static void ZIP_ReadDeflated(InBuffer &fin, void *buffer, size_t len)
{
   ZIPDeflateReader reader(fin);

   reader.read(buffer, len);
}

//
// ZipLump::setAddress
//
// The first time a lump is read from a zip, there is a need to calculate the
// offset to the actual file data, because it is preceded by a local file
// header with unique geometry.
//
void ZipLump::setAddress(InBuffer &fin)
{
   edefstructvar(ZIPLocalFileHeader, lfh);

   if(!(flags & ZipFile::LF_CALCOFFSET))
      return;

   if(fin.seek(offset, SEEK_SET))
      I_Error("ZipLump::setAddress: could not seek to '%s'\n", name);

   if(!localFileReader.readFields(lfh, fin))
      I_Error("ZipLump::setAddress: could not read local header for '%s'\n", name);

   // verify signature
   if(memcmp(&lfh.signature, ZIP_LOCAL_FILE_SIG, 4))
      I_Error("ZipLump::setAddress: invalid local signature for '%s'\n", name);

   size_t skipSize = lfh.nameLength + lfh.extraLength;

   // skip over name and extra
   if(skipSize > 0 && fin.skip(skipSize))
      I_Error("ZipLump::setAddress: could not skip local name for '%s'\n", name);

   // calculate total length of the local file header and advance offset
   offset += static_cast<long>(ZIP_LOCAL_FILE_SIZE + skipSize);

   // clear LF_CALCOFFSET flag
   flags &= ~ZipFile::LF_CALCOFFSET;
}

//
// ZipLump::read(void *)
//
// Read a zip lump out of the zip file.
//
void ZipLump::read(void *buffer)
{
   InBuffer reader;

   reader.openExisting(file->getFile(), InBuffer::LENDIAN);

   // Calculate an offset beyond the lump's local file header, if such hasn't
   // been done already. This will modify Lump::offset. Note if we call this,
   // we'll end up in reading position, so a seek is unnecessary then.
   if(flags & ZipFile::LF_CALCOFFSET)
      setAddress(reader);
   else
   {
      if(reader.seek(offset, SEEK_SET))
         I_Error("ZipLump::read: could not seek to lump '%s'\n", name);
   }

   // Read the file according to its indicated storage method.
   switch(method)
   {
   case ZipFile::METHOD_STORED:
      ZIP_ReadStored(reader, buffer, size);
      break;
   case ZipFile::METHOD_DEFLATE:
      ZIP_ReadDeflated(reader, buffer, size);
      break;
   default:
      // shouldn't happen; files with other methods are removed from the directory
      I_Error("ZipLump::read: internal error - unsupported compression type %d\n",
              method);
      break;
   }
}

//
// ZipLump::read(ZAutoBuffer &, bool)
//
// Read a lump's contents into an auto buffer object.
//
void ZipLump::read(ZAutoBuffer &buf, bool asString)
{
   uint32_t totalSize = size;
   if(asString)
      totalSize += 1;
   if(totalSize)
   {
      buf.alloc(totalSize, true);
      read(buf.get());
   }
}

// EOF

