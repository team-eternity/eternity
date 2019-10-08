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
#include "b_botmap.h"
#include "../doomstat.h"
#include "../e_exdata.h"
#include "../p_mobj.h"
#include "../p_setup.h"
#include "../m_dllist.h"
#include "../polyobj.h"
#include "../r_data.h"
#include "../r_portal.h"
#include "../r_state.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// BotMap stuff
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool BotMap::blockLinesIterator(int x, int y,
                                bool lineHit(const Line &, void *),
                                void *context) const
{
   if(x < 0 || y < 0 || x >= bMapWidth || y >= bMapHeight)
      return true;
   int offset = y * bMapWidth + x;

   const PODCollection<Line *> &coll = lineBlocks[offset];
   for(const Line *line : coll)
   {
      // TODO: linked portals
      size_t index = line - lines;
      if(VALID_ISSET(validLines, index))
         continue;
      VALID_SET(validLines, index);
      if(!lineHit(*line, context))
         return false;
   }

   return true;
}

//                                                     //
//                                                     //
// !!! WARNING !!! WARNING !!! WARNING !!! WARNING !!! //
//                                                     //
//  Some code here is copy-pasted from CAM_Sight.cpp,  //
// PathTraverser::traverse.  Such code stays under the //
//  ZDoom Source Distribution License.  More exactly,  //
//  it's the code with [RH] markings in the comments.  //
//                                                     //
//                                                     //

bool BotMap::pathTraverse(divline_t trace,
                          const std::function<bool(const Line&, const divline_t &, fixed_t)> &lineHit) const
{
   // Gotta copy all the code from other traversers

   VALID_CLEAR(validLines, numlines);

   // don't side exactly on a line
   if(!((trace.x - bMapOrgX) & (BOTMAPBLOCKSIZE - 1)))
   {
      trace.x += FRACUNIT;
      trace.dx -= FRACUNIT;
   }
   if(!((trace.y - bMapOrgY) & (BOTMAPBLOCKSIZE - 1)))
   {
      trace.y += FRACUNIT;
      trace.dy -= FRACUNIT;
   }

   divline_t orgtrace = trace;

   trace.v -= v2fixed_t{ bMapOrgX, bMapOrgY };
   v2fixed_t vt1 = trace.v >> BOTMAPBLOCKSHIFT;
   v2fixed_t vt2 = (trace.v + trace.dv) >> BOTMAPBLOCKSHIFT;

   v2fixed_t mapstep;
   v2fixed_t partial;
   v2fixed_t step;
   v2fixed_t intercept;
   if(vt2.x > vt1.x)
   {
      mapstep.x = 1;
      partial.x = FRACUNIT - ((trace.x >> BOTMAPBTOFRAC) & (FRACUNIT - 1));
      step.y = FixedDiv(trace.dy, D_abs(trace.dx));
   }
   else if(vt2.x < vt1.x)
   {
      mapstep.x = -1;
      partial.x = (trace.x >> BOTMAPBTOFRAC) & (FRACUNIT - 1);
      step.y = FixedDiv(trace.dy, D_abs(trace.dx));
   }
   else
   {
      mapstep.x = 0;
      partial.x = FRACUNIT;
      step.y = 256 * FRACUNIT;
   }

   intercept.y = (trace.y >> BOTMAPBTOFRAC) + FixedMul(partial.x, step.y);

   if(vt2.y > vt1.y)
   {
      mapstep.y = 1;
      partial.y = FRACUNIT - ((trace.y >> BOTMAPBTOFRAC) & (FRACUNIT - 1));
      step.x = FixedDiv(trace.dx, D_abs(trace.dy));
   }
   else if(vt2.y < vt1.y)
   {
      mapstep.y = -1;
      partial.y = (trace.y >> BOTMAPBTOFRAC) & (FRACUNIT - 1);
      step.x = FixedDiv(trace.dx, D_abs(trace.dy));
   }
   else
   {
      mapstep.y = 0;
      partial.y = FRACUNIT;
      step.x = 256 * FRACUNIT;
   }

   intercept.x = (trace.x >> BOTMAPBTOFRAC) + FixedMul(partial.y, step.x);

   // From ZDoom (usable under ZDoom code license):
   // [RH] Fix for traces that pass only through blockmap corners. In that case,
   // xintercept and yintercept can both be set ahead of mapx and mapy, so the
   // for loop would never advance anywhere.

   if(step.elemabs() == v2fixed_t(FRACUNIT, FRACUNIT))
   {
      if(step.y < 0)
         partial.x = FRACUNIT - partial.x;
      if(step.x < 0)
         partial.y = FRACUNIT - partial.y;
      if(partial.x == partial.y)
         intercept = vt1 << FRACBITS;
   }

   // step through map blocks
   // Count is present to prevent a round off error from skipping the break
   v2fixed_t map = vt1;

   struct Intercept
   {
      fixed_t frac;
      const Line *line;
   };

   struct Context
   {
      divline_t *trace;
      PODCollection<Intercept> intercepts;
   } ctx;
   ctx.trace = &orgtrace;

   static auto lineHitFunc = [](const Line &line, void *vcontext) {
      auto ctx = (Context *)vcontext;
      int s1 = P_PointOnDivlineSide(line.v[0]->x, line.v[0]->y, ctx->trace);
      int s2 = P_PointOnDivlineSide(line.v[1]->x, line.v[1]->y, ctx->trace);
      if(s1 == s2)
         return true;   // seg not crossed
      divline_t dl = divline_t::points(*line.v[0], *line.v[1]);
      s1 = P_PointOnDivlineSide(ctx->trace->x, ctx->trace->y, &dl);
      s2 = P_PointOnDivlineSide(ctx->trace->x + ctx->trace->dx,
                                ctx->trace->y + ctx->trace->dy, &dl);
      if(s1 == s2)
         return true;   // seg not crossed

      ctx->intercepts.addNew().line = &line;

      return true;
   };

   for(int count = 0; count < 100; ++count)
   {
      blockLinesIterator(map.x, map.y, lineHitFunc, &ctx);
      if((mapstep.x | mapstep.y) == 0)
         break;
      // From ZDoom (usable under the ZDoom code license):
      // This is the fix for the "Anywhere Moo" bug, which caused monsters to
      // occasionally see the player through an arbitrary number of walls in
      // Doom 1.2, and persisted into Heretic, Hexen, and some versions of
      // ZDoom.
      switch((((intercept.y >> FRACBITS) == map.y) << 1) |
             ((intercept.x >> FRACBITS) == map.x))
      {
         case 0:
            // Neither xintercept nor yintercept match!
            // Continuing won't make things any better, so we might as well stop.
            count = 100;
            break;
         case 1:
            // xintercept matches
            intercept.x += step.x;
            map.y += mapstep.y;
            if(map.y == vt2.y)
               mapstep.y = 0;
            break;
         case 2:
            // yintercept matches
            intercept.y += step.y;
            map.x += mapstep.x;
            if(map.x == vt2.x)
               mapstep.x = 0;
            break;
         case 3:
            // xintercept and yintercept both match
            // The trace is exiting a block through its corner. Not only does the
            // block being entered need to be checked (which will happen when this
            // loop continues), but the other two blocks adjacent to the corner
            // also need to be checked.
            blockLinesIterator(map.x + mapstep.x, map.y, lineHitFunc, &ctx);
            blockLinesIterator(map.x, map.y + mapstep.y, lineHitFunc, &ctx);
            intercept += step;
            map += mapstep;
            if(map.x == vt2.x)
               mapstep.x = 0;
            if(map.y == vt2.y)
               mapstep.y = 0;
            break;
      }
   }

   // traverse intercepts
   PODCollection<Intercept>::iterator scan, end, in;

   size_t count = ctx.intercepts.getLength();
   end = ctx.intercepts.end();

   for(scan = ctx.intercepts.begin(); scan < end; ++scan)
   {
      divline_t mdl = divline_t::points(*scan->line->v[0], *scan->line->v[1]);
      scan->frac = P_InterceptVector(&orgtrace, &mdl);
   }

   in = nullptr;

   while(count--)
   {
      fixed_t dist = D_MAXINT;
      for(scan = ctx.intercepts.begin(); scan < end; ++scan)
      {
         if(scan->frac < dist)
         {
            dist = scan->frac;
            in = scan;
         }
      }

      if(in)
      {
         if(!lineHit(*in->line, orgtrace, in->frac))
            return false;
         in->frac = D_MAXINT;
      }
   }

   return true;
}
