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
//   Functions to deal with retrieving data from raw binary.
//
//-----------------------------------------------------------------------------

#ifndef M_BINARY_H__
#define M_BINARY_H__

#include "doomtype.h"
#include "m_swap.h"

// haleyjd 10/30/10: Read a little-endian short without alignment assumptions
#define read16_le(b, t) ((b)[0] | ((t)((b)[1]) << 8))

// haleyjd 10/30/10: Read a little-endian dword without alignment assumptions
#define read32_le(b, t)   \
   ((b)[0] |              \
    ((t)((b)[1]) <<  8) | \
    ((t)((b)[2]) << 16) | \
    ((t)((b)[3]) << 24))

//
// GetBinaryWord
//
// Reads an int16 from the lump data and increments the read pointer.
//
inline int16_t GetBinaryWord(byte *&data)
{
   const int16_t val = SwapShort(read16_le(data, int16_t));
   data += 2;

   return val;
}

//
// GetBinaryUWord
//
// Reads a uint16 from the lump data and increments the read pointer.
//
inline uint16_t GetBinaryUWord(byte *&data)
{
   const uint16_t val = SwapUShort(read16_le(data, uint16_t));
   data += 2;

   return val;
}

//
// GetBinaryDWord
//
// Reads an int32 from the lump data and increments the read pointer.
//
inline int32_t GetBinaryDWord(byte *&data)
{
   const int32_t val = SwapLong(read32_le(data, int32_t));
   data += 4;

   return val;
}

//
// GetBinaryUDWord
//
// Reads a uint32 from the lump data and increments the read pointer.
//
inline uint32_t GetBinaryUDWord(byte *&data)
{
   const uint32_t val = SwapULong(read32_le(data, uint32_t));
   data += 4;

   return val;
}

//
// GetBinaryString
//
// Reads a "len"-byte string from the lump data and writes it into the 
// destination buffer. The read pointer is incremented by len bytes.
//
inline void GetBinaryString(byte *&data, char *dest, const int len)
{
   const char *loc = reinterpret_cast<const char *>(data);

   memcpy(dest, loc, len);

   data += len;
}

#endif

// EOF

