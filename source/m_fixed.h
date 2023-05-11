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
//      Fixed point arithemtics, implementation.
//
//-----------------------------------------------------------------------------

#ifndef M_FIXED_H__
#define M_FIXED_H__

#include "doomtype.h"

//
// Fixed point, 32bit as 16.16.
//

#define FRACBITS 16
#define FRACUNIT (1<<FRACBITS)

typedef int32_t fixed_t;

//
// Absolute Value
//

// killough 5/10/98: In djgpp, use inlined assembly for performance
// killough 9/05/98: better code seems to be gotten from using inlined C

// haleyjd 04/24/02: renamed to D_abs, added Win32 version
#if defined(__GNUC__)
#define D_abs(x) ({fixed_t _t = (x), _s = _t >> (8*sizeof _t-1); (_t^_s)-_s;})
#else
inline static int D_abs(int x)
{
   fixed_t _t = x, _s;
   _s = _t >> (8*sizeof _t-1);
   return (_t^_s) - _s;
}
#endif

// haleyjd 11/30/02: removed asm const modifiers due to continued
// problems

//
// Fixed Point Multiplication
//
inline static fixed_t FixedMul(fixed_t a, fixed_t b)
{
   return (fixed_t)((int64_t) a*b >> FRACBITS);
}

//
// Fixed Point Division
//
inline static fixed_t FixedDiv(fixed_t a, fixed_t b)
{
   return (D_abs(a)>>14) >= D_abs(b) ? ((a^b)>>31) ^ D_MAXINT :
      (fixed_t)(((int64_t) a << FRACBITS) / b);
}

// SoM: this is only the case for 16.16 bit fixed point. If a different 
// precision is desired, this must be changed accordingly
#define FPFRACUNIT 65536.0

// SoM 5/10/09: These are now macroized for the sake of uniformity
#define M_FloatToFixed(f) ((fixed_t)((f) * FPFRACUNIT))
#define M_FixedToFloat(f) ((float)((f) / FPFRACUNIT))

#define M_FixedToDouble(f) ((double)((f) / FPFRACUNIT))
#define M_DoubleToFixed(f) ((fixed_t)((f) * FPFRACUNIT))

#endif

//----------------------------------------------------------------------------
//
// $Log: m_fixed.h,v $
// Revision 1.5  1998/05/10  23:42:22  killough
// Add inline assembly for djgpp (x86) target
//
// Revision 1.4  1998/04/27  01:53:37  killough
// Make gcc extensions #ifdef'ed
//
// Revision 1.3  1998/02/02  13:30:35  killough
// move fixed point arith funcs to m_fixed.h
//
// Revision 1.2  1998/01/26  19:27:09  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:53  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

