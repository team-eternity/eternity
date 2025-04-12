//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Ioan Chera, et al.
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: Linked portal aware autoaiming (used by player and some monsters).
// Authors: Ioan Chera
//

#include "z_zone.h"

#include "cam_common.h"
#include "cam_sight.h"
#include "d_player.h"
#include "e_exdata.h"
#include "m_compare.h"
#include "p_mobj.h"
#include "p_portal.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_pcheck.h"
#include "r_portal.h"

#define RECURSION_LIMIT 64

//
// The entire context used during autoaiming
//
class AimContext
{
public:
   //
   // State information that gets propagated between portals
   //
   struct State
   {
      fixed_t origindist;     // distance travelled so far
      Surfaces<fixed_t> slope;   // sight slopes (floor and ceiling)
      v3fixed_t c;          // source coordinates
      int groupid;            // current group ID
      const AimContext *prev; // reference to previous aim context in recursion
      int reclevel;           // recursion level (for safety)
   };

   static fixed_t aimLineAttack(const Mobj *t1, angle_t angle, fixed_t distance,
      bool mask, const State *state, Mobj **outTarget, fixed_t *outDist);

private:
   AimContext(
      const Mobj *t1, 
      angle_t angle, 
      fixed_t distance, 
      bool mask,
      const State *state);
   static bool aimTraverse(const intercept_t *in, void *data, const divline_t &trace);
   bool checkPortalSector(const sector_t *sector, fixed_t totalfrac, fixed_t partialfrac,
                          const divline_t &trace);
   void checkEdgePortals(const line_t *li, fixed_t totaldist, const divline_t &trace,
                         fixed_t frac, v2fixed_t edgepos);
   fixed_t recurse(State &newstate, fixed_t partialfrac, fixed_t *outSlope, Mobj **outTarget,
                   fixed_t *outDist, const linkdata_t &data) const;

   const Mobj *thing;
   fixed_t attackrange;
   bool aimflagsmask;
   State state;
   fixed_t lookslope;
   fixed_t aimslope;
   Mobj *linetarget;
   fixed_t targetdist;
   angle_t angle;
};



//
// Creates the context
//
AimContext::AimContext(const Mobj *t1, angle_t inangle, fixed_t distance,
   bool mask, const State *instate) :
   thing(t1), attackrange(distance), aimflagsmask(mask), aimslope(0),
   linetarget(nullptr), targetdist(D_MAXINT), angle(inangle)
{
   fixed_t pitch = t1 && t1->player ? t1->player->pitch : 0;
   lookslope = pitch ? finetangent[(ANG90 - pitch) >> ANGLETOFINESHIFT] : 0;

   if(instate)
      state = *instate;
   else
   {
      state.origindist = 0;
      if(t1)
      {
         state.c.x = t1->x;
         state.c.y = t1->y;
         state.c.z = t1->z + (t1->height >> 1) + 8 * FRACUNIT;
         state.groupid = t1->groupid;
      }
      else
      {
         state.c.x = state.c.y = 0;
         state.c.z = 8 * FRACUNIT;
         state.groupid = 0;
      }
      state.prev = nullptr;
      state.reclevel = 0;

      if(!pitch)
      {
         state.slope.ceiling = DEFAULT_AIM_SLOPE;
         state.slope.floor = -DEFAULT_AIM_SLOPE;
      }
      else
      {
         fixed_t topangle, bottomangle;
         topangle = pitch - ANGLE_1 * AUTOAIM_PITCH_DEGREES;
         bottomangle = pitch + ANGLE_1 * AUTOAIM_PITCH_DEGREES;

         state.slope.ceiling = finetangent[(ANG90 - topangle) >> ANGLETOFINESHIFT];
         state.slope.floor = finetangent[(ANG90 - bottomangle) >> ANGLETOFINESHIFT];
      }
   }

}

//
// AimContext::checkPortalSector
//
bool AimContext::checkPortalSector(const sector_t *sector, const fixed_t totaldist,
                                   const fixed_t partialfrac, const divline_t &trace)
{
   for(surf_e surf : SURFS)
   {
      const surface_t &surface = sector->srf[surf];
      fixed_t slope = state.slope[surf];
      int newfromid = R_NOGROUP;
      if(isOuter(surf, slope, 0) && surface.pflags & PS_PASSABLE &&
         (newfromid = surface.portal->data.link.toid) != state.groupid)
      {
         fixed_t linehitz = state.c.z + FixedMul(slope, totaldist);
         fixed_t planez = P_PortalZ(surface);
         if(isOuter(surf, linehitz, planez))
         {
            // get x and y of position
            v2fixed_t v;
            fixed_t newtotaldist = totaldist;
            fixed_t newpartialfrac = partialfrac;
            if(linehitz == state.c.z)
            {
               // handle this edge case: put point right on line
               v = trace.v + trace.dv.fixedMul(newpartialfrac);
            }
            else
            {
               // add a unit just to ensure that it enters the sector
               fixed_t fixedratio = FixedDiv(planez - state.c.z, linehitz - state.c.z);
               // update z frac
               newtotaldist = FixedMul(fixedratio, newtotaldist);
               // retrieve the xy frac using the origin frac
               newpartialfrac = FixedDiv(newtotaldist - state.origindist, attackrange);

               v = trace.v + trace.dv.fixedMul(newpartialfrac + 1);
            }

            // don't allow if it's going back
            if(newpartialfrac + 1 > 0 && R_PointInSubsector(v)->sector == sector)
            {
               fixed_t outSlope;
               Mobj *outTarget = nullptr;
               fixed_t outDist;

               State newstate(state);
               newstate.c.x = v.x;
               newstate.c.y = v.y;
               newstate.groupid = newfromid;
               newstate.origindist = newtotaldist;
               // don't allow the opposite slope to keep going forth
               newstate.slope[!surf] = state.slope[!surf];
               if(isOuter(!surf, newstate.slope[!surf], 0))
                  newstate.slope[!surf] = 0;
               newstate.reclevel = state.reclevel + 1;

               if(recurse(newstate, newpartialfrac, &outSlope, &outTarget, &outDist,
                          surface.portal->data.link))
               {
                  if(outTarget && (!linetarget || outDist < targetdist))
                  {
                     linetarget = outTarget;
                     targetdist = outDist;
                     aimslope = outSlope;
                  }
               }
            }
         }
      }
   }
   return false;
}


fixed_t AimContext::recurse(State &newstate, fixed_t partialfrac, fixed_t *outSlope,
                            Mobj **outTarget, fixed_t *outDist, const linkdata_t &data) const
{
   if(newstate.groupid == state.groupid)
      return false;

   if(state.reclevel > RECURSION_LIMIT)
      return false;

   newstate.c += data.delta;

   fixed_t lessdist = attackrange - FixedMul(attackrange, partialfrac);
   newstate.prev = this;

   fixed_t res = aimLineAttack(thing, angle, lessdist, aimflagsmask, &newstate, outTarget, outDist);
   if(outSlope)
      *outSlope = res;
   return true;
}

//
// Check if hitting an edge portal line
//
void AimContext::checkEdgePortals(const line_t *li, fixed_t totaldist, const divline_t &trace,
                                  fixed_t frac, v2fixed_t edgepos)
{
   if(!li->backsector || P_PointOnLineSidePrecise(trace.x, trace.y, li) != 0 || frac <= 0)
      return;

   for(surf_e surf : SURFS)
   {
      const surface_t &surface = li->backsector->srf[surf];
      fixed_t contextslope = state.slope[surf];
      fixed_t refslope = FixedDiv(surface.getZAt(edgepos) - state.c.z, totaldist);

      if(li->extflags & e_edgePortalFlags[surf] && surface.pflags & PS_PASSABLE &&
         !isInner(surf, contextslope, refslope))
      {
         State newState(state);
         newState.c.x = trace.x + FixedMul(trace.dx, frac);
         newState.c.y = trace.y + FixedMul(trace.dy, frac);
         newState.groupid = surface.portal->data.link.toid;
         newState.origindist = totaldist;
         newState.reclevel = state.reclevel + 1;

         fixed_t outSlope;
         Mobj *outTarget = nullptr;
         fixed_t outDist;

         if(recurse(newState, frac, &outSlope, &outTarget, &outDist, surface.portal->data.link))
         {
            if(outTarget && (!linetarget || outDist < targetdist))
            {
               linetarget = outTarget;
               targetdist = outDist;
               aimslope = outSlope;
            }
         }
      }
   }
}

//
// Called when hitting a line or object
//
bool AimContext::aimTraverse(const intercept_t *in, void *vdata, const divline_t &trace)
{
   auto &context = *static_cast<AimContext *>(vdata);

   fixed_t totaldist = context.state.origindist + FixedMul(context.attackrange, in->frac);
   const sector_t *sector;
   const line_t *li = in->d.line;
   Mobj *th = in->d.thing;
   if(in->isaline)
      sector = P_PointOnLineSidePrecise(trace.x, trace.y, li) == 0 ? li->frontsector : li->backsector;
   else
      sector = th->subsector->sector;
   if(sector && totaldist > 0)
   {
      context.checkPortalSector(sector, totaldist, in->frac, trace);
      // if a closer target than how we already are has been found, then exit
      if(context.linetarget && context.targetdist <= totaldist)
         return false;
   }

   if(in->isaline)
   {
      if(!(li->flags & ML_TWOSIDED) || li->extflags & EX_ML_BLOCKALL)
         return false;

      v2fixed_t edgepos = trace.v + trace.dv.fixedMul(in->frac);
      tracelineopening_t lo = { 0 };
      lo.calculateAtPoint(*li, edgepos);

      if(lo.openrange <= 0)
         return false;

      const sector_t *osector = sector == li->frontsector ? li->backsector : li->frontsector;
      fixed_t slope;

      for(surf_e surf : SURFS)
      {
         const surface_t &surface = sector->srf[surf];
         const surface_t &otherSurface = osector->srf[surf];
         if(surface.getZAt(edgepos) != otherSurface.getZAt(edgepos) ||
            (surface.pflags & PS_PASSABLE) != (otherSurface.pflags & PS_PASSABLE))
         {
            slope = FixedDiv(lo.open[surf] - context.state.c.z, totaldist);
            if(isInner(surf, slope, context.state.slope[surf]))
               context.state.slope[surf] = slope;
         }
      }

      if(context.state.slope.ceiling <= context.state.slope.floor)
         return false;

      // Check for edge portals, but don't stop looking
      context.checkEdgePortals(li, totaldist, trace, in->frac, edgepos);

      if(li->pflags & PS_PASSABLE && P_PointOnLineSidePrecise(trace.x, trace.y, li) == 0 &&
         in->frac > 0)
      {
         State newState(context.state);
         newState.c.x = trace.x + FixedMul(trace.dx, in->frac);
         newState.c.y = trace.y + FixedMul(trace.dy, in->frac);
         newState.groupid = li->portal->data.link.toid;
         newState.origindist = totaldist;
         newState.reclevel = context.state.reclevel + 1;
         return !context.recurse(newState, in->frac, &context.aimslope, &context.linetarget,
                                 nullptr, li->portal->data.link);
      }

      return true;
   }
   else
   {
      if(!P_CheckThingAimAvailability(th, context.thing, context.aimflagsmask))
         return true;

      edefstructvar(linetracer_t, atrace);
      atrace.attackrange = context.attackrange;
      atrace.z = context.state.c.z;
      atrace.bottomslope = context.state.slope.floor;
      atrace.topslope = context.state.slope.ceiling;
      if(!P_CheckThingAimSlopes(th, context.state.origindist, in->frac, atrace))
         return true;

      context.aimslope = atrace.aimslope;
      context.targetdist = totaldist;
      context.linetarget = th;
      return false;
   }
}

//
// Starts aiming
//
fixed_t AimContext::aimLineAttack(const Mobj *t1, angle_t angle, fixed_t distance, bool mask,
                                  const State *state, Mobj **outTarget, fixed_t *outDist)
{
   AimContext context(t1, angle, distance, mask, state);

   PTDef def;
   def.flags = CAM_ADDLINES | CAM_ADDTHINGS;
   def.earlyOut = PTDef::eo_no;
   def.trav = aimTraverse;
   PathTraverser traverser(def, &context);

   fixed_t tx = context.state.c.x + (distance >> FRACBITS) * finecosine[angle >> ANGLETOFINESHIFT];
   fixed_t ty = context.state.c.y + (distance >> FRACBITS) * finesine[angle >> ANGLETOFINESHIFT];

   if(traverser.traverse(context.state.c.x, context.state.c.y, tx, ty))
   {
      const sector_t *endsector = R_PointInSubsector(tx, ty)->sector;
      if(context.checkPortalSector(endsector, distance, FRACUNIT, traverser.trace))
      {
         if(outTarget)
            *outTarget = context.linetarget;
         if(outDist)
            *outDist = context.targetdist;

         return context.linetarget ? context.aimslope : context.lookslope;
      }
   }

   if(outTarget)
      *outTarget = context.linetarget;
   if(outDist)
      *outDist = context.targetdist;

   return context.linetarget ? context.aimslope : context.lookslope;
}

//
// CAM_AimLineAttack
//
// Reentrant autoaim
//
fixed_t CAM_AimLineAttack(const Mobj *t1, angle_t angle, fixed_t distance,
   bool mask, Mobj **outTarget)
{
   return AimContext::aimLineAttack(t1, angle, distance, mask, nullptr, outTarget, nullptr);
}

// EOF

