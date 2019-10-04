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

#ifndef B_UTIL_H_
#define B_UTIL_H_

#include <unordered_map>
#include "b_lineeffect.h"
#include "../m_collection.h"
#include "../m_dllist.h"
#include "../m_vector.h"
#include "../tables.h"

// Clock measuring functions
#ifdef _DEBUG

#define B_BEGIN_CLOCK clock_t B_CLK_MEASURE = clock();

#define B_MEASURE_CLOCK(func) \
B_CLK_MEASURE = clock() - B_CLK_MEASURE; \
B_Log("%s/" #func ": %lu ticks", __FUNCTION__, B_CLK_MEASURE);

#define B_NEW_CLOCK B_CLK_MEASURE = clock();

#else

#define B_BEGIN_CLOCK
#define B_MEASURE_CLOCK(func)
#define B_NEW_CLOCK

#endif

// ioanch 20151230: defined macros for the validcount sets here
#ifndef VALID_ALLOC
#define VALID_ALLOC(set, n) ((set) = ecalloc(byte *, 1, (((n) + 7) & ~7) / 8))
#endif

#ifndef VALID_FREE
#define VALID_FREE(set) efree(set)
#endif

#ifndef VALID_ISSET
#define VALID_ISSET(set, i) ((set)[(i) >> 3] & (1 << ((i) & 7)))
#endif

#ifndef VALID_SET
#define VALID_SET(set, i) ((set)[(i) >> 3] |= 1 << ((i) & 7))
#endif

#define VALID_CLEAR(set, n) memset((set), 0, (((n) + 7) & ~7) / 8)


class MetaTable;
struct v2double_t;

//
// LineEq
//
// ax + by + c = 0
//
struct LineEq
{
   double a, b, c;
   static LineEq MakeDouble(double inA, double inB, double inC)
   {
      LineEq ret;
      ret.a = inA;
      ret.b = inB;
      ret.c = inC;
      return ret;
   }
   static LineEq MakeFixed(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
   {
      LineEq ret;
      ret.a = M_FixedToDouble(y1 - y2);
      ret.b = M_FixedToDouble(x2 - x1);
      ret.c = M_FixedToDouble(FixedMul64(x1, y2) -
                              FixedMul64(x2, y1));
      return ret;
   }
   template <typename T> static LineEq MakeFloat(const T &obj)
   {
      LineEq ret;
      ret.a = obj.a;
      ret.b = obj.b;
      ret.c = obj.c;
      return ret;
   }
   template <typename T> static LineEq MakeFloat(const T &crd1, const T &crd2)
   {
      LineEq ret;
      ret.a = crd1.y - crd2.y;
      ret.b = crd2.x - crd1.x;
      ret.c = crd1.x * crd2.y - crd2.x * crd1.y;
      return ret;
   }
   template <typename T, typename U> static LineEq MakeFixed(const T &crd1,
                                                             const U &crd2)
   {
      LineEq ret;
      ret.a = M_FixedToDouble(crd1.y - crd2.y);
      ret.b = M_FixedToDouble(crd2.x - crd1.x);
      ret.c = M_FixedToDouble(FixedMul64(crd1.x, crd2.y) -
                              FixedMul64(crd2.x, crd1.y));
      return ret;
   }
};

#define B_Frac2Int(x) ((x)>>FRACBITS)

//
// B_MakeV2Fixed
//
// Returns a v2fixed_t value
//
inline static v2fixed_t B_MakeV2Fixed(fixed_t x, fixed_t y)
{
   v2fixed_t ret;
   ret.x = x;
   ret.y = y;
   return ret;
}

//
// operator == on v2fixed_t
//
inline static bool operator == (const v2fixed_t &v1, const v2fixed_t &v2)
{
   return v1.x == v2.x && v1.y == v2.y;
}

//
// B_Sign
//
// Utility signum function
//
inline static fixed_t B_Sign(fixed_t value)
{
   return value / D_abs(value);
}

//
// B_ExactDistance
//
// Better than P_AproxDistance and no slower!
//
inline static fixed_t B_ExactDistance(fixed_t dx, fixed_t dy)
{
   return (fixed_t)sqrt((double)dx * dx + (double)dy * dy);
}
template <typename T, typename U>
inline static fixed_t B_ExactDistance(const T& m, const U& n)
{
   return B_ExactDistance(n.x - m.x, n.y - m.y);
}

//
// B_AngleSine
// B_AngleCosine
//
// finesine with ANGLETOFINESHIFT
//
#define B_AngleSine(A) (finesine[(A) >> ANGLETOFINESHIFT])
#define B_AngleCosine(A) (finecosine[(A) >> ANGLETOFINESHIFT])

//
// B_GetBlockCoords
//
// Translates given x/y coordinates to blockmap coords
//
inline static int B_GetBlockCoords(fixed_t x, fixed_t y, fixed_t orgX,
                                   fixed_t orgY, int width, fixed_t bsize)
{
   return (x - orgX) / bsize + (y - orgY) / bsize * width;
}

//
// LinePointCompare
//
// Used by qsort to sort points on a line
//
struct LinePointCompare
{
   bool operator() (const v2fixed_t &a, const v2fixed_t &b) const
   {
      return a.x == b.x ? a.y > b.y : a.x > b.x;
   }
};

//
// B_PointOnLineSide
// Returns 0 or 1
//
// Based on P_PointOnLineSide from P_MAPUTL.CPP
//
inline static int B_PointOnLineSide(fixed_t x, fixed_t y, fixed_t x1,
                                    fixed_t y1, fixed_t dx, fixed_t dy)
{
   return
   !dx ? x <= x1 ? dy > 0 : dy < 0 : !dy ? y <= y1 ? dx < 0 : dx > 0 :
   FixedMul(y - y1, dx >> FRACBITS) >= FixedMul(dy >> FRACBITS, x - x1);
}

v2double_t B_ProjectionOnLine(double x, double y, double x1, double y1,
                              double dx, double dy);
v2fixed_t B_ProjectionOnLine(fixed_t x, fixed_t y, fixed_t x1, fixed_t y1,
                             fixed_t dx, fixed_t dy);
v2fixed_t B_ProjectionOnSegment(fixed_t x, fixed_t y, fixed_t x1, fixed_t y1,
    fixed_t dx, fixed_t dy, fixed_t padding = 0);
v2fixed_t B_ProjectionOnLine_f(fixed_t x, fixed_t y, fixed_t x1, fixed_t y1,
   fixed_t dx, fixed_t dy);
void B_GetMapBounds(fixed_t &minx, fixed_t &miny, fixed_t &maxx, fixed_t &maxy);
bool B_IntersectionPoint(const LineEq &l1, const LineEq &l2, double &ix,
                               double &iy);
int B_BoxOnLineSide(fixed_t top, fixed_t bottom, fixed_t left, fixed_t right,
                    fixed_t x1, fixed_t y1, fixed_t dx, fixed_t dy);
void B_EmptyTableAndDelete(MetaTable &meta);
bool B_SegmentsIntersect(fixed_t x11, fixed_t y11, fixed_t x12, fixed_t y12,
                         fixed_t x21, fixed_t y21, fixed_t x22, fixed_t y22);
double B_RatioAlongLine(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                        fixed_t x, fixed_t y);

template <typename T, typename U, typename V, typename W> bool B_SegmentsIntersect(const T &v11, const U &v12, const V &v21, const W &v22)
{
   return B_SegmentsIntersect(v11.x, v11.y, v12.x, v12.y, v21.x, v21.y, v22.x, v22.y);
}

inline static bool B_CheckAllocSize(int n)
{
    enum
    {
        // Maximum allowed alloc size: hopefully malloc won't crash with anything
        // less than this. Used when deserializing BotMap caches
        ARBITRARY_LARGE_VALUE = 1048576,
    };
    return n >= 0 && n <= ARBITRARY_LARGE_VALUE; 
}

inline static bool B_CheckNullablePtrRange(const void *p, size_t max)
{
   return (intptr_t)p == -1 || (uintptr_t)p < max;
}

template <typename T>
inline static bool B_ConvertPtrToArrayItem(const T *&p, const T *base,
                                           size_t max)
{
   if(!B_CheckNullablePtrRange(p, max))
      return false;
   if((intptr_t)p == -1)
      p = nullptr;
   else
      p = (T*)base + (intptr_t)p;
   return true;
}

// Just a copy without "const"
template <typename T>
inline static bool B_ConvertPtrToArrayItem(T *&p, const T *base,
                                           size_t max)
{
   if(!B_CheckNullablePtrRange(p, max))
      return false;
   if((intptr_t)p == -1)
      p = nullptr;
   else
      p = (T*)base + (intptr_t)p;
   return true;
}


template <typename T, typename U>
inline static bool B_ConvertPtrToCollItem(T *&p, const U &base, size_t max)
{
   if(!B_CheckNullablePtrRange(p, max))
      return false;
   if((intptr_t)p == -1)
      p = nullptr;
   else
      p = base[(intptr_t)p];
   return true;
}

//
// RandomGenerator
//
// Basic linear congruential, 31-bit bound. Also in AutoWolf
// Kept separate from M_RANDOM
//
class RandomGenerator
{
public:
   uint64_t state;
   int operator() ()
   {
      return (int)(state = state * 48271 % 0x7fffffff);
   }
   // From min to max inclusive
   int range(int min, int max)
   {
      return (this->operator()() % (max - min + 1)) + min;
   }
   void initialize(int inState)
   {
      state = (uint64_t)inState % 0x7fffffff;
   }
   // Sets it to the current day value
   void initializeByDay()
   {
      time_t rawtime;
      tm *timeinfo;
      time(&rawtime);
      timeinfo = localtime(&rawtime);
      state = timeinfo->tm_yday + 366 * timeinfo->tm_year;
   }
};

inline static bool B_IsWalkTeleportation(int special)
{
    VanillaLineSpecial vls = (VanillaLineSpecial)special;
    switch (vls)
    {
    case VLS_W1Teleport:
    case VLS_WRTeleport:
    case VLS_W1SilentTeleport:
    case VLS_WRSilentTeleport:
        return true;
    default:
        break;
    }
    return false;
}

#ifdef _DEBUG
void B_Log(const char *output, ...);
#else
#define B_Log(...)
#endif

//
// TIME MEASUREMENT
//

class TimeMeasurement
{
public:
    TimeMeasurement();
    void start();
    void end();
    double getAverage() const
    {
        return m_invalid ? NAN : m_totalRun / m_numRuns;
    }
private:
    long long m_frequency;
    long long m_startTime;

    double m_totalRun;
    long long m_numRuns;

    bool m_invalid;
};

#endif
// EOF

