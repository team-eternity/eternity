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
// B_CoordXY
//
// Gets the .x and .y coordinates of generic class into v2fixed_t
//
template <typename T> inline static v2fixed_t B_CoordXY(const T &misc)
{
   v2fixed_t ret;
   ret.x = misc.x;
   ret.y = misc.y;
   return ret;
}

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
    fixed_t dx, fixed_t dy);
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

template <typename T, typename U, typename V, typename W> bool B_SegmentsIntersect(const T &v11, const U &v12, const V &v21, const W &v22)
{
   return B_SegmentsIntersect(v11.x, v11.y, v12.x, v12.y, v21.x, v21.y, v22.x, v22.y);
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

//
// Bipartite
//
// Template class that connects two classes together
//
template <typename A, typename B> class Bipartite : public ZoneObject
{
public:
   struct Link
   {
      A* ref1;
      B* ref2;
      DLListItem<A> link1;
      DLListItem<B> link2;
   };
   
private:
   std::unordered_map<A *, DLList<Link, &Link::link1> > map1;
   std::unordered_map<B *, DLList<Link, &Link::link1> > map2;
   
public:
   Link *getList1(A *obj)
   {
      if (map1.count(obj))
         return map1.at(obj);
   }
   Link *getList2(B *obj)
   {
      if (map2.count(obj))
         return map2.at(obj);
   }
   Link *addLink(A *obj1, B *obj2)
   {
      if(map1.count(obj1))
      {
         Link *l1 = map1.at(obj1);
         while(l1)
         {
            if(l1->ref2 == obj2)
               return l1;
            l1 = l1->link1.dllNext->dllObject;
         }
      }
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

template<typename T> class List
{
    struct Link
    {
        T       obj;
        int     next;
        int     prev;
    };

    
    PODCollection<Link> pool;
    int base;
    int count;
    int iter;

public:
    List() : base(-1), count(0), iter(0)
    {
    }

    void Add(T item) // adds to front
    {
        Link& L = pool.addNew();
        L.obj = item;
        L.next = base;
        L.prev = -1;
        base = pool.getLength() - 1;
        if (L.next >= 0)
            pool[L.next].prev = base;
        
        ++count;
    }

    T GetFirst()
    {
        return pool[iter = base].obj;
    }

    bool HasNext() const
    {
        return iter >= 0 ? pool[iter].next >= 0 : base >= 0;
    }

    T GetNext()
    {
        return pool[iter = iter >= 0 ? pool[iter].next : base].obj;
    }

    int Count() const
    {
        return count;
    }

    bool Empty() const
    {
        return count == 0;
    }

    void RemoveCurrent()
    {
        if (base == iter)
            base = pool[iter].next;

        Link& L = pool[iter];
        int bup = L.prev;
        if (L.prev >= 0)
            pool[L.prev].next = L.next;
        if (L.next >= 0)
            pool[L.next].prev = L.prev;

        Link& F = pool.back();
        if (F.prev >= 0)
            pool[F.prev].next = iter;
        if (F.next >= 0)
            pool[F.next].prev = iter;
        pool[iter] = F;
        pool.justPop();
        --count;

        iter = bup; // go back so we can advance again
    }

    void Clear()
    {
        base = -1;
        count = 0;
        iter = 0;
        pool.template makeEmpty<true>();
    }
};

#endif
// EOF

