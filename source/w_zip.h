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
#include "doomtype.h"
#include "m_qstr.h"

class InBuffer;
struct ZIPEndOfCentralDir;

class ZipFile : public ZoneObject
{
public:
   class Lump : public ZoneObject
   {
   protected:
      int      gpflags;        // GP flags from zip directory entry
      int      flags;          // internal flags
      byte     method;         // compression method
      uint32_t compressedSize; // compressed size
      uint32_t size;           // uncompressed size
      long     offset;         // file offset
      qstring  name;           // full name

   public:
      Lump()
         : ZoneObject(),
           gpflags(0), flags(0), method(0), compressedSize(0), size(0),
           offset(0), name()
      {
      }
   };

protected:
   Lump **lumps;
   int    numLumps;

   bool readEndOfCentralDir(InBuffer &fin, ZIPEndOfCentralDir &zcd);
   bool readCentralDirEntry(byte *&dir, Lump &lump);
   bool readCentralDirectory(InBuffer &fin, long offset, uint32_t size);

public:
   ZipFile() : ZoneObject(), lumps(NULL), numLumps(0) {}
   ~ZipFile();

   bool readFromFile(FILE *f);
};

#endif

// EOF

