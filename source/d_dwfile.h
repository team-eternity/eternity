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
//------------------------------------------------------------------------------
//
// Purpose: File/WAD standard input routines.
//
//  Separated DWFILE functions from d_io.h
//
//  This code was moved here from d_deh.c and generalized to make a
//  consistent interface for emulating stdio functions on wad lumps.
//  The structure here handles either external files or wad lumps,
//  depending on how it is initialized, and emulates stdio regardless.
//
// Authors: James Haley
//

#ifndef D_DWFILE_H__
#define D_DWFILE_H__

#include "doomtype.h"

//
// DWFILE
//
// lump/file input abstraction. Converted to a proper class 5/11/2013, with
// addition of RAII semantics (file will close if open when the object is
// destroyed, or if it is reopened with a new data source).
//
class DWFILE
{
protected:
   int type;
   byte *inp, *lump, *data; // Pointer to lump, FILE, or data
   int size;
   int origsize;            // for ungetc
   int lumpnum;             // haleyjd 03/08/06: need to save this

public:
   DWFILE();
   ~DWFILE();

   char  *getStr(char *buf, size_t n);
   int    atEof() const;
   int    getChar();
   int    unGetChar(int c);
   void   openFile(const char *filename, const char *mode);
   void   openLump(int p_lumpnum);
   void   close();
   size_t read(void *dest, size_t p_size, size_t p_num);
   long   fileLength() const;

   inline bool isOpen()     const { return !!inp;   }
   inline bool isLump()     const { return !!lump;  }
   inline bool isData()     const { return !!data;  }
   inline int  getLumpNum() const { return lumpnum; }

   // haleyjd 03/21/10
   enum DWFType
   {
      DWF_FILE,
      DWF_LUMP,
      DWF_DATA,
      DWF_NUMTYPES
   };
};

#endif

// EOF

