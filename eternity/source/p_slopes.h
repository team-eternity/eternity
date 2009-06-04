// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2004 Stephen McGranahan
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
//      Slopes
//      SoM created 05/10/09
//
//-----------------------------------------------------------------------------


#ifndef P_SLOPES_H__
#define P_SLOPES_H__


// P_MakeLineNormal
// Calculates a 2D normal for the given line and stores it in the line
void P_MakeLineNormal(line_t *line);


// P_SpawnSlope_Line
// Creates one or more slopes based on the given line type and front/back
// sectors.
void P_SpawnSlope_Line(int linenum);


// Returns the height of the sloped plane at (x, y) as a fixed_t
fixed_t P_GetZAt(pslope_t *slope, fixed_t x, fixed_t y);


// Returns the height of the sloped plane at (x, y) as a float
float P_GetZAtf(pslope_t *slope, float x, float y);


// Returns the distance of the given point from the given origin and normal.
float P_DistFromPlanef(const v3float_t *point, const v3float_t *pori, 
                       const v3float_t *pnormal);

// Vector ops... (should be in own file maybe?)

// P_CrossProduct3f
// Gets the cross product of v1 and v2 and stores in dest 
void P_CrossProduct3f(v3float_t *dest, const v3float_t *v1, const v3float_t *v2);

// P_SubVec3f
// Subtracts v2 from v1 and stores the result in dest
void P_SubVec3f(v3float_t *dest, const v3float_t *v1, const v3float_t *v2);

// P_CrossVec3f
float P_CrossVec3f(const v3float_t *v1, const v3float_t *v2);


#endif

// EOF

