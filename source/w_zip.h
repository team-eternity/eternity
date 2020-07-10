// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      ZIP Archive Loading
//
//-----------------------------------------------------------------------------

#ifndef W_ZIP_H__
#define W_ZIP_H__

#include "z_zone.h"
#include "m_dllist.h"

class  InBuffer;
class  WadDirectory;
class  ZAutoBuffer;
struct ZIPEndOfCentralDir;
class  ZipFile;

struct ZipLump
{
   int       gpFlags;    // GP flags from zip directory entry
   int       flags;      // internal flags
   int       method;     // compression method
   uint32_t  compressed; // compressed size
   uint32_t  size;       // uncompressed size
   long      offset;     // file offset
   char     *name;       // full name 
   ZipFile  *file;       // parent zipfile

   void setAddress(InBuffer &fin);
   void read(void *buffer);
   void read(ZAutoBuffer &buf, bool asString);
};

struct ZipWad
{
   void   *buffer;
   size_t  size;

   DLListItem<ZipWad> links;
};

class ZipFile : public ZoneObject
{
public:
   // Compression methods (definition does not imply support)
   enum 
   {
      METHOD_STORED  = 0,
      METHOD_SHRINK  = 1,
      METHOD_IMPLODE = 6,
      METHOD_DEFLATE = 8,
      METHOD_BZIP2   = 12,
      METHOD_LZMA    = 14,
      METHOD_PPMD    = 98
   };

   // Lump flags
   enum
   {
      LF_CALCOFFSET    = 0x00000001, // Needs true data offset calculated
      LF_ISEMBEDDEDWAD = 0x00000002  // Is an embedded WAD file
   };

protected:
   ZipLump *lumps;    // directory
   int      numLumps; // directory size
   FILE    *file;     // physical disk file

   DLListItem<ZipFile> links; // links for use by WadDirectory

   DLListItem<ZipWad> *wads;  // wads loaded from inside the zip

   bool readEndOfCentralDir(InBuffer &fin, ZIPEndOfCentralDir &zcd);
   bool readCentralDirEntry(InBuffer &fin, ZipLump &lump, bool &skip);
   bool readCentralDirectory(InBuffer &fin, long offset, uint32_t size);

public:
   ZipFile() 
      : ZoneObject(), lumps(nullptr), numLumps(0), file(nullptr), links(), wads(nullptr) 
   {
   }
   
   ~ZipFile();

   bool readFromFile(FILE *f);

   void checkForWadFiles(WadDirectory &parentDir);

   void     linkTo(DLListItem<ZipFile> **head);
   ZipLump &getLump(int lumpNum);
   int      findLump(const char *name) const;
   int      getNumLumps() const { return numLumps; }   
   FILE    *getFile()     const { return file;     }
};

#endif

// EOF

