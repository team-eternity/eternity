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

#include "z_zone.h"
#include "linkoffs.h"
#include "m_bbox.h"
#include "m_vector.h"
#include "r_main.h"

// 
// M_MagnitudeVec2
//
// Get the magnitude of a two-dimensional vector
//
float M_MagnitudeVec2(const v2float_t &vec)
{
   return sqrt(vec.x * vec.x + vec.y * vec.y);
}

//
// M_NormalizeVec2
//
// Normalize a two-dimensional vector in-place.
//
void M_NormalizeVec2(v2float_t &vec)
{
   float mag = M_MagnitudeVec2(vec);

   vec.x /= mag;
   vec.y /= mag;
}

//
// M_NormalizeVec2
//
// Normalize a two-dimensional vector in-place using precomputed
// magnitude.
//
void M_NormalizeVec2(v2float_t &vec, float mag)
{
   vec.x /= mag;
   vec.y /= mag;
}

// 
// M_TranslateVec3f
//
// Translates the given vector (in doom's coordinate system) to the camera
// space (in right-handed coordinate system) This function is used for slopes.
// 
void M_TranslateVec3f(v3float_t *vec)
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

void M_TranslateVec3 (v3double_t *vec)
{
   double tx, ty, tz;

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
// Adds v2 to v1 stores in dest
//
void M_AddVec3f(v3float_t *dest, const v3float_t *v1, const v3float_t *v2)
{
   dest->x = v1->x + v2->x;
   dest->y = v1->y + v2->y;
   dest->z = v1->z + v2->z;
}


void M_AddVec3 (v3double_t *dest, const v3double_t *v1, const v3double_t *v2)
{
   dest->x = v1->x + v2->x;
   dest->y = v1->y + v2->y;
   dest->z = v1->z + v2->z;
}

// 
// M_SubVec3f
//
// Subtracts v2 from v1 stores in dest
//
void M_SubVec3f(v3float_t *dest, const v3float_t *v1, const v3float_t *v2)
{
   dest->x = v1->x - v2->x;
   dest->y = v1->y - v2->y;
   dest->z = v1->z - v2->z;
}

void M_SubVec3 (v3double_t *dest, const v3double_t *v1, const v3double_t *v2)
{
   dest->x = v1->x - v2->x;
   dest->y = v1->y - v2->y;
   dest->z = v1->z - v2->z;
}

// 
// M_DotVec3f
//
// Returns the dot product of v1 and v2
//
float M_DotVec3f(const v3float_t *v1, const v3float_t *v2)
{
   return (v1->x * v2->x) + (v1->y * v2->y) + (v1->z * v2->z);
}

double M_DotVec3 (const v3double_t *v1, const v3double_t *v2)
{
   return (v1->x * v2->x) + (v1->y * v2->y) + (v1->z * v2->z);
}

//
// M_CrossProduct3f
//
// Gets the cross product of v1 and v2 and stores in dest 
//
void M_CrossProduct3f(v3float_t *dest, const v3float_t *v1, const v3float_t *v2)
{
   v3float_t tmp;
   tmp.x = (v1->y * v2->z) - (v1->z * v2->y);
   tmp.y = (v1->z * v2->x) - (v1->x * v2->z);
   tmp.z = (v1->x * v2->y) - (v1->y * v2->x);
   memcpy(dest, &tmp, sizeof(v3float_t));
}

void M_CrossProduct3 (v3double_t *dest, const v3double_t *v1, const v3double_t *v2)
{
   v3double_t tmp;
   tmp.x = (v1->y * v2->z) - (v1->z * v2->y);
   tmp.y = (v1->z * v2->x) - (v1->x * v2->z);
   tmp.z = (v1->x * v2->y) - (v1->y * v2->x);
   memcpy(dest, &tmp, sizeof(v3double_t));
}

//=============================================================================
//
// linkmatrix_t methods
//
//=============================================================================

void linkmatrix_t::rotate(angle_t angle)
{
    // be careful of straight angles
    if(angle == 0)
        return;
    rotation += angle;
    if(angle == ANG90)
    {
        a[0][0] = -a[1][0];
        a[0][1] = -a[1][1];
        a[0][2] = -a[1][2];
        a[1][0] = a[0][0];
        a[1][1] = a[0][1];
        a[1][2] = a[0][2];
        return;
    }
    if(angle == ANG180)
    {
        a[0][0] = -a[0][0];
        a[0][1] = -a[0][1];
        a[0][2] = -a[0][2];
        a[1][0] = -a[1][0];
        a[1][1] = -a[1][1];
        a[1][2] = -a[1][2];
        return;
    }
    if(angle == ANG270)
    {
        a[0][0] = a[1][0];
        a[0][1] = a[1][1];
        a[0][2] = a[1][2];
        a[1][0] = -a[0][0];
        a[1][1] = -a[0][1];
        a[1][2] = -a[0][2];
        return;
    }
    unsigned fine = angle >> ANGLETOFINESHIFT;
    fixed_t cosine = finecosine[fine];
    fixed_t sine = finesine[fine];
    a[0][0] = FixedMul(cosine, a[0][0]) - FixedMul(sine, a[1][0]);
    a[0][1] = FixedMul(cosine, a[0][1]) - FixedMul(sine, a[1][1]);
    a[0][2] = FixedMul(cosine, a[0][2]) - FixedMul(sine, a[1][2]);
    a[1][0] = FixedMul(sine, a[0][0]) + FixedMul(cosine, a[1][0]);
    a[1][1] = FixedMul(sine, a[0][1]) + FixedMul(cosine, a[1][1]);
    a[1][2] = FixedMul(sine, a[0][2]) + FixedMul(cosine, a[1][2]);
}

void linkmatrix_t::portal(fixed_t ox, fixed_t oy, fixed_t dx, fixed_t dy, angle_t angle)
{
    // Formula: D P A P^-1
    // identity assumed
    if(!angle)
    {
        translate(dx, dy); // quick out
        return;
    }
    translate(-ox, -oy);
    rotate(angle);
    translate(ox + dx, oy + dy);
}

void portaltransform_t::apply(fixed_t &x, fixed_t &y, fixed_t &z) const
{
   double dx = M_FixedToDouble(x);
   double dy = M_FixedToDouble(y);
   double dz = M_FixedToDouble(z);
   x = M_DoubleToFixed(rot[0][0] * dx + rot[0][1] * dy + move.x);
   y = M_DoubleToFixed(rot[1][0] * dx + rot[1][1] * dy + move.y);
   z = M_DoubleToFixed(dz + move.z);
}

void portaltransform_t::apply(fixed_t &x, fixed_t &y) const
{
   double dx = M_FixedToDouble(x);
   double dy = M_FixedToDouble(y);
   x = M_DoubleToFixed(rot[0][0] * dx + rot[0][1] * dy + move.x);
   y = M_DoubleToFixed(rot[1][0] * dx + rot[1][1] * dy + move.y);
}

void portaltransform_t::apply(double &x, double &y) const
{
   double dx = x;
   double dy = y;
   x = rot[0][0] * dx + rot[0][1] * dy + move.x;
   y = rot[1][0] * dx + rot[1][1] * dy + move.y;
}

void portaltransform_t::apply(portaltransform_t &tr) const
{
   double newrot00 = rot[0][0] * tr.rot[0][0] + rot[0][1] * tr.rot[1][0];
   double newrot01 = rot[0][0] * tr.rot[0][1] + rot[0][1] * tr.rot[1][1];
   double newmovex = rot[0][0] * tr.move.x + rot[0][1] * tr.move.y + move.x;
   tr.rot[1][0] = rot[1][0] * tr.rot[0][0] + rot[1][1] * tr.rot[1][0];
   tr.rot[1][1] = rot[1][0] * tr.rot[0][1] + rot[1][1] * tr.rot[1][1];
   tr.move.y = rot[1][0] * tr.move.x + rot[1][1] * tr.move.y + move.y;
   tr.rot[0][0] = newrot00;
   tr.rot[0][1] = newrot01;
   tr.move.x = newmovex;
   tr.move.z += move.z;
   tr.angle += angle;
   if(tr.angle >= 2 * PI)
      tr.angle -= 2 * PI;
   else if(tr.angle < 0)
      tr.angle += 2 * PI;
}

#define EPSILON 0.001

bool portaltransform_t::isInverseOf(const portaltransform_t &o) const
{
   // Multiplies with another, and stops if it doesn't get to identity
   double v = rot[0][0] * o.rot[0][0] + rot[0][1] * o.rot[1][0];
   if(fabs(v - 1) > EPSILON)  // 0 0
      return false;
   v = rot[0][0] * o.rot[0][1] + rot[0][1] * rot[1][1];
   if(fabs(v) > EPSILON)      // 0 1
      return false;
   v = rot[0][0] * o.move.x + rot[0][1] * o.move.y + move.x;
   if(fabs(v) > EPSILON)      // 0 3 (0 2 is zero always)
      return false;
   v = rot[1][0] * o.rot[0][0] + rot[1][1] * o.rot[1][0];
   if(fabs(v) > EPSILON)      // 1 0
      return false;
   v = rot[1][0] * o.rot[0][1] + rot[1][1] * o.rot[1][1];
   if(fabs(v - 1) > EPSILON)  // 1 1
      return false;
   v = rot[1][0] * o.move.x + rot[1][1] * o.move.y + move.y;
   if(fabs(v) > EPSILON)      // 1 3 (1 2 is zero always)
      return false;
   v = o.move.z + move.z;
   if(fabs(v) > EPSILON)      // 2 3
      return false;
   return true;
}

void portalfixedform_t::apply(fixed_t &x, fixed_t &y) const
{
   fixed_t oldx = x;
   x = FixedMul(rot[0][0], oldx) + FixedMul(rot[0][1], y) + move.x;
   y = FixedMul(rot[1][0], oldx) + FixedMul(rot[1][1], y) + move.y;
}

void portalfixedform_t::apply(fixed_t &x, fixed_t &y, fixed_t &z) const
{
   fixed_t oldx = x;
   x = FixedMul(rot[0][0], oldx) + FixedMul(rot[0][1], y) + move.x;
   y = FixedMul(rot[1][0], oldx) + FixedMul(rot[1][1], y) + move.y;
   z += move.z;
}

void portalfixedform_t::apply(portalfixedform_t &tr) const
{
   fixed_t newrot00 = FixedMul(rot[0][0], tr.rot[0][0])
   + FixedMul(rot[0][1], tr.rot[1][0]);
   fixed_t newrot01 = FixedMul(rot[0][0], tr.rot[0][1])
   + FixedMul(rot[0][1], tr.rot[1][1]);
   fixed_t newmovex = FixedMul(rot[0][0], tr.move.x)
   + FixedMul(rot[0][1], tr.move.y) + move.x;
   tr.rot[1][0] = FixedMul(rot[1][0], tr.rot[0][0])
   + FixedMul(rot[1][1], tr.rot[1][0]);
   tr.rot[1][1] = FixedMul(rot[1][0], tr.rot[0][1])
   + FixedMul(rot[1][1], tr.rot[1][1]);
   tr.move.y = FixedMul(rot[1][0], tr.move.x)
   + FixedMul(rot[1][1], tr.move.y) + move.y;
   tr.rot[0][0] = newrot00;
   tr.rot[0][1] = newrot01;
   tr.move.x = newmovex;
   tr.move.z += move.z;
   tr.angle += angle;
}

void portalfixedform_t::applyRotation(fixed_t &x, fixed_t &y) const
{
   fixed_t oldx = x;
   x = FixedMul(rot[0][0], oldx) + FixedMul(rot[0][1], y);
   y = FixedMul(rot[1][0], oldx) + FixedMul(rot[1][1], y);
}

void portalfixedform_t::applyBox(fixed_t *bbox) const
{
   if(angle)
   {
      fixed_t bottomleft[2] = {bbox[BOXLEFT], bbox[BOXBOTTOM]};
      fixed_t bottomright[2] = {bbox[BOXRIGHT], bbox[BOXBOTTOM]};
      fixed_t topleft[2] = {bbox[BOXLEFT], bbox[BOXTOP]};
      fixed_t topright[2] = {bbox[BOXRIGHT], bbox[BOXTOP]};
      apply(bottomleft[0], bottomleft[1]);
      apply(bottomright[0], bottomright[1]);
      apply(topleft[0], topleft[1]);
      apply(topright[0], topright[1]);

      bbox[BOXLEFT] = bottomleft[0];
      if(bottomright[0] < bbox[BOXLEFT])
         bbox[BOXLEFT] = bottomright[0];
      if(topleft[0] < bbox[BOXLEFT])
         bbox[BOXLEFT] = topleft[0];
      if(topright[0] < bbox[BOXLEFT])
         bbox[BOXLEFT] = topright[0];

      bbox[BOXBOTTOM] = bottomleft[1];
      if(bottomright[1] < bbox[BOXBOTTOM])
         bbox[BOXBOTTOM] = bottomright[1];
      if(topleft[1] < bbox[BOXBOTTOM])
         bbox[BOXBOTTOM] = topleft[1];
      if(topright[1] < bbox[BOXBOTTOM])
         bbox[BOXBOTTOM] = topright[1];

      bbox[BOXRIGHT] = bottomleft[0];
      if(bottomright[0] > bbox[BOXRIGHT])
         bbox[BOXRIGHT] = bottomright[0];
      if(topleft[0] > bbox[BOXRIGHT])
         bbox[BOXRIGHT] = topleft[0];
      if(topright[0] > bbox[BOXRIGHT])
         bbox[BOXRIGHT] = topright[0];

      bbox[BOXTOP] = bottomleft[1];
      if(bottomright[1] > bbox[BOXTOP])
         bbox[BOXTOP] = bottomright[1];
      if(topleft[1] > bbox[BOXTOP])
         bbox[BOXTOP] = topleft[1];
      if(topright[1] > bbox[BOXTOP])
         bbox[BOXTOP] = topright[1];
      return;
   }
   apply(bbox[BOXLEFT], bbox[BOXBOTTOM]);
   apply(bbox[BOXRIGHT], bbox[BOXTOP]);
}

v2fixed_t portalfixedform_t::applied(fixed_t x, fixed_t y) const
{
   v2fixed_t ret = {x, y};
   apply(ret.x, ret.y);
   return ret;
}

bool linkoffset_t::approxEqual(const linkoffset_t &other) const
{
   return fabs(visual.rot[0][0] - other.visual.rot[0][0]) < EPSILON
   && fabs(visual.rot[0][1] - other.visual.rot[0][1]) < EPSILON
   && fabs(visual.rot[1][0] - other.visual.rot[1][0]) < EPSILON
   && fabs(visual.rot[1][1] - other.visual.rot[1][1]) < EPSILON
   && fabs(visual.move.x - other.visual.move.x) < EPSILON
   && fabs(visual.move.y - other.visual.move.y) < EPSILON
   && fabs(visual.move.z - other.visual.move.z) < EPSILON;
}

// EOF

