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
//      Slopes
//      SoM created 05/10/09
//
//-----------------------------------------------------------------------------

#ifndef P_SLOPES_H__
#define P_SLOPES_H__

#include "linkoffs.h"

struct line_t;
struct pslope_t;
struct v3float_t;
class UDMFSetupSettings;

void P_SlopeMapInit();

// P_MakeLineNormal
// Calculates a 2D normal for the given line and stores it in the line
void P_MakeLineNormal(line_t *line);


// P_SpawnSlope_Line
// Creates one or more slopes based on the given line type and front/back
// sectors.
void P_SpawnSlope_Line(int linenum, int staticFn);

void P_SpawnVertexSlopes(const UDMFSetupSettings &setupSettings);

//
// P_CopySectorSlope
//
// Searches through tagged sectors and copies
//
void P_CopySectorSlope(line_t *line, int staticFn);

// Returns the height of the sloped plane at (x, y) as a fixed_t
fixed_t P_GetZAt(pslope_t *slope, fixed_t x, fixed_t y);


// Returns the height of the sloped plane at (x, y) as a float
float P_GetZAtf(const pslope_t *slope, float x, float y);


// Returns the distance of the given point from the given origin and normal.
float P_DistFromPlanef(const v3float_t *point, const v3float_t *pori, 
                       const v3float_t *pnormal);

fixed_t P_GetFloorHeight(const sector_t *sector, fixed_t x, fixed_t y,
   int groupid = R_NOGROUP);

fixed_t P_GetCeilingHeight(const sector_t *sector, fixed_t x, fixed_t y,
   int groupid = R_NOGROUP);

//
// Use templates to avoid the need of including headers.
// This is for convenience.
//
template <typename MOBJ>
inline static fixed_t P_GetFloorHeight(const sector_t *sector, const MOBJ *mobj)
{
   return P_GetFloorHeight(sector, mobj->x, mobj->y, mobj->groupid);
}
template <typename MOBJ>
inline static fixed_t P_GetCeilingHeight(const sector_t *sector, const MOBJ *mobj)
{
   return P_GetCeilingHeight(sector, mobj->x, mobj->y, mobj->groupid);
}

#endif

// EOF

