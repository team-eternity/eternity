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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Moving object handling. Spawn functions.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "a_args.h"
#include "a_small.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_mod.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "e_inventory.h"
#include "e_player.h"
#include "e_states.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "info.h"
#include "in_lude.h"
#include "m_random.h"
#include "p_chase.h"
#include "p_enemy.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_map3d.h"
#include "p_partcl.h"
#include "p_portal.h"
#include "p_saveg.h"
#include "p_sector.h"
#include "p_skin.h"
#include "p_tick.h"
#include "p_spec.h"    // haleyjd 04/05/99: TerrainTypes
#include "p_user.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_pcheck.h"
#include "r_portal.h"
#include "r_state.h"
#include "sounds.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_misc.h"
#include "v_video.h"

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

//=============================================================================
// 
// PointThinker Methods
//

IMPLEMENT_THINKER_TYPE(PointThinker)

//
// PointThinker::serialize
//
// Saves/loads a PointThinker.
//
void PointThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << x << y << z << groupid;
}

//
// PointThinker::removeThinker
//
// Stop any sounds related to this PointThinker instance, and then invoke the
// parent implementation.
//
void PointThinker::removeThinker()
{
   // stop any playing sound
   S_StopSound(this, CHAN_ALL);
   Super::removeThinker();
}

//=============================================================================
//
// Mobj
//

// haleyjd 03/27/10: new solution for state cycle detection
struct seenstate_t
{
   DLListItem<seenstate_t> link;
   int statenum;
};

// haleyjd 03/27/10: freelist of seenstate objects
static DLListItem<seenstate_t> *seenstate_freelist;

//
// P_InitSeenStates
//
// haleyjd 03/27/10: creates an initial pool of seenstate objects.
//
static void P_InitSeenStates(void)
{
   seenstate_t *newss;
   int i;

   newss = ecalloc(seenstate_t *, 32, sizeof(seenstate_t));

   for(i = 0; i < 32; ++i)
      newss[i].link.insert(&newss[i], &seenstate_freelist);
}

//
// P_FreeSeenStates
//
// haleyjd 03/27/10: puts a set of seenstates back onto the freelist
//
static void P_FreeSeenStates(DLListItem<seenstate_t> *list)
{
   DLListItem<seenstate_t> *oldhead = seenstate_freelist;
   DLListItem<seenstate_t> *newtail = list;

   // find tail of list of objects to free
   while(newtail->dllNext)
      newtail = newtail->dllNext;

   // attach current freelist to tail
   newtail->dllNext = oldhead;
   if(oldhead)
      oldhead->dllPrev = &newtail->dllNext;

   // attach new list to seenstate_freelist
   seenstate_freelist = list;
   list->dllPrev = &seenstate_freelist;
}

//
// P_GetSeenState
//
// Gets a seenstate from the free list if one is available, or creates a
// new one.
//
static seenstate_t *P_GetSeenState()
{
   seenstate_t *ret;

   if(seenstate_freelist)
   {
      DLListItem<seenstate_t> *item = seenstate_freelist;

      item->remove();

      ret = *item;
      memset(ret, 0, sizeof(seenstate_t));
   }
   else
      ret = ecalloc(seenstate_t *, 1, sizeof(seenstate_t));

   return ret;
}

//
// P_AddSeenState
//
// Marks a new state as having been seen.
//
static void P_AddSeenState(int statenum, DLListItem<seenstate_t> **list)
{
   seenstate_t *newss = P_GetSeenState();

   newss->statenum = statenum;

   newss->link.insert(newss, list);
}

//
// P_CheckSeenState
//
// Checks if the given state has been seen
//
static bool P_CheckSeenState(int statenum, DLListItem<seenstate_t> *list)
{
   DLListItem<seenstate_t> *link = list;

   while(link)
   {
      if(statenum == (*link)->statenum)
         return true;

      link = link->dllNext;
   }

   return false;
}

//
// P_SetMobjState
//
// Returns true if the mobj is still present.
//
bool P_SetMobjState(Mobj* mobj, statenum_t state)
{
   state_t *st;

   // haleyjd 03/27/10: new state cycle detection
   static bool firsttime = true; // for initialization
   DLListItem<seenstate_t> *seenstates  = NULL; // list of seenstates for this instance
   bool ret = true;                           // return value

   if(firsttime)
   {
      P_InitSeenStates();
      firsttime = false;
   }

   do
   {
      if(state == NullStateNum)
      {
         mobj->state = NULL;
         mobj->removeThinker();
         ret = false;
         break;                 // killough 4/9/98
      }

      st = states[state];
      mobj->state = st;
      mobj->tics = st->tics;

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
      {
         actionargs_t actionargs;

         actionargs.actiontype = actionargs_t::MOBJFRAME;
         actionargs.actor      = mobj;
         actionargs.args       = st->args;
         actionargs.pspr       = NULL;

         st->action(&actionargs);
      }

      // haleyjd 05/20/02: run particle events
      if(st->particle_evt)
         P_RunEvent(mobj);

      P_AddSeenState(state, &seenstates);

      state = st->nextstate;
   }
   while(!mobj->tics && !P_CheckSeenState(state, seenstates));
   
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
bool P_SetMobjStateNF(Mobj *mobj, statenum_t state)
{
   state_t *st;

   if(state == NullStateNum)
   {
      // remove mobj
      mobj->state = NULL;
      mobj->removeThinker();
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

   return true;
}


//
// P_ExplodeMissile
//
void P_ExplodeMissile(Mobj *mo)
{
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
         mo->removeThinker(); // don't explode on the actual sky itself
         return;
      }
   }

   P_SetMobjState(mo, mobjinfo[mo->type]->deathstate);

   if(!(mo->flags4 & MF4_NORANDOMIZE))
   {
      mo->tics -= P_Random(pr_explode) & 3;

      if(mo->tics < 1)
         mo->tics = 1;
   }

   mo->flags &= ~MF_MISSILE;

   S_StartSound(mo, mo->info->deathsound);

   // haleyjd: disable any particle effects
   mo->effects = 0;
}

void P_ThrustMobj(Mobj *mo, angle_t angle, fixed_t move)
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
//
void P_XYMovement(Mobj* mo)
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

      if(xmove > MAXMOVE/2 || ymove > MAXMOVE/2 ||  // killough 8/9/98:
         ((xmove < -MAXMOVE/2 || ymove < -MAXMOVE/2) && demo_version >= 203))
      {
         ptryx = mo->x + xmove/2;
         ptryy = mo->y + ymove/2;
         xmove >>= 1;
         ymove >>= 1;
      }
      else
      {
         ptryx = mo->x + xmove;
         ptryy = mo->y + ymove;
         xmove = ymove = 0;
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
            if (clip.blockline)
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
                     P_SetTarget<Mobj>(&mo->tracer, mo->target);
               }

               P_SetTarget<Mobj>(&mo->target, clip.BlockingMobj);
               return;
            }
            // explode a missile

            if(clip.ceilingline && clip.ceilingline->backsector &&
               clip.ceilingline->backsector->intflags & SIF_SKY)
            {
               if (demo_compatibility ||  // killough
                  mo->z > clip.ceilingline->backsector->ceilingheight)
               {
                  // Hack to prevent missiles exploding
                  // against the sky.
                  // Does not handle sky floors.

                  // haleyjd: in fact, doesn't handle sky ceilings either --
                  // this fix is for "sky hack walls" only apparently --
                  // see P_ExplodeMissile for my real sky fix

                  mo->removeThinker();
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
   // 06/5/12: flying players
   if(mo->z > mo->floorz && !(mo->flags4 & MF4_FLY) &&
      (!P_Use3DClipping() || !(mo->intflags & MIF_ONMOBJ)))
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

      // haleyjd 09/29/07: use E_PlayerInWalkingState
      // killough 10/98:
      // Don't affect main player when voodoo dolls stop, except in old demos:

      if(player && E_PlayerInWalkingState(player) && 
         (player->mo == mo || demo_version < 203))
         P_SetMobjState(player->mo, player->mo->info->spawnstate);

      mo->momx = mo->momy = 0;

      // killough 10/98: kill any bobbing momentum too (except in voodoo dolls)
      if (player && player->mo == mo)
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

         if ((oldx == mo->x) && (oldy == mo->y)) // Did you go anywhere?
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
void P_PlayerHitFloor(Mobj *mo, bool onthing)
{
   // Squat down.
   // Decrease viewheight for a moment
   // after hitting the ground (hard),
   // and utter appropriate sound.

   // haleyjd 06/05/12: not when flying
   if(mo->flags4 & MF4_FLY)
      return;

   mo->player->deltaviewheight = mo->momz >> 3;
   mo->player->jumptime = 10;

   // haleyjd 05/09/99 no oof when dead :)
   if(demo_version < 329 || mo->health > 0)
   {
      if(!comp[comp_fallingdmg] && demo_version >= 329)
      {
         // haleyjd: new features -- feet sound for normal hits,
         //          grunt for harder, falling damage for worse

         if(mo->momz < -23*FRACUNIT)
         {
            if(!mo->player->powers[pw_invulnerability] &&
               !(mo->player->cheats & CF_GODMODE))
               P_FallingDamage(mo->player);
            else
               S_StartSound(mo, GameModeInfo->playerSounds[sk_oof]);
         }
         else if(mo->momz < -12*FRACUNIT)
            S_StartSound(mo, GameModeInfo->playerSounds[sk_oof]);
         else if(onthing || !E_GetThingFloorType(mo, true)->liquid)
            S_StartSound(mo, GameModeInfo->playerSounds[sk_plfeet]);
      }
      else if(onthing || !E_GetThingFloorType(mo, true)->liquid)
         S_StartSound(mo, GameModeInfo->playerSounds[sk_oof]);
   }
}

//
// P_ZMovement
//
// Attempt vertical movement.

static void P_ZMovement(Mobj* mo)
{
   // haleyjd: part of lost soul fix, moved up here for maximum
   //          scope
   bool correct_lost_soul_bounce;
   bool moving_down;

   if(demo_compatibility) // v1.9 demos
      correct_lost_soul_bounce = ((GameModeInfo->flags & GIF_LOSTSOULBOUNCE) != 0);
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

      if (mo->z <= mo->floorz)                  // bounce off floors
      {
         mo->z = mo->floorz;
         E_HitFloor(mo); // haleyjd
         if (mo->momz < 0)
         {
            mo->momz = -mo->momz;
            if (!(mo->flags & MF_NOGRAVITY))  // bounce back with decay
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
            if (mo->flags & MF_TOUCHY && mo->intflags & MIF_ARMED &&
               mo->health > 0)
               P_DamageMobj(mo, NULL, NULL, mo->health,MOD_UNKNOWN);
            else if (mo->flags & MF_FLOAT && sentient(mo))
               goto floater;
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
                  mo->removeThinker();      // missiles don't bounce off skies
                  if(demo_version >= 331)
                     return; // haleyjd: return here for below fix
               }
               else if(mo->flags & MF_NOGRAVITY)
                  mo->momz = -mo->momz; // bounce unless under gravity
            }

            if (mo->flags & MF_FLOAT && sentient(mo))
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

      if (mo->flags & MF_MISSILE)
      {
         if(clip.ceilingline &&
            clip.ceilingline->backsector &&
            (mo->z > clip.ceilingline->backsector->ceilingheight) &&
            clip.ceilingline->backsector->intflags & SIF_SKY)
         {
            mo->removeThinker();  // don't explode on skies
         }
         else
         {
            P_ExplodeMissile(mo);
         }
      }

      if (mo->flags & MF_FLOAT && sentient(mo))
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

   // haleyjd 06/05/12: flying players
   if(mo->player && mo->flags4 & MF4_FLY && mo->z > mo->floorz)
      mo->z += finesine[(FINEANGLES / 80 * leveltime) & FINEMASK] / 8;

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
            P_DamageMobj(mo, NULL, NULL, mo->health, MOD_UNKNOWN);
         else if(mo->player && // killough 5/12/98: exclude voodoo dolls
                 mo->player->mo == mo &&
                 mo->momz < -LevelInfo.gravity*8)
         {
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
   else if(mo->flags4 & MF4_FLY)
   {
      // davidph 06/06/12: Objects in flight need to have air friction.
      mo->momz = FixedMul(mo->momz, FRICTION_FLY);
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
         P_ExplodeMissile (mo);
         return;
      }
   }
}

//
// P_NightmareRespawn
//
void P_NightmareRespawn(Mobj* mobj)
{
   fixed_t      x;
   fixed_t      y;
   fixed_t      z;
   subsector_t* ss;
   Mobj*        mo;
   mapthing_t*  mthing;
   bool         check; // haleyjd 11/11/04

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

   if(P_Use3DClipping()) // haleyjd: OVER_UNDER
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

   mo = P_SpawnMobj(mobj->x, mobj->y,
                    mobj->subsector->sector->floorheight +
                       GameModeInfo->teleFogHeight,
                    E_SafeThingName(GameModeInfo->teleFogType));

   // initiate teleport sound

   S_StartSound(mo, GameModeInfo->teleSound);

   // spawn a teleport fog at the new spot

   ss = R_PointInSubsector(x,y);

   mo = P_SpawnMobj(x, y,
                    ss->sector->floorheight + GameModeInfo->teleFogHeight,
                    E_SafeThingName(GameModeInfo->teleFogType));

   S_StartSound(mo, GameModeInfo->teleSound);

   // spawn the new monster

   mthing = &mobj->spawnpoint;
   z = mobj->info->flags & MF_SPAWNCEILING ? ONCEILINGZ : ONFLOORZ;

   // inherit attributes from deceased one

   mo = P_SpawnMobj(x, y, z, mobj->type);
   mo->spawnpoint = mobj->spawnpoint;
   // sf: use R_WadToAngle
   mo->angle = R_WadToAngle(mthing->angle);

   if(mthing->options & MTF_AMBUSH)
      mo->flags |= MF_AMBUSH;

   // killough 11/98: transfer friendliness from deceased
   mo->flags = (mo->flags & ~MF_FRIEND) | (mobj->flags & MF_FRIEND);

   mo->reactiontime = 18;

   // remove the old monster,

   mobj->removeThinker();
}

// PTODO
#ifdef R_LINKEDPORTALS
static bool P_CheckPortalTeleport(Mobj *mobj)
{
   bool ret = false;

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
// Mobj::shouldApplyTorque
//
// Returns true or false if the Mobj should have torque simulation applied.
//
bool Mobj::shouldApplyTorque()
{
   if(demo_version < 203)
      return false; // never in old demos
   if(comp[comp_falloff] && !(flags4 & MF4_ALWAYSTORQUE))
      return false; // torque is disabled
   if(flags & MF_NOGRAVITY  ||
      flags2 & MF2_FLOATBOB ||
      (flags4 & MF4_NOTORQUE && full_demo_version >= make_full_version(340, 46)))
      return false; // flags say no.
   
   // all else considered, only dependent on presence of a dropoff
   return z > dropoffz;
}

// Mobj RTTI Proxy Type
IMPLEMENT_THINKER_TYPE(Mobj)

//
// P_MobjThinker
//
void Mobj::Think()
{
   int oldwaterstate, waterstate = 0;
   fixed_t lz;

   // haleyjd 01/04/14: backup current position at start of frame;
   // note players do this for themselves in P_PlayerThink.
   if(!player || player->mo != this)
      backupPosition();

   // killough 11/98:
   // removed old code which looked at target references
   // (we use pointer reference counting now)

   clip.BlockingMobj = NULL; // haleyjd 1/17/00: global hit reference

   // haleyjd 08/07/04: handle deep water plane hits
   if(subsector->sector->heightsec != -1)
   {
      sector_t *hs = &sectors[subsector->sector->heightsec];

      waterstate = (z < hs->floorheight);
   }

   // haleyjd 03/12/03: Heretic Wind transfer specials
   // haleyjd 03/19/06: moved here from P_XYMovement
   if((flags3 & MF3_WINDTHRUST) && !(flags & MF_NOCLIP))
   {
      sector_t *sec = subsector->sector;

      if(sec->hticPushType == SECTOR_HTIC_WIND)
         P_ThrustMobj(this, sec->hticPushAngle, sec->hticPushForce);
   }

   // momentum movement
   clip.BlockingMobj = NULL;
   if(momx | momy || flags & MF_SKULLFLY)
   {
      P_XYMovement(this);
      if(removed) // killough
         return;       // mobj was removed
   }

   if(!P_Use3DClipping())
      clip.BlockingMobj = NULL;

   lz = z;

   if(flags2 & MF2_FLOATBOB) // haleyjd
   {
      int idx = (floatbob + leveltime) & 63;

      z += FloatBobDiffs[idx];
      lz = z - FloatBobOffsets[idx];
   }

   // haleyjd: OVER_UNDER: major changes
   if(momz || clip.BlockingMobj || lz != floorz)
   {
      if(P_Use3DClipping() && ((flags3 & MF3_PASSMOBJ) || (flags & MF_SPECIAL)))
      {
         Mobj *onmo;

         if(!(onmo = P_GetThingUnder(this)))
         {
            P_ZMovement(this);
            intflags &= ~MIF_ONMOBJ;
         }
         else
         {
            // haleyjd 09/23/06: topdamage -- at last, we can have torches and
            // other hot objects burn things that stand on them :)
            if(demo_version >= 333 && onmo->info->topdamage > 0)
            {
               if(!(leveltime & onmo->info->topdamagemask) &&
                  (!player || !player->powers[pw_ironfeet]))
               {
                  P_DamageMobj(this, onmo, onmo,
                               onmo->info->topdamage, 
                               onmo->info->mod);
               }
            }

            if(player && this == player->mo &&
               momz < -LevelInfo.gravity*8)
            {
               P_PlayerHitFloor(this, true);
            }
            if(onmo->z + onmo->height - z <= 24*FRACUNIT)
            {
               if(player && player->mo == this)
               {
                  fixed_t deltaview;
                  player->viewheight -= onmo->z + onmo->height - z;
                  deltaview = (VIEWHEIGHT - player->viewheight)>>3;
                  if(deltaview > player->deltaviewheight)
                  {
                     player->deltaviewheight = deltaview;
                  }
               }
               z = onmo->z + onmo->height;
            }
            intflags |= MIF_ONMOBJ;
            momz = 0;

            if(info->crashstate != NullStateNum
               && flags & MF_CORPSE
               && !(intflags & MIF_CRASHED))
            {
               intflags |= MIF_CRASHED;
               P_SetMobjState(this, info->crashstate);
            }
         }
      }
      else
         P_ZMovement(this);

      if(removed) // killough
         return;       // mobj was removed
   }
   else if(!(momx | momy) && !sentient(this))
   {                                  // non-sentient objects at rest
      intflags |= MIF_ARMED;    // arm a mine which has come to rest

      // killough 9/12/98: objects fall off ledges if they are hanging off
      // slightly push off of ledge if hanging more than halfway off

      if(shouldApplyTorque())
         P_ApplyTorque(this);                // Apply torque
      else
         intflags &= ~MIF_FALLING, gear = 0; // Reset torque
   }

#ifdef R_LINKEDPORTALS
   P_CheckPortalTeleport(this);
#endif

   // haleyjd 11/06/05: handle crashstate here
   if(info->crashstate != NullStateNum
      && flags & MF_CORPSE
      && !(intflags & MIF_CRASHED)
      && z <= floorz)
   {
      intflags |= MIF_CRASHED;
      P_SetMobjState(this, info->crashstate);
   }

   // haleyjd 08/07/04: handle deep water plane hits
   oldwaterstate = waterstate;

   if(subsector->sector->heightsec != -1)
   {
      sector_t *hs = &sectors[subsector->sector->heightsec];

      waterstate = (z < hs->floorheight);
   }
   else
      waterstate = 0;

   if(flags & (MF_MISSILE|MF_BOUNCES))
   {
      // any time a missile or bouncer crosses, splash
      if(oldwaterstate != waterstate)
         E_HitWater(this, subsector->sector);
   }
   else if(oldwaterstate == 0 && waterstate != 0)
   {
      // normal things only splash going into the water
      E_HitWater(this, subsector->sector);
   }

   // cycle through states,
   // calling action functions at transitions
   // killough 11/98: simplify

   if(tics != -1) // you can cycle through multiple states in a tic
   {
      if(!--tics)
         P_SetMobjState(this, state->nextstate);
   }
   else
   {
      // haleyjd: minor restructuring, eternity features
      // A thing can respawn if:
      // 1) counts for kill AND
      // 2) respawn is on OR
      // 3) thing always respawns or removes itself after death.
      bool can_respawn =
         flags & MF_COUNTKILL &&
           (respawnmonsters ||
            (flags2 & (MF2_ALWAYSRESPAWN | MF2_REMOVEDEAD)));

      // haleyjd 07/13/05: increment mobj->movecount earlier
      if(can_respawn || effects & FX_FLIESONDEATH)
         ++movecount;

      // don't respawn dormant things
      // don't respawn norespawn things
      if(flags2 & (MF2_DORMANT | MF2_NORESPAWN))
         return;

      if(can_respawn && movecount >= info->respawntime &&
         !(leveltime & 31) && P_Random(pr_respawn) <= info->respawnchance)
      { 
         // check for nightmare respawn
         if(flags2 & MF2_REMOVEDEAD)
            this->removeThinker();
         else
            P_NightmareRespawn(this);
      }
   }
}

//
// Mobj::serialize
//
// Handles saving and loading of mobjs through the thinker serialization 
// mechanism.
//
void Mobj::serialize(SaveArchive &arc)
{
   // PointThinker will handle x,y,z coordinates, groupid, and the thinker name
   // (via Thinker::serialize).
   Super::serialize(arc);

   // Basic Properties
   arc 
      // Identity
      << type << tid
      // Position
      << angle                                             // Angles
      << momx << momy << momz                              // Momenta
      << floorz << ceilingz << dropoffz                    // Basic z coords
      << secfloorz << secceilz << passfloorz << passceilz  // Advanced z coords
      << spawnpoint                                        // Spawn info
      << friction << movefactor                            // BOOM 202 friction
      << floatbob                                          // Floatbobbing
      << floorclip                                         // Foot clipping
      // Bit flags
      << flags << flags2 << flags3 << flags4               // Editor flags
      << effects                                           // Particle flags
      << intflags                                          // Internal flags
      // Basic metrics (copied to Mobj from mobjinfo)
      << radius << height << health << damage
      // State info (copied to Mobj from state)
      << sprite << frame << tics
      // Movement logic and clipping
      << validcount                                        // Traversals
      << movedir << movecount << strafecount               // Movement
      << reactiontime << threshold << pursuecount          // Attack AI
      << lastlook                                          // More AI
      << gear                                              // Lee's torque
      // Appearance
      << colour                                            // Translations
      << translucency << alphavelocity                     // Alpha blending
      << xscale << yscale                                  // Scaling
      // Inventory related fields
      << dropamount;

   // Arrays
   P_ArchiveArray<int>(arc, counters, NUMMOBJCOUNTERS); // Counters
   P_ArchiveArray<int>(arc, args,     NUMMTARGS);       // Arguments 

   if(arc.isSaving()) // Saving
   {
      unsigned int temp, targetNum, tracerNum, enemyNum;

      // Basic serializable pointers (state, player)
      arc << state->index;
      temp = static_cast<unsigned>(player ? player - players + 1 : 0);
      arc << temp;

      // Pointers to other mobjs
      targetNum = P_NumForThinker(target);
      tracerNum = P_NumForThinker(tracer);
      enemyNum  = P_NumForThinker(lastenemy);

      arc << targetNum << tracerNum << enemyNum;
   }
   else // Loading
   {
      // Restore basic pointers
      int temp;

      arc << temp; // State index
      state = states[temp];
      
      // haleyjd 07/23/09: this must be before skin setting!
      info = mobjinfo[type]; 

      arc << temp; // Player number
      if(temp)
      {
         // Mobj is a player body
         int playernum = temp - 1;
         (player = &players[playernum])->mo = this;

         // PCLASS_FIXME: Need to save and restore proper player class!
         // Temporary hack.
         players[playernum].pclass = E_PlayerClassForName(GameModeInfo->defPClassName);
         
         // PCLASS_FIXME: Need to save skin and attempt to restore, then fall
         // back to default for player class if non-existant. Note: must be 
         // after player class is set.
         P_SetSkin(P_GetDefaultSkin(&players[playernum]), playernum); // haleyjd
      }
      else
      {
         // haleyjd 09/26/04: restore monster skins
         // haleyjd 07/23/09: do not clear player->mo->skin here
         if(info->altsprite != -1)
            skin = P_GetMonsterSkin(info->altsprite);
         else
            skin = NULL;
      }

      backupPosition();
      P_SetThingPosition(this);
      P_AddThingTID(this, tid);

      // create the deswizzle info structure
      dsInfo = estructalloctag(deswizzle_info, 1, PU_LEVEL);

      // Get the swizzled pointers
      arc << dsInfo->target << dsInfo->tracer << dsInfo->lastenemy;
   }
}

//
// Mobj::deSwizzle
//
// Handles resolving swizzled references to other mobjs immediately after
// deserialization. All saved mobj pointers MUST be restored here.
//
void Mobj::deSwizzle()
{
   Mobj *lTarget, *lTracer, *lLEnemy;

   if(!dsInfo) // eh?
      return;

   // Get the thinker pointers for each number
   lTarget = thinker_cast<Mobj *>(P_ThinkerForNum(dsInfo->target));
   lTracer = thinker_cast<Mobj *>(P_ThinkerForNum(dsInfo->tracer));
   lLEnemy = thinker_cast<Mobj *>(P_ThinkerForNum(dsInfo->lastenemy));

   // Restore the pointers using P_SetNewTarget
   P_SetNewTarget(&target,    lTarget);
   P_SetNewTarget(&tracer,    lTracer);
   P_SetNewTarget(&lastenemy, lLEnemy);

   // Done with the deswizzle info structure.
   efree(dsInfo);
   dsInfo = NULL;
}

//
// Mobj::updateThinker
//
// haleyjd 11/22/10: Overrides Thinker::updateThinker.
// Moved custom logic for mobjs out of what was P_UpdateThinker, to here.
// 
void Mobj::updateThinker()
{
   int tclass = th_misc;

   if(removed)
      tclass = th_delete;
   else if(health > 0 && 
           (flags & MF_COUNTKILL || flags3 & MF3_KILLABLE))
   {
      if(flags & MF_FRIEND)
         tclass = th_friends;
      else
         tclass = th_enemies;
   }

   addToThreadedList(tclass);
}

//
// Mobj::backupPosition
//
// Save the Mobj's position data which is relevant to interpolation.
// Done at the beginning of each gametic, and occasionally when the position of
// an Mobj is abruptly changed (such as when teleporting).
//
void Mobj::backupPosition()
{
   prevpos.x     = x;
   prevpos.y     = y;
   prevpos.z     = z;
   prevpos.angle = angle; // NB: only used for player objects
}

//
// Mobj::copyPosition
//
// Copy all the location data from one Mobj to another.
//
void Mobj::copyPosition(const Mobj *other)
{
   x          = other->x;
   y          = other->y;
   z          = other->z;
   angle      = other->angle;
   groupid    = other->groupid;
   floorz     = other->floorz;
   ceilingz   = other->ceilingz;
   dropoffz   = other->dropoffz;
   passfloorz = other->passfloorz;
   passceilz  = other->passceilz;
   secfloorz  = other->secfloorz;
   secceilz   = other->secceilz;

   intflags  &= ~(MIF_ONFLOOR|MIF_ONSECFLOOR|MIF_ONMOBJ);
   intflags  |= (other->intflags & (MIF_ONFLOOR|MIF_ONSECFLOOR|MIF_ONMOBJ));
}

extern fixed_t tmsecfloorz;
extern fixed_t tmsecceilz;

//
// P_SpawnMobj
//
Mobj *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
   Mobj       *mobj = new Mobj;
   mobjinfo_t *info = mobjinfo[type];
   state_t    *st;

   mobj->type    = type;
   mobj->info    = info;
   mobj->x       = x;
   mobj->y       = y;
   mobj->radius  = info->radius;
   mobj->height  = P_ThingInfoHeight(info); // phares
   mobj->flags   = info->flags;
   mobj->flags2  = info->flags2;     // haleyjd
   mobj->flags3  = info->flags3;     // haleyjd
   mobj->flags4  = info->flags4;     // haleyjd
   mobj->effects = info->particlefx; // haleyjd 07/13/03
   mobj->damage  = info->damage;     // haleyjd 08/02/04

#ifdef R_LINKEDPORTALS
   mobj->groupid = R_NOGROUP;
#endif

   // haleyjd 09/26/04: rudimentary support for monster skins
   if(info->altsprite != -1)
      mobj->skin = P_GetMonsterSkin(info->altsprite);

   // haleyjd: zdoom-style translucency level
   mobj->translucency  = info->translucency;
   
   if(info->alphavelocity != 0) // 5/23/08
      P_StartMobjFade(mobj, info->alphavelocity);
   else
      mobj->alphavelocity = 0;

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
      if(space > 48*FRACUNIT)
      {
         space -= 40*FRACUNIT;
         mobj->z = ((space * P_Random(pr_spawnfloat)) >> 8)
                     + mobj->floorz + 40*FRACUNIT;
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

   mobj->addThinker();

   // haleyjd 01/04/14: set initial position as backup position
   mobj->backupPosition();

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

   return mobj;
}

static mapthing_t itemrespawnque[ITEMQUESIZE];
static int itemrespawntime[ITEMQUESIZE];
int iquehead, iquetail;

//
// P_RemoveMobj
//
void Mobj::removeThinker()
{
   bool respawnitem = false;

   // normal items respawn, and respawning barrels dmflag
   if((this->flags & MF_SPECIAL) ||
      ((dmflags & DM_BARRELRESPAWN) && this->type == E_ThingNumForDEHNum(MT_BARREL)))
   {
      if(this->flags3 & MF3_SUPERITEM)
      {
         // super items ONLY respawn when enabled through dmflags
         respawnitem = ((dmflags & DM_RESPAWNSUPER) == DM_RESPAWNSUPER);
      }
      else
      {
         if(!(this->flags  & MF_DROPPED) &&   // dropped items don't respawn
            !(this->flags3 & MF3_NOITEMRESP)) // NOITEMRESP items don't respawn
            respawnitem = true;
      }
   }

   if(respawnitem)
   {
      // haleyjd FIXME/TODO: spawnpoint is vulnerable to zeroing
      itemrespawnque[iquehead] = this->spawnpoint;
      itemrespawntime[iquehead++] = leveltime;
      if((iquehead &= ITEMQUESIZE-1) == iquetail)
      {
         // lose one off the end?
         iquetail = (iquetail+1)&(ITEMQUESIZE-1);
      }
   }

   // haleyjd 08/19/13: ensure object is no longer in player body queue
   if(this->intflags & MIF_PLYRCORPSE)
      G_DeQueuePlayerCorpse(this);

   // haleyjd 02/02/04: remove from tid hash
   P_RemoveThingTID(this);

   // unlink from sector and block lists
   P_UnsetThingPosition(this);

   // Delete all nodes on the current sector_list -- phares 3/16/98
   if(this->old_sectorlist)
      P_DelSeclist(this->old_sectorlist);

   // haleyjd 08/13/10: ensure that the object cannot be relinked, and
   // nullify old_sectorlist to avoid multiple release of msecnodes.
   if(demo_version > 337)
   {
      this->flags |= (MF_NOSECTOR | MF_NOBLOCKMAP);
      this->old_sectorlist = NULL; 
   }

   // killough 11/98: Remove any references to other mobjs.
   // Older demos might depend on the fields being left alone, however, if 
   // multiple thinkers reference each other indirectly before the end of the
   // current tic.
   if(demo_version >= 203)
   {
      P_SetTarget<Mobj>(&this->target,    NULL);
      P_SetTarget<Mobj>(&this->tracer,    NULL);
      P_SetTarget<Mobj>(&this->lastenemy, NULL);
   }

   // remove from thinker list
   Super::removeThinker();
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
   int i;

   if(!hash)
   {
      hash = (dnumhash_s *)(Z_Malloc(sizeof(*hash) * NUMMOBJTYPES, PU_CACHE, (void **)&hash));
      for(i = 0; i < NUMMOBJTYPES; i++)
         hash[i].first = NUMMOBJTYPES;
      for(i = 0; i < NUMMOBJTYPES; i++)
      {
         if(mobjinfo[i]->doomednum != -1)
         {
            unsigned h = (unsigned int) mobjinfo[i]->doomednum % NUMMOBJTYPES;
            hash[i].next = hash[h].first;
            hash[h].first = i;
         }
      }
   }

   i = hash[type % NUMMOBJTYPES].first;
   while(i < NUMMOBJTYPES && mobjinfo[i]->doomednum != type)
      i = hash[i].next;
   return i;
}

//
// P_RespawnSpecials
//
void P_RespawnSpecials()
{
   fixed_t x, y, z;
   subsector_t*  ss;
   Mobj*       mo;
   mapthing_t*   mthing;
   int           i;

   if(!(dmflags & DM_ITEMRESPAWN) ||  // only respawn items in deathmatch
      iquehead == iquetail ||  // nothing left to respawn?
      leveltime - itemrespawntime[iquetail] < 30*35) // wait 30 seconds
      return;

   mthing = &itemrespawnque[iquetail];

   x = mthing->x << FRACBITS;
   y = mthing->y << FRACBITS;

   // spawn a teleport fog at the new spot
   ss = R_PointInSubsector(x,y);
   mo = P_SpawnMobj(x, y, ss->sector->floorheight , E_SafeThingType(MT_IFOG));
   S_StartSound(mo, sfx_itmbk);

   // find which type to spawn

   // killough 8/23/98: use table for faster lookup
   i = P_FindDoomedNum(mthing->type);

   // spawn it
   z = mobjinfo[i]->flags & MF_SPAWNCEILING ? ONCEILINGZ : ONFLOORZ;

   mo = P_SpawnMobj(x, y, z, i);
   mo->spawnpoint = *mthing;
   // sf
   mo->angle = R_WadToAngle(mthing->angle);

   // pull it from the queue
   iquetail = (iquetail+1)&(ITEMQUESIZE-1);
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
   Mobj*   mobj;

   // not playing?
   if(!playeringame[mthing->type - 1])
      return;

   p = &players[mthing->type-1];

   if(p->playerstate == PST_REBORN)
      G_PlayerReborn(mthing->type - 1);

   x    = mthing->x << FRACBITS;
   y    = mthing->y << FRACBITS;
   z    = ONFLOORZ;
   mobj = P_SpawnMobj(x, y, z, p->pclass->type);

   // sf: set color translations for player sprites
   mobj->colour = players[mthing->type - 1].colormap;

   mobj->angle  = R_WadToAngle(mthing->angle);
   mobj->player = p;
   mobj->health = p->health;
   mobj->backupPosition();

   // haleyjd: verify that the player skin is valid
   if(!p->skin)
      I_Error("P_SpawnPlayer: player skin undefined!\n");

   mobj->skin   = p->skin;
   mobj->sprite = p->skin->sprite;

   p->mo            = mobj;
   p->playerstate   = PST_LIVE;
   p->refire        = 0;
   p->damagecount   = 0;
   p->bonuscount    = 0;
   p->extralight    = 0;
   p->fixedcolormap = 0;
   p->viewheight    = VIEWHEIGHT;
   p->viewz         = mobj->z + VIEWHEIGHT;
   p->prevviewz     = p->viewz;

   p->momx = p->momy = 0;   // killough 10/98: initialize bobbing to 0.

   // setup gun psprite

   P_SetupPsprites(p);

   // give all cards in death match mode
   if(GameType == gt_dm)
      E_GiveAllKeys(p);

   if(mthing->type - 1 == consoleplayer)
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
// sf: made to return Mobj* spawned
//
// haleyjd 03/03/07: rewritten again to use a unified mapthing_t type.
//
Mobj *P_SpawnMapThing(mapthing_t *mthing)
{
   int    i;
   Mobj *mobj;
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

      // haleyjd 01/03/15: must not spawn more than 10 DM starts during old demos
      if(demo_compatibility && offset >= 10u)
         return NULL;

      if(offset >= num_deathmatchstarts)
      {
         num_deathmatchstarts = num_deathmatchstarts ?
            num_deathmatchstarts*2 : 16;
         deathmatchstarts = erealloc(mapthing_t *, deathmatchstarts,
                                     num_deathmatchstarts * sizeof(*deathmatchstarts));
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
            if(HelperThing != -1)
               i = HelperThing;
            else
               doom_printf(FC_ERROR "Invalid value for helper, ignored.");
         }

         goto spawnit;
      }

      // save spots for respawning in network games
      playerstarts[mthing->type-1] = *mthing;
      if(GameType != gt_dm)
         P_SpawnPlayer(mthing);

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

   if(mthing->type >= 1200 && mthing->type < 1300)         // enviro sequences
      i = E_SafeThingName("EEEnviroSequence");
   else if(mthing->type >= 1400 && mthing->type < 1500)    // sector sequence
      i = E_SafeThingName("EESectorSequence");
   else if(mthing->type >= 9027 && mthing->type <= 9033)   // particle fountains
      i = E_SafeThingName("EEParticleFountain");
   else if(mthing->type >= 14001 && mthing->type <= 14064) // ambience
      i = E_SafeThingName("EEAmbience");
   else if(mthing->type >= 14101 && mthing->type <= 14164) // music changer
      i = E_SafeThingName("EEMusicChanger");
   else
   {
      // killough 8/23/98: use table for faster lookup
      i = P_FindDoomedNum(mthing->type);
   }

   // phares 5/16/98:
   // Do not abort because of an unknown thing. Ignore it, but post a
   // warning message for the player.

   // EDF3-FIXME: eliminate different behavior of doomednum hash
   if(i == -1 || i == NUMMOBJTYPES)
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

   if(GameType == gt_dm && (mobjinfo[i]->flags & MF_NOTDMATCH))
      return NULL;        // sf

   // don't spawn any monsters if -nomonsters

   if(nomonsters &&
      ((mobjinfo[i]->flags3 & MF3_KILLABLE) ||
       (mobjinfo[i]->flags & MF_COUNTKILL)))
      return NULL;        // sf

   // haleyjd 08/18/13: Heretic includes some registered-only items in its
   // first episode, as a bonus for play on the registered version. Unlike
   // some other checks EE has removed, we still need to enforce this one as
   // the items have no resources in the shareware IWAD.
   if(GameModeInfo->flags & GIF_SHAREWARE && mobjinfo[i]->flags4 & MF4_NOTSHAREWARE)
      return NULL;

   // spawn it
spawnit:

   x = mthing->x << FRACBITS;
   y = mthing->y << FRACBITS;

   z = mobjinfo[i]->flags & MF_SPAWNCEILING ? ONCEILINGZ : ONFLOORZ;

   // haleyjd 10/13/02: float rand z
   if(mobjinfo[i]->flags2 & MF2_SPAWNFLOAT)
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
      mobj->updateThinker();                      // transfer friendliness flag
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

   // haleyjd: set particle fountain color
   if(mthing->type >= 9027 && mthing->type <= 9033)
      mobj->effects |= (mthing->type - 9026u) << FX_FOUNTAINSHIFT;

   // haleyjd: sector sound zones
   if(mthing->type == 9048)
      P_SetSectorZoneFromMobj(mobj);

   // haleyjd: set ambience sequence # for first 64 types
   if(mthing->type >= 14001 && mthing->type <= 14064)
      mobj->args[0] = mthing->type - 14000;

   // haleyjd: set music number for first 64 types
   if(mthing->type >= 14101 && mthing->type <= 14164)
      mobj->args[0] = mthing->type - 14100;

   mobj->backupPosition();

   return mobj;
}

//
// GAME SPAWN FUNCTIONS
//

//
// P_SpawnPuff
//
void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z, angle_t dir,
                 int updown, bool ptcl)
{
   Mobj* th;

   // haleyjd 08/05/04: use new function
   z += P_SubRandom(pr_spawnpuff) << 10;

   th = P_SpawnMobj(x, y, z, E_SafeThingType(MT_PUFF));
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
}


//
// P_SpawnBlood
//
void P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, angle_t dir, int damage, Mobj *target)
{
   // HTIC_TODO: Heretic support
   Mobj *th;

   // haleyjd 08/05/04: use new function
   z += P_SubRandom(pr_spawnblood) << 10;

   th = P_SpawnMobj(x,y,z, E_SafeThingType(MT_BLOOD));
   th->momz = FRACUNIT*2;
   th->tics -= P_Random(pr_spawnblood)&3;

   if(th->tics < 1)
      th->tics = 1;

   if(damage <= 12 && damage >= 9)
   {
      P_SetMobjState(th, E_SafeState(S_BLOOD2));
   }
   else if (damage < 9)
   {
      P_SetMobjState(th, E_SafeState(S_BLOOD3));
   }

   // for demo sync, etc, we still need to do the above, so
   // we'll make the sprites above invisible and draw particles
   // instead
   if(drawparticles && bloodsplat_particle)
   {
      if(bloodsplat_particle != 2)
         th->translucency = 0;
      P_BloodSpray(target, 32, x, y, z, dir);
   }
}

// FIXME: This function is left over from an mobj-based
// particle system attempt in SMMU -- the particle line
// function could be useful for real particles maybe?

/*
void P_ParticleLine(Mobj *source, Mobj *dest)
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

//=============================================================================
//
// Missiles
//

//
// P_CheckMissileSpawn
//
// Moves the missile forward a bit
//  and possibly explodes it right there.
//
// haleyjd 03/19/11: added bool return type for Hexen logic
//
bool P_CheckMissileSpawn(Mobj* th)
{
   bool ok = true;

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
      return ok;

   // killough 3/15/98: no dropoff (really = don't care for missiles)
   if(!P_TryMove(th, th->x, th->y, false))
   {
      P_ExplodeMissile(th);
      ok = false;
   }

   return ok;
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
// P_SpawnMissileEx
//
// haleyjd 08/08/11: Core shared missile firing logic, taking a structure as a 
// parameter to hold all possible information relevant to firing the missile.
//
Mobj *P_SpawnMissileEx(const missileinfo_t &missileinfo)
{
   Mobj    *source = missileinfo.source;
   Mobj    *dest   = missileinfo.dest;
   fixed_t  z      = missileinfo.z;
   Mobj    *mo;
   angle_t  an;

   if(z != ONFLOORZ)
      z -= source->floorclip;

   mo = P_SpawnMobj(source->x, source->y, z, missileinfo.type);

   S_StartSound(mo, mo->info->seesound);

   P_SetTarget<Mobj>(&mo->target, source); // where it came from -- killough 11/98

   if(missileinfo.flags & missileinfo_t::USEANGLE)
      an = missileinfo.angle;
   else
      an = P_PointToAngle(source->x, source->y, missileinfo.destx, missileinfo.desty);

   // fuzzy player -- haleyjd: add total invisibility, ghost
   if(dest && !(missileinfo.flags & missileinfo_t::NOFUZZ))
   {
      int shiftamount = P_GetAimShift(dest, true);
      if(shiftamount >= 0)
         an += P_SubRandom(pr_shadow) << shiftamount;
   }
   
   mo->angle = an;
   an >>= ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy = FixedMul(mo->info->speed, finesine[an]);   
   
   if(missileinfo.flags & missileinfo_t::USEANGLE)
      mo->momz = missileinfo.momz;
   else
   {
      mo->momz = P_MissileMomz(missileinfo.destx - source->x,
                               missileinfo.desty - source->y,
                               missileinfo.destz - source->z,
                               mo->info->speed);
   }

   P_CheckMissileSpawn(mo);

   return mo;
}

//
// P_SpawnMissile
//
Mobj *P_SpawnMissile(Mobj *source, Mobj *dest, mobjtype_t type, fixed_t z)
{
   missileinfo_t missileinfo;

   memset(&missileinfo, 0, sizeof(missileinfo));

   missileinfo.source = source;
   missileinfo.dest   = dest;
   missileinfo.destx  = getThingX(source, dest);
   missileinfo.desty  = getThingY(source, dest);
   missileinfo.destz  = dest->z;
   missileinfo.type   = type;
   missileinfo.z      = z;

   return P_SpawnMissileEx(missileinfo);
}

//
// P_PlayerPitchSlope
//
// haleyjd 12/28/10: Don't use mlook calculations in old demos!
//
fixed_t P_PlayerPitchSlope(player_t *player)
{
   // Support purely vanilla behavior, and also use in EE if pitch is 0.
   // Avoids very small roundoff errors.
   if(demo_version < 333 ||
      (full_demo_version > make_full_version(339, 21) && player->pitch == 0))
      return 0;
   else
      return finetangent[(ANG90 - player->pitch) >> ANGLETOFINESHIFT];
}

//
// P_SpawnPlayerMissile
//
// Tries to aim at a nearby monster
//
Mobj *P_SpawnPlayerMissile(Mobj* source, mobjtype_t type)
{
   Mobj *th;
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
            slope = P_PlayerPitchSlope(source->player);
         }
      }
      while(mask && (mask=0, !clip.linetarget));  // killough 8/2/98
   }
   else
   {
      // haleyjd: use true slope angle
      slope = P_PlayerPitchSlope(source->player);
   }

   x = source->x;
   y = source->y;
   z = source->z + 4*8*FRACUNIT - source->floorclip;

   th = P_SpawnMobj(x, y, z, type);

   if(source->player && source->player->powers[pw_silencer] &&
      P_GetReadyWeapon(source->player)->flags & WPF_SILENCER)
   {
      S_StartSoundAtVolume(th, th->info->seesound, WEAPON_VOLUME_SILENCED, 
                           ATTN_NORMAL, CHAN_AUTO);
   }
   else
      S_StartSound(th, th->info->seesound);

   P_SetTarget<Mobj>(&th->target, source);   // killough 11/98
   th->angle = an;
   th->momx = FixedMul(th->info->speed, finecosine[an>>ANGLETOFINESHIFT]);
   th->momy = FixedMul(th->info->speed, finesine[an>>ANGLETOFINESHIFT]);
   th->momz = FixedMul(th->info->speed, slope);

   P_CheckMissileSpawn(th);

   return th;    //sf
}

Mobj *P_SpawnMissileAngle(Mobj *source, mobjtype_t type,
                          angle_t angle, fixed_t momz, fixed_t z)
{
   missileinfo_t missileinfo;

   memset(&missileinfo, 0, sizeof(missileinfo));

   missileinfo.source = source;
   missileinfo.type   = type;
   missileinfo.z      = z;
   missileinfo.angle  = angle;
   missileinfo.momz   = momz;
   missileinfo.flags  = (missileinfo_t::USEANGLE | missileinfo_t::NOFUZZ);

   return P_SpawnMissileEx(missileinfo);
}

//
// P_SpawnMissileWithDest
//
// haleyjd 08/07/11: Ugly hack to solve a problem Lee created in A_Mushroom.
//
Mobj* P_SpawnMissileWithDest(Mobj* source, Mobj* dest, mobjtype_t type,
                             fixed_t srcz, 
                             fixed_t destx, fixed_t desty, fixed_t destz)
{
   missileinfo_t missileinfo;

   memset(&missileinfo, 0, sizeof(missileinfo));

   missileinfo.source = source;
   missileinfo.dest   = dest;
   missileinfo.destx  = destx; // Use provided destination coordinates
   missileinfo.desty  = desty;
   missileinfo.destz  = destz;
   missileinfo.type   = type;
   missileinfo.z      = srcz;

   return P_SpawnMissileEx(missileinfo);
}

//=============================================================================
//
// New Eternity mobj functions
//

void P_Massacre(int friends)
{
   Mobj *mo;
   Thinker *think;

   for(think = thinkercap.next; think != &thinkercap; think = think->next)
   {
      if(!(mo = thinker_cast<Mobj *>(think)))
         continue;

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

   if(damage >= player->mo->health)
      player->mo->intflags |= MIF_DIEDFALLING;

   P_DamageMobj(player->mo, NULL, NULL, damage, MOD_FALLING);
}

//
// P_AdjustFloorClip
//
// Adapted from zdoom source, see the zdoom license.
// Thanks to Randy Heit.
//
void P_AdjustFloorClip(Mobj *thing)
{
   fixed_t oldclip = thing->floorclip;
   fixed_t shallowestclip = 0x7fffffff;
   const msecnode_t *m;

   // absorb test for FOOTCLIP flag here
   if(comp[comp_terrain] || !(thing->flags2 & MF2_FOOTCLIP))
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
         fixed_t fclip = E_SectorFloorClip(m->m_sector);

         if(fclip < shallowestclip)
            shallowestclip = fclip;
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
   Thinker *th;

   if(gamestate != GS_LEVEL)
      return;

   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      Mobj *mo;
      if((mo = thinker_cast<Mobj *>(th)))
      {
         if(mo->height == mo->info->height || mo->height == mo->info->c3dheight)
            mo->height = P_ThingInfoHeight(mo->info);
      }
   }
}

//
// haleyjd 02/02/04: Thing IDs (aka TIDs)
//

#define TIDCHAINS 131

// TID hash chains
static Mobj *tidhash[TIDCHAINS];

//
// P_InitTIDHash
//
// Initializes the tid hash table.
//
void P_InitTIDHash(void)
{
   memset(tidhash, 0, TIDCHAINS*sizeof(Mobj *));
}

//
// P_AddThingTID
//
// Adds a thing to the tid hash table
//
void P_AddThingTID(Mobj *mo, int tid)
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
void P_RemoveThingTID(Mobj *mo)
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
Mobj *P_FindMobjFromTID(int tid, Mobj *rover, Mobj *trigger)
{
   // Normal TIDs
   if(tid > 0)
   {
      rover = rover ? rover->tid_next : tidhash[tid % TIDCHAINS];

      while(rover && rover->tid != tid)
         rover = rover->tid_next;

      return rover;
   }

   // Reserved TIDs
   switch(tid)
   {
   case 0:   // script trigger object (may be NULL, which is fine)
      return !rover ? trigger : NULL;

   case -1:  // players are -1 through -4
   case -2:
   case -3:
   case -4:
      {
         int pnum = -tid - 1;

         return !rover && playeringame[pnum] ? players[pnum].mo : NULL;
      }

   default:
      return NULL;
   }
}

//=============================================================================
//
// MobjFadeThinker
//
// Handle alpha velocity fading for Mobj. Very few things use this so it does
// not belong inside Mobj::Think where it adds overhead to every thing.
//

IMPLEMENT_THINKER_TYPE(MobjFadeThinker)

//
// MobjFadeThinker::setTarget
//
// Set the Mobj to which an MobjFadeThinker applies.
//
void MobjFadeThinker::setTarget(Mobj *pTarget)
{
   P_SetTarget<Mobj>(&target, pTarget);
}

//
// MobjFadeThinker::removeThinker
//
// Virtual method, overrides Thinker::removeThinker.
// Clear the counted reference to the parent Mobj before removing self.
//
void MobjFadeThinker::removeThinker()
{
   P_SetTarget<Mobj>(&target, NULL); // clear reference to parent Mobj
   Super::removeThinker();           // call parent implementation
}

//
// MobjFadeThinker::Think
//
// Fade the parent Mobj if applicable. If not, remove self.
//
void MobjFadeThinker::Think()
{
   // If target is being removed, or won't fade, remove self.
   if(target->isRemoved() || !target->alphavelocity)
   {
      removeThinker();
      return;
   }

   target->translucency += target->alphavelocity;

   if(target->translucency < 0)
   {
      target->translucency = 0;
      if(target->flags3 & MF3_CYCLEALPHA)
         target->alphavelocity = -target->alphavelocity;
      else
         removeThinker(); // done.
   }
   else if(target->translucency > FRACUNIT)
   {
      target->translucency = FRACUNIT;
      if(target->flags3 & MF3_CYCLEALPHA)
         target->alphavelocity = -target->alphavelocity;
      else
         removeThinker(); // done.
   }
}

//
// MobjFadeThinker::serialize
//
// Serialization for the parent Mobj pointer.
//
void MobjFadeThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   if(arc.isSaving()) // Saving
   {
      // Pointer to mobj
      unsigned int targetNum = P_NumForThinker(target);
 
      arc << targetNum;
   }
   else
      arc << swizzled_target; // get swizzled pointer
}

//
// MobjFadeThinker::deSwizzle
//
// Since this thinker stores an Mobj pointer, the pointer must be de-swizzled
// when loading save games.
//
void MobjFadeThinker::deSwizzle()
{
   Mobj *lTarget = thinker_cast<Mobj *>(P_ThinkerForNum(swizzled_target));

   P_SetNewTarget(&target, lTarget);
}

//
// P_StartMobjFade
//
// Call to start an Mobj fading due to a change in its alphavelocity field.
// Just setting alphavelocity won't work unless the Mobj happens to already
// have an MobjFadeThinker so, don't do that. You CAN set it to 0, however,
// in which case any existing MobjFadeThinker will remove itself the next time
// it executes.
//
void P_StartMobjFade(Mobj *mo, int alphavelocity)
{
   MobjFadeThinker *mf = NULL;
   Thinker *cap = &thinkerclasscap[th_misc];

   for(Thinker *th = cap->cnext; th != cap; th = th->cnext)
   {
      if((mf = thinker_cast<MobjFadeThinker *>(th)) && mf->getTarget() == mo)
         break; // found one

      // keep looking
      mf = NULL;
   }

   if(!mf && alphavelocity != 0) // not one? spawn it now.
   {
      mf = new MobjFadeThinker;
      mf->addThinker();
      mf->setTarget(mo);
   }

   // otherwise, the existing thinker will pick up this change.
   mo->alphavelocity = alphavelocity;
}

#if 0
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
   Mobj *mo;

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
   Mobj *mo, *spawnspot = NULL;
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
   Mobj *mo = NULL;
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
   Mobj *mo = NULL;
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
   Mobj *mo = NULL;
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
   Mobj *mo = NULL;
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
   Mobj *mo = NULL;
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
   Mobj *mo = NULL;
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

   efree(flags);

   return 0;
}

//
// sm_thingsetfriend
// * Implements _ThingSetFriend(tid, friend)
//
static cell AMX_NATIVE_CALL sm_thingsetfriend(AMX *amx, cell *params)
{
   int tid, friendly;
   Mobj *mo = NULL;
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

      mo->updateThinker();
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
   bool friendly = false;
   Mobj *mo = NULL;
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
   Mobj *mo = NULL;
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
   Mobj  *mo   = NULL;
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
   Mobj *mo = NULL;
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

enum 
{
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

   Mobj *mo = NULL;
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
                  E_SafeThingName(GameModeInfo->teleFogType)),
                  GameModeInfo->teleSound);

         // spawn teleport fog and emit sound at destination
         S_StartSound(P_SpawnMobj(mo->x + 20*finecosine[mo->angle>>ANGLETOFINESHIFT],
                  mo->y + 20*finesine[mo->angle>>ANGLETOFINESHIFT],
                  mo->z + GameModeInfo->teleFogHeight,
                  E_SafeThingName(GameModeInfo->teleFogType)),
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

