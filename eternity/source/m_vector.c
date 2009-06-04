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
//      Vectors
//      SoM created 05/18/09
//
//-----------------------------------------------------------------------------

#include "m_vector.h"
#include "r_main.h"


// 
// M_TranslateVector
//
// Translates the given vector (in doom's coordinate system) to the camera
// space (in right-handed coordinate system) This function is used for slopes.
// 

void M_TranslateVector(v3float_t *vec)
{
   float tx, ty, tz;

   tx = vec->x - view.x;
   ty = view.z - vec->y;
   tz = vec->z - view.y;

   // Just like wall projection.
   vec->x = (tx * view.cos) - (tz * view.sin);
   vec->z = (tz * view.cos) + (tx * view.sin);
   vec->y = ty;
}



//
// M_AddVec3f
//
// Adds v2 to v1
void M_AddVec3f(v3float_t *v1, const v3float_t *v2)
{
   v1->x += v2->x;
   v1->y += v2->y;
   v1->z += v2->z;
}


// 
// M_SubVec3f
//
// Subtracts v2 from v1
void M_SubVec3f(v3float_t *v1, const v3float_t *v2)
{
   v1->x -= v2->x;
   v1->y -= v2->y;
   v1->z -= v2->z;
}



//
// M_CrossVec3f
// 
// Calculates the cross product of the v1 and v2 and stores the result in dest.
void M_CrossVec3f(v3float_t *dest, const v3float_t *v1, const v3float_t *v2)
{
   dest->x = (v1->y * v2->z) - (v1->z * v2->y);
   dest->y = (v1->z * v2->x) - (v1->x * v2->z);
   dest->z = (v1->x * v2->y) - (v1->y * v2->x);
}



// 
// M_DotVec3f
//
// Returns the dot product of v1 and v2
float M_DotVec3f(const v3float_t *v1, const v3float_t *v2)
{
   return (v1->x * v2->x) + (v1->y * v2->y) + (v1->z * v2->z);
}