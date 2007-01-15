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

static const char
rcsid[] = "$Id: r_bsp.c,v 1.17 1998/05/03 22:47:33 killough Exp $";

#include "doomstat.h"
#include "m_bbox.h"
#include "i_system.h"
#include "r_main.h"
#include "r_segs.h"
#include "r_plane.h"
#include "r_things.h"

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

//
// R_ClipSolidWallSegment
//
// Handles solid walls,
//  e.g. single sided LineDefs (middle texture)
//  that entirely block the view.
//
static void R_ClipSolidWallSegment()
{
   cliprange_t *next, *start;
   
   // Find the first range that touches the range
   // (adjacent pixels are touching).
   
   start = solidsegs;
   while(start->last < seg.x1 - 1)
      ++start;

   if(seg.x1 < start->first)
   {
      if(seg.x2 < start->first - 1)
      {
         // Post is entirely visible (above start), so insert a new clippost.
         R_StoreWallRange(seg.x1, seg.x2);
         
         // 1/11/98 killough: performance tuning using fast memmove
         memmove(start + 1, start, (++newend - start) * sizeof(*start));
         start->first = seg.x1;
         start->last = seg.x2;
         return;
      }

      // There is a fragment above *start.
      R_StoreWallRange(seg.x1, start->first - 1);
      
      // Now adjust the clip size.
      start->first = seg.x1;
   }

   // Bottom contained in start?
   if(seg.x2 <= start->last)
      return;

   next = start;
   while(seg.x2 >= (next + 1)->first - 1)
   {      // There is a fragment between two posts.
      R_StoreWallRange(next->last + 1, (next + 1)->first - 1);
      ++next;
      if(seg.x2 <= next->last)
      {  
         // Bottom is contained in next. Adjust the clip size.
         start->last = next->last;
         goto crunch;
      }
   }

   // There is a fragment after *next.
   R_StoreWallRange(next->last + 1, seg.x2);
   
   // Adjust the clip size.
   start->last = seg.x2;
   
   // Remove start+1 to next from the clip list,
   // because start now covers their area.
crunch:
   if(next == start) // Post just extended past the bottom of one post.
      return;
   
   while(next++ != newend)      // Remove a post.
      *++start = *next;
   
   newend = start + 1;
}

//
// R_ClipPassWallSegment
//
// Clips the given range of columns,
//  but does not includes it in the clip list.
// Does handle windows,
//  e.g. LineDefs with upper and lower texture.
//
static void R_ClipPassWallSegment()
{
   cliprange_t *start = solidsegs;
   
   // Find the first range that touches the range
   //  (adjacent pixels are touching).
   while(start->last < seg.x1 - 1)
      ++start;

   if(seg.x1 < start->first)
   {
      if(seg.x2 < start->first - 1)
      {
         // Post is entirely visible (above start).
         R_StoreWallRange(seg.x1, seg.x2);
         return;
      }

      // There is a fragment above *start.
      R_StoreWallRange(seg.x1, start->first - 1);
   }

   // Bottom contained in start?
   if(seg.x2 <= start->last)
      return;

   while(seg.x2 >= (start + 1)->first - 1)
   {
      // There is a fragment between two posts.
      R_StoreWallRange(start->last + 1, (start + 1)->first - 1);
      ++start;
      
      if(seg.x2 <= start->last)
         return;
   }
   
   // There is a fragment after *next.
   R_StoreWallRange(start->last + 1, seg.x2);
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
}

#ifdef R_PORTALS
// haleyjd: DEBUG
boolean R_SetupPortalClipsegs(float *top, float *bottom)
{
   int i = 0;
   cliprange_t *seg;
   
   R_ClearClipSegs();
   
   // extend first solidseg to one column left of first open post
   while(i < viewwidth && bottom[i] - top[i] <= -1.0f) 
      ++i;
   
   // first open post found, set last closed post to last closed post (i - 1);
   solidsegs[0].last = i - 1;
   
   // the entire thing is closed?
   if(i == viewwidth)
      return false;
   
   seg = solidsegs + 1;
   
   while(1)
   {
      //find the first closed post.
      while(i < viewwidth && bottom[i] - top[i] > -1.0f) 
         ++i;
      
      if(i == viewwidth)
         goto endopen;
      
      // set the solidsegs
      seg->first = i;
      
      // find the first open post
      while(i < viewwidth && top[i] + 1 >= bottom[i]) i++;
      if(i == viewwidth)
         goto endclosed;
      
      seg->last = i - 1;
      seg++;
   }
   
endopen:
   seg->first = viewwidth;
   seg->last = 0x7fff;
   newend = seg + 1;
   return true;
   
endclosed:
   seg->last = 0x7fff;
   newend = seg + 1;
   return true;
}
#endif

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
	 (heightsec!=-1 && viewz<=sectors[heightsec].floorheight);

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
// R_AddLine
//
// Clips the given segment
// and adds any visible pieces to the line list.
//
#define NEARCLIP 0.1f
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

   tempsec.frameid = 0;

   seg.clipsolid = false;
   seg.backsec = line->backsector ? R_FakeFlat(line->backsector, &tempsec, NULL, NULL, true) : NULL;
   seg.line = line;

   if(seg.backsec && seg.backsec->frameid != frameid)
   {
      seg.backsec->floorheightf = (float)seg.backsec->floorheight / 65536.0f;
      seg.backsec->ceilingheightf = (float)seg.backsec->ceilingheight / 65536.0f;
      seg.backsec->frameid = frameid;
   }

   // If the frontsector is closed, don't render the line!
   // This fixes a very specific type of slime trail.
   if(seg.frontsec->ceilingheight <= seg.frontsec->floorheight)
      return;

   // Reject empty two-sided lines used for line specials.
   if(seg.backsec && seg.frontsec 
      && seg.backsec->ceilingpic == seg.frontsec->ceilingpic 
      && seg.backsec->floorpic == seg.frontsec->floorpic
      && seg.backsec->lightlevel == seg.frontsec->lightlevel 
      && seg.line->sidedef->midtexture == 0
      
      // killough 3/7/98: Take flats offsets into account:
      && seg.backsec->floor_xoffs == seg.frontsec->floor_xoffs
      && seg.backsec->floor_yoffs == seg.frontsec->floor_yoffs
      && seg.backsec->ceiling_xoffs == seg.frontsec->ceiling_xoffs
      && seg.backsec->ceiling_yoffs == seg.frontsec->ceiling_yoffs
      
      // killough 4/16/98: consider altered lighting
      && seg.backsec->floorlightsec == seg.frontsec->floorlightsec
      && seg.backsec->ceilinglightsec == seg.frontsec->ceilinglightsec

      && seg.backsec->floorheight == seg.frontsec->floorheight
      && seg.backsec->ceilingheight == seg.frontsec->ceilingheight
      
      // sf: coloured lighting
      && seg.backsec->heightsec == seg.frontsec->heightsec
#ifdef R_PORTALS
      // SoM 12/10/03: PORTALS
      && seg.backsec->c_portal == seg.frontsec->c_portal
      && seg.backsec->f_portal == seg.frontsec->f_portal
#endif
      )
      return;
      

   // The first step is to do calculations for the entire wall seg, then
   // send the wall to the clipping functions.
   temp.fx = line->v1->fx - view.x;
   temp.fy = line->v1->fy - view.y;
   t1.fx = (temp.fx * view.cos) - (temp.fy * view.sin);
   t1.fy = (temp.fy * view.cos) + (temp.fx * view.sin);

   temp.fx = line->v2->fx - view.x;
   temp.fy = line->v2->fy - view.y;
   t2.fx = (temp.fx * view.cos) - (temp.fy * view.sin);
   t2.fy = (temp.fy * view.cos) + (temp.fx * view.sin);

   // Simple reject for lines entirely behind the view plane.
   if(t1.fy < NEARCLIP && t2.fy < NEARCLIP)
      return;

   toffsetx = toffsety = 0;

   if(t1.fy < NEARCLIP)
   {
      float move, movey;

      // SoM: optimization would be to store the line slope in float format in the segs
      movey = NEARCLIP - t1.fy;
      move = movey * ((t2.fx - t1.fx) / (t2.fy - t1.fy));

      t1.fx += move;
      toffsetx += (float)sqrt(move * move + movey * movey);
      t1.fy = NEARCLIP;
      i1 = 1.0f / NEARCLIP;
      x1 = (view.xcenter + (t1.fx * i1 * view.xfoc));
   }
   else
   {
      i1 = 1.0f / t1.fy;
      x1 = (view.xcenter + (t1.fx * i1 * view.xfoc));
   }


   if(t2.fy < NEARCLIP)
   {
      // SoM: optimization would be to store the line slope in float format in the segs
      t2.fx += (NEARCLIP - t2.fy) * ((t2.fx - t1.fx) / (t2.fy - t1.fy));
      t2.fy = NEARCLIP;
      i2 = 1.0f / NEARCLIP;
      x2 = (view.xcenter + (t2.fx * i2 * view.xfoc));
   }
   else
   {
      i2 = 1.0f / t2.fy;
      x2 = (view.xcenter + (t2.fx * i2 * view.xfoc));
   }

   // SoM: Handle the case where a wall is only occupying a single post but still needs to be 
   // rendered to keep groups of single post walls from not being rendered and causing slime 
   // trails.
   floorx1 = (float)floor(x1 + 0.5f);
   floorx2 = (float)floor(x2 - 0.5f);

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
   
   seg.toffsetx = toffsetx + (side->textureoffset / 65536.0f) + (line->offset / 65536.0f);
   seg.toffsety = toffsety + (side->rowoffset / 65536.0f);

   if(seg.toffsetx < 0)
   {
      float maxtexw;
      // SoM: ok, this was driving me crazy. It seems that when the offset is less than 0, the
      // fractional part will cause the texel at 0 + abs(seg.toffsetx) to double and it will
      // strip the first texel to one column. This is because -64 + ANY FRACTION is going to cast
      // to 63 and when you cast -0.999 through 0.999 it will cast to 0. The first step is to
      // find the largest texture width on the line to make sure all the textures will start
      // at the same column when the offsets are adjusted.

      maxtexw = 0.0f;
      if(side->toptexture)
         maxtexw = (float)texturewidthmask[side->toptexture];
      if(side->midtexture && texturewidthmask[side->midtexture] > maxtexw)
         maxtexw = (float)texturewidthmask[side->midtexture];
      if(side->bottomtexture && texturewidthmask[side->bottomtexture] > maxtexw)
         maxtexw = (float)texturewidthmask[side->bottomtexture];

      // Then adjust the offset to zero or the first positive value that will repeat correctly
      // with the largest texture on the line.
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

#ifdef R_PORTALS
   seg.f_portalignore = seg.c_portalignore = false;
#endif

   if(!seg.backsec)
   {
      seg.twosided = false;
      seg.toptex = seg.bottomtex = 0;
      seg.midtex = texturetranslation[side->midtexture];
      seg.midtexh = textureheight[side->midtexture] >> FRACBITS;

      if(seg.line->linedef->flags & ML_DONTPEGBOTTOM)
         seg.midtexmid = (int)((seg.bottom + seg.midtexh + seg.toffsety) * FRACUNIT);
      else
         seg.midtexmid = (int)((seg.top + seg.toffsety) * FRACUNIT);

      seg.markceiling = seg.ceilingplane ? true : false;
      seg.markfloor = seg.floorplane ? true : false;
      seg.clipsolid = true;
      seg.segtextured = seg.midtex ? true : false;

#ifdef R_PORTALS
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
      boolean mark;

      seg.twosided = true;

      mark = seg.frontsec->lightlevel != seg.backsec->lightlevel ||
             seg.frontsec->heightsec != -1 ||
             seg.frontsec->heightsec != seg.backsec->heightsec ? true : false;


      seg.high = seg.backsec->ceilingheightf - view.z;

      seg.clipsolid = seg.backsec->ceilingheight <= seg.backsec->floorheight
                || ((seg.frontsec->ceilingheight <= seg.backsec->floorheight
                || seg.backsec->ceilingheight <= seg.frontsec->floorheight 
                || seg.backsec->floorheight >= seg.backsec->ceilingheight) 
                && !((seg.frontsec->ceilingpic == skyflatnum 
                   || seg.frontsec->ceilingpic == sky2flatnum)
                   && (seg.backsec->ceilingpic == skyflatnum 
                   ||  seg.backsec->ceilingpic == sky2flatnum))) ? true : false;

      if((seg.frontsec->ceilingpic == skyflatnum 
          || seg.frontsec->ceilingpic == sky2flatnum)
          && (seg.backsec->ceilingpic == skyflatnum 
          ||  seg.backsec->ceilingpic == sky2flatnum))
         seg.top = seg.high;

      seg.markceiling = seg.ceilingplane && (mark || seg.clipsolid ||
         seg.top != seg.high ||
         seg.frontsec->ceiling_xoffs != seg.backsec->ceiling_xoffs ||
         seg.frontsec->ceiling_yoffs != seg.backsec->ceiling_yoffs ||
         seg.frontsec->ceilingpic != seg.backsec->ceilingpic ||
         seg.frontsec->c_portal != seg.backsec->c_portal ||
         seg.frontsec->ceilinglightsec != seg.backsec->ceilinglightsec) ? true : false;

      if(seg.high < seg.top && side->toptexture)
      {
         seg.toptex = texturetranslation[side->toptexture];
         seg.toptexh = textureheight[side->toptexture] >> FRACBITS;

         if(seg.line->linedef->flags & ML_DONTPEGTOP)
            seg.toptexmid = (int)((seg.top + seg.toffsety) * FRACUNIT);
         else
            seg.toptexmid = (int)((seg.high + seg.toptexh + seg.toffsety) * FRACUNIT);
      }
      else
         seg.toptex = 0;


      seg.markfloor = seg.floorplane && (mark || seg.clipsolid ||  
         seg.frontsec->floorheight != seg.backsec->floorheight ||
         seg.frontsec->floor_xoffs != seg.backsec->floor_xoffs ||
         seg.frontsec->floor_yoffs != seg.backsec->floor_yoffs ||
         seg.frontsec->floorpic != seg.backsec->floorpic ||
         seg.frontsec->f_portal != seg.backsec->f_portal ||
         seg.frontsec->floorlightsec != seg.backsec->floorlightsec) ? true : false;

#ifdef R_PORTALS
      // SoM: some portal types should be rendered even if the player is above or below the
      // ceiling or floor plane.
      // haleyjd 03/12/06: inverted predicates to simplify
      if(seg.backsec->f_portal != seg.frontsec->f_portal)
      {
         if(seg.frontsec->f_portal && seg.frontsec->f_portal->type != R_LINKED && 
            seg.frontsec->f_portal->type != R_TWOWAY)
            seg.f_portalignore = true;
      }

      if(seg.backsec->c_portal != seg.frontsec->c_portal)
      {
         if(seg.frontsec->c_portal && seg.frontsec->c_portal->type != R_LINKED &&
            seg.frontsec->c_portal->type != R_TWOWAY)
            seg.c_portalignore = true;
      }
#endif

      seg.low = seg.backsec->floorheightf - view.z;
      if(seg.bottom < seg.low && side->bottomtexture)
      {
         seg.bottomtex = texturetranslation[side->bottomtexture];
         seg.bottomtexh = textureheight[side->bottomtexture] >> FRACBITS;

         if(seg.line->linedef->flags & ML_DONTPEGBOTTOM)
            seg.bottomtexmid = (int)((seg.top + seg.toffsety) * FRACUNIT);
         else
            seg.bottomtexmid = (int)((seg.low + seg.toffsety) * FRACUNIT);
      }
      else
         seg.bottomtex = 0;

      seg.midtex = 0;
      seg.maskedtex = seg.side->midtexture ? true : false;
      seg.segtextured = seg.maskedtex || seg.bottomtex || seg.toptex ? true : false;
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

   #ifdef R_PORTALS
   if(portalrender && !R_ClipSeg())
      return;
   #endif

   if(seg.clipsolid)
      R_ClipSolidWallSegment();
   else
      R_ClipPassWallSegment();
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
   
   // SoM: To account for the rounding error of the old BSP system, I needed to make adjustments.
   // SoM: Moved this to before the "does not cross a pixel" check to fix another slime trail
   if(sx1 > 0) sx1--;
   if(sx2 < viewwidth - 1) sx2++;

   // Does not cross a pixel.
   if(sx1 == sx2)
      return false;

   start = solidsegs;
   while(start->last < sx2)
      ++start;
   
   if(sx1 >= start->first && sx2 <= start->last)
      return false;      // The clippost contains the new span.
   
   return true;
}

#ifdef POLYOBJECTS

static int numpolys;        // number of polyobjects in current subsector
static int num_po_ptrs;     // number of polyobject pointers allocated
static polyobj_t **po_ptrs; // temp ptr array to sort polyobject pointers

//
// R_PolyobjCompare
//
// Callback for qsort that compares the z distance of two polyobjects.
// Returns the difference such that the closer polyobject will be
// sorted first.
//
static int R_PolyobjCompare(const void *p1, const void *p2)
{
   const polyobj_t *po1 = *(polyobj_t **)p1;
   const polyobj_t *po2 = *(polyobj_t **)p2;

   return po1->zdist - po2->zdist;
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
static void R_SortPolyObjects(polyobj_t *po)
{
   int i = 0;
         
   while(po)
   {
      po->zdist = R_PointToDist2(viewx, viewy, 
         po->centerPt.x, po->centerPt.y);
      po_ptrs[i++] = po;
      po = (polyobj_t *)(po->link.next);
   }
   
   // the polyobjects are NOT in any particular order, so use qsort
   qsort(po_ptrs, numpolys, sizeof(polyobj_t *), R_PolyobjCompare);
}

//
// R_AddPolyObjects
//
// haleyjd 02/19/06
// Adds all segs in all polyobjects in the given subsector.
//
static void R_AddPolyObjects(subsector_t *sub)
{
   polyobj_t *po = (polyobj_t *)(sub->polyList->link.next);
   int i, j;

   numpolys = 1; // we know there is at least one

   // count polyobjects
   while(po)
   {
      ++numpolys;
      po = (polyobj_t *)(po->link.next);
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

   // render polyobjects
   for(i = 0; i < numpolys; ++i)
   {
      for(j = 0; j < po_ptrs[i]->segCount; ++j)
         R_AddLine(po_ptrs[i]->segs[j]);
   }
}
#endif

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
   
#ifdef RANGECHECK
   if(num >= numsubsectors)
      I_Error("R_Subsector: ss %i with numss = %i", num, numsubsectors);
#endif

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

   // killough 3/7/98: Add (x,y) offsets to flats, add deep water check
   // killough 3/16/98: add floorlightlevel
   // killough 10/98: add support for skies transferred from sidedefs

   seg.floorplane = seg.frontsec->floorheight < viewz || // killough 3/7/98
     (seg.frontsec->heightsec != -1 &&
      (sectors[seg.frontsec->heightsec].ceilingpic == skyflatnum ||
       sectors[seg.frontsec->heightsec].ceilingpic == sky2flatnum)) ?
     R_FindPlane(seg.frontsec->floorheight, 
                 (seg.frontsec->floorpic == skyflatnum ||
                  seg.frontsec->floorpic == sky2flatnum) &&  // kilough 10/98
                 seg.frontsec->sky & PL_SKYFLAT ? seg.frontsec->sky :
                 seg.frontsec->floorpic,
                 floorlightlevel,                // killough 3/16/98
                 seg.frontsec->floor_xoffs,       // killough 3/7/98
                 seg.frontsec->floor_yoffs) : NULL;

   seg.ceilingplane = seg.frontsec->ceilingheight > viewz ||
     (seg.frontsec->ceilingpic == skyflatnum ||
      seg.frontsec->ceilingpic == sky2flatnum) ||
     (seg.frontsec->heightsec != -1 &&
      (sectors[seg.frontsec->heightsec].floorpic == skyflatnum ||
       sectors[seg.frontsec->heightsec].floorpic == sky2flatnum)) ?
     R_FindPlane(seg.frontsec->ceilingheight,     // killough 3/8/98
                 (seg.frontsec->ceilingpic == skyflatnum ||
                  seg.frontsec->ceilingpic == sky2flatnum) &&  // kilough 10/98
                 seg.frontsec->sky & PL_SKYFLAT ? seg.frontsec->sky :
                 seg.frontsec->ceilingpic,
                 ceilinglightlevel,              // killough 4/11/98
                 seg.frontsec->ceiling_xoffs,     // killough 3/7/98
                 seg.frontsec->ceiling_yoffs) : NULL;
  
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

#ifdef POLYOBJECTS
   // haleyjd 02/19/06: draw polyobjects before static lines
   // haleyjd 10/09/06: skip call entirely if no polyobjects
   if(sub->polyList)
      R_AddPolyObjects(sub);
#endif

   while(count--)
      R_AddLine(line++);
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
