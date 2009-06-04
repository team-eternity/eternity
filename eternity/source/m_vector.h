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


#ifndef M_VECTOR_H__
#define M_VECTOR_H__

#include "m_fixed.h"


typedef struct
{
   fixed_t x, y, z;
} v3fixed_t;

typedef struct
{
   fixed_t x, y;
} v2fixed_t;

typedef struct
{
   float x, y, z;
} v3float_t;

typedef struct
{
   float x, y;
} v2float_t;



// 
// M_TranslateVec3f
//
// Translates the given vector (in doom's coordinate system) to the camera
// space (in right-handed coordinate system) This function is used for slopes.
// 
void M_TranslateVec3f(v3float_t *vec);


//
// M_AddVec3f
//
// Adds v2 to v1 stores in dest
void M_AddVec3f(v3float_t *dest, const v3float_t *v1, const v3float_t *v2);


// 
// M_SubVec3f
//
// Subtracts v2 from v1 stores in dest
void M_SubVec3f(v3float_t *dest, const v3float_t *v1, const v3float_t *v2);

// 
// M_DotVec3f
//
// Returns the dot product of v1 and v2
float M_DotVec3f(const v3float_t *v1, const v3float_t *v2);

//
// M_CrossProduct3f
//
// Gets the cross product of v1 and v2 and stores in dest 
void M_CrossProduct3f(v3float_t *dest, const v3float_t *v1, const v3float_t *v2);

#endif