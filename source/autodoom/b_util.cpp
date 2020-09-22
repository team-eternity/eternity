// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Ioan Chera
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Simple global utility functions
//
//-----------------------------------------------------------------------------

#ifdef _WIN32
#include <Windows.h>
#elif defined __APPLE__
#define Collection CSCollection  // redefined to avoid namespace collision
#include <CoreServices/CoreServices.h>
#undef Collection
#include <mach/mach_time.h>
#endif

#include "../z_zone.h"

#include "b_util.h"
#include "../hal/i_platform.h"
#include "../metaapi.h"
#include "../p_maputl.h"
#include "../r_defs.h"
#include "../r_main.h"
#include "../r_state.h"

//
// Returns the intersection with another line. Returns v2double_t::invalid if parallel
//
v2double_t LineEq::intersection(const LineEq &other) const
{
   //   delta = a1b2 - a2b1
   //   deltax = -c1b2 + c2b1
   //   deltay = -a1c2 + a2c1
   //   
   //   x = deltax / delta
   //   y = deltay / delta

   v2double_t deltav;
   double delta;
   delta = a * other.b - other.a * b;
   if(!delta)  // parallel
      return v2double_invalid;

   deltav.x = -c * other.b + other.c * b;
   deltav.y = -a * other.c + other.a * c;

   return deltav / delta;
}

//
// B_ProjectionOnLine
//
// Returns the projection of point "vert" on line described by "v1" and "v2".
// Has an overload for fixed-point
//
v2double_t B_ProjectionOnLine(double x, double y, double x1, double y1,
                             double dx, double dy)
{
   v2double_t ret = {x1, y1};  // in case it's just a dot
   double deltax, deltay, delta;
   double dxsq = dx * dx, dysq = dy * dy;
   
   deltax = -x * dxsq - x1 * dysq + (y1 - y) * dx * dy;
   deltay = -y * dysq - y1 * dxsq + (x1 - x) * dy * dx;
   delta = -dxsq - dysq;
   
   if (delta)
   {
      ret.x = deltax / delta;
      ret.y = deltay / delta;
   }
   
   return ret;
}

v2fixed_t B_ProjectionOnLine(fixed_t x, fixed_t y, fixed_t x1, fixed_t y1,
                              fixed_t dx, fixed_t dy)
{
   v2fixed_t ret;
   v2double_t dres = B_ProjectionOnLine(M_FixedToDouble(x),
                             M_FixedToDouble(y),
                             M_FixedToDouble(x1),
                             M_FixedToDouble(y1),
                             M_FixedToDouble(dx),
                             M_FixedToDouble(dy));
   ret.x = M_DoubleToFixed(dres.x);
   ret.y = M_DoubleToFixed(dres.y);
   return ret;
}

v2fixed_t B_ProjectionOnSegment(v2fixed_t v, v2fixed_t v1, v2fixed_t dv, fixed_t padding)
{
   angle_t angle = dv.angle();

   if(padding)
   {
      v2fixed_t olddiff = dv;
      v1 += v2fixed_t::polar(padding, angle);
      dv -= v2fixed_t::polar(2 * padding, angle);

      if((olddiff.x ^ dv.x) <= 0 && (olddiff.y ^ dv.y) <= 0)
         return v1 + dv / 2;
   }

    v2fixed_t proj = B_ProjectionOnLine(v.x, v.y, v1.x, v1.y, dv.x, dv.y);
    if (dv.x)
    {
        if (((proj.x - v1.x) ^ dv.x) < 0)
           return v1;
        if (((proj.x - v1.x - dv.x) ^ -dv.x) < 0)
           return v1 + dv;
    }
    else
    {
        if (((proj.y - v1.y) ^ dv.y) < 0)
           return v1;
        if (((proj.y - v1.y - dv.y) ^ -dv.y) < 0)
           return v1 + dv;
    }

    return proj;
}

v2float_t B_ProjectionOnLine_f(float x, float y, float x1, float y1, float dx, float dy)
{
   v2float_t ret = { x1, y1 };  // in case it's just a dot
   float deltax, deltay, delta;
   float dxsq = dx * dx, dysq = dy * dy;

   deltax = -x * dxsq - x1 * dysq + (y1 - y) * dx * dy;
   deltay = -y * dysq - y1 * dxsq + (x1 - x) * dy * dx;
   delta = -dxsq - dysq;

   if (delta)
   {
      ret.x = deltax / delta;
      ret.y = deltay / delta;
   }

   return ret;
}

v2fixed_t B_ProjectionOnLine_f(fixed_t x, fixed_t y, fixed_t x1, fixed_t y1,
   fixed_t dx, fixed_t dy)
{
   v2fixed_t ret;
   v2float_t dres = B_ProjectionOnLine_f(M_FixedToFloat(x),
      M_FixedToFloat(y),
      M_FixedToFloat(x1),
      M_FixedToFloat(y1),
      M_FixedToFloat(dx),
      M_FixedToFloat(dy));
   ret.x = M_FloatToFixed(dres.x);
   ret.y = M_FloatToFixed(dres.y);
   return ret;
}

//
// Obtains map bounds without translating by FRACBITS
//
void B_GetMapBounds(v2fixed_t &min, v2fixed_t &max)
{
   min.x = D_MAXINT;
   min.y = D_MAXINT;
   max.x = D_MININT;
   max.y = D_MININT;

   // First find limits of map

   for(int i = 0; i < numvertexes; ++i)
   {
      if(vertexes[i].x < min.x)
         min.x = vertexes[i].x;
      if(vertexes[i].x > max.x)
         max.x = vertexes[i].x;

      if(vertexes[i].y < min.y)
         min.y = vertexes[i].y;
      if(vertexes[i].y > max.y)
         max.y = vertexes[i].y;
   }
}

//
// B_IntersectionPoint
//
// Returns true if lines intersect and if so sends the result to a third parm
//
bool B_IntersectionPoint(const LineEq &l1, const LineEq &l2, double &ix, double
                         &iy)
{
//   delta = a1b2 - a2b1
//   deltax = -c1b2 + c2b1
//   deltay = -a1c2 + a2c1
//   
//   x = deltax / delta
//   y = deltay / delta
   double deltax, deltay, delta;
   delta = l1.a * l2.b - l2.a * l1.b;
   if(!delta)  // parallel
      return false;
   
   deltax = -l1.c * l2.b + l2.c * l1.b;
   deltay = -l1.a * l2.c + l2.a * l1.c;
   
   ix = deltax/delta;
   iy = deltay/delta;
   return true;
}

//
// B_BoxOnLineSide
// Copied from P_MAPUTL
//
int B_BoxOnLineSide(fixed_t top, fixed_t bottom, fixed_t left, fixed_t right,
                    fixed_t x1, fixed_t y1, fixed_t dx, fixed_t dy)
{
   int p;
   
   if(!dy)
      return (bottom > y1) == (p = top > y1) ? p ^ (dx < 0) : -1;
   if(!dx)
      return (left < x1) == (p = right < x1) ? p ^ (dy < 0) : -1;
   if(FixedDiv(dy, dx) > 0)
      return B_PointOnLineSide(right, bottom, x1, y1, dx, dy) ==
      (p = B_PointOnLineSide(left, top, x1, y1, dx, dy)) ? p : -1;
   else
      return (B_PointOnLineSide(left, bottom, x1, y1, dx, dy)) ==
      (p = B_PointOnLineSide(right, top, x1, y1, dx, dy)) ? p : -1;
}

//
// B_EmptyTableAndDelete
//
// Removes all table's elements and also deletes them from memory
//
void B_EmptyTableAndDelete(MetaTable &meta)
{
   MetaObject *mo;
   
   while((mo = meta.tableIterator((MetaObject *)NULL)))
   {
      meta.removeObject(mo);
      delete mo;
   }
}

//
// B_SegmentsIntersect
//
// Returns true if two segments do intersect
//
bool B_SegmentsIntersect(fixed_t x11, fixed_t y11, fixed_t x12, fixed_t y12,
                         fixed_t x21, fixed_t y21, fixed_t x22, fixed_t y22)
{
   angle_t ang1 = P_PointToAngle(x11, y11, x12, y12);
   angle_t ang2 = P_PointToAngle(x21, y21, x22, y22);
   angle_t ang21m1 = P_PointToAngle(x11, y11, x21, y21) - ang1;
   angle_t ang1m22 = ang1 - P_PointToAngle(x11, y11, x22, y22);
   angle_t ang11m2 = P_PointToAngle(x21, y21, x11, y11) - ang2;
   angle_t ang2m12 = ang2 - P_PointToAngle(x21, y21, x12, y12);
   
   if(!ang21m1 || !ang1m22 || !ang11m2 || !ang2m12)
   {
      return false;  // critical case: line-on-edge. Choose safe option
   }
   
   if((ang21m1 ^ ang1m22) >= ANG180 || (ang11m2 ^ ang2m12) >= ANG180 )
   {
      return false;
   }
   
   return true;
}

//
// Calculates how a point is in relation to a line: 0 is on top of v1, 1 on top
// of v2, decimal values in-between.
//
double B_RatioAlongLine(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                        fixed_t x, fixed_t y)
{
   double x1d = M_FixedToDouble(x1);
   double x2d = M_FixedToDouble(x2);
   double y1d = M_FixedToDouble(y1);
   double y2d = M_FixedToDouble(y2);
   double linesize = sqrt(pow(x2d - x1d, 2) + pow(y2d - y1d, 2));
   if(!linesize)
      return 0;
   v2double_t v1 = { (x2d - x1d) / linesize, (y2d - y1d) / linesize };
   return ((M_FixedToDouble(x) - x1d) * v1.x + (M_FixedToDouble(y) - y1d) * v1.y)
   / linesize;
}

//
// B_Log
//
// Only in debug mode, it writes an output message for debugging
//
#ifdef _DEBUG
void B_Log(const char *output, ...)
{
   static char tempstr[1024];
   va_list args;
   
   va_start(args, output);
   vsnprintf(tempstr, sizeof(tempstr), output, args);
   va_end(args);
   
#ifdef _WIN32
   OutputDebugString(tempstr);
   OutputDebugString("\n");
#else
   printf("%d: %s\n", frameid, tempstr);
#endif
}
#endif

//
// TIME MEASUREMENT
//
// For comparing before optimizing
// Kinda platform dependent, but hey...
//
TimeMeasurement::TimeMeasurement() : m_totalRun(0), m_numRuns(0), m_invalid(false)
{
    // http://stackoverflow.com/a/1825740
#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
    LARGE_INTEGER frequency;
    if (!::QueryPerformanceFrequency(&frequency))
    {
        m_invalid = true;
        return;
    }
    
    m_frequency = frequency.QuadPart;
#elif EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX

#else
   m_invalid = true;
#endif
}

void TimeMeasurement::start()
{
#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
    LARGE_INTEGER start;
    if (!::QueryPerformanceCounter(&start))
    {
        m_invalid = true;
        return;
    }
    m_startTime = start.QuadPart;
#elif EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX
   m_startTime = mach_absolute_time();
#endif
}

void TimeMeasurement::end()
{
    if (m_invalid)
        return;
#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
    LARGE_INTEGER end;
    if (!::QueryPerformanceCounter(&end))
    {
        m_invalid = true;
        return;
    }
    m_totalRun += static_cast<double>(end.QuadPart - m_startTime) / m_frequency;
#elif EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX
   long long diff = mach_absolute_time() - m_startTime;
   AbsoluteTime time = *(AbsoluteTime*)&diff;
   Nanoseconds elapsedNano = AbsoluteToNanoseconds(time);
   m_totalRun += static_cast<double>(*(long long*)&elapsedNano) / 1000000000.;
#endif
   ++m_numRuns;
}

// EOF

