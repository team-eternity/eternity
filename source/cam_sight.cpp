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
// DESCRIPTION:
//      Line of sight checking for cameras
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "m_collection.h"
#include "m_fixed.h"
#include "p_maputl.h"
#include "p_setup.h"
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

   PODCollection<intercept_t> intercepts;
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

//
// CAM_SightBlockLinesIterator
//
static bool CAM_SightBlockLinesIterator(camsight_t &cam, int x, int y)
{
   int        offset;
   int       *list;
   int        s1, s2;
   divline_t  dl;

   offset = y * bmapwidth + x;

   offset = *(blockmap + offset);
   list = blockmaplump + offset;

   // skip 0 starting delimiter
   ++list;

   for(; *list != -1; list++)
   {
      line_t *ld;
      
      if(*list >= numlines)
         continue;

      ld = &lines[*list];
      if(ld->validcount == validcount)
         continue; // line has already been checked

      ld->validcount = validcount;

      s1 = P_PointOnDivlineSide(ld->v1->x, ld->v1->y, &cam.trace);
      s2 = P_PointOnDivlineSide(ld->v2->x, ld->v2->y, &cam.trace);
      if(s1 == s2)
         continue; // line isn't crossed

      P_MakeDivline(ld, &dl);
      s1 = P_PointOnDivlineSide(cam.trace.x, cam.trace.y, &dl);
      s2 = P_PointOnDivlineSide(cam.trace.x + cam.trace.dx, 
                                cam.trace.y + cam.trace.dy, &dl);
      if(s1 == s2)
         continue; // line isn't crossed

      // try to early out the check
      if(!ld->backsector)
         return false; // stop checking

      // store the line for later intersection testing
      cam.intercepts.addNew().d.line = ld;
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
   size_t       count;
   fixed_t      dist;
   divline_t    dl;
   PODCollection<intercept_t>::iterator scan;
   PODCollection<intercept_t>::iterator end;
   PODCollection<intercept_t>::iterator in;

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

      if(!CAM_SightTraverse(cam, in))
         return false; // don't bother going farther

      in->frac = D_MAXINT;
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
   fixed_t partial;
   fixed_t xintercept, yintercept;
   int     mapx, mapy, mapxstep, mapystep;
		
   ++validcount;

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
      partial  = FRACUNIT - ((cam.cx >> MAPBTOFRAC) & (FRACUNIT - 1));
      ystep    = FixedDiv(cam.ty - cam.cy, abs(cam.tx - cam.cx));
   }
   else if(xt2 < xt1)
   {
      mapxstep = -1;
      partial  = (cam.cx >> MAPBTOFRAC) & (FRACUNIT - 1);
      ystep    = FixedDiv(cam.ty - cam.cy, abs(cam.tx - cam.cx));
   }
   else
   {
      mapxstep = 0;
      partial  = FRACUNIT;
      ystep    = 256*FRACUNIT;
   }	

   yintercept = (cam.cy >> MAPBTOFRAC) + FixedMul(partial, ystep);
	
   if(yt2 > yt1)
   {
      mapystep = 1;
      partial  = FRACUNIT - ((cam.cy >> MAPBTOFRAC) & (FRACUNIT - 1));
      xstep    = FixedDiv(cam.tx - cam.cx, abs(cam.ty - cam.cy));
   }
   else if(yt2 < yt1)
   {
      mapystep = -1;
      partial  = (cam.cy >> MAPBTOFRAC) & (FRACUNIT - 1);
      xstep    = FixedDiv(cam.tx - cam.cx, abs(cam.ty - cam.cy));
   }
   else
   {
      mapystep = 0;
      partial  = FRACUNIT;
      xstep    = 256*FRACUNIT;
   }	

   xintercept = (cam.cx >> MAPBTOFRAC) + FixedMul(partial, xstep);
	
   // step through map blocks
   // Count is present to prevent a round off error from skipping the break
   mapx = xt1;
   mapy = yt1;
	
   for(int count = 0; count < 64; count++)
   {
      if(!CAM_SightBlockLinesIterator(cam, mapx, mapy))
         return false;	// early out
		
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
bool CAM_CheckSight(fixed_t cx, fixed_t cy, fixed_t cz, 
                    fixed_t tx, fixed_t ty, fixed_t tz, fixed_t theight)
{
   int s1, s2, pnum;
   bool result = false;

   //
   // check for trivial rejection
   //
   s1   = R_PointInSubsector(cx, cy)->sector - sectors;
   s2   = R_PointInSubsector(tx, ty)->sector - sectors;
   pnum = s1 * numsectors + s2;
	
   if(!(rejectmatrix[pnum >> 3] & (1 << (pnum & 7))))
   {
      camsight_t newCam;

      //
      // check precisely
      //
      newCam.cx          = cx;
      newCam.cy          = cy;
      newCam.tx          = tx;
      newCam.ty          = ty;
      newCam.sightzstart = cz;
      newCam.topslope    = (tz + theight) - newCam.sightzstart;
      newCam.bottomslope = tz - newCam.sightzstart;

      result = CAM_SightPathTraverse(newCam);
   }

   return result;
}

// EOF

