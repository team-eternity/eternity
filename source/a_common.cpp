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
//      Action Pointer Functions
//      that are associated with states/frames.
//
//      Common action functions shared by all games.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "a_args.h"
#include "a_common.h"
#include "d_gi.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_args.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "m_compare.h"
#include "metaapi.h"
#include "p_enemy.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_mobjcol.h"
#include "p_portalcross.h"
#include "p_pspr.h"
#include "p_setup.h"
#include "p_skin.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_user.h"
#include "r_defs.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"

//
// P_MakeSeeSound
//
// haleyjd 01/20/11: Isolated from A_Look
//
void P_MakeSeeSound(Mobj *actor, pr_class_t rngnum)
{
   if(actor->info->seesound)
   {
      int sound;
      Mobj *emitter = actor;

      // EDF_FIXME: needs to be generalized like in zdoom
      switch (actor->info->seesound)
      {
      case sfx_posit1:
      case sfx_posit2:
      case sfx_posit3:
         sound = sfx_posit1 + P_Random(rngnum) % 3;
         break;
         
      case sfx_bgsit1:
      case sfx_bgsit2:
         sound = sfx_bgsit1 + P_Random(rngnum) % 2;
         break;
                  
      default:
         sound = actor->info->seesound;
         break;
      }
      
      // haleyjd: generalize to all bosses
      if(actor->flags2 & MF2_BOSS)
         emitter = NULL;

      S_StartSound(emitter, sound);
   }
}

//
// A_Look
//
// Stay in state until a player is sighted.
//
void A_Look(actionargs_t *actionargs)
{
   Mobj *actor     = actionargs->actor;
   Mobj *sndtarget = actor->subsector->sector->soundtarget;
   bool  allaround = false;

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
         if(actor->flags & MF_AMBUSH && sndtarget->flags4 & MF4_TOTALINVISIBLE)
            return;

         // soundtarget is valid, acquire it.
         P_SetTarget<Mobj>(&actor->target, sndtarget);

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
   P_MakeSeeSound(actor, pr_see);
   P_SetMobjState(actor, actor->info->seestate);
}

//
// A_KeepChasing
//
// killough 10/98:
// Allows monsters to continue movement while attacking
//
void A_KeepChasing(actionargs_t *actionargs)
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
   P_SmartMove(actionargs->actor);
}

//
// P_SuperFriend
//
// haleyjd 01/11/04: returns true if this thing is a "super friend"
// and is going to attack a friend
//
static bool P_SuperFriend(Mobj *actor)
{
   Mobj *target = actor->target;

   return ((actor->flags3 & MF3_SUPERFRIEND) && // thing is a super friend,
           target &&                            // target is valid, and
           (actor->flags & target->flags & MF_FRIEND)); // both are friends
}

//
// P_MakeActiveSound
//
// haleyjd 06/15/05: isolated from A_Chase.
//
static void P_MakeActiveSound(Mobj *actor)
{
   if(actor->info->activesound && P_Random(pr_see) < 3)
   {
      int sound = actor->info->activesound;

      // haleyjd: some heretic enemies use their seesound on
      // occasion, so I've made this a general feature
      if(actor->flags3 & MF3_ACTSEESOUND)
      {
         if(P_Random(pr_lookact) < 128 && actor->info->seesound)
            sound = actor->info->seesound;
      }

      // haleyjd: some heretic enemies snort at full volume :)
      if(actor->flags3 & MF3_LOUDACTIVE)
         actor = NULL;

      S_StartSound(actor, sound);
   }
}

//
// A_FaceTarget
//
void A_FaceTarget(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   if(!actor->target)
      return;

   // haleyjd: special feature for player thunking
   if(actor->intflags & MIF_NOFACE)
      return;

   actor->flags &= ~MF_AMBUSH;
   actor->angle = P_PointToAngle(actor->x, actor->y,
                                 getTargetX(actor), getTargetY(actor));

   int shiftamount = P_GetAimShift(actor->target, false);
   if(shiftamount >= 0)
      actor->angle += P_SubRandom(pr_facetarget) << shiftamount;
}

//
// A_Chase
//
// Actor has a melee attack,
// so it tries to close as fast as possible
//
void A_Chase(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   bool superfriend = false;

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

   // Support Raven game Heretic actor speedup
   if(GameModeInfo->flags & GIF_CHASEFAST &&
      (gameskill >= sk_nightmare || fastparm ||
       actor->flags3 & MF3_ALWAYSFAST) && actor->tics > 3)
   {
      // Unlike vanilla Heretic, don't make frames shorter than 3 tics actually
      // longer. This shouldn't affect any monster from vanilla Heretic, except
      // the enraged D'sparil Serpent, which instead gets an extra check in
      // A_Sor1Chase.
      actor->tics -= actor->tics / 2;
      if(actor->tics < 3)
         actor->tics = 3;
   }

   // turn towards movement direction if not there yet
   // killough 9/7/98: keep facing towards target if strafing or backing out

   if(actor->strafecount)
      A_FaceTarget(actionargs);
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
      if(!P_LookForTargets(actor,true)) // look for a new target
         P_SetMobjState(actor, actor->info->spawnstate); // no new target
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
      S_StartSound(actor, actor->info->attacksound);
      
      P_SetMobjState(actor, actor->info->meleestate);

      if(actor->info->missilestate == NullStateNum)
         actor->flags |= MF_JUSTHIT; // killough 8/98: remember an attack
      return;
   }

   // check for missile attack
   if(actor->info->missilestate != NullStateNum && !superfriend)
   {
      // haleyjd 05/22/06: ALWAYSFAST flag
      if(!actor->movecount || gameskill >= sk_nightmare || fastparm || 
         (actor->flags3 & MF3_ALWAYSFAST))
      {
         if(P_CheckMissileRange(actor))
         {
            P_SetMobjState(actor, actor->info->missilestate);
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
void A_RandomWalk(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   int checkdirs[NUMDIRS];

   for(int &checkdir : checkdirs)
      checkdir = 0;

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
      bool      dirfound = false;

      if(P_Random(pr_rndwspawn) < 24)
      {
         P_SetMobjState(actor, actor->info->spawnstate);
         return;
      }
   
      if(turnaround != DI_NODIR) // find reverse direction
         turnaround ^= 4;

      // try a completely random direction
      tdir = P_Random(pr_rndwnewdir) & 7;
      if(tdir != turnaround && (actor->movedir = tdir, P_Move(actor, 0)))
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
               
               if(tdir != turnaround &&  (actor->movedir = tdir, P_Move(actor, 0)))
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

               if(tdir != turnaround && (actor->movedir = tdir, P_Move(actor, 0)))
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

void A_Scream(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
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

void A_PlayerScream(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;
   int sound;

   if(mo->player && strcasecmp(mo->player->skin->sounds[sk_plwdth], "none") &&
      mo->intflags & MIF_WIMPYDEATH)
   {
      // haleyjd 09/29/07: wimpy death, if supported
      sound = sk_plwdth;
   }
   else if((GameModeInfo->flags & GIF_NODIEHI) || mo->health >= -50)
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

//
// A_RavenPlayerScream
//
// Version of A_PlayerScream compatible with Raven's games.
//
void A_RavenPlayerScream(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   int sound;

   if(actor->player && 
      strcasecmp(actor->player->skin->sounds[sk_plwdth], "none") &&
      actor->intflags & MIF_WIMPYDEATH)
   {
      // Wimpy death sound
      sound = sk_plwdth;
   }
   else if(actor->health > -50)
   {
      // Normal death sound
      sound = sk_pldeth; 
   }
   else if(actor->health > -100)
   {
      // Crazy death sound
      sound = sk_pdiehi;
   }
   else
   {
      // Extreme death sound
      sound = sk_slop;
   }

   // if died falling, gross falling death sound
   if(!comp[comp_fallingdmg] && actor->intflags & MIF_DIEDFALLING)
      sound = sk_fallht;
      
   S_StartSound(actor, GameModeInfo->playerSounds[sound]);
}

// PCLASS_FIXME: skull height a playerclass property?
#define SKULLHEIGHT (48 * FRACUNIT)

void A_PlayerSkull(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   Mobj *head;

   // PCLASS_FIXME: skull type a playerclass property?
   int skullType = E_SafeThingType(MT_HPLAYERSKULL);

   head = P_SpawnMobj(actor->x, actor->y, actor->z + SKULLHEIGHT, skullType);

   // transfer player to head
   head->player = actor->player;
   head->health = actor->health;
   head->angle  = actor->angle;

   // player object needs to backup new view angle
   head->backupPosition();

   // clear old body of player
   actor->flags &= ~MF_SOLID;
   actor->player = NULL;

   // fiddle with player properties
   if(head->player)
   {
      head->player->mo          = head;  // set player's thing to head
      head->player->pitch       = 0;     // don't look up or down
      head->player->prevpitch   = 0;     // and snap to central view immediately
      head->player->damagecount = 32;    // see red for a while
      P_SetPlayerAttacker(head->player, actor); // look at old body
   }
   
   // send head flying
   head->momx = 512 * P_SubRandom(pr_skullpop);
   head->momy = 512 * P_SubRandom(pr_skullpop);
   head->momz =  64 * P_Random(pr_skullpop) + 2 * FRACUNIT;
}

void A_XScream(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   int sound   = GameModeInfo->playerSounds[sk_slop];
   
   // haleyjd: falling damage
   if(!comp[comp_fallingdmg] && demo_version >= 329)
   {
      if(actor->player && actor->intflags & MIF_DIEDFALLING)
         sound = GameModeInfo->playerSounds[sk_fallht];
   }
   
   S_StartSound(actor, sound);
}

void A_Pain(actionargs_t *actionargs)
{
   S_StartSound(actionargs->actor, actionargs->actor->info->painsound);
}

void A_Fall(actionargs_t *actionargs)
{
   // actor is on ground, it can be walked over
   actionargs->actor->flags &= ~MF_SOLID;

   // haleyjd 08/06/13: toss-type dropitems are spawned now
   P_DropItems(actionargs->actor, true);
}

// killough 11/98: kill an object
void A_Die(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   actor->flags2 &= ~MF2_INVULNERABLE;  // haleyjd: just in case
   P_DamageMobj(actor, NULL, NULL, actor->health, MOD_UNKNOWN);
}

//
// A_Explode
//
void A_Explode(actionargs_t *actionargs)
{
   Mobj *thingy = actionargs->actor;
   P_RadiusAttack(thingy, thingy->target, kExplosionRadius, kExplosionRadius, thingy->info->mod, 0);

   // ioanch 20160116: portal aware Z
   if(thingy->z <= thingy->secfloorz + kExplosionRadius*FRACUNIT)
      E_HitWater(thingy, P_ExtremeSectorAtPoint(thingy, false));
}

//
// SMMU nailbomb.
// Parameters: (damage, radius, numnails, dmgfactor, dmgmod, pufftype)
// Defaults:   (128,    128,    30,       10,        1,      (default))
//
void A_Nailbomb(actionargs_t *actionargs)
{
   Mobj *thing = actionargs->actor;
   int i;

   arglist_t *args = actionargs->args;
   int damage = E_ArgAsInt(args, 0, 128);
   int radius = E_ArgAsInt(args, 1, 128);
   int numnails = E_ArgAsInt(args, 2, 30);
   int dmgfactor = E_ArgAsInt(args, 3, 10);
   int dmgmod = eclamp(E_ArgAsInt(args, 4, 1), 1, 256);
   const char *pufftype = E_ArgAsString(args, 5, nullptr);
   
   P_RadiusAttack(thing, thing->target, damage, radius, thing->info->mod, 0);

   // haleyjd: added here as of 3.31b3 -- was overlooked
   // ioanch 20160116: portal aware Z
   if(demo_version >= 331 && thing->z <= thing->secfloorz + radius*FRACUNIT)
      E_HitWater(thing, P_ExtremeSectorAtPoint(thing, false));

   for(i = 0; i < numnails; ++i)
   {
      int dmg = dmgfactor;
      if(dmgmod > 1)
         dmg *= P_Random(pr_nailbombshoot) % dmgmod + 1;
      P_LineAttack(thing, i*(ANG180/numnails*2), MISSILERANGE, 0, dmg, pufftype);
   }
}


//
// A_Detonate
//
// killough 8/9/98: same as A_Explode, except that the damage is variable
//
void A_Detonate(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;

   P_RadiusAttack(mo, mo->target, mo->damage, mo->damage, mo->info->mod, 0);

   // haleyjd: added here as of 3.31b3 -- was overlooked
   // ioanch 20160116: portal aware Z
   if(demo_version >= 331 && mo->z <= mo->secfloorz + mo->damage*FRACUNIT)
      E_HitWater(mo, P_ExtremeSectorAtPoint(mo, false));
}

//=============================================================================
//
// Raven-style Item Respawn Actions
//

void A_HideThing(actionargs_t *actionargs)
{
   actionargs->actor->flags2 |= MF2_DONTDRAW;
}

void A_UnHideThing(actionargs_t *actionargs)
{
   actionargs->actor->flags2 &= ~MF2_DONTDRAW;
}

void A_RestoreArtifact(actionargs_t *actionargs)
{
   Mobj *arti = actionargs->actor;

   arti->flags |= MF_SPECIAL;
   P_SetMobjState(arti, arti->info->spawnstate);
   S_StartSound(arti, arti->info->seesound);
}

void A_RestoreSpecialThing1(actionargs_t *actionargs)
{
   Mobj       *thing = actionargs->actor;
   const char *spot  = thing->info->meta->getString("itemrespawnat", "");
 
   // Check for randomized respawns at collections (was Fire Mace specific)   
   MobjCollection *col;
   if(*spot && (col = MobjCollections.collectionForName(spot)))
      col->moveToRandom(thing);

   thing->flags2 &= ~MF2_DONTDRAW;
   S_StartSound(thing, thing->info->seesound);
}

void A_RestoreSpecialThing2(actionargs_t *actionargs)
{
   Mobj *thing = actionargs->actor;

   thing->flags |= MF_SPECIAL;
   P_SetMobjState(thing, thing->info->spawnstate);
}

// EOF

