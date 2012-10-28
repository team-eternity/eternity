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

#ifndef W_ZIP_H__
#define W_ZIP_H__

#include "z_zone.h"

class  InBuffer;
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
      LF_CALCOFFSET = 0x00000001 // Needs true data offset calculated
   };

protected:
   ZipLump *lumps;    // directory
   int      numLumps; // directory size
   FILE    *file;     // physical disk file

   bool readEndOfCentralDir(InBuffer &fin, ZIPEndOfCentralDir &zcd);
   bool readCentralDirEntry(InBuffer &fin, ZipLump &lump, bool &skip);
   bool readCentralDirectory(InBuffer &fin, long offset, uint32_t size);

public:
   ZipFile() : ZoneObject(), lumps(NULL), numLumps(0), file(NULL) {}
   ~ZipFile();

   bool readFromFile(FILE *f);

   ZipLump &getLump(int lumpNum);
   int      getNumLumps() const { return numLumps; }   
   FILE    *getFile()     const { return file;     }

};

#endif

// EOF

