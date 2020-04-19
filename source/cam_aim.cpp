//
// The Eternity Engine
// Copyright(C) 2016 James Haley, Ioan Chera, et al.
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
// Purpose: linked portal aware autoaiming (used by player and some monsters)
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
      fixed_t bottomslope;    // bottom sight slope (gets narrowed by obstacles)
      fixed_t topslope;       // top sight slope
      fixed_t cx, cy, cz;     // source coordinates (FIXME: surely?)
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
                         fixed_t frac);
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
   fixed_t pitch = t1->player ? t1->player->pitch : 0;
   lookslope = pitch ? finetangent[(ANG90 - pitch) >> ANGLETOFINESHIFT] : 0;

   if(instate)
      state = *instate;
   else
   {
      state.origindist = 0;
      state.cx = t1->x;
      state.cy = t1->y;
      state.cz = t1->z + (t1->height >> 1) + 8 * FRACUNIT;
      state.groupid = t1->groupid;
      state.prev = nullptr;
      state.reclevel = 0;

      if(!pitch)
      {
         state.topslope = 100 * FRACUNIT / 160;
         state.bottomslope = -100 * FRACUNIT / 160;
      }
      else
      {
         fixed_t topangle, bottomangle;
         topangle = pitch - ANGLE_1 * 32;
         bottomangle = pitch + ANGLE_1 * 32;

         state.topslope = finetangent[(ANG90 - topangle) >> ANGLETOFINESHIFT];
         state.bottomslope = finetangent[(ANG90 - bottomangle) >>
            ANGLETOFINESHIFT];
      }
   }

}

//
// AimContext::checkPortalSector
//
bool AimContext::checkPortalSector(const sector_t *sector, fixed_t totalfrac, fixed_t partialfrac,
                                   const divline_t &trace)
{
   fixed_t linehitz, fixedratio;
   int newfromid;

   fixed_t x, y;

   if(state.topslope > 0 && sector->c_pflags & PS_PASSABLE &&
      (newfromid = sector->c_portal->data.link.toid) != state.groupid)
   {
      // ceiling portal (slope must be up)
      linehitz = state.cz + FixedMul(state.topslope, totalfrac);
      fixed_t planez = P_CeilingPortalZ(*sector);
      if(linehitz > planez)
      {
         // get x and y of position
         if(linehitz == state.cz)
         {
            // handle this edge case: put point right on line
            x = trace.x + FixedMul(trace.dx, partialfrac);
            y = trace.y + FixedMul(trace.dy, partialfrac);
         }
         else
         {
            // add a unit just to ensure that it enters the sector
            fixedratio = FixedDiv(planez - state.cz, linehitz - state.cz);
            // update z frac
            totalfrac = FixedMul(fixedratio, totalfrac);
            // retrieve the xy frac using the origin frac
            partialfrac = FixedDiv(totalfrac - state.origindist, attackrange);

            x = trace.x + FixedMul(partialfrac + 1, trace.dx);
            y = trace.y + FixedMul(partialfrac + 1, trace.dy);

         }

         // don't allow if it's going back
         if(partialfrac + 1 > 0 && R_PointInSubsector(x, y)->sector == sector)
         {
            fixed_t outSlope;
            Mobj *outTarget = nullptr;
            fixed_t outDist;

            State newstate(state);
            newstate.cx = x;
            newstate.cy = y;
            newstate.groupid = newfromid;
            newstate.origindist = totalfrac;
            // don't allow the bottom slope to keep going down
            newstate.bottomslope = emax(0, state.bottomslope);
            newstate.reclevel = state.reclevel + 1;

            if(recurse(newstate, partialfrac, &outSlope, &outTarget, &outDist, *R_CPLink(sector)))
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
   if(state.bottomslope < 0 && sector->f_pflags & PS_PASSABLE &&
      (newfromid = sector->f_portal->data.link.toid) != state.groupid)
   {
      linehitz = state.cz + FixedMul(state.bottomslope, totalfrac);
      fixed_t planez = P_FloorPortalZ(*sector);
      if(linehitz < planez)
      {
         if(linehitz == state.cz)
         {
            x = trace.x + FixedMul(trace.dx, partialfrac);
            y = trace.y + FixedMul(trace.dy, partialfrac);
         }
         else
         {
            fixedratio = FixedDiv(planez - state.cz, linehitz - state.cz);
            totalfrac = FixedMul(fixedratio, totalfrac);
            partialfrac = FixedDiv(totalfrac - state.origindist, attackrange);

            x = trace.x + FixedMul(partialfrac + 1, trace.dx);
            y = trace.y + FixedMul(partialfrac + 1, trace.dy);
         }

         if(partialfrac + 1 > 0 && R_PointInSubsector(x, y)->sector == sector)
         {
            fixed_t outSlope;
            Mobj *outTarget = nullptr;
            fixed_t outDist;

            State newstate(state);
            newstate.cx = x;
            newstate.cy = y;
            newstate.groupid = newfromid;
            newstate.origindist = totalfrac;
            newstate.topslope = emin(0, state.topslope);
            newstate.reclevel = state.reclevel + 1;

            if(recurse(newstate, partialfrac, &outSlope, &outTarget, &outDist, *R_FPLink(sector)))
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
   return false;
}


fixed_t AimContext::recurse(State &newstate, fixed_t partialfrac,
   fixed_t *outSlope, Mobj **outTarget,
   fixed_t *outDist, const linkdata_t &data) const
{
   if(newstate.groupid == state.groupid)
      return false;

   if(state.reclevel > RECURSION_LIMIT)
      return false;

   newstate.cx += data.deltax;
   newstate.cy += data.deltay;
   newstate.cz += data.deltaz;

   fixed_t lessdist = attackrange - FixedMul(attackrange, partialfrac);
   newstate.prev = this;

   fixed_t res = aimLineAttack(thing, angle, lessdist, aimflagsmask, &newstate,
      outTarget, outDist);
   if(outSlope)
      *outSlope = res;
   return true;
}

//
// Check if hitting an edge portal line
//
void AimContext::checkEdgePortals(const line_t *li, fixed_t totaldist, const divline_t &trace,
                                  fixed_t frac)
{
   if(!li->backsector || P_PointOnLineSide(trace.x, trace.y, li) != 0 || frac <= 0)
      return;

   struct edgepart_t
   {
      unsigned extflag;
      unsigned pflags;
      portal_t *portal;
      fixed_t contextslope;
      fixed_t refslope;
   } edgeparts[2] =
   {
      {
         EX_ML_LOWERPORTAL,
         li->backsector->f_pflags,
         li->backsector->f_portal,
         state.bottomslope,
         FixedDiv(li->backsector->floorheight - state.cz, totaldist),
      },
      {
         EX_ML_UPPERPORTAL,
         li->backsector->c_pflags,
         li->backsector->c_portal,
         -state.topslope,
         -FixedDiv(li->backsector->ceilingheight - state.cz, totaldist),
      },
   };

   for(int partnum = 0; partnum < 2; ++partnum)
   {
      edgepart_t &part = edgeparts[partnum];
      if(li->extflags & part.extflag && part.pflags & PS_PASSABLE &&
         part.contextslope <= part.refslope)
      {
         State newState(state);
         newState.cx = trace.x + FixedMul(trace.dx, frac);
         newState.cy = trace.y + FixedMul(trace.dy, frac);
         newState.groupid = part.portal->data.link.toid;
         newState.origindist = totaldist;
         newState.reclevel = state.reclevel + 1;

         fixed_t outSlope;
         Mobj *outTarget = nullptr;
         fixed_t outDist;

         if(recurse(newState, frac, &outSlope, &outTarget, &outDist, part.portal->data.link))
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

   fixed_t totaldist;
   if(in->isaline)
   {
      const line_t *li = in->d.line;
      totaldist = context.state.origindist + FixedMul(context.attackrange, in->frac);
      const sector_t *sector = P_PointOnLineSide(trace.x, trace.y, li) == 0 ? li->frontsector
                                                                            : li->backsector;
      if(sector && totaldist > 0)
      {
         // Don't care about return value; data will be collected in cam's fields

         context.checkPortalSector(sector, totaldist, in->frac, trace);

         // if a closer target than how we already are has been found, then exit
         if(context.linetarget && context.targetdist <= totaldist)
            return false;
      }

      if(!(li->flags & ML_TWOSIDED) || li->extflags & EX_ML_BLOCKALL)
         return false;

      lineopening_t lo = { 0 };
      lo.calculate(li);

      if(lo.openrange <= 0)
         return false;

      const sector_t *osector = sector == li->frontsector ? li->backsector : li->frontsector;
      fixed_t slope;

      if(sector->floorheight != osector->floorheight || (!!(sector->f_pflags & PS_PASSABLE) ^
                                                         !!(osector->f_pflags & PS_PASSABLE)))
      {
         slope = FixedDiv(lo.openbottom - context.state.cz, totaldist);
         if(slope > context.state.bottomslope)
            context.state.bottomslope = slope;
      }

      if(sector->ceilingheight != osector->ceilingheight || (!!(sector->c_pflags & PS_PASSABLE) ^
                                                             !!(osector->c_pflags & PS_PASSABLE)))
      {
         slope = FixedDiv(lo.opentop - context.state.cz, totaldist);
         if(slope < context.state.topslope)
            context.state.topslope = slope;
      }

      if(context.state.topslope <= context.state.bottomslope)
         return false;

      // Check for edge portals, but don't stop looking
      context.checkEdgePortals(li, totaldist, trace, in->frac);

      if(li->pflags & PS_PASSABLE && P_PointOnLineSide(trace.x, trace.y, li) == 0 &&
         in->frac > 0)
      {
         State newState(context.state);
         newState.cx = trace.x + FixedMul(trace.dx, in->frac);
         newState.cy = trace.y + FixedMul(trace.dy, in->frac);
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
      Mobj *th = in->d.thing;
      fixed_t thingtopslope, thingbottomslope;
      if(!(th->flags & MF_SHOOTABLE) || th == context.thing)
         return true;
      if(context.aimflagsmask && ((th->flags & context.thing->flags & MF_FRIEND &&
                                   !th->player) || th->flags4 & MF4_LOWAIMPRIO))
      {
         return true;
      }

      totaldist = context.state.origindist + FixedMul(context.attackrange,
         in->frac);

      // check ceiling and floor
      const sector_t *sector = th->subsector->sector;
      if(sector && totaldist > 0)
      {
         context.checkPortalSector(sector, totaldist, in->frac, trace);
         // if a closer target than how we already are has been found, then exit
         if(context.linetarget && context.targetdist <= totaldist)
            return false;
      }

      thingtopslope = FixedDiv(th->z + th->height - context.state.cz,
         totaldist);

      if(thingtopslope < context.state.bottomslope)
         return true; // shot over the thing

      thingbottomslope = FixedDiv(th->z - context.state.cz, totaldist);
      if(thingbottomslope > context.state.topslope)
         return true; // shot under the thing

                      // this thing can be hit!
      if(thingtopslope > context.state.topslope)
         thingtopslope = context.state.topslope;

      if(thingbottomslope < context.state.bottomslope)
         thingbottomslope = context.state.bottomslope;

      // check if this thing is closer than potentially others found beyond
      // flat portals
      if(!context.linetarget || totaldist < context.targetdist)
      {
         context.aimslope = (thingtopslope + thingbottomslope) / 2;
         context.targetdist = totaldist;
         context.linetarget = th;
      }
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

   fixed_t tx = context.state.cx + (distance >> FRACBITS) * finecosine[angle >> ANGLETOFINESHIFT];
   fixed_t ty = context.state.cy + (distance >> FRACBITS) * finesine[angle >> ANGLETOFINESHIFT];

   if(traverser.traverse(context.state.cx, context.state.cy, tx, ty))
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

