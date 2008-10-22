// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley
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
//      BSP traversal, handling of LineSegs for rendering.
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "m_bbox.h"
#include "i_system.h"
#include "r_main.h"
#include "r_segs.h"
#include "r_plane.h"
#include "r_things.h"
#include "r_dynseg.h"

drawseg_t *ds_p;

// killough 4/7/98: indicates doors closed wrt automap bugfix:
int      doorclosed;

// killough: New code which removes 2s linedef limit
drawseg_t *drawsegs = NULL;
unsigned long maxdrawsegs;
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

typedef struct 
{
  short first, last;      // killough
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

#define MAXSEGS (MAX_SCREENWIDTH/2+1)   /* killough 1/11/98, 2/8/98 */

// newend is one past the last valid seg
static cliprange_t *newend;
static cliprange_t solidsegs[MAXSEGS];

// addend is one past the last valid added seg.
static cliprange_t addedsegs[MAXSEGS];
static cliprange_t *addend = addedsegs;


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
         *rover++;
      }

      newend = rover;
   }
   
   verify:
#ifdef RANGECHECK
   // Verify the new segs
   for(rover = solidsegs; (rover + 1) < newend; rover++)
   {
      if(rover->last >= (rover+1)->first)
         I_Error("R_AddSolidSeg created a seg that overlaps next seg: "
                 "(%i)->last = %i, (%i)->first = %i\n", 
                 rover - solidsegs, 
                 rover->last, 
                 (rover + 1) - solidsegs, 
                 (rover + 1)->last);
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
         I_Error("R_ClipSolidWallSegment created a seg that overlaps next seg: "
                 "(%i)->last = %i, (%i)->first = %i\n", 
                 start - solidsegs, 
                 start->last, 
                 (start + 1) - solidsegs, 
                 (start + 1)->last);
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
void R_ClearClipSegs(void)
{
   solidsegs[0].first = -0x7fff; // ffff;    new short limit --  killough
   solidsegs[0].last = -1;
   solidsegs[1].first = viewwidth;
   solidsegs[1].last = 0x7fff; // ffff;      new short limit --  killough
   newend = solidsegs+2;
   addend = addedsegs;

   // haleyjd 09/22/07: must clear seg and segclip structures
   memset(&seg, 0, sizeof(cb_seg_t));
   memset(&segclip, 0, sizeof(cb_seg_t));
}


boolean R_SetupPortalClipsegs(int minx, int maxx, float *top, float *bottom)
{
   int i = minx, stop = maxx + 1;
   cliprange_t *solidseg = solidsegs;
   
   R_ClearClipSegs();

   // SoM: This should be done here instead of having an additional loop
   portalrender.miny = MAX_SCREENHEIGHT;
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
      
      if(i == viewwidth)
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
   solidseg->last = 0x7fff;
   newend = solidseg + 1;
   return true;
   
endclosed:
   solidseg->last = 0x7fff;
   newend = solidseg + 1;
   return true;
}



// 
// R_ClipInitialSegRange
//
// Used by R_ClipSeg to quickly reject segs outside the open range of the 
// portal and to clip segs to the portals open range on the x-axis
//
boolean R_ClipInitialSegRange(void)
{
   float clipx;

   if(seg.x2 < portalrender.minx || seg.x1 > portalrender.maxx)
      return false;


   if(portalrender.minx > seg.x1)
   {
      clipx = portalrender.minx - seg.x1frac;

      seg.x1 = portalrender.minx;
      seg.x1frac = (float)portalrender.minx;

      seg.dist += clipx * seg.diststep;
      seg.len += clipx * seg.lenstep;
   }
   // ok, the left edge is done

   // do the right edge
   if(portalrender.maxx < seg.x2)
   {
      clipx = portalrender.maxx - seg.x2frac;

      seg.x2 = portalrender.maxx;
      seg.x2frac = (float)portalrender.maxx;

      seg.dist2 += clipx * seg.diststep;
      seg.len2 += clipx * seg.lenstep;
   }

   return true;
}

// killough 1/18/98 -- This function is used to fix the automap bug which
// showed lines behind closed doors simply because the door had a dropoff.
//
// It assumes that Doom has already ruled out a door being closed because
// of front-back closure (e.g. front floor is taller than back ceiling).

int R_DoorClosed(void)
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
     && ((seg.backsec->ceilingpic != skyflatnum && 
          seg.backsec->ceilingpic != sky2flatnum) ||
         (seg.frontsec->ceilingpic != skyflatnum &&
          seg.frontsec->ceilingpic != sky2flatnum));
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
                     boolean back)
{
   if(floorlightlevel)
   {
      *floorlightlevel = sec->floorlightsec == -1 ?
         sec->lightlevel : sectors[sec->floorlightsec].lightlevel;
   }

   if(ceilinglightlevel)
   {
      *ceilinglightlevel = sec->ceilinglightsec == -1 ? // killough 4/11/98
         sec->lightlevel : sectors[sec->ceilinglightsec].lightlevel;
   }


   if(sec->heightsec != -1)
   {
      int underwater; // haleyjd: restructured
      
      int heightsec = -1;
      
      const sector_t *s = &sectors[sec->heightsec];
      
      // haleyjd: Lee assumed that only players would ever be
      // involved in LOS calculations for deep water -- must be
      // fixed for cameras -- thanks to Julian for finding the
      // solution to this old problem!

      heightsec = camera ? camera->heightsec
                         : viewplayer->mo->subsector->sector->heightsec;
            
      underwater = 
	 (heightsec != -1 && viewz <= sectors[heightsec].floorheight);

      // Replace sector being drawn, with a copy to be hacked
      *tempsec = *sec;

      // Replace floor and ceiling height with other sector's heights.
      tempsec->floorheight   = s->floorheight;
      tempsec->ceilingheight = s->ceilingheight;

      // killough 11/98: prevent sudden light changes from non-water sectors:
      if(underwater && (tempsec->floorheight   = sec->floorheight,
                        tempsec->ceilingheight = s->floorheight-1, !back))
      {
         // head-below-floor hack
         tempsec->floorpic    = s->floorpic;
         tempsec->floor_xoffs = s->floor_xoffs;
         tempsec->floor_yoffs = s->floor_yoffs;

         // haleyjd 03/13/05: removed redundant if(underwater) check
         if(s->ceilingpic == skyflatnum || s->ceilingpic == sky2flatnum)
         {
            tempsec->floorheight   = tempsec->ceilingheight+1;
            tempsec->ceilingpic    = tempsec->floorpic;
            tempsec->ceiling_xoffs = tempsec->floor_xoffs;
            tempsec->ceiling_yoffs = tempsec->floor_yoffs;
         }
         else
         {
            tempsec->ceilingpic    = s->ceilingpic;
            tempsec->ceiling_xoffs = s->ceiling_xoffs;
            tempsec->ceiling_yoffs = s->ceiling_yoffs;
         }

         tempsec->lightlevel  = s->lightlevel;
         
         if(floorlightlevel)
         {
            *floorlightlevel = s->floorlightsec == -1 ? s->lightlevel :
               sectors[s->floorlightsec].lightlevel; // killough 3/16/98
         }

         if (ceilinglightlevel)
         {
            *ceilinglightlevel = s->ceilinglightsec == -1 ? s->lightlevel :
               sectors[s->ceilinglightsec].lightlevel; // killough 4/11/98
         }
      }
      else if(heightsec != -1 && 
              viewz >= sectors[heightsec].ceilingheight &&
              sec->ceilingheight > s->ceilingheight)
      {   
         // Above-ceiling hack
         tempsec->ceilingheight = s->ceilingheight;
         tempsec->floorheight   = s->ceilingheight + 1;

         tempsec->floorpic    = tempsec->ceilingpic    = s->ceilingpic;
         tempsec->floor_xoffs = tempsec->ceiling_xoffs = s->ceiling_xoffs;
         tempsec->floor_yoffs = tempsec->ceiling_yoffs = s->ceiling_yoffs;

         if(s->floorpic != skyflatnum && s->floorpic != sky2flatnum)
         {
            tempsec->ceilingheight = sec->ceilingheight;
            tempsec->floorpic      = s->floorpic;
            tempsec->floor_xoffs   = s->floor_xoffs;
            tempsec->floor_yoffs   = s->floor_yoffs;
         }
         
         tempsec->lightlevel  = s->lightlevel;
         
         if(floorlightlevel)
         {
            *floorlightlevel = s->floorlightsec == -1 ? s->lightlevel :
               sectors[s->floorlightsec].lightlevel; // killough 3/16/98
         }

         if(ceilinglightlevel)
         {
            *ceilinglightlevel = s->ceilinglightsec == -1 ? s->lightlevel :
               sectors[s->ceilinglightsec].lightlevel; // killough 4/11/98
         }
      }
      sec = tempsec;               // Use other sector
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
static void R_ClipSegToPortal(void)
{
   int i, startx;

   // The way we handle segs depends on relative camera position. If the 
   // camera is above we need to reject segs based on the top of the seg.
   // If the camera is below the bottom of the seg the bottom edge needs 
   // to be clipped. This is done so visplanes will still be rendered 
   // fully.
   if(portalrender.w->type == pw_floor)
   {
      float top, topstep;
      float y1, y2;

      // I totally overlooked this when I moved all the wall panel projection 
      // to r_segs.c
      top = y1 = view.ycenter - (seg.top * seg.dist * view.yfoc);
      y2 = view.ycenter - (seg.top * seg.dist2 * view.yfoc);

      // SoM: Quickly reject the seg based on the bounding box of the portal
      if(portalrender.maxy - y1 < -1.0f && portalrender.maxy - y2 < -1.0f)
         return;
      if(seg.x1 > portalrender.maxx || seg.x2 < portalrender.minx)
         return;

      topstep = seg.x2frac > seg.x1frac ? (y2 - y1) / (seg.x2frac - seg.x1frac) : 0.0f;

      for(i = seg.x1; i <= seg.x2; i++)
      {
         // skip past the closed or out of sight columns to find the first visible column
         for(; i <= seg.x2 && floorclip[i] - top <= -1.0f; i++)
            top += topstep;

         if(i > seg.x2)
            return;

         startx = i; // mark

         // skip past visible columns 
         for(; i <= seg.x2 && floorclip[i] - top > -1.0f; i++)
            top += topstep;

         if(seg.clipsolid)
            R_ClipSolidWallSegment(startx, i - 1);
         else
            R_ClipPassWallSegment(startx, i - 1);
      }
   }
   else if(portalrender.w->type == pw_ceiling)
   {
      float bottom, bottomstep;
      float y1, y2;

      bottom = y1 = view.ycenter - (seg.bottom * seg.dist * view.yfoc) - 1;
      y2 = view.ycenter - (seg.bottom * seg.dist2 * view.yfoc) - 1;

      // SoM: Quickly reject the seg based on the bounding box of the portal
      if(y1 < portalrender.miny && y2 < portalrender.miny)
         return;
      if(seg.x1 > portalrender.maxx || seg.x2 < portalrender.minx)
         return;

      bottomstep = seg.x2frac > seg.x1frac ? (y2 - y1) / (seg.x2frac - seg.x1frac) : 0.0f;

      for(i = seg.x1; i <= seg.x2; i++)
      {
         for(; i <= seg.x2 && bottom < ceilingclip[i]; i++)
            bottom += bottomstep;

         if(i > seg.x2)
            return;

         startx = i;

         for(; i <= seg.x2 && bottom >= ceilingclip[i]; i++)
            bottom += bottomstep;

         if(seg.clipsolid)
            R_ClipSolidWallSegment(startx, i - 1);
         else
            R_ClipPassWallSegment(startx, i - 1);
      }
   }
   else
   {
      // Line based portal. This requires special clipping...
      // SoM: Quickly reject the seg based on the bounding box of the portal
      if(seg.x1 > portalrender.maxx || seg.x2 < portalrender.minx)
         return;

      // I'm not sure the first case ever even happens...
      if(!seg.floorplane && !seg.ceilingplane)
      {
         // Should this case ever actually happen?
         float top, topstep, bottom, bottomstep;
         float y1, y2;

         bottom = y1 = view.ycenter - (seg.bottom * seg.dist * view.yfoc) - 1;
         y2 = view.ycenter - (seg.bottom * seg.dist2 * view.yfoc) - 1;

         // SoM: Quickly reject the seg based on the bounding box of the portal
         if(y1 < portalrender.miny && y2 < portalrender.miny)
            return;

         bottomstep = seg.x2frac > seg.x1frac ? (y2 - y1) / (seg.x2frac - seg.x1frac) : 0.0f;

         // I totally overlooked this when I moved all the wall panel projection 
         // to r_segs.c
         top = y1 = view.ycenter - (seg.top * seg.dist * view.yfoc);
         y2 = view.ycenter - (seg.top * seg.dist2 * view.yfoc);

         // SoM: Quickly reject the seg based on the bounding box of the portal
         if(portalrender.maxy - y1 < -1.0f && portalrender.maxy - y2 < -1.0f)
            return;

         topstep = seg.x2frac > seg.x1frac ? (y2 - y1) / (seg.x2frac - seg.x1frac) : 0.0f;
         
         for(i = seg.x1; i <= seg.x2; i++)
         {
            for(; i <= seg.x2 && (bottom < ceilingclip[i] || floorclip[i] - top <= -1.0f); i++)
            {
               bottom += bottomstep;
               top += topstep;
            }

            if(i > seg.x2)
               return;

            startx = i;

            for(; i <= seg.x2 && bottom >= ceilingclip[i] && floorclip[i] - top > -1.0f; i++)
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
      else if(!seg.floorplane)
      {
         // If the seg has no floor plane, the camera is most likely below it,
         // so rejection is carried out as if the seg is being viewed through
         // a ceiling portal.

         float bottom, bottomstep;
         float y1, y2;

         bottom = y1 = view.ycenter - (seg.bottom * seg.dist * view.yfoc) - 1;
         y2 = view.ycenter - (seg.bottom * seg.dist2 * view.yfoc) - 1;

         // SoM: Quickly reject the seg based on the bounding box of the portal
         if(y1 < portalrender.miny && y2 < portalrender.miny)
            return;

         bottomstep = seg.x2frac > seg.x1frac ? (y2 - y1) / (seg.x2frac - seg.x1frac) : 0.0f;

         for(i = seg.x1; i <= seg.x2; i++)
         {
            for(; i <= seg.x2 && bottom < ceilingclip[i]; i++)
               bottom += bottomstep;

            if(i > seg.x2)
               return;

            startx = i;

            for(; i <= seg.x2 && bottom >= ceilingclip[i]; i++)
               bottom += bottomstep;

            if(seg.clipsolid)
               R_ClipSolidWallSegment(startx, i - 1);
            else
               R_ClipPassWallSegment(startx, i - 1);
         }
      }
      else if(!seg.ceilingplane)
      {
         // If the seg has no floor plane, the camera is most likely above it,
         // so rejection is carried out as if the seg is being viewed through
         // a floor portal.

         float top, topstep;
         float y1, y2;

         // I totally overlooked this when I moved all the wall panel projection 
         // to r_segs.c
         top = y1 = view.ycenter - (seg.top * seg.dist * view.yfoc);
         y2 = view.ycenter - (seg.top * seg.dist2 * view.yfoc);

         // SoM: Quickly reject the seg based on the bounding box of the portal
         if(portalrender.maxy - y1 < -1.0f && portalrender.maxy - y2 < -1.0f)
            return;

         topstep = seg.x2frac > seg.x1frac ? (y2 - y1) / (seg.x2frac - seg.x1frac) : 0.0f;

         for(i = seg.x1; i <= seg.x2; i++)
         {
            // skip past the closed or out of sight columns to find the first visible column
            for(; i <= seg.x2 && floorclip[i] - top <= -1.0f; i++)
               top += topstep;

            if(i > seg.x2)
               return;

            startx = i; // mark

            // skip past visible columns 
            for(; i <= seg.x2 && floorclip[i] - top > -1.0f; i++)
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
         for(i = seg.x1; i <= seg.x2; i++)
         {
            while(i <= seg.x2 && floorclip[i] < ceilingclip[i]) i++;

            if(i > seg.x2)
               return;

            startx = i;
            while(i <= seg.x2 && floorclip[i] >= ceilingclip[i]) i++;
            if(seg.clipsolid)
               R_ClipSolidWallSegment(startx, i - 1);
            else
               R_ClipPassWallSegment(startx, i - 1);

         }
      }
   }
}




//
// R_AddLine
//
// Clips the given segment
// and adds any visible pieces to the line list.
//
#define NEARCLIP 0.05f
#define PNEARCLIP 0.001f
extern int       *texturewidthmask;

static void R_AddLine(seg_t *line)
{
   float x1, x2;
   float toffsetx, toffsety;
   float i1, i2, pstep;
   float dx, dy, length;
   vertex_t  t1, t2, temp;
   side_t *side;
   static sector_t tempsec;
   float floorx1, floorx2;
   vertex_t  *v1, *v2;
   // SoM: one of the byproducts of the portal height enforcement: The top 
   // silhouette should be drawn at ceilingheight but the actual texture 
   // coords should start at ceilingz. Yeah Quasar, it did get a LITTLE 
   // complicated :/
   float textop, texhigh, texlow, texbottom;

   tempsec.frameid = 0;

   seg.clipsolid = false;
   if(line->backsector)
      seg.backsec = R_FakeFlat(line->backsector, &tempsec, NULL, NULL, true);
   else
      seg.backsec = NULL;
   seg.line = line;

   if(seg.backsec && seg.backsec->frameid != frameid)
   {
      seg.backsec->floorheightf   = (float)seg.backsec->floorheight / 65536.0f;
      seg.backsec->ceilingheightf = (float)seg.backsec->ceilingheight / 65536.0f;
      seg.backsec->frameid        = frameid;
   }

   // If the frontsector is closed, don't render the line!
   // This fixes a very specific type of slime trail.
   // Unless we are viewing down into a portal...??
   if(seg.frontsec->ceilingheight <= seg.frontsec->floorheight &&
      seg.frontsec->ceilingpic != skyflatnum &&
      seg.frontsec->ceilingpic != sky2flatnum &&
      !((R_LinkedCeilingActive(seg.frontsec) && 
        viewz > R_CPCam(seg.frontsec)->planez) || 
        (R_LinkedFloorActive(seg.frontsec) && 
        viewz < R_FPCam(seg.frontsec)->planez)))
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
      
      // killough 4/16/98: consider altered lighting
      && seg.backsec->floorlightsec   == seg.frontsec->floorlightsec
      && seg.backsec->ceilinglightsec == seg.frontsec->ceilinglightsec

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

      && !seg.line->linedef->portal
      )
      return;
      

   // The first step is to do calculations for the entire wall seg, then
   // send the wall to the clipping functions.
   v1 = line->v1;
   v2 = line->v2;

   if(v1->frameid != frameid)
   {
      temp.fx = v1->fx - view.x;
      temp.fy = v1->fy - view.y;
      t1.fx = v1->tx = (temp.fx * view.cos) - (temp.fy * view.sin);
      t1.fy = v1->ty = (temp.fy * view.cos) + (temp.fx * view.sin);
   }
   else
   {
      t1.fx = v1->tx;
      t1.fy = v1->ty;
   }

   if(v2->frameid != frameid)
   {
      temp.fx = v2->fx - view.x;
      temp.fy = v2->fy - view.y;
      t2.fx = v2->tx = (temp.fx * view.cos) - (temp.fy * view.sin);
      t2.fy = v2->ty = (temp.fy * view.cos) + (temp.fx * view.sin);
   }
   else
   {
      t2.fx = v2->tx;
      t2.fy = v2->ty;
   }

   // SoM: Portal lines are not texture and as a result can be clipped MUCH 
   // closer to the camera than normal lines can. This closer clipping 
   // distance is used to stave off the flash that can sometimes occur when
   // passing through a linked portal line.
   if(line->linedef->portal)
   {
      if(t1.fy < PNEARCLIP && t2.fy < PNEARCLIP)
         return; // Don't mark the vertices.

      toffsetx = toffsety = 0;
      if(t1.fy < PNEARCLIP)
      {
         float move, movey;

         // SoM: optimization would be to store the line slope in float 
         // format in the segs
         movey = PNEARCLIP - t1.fy;
         move = movey * ((t2.fx - t1.fx) / (t2.fy - t1.fy));

         t1.fx += move;
         toffsetx += (float)sqrt(move * move + movey * movey);
         t1.fy = PNEARCLIP;
         i1 = 1.0f / PNEARCLIP;
         x1 = (view.xcenter + (t1.fx * i1 * view.xfoc));
         // Don't mark the vertices.
      }
      else if(v1->frameid == frameid)
      {
         // if it's already marked that's ok
         i1 = v1->proj_dist;
         x1 = v1->proj_x;
      }
      else
      {
         i1 = 1.0f / t1.fy;
         x1 = (view.xcenter + (t1.fx * i1 * view.xfoc));
      }

      if(t2.fy < PNEARCLIP)
      {
         // SoM: optimization would be to store the line slope in float 
         // format in the segs
         t2.fx += (PNEARCLIP - t2.fy) * ((t2.fx - t1.fx) / (t2.fy - t1.fy));
         t2.fy = PNEARCLIP;
         i2 = 1.0f / PNEARCLIP;
         x2 = (view.xcenter + (t2.fx * i2 * view.xfoc));
         // Don't mark the vertices.
      }
      else if(v2->frameid == frameid)
      {
         // if it's already marked that's ok
         i2 = v2->proj_dist;
         x2 = v2->proj_x;
      }
      else
      {
         i2 = 1.0f / t2.fy;
         x2 = (view.xcenter + (t2.fx * i2 * view.xfoc));
      }
   }
   else
   {
      // Simple reject for lines entirely behind the view plane.
      if(t1.fy < NEARCLIP && t2.fy < NEARCLIP)
      {
         // Make sure both vertices are marked.
         v1->frameid = v2->frameid = frameid;
         return;
      }

      toffsetx = toffsety = 0;

      if(t1.fy < NEARCLIP)
      {
         float move, movey;

         // SoM: optimization would be to store the line slope in float 
         // format in the segs
         movey = NEARCLIP - t1.fy;
         move = movey * ((t2.fx - t1.fx) / (t2.fy - t1.fy));

         t1.fx += move;
         toffsetx += (float)sqrt(move * move + movey * movey);
         t1.fy = NEARCLIP;
         i1 = 1.0f / NEARCLIP;
         x1 = (view.xcenter + (t1.fx * i1 * view.xfoc));

         // SoM: you can't store the clipped vertex projection so mark it as
         // finished and t1.fy < NEARCLIP will be true when the vertex is used
         // again, so it won't be cached.
         v1->frameid = frameid;
      }
      else if(v1->frameid == frameid)
      {
         i1 = v1->proj_dist;
         x1 = v1->proj_x;
      }
      else
      {
         i1 = v1->proj_dist = 1.0f / t1.fy;
         x1 = v1->proj_x = (view.xcenter + (t1.fx * i1 * view.xfoc));
         v1->frameid = frameid;
      }


      if(t2.fy < NEARCLIP)
      {
         // SoM: optimization would be to store the line slope in float 
         // format in the segs
         t2.fx += (NEARCLIP - t2.fy) * ((t2.fx - t1.fx) / (t2.fy - t1.fy));
         t2.fy = NEARCLIP;
         i2 = 1.0f / NEARCLIP;
         x2 = (view.xcenter + (t2.fx * i2 * view.xfoc));

         // SoM: you can't store the clipped vertex projection so mark it as 
         // finished and t2.fy < NEARCLIP will be true when the vertex is used
         // again, so it won't be cached.
         v2->frameid = frameid;
      }
      else if(v2->frameid == frameid)
      {
         i2 = v2->proj_dist;
         x2 = v2->proj_x;
      }
      else
      {
         i2 = v2->proj_dist = 1.0f / t2.fy;
         x2 = v2->proj_x = (view.xcenter + (t2.fx * i2 * view.xfoc));
         v2->frameid = frameid;
      }
   }

   // SoM: Handle the case where a wall is only occupying a single post but 
   // still needs to be rendered to keep groups of single post walls from not
   // being rendered and causing slime trails.

   // 
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
   
   seg.toffsetx = toffsetx + (side->textureoffset / 65536.0f) 
                     + (line->offset / 65536.0f);
   seg.toffsety = toffsety + (side->rowoffset / 65536.0f);

   if(seg.toffsetx < 0)
   {
      float maxtexw;

      // SoM: ok, this was driving me crazy. It seems that when the offset is 
      // less than 0, the fractional part will cause the texel at 
      // 0 + abs(seg.toffsetx) to double and it will strip the first texel to
      // one column. This is because -64 + ANY FRACTION is going to cast to 63
      // and when you cast -0.999 through 0.999 it will cast to 0. The first 
      // step is to find the largest texture width on the line to make sure all
      // the textures will start at the same column when the offsets are 
      // adjusted.

      maxtexw = 0.0f;
      if(side->toptexture)
         maxtexw = (float)texturewidthmask[side->toptexture];
      if(side->midtexture && texturewidthmask[side->midtexture] > maxtexw)
         maxtexw = (float)texturewidthmask[side->midtexture];
      if(side->bottomtexture && texturewidthmask[side->bottomtexture] > maxtexw)
         maxtexw = (float)texturewidthmask[side->bottomtexture];

      // Then adjust the offset to zero or the first positive value that will 
      // repeat correctly with the largest texture on the line.
      if(maxtexw)
      {
         maxtexw++;
         while(seg.toffsetx < 0.0f) 
            seg.toffsetx += maxtexw;
      }
   }

   dx = t2.fx - t1.fx;
   dy = t2.fy - t1.fy;
   length = (float)sqrt(dx * dx + dy * dy);

   seg.dist = i1;
   seg.dist2 = i2;
   seg.diststep = (i2 - i1) * pstep;

   seg.len = 0;
   seg.len2 = length * i2 * view.yfoc;
   seg.lenstep = length * i2 * pstep * view.yfoc;

   seg.side = side;

   seg.top = seg.frontsec->ceilingheightf - view.z;
   seg.bottom = seg.frontsec->floorheightf - view.z;

   textop = (seg.frontsec->ceilingz / 65536.0f) - view.z;
   texbottom = (seg.frontsec->floorz / 65536.0f) - view.z;

   seg.f_portalignore = seg.c_portalignore = false;

   if(!seg.backsec)
   {
      seg.twosided = false;
      seg.toptex   = seg.bottomtex = 0;
      seg.midtex   = texturetranslation[side->midtexture];
      seg.midtexh  = textureheight[side->midtexture] >> FRACBITS;

      if(seg.line->linedef->flags & ML_DONTPEGBOTTOM)
         seg.midtexmid = (int)((texbottom + seg.midtexh + seg.toffsety) * FRACUNIT);
      else
         seg.midtexmid = (int)((textop + seg.toffsety) * FRACUNIT);

      // SoM: these should be treated differently! 
      seg.markcportal = R_RenderCeilingPortal(seg.frontsec);
      seg.c_window    = seg.markcportal ?
                        R_GetCeilingPortalWindow(seg.frontsec->c_portal) :
                        NULL;

      seg.markfportal = R_RenderFloorPortal(seg.frontsec);
      seg.f_window    = seg.markfportal ?
                        R_GetFloorPortalWindow(seg.frontsec->f_portal) :
                        NULL;

      seg.markceiling = (seg.ceilingplane != NULL);
      seg.markfloor   = (seg.floorplane != NULL);
      seg.clipsolid   = true;
      seg.segtextured = (seg.midtex != 0);
      seg.l_window    = line->linedef->portal ?
                        R_GetLinePortalWindow(line->linedef->portal, line->linedef) : NULL;

#ifdef R_LINKEDPORTALS
      // haleyjd 03/12/06: inverted predicates to simplify
      if(seg.frontsec->f_portal && seg.frontsec->f_portal->type != R_LINKED && 
         seg.frontsec->f_portal->type != R_TWOWAY)
         seg.f_portalignore = true;
      if(seg.frontsec->c_portal && seg.frontsec->c_portal->type != R_LINKED && 
         seg.frontsec->c_portal->type != R_TWOWAY)
         seg.c_portalignore = true;
#endif
   }
   else
   {
      boolean mark; // haleyjd
      boolean uppermissing, lowermissing;

      seg.twosided = true;

      // haleyjd 03/04/07: reformatted and eliminated numerous unnecessary
      // conditional statements (predicates already provide the needed
      // true/false value without a branch). Also added tweak for sector
      // colormaps.

      mark = (seg.frontsec->lightlevel != seg.backsec->lightlevel ||
              seg.frontsec->heightsec != -1 ||
              seg.frontsec->heightsec != seg.backsec->heightsec ||
              seg.frontsec->midmap != seg.backsec->midmap); // haleyjd

      seg.high = seg.backsec->ceilingheightf - view.z;
      texhigh = (seg.backsec->ceilingz / 65536.0f) - view.z;

      uppermissing = (seg.frontsec->ceilingheight > seg.backsec->ceilingheight &&
                      seg.side->toptexture == 0);

      lowermissing = (seg.frontsec->floorheight < seg.backsec->floorheight &&
                      seg.side->bottomtexture == 0);

      if((seg.frontsec->ceilingpic == skyflatnum ||
              seg.frontsec->ceilingpic == sky2flatnum) &&
             (seg.backsec->ceilingpic  == skyflatnum ||
              seg.backsec->ceilingpic  == sky2flatnum))
      {
         seg.top = seg.high;
         //uppermissing = false;
      }

      // New clipsolid code will emulate the old doom behavior and still manages to 
      // keep valid closed door cases handled.
      seg.clipsolid = ((seg.backsec->floorheight != seg.frontsec->floorheight ||
          seg.backsec->ceilingheight != seg.frontsec->ceilingheight) &&
          (seg.backsec->floorheight >= seg.frontsec->ceilingheight ||
           seg.backsec->ceilingheight <= seg.frontsec->floorheight ||
           (seg.backsec->ceilingheight <= seg.backsec->floorheight && !uppermissing && !lowermissing)));

      seg.markcportal = 
         (R_RenderCeilingPortal(seg.frontsec) && 
         (seg.clipsolid || seg.top != seg.high || 
          seg.frontsec->c_portal != seg.backsec->c_portal));

      seg.c_window = seg.markcportal ? 
                     R_GetCeilingPortalWindow(seg.frontsec->c_portal) : NULL;

      seg.markceiling = 
         (seg.ceilingplane && 
          (mark || seg.clipsolid || seg.top != seg.high || 
           seg.frontsec->ceiling_xoffs != seg.backsec->ceiling_xoffs ||
           seg.frontsec->ceiling_yoffs != seg.backsec->ceiling_yoffs ||
           seg.frontsec->ceilingpic != seg.backsec->ceilingpic ||
           seg.frontsec->ceilinglightsec != seg.backsec->ceilinglightsec ||
           seg.frontsec->topmap != seg.backsec->topmap ||
           seg.frontsec->c_portal != seg.backsec->c_portal)); // haleyjd

      if(seg.high < seg.top && side->toptexture)
      {
         seg.toptex = texturetranslation[side->toptexture];
         seg.toptexh = textureheight[side->toptexture] >> FRACBITS;

         if(seg.line->linedef->flags & ML_DONTPEGTOP)
            seg.toptexmid = (int)((textop + seg.toffsety) * FRACUNIT);
         else
            seg.toptexmid = (int)((texhigh + seg.toptexh + seg.toffsety) * FRACUNIT);
      }
      else
         seg.toptex = 0;

      seg.markfportal = 
         (R_RenderFloorPortal(seg.frontsec) &&
         (seg.clipsolid || seg.frontsec->floorheight != seg.backsec->floorheight ||
          seg.frontsec->f_portal != seg.backsec->f_portal));

      seg.f_window = seg.markfportal ? 
                     R_GetFloorPortalWindow(seg.frontsec->f_portal) : NULL;

      seg.markfloor = 
         (seg.floorplane && 
          (mark || seg.clipsolid ||  
           seg.frontsec->floorheight != seg.backsec->floorheight ||
           seg.frontsec->floor_xoffs != seg.backsec->floor_xoffs ||
           seg.frontsec->floor_yoffs != seg.backsec->floor_yoffs ||
           seg.frontsec->floorpic != seg.backsec->floorpic ||
           seg.frontsec->floorlightsec != seg.backsec->floorlightsec ||
           seg.frontsec->bottommap != seg.backsec->bottommap ||
           seg.frontsec->f_portal != seg.backsec->f_portal)); // haleyjd

#ifdef R_LINKEDPORTALS
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
#endif

      seg.low = seg.backsec->floorheightf - view.z;
      texlow = (seg.backsec->floorz / 65536.0f) - view.z;
      if(seg.bottom < seg.low && side->bottomtexture)
      {
         seg.bottomtex = texturetranslation[side->bottomtexture];
         seg.bottomtexh = textureheight[side->bottomtexture] >> FRACBITS;

         if(seg.line->linedef->flags & ML_DONTPEGBOTTOM)
            seg.bottomtexmid = (int)((textop + seg.toffsety) * FRACUNIT);
         else
            seg.bottomtexmid = (int)((texlow + seg.toffsety) * FRACUNIT);
      }
      else
         seg.bottomtex = 0;

      seg.midtex = 0;
      seg.maskedtex = !!seg.side->midtexture;
      seg.segtextured = (seg.maskedtex || seg.bottomtex || seg.toptex);

      seg.l_window = line->linedef->portal &&
                     line->linedef->sidenum[0] != line->linedef->sidenum[1] &&
                     line->linedef->sidenum[0] == line->sidedef - sides ?
                     R_GetLinePortalWindow(line->linedef->portal, line->linedef) : NULL;
   }

   if(x1 < 0)
   {
      seg.dist += seg.diststep * -x1;
      seg.len += seg.lenstep * -x1;
      seg.x1frac = 0.0f;
      seg.x1 = 0;
   }
   else
   {
      seg.x1 = (int)floorx1;
      seg.x1frac = x1;
   }

   if(x2 >= view.width)
   {
      float clipx = x2 - view.width + 1.0f;

      seg.dist2 -= seg.diststep * clipx;
      seg.len2 -= seg.lenstep * clipx;

      seg.x2frac = view.width - 1.0f;
      seg.x2 = viewwidth - 1;
   }
   else
   {
      seg.x2 = (int)floorx2;
      seg.x2frac = x2;
   }

   if(portalrender.active)
      R_ClipSegToPortal();
   else if(seg.clipsolid)
      R_ClipSolidWallSegment(seg.x1, seg.x2);
   else
      R_ClipPassWallSegment(seg.x1, seg.x2);

   // Add new solid segs when it is safe to do so...
   R_AddMarkedSegs();
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
static boolean R_CheckBBox(fixed_t *bspcoord) // killough 1/28/98: static
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
      
      angle2 = -clipangle;
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
   if(sx2 < viewwidth - 1) sx2++;

   // SoM: Removed the "does not cross a pixel" test

   start = solidsegs;
   while(start->last < sx2)
      ++start;
   
   if(sx1 >= start->first && sx2 <= start->last)
      return false;      // The clippost contains the new span.
   
   return true;
}

static int numpolys;         // number of polyobjects in current subsector
static int num_po_ptrs;      // number of polyobject pointers allocated
static rpolyobj_t **po_ptrs; // temp ptr array to sort polyobject pointers

//
// R_PolyobjCompare
//
// Callback for qsort that compares the z distance of two polyobjects.
// Returns the difference such that the closer polyobject will be
// sorted first.
//
static int R_PolyobjCompare(const void *p1, const void *p2)
{
   const rpolyobj_t *po1 = *(rpolyobj_t **)p1;
   const rpolyobj_t *po2 = *(rpolyobj_t **)p2;

   return po1->polyobj->zdist - po2->polyobj->zdist;
}

//
// R_SortPolyObjects
//
// haleyjd 03/03/06: Here's the REAL meat of Eternity's polyobject system.
// Hexen just figured this was impossible, but as mentioned in polyobj.c,
// it is perfectly doable within the confines of the BSP tree. Polyobjects
// must be sorted to draw in DOOM's front-to-back order within individual
// subsectors. This is a modified version of R_SortVisSprites.
//
static void R_SortPolyObjects(rpolyobj_t *rpo)
{
   int i = 0;
         
   while(rpo)
   {
      polyobj_t *po = rpo->polyobj;

      po->zdist = R_PointToDist2(viewx, viewy, po->centerPt.x, po->centerPt.y);
      po_ptrs[i++] = rpo;
      rpo = (rpolyobj_t *)(rpo->link.next);
   }
   
   // the polyobjects are NOT in any particular order, so use qsort
   qsort(po_ptrs, numpolys, sizeof(rpolyobj_t *), R_PolyobjCompare);
}

static void R_AddDynaSegs(subsector_t *sub)
{
   rpolyobj_t *rpo = (rpolyobj_t *)(sub->polyList->link.next);
   int i;

   numpolys = 1; // we know there is at least one

   // count polyobject fragments
   while(rpo)
   {
      ++numpolys;
      rpo = (rpolyobj_t *)(rpo->link.next);
   }

   // allocate twice the number needed to minimize allocations
   if(num_po_ptrs < numpolys*2)
   {
      // use free instead realloc since faster (thanks Lee ^_^)
      free(po_ptrs);
      po_ptrs = malloc((num_po_ptrs = numpolys*2) * sizeof(*po_ptrs));
   }

   // sort polyobjects if necessary
   if(numpolys > 1)
      R_SortPolyObjects(sub->polyList);
   else
      po_ptrs[0] = sub->polyList;

   // render polyobject fragments
   for(i = 0; i < numpolys; ++i)
   {
      dynaseg_t *ds = po_ptrs[i]->dynaSegs;

      while(ds)
      {
         R_AddLine(&ds->seg);

         ds = ds->subnext;
      }
   }
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
   
#ifdef RANGECHECK
   if(num >= numsubsectors)
      I_Error("R_Subsector: ss %i with numss = %i", num, numsubsectors);
#endif

   // haleyjd 09/22/07: clear seg structure
   memset(&seg, 0, sizeof(cb_seg_t));

   sub = &subsectors[num];
   seg.frontsec = sub->sector;
   count = sub->numlines;
   line = &segs[sub->firstline];

   R_SectorColormap(seg.frontsec);

   // SoM: Cardboard optimization
   tempsec.frameid = 0;
   
   // killough 3/8/98, 4/4/98: Deep water / fake ceiling effect
   seg.frontsec = R_FakeFlat(seg.frontsec, &tempsec, &floorlightlevel,
                            &ceilinglightlevel, false);   // killough 4/11/98

   if(seg.frontsec->frameid != frameid)
   {
      seg.frontsec->floorheightf = (float)seg.frontsec->floorheight / 65536.0f;
      seg.frontsec->ceilingheightf = (float)seg.frontsec->ceilingheight / 65536.0f;
      seg.frontsec->frameid = frameid;
   }

   // haleyjd 01/05/08: determine angles for floor and ceiling
   floorangle   = seg.frontsec->floorbaseangle   + seg.frontsec->floorangle;
   ceilingangle = seg.frontsec->ceilingbaseangle + seg.frontsec->ceilingangle;

   // killough 3/7/98: Add (x,y) offsets to flats, add deep water check
   // killough 3/16/98: add floorlightlevel
   // killough 10/98: add support for skies transferred from sidedefs

   // SoM: If there is an active portal, forget about the floorplane.
   seg.floorplane = !R_FloorPortalActive(seg.frontsec) && 
     (seg.frontsec->floorheight < viewz || // killough 3/7/98
     (seg.frontsec->heightsec != -1 &&
      (sectors[seg.frontsec->heightsec].ceilingpic == skyflatnum ||
       sectors[seg.frontsec->heightsec].ceilingpic == sky2flatnum))) ?
     R_FindPlane(seg.frontsec->floorheight, 
                 (seg.frontsec->floorpic == skyflatnum ||
                  seg.frontsec->floorpic == sky2flatnum) &&  // kilough 10/98
                 seg.frontsec->sky & PL_SKYFLAT ? seg.frontsec->sky :
                 seg.frontsec->floorpic,
                 floorlightlevel,                // killough 3/16/98
                 seg.frontsec->floor_xoffs,       // killough 3/7/98
                 seg.frontsec->floor_yoffs,
                 floorangle) : NULL;

   seg.ceilingplane = !R_CeilingPortalActive(seg.frontsec) &&
     (seg.frontsec->ceilingheight > viewz ||
     (seg.frontsec->ceilingpic == skyflatnum ||
      seg.frontsec->ceilingpic == sky2flatnum) ||
     (seg.frontsec->heightsec != -1 &&
      (sectors[seg.frontsec->heightsec].floorpic == skyflatnum ||
       sectors[seg.frontsec->heightsec].floorpic == sky2flatnum))) ?
     R_FindPlane(seg.frontsec->ceilingheight,     // killough 3/8/98
                 (seg.frontsec->ceilingpic == skyflatnum ||
                  seg.frontsec->ceilingpic == sky2flatnum) &&  // kilough 10/98
                 seg.frontsec->sky & PL_SKYFLAT ? seg.frontsec->sky :
                 seg.frontsec->ceilingpic,
                 ceilinglightlevel,              // killough 4/11/98
                 seg.frontsec->ceiling_xoffs,     // killough 3/7/98
                 seg.frontsec->ceiling_yoffs,
                 ceilingangle) : NULL;
  
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
   {
      if(!line->nodraw) // haleyjd
         R_AddLine(line++);
   }
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
