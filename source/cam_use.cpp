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
// Purpose: linked portal aware switch using
// Authors: Ioan Chera
//

#include "z_zone.h"

#include "cam_common.h"
#include "cam_sight.h"
#include "d_gi.h"
#include "d_player.h"
#include "e_exdata.h"
#include "p_map.h"
#include "p_mobj.h"
#include "p_skin.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_portal.h"
#include "s_sound.h"

#define RECURSION_LIMIT 64

//
// UseContext
//
// Context for use actions
//
class UseContext
{
public:
   struct State
   {
      const UseContext *prev;
      fixed_t attackrange;
      int groupid;
      int reclevel;
   };

   static void useLines(const player_t *player, fixed_t x, fixed_t y,
      const State *instate);

private:
   UseContext(const player_t *player, const State *state);
   static bool useTraverse(const intercept_t *in, void *context,
      const divline_t &trace);
   static bool noWayTraverse(const intercept_t *in, void *context,
      const divline_t &trace);

   State state;
   const player_t *player;
   Mobj *thing;
   bool portalhit;
};

//
// UseContext::useLines
//
// Actions. Returns true if a switch has been hit
//
void UseContext::useLines(const player_t *player, fixed_t x, fixed_t y,
   const State *state)
{
   UseContext context(player, state);

   int angle = player->mo->angle >> ANGLETOFINESHIFT;

   fixed_t x2 = x + (context.state.attackrange >> FRACBITS) * finecosine[angle];
   fixed_t y2 = y + (context.state.attackrange >> FRACBITS) * finesine[angle];

   PTDef def;
   def.earlyOut = PTDef::eo_no;
   def.flags = CAM_ADDLINES;
   def.trav = useTraverse;
   PathTraverser traverser(def, &context);
   if(traverser.traverse(x, y, x2, y2))
   {
      if(!context.portalhit)
      {
         PTDef def;
         def.earlyOut = PTDef::eo_no;
         def.flags = CAM_ADDLINES;
         def.trav = noWayTraverse;
         PathTraverser traverser(def, &context);
         if(!traverser.traverse(x, y, x2, y2) && strcasecmp(player->skin->sounds[sk_noway], "none"))
            S_StartSound(context.thing, GameModeInfo->playerSounds[sk_noway]);
      }
   }
}

//
// UseContext::UseContext
//
// Constructor
//
UseContext::UseContext(const player_t *inplayer, const State *instate)
   : player(inplayer), thing(inplayer->mo), portalhit(false)
{
   if(instate)
      state = *instate;
   else
   {
      state.attackrange = USERANGE;
      state.prev = nullptr;
      state.groupid = inplayer->mo->groupid;
      state.reclevel = 0;
   }
}

//
// UseContext::useTraverse
//
// Use traverse. Based on PTR_UseTraverse.
//
bool UseContext::useTraverse(const intercept_t *in, void *vcontext,
   const divline_t &trace)
{
   auto context = static_cast<UseContext *>(vcontext);
   line_t *li = in->d.line;

   // ioanch 20160131: don't treat passable lines as portals; another code path
   // will use them.
   if(li->special && !(li->pflags & PS_PASSABLE))
   {
      // ioanch 20160131: portal aware
      const linkoffset_t *link = P_GetLinkOffset(context->thing->groupid,
         li->frontsector->groupid);
      P_UseSpecialLine(context->thing, li,
         P_PointOnLineSide(context->thing->x + link->x,
            context->thing->y + link->y, li) == 1);

      //WAS can't use for than one special line in a row
      //jff 3/21/98 NOW multiple use allowed with enabling line flag
      return !!(li->flags & ML_PASSUSE);
   }

   // no special
   lineopening_t lo = { 0 };
   if(li->extflags & EX_ML_BLOCKALL) // haleyjd 04/30/11
      lo.openrange = 0;
   else
      lo.calculate(li);

   if(lo.openrange <= 0)
   {
      // can't use through a wall
      if(strcasecmp(context->thing->player->skin->sounds[sk_noway], "none"))
         S_StartSound(context->thing, GameModeInfo->playerSounds[sk_noway]);
      return false;
   }

   // ioanch 20160131: opportunity to pass through portals
   const portal_t *portal = nullptr;
   if(li->pflags & PS_PASSABLE)
      portal = li->portal;
   else if(li->extflags & EX_ML_LOWERPORTAL && li->backsector &&
      li->backsector->srf.floor.pflags & PS_PASSABLE)
   {
      portal = li->backsector->srf.floor.portal;
   }
   if(portal && P_PointOnLineSide(trace.x, trace.y, li) == 0 && in->frac > 0)
   {
      int newfromid = portal->data.link.toid;
      if(newfromid == context->state.groupid ||
         context->state.reclevel >= RECURSION_LIMIT)
      {
         return true;
      }

      State newState(context->state);
      newState.prev = context;
      newState.attackrange -= FixedMul(newState.attackrange, in->frac);
      newState.groupid = newfromid;
      newState.reclevel++;
      fixed_t x = trace.x + FixedMul(trace.dx, in->frac) + portal->data.link.deltax;
      fixed_t y = trace.y + FixedMul(trace.dy, in->frac) + portal->data.link.deltay;

      useLines(context->player, x, y, &newState);
      context->portalhit = true;
      return false;
   }

   // not a special line, but keep checking
   return true;
}

//
// UseContext::noWayTraverse
//
// Same as PTR_NoWayTraverse. Will NEVER be called if portals had been hit.
//
bool UseContext::noWayTraverse(const intercept_t *in, void *vcontext,
   const divline_t &trace)
{
   const line_t *ld = in->d.line; // This linedef
   if(ld->special) // Ignore specials
      return true;
   if(ld->flags & ML_BLOCKING) // Always blocking
      return false;
   lineopening_t lo = { 0 };
   lo.calculate(ld);

   const UseContext *context = static_cast<const UseContext *>(vcontext);

   return
      !(lo.openrange <= 0 ||                                  // No opening
         lo.openbottom > context->thing->z + STEPSIZE ||// Too high, it blocks
         lo.opentop    < context->thing->z + context->thing->height);
   // Too low, it blocks
}

//
// CAM_UseLines
//
// ioanch 20160131: portal-aware variant of P_UseLines
//
void CAM_UseLines(const player_t *player)
{
   UseContext::useLines(player, player->mo->x, player->mo->y, nullptr);
}

// EOF

