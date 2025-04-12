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
// Purpose: Lookup tables.
//  Do not try to look them up :-).
//  In the order of appearance:
//
//  int finetangent[4096]   - Tangens LUT.
//   Should work with BAM fairly well (12 of 16bit,
//   effectively, by shifting).
//
//  int finesine[10240]             - Sine lookup.
//   Guess what, serves as cosine, too.
//   Remarkable thing is, how to use BAMs with this?
//
//  int tantoangle[2049]    - ArcTan LUT,
//    maps tan(angle) to angle fast. Gotta search.
//
// Authors: James Haley, Ioan Chera, Max Waine
//

#ifndef __TABLES__
#define __TABLES__

#include "m_fixed.h"

#define FINEANGLES              8192
#define FINEMASK                (FINEANGLES-1)

// 0x100000000 to 0x2000
#define ANGLETOFINESHIFT        19

// Effective size is 10240.
extern const fixed_t finesine[5*FINEANGLES/4];

// Re-use data, is just PI/2 phase shift.
extern const fixed_t *const finecosine;

// Effective size is 4096.
extern const fixed_t finetangent[FINEANGLES/2];

// Binary Angle Measument, BAM.
#define ANG45   0x20000000
#define ANG90   0x40000000
#define ANG180  0x80000000
#define ANG270  0xc0000000

#define ANG360  0xffffffff

// haleyjd 1/17/00

#define ANGLE_1 (ANG45/45)

// haleyjd 08/06/13: Heretic uses a value of "ANGLE_1" that is off
// by 40% - the correct value is 0xb60b60. This is rougly 1.4 degrees.
#define HTICANGLE_1 0x1000000

#define SLOPERANGE 2048
#define SLOPEBITS    11
#define DBITS      (FRACBITS-SLOPEBITS)

// PORTABILITY WARNING: angle_t depends on 32-bit math overflow

typedef uint32_t angle_t;

// Effective size is 2049;
// The +1 size is to handle the case when x==y without additional checking.

extern const angle_t tantoangle[2049];
extern angle_t tantoangle_acc[2049];

void Table_InitTanToAngle(void);

//
// haleyjd 06/07/06:
//
// fixed_t <-> angle conversions -- note these assume the fixed_t number is
// already within the range of 0 to 359. If not, the resulting value begins
// to lose accuracy, with the inaccuracy increasing with distance from this
// range.
//
inline static angle_t FixedToAngle(fixed_t a)
{
   return (angle_t)(((uint64_t)a * ANGLE_1) >> FRACBITS);
}

inline static fixed_t AngleToFixed(angle_t a)
{
   return (fixed_t)(((uint64_t)a << FRACBITS) / ANGLE_1);
}

// [XA] Clamped angle->slope, for convenience
inline static fixed_t AngleToSlope(int a)
{
   if(a > ANG90)
      return finetangent[0];
   else if(-a > ANG90)
      return finetangent[FINEANGLES / 2 - 1];
   else
      return finetangent[(ANG90 - a) >> ANGLETOFINESHIFT];
}

// [XA] Ditto, using fixed-point-degrees input
inline static fixed_t DegToSlope(fixed_t a)
{
   if(a >= 0)
      return AngleToSlope(FixedToAngle(a));
   else
      return AngleToSlope(-(int)FixedToAngle(-a));
}

#endif

//----------------------------------------------------------------------------
//
// $Log: tables.h,v $
// Revision 1.3  1998/05/03  22:58:56  killough
// beautification
//
// Revision 1.2  1998/01/26  19:27:58  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:05  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
