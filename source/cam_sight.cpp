// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// For portions of code under the ZDoom Source Distribution License:
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

#include "e_exdata.h"
#include "m_collection.h"
#include "m_fixed.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "polyobj.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_state.h"

struct camsight_t
{
   fixed_t cx;          // camera coordinates
   fixed_t cy;
   fixed_t tx;          // target coordinates
   fixed_t ty;
   fixed_t sightzstart; // eye z of looker
   fixed_t topslope;    // slope to top of target
   fixed_t bottomslope; // slope to bottom of target

   divline_t trace;

   fixed_t opentop;     // top of linedef silhouette
   fixed_t openbottom;  // bottom of linedef silhouette
   fixed_t openrange;   // height of opening

   // Intercepts vector
   PODCollection<intercept_t> intercepts;

   // linedef validcount substitute
   byte *validlines;
   byte *validpolys;
};

//
// CAM_LineOpening
//
// Sets opentop and openbottom to the window
// through a two sided line.
//
void CAM_LineOpening(camsight_t &cam, line_t *linedef)
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
static bool CAM_SightTraverse(camsight_t &cam, intercept_t *in)
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

   if(li->frontsector->floorheight != li->backsector->floorheight)
   {
      slope = FixedDiv(cam.openbottom - cam.sightzstart, in->frac);
      if(slope > cam.bottomslope)
         cam.bottomslope = slope;
   }
	
   if(li->frontsector->ceilingheight != li->backsector->ceilingheight)
   {
      slope = FixedDiv(cam.opentop - cam.sightzstart, in->frac);
      if(slope < cam.topslope)
         cam.topslope = slope;
   }

   if(cam.topslope <= cam.bottomslope)
      return false; // stop

   return true; // keep going
}

static bool CAM_SightCheckLine(camsight_t &cam, int linenum)
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

   // try to early out the check
   if(!ld->backsector)
      return false; // stop checking

   // haleyjd: block-all lines block sight
   if(ld->extflags & EX_ML_BLOCKALL)
      return false; // can't see through it

   // store the line for later intersection testing
   cam.intercepts.addNew().d.line = ld;

   return true;
}

//
// CAM_SightBlockLinesIterator
//
static bool CAM_SightBlockLinesIterator(camsight_t &cam, int x, int y)
{
   int  offset;
   int *list;
   DLListItem<polymaplink_t> *plink;

   offset = y * bmapwidth + x;

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

   return true; // everything was checked
}

//
// CAM_SightTraverseIntercepts
//
// Returns true if the traverser function returns true for all lines
//
static bool CAM_SightTraverseIntercepts(camsight_t &cam)
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
static bool CAM_SightPathTraverse(camsight_t &cam)
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
bool CAM_CheckSight(fixed_t cx, fixed_t cy, fixed_t cz, fixed_t cheight,
                    fixed_t tx, fixed_t ty, fixed_t tz, fixed_t theight)
{
   sector_t *csec, *tsec;
   int s1, s2, pnum;
   bool result = false;

   //
   // check for trivial rejection
   //
   s1   = (csec = R_PointInSubsector(cx, cy)->sector) - sectors;
   s2   = (tsec = R_PointInSubsector(tx, ty)->sector) - sectors;
   pnum = s1 * numsectors + s2;
	
   if(!(rejectmatrix[pnum >> 3] & (1 << (pnum & 7))))
   {
      camsight_t newCam;

      // killough 4/19/98: make fake floors and ceilings block monster view
      if((csec->heightsec != -1 &&
          ((cz + cheight <= sectors[csec->heightsec].floorheight &&
            tz >= sectors[csec->heightsec].floorheight) ||
           (cz >= sectors[csec->heightsec].ceilingheight &&
            tz + cheight <= sectors[csec->heightsec].ceilingheight)))
         ||
         (tsec->heightsec != -1 &&
          ((tz + theight <= sectors[tsec->heightsec].floorheight &&
            cz >= sectors[tsec->heightsec].floorheight) ||
           (tz >= sectors[tsec->heightsec].ceilingheight &&
            cz + theight <= sectors[tsec->heightsec].ceilingheight))))
         return false;

      //
      // check precisely
      //
      newCam.cx          = cx;
      newCam.cy          = cy;
      newCam.tx          = tx;
      newCam.ty          = ty;
      newCam.sightzstart = cz + cheight - (cheight >> 2);
      newCam.bottomslope = tz - newCam.sightzstart;
      newCam.topslope    = newCam.bottomslope + theight;
      newCam.validlines  = ecalloc(byte *, 1, ((numlines + 7) & ~7) / 8);
      newCam.validpolys  = ecalloc(byte *, 1, ((numPolyObjects + 7) & ~7) / 8);

      result = CAM_SightPathTraverse(newCam);

      efree(newCam.validlines);
      efree(newCam.validpolys);
   }

   return result;
}

// EOF

