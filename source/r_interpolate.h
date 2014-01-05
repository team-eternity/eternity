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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//    Tools for renderer frame interpolation
//
//-----------------------------------------------------------------------------

#ifndef R_INTERPOLATE_H__
#define R_INTERPOLATE_H__

#include "doomtype.h"
#include "tables.h"

//
// lerpCoord
//
// Do linear interpolation on a fixed_t coordinate value from oldpos to
// newpos.
//
inline fixed_t lerpCoord(fixed_t lerp, fixed_t oldpos, fixed_t newpos)
{
   return oldpos + FixedMul(lerp, newpos - oldpos);
}

#define LLANG360 4294967296LL

//
// lerpAngle
//
// Do linear interpolation on an angle_t BAM from astart to aend.
//
inline angle_t lerpAngle(fixed_t lerp, angle_t astart, angle_t aend)
{
   int64_t start = astart;
   int64_t end   = aend;
   int64_t value = abs(start - end);
   if(value > ANG180)
   {
      if(end > start)
         start += LLANG360;
      else
         end += LLANG360;
   }
   value = start + ((end - start) * lerp / 65536);
   if(value >= 0 && value < LLANG360)
      return (angle_t)value;
   else
      return (angle_t)(value % LLANG360);
}

#endif

// EOF

