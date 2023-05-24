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

struct cbviewpoint_t;
struct v3float_t;

struct v3fixed_t
{
   v3fixed_t operator + (const v3fixed_t &other) const
   {
      return { x + other.x, y + other.y, z + other.z };
   }
   
   bool operator ! () const
   {
      return !x && !y && !z;
   }

   bool operator == (const v3fixed_t &other) const
   {
      return x == other.x && y == other.y && z == other.z;
   }

   bool operator != (const v3fixed_t &other) const
   {
      return x != other.x || y != other.y || z != other.z;
   }

   v3fixed_t &operator += (const v3fixed_t &other)
   {
      x += other.x;
      y += other.y;
      z += other.z;
      return *this;
   }

   static v3fixed_t floatToFixed(const v3float_t &v);

   fixed_t x, y, z;
};

struct v2float_t;
struct v2double_t;

struct v2fixed_t
{
   fixed_t x, y;

   // ioanch 20160106: added operators as needed

   v2fixed_t() = default;
   v2fixed_t(const fixed_t x, const fixed_t y) : x(x), y(y)
   {
   }
   explicit v2fixed_t(const v3fixed_t &other3d) : x(other3d.x), y(other3d.y)
   {
   }

   v2fixed_t operator + (const v2fixed_t other) const
   {
      return { x + other.x, y + other.y };
   }

   v2fixed_t operator - (const v2fixed_t other) const
   {
      return { x - other.x, y - other.y };
   }

   v2fixed_t &operator += (const v2fixed_t other)
   {
      x += other.x;
      y += other.y;
      return *this;
   }

   v2fixed_t &operator -= (const v2fixed_t other)
   {
      x -= other.x;
      y -= other.y;
      return *this;
   }

   v2fixed_t &operator /= (int scalar)
   {
      x /= scalar;
      y /= scalar;
      return *this;
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

   bool operator != (const v2fixed_t other) const
   {
      return x != other.x || y != other.y;
   }

   bool operator == (const v2fixed_t other) const
   {
      return x == other.x && y == other.y;
   }

   static v2fixed_t floatToFixed(const v2float_t &v);
   static v2fixed_t doubleToFixed(const v2double_t &v);

   v2fixed_t fixedMul(const fixed_t scalar) const
   {
      return { FixedMul(x, scalar), FixedMul(y, scalar) };
   }

   v2fixed_t fixedDiv(const fixed_t scalar) const
   {
      return { FixedDiv(x, scalar), FixedDiv(y, scalar) };
   }

   v2fixed_t operator / (int scalar) const
   {
      return { x / scalar, y / scalar };
   }

   v2fixed_t operator * (int scalar) const
   {
      return { x * scalar, y * scalar };
   }
};

struct v3float_t
{
   float x, y, z;

   v3float_t operator - (const v3float_t &other) const
   {
      return { x - other.x, y - other.y, z - other.z };
   }

   // cross product
   v3float_t operator % (const v3float_t &other) const
   {
      return {
         y * other.z - z * other.y,
         z * other.x - x * other.z,
         x * other.y - y * other.x
      };
   }

   v3float_t &operator /= (float operand)
   {
      x /= operand;
      y /= operand;
      z /= operand;
      return *this;
   }

   float abs() const;
};

struct v2float_t;

struct v2double_t
{
   double x, y;

   explicit operator v2float_t() const;
};

struct v3double_t
{
   double x, y, z;
};

inline v3fixed_t v3fixed_t::floatToFixed(const v3float_t &v)
{
   return { M_FloatToFixed(v.x), M_FloatToFixed(v.y), M_FloatToFixed(v.z) };
}

struct v2float_t
{
   float x, y;

   bool operator != (const v2float_t other) const
   {
      return x != other.x || y != other.y;
   }

   bool operator == (const v2float_t other) const
   {
      return x == other.x && y == other.y;
   }

   v2float_t operator + (const v2float_t other) const
   {
      return { x + other.x, y + other.y };
   }

   v2float_t operator - (const v2float_t other) const
   {
      return { x - other.x, y - other.y };
   }

   v2float_t operator - () const
   {
      return { -x, -y };
   }

   float operator * (const v2float_t other) const
   {
      return x * other.x + y * other.y;
   }

   v2float_t operator * (const float other) const
   {
      return { x * other, y * other };
   }

   v2float_t &operator += (const v2float_t other)
   {
      x += other.x;
      y += other.y;
      return *this;
   }
   
   v2float_t &operator -= (const v2float_t other)
   {
      x -= other.x;
      y -= other.y;
      return *this;
   }

   //
   // Z of cross product assuming these two vectors have z=0. Uses the corkscrew rule. Useful to
   // know line side of point.
   //
   float operator % (const v2float_t other) const
   {
      // i  j  k
      // x  y  0
      // ox oy 0
      return x * other.y - other.x * y;
   }

   bool nonzero() const
   {
      return x || y;
   }

   static v2float_t fromFixed(const v2fixed_t other)
   {
      return { M_FixedToFloat(other.x), M_FixedToFloat(other.y) };
   }
};

//
// Vector-wise operation
//
inline v2fixed_t v2fixed_t::floatToFixed(const v2float_t &v)
{
   return { M_FloatToFixed(v.x), M_FloatToFixed(v.y) };
}
inline v2fixed_t v2fixed_t::doubleToFixed(const v2double_t &v)
{
   return { M_DoubleToFixed(v.x), M_DoubleToFixed(v.y) };
}

inline v2double_t::operator v2float_t() const
{
   return v2float_t{ static_cast<float>(x), static_cast<float>(y) };
}

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
void M_TranslateVec3f(const cbviewpoint_t &cb_viewpoint, v3float_t *vec);
void M_TranslateVec3 (const cbviewpoint_t &cb_viewpoint, v3double_t *vec);


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

