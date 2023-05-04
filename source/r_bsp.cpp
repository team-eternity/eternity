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
#include "r_context.h"
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
static constexpr float kPortalSegRejectionFudge = 1.f / 256;

// killough 4/7/98: indicates doors closed wrt automap bugfix:
int      doorclosed;


//
// R_ClearDrawSegs
//
void R_ClearDrawSegs(bspcontext_t &context)
{
   context.ds_p = context.drawsegs;
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
// clip based on solidsegs because the first solidseg is from MININT,
// context.startcolumn - 1 to context.endcolumn, MAXINT

struct cliprange_t
{
  int first, last;
};


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

#define MAXSEGS (w/2+r_numcontexts)   /* killough 1/11/98, 2/8/98 */

static cliprange_t *g_solidsegs = nullptr;

VALLOCATION(solidsegs)
{
   g_solidsegs = ecalloctag(cliprange_t *, MAXSEGS * 2, sizeof(cliprange_t), PU_VALLOC, nullptr);

   r_globalcontext.bspcontext.solidsegs = g_solidsegs;
   r_globalcontext.bspcontext.addedsegs = g_solidsegs + (r_globalcontext.bounds.numcolumns / 2 + 1);

   if(r_numcontexts > 1)
   {
      cliprange_t *buf = g_solidsegs;
      for(int i = 0; i < r_numcontexts; i++)
      {
         rendercontext_t &basecontext = R_GetContext(i);
         bspcontext_t    &context     = basecontext.bspcontext;
         ZoneHeap        &heap        = *basecontext.heap;
         const int        CONTEXTSEGS = basecontext.bounds.numcolumns / 2 + 1;

         context.solidsegs  = buf;
         buf               += CONTEXTSEGS;
         context.addedsegs  = buf;
         buf               += CONTEXTSEGS;
      }
   }
}


static void R_addSolidSeg(bspcontext_t &context,
                          int x1, int x2)
{
   cliprange_t *&solidsegs = context.solidsegs;
   cliprange_t *&newend    = context.newend;

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
         I_Error("R_addSolidSeg: created a seg that overlaps next seg:\n"
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

void R_MarkSolidSeg(bspcontext_t &context, int x1, int x2)
{
   context.addend->first = x1;
   context.addend->last = x2;
   context.addend++;
}

static void R_addMarkedSegs(bspcontext_t &context)
{
   cliprange_t *r;

   for(r = context.addedsegs; r < context.addend; r++)
      R_addSolidSeg(context, r->first, r->last);

   context.addend = context.addedsegs;
}

//
// Handles solid walls,
//  e.g. single sided LineDefs (middle texture)
//  that entirely block the view.
//
static void R_clipSolidWallSegment(bspcontext_t &bspcontext, cmapcontext_t &cmapcontext,
                                   planecontext_t &planecontext, portalcontext_t &portalcontext,
                                   ZoneHeap &heap, const viewpoint_t &viewpoint,
                                   const cbviewpoint_t &cb_viewpoint, const contextbounds_t &bounds,
                                   const cb_seg_t &seg, const int x1, const int x2)
{
   cliprange_t *&solidsegs = bspcontext.solidsegs;
   cliprange_t *&newend    = bspcontext.newend;

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
         R_StoreWallRange(
            bspcontext, cmapcontext, planecontext, portalcontext, heap,
            viewpoint, cb_viewpoint, bounds, seg, x1, x2
         );

         // 1/11/98 killough: performance tuning using fast memmove
         memmove(start + 1, start, (++newend - start) * sizeof(*start));
         start->first = x1;
         start->last = x2;
         goto verifysegs;
      }

      // There is a fragment above *start.
      R_StoreWallRange(
         bspcontext, cmapcontext, planecontext, portalcontext, heap,
         viewpoint, cb_viewpoint, bounds, seg, x1, start->first - 1
      );

      // Now adjust the clip size.
      start->first = x1;
   }

   // Bottom contained in start?
   if(x2 <= start->last)
      goto verifysegs;

   next = start;
   while(x2 >= (next + 1)->first - 1)
   {      // There is a fragment between two posts.
      R_StoreWallRange(
         bspcontext, cmapcontext, planecontext, portalcontext, heap,
         viewpoint, cb_viewpoint,
         bounds, seg, next->last + 1, (next + 1)->first - 1
      );
      ++next;
      if(x2 <= next->last)
      {  
         // Bottom is contained in next. Adjust the clip size.
         start->last = next->last;
         goto crunch;
      }
   }

   // There is a fragment after *next.
   R_StoreWallRange(
      bspcontext, cmapcontext, planecontext, portalcontext, heap,
      viewpoint, cb_viewpoint, bounds, seg, next->last + 1, x2
   );

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
         I_Error("R_clipSolidWallSegment: created a seg that overlaps next seg:\n"
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
// Clips the given range of columns,
//  but does not includes it in the clip list.
// Does handle windows,
//  e.g. LineDefs with upper and lower texture.
//
static void R_clipPassWallSegment(bspcontext_t &bspcontext, cmapcontext_t &cmapcontext,
                                  planecontext_t &planecontext, portalcontext_t &portalcontext,
                                  ZoneHeap &heap,
                                  const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                                  const contextbounds_t &bounds, const cb_seg_t &seg,
                                  const int x1, const int x2)
{
   cliprange_t *&solidsegs = bspcontext.solidsegs;
   cliprange_t *&newend    = bspcontext.newend;

   const cliprange_t *start;
   
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
         R_StoreWallRange(
            bspcontext, cmapcontext, planecontext, portalcontext, heap,
            viewpoint, cb_viewpoint, bounds, seg, x1, x2
         );
         return;
      }

      // There is a fragment above *start.
      R_StoreWallRange(
         bspcontext, cmapcontext, planecontext, portalcontext, heap,
         viewpoint, cb_viewpoint, bounds, seg, x1, start->first - 1
      );
   }

   // Bottom contained in start?
   if(x2 <= start->last)
      return;

   while(x2 >= (start + 1)->first - 1)
   {
      // There is a fragment between two posts.
      R_StoreWallRange(
         bspcontext, cmapcontext, planecontext, portalcontext, heap,
         viewpoint, cb_viewpoint,
         bounds, seg, start->last + 1, (start + 1)->first - 1
      );
      ++start;
      
      if(x2 <= start->last)
         return;
   }
   
   // There is a fragment after *next.
   R_StoreWallRange(
      bspcontext, cmapcontext, planecontext, portalcontext, heap,
      viewpoint, cb_viewpoint, bounds, seg, start->last + 1, x2
   );
}

//
// Sets up the solid seg pointers
//
void R_SetupSolidSegs()
{
   r_globalcontext.bspcontext.solidsegs = g_solidsegs;
   r_globalcontext.bspcontext.addedsegs = g_solidsegs + (r_globalcontext.bounds.numcolumns / 2 + 1);

   if(r_numcontexts > 1)
   {
      cliprange_t *buf = g_solidsegs;
      for(int i = 0; i < r_numcontexts; i++)
      {
         rendercontext_t &basecontext = R_GetContext(i);
         bspcontext_t    &context     = basecontext.bspcontext;
         const int        CONTEXTSEGS = basecontext.bounds.numcolumns / 2 + 1;

         context.solidsegs  = buf;
         buf               += CONTEXTSEGS;
         context.addedsegs  = buf;
         buf               += CONTEXTSEGS;
      }
   }
}

//
// R_ClearClipSegs
//
void R_ClearClipSegs(bspcontext_t &context, const contextbounds_t &bounds)
{
   context.solidsegs[0].first = D_MININT + 1;
   context.solidsegs[0].last  = bounds.startcolumn - 1;
   context.solidsegs[1].first = bounds.endcolumn;
   context.solidsegs[1].last  = D_MAXINT - 1;
   context.newend = context.solidsegs+2;
   context.addend = context.addedsegs;
}

//
// R_SetupPortalClipsegs
//
bool R_SetupPortalClipsegs(bspcontext_t &context, const contextbounds_t &bounds,
                           portalrender_t &portalrender,
                           int minx, int maxx, const float *top, const float *bottom)
{
   cliprange_t *&solidsegs = context.solidsegs;
   cliprange_t *&newend    = context.newend;

   int i = minx, stop = maxx + 1;
   cliprange_t *solidseg = solidsegs;
   
   R_ClearClipSegs(context, bounds);

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
      
      if(i == bounds.endcolumn)
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
// Get surface light level based on rules
//
int R_GetSurfaceLightLevel(surf_e surf, const sector_t *sec)
{
   return (sec->flags & secf_surfLightAbsolute[surf] ? 0 : sec->srf[surf].lightsec == -1 ?
      sec->lightlevel : sectors[sec->srf[surf].lightsec].lightlevel) + sec->srf[surf].lightdelta;
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

const sector_t *R_FakeFlat(const fixed_t viewz, const sector_t *sec, sector_t *tempsec,
                           int *floorlightlevel, int *ceilinglightlevel, bool back)
{
   if(!sec)
      return nullptr;

   if(floorlightlevel)
      *floorlightlevel = R_GetSurfaceLightLevel(surf_floor, sec);

   // killough 4/11/98
   if(ceilinglightlevel)
      *ceilinglightlevel = R_GetSurfaceLightLevel(surf_ceil, sec);

   if(sec->heightsec != -1 || sec->srf.floor.portal || sec->srf.ceiling.portal)
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

      underwater = (heightsec != -1 && viewz <= sectors[heightsec].srf.floor.height);

      // Replace floor and ceiling height with other sector's heights.
      tempsec->srf.floor.height = s->srf.floor.height;
      tempsec->srf.ceiling.height = s->srf.ceiling.height;

      // killough 11/98: prevent sudden light changes from non-water sectors:
      if(underwater && (tempsec->srf.floor.height = sec->srf.floor.height,
                        tempsec->srf.ceiling.height = s->srf.floor.height -1, !back))
      {
         // SoM: kill any ceiling portals that may try to render
         tempsec->srf.ceiling.portal = nullptr;

         // head-below-floor hack
         tempsec->srf.floor.pic = s->srf.floor.pic;
         tempsec->srf.floor.offset = s->srf.floor.offset;
         tempsec->srf.floor.scale = s->srf.floor.scale;
         tempsec->srf.floor.baseangle = s->srf.floor.baseangle; // haleyjd: angles
         tempsec->srf.floor.angle = s->srf.floor.angle;

         // haleyjd 03/13/05: removed redundant if(underwater) check
         if(s->intflags & SIF_SKY)
         {
            tempsec->srf.floor.height = tempsec->srf.ceiling.height +1;
            tempsec->srf.floor.pic = tempsec->srf.floor.pic;
            tempsec->srf.ceiling.offset = tempsec->srf.floor.offset;
            tempsec->srf.ceiling.scale = tempsec->srf.floor.scale;
            tempsec->srf.ceiling.baseangle = tempsec->srf.floor.baseangle; // haleyjd: angles
            tempsec->srf.ceiling.angle = tempsec->srf.floor.angle;
         }
         else
         {
            tempsec->srf.ceiling.pic = s->srf.ceiling.pic;
            tempsec->srf.ceiling.offset = s->srf.ceiling.offset;
            tempsec->srf.ceiling.scale = s->srf.ceiling.scale;
            tempsec->srf.ceiling.baseangle = s->srf.ceiling.baseangle; // haleyjd: angles
            tempsec->srf.ceiling.angle = s->srf.ceiling.angle;
         }

         // haleyjd 03/20/10: must clear SIF_SKY flag from tempsec!
         // ioanch 20160205: not always
         if(!R_IsSkyFlat(tempsec->srf.ceiling.pic))
            tempsec->intflags &= ~SIF_SKY;
         else
            tempsec->intflags |= SIF_SKY;

         tempsec->lightlevel  = s->lightlevel;
         
         if(floorlightlevel)
         {
            *floorlightlevel =
            (s->flags & SECF_FLOORLIGHTABSOLUTE ? 0 : s->srf.floor.lightsec == -1 ?
             s->lightlevel : sectors[s->srf.floor.lightsec].lightlevel)
            + s->srf.floor.lightdelta;
            // killough 3/16/98
         }

         if (ceilinglightlevel)
         {
            *ceilinglightlevel =
            (s->flags & SECF_CEILLIGHTABSOLUTE ? 0 : s->srf.ceiling.lightsec == -1 ?
             s->lightlevel : sectors[s->srf.ceiling.lightsec].lightlevel)
            + s->srf.ceiling.lightdelta;
            // killough 4/11/98
         }
      }
      else if(heightsec != -1 &&
              viewz >= sectors[heightsec].srf.ceiling.height &&
              sec->srf.ceiling.height > s->srf.ceiling.height)
      {
         // SoM: kill any floor portals that may try to render
         tempsec->srf.floor.portal = nullptr;

         // Above-ceiling hack
         tempsec->srf.ceiling.height = s->srf.ceiling.height;
         tempsec->srf.floor.height = s->srf.ceiling.height + 1;

         tempsec->srf.floor.pic = tempsec->srf.ceiling.pic = s->srf.ceiling.pic;
         tempsec->srf.floor.offset = tempsec->srf.ceiling.offset = s->srf.ceiling.offset;
         tempsec->srf.floor.scale = tempsec->srf.ceiling.scale = s->srf.ceiling.scale;
         tempsec->srf.floor.baseangle = tempsec->srf.ceiling.baseangle = s->srf.ceiling.baseangle;
         tempsec->srf.floor.angle = tempsec->srf.ceiling.angle = s->srf.ceiling.angle; // haleyjd: angles

         if(!R_IsSkyFlat(s->srf.floor.pic))
         {
            tempsec->srf.ceiling.height = sec->srf.ceiling.height;
            tempsec->srf.floor.pic = s->srf.floor.pic;
            tempsec->srf.floor.offset = s->srf.floor.offset;
            tempsec->srf.floor.scale = s->srf.floor.scale;
            tempsec->srf.floor.baseangle = s->srf.floor.baseangle; // haleyjd: angles
            tempsec->srf.floor.angle = s->srf.floor.angle;
         }

         // haleyjd 03/20/10: must clear SIF_SKY flag from tempsec
         // ioanch 20160205: not always
         if(!R_IsSkyFlat(tempsec->srf.ceiling.pic))
            tempsec->intflags &= ~SIF_SKY;
         else
            tempsec->intflags |= SIF_SKY;
         
         tempsec->lightlevel  = s->lightlevel;
         
         if(floorlightlevel)
         {
            *floorlightlevel =
            (s->flags & SECF_FLOORLIGHTABSOLUTE ? 0 : s->srf.floor.lightsec == -1 ?
             s->lightlevel : sectors[s->srf.floor.lightsec].lightlevel)
            + s->srf.floor.lightdelta;
            // killough 3/16/98
         }

         if(ceilinglightlevel)
         {
            *ceilinglightlevel =
            (s->flags & SECF_CEILLIGHTABSOLUTE ? 0 : s->srf.ceiling.lightsec == -1 ?
             s->lightlevel : sectors[s->srf.ceiling.lightsec].lightlevel)
            + s->srf.ceiling.lightdelta;
            // killough 4/11/98
         }
      }
      else if(heightsec != -1)
      {
         if(sec->srf.ceiling.height != s->srf.ceiling.height)
            tempsec->srf.ceiling.portal = nullptr;
         if(sec->srf.floor.height != s->srf.floor.height)
            tempsec->srf.floor.portal = nullptr;
      }

      tempsec->srf.ceiling.heightf = M_FixedToFloat(tempsec->srf.ceiling.height);
      tempsec->srf.floor.heightf = M_FixedToFloat(tempsec->srf.floor.height);
      sec = tempsec;            // Use other sector
   }

   if(sec->srf.ceiling.portal)
   {
      if(sec->srf.ceiling.portal->type == R_LINKED && !(sec->srf.ceiling.pflags & PF_ATTACHEDPORTAL))
      {
         if(sec->srf.ceiling.height < R_CPLink(sec)->planez)
         {
            tempsec->srf.ceiling.portal = nullptr;
            tempsec->srf.ceiling.pflags = 0;
         }
         else
            P_SetSectorHeight(*tempsec, surf_ceil, R_CPLink(sec)->planez);
         sec = tempsec;
      }
      else if(!(sec->srf.ceiling.pflags & PS_VISIBLE))
      {
         tempsec->srf.ceiling.portal = nullptr;
         tempsec->srf.ceiling.pflags = 0;
         sec = tempsec;
      }
   }
   if(sec->srf.floor.portal)
   {
      if(sec->srf.floor.portal->type == R_LINKED && !(sec->srf.floor.pflags & PF_ATTACHEDPORTAL))
      {
         if(sec->srf.floor.height > R_FPLink(sec)->planez)
         {
            tempsec->srf.floor.portal = nullptr;
            tempsec->srf.floor.pflags = 0;
         }
         else
            P_SetSectorHeight(*tempsec, surf_floor, R_FPLink(sec)->planez);
            
         sec = tempsec;
      }
      else if(!(sec->srf.floor.pflags & PS_VISIBLE))
      {
         tempsec->srf.floor.portal = nullptr;
         tempsec->srf.floor.pflags = 0;
         sec = tempsec;
      }
   }

   return sec;
}

//
// As R_FakeFlat but it only calculates the overall lighting (for sprites)
//
int R_FakeFlatSpriteLighting(const fixed_t viewz, const sector_t *sec)
{
   if(!sec)
      return 0;

   int floorlightlevel   = R_GetSurfaceLightLevel(surf_floor, sec);
   int ceilinglightlevel = R_GetSurfaceLightLevel(surf_ceil, sec);

   if(sec->heightsec != -1)
   {
      const sector_t *s = &sectors[sec->heightsec];

      // Get from view.sector due to interpolation
      const int  heightsec = view.sector->heightsec;
      const bool underwater = (heightsec != -1 && viewz <= sectors[heightsec].srf.floor.height);

      // Prevent sudden light changes from non-water sectors:
      if(underwater ||
         (heightsec != -1 && viewz >= sectors[heightsec].srf.ceiling.height &&
          sec->srf.ceiling.height > s->srf.ceiling.height))
      {
         floorlightlevel = s->srf.floor.lightdelta;
         if(!(s->flags & SECF_FLOORLIGHTABSOLUTE))
         {
            if(s->srf.floor.lightsec == -1)
               floorlightlevel += s->lightlevel;
            else
               floorlightlevel += sectors[s->srf.floor.lightsec].lightlevel;
         }

         ceilinglightlevel = s->srf.ceiling.lightdelta;
         if(!(s->flags & SECF_CEILLIGHTABSOLUTE))
         {
            if(s->srf.ceiling.lightsec == -1)
               ceilinglightlevel += s->lightlevel;
            else
               ceilinglightlevel += sectors[s->srf.ceiling.lightsec].lightlevel;
         }
      }
   }

   return (floorlightlevel + ceilinglightlevel) / 2;
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

VALLOCATION(slopemark)
{
   R_ForEachContext([w](rendercontext_t &basecontext) {
      bspcontext_t &context =  basecontext.bspcontext;
      ZoneHeap     &heap    = *basecontext.heap;

      context.slopemark = zhcalloctag(heap, float *, w, sizeof(float), PU_VALLOC, nullptr);
   });
}

void R_ClearSlopeMark(float *const slopemark, int minx, int maxx, pwindowtype_e type)
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
// Performs initial clipping based on the opening of the portal. This can
// quickly reject segs that are all the way off the left or right edges
// of the portal window.
//
static bool R_clipInitialSegRange(const cb_seg_t &seg, const portalrender_t &portalrender,
                                  int *start, int *stop, float *clipx1, float *clipx2)
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

static void R_clipSegToFPortal(bspcontext_t &bspcontext, cmapcontext_t &cmapcontext,
                               planecontext_t &planecontext, portalcontext_t &portalcontext,
                               ZoneHeap &heap,
                               const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                               const contextbounds_t &bounds, const cb_seg_t &seg)
{
   float                *&slopemark    = bspcontext.slopemark;
   float           *const floorclip    = planecontext.floorclip;
   const portalrender_t  &portalrender = portalcontext.portalrender;

   int i, startx;
   float clipx1, clipx2;
   int start, stop;

   if(!R_clipInitialSegRange(seg, portalrender, &start, &stop, &clipx1, &clipx2))
      return;

   if(seg.plane.ceiling && seg.plane.ceiling->pslope)
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
         {
            R_clipSolidWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
         else
         {
            R_clipPassWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
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
         {
            R_clipSolidWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
         else
         {
            R_clipPassWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
      }
   }
}

static void R_clipSegToCPortal(bspcontext_t &bspcontext, cmapcontext_t &cmapcontext,
                               planecontext_t &planecontext, portalcontext_t &portalcontext,
                               ZoneHeap &heap,
                               const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                               const contextbounds_t &bounds, const cb_seg_t &seg)
{
   float                *&slopemark    = bspcontext.slopemark;
   float           *const ceilingclip  = planecontext.ceilingclip;
   const portalrender_t  &portalrender = portalcontext.portalrender;

   int i, startx;
   float clipx1, clipx2;
   int start, stop;

   if(!R_clipInitialSegRange(seg, portalrender, &start, &stop, &clipx1, &clipx2))
      return;

   if(seg.plane.floor && seg.plane.floor->pslope)
   {
      for(i = start; i <= stop; i++)
      {
         while(i <= stop && slopemark[i] < ceilingclip[i]) i++;

         if(i > stop)
            return;

         startx = i;

         while(i <= stop && slopemark[i] >= ceilingclip[i]) i++;

         if(seg.clipsolid)
         {
            R_clipSolidWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
         else
         {
            R_clipPassWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
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
         {
            R_clipSolidWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
         else
         {
            R_clipPassWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
      }
   }
}

static void R_clipSegToLPortal(bspcontext_t &bspcontext, cmapcontext_t &cmapcontext,
                               planecontext_t &planecontext, portalcontext_t &portalcontext,
                               ZoneHeap &heap,
                               const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                               const contextbounds_t &bounds, const cb_seg_t &seg)
{
   float          *const floorclip    = planecontext.floorclip;
   float          *const ceilingclip  = planecontext.ceilingclip;
   const portalrender_t &portalrender = portalcontext.portalrender;

   int i, startx;
   float clipx1, clipx2;
   int start, stop;

   // Line based portal. This requires special clipping...
   if(!R_clipInitialSegRange(seg, portalrender, &start, &stop, &clipx1, &clipx2))
      return;

   // This can actually happen with slopes!
   if(!seg.plane.floor && !seg.plane.ceiling && !seg.secwindow.floor && !seg.secwindow.ceiling)
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
         {
            R_clipSolidWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
         else
         {
            R_clipPassWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
      }
   }
   else if(!seg.plane.floor && !seg.secwindow.floor)
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
         {
            R_clipSolidWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
         else
         {
            R_clipPassWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
      }
   }
   else if(!seg.plane.ceiling && !seg.secwindow.ceiling)
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
         {
            R_clipSolidWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
         else
         {
            R_clipPassWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
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
         {
            R_clipSolidWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
         else
         {
            R_clipPassWallSegment(
               bspcontext, cmapcontext, planecontext, portalcontext, heap,
               viewpoint, cb_viewpoint, bounds, seg, startx, i - 1
            );
         }
      }
   }
}

//
// When a new seg obtains a sector portal window, make sure to update the render barrier accordingly
// Needs to be done each time a sector window is detected.
//
static void R_updateWindowSectorBarrier(portalcontext_t &context, const uint64_t visitid, cb_seg_t &seg, surf_e surf)
{
   sectorboxvisit_t  &boxvisitid = context.visitids[seg.line->frontsector - sectors];
   const sectorbox_t &box        = pSectorBoxes[seg.line->frontsector - sectors];
   if(seg.secwindow[surf] && boxvisitid[surf] != visitid)
   {
      boxvisitid[surf] = visitid;
      R_CalcRenderBarrier(*seg.secwindow[surf], box);
   }
}


R_ClipSegFunc segclipfuncs[] =
{
   R_clipSegToFPortal,
   R_clipSegToCPortal,
   R_clipSegToLPortal
};

#define NEARCLIP 0.05f

static void R_2S_Sloped(cmapcontext_t &cmapcontext, planecontext_t &planecontext,
                        portalcontext_t &portalcontext, ZoneHeap &heap,
                        const viewpoint_t &viewpoint,
                        const cbviewpoint_t &cb_viewpoint, const contextbounds_t &bounds,
                        const uint64_t visitid, cb_seg_t &seg,
                        float pstep, float i1, float i2, float textop, float texbottom,
                        const vertex_t *v1, const vertex_t *v2, float lclip1, float lclip2)
{
   const portalrender_t &portalrender = portalcontext.portalrender;

   bool mark, markblend; // haleyjd
   // ioanch: needed to prevent transfer_heights from affecting sky hacks.
   bool marktheight, blocktheight;
   bool heightchange;
   float texhigh, texlow;
   const side_t *side = seg.side;
   const seg_t  *line = seg.line;

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

   if(seg.backsec->srf.ceiling.slope)
   {
      float z1, z2, zstep;

      z1 = P_GetZAtf(seg.backsec->srf.ceiling.slope, v1->fx, v1->fy);
      z2 = P_GetZAtf(seg.backsec->srf.ceiling.slope, v2->fx, v2->fy);
      zstep = (z2 - z1) / seg.line->len;

      z1 += lclip1 * zstep;
      z2 -= (seg.line->len - lclip2) * zstep;

      seg.high = view.ycenter - ((z1 - cb_viewpoint.z) * i1) - 1.0f;
      seg.high2 = view.ycenter - ((z2 - cb_viewpoint.z) * i2) - 1.0f;

      seg.minbackceil = M_FloatToFixed(z1 < z2 ? z1 : z2);
   }
   else
   {
      seg.high = view.ycenter - ((seg.backsec->srf.ceiling.heightf - cb_viewpoint.z) * i1) - 1.0f;
      seg.high2 = view.ycenter - ((seg.backsec->srf.ceiling.heightf - cb_viewpoint.z) * i2) - 1.0f;
      seg.minbackceil = seg.backsec->srf.ceiling.height;
   }

   seg.highstep = (seg.high2 - seg.high) * pstep;

   // SoM: Get this from the actual sector because R_FakeFlat can mess with heights.
   texhigh = seg.line->backsector->srf.ceiling.heightf - cb_viewpoint.z;

   if(seg.backsec->srf.floor.slope)
   {
      float z1, z2, zstep;

      z1 = P_GetZAtf(seg.backsec->srf.floor.slope, v1->fx, v1->fy);
      z2 = P_GetZAtf(seg.backsec->srf.floor.slope, v2->fx, v2->fy);
      zstep = (z2 - z1) / seg.line->len;

      z1 += lclip1 * zstep;
      z2 -= (seg.line->len - lclip2) * zstep;
      seg.low = view.ycenter - ((z1 - cb_viewpoint.z) * i1);
      seg.low2 = view.ycenter - ((z2 - cb_viewpoint.z) * i2);

      seg.maxbackfloor = M_FloatToFixed(z1 > z2 ? z1 : z2);
   }
   else
   {
      seg.low = view.ycenter - ((seg.backsec->srf.floor.heightf - cb_viewpoint.z) * i1);
      seg.low2 = view.ycenter - ((seg.backsec->srf.floor.heightf - cb_viewpoint.z) * i2);
      seg.maxbackfloor = seg.backsec->srf.floor.height;
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
   heightchange = seg.frontsec->srf.ceiling.slope || seg.backsec->srf.ceiling.slope ?
                  (t != h || t2 != h2) :
                  (seg.backsec->srf.ceiling.height != seg.frontsec->srf.ceiling.height);

   seg.markflags = 0;
   
   markblend = seg.frontsec->srf.ceiling.portal != nullptr
         && seg.backsec->srf.ceiling.portal != nullptr
         && (seg.frontsec->srf.ceiling.pflags & PS_BLENDFLAGS) != (seg.backsec->srf.ceiling.pflags & PS_BLENDFLAGS);

   if(seg.portal.ceiling)
   {
      if(seg.clipsolid || heightchange ||
         seg.frontsec->srf.ceiling.portal != seg.backsec->srf.ceiling.portal)
      {
         seg.markflags |= SEG_MARKCPORTAL;
         seg.secwindow.ceiling = R_GetSectorPortalWindow(
            planecontext, portalcontext, heap, viewpoint, bounds, surf_ceil, seg.frontsec->srf.ceiling
         );
         R_updateWindowSectorBarrier(portalcontext, visitid, seg, surf_ceil);
         R_MovePortalOverlayToWindow(cmapcontext, planecontext, heap, viewpoint, cb_viewpoint, bounds, seg, surf_ceil);
      }
      else if(!heightchange && seg.frontsec->srf.ceiling.portal == seg.backsec->srf.ceiling.portal)
      {
         seg.secwindow.ceiling = R_GetSectorPortalWindow(
            planecontext, portalcontext, heap, viewpoint, bounds, surf_ceil, seg.frontsec->srf.ceiling
         );
         R_updateWindowSectorBarrier(portalcontext, visitid, seg, surf_ceil);
         R_MovePortalOverlayToWindow(cmapcontext, planecontext, heap, viewpoint, cb_viewpoint, bounds, seg, surf_ceil);
         seg.secwindow.ceiling = nullptr;
      }
      else
         seg.secwindow.ceiling = nullptr;
   }
   else
      seg.secwindow.ceiling = nullptr;

   if(seg.plane.ceiling &&
       (mark || (marktheight && !blocktheight) || seg.clipsolid || heightchange ||
        seg.frontsec->srf.ceiling.offset != seg.backsec->srf.ceiling.offset ||
        seg.frontsec->srf.ceiling.scale != seg.backsec->srf.ceiling.scale ||
        (seg.frontsec->srf.ceiling.baseangle + seg.frontsec->srf.ceiling.angle !=
         seg.backsec->srf.ceiling.baseangle + seg.backsec->srf.ceiling.angle) || // haleyjd: angles
        seg.frontsec->srf.ceiling.pic != seg.backsec->srf.ceiling.pic ||
        seg.frontsec->srf.ceiling.lightsec != seg.backsec->srf.ceiling.lightsec ||
        seg.frontsec->srf.ceiling.lightdelta != seg.backsec->srf.ceiling.lightdelta ||
        (seg.frontsec->flags & SECF_CEILLIGHTABSOLUTE) !=
         (seg.backsec->flags & SECF_CEILLIGHTABSOLUTE) ||
        seg.frontsec->topmap != seg.backsec->topmap ||
        seg.frontsec->srf.ceiling.portal != seg.backsec->srf.ceiling.portal ||
        !R_CompareSlopes(seg.frontsec->srf.ceiling.slope, seg.backsec->srf.ceiling.slope) || markblend)) // haleyjd
   {
      seg.markflags |= seg.portal.ceiling ? SEG_MARKCOVERLAY : SEG_MARKCEILING;
   }

   bool havetportal = seg.backsec && seg.backsec->srf.ceiling.portal &&
         seg.line->linedef->extflags & EX_ML_UPPERPORTAL;

   bool toohigh = havetportal && portalrender.w &&
   portalrender.w->type == pw_floor && portalrender.w->portal->type != R_SKYBOX &&
   portalrender.w->planez + viewpoint.z - portalrender.w->vz <= seg.backsec->srf.ceiling.height;

   if(!toohigh && !havetportal && heightchange && 
      !(seg.frontsec->intflags & SIF_SKY && seg.backsec->intflags & SIF_SKY) && 
      side->toptexture)
   {
      seg.toptex = texturetranslation[side->toptexture];
      seg.toptexh = textures[side->toptexture]->height;

      if(seg.line->linedef->flags & ML_DONTPEGTOP)
         seg.toptexmid = M_FloatToFixed(textop + seg.toffsety); // SCALE_TODO: Y scale-factor here
      else
         seg.toptexmid = M_FloatToFixed(texhigh + seg.toptexh + seg.toffsety); // SCALE_TODO: Y scale-factor here
   }
   else
      seg.toptex = 0;

   if(!toohigh && havetportal && heightchange)
   {
      seg.t_window = R_GetLinePortalWindow(
         planecontext, portalcontext, heap, viewpoint, bounds, seg.backsec->srf.ceiling.portal, line
      );
      seg.segtextured = true;
   }
   else
      seg.t_window = nullptr;

   // -- Floors --
   // SoM: TODO: Float comparisons should be done within an epsilon
   heightchange = seg.frontsec->srf.floor.slope || seg.backsec->srf.floor.slope ? (l != b || l2 != b2) :
                  seg.backsec->srf.floor.height != seg.frontsec->srf.floor.height;

   markblend = seg.frontsec->srf.floor.portal != nullptr
         && seg.backsec->srf.floor.portal != nullptr
         && (seg.frontsec->srf.floor.pflags & PS_BLENDFLAGS) != (seg.backsec->srf.floor.pflags & PS_BLENDFLAGS);

   if(seg.portal.floor)
   {
      if(seg.clipsolid || heightchange ||
         seg.frontsec->srf.floor.portal != seg.backsec->srf.floor.portal)
      {
         seg.markflags |= SEG_MARKFPORTAL;
         seg.secwindow.floor = R_GetSectorPortalWindow(
            planecontext, portalcontext, heap, viewpoint, bounds, surf_floor, seg.frontsec->srf.floor
         );
         R_updateWindowSectorBarrier(portalcontext, visitid, seg, surf_floor);
         R_MovePortalOverlayToWindow(cmapcontext, planecontext, heap, viewpoint, cb_viewpoint, bounds, seg, surf_floor);
      }
      else if(!heightchange && seg.frontsec->srf.floor.portal == seg.backsec->srf.floor.portal)
      {
         seg.secwindow.floor = R_GetSectorPortalWindow(
            planecontext, portalcontext, heap, viewpoint, bounds, surf_floor, seg.frontsec->srf.floor
         );
         R_updateWindowSectorBarrier(portalcontext, visitid, seg, surf_floor);
         R_MovePortalOverlayToWindow(cmapcontext, planecontext, heap, viewpoint, cb_viewpoint, bounds, seg, surf_floor);
         seg.secwindow.floor = nullptr;
      }
      else
         seg.secwindow.floor = nullptr;
   }
   else
      seg.secwindow.floor = nullptr;

   if(seg.plane.floor &&
      (mark || marktheight || seg.clipsolid || heightchange ||
       seg.frontsec->srf.floor.offset != seg.backsec->srf.floor.offset ||
       seg.frontsec->srf.floor.scale != seg.backsec->srf.floor.scale ||
       (seg.frontsec->srf.floor.baseangle + seg.frontsec->srf.floor.angle !=
        seg.backsec->srf.floor.baseangle + seg.backsec->srf.floor.angle) || // haleyjd: angles
       seg.frontsec->srf.floor.pic != seg.backsec->srf.floor.pic ||
       seg.frontsec->srf.floor.lightsec != seg.backsec->srf.floor.lightsec ||
       seg.frontsec->srf.floor.lightdelta != seg.backsec->srf.floor.lightdelta ||
       (seg.frontsec->flags & SECF_FLOORLIGHTABSOLUTE) !=
        (seg.backsec->flags & SECF_FLOORLIGHTABSOLUTE) ||
       seg.frontsec->bottommap != seg.backsec->bottommap ||
       seg.frontsec->srf.floor.portal != seg.backsec->srf.floor.portal ||
       !R_CompareSlopes(seg.frontsec->srf.floor.slope, seg.backsec->srf.floor.slope) || markblend)) // haleyjd
   {
      seg.markflags |= seg.portal.floor ? SEG_MARKFOVERLAY : SEG_MARKFLOOR;
   }

   // SoM: some portal types should be rendered even if the player is above
   // or below the ceiling or floor plane.
   // haleyjd 03/12/06: inverted predicates to simplify
   if(seg.backsec->srf.floor.portal != seg.frontsec->srf.floor.portal)
   {
      if(seg.frontsec->srf.floor.portal &&
         seg.frontsec->srf.floor.portal->type != R_LINKED &&
         seg.frontsec->srf.floor.portal->type != R_TWOWAY)
         seg.f_portalignore = true;
   }

   if(seg.backsec->srf.ceiling.portal != seg.frontsec->srf.ceiling.portal)
   {
      if(seg.frontsec->srf.ceiling.portal &&
         seg.frontsec->srf.ceiling.portal->type != R_LINKED &&
         seg.frontsec->srf.ceiling.portal->type != R_TWOWAY)
         seg.c_portalignore = true;
   }

   bool havebportal = seg.backsec && seg.backsec->srf.floor.portal &&
         seg.line->linedef->extflags & EX_ML_LOWERPORTAL;
   bool toolow = havebportal && portalrender.w &&
   portalrender.w->type == pw_ceiling && portalrender.w->portal->type != R_SKYBOX &&
   portalrender.w->planez + viewpoint.z - portalrender.w->vz >= seg.backsec->srf.floor.height;

   // SoM: Get this from the actual sector because R_FakeFlat can mess with heights.

   texlow = seg.line->backsector->srf.floor.heightf - cb_viewpoint.z;
   if(!toolow && !havebportal && (b > l || b2 > l2) && side->bottomtexture)
   {
      seg.bottomtex = texturetranslation[side->bottomtexture];
      seg.bottomtexh = textures[side->bottomtexture]->height;

      if(seg.line->linedef->flags & ML_DONTPEGBOTTOM)
         seg.bottomtexmid = M_FloatToFixed(textop + seg.toffsety); // SCALE_TODO: Y scale-factor here
      else
         seg.bottomtexmid = M_FloatToFixed(texlow + seg.toffsety); // SCALE_TODO: Y scale-factor here
   }
   else
      seg.bottomtex = 0;

   seg.midtex = 0;
   seg.maskedtex = !!seg.side->midtexture;
   seg.segtextured = (seg.maskedtex || seg.bottomtex || seg.toptex || seg.t_window);

   if(line->linedef->portal && //line->linedef->sidenum[0] != line->linedef->sidenum[1] &&
      line->linedef->sidenum[0] == line->sidedef - sides)
   {
      seg.l_window = R_GetLinePortalWindow(
         planecontext, portalcontext, heap, viewpoint, bounds, line->linedef->portal, line
      );
      seg.clipsolid = true;
   }
   else
      seg.l_window = nullptr;
   if(R_IsSkyFlat(seg.side->midtexture))
   {
      seg.skyflat = seg.side->sector->sky & PL_SKYFLAT ? seg.side->sector->sky : seg.side->midtexture;
      seg.maskedtex = false;
   }
   else
      seg.skyflat = 0;

   if(!toolow && havebportal && (b > l || b2 > l2))
   {
      seg.b_window = R_GetLinePortalWindow(
         planecontext, portalcontext, heap, viewpoint, bounds, seg.backsec->srf.floor.portal, line
      );
      seg.segtextured = true;
   }
   else
      seg.b_window = nullptr;
}

static void R_2S_Normal(cmapcontext_t &cmapcontext, planecontext_t &planecontext,
                        portalcontext_t &portalcontext, ZoneHeap &heap,
                        const viewpoint_t &viewpoint,
                        const cbviewpoint_t &cb_viewpoint, const contextbounds_t &bounds,
                        const uint64_t visitid, cb_seg_t &seg, float pstep,
                        float i1, float i2, float textop, float texbottom)
{
   const portalrender_t &portalrender = portalcontext.portalrender;

   bool mark, markblend; // haleyjd
   // ioanch: needed to prevent transfer_heights from affecting sky hacks.
   bool marktheight, blocktheight;
   bool uppermissing, lowermissing;
   float texhigh, texlow;
   const side_t *side = seg.side;
   const seg_t  *line = seg.line;
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

   frontc = seg.frontsec->srf.ceiling.height;
   backc  = seg.backsec->srf.ceiling.height;

   seg.high  = view.ycenter - ((seg.backsec->srf.ceiling.heightf - cb_viewpoint.z) * i1) - 1.0f;
   seg.high2 = view.ycenter - ((seg.backsec->srf.ceiling.heightf - cb_viewpoint.z) * i2) - 1.0f;
   seg.highstep = (seg.high2 - seg.high) * pstep;

   seg.minbackceil = backc;

   // SoM: Get this from the actual sector because R_FakeFlat can mess with heights.
   texhigh = seg.line->backsector->srf.ceiling.heightf - cb_viewpoint.z;

   uppermissing = (seg.frontsec->srf.ceiling.height > seg.backsec->srf.ceiling.height &&
                   seg.side->toptexture == 0);

   lowermissing = (seg.frontsec->srf.floor.height < seg.backsec->srf.floor.height &&
                   seg.side->bottomtexture == 0);

   bool portaltouch = portalrender.active &&
   ((portalrender.w->type == pw_floor && (portalrender.w->planez ==
                                          seg.backsec->srf.floor.height ||
                                          portalrender.w->planez ==
                                          seg.frontsec->srf.floor.height)) ||
    (portalrender.w->type == pw_ceiling && (portalrender.w->planez ==
                                            seg.backsec->srf.ceiling.height ||
                                            portalrender.w->planez ==
                                            seg.frontsec->srf.ceiling.height)));

   // New clipsolid code will emulate the old doom behavior and still manages to
   // keep valid closed door cases handled.
   seg.clipsolid = !portaltouch && ((seg.backsec->srf.floor.height !=
                                     seg.frontsec->srf.floor.height ||
       seg.backsec->srf.ceiling.height != seg.frontsec->srf.ceiling.height) &&
       (seg.backsec->srf.floor.height >= seg.frontsec->srf.ceiling.height ||
        seg.backsec->srf.ceiling.height <= seg.frontsec->srf.floor.height ||
        (seg.backsec->srf.ceiling.height <= seg.backsec->srf.floor.height &&
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
   
   markblend = seg.frontsec->srf.ceiling.portal != nullptr
            && seg.backsec->srf.ceiling.portal != nullptr
            && (seg.frontsec->srf.ceiling.pflags & PS_BLENDFLAGS) != (seg.backsec->srf.ceiling.pflags & PS_BLENDFLAGS);
               
   if(mark || (marktheight && !blocktheight) || seg.clipsolid || frontc != backc || 
      seg.frontsec->srf.ceiling.offset != seg.backsec->srf.ceiling.offset ||
      seg.frontsec->srf.ceiling.scale != seg.backsec->srf.ceiling.scale ||
      (seg.frontsec->srf.ceiling.baseangle + seg.frontsec->srf.ceiling.angle !=
       seg.backsec->srf.ceiling.baseangle + seg.backsec->srf.ceiling.angle) || // haleyjd: angles
      seg.frontsec->srf.ceiling.pic != seg.backsec->srf.ceiling.pic ||
      seg.frontsec->srf.ceiling.lightsec != seg.backsec->srf.ceiling.lightsec ||
      seg.frontsec->srf.ceiling.lightdelta != seg.backsec->srf.ceiling.lightdelta ||
      (seg.frontsec->flags & SECF_CEILLIGHTABSOLUTE) !=
       (seg.backsec->flags & SECF_CEILLIGHTABSOLUTE) ||
      seg.frontsec->topmap != seg.backsec->topmap ||
      seg.frontsec->srf.ceiling.portal != seg.backsec->srf.ceiling.portal || markblend) // haleyjd
   {
      seg.markflags |= seg.portal.ceiling ? SEG_MARKCOVERLAY :
                    seg.plane.ceiling ? SEG_MARKCEILING : 0;
   }

   if(seg.portal.ceiling)
   {
      if(seg.clipsolid ||
         seg.frontsec->srf.ceiling.height != seg.backsec->srf.ceiling.height ||
         seg.frontsec->srf.ceiling.portal != seg.backsec->srf.ceiling.portal)
      {
         seg.markflags |= SEG_MARKCPORTAL;
         seg.secwindow.ceiling = R_GetSectorPortalWindow(
            planecontext, portalcontext, heap, viewpoint, bounds, surf_ceil, seg.frontsec->srf.ceiling
         );
         R_updateWindowSectorBarrier(portalcontext, visitid, seg, surf_ceil);
         R_MovePortalOverlayToWindow(cmapcontext, planecontext, heap, viewpoint, cb_viewpoint, bounds, seg, surf_ceil);
      }
      else if(seg.frontsec->srf.ceiling.portal == seg.backsec->srf.ceiling.portal &&
              seg.frontsec->srf.ceiling.height == seg.backsec->srf.ceiling.height)
      {
         // We need to do this just to transfer the plane
         seg.secwindow.ceiling = R_GetSectorPortalWindow(
            planecontext, portalcontext, heap, viewpoint, bounds, surf_ceil, seg.frontsec->srf.ceiling
         );
         R_updateWindowSectorBarrier(portalcontext, visitid, seg, surf_ceil);
         R_MovePortalOverlayToWindow(cmapcontext, planecontext, heap, viewpoint, cb_viewpoint, bounds, seg, surf_ceil);
         seg.secwindow.ceiling = nullptr;
      }
      else
         seg.secwindow.ceiling = nullptr;
   }
   else
      seg.secwindow.ceiling = nullptr;

   bool havetportal = seg.backsec && seg.backsec->srf.ceiling.portal &&
         seg.line->linedef->extflags & EX_ML_UPPERPORTAL;
   bool toohigh = havetportal && portalrender.w &&
   portalrender.w->type == pw_floor && portalrender.w->portal->type != R_SKYBOX &&
   portalrender.w->planez + viewpoint.z - portalrender.w->vz <= seg.backsec->srf.ceiling.height;

   if(!toohigh && !havetportal &&
      seg.frontsec->srf.ceiling.height > seg.backsec->srf.ceiling.height &&
     !(seg.frontsec->intflags & SIF_SKY && seg.backsec->intflags & SIF_SKY) &&
      side->toptexture)
   {
      seg.toptex = texturetranslation[side->toptexture];
      seg.toptexh = textures[side->toptexture]->height;

      if(seg.line->linedef->flags & ML_DONTPEGTOP)
         seg.toptexmid = M_FloatToFixed(textop + seg.toffsety); // SCALE_TODO: Y scale-factor here
      else
         seg.toptexmid = M_FloatToFixed(texhigh + seg.toptexh + seg.toffsety); // SCALE_TODO: Y scale-factor here
   }
   else
      seg.toptex = 0;

   if(!toohigh && havetportal &&
      seg.frontsec->srf.ceiling.height > seg.backsec->srf.ceiling.height)
   {
      seg.t_window = R_GetLinePortalWindow(
         planecontext, portalcontext, heap, viewpoint, bounds, seg.backsec->srf.ceiling.portal, line
      );
      seg.segtextured = true;
   }
   else
      seg.t_window = nullptr;

   markblend = seg.frontsec->srf.floor.portal != nullptr
            && seg.backsec->srf.floor.portal != nullptr
            && (seg.frontsec->srf.floor.pflags & PS_BLENDFLAGS) != (seg.backsec->srf.floor.pflags & PS_BLENDFLAGS);

   if(mark || marktheight || seg.clipsolid ||
      seg.frontsec->srf.floor.height != seg.backsec->srf.floor.height ||
      seg.frontsec->srf.floor.offset != seg.backsec->srf.floor.offset ||
      seg.frontsec->srf.floor.scale != seg.backsec->srf.floor.scale ||
      (seg.frontsec->srf.floor.baseangle + seg.frontsec->srf.floor.angle !=
       seg.backsec->srf.floor.baseangle + seg.backsec->srf.floor.angle) || // haleyjd
      seg.frontsec->srf.floor.pic != seg.backsec->srf.floor.pic ||
      seg.frontsec->srf.floor.lightsec != seg.backsec->srf.floor.lightsec ||
      seg.frontsec->srf.floor.lightdelta != seg.backsec->srf.floor.lightdelta ||
      (seg.frontsec->flags & SECF_FLOORLIGHTABSOLUTE) !=
       (seg.backsec->flags & SECF_FLOORLIGHTABSOLUTE) ||
      seg.frontsec->bottommap != seg.backsec->bottommap ||
      seg.frontsec->srf.floor.portal != seg.backsec->srf.floor.portal ||
      markblend) // haleyjd
   {
      seg.markflags |= seg.portal.floor ? SEG_MARKFOVERLAY :
                       seg.plane.floor ? SEG_MARKFLOOR : 0;
   }

   if(seg.portal.floor)
   {
      if(seg.clipsolid ||
         seg.frontsec->srf.floor.height != seg.backsec->srf.floor.height ||
         seg.frontsec->srf.floor.portal != seg.backsec->srf.floor.portal)
      {
         seg.markflags |= SEG_MARKFPORTAL;
         seg.secwindow.floor = R_GetSectorPortalWindow(
            planecontext, portalcontext, heap, viewpoint, bounds, surf_floor, seg.frontsec->srf.floor
         );
         R_updateWindowSectorBarrier(portalcontext, visitid, seg, surf_floor);
         R_MovePortalOverlayToWindow(cmapcontext, planecontext, heap, viewpoint, cb_viewpoint, bounds, seg, surf_floor);
      }
      else if(seg.frontsec->srf.floor.height == seg.backsec->srf.floor.height &&
              seg.frontsec->srf.floor.portal == seg.backsec->srf.floor.portal)
      {
         // We need to do this just to transfer the plane
         seg.secwindow.floor = R_GetSectorPortalWindow(
            planecontext, portalcontext, heap, viewpoint, bounds, surf_floor, seg.frontsec->srf.floor
         );
         R_updateWindowSectorBarrier(portalcontext, visitid, seg, surf_floor);
         R_MovePortalOverlayToWindow(cmapcontext, planecontext, heap, viewpoint, cb_viewpoint, bounds, seg, surf_floor);
         seg.secwindow.floor = nullptr;
      }
      else
         seg.secwindow.floor = nullptr;
   }
   else
      seg.secwindow.floor = nullptr;

   // SoM: some portal types should be rendered even if the player is above
   // or below the ceiling or floor plane.
   // haleyjd 03/12/06: inverted predicates to simplify
   if(seg.backsec->srf.floor.portal != seg.frontsec->srf.floor.portal)
   {
      if(seg.frontsec->srf.floor.portal &&
         seg.frontsec->srf.floor.portal->type != R_LINKED &&
         seg.frontsec->srf.floor.portal->type != R_TWOWAY)
         seg.f_portalignore = true;
   }

   if(seg.backsec->srf.ceiling.portal != seg.frontsec->srf.ceiling.portal)
   {
      if(seg.frontsec->srf.ceiling.portal &&
         seg.frontsec->srf.ceiling.portal->type != R_LINKED &&
         seg.frontsec->srf.ceiling.portal->type != R_TWOWAY)
         seg.c_portalignore = true;
   }

   seg.low  = view.ycenter - ((seg.backsec->srf.floor.heightf - cb_viewpoint.z) * i1);
   seg.low2 = view.ycenter - ((seg.backsec->srf.floor.heightf - cb_viewpoint.z) * i2);
   seg.lowstep = (seg.low2 - seg.low) * pstep;
   seg.maxbackfloor = seg.backsec->srf.floor.height;

   // ioanch: don't render lower textures or portals if they're below the
   // current plane-z window. Necessary for edge portals
   bool havebportal = seg.backsec && seg.backsec->srf.floor.portal &&
         seg.line->linedef->extflags & EX_ML_LOWERPORTAL;
   bool toolow = havebportal && portalrender.w &&
   portalrender.w->type == pw_ceiling && portalrender.w->portal->type != R_SKYBOX &&
   portalrender.w->planez + viewpoint.z - portalrender.w->vz >= seg.backsec->srf.floor.height;

   // SoM: Get this from the actual sector because R_FakeFlat can mess with heights.

   texlow = seg.line->backsector->srf.floor.heightf - cb_viewpoint.z;
   if(!toolow && !havebportal
      && seg.frontsec->srf.floor.height < seg.backsec->srf.floor.height
      && side->bottomtexture)
   {
      seg.bottomtex  = texturetranslation[side->bottomtexture];
      seg.bottomtexh = textures[side->bottomtexture]->height;

      if(seg.line->linedef->flags & ML_DONTPEGBOTTOM)
         seg.bottomtexmid = M_FloatToFixed(textop + seg.toffsety); // SCALE_TODO: Y scale-factor here
      else
         seg.bottomtexmid = M_FloatToFixed(texlow + seg.toffsety); // SCALE_TODO: Y scale-factor here
   }
   else
      seg.bottomtex = 0;

   seg.midtex = 0;
   seg.maskedtex = !!seg.side->midtexture;
   seg.segtextured = (seg.maskedtex || seg.bottomtex || seg.toptex || seg.t_window);

   if(line->linedef->portal && //line->linedef->sidenum[0] != line->linedef->sidenum[1] &&
      line->linedef->sidenum[0] == line->sidedef - sides)
   {
      seg.l_window = R_GetLinePortalWindow(
         planecontext, portalcontext, heap, viewpoint, bounds, line->linedef->portal, line
      );
      seg.clipsolid = true;
   }
   else
      seg.l_window = nullptr;
   if(R_IsSkyFlat(seg.side->midtexture))
   {
      seg.skyflat = seg.side->sector->sky & PL_SKYFLAT ? seg.side->sector->sky : seg.side->midtexture;
      seg.maskedtex = false;
   }
   else
      seg.skyflat = 0;

   if(!toolow && havebportal &&
      seg.frontsec->srf.floor.height < seg.backsec->srf.floor.height)
   {
      seg.b_window = R_GetLinePortalWindow(
         planecontext, portalcontext, heap, viewpoint, bounds, seg.backsec->srf.floor.portal, line
      );
      seg.segtextured = true;
   }
   else
      seg.b_window = nullptr;
}

//
// Prepare 1-sided line for rendering (extracted from R_addLine due to size)
// beyond is the optional sector on the other side of a polyobject/1-sided wall portal
//
static void R_1SidedLine(cmapcontext_t &cmapcontext, planecontext_t &planecontext,
                         portalcontext_t &portalcontext, ZoneHeap &heap,
                         const viewpoint_t &viewpoint,
                         const cbviewpoint_t &cb_viewpoint, const contextbounds_t &bounds,
                         const uint64_t visitid, cb_seg_t &seg,
                         float pstep, float i1, float i2, float textop, float texbottom,
                         const sector_t *beyond, const side_t *side, const seg_t *line)
{
   seg.twosided = false;
   if(!beyond)
      seg.toptex = seg.bottomtex = 0;
   else
   {
      // ioanch FIXME: copy-paste from R_2S_Normal
      if(seg.frontsec->srf.ceiling.height > beyond->srf.ceiling.height &&
         !(seg.frontsec->intflags & SIF_SKY && beyond->intflags & SIF_SKY) &&
         side->toptexture)
      {
         seg.toptex = texturetranslation[side->toptexture];
         seg.toptexh = textures[side->toptexture]->height;

         float texhigh = beyond->srf.ceiling.heightf - cb_viewpoint.z;

         if(seg.line->linedef->flags & ML_DONTPEGTOP)
            seg.toptexmid = M_FloatToFixed(textop + seg.toffsety); // SCALE_TODO: Y scale-factor here
         else
            seg.toptexmid = M_FloatToFixed(texhigh + seg.toptexh + seg.toffsety); // SCALE_TODO: Y scale-factor here

         seg.high  = view.ycenter - ((beyond->srf.ceiling.heightf - cb_viewpoint.z) * i1) - 1.0f;
         seg.high2 = view.ycenter - ((beyond->srf.ceiling.heightf - cb_viewpoint.z) * i2) - 1.0f;
         seg.highstep = (seg.high2 - seg.high) * pstep;
      }
      else
         seg.toptex = 0;

      if(seg.frontsec->srf.floor.height < beyond->srf.floor.height && side->bottomtexture)
      {
         seg.bottomtex  = texturetranslation[side->bottomtexture];
         seg.bottomtexh = textures[side->bottomtexture]->height;

         float texlow = beyond->srf.floor.heightf - cb_viewpoint.z;

         if(seg.line->linedef->flags & ML_DONTPEGBOTTOM)
            seg.bottomtexmid = M_FloatToFixed(textop + seg.toffsety); // SCALE_TODO: Y scale-factor here
         else
            seg.bottomtexmid = M_FloatToFixed(texlow + seg.toffsety); // SCALE_TODO: Y scale-factor here

         seg.low  = view.ycenter - ((beyond->srf.floor.heightf - cb_viewpoint.z) * i1);
         seg.low2 = view.ycenter - ((beyond->srf.floor.heightf - cb_viewpoint.z) * i2);
         seg.lowstep = (seg.low2 - seg.low) * pstep;
      }
      else
         seg.bottomtex = 0;

   }

   bool sky = R_IsSkyFlat(side->midtexture);
   if(!sky)
   {
      seg.midtex   = texturetranslation[side->midtexture];
      seg.midtexh  = textures[side->midtexture]->height;

      if(seg.line->linedef->flags & ML_DONTPEGBOTTOM)
         seg.midtexmid = M_FloatToFixed(texbottom + seg.midtexh + seg.toffsety); // SCALE_TODO: Y scale-factor here
      else
         seg.midtexmid = M_FloatToFixed(textop + seg.toffsety); // SCALE_TODO: Y scale-factor here
      seg.skyflat = 0;
   }
   else
   {
      seg.midtex = 0;
      seg.skyflat = side->sector->sky & PL_SKYFLAT ? side->sector->sky : side->midtexture;
   }

   seg.markflags = beyond ? SEG_MARK1SLPORTAL : 0;
   seg.secwindow.ceiling = seg.secwindow.floor = nullptr;

   // SoM: these should be treated differently!
   if(seg.frontsec->srf.ceiling.portal && (seg.frontsec->srf.ceiling.portal->type < R_TWOWAY ||
                                 (seg.frontsec->srf.ceiling.pflags & PS_VISIBLE && seg.frontsec->srf.ceiling.height > viewpoint.z)))
   {
      seg.markflags |= SEG_MARKCPORTAL;
      seg.secwindow.ceiling = R_GetSectorPortalWindow(
         planecontext, portalcontext, heap, viewpoint, bounds, surf_ceil, seg.frontsec->srf.ceiling
      );
      R_updateWindowSectorBarrier(portalcontext, visitid, seg, surf_ceil);
      R_MovePortalOverlayToWindow(cmapcontext, planecontext, heap, viewpoint, cb_viewpoint, bounds, seg, surf_ceil);
   }

   if(seg.frontsec->srf.floor.portal && (seg.frontsec->srf.floor.portal->type < R_TWOWAY ||
                                 (seg.frontsec->srf.floor.pflags & PS_VISIBLE && seg.frontsec->srf.floor.height <= viewpoint.z)))
   {
      seg.markflags |= SEG_MARKFPORTAL;
      seg.secwindow.floor = R_GetSectorPortalWindow(
         planecontext, portalcontext, heap, viewpoint, bounds, surf_floor, seg.frontsec->srf.floor
      );
      R_updateWindowSectorBarrier(portalcontext, visitid, seg, surf_floor);
      R_MovePortalOverlayToWindow(cmapcontext, planecontext, heap, viewpoint, cb_viewpoint, bounds, seg, surf_floor);
   }

   if(seg.plane.ceiling != nullptr)
      seg.markflags |= seg.frontsec->srf.ceiling.portal ? SEG_MARKCOVERLAY : SEG_MARKCEILING;
   if(seg.plane.floor != nullptr)
      seg.markflags |= seg.frontsec->srf.floor.portal ? SEG_MARKFOVERLAY : SEG_MARKFLOOR;

   seg.clipsolid   = true;
   seg.segtextured = seg.midtex || seg.toptex || seg.bottomtex;
   seg.l_window    = line->linedef->portal ?
   R_GetLinePortalWindow(planecontext, portalcontext, heap, viewpoint, bounds, line->linedef->portal, line) : nullptr;

   // haleyjd 03/12/06: inverted predicates to simplify
   if(seg.frontsec->srf.floor.portal && seg.frontsec->srf.floor.portal->type != R_LINKED &&
      seg.frontsec->srf.floor.portal->type != R_TWOWAY)
      seg.f_portalignore = true;
   if(seg.frontsec->srf.ceiling.portal && seg.frontsec->srf.ceiling.portal->type != R_LINKED &&
      seg.frontsec->srf.ceiling.portal->type != R_TWOWAY)
      seg.c_portalignore = true;
}

inline static bool tooclose(v2float_t v1, v2float_t v2)
{
   return fabsf(v1.x - v2.x) < 1 / 256.0f && fabsf(v1.y - v2.y) < 1 / 256.0f;
}

//
// Checks if a line is behind a line portal generated barrier
//
static bool R_allowBehindBarrier(const windowlinegen_t &linegen, const seg_t *renderSeg,
   bool reverse = false)
{
   v2float_t rstart;
   v2float_t rdelta;
   if(!reverse)
   {
      rstart = { renderSeg->v1->fx, renderSeg->v1->fy };
      rdelta = { renderSeg->v2->fx - rstart.x, renderSeg->v2->fy - rstart.y };
   }
   else  // this may be needed sometimes
   {
      rstart = { renderSeg->v2->fx, renderSeg->v2->fy };
      rdelta = { renderSeg->v1->fx - rstart.x, renderSeg->v1->fy - rstart.y };
   }
   // HACK: pull render-seg to me as a slack to avoid on-line points
   rstart += linegen.normal * kPortalSegRejectionFudge;

   float s1 = linegen.normal * (rstart - linegen.start);
   float s2 = linegen.normal * (rstart + rdelta - linegen.start);
   if(s1 * s2 >= 0)  // both on same side
      return s1 < 0;   // only accept if behind the barrier line

   // Check cases where vertices are common
   // the other portal tip to the right of seg
   if(tooclose(linegen.start, rstart)) 
      return rdelta % (linegen.start + linegen.delta - rstart) < 0;
   if(tooclose(linegen.start + linegen.delta, rstart + rdelta))
      return rdelta % (linegen.start - rstart) < 0;

   // At least one point must be in front of the rendered line
   return rdelta % (linegen.start - rstart) < 0 || 
      rdelta % (linegen.start + linegen.delta - rstart) < 0;
}

//
// Picks the two bounding box lines pointed towards the viewer.
//
bool R_PickNearestBoxLines(const cbviewpoint_t &cb_viewpoint,
                           const float fbox[4], windowlinegen_t &linegen1,
                           windowlinegen_t &linegen2, slopetype_t *slope)
{
   linegen2.normal = {};   // mark normal as empty to prevent stuff
   if(cb_viewpoint.x < fbox[BOXLEFT])
   {
      if(cb_viewpoint.y < fbox[BOXBOTTOM])
      {
         linegen1.start = { fbox[BOXLEFT], fbox[BOXTOP] };
         linegen1.delta = { 0, fbox[BOXBOTTOM] - fbox[BOXTOP] };
         linegen1.normal = { -1, 0 };

         linegen2.start = { fbox[BOXLEFT], fbox[BOXBOTTOM] };
         linegen2.delta = { fbox[BOXRIGHT] - fbox[BOXLEFT], 0 };
         linegen2.normal = { 0, -1 };

         if(slope)
            *slope = ST_POSITIVE;
      }
      else if(cb_viewpoint.y > fbox[BOXTOP])
      {
         // The divlines MUST be left to right relative to view.
         linegen1.start = { fbox[BOXRIGHT], fbox[BOXTOP] };
         linegen1.delta = { fbox[BOXLEFT] - fbox[BOXRIGHT], 0 };
         linegen1.normal = { 0, 1 };

         linegen2.start = { fbox[BOXLEFT], fbox[BOXTOP] };
         linegen2.delta = { 0, fbox[BOXBOTTOM] - fbox[BOXTOP] };
         linegen2.normal = { -1, 0 };

         if(slope)
            *slope = ST_NEGATIVE;
      }
      else
      {
         linegen1.start = { fbox[BOXLEFT], fbox[BOXTOP] };
         linegen1.delta = { 0, fbox[BOXBOTTOM] - fbox[BOXTOP] };
         linegen1.normal = { -1, 0 };

         if(slope)
            *slope = ST_VERTICAL;
      }
   }
   else if(cb_viewpoint.x <= fbox[BOXRIGHT])
   {
      if(cb_viewpoint.y < fbox[BOXBOTTOM])
      {
         linegen1.start = { fbox[BOXLEFT], fbox[BOXBOTTOM] };
         linegen1.delta = { fbox[BOXRIGHT] - fbox[BOXLEFT], 0 };
         linegen1.normal = { 0, -1 };
      }
      else if(cb_viewpoint.y <= fbox[BOXTOP])
         return false;   // if actor is below portal, just render everything
      else
      {
         linegen1.start = { fbox[BOXRIGHT], fbox[BOXTOP] };
         linegen1.delta = { fbox[BOXLEFT] - fbox[BOXRIGHT], 0 };
         linegen1.normal = { 0, 1 };
      }

      if(slope)
         *slope = ST_HORIZONTAL;
   }
   else
   {
      if(cb_viewpoint.y < fbox[BOXBOTTOM])
      {
         linegen1.start = { fbox[BOXLEFT], fbox[BOXBOTTOM] };
         linegen1.delta = { fbox[BOXRIGHT] - fbox[BOXLEFT], 0 };
         linegen1.normal = { 0, -1 };

         linegen2.start = { fbox[BOXRIGHT], fbox[BOXBOTTOM] };
         linegen2.delta = { 0, fbox[BOXTOP] - fbox[BOXBOTTOM] };
         linegen2.normal = { 1, 0 };

         if(slope)
            *slope = ST_NEGATIVE;
      }
      else if(cb_viewpoint.y > fbox[BOXTOP])
      {
         linegen1.start = { fbox[BOXRIGHT], fbox[BOXBOTTOM] };
         linegen1.delta = { 0, fbox[BOXTOP] - fbox[BOXBOTTOM] };
         linegen1.normal = { 1, 0 };

         linegen2.start = { fbox[BOXRIGHT], fbox[BOXTOP] };
         linegen2.delta = { fbox[BOXLEFT] - fbox[BOXRIGHT], 0 };
         linegen2.normal = { 0, 1 };

         if(slope)
            *slope = ST_POSITIVE;
      }
      else
      {
         linegen1.start = { fbox[BOXRIGHT], fbox[BOXBOTTOM] };
         linegen1.delta = { 0, fbox[BOXTOP] - fbox[BOXBOTTOM] };
         linegen1.normal = { 1, 0 };

         if(slope)
            *slope = ST_VERTICAL;
      }
   }
   return true;
}

//
// Check seg against barrier bbox
//
static bool R_allowBehindSectorPortal(const cbviewpoint_t &cb_viewpoint,
                                      const float fbox[4], const seg_t &tryseg)
{
   v2float_t start = { tryseg.v1->fx, tryseg.v1->fy };
   v2float_t delta = { tryseg.v2->fx - start.x, tryseg.v2->fy - start.y };

   int boxside = P_BoxOnDivlineSideFloat(fbox, start, delta);

   if(boxside == 0)
      return true;
   if(boxside == 1)
      return false;

   windowlinegen_t linegen1, linegen2;

   slopetype_t slope, lnslope = tryseg.linedef->slopetype;
   if(!R_PickNearestBoxLines(cb_viewpoint, fbox, linegen1, linegen2, &slope))
      return true;

   if(slope == ST_VERTICAL || slope == ST_HORIZONTAL)
      return R_allowBehindBarrier(linegen1, &tryseg);

   // Slanted
   if(slope != lnslope)
      return R_allowBehindBarrier(linegen1, &tryseg) && R_allowBehindBarrier(linegen2, &tryseg);

   // Pointed to the corner
   bool revfirst = slope == ST_POSITIVE ? 
      !!((delta.x > 0) ^ (linegen1.start.x == fbox[BOXRIGHT])) :
      !!((delta.x > 0) ^ (linegen1.start.x == fbox[BOXLEFT]));

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

   return R_allowBehindBarrier(linegen1, &tryseg, revfirst) && 
      R_allowBehindBarrier(linegen2, &tryseg, !revfirst);
}

//
// Clips the given segment
// and adds any visible pieces to the line list.
//
static void R_addLine(bspcontext_t &bspcontext, cmapcontext_t &cmapcontext, planecontext_t &planecontext,
                      portalcontext_t &portalcontext, ZoneHeap &heap,
                      const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                      const contextbounds_t &bounds, const uint64_t visitid,
                      cb_seg_t &seg,
                      const seg_t *line, bool dynasegs)
{
   const portalrender_t &portalrender = portalcontext.portalrender;

   static thread_local sector_t tempsec;

   float x1, x2;
   float i1, i2, pstep;
   float lclip1, lclip2;
   v2float_t t1, t2, temp;
   const side_t *side;
   float floorx1, floorx2;
   const vertex_t *v1, *v2;

   // ioanch 20160125: reject segs in front of line when rendering line portal
   if(portalrender.active && portalrender.w->portal->type != R_SKYBOX)
   {
      // only reject if they're anchored portals (including linked)
      if(portalrender.w->type == pw_line)
      {
         if(!R_allowBehindBarrier(portalrender.w->barrier.linegen, line))
            return;
      }
      else
      {
         if(portalrender.w->line && !R_allowBehindBarrier(portalrender.w->barrier.linegen, line))
            return;
         if(!R_allowBehindSectorPortal(cb_viewpoint, portalrender.w->barrier.fbox, *line))
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

   seg.backsec = R_FakeFlat(viewpoint.z, line->backsector, &tempsec, nullptr, nullptr, true);

   // haleyjd: TEST
   // This seems to fix fiffy5, but smells like a hack to me.
   if(seg.frontsec == seg.backsec &&
      seg.frontsec->intflags & SIF_SKY &&
      seg.frontsec->srf.ceiling.height == seg.frontsec->srf.floor.height)
      seg.backsec = nullptr;

   if(!dynasegs && (line->linedef->intflags & MLI_DYNASEGLINE)) // haleyjd
      return;

   // If the frontsector is closed, don't render the line!
   // This fixes a very specific type of slime trail.
   // Unless we are viewing down into a portal...??

   //
   // IOANCH 20160120: ADD C_PORTAL AND F_PORTAL CHECK BECAUSE IT MIGHT HAVE
   // BEEN REMOVED BY R_FAKEFLAT WITHOUT ALSO CANCELLING PS_PASSABLE!
   //
   if(!seg.frontsec->srf.floor.slope && !seg.frontsec->srf.ceiling.slope &&
      seg.frontsec->srf.ceiling.height <= seg.frontsec->srf.floor.height &&
      !(seg.frontsec->intflags & SIF_SKY) &&
      !((seg.frontsec->srf.ceiling.pflags & PS_PASSABLE && seg.frontsec->srf.ceiling.portal &&
        viewpoint.z > P_PortalZ(surf_ceil, *seg.frontsec)) ||
        (seg.frontsec->srf.floor.pflags & PS_PASSABLE && seg.frontsec->srf.floor.portal &&
        viewpoint.z < P_PortalZ(surf_floor, *seg.frontsec))))
      return;

   // Reject empty two-sided lines used for line specials.
   if(seg.backsec && seg.frontsec
      && seg.backsec->srf.ceiling.pic == seg.frontsec->srf.ceiling.pic
      && seg.backsec->srf.floor.pic == seg.frontsec->srf.floor.pic
      && seg.backsec->lightlevel == seg.frontsec->lightlevel
      && seg.line->sidedef->midtexture == 0

      // killough 3/7/98: Take flats offsets into account:
      && seg.backsec->srf.floor.offset == seg.frontsec->srf.floor.offset
      && seg.backsec->srf.ceiling.offset == seg.frontsec->srf.ceiling.offset

      && seg.backsec->srf.floor.scale == seg.frontsec->srf.floor.scale
      && seg.backsec->srf.ceiling.scale == seg.frontsec->srf.ceiling.scale

      // haleyjd 11/04/10: angles
      && (seg.backsec->srf.floor.baseangle + seg.backsec->srf.floor.angle ==
          seg.frontsec->srf.floor.baseangle + seg.frontsec->srf.floor.angle)
      && (seg.backsec->srf.ceiling.baseangle + seg.backsec->srf.ceiling.angle ==
          seg.frontsec->srf.ceiling.baseangle + seg.frontsec->srf.ceiling.angle)

      // killough 4/16/98: consider altered lighting
      && seg.backsec->srf.floor.lightsec == seg.frontsec->srf.floor.lightsec
      && seg.backsec->srf.floor.lightdelta == seg.frontsec->srf.floor.lightdelta
      && seg.backsec->srf.ceiling.lightsec == seg.frontsec->srf.ceiling.lightsec
      && seg.backsec->srf.ceiling.lightdelta == seg.frontsec->srf.ceiling.lightdelta
      && (seg.backsec->flags & (SECF_FLOORLIGHTABSOLUTE | SECF_CEILLIGHTABSOLUTE))
      == (seg.frontsec->flags & (SECF_FLOORLIGHTABSOLUTE | SECF_CEILLIGHTABSOLUTE))

      && seg.backsec->srf.floor.height == seg.frontsec->srf.floor.height
      && seg.backsec->srf.ceiling.height == seg.frontsec->srf.ceiling.height

      // sf: coloured lighting
      // haleyjd 03/04/07: must test against maps, not heightsec
      && seg.backsec->bottommap == seg.frontsec->bottommap
      && seg.backsec->midmap    == seg.frontsec->midmap
      && seg.backsec->topmap    == seg.frontsec->topmap

      // SoM 12/10/03: PORTALS
      && seg.backsec->srf.ceiling.portal == seg.frontsec->srf.ceiling.portal
      && seg.backsec->srf.floor.portal == seg.frontsec->srf.floor.portal

      && (seg.backsec->srf.ceiling.portal != nullptr && (seg.backsec->srf.ceiling.pflags & PS_BLENDFLAGS) == (seg.frontsec->srf.ceiling.pflags & PS_BLENDFLAGS))
      && (seg.backsec->srf.floor.portal != nullptr && (seg.backsec->srf.floor.pflags & PS_BLENDFLAGS) == (seg.frontsec->srf.floor.pflags & PS_BLENDFLAGS))

      && !seg.line->linedef->portal

      && seg.backsec->srf.floor.slope == seg.frontsec->srf.floor.slope
      && seg.backsec->srf.ceiling.slope == seg.frontsec->srf.ceiling.slope
      )
      return;

   // The first step is to do calculations for the entire wall seg, then
   // send the wall to the clipping functions.
   v1 = line->v1;
   v2 = line->v2;

   lclip2 = line->len;
   lclip1 = 0.0f;

   temp.x = v1->fx - cb_viewpoint.x;
   temp.y = v1->fy - cb_viewpoint.y;
   t1.x   = (temp.x * cb_viewpoint.cos) - (temp.y * cb_viewpoint.sin);
   t1.y   = (temp.y * cb_viewpoint.cos) + (temp.x * cb_viewpoint.sin);
   temp.x = v2->fx - cb_viewpoint.x;
   temp.y = v2->fy - cb_viewpoint.y;
   t2.x   = (temp.x * cb_viewpoint.cos) - (temp.y * cb_viewpoint.sin);
   t2.y   = (temp.y * cb_viewpoint.cos) + (temp.x * cb_viewpoint.sin);

   // SoM: Portal lines are not texture and as a result can be clipped MUCH 
   // closer to the camera than normal lines can. This closer clipping 
   // distance is used to stave off the flash that can sometimes occur when
   // passing through a linked portal line.

   bool lineisportal;
   {
      const line_t &linedef = *line->linedef;
      lineisportal = linedef.portal ||
      (linedef.backsector && line->sidedef == &sides[linedef.sidenum[0]] &&
       ((linedef.backsector->srf.floor.portal && linedef.extflags & EX_ML_LOWERPORTAL) ||
        (linedef.backsector->srf.ceiling.portal && linedef.extflags & EX_ML_UPPERPORTAL)));
   }

   if(lineisportal && t1.x && t2.x && t1.x < t2.x &&
      ((t1.y >= 0 && t1.y < NEARCLIP && t2.y / t2.x >= t1.y / t1.x) ||
       (t2.y >= 0 && t2.y < NEARCLIP && t1.y / t1.x <= t2.y / t2.x)))
   {
      // handle the edge case where you're right with the nose on a portal line
      t1.y = t2.y = NEARCLIP;
      t1.x = -(t2.x = 10 * FRACUNIT); // some large enough value
   }

   // Use these to prevent portal lines from being cut off by the viewport
   bool clipped = false;
   bool markx1cover = false;

   if(t1.y < NEARCLIP)
   {      
      float move, movey;

      clipped = true;

      // Simple reject for lines entirely behind the view plane.
      if(t2.y < NEARCLIP)
         return;

      movey = NEARCLIP - t1.y;
      t1.x += (move = movey * ((t2.x - t1.x) / (t2.y - t1.y)));

      lclip1 = (float)sqrt(move * move + movey * movey);
      t1.y = NEARCLIP;
   }

   i1 = 1.0f / t1.y;
   x1 = (view.xcenter + (t1.x * i1 * view.xfoc));
   if(lineisportal && x1 > bounds.fstartcolumn && clipped)
      markx1cover = true;

   clipped = false;
   if(t2.y < NEARCLIP)
   {
      float move, movey;

      clipped = true;

      movey = NEARCLIP - t2.y;
      t2.x += (move = movey * ((t2.x - t1.x) / (t2.y - t1.y)));

      lclip2 -= (float)sqrt(move * move + movey * movey);
      t2.y = NEARCLIP;
   }

   i2 = 1.0f / t2.y;
   x2 = (view.xcenter + (t2.x * i2 * view.xfoc));

   // Fix now any wall or edge portal viewport cutoffs
   if(lineisportal && x2 < bounds.fendcolumn && clipped && x2 >= x1)
      x2 = bounds.fendcolumn;
   if(markx1cover && x2 >= x1)
      x1 = bounds.fstartcolumn;

   // SoM: Handle the case where a wall is only occupying a single post but 
   // still needs to be rendered to keep groups of single post walls from not
   // being rendered and causing slime trails.
   floorx1 = (float)floor(x1 + 0.999f);
   floorx2 = (float)floor(x2 - 0.001f);

   // backface rejection
   if(floorx2 < floorx1)
      return;

   // off the screen rejection
   if(floorx2 < bounds.fstartcolumn || floorx1 >= bounds.fendcolumn)
      return;

   if(x2 > x1)
      pstep = 1.0f / (x2 - x1);
   else
      pstep = 1.0f;

   side = line->sidedef;
   
   seg.toffsetx = M_FixedToFloat(side->textureoffset) + line->offset;
   seg.toffsety = M_FixedToFloat(side->rowoffset);

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

   if(seg.frontsec->srf.ceiling.slope)
   {
      float z1, z2, zstep;

      z1 = P_GetZAtf(seg.frontsec->srf.ceiling.slope, v1->fx, v1->fy);
      z2 = P_GetZAtf(seg.frontsec->srf.ceiling.slope, v2->fx, v2->fy);
      zstep = (z2 - z1) / seg.line->len;

      z1 += lclip1 * zstep;
      z2 -= (seg.line->len - lclip2) * zstep;
      seg.top = view.ycenter - ((z1 - cb_viewpoint.z) * i1);
      seg.top2 = view.ycenter - ((z2 - cb_viewpoint.z) * i2);

      seg.minfrontceil = M_FloatToFixed(z1 < z2 ? z1 : z2);
   }
   else
   {
      seg.top = view.ycenter - ((seg.frontsec->srf.ceiling.heightf - cb_viewpoint.z) * i1);
      seg.top2 = view.ycenter - ((seg.frontsec->srf.ceiling.heightf - cb_viewpoint.z) * i2);
      seg.minfrontceil = seg.frontsec->srf.ceiling.height;
   }
   seg.topstep = (seg.top2 - seg.top) * pstep;


   if(seg.frontsec->srf.floor.slope)
   {
      float z1, z2, zstep;

      z1 = P_GetZAtf(seg.frontsec->srf.floor.slope, v1->fx, v1->fy);
      z2 = P_GetZAtf(seg.frontsec->srf.floor.slope, v2->fx, v2->fy);
      zstep = (z2 - z1) / seg.line->len;

      z1 += lclip1 * zstep;
      z2 -= (seg.line->len - lclip2) * zstep;
      seg.bottom = view.ycenter - ((z1 - cb_viewpoint.z) * i1) - 1.0f;
      seg.bottom2 = view.ycenter - ((z2 - cb_viewpoint.z) * i2) - 1.0f;

      seg.maxfrontfloor = M_FloatToFixed(z1 > z2 ? z1 : z2);
   }
   else
   {      
      seg.bottom  = view.ycenter - ((seg.frontsec->srf.floor.heightf - cb_viewpoint.z) * i1) - 1.0f;
      seg.bottom2 = view.ycenter - ((seg.frontsec->srf.floor.heightf - cb_viewpoint.z) * i2) - 1.0f;
      seg.maxfrontfloor = seg.frontsec->srf.floor.height;
   }

   seg.bottomstep = (seg.bottom2 - seg.bottom) * pstep;

   // Get these from the actual sectors because R_FakeFlat could have changed the actual heights.
   textop    = seg.line->frontsector->srf.ceiling.heightf - cb_viewpoint.z;
   texbottom = seg.line->frontsector->srf.floor.heightf - cb_viewpoint.z;

   seg.f_portalignore = seg.c_portalignore = false;

   // ioanch 20160312: also treat polyobject portal lines as 1-sided
   const sector_t *beyond = seg.line->linedef->intflags & MLI_1SPORTALLINE && 
      seg.line->linedef->beyondportalline ? 
      seg.line->linedef->beyondportalline->frontsector : nullptr;
   if(!seg.backsec || beyond) 
   {
      R_1SidedLine(
         cmapcontext, planecontext, portalcontext, heap, viewpoint, cb_viewpoint,
         bounds, visitid, seg, pstep,
         i1, i2, textop, texbottom, beyond, side, line
      );
   }
   else
   {
      if(seg.frontsec->srf.floor.slope || seg.frontsec->srf.ceiling.slope ||
         seg.backsec->srf.floor.slope || seg.backsec->srf.ceiling.slope)
      {
         R_2S_Sloped(
            cmapcontext, planecontext, portalcontext, heap, viewpoint, cb_viewpoint,
            bounds, visitid, seg, pstep,
            i1, i2, textop, texbottom, v1, v2, lclip1, lclip2
         );
      }
      else
      {
         R_2S_Normal(
            cmapcontext, planecontext, portalcontext, heap, viewpoint, cb_viewpoint,
            bounds, visitid, seg, pstep, i1, i2, textop, texbottom
         );
      }
   }

   // SoM: This really needs to be handled here. The float values need to be 
   // clipped to sane values here because floats have a higher range of values
   // than ints do. If the clipping is done after the casting, the step values
   // will no longer be accurate. This ensures more correct projection and 
   // texturing.
   if(x1 < bounds.fstartcolumn)
   {
      float clipx = bounds.fstartcolumn - x1;

      seg.dist += clipx * seg.diststep;
      seg.len += clipx * seg.lenstep;

      seg.top += clipx * seg.topstep;
      seg.bottom += clipx * seg.bottomstep;

      if(seg.toptex || seg.t_window)
         seg.high += clipx * seg.highstep;
      if(seg.bottomtex || seg.b_window)
         seg.low += clipx * seg.lowstep;

      x1 = floorx1 = bounds.fstartcolumn;
   }

   if(x2 >= bounds.fendcolumn)
   {
      float clipx = x2 - (bounds.fendcolumn - 1.0f);

      seg.dist2 -= clipx * seg.diststep;
      seg.len2 -= clipx * seg.lenstep;

      seg.top2 -= clipx * seg.topstep;
      seg.bottom2 -= clipx * seg.bottomstep;

      if(seg.toptex || seg.t_window)
         seg.high2 -= clipx * seg.highstep;
      if(seg.bottomtex || seg.b_window)
         seg.low2 -= clipx * seg.lowstep;

      x2 = floorx2 = (bounds.fendcolumn - 1.0f);
   }

   seg.x1 = (int)floorx1;
   seg.x1frac = x1;
   seg.x2 = (int)floorx2;
   seg.x2frac = x2;

   if(portalrender.active && portalrender.segClipFunc)
   {
      portalrender.segClipFunc(
         bspcontext, cmapcontext, planecontext, portalcontext, heap,
         viewpoint, cb_viewpoint, bounds, seg
      );
   }
   else if(seg.clipsolid)
   {
      R_clipSolidWallSegment(
         bspcontext, cmapcontext, planecontext, portalcontext, heap,
         viewpoint, cb_viewpoint, bounds, seg, seg.x1, seg.x2
      );
   }
   else
   {
      R_clipPassWallSegment(
         bspcontext, cmapcontext, planecontext, portalcontext, heap,
         viewpoint, cb_viewpoint, bounds, seg, seg.x1, seg.x2
      );
   }

   // Add new solid segs when it is safe to do so...
   R_addMarkedSegs(bspcontext);

}

static constexpr int checkcoord[12][4] = // killough -- static const
{
   { BOXRIGHT, BOXTOP,    BOXLEFT, BOXBOTTOM }, // 0
   { BOXRIGHT, BOXTOP,    BOXLEFT, BOXTOP    }, // 1
   { BOXRIGHT, BOXBOTTOM, BOXLEFT, BOXTOP    }, // 2
   { 0 },                                       // 3, never happens
   { BOXLEFT,  BOXTOP,    BOXLEFT, BOXBOTTOM }, // 4
   { 0,        0,         0,       0,        }, // 5, special case (return true)
   { BOXRIGHT, BOXBOTTOM, BOXRIGHT,BOXTOP    }, // 6
   { 0 },                                       // 7, never happens
   { BOXLEFT,  BOXTOP,    BOXRIGHT,BOXBOTTOM }, // 8
   { BOXLEFT,  BOXBOTTOM, BOXRIGHT,BOXBOTTOM }, // 9
   { BOXLEFT,  BOXBOTTOM, BOXRIGHT,BOXTOP    }  // 10
};

//
// Checks BSP node/subtree bounding box.
// Returns true if some part of the bbox might be visible.
//
static bool R_checkBBox(const viewpoint_t &viewpoint,
                        const contextbounds_t &bounds,
                        const cliprange_t *const solidsegs,
                        const fixed_t *const bspcoord) // killough 1/28/98: static
{
   int     boxpos, boxx, boxy;
   fixed_t x1, x2, y1, y2;
   angle_t angle1, angle2, span, tspan;
   int     sx1, sx2;
   const cliprange_t *start;

   // 0,0 | 1,0 | 2,0   |  0  |  1  |  2
   //  ---|-----|---    |  ---|-----|---
   // 0,1 | 1,1 | 2,1   |  4  |  5  |  6
   //  ---|-----|---    |  ---|-----|---
   // 0,2 | 1,2 | 2,2   |  8  |  9  |  10

   // Find the corners of the box
   // that define the edges from current viewpoint.
   boxx = viewpoint.x <= bspcoord[BOXLEFT] ? 0 : viewpoint.x < bspcoord[BOXRIGHT ] ? 1 : 2;
   boxy = viewpoint.y >= bspcoord[BOXTOP ] ? 0 : viewpoint.y > bspcoord[BOXBOTTOM] ? 1 : 2;

   boxpos = (boxy << 2) + boxx;
   if(boxpos == 5)
      return true;

   x1 = bspcoord[checkcoord[boxpos][0]];
   y1 = bspcoord[checkcoord[boxpos][1]];
   x2 = bspcoord[checkcoord[boxpos][2]];
   y2 = bspcoord[checkcoord[boxpos][3]];

   // check clip list for an open space
   angle1 = R_PointToAngle(viewpoint.x, viewpoint.y, x1, y1) - viewpoint.angle;
   angle2 = R_PointToAngle(viewpoint.x, viewpoint.y, x2, y2) - viewpoint.angle;
   
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
   if(sx1 > bounds.startcolumn) sx1--;
   if(sx2 < bounds.endcolumn - 1) sx2++;

   // SoM: Removed the "does not cross a pixel" test

   start = solidsegs;
   while(start->last < sx2)
      ++start;
   
   if(sx1 >= start->first && sx2 <= start->last)
      return false;      // The clippost contains the new span.
   
   return true;
}

//
// Recurse through a polynode mini-BSP
//
static void R_renderPolyNode(bspcontext_t &bspcontext, cmapcontext_t &cmapcontext, planecontext_t &planecontext,
                             portalcontext_t &portalcontext, ZoneHeap &heap,
                             const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                             const contextbounds_t &bounds, const uint64_t visitid,
                             cb_seg_t &cbseg,
                             const rpolynode_t *node)
{
   while(node)
   {
      const int side = R_PointOnDynaSegSide(node->partition, cb_viewpoint.x, cb_viewpoint.y);

      // render frontspace
      R_renderPolyNode(
         bspcontext, cmapcontext, planecontext, portalcontext, heap,
         viewpoint, cb_viewpoint, bounds, visitid, cbseg, node->children[side]
      );

      // render partition seg
      R_addLine(
         bspcontext, cmapcontext, planecontext, portalcontext, heap,
         viewpoint, cb_viewpoint, bounds, visitid, cbseg, &node->partition->seg, true
      );

      // continue to render backspace
      node = node->children[side ^ 1];
   }
}

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
static void R_addDynaSegs(bspcontext_t &bspcontext, cmapcontext_t &cmapcontext, planecontext_t &planecontext,
                          portalcontext_t &portalcontext, ZoneHeap &heap,
                          const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                          const contextbounds_t &bounds, const uint64_t visitid,
                          cb_seg_t &seg,
                          const subsector_t *sub)
{
   if(sub->bsp)
   {
      R_renderPolyNode(
         bspcontext, cmapcontext, planecontext, portalcontext, heap,
         viewpoint, cb_viewpoint, bounds, visitid, seg, sub->bsp->root
      );
   }
}

//
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
// killough 1/31/98 -- made static, polished
//
static void R_subsector(rendercontext_t &context, const int num)
{
#ifdef RANGECHECK
   if(num >= numsubsectors)
      I_Error("R_subsector: ss %i with numss = %i\n", num, numsubsectors);
#endif

   bspcontext_t    &bspcontext     = context.bspcontext;
   cmapcontext_t   &cmapcontext    = context.cmapcontext;
   planecontext_t  &planecontext   = context.planecontext;
   portalcontext_t &portalcontext  = context.portalcontext;
   spritecontext_t &spritecontext  = context.spritecontext;

   ZoneHeap        &heap           = *context.heap;

   const viewpoint_t     &viewpoint    = context.view;
   const cbviewpoint_t   &cb_viewpoint = context.cb_view;
   const contextbounds_t &bounds       = context.bounds;
   const uint64_t         visitid      = R_GetVisitID(context);

   const portalrender_t &portalrender = portalcontext.portalrender;

   static thread_local sector_t tempsec; // killough 3/7/98: deep water hack

   int                count;
   const seg_t       *line;
   const subsector_t *sub;
   int                floorlightlevel;      // killough 3/16/98: set floor lightlevel
   int                ceilinglightlevel;    // killough 4/11/98
   float              floorangle;           // haleyjd 01/05/08: plane angles
   float              ceilingangle;

   bool               visible;
   v3float_t          cam;

   cb_seg_t           seg{}; // haleyjd 09/22/07: clear seg structure

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

   R_SectorColormap(cmapcontext, viewpoint.z, seg.frontsec);

   // killough 3/8/98, 4/4/98: Deep water / fake ceiling effect
   seg.frontsec = R_FakeFlat(viewpoint. z, seg.frontsec, &tempsec,
                             &floorlightlevel, &ceilinglightlevel, false);   // killough 4/11/98

   // ioanch: reject all sectors fully above or below a sector portal.
   if(portalrender.active && portalrender.w->portal->type != R_SKYBOX &&
      ((portalrender.w->type == pw_ceiling &&
        seg.frontsec->srf.ceiling.height < portalrender.w->planez + viewpoint.z - portalrender.w->vz) ||
       (portalrender.w->type == pw_floor &&
        seg.frontsec->srf.floor.height > portalrender.w->planez + viewpoint.z - portalrender.w->vz)))
   {
      return;
   }

   // haleyjd 01/05/08: determine angles for floor and ceiling
   floorangle   = seg.frontsec->srf.floor.baseangle + seg.frontsec->srf.floor.angle;
   ceilingangle = seg.frontsec->srf.ceiling.baseangle + seg.frontsec->srf.ceiling.angle;

   // killough 3/7/98: Add (x,y) offsets to flats, add deep water check
   // killough 3/16/98: add floorlightlevel
   // killough 10/98: add support for skies transferred from sidedefs

   // SoM: Slopes!
   cam.x = cb_viewpoint.x;
   cam.y = cb_viewpoint.y;
   cam.z = cb_viewpoint.z;

   // -- Floor plane and portal --
   visible  = (!seg.frontsec->srf.floor.slope && seg.frontsec->srf.floor.height < viewpoint.z)
           || (seg.frontsec->srf.floor.slope
           &&  P_DistFromPlanef(&cam, &seg.frontsec->srf.floor.slope->of,
                                &seg.frontsec->srf.floor.slope->normalf) > 0.0f);

   // ioanch 20160118: ADDED A f_portal existence check!
   seg.portal.floor = seg.frontsec->srf.floor.pflags & PS_VISIBLE
               && (!portalrender.active || portalrender.w->type != pw_ceiling)
               && (visible ||
               (seg.frontsec->srf.floor.portal && seg.frontsec->srf.floor.portal->type < R_TWOWAY))
               ? seg.frontsec->srf.floor.portal : nullptr;

   // This gets a little convoluted if you try to do it on one inequality
   if(seg.portal.floor)
   {
      unsigned int fpalpha = (seg.frontsec->srf.floor.pflags >> PO_OPACITYSHIFT) & 0xff;
      visible = (visible && (fpalpha > 0));

      seg.plane.floor = visible && seg.frontsec->srf.floor.pflags & PS_OVERLAY ?
        R_FindPlane(cmapcontext,
                    planecontext,
                    heap,
                    viewpoint,
                    cb_viewpoint,
                    bounds,
                    seg.frontsec->srf.floor.height,
                    seg.frontsec->srf.floor.pflags & PS_USEGLOBALTEX ?
                    seg.portal.floor->globaltex : seg.frontsec->srf.floor.pic,
                    floorlightlevel,                // killough 3/16/98
                    seg.frontsec->srf.floor.offset,       // killough 3/7/98
                    seg.frontsec->srf.floor.scale,
                    floorangle, seg.frontsec->srf.floor.slope,
                    seg.frontsec->srf.floor.pflags,
                    fpalpha,
                    portalcontext.portalstates[seg.portal.floor->index].poverlay) : nullptr;
   }
   else
   {
      // SoM: If there is an active portal, forget about the floorplane.
      seg.plane.floor = (visible || // killough 3/7/98
         (seg.frontsec->heightsec != -1 &&
          sectors[seg.frontsec->heightsec].intflags & SIF_SKY)) ?
        R_FindPlane(cmapcontext,
                    planecontext,
                    heap,
                    viewpoint,
                    cb_viewpoint,
                    bounds,
                    seg.frontsec->srf.floor.height,
                    R_IsSkyFlat(seg.frontsec->srf.floor.pic) &&  // kilough 10/98
                    seg.frontsec->sky & PL_SKYFLAT ? seg.frontsec->sky :
                    seg.frontsec->srf.floor.pic,
                    floorlightlevel,                // killough 3/16/98
                    seg.frontsec->srf.floor.offset,       // killough 3/7/98
                    seg.frontsec->srf.floor.scale,
                    floorangle, seg.frontsec->srf.floor.slope, 0, 255, nullptr) : nullptr;
   }


   // -- Ceiling plane and portal --
   visible  = (!seg.frontsec->srf.ceiling.slope && seg.frontsec->srf.ceiling.height > viewpoint.z)
           || (seg.frontsec->srf.ceiling.slope
           &&  P_DistFromPlanef(&cam, &seg.frontsec->srf.ceiling.slope->of,
                                &seg.frontsec->srf.ceiling.slope->normalf) > 0.0f);

   // ioanch 20160118: ADDED A c_portal existence check!
   seg.portal.ceiling = seg.frontsec->srf.ceiling.pflags & PS_VISIBLE
               && (!portalrender.active || portalrender.w->type != pw_floor)
               && (visible ||
               (seg.frontsec->srf.ceiling.portal && seg.frontsec->srf.ceiling.portal->type < R_TWOWAY))
               ? seg.frontsec->srf.ceiling.portal : nullptr;

   // This gets a little convoluted if you try to do it on one inequality
   if(seg.portal.ceiling)
   {
      unsigned int cpalpha = (seg.frontsec->srf.ceiling.pflags >> PO_OPACITYSHIFT) & 0xff;
      visible = (visible && (cpalpha > 0));

      seg.plane.ceiling = visible && seg.frontsec->srf.ceiling.pflags & PS_OVERLAY ?
        R_FindPlane(cmapcontext,
                    planecontext,
                    heap,
                    viewpoint,
                    cb_viewpoint,
                    bounds,
                    seg.frontsec->srf.ceiling.height,
                    seg.frontsec->srf.ceiling.pflags & PS_USEGLOBALTEX ?
                    seg.portal.ceiling->globaltex : seg.frontsec->srf.ceiling.pic,
                    ceilinglightlevel,                // killough 3/16/98
                    seg.frontsec->srf.ceiling.offset,       // killough 3/7/98
                    seg.frontsec->srf.ceiling.scale,
                    ceilingangle, seg.frontsec->srf.ceiling.slope,
                    seg.frontsec->srf.ceiling.pflags,
                    cpalpha,
                    portalcontext.portalstates[seg.portal.ceiling->index].poverlay) : nullptr;
   }
   else
   {
      seg.plane.ceiling = (visible ||
         (seg.frontsec->intflags & SIF_SKY) ||
        (seg.frontsec->heightsec != -1 &&
         R_IsSkyFlat(sectors[seg.frontsec->heightsec].srf.floor.pic))) ?
        R_FindPlane(cmapcontext,
                    planecontext,
                    heap,
                    viewpoint,
                    cb_viewpoint,
                    bounds,
                    seg.frontsec->srf.ceiling.height,     // killough 3/8/98
                    (seg.frontsec->intflags & SIF_SKY) &&  // kilough 10/98
                    seg.frontsec->sky & PL_SKYFLAT ? seg.frontsec->sky :
                    seg.frontsec->srf.ceiling.pic,
                    ceilinglightlevel,              // killough 4/11/98
                    seg.frontsec->srf.ceiling.offset,     // killough 3/7/98
                    seg.frontsec->srf.ceiling.scale,
                    ceilingangle, seg.frontsec->srf.ceiling.slope, 0, 255, nullptr) : nullptr;
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

   R_AddSprites(
      context.cmapcontext, spritecontext, *context.heap, viewpoint, cb_viewpoint, bounds,
      portalrender, sub->sector, (floorlightlevel + ceilinglightlevel) / 2
   );

   // haleyjd 02/19/06: draw polyobjects before static lines
   // haleyjd 10/09/06: skip call entirely if no polyobjects

   if(sub->polyList)
   {
      R_addDynaSegs(
         bspcontext, cmapcontext, planecontext, portalcontext, heap,
         viewpoint, cb_viewpoint, bounds, visitid, seg, sub
      );
   }

   while(count--)
   {
      R_addLine(
         bspcontext, cmapcontext, planecontext, portalcontext, heap,
         viewpoint, cb_viewpoint, bounds, visitid, seg, line++, false
      );
   }
}

//
// Actually iterate over all the nodes in an rpolynod_t tree
//
void R_forEachPolyNodeRecursive(void (*func)(rpolynode_t *node), rpolynode_t *root)
{
   if(!root)
      return;

   func(root);

   R_forEachPolyNodeRecursive(func, root->children[0]);
   R_forEachPolyNodeRecursive(func, root->children[1]);
}

//
// Iterate over every subsector's root rpolynode_t
//
void R_ForEachPolyNode(void (*func)(rpolynode_t *node))
{
   for(int p = 0; p < numPolyObjects; p++)
   {
      for(int ss = 0; ss < PolyObjects[p].numDSS; ss++)
      {
         subsector_t *sub = PolyObjects[p].dynaSubsecs[ss];

         if(sub->bsp)
            R_forEachPolyNodeRecursive(func, sub->bsp->root);
      }
   }
}

//
// Pre-rendering setup
//
void R_PreRenderBSP()
{
   for(int p = 0; p < numPolyObjects; p++)
   {
      for(int ss = 0; ss < PolyObjects[p].numDSS; ss++)
      {
         subsector_t *sub = PolyObjects[p].dynaSubsecs[ss];

         bool needbsp = (!sub->bsp || sub->bsp->dirty);

         if(needbsp)
         {
            if(sub->bsp)
               R_FreeDynaBSP(sub->bsp);
            sub->bsp = R_BuildDynaBSP(sub);
         }
      }
   }
}

//
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.
//
// killough 5/2/98: reformatted, removed tail recursion
//
void R_RenderBSPNode(rendercontext_t &context, int bspnum)
{
   while(!(bspnum & NF_SUBSECTOR))  // Found a subsector?
   {
      const node_t *bsp = &nodes[bspnum];

      // Decide which side the view point is on.
      int side = R_PointOnSide(context.view.x, context.view.y, bsp);

      // Recursively divide front space.
      R_RenderBSPNode(context, bsp->children[side]);

      // Possibly divide back space.
      
      if(!R_checkBBox(context.view, context.bounds, context.bspcontext.solidsegs, bsp->bbox[side^=1]))
         return;
      
      bspnum = bsp->children[side];
   }
   R_subsector(context, bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
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
