// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2016 Ioan Chera et al.
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
//      Linked portal clipping. Mostly called from functions like
//      when map has portals PIT_CheckPosition3D
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomstat.h"
#include "e_exdata.h"
#include "m_bbox.h"
#include "m_compare.h"
#include "polyobj.h"
#include "p_portal.h"
#include "p_portalclip.h"
#include "p_portalcross.h"
#include "p_slopes.h"
#include "r_main.h"
#include "r_portal.h"

struct lineheights_t
{
   fixed_t bottomend, bottomedge, topedge, topend;
   const sector_t *bottomedgesector, *topedgesector;
};

template<surf_e S>
inline static bool P_surfacePastPortal(const surface_t &surface, v2fixed_t pos)
{
   return surface.pflags & PS_PASSABLE && !(surface.pflags & PF_ATTACHEDPORTAL) && 
      isInner<S>(surface.portal->data.link.planez, surface.getZAt(pos));
}

template<surf_e S>
inline static fixed_t P_visibleHeight(const surface_t &surface, v2fixed_t pos)
{
   return P_surfacePastPortal<S>(surface, pos) ? surface.portal->data.link.planez : surface.getZAt(pos);
}

//
// P_getLineHeights
//
// ioanch 20160112: helper function to get line extremities
//
static lineheights_t P_getLineHeights(const line_t *ld, v2fixed_t pos)
{
   edefstructvar(lineheights_t, result);
   result.bottomend = P_visibleHeight<surf_floor>(ld->frontsector->srf.floor, pos);
   result.topend = P_visibleHeight<surf_ceil>(ld->frontsector->srf.ceiling, pos);

   if(ld->backsector)
   {
      fixed_t bottomback = P_visibleHeight<surf_floor>(ld->backsector->srf.floor, pos);
      if(bottomback < result.bottomend)
      {
         result.bottomedge = result.bottomend;
         result.bottomedgesector = ld->frontsector;
         result.bottomend = bottomback;
      }
      else
      {
         result.bottomedge = bottomback;
         result.bottomedgesector = ld->backsector;
      }

      fixed_t topback = P_visibleHeight<surf_ceil>(ld->backsector->srf.ceiling, pos);
      if(topback > result.topend)
      {
         result.topedge = result.topend;
         result.topedgesector = ld->frontsector;
         result.topend = topback;
      }
      else
      {
         result.topedge = topback;
         result.topedgesector = ld->backsector;
      }
   }
   else
   {
      result.bottomedge = result.topend;
      result.topedge = result.bottomend;
      // sectors null
   }
   return result;
}

//
// P_addPortalHitLine
//
// Adds a portalhit (postponed) line to clip
//
static void P_addPortalHitLine(line_t *ld, polyobj_t *po)
{
   if(clip.numportalhit >= clip.portalhit_max)
   {
      clip.portalhit_max = clip.portalhit_max ? clip.portalhit_max * 2 : 8;
      clip.portalhit = erealloc(doom_mapinter_t::linepoly_t *, clip.portalhit, 
         sizeof(*clip.portalhit) * clip.portalhit_max);
   }
   clip.portalhit[clip.numportalhit].ld = ld;
   clip.portalhit[clip.numportalhit++].po = po;
}

//
// P_blockingLineDifferentLevel
//
// ioanch 20160112: Call this if there's a blocking line at a different level
//
static void P_blockingLineDifferentLevel(line_t *ld, fixed_t thingz,
                                         fixed_t thingmid, fixed_t thingtopz,
                                         const lineheights_t &innerheights,
                                         const lineheights_t &outerheights,
                                         PODCollection<line_t *> *pushhit)
{
   fixed_t linemid = innerheights.topend / 2 + innerheights.bottomend / 2;
   surf_e towards = thingmid >= linemid ? surf_ceil : surf_floor;

   if(towards == surf_floor && outerheights.bottomend < clip.zref.ceiling)
   {
      clip.zref.ceiling = outerheights.bottomend;
      clip.zref.ceilingsector = nullptr;
      clip.ceilingline = ld;
      clip.blockline = ld;
   }
   if(towards == surf_ceil && outerheights.topend > clip.zref.floor)
   {
      clip.zref.floor = outerheights.topend;
      clip.zref.floorgroupid = ld->frontsector->groupid;
      // TODO: we'll need to handle sloped portals one day
      clip.zref.floorsector = nullptr;

      clip.floorline = ld;
      clip.blockline = ld;
   }

   fixed_t lowfloor;
   if(!ld->backsector || towards == surf_floor)   // if line is 1-sided or above thing
      lowfloor = outerheights.bottomend;
   else
      lowfloor = outerheights.bottomedge; // 2-sided and below the thing: pick the higher floor

   // SAME TRICK AS IN P_UpdateFromOpening!
   if(lowfloor < clip.zref.dropoff && outerheights.topend >= clip.zref.dropoff)
      clip.zref.dropoff = lowfloor;

   // ioanch: only change if postpone is false by now
   if(towards == surf_ceil && outerheights.topend > clip.zref.secfloor)
      clip.zref.secfloor = outerheights.topend;
   if(towards == surf_floor && outerheights.bottomend < clip.zref.secceil)
      clip.zref.secceil = outerheights.bottomend;

   if(towards == surf_ceil && clip.zref.floor > clip.zref.passfloor)
      clip.zref.passfloor = clip.zref.floor;
   if(towards == surf_floor && clip.zref.ceiling < clip.zref.passceil)
      clip.zref.passceil = clip.zref.ceiling;

   // We need now to collect spechits for push activation.
   if(pushhit && full_demo_version >= make_full_version(401, 0) &&
      (clip.thing->groupid == ld->frontsector->groupid ||
      (outerheights.topend > thingz && outerheights.bottomend < thingtopz &&
       !(ld->pflags & PS_PASSABLE))))
   {
      pushhit->add(ld);
   }
}

//
// PIT_CheckLine3D
//
// ioanch 20160112: 3D (portal) version of PIT_CheckLine. If map has no portals,
// fall back to PIT_CheckLine
//
bool PIT_CheckLine3D(line_t *ld, polyobj_t *po, void *context)
{
   auto pcl = static_cast<pitcheckline_t *>(context);
   PODCollection<line_t *> *pushhit = pcl->pushhit;

   if(!useportalgroups || full_demo_version < make_full_version(340, 48))
      return PIT_CheckLine(ld, po, context);

   int linegroupid = ld->frontsector->groupid;

   const linkoffset_t *link = P_GetLinkOffset(clip.thing->groupid, linegroupid);
   fixed_t bbox[4];
   bbox[BOXLEFT] = clip.bbox[BOXLEFT] + link->x;
   bbox[BOXBOTTOM] = clip.bbox[BOXBOTTOM] + link->y;
   bbox[BOXRIGHT] = clip.bbox[BOXRIGHT] + link->x;
   bbox[BOXTOP] = clip.bbox[BOXTOP] + link->y;

   if(bbox[BOXRIGHT]  <= ld->bbox[BOXLEFT]   ||
      bbox[BOXLEFT]   >= ld->bbox[BOXRIGHT]  ||
      bbox[BOXTOP]    <= ld->bbox[BOXBOTTOM] ||
      bbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
      return true; // didn't hit it

   if(P_BoxOnLineSide(bbox, ld) != -1)
      return true; // didn't hit it

   // Have ranges due to possible slopes
   lineheights_t outerheights, innerheights;
   bool haveSlopes = P_AnySlope(*ld);
   v2fixed_t i1{}, i2{};
   bool calculatedSlopes = false;
   if(po)
   {
      // NOTE: this may need to be better supported

      v2fixed_t center = { po->centerPt.x, po->centerPt.y };
      const sector_t *midsector = R_PointInSubsector(center)->sector;

      outerheights.bottomend = innerheights.bottomend = midsector->srf.floor.getZAt(center);
      outerheights.topend = innerheights.topend = midsector->srf.ceiling.getZAt(center);
      outerheights.bottomedge = innerheights.bottomedge = D_MAXINT;
      outerheights.topedge = innerheights.topedge = D_MININT;
   }
   else if(haveSlopes)
   {
      calculatedSlopes = true;
      pcl->haveslopes = true;
      P_ExactBoxLinePoints(bbox, *ld, i1, i2);
      lineheights_t heights[2];
      heights[0] = P_getLineHeights(ld, i1);
      heights[1] = P_getLineHeights(ld, i2);

      outerheights.bottomend = emin(heights[0].bottomend, heights[1].bottomend);
      outerheights.bottomedge = emin(heights[0].bottomedge, heights[1].bottomedge);
      outerheights.topedge = emax(heights[0].topedge, heights[1].topedge);
      outerheights.topend = emax(heights[0].topend, heights[1].topend);
      innerheights.bottomend = emax(heights[0].bottomend, heights[1].bottomend);
      innerheights.bottomedge = emax(heights[0].bottomedge, heights[1].bottomedge);
      innerheights.topedge = emin(heights[0].topedge, heights[1].topedge);
      innerheights.topend = emin(heights[0].topend, heights[1].topend);
   }
   else
   {
      outerheights = P_getLineHeights(ld, v2fixed_t{});
      innerheights = outerheights;
   }

   // values to set on exit:
   // clip.ceilingz
   // clip.ceilingline
   // clip.blockline
   // clip.floorz
   // clip.floorline
   // clip.dropoffz
   // clip.secfloorz
   // clip.secceilz
   // clip.passfloorz
   // clip.passceilz
   // (spechit)

   // values obtained from P_LineOpening
   // clip.openfrontsector -> not used
   // clip.openbacksector  -> not used
   // clip.openrange
   // clip.opentop
   // clip.openbottom
   // clip.lowfloor
   // clip.floorpic
   // clip.opensecfloor
   // clip.opensecceil
   // clip.touch3dside

   const fixed_t thingtopz = clip.thing->z + clip.thing->height;
   const fixed_t thingz = clip.thing->z;
   const fixed_t thingmid = thingz / 2 + thingtopz / 2;

   // ioanch 20160121: possibility to postpone floorz, ceilz if it's from a
   // different group, to portalhits array
   
   if(!gGroupVisit[ld->frontsector->groupid])   // portals are guaranteed here
   {
      bool postpone = false;
      v2fixed_t inters = P_BoxLinePoint(bbox, ld);
      v2fixed_t i2;
      angle_t angle = P_PointToAngle(ld->v1->x, ld->v1->y, ld->dx, ld->dy);
      angle -= ANG90;
      i2 = inters;
      i2.x += FixedMul(FRACUNIT >> 12, finecosine[angle >> ANGLETOFINESHIFT]);
      i2.y += FixedMul(FRACUNIT >> 12, finesine[angle >> ANGLETOFINESHIFT]);

      surf_e floorceiling = surf_NUM;	// "none" value
      const sector_t *reachedsec;
      fixed_t linemid = (innerheights.bottomend + innerheights.topend) / 2;

      if(!(reachedsec =
           P_PointReachesGroupVertically(i2.x, i2.y, linemid, linegroupid,
                                         clip.thing->groupid, ld->frontsector,
                                         thingmid, &floorceiling)))
      {
         if(ld->backsector)
         {
            angle += ANG180;
            i2 = inters;
            i2.x += FixedMul(FRACUNIT >> 12, finecosine[angle >> ANGLETOFINESHIFT]);
            i2.y += FixedMul(FRACUNIT >> 12, finesine[angle >> ANGLETOFINESHIFT]);
            if(!(reachedsec =
                 P_PointReachesGroupVertically(i2.x, i2.y, linemid, linegroupid,
                                               clip.thing->groupid,
                                               ld->backsector, thingmid,
                                               &floorceiling)))
            {
               postpone = true;
            }
         }
         else
            postpone = true;
      }
      // ioanch: If a line is in another portal and unlikely to be a real hit,
      // postpone its PIT_CheckLine
      if(postpone)
      {
         P_addPortalHitLine(ld, po);
         return true;
      }

      // Cap the line bottom and top if it's a line from another portal
      fixed_t planez;
      if(floorceiling == surf_floor)
      {
         planez = P_PortalZ(surf_ceil, *reachedsec);
         if(outerheights.bottomend < planez)
            outerheights.bottomend = planez;
         if(innerheights.bottomend < planez)
            innerheights.bottomend = planez;
         if(outerheights.bottomedge < planez)
            outerheights.bottomedge = planez;
         if(innerheights.bottomedge < planez)
            innerheights.bottomedge = planez;
      }
      if(floorceiling == surf_ceil)
      {
         planez = P_PortalZ(surf_floor, *reachedsec);
         if(outerheights.topend > planez)
            outerheights.topend = planez;
         if(innerheights.topend > planez)
            innerheights.topend = planez;
         if(outerheights.topedge > planez)
            outerheights.topedge = planez;
         if(innerheights.topedge > planez)
            innerheights.topedge = planez;
      }
   }

   if(innerheights.bottomend <= thingz && innerheights.topend >= thingtopz)
   {
      // classic Doom behaviour

      // killough 8/10/98: allow bouncing objects to pass through as missiles
      bool thingblock;
      if(P_CheckLineBlocksThing(ld, link, pushhit, thingblock))
         return thingblock;
   }
   else
   {
      // treat impassable lines as lower/upper
      // same conditions as above
      if(!ld->backsector || (ld->extflags & EX_ML_BLOCKALL))
      {
         P_blockingLineDifferentLevel(ld, thingz, thingmid, thingtopz, innerheights, outerheights,
            pushhit);
         return true;
      }
      if(!(clip.thing->flags & (MF_MISSILE | MF_BOUNCES)))
      {
         if((ld->flags & ML_BLOCKING) ||
            (mbf21_demo && !(ld->flags & ML_RESERVED) && clip.thing->player && (ld->flags & ML_BLOCKPLAYERS)))
         {
            // explicitly blocking everything
            // or blocking player
            P_blockingLineDifferentLevel(ld, thingz, thingmid, thingtopz, innerheights,
               outerheights, pushhit);
            return true;
         }
         if(!(ld->flags & ML_3DMIDTEX) && P_BlockedAsMonster(*clip.thing) &&
            (
               ld->flags & ML_BLOCKMONSTERS ||
               (mbf21_demo && (ld->flags & ML_BLOCKLANDMONSTERS) && !(clip.thing->flags & MF_FLOAT))
               )
            )
         {
            P_blockingLineDifferentLevel(ld, thingz, thingmid, thingtopz, innerheights,
               outerheights, pushhit);
            return true;
         }
      }
   }

   // TODO: check the sloped line points here
   // better detection of in-portal lines
   uint32_t lineclipflags = 0;

   if(haveSlopes)
   {
      pcl->haveslopes = true;
      if(!calculatedSlopes)   // may have already calculated them when checking cross-portal heights
         P_ExactBoxLinePoints(bbox, *ld, i1, i2);
     
      // Use the smallest opening from both points
      clip.open = P_LineOpening(ld, clip.thing, &i1, true, &lineclipflags);
      clip.open.intersect(P_LineOpening(ld, clip.thing, &i2, true, &lineclipflags));
   }
   else
      clip.open = P_LineOpening(ld, clip.thing, nullptr, true, &lineclipflags);

   // now apply correction to openings in case thing is positioned differently
   bool samegroupid = clip.thing->groupid == linegroupid;

   bool aboveportal = !!(lineclipflags & LINECLIP_ABOVEPORTAL);
   bool underportal = !!(lineclipflags & LINECLIP_UNDERPORTAL);

   if(!samegroupid && !aboveportal && thingz < innerheights.bottomend && 
      thingmid < (outerheights.bottomend + clip.open.height.floor) / 2)
   {
      clip.open.height.ceiling = outerheights.bottomend;
      clip.open.height.floor = D_MININT;
      clip.open.floorsector = nullptr;
      clip.open.sec.ceiling = outerheights.bottomend;
      clip.open.sec.floor = D_MININT;
      lineclipflags &= ~LINECLIP_UNDERPORTAL;
   }
   if(!samegroupid && !underportal && thingtopz > innerheights.topend &&
      thingmid >= (outerheights.topend + clip.open.height.ceiling) / 2)
   {
      // adjust the lowfloor to the real observed value, to prevent
      // wrong dropoffz
      if(ld->backsector)
      {
         if(!haveSlopes)
         {
            if((clip.open.sec.ceiling == ld->backsector->srf.ceiling.height &&
               clip.open.sec.floor == ld->frontsector->srf.floor.height) ||
               (clip.open.sec.ceiling == ld->frontsector->srf.ceiling.height &&
                  clip.open.sec.floor == ld->backsector->srf.floor.height))
            {
               clip.open.lowfloor = clip.open.sec.floor;
            }
         }
         else
         {
            // Different treatment due to complexity
            if((innerheights.topedgesector == ld->backsector && 
               innerheights.bottomedgesector == ld->frontsector) || 
               (innerheights.topedgesector == ld->frontsector && 
                  innerheights.bottomedgesector == ld->backsector))
            {
               clip.open.lowfloor = outerheights.bottomedge;
            }
         }
      }

      clip.open.height.floor = outerheights.topend;
      clip.open.height.ceiling = D_MAXINT;
      clip.open.ceilsector = nullptr;
      clip.open.sec.floor = outerheights.topend;
      clip.open.sec.ceiling = D_MAXINT;
      lineclipflags &= ~LINECLIP_ABOVEPORTAL;
   }

   // update stuff
   P_UpdateFromOpening(clip.open, ld, clip, underportal, aboveportal, lineclipflags, samegroupid, 
      outerheights.topend);

   // ioanch: only allow spechits if on contact or simply same group.
   // Line portals however are ONLY collected if on the same group

   // if contacted a special line, add it to the list

   if(samegroupid || (outerheights.topend > thingz && outerheights.bottomend < thingtopz && 
      !(ld->pflags & PS_PASSABLE)))
   {
      P_CollectSpechits(ld, pushhit);
   }

   return true;
}

// EOF

