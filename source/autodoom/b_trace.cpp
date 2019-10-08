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

bool BotMap::pathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                          std::function<bool(const Line&, const divline_t &, fixed_t)>
                          &&lineHit) const
{
   // Gotta copy all the code from other traversers

   VALID_CLEAR(validLines, numlines);

   // don't side exactly on a line
   if(!((x1 - bMapOrgX) & (BOTMAPBLOCKSIZE - 1)))
      x1 += FRACUNIT;
   if(!((y1 - bMapOrgY) & (BOTMAPBLOCKSIZE - 1)))
      y1 += FRACUNIT;

   divline_t dl;
   dl.x = x1;
   dl.y = y1;
   dl.dx = x2 - x1;
   dl.dy = y2 - y1;

   x1 -= bMapOrgX;
   y1 -= bMapOrgY;
   fixed_t xt1 = x1 >> BOTMAPBLOCKSHIFT;
   fixed_t yt1 = y1 >> BOTMAPBLOCKSHIFT;

   x2 -= bMapOrgX;
   y2 -= bMapOrgY;
   fixed_t xt2 = x2 >> BOTMAPBLOCKSHIFT;
   fixed_t yt2 = y2 >> BOTMAPBLOCKSHIFT;

   fixed_t mapxstep;
   fixed_t mapystep;
   fixed_t partialx;
   fixed_t partialy;
   fixed_t xstep;
   fixed_t ystep;
   fixed_t xintercept;
   fixed_t yintercept;
   if(xt2 > xt1)
   {
      mapxstep = 1;
      partialx = FRACUNIT - ((x1 >> BOTMAPBTOFRAC) & (FRACUNIT - 1));
      ystep = FixedDiv(y2 - y1, D_abs(x2 - x1));
   }
   else if(xt2 < xt1)
   {
      mapxstep = -1;
      partialx = (x1 >> BOTMAPBTOFRAC) & (FRACUNIT - 1);
      ystep = FixedDiv(y2 - y1, D_abs(x2 - x1));
   }
   else
   {
      mapxstep = 0;
      partialx = FRACUNIT;
      ystep = 256 * FRACUNIT;
   }

   yintercept = (y1 >> BOTMAPBTOFRAC) + FixedMul(partialx, ystep);

   if(yt2 > yt1)
   {
      mapystep = 1;
      partialy = FRACUNIT - ((y1 >> BOTMAPBTOFRAC) & (FRACUNIT - 1));
      xstep = FixedDiv(x2 - x1, D_abs(y2 - y1));
   }
   else if(yt2 < yt1)
   {
      mapystep = -1;
      partialy = (y1 >> BOTMAPBTOFRAC) & (FRACUNIT - 1);
      xstep = FixedDiv(x2 - x1, D_abs(y2 - y1));
   }
   else
   {
      mapystep = 0;
      partialy = FRACUNIT;
      xstep = 256 * FRACUNIT;
   }

   xintercept = (x1 >> BOTMAPBTOFRAC) + FixedMul(partialy, xstep);

   // From ZDoom (usable under ZDoom code license):
   // [RH] Fix for traces that pass only through blockmap corners. In that case,
   // xintercept and yintercept can both be set ahead of mapx and mapy, so the
   // for loop would never advance anywhere.

   if(D_abs(xstep) == FRACUNIT && D_abs(ystep) == FRACUNIT)
   {
      if(ystep < 0)
         partialx = FRACUNIT - partialx;
      if(ystep < 0)
         partialy = FRACUNIT - partialy;
      if(partialx == partialy)
      {
         xintercept = xt1 << FRACBITS;
         yintercept = yt1 << FRACBITS;
      }
   }

   // step through map blocks
   // Count is present to prevent a round off error from skipping the break
   fixed_t mapx = xt1;
   fixed_t mapy = yt1;

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
   ctx.trace = &dl;

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
      blockLinesIterator(mapx, mapy, lineHitFunc, &ctx);
      if((mapxstep | mapystep) == 0)
         break;
      // From ZDoom (usable under the ZDoom code license):
      // This is the fix for the "Anywhere Moo" bug, which caused monsters to
      // occasionally see the player through an arbitrary number of walls in
      // Doom 1.2, and persisted into Heretic, Hexen, and some versions of
      // ZDoom.
      switch((((yintercept >> FRACBITS) == mapy) << 1) |
             ((xintercept >> FRACBITS) == mapx))
      {
         case 0:
            // Neither xintercept nor yintercept match!
            // Continuing won't make things any better, so we might as well stop.
            count = 100;
            break;
         case 1:
            // xintercept matches
            xintercept += xstep;
            mapy += mapystep;
            if(mapy == yt2)
               mapystep = 0;
            break;
         case 2:
            // yintercept matches
            yintercept += ystep;
            mapx += mapxstep;
            if(mapx == xt2)
               mapxstep = 0;
            break;
         case 3:
            // xintercept and yintercept both match
            // The trace is exiting a block through its corner. Not only does the
            // block being entered need to be checked (which will happen when this
            // loop continues), but the other two blocks adjacent to the corner
            // also need to be checked.
            blockLinesIterator(mapx + mapxstep, mapy, lineHitFunc, &ctx);
            blockLinesIterator(mapx, mapy + mapystep, lineHitFunc, &ctx);
            xintercept += xstep;
            yintercept += ystep;
            mapx += mapxstep;
            mapy += mapystep;
            if(mapx == xt2)
               mapxstep = 0;
            if(mapy == yt2)
               mapystep = 0;
            break;
      }
   }

   // traverse intercepts
   size_t count;
   fixed_t dist;
   PODCollection<Intercept>::iterator scan, end, in;

   count = ctx.intercepts.getLength();
   end = ctx.intercepts.end();

   for(scan = ctx.intercepts.begin(); scan < end; ++scan)
   {
      divline_t mdl = divline_t::points(*scan->line->v[0], *scan->line->v[1]);
      scan->frac = P_InterceptVector(&dl, &mdl);
   }

   in = nullptr;

   while(count--)
   {
      dist = D_MAXINT;
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
         if(!lineHit(*in->line, dl, in->frac))
            return false;
         in->frac = D_MAXINT;
      }
   }

   return true;
}
