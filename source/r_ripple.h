//--------------------------------------------------------------------------
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

#ifndef __R_RIPPLE_H__
#define __R_RIPPLE_H__

#include "doomtype.h"

class ZoneHeap;

enum
{
   SWIRL_TICS = 65536   // the amount to set in definition lumps
};

byte *R_DistortedFlat(ZoneHeap &heap, int flatnum, bool usegametic = false);
extern int r_swirl;

#endif

// EOF
