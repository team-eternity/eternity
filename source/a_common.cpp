// Emacs style mode select   -*- C -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2010 James Haley
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
//      Action Pointer Functions
//      that are associated with states/frames.
//
//      Common action functions shared by all games.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomstat.h"
#include "d_gi.h"
#include "d_mod.h"
#include "p_mobj.h"
#include "p_enemy.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_pspr.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_state.h"
#include "sounds.h"
#include "s_sound.h"
#include "a_small.h"

#include "e_args.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h"
#include "e_ttypes.h"

//
// A_Look
//
// Stay in state until a player is sighted.
//
void A_Look(mobj_t *actor)
{
   mobj_t *sndtarget = actor->subsector->sector->soundtarget;
   boolean allaround = false;

   // killough 7/18/98:
   // Friendly monsters go after other monsters first, but 
   // also return to player, without attacking them, if they
   // cannot find any targets. A marine's best friend :)
   
   actor->threshold = actor->pursuecount = 0;

   if(actor->flags4 & MF4_LOOKALLAROUND)
      allaround = true;

   // If a friend, look for targets immediately.
   // If a monster, go on to check soundtarget first.
   if(!(actor->flags & MF_FRIEND) || !P_LookForTargets(actor, allaround))
   {
      if(!sndtarget || !(sndtarget->flags & MF_SHOOTABLE))
      {
         // There is no valid soundtarget.
         // If a friend, return.
         // If a monster, look for players.
         if(actor->flags & MF_FRIEND || !P_LookForTargets(actor, allaround))
            return;
      }
      else
      {
         // haleyjd 1/25/00: test for AMBUSH enemies seeing totally invisible
         // players after the soundtarget is activated.
         if(actor->flags & MF_AMBUSH && sndtarget->flags2 & MF2_DONTDRAW)
            return;

         // soundtarget is valid, acquire it.
         P_SetTarget<mobj_t>(&actor->target, sndtarget);

         // If an ambush monster, we must additionally be able to see it.
         if(actor->flags & MF_AMBUSH && !P_CheckSight(actor, sndtarget))
         {
            // We couldn't see the soundtarget.
            // If a friend, return.
            // If a monster, look for other players.
            if(actor->flags & MF_FRIEND || !P_LookForTargets(actor, allaround))
               return;
         }
      }

      // Target is now valid, so the monster will awaken.
   }

   // go into chase state
   
   if(actor->info->seesound)
   {
      int sound;

      // EDF_FIXME: needs to be generalized like in zdoom
      switch (actor->info->seesound)
      {
      case sfx_posit1:
      case sfx_posit2:
      case sfx_posit3:
         sound = sfx_posit1 + P_Random(pr_see) % 3;
         break;
         
      case sfx_bgsit1:
      case sfx_bgsit2:
         sound = sfx_bgsit1 + P_Random(pr_see) % 2;
         break;
                  
      default:
         sound = actor->info->seesound;
         break;
      }
      
      // haleyjd: generalize to all bosses
      if(actor->flags2 & MF2_BOSS)
         S_StartSound(NULL, sound);          // full volume
      else
         S_StartSound(actor, sound);
   }

   P_SetMobjState(actor, actor->info->seestate);
}

//
// A_KeepChasing
//
// killough 10/98:
// Allows monsters to continue movement while attacking
//
void A_KeepChasing(mobj_t *actor)
{
   /*
   if(actor->movecount)
   {
      actor->movecount--;
      if(actor->strafecount)
         actor->strafecount--;
      P_SmartMove(actor);
   }
   */
   P_SmartMove(actor);
}

//
// P_SuperFriend
//
// haleyjd 01/11/04: returns true if this thing is a "super friend"
// and is going to attack a friend
//
static boolean P_SuperFriend(mobj_t *actor)
{
   mobj_t *target = actor->target;

   return ((actor->flags3 & MF3_SUPERFRIEND) && // thing is a super friend,
           target &&                            // target is valid, and
           (actor->flags & target->flags & MF_FRIEND)); // both are friends
}

//
// P_MakeActiveSound
//
// haleyjd 06/15/05: isolated from A_Chase.
//
static void P_MakeActiveSound(mobj_t *actor)
{
   if(actor->info->activesound && P_Random(pr_see) < 3)
   {
      int sound = actor->info->activesound;
      int attn  = ATTN_NORMAL;

      // haleyjd: some heretic enemies use their seesound on
      // occasion, so I've made this a general feature
      if(actor->flags3 & MF3_ACTSEESOUND)
      {
         if(P_Random(pr_lookact) < 128 && actor->info->seesound)
            sound = actor->info->seesound;
      }

      // haleyjd: some heretic enemies snort at full volume :)
      // haleyjd: but when they do so, they do not cut off a sound
      // they are already playing.
      if(actor->flags3 & MF3_LOUDACTIVE)
      {
         sfxinfo_t *sfx = E_SoundForDEHNum(sound);
         if(sfx && S_CheckSoundPlaying(actor, sfx))
            return;
         attn = ATTN_NONE;
      }

      S_StartSoundAtVolume(actor, sound, 127, attn, CHAN_AUTO);
   }
}

//
// A_FaceTarget
//
void A_FaceTarget(mobj_t *actor)
{
   if(!actor->target)
      return;

   // haleyjd: special feature for player thunking
   if(actor->intflags & MIF_NOFACE)
      return;

   actor->flags &= ~MF_AMBUSH;
   actor->angle = P_PointToAngle(actor->x, actor->y,
#ifdef R_LINKEDPORTALS
                                  getTargetX(actor), getTargetY(actor));
#else
                                  actor->target->x, actor->target->y);
#endif
   if(actor->target->flags & MF_SHADOW ||
      actor->target->flags2 & MF2_DONTDRAW || // haleyjd
      actor->target->flags3 & MF3_GHOST)      // haleyjd
   {
      actor->angle += P_SubRandom(pr_facetarget) << 21;
   }
}

//
// A_Chase
//
// Actor has a melee attack,
// so it tries to close as fast as possible
//
void A_Chase(mobj_t *actor)
{
   boolean superfriend = false;

   if(actor->reactiontime)
      actor->reactiontime--;

   // modify target threshold
   if(actor->threshold)
   {
      if(!actor->target || actor->target->health <= 0)
         actor->threshold = 0;
      else
         actor->threshold--;
   }

   // turn towards movement direction if not there yet
   // killough 9/7/98: keep facing towards target if strafing or backing out

   if(actor->strafecount)
      A_FaceTarget(actor);
   else
   {
      if(actor->movedir < 8)
      {
         int delta = (actor->angle &= (7<<29)) - (actor->movedir << 29);

         if(delta > 0)
            actor->angle -= ANG90/2;
         else if (delta < 0)
            actor->angle += ANG90/2;
      }
   }

   if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
   {
      // haleyjd 07/26/04: Detect and prevent infinite recursion if
      // Chase is called from a thing's spawnstate.
      static boolean recursion = false;

      // if recursion is true at this point, P_SetMobjState sent
      // us back here -- print an error message and return
      if(recursion)
      {
         doom_printf("Warning: Chase recursion detected\n");
         return;
      }

      // set the flag true before calling P_SetMobjState
      recursion = true;

      if(!P_LookForTargets(actor,true)) // look for a new target
         P_SetMobjState(actor, actor->info->spawnstate); // no new target

      // clear the flag after the state set occurs
      recursion = false;

      return;
   }

   // do not attack twice in a row
   if(actor->flags & MF_JUSTATTACKED)
   {
      // haleyjd 05/22/06: ALWAYSFAST flag
      actor->flags &= ~MF_JUSTATTACKED;
      if(gameskill != sk_nightmare && !fastparm && !(actor->flags3 & MF3_ALWAYSFAST))
         P_NewChaseDir(actor);
      return;
   }

   // haleyjd 01/11/04: check superfriend status
   if(demo_version >= 331)
      superfriend = P_SuperFriend(actor);

  // check for melee attack
   if(actor->info->meleestate != NullStateNum && P_CheckMeleeRange(actor) && 
      !superfriend)
   {
      // haleyjd 05/01/05: Detect and prevent infinite recursion if
      // Chase is called from a thing's attack state
      static boolean recursion = false;

      if(recursion)
      {
         doom_printf("Warning: Chase recursion detected\n");
         return;
      }

      S_StartSound(actor, actor->info->attacksound);
      
      recursion = true;

      P_SetMobjState(actor, actor->info->meleestate);

      recursion = false;
      
      if(actor->info->missilestate == NullStateNum)
         actor->flags |= MF_JUSTHIT; // killough 8/98: remember an attack
      return;
   }

   // check for missile attack
   if(actor->info->missilestate != NullStateNum && !superfriend)
   {
      // haleyjd 05/01/05: Detect and prevent infinite recursion if
      // Chase is called from a thing's attack state
      static boolean recursion = false;

      if(recursion)
      {
         doom_printf("Warning: Chase recursion detected\n");
         return;
      }

      // haleyjd 05/22/06: ALWAYSFAST flag
      if(!actor->movecount || gameskill >= sk_nightmare || fastparm || 
         (actor->flags3 & MF3_ALWAYSFAST))
      {
         if(P_CheckMissileRange(actor))
         {
            recursion = true;
            P_SetMobjState(actor, actor->info->missilestate);
            recursion = false;
            actor->flags |= MF_JUSTATTACKED;
            return;
         }
      }
   }

   if (!actor->threshold)
   {
      if(demo_version < 203)
      {
         // killough 9/9/98: for backward demo compatibility
         if(netgame && !P_CheckSight(actor, actor->target) &&
            P_LookForPlayers(actor, true))
            return;  
      }
      else  // killough 7/18/98, 9/9/98: new monster AI
      {
         if(help_friends && P_HelpFriend(actor))
         {
            return;      // killough 9/8/98: Help friends in need
         }
         else  // Look for new targets if current one is bad or is out of view
         {
            if (actor->pursuecount)
               actor->pursuecount--;
            else
            {
               actor->pursuecount = BASETHRESHOLD;
               
               // If current target is bad and a new one is found, return:

               if(!(actor->target && actor->target->health > 0 &&
                   ((comp[comp_pursuit] && !netgame) || 
                    (((actor->target->flags ^ actor->flags) & MF_FRIEND ||
                      (!(actor->flags & MF_FRIEND) && monster_infighting)) &&
                    P_CheckSight(actor, actor->target)))) &&
                    P_LookForTargets(actor, true))
               {
                  return;
               }
              
               // (Current target was good, or no new target was found)
               //
               // If monster is a missile-less friend, give up pursuit 
               // and return to player, if no attacks have occurred 
               // recently.
               
               if(actor->info->missilestate == NullStateNum && 
                  actor->flags & MF_FRIEND)
               {
                  if(actor->flags & MF_JUSTHIT)        // if recent action,
                     actor->flags &= ~MF_JUSTHIT;      // keep fighting
                  else
                     if(P_LookForPlayers(actor, true)) // else return to player
                        return;
               }
            }
         }
      }
   }
   
   if(actor->strafecount)
      actor->strafecount--;
   
   // chase towards player
   if(--actor->movecount<0 || !P_SmartMove(actor))
      P_NewChaseDir(actor);

   // make active sound
   P_MakeActiveSound(actor);   
}

//
// A_RandomWalk
//
// haleyjd 06/15/05: Makes an object walk in random directions without
// following or attacking any target.
//
void A_RandomWalk(mobj_t *actor)
{
   int i, checkdirs[NUMDIRS];

   for(i = 0; i < NUMDIRS; ++i)
      checkdirs[i] = 0;

   // turn toward movement direction
   if(actor->movedir < 8)
   {
      int delta = (actor->angle &= (7 << 29)) - (actor->movedir << 29);

      if(delta > 0)
         actor->angle -= ANG90 / 2;
      else if(delta < 0)
         actor->angle += ANG90 / 2;
   }
   
   // time to move?
   if(--actor->movecount < 0 || !P_Move(actor, 0))
   {
      dirtype_t tdir;
      dirtype_t turnaround = actor->movedir;
      boolean dirfound = false;

      if(P_Random(pr_rndwspawn) < 24)
      {
         P_SetMobjState(actor, actor->info->spawnstate);
         return;
      }
   
      if(turnaround != DI_NODIR) // find reverse direction
         turnaround ^= 4;

      // try a completely random direction
      tdir = P_Random(pr_rndwnewdir) & 7;
      if(tdir != turnaround && 
         (actor->movedir = tdir, P_Move(actor, 0)))
      {
         checkdirs[tdir] = 1;
         dirfound = true;
      }

      // randomly determine search direction
      if(!dirfound)
      {
         if(tdir & 1)
         {
            for(tdir = DI_EAST; tdir <= DI_SOUTHEAST; ++tdir)
            {
               // don't try the one we already tried before
               if(checkdirs[tdir])
                  continue;
               
               if(tdir != turnaround && 
                  (actor->movedir = tdir, P_Move(actor, 0)))
               {
                  dirfound = true;
                  break;
               }
            }
         }
         else
         {
            for(tdir = DI_SOUTHEAST; tdir != DI_EAST - 1; --tdir)
            {
               // don't try the one we already tried before
               if(checkdirs[tdir])
                  continue;
               
               if(tdir != turnaround && 
                  (actor->movedir = tdir, P_Move(actor, 0)))
               {
                  dirfound = true;
                  break;
               }
            }
         }
      }

      // if didn't find a direction, try the opposite direction
      if(!dirfound)
      {
         if((actor->movedir = turnaround) != DI_NODIR && !P_Move(actor, 0))
            actor->movedir = DI_NODIR;
         else
            dirfound = true;
      }
      
      // if moving, reset movecount
      if(dirfound)
         actor->movecount = P_Random(pr_rndwmovect) & 15;
   }

   // make active sound
   P_MakeActiveSound(actor);
}

void A_Scream(mobj_t *actor)
{
   int sound;
   
   switch(actor->info->deathsound)
   {
   case 0:
      return;

   case sfx_podth1:
   case sfx_podth2:
   case sfx_podth3:
      sound = sfx_podth1 + P_Random(pr_scream)%3;
      break;
      
   case sfx_bgdth1:
   case sfx_bgdth2:
      sound = sfx_bgdth1 + P_Random(pr_scream)%2;
      break;
      
   default:
      sound = actor->info->deathsound;
      break;
   }

   // Check for bosses.
   // haleyjd: generalize to all bosses
   if(actor->flags2 & MF2_BOSS)
      S_StartSound(NULL, sound); // full volume
   else
      S_StartSound(actor, sound);
}

void A_PlayerScream(mobj_t *mo)
{
   int sound;

   if(mo->player && strcasecmp(mo->player->skin->sounds[sk_plwdth], "none") &&
      mo->intflags & MIF_WIMPYDEATH)
   {
      // haleyjd 09/29/07: wimpy death, if supported
      sound = sk_plwdth;
   }
   else if(GameModeInfo->id == shareware || mo->health >= -50)
   {
      // Default death sound
      sound = sk_pldeth; 
   }
   else
   {
      // IF THE PLAYER DIES LESS THAN -50% WITHOUT GIBBING
      sound = sk_pdiehi;
   }

   // if died falling, gross falling death sound
   if(!comp[comp_fallingdmg] && demo_version >= 329 &&
      mo->intflags & MIF_DIEDFALLING)
      sound = sk_fallht;
      
   S_StartSound(mo, GameModeInfo->playerSounds[sound]);
}

// PCLASS_FIXME: skull height a playerclass property?
#define SKULLHEIGHT (48 * FRACUNIT)

void A_PlayerSkull(mobj_t *actor)
{
   mobj_t *head;
   static int skullType = -1;

   // PCLASS_FIXME: skull type a playerclass property?
   if(skullType == -1)
      skullType = E_SafeThingType(MT_HPLAYERSKULL);

   head = P_SpawnMobj(actor->x, actor->y, actor->z + SKULLHEIGHT, skullType);

   // transfer player to head
   head->player = actor->player;
   head->health = actor->health;
   head->angle  = actor->angle;

   // clear old body of player
   actor->flags &= ~MF_SOLID;
   actor->player = NULL;

   // fiddle with player properties
   if(head->player)
   {
      head->player->mo          = head;  // set player's thing to head
      head->player->pitch       = 0;     // don't look up or down
      head->player->damagecount = 32;    // see red for a while
      head->player->attacker    = actor; // look at old body
   }
   
   // send head flying
   head->momx = 512 * P_SubRandom(pr_skullpop);
   head->momy = 512 * P_SubRandom(pr_skullpop);
   head->momz =  64 * P_Random(pr_skullpop) + 2 * FRACUNIT;
}

void A_XScream(mobj_t *actor)
{
   int sound = GameModeInfo->playerSounds[sk_slop];
   
   // haleyjd: falling damage
   if(!comp[comp_fallingdmg] && demo_version >= 329)
   {
      if(actor->player && actor->intflags & MIF_DIEDFALLING)
         sound = GameModeInfo->playerSounds[sk_fallht];
   }
   
   S_StartSound(actor, sound);
}

void A_Pain(mobj_t *actor)
{
   S_StartSound(actor, actor->info->painsound);
}

void A_Fall(mobj_t *actor)
{
   // actor is on ground, it can be walked over
   actor->flags &= ~MF_SOLID;
}

// killough 11/98: kill an object
void A_Die(mobj_t *actor)
{
   actor->flags2 &= ~MF2_INVULNERABLE;  // haleyjd: just in case
   P_DamageMobj(actor, NULL, NULL, actor->health, MOD_UNKNOWN);
}

//
// A_Explode
//
void A_Explode(mobj_t *thingy)
{
   P_RadiusAttack(thingy, thingy->target, 128, thingy->info->mod);

   if(thingy->z <= thingy->secfloorz + 128*FRACUNIT)
      E_HitWater(thingy, thingy->subsector->sector);
}

void A_Nailbomb(mobj_t *thing)
{
   int i;
   
   P_RadiusAttack(thing, thing->target, 128, thing->info->mod);

   // haleyjd: added here as of 3.31b3 -- was overlooked
   if(demo_version >= 331 && thing->z <= thing->secfloorz + 128*FRACUNIT)
      E_HitWater(thing, thing->subsector->sector);

   for(i = 0; i < 30; ++i)
      P_LineAttack(thing, i*(ANG180/15), MISSILERANGE, 0, 10);
}


//
// A_Detonate
// killough 8/9/98: same as A_Explode, except that the damage is variable
//

void A_Detonate(mobj_t *mo)
{
   P_RadiusAttack(mo, mo->target, mo->damage, mo->info->mod);

   // haleyjd: added here as of 3.31b3 -- was overlooked
   if(demo_version >= 331 && mo->z <= mo->secfloorz + mo->damage*FRACUNIT)
      E_HitWater(mo, mo->subsector->sector);
}

// EOF

