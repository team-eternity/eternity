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
//      Movement/collision utility functions,
//      as used by function in p_map.c.
//      BLOCKMAP Iterator functions,
//      and some PIT_* functions to use for iteration.
//
//-----------------------------------------------------------------------------

#include <assert.h>
#include "z_zone.h"

#include "doomstat.h"
#include "e_exdata.h"
#include "ev_specials.h"
#include "m_bbox.h"
#include "m_compare.h"
#include "p_map.h"
#include "p_map3d.h"
#include "p_maputl.h"
#include "p_portalblockmap.h"
#include "p_portalcross.h"
#include "p_portalclip.h"
#include "p_setup.h"
#include "p_spec.h"
#include "polyobj.h"
#include "r_data.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_state.h"


//
// P_AproxDistance
// Gives an estimation of distance (not exact)
//
fixed_t P_AproxDistance(fixed_t dx, fixed_t dy)
{
   dx = D_abs(dx);
   dy = D_abs(dy);
   if(dx < dy)
      return dx+dy-(dx>>1);
   return dx+dy-(dy>>1);
}

//
// P_PointOnLineSide
// Returns 0 or 1
//
// killough 5/3/98: reformatted, cleaned up
// ioanch 20151228: made line const
//
int P_PointOnLineSideClassic(fixed_t x, fixed_t y, const line_t *line)
{
   return
      !line->dx ? x <= line->v1->x ? line->dy > 0 : line->dy < 0 :
      !line->dy ? y <= line->v1->y ? line->dx < 0 : line->dx > 0 :
      FixedMul(y-line->v1->y, line->dx>>FRACBITS) >=
      FixedMul(line->dy>>FRACBITS, x-line->v1->x);
}

int P_PointOnLineSidePrecise(fixed_t x, fixed_t y, const line_t *line)
{
   return
      !line->dx ? x <= line->v1->x ? line->dy > 0 : line->dy < 0 :
      !line->dy ? y <= line->v1->y ? line->dx < 0 : line->dx > 0 :
      ((int64_t)y - line->v1->y) * line->dx >= line->dy * ((int64_t)x - line->v1->x);
}
int (*P_PointOnLineSide)(fixed_t x, fixed_t y, const line_t *line) = P_PointOnLineSideClassic;

//
// Return 0 or 1, or -1 if flush
//
int P_PointOnLineSideExclusive(fixed_t x, fixed_t y, const line_t *line)
{
   if(!line->dx)
      return x <= line->v1->x ? line->dy > 0 : line->dy < 0;
   else if(!line->dy)
      return y <= line->v1->y ? line->dx < 0 : line->dx > 0;
   else
   {
      const int64_t yTerm = ((int64_t)y - line->v1->y) * line->dx;
      const int64_t xTerm = line->dy * ((int64_t)x - line->v1->x);
      if(xTerm == yTerm)
         return -1;
      else
         return xTerm >= yTerm ? 1 : 0;
   }
}

//
// P_BoxOnLineSide
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
// killough 5/3/98: reformatted, cleaned up
//
int P_BoxOnLineSide(const fixed_t *tmbox, const line_t *ld)
{
   int p;

   switch (ld->slopetype)
   {
   default: // shut up compiler warnings -- killough
   case ST_HORIZONTAL:
      return
      (tmbox[BOXBOTTOM] > ld->v1->y) == (p = tmbox[BOXTOP] > ld->v1->y) ?
        p ^ (ld->dx < 0) : -1;
   case ST_VERTICAL:
      return
        (tmbox[BOXLEFT] < ld->v1->x) == (p = tmbox[BOXRIGHT] < ld->v1->x) ?
        p ^ (ld->dy < 0) : -1;
   case ST_POSITIVE:
      return
        P_PointOnLineSide(tmbox[BOXRIGHT], tmbox[BOXBOTTOM], ld) ==
        (p = P_PointOnLineSide(tmbox[BOXLEFT], tmbox[BOXTOP], ld)) ? p : -1;
   case ST_NEGATIVE:
      return
        (P_PointOnLineSide(tmbox[BOXLEFT], tmbox[BOXBOTTOM], ld)) ==
        (p = P_PointOnLineSide(tmbox[BOXRIGHT], tmbox[BOXTOP], ld)) ? p : -1;
    }
}

//
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses (or is flush with) the line.
//
int P_BoxOnLineSideExclusive(const fixed_t *tmbox, const line_t *ld)
{
   switch(ld->slopetype)
   {
   default:
   case ST_HORIZONTAL:
   {
      const bool boxBottomAbove = tmbox[BOXBOTTOM] >= ld->v1->y;
      const bool boxTopAbove    = tmbox[BOXTOP] >= ld->v1->y;
      if(boxBottomAbove == boxTopAbove)
         return boxTopAbove != (ld->dx < 0) ? 1 : 0;
      else
         return -1;
   }
   case ST_VERTICAL:
   {
      const bool boxLeftLeft  = tmbox[BOXLEFT] <= ld->v1->x;
      const bool boxRightLeft = tmbox[BOXRIGHT] <= ld->v1->x;
      if(boxLeftLeft == boxRightLeft)
         return boxRightLeft != (ld->dy < 0) ? 1 : 0;
      else
         return -1;
   }
   case ST_POSITIVE:
   {
      const int boxBottomRightCorner = P_PointOnLineSideExclusive(tmbox[BOXRIGHT], tmbox[BOXBOTTOM], ld);
      const int boxTopLeftCorner     = P_PointOnLineSideExclusive(tmbox[BOXLEFT], tmbox[BOXTOP], ld);
      if(boxBottomRightCorner == -1)
         return boxTopLeftCorner;
      else if(boxTopLeftCorner == -1)
         return boxBottomRightCorner;
      else
         return boxBottomRightCorner == boxTopLeftCorner ? boxTopLeftCorner : -1;
   }
   case ST_NEGATIVE:
   {
      const int boxBottomLeftCorner = P_PointOnLineSideExclusive(tmbox[BOXLEFT], tmbox[BOXBOTTOM], ld);
      const int boxTopRightCorner   = P_PointOnLineSideExclusive(tmbox[BOXRIGHT], tmbox[BOXTOP], ld);
      if(boxBottomLeftCorner == -1)
         return boxTopRightCorner;
      else if(boxTopRightCorner == -1)
         return boxBottomLeftCorner;
      else
         return boxBottomLeftCorner == boxTopRightCorner ? boxTopRightCorner : -1;
   }
   }
}

//
// Assuming P_BoxOnLineSide returned -1 and the line is not outside the box, find the inter-
// section points. If any point is outside the line, then just use the endpoint of the line.
//
void P_ExactBoxLinePoints(const fixed_t *tmbox, const line_t &line, v2fixed_t &i1,
                          v2fixed_t &i2)
{
   //
   // Helper function to get the mid value between left and right depending on two ratios
   //
   auto findMiddle = [](fixed_t dstart, fixed_t dend, fixed_t min, fixed_t max)
   {
      if(dstart < dend) // safeguard against microscopic dstart or dend
         return max - FixedMul(max - min, FixedDiv(dend, dstart + dend));
      return min + FixedMul(max - min, FixedDiv(dstart, dstart + dend));
   };

   switch(line.slopetype)
   {
      case ST_HORIZONTAL:
         i1 = { emax(tmbox[BOXLEFT], line.bbox[BOXLEFT]), line.v1->y };
         i2 = { emin(tmbox[BOXRIGHT], line.bbox[BOXRIGHT]), line.v1->y };
         return;
      case ST_VERTICAL:
         i1 = { line.v1->x, emax(tmbox[BOXBOTTOM], line.bbox[BOXBOTTOM]) };
         i2 = { line.v1->x, emin(tmbox[BOXTOP], line.bbox[BOXTOP]) };
         return;
      default:
         break;
   }

   divline_t dl;
   v2fixed_t corners[2];
   if(line.slopetype == ST_POSITIVE)
   {
      dl = {{ line.bbox[BOXLEFT], line.bbox[BOXBOTTOM] },
         { line.bbox[BOXRIGHT] - line.bbox[BOXLEFT],
            line.bbox[BOXTOP] - line.bbox[BOXBOTTOM] }};
      corners[0] = { tmbox[BOXLEFT], tmbox[BOXBOTTOM] };
      corners[1] = { tmbox[BOXRIGHT], tmbox[BOXTOP] };
   }
   else
   {
      dl = {{ line.bbox[BOXLEFT], line.bbox[BOXTOP] },
         { line.bbox[BOXRIGHT] - line.bbox[BOXLEFT],
            line.bbox[BOXBOTTOM] - line.bbox[BOXTOP] }};
      corners[0] = { tmbox[BOXLEFT], tmbox[BOXTOP] };
      corners[1] = { tmbox[BOXRIGHT], tmbox[BOXBOTTOM] };
   }
   fixed_t dstart, dend;
   dstart = corners[0].x - dl.x;
   dend = dl.x + dl.dx - corners[0].x;
   i1.x = corners[0].x;
   i1.y = findMiddle(dstart, dend, dl.y, dl.y + dl.dy);
   if((dstart ^ dend) < 0 || i1.y < tmbox[BOXBOTTOM] || i1.y > tmbox[BOXTOP])
   {
      dstart = corners[0].y - dl.y;
      dend = dl.y + dl.dy - corners[0].y;
      i1.x = findMiddle(dstart, dend, dl.x, dl.x + dl.dx);
      i1.y = corners[0].y;
   }
   if(i1.x < line.bbox[BOXLEFT] || i1.y < line.bbox[BOXBOTTOM] || i1.y > line.bbox[BOXTOP])
      i1 = dl.v;

   dstart = corners[1].y - dl.y;
   dend = dl.y + dl.dy - corners[1].y;
   i2.x = findMiddle(dstart, dend, dl.x, dl.x + dl.dx);
   i2.y = corners[1].y;
   if(i1.x < tmbox[BOXLEFT] || i1.x > tmbox[BOXRIGHT])
   {
      dstart = corners[1].x - dl.x;
      dend = dl.x + dl.dx - corners[1].x;
      i2.x = corners[1].x;
      i2.y = findMiddle(dstart, dend, dl.y, dl.y + dl.dy);
   }
   if(i2.x > line.bbox[BOXRIGHT] || i2.y < line.bbox[BOXBOTTOM] || i2.y > line.bbox[BOXTOP])
      i2 = dl.v + dl.dv;
}

//
// Floating-point version
//
int P_BoxOnDivlineSideFloat(const float *box, v2float_t start, v2float_t delta)
{
   bool p;
   if(!delta.y)
      return (box[BOXBOTTOM] > start.y) == (p = box[BOXTOP] > start.y) ? p ^ (delta.x < 0) : -1;
   if(!delta.x)
      return (box[BOXLEFT] < start.x) == (p = box[BOXRIGHT] < start.x) ? p ^ (delta.y < 0) : -1;
   if(delta.x * delta.y >= 0)
   {
      v2float_t bottomRight = { box[BOXRIGHT], box[BOXBOTTOM] };
      v2float_t topLeft = { box[BOXLEFT], box[BOXTOP] };
      float prod = delta % (topLeft - start);
      float prod2 = delta % (bottomRight - start);
      return prod2 * prod >= 0 ? prod + prod2 >= 0 : -1;
   }
   v2float_t bottomLeft = { box[BOXLEFT], box[BOXBOTTOM] };
   v2float_t topRight = { box[BOXRIGHT], box[BOXTOP] };
   float prod = delta % (topRight - start);
   float prod2 = delta % (bottomLeft - start);
   return prod2 * prod >= 0 ? prod + prod2 >= 0 : -1;
}

enum outCode_e
{
   OC_INSIDE = 0x00000000,
   OC_LEFT   = 0x00000001,
   OC_RIGHT  = 0x00000002,
   OC_BOTTOM = 0x00000004,
   OC_TOP    = 0x00000008,
};

int GetVertexOutCode(const fixed_t *tmbox, const fixed_t x, const fixed_t y)
{
   int code = OC_INSIDE;
   if(x < tmbox[BOXLEFT])
      code |= OC_LEFT;
   else if(x > tmbox[BOXRIGHT])
      code |= OC_RIGHT;
   if(y < tmbox[BOXTOP])
      code |= OC_TOP;
   else if(y > tmbox[BOXBOTTOM])
      code |= OC_BOTTOM;

   return code;
}

//
// MaxW: Perform a Cohen Sutherland line clip to calculate if a line intersects a box
//
bool P_LineIntersectsBox(const line_t *ld, const fixed_t *tmboxin)
{
   // We need for top and left to be min and right and bottom to be max
   fixed_t tmbox[4];
   tmbox[BOXTOP]    = emin(tmboxin[BOXTOP], tmboxin[BOXBOTTOM]);
   tmbox[BOXBOTTOM] = emax(tmboxin[BOXTOP], tmboxin[BOXBOTTOM]);
   tmbox[BOXLEFT]   = emin(tmboxin[BOXLEFT], tmboxin[BOXRIGHT]);
   tmbox[BOXRIGHT]  = emax(tmboxin[BOXLEFT], tmboxin[BOXRIGHT]);


   int ocV1 = GetVertexOutCode(tmbox, ld->v1->x, ld->v1->y);
   int ocV2 = GetVertexOutCode(tmbox, ld->v2->x, ld->v2->y);

   // These values represent the two intersect points (left as-is if both points are inside the box)
   fixed_t x1 = ld->v1->x;
   fixed_t y1 = ld->v1->y;
   fixed_t x2 = ld->v2->x;
   fixed_t y2 = ld->v2->y;

   bool intersects = false;

   while(true) {
      if(!(ocV1 | ocV2))
      {
         intersects = true;
         break;
      }
      else if(ocV1 & ocV2)
         break;
      else
      {
         fixed_t x = 0, y = 0;
         const int outsideOutcode = emax(ocV2, ocV1);

         if(outsideOutcode & OC_TOP)
         {
            x = x1 + FixedDiv(FixedMul((x2 - x1), (tmbox[BOXTOP] - y1)), (y2 - y1));
            y = tmbox[BOXTOP];
         }
         else if(outsideOutcode & OC_BOTTOM)
         {
            x = x1 + FixedDiv(FixedMul((x2 - x1), (tmbox[BOXBOTTOM] - y1)), (y2 - y1));
            y = tmbox[BOXBOTTOM];
         }
         else if(outsideOutcode & OC_RIGHT)
         { 
            y = y1 + FixedDiv(FixedMul((y2 - y1), (tmbox[BOXRIGHT] - x1)),  (x2 - x1));
            x = tmbox[BOXRIGHT];
         }
         else if(outsideOutcode & OC_LEFT)
         {
            y = y1 + FixedDiv(FixedMul((y2 - y1), (tmbox[BOXLEFT] - x1)), (x2 - x1));
            x = tmbox[BOXLEFT];
         }

         if(outsideOutcode == ocV1) {
            x1 = x;
            y1 = y;
            ocV1 = GetVertexOutCode(tmbox, x1, y1);
         }
         else {
            x2 = x;
            y2 = y;
            ocV2 = GetVertexOutCode(tmbox, x2, y2);
         }
      }
   }

   return intersects;
}

//
// P_BoxLinePoint
//
// ioanch 20160116: returns a good point of intersection between the bounding
// box diagonals and linedef. This assumes P_BoxOnLineSide returned -1.
//
v2fixed_t P_BoxLinePoint(const fixed_t bbox[4], const line_t *ld)
{
   v2fixed_t ret;
   switch(ld->slopetype)
   {
   default:
      ret.x = ret.y = 0;   // just so no warnings ari
      break;
   case ST_HORIZONTAL:
      ret.x = bbox[BOXLEFT] / 2 + bbox[BOXRIGHT] / 2;
      ret.y = ld->v1->y;
      break;
   case ST_VERTICAL:
      ret.x = ld->v1->x;
      ret.y = bbox[BOXBOTTOM] / 2 + bbox[BOXTOP] / 2;
      break;
   case ST_POSITIVE:
      {
         divline_t d1 = { bbox[BOXLEFT], bbox[BOXTOP], 
            bbox[BOXRIGHT] - bbox[BOXLEFT],
            bbox[BOXBOTTOM] - bbox[BOXTOP] };
         divline_t d2 = { ld->v1->x, ld->v1->y, ld->dx, ld->dy };
         fixed_t frac = P_InterceptVector(&d1, &d2);
         ret.x = d1.x + FixedMul(d1.dx, frac);
         ret.y = d1.y + FixedMul(d1.dy, frac);
      }
      break;
   case ST_NEGATIVE:
      {
         divline_t d1 = { bbox[BOXLEFT], bbox[BOXBOTTOM], 
            bbox[BOXRIGHT] - bbox[BOXLEFT],
            bbox[BOXTOP] - bbox[BOXBOTTOM] };
         divline_t d2 = { ld->v1->x, ld->v1->y, ld->dx, ld->dy };
         fixed_t frac = P_InterceptVector(&d1, &d2);
         ret.x = d1.x + FixedMul(d1.dx, frac);
         ret.y = d1.y + FixedMul(d1.dy, frac);
      }
      break;
   }
   return ret;
}

//
// Utility function which returns true if the divline dl crosses line
// Returns -1 if there's no crossing, or the starting point's 0 or 1 side.
//
int P_LineIsCrossed(const line_t &line, const divline_t &dl)
{
   int a;
   return (a = P_PointOnLineSide(dl.x, dl.y, &line)) !=
   P_PointOnLineSide(dl.x + dl.dx, dl.y + dl.dy, &line) &&
   P_PointOnDivlineSidePrecise(line.v1->x, line.v1->y, &dl) !=
   P_PointOnDivlineSidePrecise(line.v1->x + line.dx, line.v1->y + line.dy, &dl) ? a : -1;
}

//
// Checks if a point is behind a subsector's 1-sided seg
//
bool P_IsInVoid(fixed_t x, fixed_t y, const subsector_t &ss)
{
   for(int i = 0; i < ss.numlines; ++i)
   {
      const seg_t &seg = segs[ss.firstline + i];
      if(seg.backsector)
         continue;
      if(P_PointOnLineSide(x, y, seg.linedef))
         return true;
   }
   return false;
}

//
// Returns true if two bounding boxes intersect. Assumes they're correctly set.
//
bool P_BoxesIntersect(const fixed_t bbox1[4], const fixed_t bbox2[4])
{
   return bbox1[BOXLEFT] < bbox2[BOXRIGHT] &&
   bbox1[BOXRIGHT] > bbox2[BOXLEFT] && bbox1[BOXBOTTOM] < bbox2[BOXTOP] &&
   bbox1[BOXTOP] > bbox2[BOXBOTTOM];
}

//
// P_PointOnDivlineSide
// Returns 0 or 1.
//
// killough 5/3/98: reformatted, cleaned up
//
int P_PointOnDivlineSideClassic(fixed_t x, fixed_t y, const divline_t *line)
{
   return
      !line->dx ? x <= line->x ? line->dy > 0 : line->dy < 0 :
      !line->dy ? y <= line->y ? line->dx < 0 : line->dx > 0 :
      (line->dy^line->dx^(x -= line->x)^(y -= line->y)) < 0 ? (line->dy^x) < 0 :
      FixedMul(y>>8, line->dx>>8) >= FixedMul(line->dy>>8, x>>8);
}
int P_PointOnDivlineSidePrecise(fixed_t x, fixed_t y, const divline_t *line)
{
   return
      !line->dx ? x <= line->x ? line->dy > 0 : line->dy < 0 :
      !line->dy ? y <= line->y ? line->dx < 0 : line->dx > 0 :
      (line->dy ^ line->dx ^ (x -= line->x) ^ (y -= line->y)) < 0 ? (line->dy ^ x) < 0 :
      static_cast<int64_t>(y) * line->dx >= static_cast<int64_t>(line->dy) * x;
}
int (*P_PointOnDivlineSide)(fixed_t x, fixed_t y, const divline_t *line) = 
      P_PointOnDivlineSideClassic;

//
// P_MakeDivline
//
// ioanch 20151230: made const
//
void P_MakeDivline(const line_t *li, divline_t *dl)
{
   dl->x  = li->v1->x;
   dl->y  = li->v1->y;
   dl->dx = li->dx;
   dl->dy = li->dy;
}

//
// P_InterceptVector
// Returns the fractional intercept point
// along the first divline.
// This is only called by the addthings
// and addlines traversers.
//
// killough 5/3/98: reformatted, cleaned up
// ioanch 20151229: added const
//
fixed_t P_InterceptVector(const divline_t *v2, const divline_t *v1)
{
   if(demo_version < 403)
   {
      const fixed_t den = FixedMul(v1->dy>>8, v2->dx) - FixedMul(v1->dx>>8, v2->dy);
      return den ? FixedDiv((FixedMul((v1->x-v2->x)>>8, v1->dy) +
                             FixedMul((v2->y-v1->y)>>8, v1->dx)), den) : 0;
   }
   else
   {
      // This is from PRBoom+ by Colin Phipps , GPL 2
      /* cph - This was introduced at prboom_4_compatibility - no precision/overflow problems */
      int64_t den = int64_t(v1->dy) * v2->dx - int64_t(v1->dx) * v2->dy;
      den >>= 16;
      if(!den)
         return 0;
      return fixed_t((int64_t(v1->x - v2->x) * v1->dy - int64_t(v1->y - v2->y) * v1->dx) / den);
   }
}

//
// Get opening on a point, against slopes
//
lineopening_t P_SlopeOpening(v2fixed_t pos)
{
   const sector_t &sector = *R_PointInSubsector(pos.x, pos.y)->sector;
   lineopening_t open = {};
   open.height.floor = sector.srf.floor.getZAt(pos);
   open.height.ceiling = sector.srf.ceiling.getZAt(pos);
   open.sec = open.height; // do not consider 3dmidtex here
   open.range = open.height.ceiling - open.height.floor;
   open.bottomgroupid = sector.groupid;
   open.lowfloor = open.height.floor;
   open.floorpic = sector.srf.floor.pic;
   open.floorsector = &sector;
   open.ceilsector = &sector;
   open.touch3dside = 0;
   return open;
}

lineopening_t P_SlopeOpeningPortalAware(v2fixed_t pos)
{
   v2fixed_t floorpos;
   const sector_t *floorsector = P_ExtremeSectorAtPoint(pos.x, pos.y, surf_floor, nullptr, &floorpos);
   floorpos += pos;
   v2fixed_t ceilpos;
   const sector_t *ceilsector = P_ExtremeSectorAtPoint(pos.x, pos.y, surf_ceil, nullptr, &ceilpos);
   ceilpos += pos;
   lineopening_t open = {};
   open.height.floor = floorsector->srf.floor.getZAt(floorpos);
   open.height.ceiling = ceilsector->srf.ceiling.getZAt(ceilpos);
   open.sec = open.height;
   open.range = open.height.ceiling - open.height.floor;
   open.bottomgroupid = floorsector->groupid;
   open.lowfloor = open.height.floor;
   open.floorpic = floorsector->srf.floor.pic;
   open.floorsector = floorsector;
   open.ceilsector = ceilsector;
   open.touch3dside = 0;
   return open;
}

//
// P_LineOpening
//
// Sets opentop and openbottom to the window
// through a two sided line.
// OPTIMIZE: keep this precalculated
// ioanch 20160113: added portal detection (optional)
//
lineopening_t P_LineOpening(const line_t *linedef, const Mobj *mo, const v2fixed_t *ppoint,
                            bool portaldetect, uint32_t *lineclipflags)
{
   // keep track of lineopening_t from "clip", for compatibility's sake
   lineopening_t open = clip.open;

   // NOTE: default to line center if ppoint unspecified
   v2fixed_t point = ppoint ? *ppoint : v2fixed_t(linedef->soundorg.x, linedef->soundorg.y);

   fixed_t frontceilz, frontfloorz, backceilz, backfloorz;
   int frontfloorgroupid, backfloorgroupid;
   // SoM: used for 3dmidtex
   fixed_t frontcz, frontfz, backcz, backfz, otop, obot;

   if(linedef->sidenum[1] == -1)      // single sided line
   {
      open.range = 0;
      return open;
   }

   if(portaldetect)  // ioanch
   {
      *lineclipflags = 0;
   }
   const sector_t *openfrontsector = linedef->frontsector;
   const sector_t *openbacksector = linedef->backsector;
   sector_t *beyond = linedef->intflags & MLI_1SPORTALLINE && linedef->beyondportalline ?
      linedef->beyondportalline->frontsector : nullptr;
   if(beyond)
      openbacksector = beyond;

   // SoM: ok, new plan. The only way a 2s line should give a lowered floor or hightened ceiling
   // z is if both sides of that line have the same portal.
   const surface_t &frontceiling = openfrontsector->srf.ceiling;
   const surface_t &backceiling = openbacksector->srf.ceiling;
   {
      if(mo && demo_version >= 333 &&
         ((frontceiling.pflags & PS_PASSABLE && backceiling.pflags & PS_PASSABLE &&
           frontceiling.portal == backceiling.portal) ||
          (frontceiling.pflags & PS_PASSABLE && linedef->pflags & PS_PASSABLE &&
           frontceiling.portal->data.link.delta == linedef->portal->data.link.delta)))
      {
         // also handle line portal + ceiling portal, for edge portals
         if(!portaldetect) // ioanch
            frontceilz = backceilz = frontceiling.getZAt(point) + 1024 * FRACUNIT;
         else
         {
            *lineclipflags |= LINECLIP_UNDERPORTAL;
            frontceilz = frontceiling.getZAt(point);
            backceilz  = backceiling.getZAt(point);
         }
      }
      else
      {
         frontceilz = frontceiling.getZAt(point);
         backceilz  = backceiling.getZAt(point);
      }

      frontcz = frontceiling.getZAt(point);
      backcz  = backceiling.getZAt(point);
   }

   const surface_t &frontfloor = openfrontsector->srf.floor;
   const surface_t &backfloor = openbacksector->srf.floor;
   {
      if(mo && demo_version >= 333 &&
         ((frontfloor.pflags & PS_PASSABLE && backfloor.pflags & PS_PASSABLE &&
           frontfloor.portal == backfloor.portal) ||
          (frontfloor.pflags & PS_PASSABLE && linedef->pflags & PS_PASSABLE &&
           frontfloor.portal->data.link.delta == linedef->portal->data.link.delta)))
      {
         if(!portaldetect)  // ioanch
         {
            frontfloorz = backfloorz = frontfloor.getZAt(point) - 1024 * FRACUNIT; //mo->height;
            // Not exactly "extreme"
            frontfloorgroupid = backfloorgroupid = frontfloor.portal->data.link.toid;
         }
         else
         {
            *lineclipflags |= LINECLIP_ABOVEPORTAL;
            frontfloorz = frontfloor.getZAt(point);
            frontfloorgroupid = openfrontsector->groupid;
            backfloorz  = backfloor.getZAt(point);
            backfloorgroupid = openbacksector->groupid;
         }
      }
      else
      {
         frontfloorz = frontfloor.getZAt(point);
         frontfloorgroupid = openfrontsector->groupid;
         backfloorz  = backfloor.getZAt(point);
         backfloorgroupid = openbacksector->groupid;
      }

      frontfz = frontfloor.getZAt(point);
      backfz = backfloor.getZAt(point);
   }

   if(linedef->extflags & EX_ML_UPPERPORTAL && backceiling.pflags & PS_PASSABLE)
   {
      open.height.ceiling = frontceilz;
      open.ceilsector = openfrontsector;
   }
   else if(frontceilz < backceilz)
   {
      open.height.ceiling = frontceilz;
      open.ceilsector = openfrontsector;
   }
   else
   {
      open.height.ceiling = backceilz;
      open.ceilsector = openbacksector;
   }

   // ioanch 20160114: don't change floorpic if portaldetect is on
   if(linedef->extflags & EX_ML_LOWERPORTAL && backfloor.pflags & PS_PASSABLE)
   {
      open.height.floor = frontfloorz;
      open.bottomgroupid = frontfloorgroupid;
      open.lowfloor = frontfloorz;
      open.floorsector = openfrontsector;
      if(!portaldetect || !(frontfloor.pflags & PS_PASSABLE))
         open.floorpic = frontfloor.pic;
   }
   else if(frontfloorz > backfloorz)
   {
      open.height.floor = frontfloorz;
      open.bottomgroupid = frontfloorgroupid;
      open.lowfloor = backfloorz;
      open.floorsector = openfrontsector;
      // haleyjd
      if(!portaldetect || !(frontfloor.pflags & PS_PASSABLE))
         open.floorpic = frontfloor.pic;
   }
   else
   {
      open.height.floor = backfloorz;
      open.bottomgroupid = backfloorgroupid;
      open.lowfloor = frontfloorz;
      open.floorsector = openbacksector;
      // haleyjd
      if(!portaldetect || !(backfloor.pflags & PS_PASSABLE))
         open.floorpic = backfloor.pic;
   }

   if(frontcz < backcz)
      otop = frontcz;
   else
      otop = backcz;

   if(frontfz > backfz)
      obot = frontfz;
   else
      obot = backfz;

   open.sec = open.height;

   // SoM 9/02/02: Um... I know I told Quasar` I would do this after 
   // I got SDL_Mixer support and all, but I WANT THIS NOW hehe
   if(demo_version >= 331 && mo && (linedef->flags & ML_3DMIDTEX) && 
      sides[linedef->sidenum[0]].midtexture && (!(linedef->extflags & EX_ML_3DMTPASSPROJ) ||
                                                !(mo->flags & (MF_MISSILE | MF_BOUNCES))))
   {
      // ioanch: also support midtex3dimpassible

      fixed_t textop, texbot, texmid;
      side_t *side = &sides[linedef->sidenum[0]];
      
      if(linedef->flags & ML_DONTPEGBOTTOM)
      {
         texbot = side->offset_base_y + side->offset_mid_y + obot;
         textop = texbot + textures[side->midtexture]->heightfrac;
      }
      else
      {
         textop = otop + side->offset_base_y + side->offset_mid_y;
         texbot = textop - textures[side->midtexture]->heightfrac;
      }
      texmid = (textop + texbot)/2;

      // SoM 9/7/02: use monster blocking line to provide better
      // clipping
      if((linedef->flags & ML_BLOCKMONSTERS) && 
         !(mo->flags & (MF_FLOAT | MF_DROPOFF)) &&
         D_abs(mo->z - textop) <= STEPSIZE)
      {
         open.height.ceiling = open.height.floor;
         open.range = 0;
         return open;
      }
      
      if(mo->z + (P_ThingInfoHeight(mo->info) / 2) < texmid)
      {
         if(texbot < open.height.ceiling)
         {
            open.height.ceiling = texbot;
            open.ceilsector = nullptr; // not under a slope now
         }
         // ioanch 20160318: mark if 3dmidtex affects clipping
         // Also don't flag lines that are offset into the floor/ceiling
         if(portaldetect && (texbot < frontceiling.getZAt(point) || texbot < backceiling.getZAt(point)))
            *lineclipflags |= LINECLIP_UNDER3DMIDTEX;
      }
      else
      {
         if(textop > open.height.floor)
         {
            open.height.floor = textop;
            open.bottomgroupid = linedef->frontsector->groupid;
            open.floorsector = nullptr;   // not on a slope now
         }
         // ioanch 20160318: mark if 3dmidtex affects clipping
         // Also don't flag lines that are offset into the floor/ceiling
         if(portaldetect && (textop > frontfloor.getZAt(point) || textop > backfloor.getZAt(point)))
            *lineclipflags |= LINECLIP_OVER3DMIDTEX;

         // The mobj is above the 3DMidTex, so check to see if it's ON the 3DMidTex
         // SoM 01/12/06: let monsters walk over dropoffs
         if(abs(mo->z - textop) <= STEPSIZE)
            open.touch3dside = 1;
      }
   }

   open.range = open.height.ceiling - open.height.floor;
   return open;
}

//
// Reduces this opening by the other, raising the floor and lowering the ceiling a necessary.
//
void lineopening_t::intersect(const lineopening_t &other)
{
   if(other.height.floor > height.floor)
   {
      height.floor = other.height.floor;
      bottomgroupid = other.bottomgroupid;
      floorpic = other.floorpic;
      touch3dside = other.touch3dside;
      floorsector = other.floorsector;
   }
   if(other.height.ceiling < height.ceiling)
   {
      height.ceiling = other.height.ceiling;
      ceilsector = other.ceilsector;
   }
   range = height.ceiling - height.floor;

   if(other.sec.floor > sec.floor)
      sec.floor = other.sec.floor;
   if(other.sec.ceiling < sec.ceiling)
      sec.ceiling = other.sec.ceiling;

   if(other.lowfloor < lowfloor)
      lowfloor = other.lowfloor;
}

//
// THING POSITION SETTING
//

//
// P_LogThingPosition
//
// haleyjd 04/15/2010: thing position logging for debugging demo problems.
// Pass a nullptr mobj to close the log.
//
//#define THING_LOGGING
#ifdef THING_LOGGING
void P_LogThingPosition(Mobj *mo, const char *caller)
{
   static FILE *thinglog;

   if(!thinglog)
      thinglog = fopen("thinglog.txt", "wt");

   if(!mo)
   {
      if(thinglog)
         fclose(thinglog);
      thinglog = nullptr;
      return;
   }

   if(thinglog)
   {
      fprintf(thinglog,
              "%010d:%s::%+010d:(%g:%g:%g):(%g:%g:%g):%08x\n",
              gametic, caller, (int)(mo->info->dehnum-1), mo->x / 65536., mo->y / 65536., mo->z / 65536.,
                           mo->momx / 65536., mo->momy / 65536., mo->momz / 65536., mo->flags & 0x7fffffff);
   }
}
#else
#define P_LogThingPosition(a, b)
#endif

//
// P_UnsetThingPosition
// Unlinks a thing from block map and sectors.
// On each position change, BLOCKMAP and other
// lookups maintaining lists ot things inside
// these structures need to be updated.
//
void P_UnsetThingPosition(Mobj *thing)
{
   P_LogThingPosition(thing, "unset");

   if(!(thing->flags & MF_NOSECTOR))
   {
      // invisible things don't need to be in sector list
      // unlink from subsector
      
      // killough 8/11/98: simpler scheme using pointers-to-pointers for prev
      // pointers, allows head node pointers to be treated like everything else
      Mobj **sprev = thing->sprev;
      Mobj  *snext = thing->snext;
      if((*sprev = snext))  // unlink from sector list
         snext->sprev = sprev;

      // phares 3/14/98
      //
      // Save the sector list pointed to by touching_sectorlist.
      // In P_SetThingPosition, we'll keep any nodes that represent
      // sectors the Thing still touches. We'll add new ones then, and
      // delete any nodes for sectors the Thing has vacated. Then we'll
      // put it back into touching_sectorlist. It's done this way to
      // avoid a lot of deleting/creating for nodes, when most of the
      // time you just get back what you deleted anyway.
      //
      // If this Thing is being removed entirely, then the calling
      // routine will clear out the nodes in sector_list.
      
      thing->old_sectorlist = thing->touching_sectorlist;
      thing->touching_sectorlist = nullptr; // to be restored by P_SetThingPosition
      
      R_UnlinkSpriteProj(*thing);
   }

   if(!(thing->flags & MF_NOBLOCKMAP))
   {
      // inert things don't need to be in blockmap
      
      // killough 8/11/98: simpler scheme using pointers-to-pointers for prev
      // pointers, allows head node pointers to be treated like everything else
      //
      // Also more robust, since it doesn't depend on current position for
      // unlinking. Old method required computing head node based on position
      // at time of unlinking, assuming it was the same position as during
      // linking.
      
      Mobj *bnext, **bprev = thing->bprev;
      if(bprev && (*bprev = bnext = thing->bnext))  // unlink from block map
         bnext->bprev = bprev;
   }
}

//
// P_SetThingPosition
// Links a thing into both a block and a subsector
// based on it's x y.
// Sets thing->subsector properly
//
// killough 5/3/98: reformatted, cleaned up
//
void P_SetThingPosition(Mobj *thing)
{
   subsector_t *prevss = thing->subsector;
   // link into subsector
   subsector_t *ss = thing->subsector = R_PointInSubsector(thing->x, thing->y);

   P_LogThingPosition(thing, " set ");

   thing->groupid = ss->sector->groupid;

   if(!(thing->flags & MF_NOSECTOR))
   {
      // invisible things don't go into the sector links
      
      // killough 8/11/98: simpler scheme using pointer-to-pointer prev
      // pointers, allows head nodes to be treated like everything else
      
      Mobj **link = &ss->sector->thinglist;
      Mobj *snext = *link;
      if((thing->snext = snext))
         snext->sprev = &thing->snext;
      thing->sprev = link;
      *link = thing;

      // phares 3/16/98
      //
      // If sector_list isn't nullptr, it has a collection of sector
      // nodes that were just removed from this Thing.
      //
      // Collect the sectors the object will live in by looking at
      // the existing sector_list and adding new nodes and deleting
      // obsolete ones.
      //
      // When a node is deleted, its sector links (the links starting
      // at sector_t->touching_thinglist) are broken. When a node is
      // added, new sector links are created.

      thing->touching_sectorlist = P_CreateSecNodeList(thing, thing->x, thing->y);
      thing->old_sectorlist = nullptr;

      // MaxW: EESectorActionEnter and EESectorActionExit
      if(prevss && prevss->sector != ss->sector)
      {
         EV_ActivateSectorAction(ss->sector, thing, SEAC_ENTER);
         EV_ActivateSectorAction(prevss->sector, thing, SEAC_EXIT);
      }
      
      // ioanch: link to portals
      R_LinkSpriteProj(*thing);
   }

   // link into blockmap
   if(!(thing->flags & MF_NOBLOCKMAP))
   {
      // inert things don't need to be in blockmap
      int blockx = (thing->x - bmaporgx) >> MAPBLOCKSHIFT;
      int blocky = (thing->y - bmaporgy) >> MAPBLOCKSHIFT;
      
      if(blockx >= 0 && blockx < bmapwidth && blocky >= 0 && blocky < bmapheight)
      {
         // killough 8/11/98: simpler scheme using pointer-to-pointer prev
         // pointers, allows head nodes to be treated like everything else

         Mobj **link = &blocklinks[blocky*bmapwidth+blockx];
         Mobj *bnext = *link;
         if((thing->bnext = bnext))
            bnext->bprev = &thing->bnext;
         thing->bprev = link;
         *link = thing;
      }
      else        // thing is off the map
      {
         thing->bnext = nullptr;
         thing->bprev = nullptr;
      }
   }
}

// killough 3/15/98:
//
// A fast function for testing intersections between things and linedefs.
//
// haleyjd: note -- this is never called, and is, according to
// SoM, VERY inaccurate. I don't really know what its for or why
// its here, but I'm leaving it be.
//
bool ThingIsOnLine(const Mobj *t, const line_t *l)
{
   int dx = l->dx >> FRACBITS;                           // Linedef vector
   int dy = l->dy >> FRACBITS;
   int a = (l->v1->x >> FRACBITS) - (t->x >> FRACBITS);  // Thing-->v1 vector
   int b = (l->v1->y >> FRACBITS) - (t->y >> FRACBITS);
   int r = t->radius >> FRACBITS;                        // Thing radius

   // First make sure bounding boxes of linedef and thing intersect.
   // Leads to quick rejection using only shifts and adds/subs/compares.
   
   if(D_abs(a*2+dx)-D_abs(dx) > r*2 || D_abs(b*2+dy)-D_abs(dy) > r*2)
      return 0;

   // Next, make sure that at least one thing crosshair intersects linedef's
   // extension. Requires only 3-4 multiplications, the rest adds/subs/
   // shifts/xors (writing the steps out this way leads to better codegen).

   a *= dy;
   b *= dx;
   a -= b;
   b = dx + dy;
   b *= r;
   if(((a-b)^(a+b)) < 0)
      return 1;
   dy -= dx;
   dy *= r;
   b = a+dy;
   a -= dy;
   return (a^b) < 0;
}

//
// BLOCK MAP ITERATORS
// For each line/thing in the given mapblock,
// call the passed PIT_* function.
// If the function returns false,
// exit with false without checking anything else.
//

//
// P_BlockLinesIterator
// The validcount flags are used to avoid checking lines
// that are marked in multiple mapblocks,
// so increment validcount before the first call
// to P_BlockLinesIterator, then make one or more calls
// to it.
//
// killough 5/3/98: reformatted, cleaned up
// ioanch 20160111: added groupid
// ioanch 20160114: enhanced the callback
//
bool P_BlockLinesIterator(int x, int y, bool func(line_t*, polyobj_t*, void *), int groupid,
   void *context)
{
   int        offset;
   const int  *list;     // killough 3/1/98: for removal of blockmap limit
   DLListItem<polymaplink_t> *plink; // haleyjd 02/22/06
   
   if(x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight)
      return true;
   offset = y * bmapwidth + x;

   // haleyjd 02/22/06: consider polyobject lines
   plink = polyblocklinks[offset];

   while(plink)
   {
      polyobj_t *po = (*plink)->po;

      if(po->validcount != validcount) // if polyobj hasn't been checked
      {
         int i;
         po->validcount = validcount;
         
         for(i = 0; i < po->numLines; ++i)
         {
            if(po->lines[i]->validcount == validcount) // line has been checked
               continue;
            po->lines[i]->validcount = validcount;
            if(!func(po->lines[i], po, context))
               return false;
         }
      }
      plink = plink->dllNext;
   }

   // original was reading delimiting 0 as linedef 0 -- phares
   offset = *(blockmap + offset);
   list = blockmaplump + offset;

   // MaxW: 2016/02/02: This skip isn't feasible to do for recent play,
   // as it has been found that the starting delimiter can have a use.

   // killough 1/31/98: for compatibility we need to use the old method.
   // Most demos go out of sync, and maybe other problems happen, if we
   // don't consider linedef 0. For safety this should be qualified.

   // MaxW: 2016/02/02: if before 3.42 always skip, skip if all blocklists start w/ 0
   // killough 2/22/98: demo_compatibility check
   // skip 0 starting delimiter -- phares
   if((!demo_compatibility && demo_version < 342) || (demo_version >= 342 && skipblstart))
      list++;     
   for( ; *list != -1; list++)
   {
      line_t *ld;
      
      // haleyjd 04/06/10: to avoid some crashes during demo playback due to
      // invalid blockmap lumps
      if(*list >= numlines)
         continue;

      ld = &lines[*list];
      // ioanch 20160111: check groupid
      if(groupid != R_NOGROUP && groupid != ld->frontsector->groupid)
         continue;
      if(ld->validcount == validcount)
         continue;       // line has already been checked
      ld->validcount = validcount;
      if(!func(ld, nullptr, context))
         return false;
   }
   return true;  // everything was checked
}

//
// P_BlockThingsIterator
//
// killough 5/3/98: reformatted, cleaned up
// ioanch 20160108: variant with groupid
//
bool P_BlockThingsIterator(int x, int y, int groupid, bool (*func)(Mobj *, void *),
                           void *context)
{
   if(!(x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight))
   {
      Mobj *mobj = blocklinks[y * bmapwidth + x];

      for(; mobj; mobj = mobj->bnext)
      {
         // ioanch: if mismatching group id (in case it's declared), skip
         if(groupid != R_NOGROUP && mobj->groupid != R_NOGROUP && 
            groupid != mobj->groupid)
         {
            continue;   // ignore objects from wrong groupid
         }
         if(!func(mobj, context))
            return false;
      }
   }
   return true;
}

//
// P_PointToAngle
//
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table. The +1 size of tantoangle[]
//  is to handle the case when x==y without additional
//  checking.
//
// killough 5/2/98: reformatted, cleaned up
// haleyjd 01/28/10: restored to Vanilla and made some modifications;
//                   added P_ version for use by gamecode.
//
angle_t P_PointToAngle(fixed_t xo, fixed_t yo, fixed_t x, fixed_t y)
{	
   x -= xo;
   y -= yo;

   if((x | y) == 0)
      return 0;

   if(x >= 0)
   {
      if (y >= 0)
      {
         if(x > y)
         {
            // octant 0
            return tantoangle[SlopeDiv(y, x)];
         }
         else
         {
            // octant 1
            return ANG90 - 1 - tantoangle[SlopeDiv(x, y)];
         }
      }
      else
      {
         y = -y;

         if(x > y)
         {
            // octant 8
            return 0 - tantoangle[SlopeDiv(y, x)];
         }
         else
         {
            // octant 7
            return ANG270 + tantoangle[SlopeDiv(x, y)];
         }
      }
   }
   else
   {
      x = -x;

      if(y >= 0)
      {
         if(x > y)
         {
            // octant 3
            return ANG180 - 1 - tantoangle[SlopeDiv(y, x)];
         }
         else
         {
            // octant 2
            return ANG90 + tantoangle[SlopeDiv(x, y)];
         }
      }
      else
      {
         y = -y;

         if(x > y)
         {
            // octant 4
            return ANG180 + tantoangle[SlopeDiv(y, x)];
         }
         else
         {
            // octant 5
            return ANG270 - 1 - tantoangle[SlopeDiv(x, y)];
         }
      }
   }

   return 0;
}

//
// [XA] 02/29/20:
//
// double --> angle conversion, mainly for EDF usage.
// presumes 'a' is in degrees, like the rest of
// the various to-angle conversion functions. Negative
// angles are supported.
//
angle_t P_DoubleToAngle(double a)
{
   // normalize the angle to [0, 360)
   a = fmod(a, 360.0);
   if(a < 0)
      a += 360.0;

   // convert dat shit
   return FixedToAngle(M_DoubleToFixed(a));
}

//
// [XA] 02/29/20:
//
// Rotates a point by the specified angle. 'Nuff said.
//
void P_RotatePoint(fixed_t &x, fixed_t &y, const angle_t angle)
{
   fixed_t tmp;
   fixed_t sin = finesine[angle >> ANGLETOFINESHIFT];
   fixed_t cos = finecosine[angle >> ANGLETOFINESHIFT];
   tmp = FixedMul(x, cos) - FixedMul(y, sin);
   y = FixedMul(x, sin) + FixedMul(y, cos);
   x = tmp;
}

//
// Gets a playsim-safe line normal (nx, ny are floating point so they're not safe)
//
v2fixed_t P_GetSafeLineNormal(const line_t &line)
{
   fixed_t len = P_AproxDistance(line.dx, line.dy);
   return { FixedDiv(line.dy, len), -FixedDiv(line.dx, len) };
}

//
// True if two segments strictly intersect, without the point being on top of each segment.         
// This uses an epsilon of 1/256.
// 
// Arguments are intentionally copied
//
static bool P_segmentsStrictlyIntersect(divline_t dl1, divline_t dl2)
{
   if(!dl1.dv || !dl2.dv)
      return false;  // no degenerate lines allowed

   // Block any colinear or parallel lines
   if((!dl1.dx && !dl2.dx) || (!dl1.dy && !dl2.dy))
      return false;

   // Now we're left with concurrent lines.

   fixed_t len1 = P_AproxDistance(dl1.dv);
   fixed_t len2 = P_AproxDistance(dl2.dv);
   v2fixed_t nudge1 = dl1.dv.fixedDiv(len1) / (1 << (FRACBITS - 8));
   v2fixed_t nudge2 = dl2.dv.fixedDiv(len2) / (1 << (FRACBITS - 8));

   // Now reduce the lines to make sure they don't intersect if barely touching
   dl1.v += nudge1;
   dl1.dv -= nudge1 * 2;
   dl2.v += nudge2;
   dl2.dv -= nudge2 * 2;

   int side1 = P_PointOnDivlineSidePrecise(dl1.x, dl1.y, &dl2);
   int side2 = P_PointOnDivlineSidePrecise(dl1.x + dl1.dx, dl1.y + dl1.dy, &dl2);
   if(side1 == side2)
      return false;

   side1 = P_PointOnDivlineSidePrecise(dl2.x, dl2.y, &dl1);
   side2 = P_PointOnDivlineSidePrecise(dl2.x + dl2.dx, dl2.y + dl2.dy, &dl1);
   if(side1 != side2)
   {
      // Possibly intersecting. But they may still be overlapping diagonal lines, so let's nudge one
      // a bit and see if it still intersects. Pick the longer line to nudge and measure the shorter
      // one against that.

      divline_t *tonudge;
      const divline_t *tocheck;
      if(len2 > len1)
      {
         tonudge = &dl2;
         nudge1 = { nudge2.y, -nudge2.x };
         tocheck = &dl1;
      }
      else
      {
         tonudge = &dl1;
         nudge1 = { nudge1.y, -nudge1.x };
         tocheck = &dl2;
      }

      tonudge->v += nudge1;
      
      side1 = P_PointOnDivlineSidePrecise(tocheck->x, tocheck->y, tonudge);
      side2 = P_PointOnDivlineSidePrecise(tocheck->x + tocheck->dx, tocheck->y + tocheck->dy, 
                                          tonudge);
      return side1 != side2;
   }
   return false;
}

//
// True if the line described by points v1 and v2 intersects one of the lines of the sector
//
bool P_SegmentIntersectsSector(v2fixed_t v1, v2fixed_t v2, const sector_t &sector)
{
   // Make a divline out of the points
   divline_t dl;
   dl.v = v1;
   dl.dv = v2 - v1;

   for(int i = 0; i < sector.linecount; ++i)
   {
      // Check our line against sector's line
      assert(sector.lines[i]);
      const line_t &line = *sector.lines[i];
      if(!line.dx && !line.dy)
         continue;
      // We need to send the extremities into the linedef to prevent edge uncertainties
      divline_t sectordl;
      P_MakeDivline(&line, &sectordl);

      if(P_segmentsStrictlyIntersect(dl, sectordl))
         return true;   // found one
   }
   return false;
}

//----------------------------------------------------------------------------
//
// $Log: p_maputl.c,v $
// Revision 1.13  1998/05/03  22:16:48  killough
// beautification
//
// Revision 1.12  1998/03/20  00:30:03  phares
// Changed friction to linedef control
//
// Revision 1.11  1998/03/19  14:37:12  killough
// Fix ThingIsOnLine()
//
// Revision 1.10  1998/03/19  00:40:52  killough
// Change ThingIsOnLine() comments
//
// Revision 1.9  1998/03/16  12:34:45  killough
// Add ThingIsOnLine() function
//
// Revision 1.8  1998/03/09  18:27:10  phares
// Fixed bug in neighboring variable friction sectors
//
// Revision 1.7  1998/03/09  07:19:26  killough
// Remove use of FP for point/line queries
//
// Revision 1.6  1998/03/02  12:03:43  killough
// Change blockmap offsets to 32-bit
//
// Revision 1.5  1998/02/23  04:45:24  killough
// Relax blockmap iterator to demo_compatibility
//
// Revision 1.4  1998/02/02  13:41:38  killough
// Fix demo sync programs caused by last change
//
// Revision 1.3  1998/01/30  23:13:10  phares
// Fixed delimiting 0 bug in P_BlockLinesIterator
//
// Revision 1.2  1998/01/26  19:24:11  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:00  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

