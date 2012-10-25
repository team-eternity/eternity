// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      ZIP Archive Loading
//
//-----------------------------------------------------------------------------

#include "z_auto.h"

#include "m_buffer.h"
#include "m_structio.h"
#include "m_swap.h"
#include "w_zip.h"

#include "../zlib/zlib.h"

#define MEMBER16 MUint16Descriptor
#define MEMBER32 MUint32Descriptor

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

typedef ZIPLocalFileHeader ZLFH_t;

static MEMBER16<ZLFH_t, &ZLFH_t::extraLength > lfExtraLength (NULL);
static MEMBER16<ZLFH_t, &ZLFH_t::nameLength  > lfNameLength  (&lfExtraLength);
static MEMBER32<ZLFH_t, &ZLFH_t::uncompressed> lfUncompressed(&lfNameLength);
static MEMBER32<ZLFH_t, &ZLFH_t::compressed  > lfCompressed  (&lfUncompressed);
static MEMBER32<ZLFH_t, &ZLFH_t::crc32       > lfCrc32       (&lfCompressed);
static MEMBER16<ZLFH_t, &ZLFH_t::fileDate    > lfFileDate    (&lfCrc32);
static MEMBER16<ZLFH_t, &ZLFH_t::fileTime    > lfFileTime    (&lfFileDate);
static MEMBER16<ZLFH_t, &ZLFH_t::method      > lfMethod      (&lfFileTime);
static MEMBER16<ZLFH_t, &ZLFH_t::gpFlags     > lfGPFlags     (&lfMethod);
static MEMBER16<ZLFH_t, &ZLFH_t::extrVersion > lfExtrVersion (&lfGPFlags);
static MEMBER32<ZLFH_t, &ZLFH_t::signature   > lfSignature   (&lfExtrVersion);

static MStructReader<ZIPLocalFileHeader> localFileReader(&lfSignature);

#define ZIP_CENTRAL_DIR_SIG  "PK\x1\x2"
#define ZIP_CENTRAL_DIR_SIZE 46

struct ZIPCentralDirEntry
{
   uint32_t signature;     // Must be "PK\x1\2"
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

typedef ZIPCentralDirEntry ZCDE_t;

static MEMBER32<ZCDE_t, &ZCDE_t::localOffset  > cdLocalOffset  (NULL);
static MEMBER32<ZCDE_t, &ZCDE_t::extAttribs   > cdExtAttribs   (&cdLocalOffset);
static MEMBER16<ZCDE_t, &ZCDE_t::intAttribs   > cdIntAttribs   (&cdExtAttribs);
static MEMBER16<ZCDE_t, &ZCDE_t::diskStartNum > cdDiskStartNum (&cdIntAttribs);
static MEMBER16<ZCDE_t, &ZCDE_t::commentLength> cdCommentLength(&cdDiskStartNum);
static MEMBER16<ZCDE_t, &ZCDE_t::extraLength  > cdExtraLength  (&cdCommentLength);
static MEMBER16<ZCDE_t, &ZCDE_t::nameLength   > cdNameLength   (&cdExtraLength);
static MEMBER32<ZCDE_t, &ZCDE_t::uncompressed > cdUncompressed (&cdNameLength);
static MEMBER32<ZCDE_t, &ZCDE_t::compressed   > cdCompressed   (&cdUncompressed);
static MEMBER32<ZCDE_t, &ZCDE_t::crc32        > cdCrc32        (&cdCompressed);
static MEMBER16<ZCDE_t, &ZCDE_t::fileDate     > cdFileDate     (&cdCrc32);
static MEMBER16<ZCDE_t, &ZCDE_t::fileTime     > cdFileTime     (&cdFileDate);
static MEMBER16<ZCDE_t, &ZCDE_t::method       > cdMethod       (&cdFileTime);
static MEMBER16<ZCDE_t, &ZCDE_t::gpFlags      > cdGPFlags      (&cdMethod);
static MEMBER16<ZCDE_t, &ZCDE_t::extrVersion  > cdExtrVersion  (&cdGPFlags);
static MEMBER16<ZCDE_t, &ZCDE_t::madeByVersion> cdMadeByVersion(&cdExtrVersion);
static MEMBER32<ZCDE_t, &ZCDE_t::signature    > cdSignature    (&cdMadeByVersion);

static MStructReader<ZIPCentralDirEntry> centralDirReader(&cdSignature);

#define ZIP_END_OF_DIR_SIG  "PK\x5\x6"
#define ZIP_END_OF_DIR_SIZE 22

struct ZIPEndOfCentralDir
{
   uint32_t signature;        // Must be "PK\x5\6"
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

typedef ZIPEndOfCentralDir ZECD_t;

static MEMBER16<ZECD_t, &ZECD_t::zipCommentLength> endZipCommentLength(NULL);
static MEMBER32<ZECD_t, &ZECD_t::centralDirOffset> endCentralDirOffset(&endZipCommentLength);
static MEMBER32<ZECD_t, &ZECD_t::centralDirSize  > endCentralDirSize  (&endCentralDirOffset);
static MEMBER16<ZECD_t, &ZECD_t::numEntriesTotal > endNumEntriesTotal (&endCentralDirSize);
static MEMBER16<ZECD_t, &ZECD_t::numEntriesOnDisk> endNumEntriesOnDisk(&endNumEntriesTotal);
static MEMBER16<ZECD_t, &ZECD_t::centralDirDiskNo> endCentralDirDiskNo(&endNumEntriesOnDisk);
static MEMBER16<ZECD_t, &ZECD_t::diskNum         > endDiskNum         (&endCentralDirDiskNo);
static MEMBER32<ZECD_t, &ZECD_t::signature       > endSignature       (&endDiskNum);

static MStructReader<ZIPEndOfCentralDir> endCentralDirReader(&endSignature);

#define ZF_ENCRYPTED   0x01
#define BUFREADCOMMENT 0x400

template<typename T> static inline T zipmin(T a, T b) 
{
   return (a < b ? a : b);
}

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

   if(fin.Seek(0, SEEK_END))
      return false;

   FileSize  = fin.Tell();
   uMaxBack  = zipmin<long>(0xffff, FileSize);
   uBackRead = 4;

   while(uBackRead < uMaxBack)
   {
      long uReadSize, uReadPos;

      if(uBackRead + BUFREADCOMMENT > uMaxBack)
         uBackRead = uMaxBack;
      else
         uBackRead += BUFREADCOMMENT;

      uReadPos  = FileSize - uBackRead;
      uReadSize = zipmin<long>(BUFREADCOMMENT + 4, FileSize - uReadPos);

      if(fin.Seek(uReadPos, SEEK_SET))
         return false;
      if(!fin.Read(buf, static_cast<size_t>(uReadSize)))
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

   if(fin.Seek(centralDirEnd, SEEK_SET))
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
   lumps    = ecalloc(Lump *, numLumps + 1, sizeof(Lump));

   return true;
}

//
// ZipFile::readCentralDirEntry
//
// Protected method.
// Read a single entry from the central directory.
//
bool ZipFile::readCentralDirEntry(InBuffer &fin, Lump &lump, bool &skip)
{
   qstring namestr;
   ZIPCentralDirEntry entry;

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

   if(!fin.Read(name, totalStrLen))
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
   lump.name       = namestr.duplicate(PU_STATIC);
   lump.gpFlags    = entry.gpFlags;
   lump.method     = entry.method;
   lump.compressed = entry.compressed;
   lump.size       = entry.uncompressed;
   lump.offset     = entry.localOffset;

   // Lump will need true offset to file data calculated the first time it is
   // read from the file.
   lump.flags |= LF_CALCOFFSET;

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
   int lumpidx   = 0; // current index into lumps[]
   int skipLumps = 0; // number of lumps skipped

   // seek to start of directory
   if(fin.Seek(offset, SEEK_SET))
      return false;
  
   for(int i = 0; i < numLumps; i++)
   {
      Lump &lump   = lumps[lumpidx];
      bool skipped = false;
      
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
   const ZipFile::Lump *lumpA = static_cast<const ZipFile::Lump *>(va);
   const ZipFile::Lump *lumpB = static_cast<const ZipFile::Lump *>(vb);

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
   ZIPEndOfCentralDir zcd;

   reader.OpenExisting(f, 0x10000, InBuffer::LENDIAN);

   // read in the end-of-central-directory structure
   if(!readEndOfCentralDir(reader, zcd))
      return false;

   // read in the directory
   if(!readCentralDirectory(reader, zcd.centralDirOffset, zcd.centralDirSize))
      return false;

   // sort the directory
   qsort(lumps, numLumps, sizeof(ZipFile::Lump), ZIP_LumpSortCB);

   return true;
}

// EOF

