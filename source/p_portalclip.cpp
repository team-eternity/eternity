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
// untouchedViaPortals
//
// ioanch 20160112: linked portal aware version of untouched from p_map.cpp
//
static int untouchedViaOffset(line_t *ld, const linkoffset_t *link)
{
   fixed_t x, y, tmbbox[4];
   return 
     (tmbbox[BOXRIGHT] = (x = clip.thing->x + link->x) + clip.thing->radius) <= 
     ld->bbox[BOXLEFT] ||
     (tmbbox[BOXLEFT] = x - clip.thing->radius) >= ld->bbox[BOXRIGHT] ||
     (tmbbox[BOXTOP] = (y = clip.thing->y + link->y) + clip.thing->radius) <= 
     ld->bbox[BOXBOTTOM] ||
     (tmbbox[BOXBOTTOM] = y - clip.thing->radius) >= ld->bbox[BOXTOP] ||
     P_BoxOnLineSide(tmbbox, ld) != -1;
}

//
// P_getLineHeights
//
// ioanch 20160112: helper function to get line extremities
//
static void P_getLineHeights(const line_t *ld, fixed_t &linebottom, 
                             fixed_t &linetop)
{
   if(ld->frontsector->f_pflags & PS_PASSABLE &&
      !(ld->frontsector->f_pflags & PF_ATTACHEDPORTAL) &&
      ld->frontsector->f_portal->data.link.planez > ld->frontsector->floorheight)
   {
      linebottom = ld->frontsector->f_portal->data.link.planez;
   }
   else
      linebottom = ld->frontsector->floorheight;
   
   if(ld->frontsector->c_pflags & PS_PASSABLE &&
      !(ld->frontsector->c_pflags & PF_ATTACHEDPORTAL) &&
      ld->frontsector->c_portal->data.link.planez < 
      ld->frontsector->ceilingheight)
   {
      linetop = ld->frontsector->c_portal->data.link.planez;
   }
   else
      linetop = ld->frontsector->ceilingheight;

   if(ld->backsector)
   {
      fixed_t bottomback;
      if(ld->backsector->f_pflags & PS_PASSABLE &&
         !(ld->backsector->f_pflags & PF_ATTACHEDPORTAL) &&
         ld->backsector->f_portal->data.link.planez > 
         ld->backsector->floorheight)
      {
         bottomback = ld->backsector->f_portal->data.link.planez;
      }
      else
         bottomback = ld->backsector->floorheight;
      if(bottomback < linebottom)
         linebottom = bottomback;

      fixed_t topback;
      if(ld->backsector->c_pflags & PS_PASSABLE &&
         !(ld->backsector->c_pflags & PF_ATTACHEDPORTAL) &&
         ld->backsector->c_portal->data.link.planez < 
         ld->backsector->ceilingheight)
      {
         topback = ld->backsector->c_portal->data.link.planez;
      }
      else
         topback = ld->backsector->ceilingheight;
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
static void P_blockingLineDifferentLevel(line_t *ld, polyobj_t *po, fixed_t thingz, 
                                         fixed_t thingmid, fixed_t thingtopz,
                                         fixed_t linebottom, fixed_t linetop, 
                                         PODCollection<line_t *> *pushhit)
{
   fixed_t linemid = linetop / 2 + linebottom / 2;
   bool moveup = thingmid >= linemid;

   if(!moveup && linebottom < clip.ceilingz)
   {
      clip.ceilingz = linebottom;
      clip.ceilingline = ld;
      clip.blockline = ld;
   }
   if(moveup && linetop > clip.floorz)
   {
      clip.floorz = linetop;
      clip.floorline = ld;
      clip.blockline = ld;
   }

   fixed_t lowfloor;
   if(!ld->backsector || !moveup)   // if line is 1-sided or above thing
      lowfloor = linebottom;
   else if(linebottom == ld->backsector->floorheight) 
      lowfloor = ld->frontsector->floorheight;
   else
      lowfloor = ld->backsector->floorheight;
   // 2-sided and below the thing: pick the higher floor ^^^

   // SAME TRICK AS BELOW!
   if(lowfloor < clip.dropoffz && linetop >= clip.dropoffz)
      clip.dropoffz = lowfloor;

   // ioanch: only change if postpone is false by now
   if(moveup && linetop > clip.secfloorz)
      clip.secfloorz = linetop;
   if(!moveup && linebottom < clip.secceilz)
      clip.secceilz = linebottom;
         
   if(moveup && clip.floorz > clip.passfloorz)
      clip.passfloorz = clip.floorz;
   if(!moveup && clip.ceilingz < clip.passceilz)
      clip.passceilz = clip.ceilingz;

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
      linebottom = midsector->floorheight;
      linetop = midsector->ceilingheight;
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
         linebottom < (planez = P_CeilingPortalZ(*reachedsec)))
      {
         linebottom = planez;
      }
      if(floorceiling == sector_t::ceiling &&
         linetop > (planez = P_FloorPortalZ(*reachedsec)))
      {
         linetop = planez;
      }
   }

   auto pushhit = static_cast<PODCollection<line_t *> *>(context);
   if(linebottom <= thingz && linetop >= thingtopz)
   {
      // classic Doom behaviour
      if(!ld->backsector || (ld->extflags & EX_ML_BLOCKALL)) // one sided line
      {
         clip.blockline = ld;
         bool result = clip.unstuck && !untouchedViaOffset(ld, link) &&
            FixedMul(clip.x - clip.thing->x, ld->dy) >
            FixedMul(clip.y - clip.thing->y, ld->dx);
         if(!result && pushhit && ld->special &&
            full_demo_version >= make_full_version(401, 0))
         {
            pushhit->add(ld);
         }
         return result;
      }

      // killough 8/10/98: allow bouncing objects to pass through as missiles
      if(!(clip.thing->flags & (MF_MISSILE | MF_BOUNCES)))
      {
         if(ld->flags & ML_BLOCKING)           // explicitly blocking everything
         {
            bool result = clip.unstuck && !untouchedViaOffset(ld, link);
            if(!result && pushhit && ld->special &&
               full_demo_version >= make_full_version(401, 0))
            {
               pushhit->add(ld);
            }
            return result;
         }
         // killough 8/1/98: allow escape

         // killough 8/9/98: monster-blockers don't affect friends
         // SoM 9/7/02: block monsters standing on 3dmidtex only
         if(ld->flags & ML_BLOCKMONSTERS && !(ld->flags & ML_3DMIDTEX) &&
            P_BlockedAsMonster(*clip.thing))
         {
            return false; // block monsters only
         }
      }
   }
   else
   {
      // treat impassable lines as lower/upper
      // same conditions as above
      if(!ld->backsector || (ld->extflags & EX_ML_BLOCKALL))
      {
         P_blockingLineDifferentLevel(ld, po, thingz, thingmid, thingtopz, linebottom, linetop, 
            pushhit);
         return true;
      }
      if(!(clip.thing->flags & (MF_MISSILE | MF_BOUNCES)))
      {
         if(ld->flags & ML_BLOCKING)           // explicitly blocking everything
         {
            P_blockingLineDifferentLevel(ld, po, thingz, thingmid, thingtopz, linebottom, linetop, 
               pushhit);
            return true;
         }
         if(ld->flags & ML_BLOCKMONSTERS && !(ld->flags & ML_3DMIDTEX) &&
            P_BlockedAsMonster(*clip.thing))
         {
            P_blockingLineDifferentLevel(ld, po, thingz, thingmid, thingtopz, linebottom, linetop, 
               pushhit);
            return true;
         }
      }
   }

   // better detection of in-portal lines
   uint32_t lineclipflags = 0;
   P_LineOpening(ld, clip.thing, true, &lineclipflags);

   // now apply correction to openings in case thing is positioned differently
   bool samegroupid = clip.thing->groupid == linegroupid;

   bool aboveportal = !!(lineclipflags & LINECLIP_ABOVEPORTAL);
   bool underportal = !!(lineclipflags & LINECLIP_UNDERPORTAL);

   if(!samegroupid && !aboveportal && thingz < linebottom && 
      thingmid < (linebottom + clip.openbottom) / 2)
   {
      clip.opentop = linebottom;
      clip.openbottom = D_MININT;
      clip.opensecceil = linebottom;
      clip.opensecfloor = D_MININT;
      lineclipflags &= ~LINECLIP_UNDERPORTAL;
   }
   if(!samegroupid && !underportal && thingtopz > linetop &&
      thingmid >= (linetop + clip.opentop) / 2)
   {
      // adjust the lowfloor to the real observed value, to prevent
      // wrong dropoffz
      if(ld->backsector && 
         ((clip.opensecceil == ld->backsector->ceilingheight &&
         clip.opensecfloor == ld->frontsector->floorheight) ||
         (clip.opensecceil == ld->frontsector->ceilingheight && 
         clip.opensecfloor == ld->backsector->floorheight)))
      {
         clip.lowfloor = clip.opensecfloor;
      }

      clip.openbottom = linetop;
      clip.opentop = D_MAXINT;
      clip.opensecfloor = linetop;
      clip.opensecceil = D_MAXINT;
      lineclipflags &= ~LINECLIP_ABOVEPORTAL;
   }

   // update stuff
   // ioanch 20160315: don't forget about 3dmidtex on the same group ID if they
   // decrease the opening
   if((!underportal || (lineclipflags & LINECLIP_UNDER3DMIDTEX)) 
      && clip.opentop < clip.ceilingz)
   {
      clip.ceilingz = clip.opentop;
      clip.ceilingline = ld;
      clip.blockline = ld;
   }
   if((!aboveportal || (lineclipflags & LINECLIP_OVER3DMIDTEX)) 
      && clip.openbottom > clip.floorz)
   {
      clip.floorz = clip.openbottom;
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
   if(clip.lowfloor < clip.dropoffz && (samegroupid || linetop >= clip.dropoffz))
   {
      clip.dropoffz = clip.lowfloor;
   }

   // haleyjd 11/10/04: 3DMidTex fix: never consider dropoffs when
   // touching 3DMidTex lines.
   if(demo_version >= 331 && clip.touch3dside)
      clip.dropoffz = clip.floorz;

   if(!aboveportal && clip.opensecfloor > clip.secfloorz)
      clip.secfloorz = clip.opensecfloor;
   if(!underportal && clip.opensecceil < clip.secceilz)
      clip.secceilz = clip.opensecceil;

   // SoM 11/6/02: AGHAH
   if(clip.floorz > clip.passfloorz)
      clip.passfloorz = clip.floorz;
   if(clip.ceilingz < clip.passceilz)
      clip.passceilz = clip.ceilingz;

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

