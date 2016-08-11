// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Stephen McGranahan et al.
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
//      Linked portals
//      SoM created 02/13/06
//
//-----------------------------------------------------------------------------

#ifndef P_LINKOFFSET_H__
#define P_LINKOFFSET_H__

#include "m_fixed.h"
#include "tables.h"

#ifndef R_NOGROUP
// No link group. I know this means there is a signed limit on portal groups but
// do you think anyone is going to make a level with 2147483647 groups that 
// doesn't have NUTS in the wad name? I didn't think so either.
#define R_NOGROUP -1
#endif

struct linkoffset_t
{
   fixed_t x, y, z;
};

//
// Combined portal transformation. Contains rotation and translation. Formula
// for a single portal is: D * P * A * P^-1, where:
//
// P is translation(line.center.x, line.center.y)
// A is rotation(angle)
// D is translation(deltax, deltay)
//
// Only store the relevant lines, the other ones are constant.
//
// Type is fixed point because it must be deterministic. Do NOT use this for
// rendering.
//
// "rotation" is total angle difference, needed for gameplay to know how much
// to rotate things.
//
// Non-inline methods are defined in m_vector.cpp
//
struct linkmatrix_t
{
    fixed_t a[2][3];
    angle_t rotation;

    void identity()
    {
        a[0][0] = a[1][1] = 1;
        a[0][1] = a[0][2] = a[1][0] = a[1][2] = 0;
        rotation = 0;
    }

    void translate(fixed_t dx, fixed_t dy)
    {
        a[0][2] += dx;
        a[1][2] += dy;
    }

    void rotate(angle_t angle);
    void portal(fixed_t ox, fixed_t oy, fixed_t dx, fixed_t dy, angle_t angle);
};

extern linkoffset_t **linktable;
extern linkoffset_t zerolink;

linkoffset_t *P_GetLinkOffset(int startgroup, int targetgroup);
linkoffset_t *P_GetLinkIfExists(int fromgroup, int togroup);

#endif

// EOF

