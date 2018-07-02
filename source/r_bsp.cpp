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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      BSP traversal, handling of LineSegs for rendering.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "doomstat.h"
#include "e_exdata.h"
#include "m_bbox.h"
#include "p_chase.h"
#include "p_maputl.h"   // ioanch 20160125
#include "p_portal.h"
#include "p_slopes.h"
#include "r_data.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_pcheck.h"
#include "r_plane.h"
#include "r_dynseg.h"
#include "r_dynabsp.h"
#include "r_portal.h"
#include "r_segs.h"
#include "r_sky.h"
#include "r_state.h"
#include "r_things.h"
#include "v_alloc.h"
#include "v_misc.h"

// Microscopic distance to move rejectable segs from portal lines to avoid point
// on line overlaps.
static const float kPortalSegRejectionFudge = 1.f / 256;

drawseg_t *ds_p;

// killough 4/7/98: indicates doors closed wrt automap bugfix:
int      doorclosed;

// killough: New code which removes 2s linedef limit
drawseg_t *drawsegs = NULL;
unsigned int maxdrawsegs;
// drawseg_t drawsegs[MAXDRAWSEGS];       // old code -- killough


//
// R_ClearDrawSegs
//
void R_ClearDrawSegs(void)
{
   ds_p = drawsegs;
}

//
// ClipWallSegment
// Clips the given range of columns
// and includes it in the new clip list.
//
// 1/11/98 killough: Since a type "short" is sufficient, we
// should use it, since smaller arrays fit better in cache.
//

// SoM 05/14/09: This actually becomes a bit of an optimization problem
// see, currently the code has to clip segs to the screen manually, and
// then clip them based on solid segs. This could be reduced to a single
// clip based on solidsegs because the first solidseg is from MININT, -1
// to viewwindow.width, MAXINT

typedef struct cliprange_s
{
  int first, last;
} cliprange_t;


// 1/11/98: Lee Killough
//
// This fixes many strange venetian blinds crashes, which occurred when a scan
// line had too many "posts" of alternating non-transparent and transparent
// regions. Using a doubly-linked list to represent the posts is one way to
// do it, but it has increased overhead and poor spatial locality, which hurts
// cache performance on modern machines. Since the maximum number of posts
// theoretically possible is a function of screen width, a static limit is
// okay in this case. It used to be 32, which was way too small.
//
// This limit was frequently mistaken for the visplane limit in some Doom
// editing FAQs, where visplanes were said to "double" if a pillar or other
// object split the view's space into two pieces horizontally. That did not
// have anything to do with visplanes, but it had everything to do with these
// clip posts.

#define MAXSEGS (w/2+1)   /* killough 1/11/98, 2/8/98 */

// newend is one past the last valid seg
static cliprange_t *newend;
static cliprange_t *solidsegs;

// addend is one past the last valid added seg.
static cliprange_t *addedsegs;
static cliprange_t *addend;

VALLOCATION(solidsegs)
{
   cliprange_t *buf = 
      ecalloctag(cliprange_t *, MAXSEGS*2, sizeof(cliprange_t), PU_VALLOC, NULL);

   solidsegs = buf;
   addedsegs = buf + MAXSEGS;
   addend = addedsegs;
}


static void R_AddSolidSeg(int x1, int x2)
{
   cliprange_t *rover = solidsegs;

   while(rover->last < x1 - 1) rover++;

   if(x1 < rover->first)
   {
      if(x2 < rover->first - 1)
      {
         memmove(rover + 1, rover, (++newend - rover) * sizeof(*rover));
         rover->first = x1;
         rover->last = x2;
         goto verify;
      }

      rover->first = x1;
   }

   if(rover->last >= x2)
      goto verify;

   rover->last = x2;

   if(rover->last >= (rover + 1)->first - 1)
   {
      (rover + 1)->first = rover->first;

      while(rover + 1 < newend)
      {
         *rover = *(rover + 1);
         rover++;
      }

      newend = rover;
   }
   
   verify:
#ifdef RANGECHECK
   // Verify the new segs
   for(rover = solidsegs; (rover + 1) < newend; rover++)
   {
      if(rover->last >= (rover+1)->first)
      {
         I_Error("R_AddSolidSeg: created a seg that overlaps next seg:\n"
                 "   (%i)->last = %i, (%i)->first = %i\n", 
                 static_cast<int>(rover - solidsegs),
                 rover->last, 
                 static_cast<int>((rover + 1) - solidsegs),
                 (rover + 1)->last);
      }
   }
#else
   return;
#endif
}

void R_MarkSolidSeg(int x1, int x2)
{
   addend->first = x1;
   addend->last = x2;
   addend++;
}

static void R_AddMarkedSegs()
{
   cliprange_t *r;

   for(r = addedsegs; r < addend; r++)
      R_AddSolidSeg(r->first, r->last);

   addend = addedsegs;
}

//
// R_ClipSolidWallSegment
//
// Handles solid walls,
//  e.g. single sided LineDefs (middle texture)
//  that entirely block the view.
//
static void R_ClipSolidWallSegment(int x1, int x2)
{
   cliprange_t *next, *start;
   
   // Find the first range that touches the range
   // (adjacent pixels are touching).
   
   start = solidsegs;
   while(start->last < x1 - 1)
      ++start;

   if(x1 < start->first)
   {
      if(x2 < start->first - 1)
      {
         // Post is entirely visible (above start), so insert a new clippost.
         R_StoreWallRange(x1, x2);
         
         // 1/11/98 killough: performance tuning using fast memmove
         memmove(start + 1, start, (++newend - start) * sizeof(*start));
         start->first = x1;
         start->last = x2;
         goto verifysegs;
      }

      // There is a fragment above *start.
      R_StoreWallRange(x1, start->first - 1);
      
      // Now adjust the clip size.
      start->first = x1;
   }

   // Bottom contained in start?
   if(x2 <= start->last)
      goto verifysegs;

   next = start;
   while(x2 >= (next + 1)->first - 1)
   {      // There is a fragment between two posts.
      R_StoreWallRange(next->last + 1, (next + 1)->first - 1);
      ++next;
      if(x2 <= next->last)
      {  
         // Bottom is contained in next. Adjust the clip size.
         start->last = next->last;
         goto crunch;
      }
   }

   // There is a fragment after *next.
   R_StoreWallRange(next->last + 1, x2);
   
   // Adjust the clip size.
   start->last = x2;
   
   // Remove start+1 to next from the clip list,
   // because start now covers their area.
crunch:
   if(next == start) // Post just extended past the bottom of one post.
      goto verifysegs;
   
   while(next++ != newend)      // Remove a post.
      *++start = *next;
   
   newend = start;

   verifysegs:
#ifdef RANGECHECK
   // Verify the new segs
   for(start = solidsegs; (start + 1) < newend; start++)
   {
      if(start->last >= (start+1)->first)
      {
         I_Error("R_ClipSolidWallSegment: created a seg that overlaps next seg:\n"
                 "   (%i)->last = %i, (%i)->first = %i\n", 
                 static_cast<int>(start - solidsegs),
                 start->last, 
                 static_cast<int>((start + 1) - solidsegs),
                 (start + 1)->last);
      }
   }
#else
   return;
#endif
}

//
// R_ClipPassWallSegment
//
// Clips the given range of columns,
//  but does not includes it in the clip list.
// Does handle windows,
//  e.g. LineDefs with upper and lower texture.
//
static void R_ClipPassWallSegment(int x1, int x2)
{
   cliprange_t *start;
   
   start = solidsegs;
   
   // Find the first range that touches the range
   //  (adjacent pixels are touching).
   while(start->last < x1 - 1)
      ++start;

   if(x1 < start->first)
   {
      if(x2 < start->first - 1)
      {
         // Post is entirely visible (above start).
         R_StoreWallRange(x1, x2);
         return;
      }

      // There is a fragment above *start.
      R_StoreWallRange(x1, start->first - 1);
   }

   // Bottom contained in start?
   if(x2 <= start->last)
      return;

   while(x2 >= (start + 1)->first - 1)
   {
      // There is a fragment between two posts.
      R_StoreWallRange(start->last + 1, (start + 1)->first - 1);
      ++start;
      
      if(x2 <= start->last)
         return;
   }
   
   // There is a fragment after *next.
   R_StoreWallRange(start->last + 1, x2);
}

//
// R_ClearClipSegs
//
void R_ClearClipSegs()
{
   solidsegs[0].first = D_MININT + 1;
   solidsegs[0].last  = -1;
   solidsegs[1].first = viewwindow.width;
   solidsegs[1].last  = D_MAXINT - 1;
   newend = solidsegs+2;
   addend = addedsegs;

   // haleyjd 09/22/07: must clear seg and segclip structures
   memset(&seg,     0, sizeof(cb_seg_t));
   memset(&segclip, 0, sizeof(cb_seg_t));
}

//
// R_SetupPortalClipsegs
//
bool R_SetupPortalClipsegs(int minx, int maxx, 
   const float *top, const float *bottom)
{
   int i = minx, stop = maxx + 1;
   cliprange_t *solidseg = solidsegs;
   
   R_ClearClipSegs();

   // SoM: This should be done here instead of having an additional loop
   portalrender.miny = (float)(video.height);
   portalrender.maxy = 0;
   
   // extend first solidseg to one column left of first open post
   while(i < stop && bottom[i] < top[i]) 
      ++i;
   
   // the entire thing is closed?
   if(i == stop)
      return false;

   solidseg->last = i - 1;
   solidseg++;   
   
   while(1)
   {
      //find the first closed post.
      while(i < stop && bottom[i] >= top[i]) 
      {
         if(top[i] < portalrender.miny) 
            portalrender.miny = top[i];

         if(bottom[i] > portalrender.maxy) 
            portalrender.maxy = bottom[i];

         ++i;
      }
      
      if(i == viewwindow.width)
         goto endopen;

      // set the solidsegs
      solidseg->first = i;
      
      // find the first open post
      while(i < stop && top[i] > bottom[i]) i++;
      if(i == stop)
         goto endclosed;
      
      solidseg->last = i - 1;
      solidseg++;
   }
   
endopen:
   solidseg->first = stop;
   solidseg->last = D_MAXINT;
   newend = solidseg + 1;
   return true;
   
endclosed:
   solidseg->last = D_MAXINT;
   newend = solidseg + 1;
   return true;
}

//
// R_DoorClosed
//
// killough 1/18/98 -- This function is used to fix the automap bug which
// showed lines behind closed doors simply because the door had a dropoff.
//
// It assumes that Doom has already ruled out a door being closed because
// of front-back closure (e.g. front floor is taller than back ceiling).
//
// FIXME: no longer used or needed in Eternity?
//
static int R_DoorClosed(void)
{
   return

     // if door is closed because back is shut:
     seg.backsec->ceilingheight <= seg.backsec->floorheight

     // preserve a kind of transparent door/lift special effect:
     && (seg.backsec->ceilingheight >= seg.frontsec->ceilingheight ||
         seg.line->sidedef->toptexture)

     && (seg.backsec->floorheight <= seg.frontsec->floorheight ||
         seg.line->sidedef->bottomtexture)

     // properly render skies (consider door "open" if both ceilings are sky):
     && (!(seg.backsec->intflags & SIF_SKY) || !(seg.frontsec->intflags & SIF_SKY));
}

//
// killough 3/7/98: Hack floor/ceiling heights for deep water etc.
//
// If player's view height is underneath fake floor, lower the
// drawn ceiling to be just under the floor height, and replace
// the drawn floor and ceiling textures, and light level, with
// the control sector's.
//
// Similar for ceiling, only reflected.
//
// killough 4/11/98, 4/13/98: fix bugs, add 'back' parameter
//

extern camera_t *camera; // haleyjd

sector_t *R_FakeFlat(sector_t *sec, sector_t *tempsec,
                     int *floorlightlevel, int *ceilinglightlevel,
                     bool back)
{
   if(!sec)
      return NULL;

   if(floorlightlevel)
   {
      *floorlightlevel =
      (sec->flags & SECF_FLOORLIGHTABSOLUTE ? 0 : sec->floorlightsec == -1 ?
       sec->lightlevel : sectors[sec->floorlightsec].lightlevel)
      + sec->floorlightdelta;
   }

   if(ceilinglightlevel)
   {
       // killough 4/11/98
      *ceilinglightlevel =
      (sec->flags & SECF_CEILLIGHTABSOLUTE ? 0 : sec->ceilinglightsec == -1 ?
       sec->lightlevel : sectors[sec->ceilinglightsec].lightlevel)
      + sec->ceilinglightdelta;
   }

   if(sec->heightsec != -1 || sec->f_portal || sec->c_portal)
   {
      // SoM: This was moved here for use with portals
      // Replace sector being drawn, with a copy to be hacked
      *tempsec = *sec;
   }
   
   if(sec->heightsec != -1)
   {
      int underwater; // haleyjd: restructured
      
      int heightsec;
      
      const sector_t *s = &sectors[sec->heightsec];
      
      // haleyjd 01/07/14: get from view.sector due to interpolation
      heightsec = view.sector->heightsec;
            
      underwater = (heightsec != -1 && viewz <= sectors[heightsec].floorheight);

      // Replace floor and ceiling height with other sector's heights.
      tempsec->floorheight   = s->floorheight;
      tempsec->ceilingheight = s->ceilingheight;

      // killough 11/98: prevent sudden light changes from non-water sectors:
      if(underwater && (tempsec->floorheight   = sec->floorheight,
                        tempsec->ceilingheight = s->floorheight-1, !back))
      {
         // SoM: kill any ceiling portals that may try to render
         tempsec->c_portal = NULL;

         // head-below-floor hack
         tempsec->floorpic       = s->floorpic;
         tempsec->floor_xoffs    = s->floor_xoffs;
         tempsec->floor_yoffs    = s->floor_yoffs;
         tempsec->floor_xscale   = s->floor_xscale;
         tempsec->floor_yscale   = s->floor_yscale;
         tempsec->floorbaseangle = s->floorbaseangle; // haleyjd: angles
         tempsec->floorangle     = s->floorangle;

         // haleyjd 03/13/05: removed redundant if(underwater) check
         if(s->intflags & SIF_SKY)
         {
            tempsec->floorheight      = tempsec->ceilingheight+1;
            tempsec->ceilingpic       = tempsec->floorpic;
            tempsec->ceiling_xoffs    = tempsec->floor_xoffs;
            tempsec->ceiling_yoffs    = tempsec->floor_yoffs;
            tempsec->ceiling_xscale     = tempsec->floor_xscale;
            tempsec->ceiling_yscale     = tempsec->floor_yscale;
            tempsec->ceilingbaseangle = tempsec->floorbaseangle; // haleyjd: angles
            tempsec->ceilingangle     = tempsec->floorangle;
         }
         else
         {
            tempsec->ceilingpic       = s->ceilingpic;
            tempsec->ceiling_xoffs    = s->ceiling_xoffs;
            tempsec->ceiling_yoffs    = s->ceiling_yoffs;
            tempsec->ceiling_xscale   = s->ceiling_xscale;
            tempsec->ceiling_yscale   = s->ceiling_yscale;
            tempsec->ceilingbaseangle = s->ceilingbaseangle; // haleyjd: angles
            tempsec->ceilingangle     = s->ceilingangle;
         }

         // haleyjd 03/20/10: must clear SIF_SKY flag from tempsec!
         // ioanch 20160205: not always
         if(!R_IsSkyFlat(tempsec->ceilingpic))
            tempsec->intflags &= ~SIF_SKY;
         else
            tempsec->intflags |= SIF_SKY;

         tempsec->lightlevel  = s->lightlevel;
         
         if(floorlightlevel)
         {
            *floorlightlevel =
            (s->flags & SECF_FLOORLIGHTABSOLUTE ? 0 : s->floorlightsec == -1 ?
             s->lightlevel : sectors[s->floorlightsec].lightlevel)
            + s->floorlightdelta;
            // killough 3/16/98
         }

         if (ceilinglightlevel)
         {
            *ceilinglightlevel =
            (s->flags & SECF_CEILLIGHTABSOLUTE ? 0 : s->ceilinglightsec == -1 ?
             s->lightlevel : sectors[s->ceilinglightsec].lightlevel)
            + s->ceilinglightdelta;
            // killough 4/11/98
         }
      }
      else if(heightsec != -1 && 
              viewz >= sectors[heightsec].ceilingheight &&
              sec->ceilingheight > s->ceilingheight)
      {   
         // SoM: kill any floor portals that may try to render
         tempsec->f_portal = NULL;

         // Above-ceiling hack
         tempsec->ceilingheight = s->ceilingheight;
         tempsec->floorheight   = s->ceilingheight + 1;

         tempsec->floorpic       = tempsec->ceilingpic       = s->ceilingpic;
         tempsec->floor_xoffs    = tempsec->ceiling_xoffs    = s->ceiling_xoffs;
         tempsec->floor_yoffs    = tempsec->ceiling_yoffs    = s->ceiling_yoffs;
         tempsec->floor_xscale   = tempsec->ceiling_xscale   = s->ceiling_xscale;
         tempsec->floor_yscale   = tempsec->ceiling_yscale   = s->ceiling_yscale;
         tempsec->floorbaseangle = tempsec->ceilingbaseangle = s->ceilingbaseangle;
         tempsec->floorangle     = tempsec->ceilingangle     = s->ceilingangle; // haleyjd: angles

         if(!R_IsSkyFlat(s->floorpic))
         {
            tempsec->ceilingheight  = sec->ceilingheight;
            tempsec->floorpic       = s->floorpic;
            tempsec->floor_xoffs    = s->floor_xoffs;
            tempsec->floor_yoffs    = s->floor_yoffs;
            tempsec->floor_xscale   = s->floor_xscale;
            tempsec->floor_yscale   = s->floor_yscale;
            tempsec->floorbaseangle = s->floorbaseangle; // haleyjd: angles
            tempsec->floorangle     = s->floorangle;
         }

         // haleyjd 03/20/10: must clear SIF_SKY flag from tempsec
         // ioanch 20160205: not always
         if(!R_IsSkyFlat(tempsec->ceilingpic))
            tempsec->intflags &= ~SIF_SKY;
         else
            tempsec->intflags |= SIF_SKY;
         
         tempsec->lightlevel  = s->lightlevel;
         
         if(floorlightlevel)
         {
            *floorlightlevel =
            (s->flags & SECF_FLOORLIGHTABSOLUTE ? 0 : s->floorlightsec == -1 ?
             s->lightlevel : sectors[s->floorlightsec].lightlevel)
            + s->floorlightdelta;
            // killough 3/16/98
         }

         if(ceilinglightlevel)
         {
            *ceilinglightlevel =
            (s->flags & SECF_CEILLIGHTABSOLUTE ? 0 : s->ceilinglightsec == -1 ?
             s->lightlevel : sectors[s->ceilinglightsec].lightlevel)
            + s->ceilinglightdelta;
            // killough 4/11/98
         }
      }
      else if(heightsec != -1)
      {
         if(sec->ceilingheight != s->ceilingheight)
            tempsec->c_portal = NULL;
         if(sec->floorheight != s->floorheight)
            tempsec->f_portal = NULL;
      }
      
      tempsec->ceilingheightf = M_FixedToFloat(tempsec->ceilingheight);
      tempsec->floorheightf   = M_FixedToFloat(tempsec->floorheight);
      sec = tempsec;            // Use other sector
   }

   if(sec->c_portal)
   {
      if(sec->c_portal->type == R_LINKED && !(sec->c_pflags & PF_ATTACHEDPORTAL))
      {
         if(sec->ceilingheight < R_CPLink(sec)->planez)
         {
            tempsec->c_portal = NULL;
            tempsec->c_pflags = 0;
         }
         else
            P_SetCeilingHeight(tempsec, R_CPLink(sec)->planez);
         sec = tempsec;
      }
      else if(!(sec->c_pflags & PS_VISIBLE))
      {
         tempsec->c_portal = NULL;
         tempsec->c_pflags = 0;
         sec = tempsec;
      }
   }
   if(sec->f_portal)
   {
      if(sec->f_portal->type == R_LINKED && !(sec->f_pflags & PF_ATTACHEDPORTAL))
      {
         if(sec->floorheight > R_FPLink(sec)->planez)
         {
            tempsec->f_portal = NULL;
            tempsec->f_pflags = 0;
         }
         else
            P_SetFloorHeight(tempsec, R_FPLink(sec)->planez);
            
         sec = tempsec;
      }
      else if(!(sec->f_pflags & PS_VISIBLE))
      {
         tempsec->f_portal = NULL;
         tempsec->f_pflags = 0;
         sec = tempsec;
      }
   }

   return sec;
}

//
// R_ClipSegToPortal
//
// SoM 3/14/2005: This function will reject segs that are completely 
// outside the portal window based on a few conditions. It will also 
// clip the start and stop values of the seg based on what range it is 
// actually visible in. This function is sound and Doom could even use 
// this for normal rendering, but it adds some overhead.
//
// SoM 6/24/2007: Moved this here and rewrote it a bit.
//

// The way we handle segs depends on relative camera position. If the 
// camera is above we need to reject segs based on the top of the seg.
// If the camera is below the bottom of the seg the bottom edge needs 
// to be clipped. This is done so visplanes will still be rendered 
// fully.

static float *slopemark;

VALLOCATION(slopemark)
{
   slopemark = ecalloctag(float *, w, sizeof(float), PU_VALLOC, NULL);
}

void R_ClearSlopeMark(int minx, int maxx, pwindowtype_e type)
{
   int i;

   if(type == pw_floor)
   {
      for(i = minx; i <= maxx; i++)
         slopemark[i] = view.height;
   }
   else if(type == pw_ceiling)
   {
      for(i = minx; i <= maxx; i++)
         slopemark[i] = -1;
   }
}



//
// R_ClipInitialSegRange
//
// Performs initial clipping based on the opening of the portal. This can 
// quickly reject segs that are all the way off the left or right edges 
// of the portal window.
// 
static bool R_ClipInitialSegRange(int *start, int *stop, float *clipx1, float *clipx2)
{
   // SoM: Quickly reject the seg based on the bounding box of the portal
   if(seg.x1 > portalrender.maxx || seg.x2 < portalrender.minx)
      return false;

   // Do initial clipping.
   if(portalrender.minx > seg.x1)
   {
      *start = portalrender.minx;
      *clipx1 = *start - seg.x1frac;
   }
   else
   {
      *start = seg.x1;
      *clipx1 = 0.0;
   }

   if(portalrender.maxx < seg.x2)
   {
      *stop = portalrender.maxx;
      *clipx2 = seg.x2frac - *stop;
   }
   else
   {
      *stop = seg.x2;
      *clipx2 = 0.0;
   }

   if(*start > *stop)
      return false;

   return true;
}

static void R_ClipSegToFPortal(void)
{
   int i, startx;
   float clipx1, clipx2;
   int start, stop;

   if(!R_ClipInitialSegRange(&start, &stop, &clipx1, &clipx2))
      return; 

   if(seg.ceilingplane && seg.ceilingplane->pslope)
   {
      for(i = start; i <= stop; i++)
      {
         // skip past the closed or out of sight columns to find the first 
         // visible column
         while(i <= stop && floorclip[i] - slopemark[i] <= -1.0) i++;

         if(i > stop)
            return;

         startx = i; // mark

         // skip past visible columns 
         while(i <= stop && floorclip[i] - slopemark[i] > -1.0f) i++;

         if(seg.clipsolid)
            R_ClipSolidWallSegment(startx, i - 1);
         else
            R_ClipPassWallSegment(startx, i - 1);
      }
   }
   else
   {
      float top, top2;

      top = seg.top + seg.topstep * clipx1;
      top2 = seg.top2 - seg.topstep * clipx2;

      // SoM: Quickly reject the seg based on the bounding box of the portal
      if(portalrender.maxy - top < -1.0f && portalrender.maxy - top2 < -1.0f)
         return;

      for(i = start; i <= stop; i++)
      {
         // skip past the closed or out of sight columns to find the first visible column
         for(; i <= stop && floorclip[i] - top <= -1.0f; i++)
            top += seg.topstep;

         if(i > stop)
            return;

         startx = i; // mark

         // skip past visible columns 
         for(; i <= stop && floorclip[i] - top > -1.0f; i++)
         {
            slopemark[i] = top;
            top += seg.topstep;
         }

         if(seg.clipsolid)
            R_ClipSolidWallSegment(startx, i - 1);
         else
            R_ClipPassWallSegment(startx, i - 1);
      }
   }
}

static void R_ClipSegToCPortal(void)
{
   int i, startx;
   float clipx1, clipx2;
   int start, stop;

   if(!R_ClipInitialSegRange(&start, &stop, &clipx1, &clipx2))
      return; 

   if(seg.floorplane && seg.floorplane->pslope)
   {
      for(i = start; i <= stop; i++)
      {
         while(i <= stop && slopemark[i] < ceilingclip[i]) i++;

         if(i > stop)
            return;

         startx = i;

         while(i <= stop && slopemark[i] >= ceilingclip[i]) i++;

         if(seg.clipsolid)
            R_ClipSolidWallSegment(startx, i - 1);
         else
            R_ClipPassWallSegment(startx, i - 1);
      }
   }
   else
   {
      float bottom, bottom2;

      bottom = seg.bottom + seg.bottomstep * clipx1;
      bottom2 = seg.bottom2 - seg.bottomstep * clipx2;

      // SoM: Quickly reject the seg based on the bounding box of the portal
      if(bottom < portalrender.miny && bottom2 < portalrender.miny)
         return;

      for(i = start; i <= stop; i++)
      {
         for(; i <= stop && bottom < ceilingclip[i]; i++)
            bottom += seg.bottomstep;

         if(i > stop)
            return;

         startx = i;

         for(; i <= stop && bottom >= ceilingclip[i]; i++)
         {
            slopemark[i] = bottom;
            bottom += seg.bottomstep;
         }

         if(seg.clipsolid)
            R_ClipSolidWallSegment(startx, i - 1);
         else
            R_ClipPassWallSegment(startx, i - 1);
      }
   }
}

static void R_ClipSegToLPortal(void)
{
   int i, startx;
   float clipx1, clipx2;
   int start, stop;

   // Line based portal. This requires special clipping...
   if(!R_ClipInitialSegRange(&start, &stop, &clipx1, &clipx2))
      return; 

   // This can actually happen with slopes!
   if(!seg.floorplane && !seg.ceilingplane && !seg.f_window && !seg.c_window)
   {
      float top, top2, topstep, bottom, bottom2, bottomstep;

      bottom = seg.bottom + seg.bottomstep * clipx1;
      bottom2 = seg.bottom2 - seg.bottomstep * clipx2;

      // SoM: Quickly reject the seg based on the bounding box of the portal
      if(bottom < portalrender.miny && bottom2 < portalrender.miny)
         return;

      bottomstep = seg.bottomstep;

      top = seg.top + seg.topstep * clipx1;
      top2 = seg.top2 - seg.topstep * clipx2;

      // SoM: Quickly reject the seg based on the bounding box of the portal
      if(portalrender.maxy - top < -1.0f && portalrender.maxy - top2 < -1.0f)
         return;

      topstep = seg.topstep;
      
      for(i = start; i <= stop; i++)
      {
         for(; i <= stop && (bottom < ceilingclip[i] || floorclip[i] - top <= -1.0f); i++)
         {
            bottom += bottomstep;
            top += topstep;
         }

         if(i > stop)
            return;

         startx = i;

         for(; i <= stop && bottom >= ceilingclip[i] && floorclip[i] - top > -1.0f; i++)
         {
            bottom += bottomstep;
            top += topstep;
         }

         if(seg.clipsolid)
            R_ClipSolidWallSegment(startx, i - 1);
         else
            R_ClipPassWallSegment(startx, i - 1);
      }
   }
   else if(!seg.floorplane && !seg.f_window)
   {
      // If the seg has no floor plane, the camera is most likely below it,
      // so rejection is carried out as if the seg is being viewed through
      // a ceiling portal.

      float bottom, bottom2, bottomstep;

      bottom = seg.bottom + seg.bottomstep * clipx1;
      bottom2 = seg.bottom2 - seg.bottomstep * clipx2;

      // SoM: Quickly reject the seg based on the bounding box of the portal
      if(bottom < portalrender.miny && bottom2 < portalrender.miny)
         return;

      bottomstep = seg.bottomstep;

      for(i = start; i <= stop; i++)
      {
         for(; i <= stop && bottom < ceilingclip[i]; i++)
            bottom += bottomstep;

         if(i > stop)
            return;

         startx = i;

         for(; i <= stop && bottom >= ceilingclip[i]; i++)
            bottom += bottomstep;

         if(seg.clipsolid)
            R_ClipSolidWallSegment(startx, i - 1);
         else
            R_ClipPassWallSegment(startx, i - 1);
      }
   }
   else if(!seg.ceilingplane && !seg.c_window)
   {
      // If the seg has no floor plane, the camera is most likely above it,
      // so rejection is carried out as if the seg is being viewed through
      // a floor portal.

      float top, top2, topstep;

      // I totally overlooked this when I moved all the wall panel projection 
      // to r_segs.c
      top = seg.top + seg.topstep * clipx1;
      top2 = seg.top2 - seg.topstep * clipx2;

      // SoM: Quickly reject the seg based on the bounding box of the portal
      if(portalrender.maxy - top < -1.0f && portalrender.maxy - top2 < -1.0f)
         return;

      topstep = seg.topstep;

      for(i = start; i <= stop; i++)
      {
         // skip past the closed or out of sight columns to find the first 
         // visible column
         for(; i <= stop && floorclip[i] - top <= -1.0f; i++)
            top += topstep;

         if(i > stop)
            return;

         startx = i; // mark

         // skip past visible columns 
         for(; i <= stop && floorclip[i] - top > -1.0f; i++)
            top += topstep;

         if(seg.clipsolid)
            R_ClipSolidWallSegment(startx, i - 1);
         else
            R_ClipPassWallSegment(startx, i - 1);
      }
   }
   else
   {
      // Seg is most likely being viewed from straight on...
      for(i = start; i <= stop; i++)
      {
         while(i <= stop && floorclip[i] < ceilingclip[i]) i++;

         if(i > stop)
            return;

         startx = i;
         while(i <= stop && floorclip[i] >= ceilingclip[i]) i++;

         if(seg.clipsolid)
            R_ClipSolidWallSegment(startx, i - 1);
         else
            R_ClipPassWallSegment(startx, i - 1);
      }
   }
}



R_ClipSegFunc segclipfuncs[] = 
{
   R_ClipSegToFPortal,
   R_ClipSegToCPortal,
   R_ClipSegToLPortal
};

#define NEARCLIP 0.05f
#define PNEARCLIP 0.001f

static void R_2S_Sloped(float pstep, float i1, float i2, float textop, 
                        float texbottom, vertex_t *v1, vertex_t *v2, 
                        float lclip1, float lclip2)
{
   bool mark, markblend; // haleyjd
   // ioanch: needed to prevent transfer_heights from affecting sky hacks.
   bool marktheight, blocktheight;
   bool heightchange;
   float texhigh, texlow;
   side_t *side = seg.side;
   seg_t  *line = seg.line;

   int    h, h2, l, l2, t, t2, b, b2;

   seg.twosided = true;

   // haleyjd 03/04/07: reformatted and eliminated numerous unnecessary
   // conditional statements (predicates already provide the needed
   // true/false value without a branch). Also added tweak for sector
   // colormaps.

   // ioanch 20160130: be sure to check for portal line too!
   mark = (seg.frontsec->lightlevel != seg.backsec->lightlevel ||
           seg.line->linedef->portal ||
           seg.frontsec->midmap != seg.backsec->midmap ||
           (seg.line->sidedef->midtexture &&
            (seg.line->linedef->extflags & EX_ML_CLIPMIDTEX)));
   marktheight = seg.frontsec->heightsec != seg.backsec->heightsec;

   t = (int)seg.top;
   t2 = (int)seg.top2;
   b = (int)seg.bottom;
   b2 = (int)seg.bottom2;

   if(seg.backsec->c_slope)
   {
      float z1, z2, zstep;

      z1 = P_GetZAtf(seg.backsec->c_slope, v1->fx, v1->fy);
      z2 = P_GetZAtf(seg.backsec->c_slope, v2->fx, v2->fy);
      zstep = (z2 - z1) / seg.line->len;

      z1 += lclip1 * zstep;
      z2 -= (seg.line->len - lclip2) * zstep;

      seg.high = view.ycenter - ((z1 - view.z) * i1) - 1.0f;
      seg.high2 = view.ycenter - ((z2 - view.z) * i2) - 1.0f;
   }
   else
   {
      seg.high = view.ycenter - ((seg.backsec->ceilingheightf - view.z) * i1) - 1.0f;
      seg.high2 = view.ycenter - ((seg.backsec->ceilingheightf - view.z) * i2) - 1.0f;
   }
   seg.highstep = (seg.high2 - seg.high) * pstep;

   // SoM: Get this from the actual sector because R_FakeFlat can mess with heights.
   texhigh = seg.line->backsector->ceilingheightf - view.z;

   if(seg.backsec->f_slope)
   {
      float z1, z2, zstep;

      z1 = P_GetZAtf(seg.backsec->f_slope, v1->fx, v1->fy);
      z2 = P_GetZAtf(seg.backsec->f_slope, v2->fx, v2->fy);
      zstep = (z2 - z1) / seg.line->len;

      z1 += lclip1 * zstep;
      z2 -= (seg.line->len - lclip2) * zstep;
      seg.low = view.ycenter - ((z1 - view.z) * i1);
      seg.low2 = view.ycenter - ((z2 - view.z) * i2);
   }
   else
   {
      seg.low = view.ycenter - ((seg.backsec->floorheightf - view.z) * i1);
      seg.low2 = view.ycenter - ((seg.backsec->floorheightf - view.z) * i2);
   }
   seg.lowstep = (seg.low2 - seg.low) * pstep;


   h = (int)seg.high;
   h2 = (int)seg.high2;
   l = (int)seg.low;
   l2 = (int)seg.low2;

   // TODO: Fix for use with portals.
   // Shouldn't this be after the skyflat check?
   // ^ No, because the opening is checked against either top or high,
   // and the behavior of the sky hack needs to change if the line is closed.
   seg.clipsolid = (b <= h && b2 <= h2) ||
                   (l <= t && l2 <= t2) ||
                   (l <= h && l2 <= h2);

   if(seg.frontsec->intflags & SIF_SKY && seg.backsec->intflags & SIF_SKY)
   {
      // If the line is clipped solid, it can't be assumed that the upper 
      // texture portion meets up with an adjacent ceiling flat.
      if(seg.clipsolid)
      {
         seg.high += 1.0f;
         seg.high2 += 1.0f;
         h++; h2++;
      }

      seg.top = seg.high;
      seg.top2 = seg.high2;
      seg.topstep = seg.highstep;
      t = h; t2 = h2;

      blocktheight = true;
   }
   else
      blocktheight = false;


   // -- Ceilings -- 
   // SoM: TODO: Float comparisons should be done within an epsilon
   heightchange = seg.frontsec->c_slope || seg.backsec->c_slope ? 
                  (t != h || t2 != h2) :
                  (seg.backsec->ceilingheight != seg.frontsec->ceilingheight);

   seg.markflags = 0;
   
   markblend = seg.frontsec->c_portal != NULL 
         && seg.backsec->c_portal != NULL 
         && (seg.frontsec->c_pflags & PS_BLENDFLAGS) != (seg.backsec->c_pflags & PS_BLENDFLAGS);

   if(seg.c_portal)
   {
      if(seg.clipsolid || heightchange ||
         seg.frontsec->c_portal != seg.backsec->c_portal)
      {
         seg.markflags |= SEG_MARKCPORTAL;
         seg.c_window   = R_GetCeilingPortalWindow(seg.frontsec->c_portal,
                                                   seg.frontsec->ceilingheight);
         R_MovePortalOverlayToWindow(true);
      }
      else if(!heightchange && seg.frontsec->c_portal == seg.backsec->c_portal)
      {
         seg.c_window   = R_GetCeilingPortalWindow(seg.frontsec->c_portal,
                                                   seg.frontsec->ceilingheight);
         R_MovePortalOverlayToWindow(true);
         seg.c_window = nullptr;
      }
      else
         seg.c_window = nullptr;
   }
   else
      seg.c_window   = NULL;

   if(seg.ceilingplane && 
       (mark || (marktheight && !blocktheight) || seg.clipsolid || heightchange ||
        seg.frontsec->ceiling_xoffs != seg.backsec->ceiling_xoffs ||
        seg.frontsec->ceiling_yoffs != seg.backsec->ceiling_yoffs ||
        seg.frontsec->ceiling_xscale != seg.backsec->ceiling_xscale ||
        seg.frontsec->ceiling_yscale != seg.backsec->ceiling_yscale ||
        (seg.frontsec->ceilingbaseangle + seg.frontsec->ceilingangle !=
         seg.backsec->ceilingbaseangle + seg.backsec->ceilingangle) || // haleyjd: angles
        seg.frontsec->ceilingpic != seg.backsec->ceilingpic ||
        seg.frontsec->ceilinglightsec != seg.backsec->ceilinglightsec ||
        seg.frontsec->ceilinglightdelta != seg.backsec->ceilinglightdelta ||
        (seg.frontsec->flags & SECF_CEILLIGHTABSOLUTE) !=
         (seg.backsec->flags & SECF_CEILLIGHTABSOLUTE) ||
        seg.frontsec->topmap != seg.backsec->topmap ||
        seg.frontsec->c_portal != seg.backsec->c_portal ||
        !R_CompareSlopes(seg.frontsec->c_slope, seg.backsec->c_slope) || markblend)) // haleyjd
   {
      seg.markflags |= seg.c_portal ? SEG_MARKCOVERLAY : SEG_MARKCEILING;
   }

   bool havetportal = seg.backsec && seg.backsec->c_portal &&
         seg.line->linedef->extflags & EX_ML_UPPERPORTAL;

   bool toohigh = havetportal && portalrender.w &&
   portalrender.w->type == pw_floor && portalrender.w->portal->type != R_SKYBOX &&
   portalrender.w->planez + viewz - portalrender.w->vz <= seg.backsec->ceilingheight;

   if(!toohigh && !havetportal && heightchange && 
      !(seg.frontsec->intflags & SIF_SKY && seg.backsec->intflags & SIF_SKY) && 
      side->toptexture)
   {
      seg.toptex = texturetranslation[side->toptexture];
      seg.toptexh = textures[side->toptexture]->height;

      if(seg.line->linedef->flags & ML_DONTPEGTOP)
         seg.toptexmid = M_FloatToFixed(textop + seg.toffsety);
      else
         seg.toptexmid = M_FloatToFixed(texhigh + seg.toptexh + seg.toffsety);
   }
   else
      seg.toptex = 0;

   if(!toohigh && havetportal && heightchange)
   {
      seg.t_window = R_GetLinePortalWindow(seg.backsec->c_portal, line->linedef);
      seg.segtextured = true;
   }
   else
      seg.t_window = nullptr;

   // -- Floors -- 
   // SoM: TODO: Float comparisons should be done within an epsilon
   heightchange = seg.frontsec->f_slope || seg.backsec->f_slope ? (l != b || l2 != b2) :
                  seg.backsec->floorheight != seg.frontsec->floorheight;

   markblend = seg.frontsec->f_portal != NULL 
         && seg.backsec->f_portal != NULL 
         && (seg.frontsec->f_pflags & PS_BLENDFLAGS) != (seg.backsec->f_pflags & PS_BLENDFLAGS);

   if(seg.f_portal)
   {
      if(seg.clipsolid || heightchange ||
         seg.frontsec->f_portal != seg.backsec->f_portal)
      {
         seg.markflags |= SEG_MARKFPORTAL;
         seg.f_window   = R_GetFloorPortalWindow(seg.frontsec->f_portal,
                                                 seg.frontsec->floorheight);
         R_MovePortalOverlayToWindow(false);
      }
      else if(!heightchange && seg.frontsec->f_portal == seg.backsec->f_portal)
      {
         seg.f_window   = R_GetFloorPortalWindow(seg.frontsec->f_portal,
                                                 seg.frontsec->floorheight);
         R_MovePortalOverlayToWindow(false);
         seg.f_window = nullptr;
      }
      else
         seg.f_window = nullptr;
   }
   else
      seg.f_window = NULL;

   if(seg.floorplane && 
      (mark || marktheight || seg.clipsolid || heightchange ||
       seg.frontsec->floor_xoffs != seg.backsec->floor_xoffs ||
       seg.frontsec->floor_yoffs != seg.backsec->floor_yoffs ||
       seg.frontsec->floor_xscale != seg.backsec->floor_xscale ||
       seg.frontsec->floor_yscale != seg.backsec->floor_yscale ||
       (seg.frontsec->floorbaseangle + seg.frontsec->floorangle != 
        seg.backsec->floorbaseangle + seg.backsec->floorangle) || // haleyjd: angles
       seg.frontsec->floorpic != seg.backsec->floorpic ||
       seg.frontsec->floorlightsec != seg.backsec->floorlightsec ||
       seg.frontsec->floorlightdelta != seg.backsec->floorlightdelta ||
       (seg.frontsec->flags & SECF_FLOORLIGHTABSOLUTE) !=
        (seg.backsec->flags & SECF_FLOORLIGHTABSOLUTE) ||
       seg.frontsec->bottommap != seg.backsec->bottommap ||
       seg.frontsec->f_portal != seg.backsec->f_portal ||
       !R_CompareSlopes(seg.frontsec->f_slope, seg.backsec->f_slope) || markblend)) // haleyjd
   {
      seg.markflags |= seg.f_portal ? SEG_MARKFOVERLAY : SEG_MARKFLOOR;
   }

   // SoM: some portal types should be rendered even if the player is above
   // or below the ceiling or floor plane.
   // haleyjd 03/12/06: inverted predicates to simplify
   if(seg.backsec->f_portal != seg.frontsec->f_portal)
   {
      if(seg.frontsec->f_portal && 
         seg.frontsec->f_portal->type != R_LINKED &&
         seg.frontsec->f_portal->type != R_TWOWAY)
         seg.f_portalignore = true;
   }

   if(seg.backsec->c_portal != seg.frontsec->c_portal)
   {
      if(seg.frontsec->c_portal && 
         seg.frontsec->c_portal->type != R_LINKED &&
         seg.frontsec->c_portal->type != R_TWOWAY)
         seg.c_portalignore = true;
   }

   bool havebportal = seg.backsec && seg.backsec->f_portal &&
         seg.line->linedef->extflags & EX_ML_LOWERPORTAL;
   bool toolow = havebportal && portalrender.w &&
   portalrender.w->type == pw_ceiling && portalrender.w->portal->type != R_SKYBOX &&
   portalrender.w->planez + viewz - portalrender.w->vz >= seg.backsec->floorheight;

   // SoM: Get this from the actual sector because R_FakeFlat can mess with heights.

   texlow = seg.line->backsector->floorheightf - view.z;
   if(!toolow && !havebportal && (b > l || b2 > l2) && side->bottomtexture)
   {
      seg.bottomtex = texturetranslation[side->bottomtexture];
      seg.bottomtexh = textures[side->bottomtexture]->height;

      if(seg.line->linedef->flags & ML_DONTPEGBOTTOM)
         seg.bottomtexmid = M_FloatToFixed(textop + seg.toffsety);
      else
         seg.bottomtexmid = M_FloatToFixed(texlow + seg.toffsety);
   }
   else
      seg.bottomtex = 0;

   seg.midtex = 0;
   seg.maskedtex = !!seg.side->midtexture;
   seg.segtextured = (seg.maskedtex || seg.bottomtex || seg.toptex || seg.t_window);

   if(line->linedef->portal && //line->linedef->sidenum[0] != line->linedef->sidenum[1] &&
      line->linedef->sidenum[0] == line->sidedef - sides)
   {
      seg.l_window = R_GetLinePortalWindow(line->linedef->portal, line->linedef);
      seg.clipsolid = true;
   }
   else
      seg.l_window = NULL;

   if(!toolow && havebportal && (b > l || b2 > l2))
   {
      seg.b_window = R_GetLinePortalWindow(seg.backsec->f_portal, line->linedef);
      seg.segtextured = true;
   }
   else
      seg.b_window = nullptr;
}

static void R_2S_Normal(float pstep, float i1, float i2, float textop, 
                        float texbottom)
{
   bool mark, markblend; // haleyjd
   // ioanch: needed to prevent transfer_heights from affecting sky hacks.
   bool marktheight, blocktheight;
   bool uppermissing, lowermissing;
   float texhigh, texlow;
   side_t *side = seg.side;
   seg_t  *line = seg.line;
   fixed_t frontc, backc;

   seg.twosided = true;

   // haleyjd 03/04/07: reformatted and eliminated numerous unnecessary
   // conditional statements (predicates already provide the needed
   // true/false value without a branch). Also added tweak for sector
   // colormaps.

   // ioanch 20160130: be sure to check for portal line too!
   mark = (seg.frontsec->lightlevel != seg.backsec->lightlevel ||
           seg.line->linedef->portal ||
           seg.frontsec->midmap != seg.backsec->midmap ||
           (seg.line->sidedef->midtexture && 
            (seg.line->linedef->extflags & EX_ML_CLIPMIDTEX)));
   marktheight = seg.frontsec->heightsec != seg.backsec->heightsec;

   frontc = seg.frontsec->ceilingheight;
   backc  = seg.backsec->ceilingheight;

   seg.high  = view.ycenter - ((seg.backsec->ceilingheightf - view.z) * i1) - 1.0f;
   seg.high2 = view.ycenter - ((seg.backsec->ceilingheightf - view.z) * i2) - 1.0f;
   seg.highstep = (seg.high2 - seg.high) * pstep;

   // SoM: Get this from the actual sector because R_FakeFlat can mess with heights.
   texhigh = seg.line->backsector->ceilingheightf - view.z;

   uppermissing = (seg.frontsec->ceilingheight > seg.backsec->ceilingheight &&
                   seg.side->toptexture == 0);

   lowermissing = (seg.frontsec->floorheight < seg.backsec->floorheight &&
                   seg.side->bottomtexture == 0);

   bool portaltouch = portalrender.active &&
   ((portalrender.w->type == pw_floor && (portalrender.w->planez ==
                                          seg.backsec->floorheight ||
                                          portalrender.w->planez ==
                                          seg.frontsec->floorheight)) ||
    (portalrender.w->type == pw_ceiling && (portalrender.w->planez ==
                                            seg.backsec->ceilingheight ||
                                            portalrender.w->planez ==
                                            seg.frontsec->ceilingheight)));

   // New clipsolid code will emulate the old doom behavior and still manages to 
   // keep valid closed door cases handled.
   seg.clipsolid = !portaltouch && ((seg.backsec->floorheight !=
                                     seg.frontsec->floorheight ||
       seg.backsec->ceilingheight != seg.frontsec->ceilingheight) &&
       (seg.backsec->floorheight >= seg.frontsec->ceilingheight ||
        seg.backsec->ceilingheight <= seg.frontsec->floorheight ||
        (seg.backsec->ceilingheight <= seg.backsec->floorheight && 
         !uppermissing && !lowermissing)));

   // This was moved here because the behavior changes based on the value of seg.clipsolid.
   if(seg.frontsec->intflags & SIF_SKY && seg.backsec->intflags & SIF_SKY)
   {
      if(seg.clipsolid)
      {
         // SoM: Because this is solid, the upper texture can't be assumed to be
         // connecting to any further ceiling flats.
         seg.high  += 1.0f;
         seg.high2 += 1.0f;
      }

      seg.top  = seg.high;
      seg.top2 = seg.high2;

      seg.topstep = seg.highstep;
      frontc = backc;
      //uppermissing = false;
      blocktheight = true; // apply the hack of ignoring sector
   }
   else
      blocktheight = false;

   seg.markflags = 0;
   
   markblend = seg.frontsec->c_portal != NULL 
            && seg.backsec->c_portal != NULL 
            && (seg.frontsec->c_pflags & PS_BLENDFLAGS) != (seg.backsec->c_pflags & PS_BLENDFLAGS);
               
   if(mark || (marktheight && !blocktheight) || seg.clipsolid || frontc != backc || 
      seg.frontsec->ceiling_xoffs != seg.backsec->ceiling_xoffs ||
      seg.frontsec->ceiling_yoffs != seg.backsec->ceiling_yoffs ||
      seg.frontsec->ceiling_xscale != seg.backsec->ceiling_xscale ||
      seg.frontsec->ceiling_yscale != seg.backsec->ceiling_yscale ||
      (seg.frontsec->ceilingbaseangle + seg.frontsec->ceilingangle !=
       seg.backsec->ceilingbaseangle + seg.backsec->ceilingangle) || // haleyjd: angles
      seg.frontsec->ceilingpic != seg.backsec->ceilingpic ||
      seg.frontsec->ceilinglightsec != seg.backsec->ceilinglightsec ||
      seg.frontsec->ceilinglightdelta != seg.backsec->ceilinglightdelta ||
      (seg.frontsec->flags & SECF_CEILLIGHTABSOLUTE) !=
       (seg.backsec->flags & SECF_CEILLIGHTABSOLUTE) ||
      seg.frontsec->topmap != seg.backsec->topmap ||
      seg.frontsec->c_portal != seg.backsec->c_portal || markblend) // haleyjd
   {
      seg.markflags |= seg.c_portal ? SEG_MARKCOVERLAY : 
                    seg.ceilingplane ? SEG_MARKCEILING : 0;
   }

   if(seg.c_portal)
   {
      if(seg.clipsolid ||
         seg.frontsec->ceilingheight != seg.backsec->ceilingheight ||
         seg.frontsec->c_portal != seg.backsec->c_portal)
      {
         seg.markflags |= SEG_MARKCPORTAL;
         seg.c_window   = R_GetCeilingPortalWindow(seg.frontsec->c_portal,
                                                   seg.frontsec->ceilingheight);
         R_MovePortalOverlayToWindow(true);
      }
      else if(seg.frontsec->c_portal == seg.backsec->c_portal &&
              seg.frontsec->ceilingheight == seg.backsec->ceilingheight)
      {
         // We need to do this just to transfer the plane
         seg.c_window = R_GetCeilingPortalWindow(seg.frontsec->c_portal,
                                                 seg.frontsec->ceilingheight);
         R_MovePortalOverlayToWindow(true);
         seg.c_window = nullptr;
      }
      else
         seg.c_window = nullptr;
   }
   else
      seg.c_window = NULL;

   bool havetportal = seg.backsec && seg.backsec->c_portal &&
         seg.line->linedef->extflags & EX_ML_UPPERPORTAL;
   bool toohigh = havetportal && portalrender.w &&
   portalrender.w->type == pw_floor && portalrender.w->portal->type != R_SKYBOX &&
   portalrender.w->planez + viewz - portalrender.w->vz <= seg.backsec->ceilingheight;

   if(!toohigh && !havetportal &&
      seg.frontsec->ceilingheight > seg.backsec->ceilingheight &&
     !(seg.frontsec->intflags & SIF_SKY && seg.backsec->intflags & SIF_SKY) && 
      side->toptexture)
   {
      seg.toptex = texturetranslation[side->toptexture];
      seg.toptexh = textures[side->toptexture]->height;

      if(seg.line->linedef->flags & ML_DONTPEGTOP)
         seg.toptexmid = M_FloatToFixed(textop + seg.toffsety);
      else
         seg.toptexmid = M_FloatToFixed(texhigh + seg.toptexh + seg.toffsety);
   }
   else
      seg.toptex = 0;

   if(!toohigh && havetportal &&
      seg.frontsec->ceilingheight > seg.backsec->ceilingheight)
   {
      seg.t_window = R_GetLinePortalWindow(seg.backsec->c_portal, line->linedef);
      seg.segtextured = true;
   }
   else
      seg.t_window = nullptr;

   markblend = seg.frontsec->f_portal != NULL
            && seg.backsec->f_portal != NULL 
            && (seg.frontsec->f_pflags & PS_BLENDFLAGS) != (seg.backsec->f_pflags & PS_BLENDFLAGS);
             
   if(mark || marktheight || seg.clipsolid ||  
      seg.frontsec->floorheight != seg.backsec->floorheight ||
      seg.frontsec->floor_xoffs != seg.backsec->floor_xoffs ||
      seg.frontsec->floor_yoffs != seg.backsec->floor_yoffs ||
      seg.frontsec->floor_xscale != seg.backsec->floor_xscale ||
      seg.frontsec->floor_yscale != seg.backsec->floor_yscale ||
      (seg.frontsec->floorbaseangle + seg.frontsec->floorangle !=
       seg.backsec->floorbaseangle + seg.backsec->floorangle) || // haleyjd
      seg.frontsec->floorpic != seg.backsec->floorpic ||
      seg.frontsec->floorlightsec != seg.backsec->floorlightsec ||
      seg.frontsec->floorlightdelta != seg.backsec->floorlightdelta ||
      (seg.frontsec->flags & SECF_FLOORLIGHTABSOLUTE) !=
       (seg.backsec->flags & SECF_FLOORLIGHTABSOLUTE) ||
      seg.frontsec->bottommap != seg.backsec->bottommap ||
      seg.frontsec->f_portal != seg.backsec->f_portal || 
      markblend) // haleyjd
   {
      seg.markflags |= seg.f_portal ? SEG_MARKFOVERLAY : 
                       seg.floorplane ? SEG_MARKFLOOR : 0;
   }
   
   if(seg.f_portal)
   {
      if(seg.clipsolid ||
         seg.frontsec->floorheight != seg.backsec->floorheight ||
         seg.frontsec->f_portal != seg.backsec->f_portal)
      {
         seg.markflags |= SEG_MARKFPORTAL;
         seg.f_window   = R_GetFloorPortalWindow(seg.frontsec->f_portal,
                                                 seg.frontsec->floorheight);
         R_MovePortalOverlayToWindow(false);
      }
      else if(seg.frontsec->floorheight == seg.backsec->floorheight &&
              seg.frontsec->f_portal == seg.backsec->f_portal)
      {
         // We need to do this just to transfer the plane
         seg.f_window = R_GetFloorPortalWindow(seg.frontsec->f_portal,
                                               seg.frontsec->floorheight);
         R_MovePortalOverlayToWindow(false);
         seg.f_window = nullptr;
      }
      else
         seg.f_window = nullptr;
   }
   else
      seg.f_window = NULL;

   // SoM: some portal types should be rendered even if the player is above
   // or below the ceiling or floor plane.
   // haleyjd 03/12/06: inverted predicates to simplify
   if(seg.backsec->f_portal != seg.frontsec->f_portal)
   {
      if(seg.frontsec->f_portal && 
         seg.frontsec->f_portal->type != R_LINKED &&
         seg.frontsec->f_portal->type != R_TWOWAY)
         seg.f_portalignore = true;
   }

   if(seg.backsec->c_portal != seg.frontsec->c_portal)
   {
      if(seg.frontsec->c_portal && 
         seg.frontsec->c_portal->type != R_LINKED &&
         seg.frontsec->c_portal->type != R_TWOWAY)
         seg.c_portalignore = true;
   }

   seg.low  = view.ycenter - ((seg.backsec->floorheightf - view.z) * i1);
   seg.low2 = view.ycenter - ((seg.backsec->floorheightf - view.z) * i2);
   seg.lowstep = (seg.low2 - seg.low) * pstep;

   // ioanch: don't render lower textures or portals if they're below the
   // current plane-z window. Necessary for edge portals
   bool havebportal = seg.backsec && seg.backsec->f_portal &&
         seg.line->linedef->extflags & EX_ML_LOWERPORTAL;
   bool toolow = havebportal && portalrender.w &&
   portalrender.w->type == pw_ceiling && portalrender.w->portal->type != R_SKYBOX &&
   portalrender.w->planez + viewz - portalrender.w->vz >= seg.backsec->floorheight;

   // SoM: Get this from the actual sector because R_FakeFlat can mess with heights.

   texlow = seg.line->backsector->floorheightf - view.z;
   if(!toolow && !havebportal
      && seg.frontsec->floorheight < seg.backsec->floorheight
      && side->bottomtexture)
   {
      seg.bottomtex  = texturetranslation[side->bottomtexture];
      seg.bottomtexh = textures[side->bottomtexture]->height;

      if(seg.line->linedef->flags & ML_DONTPEGBOTTOM)
         seg.bottomtexmid = M_FloatToFixed(textop + seg.toffsety);
      else
         seg.bottomtexmid = M_FloatToFixed(texlow + seg.toffsety);
   }
   else
      seg.bottomtex = 0;

   seg.midtex = 0;
   seg.maskedtex = !!seg.side->midtexture;
   seg.segtextured = (seg.maskedtex || seg.bottomtex || seg.toptex || seg.t_window);

   if(line->linedef->portal && //line->linedef->sidenum[0] != line->linedef->sidenum[1] &&
      line->linedef->sidenum[0] == line->sidedef - sides)
   {
      seg.l_window = R_GetLinePortalWindow(line->linedef->portal, line->linedef);
      seg.clipsolid = true;
   }
   else
      seg.l_window = NULL;

   if(!toolow && havebportal &&
      seg.frontsec->floorheight < seg.backsec->floorheight)
   {
      seg.b_window = R_GetLinePortalWindow(seg.backsec->f_portal, line->linedef);
      seg.segtextured = true;
   }
   else
      seg.b_window = nullptr;
}

inline static const bool tooclose(fixed_t n1, fixed_t n2)
{
   return D_abs(n1 - n2) < 256;
}

//
// Checks if a line is behind a portal-generated divline (barrier)
//
static bool R_allowBehindDivline(const dlnormal_t &dln, const seg_t *renderSeg,
                                 bool reverse = false)
{
   divline_t rend;
   if(!reverse)
   {
      rend.x = renderSeg->v1->x;
      rend.y = renderSeg->v1->y;
      rend.dx = renderSeg->v2->x - rend.x;
      rend.dy = renderSeg->v2->y - rend.y;
   }
   else  // this may be needed sometimes
   {
      rend.x = renderSeg->v2->x;
      rend.y = renderSeg->v2->y;
      rend.dx = renderSeg->v1->x - rend.x;
      rend.dy = renderSeg->v1->y - rend.y;
   }

   // HACK: pull render-seg to me as a slack to avoid on-line points
   rend.x += M_FloatToFixed(dln.nx * kPortalSegRejectionFudge);
   rend.y += M_FloatToFixed(dln.ny * kPortalSegRejectionFudge);

   const divline_t &dl = dln.dl;

   int p1 = P_PointOnDivlineSide(rend.x, rend.y, &dl);
   int p2 = P_PointOnDivlineSide(rend.x + rend.dx, rend.y + rend.dy, &dl);

   if(p1 == p2)
      return p1 == 1;   // only accept if behind the barrier line

   // Check cases where vertices are common
   if(tooclose(dl.x, rend.x) && tooclose(dl.y, rend.y))
      return P_PointOnDivlineSide(dl.x + dl.dx, dl.y + dl.dy, &rend) == 0;
   if(tooclose(dl.x + dl.dx, rend.x + rend.dx) && 
      tooclose(dl.y + dl.dy, rend.y + rend.dy))
   {
      return P_PointOnDivlineSide(dl.x, dl.y, &rend) == 0;
   }

   // At least one point must be in front of the rendered line
   return P_PointOnDivlineSide(dl.x, dl.y, &rend) == 0 || 
      P_PointOnDivlineSide(dl.x + dl.dx, dl.y + dl.dy, &rend) == 0;
}

//
// Picks the two bounding box lines pointed towards the viewer.
//
bool R_PickNearestBoxLines(const fixed_t bbox[4], dlnormal_t &dl1,
                           dlnormal_t &dl2, slopetype_t *slope)
{
   dl2.dl.x = D_MAXINT;
   if(viewx < bbox[BOXLEFT])
   {
      if(viewy < bbox[BOXBOTTOM])
      {
         dl1.dl.x = bbox[BOXLEFT];
         dl1.dl.y = bbox[BOXTOP];
         dl1.dl.dx = 0;
         dl1.dl.dy = bbox[BOXBOTTOM] - bbox[BOXTOP];
         dl1.nx = -1;
         dl1.ny = 0;

         dl2.dl.x = bbox[BOXLEFT];
         dl2.dl.y = bbox[BOXBOTTOM];
         dl2.dl.dx = bbox[BOXRIGHT] - bbox[BOXLEFT];
         dl2.dl.dy = 0;
         dl2.nx = 0;
         dl2.ny = -1;

         if(slope)
            *slope = ST_POSITIVE;
      }
      else if(viewy > bbox[BOXTOP])
      {
         // The divlines MUST be left to right relative to view.
         dl1.dl.x = bbox[BOXRIGHT];
         dl1.dl.y = bbox[BOXTOP];
         dl1.dl.dx = bbox[BOXLEFT] - bbox[BOXRIGHT];
         dl1.dl.dy = 0;
         dl1.nx = 0;
         dl1.ny = 1;

         dl2.dl.x = bbox[BOXLEFT];
         dl2.dl.y = bbox[BOXTOP];
         dl2.dl.dx = 0;
         dl2.dl.dy = bbox[BOXBOTTOM] - bbox[BOXTOP];
         dl2.nx = -1;
         dl2.ny = 0;

         if(slope)
            *slope = ST_NEGATIVE;
      }
      else
      {
         dl1.dl.x = bbox[BOXLEFT];
         dl1.dl.y = bbox[BOXTOP];
         dl1.dl.dx = 0;
         dl1.dl.dy = bbox[BOXBOTTOM] - bbox[BOXTOP];
         dl1.nx = -1;
         dl1.ny = 0;

         if(slope)
            *slope = ST_VERTICAL;
      }
   }
   else if(viewx <= bbox[BOXRIGHT])
   {
      if(viewy < bbox[BOXBOTTOM])
      {
         dl1.dl.x = bbox[BOXLEFT];
         dl1.dl.y = bbox[BOXBOTTOM];
         dl1.dl.dx = bbox[BOXRIGHT] - bbox[BOXLEFT];
         dl1.dl.dy = 0;
         dl1.nx = 0;
         dl1.ny = -1;

      }
      else if(viewy <= bbox[BOXTOP])
         return false;   // if actor is below portal, just render everything
      else
      {
         dl1.dl.x = bbox[BOXRIGHT];
         dl1.dl.y = bbox[BOXTOP];
         dl1.dl.dx = bbox[BOXLEFT] - bbox[BOXRIGHT];
         dl1.dl.dy = 0;
         dl1.nx = 0;
         dl1.ny = 1;
      }

      if(slope)
         *slope = ST_HORIZONTAL;
   }
   else
   {
      if(viewy < bbox[BOXBOTTOM])
      {
         dl1.dl.x = bbox[BOXLEFT];
         dl1.dl.y = bbox[BOXBOTTOM];
         dl1.dl.dx = bbox[BOXRIGHT] - bbox[BOXLEFT];
         dl1.dl.dy = 0;
         dl1.nx = 0;
         dl1.ny = -1;

         dl2.dl.x = bbox[BOXRIGHT];
         dl2.dl.y = bbox[BOXBOTTOM];
         dl2.dl.dx = 0;
         dl2.dl.dy = bbox[BOXTOP] - bbox[BOXBOTTOM];
         dl2.nx = 1;
         dl2.ny = 0;

         if(slope)
            *slope = ST_NEGATIVE;
      }
      else if(viewy > bbox[BOXTOP])
      {
         dl1.dl.x = bbox[BOXRIGHT];
         dl1.dl.y = bbox[BOXBOTTOM];
         dl1.dl.dx = 0;
         dl1.dl.dy = bbox[BOXTOP] - bbox[BOXBOTTOM];
         dl1.nx = 1;
         dl1.ny = 0;

         dl2.dl.x = bbox[BOXRIGHT];
         dl2.dl.y = bbox[BOXTOP];
         dl2.dl.dx = bbox[BOXLEFT] - bbox[BOXRIGHT];
         dl2.dl.dy = 0;
         dl2.nx = 0;
         dl2.ny = 1;

         if(slope)
            *slope = ST_POSITIVE;
      }
      else
      {
         dl1.dl.x = bbox[BOXRIGHT];
         dl1.dl.y = bbox[BOXBOTTOM];
         dl1.dl.dx = 0;
         dl1.dl.dy = bbox[BOXTOP] - bbox[BOXBOTTOM];
         dl1.nx = 1;
         dl1.ny = 0;

         if(slope)
            *slope = ST_VERTICAL;
      }
   }

   return true;
}

//
// Check seg against barrier bbox
//
static bool R_allowBehindSectorPortal(const fixed_t bbox[4], const seg_t &tryseg)
{
   divline_t segdl;
   segdl.x = tryseg.v1->x;
   segdl.y = tryseg.v1->y;
   segdl.dx = tryseg.v2->x - segdl.x;
   segdl.dy = tryseg.v2->y - segdl.y;

   int boxside = P_BoxOnDivlineSide(bbox, segdl);

   if(boxside == 0)
      return true;
   if(boxside == 1)
      return false;

   dlnormal_t dl1;
   dlnormal_t dl2;

   slopetype_t slope, lnslope = tryseg.linedef->slopetype;
   if(!R_PickNearestBoxLines(bbox, dl1, dl2, &slope))
      return true;

   if(slope == ST_VERTICAL || slope == ST_HORIZONTAL)
      return R_allowBehindDivline(dl1, &tryseg);

   // Slanted
   if(slope != lnslope)
   {
      return R_allowBehindDivline(dl1, &tryseg) &&
      R_allowBehindDivline(dl2, &tryseg);
   }

   // Pointed to the corner
   bool revfirst = slope == ST_POSITIVE ?
   !!((segdl.dx > 0) ^ (dl1.dl.x == bbox[BOXRIGHT])) :
   !!((segdl.dx > 0) ^ (dl1.dl.x == bbox[BOXLEFT]));

   // truth table:
   // Positive slope
   // v1--->v2     top right   =>  revfirst
   //  false          false           false
   //  false           true           true
   //   true          false           true
   //   true           true           false
   // Negative slope
   // v1--->v2     top left   =>  revfirst
   //  false          false           false
   //  false           true           true
   //   true          false           true
   //   true           true           false

   return R_allowBehindDivline(dl1, &tryseg, revfirst) &&
      R_allowBehindDivline(dl2, &tryseg, !revfirst);
}

//
// R_AddLine
//
// Clips the given segment
// and adds any visible pieces to the line list.
//
static void R_AddLine(seg_t *line, bool dynasegs)
{
   static sector_t tempsec;

   float x1, x2;
   float toffsetx = 0.0f, toffsety = 0.0f;
   float i1, i2, pstep;
   float lclip1, lclip2;
   float nearclip = NEARCLIP;
   vertex_t  t1, t2, temp;
   side_t *side;
   float floorx1, floorx2;
   vertex_t  *v1, *v2;

   // ioanch 20160125: reject segs in front of line when rendering line portal
   if(portalrender.w && portalrender.w->portal &&
      portalrender.w->portal->type != R_SKYBOX)
   {
      // only reject if they're anchored portals (including linked)
      if(portalrender.w->line)
      {
         if(!R_allowBehindDivline(portalrender.w->barrier.dln, line))
            return;
      }
      else
      {
         if(!R_allowBehindSectorPortal(portalrender.w->barrier.bbox, *line))
            return;
      }
   }
   // SoM: one of the byproducts of the portal height enforcement: The top 
   // silhouette should be drawn at ceilingheight but the actual texture 
   // coords should start at ceilingz. Yeah Quasar, it did get a LITTLE 
   // complicated :/
   float textop, texbottom;

   seg.clipsolid = false;
   seg.line = line;

   seg.backsec = R_FakeFlat(line->backsector, &tempsec, NULL, NULL, true);

   // haleyjd: TEST
   // This seems to fix fiffy5, but smells like a hack to me.
   if(seg.frontsec == seg.backsec &&
      seg.frontsec->intflags & SIF_SKY &&
      seg.frontsec->ceilingheight == seg.frontsec->floorheight)
      seg.backsec = NULL;

   if(!dynasegs && (line->linedef->intflags & MLI_DYNASEGLINE)) // haleyjd
      return;

   // If the frontsector is closed, don't render the line!
   // This fixes a very specific type of slime trail.
   // Unless we are viewing down into a portal...??

   //
   // IOANCH 20160120: ADD C_PORTAL AND F_PORTAL CHECK BECAUSE IT MIGHT HAVE
   // BEEN REMOVED BY R_FAKEFLAT WITHOUT ALSO CANCELLING PS_PASSABLE!
   //
   if(!seg.frontsec->f_slope && !seg.frontsec->c_slope &&
      seg.frontsec->ceilingheight <= seg.frontsec->floorheight &&
      !(seg.frontsec->intflags & SIF_SKY) &&
      !((seg.frontsec->c_pflags & PS_PASSABLE && seg.frontsec->c_portal &&
        viewz > P_CeilingPortalZ(*seg.frontsec)) ||
        (seg.frontsec->f_pflags & PS_PASSABLE && seg.frontsec->f_portal &&
        viewz < P_FloorPortalZ(*seg.frontsec))))
      return;

   // Reject empty two-sided lines used for line specials.
   if(seg.backsec && seg.frontsec
      && seg.backsec->ceilingpic == seg.frontsec->ceilingpic 
      && seg.backsec->floorpic   == seg.frontsec->floorpic
      && seg.backsec->lightlevel == seg.frontsec->lightlevel 
      && seg.line->sidedef->midtexture == 0
      
      // killough 3/7/98: Take flats offsets into account:
      && seg.backsec->floor_xoffs   == seg.frontsec->floor_xoffs
      && seg.backsec->floor_yoffs   == seg.frontsec->floor_yoffs
      && seg.backsec->ceiling_xoffs == seg.frontsec->ceiling_xoffs
      && seg.backsec->ceiling_yoffs == seg.frontsec->ceiling_yoffs

      && seg.backsec->floor_xscale   == seg.frontsec->floor_xscale
      && seg.backsec->floor_yscale   == seg.frontsec->floor_yscale
      && seg.backsec->ceiling_xscale == seg.backsec->ceiling_xscale
      && seg.backsec->ceiling_yscale == seg.backsec->ceiling_yscale

      // haleyjd 11/04/10: angles
      && (seg.backsec->floorbaseangle + seg.backsec->floorangle ==
          seg.frontsec->floorbaseangle + seg.frontsec->floorangle)
      && (seg.backsec->ceilingbaseangle + seg.backsec->ceilingangle ==
          seg.frontsec->ceilingbaseangle + seg.frontsec->ceilingangle)
      
      // killough 4/16/98: consider altered lighting
      && seg.backsec->floorlightsec   == seg.frontsec->floorlightsec
      && seg.backsec->floorlightdelta == seg.frontsec->floorlightdelta
      && seg.backsec->ceilinglightsec == seg.frontsec->ceilinglightsec
      && seg.backsec->ceilinglightdelta == seg.frontsec->ceilinglightdelta
      && (seg.backsec->flags & (SECF_FLOORLIGHTABSOLUTE | SECF_CEILLIGHTABSOLUTE))
      == (seg.frontsec->flags & (SECF_FLOORLIGHTABSOLUTE | SECF_CEILLIGHTABSOLUTE))

      && seg.backsec->floorheight   == seg.frontsec->floorheight
      && seg.backsec->ceilingheight == seg.frontsec->ceilingheight
      
      // sf: coloured lighting
      // haleyjd 03/04/07: must test against maps, not heightsec
      && seg.backsec->bottommap == seg.frontsec->bottommap 
      && seg.backsec->midmap    == seg.frontsec->midmap 
      && seg.backsec->topmap    == seg.frontsec->topmap

      // SoM 12/10/03: PORTALS
      && seg.backsec->c_portal == seg.frontsec->c_portal
      && seg.backsec->f_portal == seg.frontsec->f_portal
      
      && (seg.backsec->c_portal != NULL && (seg.backsec->c_pflags & PS_BLENDFLAGS) == (seg.frontsec->c_pflags & PS_BLENDFLAGS))
      && (seg.backsec->f_portal != NULL && (seg.backsec->f_pflags & PS_BLENDFLAGS) == (seg.frontsec->f_pflags & PS_BLENDFLAGS))

      && !seg.line->linedef->portal

      && seg.backsec->f_slope == seg.frontsec->f_slope
      && seg.backsec->c_slope == seg.frontsec->c_slope
      )
      return;      

   // The first step is to do calculations for the entire wall seg, then
   // send the wall to the clipping functions.
   v1 = line->v1;
   v2 = line->v2;

   lclip2 = line->len;
   lclip1 = 0.0f;

   temp.fx = v1->fx - view.x;
   temp.fy = v1->fy - view.y;
   t1.fx   = (temp.fx * view.cos) - (temp.fy * view.sin);
   t1.fy   = (temp.fy * view.cos) + (temp.fx * view.sin);
   temp.fx = v2->fx - view.x;
   temp.fy = v2->fy - view.y;
   t2.fx   = (temp.fx * view.cos) - (temp.fy * view.sin);
   t2.fy   = (temp.fy * view.cos) + (temp.fx * view.sin);

   // SoM: Portal lines are not texture and as a result can be clipped MUCH 
   // closer to the camera than normal lines can. This closer clipping 
   // distance is used to stave off the flash that can sometimes occur when
   // passing through a linked portal line.
   if(line->linedef->portal)
      nearclip = PNEARCLIP;

   if(t1.fy < nearclip)
   {      
      float move, movey;

      // Simple reject for lines entirely behind the view plane.
      if(t2.fy < nearclip)
         return;

      movey = NEARCLIP - t1.fy;
      t1.fx += (move = movey * ((t2.fx - t1.fx) / (t2.fy - t1.fy)));

      lclip1 = (float)sqrt(move * move + movey * movey);
      t1.fy = NEARCLIP;
   }

   i1 = 1.0f / t1.fy;
   x1 = (view.xcenter + (t1.fx * i1 * view.xfoc));

   if(t2.fy < NEARCLIP)
   {
      float move, movey;

      movey = NEARCLIP - t2.fy;
      t2.fx += (move = movey * ((t2.fx - t1.fx) / (t2.fy - t1.fy)));

      lclip2 -= (float)sqrt(move * move + movey * movey);
      t2.fy = NEARCLIP;
   }

   i2 = 1.0f / t2.fy;
   x2 = (view.xcenter + (t2.fx * i2 * view.xfoc));

   // SoM: Handle the case where a wall is only occupying a single post but 
   // still needs to be rendered to keep groups of single post walls from not
   // being rendered and causing slime trails.
   floorx1 = (float)floor(x1 + 0.999f);
   floorx2 = (float)floor(x2 - 0.001f);

   // backface rejection
   if(floorx2 < floorx1)
      return;

   // off the screen rejection
   if(floorx2 < 0 || floorx1 >= view.width)
      return;

   if(x2 > x1)
      pstep = 1.0f / (x2 - x1);
   else
      pstep = 1.0f;

   side = line->sidedef;
   
   seg.toffsetx = toffsetx + M_FixedToFloat(side->textureoffset) + line->offset; 
   seg.toffsety = toffsety + M_FixedToFloat(side->rowoffset);

   if(seg.toffsetx < 0)
   {
      float maxtexw = 0.0f;

      // SoM: ok, this was driving me crazy. It seems that when the offset is 
      // less than 0, the fractional part will cause the texel at 
      // 0 + abs(seg.toffsetx) to double and it will strip the first texel to
      // one column. This is because -64 + ANY FRACTION is going to cast to 63
      // and when you cast -0.999 through 0.999 it will cast to 0. The first 
      // step is to find the largest texture width on the line to make sure all
      // the textures will start at the same column when the offsets are 
      // adjusted.

      if(side->toptexture)
         maxtexw = (float)textures[side->toptexture]->widthmask;
      if(side->midtexture && textures[side->midtexture]->widthmask > maxtexw)
         maxtexw = (float)textures[side->midtexture]->widthmask;
      if(side->bottomtexture && textures[side->bottomtexture]->widthmask > maxtexw)
         maxtexw = (float)textures[side->bottomtexture]->widthmask;

      // Then adjust the offset to zero or the first positive value that will 
      // repeat correctly with the largest texture on the line.
      if(maxtexw)
      {
         maxtexw++;
         while(seg.toffsetx < 0.0f) 
            seg.toffsetx += maxtexw;
      }
   }

   seg.dist = i1;
   seg.dist2 = i2;
   seg.diststep = (i2 - i1) * pstep;

   i1 *= view.yfoc; i2 *= view.yfoc;

   seg.len = lclip1 * i1;
   seg.len2 = lclip2 * i2;
   seg.lenstep = (seg.len2 - seg.len) * pstep;

   seg.side = side;

   if(seg.frontsec->c_slope)
   {
      float z1, z2, zstep;

      z1 = P_GetZAtf(seg.frontsec->c_slope, v1->fx, v1->fy);
      z2 = P_GetZAtf(seg.frontsec->c_slope, v2->fx, v2->fy);
      zstep = (z2 - z1) / seg.line->len;

      z1 += lclip1 * zstep;
      z2 -= (seg.line->len - lclip2) * zstep;
      seg.top = view.ycenter - ((z1 - view.z) * i1);
      seg.top2 = view.ycenter - ((z2 - view.z) * i2);
   }
   else
   {
      seg.top = view.ycenter - ((seg.frontsec->ceilingheightf - view.z) * i1);
      seg.top2 = view.ycenter - ((seg.frontsec->ceilingheightf - view.z) * i2);
   }
   seg.topstep = (seg.top2 - seg.top) * pstep;


   if(seg.frontsec->f_slope)
   {
      float z1, z2, zstep;

      z1 = P_GetZAtf(seg.frontsec->f_slope, v1->fx, v1->fy);
      z2 = P_GetZAtf(seg.frontsec->f_slope, v2->fx, v2->fy);
      zstep = (z2 - z1) / seg.line->len;

      z1 += lclip1 * zstep;
      z2 -= (seg.line->len - lclip2) * zstep;
      seg.bottom = view.ycenter - ((z1 - view.z) * i1) - 1.0f;
      seg.bottom2 = view.ycenter - ((z2 - view.z) * i2) - 1.0f;
   }
   else
   {      
      seg.bottom  = view.ycenter - ((seg.frontsec->floorheightf - view.z) * i1) - 1.0f;
      seg.bottom2 = view.ycenter - ((seg.frontsec->floorheightf - view.z) * i2) - 1.0f;
   }

   seg.bottomstep = (seg.bottom2 - seg.bottom) * pstep;

   // Get these from the actual sectors because R_FakeFlat could have changed the actual heights.
   textop    = seg.line->frontsector->ceilingheightf - view.z;
   texbottom = seg.line->frontsector->floorheightf   - view.z;

   seg.f_portalignore = seg.c_portalignore = false;

   // ioanch 20160312: also treat polyobject portal lines as 1-sided
   const sector_t *beyond = seg.line->linedef->intflags & MLI_1SPORTALLINE && 
      seg.line->linedef->beyondportalline ? 
      seg.line->linedef->beyondportalline->frontsector : nullptr;
   if(!seg.backsec || beyond) 
   {
      seg.twosided = false;
      if(!beyond)
         seg.toptex   = seg.bottomtex = 0;
      else
      {
         // ioanch FIXME: copy-paste from R_2S_Normal
         if(seg.frontsec->ceilingheight > beyond->ceilingheight &&
           !(seg.frontsec->intflags & SIF_SKY && beyond->intflags & SIF_SKY) && 
            side->toptexture)
         {
            seg.toptex = texturetranslation[side->toptexture];
            seg.toptexh = textures[side->toptexture]->height;

            float texhigh = beyond->ceilingheightf - view.z;

            if(seg.line->linedef->flags & ML_DONTPEGTOP)
               seg.toptexmid = M_FloatToFixed(textop + seg.toffsety);
            else
               seg.toptexmid = M_FloatToFixed(texhigh + seg.toptexh + seg.toffsety);

            seg.high  = view.ycenter - ((beyond->ceilingheightf - view.z) * i1) - 1.0f;
            seg.high2 = view.ycenter - ((beyond->ceilingheightf - view.z) * i2) - 1.0f;
            seg.highstep = (seg.high2 - seg.high) * pstep;
         }
         else
            seg.toptex = 0;

         if(seg.frontsec->floorheight < beyond->floorheight && side->bottomtexture)
         {
            seg.bottomtex  = texturetranslation[side->bottomtexture];
            seg.bottomtexh = textures[side->bottomtexture]->height;

            float texlow = beyond->floorheightf - view.z;

            if(seg.line->linedef->flags & ML_DONTPEGBOTTOM)
               seg.bottomtexmid = M_FloatToFixed(textop + seg.toffsety);
            else
               seg.bottomtexmid = M_FloatToFixed(texlow + seg.toffsety);

            seg.low  = view.ycenter - ((beyond->floorheightf - view.z) * i1);
            seg.low2 = view.ycenter - ((beyond->floorheightf - view.z) * i2);
            seg.lowstep = (seg.low2 - seg.low) * pstep;
         }
         else
            seg.bottomtex = 0;

      }
      seg.midtex   = texturetranslation[side->midtexture];
      seg.midtexh  = textures[side->midtexture]->height;

      if(seg.line->linedef->flags & ML_DONTPEGBOTTOM)
         seg.midtexmid = M_FloatToFixed(texbottom + seg.midtexh + seg.toffsety);
      else
         seg.midtexmid = M_FloatToFixed(textop + seg.toffsety);

      seg.markflags = beyond ? SEG_MARK1SLPORTAL : 0;
      seg.c_window = seg.f_window = NULL;

      // SoM: these should be treated differently! 
      if(seg.frontsec->c_portal && (seg.frontsec->c_portal->type < R_TWOWAY ||
         (seg.frontsec->c_pflags & PS_VISIBLE && seg.frontsec->ceilingheight > viewz)))
      {
         seg.markflags |= SEG_MARKCPORTAL;
         seg.c_window   = R_GetCeilingPortalWindow(seg.frontsec->c_portal,
                                                   seg.frontsec->ceilingheight);
         R_MovePortalOverlayToWindow(true);
      }

      if(seg.frontsec->f_portal && (seg.frontsec->f_portal->type < R_TWOWAY ||
        (seg.frontsec->f_pflags & PS_VISIBLE && seg.frontsec->floorheight <= viewz)))
      {
         seg.markflags |= SEG_MARKFPORTAL;
         seg.f_window   = R_GetFloorPortalWindow(seg.frontsec->f_portal,
                                                 seg.frontsec->floorheight);
         R_MovePortalOverlayToWindow(false);
      }

      if(seg.ceilingplane != NULL)
         seg.markflags |= seg.frontsec->c_portal ? SEG_MARKCOVERLAY : SEG_MARKCEILING;
      if(seg.floorplane != NULL)
         seg.markflags |= seg.frontsec->f_portal ? SEG_MARKFOVERLAY : SEG_MARKFLOOR;
         
      seg.clipsolid   = true;
      seg.segtextured = (seg.midtex != 0);
      seg.l_window    = line->linedef->portal ?
                        R_GetLinePortalWindow(line->linedef->portal, line->linedef) : NULL;

      // haleyjd 03/12/06: inverted predicates to simplify
      if(seg.frontsec->f_portal && seg.frontsec->f_portal->type != R_LINKED && 
         seg.frontsec->f_portal->type != R_TWOWAY)
         seg.f_portalignore = true;
      if(seg.frontsec->c_portal && seg.frontsec->c_portal->type != R_LINKED && 
         seg.frontsec->c_portal->type != R_TWOWAY)
         seg.c_portalignore = true;
   }
   else
   {
      if(seg.frontsec->f_slope || seg.frontsec->c_slope ||
         seg.backsec->f_slope || seg.backsec->c_slope)
         R_2S_Sloped(pstep, i1, i2, textop, texbottom, v1, v2, lclip1, lclip2);
      else
         R_2S_Normal(pstep, i1, i2, textop, texbottom);
   }

   // SoM: This really needs to be handled here. The float values need to be 
   // clipped to sane values here because floats have a higher range of values
   // than ints do. If the clipping is done after the casting, the step values
   // will no longer be accurate. This ensures more correct projection and 
   // texturing.
   if(x1 < 0)
   {
      float clipx = -x1;

      seg.dist += clipx * seg.diststep;
      seg.len += clipx * seg.lenstep;

      seg.top += clipx * seg.topstep;
      seg.bottom += clipx * seg.bottomstep;

      if(seg.toptex || seg.t_window)
         seg.high += clipx * seg.highstep;
      if(seg.bottomtex || seg.b_window)
         seg.low += clipx * seg.lowstep;

      x1 = floorx1 = 0;
   }

   if(x2 >= view.width)
   {
      float clipx = x2 - (view.width - 1.0f);

      seg.dist2 -= clipx * seg.diststep;
      seg.len2 -= clipx * seg.lenstep;

      seg.top2 -= clipx * seg.topstep;
      seg.bottom2 -= clipx * seg.bottomstep;

      if(seg.toptex || seg.t_window)
         seg.high2 -= clipx * seg.highstep;
      if(seg.bottomtex || seg.b_window)
         seg.low2 -= clipx * seg.lowstep;

      x2 = floorx2 = (view.width - 1.0f);
   }

   seg.x1 = (int)floorx1;
   seg.x1frac = x1;
   seg.x2 = (int)floorx2;
   seg.x2frac = x2;

   if(portalrender.active && portalrender.segClipFunc)
      portalrender.segClipFunc();
   else if(seg.clipsolid)
      R_ClipSolidWallSegment(seg.x1, seg.x2);
   else
      R_ClipPassWallSegment(seg.x1, seg.x2);

   // Add new solid segs when it is safe to do so...
   R_AddMarkedSegs();

   sectorbox_t &box = pSectorBoxes[seg.line->frontsector - sectors];
   if(seg.f_window && box.fframeid != frameid)
   {
      box.fframeid = frameid;
      R_CalcRenderBarrier(*seg.f_window, box);
   }
   if(seg.c_window && box.cframeid != frameid)
   {
      box.cframeid = frameid;
      R_CalcRenderBarrier(*seg.c_window, box);
   }
}


static const int checkcoord[12][4] = // killough -- static const
{
   {3,0,2,1},
   {3,0,2,0},
   {3,1,2,0},
   {0},
   {2,0,2,1},
   {0,0,0,0},
   {3,1,3,0},
   {0},
   {2,0,3,1},
   {2,1,3,1},
   {2,1,3,0}
};

//
// R_CheckBBox
//
// Checks BSP node/subtree bounding box.
// Returns true if some part of the bbox might be visible.
//
static bool R_CheckBBox(fixed_t *bspcoord) // killough 1/28/98: static
{
   int     boxpos, boxx, boxy;
   fixed_t x1, x2, y1, y2;
   angle_t angle1, angle2, span, tspan;
   int     sx1, sx2;
   cliprange_t *start;

   // Find the corners of the box
   // that define the edges from current viewpoint.
   boxx = viewx <= bspcoord[BOXLEFT] ? 0 : viewx < bspcoord[BOXRIGHT ] ? 1 : 2;
   boxy = viewy >= bspcoord[BOXTOP ] ? 0 : viewy > bspcoord[BOXBOTTOM] ? 1 : 2;

   boxpos = (boxy << 2) + boxx;
   if(boxpos == 5)
      return true;

   x1 = bspcoord[checkcoord[boxpos][0]];
   y1 = bspcoord[checkcoord[boxpos][1]];
   x2 = bspcoord[checkcoord[boxpos][2]];
   y2 = bspcoord[checkcoord[boxpos][3]];

   // check clip list for an open space
   angle1 = R_PointToAngle (x1, y1) - viewangle;
   angle2 = R_PointToAngle (x2, y2) - viewangle;
   
   span = angle1 - angle2;
   
   // Sitting on a line?
   if(span >= ANG180)
      return true;

   tspan = angle1 + clipangle;
   if(tspan > 2 * clipangle)
   {
      tspan -= (2 * clipangle);
      
      // Totally off the left edge?
      if(tspan >= span)
         return false;
      
      angle1 = clipangle;
   }

   tspan = clipangle - angle2;
   if(tspan > 2 * clipangle)
   {
      tspan -= (2 * clipangle);
      
      // Totally off the left edge?
      if(tspan >= span)
         return false;
      
      angle2 = 0-clipangle;
   }

   // Find the first clippost
   //  that touches the source post
   //  (adjacent pixels are touching).
   angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
   angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;
   sx1 = viewangletox[angle1];
   sx2 = viewangletox[angle2];
   
   // SoM: To account for the rounding error of the old BSP system, I needed to
   // make adjustments.
   // SoM: Moved this to before the "does not cross a pixel" check to fix 
   // another slime trail
   if(sx1 > 0) sx1--;
   if(sx2 < viewwindow.width - 1) sx2++;

   // SoM: Removed the "does not cross a pixel" test

   start = solidsegs;
   while(start->last < sx2)
      ++start;
   
   if(sx1 >= start->first && sx2 <= start->last)
      return false;      // The clippost contains the new span.
   
   return true;
}

//
// R_interpolateViewPoint
//
// Interpolate a rendering view point based on the player's location.
//
static void R_interpolateVertex(dynavertex_t &v, v2fixed_t &org, v2float_t &forg)
{
   org.x = v.x;
   org.y = v.y;
   forg.x = v.fx;
   forg.y = v.fy;
   if(view.lerp != FRACUNIT)
   {
      v.x = lerpCoord(view.lerp, v.backup.x, v.x);
      v.y = lerpCoord(view.lerp, v.backup.y, v.y);
      v.fx = M_FixedToFloat(v.x);
      v.fy = M_FixedToFloat(v.y);
   }
}

//
// R_RenderPolyNode
//
// Recurse through a polynode mini-BSP
//
static void R_RenderPolyNode(rpolynode_t *node)
{
   while(node)
   {
      int side = R_PointOnDynaSegSide(node->partition, view.x, view.y);
      
      // render frontspace
      R_RenderPolyNode(node->children[side]);

      // render partition seg
      v2fixed_t org[2];
      v2float_t forg[2];
      seg_t *seg = &node->partition->seg;
      R_interpolateVertex(*seg->dyv1, org[0], forg[0]);
      R_interpolateVertex(*seg->dyv2, org[1], forg[1]);
      R_AddLine(seg, true);
      seg->v1->x = org[0].x;
      seg->v1->y = org[0].y;
      seg->v2->x = org[1].x;
      seg->v2->y = org[1].y;
      seg->v1->fx = forg[0].x;
      seg->v1->fy = forg[0].y;
      seg->v2->fx = forg[1].x;
      seg->v2->fy = forg[1].y;

      // continue to render backspace
      node = node->children[side^1];
   }
}

//
// R_AddDynaSegs
//
// haleyjd: Adds dynamic segs contained in all of the rpolyobj_t fragments
// contained inside the given subsector into a mini-BSP tree and then 
// renders the BSP. BSPs are only recomputed when polyobject fragments
// move into or out of the subsector. This is the ultimate heart of the 
// polyobject code.
//
// See r_dynseg.cpp to see how dynasegs get attached to a subsector in the
// first place :)
//
// See r_dynabsp.cpp for rpolybsp generation.
//
static void R_AddDynaSegs(subsector_t *sub)
{
   bool needbsp = (!sub->bsp || sub->bsp->dirty);

   if(needbsp)
   {
      if(sub->bsp)
         R_FreeDynaBSP(sub->bsp);
      sub->bsp = R_BuildDynaBSP(sub);
   }
   if(sub->bsp)
      R_RenderPolyNode(sub->bsp->root);
}

//
// R_Subsector
//
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
// killough 1/31/98 -- made static, polished
//
static void R_Subsector(int num)
{
   int         count;
   seg_t       *line;
   subsector_t *sub;
   sector_t    tempsec;              // killough 3/7/98: deep water hack
   int         floorlightlevel;      // killough 3/16/98: set floor lightlevel
   int         ceilinglightlevel;    // killough 4/11/98
   float       floorangle;           // haleyjd 01/05/08: plane angles
   float       ceilingangle;

   bool        visible;
   v3float_t   cam;
   
#ifdef RANGECHECK
   if(num >= numsubsectors)
      I_Error("R_Subsector: ss %i with numss = %i\n", num, numsubsectors);
#endif

   // haleyjd 09/22/07: clear seg structure
   memset(&seg, 0, sizeof(cb_seg_t));

   sub = &subsectors[num];
   seg.frontsec = sub->sector;

   // ioanch 20160120: don't draw from other group ids than the intended one
   // Also ignore sectors behind portal lines
   if((portalrender.active &&
       portalrender.w->portal->type == R_LINKED &&
       portalrender.w->portal->data.link.toid != seg.frontsec->groupid))
   {
      return;
   }

   count = sub->numlines;
   line = &segs[sub->firstline];

   R_SectorColormap(seg.frontsec);

   // killough 3/8/98, 4/4/98: Deep water / fake ceiling effect
   seg.frontsec = R_FakeFlat(seg.frontsec, &tempsec, &floorlightlevel,
                             &ceilinglightlevel, false);   // killough 4/11/98

   // ioanch: reject all sectors fully above or below a sector portal.
   if(portalrender.active && portalrender.w->portal->type != R_SKYBOX &&
      ((portalrender.w->type == pw_ceiling &&
        seg.frontsec->ceilingheight < portalrender.w->planez + viewz - portalrender.w->vz) ||
       (portalrender.w->type == pw_floor &&
        seg.frontsec->floorheight > portalrender.w->planez + viewz - portalrender.w->vz)))
   {
      return;
   }

   // haleyjd 01/05/08: determine angles for floor and ceiling
   floorangle   = seg.frontsec->floorbaseangle   + seg.frontsec->floorangle;
   ceilingangle = seg.frontsec->ceilingbaseangle + seg.frontsec->ceilingangle;

   // killough 3/7/98: Add (x,y) offsets to flats, add deep water check
   // killough 3/16/98: add floorlightlevel
   // killough 10/98: add support for skies transferred from sidedefs

   // SoM: Slopes!
   cam.x = view.x;
   cam.y = view.y;
   cam.z = view.z;

   // -- Floor plane and portal --
   visible  = (!seg.frontsec->f_slope && seg.frontsec->floorheight < viewz)
           || (seg.frontsec->f_slope 
           &&  P_DistFromPlanef(&cam, &seg.frontsec->f_slope->of, 
                                &seg.frontsec->f_slope->normalf) > 0.0f);

   // ioanch 20160118: ADDED A f_portal existence check!
   seg.f_portal = seg.frontsec->f_pflags & PS_VISIBLE 
               && (!portalrender.active || portalrender.w->type != pw_ceiling)
               && (visible || 
               (seg.frontsec->f_portal && seg.frontsec->f_portal->type < R_TWOWAY))
               ? seg.frontsec->f_portal : NULL;

   // This gets a little convoluted if you try to do it on one inequality
   if(seg.f_portal)
   {
      unsigned int fpalpha = (seg.frontsec->f_pflags >> PO_OPACITYSHIFT) & 0xff;
      visible = (visible && (fpalpha > 0));

      seg.floorplane = visible && seg.frontsec->f_pflags & PS_OVERLAY ?
        R_FindPlane(seg.frontsec->floorheight,
                    seg.frontsec->f_pflags & PS_USEGLOBALTEX ? 
                    seg.f_portal->globaltex : seg.frontsec->floorpic,
                    floorlightlevel,                // killough 3/16/98
                    seg.frontsec->floor_xoffs,       // killough 3/7/98
                    seg.frontsec->floor_yoffs,
                    seg.frontsec->floor_xscale,
                    seg.frontsec->floor_yscale,
                    floorangle, seg.frontsec->f_slope, 
                    seg.frontsec->f_pflags,
                    fpalpha,
                    seg.f_portal->poverlay) : NULL;
   }
   else
   {
      // SoM: If there is an active portal, forget about the floorplane.
      seg.floorplane = (visible || // killough 3/7/98
         (seg.frontsec->heightsec != -1 &&
          sectors[seg.frontsec->heightsec].intflags & SIF_SKY)) ?
        R_FindPlane(seg.frontsec->floorheight, 
                    R_IsSkyFlat(seg.frontsec->floorpic) &&  // kilough 10/98
                    seg.frontsec->sky & PL_SKYFLAT ? seg.frontsec->sky :
                    seg.frontsec->floorpic,
                    floorlightlevel,                // killough 3/16/98
                    seg.frontsec->floor_xoffs,       // killough 3/7/98
                    seg.frontsec->floor_yoffs,
                    seg.frontsec->floor_xscale,
                    seg.frontsec->floor_yscale,
                    floorangle, seg.frontsec->f_slope, 0, 255, NULL) : NULL;
   }
   

   // -- Ceiling plane and portal --
   visible  = (!seg.frontsec->c_slope && seg.frontsec->ceilingheight > viewz)
           || (seg.frontsec->c_slope 
           &&  P_DistFromPlanef(&cam, &seg.frontsec->c_slope->of, 
                                &seg.frontsec->c_slope->normalf) > 0.0f);

   // ioanch 20160118: ADDED A c_portal existence check!
   seg.c_portal = seg.frontsec->c_pflags & PS_VISIBLE 
               && (!portalrender.active || portalrender.w->type != pw_floor)
               && (visible || 
               (seg.frontsec->c_portal && seg.frontsec->c_portal->type < R_TWOWAY)) 
               ? seg.frontsec->c_portal : NULL;

   // This gets a little convoluted if you try to do it on one inequality
   if(seg.c_portal)
   {
      unsigned int cpalpha = (seg.frontsec->c_pflags >> PO_OPACITYSHIFT) & 0xff;
      visible = (visible && (cpalpha > 0));

      seg.ceilingplane = visible && seg.frontsec->c_pflags & PS_OVERLAY ?
        R_FindPlane(seg.frontsec->ceilingheight,
                    seg.frontsec->c_pflags & PS_USEGLOBALTEX ? 
                    seg.c_portal->globaltex : seg.frontsec->ceilingpic,
                    ceilinglightlevel,                // killough 3/16/98
                    seg.frontsec->ceiling_xoffs,       // killough 3/7/98
                    seg.frontsec->ceiling_yoffs,
                    seg.frontsec->ceiling_xscale,
                    seg.frontsec->ceiling_yscale,
                    ceilingangle, seg.frontsec->c_slope, 
                    seg.frontsec->c_pflags,
                    cpalpha,
                    seg.c_portal->poverlay) : NULL;
   }
   else
   {
      seg.ceilingplane = (visible ||
         (seg.frontsec->intflags & SIF_SKY) ||
        (seg.frontsec->heightsec != -1 &&
         R_IsSkyFlat(sectors[seg.frontsec->heightsec].floorpic))) ?
        R_FindPlane(seg.frontsec->ceilingheight,     // killough 3/8/98
                    (seg.frontsec->intflags & SIF_SKY) &&  // kilough 10/98
                    seg.frontsec->sky & PL_SKYFLAT ? seg.frontsec->sky :
                    seg.frontsec->ceilingpic,
                    ceilinglightlevel,              // killough 4/11/98
                    seg.frontsec->ceiling_xoffs,     // killough 3/7/98
                    seg.frontsec->ceiling_yoffs,
                    seg.frontsec->ceiling_xscale,
                    seg.frontsec->ceiling_yscale,
                    ceilingangle, seg.frontsec->c_slope, 0, 255, NULL) : NULL;
   }
  
   // killough 9/18/98: Fix underwater slowdown, by passing real sector 
   // instead of fake one. Improve sprite lighting by basing sprite
   // lightlevels on floor & ceiling lightlevels in the surrounding area.
   //
   // 10/98 killough:
   //
   // NOTE: TeamTNT fixed this bug incorrectly, messing up sprite lighting!!!
   // That is part of the 242 effect!!!  If you simply pass sub->sector to
   // the old code you will not get correct lighting for underwater sprites!!!
   // Either you must pass the fake sector and handle validcount here, on the
   // real sector, or you must account for the lighting in some other way, 
   // like passing it as an argument.

   R_AddSprites(sub->sector, (floorlightlevel+ceilinglightlevel)/2);

   // haleyjd 02/19/06: draw polyobjects before static lines
   // haleyjd 10/09/06: skip call entirely if no polyobjects

   if(sub->polyList)
      R_AddDynaSegs(sub);

   while(count--)
      R_AddLine(line++, false);
}

//
// R_RenderBSPNode
//
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.
//
// killough 5/2/98: reformatted, removed tail recursion
//
void R_RenderBSPNode(int bspnum)
{
   while(!(bspnum & NF_SUBSECTOR))  // Found a subsector?
   {
      node_t *bsp = &nodes[bspnum];
      
      // Decide which side the view point is on.
      int side = R_PointOnSide(viewx, viewy, bsp);
      
      // Recursively divide front space.
      R_RenderBSPNode(bsp->children[side]);
      
      // Possibly divide back space.
      
      if(!R_CheckBBox(bsp->bbox[side^=1]))
         return;
      
      bspnum = bsp->children[side];
   }
   R_Subsector(bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
}

//----------------------------------------------------------------------------
//
// $Log: r_bsp.c,v $
// Revision 1.17  1998/05/03  22:47:33  killough
// beautification
//
// Revision 1.16  1998/04/23  12:19:50  killough
// Testing untabify feature
//
// Revision 1.15  1998/04/17  10:22:22  killough
// Fix 213, 261 (floor/ceiling lighting)
//
// Revision 1.14  1998/04/14  08:15:55  killough
// Fix light levels on 2s textures
//
// Revision 1.13  1998/04/13  09:44:40  killough
// Fix head-over ceiling effects
//
// Revision 1.12  1998/04/12  01:57:18  killough
// Fix deep water effects
//
// Revision 1.11  1998/04/07  06:41:14  killough
// Fix disappearing things, AASHITTY sky wall HOM, remove obsolete HOM detector
//
// Revision 1.10  1998/04/06  04:37:48  killough
// Make deep water / fake ceiling handling more consistent
//
// Revision 1.9  1998/03/28  18:14:27  killough
// Improve underwater support
//
// Revision 1.8  1998/03/16  12:40:11  killough
// Fix underwater effects, floor light levels from other sectors
//
// Revision 1.7  1998/03/09  07:22:41  killough
// Add primitive underwater support
//
// Revision 1.6  1998/03/02  11:50:53  killough
// Add support for scrolling flats
//
// Revision 1.5  1998/02/17  06:21:57  killough
// Change commented-out code to #if'ed out code
//
// Revision 1.4  1998/02/09  03:14:55  killough
// Make HOM detector under control of TNTHOM cheat
//
// Revision 1.3  1998/02/02  13:31:23  killough
// Performance tuning, add HOM detector
//
// Revision 1.2  1998/01/26  19:24:36  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:02  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
