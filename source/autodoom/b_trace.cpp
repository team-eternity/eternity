// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2014 Ioan Chera
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
//      Version of traversing functions from P_, that use self-contained vars
//      Meant to prevent any global variable interference and possible desync
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"
#include "b_trace.h"
#include "../doomstat.h"
#include "../p_setup.h"
#include "../m_dllist.h"
#include "../polyobj.h"
#include "../r_state.h"

//
// RTraversal::Execute
//
// Code copied from p_trace/P_PathTraverse, adapted for class-local state
//
bool RTraversal::Execute(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags, bool (*trav)(intercept_t*))
{
   fixed_t xt1, yt1;
   fixed_t xt2, yt2;
   fixed_t xstep, ystep;
   fixed_t partial;
   fixed_t xintercept, yintercept;
   int     mapx, mapy;
   int     mapxstep, mapystep;
   int     count;
   
   ++m_validcount;
   m_intercept_p = m_intercepts;
   
   if(!((x1-bmaporgx)&(MAPBLOCKSIZE-1)))
      x1 += FRACUNIT;     // don't side exactly on a line
   
   if(!((y1-bmaporgy)&(MAPBLOCKSIZE-1)))
      y1 += FRACUNIT;     // don't side exactly on a line
   
   m_trace.dl.x = x1;
   m_trace.dl.y = y1;
   m_trace.dl.dx = x2 - x1;
   m_trace.dl.dy = y2 - y1;
   
   x1 -= bmaporgx;
   y1 -= bmaporgy;
   xt1 = x1 >> MAPBLOCKSHIFT;
   yt1 = y1 >> MAPBLOCKSHIFT;
   
   x2 -= bmaporgx;
   y2 -= bmaporgy;
   xt2 = x2 >> MAPBLOCKSHIFT;
   yt2 = y2 >> MAPBLOCKSHIFT;
   
   if(xt2 > xt1)
   {
      mapxstep = 1;
      partial = FRACUNIT - ((x1 >> MAPBTOFRAC) & (FRACUNIT-1));
      ystep = FixedDiv(y2 - y1, D_abs(x2 - x1));
   }
   else if(xt2 < xt1)
   {
      mapxstep = -1;
      partial = (x1 >> MAPBTOFRAC) & (FRACUNIT - 1);
      ystep = FixedDiv(y2 - y1, D_abs(x2 - x1));
   }
   else
   {
      mapxstep = 0;
      partial = FRACUNIT;
      ystep = 256 * FRACUNIT;
   }
   
   yintercept = (y1 >> MAPBTOFRAC) + FixedMul(partial, ystep);
   
   if(yt2 > yt1)
   {
      mapystep = 1;
      partial = FRACUNIT - ((y1 >> MAPBTOFRAC) & (FRACUNIT - 1));
      xstep = FixedDiv(x2 - x1, D_abs(y2 - y1));
   }
   else if(yt2 < yt1)
   {
      mapystep = -1;
      partial = (y1 >> MAPBTOFRAC) & (FRACUNIT - 1);
      xstep = FixedDiv(x2 - x1, D_abs(y2 - y1));
   }
   else
   {
      mapystep = 0;
      partial = FRACUNIT;
      xstep = 256 * FRACUNIT;
   }
   
   xintercept = (x1 >> MAPBTOFRAC) + FixedMul(partial, xstep);
   
   // Step through map blocks.
   // Count is present to prevent a round off error
   // from skipping the break.
   
   mapx = xt1;
   mapy = yt1;

   for(count = 0; count < 64; count++)
   {
      if(flags & PT_ADDLINES)
      {
         if(!blockLinesIterator(mapx, mapy, [this](line_t* ld){
            return addLineIntercepts(ld);
         }))
            return false; // early out
      }
      
      if(flags & PT_ADDTHINGS)
      {
         if(!blockThingsIterator(mapx, mapy, [this](Mobj* thing){
            return addThingIntercepts(thing);
         }))
            return false; // early out
      }
      
      if(mapx == xt2 && mapy == yt2)
         break;
      
      if((yintercept >> FRACBITS) == mapy)
      {
         yintercept += ystep;
         mapx += mapxstep;
      }
      else if((xintercept >> FRACBITS) == mapx)
      {
         xintercept += xstep;
         mapy += mapystep;
      }
   }
   
   // go through the sorted list
   // SoM: just store this for a sec
   
   return traverseIntercepts(trav, FRACUNIT);
}

bool RTraversal::traverseIntercepts(traverser_t func, fixed_t maxfrac)
{
   intercept_t *in = nullptr;
   int count = static_cast<int>(m_intercept_p - m_intercepts);
   while(count--)
   {
      fixed_t dist = D_MAXINT;
      intercept_t *scan;
      for(scan = m_intercepts; scan < m_intercept_p; scan++)
         if(scan->frac < dist)
            dist = (in = scan)->frac;
      if(dist > maxfrac)
         return true;    // checked everything in range
      
      if(in) // haleyjd: for safety
      {
         if(!func(in))
            return false;           // don't bother going farther
         in->frac = D_MAXINT;
      }
   }
   return true;                  // everything was traversed
}

//
// RTraversal::blockLinesIterator
//
// Pretty much from P_Trace, except with safe calls
//
bool RTraversal::blockLinesIterator(int x, int y, const std::function<bool(line_t*)>& func)
{
   int        offset;
   const int  *list;     // killough 3/1/98: for removal of blockmap limit
   const DLListItem<polymaplink_t> *plink; // haleyjd 02/22/06
   
   if(x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight)
      return true;
   offset = y * bmapwidth + x;
   
   // haleyjd 02/22/06: consider polyobject lines
   plink = polyblocklinks[offset];

   while(plink)
   {
      const polyobj_t *po = (*plink)->po;
      
      if(m_polyValidcount[po] != m_validcount) // if polyobj hasn't been checked
      {
         int i;
         m_polyValidcount[po] = m_validcount;
         
         for(i = 0; i < po->numLines; ++i)
         {
            if(m_lineValidcount[po->lines[i] - lines] == m_validcount) // line has been checked
               continue;
            m_lineValidcount[po->lines[i] - lines] = m_validcount;
            if(!func(po->lines[i]))
               return false;
         }
      }
      plink = plink->dllNext;
   }
   
   // original was reading delimiting 0 as linedef 0 -- phares
   offset = *(blockmap + offset);
   list = blockmaplump + offset;
   
   // killough 1/31/98: for compatibility we need to use the old method.
   // Most demos go out of sync, and maybe other problems happen, if we
   // don't consider linedef 0. For safety this should be qualified.
   
   // killough 2/22/98: demo_compatibility check
   // skip 0 starting delimiter -- phares
   if(!demo_compatibility)
      list++;
   for( ; *list != -1; list++)
   {
      line_t *ld;
      
      // haleyjd 04/06/10: to avoid some crashes during demo playback due to
      // invalid blockmap lumps
      if(*list >= numlines)
         continue;
      
      ld = &lines[*list];
      if(m_lineValidcount[*list] == m_validcount)
         continue;       // line has already been checked
      m_lineValidcount[*list] = m_validcount;
      if(!func(ld))
         return false;
   }
   return true;  // everything was checked
}

//
// RTraversal::blockThingsIterator
//
// This really doesn't look different from P_BlockThingsIterator
//
bool RTraversal::blockThingsIterator(int x, int y, const std::function<bool(Mobj*)>& func)
{
   if(!(x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight))
   {
      Mobj *mobj = blocklinks[y * bmapwidth + x];
      
      for(; mobj; mobj = mobj->bnext)
         if(!func(mobj))
            return false;
   }
   return true;
}

//
// RTraversal::addLineIntercepts
//
bool RTraversal::addLineIntercepts(line_t* ld)
{
   int       s1;
   int       s2;
   fixed_t   frac;
   divline_t dl;
   
   // avoid precision problems with two routines
   if(m_trace.dl.dx >  FRACUNIT*16 || m_trace.dl.dy >  FRACUNIT*16 ||
      m_trace.dl.dx < -FRACUNIT*16 || m_trace.dl.dy < -FRACUNIT*16)
   {
      s1 = P_PointOnDivlineSide (ld->v1->x, ld->v1->y, &m_trace.dl);
      s2 = P_PointOnDivlineSide (ld->v2->x, ld->v2->y, &m_trace.dl);
   }
   else
   {
      s1 = P_PointOnLineSide (m_trace.dl.x, m_trace.dl.y, ld);
      s2 = P_PointOnLineSide (m_trace.dl.x + m_trace.dl.dx, m_trace.dl.y + m_trace.dl.dy, ld);
   }
   
   if(s1 == s2)
      return true;        // line isn't crossed
   
   // hit the line
   P_MakeDivline(ld, &dl);
   frac = P_InterceptVector(&m_trace.dl, &dl);
   
   if(frac < 0)
      return true;        // behind source
   
   checkIntercept();    // killough
   
   m_intercept_p->frac = frac;
   m_intercept_p->isaline = true;
   m_intercept_p->d.line = ld;
   m_intercept_p++;
   
   return true;  // continue
}

bool RTraversal::addThingIntercepts(Mobj *thing)
{
   fixed_t x1, y1;
   fixed_t x2, y2;
   int s1, s2;
   divline_t dl;
   fixed_t frac;
   
   if((m_trace.dl.dx ^ m_trace.dl.dy) > 0)
   {
      x1 = thing->x - thing->radius;
      y1 = thing->y + thing->radius;
      x2 = thing->x + thing->radius;
      y2 = thing->y - thing->radius;
   }
   else
   {
      x1 = thing->x - thing->radius;
      y1 = thing->y - thing->radius;
      x2 = thing->x + thing->radius;
      y2 = thing->y + thing->radius;
   }
   
   s1 = P_PointOnDivlineSide (x1, y1, &m_trace.dl);
   s2 = P_PointOnDivlineSide (x2, y2, &m_trace.dl);
   
   if(s1 == s2)
      return true;
   
   dl.x = x1;
   dl.y = y1;
   dl.dx = x2-x1;
   dl.dy = y2-y1;
   
   frac = P_InterceptVector (&m_trace.dl, &dl);
   
   if (frac < 0)
      return true;                // behind source
   
   checkIntercept();
   
   m_intercept_p->frac = frac;
   m_intercept_p->isaline = false;
   m_intercept_p->d.thing = thing;
   m_intercept_p++;
   
   return true;          // keep going
}

void RTraversal::checkIntercept()
{
   size_t offset = m_intercept_p - m_intercepts;
   if(offset >= m_num_intercepts)
   {
      m_num_intercepts = m_num_intercepts ? m_num_intercepts * 2 : 128;
      m_intercepts = erealloc(intercept_t *, m_intercepts, sizeof(*m_intercepts) * m_num_intercepts);
      m_intercept_p = m_intercepts + offset;
   }
   
   
}

//
// RTraversal::RTraversal
//
// Constructor
//
// IMPORTANT NOTE: upon construction, it will build an array the size of
// numlines. And it will not update after P_SetupLevel. So it's best if the
// lifetime of such an object is the same as the level.
//
RTraversal::RTraversal() :
m_trace(),
m_validcount(0),
m_intercepts(nullptr),
m_intercept_p(nullptr),
m_num_intercepts(0)
{
   m_lineValidcount = ecalloc(int*, numlines, sizeof(int));
}
