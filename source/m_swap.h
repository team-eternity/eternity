// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//      Endianess handling, swapping 16bit and 32bit.
//
//-----------------------------------------------------------------------------

#ifndef M_SWAP_H__
#define M_SWAP_H__

// Endianess handling.
// WAD files are stored little endian.
//
// killough 5/1/98:
// Replaced old code with inlined code which works regardless of endianess.
//

// haleyjd 12/28/09: added semi-redundant unsigned versions for type safety.

//=============================================================================
//
// Swap 16bit, that is, MSB and LSB byte.
//

inline static int16_t SwapShort(int16_t x)
{
  return (((uint8_t *) &x)[1] << 8) |
          ((uint8_t *) &x)[0];
}

inline static uint16_t SwapUShort(uint16_t x)
{
   return (((uint8_t *) &x)[1] << 8) |
           ((uint8_t *) &x)[0];
}

// haleyjd: same routine but for big-endian input

inline static int16_t SwapBigShort(int16_t x)
{
   return (((uint8_t *) &x)[0] << 8) |
           ((uint8_t *) &x)[1];
}

inline static uint16_t SwapBigUShort(uint16_t x)
{
   return (((uint8_t *) &x)[0] << 8) |
           ((uint8_t *) &x)[1];
}

//=============================================================================
//
// Swapping 32bit.
//

inline static int32_t SwapLong(int32_t x)
{
  return (((uint8_t *) &x)[3] << 24) |
         (((uint8_t *) &x)[2] << 16) |
         (((uint8_t *) &x)[1] <<  8) |
          ((uint8_t *) &x)[0];
}

inline static uint32_t SwapULong(uint32_t x)
{
   return (((uint8_t *) &x)[3] << 24) |
          (((uint8_t *) &x)[2] << 16) |
          (((uint8_t *) &x)[1] <<  8) |
           ((uint8_t *) &x)[0];
}

// haleyjd: same routine but for big-endian input

inline static int32_t SwapBigLong(int32_t x)
{
  return (((uint8_t *) &x)[0] << 24) |
         (((uint8_t *) &x)[1] << 16) |
         (((uint8_t *) &x)[2] <<  8) |
          ((uint8_t *) &x)[3];
}

inline static uint32_t SwapBigULong(uint32_t x)
{
   return (((uint8_t *) &x)[0] << 24) |
          (((uint8_t *) &x)[1] << 16) |
          (((uint8_t *) &x)[2] <<  8) |
           ((uint8_t *) &x)[3];
}

#endif

//----------------------------------------------------------------------------
//
// $Log: m_swap.h,v $
// Revision 1.3  1998/05/03  23:14:03  killough
// Make endian independent, beautify
//
// Revision 1.2  1998/01/26  19:27:15  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
