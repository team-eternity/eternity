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
//      Vectors
//      SoM created 05/18/09
//
//-----------------------------------------------------------------------------

#ifndef M_VECTOR_H__
#define M_VECTOR_H__

#include "m_fixed.h"

struct v3fixed_t
{
   fixed_t x, y, z;
};

struct v2fixed_t
{
   fixed_t x, y;

   v2fixed_t() = default;
   v2fixed_t(fixed_t x, fixed_t y) : x(x), y(y)
   {
   }

   template<typename T>
   explicit v2fixed_t(const T &misc) : x(misc.x), y(misc.y)
   {
   }
   
   // ioanch 20160106: added operators as needed
   template<typename T>
   bool operator == (T &&other) const { return x == other.x && 
                                               y == other.y; }
   template<typename T>
   bool operator != (T &&other) const { return x != other.x ||
      y != other.y; }

   template<typename T>
   v2fixed_t &operator += (T &&other) { x += other.x; 
                                        y += other.y; return *this; }
                                        
   template<typename T>
   v2fixed_t operator - (T &&other) const 
   { 
      v2fixed_t ret = { x - other.x, y - other.y };
      return ret;
   }

   template<typename T>
   v2fixed_t &operator -= (const T &other)
   {
      x -= other.x;
      y -= other.y;
      return *this;
   }

   v2fixed_t operator / (int n) const
   {
      return { x / n, y / n };
   }

   v2fixed_t operator-() const
   {
      return { -x, -y };
   }

   v2fixed_t abs() const
   {
      return { D_abs(x), D_abs(y) };
   }

   bool operator ! () const
   {
      return !x && !y;
   }

   //
   // Returns the exact absolute value. MUST NOT BE USED IN DEMO CRITICAL CODE because it relies on
   // floating point
   //
   fixed_t sqrtabs() const
   {
      return (fixed_t)sqrt((double)x * x + (double)y * y);
   }
};

struct v3float_t
{
   float x, y, z;
};

struct v2double_t
{
   double x, y;
};

struct v3double_t
{
   double x, y, z;
};

struct v2float_t
{
   float x, y;
};

// 
// M_MagnitudeVec2
//
// Get the magnitude of a two-dimensional vector
//
float M_MagnitudeVec2(const v2float_t &vec);

//
// M_NormalizeVec2
//
// Normalize a two-dimensional vector.
// The overload taking a double allows passing in the precomputed magnitude
// when it is available.
//
void M_NormalizeVec2(v2float_t &vec);
void M_NormalizeVec2(v2float_t &vec, float mag);

// 
// M_TranslateVec3
//
// Translates the given vector (in doom's coordinate system) to the camera
// space (in right-handed coordinate system) This function is used for slopes.
// 
void M_TranslateVec3f(v3float_t *vec);
void M_TranslateVec3 (v3double_t *vec);


// 
// M_SubVec3
//
// Subtracts v2 from v1 stores in dest
//
void M_SubVec3f(v3float_t *dest,  const v3float_t *v1,  const v3float_t *v2);
void M_SubVec3 (v3double_t *dest, const v3double_t *v1, const v3double_t *v2);

// 
// M_DotVec3
//
// Returns the dot product of v1 and v2
//
float  M_DotVec3f(const v3float_t *v1,  const v3float_t *v2);
double M_DotVec3 (const v3double_t *v1, const v3double_t *v2);

//
// M_CrossProduct3f
//
// Gets the cross product of v1 and v2 and stores in dest
//
void M_CrossProduct3f(v3float_t *dest,  const v3float_t *v1,  const v3float_t *v2);
void M_CrossProduct3 (v3double_t *dest, const v3double_t *v1, const v3double_t *v2);

#endif

// EOF

