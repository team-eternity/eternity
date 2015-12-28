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
#include "e_exdata.h"
#include "m_collection.h"
#include "m_fixed.h"
#include "p_chase.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "polyobj.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_state.h"

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

   // linedef validcount substitute
   byte *validlines;
   byte *validpolys;

   // portal traversal information
   int  fromid;        // current source group id
   int  toid;          // group id of the target
   bool hitpblock;     // traversed a block with a line portal
   bool addedportal;   // added a portal line to the intercepts list in current block
   bool portalresult;  // result from portal recursion
   bool portalexit;    // if true, returned from portal

   // pointer to invocation parameters
   const camsightparams_t *params;

   // ioanch 20151229: line portal sight fix
   fixed_t originfrac;

   CamSight(const camsightparams_t &sp, fixed_t inbasefrac = 0)
      : cx(sp.cx), cy(sp.cy), tx(sp.tx), ty(sp.ty), 
        opentop(0), openbottom(0), openrange(0),
        intercepts(),
        fromid(sp.cgroupid), toid(sp.tgroupid), 
        hitpblock(false), addedportal(false), 
        portalresult(false), portalexit(false),
        params(&sp), originfrac(inbasefrac)
   {
      memset(&trace, 0, sizeof(trace));
    
      sightzstart = params->cz + params->cheight - (params->cheight >> 2);
      bottomslope = params->tz - sightzstart;
      topslope    = bottomslope + params->theight;

      validlines = ecalloc(byte *, 1, ((numlines + 7) & ~7) / 8);
      validpolys = ecalloc(byte *, 1, ((numPolyObjects + 7) & ~7) / 8);
   }

   ~CamSight()
   {
      efree(validlines);
      efree(validpolys);
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

//=============================================================================
//
// Sight Checking
//

//
// CAM_LineOpening
//
// Sets opentop and openbottom to the window
// through a two sided line.
//
void CAM_LineOpening(CamSight &cam, line_t *linedef)
{
   sector_t *front, *back;
   fixed_t frontceilz, frontfloorz, backceilz, backfloorz;

   if(linedef->sidenum[1] == -1)      // single sided line
   {
      cam.openrange = 0;
      return;
   }
   
   front = linedef->frontsector;
   back  = linedef->backsector;

   frontceilz  = front->ceilingheight;
   backceilz   = back->ceilingheight;
   frontfloorz = front->floorheight;
   backfloorz  = back->floorheight;

   if(frontceilz < backceilz)
      cam.opentop = frontceilz;
   else
      cam.opentop = backceilz;

   if(frontfloorz > backfloorz)
      cam.openbottom = frontfloorz;
   else
      cam.openbottom = backfloorz;

   cam.openrange = cam.opentop - cam.openbottom;
}

//
// CAM_SightTraverse
//
static bool CAM_CheckSight(const camsightparams_t &params, fixed_t originfrac);
static bool CAM_SightTraverse(CamSight &cam, intercept_t *in)
{
   line_t  *li;
   fixed_t slope;
	
   li = in->d.line;

   //
   // crosses a two sided line
   //
   CAM_LineOpening(cam, li);

   if(cam.openrange <= 0) // quick test for totally closed doors
      return false; // stop

   // ioanch 20151229: line portal fix
   // total initial length = 1
   // we have (originfrac + infrac * (1 - originfrac))
   // this is fixedfrac, and is the fraction from the master cam origin
   fixed_t fixedfrac = cam.originfrac ? cam.originfrac 
      + FixedMul(in->frac, FRACUNIT - cam.originfrac) : in->frac;

   if(li->frontsector->floorheight != li->backsector->floorheight)
   {
      slope = FixedDiv(cam.openbottom - cam.sightzstart, fixedfrac);
      if(slope > cam.bottomslope)
         cam.bottomslope = slope;
   }
	
   if(li->frontsector->ceilingheight != li->backsector->ceilingheight)
   {
      slope = FixedDiv(cam.opentop - cam.sightzstart, fixedfrac);
      if(slope < cam.topslope)
         cam.topslope = slope;
   }

   if(cam.topslope <= cam.bottomslope)
      return false; // stop

   // have we hit a portal line?
   if(li->pflags & PS_PASSABLE)
   {
      camsightparams_t params;
      int newfromid = li->portal->data.link.toid;

      if(newfromid == cam.fromid) // not taking us anywhere...
         return true;

      const camsightparams_t *prev = cam.params->prev;
      while(prev)
      {
         // we are trying to walk backward?
         if(prev->cgroupid == newfromid)
            return true; // ignore this line
         prev = prev->prev;
      }

      linkoffset_t *link = P_GetLinkIfExists(cam.fromid, newfromid);

      params.cx           = cam.params->cx + FixedMul(cam.trace.dx, in->frac);
      params.cy           = cam.params->cy + FixedMul(cam.trace.dy, in->frac);
      params.cz           = cam.params->cz;
      params.cheight      = cam.params->cheight;
      params.tx           = cam.params->tx;
      params.ty           = cam.params->ty;
      params.tz           = cam.params->tz;
      params.theight      = cam.params->theight;
      params.cgroupid     = newfromid;
      params.tgroupid     = cam.toid;
      params.prev         = cam.params;

      if(link)
      {
         params.cx += link->x;
         params.cy += link->y;
      }

      cam.portalresult = CAM_CheckSight(params, fixedfrac);
      cam.portalexit   = true;
      return false;    // break out      
   }

   return true; // keep going
}

static bool CAM_SightCheckLine(CamSight &cam, int linenum)
{
   line_t *ld = &lines[linenum];
   int s1, s2;
   divline_t dl;

   cam.validlines[linenum >> 3] |= 1 << (linenum & 7);

   s1 = P_PointOnDivlineSide(ld->v1->x, ld->v1->y, &cam.trace);
   s2 = P_PointOnDivlineSide(ld->v2->x, ld->v2->y, &cam.trace);
   if(s1 == s2)
      return true; // line isn't crossed

   P_MakeDivline(ld, &dl);
   s1 = P_PointOnDivlineSide(cam.trace.x, cam.trace.y, &dl);
   s2 = P_PointOnDivlineSide(cam.trace.x + cam.trace.dx, 
                             cam.trace.y + cam.trace.dy, &dl);
   if(s1 == s2)
      return true; // line isn't crossed

   // Early outs are only possible if we haven't crossed a portal block
   if(!cam.hitpblock) 
   {
      // try to early out the check
      if(!ld->backsector)
         return false; // stop checking

      // haleyjd: block-all lines block sight
      if(ld->extflags & EX_ML_BLOCKALL)
         return false; // can't see through it
   }

   // store the line for later intersection testing
   cam.intercepts.addNew().d.line = ld;

   // if this is a passable portal line, remember we just added it
   if(ld->pflags & PS_PASSABLE)
      cam.addedportal = true;

   return true;
}

//
// CAM_SightBlockLinesIterator
//
static bool CAM_SightBlockLinesIterator(CamSight &cam, int x, int y)
{
   int  offset;
   int *list;
   DLListItem<polymaplink_t> *plink;

   offset = y * bmapwidth + x;

   // haleyjd 05/17/13: check portalmap; once we enter a cell with
   // a line portal in it, we can't short-circuit any further and must
   // build a full intercepts list.
   if(portalmap[offset] & PMF_LINE)
      cam.hitpblock = true;

   // Check polyobjects first
   // haleyjd 02/22/06: consider polyobject lines
   plink = polyblocklinks[offset];

   while(plink)
   {
      polyobj_t *po = (*plink)->po;
      int polynum = po - PolyObjects;

       // if polyobj hasn't been checked
      if(!(cam.validpolys[polynum >> 3] & (1 << (polynum & 7))))
      {
         cam.validpolys[polynum >> 3] |= 1 << (polynum & 7);
         
         for(int i = 0; i < po->numLines; ++i)
         {
            int linenum = po->lines[i] - lines;

            if(cam.validlines[linenum >> 3] & (1 << (linenum & 7)))
               continue; // line has already been checked

            if(!CAM_SightCheckLine(cam, po->lines[i] - lines))
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

      if(cam.validlines[linenum >> 3] & (1 << (linenum & 7)))
         continue; // line has already been checked

      if(!CAM_SightCheckLine(cam, linenum))
         return false;
   }

   // haleyjd 08/20/13: optimization
   // we can go back to short-circuiting in future blockmap cells if we haven't 
   // actually intercepted any portal lines yet.
   if(!cam.addedportal)
      cam.hitpblock = false;

   return true; // everything was checked
}

//
// CAM_SightTraverseIntercepts
//
// Returns true if the traverser function returns true for all lines
//
static bool CAM_SightTraverseIntercepts(CamSight &cam)
{
   size_t    count;
   fixed_t   dist;
   divline_t dl;
   PODCollection<intercept_t>::iterator scan, end, in;

   count = cam.intercepts.getLength();
   end   = cam.intercepts.end();

   //
   // calculate intercept distance
   //
   for(scan = cam.intercepts.begin(); scan < end; scan++)
   {
      P_MakeDivline(scan->d.line, &dl);
      scan->frac = P_InterceptVector(&cam.trace, &dl);
   }
	
   //
   // go through in order
   //	
   in = NULL; // shut up compiler warning
	
   while(count--)
   {
      dist = D_MAXINT;

      for(scan = cam.intercepts.begin(); scan < end; scan++)
      {
         if(scan->frac < dist)
         {
            dist = scan->frac;
            in   = scan;
         }
      }

      if(in)
      {
         if(!CAM_SightTraverse(cam, in))
            return false; // don't bother going farther

         in->frac = D_MAXINT;
      }
   }

   return true; // everything was traversed
}

//
// CAM_SightPathTraverse
//
// Traces a line from x1,y1 to x2,y2, calling the traverser function for each
// Returns true if the traverser function returns true for all lines
//
static bool CAM_SightPathTraverse(CamSight &cam)
{
   fixed_t xt1, yt1, xt2, yt2;
   fixed_t xstep, ystep;
   fixed_t partialx, partialy;
   fixed_t xintercept, yintercept;
   int     mapx, mapy, mapxstep, mapystep;
		
   if(((cam.cx - bmaporgx) & (MAPBLOCKSIZE - 1)) == 0)
      cam.cx += FRACUNIT; // don't side exactly on a line
   
   if(((cam.cy - bmaporgy) & (MAPBLOCKSIZE - 1)) == 0)
      cam.cy += FRACUNIT; // don't side exactly on a line

   cam.trace.x  = cam.cx;
   cam.trace.y  = cam.cy;
   cam.trace.dx = cam.tx - cam.cx;
   cam.trace.dy = cam.ty - cam.cy;

   cam.cx -= bmaporgx;
   cam.cy -= bmaporgy;
   xt1 = cam.cx >> MAPBLOCKSHIFT;
   yt1 = cam.cy >> MAPBLOCKSHIFT;

   cam.tx -= bmaporgx;
   cam.ty -= bmaporgy;
   xt2 = cam.tx >> MAPBLOCKSHIFT;
   yt2 = cam.ty >> MAPBLOCKSHIFT;

   // points should never be out of bounds, but check once instead of
   // each block
   if(xt1 < 0 || yt1 < 0 || xt1 >= bmapwidth || yt1 >= bmapheight ||
      xt2 < 0 || yt2 < 0 || xt2 >= bmapwidth || yt2 >= bmapheight)
      return false;

   if(xt2 > xt1)
   {
      mapxstep = 1;
      partialx = FRACUNIT - ((cam.cx >> MAPBTOFRAC) & (FRACUNIT - 1));
      ystep    = FixedDiv(cam.ty - cam.cy, abs(cam.tx - cam.cx));
   }
   else if(xt2 < xt1)
   {
      mapxstep = -1;
      partialx = (cam.cx >> MAPBTOFRAC) & (FRACUNIT - 1);
      ystep    = FixedDiv(cam.ty - cam.cy, abs(cam.tx - cam.cx));
   }
   else
   {
      mapxstep = 0;
      partialx = FRACUNIT;
      ystep    = 256*FRACUNIT;
   }	

   yintercept = (cam.cy >> MAPBTOFRAC) + FixedMul(partialx, ystep);
	
   if(yt2 > yt1)
   {
      mapystep = 1;
      partialy = FRACUNIT - ((cam.cy >> MAPBTOFRAC) & (FRACUNIT - 1));
      xstep    = FixedDiv(cam.tx - cam.cx, abs(cam.ty - cam.cy));
   }
   else if(yt2 < yt1)
   {
      mapystep = -1;
      partialy = (cam.cy >> MAPBTOFRAC) & (FRACUNIT - 1);
      xstep    = FixedDiv(cam.tx - cam.cx, abs(cam.ty - cam.cy));
   }
   else
   {
      mapystep = 0;
      partialy = FRACUNIT;
      xstep    = 256*FRACUNIT;
   }	

   xintercept = (cam.cx >> MAPBTOFRAC) + FixedMul(partialy, xstep);

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
      if(!CAM_SightBlockLinesIterator(cam, mapx, mapy))
         return false;	// early out
		
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
         if(!CAM_SightBlockLinesIterator(cam, mapx + mapxstep, mapy) ||
            !CAM_SightBlockLinesIterator(cam, mapx, mapy + mapystep))
         {
            return false;
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
   return CAM_SightTraverseIntercepts(cam);
}

//
// CAM_CheckSight
//
// Returns true if a straight line between the camera location and a
// thing's coordinates is unobstructed.
//
// ioanch 20151229: line portal sight fix
//
static bool CAM_CheckSight(const camsightparams_t &params, fixed_t originfrac)
{
   sector_t *csec, *tsec;
   int s1, s2, pnum;
   bool result = false;
   linkoffset_t *link = NULL;

   // Camera and target are not in same group?
   if(params.cgroupid != params.tgroupid)
   {
      // is there a link between these groups?
      // if so, ignore reject
      link = P_GetLinkIfExists(params.cgroupid, params.tgroupid);
   }

   //
   // check for trivial rejection
   //
   s1   = (csec = R_PointInSubsector(params.cx, params.cy)->sector) - sectors;
   s2   = (tsec = R_PointInSubsector(params.tx, params.ty)->sector) - sectors;
   pnum = s1 * numsectors + s2;
	
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
      CamSight newCam(params, originfrac);

      // if there is a valid portal link, adjust the target's coordinates now
      // so that we trace in the proper direction given the current link
      if(link)
      {
         newCam.tx -= link->x;
         newCam.ty -= link->y;
      }

      result = CAM_SightPathTraverse(newCam);

      // if we broke out due to doing a portal recursion, replace the local
      // result with the result of the portal operation
      if(newCam.portalexit)
         result = newCam.portalresult;
      else if(newCam.fromid != newCam.toid)
      {
         // didn't hit a portal but in different groups; not visible.
         result = false;
      }
   }

   return result;
}
bool CAM_CheckSight(const camsightparams_t &params)
{
   return CAM_CheckSight(params, 0);
}

// EOF

