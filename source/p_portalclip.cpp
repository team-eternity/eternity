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
#include "polyobj.h"
#include "p_info.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_portal.h"
#include "p_portalclip.h"
#include "p_portalcross.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_portal.h"

//
// P_getLineHeights
//
// ioanch 20160112: helper function to get line extremities
//
static void P_getLineHeights(const line_t *ld, fixed_t &linebottom,
                             fixed_t &linetop)
{
   if(ld->frontsector->srf.floor.pflags & PS_PASSABLE &&
      !(ld->frontsector->srf.floor.pflags & PF_ATTACHEDPORTAL) &&
      ld->frontsector->srf.floor.portal->data.link.planez > ld->frontsector->srf.floor.height)
   {
      linebottom = ld->frontsector->srf.floor.portal->data.link.planez;
   }
   else
      linebottom = ld->frontsector->srf.floor.height;

   if(ld->frontsector->srf.ceiling.pflags & PS_PASSABLE &&
      !(ld->frontsector->srf.ceiling.pflags & PF_ATTACHEDPORTAL) &&
      ld->frontsector->srf.ceiling.portal->data.link.planez <
      ld->frontsector->srf.ceiling.height)
   {
      linetop = ld->frontsector->srf.ceiling.portal->data.link.planez;
   }
   else
      linetop = ld->frontsector->srf.ceiling.height;

   if(ld->backsector)
   {
      fixed_t bottomback;
      if(ld->backsector->srf.floor.pflags & PS_PASSABLE &&
         !(ld->backsector->srf.floor.pflags & PF_ATTACHEDPORTAL) &&
         ld->backsector->srf.floor.portal->data.link.planez >
         ld->backsector->srf.floor.height)
      {
         bottomback = ld->backsector->srf.floor.portal->data.link.planez;
      }
      else
         bottomback = ld->backsector->srf.floor.height;
      if(bottomback < linebottom)
         linebottom = bottomback;

      fixed_t topback;
      if(ld->backsector->srf.ceiling.pflags & PS_PASSABLE &&
         !(ld->backsector->srf.ceiling.pflags & PF_ATTACHEDPORTAL) &&
         ld->backsector->srf.ceiling.portal->data.link.planez <
         ld->backsector->srf.ceiling.height)
      {
         topback = ld->backsector->srf.ceiling.portal->data.link.planez;
      }
      else
         topback = ld->backsector->srf.ceiling.height;
      if(topback > linetop)
         linetop = topback;
   }
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
                                         fixed_t linebottom, fixed_t linetop, 
                                         PODCollection<line_t *> *pushhit)
{
   fixed_t linemid = linetop / 2 + linebottom / 2;
   bool moveup = thingmid >= linemid;

   if(!moveup && linebottom < clip.zref.ceiling)
   {
      clip.zref.ceiling = linebottom;
      clip.ceilingline = ld;
      clip.blockline = ld;
   }
   if(moveup && linetop > clip.zref.floor)
   {
      clip.zref.floor = linetop;
      clip.zref.floorgroupid = ld->frontsector->groupid;
      clip.floorline = ld;
      clip.blockline = ld;
   }

   fixed_t lowfloor;
   if(!ld->backsector || !moveup)   // if line is 1-sided or above thing
      lowfloor = linebottom;
   else if(linebottom == ld->backsector->srf.floor.height)
      lowfloor = ld->frontsector->srf.floor.height;
   else
      lowfloor = ld->backsector->srf.floor.height;
   // 2-sided and below the thing: pick the higher floor ^^^

   // SAME TRICK AS BELOW!
   if(lowfloor < clip.zref.dropoff && linetop >= clip.zref.dropoff)
      clip.zref.dropoff = lowfloor;

   // ioanch: only change if postpone is false by now
   if(moveup && linetop > clip.zref.secfloor)
      clip.zref.secfloor = linetop;
   if(!moveup && linebottom < clip.zref.secceil)
      clip.zref.secceil = linebottom;

   if(moveup && clip.zref.floor > clip.zref.passfloor)
      clip.zref.passfloor = clip.zref.floor;
   if(!moveup && clip.zref.ceiling < clip.zref.passceil)
      clip.zref.passceil = clip.zref.ceiling;

   // We need now to collect spechits for push activation.
   if(pushhit && full_demo_version >= make_full_version(401, 0) &&
      (clip.thing->groupid == ld->frontsector->groupid ||
      (linetop > thingz && linebottom < thingtopz && !(ld->pflags & PS_PASSABLE))))
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

   fixed_t linetop, linebottom;
   if(po)
   {
      const sector_t *midsector = R_PointInSubsector(po->centerPt.x,
                                                     po->centerPt.y)->sector;
      linebottom = midsector->srf.floor.height;
      linetop = midsector->srf.ceiling.height;
   }
   else
      P_getLineHeights(ld, linebottom, linetop);

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

      uint8_t floorceiling = 0;
      const sector_t *reachedsec;
      fixed_t linemid = (linebottom + linetop) / 2;

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
      if(floorceiling == sector_t::floor &&
         linebottom < (planez = P_PortalZ(surf_ceil, *reachedsec)))
      {
         linebottom = planez;
      }
      if(floorceiling == sector_t::ceiling &&
         linetop > (planez = P_PortalZ(surf_floor, *reachedsec)))
      {
         linetop = planez;
      }
   }

   if(linebottom <= thingz && linetop >= thingtopz)
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
         P_blockingLineDifferentLevel(ld, thingz, thingmid, thingtopz, linebottom, linetop,
            pushhit);
         return true;
      }
      if(!(clip.thing->flags & (MF_MISSILE | MF_BOUNCES)))
      {
         if((ld->flags & ML_BLOCKING) ||
            (mbf21_temp && !(ld->flags & ML_RESERVED) && clip.thing->player && (ld->flags & ML_BLOCKPLAYERS)))
         {
            // explicitly blocking everything
            // or blocking player
            P_blockingLineDifferentLevel(ld, thingz, thingmid, thingtopz, linebottom, linetop,
               pushhit);
            return true;
         }
         if(!(ld->flags & ML_3DMIDTEX) && P_BlockedAsMonster(*clip.thing) &&
            (
               ld->flags & ML_BLOCKMONSTERS ||
               (mbf21_temp && (ld->flags & ML_BLOCKLANDMONSTERS) && !(clip.thing->flags & MF_FLOAT))
               )
            )
         {
            P_blockingLineDifferentLevel(ld, thingz, thingmid, thingtopz, linebottom, linetop,
               pushhit);
            return true;
         }
      }
   }

   // better detection of in-portal lines
   uint32_t lineclipflags = 0;
   clip.open = P_LineOpening(ld, clip.thing, nullptr, true, &lineclipflags);

   // now apply correction to openings in case thing is positioned differently
   bool samegroupid = clip.thing->groupid == linegroupid;

   bool aboveportal = !!(lineclipflags & LINECLIP_ABOVEPORTAL);
   bool underportal = !!(lineclipflags & LINECLIP_UNDERPORTAL);

   if(!samegroupid && !aboveportal && thingz < linebottom && 
      thingmid < (linebottom + clip.open.height.floor) / 2)
   {
      clip.open.height.ceiling = linebottom;
      clip.open.height.floor = D_MININT;
      clip.open.sec.ceiling = linebottom;
      clip.open.sec.floor = D_MININT;
      lineclipflags &= ~LINECLIP_UNDERPORTAL;
   }
   if(!samegroupid && !underportal && thingtopz > linetop &&
      thingmid >= (linetop + clip.open.height.ceiling) / 2)
   {
      // adjust the lowfloor to the real observed value, to prevent
      // wrong dropoffz
      if(ld->backsector &&
         ((clip.open.sec.ceiling == ld->backsector->srf.ceiling.height &&
         clip.open.sec.floor == ld->frontsector->srf.floor.height) ||
         (clip.open.sec.ceiling == ld->frontsector->srf.ceiling.height &&
         clip.open.sec.floor == ld->backsector->srf.floor.height)))
      {
         clip.open.lowfloor = clip.open.sec.floor;
      }

      clip.open.height.floor = linetop;
      clip.open.height.ceiling = D_MAXINT;
      clip.open.sec.floor = linetop;
      clip.open.sec.ceiling = D_MAXINT;
      lineclipflags &= ~LINECLIP_ABOVEPORTAL;
   }

   // update stuff
   // ioanch 20160315: don't forget about 3dmidtex on the same group ID if they
   // decrease the opening
   if((!underportal || (lineclipflags & LINECLIP_UNDER3DMIDTEX)) 
      && clip.open.height.ceiling < clip.zref.ceiling)
   {
      clip.zref.ceiling = clip.open.height.ceiling;
      clip.ceilingline = ld;
      clip.blockline = ld;
   }
   if((!aboveportal || (lineclipflags & LINECLIP_OVER3DMIDTEX)) 
      && clip.open.height.floor > clip.zref.floor)
   {
      clip.zref.floor = clip.open.height.floor;
      clip.zref.floorgroupid = clip.open.bottomgroupid;
      clip.floorline = ld;          // killough 8/1/98: remember floor linedef
      clip.blockline = ld;
   }

   // ioanch 20160116: this is crazy. If the lines belong in separate groups,
   // make sure to only decrease dropoffz if the line top really reaches the
   // current value of dropoffz. Since layers get explored progressively from
   // top to bottom (when going down), dropoffz will then gradually fall down
   // as each layer is explored, if there really is a gap, and accidental
   // detail downstairs will not count, considering the linetop would always
   // be below any dropfloorz upstairs.
   if(clip.open.lowfloor < clip.zref.dropoff && (samegroupid || linetop >= clip.zref.dropoff))
   {
      clip.zref.dropoff = clip.open.lowfloor;
   }

   // haleyjd 11/10/04: 3DMidTex fix: never consider dropoffs when
   // touching 3DMidTex lines.
   if(demo_version >= 331 && clip.open.touch3dside)
      clip.zref.dropoff = clip.zref.floor;

   if(!aboveportal && clip.open.sec.floor > clip.zref.secfloor)
      clip.zref.secfloor = clip.open.sec.floor;
   if(!underportal && clip.open.sec.ceiling < clip.zref.secceil)
      clip.zref.secceil = clip.open.sec.ceiling;

   // SoM 11/6/02: AGHAH
   if(clip.zref.floor > clip.zref.passfloor)
      clip.zref.passfloor = clip.zref.floor;
   if(clip.zref.ceiling < clip.zref.passceil)
      clip.zref.passceil = clip.zref.ceiling;

   // ioanch: only allow spechits if on contact or simply same group.
   // Line portals however are ONLY collected if on the same group

   // if contacted a special line, add it to the list

   if(samegroupid || (linetop > thingz && linebottom < thingtopz && 
      !(ld->pflags & PS_PASSABLE)))
   {
      P_CollectSpechits(ld, pushhit);
   }

   return true;
}

// EOF

