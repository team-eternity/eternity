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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Simple basic typedefs, isolated here to make it easier
//       separating modules.
//
//-----------------------------------------------------------------------------

#ifndef DOOMTYPE_H__
#define DOOMTYPE_H__

#include <inttypes.h>

#ifndef BYTEBOOL__
#define BYTEBOOL__
// haleyjd 04/10/11: boolean typedef eliminated in pref. of direct use of bool
typedef uint8_t byte;
#endif

// SoM: resolve platform-specific range symbol issues

#include <limits.h>
#define D_MAXINT INT_MAX
#define D_MININT INT_MIN
#define D_MAXSHORT  SHRT_MAX

// The packed attribute forces structures to be packed into the minimum
// space necessary.  If this is not done, the compiler may align structure
// fields differently to optimize memory access, inflating the overall
// structure size.  It is important to use the packed attribute on certain
// structures where alignment is important, particularly data read/written
// to disk.

#if defined(__GNUC__)
   #define PACKED_PREFIX
   #if defined(_WIN32) && !defined(__clang__)
      #define PACKED_SUFFIX __attribute__((packed,gcc_struct))
   #else
      #define PACKED_SUFFIX __attribute__((packed))
   #endif
#elif defined(__WATCOMC__)
   #define PACKED_PREFIX _Packed
   #define PACKED_SUFFIX
#else
   #define PACKED_PREFIX
   #define PACKED_SUFFIX
#endif

#endif

//----------------------------------------------------------------------------
//
// $Log: doomtype.h,v $
// Revision 1.3  1998/05/03  23:24:33  killough
// beautification
//
// Revision 1.2  1998/01/26  19:26:43  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:51  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

