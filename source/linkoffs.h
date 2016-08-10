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

#include "m_vector.h"
#include "tables.h"

#ifndef R_NOGROUP
// No link group. I know this means there is a signed limit on portal groups but
// do you think anyone is going to make a level with 2147483647 groups that 
// doesn't have NUTS in the wad name? I didn't think so either.
#define R_NOGROUP -1
#endif

// This is a visual transform
struct portaltransform_t
{
   double rot[2][2];
   v3double_t move;
   double angle;

   // defined in m_vector.cpp
   void apply(fixed_t &x, fixed_t &y) const;
   void apply(double &x, double &y) const;
   void apply(fixed_t &x, fixed_t &y, fixed_t &z) const;
   void apply(portaltransform_t &tr) const;

   bool isInverseOf(const portaltransform_t &other) const;
};

struct portalfixedform_t
{
   fixed_t rot[2][2];
   v3fixed_t move;
   angle_t angle;

   // defined in m_vector.cpp
   void init()
   {
      rot[0][0] = rot[1][1] = FRACUNIT;
      rot[0][1] = rot[1][0] = 0;
      move.x = move.y = move.z = 0;
      angle = 0;
   }
   void apply(fixed_t &x, fixed_t &y) const;
   void apply(fixed_t &x, fixed_t &y, fixed_t &z) const;
   void apply(portalfixedform_t &tr) const;
   void applyRotation(fixed_t &x, fixed_t &y) const;
   void applyBox(fixed_t *bbox) const;
   void applyZ(fixed_t &z) const
   {
      z += move.z;   // so simple for now
   }

   v2fixed_t applied(fixed_t x, fixed_t y) const;
};

struct linkoffset_t
{
//   fixed_t x, y, z;  // TODO: remove these
   portaltransform_t visual;
   portalfixedform_t game;

   bool approxEqual(const linkoffset_t &other) const;
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

