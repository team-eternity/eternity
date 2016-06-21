// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//--------------------------------------------------------------------------
//
// For portions of code explicitly marked as being under the 
// ZDoom Source Distribution License only:
//
// Copyright 1998-2012 Randy Heit  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions 
// are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// 3. The name of the author may not be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Line of sight checking for cameras
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "cam_sight.h"
#include "doomstat.h"   // ioanch 20160101: for bullet attacks
#include "d_gi.h"       // ioanch 20160131: for use
#include "d_player.h"   // ioanch 20151230: for autoaim
#include "m_compare.h"  // ioanch 20160103: refactor
#include "e_exdata.h"
#include "m_collection.h"
#include "m_fixed.h"
#include "p_chase.h"
#include "p_inter.h"    // ioanch 20160101: for damage
#include "p_map.h"      // ioanch 20160131: for use
#include "p_maputl.h"
#include "p_setup.h"
#include "p_skin.h"     // ioanch 20160131: for use
#include "p_spec.h"     // ioanch 20160101: for bullet effects
#include "polyobj.h"
#include "r_pcheck.h"   // ioanch 20160109: for correct portal plane z
#include "r_defs.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_sky.h"      // ioanch 20160101: for bullets hitting sky
#include "r_state.h"
#include "s_sound.h"    // ioanch 20160131: for use

// ioanch 20151230: defined macros for the validcount sets here
#define VALID_ALLOC(set, n) ((set) = ecalloc(byte *, 1, (((n) + 7) & ~7) / 8))
#define VALID_FREE(set) efree(set)
#define VALID_ISSET(set, i) ((set)[(i) >> 3] & (1 << ((i) & 7)))
#define VALID_SET(set, i) ((set)[(i) >> 3] |= 1 << ((i) & 7))

#define RECURSION_LIMIT 64

//=============================================================================
//
// Structures
//

class CamSight
{
public:
   
   fixed_t cx;          // camera coordinates
   fixed_t cy;
   fixed_t tx;          // target coordinates
   fixed_t ty;
   fixed_t sightzstart; // eye z of looker
   fixed_t topslope;    // slope to top of target
   fixed_t bottomslope; // slope to bottom of target
   
   divline_t trace;     // for line crossing tests

   fixed_t opentop;     // top of linedef silhouette
   fixed_t openbottom;  // bottom of linedef silhouette
   fixed_t openrange;   // height of opening

   // Intercepts vector
   PODCollection<intercept_t> intercepts;

   // portal traversal information
   int  fromid;        // current source group id
   int  toid;          // group id of the target
   bool hitpblock;     // traversed a block with a line portal
   bool addedportal;   // added a portal line to the intercepts list in current block
   bool portalresult;  // result from portal recursion
   bool portalexit;    // if true, returned from portal

   // pointer to invocation parameters
   const camsightparams_t *params;

   // ioanch 20151229: added optional bottom and top slope preset
   // in case of portal continuation
   CamSight(const camsightparams_t &sp)
      : cx(sp.cx), cy(sp.cy), tx(sp.tx), ty(sp.ty),
        opentop(0), openbottom(0), openrange(0),
        intercepts(),
        fromid(sp.cgroupid), toid(sp.tgroupid), 
        hitpblock(false), addedportal(false), 
        portalresult(false), portalexit(false),
        params(&sp)
   {
      memset(&trace, 0, sizeof(trace));
    
      sightzstart = params->cz + params->cheight - (params->cheight >> 2);

      const linkoffset_t *link = P_GetLinkIfExists(toid, fromid);
      bottomslope = params->tz - sightzstart;
      if(link)
      {
         bottomslope += link->z; // also adjust for Z offset
      }

      topslope    = bottomslope + params->theight;

   }
};


//=============================================================================
//
// Camera Sight Parameter Methods
//

//
// camsightparams_t::setCamera
//
// Set a camera object as the camera point.
//
void camsightparams_t::setCamera(const camera_t &camera, fixed_t height)
{
   cx       = camera.x;
   cy       = camera.y;
   cz       = camera.z;
   cheight  = height;
   cgroupid = camera.groupid;
}

//
// camsightparams_t::setLookerMobj
//
// Set an Mobj as the camera point.
//
void camsightparams_t::setLookerMobj(const Mobj *mo)
{
   cx       = mo->x;
   cy       = mo->y;
   cz       = mo->z;
   cheight  = mo->height;
   cgroupid = mo->groupid;
}

//
// camsightparams_t::setTargetMobj
//
// Set an Mobj as the target point.
//
void camsightparams_t::setTargetMobj(const Mobj *mo)
{
   tx       = mo->x;
   ty       = mo->y;
   tz       = mo->z;
   theight  = mo->height;
   tgroupid = mo->groupid;
}

///////////////////////////////////////////////////////////////////////////////
//
// PTDef
//
// PathTraverser setup
//
struct PTDef
{
   bool (*trav)(const intercept_t *in, void *context, const divline_t &trace);
   bool earlyOut;
   uint32_t flags;
};

//
// PathTraverser
//
// Reentrant path-traverse caller
//
class PathTraverser
{
public:
   bool traverse(fixed_t cx, fixed_t cy, fixed_t tx, fixed_t ty);
   PathTraverser(const PTDef &indef, void *incontext) : 
      trace(), def(indef), context(incontext), portalguard()
   {
      VALID_ALLOC(validlines, ::numlines);
      VALID_ALLOC(validpolys, ::numPolyObjects);
   }
   ~PathTraverser()
   {
      VALID_FREE(validlines);
      VALID_FREE(validpolys);
   }

   divline_t trace;
private:
   bool checkLine(size_t linenum);
   bool blockLinesIterator(int x, int y);
   bool blockThingsIterator(int x, int y);
   bool traverseIntercepts() const;

   const PTDef def;
   void *const context;
   byte *validlines;
   byte *validpolys;
   struct
   {
      bool hitpblock;
      bool addedportal;
   } portalguard;
   PODCollection<intercept_t> intercepts;
};

//
// PathTraverser::traverseIntercepts
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
   end   = intercepts.end();

   //
   // calculate intercept distance
   //
   for(scan = intercepts.begin(); scan < end; scan++)
   {
      if(!scan->isaline)
         continue;   // ioanch 20151230: only lines need this treatment
      P_MakeDivline(scan->d.line, &dl);
      scan->frac = P_InterceptVector(&trace, &dl);
   }
	
   //
   // go through in order
   //	
   in = NULL; // shut up compiler warning

   while(count--)
   {
      dist = D_MAXINT;

      for(scan = intercepts.begin(); scan < end; scan++)
      {
         if(scan->frac < dist)
         {
            dist = scan->frac;
            in   = scan;
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
// PathTraverser::blockThingsIterator
//
// Iterates through things
//
bool PathTraverser::blockThingsIterator(int x, int y)
{
   if(!def.earlyOut && (x < 0 || y < 0 || x >= ::bmapwidth || y >= ::bmapheight))
      return true;

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

      s1 = P_PointOnDivlineSide(x1, y1, &trace);
      s2 = P_PointOnDivlineSide(x2, y2, &trace);

      if(s1 == s2)
         continue;

      dl.x  = x1;
      dl.y  = y1;
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
// PathTraverser::checkLine
//
// Checks if a line is valid and inserts it
//
bool PathTraverser::checkLine(size_t linenum)
{
   line_t *ld = &lines[linenum];
   int s1, s2;
   divline_t dl;

   VALID_SET(validlines, linenum);

   s1 = P_PointOnDivlineSide(ld->v1->x, ld->v1->y, &trace);
   s2 = P_PointOnDivlineSide(ld->v2->x, ld->v2->y, &trace);
   if(s1 == s2)
      return true; // line isn't crossed

   P_MakeDivline(ld, &dl);
   s1 = P_PointOnDivlineSide(trace.x, trace.y, &dl);
   s2 = P_PointOnDivlineSide(trace.x + trace.dx, 
                             trace.y + trace.dy, &dl);
   if(s1 == s2)
      return true; // line isn't crossed

   // Early outs are only possible if we haven't crossed a portal block
   // ioanch 20151230: aim cams never quit
   if(def.earlyOut && !portalguard.hitpblock) 
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
      || (fsec && (fsec->c_pflags & PS_PASSABLE || fsec->f_pflags & PS_PASSABLE))
      || (bsec && (bsec->c_pflags & PS_PASSABLE || bsec->f_pflags & PS_PASSABLE)))
   {
      portalguard.addedportal = true;
   }

   return true;
}

//
// PathTraverser::blockLinesIterator
//
// Iterates through lines reentrantly
//
bool PathTraverser::blockLinesIterator(int x, int y)
{
   if(!def.earlyOut && (x < 0 || y < 0 || x >= ::bmapwidth || y >= ::bmapheight))
      return true;

   int  offset;
   int *list;
   DLListItem<polymaplink_t> *plink;

   offset = y * bmapwidth + x;

   // haleyjd 05/17/13: check portalmap; once we enter a cell with
   // a line portal in it, we can't short-circuit any further and must
   // build a full intercepts list.
   // ioanch 20151229: don't just check for line portals, also consider 
   // floor/ceiling
   if(::portalmap[offset])
      portalguard.hitpblock = true;

   // Check polyobjects first
   // haleyjd 02/22/06: consider polyobject lines
   plink = polyblocklinks[offset];

   while(plink)
   {
      polyobj_t *po = (*plink)->po;
      int polynum = static_cast<int>(po - PolyObjects);

       // if polyobj hasn't been checked
      if(!VALID_ISSET(validpolys, polynum))
      {
         VALID_SET(validpolys, polynum);
         
         for(int i = 0; i < po->numLines; ++i)
         {
            int linenum = static_cast<int>(po->lines[i] - lines);

            if(VALID_ISSET(validlines, linenum))
               continue; // line has already been checked

            if (!checkLine(static_cast<int>(po->lines[i] - ::lines)))
               return false;
         }
      }
      plink = plink->dllNext;
   }


   offset = *(blockmap + offset);
   list = blockmaplump + offset;

   // skip 0 starting delimiter
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
// PathTraverser::traverse
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

   trace.x  = cx;
   trace.y  = cy;
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
   if(def.earlyOut &&
      (xt1 < 0 || yt1 < 0 || xt1 >= bmapwidth || yt1 >= bmapheight ||
      xt2 < 0 || yt2 < 0 || xt2 >= bmapwidth || yt2 >= bmapheight))
      return false;

   if(xt2 > xt1)
   {
      mapxstep = 1;
      partialx = FRACUNIT - ((cx >> MAPBTOFRAC) & (FRACUNIT - 1));
      ystep    = FixedDiv(ty - cy, abs(tx - cx));
   }
   else if(xt2 < xt1)
   {
      mapxstep = -1;
      partialx = (cx >> MAPBTOFRAC) & (FRACUNIT - 1);
      ystep    = FixedDiv(ty - cy, abs(tx - cx));
   }
   else
   {
      mapxstep = 0;
      partialx = FRACUNIT;
      ystep    = 256*FRACUNIT;
   }	

   yintercept = (cy >> MAPBTOFRAC) + FixedMul(partialx, ystep);
	
   if(yt2 > yt1)
   {
      mapystep = 1;
      partialy = FRACUNIT - ((cy >> MAPBTOFRAC) & (FRACUNIT - 1));
      xstep    = FixedDiv(tx - cx, abs(ty - cy));
   }
   else if(yt2 < yt1)
   {
      mapystep = -1;
      partialy = (cy >> MAPBTOFRAC) & (FRACUNIT - 1);
      xstep    = FixedDiv(tx - cx, abs(ty - cy));
   }
   else
   {
      mapystep = 0;
      partialy = FRACUNIT;
      xstep    = 256*FRACUNIT;
   }	

   xintercept = (cx >> MAPBTOFRAC) + FixedMul(partialy, xstep);

   // From ZDoom (usable under ZDoom code license):
   // [RH] Fix for traces that pass only through blockmap corners. In that case,
   // xintercept and yintercept can both be set ahead of mapx and mapy, so the
   // for loop would never advance anywhere.

   if(abs(xstep) == FRACUNIT && abs(ystep) == FRACUNIT)
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
         ::portalmap[mapy * ::bmapwidth + mapx] & PMF_LINE)
      {
         if(def.flags & CAM_ADDLINES && !blockLinesIterator(mapx, mapy))
            return false;	// early out (ioanch: not for aim)
         if(def.flags & CAM_ADDTHINGS && !blockThingsIterator(mapx, mapy))
            return false;  // ioanch 20151230: aim also looks for a thing
      }
      
      if((mapxstep | mapystep) == 0)
         break;

#if 1
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
         if(!(def.flags & CAM_REQUIRELINEPORTALS) || 
            ::portalmap[mapy * ::bmapwidth + mapx] & PMF_LINE)
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
#else
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
#endif
   }

   //
   // couldn't early out, so go through the sorted list
   //
   return traverseIntercepts();
}

///////////////////////////////////////////////////////////////////////////////
//
// LineOpening
//
// Holds opening data just for the routines here
//
struct LineOpening
{
   fixed_t openrange, opentop, openbottom;
};

//
// CAM_lineOpening
//
// Stores data into a LineOpening struct
//
static void CAM_lineOpening(LineOpening &lo, const line_t *linedef)
{
   if(linedef->sidenum[1] == -1)
   {
      lo.openrange = 0;
      return;
   }

   const sector_t *front = linedef->frontsector;
   const sector_t *back = linedef->backsector;

   // no need to apply the portal hack (1024 units) here fortunately
   lo.opentop = emin(front->ceilingheight, back->ceilingheight);
   lo.openbottom = emax(front->floorheight, back->floorheight);  
   lo.openrange = lo.opentop - lo.openbottom;
}

///////////////////////////////////////////////////////////////////////////////
//
// CamContext
//
// All the data needed for sight checking
//
class CamContext
{
public:

   struct State
   {
      fixed_t originfrac, bottomslope, topslope;
      int reclevel;
   };

   static bool checkSight(const camsightparams_t &inparams, const State *state);
      
private:

   CamContext(const camsightparams_t &inparams, const State *state);
   static bool sightTraverse(const intercept_t *in, void *context, 
      const divline_t &trace);
   bool checkPortalSector(const sector_t *sector, fixed_t totalfrac, 
      fixed_t partialfrac, const divline_t &trace) const;
   bool recurse(int groupid, fixed_t x, fixed_t y, const State &state,
      bool *result, const linkdata_t &data) const;

   bool portalexit, portalresult;
   State state;
   const camsightparams_t *params;
   fixed_t sightzstart;
};

//
// CamContext::CamContext
//
// Constructs it, either from a previous state or scratch
//
CamContext::CamContext(const camsightparams_t &inparams, 
                       const State *instate) : 
portalexit(false), portalresult(false), params(&inparams)
{
   
   sightzstart = params->cz + params->cheight - (params->cheight >> 2);
   if(instate)
      state = *instate;
   else
   {
      state.originfrac = 0;
      state.bottomslope = params->tz - sightzstart;
      state.topslope = state.bottomslope + params->theight;
      state.reclevel = 0;
   }

}

//
// CamContext::sightTraverse
//
// Intercept routine for CamContext
//
bool CamContext::sightTraverse(const intercept_t *in, void *vcontext, 
      const divline_t &trace)
{
   const line_t *li = in->d.line;
   LineOpening lo = { 0 };
   CAM_lineOpening(lo, li);

   CamContext &context = *static_cast<CamContext *>(vcontext);

   // avoid round-off errors if possible
   fixed_t totalfrac = context.state.originfrac ? context.state.originfrac +
      FixedMul(in->frac, FRACUNIT - context.state.originfrac) : in->frac;
   const sector_t *sector = P_PointOnLineSide(trace.x, trace.y, li) == 0 ?
      li->frontsector : li->backsector;
   if(sector && totalfrac > 0)
   {
      if(context.checkPortalSector(sector, totalfrac, in->frac, trace))
      {
         context.portalresult = context.portalexit = true;
         return false;
      }
   }

   // quick test for totally closed doors
   // ioanch 20151231: also check BLOCKALL lines
   if(lo.openrange <= 0 || li->extflags & EX_ML_BLOCKALL)
      return false;

   const sector_t *osector = sector == li->frontsector ? 
      li->backsector : li->frontsector;
   fixed_t slope;

   if(sector->floorheight != osector->floorheight ||
      (!!(sector->f_pflags & PS_PASSABLE) ^ !!(osector->f_pflags & PS_PASSABLE)))
   {
      slope = FixedDiv(lo.openbottom - context.sightzstart, totalfrac);
      if(slope > context.state.bottomslope)
         context.state.bottomslope = slope;
      
   }

   if(sector->ceilingheight != osector->ceilingheight ||
      (!!(sector->c_pflags & PS_PASSABLE) ^ !!(osector->c_pflags & PS_PASSABLE)))
   {
      slope = FixedDiv(lo.opentop - context.sightzstart, totalfrac);
      if(slope < context.state.topslope)
         context.state.topslope = slope;
   }

   if(context.state.topslope <= context.state.bottomslope)
      return false;  // stop

   // have we hit a portal line
   if(li->pflags & PS_PASSABLE && P_PointOnLineSide(trace.x, trace.y, li) == 0 &&
      in->frac > 0)
   {
      State state(context.state);
      state.originfrac = totalfrac;
      if(context.recurse(li->portal->data.link.toid, 
         context.params->cx + FixedMul(trace.dx, in->frac),
         context.params->cy + FixedMul(trace.dy, in->frac),
         state,
         &context.portalresult, li->portal->data.link))
      {
         context.portalexit = true;
         return false;
      }
      return true;
   }

   return true;   // keep going
}

//
// CamContext::checkPortalSector
//
// ioanch 20151229: check sector if it has portals and create cameras for them
// Returns true if any branching sight check beyond a portal returned true
//
bool CamContext::checkPortalSector(const sector_t *sector, fixed_t totalfrac, 
                                   fixed_t partialfrac, const divline_t &trace) 
                                   const
{
   int newfromid;
   fixed_t linehitz;
   fixed_t newslope;

   fixed_t sectorfrac;
   fixed_t x, y;

   State newstate;
   bool result = false;
   
   if(state.topslope > 0 && sector->c_pflags & PS_PASSABLE &&
      (newfromid = sector->c_portal->data.link.toid) != params->cgroupid)
   {
      // ceiling portal (slope must be up)
      linehitz = sightzstart + FixedMul(state.topslope, totalfrac);
      fixed_t planez = R_CPLink(sector)->planez;
      if(linehitz > planez)
      {
         // update cam.bottomslope to be the top of the sector wall
         newslope = FixedDiv(planez - sightzstart, totalfrac);

         // if totalfrac == 0, then it will just be a very big slope
         if(newslope < state.bottomslope)
            newslope = state.bottomslope;

         // get x and y of position
         if(linehitz == sightzstart)
         {
            // handle this edge case: put point right on line
            x = trace.x + FixedMul(trace.dx, partialfrac);
            y = trace.y + FixedMul(trace.dy, partialfrac);
         }
         else
         {
            sectorfrac = FixedDiv(planez - sightzstart, linehitz - sightzstart);
            // update z frac
            totalfrac = FixedMul(sectorfrac, totalfrac);
            // retrieve the xy frac using the origin frac
            partialfrac = FixedDiv(totalfrac - state.originfrac, 
               FRACUNIT - state.originfrac);

            // HACK: add a unit just to ensure that it enters the sector
            x = trace.x + FixedMul(partialfrac + 1, trace.dx);
            y = trace.y + FixedMul(partialfrac + 1, trace.dy);
         }
         
         newstate.bottomslope = newslope;
         newstate.topslope = state.topslope;
         newstate.originfrac = totalfrac;
         newstate.reclevel = state.reclevel + 1;

         if(recurse(newfromid, x, y, newstate, &result, *R_CPLink(sector)) && 
            result)
         {
            return true;
         }
      }
   }

   if(state.bottomslope < 0 && sector->f_pflags & PS_PASSABLE &&
      (newfromid = sector->f_portal->data.link.toid) != params->cgroupid)
   {
      linehitz = sightzstart + FixedMul(state.bottomslope, totalfrac);
      fixed_t planez = R_FPLink(sector)->planez;
      if(linehitz < planez)
      {
         newslope = FixedDiv(planez - sightzstart, totalfrac);
         if(newslope > state.topslope)
            newslope = state.topslope;

         if(linehitz == sightzstart)
         {
            x = trace.x + FixedMul(trace.dx, partialfrac);
            y = trace.y + FixedMul(trace.dy, partialfrac);
         }
         else
         {
            sectorfrac = FixedDiv(planez - sightzstart, linehitz - sightzstart);
            totalfrac = FixedMul(sectorfrac, totalfrac);
            partialfrac = FixedDiv(totalfrac - state.originfrac, 
               FRACUNIT - state.originfrac);

            x = trace.x + FixedMul(partialfrac + 1, trace.dx);
            y = trace.y + FixedMul(partialfrac + 1, trace.dy);
         }

         newstate.bottomslope = state.bottomslope;
         newstate.topslope = newslope;
         newstate.originfrac = totalfrac;
         newstate.reclevel = state.reclevel + 1;

         if(recurse(newfromid, x, y, newstate, &result, *R_FPLink(sector)) &&
            result)
         {
            return true;
         }
      }
   }
   return false;
}

//
// CamContext::recurse
//
// If valid, starts a new lookup from below
//
bool CamContext::recurse(int newfromid, fixed_t x, fixed_t y, 
                         const State &instate, bool *result, 
                         const linkdata_t &data) const
{
   if(newfromid == params->cgroupid)
      return false;   // not taking us anywhere...

   if(instate.reclevel >= RECURSION_LIMIT)
      return false;  // protection

   camsightparams_t params;
   params.cx = x + data.deltax;
   params.cy = y + data.deltay;
   params.cz = this->params->cz + data.deltaz;
   params.cheight = this->params->cheight;
   params.tx = this->params->tx;
   params.ty = this->params->ty;
   params.tz = this->params->tz;
   params.theight = this->params->theight;
   params.cgroupid = newfromid;
   params.tgroupid = this->params->tgroupid;
   params.prev = this->params;

   *result = checkSight(params, &instate);
   return true;
}

//
// CAM_CheckSight
//
// Returns true if a straight line between the camera location and a
// thing's coordinates is unobstructed.
//
// ioanch 20151229: line portal sight fix
// Also added bottomslope and topslope pre-setting
//
bool CamContext::checkSight(const camsightparams_t &params, 
                            const CamContext::State *state)
{
   linkoffset_t *link = nullptr;
   // Camera and target are not in same group?
   if(params.cgroupid != params.tgroupid)
   {
      // is there a link between these groups?
      // if so, ignore reject
      link = P_GetLinkIfExists(params.cgroupid, params.tgroupid);
   }

   const sector_t *csec, *tsec;
   size_t s1, s2, pnum;
   //
   // check for trivial rejection
   //
   s1   = static_cast<int>((csec = R_PointInSubsector(params.cx, params.cy)->sector) - sectors);
   s2   = static_cast<int>((tsec = R_PointInSubsector(params.tx, params.ty)->sector) - sectors);
   pnum = s1 * numsectors + s2;

   bool result = false;

   if(link || !(rejectmatrix[pnum >> 3] & (1 << (pnum & 7))))
   {
      // killough 4/19/98: make fake floors and ceilings block monster view
      if((csec->heightsec != -1 &&
          ((params.cz + params.cheight <= sectors[csec->heightsec].floorheight &&
            params.tz >= sectors[csec->heightsec].floorheight) ||
           (params.cz >= sectors[csec->heightsec].ceilingheight &&
            params.tz + params.cheight <= sectors[csec->heightsec].ceilingheight)))
         ||
         (tsec->heightsec != -1 &&
          ((params.tz + params.theight <= sectors[tsec->heightsec].floorheight &&
            params.cz >= sectors[tsec->heightsec].floorheight) ||
           (params.tz >= sectors[tsec->heightsec].ceilingheight &&
            params.cz + params.theight <= sectors[tsec->heightsec].ceilingheight))))
         return false;

      //
      // check precisely
      //
      CamContext context(params, state);

      // if there is a valid portal link, adjust the target's coordinates now
      // so that we trace in the proper direction given the current link

      // ioanch 20151229: store displaced target coordinates because newCam.tx
      // and .ty will be modified internally
      fixed_t tx = params.tx;
      fixed_t ty = params.ty;
      if(link)
      {
         tx -= link->x;
         ty -= link->y;
      }
      PTDef def;
      def.flags = CAM_ADDLINES;
      def.earlyOut = true;
      def.trav = CamContext::sightTraverse;
      PathTraverser traverser(def, &context);
      result = traverser.traverse(params.cx, params.cy, tx, ty);

      if(context.portalexit)
         result = context.portalresult;
      else if(result && params.cgroupid != params.tgroupid)
      {
         result = false;
         if(link)
         {
            tsec = R_PointInSubsector(tx, ty)->sector;
            if(context.checkPortalSector(tsec, FRACUNIT, FRACUNIT, 
               traverser.trace))
            {
               result = true;
            }
         }
      }
   }
   return result;
}

//
// CAM_CheckSight
//
// Entry
//
bool CAM_CheckSight(const camsightparams_t &params)
{
   return CamContext::checkSight(params, nullptr);
}

///////////////////////////////////////////////////////////////////////////////
//
// AimContext
//
// Reentrant aim attack context
//
class AimContext
{
public:
   struct State
   {
      fixed_t origindist, bottomslope, topslope, cx, cy, cz;
      int groupid;
      const AimContext *prev;
      int reclevel;
   };

   static fixed_t aimLineAttack(const Mobj *t1, angle_t angle, fixed_t distance, 
      uint32_t mask, const State *state, Mobj **outTarget, fixed_t *outDist);

private:
   AimContext(const Mobj *t1, angle_t angle, fixed_t distance, uint32_t mask, 
      const State *state);
   static bool aimTraverse(const intercept_t *in, void *data, 
      const divline_t &trace);
   bool checkPortalSector(const sector_t *sector, fixed_t totalfrac, 
      fixed_t partialfrac, const divline_t &trace);
   fixed_t recurse(State &newstate, fixed_t partialfrac, fixed_t *outSlope, 
      Mobj **outTarget, fixed_t *outDist, const linkdata_t &data) const;

   const Mobj *thing;
   fixed_t attackrange;
   uint32_t aimflagsmask;
   State state;
   fixed_t lookslope;
   fixed_t aimslope;
   Mobj *linetarget;
   fixed_t targetdist;
   angle_t angle;
};

//
// AimContext::aimLineAttack
//
fixed_t AimContext::aimLineAttack(const Mobj *t1, angle_t angle, 
                                  fixed_t distance, uint32_t mask, 
                                  const State *state, Mobj **outTarget, 
                                  fixed_t *outDist)
{
   AimContext context(t1, angle, distance, mask, state);

   PTDef def;
   def.flags = CAM_ADDLINES | CAM_ADDTHINGS;
   def.earlyOut = false;
   def.trav = aimTraverse;
   PathTraverser traverser(def, &context);

   fixed_t tx = context.state.cx + (distance >> FRACBITS) *
      finecosine[angle >> ANGLETOFINESHIFT];
   fixed_t ty = context.state.cy + (distance >> FRACBITS) *
      finesine[angle >> ANGLETOFINESHIFT];

   if(traverser.traverse(context.state.cx, context.state.cy, tx, ty))
   {
      const sector_t *endsector = R_PointInSubsector(tx, ty)->sector;
      if(context.checkPortalSector(endsector, distance, FRACUNIT, 
         traverser.trace))
      {
         if(outTarget)
            *outTarget = context.linetarget;
         if(outDist)
            *outDist = context.targetdist;

         return context.linetarget ? context.aimslope : context.lookslope;
      }
   }

   if(outTarget)
      *outTarget = context.linetarget;
   if(outDist)
      *outDist = context.targetdist;

   return context.linetarget ? context.aimslope : context.lookslope;
}

//
// AimContext::AimContext
//
AimContext::AimContext(const Mobj *t1, angle_t inangle, fixed_t distance,
                       uint32_t mask, const State *instate) :
thing(t1), attackrange(distance), aimflagsmask(mask), aimslope(0),
   linetarget(nullptr), targetdist(D_MAXINT), angle(inangle)
{
   fixed_t pitch = t1->player ? t1->player->pitch : 0;
   lookslope = pitch ? finetangent[(ANG90 - pitch) >> ANGLETOFINESHIFT] : 0;
   
   if(instate)
      state = *instate;
   else
   {
      state.origindist = 0;
      state.cx = t1->x;
      state.cy = t1->y;
      state.cz = t1->z + (t1->height >> 1) + 8 * FRACUNIT;
      state.groupid = t1->groupid;
      state.prev = nullptr;
      state.reclevel = 0;
      
      if(!pitch)
      {
         state.topslope = 100 * FRACUNIT / 160;
         state.bottomslope = -100 * FRACUNIT / 160;
      }
      else
      {
         fixed_t topangle, bottomangle;
         topangle = pitch - ANGLE_1 * 32;
         bottomangle = pitch + ANGLE_1 * 32;
         
         state.topslope = finetangent[(ANG90 - topangle) >> ANGLETOFINESHIFT];
         state.bottomslope = finetangent[(ANG90 - bottomangle) >> 
            ANGLETOFINESHIFT];
      }
   }

}

//
// AimContext::aimTraverse
//
// Aim traverse function
//
bool AimContext::aimTraverse(const intercept_t *in, void *vdata, 
      const divline_t &trace)
{
   auto &context = *static_cast<AimContext *>(vdata);

   fixed_t totaldist;
   if(in->isaline)
   {
      const line_t *li = in->d.line;
      totaldist = context.state.origindist + 
         FixedMul(context.attackrange, in->frac);
      const sector_t *sector =
         P_PointOnLineSide(trace.x, trace.y, li) == 0 ? li->frontsector
         : li->backsector;
      if(sector && totaldist > 0)
      {
         // Don't care about return value; data will be collected in cam's
         // fields

         context.checkPortalSector(sector, totaldist, in->frac, trace);

         // if a closer target than how we already are has been found, then exit
         if(context.linetarget && context.targetdist <= totaldist)
            return false;
      }

      if(!(li->flags & ML_TWOSIDED) || li->extflags & EX_ML_BLOCKALL)
         return false;

      LineOpening lo;
      CAM_lineOpening(lo, li);

      if(lo.openrange <= 0)
         return false;

      const sector_t *osector = sector == li->frontsector ? 
         li->backsector : li->frontsector;
      fixed_t slope;

      if(sector->floorheight != osector->floorheight || 
         (!!(sector->f_pflags & PS_PASSABLE) ^ 
         !!(osector->f_pflags & PS_PASSABLE)))
      {
         slope = FixedDiv(lo.openbottom - context.state.cz, totaldist);
         if(slope > context.state.bottomslope)
            context.state.bottomslope = slope;
      }

      if(sector->ceilingheight != osector->ceilingheight || 
         (!!(sector->c_pflags & PS_PASSABLE) ^ 
         !!(osector->c_pflags & PS_PASSABLE)))
      {
         slope = FixedDiv(lo.opentop - context.state.cz, totaldist);
         if(slope < context.state.topslope)
            context.state.topslope = slope;
      }

      if(context.state.topslope <= context.state.bottomslope)
         return false;

      if(li->pflags & PS_PASSABLE && P_PointOnLineSide(trace.x, trace.y, li) == 0 &&
         in->frac > 0)
      {
         State newState(context.state);
         newState.cx = trace.x + FixedMul(trace.dx, in->frac);
         newState.cy = trace.y + FixedMul(trace.dy, in->frac);
         newState.groupid = li->portal->data.link.toid;
         newState.origindist = totaldist;
         
         return !context.recurse(newState,
            in->frac,
            &context.aimslope,
            &context.linetarget,
            nullptr, li->portal->data.link);
      }

      return true;
   }
   else
   {
      Mobj *th = in->d.thing;
      fixed_t thingtopslope, thingbottomslope;
      if(!(th->flags & MF_SHOOTABLE) || th == context.thing)
         return true;
      if(th->flags & context.thing->flags & context.aimflagsmask 
         && !th->player)
      {
         return true;
      }

      totaldist = context.state.origindist + FixedMul(context.attackrange, 
         in->frac);

      // check ceiling and floor
      const sector_t *sector = th->subsector->sector;
      if(sector && totaldist > 0)
      {
         context.checkPortalSector(sector, totaldist, in->frac, trace);
         // if a closer target than how we already are has been found, then exit
         if(context.linetarget && context.targetdist <= totaldist)
            return false;
      }

      thingtopslope = FixedDiv(th->z + th->height - context.state.cz, 
         totaldist);

      if(thingtopslope < context.state.bottomslope)
         return true; // shot over the thing

      thingbottomslope = FixedDiv(th->z - context.state.cz, totaldist);
      if(thingbottomslope > context.state.topslope)
         return true; // shot under the thing

      // this thing can be hit!
      if(thingtopslope > context.state.topslope)
         thingtopslope = context.state.topslope;

      if(thingbottomslope < context.state.bottomslope)
         thingbottomslope = context.state.bottomslope;

      // check if this thing is closer than potentially others found beyond
      // flat portals
      if(!context.linetarget || totaldist < context.targetdist)
      {
         context.aimslope = (thingtopslope + thingbottomslope) / 2;
         context.targetdist = totaldist;
         context.linetarget = th;
      }
      return false;
   }
}

//
// AimContext::checkPortalSector
//
bool AimContext::checkPortalSector(const sector_t *sector, fixed_t totalfrac, 
                                   fixed_t partialfrac, const divline_t &trace) 
{
   fixed_t linehitz, fixedratio;
   int newfromid;

   fixed_t x, y, newslope;
   
   if(state.topslope > 0 && sector->c_pflags & PS_PASSABLE && 
      (newfromid = sector->c_portal->data.link.toid) != state.groupid)
   {
      // ceiling portal (slope must be up)
      linehitz = state.cz + FixedMul(state.topslope, totalfrac);
      fixed_t planez = R_CPLink(sector)->planez;
      if(linehitz > planez)
      {
         // update cam.bottomslope to be the top of the sector wall
         newslope = FixedDiv(planez - state.cz, totalfrac);
         // if totalfrac == 0, then it will just be a very big slope
         if(newslope < state.bottomslope)
            newslope = state.bottomslope;

         // get x and y of position
         if(linehitz == state.cz)
         {
            // handle this edge case: put point right on line
            x = trace.x + FixedMul(trace.dx, partialfrac);
            y = trace.y + FixedMul(trace.dy, partialfrac);
         }
         else
         {
            // add a unit just to ensure that it enters the sector
            fixedratio = FixedDiv(planez - state.cz, linehitz - state.cz);
            // update z frac
            totalfrac = FixedMul(fixedratio, totalfrac);
            // retrieve the xy frac using the origin frac
            partialfrac = FixedDiv(totalfrac - state.origindist, attackrange);

            x = trace.x + FixedMul(partialfrac + 1, trace.dx);
            y = trace.y + FixedMul(partialfrac + 1, trace.dy);
               
         }
         
         fixed_t outSlope;
         Mobj *outTarget = nullptr;
         fixed_t outDist;

         State newstate(state);
         newstate.cx = x;
         newstate.cy = y;
         newstate.groupid = newfromid;
         newstate.origindist = totalfrac;
         newstate.bottomslope = newslope;
         newstate.reclevel = state.reclevel + 1;

         if(recurse(newstate, partialfrac, &outSlope, &outTarget, &outDist, *R_CPLink(sector)))
         {
            if(outTarget && (!linetarget || outDist < targetdist))
            {
               linetarget = outTarget;
               targetdist = outDist;
               aimslope = outSlope;
            }
         }
      }
   }
   if(state.bottomslope < 0 && sector->f_pflags & PS_PASSABLE && 
      (newfromid = sector->f_portal->data.link.toid) != state.groupid)
   {
      linehitz = state.cz + FixedMul(state.bottomslope, totalfrac);
      fixed_t planez = R_FPLink(sector)->planez;
      if(linehitz < planez)
      {
         newslope = FixedDiv(planez - state.cz, totalfrac);
         if(newslope > state.topslope)
            newslope = state.topslope;

         if(linehitz == state.cz)
         {
            x = trace.x + FixedMul(trace.dx, partialfrac);
            y = trace.y + FixedMul(trace.dy, partialfrac);
         }
         else
         {
            fixedratio = FixedDiv(planez - state.cz, linehitz - state.cz);
            totalfrac = FixedMul(fixedratio, totalfrac);
            partialfrac = FixedDiv(totalfrac - state.origindist, attackrange);

            x = trace.x + FixedMul(partialfrac + 1, trace.dx);
            y = trace.y + FixedMul(partialfrac + 1, trace.dy);
         }

         fixed_t outSlope;
         Mobj *outTarget = nullptr;
         fixed_t outDist;

         State newstate(state);
         newstate.cx = x;
         newstate.cy = y;
         newstate.groupid = newfromid;
         newstate.origindist = totalfrac;
         newstate.topslope = newslope;
         newstate.reclevel = state.reclevel + 1;

         if(recurse(newstate, partialfrac, &outSlope, &outTarget, &outDist, *R_FPLink(sector)))
         {
            if(outTarget && (!linetarget || outDist < targetdist))
            {
               linetarget = outTarget;
               targetdist = outDist;
               aimslope = outSlope;
            }
         }
      }
   }
   return false;
}

fixed_t AimContext::recurse(State &newstate, fixed_t partialfrac, 
                            fixed_t *outSlope, Mobj **outTarget, 
                            fixed_t *outDist, const linkdata_t &data) const
{
   if(newstate.groupid == state.groupid)
      return false;

   if(state.reclevel > RECURSION_LIMIT)
      return false;

   newstate.cx += data.deltax;
   newstate.cy += data.deltay;
   newstate.cz += data.deltaz;

   fixed_t lessdist = attackrange - FixedMul(attackrange, partialfrac);
   newstate.prev = this;
         
   fixed_t res = aimLineAttack(thing, angle, lessdist, aimflagsmask, &newstate, 
      outTarget, outDist);
   if(outSlope)
      *outSlope = res;
   return true;
}

//
// CAM_AimLineAttack
//
// Reentrant autoaim
//
fixed_t CAM_AimLineAttack(const Mobj *t1, angle_t angle, fixed_t distance, 
                          uint32_t mask, Mobj **outTarget)
{
   return AimContext::aimLineAttack(t1, angle, distance, mask, nullptr, 
      outTarget, nullptr);
}

///////////////////////////////////////////////////////////////////////////////
//
// ShootContext
//
// For bullet attacks
//
class ShootContext
{
public:
   struct State
   {
      const ShootContext *prev;
      fixed_t x, y, z;
      fixed_t origindist;
      int groupid;
      int reclevel;
   };

   static void lineAttack(Mobj *source, angle_t angle, fixed_t distance, 
      fixed_t slope, int damage, const State *state);
private:

   bool checkShootFlatPortal(const sector_t *sector, fixed_t infrac) const;
   bool shoot2SLine(line_t *li, int lineside, fixed_t dist, 
      const LineOpening &lo) const;
   bool shotCheck2SLine(line_t *li, int lineside, fixed_t frac) const;
   static bool shootTraverse(const intercept_t *in, void *data, 
      const divline_t &trace);
   ShootContext(Mobj *source, angle_t inangle, fixed_t distance, fixed_t slope,
      int indamage, const State *instate);

   Mobj *thing;
   angle_t angle;
   int damage;
   fixed_t attackrange;
   fixed_t aimslope;
   fixed_t cos, sin;
   State state;
};

//
// ShootContext::lineAttack
//
// Caller
//
void ShootContext::lineAttack(Mobj *source, angle_t angle, fixed_t distance, 
                              fixed_t slope, int damage, const State *state)
{
   ShootContext context(source, angle, distance, slope, damage, state);
   angle >>= ANGLETOFINESHIFT;
   fixed_t x2 = context.state.x + (distance >> FRACBITS) * context.cos;
   fixed_t y2 = context.state.y + (distance >> FRACBITS) * context.sin;

   PTDef def;
   def.flags = CAM_ADDLINES | CAM_ADDTHINGS;
   def.earlyOut = false;
   def.trav = shootTraverse;
   PathTraverser traverser(def, &context);
   
   if(traverser.traverse(context.state.x, context.state.y, x2, y2))
   {
      // if 100% passed, check if the final sector was crossed
      const sector_t *endsector = R_PointInSubsector(x2, y2)->sector;
      context.checkShootFlatPortal(endsector, FRACUNIT);
   }
}

//
// ShootContext::checkShootFlatPortal
//
bool ShootContext::checkShootFlatPortal(const sector_t *sidesector, 
                                        fixed_t infrac) const
{
   const linkdata_t *portaldata = nullptr;
   fixed_t pfrac = 0;
   fixed_t absratio = 0;
   int newfromid = R_NOGROUP;
   fixed_t z = state.z + FixedMul(aimslope, FixedMul(infrac, attackrange));

   if(sidesector->c_pflags & PS_PASSABLE)
   {
      // ceiling portal
      fixed_t planez = R_CPLink(sidesector)->planez;
      if(z > planez)
      {
         pfrac = FixedDiv(planez - state.z, aimslope);
         absratio = FixedDiv(planez - state.z, z - state.z);
         z = planez;
         portaldata = R_CPLink(sidesector);
         newfromid = sidesector->c_portal->data.link.toid;
      }
   }
   if(!portaldata && sidesector->f_pflags & PS_PASSABLE)
   {
      // floor portal
      fixed_t planez = R_FPLink(sidesector)->planez;
      if(z < planez)
      {
         pfrac = FixedDiv(planez - state.z, aimslope);
         absratio = FixedDiv(planez - state.z, z - state.z);
         z = planez;
         portaldata = R_FPLink(sidesector);
         newfromid = sidesector->f_portal->data.link.toid;
      }
   }
   if(portaldata)
   {
      // update x and y as well
      fixed_t x = state.x + FixedMul(cos, pfrac);
      fixed_t y = state.y + FixedMul(sin, pfrac);
      if(newfromid == state.groupid || state.reclevel >= RECURSION_LIMIT)
         return false;

      // NOTE: for line attacks, sightzstart also moves!
      fixed_t dist = FixedMul(FixedMul(attackrange, infrac), absratio);
      fixed_t remdist = attackrange - dist;

      x += portaldata->deltax;
      y += portaldata->deltay;
      z += portaldata->deltaz;

      State newstate(state);
      newstate.groupid = newfromid;
      newstate.origindist += dist;
      newstate.prev = this;
      newstate.x = x;
      newstate.y = y;
      newstate.z = z;
      newstate.reclevel++;

      lineAttack(thing, angle, remdist, aimslope, damage, &newstate);

      return true;
   }
   return false;
}

//
// ShootContext::shoot2SLine
//
bool ShootContext::shoot2SLine(line_t *li, int lineside, fixed_t dist,
                               const LineOpening &lo) const
{
   // ioanch: no more need for demo version < 333 check. Also don't allow comp.
   if(FixedDiv(lo.openbottom - state.z, dist) <= aimslope &&
      FixedDiv(lo.opentop - state.z, dist) >= aimslope)
   {
      if(li->special)
         P_ShootSpecialLine(thing, li, lineside);
      return true;
   }
   return false;
}

//
// ShootContext::shotCheck2SLine
//
bool ShootContext::shotCheck2SLine(line_t *li, int lineside, fixed_t frac) const
{
   bool ret = false;
   if(li->extflags & EX_ML_BLOCKALL)
      return false;

   if(li->flags & ML_TWOSIDED)
   {
      LineOpening lo = { 0 };
      CAM_lineOpening(lo, li);
      fixed_t dist = FixedMul(attackrange, frac);
      if(shoot2SLine(li, lineside, dist, lo))
         ret = true;
   }
   return ret;
}

//
// ShootContext::shootTraverse
//
bool ShootContext::shootTraverse(const intercept_t *in, void *data, 
                                 const divline_t &trace)
{
   auto &context = *static_cast<ShootContext *>(data);
   if(in->isaline)
   {
      line_t *li = in->d.line;

      // ioanch 20160101: use the trace origin instead of assuming it being the
      // same as the source thing origin, as it happens in PTR_ShootTraverse
      int lineside = P_PointOnLineSide(trace.x, trace.y, li);

      if(context.shotCheck2SLine(li, lineside, in->frac))
      {
         // ioanch 20160101: line portal aware
         if(li->pflags & PS_PASSABLE && lineside == 0 && in->frac > 0)
         {
            int newfromid = li->portal->data.link.toid;
            if(newfromid == context.state.groupid)
               return true;

            if(context.state.reclevel >= RECURSION_LIMIT)
               return true;

            // NOTE: for line attacks, sightzstart also moves!
            fixed_t x = trace.x + FixedMul(trace.dx, in->frac);
            fixed_t y = trace.y + FixedMul(trace.dy, in->frac);
            fixed_t z = context.state.z + FixedMul(context.aimslope, 
               FixedMul(in->frac, context.attackrange));
            fixed_t dist =  FixedMul(context.attackrange, in->frac);
            fixed_t remdist = context.attackrange - dist;

            const linkdata_t &data = li->portal->data.link;
            
            x += data.deltax;
            y += data.deltay;
            z += data.deltaz;  // why not
            
            State newstate(context.state);
            newstate.groupid = newfromid;
            newstate.x = x;
            newstate.y = y;
            newstate.z = z;
            newstate.prev = &context;
            newstate.origindist += dist;
            ++newstate.reclevel;

            lineAttack(context.thing, context.angle, remdist, context.aimslope,
               context.damage, &newstate);

            return false;
         }

         return true;
      }

      // ioanch 20160102: compensate the current range with the added one
      fixed_t frac = in->frac - FixedDiv(4 * FRACUNIT, context.attackrange +
         context.state.origindist);
      fixed_t x = trace.x + FixedMul(trace.dx, frac);
      fixed_t y = trace.y + FixedMul(trace.dy, frac);
      fixed_t z = context.state.z + FixedMul(context.aimslope, 
         FixedMul(frac, context.attackrange));

      const sector_t *sidesector = lineside ? li->backsector : li->frontsector;
      bool hitplane = false;
      int updown = 2;

      // ioanch 20160102: check for Z portals

      if(sidesector)
      {
         if(context.checkShootFlatPortal(sidesector, in->frac))
            return false;  // done here

         if(z < sidesector->floorheight)
         {
            fixed_t pfrac = FixedDiv(sidesector->floorheight 
               - context.state.z, context.aimslope);

            if(R_IsSkyFlat(sidesector->floorpic))
               return false;

            x = trace.x + FixedMul(context.cos, pfrac);
            y = trace.y + FixedMul(context.sin, pfrac);
            z = sidesector->floorheight;

            hitplane = true;
            updown = 0;
         }
         else if(z > sidesector->ceilingheight)
         {
            fixed_t pfrac = FixedDiv(sidesector->ceilingheight 
               - context.state.z, context.aimslope);
            if(sidesector->intflags & SIF_SKY)
               return false;
            x = trace.x + FixedMul(context.cos, pfrac);
            y = trace.y + FixedMul(context.sin, pfrac);
            z = sidesector->ceilingheight;

            hitplane = true;
            updown = 1;
         }
      }

      if(!hitplane && li->special)
         P_ShootSpecialLine(context.thing, li, lineside);

      if(R_IsSkyFlat(li->frontsector->ceilingpic) || li->frontsector->c_portal)
      {
         if(z > li->frontsector->ceilingheight)
            return false;
         if(li->backsector && R_IsSkyFlat(li->backsector->ceilingpic))
         {
            if(li->backsector->ceilingheight < z)
               return false;
         }
      }

      // ioanch 20160102: this doesn't make sense
//      if(!hitplane && li->portal)
//         return false;

      P_SpawnPuff(x, y, z, P_PointToAngle(0, 0, li->dx, li->dy) - ANG90, 
         updown, true);

      return false;
   }
   else
   {
      Mobj *th = in->d.thing;
      // self and shootable checks already handled. Friendliness check not
      // enabled anyway
      if(!(th->flags & MF_SHOOTABLE) || th == context.thing)
         return true;

      if(th->flags3 & MF3_GHOST && context.thing->player 
         && P_GetReadyWeapon(context.thing->player)->flags & WPF_NOHITGHOSTS)
      {
         return true;
      }

      fixed_t dist = FixedMul(context.attackrange, in->frac);
      fixed_t thingtopslope = FixedDiv(th->z + th->height - context.state.z, 
         dist);
      fixed_t thingbottomslope = FixedDiv(th->z - context.state.z, dist);

      if(thingtopslope < context.aimslope)
         return true;

      if(thingbottomslope > context.aimslope)
         return true;

      // ioanch 20160102: compensate
      fixed_t frac = in->frac - FixedDiv(10 * FRACUNIT, context.attackrange +
         context.state.origindist);
      fixed_t x = trace.x + FixedMul(trace.dx, frac);
      fixed_t y = trace.y + FixedMul(trace.dy, frac);
      fixed_t z = context.state.z + FixedMul(context.aimslope, FixedMul(frac,
         context.attackrange));

      if(th->flags & MF_NOBLOOD || th->flags2 & (MF2_INVULNERABLE | MF2_DORMANT))
      {
         P_SpawnPuff(x, y, z, P_PointToAngle(0, 0, trace.dx, trace.dy)
            - ANG180, 2, true);
      }
      else
      {
         BloodSpawner(th, x, y, z, context.damage, trace, context.thing).spawn(BLOOD_SHOT);
      }
      if(context.damage)
      {
         P_DamageMobj(th, context.thing, context.thing, context.damage,
            context.thing->info->mod);
      }

      return false;
   }
}

//
// ShootContext::ShootContext
//
ShootContext::ShootContext(Mobj *source, angle_t inangle, fixed_t distance,
                           fixed_t slope, int indamage, 
                           const State *instate) : 
   thing(source), angle(inangle), damage(indamage), attackrange(distance), 
      aimslope(slope)
{
   inangle >>= ANGLETOFINESHIFT;
   cos = finecosine[inangle];
   sin = finesine[inangle];
   if(instate)
      state = *instate;
   else
   {
      state.x = source->x;
      state.y = source->y;
      state.z = source->z - source->floorclip + (source->height >> 1) + 
         8 * FRACUNIT;
      state.groupid = source->groupid;
      state.prev = nullptr;
      state.origindist = 0;
      state.reclevel = 0;
   }
}

//
// CAM_LineAttack
//
// Portal-aware bullet attack
//
void CAM_LineAttack(Mobj *source, angle_t angle, fixed_t distance, 
                    fixed_t slope, int damage)
{
   ShootContext::lineAttack(source, angle, distance, slope, damage, nullptr);
}

//
// CAM_PathTraverse
//
// Public wrapper for PathTraverser
//
bool CAM_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                      uint32_t flags, void *data,
                      bool (*trav)(const intercept_t *in, void *data,
                                   const divline_t &trace))
{
   PTDef def;
   def.flags = flags;
   def.earlyOut = false;
   def.trav = trav;
   return PathTraverser(def, data).traverse(x1, y1, x2, y2);
}

///////////////////////////////////////////////////////////////////////////////
//
// UseContext
//
// Context for use actions
//
class UseContext
{
public:
   struct State
   {
      const UseContext *prev;
      fixed_t attackrange;
      int groupid;
      int reclevel;
   };

   static void useLines(const player_t *player, fixed_t x, fixed_t y,
      const State *instate);

private:
   UseContext(const player_t *player, const State *state);
   static bool useTraverse(const intercept_t *in, void *context, 
      const divline_t &trace);
   static bool noWayTraverse(const intercept_t *in, void *context, 
      const divline_t &trace);

   State state;
   const player_t *player;
   Mobj *thing;
   bool portalhit;
};

//
// UseContext::useLines
//
// Actions. Returns true if a switch has been hit
//
void UseContext::useLines(const player_t *player, fixed_t x, fixed_t y, 
                          const State *state)
{
   UseContext context(player, state);

   int angle = player->mo->angle >> ANGLETOFINESHIFT;

   fixed_t x2 = x + (context.state.attackrange >> FRACBITS) * finecosine[angle];
   fixed_t y2 = y + (context.state.attackrange >> FRACBITS) * finesine[angle];

   PTDef def;
   def.earlyOut = false;
   def.flags = CAM_ADDLINES;
   def.trav = useTraverse;
   PathTraverser traverser(def, &context);
   if(traverser.traverse(x, y, x2, y2))
   {
      if(!context.portalhit)
      {
         PTDef def;
         def.earlyOut = false;
         def.flags = CAM_ADDLINES;
         def.trav = noWayTraverse;
         PathTraverser traverser(def, &context);
         if(!traverser.traverse(x, y, x2, y2))
            S_StartSound(context.thing, GameModeInfo->playerSounds[sk_noway]);
      }
   }
}

//
// UseContext::UseContext
//
// Constructor
//
UseContext::UseContext(const player_t *inplayer, const State *instate) 
   : player(inplayer), thing(inplayer->mo), portalhit(false)
{
   if(instate)
      state = *instate;
   else
   {
      state.attackrange = USERANGE;
      state.prev = nullptr;
      state.groupid = inplayer->mo->groupid;
      state.reclevel = 0;
   }
}

//
// UseContext::useTraverse
//
// Use traverse. Based on PTR_UseTraverse.
//
bool UseContext::useTraverse(const intercept_t *in, void *vcontext, 
                             const divline_t &trace)
{
   auto context = static_cast<UseContext *>(vcontext);
   line_t *li = in->d.line;

   // ioanch 20160131: don't treat passable lines as portals; another code path
   // will use them.
   if(li->special && !(li->pflags & PS_PASSABLE))
   {
      // ioanch 20160131: portal aware
      const linkoffset_t *link = P_GetLinkOffset(context->thing->groupid,
         li->frontsector->groupid);
      P_UseSpecialLine(context->thing, li,
         P_PointOnLineSide(context->thing->x + link->x, 
                           context->thing->y + link->y, li) == 1);

      //WAS can't use for than one special line in a row
      //jff 3/21/98 NOW multiple use allowed with enabling line flag
      return !!(li->flags & ML_PASSUSE);
   }

   // no special
   LineOpening lo;
   if(li->extflags & EX_ML_BLOCKALL) // haleyjd 04/30/11
      lo.openrange = 0;
   else
      CAM_lineOpening(lo, li);

   if(clip.openrange <= 0)
   {
      // can't use through a wall
      S_StartSound(context->thing, GameModeInfo->playerSounds[sk_noway]);
      return false;
   }

   // ioanch 20160131: opportunity to pass through portals
   if(li->pflags & PS_PASSABLE && P_PointOnLineSide(trace.x, trace.y, li) == 0 && 
      in->frac > 0)
   {
      int newfromid = li->portal->data.link.toid;
      if(newfromid == context->state.groupid || 
         context->state.reclevel >= RECURSION_LIMIT)
      {
         return true;
      }

      State newState(context->state);
      newState.prev = context;
      newState.attackrange -= FixedMul(newState.attackrange, in->frac);
      newState.groupid = newfromid;
      newState.reclevel++;
      fixed_t x = trace.x + FixedMul(trace.dx, in->frac) + li->portal->data.link.deltax;
      fixed_t y = trace.y + FixedMul(trace.dy, in->frac) + li->portal->data.link.deltay;

      useLines(context->player, x, y, &newState);
      context->portalhit = true;
      return false;
   }

   // not a special line, but keep checking
   return true;
}

//
// UseContext::noWayTraverse
//
// Same as PTR_NoWayTraverse. Will NEVER be called if portals had been hit.
//
bool UseContext::noWayTraverse(const intercept_t *in, void *vcontext, 
                               const divline_t &trace)
{
   const line_t *ld = in->d.line; // This linedef
   if(ld->special) // Ignore specials
      return true;
   if(ld->flags & ML_BLOCKING) // Always blocking
      return false;
   LineOpening lo;
   CAM_lineOpening(lo, ld);

   const UseContext *context = static_cast<const UseContext *>(vcontext);

   return 
      !(clip.openrange  <= 0 ||                                  // No opening
        clip.openbottom > context->thing->z + STEPSIZE ||// Too high, it blocks
        clip.opentop    < context->thing->z + context->thing->height);
   // Too low, it blocks
}

//
// CAM_UseLines
//
// ioanch 20160131: portal-aware variant of P_UseLines
//
void CAM_UseLines(const player_t *player)
{
   UseContext::useLines(player, player->mo->x, player->mo->y, nullptr);
}

// EOF

