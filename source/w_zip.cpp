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
#include "m_swap.h"
#include "w_zip.h"

#include "../zlib/zlib.h"

// Internal ZIP file structures

#define ZIP_LOCAL_FILE_SIG  "PK\x3\x4"
#define ZIP_LOCAL_FILE_SIZE 30

struct ZIPLocalFileHeader
{
   uint32_t signature;         // Must be PK\x3\x4
   uint16_t extractVersion;    // Version needed to extract
   uint16_t gpFlags;           // General purpose flags
   uint16_t compressionMethod; // Compression method
   uint16_t fileTime;          // DOS file modification time
   uint16_t fileDate;          // DOS file modification date
   uint32_t crc32;             // Checksum
   uint32_t compressedSize;    // Compressed file size
   uint32_t uncompressedSize;  // Uncompressed file size
   uint16_t nameLength;        // Length of file name following struct
   uint16_t extraLength;       // Length of "extra" field following name

   // Following structure:
   // const char *filename;
   // const char *extra;
};

#define ZIP_CENTRAL_DIR_SIG  "PK\x1\x2"
#define ZIP_CENTRAL_DIR_SIZE 46

struct ZIPCentralDirEntry
{
   uint32_t signature;         // Must be "PK\x1\2"
   uint16_t madeByVersion;     // Version "made by"
   uint16_t extractVersion;    // Version needed to extract
   uint16_t gpFlags;           // General purpose flags
   uint16_t compressionMethod; // Compression method
   uint16_t fileTime;          // DOS file modification time
   uint16_t fileDate;          // DOS file modification date
   uint32_t crc32;             // Checksum
   uint32_t compressedSize;    // Compressed file size
   uint32_t uncompressedSize;  // Uncompressed file size
   uint16_t nameLength;        // Length of name following structure
   uint16_t extraLength;       // Length of extra field following name
   uint16_t fileCommentLength; // Length of comment following extra
   uint16_t diskNumStart;      // Starting disk # for file
   uint16_t internalAttribs;   // Internal file attribute bitflags
   uint32_t externalAttribs;   // External file attribute bitflags
   uint32_t localHeaderOffset; // Offset to ZIPLocalFileHeader

   // Following structure:
   // const char *filename;
   // const char *extra;
   // const char *comment;
};

#define ZIP_END_OF_DIR_SIG  "PK\x5\x6"
#define ZIP_END_OF_DIR_SIZE 22

struct ZIPEndOfCentralDir
{
   uint32_t signature;         // Must be "PK\x5\6"
   uint16_t diskNum;           // Disk number (NB: multi-partite zips are NOT supported)
   uint16_t centralDirDiskNum; // Disk number containing the central directory
   uint16_t numEntriesOnDisk;  // Number of entries on this disk
   uint16_t numEntriesTotal;   // Total entries in the central directory
   uint32_t centralDirSize;    // Central directory size in bytes
   uint32_t centralDirOffset;  // Offset of central directory
   uint16_t zipCommentLength;  // Length of following zip file comment

   // Following structure:
   // const char *comment;
};

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
   
   bool readSuccess = 
      fin.ReadUint32(zcd.signature        ) &&
      fin.ReadUint16(zcd.diskNum          ) &&
      fin.ReadUint16(zcd.centralDirDiskNum) &&
      fin.ReadUint16(zcd.numEntriesOnDisk ) &&
      fin.ReadUint16(zcd.numEntriesTotal  ) &&
      fin.ReadUint32(zcd.centralDirSize   ) &&
      fin.ReadUint32(zcd.centralDirOffset ) &&
      fin.ReadUint16(zcd.zipCommentLength );

   if(!readSuccess)
      return false;

   // Basic sanity checks

   // Multi-disk zips aren't supported
   if(zcd.numEntriesTotal != zcd.numEntriesOnDisk ||
      zcd.diskNum != 0 || zcd.centralDirDiskNum != 0)
      return false;

   // Allocate directory
   numLumps = zcd.numEntriesTotal;
   lumps    = ecalloc(Lump **, numLumps, sizeof(Lump *));

   return true;
}

// FIXME/TODO: Borrowed directly from p_setup.cpp; should be shared.

// haleyjd 10/30/10: Read a little-endian short without alignment assumptions
#define read16_le(b, t) ((b)[0] | ((t)((b)[1]) << 8))

// haleyjd 10/30/10: Read a little-endian dword without alignment assumptions
#define read32_le(b, t) \
   ((b)[0] | \
    ((t)((b)[1]) <<  8) | \
    ((t)((b)[2]) << 16) | \
    ((t)((b)[3]) << 24))

static uint16_t GetDirUShort(byte *&dir)
{
   uint16_t val = SwapUShort(read16_le(dir, uint16_t));
   dir += 2;
   return val;
}

static uint32_t GetDirULong(byte *&dir)
{
   uint32_t val = SwapULong(read32_le(dir, uint32_t));
   dir += 4;
   return val;
}

//
// ZipFile::readCentralDirEntry
//
// Protected method.
// Read a single entry from the central directory.
//
bool ZipFile::readCentralDirEntry(byte *&dir, Lump &lump)
{
   ZIPCentralDirEntry entry;

   entry.signature = GetDirULong(dir);
   if(memcmp(&entry.signature, ZIP_CENTRAL_DIR_SIG, 4))
      return false; // failed signature check

   entry.madeByVersion     = GetDirUShort(dir);
   entry.extractVersion    = GetDirUShort(dir);
   entry.gpFlags           = GetDirUShort(dir);
   entry.compressionMethod = GetDirUShort(dir);
   entry.fileTime          = GetDirUShort(dir);
   entry.fileDate          = GetDirUShort(dir);
   entry.crc32             = GetDirULong(dir);
   entry.compressedSize    = GetDirULong(dir);
   entry.uncompressedSize  = GetDirULong(dir);
   entry.nameLength        = GetDirUShort(dir);
   entry.extraLength       = GetDirUShort(dir);
   entry.fileCommentLength = GetDirUShort(dir);
   entry.diskNumStart      = GetDirUShort(dir);
   entry.internalAttribs   = GetDirUShort(dir);
   entry.externalAttribs   = GetDirULong(dir);
   entry.localHeaderOffset = GetDirULong(dir);

   // TODO: extract the needed information
   
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

   // verify size is a valid directory length
   // (NB: this is a minimum size check, the best that can be done here).
   if(size / ZIP_CENTRAL_DIR_SIZE < static_cast<uint32_t>(numLumps))
      return false;

   // seek to start of directory
   if(fin.Seek(offset, SEEK_SET))
      return false;

   // read the whole directory in one chunk, because it's easier.
   ZAutoBuffer buf(size, false);
   byte *directory = buf.getAs<byte *>();
   
   if(!fin.Read(directory, size))
      return false;

   for(int i = 0; i < numLumps; i++)
   {
      // TODO: read each lump
   }

   return true;
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

   // TODO: More

   return true;
}

// EOF

