//
//
// The Eternity Engine
// Copyright (C) 2018 James Haley, Ioan Chera, et al.
//
// ZDoom
// Copyright (C) 1998-2012 Marisa Heit
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
// Purpose: Reentrant path traversing, used by several new functions
// Authors: James Haley, Ioan Chera, Max Waine
//

#include "z_zone.h"

#include "cam_common.h"
#include "cam_sight.h"
#include "d_gi.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "m_compare.h"
#include "p_portal.h"
#include "p_portalblockmap.h"
#include "p_setup.h"
#include "polyobj.h"
#include "r_portal.h"
#include "r_state.h"

#define VALID_ISSET(set, i) ((set)[(i) >> 3] & (1 << ((i) & 7)))
#define VALID_SET(set, i) ((set)[(i) >> 3] |= 1 << ((i) & 7))

//
// Constructor. Initializes dynamic structures
//
PathTraverser::PathTraverser(const PTDef &indef, void *incontext) :
   trace(), def(indef), context(incontext), portalguard()
{
   VALID_ALLOC(validlines, ::numlines);
   VALID_ALLOC(validpolys, ::numPolyObjects);
}



//
// Handles intercepts in order
//
bool PathTraverser::traverseIntercepts() const
{
   size_t    count;
   fixed_t   dist;
   divline_t dl;
   PODCollection<intercept_t>::iterator scan, end, in;

   count = intercepts.getLength();
   end = intercepts.end();

   //
   // calculate intercept distance
   //
   for(scan = intercepts.begin(); scan < end; scan++)
   {
      if(!scan->isaline)
         continue;   // ioanch 20151230: only lines need this treatment
      P_MakeDivline(scan->d.line, &dl);
      if(scan->frac = P_InterceptVector(&trace, &dl); scan->frac < 0)
         scan->frac = D_MAXINT;
   }

   //
   // go through in order
   //	
   in = nullptr; // shut up compiler warning

   while(count--)
   {
      dist = D_MAXINT;

      for(scan = intercepts.begin(); scan < end; scan++)
      {
         if(scan->frac < dist)
         {
            dist = scan->frac;
            in = scan;
         }
      }

      if(in)
      {
         if(!def.trav(in, context, trace))
            return false; // don't bother going farther

         in->frac = D_MAXINT;
      }
   }

   return true; // everything was traversed
}

//
// Iterates through things
//
bool PathTraverser::blockThingsIterator(int x, int y)
{
   if(def.earlyOut != PTDef::eo_always &&
      (x < 0 || y < 0 || x >= ::bmapwidth || y >= ::bmapheight))
   {
      return true;
   }

   Mobj *thing = blocklinks[y * bmapwidth + x];
   for(; thing; thing = thing->bnext)
   {

      fixed_t   x1, y1;
      fixed_t   x2, y2;
      int       s1, s2;
      divline_t dl;
      fixed_t   frac;

      // check a corner to corner crossection for hit
      if((trace.dx ^ trace.dy) > 0)
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

      s1 = P_PointOnDivlineSidePrecise(x1, y1, &trace);
      s2 = P_PointOnDivlineSidePrecise(x2, y2, &trace);

      if(s1 == s2)
         continue;

      dl.x = x1;
      dl.y = y1;
      dl.dx = x2 - x1;
      dl.dy = y2 - y1;

      frac = P_InterceptVector(&trace, &dl);

      if(frac < 0)
         continue;                // behind source

      intercept_t &inter = intercepts.addNew();
      inter.frac = frac;
      inter.isaline = false;
      inter.d.thing = thing;
   }
   return true;
}

//
// Checks if a line is valid and inserts it
//
bool PathTraverser::checkLine(size_t linenum)
{
   line_t *ld = &lines[linenum];
   int s1, s2;
   divline_t dl;

   VALID_SET(validlines, linenum);

   if(def.flags & CAM_REQUIRELINEPORTALS && !(ld->pflags & PS_PASSABLE))
      return true;

   s1 = P_PointOnDivlineSidePrecise(ld->v1->x, ld->v1->y, &trace);
   s2 = P_PointOnDivlineSidePrecise(ld->v2->x, ld->v2->y, &trace);
   if(s1 == s2)
      return true; // line isn't crossed

   P_MakeDivline(ld, &dl);
   s1 = P_PointOnDivlineSidePrecise(trace.x, trace.y, &dl);
   s2 = P_PointOnDivlineSidePrecise(trace.x + trace.dx,
      trace.y + trace.dy, &dl);
   if(s1 == s2)
      return true; // line isn't crossed

                   // Early outs are only possible if we haven't crossed a portal block
                   // ioanch 20151230: aim cams never quit
   if(def.earlyOut != PTDef::eo_no && !portalguard.hitpblock)
   {
      // try to early out the check
      if(!ld->backsector)
         return false; // stop checking

                       // haleyjd: block-all lines block sight
      if(ld->extflags & EX_ML_BLOCKALL)
         return false; // can't see through it
   }

   // store the line for later intersection testing
   intercept_t &inter = intercepts.addNew();
   inter.isaline = true;
   inter.d.line = ld;

   // if this is a passable portal line, remember we just added it
   // ioanch 20151229: also check sectors
   const sector_t *fsec = ld->frontsector, *bsec = ld->backsector;
   if(ld->pflags & PS_PASSABLE
      || (fsec && (fsec->srf.ceiling.pflags & PS_PASSABLE || fsec->srf.floor.pflags & PS_PASSABLE))
      || (bsec && (bsec->srf.ceiling.pflags & PS_PASSABLE || bsec->srf.floor.pflags & PS_PASSABLE)))
   {
      portalguard.addedportal = true;
   }

   return true;
}

//
// Iterates through lines reentrantly
//
bool PathTraverser::blockLinesIterator(int x, int y)
{
   if(def.earlyOut != PTDef::eo_always &&
      (x < 0 || y < 0 || x >= ::bmapwidth || y >= ::bmapheight))
   {
      return true;
   }

   int  offset;
   int *list;
   DLListItem<polymaplink_t> *plink;

   offset = y * bmapwidth + x;

   // haleyjd 05/17/13: check portalmap; once we enter a cell with
   // a line portal in it, we can't short-circuit any further and must
   // build a full intercepts list.
   // ioanch 20151229: don't just check for line portals, also consider 
   // floor/ceiling
   if(P_BlockHasLinkedPortals(offset, true))
      portalguard.hitpblock = true;

   // Check polyobjects first
   // haleyjd 02/22/06: consider polyobject lines
   plink = polyblocklinks[offset];

   while(plink)
   {
      polyobj_t *po = (*plink)->po;
      int polynum = eindex(po - PolyObjects);

      // if polyobj hasn't been checked
      if(!VALID_ISSET(validpolys, polynum))
      {
         VALID_SET(validpolys, polynum);

         for(int i = 0; i < po->numLines; ++i)
         {
            int linenum = eindex(po->lines[i] - lines);

            if(VALID_ISSET(validlines, linenum))
               continue; // line has already been checked

            if(!checkLine(po->lines[i] - ::lines))
               return false;
         }
      }
      plink = plink->dllNext;
   }


   offset = *(blockmap + offset);
   list = blockmaplump + offset;

   // skip 0 starting delimiter
   // MaxW: 2016/02/02: if before 3.42 always skip, skip if all blocklists start w/ 0
   // killough 2/22/98: demo_compatibility check
   // skip 0 starting delimiter -- phares
   if((!demo_compatibility && demo_version < 342) || (demo_version >= 342 && skipblstart))
      ++list;

   for(; *list != -1; list++)
   {
      int linenum = *list;

      if(linenum >= numlines)
         continue;

      if(VALID_ISSET(validlines, linenum))
         continue; // line has already been checked

      if(!checkLine(linenum))
         return false;
   }

   // haleyjd 08/20/13: optimization
   // we can go back to short-circuiting in future blockmap cells if we haven't 
   // actually intercepted any portal lines yet.
   if(!portalguard.addedportal)
      portalguard.hitpblock = false;

   return true; // everything was checked
}

//
// Does exploration from start to end
//
bool PathTraverser::traverse(fixed_t cx, fixed_t cy, fixed_t tx, fixed_t ty)
{
   fixed_t xt1, yt1, xt2, yt2;
   fixed_t xstep, ystep;
   fixed_t partialx, partialy;
   fixed_t xintercept, yintercept;
   int     mapx, mapy, mapxstep, mapystep;

   if(((cx - bmaporgx) & (MAPBLOCKSIZE - 1)) == 0)
      cx += FRACUNIT; // don't side exactly on a line

   if(((cy - bmaporgy) & (MAPBLOCKSIZE - 1)) == 0)
      cy += FRACUNIT; // don't side exactly on a line

   trace.x = cx;
   trace.y = cy;
   trace.dx = tx - cx;
   trace.dy = ty - cy;

   cx -= bmaporgx;
   cy -= bmaporgy;
   xt1 = cx >> MAPBLOCKSHIFT;
   yt1 = cy >> MAPBLOCKSHIFT;

   tx -= bmaporgx;
   ty -= bmaporgy;
   xt2 = tx >> MAPBLOCKSHIFT;
   yt2 = ty >> MAPBLOCKSHIFT;

   // points should never be out of bounds, but check once instead of
   // each block
   // ioanch 20151231: only for sight
   if(def.earlyOut == PTDef::eo_always &&
      (xt1 < 0 || yt1 < 0 || xt1 >= bmapwidth || yt1 >= bmapheight ||
         xt2 < 0 || yt2 < 0 || xt2 >= bmapwidth || yt2 >= bmapheight))
   {
      return false;
   }

   if(xt2 > xt1)
   {
      mapxstep = 1;
      partialx = FRACUNIT - ((cx >> MAPBTOFRAC) & (FRACUNIT - 1));
      ystep = FixedDiv(ty - cy, abs(tx - cx));
   }
   else if(xt2 < xt1)
   {
      mapxstep = -1;
      partialx = (cx >> MAPBTOFRAC) & (FRACUNIT - 1);
      ystep = FixedDiv(ty - cy, abs(tx - cx));
   }
   else
   {
      mapxstep = 0;
      partialx = FRACUNIT;
      ystep = 256 * FRACUNIT;
   }

   yintercept = (cy >> MAPBTOFRAC) + FixedMul(partialx, ystep);

   if(yt2 > yt1)
   {
      mapystep = 1;
      partialy = FRACUNIT - ((cy >> MAPBTOFRAC) & (FRACUNIT - 1));
      xstep = FixedDiv(tx - cx, abs(ty - cy));
   }
   else if(yt2 < yt1)
   {
      mapystep = -1;
      partialy = (cy >> MAPBTOFRAC) & (FRACUNIT - 1);
      xstep = FixedDiv(tx - cx, abs(ty - cy));
   }
   else
   {
      mapystep = 0;
      partialy = FRACUNIT;
      xstep = 256 * FRACUNIT;
   }

   xintercept = (cx >> MAPBTOFRAC) + FixedMul(partialy, xstep);

   // From ZDoom (usable under the GPLv3):
   // [RH] Fix for traces that pass only through blockmap corners. In that case,
   // xintercept and yintercept can both be set ahead of mapx and mapy, so the
   // for loop would never advance anywhere.

   if(!vanilla_heretic && abs(xstep) == FRACUNIT && abs(ystep) == FRACUNIT)
   {
      if(ystep < 0)
         partialx = FRACUNIT - partialx;
      if(xstep < 0)
         partialy = FRACUNIT - partialy;
      if(partialx == partialy)
      {
         xintercept = xt1 << FRACBITS;
         yintercept = yt1 << FRACBITS;
      }
   }

   // step through map blocks
   // Count is present to prevent a round off error from skipping the break
   mapx = xt1;
   mapy = yt1;

   for(int count = 0; count < 100; count++)
   {
      // if a flag is set, only accept blocks with line portals (needed for
      // some function in the code)
      if(!(def.flags & CAM_REQUIRELINEPORTALS) ||
         P_BlockHasLinkedPortals(mapy * ::bmapwidth + mapx, false))
      {
         if(def.flags & CAM_ADDLINES && !blockLinesIterator(mapx, mapy))
            return false;	// early out (ioanch: not for aim)
         if(def.flags & CAM_ADDTHINGS && !blockThingsIterator(mapx, mapy))
            return false;  // ioanch 20151230: aim also looks for a thing
      }

      if((mapxstep | mapystep) == 0)
         break;

      if(vanilla_heretic)
      {
         // vanilla Heretic demo compatibility
         if(mapx == xt2 && mapy == yt2)
            break;
         // Original code - this fails to account for all cases.
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
         continue;
      }

      // From ZDoom (usable under the GPLv3):
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
         if(!(def.flags & CAM_REQUIRELINEPORTALS) ||
            P_BlockHasLinkedPortals(mapy * ::bmapwidth + mapx, false))
         {
            if(def.flags & CAM_ADDLINES
               && (!blockLinesIterator(mapx + mapxstep, mapy)
                  || !blockLinesIterator(mapx, mapy + mapystep)))
            {
               return false;
            }
            // ioanch 20151230: autoaim support
            if(def.flags & CAM_ADDTHINGS
               && (!blockThingsIterator(mapx + mapxstep, mapy)
                  || !blockThingsIterator(mapx, mapy + mapystep)))
            {
               return false;
            }
         }
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

   //
   // couldn't early out, so go through the sorted list
   //
   return traverseIntercepts();
}

//
// Stores data into a LineOpening struct
//
void tracelineopening_t::calculate(const line_t *linedef)
{
   if(linedef->sidenum[1] == -1)
   {
      openrange = 0;
      return;
   }

   const sector_t *front = linedef->frontsector;
   const sector_t *back = linedef->backsector;

   const sector_t *beyond = linedef->intflags & MLI_1SPORTALLINE &&
      linedef->beyondportalline ? linedef->beyondportalline->frontsector : nullptr;
   if(beyond)
      back = beyond;

   // no need to apply the portal hack (1024 units) here fortunately
   if(linedef->extflags & EX_ML_UPPERPORTAL && back->srf.ceiling.pflags & PS_PASSABLE)
      open.ceiling = front->srf.ceiling.height;
   else
      open.ceiling = emin(front->srf.ceiling.height, back->srf.ceiling.height);

   if(linedef->extflags & EX_ML_LOWERPORTAL && back->srf.floor.pflags & PS_PASSABLE)
      open.floor = front->srf.floor.height;
   else
      open.floor = emax(front->srf.floor.height, back->srf.floor.height);
   openrange = open.ceiling - open.floor;
}

//
// Calculates opening for a given zero-volume point. Needed for slopes.
//
void tracelineopening_t::calculateAtPoint(const line_t &line, v2fixed_t pos)
{
   if(line.sidenum[1] == -1)
   {
      openrange = 0;
      return;
   }

   const sector_t *front = line.frontsector;
   const sector_t *back = line.backsector;

   const sector_t *beyond = line.intflags & MLI_1SPORTALLINE && line.beyondportalline ?
      line.beyondportalline->frontsector : nullptr;
   if(beyond)
      back = beyond;

   if(line.extflags & EX_ML_UPPERPORTAL && back->srf.ceiling.pflags & PS_PASSABLE)
      open.ceiling = front->srf.ceiling.getZAt(pos);
   else
      open.ceiling = emin(front->srf.ceiling.getZAt(pos), back->srf.ceiling.getZAt(pos));

   if(line.extflags & EX_ML_LOWERPORTAL && back->srf.floor.pflags & PS_PASSABLE)
      open.floor = front->srf.floor.getZAt(pos);
   else
      open.floor = emax(front->srf.floor.getZAt(pos), back->srf.floor.getZAt(pos));
   openrange = open.ceiling - open.floor;
}

//
// CAM_PathTraverse
//
// Public wrapper for PathTraverser
//
bool CAM_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
   uint32_t flags, void *data,
   bool(*trav)(const intercept_t *in, void *data,
      const divline_t &trace))
{
   PTDef def;
   def.flags = flags;
   def.earlyOut = PTDef::eo_no;
   def.trav = trav;
   return PathTraverser(def, data).traverse(x1, y1, x2, y2);
}

// EOF

