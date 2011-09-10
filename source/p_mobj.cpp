// Emacs style mode select   -*- C -*- vi:ts=3 sw=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 Simon Howard
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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Moving object handling. Spawn functions.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomdef.h"
#include "doomstat.h"
#include "m_random.h"
#include "r_main.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_map3d.h"
#include "p_tick.h"
#include "sounds.h"
#include "st_stuff.h"
#include "hu_stuff.h"
#include "s_sound.h"
#include "info.h"
#include "g_game.h"
#include "p_chase.h"
#include "p_inter.h"
#include "p_spec.h" // haleyjd 04/05/99: TerrainTypes
#include "p_partcl.h"
#include "in_lude.h"
#include "d_gi.h"
#include "p_user.h"
#include "g_dmflag.h"
#include "e_states.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "e_exdata.h"
#include "a_small.h"
#include "d_dehtbl.h"
#include "p_info.h"
#include "c_io.h"

// [CG] Added
#include "cs_main.h"
#include "cs_netid.h"
#include "cs_position.h"
#include "cs_team.h"
#include "cs_ctf.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "sv_main.h"

void P_FallingDamage(player_t *);

fixed_t FloatBobOffsets[64] = // haleyjd 04/30/99: FloatBob
{
   0, 51389, 102283, 152192,
   200636, 247147, 291278, 332604,
   370727, 405280, 435929, 462380,
   484378, 501712, 514213, 521763,
   524287, 521763, 514213, 501712,
   484378, 462380, 435929, 405280,
   370727, 332604, 291278, 247147,
   200636, 152192, 102283, 51389,
   -1, -51390, -102284, -152193,
   -200637, -247148, -291279, -332605,
   -370728, -405281, -435930, -462381,
   -484380, -501713, -514215, -521764,
   -524288, -521764, -514214, -501713,
   -484379, -462381, -435930, -405280,
   -370728, -332605, -291279, -247148,
   -200637, -152193, -102284, -51389
};

fixed_t FloatBobDiffs[64] =
{
   51389, 51389, 50894, 49909, 48444,
   46511, 44131, 41326, 38123,
   34553, 30649, 26451, 21998,
   17334, 12501, 7550, 2524,
   -2524, -7550, -12501, -17334,
   -21998, -26451, -30649, -34553,
   -38123, -41326, -44131, -46511,
   -48444, -49909, -50894, -51390,
   -51389, -50894, -49909, -48444,
   -46511, -44131, -41326, -38123,
   -34553, -30649, -26451, -21999,
   -17333, -12502, -7549, -2524,
   2524, 7550, 12501, 17334,
   21998, 26451, 30650, 34552,
   38123, 41326, 44131, 46511,
   48444, 49909, 50895
};

/* [CG] Moved definition of seenstate_t to p_mobj.h.

typedef struct seenstate_s
{
   mdllistitem_t link;
   int statenum;
} seenstate_t;
*/

// haleyjd 03/27/10: freelist of seenstate objects
static mdllistitem_t *seenstate_freelist;

//
// P_InitSeenStates
//
// haleyjd 03/27/10: creates an initial pool of seenstate objects.
//
static void P_InitSeenStates(void)
{
   seenstate_t *newss;
   int i;

   newss = calloc(32, sizeof(seenstate_t));

   for(i = 0; i < 32; ++i)
      M_DLListInsert(&(newss[i].link), &seenstate_freelist);
}

//
// P_FreeSeenStates
//
// haleyjd 03/27/10: puts a set of seenstates back onto the freelist
//
void P_FreeSeenStates(seenstate_t *list)
{
   mdllistitem_t *oldhead = seenstate_freelist;
   mdllistitem_t *newtail = &list->link;

   // find tail of list of objects to free
   while(newtail->next)
      newtail = newtail->next;

   // attach current freelist to tail
   newtail->next = oldhead;
   if(oldhead)
      oldhead->prev = &newtail->next;

   // attach new list to seenstate_freelist
   seenstate_freelist = &list->link;
   list->link.prev = &seenstate_freelist;
}

//
// P_GetSeenState
//
// Gets a seenstate from the free list if one is available, or creates a
// new one.
//
static seenstate_t *P_GetSeenState(void)
{
   seenstate_t *ret;

   if(seenstate_freelist)
   {
      mdllistitem_t *item = seenstate_freelist;

      M_DLListRemove(item);

      ret = (seenstate_t *)item;

      memset(ret, 0, sizeof(seenstate_t));
   }
   else
      ret = calloc(1, sizeof(seenstate_t));

   return ret;
}

//
// P_AddSeenState
//
// Marks a new state as having been seen.
//
void P_AddSeenState(int statenum, seenstate_t **list)
{
   seenstate_t *newss = P_GetSeenState();

   newss->statenum = statenum;

   M_DLListInsert(&newss->link, (mdllistitem_t **)list);
}

//
// P_CheckSeenState
//
// Checks if the given state has been seen
//
boolean P_CheckSeenState(int statenum, seenstate_t *list)
{
   seenstate_t *curstate = list;

   while(curstate)
   {
      if(statenum == curstate->statenum)
         return true;

      curstate = (seenstate_t *)(curstate->link.next);
   }

   return false;
}

//
// P_SetMobjState
//
// Returns true if the mobj is still present.
//
boolean P_SetMobjState(mobj_t* mobj, statenum_t state)
{
   state_t *st;
   boolean client_should_handle = false;
   int puff_type = E_SafeThingType(MT_PUFF);
   int blood_type = E_SafeThingType(MT_BLOOD);

   // haleyjd 03/27/10: new state cycle detection
   static boolean firsttime = true; // for initialization
   seenstate_t *seenstates  = NULL; // list of seenstates for this instance
   boolean ret = true;                         // return value

   if(firsttime)
   {
      P_InitSeenStates();
      firsttime = false;
   }

   if(mobj->type == blood_type ||
      mobj->type == puff_type  ||
      mobj == players[consoleplayer].mo)
   {
      client_should_handle = true;
   }
   else if(CS_CLIENT)
   {
      // [CG] Clients only set state for the local player (because they're
      //      predicting), blood, puffs and telefog.  Otherwise, they'll get a
      //      message from the server.
      return true;
   }

   do
   {
      if(state == NullStateNum)
      {
         mobj->state = NULL;

         if(CS_SERVER && !client_should_handle)
         {
            SV_BroadcastActorState(mobj, NullStateNum);
            SV_BroadcastActorRemoved(mobj);
            P_RemoveMobj(mobj);
         }
         else if(CS_CLIENT)
            CL_RemoveMobj(mobj);
         else
            P_RemoveMobj(mobj);

         ret = false;
         break;                 // killough 4/9/98
      }

      st = states[state];
      mobj->state = st;
      mobj->tics = st->tics;

      if(CS_SERVER && !client_should_handle)
         SV_BroadcastActorState(mobj, state);

      // sf: skins
      // haleyjd 06/11/08: only replace if st->sprite == default sprite
      if(mobj->skin && st->sprite == mobj->info->defsprite)
         mobj->sprite = mobj->skin->sprite;
      else
         mobj->sprite = st->sprite;

      mobj->frame = st->frame;

      // Modified handling.
      // Call action functions when the state is set

      if(st->action)
         st->action(mobj);

      // haleyjd 05/20/02: run particle events
      if(st->particle_evt)
         P_RunEvent(mobj);

      P_AddSeenState(state, &seenstates);

      state = st->nextstate;
   } while(!mobj->tics && !P_CheckSeenState(state, seenstates));

   if(ret && !mobj->tics)  // killough 4/9/98: detect state cycles
      doom_printf(FC_ERROR "Warning: State Cycle Detected");

   // haleyjd 03/27/10: put seenstates onto freelist
   if(seenstates)
      P_FreeSeenStates(seenstates);

   return ret;
}

//
// P_SetMobjStateNF
//
// haleyjd: Like P_SetMobjState, but this function does not execute the
// action function in the state, nor does it loop through states with 0-tic
// durations. Because of this, no recursion detection is needed in this
// function.
//
// This function was originally added by Raven in Heretic for the Maulotaur,
// but it has proven itself useful elsewhere.
//
boolean P_SetMobjStateNF(mobj_t *mobj, statenum_t state)
{
   state_t *st;

   if(!serverside)
      return true;

   if(state == NullStateNum)
   {
      // remove mobj
      mobj->state = NULL;
      if(CS_SERVER)
         SV_BroadcastActorRemoved(mobj);
      P_RemoveMobj(mobj);
      return false;
   }

   st = states[state];
   mobj->state = st;

   // haleyjd 09/29/09: don't leave an object in a state with 0 tics
   mobj->tics = (st->tics > 0) ? st->tics : 1;

   if(mobj->skin && st->sprite == mobj->info->defsprite)
      mobj->sprite = mobj->skin->sprite;
   else
      mobj->sprite = st->sprite;

   mobj->frame = st->frame;

   if(CS_SERVER)
      SV_BroadcastActorState(mobj, state);

   return true;
}


//
// P_ExplodeMissile
//
void P_ExplodeMissile(mobj_t *mo)
{
   // [CG] Only servers explode missiles.
   if(!serverside)
      return;

   // haleyjd 08/02/04: EXPLOCOUNT flag
   if(mo->flags3 & MF3_EXPLOCOUNT)
   {
      if(++mo->counters[1] < mo->counters[2])
         return;
   }

   mo->momx = mo->momy = mo->momz = 0;

   // haleyjd: an attempt at fixing explosions on skies (works!)
   if(demo_version >= 329)
   {
      if(mo->subsector->sector->intflags & SIF_SKY &&
         mo->z >= mo->subsector->sector->ceilingheight - P_ThingInfoHeight(mo->info))
      {
         if(CS_SERVER)
            SV_BroadcastActorRemoved(mo);
         P_RemoveMobj(mo); // don't explode on the actual sky itself
         return;
      }
   }

   P_SetMobjState(mo, mobjinfo[mo->type].deathstate);

   if(!(mo->flags4 & MF4_NORANDOMIZE))
   {
      mo->tics -= P_Random(pr_explode) & 3;

      if(mo->tics < 1)
         mo->tics = 1;
   }

   if(CS_SERVER)
      SV_BroadcastMissileExploded(mo);

   mo->flags &= ~MF_MISSILE;

   S_StartSound(mo, mo->info->deathsound);

   // haleyjd: disable any particle effects
   mo->effects = 0;
}

void P_ThrustMobj(mobj_t *mo, angle_t angle, fixed_t move)
{
   angle >>= ANGLETOFINESHIFT;
   mo->momx += FixedMul(move, finecosine[angle]);
   mo->momy += FixedMul(move, finesine[angle]);
}

//
// P_XYMovement
//
// Attempts to move something if it has momentum.
//
// killough 11/98: minor restructuring

void P_XYMovement(mobj_t* mo)
{
   player_t *player = mo->player;
   fixed_t xmove, ymove;

   //e6y
   fixed_t oldx, oldy; // phares 9/10/98: reducing bobbing/momentum on ice

   if(!(mo->momx | mo->momy)) // Any momentum?
   {
      if(mo->flags & MF_SKULLFLY)
      {
         // the skull slammed into something

         mo->flags &= ~MF_SKULLFLY;
         mo->momz = 0;

         if(demo_version >= 335)
            P_SetMobjStateNF(mo, mo->info->spawnstate);
         else
            P_SetMobjState(mo, mo->info->spawnstate);
      }

      return;
   }

   if(mo->momx > MAXMOVE)
      mo->momx = MAXMOVE;
   else if(mo->momx < -MAXMOVE)
      mo->momx = -MAXMOVE;

   if(mo->momy > MAXMOVE)
      mo->momy = MAXMOVE;
   else if(mo->momy < -MAXMOVE)
      mo->momy = -MAXMOVE;

   xmove = mo->momx;
   ymove = mo->momy;

   oldx = mo->x; // phares 9/10/98: new code to reduce bobbing/momentum
   oldy = mo->y; // when on ice & up against wall. These will be compared
                 // to your x,y values later to see if you were able to move

   do
   {
      fixed_t ptryx, ptryy;

      // killough 8/9/98: fix bug in original Doom source:
      // Large negative displacements were never considered.
      // This explains the tendency for Mancubus fireballs
      // to pass through walls.

      // [CG] This allows 2-way wallrun, which did not exist in Vanilla Doom.
      //      So c/s mode only allows this if the appropriate DMFLAG is set.
      //      In fact, I can't believe this doesn't cause demo desyncs...?
      if(!clientserver || (dmflags2 & dmf_allow_two_way_wallrun))
      {
         if(xmove > MAXMOVE / 2 || ymove > MAXMOVE / 2 ||  // killough 8/9/98:
            ((xmove < -MAXMOVE / 2 || ymove < -MAXMOVE / 2)
            && demo_version >= 203))
         {
            ptryx = mo->x + xmove / 2;
            ptryy = mo->y + ymove / 2;
            xmove >>= 1;
            ymove >>= 1;
         }
         else
         {
            ptryx = mo->x + xmove;
            ptryy = mo->y + ymove;
            xmove = ymove = 0;
         }
      }
      else
      {
         if(xmove > MAXMOVE / 2 || ymove > MAXMOVE / 2)
         {
            ptryx = mo->x + xmove / 2;
            ptryy = mo->y + ymove / 2;
            xmove >>= 1;
            ymove >>= 1;
         }
         else
         {
            ptryx = mo->x + xmove;
            ptryy = mo->y + ymove;
            xmove = ymove = 0;
         }
      }

      // killough 3/15/98: Allow objects to drop off

      if(!P_TryMove(mo, ptryx, ptryy, true))
      {
         // blocked move

         // killough 8/11/98: bouncing off walls
         // killough 10/98:
         // Add ability for objects other than players to bounce on ice

         if(!(mo->flags & MF_MISSILE) && demo_version >= 203 &&
            (mo->flags & MF_BOUNCES ||
             (!player && clip.blockline &&
              variable_friction && mo->z <= mo->floorz &&
              P_GetFriction(mo, NULL) > ORIG_FRICTION)))
         {
            if(clip.blockline)
            {
               fixed_t r = ((clip.blockline->dx >> FRACBITS) * mo->momx +
                            (clip.blockline->dy >> FRACBITS) * mo->momy) /
                    ((clip.blockline->dx >> FRACBITS)*(clip.blockline->dx >> FRACBITS)+
                     (clip.blockline->dy >> FRACBITS)*(clip.blockline->dy >> FRACBITS));
               fixed_t x = FixedMul(r, clip.blockline->dx);
               fixed_t y = FixedMul(r, clip.blockline->dy);

               // reflect momentum away from wall

               mo->momx = x*2 - mo->momx;
               mo->momy = y*2 - mo->momy;

               // if under gravity, slow down in
               // direction perpendicular to wall.

               if(!(mo->flags & MF_NOGRAVITY))
               {
                  mo->momx = (mo->momx + x)/2;
                  mo->momy = (mo->momy + y)/2;
               }
            }
            else
            {
               mo->momx = mo->momy = 0;
            }
         }
         else if(mo->flags3 & MF3_SLIDE) // haleyjd: SLIDE flag
         {
            P_SlideMove(mo); // try to slide along it
         }
         else if(mo->flags & MF_MISSILE)
         {
            // haleyjd 1/17/00: feel the might of reflection!
            if(clip.BlockingMobj &&
               clip.BlockingMobj->flags2 & MF2_REFLECTIVE)
            {
               angle_t refangle =
                  P_PointToAngle(clip.BlockingMobj->x, clip.BlockingMobj->y, mo->x, mo->y);

               // Change angle for reflection
               if(clip.BlockingMobj->flags2 & MF2_DEFLECTIVE)
               {
                  // deflect it fully
                  if(P_Random(pr_reflect) < 128)
                     refangle += ANG45;
                  else
                     refangle -= ANG45;
               }
               else
                  refangle += ANGLE_1 * ((P_Random(pr_reflect)%16)-8);

               // Reflect the missile along angle
               mo->angle = refangle;
               refangle >>= ANGLETOFINESHIFT;
               mo->momx = FixedMul(mo->info->speed>>1, finecosine[refangle]);
               mo->momy = FixedMul(mo->info->speed>>1, finesine[refangle]);

               if(mo->flags2 & MF2_SEEKERMISSILE)
               {
                  // send it back after the SOB who fired it
                  if(mo->tracer)
                     P_SetTarget(&mo->tracer, mo->target);
               }

               P_SetTarget(&mo->target, clip.BlockingMobj);
               return;
            }
            // explode a missile

            if(clip.ceilingline && clip.ceilingline->backsector &&
               clip.ceilingline->backsector->intflags & SIF_SKY)
            {
               if(demo_compatibility ||  // killough
                  mo->z > clip.ceilingline->backsector->ceilingheight)
               {
                  // Hack to prevent missiles exploding
                  // against the sky.
                  // Does not handle sky floors.

                  // haleyjd: in fact, doesn't handle sky ceilings either --
                  // this fix is for "sky hack walls" only apparently --
                  // see P_ExplodeMissile for my real sky fix

                  // [CG] Only servers remove actors.
                  if(serverside)
                  {
                     if(CS_SERVER)
                        SV_BroadcastActorRemoved(mo);
                     P_RemoveMobj(mo);
                  }
                  return;
               }
            }
            P_ExplodeMissile(mo);
         }
         else // whatever else it is, it is now standing still in (x,y)
         {
            mo->momx = mo->momy = 0;
         }
      }
   }
   while(xmove | ymove);

   // slow down

#if 0  // killough 10/98: this is unused code (except maybe in .deh files?)
   if(player && player->mo == mo && player->cheats & CF_NOMOMENTUM)
   {
      // debug option for no sliding at all
      mo->momx = mo->momy = 0;
      player->momx = player->momy = 0;         // killough 10/98
      return;
   }
#endif

   // no friction for missiles or skulls ever
   if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
      return;

   // no friction when airborne
   // haleyjd: OVER_UNDER
   if(mo->z > mo->floorz &&
      (comp[comp_overunder] || !(mo->intflags & MIF_ONMOBJ)))
      return;

   // killough 8/11/98: add bouncers
   // killough 9/15/98: add objects falling off ledges
   // killough 11/98: only include bouncers hanging off ledges
   if(((mo->flags & MF_BOUNCES && mo->z > mo->dropoffz) ||
       mo->flags & MF_CORPSE || mo->intflags & MIF_FALLING) &&
      (mo->momx > FRACUNIT/4 || mo->momx < -FRACUNIT/4 ||
       mo->momy > FRACUNIT/4 || mo->momy < -FRACUNIT/4) &&
      mo->floorz != mo->subsector->sector->floorheight)
      return;  // do not stop sliding if halfway off a step with some momentum

   // killough 11/98:
   // Stop voodoo dolls that have come to rest, despite any
   // moving corresponding player, except in old demos:

   if(mo->momx > -STOPSPEED && mo->momx < STOPSPEED &&
      mo->momy > -STOPSPEED && mo->momy < STOPSPEED &&
      (!player || !(player->cmd.forwardmove | player->cmd.sidemove) ||
      (player->mo != mo && demo_version >= 203)))
   {
      // if in a walking frame, stop moving

      // haleyjd 09/29/07: cleared fixme on gross hack
      // killough 10/98:
      // Don't affect main player when voodoo dolls stop, except in old demos:

      if(player && E_PlayerInWalkingState(player) &&
         (player->mo == mo || demo_version < 203))
         P_SetMobjState(player->mo, player->mo->info->spawnstate);

      mo->momx = mo->momy = 0;

      // killough 10/98: kill any bobbing momentum too (except in voodoo dolls)
      if(player && player->mo == mo)
         player->momx = player->momy = 0;
   }
   else
   {
      // haleyjd 04/11/10: BOOM friction compatibility
      if(demo_version <= 201)
      {
         // phares 3/17/98
         // Friction will have been adjusted by friction thinkers for icy
         // or muddy floors. Otherwise it was never touched and
         // remained set at ORIG_FRICTION
         mo->momx = FixedMul(mo->momx, mo->friction);
         mo->momy = FixedMul(mo->momy, mo->friction);
         mo->friction = ORIG_FRICTION; // reset to normal for next tic
      }
      else if(demo_version <= 202)
      {
         // phares 9/10/98: reduce bobbing/momentum when on ice & up against wall

         if((oldx == mo->x) && (oldy == mo->y)) // Did you go anywhere?
         {
            // No. Use original friction. This allows you to not bob so much
            // if you're on ice, but keeps enough momentum around to break free
            // when you're mildly stuck in a wall.
            mo->momx = FixedMul(mo->momx, ORIG_FRICTION);
            mo->momy = FixedMul(mo->momy, ORIG_FRICTION);
         }
         else
         {
            // Yes. Use stored friction.
            mo->momx = FixedMul(mo->momx,mo->friction);
            mo->momy = FixedMul(mo->momy,mo->friction);
         }
         mo->friction = ORIG_FRICTION; // reset to normal for next tic
      }
      else
      {
         fixed_t friction = P_GetFriction(mo, NULL);

         mo->momx = FixedMul(mo->momx, friction);
         mo->momy = FixedMul(mo->momy, friction);

         // killough 10/98: Always decrease player bobbing by ORIG_FRICTION.
         // This prevents problems with bobbing on ice, where it was not being
         // reduced fast enough, leading to all sorts of kludges being developed.

         if(player && player->mo == mo) // Not voodoo dolls
         {
            player->momx = FixedMul(player->momx, ORIG_FRICTION);
            player->momy = FixedMul(player->momy, ORIG_FRICTION);
         }
      }
   }
}

//
// P_PlayerHitFloor
//
// haleyjd: OVER_UNDER: Isolated code for players hitting floors/objects
//
void P_PlayerHitFloor(mobj_t *mo, boolean onthing)
{
   // Squat down.
   // Decrease viewheight for a moment
   // after hitting the ground (hard),
   // and utter appropriate sound.

   client_t *client = &clients[mo->player - players];

   mo->player->deltaviewheight = mo->momz >> 3;
   mo->player->jumptime = 10;

   // haleyjd 05/09/99 no oof when dead :)
   // [CG] Spectators don't oof either.  Also various sounds here are disabled
   //      while predicting.
   if((demo_version < 329 || mo->health > 0) && !client->spectating)
   {
      if(!comp[comp_fallingdmg] && demo_version >= 329)
      {
         // haleyjd: new features -- feet sound for normal hits,
         //          grunt for harder, falling damage for worse

         if(mo->momz < -23*FRACUNIT)
         {
            if(!mo->player->powers[pw_invulnerability] &&
               !(mo->player->cheats & CF_GODMODE))
            {
               P_FallingDamage(mo->player);
            }
            else
            {
               if(!cl_predicting)
                  S_StartSound(mo, GameModeInfo->playerSounds[sk_oof]);
            }
         }
         else if(mo->momz < -12 * FRACUNIT)
         {
            if(!cl_predicting)
               S_StartSound(mo, GameModeInfo->playerSounds[sk_oof]);
         }
         else if(onthing || !E_GetThingFloorType(mo, true)->liquid)
         {
            if(!cl_predicting)
               S_StartSound(mo, GameModeInfo->playerSounds[sk_plfeet]);
         }
      }
      else if(onthing || !E_GetThingFloorType(mo, true)->liquid)
      {
         if(!cl_predicting)
            S_StartSound(mo, GameModeInfo->playerSounds[sk_oof]);
      }
   }
}

//
// P_ZMovement
//
// Attempt vertical movement.

// [CG] Un-static'd
static void P_ZMovement(mobj_t* mo)
{
   // haleyjd: part of lost soul fix, moved up here for maximum
   //          scope
   boolean correct_lost_soul_bounce;
   boolean moving_down;

   // 10/13/05: fraggle says original DOOM has no bounce either,
   // so if gamemode != retail, no bounce.

   if(demo_compatibility) // v1.9 demos
   {
      correct_lost_soul_bounce =
         ((GameModeInfo->id == retail || GameModeInfo->id == commercial) &&
          GameModeInfo->missionInfo->id != doom2);
   }
   else if(demo_version < 331) // BOOM - EE v3.29
      correct_lost_soul_bounce = true;
   else // from now on...
      correct_lost_soul_bounce = !comp[comp_soul];

   // killough 7/11/98:
   // BFG fireballs bounced on floors and ceilings in Pre-Beta Doom
   // killough 8/9/98: added support for non-missile objects bouncing
   // (e.g. grenade, mine, pipebomb)

   if(mo->flags & MF_BOUNCES && mo->momz)
   {
      mo->z += mo->momz;

      if(mo->z <= mo->floorz)                  // bounce off floors
      {
         mo->z = mo->floorz;
         E_HitFloor(mo); // haleyjd
         if(mo->momz < 0)
         {
            mo->momz = -mo->momz;
            if(!(mo->flags & MF_NOGRAVITY))  // bounce back with decay
            {
               mo->momz = mo->flags & MF_FLOAT ?   // floaters fall slowly
                  mo->flags & MF_DROPOFF ?          // DROPOFF indicates rate
                     FixedMul(mo->momz, (fixed_t)(FRACUNIT*.85)) :
                     FixedMul(mo->momz, (fixed_t)(FRACUNIT*.70)) :
                     FixedMul(mo->momz, (fixed_t)(FRACUNIT*.45)) ;

               // Bring it to rest below a certain speed
               if(D_abs(mo->momz) <= mo->info->mass*(LevelInfo.gravity*4/256))
                  mo->momz = 0;
            }

            // killough 11/98: touchy objects explode on impact
            if(mo->flags & MF_TOUCHY && mo->intflags & MIF_ARMED &&
               mo->health > 0)
            {
               P_DamageMobj(mo, NULL, NULL, mo->health, MOD_UNKNOWN);
            }
            else if(mo->flags & MF_FLOAT && sentient(mo))
            {
               goto floater;
            }
            return;
         }
      }
      else if(mo->z >= mo->ceilingz - mo->height) // bounce off ceilings
      {
         mo->z = mo->ceilingz - mo->height;
         if(mo->momz > 0)
         {
            if(!(mo->subsector->sector->intflags & SIF_SKY))
               mo->momz = -mo->momz;    // always bounce off non-sky ceiling
            else
            {
               if(mo->flags & MF_MISSILE)
               {
                  // [CG] Only servers remove actors.
                  if(serverside)
                  {
                     if(CS_SERVER)
                        SV_BroadcastActorRemoved(mo);
                     P_RemoveMobj(mo);      // missiles don't bounce off skies
                  }
                  if(demo_version >= 331)
                     return; // haleyjd: return here for below fix
               }
               else if(mo->flags & MF_NOGRAVITY)
                  mo->momz = -mo->momz; // bounce unless under gravity
            }

            if(mo->flags & MF_FLOAT && sentient(mo))
               goto floater;

            // haleyjd 05/22/03: kludge for bouncers sticking to sky --
            // if momz is still positive, the thing is still trying
            // to go up, which it should not be doing, so apply some
            // gravity to get it out of this code segment gradually
            if(demo_version >= 331 && mo->momz > 0)
            {
               mo->momz -= mo->info->mass*(LevelInfo.gravity/256);
            }

            return;
         }
      }
      else
      {
         if(!(mo->flags & MF_NOGRAVITY))      // free-fall under gravity
            mo->momz -= mo->info->mass*(LevelInfo.gravity/256);
         if(mo->flags & MF_FLOAT && sentient(mo))
            goto floater;
         return;
      }

      // came to a stop
      mo->momz = 0;

      if(mo->flags & MF_MISSILE)
      {
         if(clip.ceilingline &&
            clip.ceilingline->backsector &&
            (mo->z > clip.ceilingline->backsector->ceilingheight) &&
            clip.ceilingline->backsector->intflags & SIF_SKY)
         {
            // [CG] Only servers remove actors.
            if(serverside)
            {
               if(CS_SERVER)
                  SV_BroadcastActorRemoved(mo);
               P_RemoveMobj(mo);  // don't explode on skies
            }
         }
         else
         {
            P_ExplodeMissile(mo);
         }
      }

      if(mo->flags & MF_FLOAT && sentient(mo))
         goto floater;
      return;
   }

   // killough 8/9/98: end bouncing object code

   // check for smooth step up

   if(mo->player &&
      mo->player->mo == mo &&  // killough 5/12/98: exclude voodoo dolls
      mo->z < mo->floorz)
   {
      mo->player->viewheight -= mo->floorz-mo->z;
      mo->player->deltaviewheight =
         (VIEWHEIGHT - mo->player->viewheight)>>3;
   }

   // adjust altitude
   mo->z += mo->momz;

floater:

   // float down towards target if too close

   if(!((mo->flags ^ MF_FLOAT) & (MF_FLOAT | MF_SKULLFLY | MF_INFLOAT)) &&
      mo->target)     // killough 11/98: simplify
   {
      fixed_t delta;
#ifdef R_LINKEDPORTALS
      if(P_AproxDistance(mo->x - getTargetX(mo), mo->y - getTargetY(mo)) <
         D_abs(delta = getTargetZ(mo) + (mo->height>>1) - mo->z)*3)
#else
      if(P_AproxDistance(mo->x - mo->target->x, mo->y - mo->target->y) <
         D_abs(delta = mo->target->z + (mo->height>>1) - mo->z)*3)
#endif
         mo->z += delta < 0 ? -FLOATSPEED : FLOATSPEED;
   }

   // clip movement
   if(mo->z <= mo->floorz)
   {
      // hit the floor

      /*
         A Mystery Unravelled - Discovered by cph -- haleyjd

         The code below was once below the point where mo->momz
         was set to zero. Someone added a comment and moved this
         code here.
      */
      if(correct_lost_soul_bounce && (mo->flags & MF_SKULLFLY))
         mo->momz = -mo->momz; // the skull slammed into something

      if((moving_down = (mo->momz < 0)))
      {
            // killough 11/98: touchy objects explode on impact
         if(mo->flags & MF_TOUCHY && mo->intflags & MIF_ARMED &&
            mo->health > 0)
         {
            P_DamageMobj(mo, NULL, NULL, mo->health, MOD_UNKNOWN);
         }
         else if(mo->player && // killough 5/12/98: exclude voodoo dolls
                 mo->player->mo == mo &&
                 mo->momz < -LevelInfo.gravity*8)
         {
            if(CS_SERVER)
               clients[mo->player - players].floor_status = cs_fs_hit;
            P_PlayerHitFloor(mo, false);
         }
         mo->momz = 0;
      }

      mo->z = mo->floorz;

      if(moving_down)
         E_HitFloor(mo);

      /* cph 2001/05/26 -
       * See lost soul bouncing comment above. We need this here for
       * bug compatibility with original Doom2 v1.9 - if a soul is
       * charging and is hit by a raising floor this incorrectly
       * reverses its Z momentum.
       */
      if(!correct_lost_soul_bounce && (mo->flags & MF_SKULLFLY))
         mo->momz = -mo->momz;

      if(!((mo->flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
      {
         if(!(mo->flags3 & MF3_FLOORMISSILE)) // haleyjd
            P_ExplodeMissile(mo);
         return;
      }
   }
   else if(mo->flags2 & MF2_LOGRAV) // haleyjd 04/09/99
   {
      if(!mo->momz)
         mo->momz = -(LevelInfo.gravity / 8) * 2;
      else
         mo->momz -= LevelInfo.gravity / 8;
   }
   else // still above the floor
   {
      if(!(mo->flags & MF_NOGRAVITY))
      {
         if(!mo->momz)
            mo->momz = -LevelInfo.gravity;
         mo->momz -= LevelInfo.gravity;
      }
   }

   // haleyjd 08/07/04: new footclip system
   P_AdjustFloorClip(mo);

   if(mo->z + mo->height > mo->ceilingz)
   {
      // hit the ceiling

      if(mo->momz > 0)
         mo->momz = 0;

      mo->z = mo->ceilingz - mo->height;

      if(mo->flags & MF_SKULLFLY)
         mo->momz = -mo->momz; // the skull slammed into something

      if(!((mo->flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
      {
         P_ExplodeMissile(mo);
         return;
      }
   }
}

//
// P_NightmareRespawn
//

void P_NightmareRespawn(mobj_t* mobj)
{
   fixed_t      x;
   fixed_t      y;
   fixed_t      z;
   subsector_t* ss;
   mobj_t*      mo;
   mapthing_t*  mthing;
   boolean      check; // haleyjd 11/11/04

   // [CG] Only servers do this.
   if(!serverside)
      return;

   x = mobj->spawnpoint.x << FRACBITS;
   y = mobj->spawnpoint.y << FRACBITS;

   // haleyjd: stupid nightmare respawning bug fix
   //
   // 08/09/00: This fixes the notorious nightmare respawning bug that
   // causes monsters that didn't spawn at level startup to respawn at
   // the point (0,0) regardless of that point's nature.

   if(!comp[comp_respawnfix] && demo_version >= 329 && x == 0 && y == 0)
   {
      // spawnpoint was zeroed out, so use point of death instead
      x = mobj->x;
      y = mobj->y;
   }

   // something is occupying its position?

   // haleyjd 11/11/04: This has been broken since BOOM when Lee changed
   // PIT_CheckThing to allow non-solid things to move through solid
   // things.

   if(demo_version >= 331)
      mobj->flags |= MF_SOLID;

   if(!comp[comp_overunder]) // haleyjd: OVER_UNDER
   {
      fixed_t sheight = mobj->height;

      // haleyjd 11/04/06: need to restore real height before checking
      mobj->height = P_ThingInfoHeight(mobj->info);

      check = P_CheckPositionExt(mobj, x, y);

      mobj->height = sheight;
   }
   else
      check = P_CheckPosition(mobj, x, y);

   if(demo_version >= 331)
      mobj->flags &= ~MF_SOLID;

   if(!check)
      return; // no respawn

   // spawn a teleport fog at old spot
   // because of removal of the body?
   mo = P_SpawnMobj(
      mobj->x,
      mobj->y,
      mobj->subsector->sector->floorheight + GameModeInfo->teleFogHeight,
      GameModeInfo->teleFogType
   );

   if(CS_SERVER)
      SV_BroadcastActorSpawned(mo);

   // initiate teleport sound
   S_StartSound(mo, GameModeInfo->teleSound);

   // spawn a teleport fog at the new spot
   ss = R_PointInSubsector(x,y);
   mo = P_SpawnMobj(
      x,
      y,
      ss->sector->floorheight + GameModeInfo->teleFogHeight,
      GameModeInfo->teleFogType
   );

   if(CS_SERVER)
      SV_BroadcastActorSpawned(mo);

   S_StartSound(mo, GameModeInfo->teleSound);

   // spawn the new monster
   mthing = &mobj->spawnpoint;
   z = mobj->info->flags & MF_SPAWNCEILING ? ONCEILINGZ : ONFLOORZ;

   // inherit attributes from deceased one
   mo = P_SpawnMobj(x, y, z, mobj->type);
   if(CS_SERVER)
      SV_BroadcastActorSpawned(mo);

   mo->spawnpoint = mobj->spawnpoint;
   // sf: use R_WadToAngle
   mo->angle = R_WadToAngle(mthing->angle);

   if(mthing->options & MTF_AMBUSH)
      mo->flags |= MF_AMBUSH;

   // killough 11/98: transfer friendliness from deceased
   mo->flags = (mo->flags & ~MF_FRIEND) | (mobj->flags & MF_FRIEND);

   mo->reactiontime = 18;

   // remove the old monster,
   if(CS_SERVER)
      SV_BroadcastActorRemoved(mobj);
   P_RemoveMobj(mobj);
}

// PTODO
#ifdef R_LINKEDPORTALS
static boolean P_CheckPortalTeleport(mobj_t *mobj)
{
   boolean ret = false;

   if(mobj->subsector->sector->f_pflags & PS_PASSABLE)
   {
      fixed_t passheight;
      linkdata_t *ldata = R_FPLink(mobj->subsector->sector);

      if(mobj->player)
      {
         P_CalcHeight(mobj->player);
         passheight = mobj->player->viewz;
      }
      else
         passheight = mobj->z + (mobj->height >> 1);


      if(passheight < ldata->planez)
      {
         linkoffset_t *link = P_GetLinkOffset(mobj->subsector->sector->groupid,
                                              ldata->toid);
         if(link)
         {
            EV_PortalTeleport(mobj, link);
            ret = true;
         }
      }
   }

   if(!ret && mobj->subsector->sector->c_pflags & PS_PASSABLE)
   {
      // Calculate the height at which the mobj should pass through the portal
      fixed_t passheight;
      linkdata_t *ldata = R_CPLink(mobj->subsector->sector);

      if(mobj->player)
      {
         P_CalcHeight(mobj->player);
         passheight = mobj->player->viewz;
      }
      else
         passheight = mobj->z + (mobj->height >> 1);

      if(passheight >= ldata->planez)
      {
         linkoffset_t *link = P_GetLinkOffset(mobj->subsector->sector->groupid,
                                              ldata->toid);
         if(link)
         {
            EV_PortalTeleport(mobj, link);
            ret = true;
         }
      }
   }

   return ret;
}
#endif


//
// P_MobjThinker
//
void P_MobjThinker(mobj_t *mobj)
{
   int oldwaterstate, waterstate = 0;
   fixed_t z;

   // killough 11/98:
   // removed old code which looked at target references
   // (we use pointer reference counting now)

   // haleyjd 05/23/08: increment counters
   if(mobj->alphavelocity)
   {
      mobj->translucency += mobj->alphavelocity;
      if(mobj->translucency < 0)
      {
         mobj->translucency = 0;
         if(mobj->flags3 & MF3_CYCLEALPHA)
            mobj->alphavelocity = -mobj->alphavelocity;
      }
      else if(mobj->translucency > FRACUNIT)
      {
         mobj->translucency = FRACUNIT;
         if(mobj->flags3 & MF3_CYCLEALPHA)
            mobj->alphavelocity = -mobj->alphavelocity;
      }
   }

   clip.BlockingMobj = NULL; // haleyjd 1/17/00: global hit reference

   // haleyjd 08/07/04: handle deep water plane hits
   if(mobj->subsector->sector->heightsec != -1)
   {
      sector_t *hs = &sectors[mobj->subsector->sector->heightsec];

      waterstate = (mobj->z < hs->floorheight);
   }

   // haleyjd 03/12/03: Heretic Wind transfer specials
   // haleyjd 03/19/06: moved here from P_XYMovement
   if(mobj->flags3 & MF3_WINDTHRUST && !(mobj->flags & MF_NOCLIP))
   {
      sector_t *sec = mobj->subsector->sector;

      if(sec->hticPushType >= 40 && sec->hticPushType <= 51)
         P_ThrustMobj(mobj, sec->hticPushAngle, sec->hticPushForce);
   }

   // momentum movement
   clip.BlockingMobj = NULL;
   if(mobj->momx | mobj->momy || mobj->flags & MF_SKULLFLY)
   {
      P_XYMovement(mobj);
      if(mobj->thinker.function == P_RemoveThinkerDelayed) // killough
         return;       // mobj was removed
   }

   if(comp[comp_overunder])
      clip.BlockingMobj = NULL;

   z = mobj->z;

   if(mobj->flags2 & MF2_FLOATBOB) // haleyjd
   {
      int idx = (mobj->floatbob + leveltime) & 63;

      mobj->z += FloatBobDiffs[idx];
      z = mobj->z - FloatBobOffsets[idx];
   }

   // haleyjd: OVER_UNDER: major changes
   if(mobj->momz || clip.BlockingMobj || z != mobj->floorz)
   {
      if(!comp[comp_overunder]  &&
         ((mobj->flags3 & MF3_PASSMOBJ) || (mobj->flags & MF_SPECIAL)))
      {
         mobj_t *onmo;

         if(!(onmo = P_GetThingUnder(mobj)))
         {
            P_ZMovement(mobj);
            mobj->intflags &= ~MIF_ONMOBJ;
         }
         else
         {
            // haleyjd 09/23/06: topdamage -- at last, we can have torches and
            // other hot objects burn things that stand on them :)
            if(demo_version >= 333 && onmo->info->topdamage > 0)
            {
               if(!(leveltime & onmo->info->topdamagemask) &&
                  (!mobj->player || !mobj->player->powers[pw_ironfeet]))
               {
                  P_DamageMobj(
                     mobj, onmo, onmo, onmo->info->topdamage, onmo->info->mod
                  );
               }
            }

            if(mobj->player && mobj == mobj->player->mo &&
               mobj->momz < -LevelInfo.gravity*8)
            {
               if(CS_SERVER)
               {
                  clients[mobj->player - players].floor_status =
                     cs_fs_hit_on_thing;
               }
               P_PlayerHitFloor(mobj, true);
            }

            if(onmo->z + onmo->height - mobj->z <= 24*FRACUNIT)
            {
               player_t *player = mobj->player;

               if(player && player->mo == mobj)
               {
                  fixed_t deltaview;
                  player->viewheight -= onmo->z + onmo->height - mobj->z;
                  deltaview = (VIEWHEIGHT - player->viewheight)>>3;
                  if(deltaview > player->deltaviewheight)
                  {
                     player->deltaviewheight = deltaview;
                  }
               }
               mobj->z = onmo->z + onmo->height;
            }
            mobj->intflags |= MIF_ONMOBJ;
            mobj->momz = 0;

            if(mobj->info->crashstate != NullStateNum
               && mobj->flags & MF_CORPSE
               && !(mobj->intflags & MIF_CRASHED))
            {
               mobj->intflags |= MIF_CRASHED;
               P_SetMobjState(mobj, mobj->info->crashstate);
            }
         }
      }
      else
      {
         P_ZMovement(mobj);
      }

      if(mobj->thinker.function == P_RemoveThinkerDelayed) // killough
         return;       // mobj was removed
   }
   else if(!(mobj->momx | mobj->momy) && !sentient(mobj))
   {                                  // non-sentient objects at rest
      mobj->intflags |= MIF_ARMED;    // arm a mine which has come to rest

      // killough 9/12/98: objects fall off ledges if they are hanging off
      // slightly push off of ledge if hanging more than halfway off

      if(mobj->z > mobj->dropoffz       &&  // Only objects contacting dropoff
         !(mobj->flags & MF_NOGRAVITY)  &&  // Only objects which fall
         !(mobj->flags2 & MF2_FLOATBOB) &&  // haleyjd: not floatbobbers
         !comp[comp_falloff] && demo_version >= 203) // Not in old demos
         P_ApplyTorque(mobj);               // Apply torque
      else
         mobj->intflags &= ~MIF_FALLING, mobj->gear = 0;  // Reset torque
   }

#ifdef R_LINKEDPORTALS
   P_CheckPortalTeleport(mobj);
#endif

   // haleyjd 11/06/05: handle crashstate here
   if(mobj->info->crashstate != NullStateNum
      && mobj->flags & MF_CORPSE
      && !(mobj->intflags & MIF_CRASHED)
      && mobj->z <= mobj->floorz)
   {
      mobj->intflags |= MIF_CRASHED;
      P_SetMobjState(mobj, mobj->info->crashstate);
   }

   // haleyjd 08/07/04: handle deep water plane hits
   oldwaterstate = waterstate;

   if(mobj->subsector->sector->heightsec != -1)
   {
      sector_t *hs = &sectors[mobj->subsector->sector->heightsec];

      waterstate = (mobj->z < hs->floorheight);
   }
   else
      waterstate = 0;

   if(mobj->flags & (MF_MISSILE|MF_BOUNCES))
   {
      // any time a missile or bouncer crosses, splash
      if(oldwaterstate != waterstate)
         E_HitWater(mobj, mobj->subsector->sector);
   }
   else if(oldwaterstate == 0 && waterstate != 0)
   {
      // normal things only splash going into the water
      E_HitWater(mobj, mobj->subsector->sector);
   }

   // cycle through states,
   // calling action functions at transitions
   // killough 11/98: simplify

   if(mobj->tics != -1) // you can cycle through multiple states in a tic
   {
      if(!--mobj->tics)
         P_SetMobjState(mobj, mobj->state->nextstate);
   }
   else
   {
      // haleyjd: minor restructuring, eternity features
      // A thing can respawn if:
      // 1) counts for kill AND
      // 2) respawn is on OR
      // 3) thing always respawns or removes itself after death.
      boolean can_respawn =
         mobj->flags & MF_COUNTKILL &&
           (respawnmonsters ||
            (mobj->flags2 & (MF2_ALWAYSRESPAWN | MF2_REMOVEDEAD)));

      // [CG] Only servers should run the rest of this function.
      if(!serverside)
         return;

      // haleyjd 07/13/05: increment mobj->movecount earlier
      if(can_respawn || mobj->effects & FX_FLIESONDEATH)
         ++mobj->movecount;

      // don't respawn dormant things
      // don't respawn norespawn things
      if(mobj->flags2 & (MF2_DORMANT | MF2_NORESPAWN))
         return;

      if(can_respawn && mobj->movecount >= mobj->info->respawntime &&
         !(leveltime & 31) && P_Random(pr_respawn) <= mobj->info->respawnchance)
      {
         // check for nightmare respawn
         if(mobj->flags2 & MF2_REMOVEDEAD)
         {
            if(CS_SERVER)
               SV_BroadcastActorRemoved(mobj);
            P_RemoveMobj(mobj);
         }
         else
            P_NightmareRespawn(mobj);
      }
   }
}

extern fixed_t tmsecfloorz;
extern fixed_t tmsecceilz;

//
// P_SpawnMobj
//
mobj_t *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
   mobj_t     *mobj;
   mobjinfo_t *info;
   state_t    *st;

   if(!CS_SPAWN_ACTOR_OK)
      I_Error("C/S clients cannot spawn actors themselves, exiting.\n");

   mobj = Z_Calloc(1, sizeof *mobj, PU_LEVEL, NULL);
   info = &mobjinfo[type];

   mobj->type    = type;
   mobj->info    = info;
   mobj->x       = x;
   mobj->y       = y;
   mobj->radius  = info->radius;
   mobj->height  = P_ThingInfoHeight(info); // phares
   mobj->flags   = info->flags;
   mobj->flags2  = info->flags2; // haleyjd
   mobj->flags3  = info->flags3; // haleyjd
   mobj->flags4  = info->flags4; // haleyjd
   mobj->effects = info->particlefx;  // haleyjd 07/13/03
   mobj->damage  = info->damage;      // haleyjd 08/02/04

#ifdef R_LINKEDPORTALS
   mobj->groupid = R_NOGROUP;
#endif

   // haleyjd 09/26/04: rudimentary support for monster skins
   if(info->altsprite != -1)
      mobj->skin = P_GetMonsterSkin(info->altsprite);

   // haleyjd: zdoom-style translucency level
   mobj->translucency  = info->translucency;
   mobj->alphavelocity = info->alphavelocity; // 5/23/08

   if(mobj->translucency != FRACUNIT)
   {
      // zdoom and BOOM style translucencies are mutually
      // exclusive, so unset the BOOM flag if the zdoom level is not
      // FRACUNIT
      mobj->flags &= ~MF_TRANSLUCENT;
   }

   // haleyjd 11/22/09: scaling
   mobj->xscale = info->xscale;
   mobj->yscale = info->yscale;

   //sf: not friends in deathmatch!

   // killough 8/23/98: no friends, bouncers, or touchy things in old demos
   if(demo_version < 203)
   {
      mobj->flags &= ~(MF_BOUNCES | MF_FRIEND | MF_TOUCHY);
   }
   else if(demo_version < 303 || !(GameType == gt_dm))
   {
      if(E_IsPlayerClassThingType(type)) // Except in old demos, players
         mobj->flags |= MF_FRIEND;       // are always friends.
   }

   mobj->health = info->spawnhealth;

   if(gameskill != sk_nightmare)
      mobj->reactiontime = info->reactiontime;

   mobj->lastlook = P_Random(pr_lastlook) % MAXPLAYERS;

   // do not set the state with P_SetMobjState,
   // because action routines can not be called yet

   st = states[info->spawnstate];

   mobj->state  = st;
   mobj->tics   = st->tics;
   if(mobj->skin && st->sprite == mobj->info->defsprite)
      mobj->sprite = mobj->skin->sprite;
   else
      mobj->sprite = st->sprite;
   mobj->frame  = st->frame;

   // set subsector and/or block links

   P_SetThingPosition(mobj);

   mobj->dropoffz =           // killough 11/98: for tracking dropoffs
      mobj->floorz = mobj->subsector->sector->floorheight;
   mobj->ceilingz = mobj->subsector->sector->ceilingheight;

   mobj->z = z == ONFLOORZ ? mobj->floorz : z == ONCEILINGZ ?
      mobj->ceilingz - mobj->height : z;

   // haleyjd 10/13/02: floatrand
   // haleyjd 03/16/09: changed to Heretic method
   if(z == FLOATRANDZ)
   {
      fixed_t space = (mobj->ceilingz - mobj->height) - mobj->floorz;
      if(space > 48 * FRACUNIT)
      {
         space -= 40 * FRACUNIT;
         mobj->z = ((space * P_Random(pr_spawnfloat)) >> 8)
                     + mobj->floorz + 40 * FRACUNIT;
      }
      else
         mobj->z = mobj->floorz;
   }

   // haleyjd: initialize floatbob seed
   if(mobj->flags2 & MF2_FLOATBOB)
   {
      mobj->floatbob = P_Random(pr_floathealth);
      mobj->z += FloatBobOffsets[(mobj->floatbob + leveltime - 1) & 63];
   }

   // haleyjd 08/07/04: new floorclip system
   P_AdjustFloorClip(mobj);

   mobj->thinker.function = P_MobjThinker;

   P_AddThinker(&mobj->thinker);

   // e6y
   mobj->friction = ORIG_FRICTION;

   // haleyjd 01/12/04: support translation lumps
   if(mobj->info->colour)
      mobj->colour = mobj->info->colour;
   else
   {
      // haleyjd 09/13/09: automatic DOOM->etc. translation
      if(mobj->flags4 & MF4_AUTOTRANSLATE && GameModeInfo->defTranslate)
      {
         // cache in mobj->info for next time
         mobj->info->colour = R_TranslationNumForName(GameModeInfo->defTranslate);
         mobj->colour = mobj->info->colour;
      }
      else
         mobj->colour = (info->flags & MF_TRANSLATION) >> MF_TRANSSHIFT;
   }

   // [CG] Some stuff for c/s.
   if(serverside || gamestate == GS_LOADING)
      CS_ObtainActorNetID(mobj);

   CS_SaveActorPosition(&mobj->old_position, mobj, gametic);

   return mobj;
}

static mapthing_t itemrespawnque[ITEMQUESIZE];
static int itemrespawntime[ITEMQUESIZE];
int iquehead, iquetail;

//
// P_RemoveMobj
//
void P_RemoveMobj(mobj_t *mobj)
{
   // haleyjd 04/14/03: restructured
   boolean respawnitem = false;

   if(!CS_REMOVE_ACTOR_OK)
      I_Error("C/S clients cannot remove actors themselves, exiting.\n");

   if((mobj->flags3 & MF3_SUPERITEM) && (dmflags & DM_RESPAWNSUPER))
      respawnitem = true; // respawning super powerups
   else if((dmflags & DM_BARRELRESPAWN) && mobj->type == E_ThingNumForDEHNum(MT_BARREL))
      respawnitem = true; // respawning barrels
   else
   {
      respawnitem =
         !((mobj->flags ^ MF_SPECIAL) & (MF_SPECIAL | MF_DROPPED)) &&
         !(mobj->flags3 & MF3_NOITEMRESP);
   }

   // [CG] Only servers place items in the respawn queue
   if(serverside)
   {
      if(respawnitem)
      {
         // haleyjd FIXME/TODO: spawnpoint is vulnerable to zeroing
         itemrespawnque[iquehead] = mobj->spawnpoint;
         itemrespawntime[iquehead++] = leveltime;
         // lose one off the end?
         if((iquehead &= ITEMQUESIZE - 1) == iquetail)
            iquetail = (iquetail + 1) & (ITEMQUESIZE - 1);
      }
   }

   CS_ReleaseActorNetID(mobj);

   // haleyjd 02/02/04: remove from tid hash
   P_RemoveThingTID(mobj);

   // unlink from sector and block lists
   P_UnsetThingPosition(mobj);

   // Delete all nodes on the current sector_list -- phares 3/16/98
   if(mobj->old_sectorlist)
      P_DelSeclist(mobj->old_sectorlist);

   // haleyjd 08/13/10: ensure that the object cannot be relinked, and
   // nullify old_sectorlist to avoid multiple release of msecnodes.
   if(demo_version > 337)
   {
      mobj->flags |= (MF_NOSECTOR | MF_NOBLOCKMAP);
      mobj->old_sectorlist = NULL;
   }

   // stop any playing sound
   S_StopSound(mobj, CHAN_ALL);

   // killough 11/98:
   //
   // Remove any references to other mobjs.
   //
   // Older demos might depend on the fields being left alone, however,
   // if multiple thinkers reference each other indirectly before the
   // end of the current tic.

   if(demo_version >= 203)
   {
      P_SetTarget(&mobj->target,    NULL);
      P_SetTarget(&mobj->tracer,    NULL);
      P_SetTarget(&mobj->lastenemy, NULL);
   }

   // free block
   P_RemoveThinker(&mobj->thinker);
}

//
// P_FindDoomedNum
//
// Finds a mobj type with a matching doomednum
//
// killough 8/24/98: rewrote to use hashing
//
int P_FindDoomedNum(int type)
{
   static struct dnumhash_s { int first, next; } *hash;
   register int i;

   if(!hash)
   {
      hash = Z_Malloc(sizeof(*hash) * NUMMOBJTYPES, PU_CACHE, (void **)&hash);
      for(i = 0; i < NUMMOBJTYPES; ++i)
         hash[i].first = NUMMOBJTYPES;
      for(i = 0; i < NUMMOBJTYPES; ++i)
      {
         if(mobjinfo[i].doomednum != -1)
         {
            unsigned h = (unsigned int) mobjinfo[i].doomednum % NUMMOBJTYPES;
            hash[i].next = hash[h].first;
            hash[h].first = i;
         }
      }
   }

   i = hash[type % NUMMOBJTYPES].first;
   while(i < NUMMOBJTYPES && mobjinfo[i].doomednum != type)
      i = hash[i].next;
   return i;
}

//
// P_RespawnSpecials
//
void P_RespawnSpecials(void)
{
   fixed_t x, y, z;
   subsector_t*  ss;
   mobj_t*       mo;
   mapthing_t*   mthing;
   int           i;

   // [CG] Only servers respawn specials.
   if(!serverside)
      return;

   if(!(dmflags & DM_ITEMRESPAWN) ||  // only respawn items in deathmatch
      iquehead == iquetail ||  // nothing left to respawn?
      leveltime - itemrespawntime[iquetail] < 30*35) // wait 30 seconds
      return;

   mthing = &itemrespawnque[iquetail];

   x = mthing->x << FRACBITS;
   y = mthing->y << FRACBITS;

   // spawn a teleport fog at the new spot

   // FIXME: Heretic support?

   ss = R_PointInSubsector(x,y);
   mo = P_SpawnMobj(x, y, ss->sector->floorheight , E_SafeThingType(MT_IFOG));
   S_StartSound(mo, sfx_itmbk);

   // [CG] Broadcast the item spawn fog.
   if(CS_SERVER)
      SV_BroadcastActorSpawned(mo);

   // find which type to spawn

   // killough 8/23/98: use table for faster lookup
   i = P_FindDoomedNum(mthing->type);

   // spawn it
   z = mobjinfo[i].flags & MF_SPAWNCEILING ? ONCEILINGZ : ONFLOORZ;

   mo = P_SpawnMobj(x, y, z, i);
   mo->spawnpoint = *mthing;
   // sf
   mo->angle = R_WadToAngle(mthing->angle);

   // [CG] Broadcast the item respawn.
   if(CS_SERVER)
      SV_BroadcastActorSpawned(mo);

   // pull it from the queue
   iquetail = (iquetail + 1) & (ITEMQUESIZE - 1);
}

//
// P_SpawnPlayer
//
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
//  between levels.
//
void P_SpawnPlayer(mapthing_t* mthing)
{
   player_t* p;
   fixed_t   x, y, z;
   mobj_t*   mobj;
   int       i;

   // not playing?

   if(!playeringame[mthing->type - 1])
      return;

   p = &players[mthing->type - 1];

   if(p->playerstate == PST_REBORN)
      G_PlayerReborn(mthing->type - 1);

   x    = mthing->x << FRACBITS;
   y    = mthing->y << FRACBITS;
   z    = ONFLOORZ;
   mobj = P_SpawnMobj(x, y, z, p->pclass->type);

   // sf: set color translations for player sprites
   mobj->colour = players[mthing->type - 1].colormap;

   mobj->angle      = R_WadToAngle(mthing->angle);
   mobj->player     = p;
   mobj->health     = p->health;

   // haleyjd: verify that the player skin is valid
   if(!p->skin)
      I_Error("P_SpawnPlayer: player skin undefined!\n");

   mobj->skin       = p->skin;
   mobj->sprite     = p->skin->sprite;

   p->mo            = mobj;
   p->playerstate   = PST_LIVE;
   p->refire        = 0;
   p->damagecount   = 0;
   p->bonuscount    = 0;
   p->extralight    = 0;
   p->fixedcolormap = 0;
   p->viewheight    = VIEWHEIGHT;
   p->viewz         = mobj->z + VIEWHEIGHT;

   p->momx = p->momy = 0;   // killough 10/98: initialize bobbing to 0.

   // setup gun psprite

   P_SetupPsprites(p);

   // give all cards in death match mode

   if(GameType == gt_dm)
   {
      for(i = 0; i < NUMCARDS; i++)
         p->cards[i] = true;
   }

   if(mthing->type-1 == consoleplayer)
   {
      ST_Start(); // wake up the status bar
      HU_Start(); // wake up the heads up text
   }

   // sf: wake up chasecam
   if(mthing->type - 1 == displayplayer)
      P_ResetChasecam();
}


//
// P_SpawnMapThing
//
// The fields of the mapthing should
// already be in host byte order.
//
// sf: made to return mobj_t* spawned
//
// haleyjd 03/03/07: rewritten again to use a unified mapthing_t type.
//
mobj_t *P_SpawnMapThing(mapthing_t *mthing)
{
   int    i;
   mobj_t *mobj;
   fixed_t x, y, z;

   switch(mthing->type)
   {
   case 0:             // killough 2/26/98: Ignore type-0 things as NOPs
   case DEN_PLAYER5:   // phares 5/14/98: Ignore Player 5-8 starts (for now)
   case DEN_PLAYER6:
   case DEN_PLAYER7:
   case DEN_PLAYER8:
      return NULL;
   case ED_CTRL_DOOMEDNUM:
      // haleyjd 10/08/03: ExtraData mapthing support
      return E_SpawnMapThingExt(mthing);
   }

   // killough 11/98: clear flags unused by Doom
   //
   // We clear the flags unused in Doom if we see flag mask 256 set, since
   // it is reserved to be 0 under the new scheme. A 1 in this reserved bit
   // indicates it's a Doom wad made by a Doom editor which puts 1's in
   // bits that weren't used in Doom (such as HellMaker wads). So we should
   // then simply ignore all upper bits.

   if(demo_compatibility ||
      (demo_version >= 203 && mthing->options & MTF_RESERVED))
      mthing->options &= MTF_EASY|MTF_NORMAL|MTF_HARD|MTF_AMBUSH|MTF_NOTSINGLE;

   // count deathmatch start positions

   if(mthing->type == 11)
   {
      // 1/11/98 killough -- new code removes limit on deathmatch starts:

      size_t offset = deathmatch_p - deathmatchstarts;

      if(offset >= num_deathmatchstarts)
      {
         num_deathmatchstarts = num_deathmatchstarts ?
            num_deathmatchstarts*2 : 16;
         deathmatchstarts = realloc(deathmatchstarts,
                                    num_deathmatchstarts *
                                    sizeof(*deathmatchstarts));
         deathmatch_p = deathmatchstarts + offset;
      }
      memcpy(deathmatch_p++, mthing, sizeof*mthing);
      return NULL; //sf
   }

   // haleyjd: now handled through IN_AddCameras
   // return NULL for old demos
   if(mthing->type == 5003 && demo_version < 331)
      return NULL;

   // check for players specially

   if(mthing->type <= 4 && mthing->type > 0) // killough 2/26/98 -- fix crashes
   {
      // killough 7/19/98: Marine's best friend :)
      if(GameType == gt_single &&
         mthing->type > 1 && mthing->type <= dogs+1 &&
         !players[mthing->type-1].secretcount)
      {
         // use secretcount to avoid multiple dogs in case of multiple starts
         players[mthing->type-1].secretcount = 1;

         // killough 10/98: force it to be a friend
         mthing->options |= MTF_FRIEND;

         i = E_SafeThingType(MT_DOGS);

         // haleyjd 9/22/99: [HELPER] bex block type substitution
         if(HelperThing != -1)
         {
            // haleyjd 07/05/03: adjusted for EDF
            if(HelperThing != NUMMOBJTYPES)
               i = HelperThing;
            else
               doom_printf(FC_ERROR "Invalid value for helper, ignored.");
         }

         goto spawnit;
      }

      // save spots for respawning in network games
      playerstarts[mthing->type-1] = *mthing;
      // [CG] Made some changes for c/s here.  The first thing to remember is
      //      that nearly all the code expects that a valid player resides at
      //      player[0], with a valid actor if gamestate is GS_LEVEL.  This
      //      means that c/s clients & servers will always start with
      //      consoleplayer set to 0, and that player will always be a
      //      spectator.  Servers cannot join a game per se, but they spawn
      //      themselves at playerstart 0 as a spectator.  Clients do the same
      //      when they connect to a server, but when they join they wait for
      //      the server to tell them where to spawn.
      if(!clientserver)
      {
         if(GameType != gt_dm)
            P_SpawnPlayer(mthing);
      }
      else if(mthing->type == 1)
      {
         // [CG] Only do this if this is the first time we're loading a map.
         //      Otherwise CS_DoWorldDone will do this for us.
         CS_SpawnPlayer(
            consoleplayer,
            mthing->x << FRACBITS,
            mthing->y << FRACBITS,
            ONFLOORZ,
            mthing->angle,
            true
         );
      }
      return NULL; //sf
   }

   // check for apropriate skill level

   //jff "not single" thing flag

   if(GameType == gt_single && (mthing->options & MTF_NOTSINGLE))
      return NULL; //sf

   //jff 3/30/98 implement "not deathmatch" thing flag

   if(GameType == gt_dm && (mthing->options & MTF_NOTDM))
      return NULL; //sf

   //jff 3/30/98 implement "not cooperative" thing flag

   if(GameType == gt_coop && (mthing->options & MTF_NOTCOOP))
      return NULL;  // sf

   // killough 11/98: simplify
   if(gameskill == sk_baby || gameskill == sk_easy ?
      !(mthing->options & MTF_EASY) :
      gameskill == sk_hard || gameskill == sk_nightmare ?
      !(mthing->options & MTF_HARD) : !(mthing->options & MTF_NORMAL))
      return NULL;  // sf

   // find which type to spawn

   // haleyjd: special thing types that need to undergo the processing
   // below must be caught here

   if(mthing->type >= 9027 && mthing->type <= 9033) // particle fountains
      i = E_SafeThingName("EEParticleFountain");
   else if(mthing->type >= 1200 && mthing->type < 1300) // enviro sequences
      i = E_SafeThingName("EEEnviroSequence");
   else if(mthing->type >= 1400 && mthing->type < 1500) // sector sequence
      i = E_SafeThingName("EESectorSequence");
   else if(mthing->type == 5080 || mthing->type == 5081)
   {
      CS_AddTeamStart(mthing);
      return NULL;
   }
   else if(mthing->type >= 14001 && mthing->type <= 14064) // ambience
      i = E_SafeThingName("EEAmbience");
   else
   {
      // killough 8/23/98: use table for faster lookup
      i = P_FindDoomedNum(mthing->type);
   }

   // phares 5/16/98:
   // Do not abort because of an unknown thing. Ignore it, but post a
   // warning message for the player.

   if(i == NUMMOBJTYPES)
   {
      // haleyjd: handle Doom Builder camera spots specially here, so that they
      // cannot desync demos recorded in BOOM-compatible ports
      if(mthing->type == 32000 && !(netgame || demorecording || demoplayback))
      {
         i = E_SafeThingName("DoomBuilderCameraSpot");
      }
      else
      {
         doom_printf(FC_ERROR "Unknown thing type %i at (%i, %i)",
                     mthing->type, mthing->x, mthing->y);

         // haleyjd 01/24/07: allow spawning unknowns to mark missing objects
         // at user's discretion, but not when recording/playing demos or in
         // netgames.
         if(markUnknowns && !(netgame || demorecording || demoplayback))
            i = UnknownThingType;
         else
            return NULL;  // sf
      }
   }

   // don't spawn keycards and players in deathmatch

   if(GameType == gt_dm && (mobjinfo[i].flags & MF_NOTDMATCH))
      return NULL;        // sf

   // don't spawn any monsters if -nomonsters

   if(nomonsters &&
      ((mobjinfo[i].flags3 & MF3_KILLABLE) ||
       (mobjinfo[i].flags & MF_COUNTKILL)))
      return NULL;        // sf

   // spawn it
spawnit:

   x = mthing->x << FRACBITS;
   y = mthing->y << FRACBITS;

   z = mobjinfo[i].flags & MF_SPAWNCEILING ? ONCEILINGZ : ONFLOORZ;

   // haleyjd 10/13/02: float rand z
   if(mobjinfo[i].flags2 & MF2_SPAWNFLOAT)
      z = FLOATRANDZ;

   mobj = P_SpawnMobj(x, y, z, i);

   // haleyjd 10/03/05: Hexen-format mapthing support

   // haleyjd 10/03/05: Hexen-style z positioning
   if(mthing->height && (z == ONFLOORZ || z == ONCEILINGZ))
   {
      fixed_t rheight = mthing->height << FRACBITS;

      if(z == ONCEILINGZ)
         rheight = -rheight;

      mobj->z += rheight;
      P_AdjustFloorClip(mobj);
   }

   // haleyjd 10/03/05: Hexen-style TID
   P_AddThingTID(mobj, mthing->tid);

   // haleyjd 10/03/05: Hexen-style args
   memcpy(mobj->args, mthing->args, 5 * sizeof(int));

   // TODO: special

   mobj->spawnpoint = *mthing;

   if(mobj->tics > 0 && !(mobj->flags4 & MF4_SYNCHRONIZED))
      mobj->tics = 1 + (P_Random(pr_spawnthing) % mobj->tics);

   if(!(mobj->flags & MF_FRIEND) &&
      mthing->options & MTF_FRIEND &&
      demo_version >= 203)
   {
      mobj->flags |= MF_FRIEND;            // killough 10/98:
      P_UpdateThinker(&mobj->thinker);     // transfer friendliness flag
   }

   // killough 7/20/98: exclude friends
   if(!((mobj->flags ^ MF_COUNTKILL) & (MF_FRIEND | MF_COUNTKILL)))
      totalkills++;

   if(mobj->flags & MF_COUNTITEM)
      totalitems++;

   mobj->angle = R_WadToAngle(mthing->angle);
   if(mthing->options & MTF_AMBUSH)
      mobj->flags |= MF_AMBUSH;

   // haleyjd: handling for dormant things
   if(mthing->options & MTF_DORMANT)
   {
      mobj->flags2 |= MF2_DORMANT;  // inert and invincible
      mobj->tics = -1; // don't go through state transitions
   }

   // haleyjd: set particle fountain color
   if(mthing->type >= 9027 && mthing->type <= 9033)
      mobj->effects |= (mthing->type - 9026) << FX_FOUNTAINSHIFT;

   // haleyjd: set environment sequence # for first 100 types
   if(mthing->type >= 1200 && mthing->type < 1300)
      mobj->args[0] = mthing->type - 1200;

   // haleyjd: handle sequence override objects -- 100 types
   if(mthing->type >= 1400 && mthing->type <= 1500)
   {
      subsector_t *subsec;

      if(mthing->type != 1500) // for non-1500, number is type-1400
         mobj->args[0] = mthing->type - 1400;
      else if(mobj->args[0] == 0) // for 1500, 0 means reset to default
         mobj->args[0] = -1;

      // set sector sequence id from mobj->args[0]
      subsec = R_PointInSubsector(mobj->x, mobj->y);
      subsec->sector->sndSeqID = mobj->args[0];
   }

   if(mthing->type == RED_FLAG_ID)
   {
      if(cs_flag_stands[team_color_red].exists)
      {
         I_Error(
            "Multiple %s flag stands, exiting.\n",
            team_color_names[team_color_red]
         );
      }
      cs_flag_stands[team_color_red].exists = true;
      cs_flag_stands[team_color_red].x = mobj->x;
      cs_flag_stands[team_color_red].y = mobj->y;
      cs_flag_stands[team_color_red].z = mobj->z;
   }
   else if(mthing->type == BLUE_FLAG_ID)
   {
      if(cs_flag_stands[team_color_blue].exists)
      {
         I_Error(
            "Multiple %s flag stands, exiting.\n",
            team_color_names[team_color_blue]
         );
      }
      cs_flag_stands[team_color_blue].exists = true;
      cs_flag_stands[team_color_blue].x = mobj->x;
      cs_flag_stands[team_color_blue].y = mobj->y;
      cs_flag_stands[team_color_blue].z = mobj->z;
   }

   // haleyjd: set ambience sequence # for first 64 types
   if(mthing->type >= 14001 && mthing->type <= 14064)
      mobj->args[0] = mthing->type - 14000;

   return mobj;
}

//
// GAME SPAWN FUNCTIONS
//

//
// P_SpawnPuff
//
mobj_t* P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z, angle_t dir,
                    int updown, boolean ptcl)
{
   mobj_t* th;

   // haleyjd 08/05/04: use new function
   z += P_SubRandom(pr_spawnpuff) << 10;

   th = P_SpawnMobj(x, y, z, E_SafeThingType(MT_PUFF));
   CS_ReleaseActorNetID(th);

   th->momz = FRACUNIT;
   th->tics -= P_Random(pr_spawnpuff) & 3;

   if(th->tics < 1)
      th->tics = 1;

   // don't make punches spark on the wall

   if(trace.attackrange == MELEERANGE)
      P_SetMobjState(th, E_SafeState(S_PUFF3));

   // haleyjd: for demo sync etc we still need to do the above, so
   // here we'll make the puff invisible and draw particles instead
   if(ptcl && drawparticles && bulletpuff_particle &&
      trace.attackrange != MELEERANGE)
   {
      if(bulletpuff_particle != 2)
         th->translucency = 0;
      P_SmokePuff(32, x, y, z, dir, updown);
   }

   return th;
}

//
// P_SpawnBlood
//
// [CG] Now returns the spawned blood instead of void.
mobj_t* P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, angle_t dir, int damage,
                     mobj_t *target)
{
   // HTIC_TODO: Heretic support
   mobj_t* th;

   // haleyjd 08/05/04: use new function
   z += P_SubRandom(pr_spawnblood) << 10;

   th = P_SpawnMobj(x, y, z, E_SafeThingType(MT_BLOOD));
   CS_ReleaseActorNetID(th);

   th->momz = FRACUNIT * 2;
   th->tics -= P_Random(pr_spawnblood) & 3;

   if(th->tics < 1)
      th->tics = 1;

   if(damage <= 12 && damage >= 9)
      P_SetMobjState(th, E_SafeState(S_BLOOD2));
   else if(damage < 9)
      P_SetMobjState(th, E_SafeState(S_BLOOD3));

   // for demo sync, etc, we still need to do the above, so
   // we'll make the sprites above invisible and draw particles
   // instead
   if(drawparticles && bloodsplat_particle)
   {
      if(bloodsplat_particle != 2)
         th->translucency = 0;
      P_BloodSpray(target, 32, x, y, z, dir);
   }

   return th;
}

// FIXME: These two functions are left over from an mobj-based
// particle system attempt in SMMU -- the particle line
// function could be useful for real particles maybe?

/*
void P_SpawnParticle(fixed_t x, fixed_t y, fixed_t z)
{
   P_SpawnMobj(x, y, z, MT_PARTICLE);
}


void P_ParticleLine(mobj_t *source, mobj_t *dest)
{
   fixed_t sourcex, sourcey, sourcez;
   fixed_t destx, desty, destz;
   int linedetail;
   int j;

   sourcex = source->x; sourcey = source->y;
   destx = dest->x; desty = dest->y;
   sourcez = source->z + (source->info->height/2);
   destz = dest->z + (dest->info->height/2);
   linedetail = P_AproxDistance(destx - sourcex, desty - sourcey)
      / FRACUNIT;

   // make the line
   for(j=0; j<linedetail; j++)
   {
      P_SpawnParticle(
         sourcex + ((destx - source->x)*j)/linedetail,
         sourcey + ((desty - source->y)*j)/linedetail,
         sourcez + ((destz - source->z)*j)/linedetail);
   }
}
*/

//
// P_CheckMissileSpawn
//
// Moves the missile forward a bit
//  and possibly explodes it right there.
//
void P_CheckMissileSpawn(mobj_t* th)
{
   // [CG] Only servers check missile spawns.
   if(!serverside)
      return;

   if(!(th->flags4 & MF4_NORANDOMIZE))
   {
      th->tics -= P_Random(pr_missile)&3;
      if(th->tics < 1)
         th->tics = 1;
   }

   // move a little forward so an angle can
   // be computed if it immediately explodes

   th->x += th->momx >> 1;
   th->y += th->momy >> 1;
   th->z += th->momz >> 1;

   // killough 8/12/98: for non-missile objects (e.g. grenades)
   if(!(th->flags & MF_MISSILE) && demo_version >= 203)
      return;

   // killough 3/15/98: no dropoff (really = don't care for missiles)
   if(!P_TryMove(th, th->x, th->y, false))
      P_ExplodeMissile(th);
}

//
// P_MissileMomz
//
// haleyjd 09/21/03: Missile z momentum calculation is now
// external to P_SpawnMissile, since it is also needed in a few
// other places.
//
fixed_t P_MissileMomz(fixed_t dx, fixed_t dy, fixed_t dz, int speed)
{
   int dist = P_AproxDistance(dx, dy);

   // haleyjd 11/17/03: a divide-by-zero error can occur here
   // if projectiles with zero speed are spawned -- use a suitable
   // default instead.
   if(speed == 0)
      speed = FRACUNIT;

   dist = dist / speed;

   if(dist < 1)
      dist = 1;

   return dz / dist;
}

//
// P_SpawnMissile
//
mobj_t* P_SpawnMissile(mobj_t* source, mobj_t* dest, mobjtype_t type,
                       fixed_t z)
{
   angle_t an;
   mobj_t *th;  // haleyjd: restructured

   if(z != ONFLOORZ)
      z -= source->floorclip;

   th = P_SpawnMobj(source->x, source->y, z, type);

   S_StartSound(th, th->info->seesound);

   P_SetTarget(&th->target, source); // where it came from // killough 11/98
   an = P_PointToAngle(source->x, source->y, dest->x, dest->y);

   // fuzzy player --  haleyjd: add total invisibility, ghost
   if(dest->flags & MF_SHADOW || dest->flags2 & MF2_DONTDRAW ||
      dest->flags3 & MF3_GHOST)
   {
      int shamt = (dest->flags3 & MF3_GHOST) ? 21 : 20; // haleyjd

      an += P_SubRandom(pr_shadow) << shamt;
   }

   th->angle = an;
   an >>= ANGLETOFINESHIFT;
   th->momx = FixedMul(th->info->speed, finecosine[an]);
   th->momy = FixedMul(th->info->speed, finesine[an]);
   th->momz = P_MissileMomz(dest->x - source->x,
                            dest->y - source->y,
                            dest->z - source->z,
                            th->info->speed);

   // [CG] Broadcast the missile spawned message.
   if(CS_SERVER)
      SV_BroadcastMissileSpawned(source, th);

   P_CheckMissileSpawn(th);

   return th;
}

//
// P_SpawnPlayerMissile
//
// Tries to aim at a nearby monster
//
mobj_t *P_SpawnPlayerMissile(mobj_t* source, mobjtype_t type)
{
   mobj_t *th;
   fixed_t x, y, z, slope = 0;

   // see which target is to be aimed at

   angle_t an = source->angle;

   // killough 7/19/98: autoaiming was not in original beta
   // sf: made a multiplayer option
   if(autoaim)
   {
      // killough 8/2/98: prefer autoaiming at enemies
      int mask = demo_version < 203 ? 0 : MF_FRIEND;
      do
      {
         slope = P_AimLineAttack(source, an, 16*64*FRACUNIT, mask);
         if(!clip.linetarget)
            slope = P_AimLineAttack(source, an += 1<<26, 16*64*FRACUNIT, mask);
         if(!clip.linetarget)
            slope = P_AimLineAttack(source, an -= 2<<26, 16*64*FRACUNIT, mask);
         if(!clip.linetarget)
         {
            an = source->angle;
            // haleyjd: use true slope angle
            slope = finetangent[(ANG90 - source->player->pitch)>>ANGLETOFINESHIFT];
         }
      }
      while(mask && (mask=0, !clip.linetarget));  // killough 8/2/98
   }
   else
   {
      // haleyjd: use true slope angle
      slope = finetangent[(ANG90 - source->player->pitch)>>ANGLETOFINESHIFT];
   }

   x = source->x;
   y = source->y;
   z = source->z + 4*8*FRACUNIT - source->floorclip;

   th = P_SpawnMobj(x, y, z, type);

   if(source->player && source->player->powers[pw_silencer] &&
      P_GetReadyWeapon(source->player)->flags & WPF_SILENCER)
   {
      S_StartSoundAtVolume(
         th, th->info->seesound, WEAPON_VOLUME_SILENCED, ATTN_NORMAL, CHAN_AUTO
      );
   }
   else
      S_StartSound(th, th->info->seesound);

   P_SetTarget(&th->target, source);   // killough 11/98
   th->angle = an;
   th->momx = FixedMul(th->info->speed,finecosine[an>>ANGLETOFINESHIFT]);
   th->momy = FixedMul(th->info->speed,finesine[an>>ANGLETOFINESHIFT]);
   th->momz = FixedMul(th->info->speed,slope);

   if(CS_SERVER)
      SV_BroadcastMissileSpawned(source, th);

   P_CheckMissileSpawn(th);

   return th;    //sf
}

//
// Start new Eternity mobj functions
//

mobj_t *P_SpawnMissileAngle(mobj_t *source, mobjtype_t type,
                            angle_t angle, fixed_t momz, fixed_t z)
{
   mobj_t *mo;

   if(z != ONFLOORZ)
      z -= source->floorclip;

   mo = P_SpawnMobj(source->x, source->y, z, type);

   S_StartSound(mo, mo->info->seesound);

   // haleyjd 09/21/03: don't set this directly!
   P_SetTarget(&mo->target, source);

   mo->angle = angle;
   angle >>= ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[angle]);
   mo->momy = FixedMul(mo->info->speed, finesine[angle]);
   mo->momz = momz;

   if(CS_SERVER)
      SV_BroadcastMissileSpawned(source, mo);

   P_CheckMissileSpawn(mo);

   return mo;
}

void P_Massacre(int friends)
{
   mobj_t *mo;
   thinker_t *think;

   for(think = thinkercap.next; think != &thinkercap; think = think->next)
   {
      if(think->function != P_MobjThinker)
         continue;

      mo = (mobj_t *)think;

      if((mo->flags & MF_COUNTKILL || mo->flags3 & MF3_KILLABLE) &&
         mo->health > 0)
      {
         switch(friends)
         {
         case 1: // kill only friends
            if(!(mo->flags & MF_FRIEND))
               continue;
            break;
         case 2: // kill only enemies
            if(mo->flags & MF_FRIEND)
               continue;
            break;
         default: // Everybody Dies (TM)
            break;
         }
         mo->flags2 &= ~MF2_INVULNERABLE; // haleyjd 04/09/99: none of that!
         P_DamageMobj(mo, NULL, NULL, 10000, MOD_UNKNOWN);
      }
   }
}

// haleyjd: falling damage

void P_FallingDamage(player_t *player)
{
   int mom, damage, dist;

   mom = D_abs(player->mo->momz);
   dist = FixedMul(mom, 16*FRACUNIT/23);

   if(mom > 63*FRACUNIT)
      damage = 10000; // instant death
   else
      damage = ((FixedMul(dist, dist)/10)>>FRACBITS)-24;

   // no-death threshold
   //   don't kill the player unless he's fallen more than this, or only
   //   has 1 point of health left (in which case he deserves it ;)
   if(player->mo->momz > -39*FRACUNIT && damage > player->mo->health &&
      player->mo->health != 1)
   {
      damage = player->mo->health - 1;
   }

   // [CG] Only servers will deal falling damage, so only set this flag if
   //      we're serverside.
   if(serverside)
   {
      if(damage >= player->mo->health)
         player->mo->intflags |= MIF_DIEDFALLING;
   }

   P_DamageMobj(player->mo, NULL, NULL, damage, MOD_FALLING);
}

//
// P_AdjustFloorClip
//
// Adapted from zdoom source, see the zdoom license.
// Thanks to Randy Heit.
//
void P_AdjustFloorClip(mobj_t *thing)
{
   fixed_t oldclip = thing->floorclip;
   fixed_t shallowestclip = 0x7fffffff;
   const msecnode_t *m;

   // absorb test for FOOTCLIP flag here
   // [CG] FIXME: Terrain types are disabled in c/s mode due to desyncs.
   if(comp[comp_terrain] || !(thing->flags2 & MF2_FOOTCLIP) || clientserver)
   {
      thing->floorclip = 0;
      return;
   }

   // find shallowest touching floor, discarding any deep water
   // involved (it does its own clipping)
   for(m = thing->touching_sectorlist; m; m = m->m_tnext)
   {
      if(m->m_sector->heightsec == -1 &&
         thing->z == m->m_sector->floorheight)
      {
         fixed_t clip = E_SectorFloorClip(m->m_sector);

         if(clip < shallowestclip)
            shallowestclip = clip;
      }
   }

   if(shallowestclip == 0x7fffffff)
      thing->floorclip = 0;
   else
      thing->floorclip = shallowestclip;

   // adjust the player's viewheight
   if(thing->player && oldclip != thing->floorclip)
   {
      player_t *p = thing->player;

      p->viewheight -= oldclip - thing->floorclip;
      p->deltaviewheight = (VIEWHEIGHT - p->viewheight) / 8;
   }
}

//
// P_ThingInfoHeight
//
// haleyjd 07/06/05:
//
// Function to retrieve proper thing height information for a thing.
//
int P_ThingInfoHeight(mobjinfo_t *mi)
{
   return
      ((demo_version >= 333 && !comp[comp_theights] &&
       mi->c3dheight) ?
       mi->c3dheight : mi->height);
}

//
// P_ChangeThingHeights
//
// Updates all things with appropriate height values depending on
// the conditions established in P_ThingInfoHeight above. Not safe
// if things are in over/under situations, but fine otherwise. This
// is used to keep the game consistent with the value of comp_theights.
//
void P_ChangeThingHeights(void)
{
   thinker_t *th;

   if(gamestate != GS_LEVEL)
      return;

   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(th->function == P_MobjThinker)
      {
         mobj_t *mo = (mobj_t *)th;

         mo->height = P_ThingInfoHeight(mo->info);
      }
   }
}

//
// haleyjd 02/02/04: Thing IDs (aka TIDs)
//

#define TIDCHAINS 131

// TID hash chains
static mobj_t *tidhash[TIDCHAINS];

//
// P_InitTIDHash
//
// Initializes the tid hash table.
//
void P_InitTIDHash(void)
{
   memset(tidhash, 0, TIDCHAINS*sizeof(mobj_t *));
}

//
// P_AddThingTID
//
// Adds a thing to the tid hash table
//
void P_AddThingTID(mobj_t *mo, int tid)
{
   // zero is no tid, and negative tids are reserved to
   // have special meanings
   if(tid <= 0)
   {
      mo->tid = 0;
      mo->tid_next = NULL;
      mo->tid_prevn = NULL;
   }
   else
   {
      int key = tid % TIDCHAINS;

      mo->tid = (uint16_t)tid;

      // insert at head of chain
      mo->tid_next  = tidhash[key];
      mo->tid_prevn = &tidhash[key];
      tidhash[key]  = mo;

      // connect to any existing things in chain
      if(mo->tid_next)
         mo->tid_next->tid_prevn = &(mo->tid_next);
   }
}

//
// P_RemoveThingTID
//
// Removes the given thing from the tid hash table if it is
// in it already.
//
void P_RemoveThingTID(mobj_t *mo)
{
   if(mo->tid > 0 && mo->tid_prevn)
   {
      // set previous thing's next field to this thing's next thing
      *(mo->tid_prevn) = mo->tid_next;

      // set next thing's prev field to this thing's prev field
      if(mo->tid_next)
         mo->tid_next->tid_prevn = mo->tid_prevn;
   }

   // clear tid
   mo->tid = 0;
}

//
// P_FindMobjFromTID
//
// Like line and sector tag search functions, this function will
// keep returning the next object with the same tid when called
// repeatedly with the previous call's return value. Returns NULL
// once the end of the chain is hit. Calling it again at that point
// would restart the search from the base of the chain.
//
// The last parameter only applies when this is called from a
// Small native function, and can be left null otherwise.
//
// haleyjd 06/10/06: eliminated infinite loop for TID_TRIGGER
//
mobj_t *P_FindMobjFromTID(int tid, mobj_t *rover, mobj_t *trigger)
{
   switch(tid)
   {
   // Reserved TIDs
   case 0:   // zero is "no tid"
      return NULL;

   case -1:  // players are -1 through -4
   case -2:
   case -3:
   case -4:
      {
         int pnum = abs(tid) - 1;

         return !rover && playeringame[pnum] ? players[pnum].mo : NULL;
      }

   case -10: // script trigger object (may be NULL, which is fine)
      return (!rover && trigger) ? trigger : NULL;

   // Normal TIDs
   default:
      if(tid < 0)
         return NULL;

      rover = rover ? rover->tid_next : tidhash[tid % TIDCHAINS];

      while(rover && rover->tid != tid)
         rover = rover->tid_next;

      return rover;
   }
}

//
// Thing Collections
//
// haleyjd: This pseudo-class is the ultimate generalization of the
// boss spawner spots code, allowing arbitrary reallocating arrays
// of mobj_t pointers to be maintained and manipulated. This is currently
// used by boss spawn spots, D'Sparil spots, and intermission cameras.
// I wish it was used by deathmatch spots, but that would present a
// compatibility problem (spawning mobj_t's at their locations would
// most likely result in demo desyncs).
//

//
// P_InitMobjCollection
//
// Sets up an MobjCollection object. This is for objects kept on the
// stack.
//
void P_InitMobjCollection(MobjCollection *mc, int type)
{
   memset(mc, 0, sizeof(MobjCollection));

   mc->type = type;
}

//
// P_ReInitMobjCollection
//
// Call this to set the number of mobjs in the collection to zero.
// Should be done for each global collection after a level ends and
// before beginning a new one. Doesn't molest the array pointer or
// numalloc fields. Resets the wrap iterator by necessity.
//
void P_ReInitMobjCollection(MobjCollection *mc, int type)
{
   mc->num = mc->wrapiterator = 0;
   mc->type = type;
}

//
// P_ClearMobjCollection
//
// Frees an mobj collection's pointer array and sets all the
// fields to zero.
//
void P_ClearMobjCollection(MobjCollection *mc)
{
   free(mc->ptrarray);

   memset(mc, 0, sizeof(MobjCollection));
}

//
// P_CollectThings
//
// Generalized from the boss brain spot code; builds a collection
// of mapthings of a certain type. The type must be set within the
// collection object before calling this.
//
void P_CollectThings(MobjCollection *mc)
{
   thinker_t *th;

   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(th->function == P_MobjThinker)
      {
         mobj_t *mo = (mobj_t *)th;

         if(mo->type == mc->type)
         {
            if(mc->num >= mc->numalloc)
            {
               mc->ptrarray = realloc(mc->ptrarray,
                  (mc->numalloc = mc->numalloc ?
                   mc->numalloc*2 : 32) * sizeof *mc->ptrarray);
            }
            (mc->ptrarray)[mc->num] = mo;
            mc->num++;
         }
      }
   }
}

//
// P_AddToCollection
//
// Adds a single object into an MobjCollection.
//
void P_AddToCollection(MobjCollection *mc, mobj_t *mo)
{
   if(mc->num >= mc->numalloc)
   {
      mc->ptrarray = realloc(mc->ptrarray,
         (mc->numalloc = mc->numalloc ?
         mc->numalloc*2 : 32) * sizeof *mc->ptrarray);
   }
   (mc->ptrarray)[mc->num] = mo;
   mc->num++;
}

//
// P_CollectionSort
//
// Sorts the pointers in an MobjCollection using the supplied callback function
// for determining collation order.
//
void P_CollectionSort(MobjCollection *mc, int (*cb)(const void *, const void *))
{
   if(mc->num > 1)
      qsort(mc->ptrarray, mc->num, sizeof(mobj_t *), cb);
}

//
// P_CollectionIsEmpty
//
// Returns true if there are no objects in the collection, and
// false otherwise.
//
boolean P_CollectionIsEmpty(MobjCollection *mc)
{
   return !mc->num;
}

//
// P_CollectionWrapIterator
//
// Returns each object in a collection in the order they were added
// at each consecutive call, wrapping to the beginning when the end
// is reached.
//
mobj_t *P_CollectionWrapIterator(MobjCollection *mc)
{
   mobj_t *ret = (mc->ptrarray)[mc->wrapiterator++];

   mc->wrapiterator %= mc->num;

   return ret;
}

//
// P_CollectionGetAt
//
// Gets the object at the specified index in the collection.
// Returns NULL if the index is out of bounds.
//
mobj_t *P_CollectionGetAt(MobjCollection *mc, unsigned int at)
{
   return at < (unsigned int)mc->num ? (mc->ptrarray)[at] : NULL;
}

//
// P_CollectionGetRandom
//
// Returns a random object from the collection using the specified
// random number generator for full compatibility.
//
mobj_t *P_CollectionGetRandom(MobjCollection *mc, pr_class_t rngnum)
{
   return (mc->ptrarray)[P_Random(rngnum) % mc->num];
}

#ifndef EE_NO_SMALL_SUPPORT
//
// Small natives
//

//
// sm_thingspawn
// * Implements ThingSpawn(type, x, y, z, tid = 0, angle = 0)
//
static cell AMX_NATIVE_CALL sm_thingspawn(AMX *amx, cell *params)
{
   int type, tid, ang;
   fixed_t x, y, z;
   angle_t angle;
   mobj_t *mo;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   type  = params[1];
   x     = params[2];
   y     = params[3];
   z     = params[4];
   tid   = params[5];
   ang   = params[6];

   // Type resolving can be done in Small itself, so we treat
   // this as a raw type. Allows the most flexibility without
   // duplicate functions.

   if(type < 0 || type >= NUMMOBJTYPES)
   {
      amx_RaiseError(amx, AMX_ERR_BOUNDS);
      return -1;
   }

   if(ang >= 360)
   {
      while(ang >= 360)
         ang = ang - 360;
   }
   else if(ang < 0)
   {
      while(ang < 0)
         ang = ang + 360;
   }

   angle = (angle_t)(((uint64_t)ang << 32) / 360);

   mo = P_SpawnMobj(x<<FRACBITS, y<<FRACBITS, z<<FRACBITS, type);

   P_AddThingTID(mo, tid);

   mo->angle = angle;

   return 0;
}

//
// sm_thingspawnspot
// * Implements ThingSpawnSpot(type, spottid, tid = 0, angle = 0)
//
static cell AMX_NATIVE_CALL sm_thingspawnspot(AMX *amx, cell *params)
{
   int type, spottid, tid, ang;
   angle_t angle;
   mobj_t *mo, *spawnspot = NULL;
   SmallContext_t *context = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   type    = params[1];
   spottid = params[2];
   tid     = params[3];
   ang     = params[4];

   // Type resolving can be done in Small itself, so we treat
   // this as a raw type. Allows the most flexibility without
   // duplicate functions.

   if(type < 0 || type >= NUMMOBJTYPES)
   {
      amx_RaiseError(amx, AMX_ERR_BOUNDS);
      return 0;
   }

   while(ang >= 360)
      ang = ang - 360;
   while(ang < 0)
      ang = ang + 360;

   angle = (angle_t)(((uint64_t)ang << 32) / 360);

   while((spawnspot = P_FindMobjFromTID(spottid, spawnspot, context->invocationData.trigger)))
   {
      mo = P_SpawnMobj(spawnspot->x, spawnspot->y, spawnspot->z, type);

      P_AddThingTID(mo, tid);

      mo->angle = angle;
   }

   return 0;
}

//
// sm_thingsound
// * Implements ThingSound(name[], tid)
//
static cell AMX_NATIVE_CALL sm_thingsound(AMX *amx, cell *params)
{
   int err, tid;
   char *sndname;
   mobj_t *mo = NULL;
   SmallContext_t *context = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   // get sound name
   if((err = SM_GetSmallString(amx, &sndname, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   // get tid
   tid = (int)params[2];

   while((mo = P_FindMobjFromTID(tid, mo, context->invocationData.trigger)))
      S_StartSoundName(mo, sndname);

   Z_Free(sndname);

   return 0;
}

//
// sm_thingsoundnum
// * Implements ThingSoundNum(num, tid)
//
static cell AMX_NATIVE_CALL sm_thingsoundnum(AMX *amx, cell *params)
{
   int tid, sndnum;
   mobj_t *mo = NULL;
   SmallContext_t *context = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   sndnum = (int)params[1];
   tid    = (int)params[2];

   while((mo = P_FindMobjFromTID(tid, mo, context->invocationData.trigger)))
   {
      S_StartSound(mo, sndnum);
   }

   return 0;
}

//
// sm_thinginfosound
// * Implements ThingInfoSound(type, tid)
//
// This function can play any of the internal thingtype sounds for
// an object. Will be very useful in custom behavior scripts.
//
static cell AMX_NATIVE_CALL sm_thinginfosound(AMX *amx, cell *params)
{
   int tid, typenum;
   mobj_t *mo = NULL;
   SmallContext_t *context = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   typenum = (int)params[1];
   tid     = (int)params[2];

   while((mo = P_FindMobjFromTID(tid, mo, context->invocationData.trigger)))
   {
      int sndnum = 0;

      switch(typenum)
      {
      case 0:
         sndnum = mo->info->seesound; break;
      case 1:
         sndnum = mo->info->activesound; break;
      case 2:
         sndnum = mo->info->attacksound; break;
      case 3:
         sndnum = mo->info->painsound; break;
      case 4:
         sndnum = mo->info->deathsound; break;
      default:
         break;
      }

      S_StartSound(mo, sndnum);
   }

   return 0;
}

//
// sm_thingmassacre
// * Implements ThingMassacre(type)
//
static cell AMX_NATIVE_CALL sm_thingmassacre(AMX *amx, cell *params)
{
   int massacreType = (int)params[1];

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   P_Massacre(massacreType);

   return 0;
}

//
// Thing property IDs as defined in Small
//
enum
{
   TF_TYPE,
   TF_TICS,
   TF_HEALTH,
   TF_SPECIAL1,
   TF_SPECIAL2,
   TF_SPECIAL3,
   TF_EFFECTS,
   TF_TRANSLUCENCY
};

//
// sm_thinggetproperty
// * Implements ThingGetProperty(tid, field)
//
static cell AMX_NATIVE_CALL sm_thinggetproperty(AMX *amx, cell *params)
{
   int value = 0, field, tid;
   mobj_t *mo = NULL;
   SmallContext_t *context = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   tid   = (int)params[1];
   field = (int)params[2];

   if((mo = P_FindMobjFromTID(tid, mo, context->invocationData.trigger)))
   {
      switch(field)
      {
      case TF_TYPE:         value = mo->type;         break;
      case TF_TICS:         value = mo->tics;         break;
      case TF_HEALTH:       value = mo->health;       break;
      case TF_SPECIAL1:     value = mo->counters[0];  break;
      case TF_SPECIAL2:     value = mo->counters[1];  break;
      case TF_SPECIAL3:     value = mo->counters[2];  break;
      case TF_EFFECTS:      value = mo->effects;      break;
      case TF_TRANSLUCENCY: value = mo->translucency; break;
      default:              value = 0;                break;
      }
   }

   return value;
}

//
// sm_thingsetproperty
// * Implements ThingSetProperty(tid, field, value)
//
static cell AMX_NATIVE_CALL sm_thingsetproperty(AMX *amx, cell *params)
{
   int field, value, tid;
   mobj_t *mo = NULL;
   SmallContext_t *context = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   tid        = (int)params[1];
   field      = (int)params[2];
   value      = (int)params[3];

   while((mo = P_FindMobjFromTID(tid, mo, context->invocationData.trigger)))
   {
      switch(field)
      {
      case TF_TICS:         mo->tics         = value; break;
      case TF_HEALTH:       mo->health       = value; break;
      case TF_SPECIAL1:     mo->counters[0]  = value; break;
      case TF_SPECIAL2:     mo->counters[1]  = value; break;
      case TF_SPECIAL3:     mo->counters[2]  = value; break;
      case TF_EFFECTS:      mo->effects      = value; break;
      case TF_TRANSLUCENCY: mo->translucency = value; break;
      }
   }

   return 0;
}

static cell AMX_NATIVE_CALL sm_thingflagsstr(AMX *amx, cell *params)
{
   int     tid, op, err;
   char   *flags;
   int    *results;
   mobj_t *mo = NULL;
   SmallContext_t *context = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   tid = (int)params[1];
   op  = (int)params[2];

   // get sound name
   if((err = SM_GetSmallString(amx, &flags, params[3])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return -1;
   }

   results = deh_ParseFlagsCombined(flags);

   while((mo = P_FindMobjFromTID(tid, mo, context->invocationData.trigger)))
   {
      switch(op)
      {
      case 0: // set flags
         mo->flags  = results[0];
         mo->flags2 = results[1];
         mo->flags3 = results[2];
         mo->flags4 = results[3];
         break;
      case 1: // add flags
         mo->flags  |= results[0];
         mo->flags2 |= results[1];
         mo->flags3 |= results[2];
         mo->flags4 |= results[3];
         break;
      case 2: // remove flags
         mo->flags  &= ~(results[0]);
         mo->flags2 &= ~(results[1]);
         mo->flags3 &= ~(results[2]);
         mo->flags4 &= ~(results[3]);
         break;
      }
   }

   free(flags);

   return 0;
}

//
// sm_thingsetfriend
// * Implements _ThingSetFriend(tid, friend)
//
static cell AMX_NATIVE_CALL sm_thingsetfriend(AMX *amx, cell *params)
{
   int tid, friendly;
   mobj_t *mo = NULL;
   SmallContext_t *context = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   tid      = params[1];
   friendly = params[2];

   while((mo = P_FindMobjFromTID(tid, mo, context->invocationData.trigger)))
   {
      switch(friendly)
      {
      case 0: // make enemy
         mo->flags &= ~MF_FRIEND;
         break;
      case 1: // make friend
         mo->flags |= MF_FRIEND;
         break;
      case 2: // toggle state
         mo->flags ^= MF_FRIEND;
         break;
      }

      P_UpdateThinker(&mo->thinker);
   }

   return 0;
}

//
// sm_thingisfriend
// * Implements _ThingIsFriend(tid)
//
static cell AMX_NATIVE_CALL sm_thingisfriend(AMX *amx, cell *params)
{
   int tid;
   boolean friendly = false;
   mobj_t *mo = NULL;
   SmallContext_t *ctx = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   tid = params[1];

   if((mo = P_FindMobjFromTID(tid, mo, ctx->invocationData.trigger)))
      friendly = ((mo->flags & MF_FRIEND) == MF_FRIEND);

   return friendly;
}

//
// sm_thingthrust3f    -- joek 7/6/06
// * Implements _ThingThrust3f(tid, x, y, z)
//
// haleyjd: changed to be "3f" version and to use fixed_t params.
//
static cell AMX_NATIVE_CALL sm_thingthrust3f(AMX *amx, cell *params)
{
   int tid;
   fixed_t x, y, z;
   mobj_t *mo = NULL;
   SmallContext_t *context = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   tid = params[1];
   x   = (fixed_t)params[2];
   y   = (fixed_t)params[3];
   z   = (fixed_t)params[4];

   while((mo = P_FindMobjFromTID(tid, mo, context->invocationData.trigger)))
   {
      mo->momx += x;
      mo->momy += y;
      mo->momz += z;
   }

   return 0;
}

//
// sm_thingthrust
//
// haleyjd 06/08/06:
// * Implements _ThingThrust(angle, force, tid)
//
static cell AMX_NATIVE_CALL sm_thingthrust(AMX *amx, cell *params)
{
   mobj_t  *mo   = NULL;
   angle_t angle = FixedToAngle((fixed_t)params[1]);
   fixed_t force = (fixed_t)params[2];
   int     tid   = params[3];
   SmallContext_t *context = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   while((mo = P_FindMobjFromTID(tid, mo, context->invocationData.trigger)))
      P_ThrustMobj(mo, angle, force);

   return 0;
}

// thing position enum values
enum
{
   TPOS_X,
   TPOS_Y,
   TPOS_Z,
   TPOS_ANGLE,
   TPOS_MOMX,
   TPOS_MOMY,
   TPOS_MOMZ,
   TPOS_FLOORZ,
   TPOS_CEILINGZ,
};

//
//  sm_thinggetpos() -- joek 7/6/06
//  * Implements _ThingGetPos(tid, valuetoget)
//
static cell AMX_NATIVE_CALL sm_thinggetpos(AMX *amx, cell *params)
{
   int tid, valuetoget;
   mobj_t *mo = NULL;
   SmallContext_t *context = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   tid = params[1];
   valuetoget = params[2];

   if((mo = P_FindMobjFromTID(tid, mo, context->invocationData.trigger)))
   {
      switch(valuetoget)
      {
      case TPOS_X:
         return (cell)mo->x;
      case TPOS_Y:
         return (cell)mo->y;
      case TPOS_Z:
         return (cell)mo->z;
      case TPOS_ANGLE:
         return (cell)AngleToFixed(mo->angle);
      case TPOS_MOMX:
         return (cell)mo->momx;
      case TPOS_MOMY:
         return (cell)mo->momy;
      case TPOS_MOMZ:
         return (cell)mo->momz;
      case TPOS_FLOORZ:
         return (cell)mo->floorz;
      case TPOS_CEILINGZ:
         return (cell)mo->ceilingz;
      default:
         return 0;
      }
   }

   return 0;
}

//
//  sm_getfreetid()	-- joek 7/6/06
//  * Implements _GetFreeTID()
//  - Returns a free TID
//
static cell AMX_NATIVE_CALL sm_getfreetid(AMX *amx, cell *params)
{
   int tid;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   for(tid = 1; tid <= 65535; ++tid)
   {
      if(P_FindMobjFromTID(tid, NULL, NULL) == NULL)
         return tid;
   }

   return 0;
}

enum {
   TELE_NORMAL,
   TELE_SILENT,
};

//
// sm_thingteleport    -- joek 6/15/07
// * implements _ThingTeleport(tid, x, y, z, silent)
//
static cell sm_thingteleport(AMX *amx, cell *params)
{
   int tid = params[1];
   fixed_t x = params[2];
   fixed_t y = params[3];
   fixed_t z = params[4];
   int silent = params[5];

   fixed_t oldx;
   fixed_t oldy;
   fixed_t oldz;

   mobj_t *mo = NULL;
   SmallContext_t *context = SM_GetContextForAMX(amx);

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   // work on all mobjs of the given tid
   while((mo = P_FindMobjFromTID(tid, mo, context->invocationData.trigger)))
   {
      oldx = mo->x;
      oldy = mo->y;
      oldz = mo->z;

      P_TeleportMove(mo, x, y, false);
      mo->z = z;

      // some crap from EV_Teleport i thought might be useful :P
      P_AdjustFloorClip(mo);

      // If the teleport isn't silent, spawn the telefog and make a nice sound :3
      if(silent == TELE_NORMAL)
      {
         // spawn teleport fog and emit sound at source
         S_StartSound(P_SpawnMobj(oldx, oldy,
                  oldz + GameModeInfo->teleFogHeight,
                  GameModeInfo->teleFogType),
                  GameModeInfo->teleSound);

         // spawn teleport fog and emit sound at destination
         S_StartSound(P_SpawnMobj(mo->x + 20*finecosine[mo->angle>>ANGLETOFINESHIFT],
                  mo->y + 20*finesine[mo->angle>>ANGLETOFINESHIFT],
                  mo->z + GameModeInfo->teleFogHeight,
                  GameModeInfo->teleFogType),
                  GameModeInfo->teleSound);
      }
   }

   return 0;
}

AMX_NATIVE_INFO mobj_Natives[] =
{
   { "_ThingSpawn",        sm_thingspawn },
   { "_ThingSpawnSpot",    sm_thingspawnspot },
   { "_ThingSound",        sm_thingsound },
   { "_ThingSoundNum",     sm_thingsoundnum },
   { "_ThingInfoSound",    sm_thinginfosound },
   { "_ThingMassacre",     sm_thingmassacre },
   { "_ThingGetProperty",  sm_thinggetproperty },
   { "_ThingSetProperty",  sm_thingsetproperty },
   { "_ThingFlagsFromStr", sm_thingflagsstr },
   { "_ThingSetFriend",    sm_thingsetfriend },
   { "_ThingIsFriend",     sm_thingisfriend },
   { "_ThingThrust3f",     sm_thingthrust3f },
   { "_ThingThrust",       sm_thingthrust },
   { "_ThingGetPos",       sm_thinggetpos },
   { "_GetFreeTID",        sm_getfreetid },
   { "_ThingTeleport",     sm_thingteleport },
   { NULL,                 NULL }
};
#endif

//----------------------------------------------------------------------------
//
// $Log: p_mobj.c,v $
// Revision 1.26  1998/05/16  00:24:12  phares
// Unknown things now flash warning msg instead of causing abort
//
// Revision 1.25  1998/05/15  00:33:19  killough
// Change function used in thing deletion check
//
// Revision 1.24  1998/05/14  08:01:56  phares
// Added Player Starts 5-8 (4001-4004)
//
// Revision 1.23  1998/05/12  12:47:21  phares
// Removed OVER UNDER code
//
// Revision 1.22  1998/05/12  06:09:32  killough
// Prevent voodoo dolls from causing player bopping
//
// Revision 1.21  1998/05/07  00:54:23  killough
// Remove dependence on evaluation order, fix (-1) ptr bug
//
// Revision 1.20  1998/05/05  15:35:16  phares
// Documentation and Reformatting changes
//
// Revision 1.19  1998/05/03  23:16:49  killough
// Remove unnecessary declarations, fix #includes
//
// Revision 1.18  1998/04/27  02:02:12  killough
// Fix crashes caused by mobjs targeting deleted thinkers
//
// Revision 1.17  1998/04/10  06:35:56  killough
// Fix mobj state machine cycle hangs
//
// Revision 1.16  1998/03/30  12:05:57  jim
// Added support for not-dm not-coop thing flags
//
// Revision 1.15  1998/03/28  18:00:58  killough
// Remove old dead code which is commented out
//
// Revision 1.14  1998/03/23  15:24:30  phares
// Changed pushers to linedef control
//
// Revision 1.13  1998/03/20  00:30:06  phares
// Changed friction to linedef control
//
// Revision 1.12  1998/03/16  12:43:41  killough
// Use new P_TryMove() allowing dropoffs in certain cases
//
// Revision 1.11  1998/03/12  14:28:46  phares
// friction and IDCLIP changes
//
// Revision 1.10  1998/03/11  17:48:28  phares
// New cheats, clean help code, friction fix
//
// Revision 1.9  1998/03/09  18:27:04  phares
// Fixed bug in neighboring variable friction sectors
//
// Revision 1.8  1998/03/04  07:40:04  killough
// comment out noclipping hack for now
//
// Revision 1.7  1998/02/26  21:15:30  killough
// Fix thing type 0 crashes, e.g. MAP25
//
// Revision 1.6  1998/02/24  09:20:11  phares
// Removed 'onground' local variable
//
// Revision 1.5  1998/02/24  08:46:21  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.4  1998/02/23  04:46:21  killough
// Preserve no-clipping cheat across idclev
//
// Revision 1.3  1998/02/17  05:47:11  killough
// Change RNG calls to use keys for each block
//
// Revision 1.2  1998/01/26  19:24:15  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:00  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

