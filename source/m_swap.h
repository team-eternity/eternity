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
// Purpose: Endianess handling, swapping 16bit and 32bit.
// Authors: James Haley
//

#ifndef M_SWAP_H__
#define M_SWAP_H__

// Endianess handling.
// WAD files are stored little endian.
//
// killough 5/1/98:
// Replaced old code with inlined code which works regardless of endianess.
//

//=============================================================================
//
// Swap 16-bit, that is, MSB and LSB byte.
//

inline static int16_t SwapShort(int16_t x)
{
    return (((uint8_t *)&x)[1] << 8) | //
           ((uint8_t *)&x)[0];
}

inline static uint16_t SwapUShort(uint16_t x)
{
    return (((uint8_t *)&x)[1] << 8) | //
           ((uint8_t *)&x)[0];
}

// haleyjd: same routine but for big-endian input

inline static int16_t SwapBigShort(int16_t x)
{
    return (((uint8_t *)&x)[0] << 8) | //
           ((uint8_t *)&x)[1];
}

inline static uint16_t SwapBigUShort(uint16_t x)
{
    return (((uint8_t *)&x)[0] << 8) | //
           ((uint8_t *)&x)[1];
}

//=============================================================================
//
// Swapping 32-bit.
//

inline static int32_t SwapLong(int32_t x)
{
    return (((uint8_t *)&x)[3] << 24) | //
           (((uint8_t *)&x)[2] << 16) | //
           (((uint8_t *)&x)[1] << 8) |  //
           ((uint8_t *)&x)[0];
}

inline static uint32_t SwapULong(uint32_t x)
{
    return (((uint8_t *)&x)[3] << 24) | //
           (((uint8_t *)&x)[2] << 16) | //
           (((uint8_t *)&x)[1] << 8) |  //
           ((uint8_t *)&x)[0];
}

// haleyjd: same routine but for big-endian input

inline static int32_t SwapBigLong(int32_t x)
{
    return (((uint8_t *)&x)[0] << 24) | //
           (((uint8_t *)&x)[1] << 16) | //
           (((uint8_t *)&x)[2] << 8) |  //
           ((uint8_t *)&x)[3];
}

inline static uint32_t SwapBigULong(uint32_t x)
{
    return (((uint8_t *)&x)[0] << 24) | //
           (((uint8_t *)&x)[1] << 16) | //
           (((uint8_t *)&x)[2] << 8) |  //
           ((uint8_t *)&x)[3];
}

//============================================================================
//
// Swapping 64-bit.
//

inline static int64_t SwapInt64(int64_t x)
{
    return (int64_t(((uint8_t *)&x)[7]) << 56) | //
           (int64_t(((uint8_t *)&x)[6]) << 48) | //
           (int64_t(((uint8_t *)&x)[5]) << 40) | //
           (int64_t(((uint8_t *)&x)[4]) << 32) | //
           (int64_t(((uint8_t *)&x)[3]) << 24) | //
           (int64_t(((uint8_t *)&x)[2]) << 16) | //
           (int64_t(((uint8_t *)&x)[1]) << 8) |  //
           int64_t(((uint8_t *)&x)[0]);
}

inline static uint64_t SwapUInt64(uint64_t x)
{
    return (uint64_t(((uint8_t *)&x)[7]) << 56) | //
           (uint64_t(((uint8_t *)&x)[6]) << 48) | //
           (uint64_t(((uint8_t *)&x)[5]) << 40) | //
           (uint64_t(((uint8_t *)&x)[4]) << 32) | //
           (uint64_t(((uint8_t *)&x)[3]) << 24) | //
           (uint64_t(((uint8_t *)&x)[2]) << 16) | //
           (uint64_t(((uint8_t *)&x)[1]) << 8) |  //
           uint64_t(((uint8_t *)&x)[0]);
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
