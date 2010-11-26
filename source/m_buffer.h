// Emacs style mode select   -*- C -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2010 James Haley
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   Buffered file output.
//
//-----------------------------------------------------------------------------

#ifndef M_BUFFER_H__
#define M_BUFFER_H__

class COutBuffer
{
private:
   FILE *f;            // destination file
   byte *buffer;       // buffer
   unsigned int len;   // total buffer length
   unsigned int idx;   // current index
   int endian;         // endianness indicator

public:
   boolean CreateFile(const char *filename, unsigned int len, int endian);
   boolean Flush();
   void    Close();
   long    Tell();
   boolean Write(const void *data, unsigned int size);
   boolean WriteUint32(uint32_t num);
   boolean WriteUint16(uint16_t num);
   boolean WriteUint8(uint8_t num);

   // endianness values
   enum
   {
      NENDIAN, // doesn't swap shorts or ints
      LENDIAN, // swaps shorts/ints to little endian
      BENDIAN  // swaps shorts/ints to big endian
   };
};

#endif

// EOF


