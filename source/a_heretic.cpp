// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3:
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
//      Heretic action functions.
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
#include "p_tick.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_state.h"
#include "sounds.h"
#include "s_sound.h"
#include "a_small.h"

#include "e_args.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h"
#include "e_ttypes.h"

#include "a_common.h"

// [CG] Added.
#include "sv_main.h"

//
// A_SpawnGlitter
//
// Parameterized code pointer to spawn inert objects with some
// positive z momentum
//
// Parameters:
// args[0] - object type (use DeHackEd number)
// args[1] - z momentum (scaled by FRACUNIT/8)
//
void A_SpawnGlitter(mobj_t *actor)
{
   mobj_t *glitter;
   int glitterType;
   fixed_t initMomentum;
   fixed_t x, y, z;

   if(!serverside)
      return;

   glitterType  = E_ArgAsThingNum(actor->state->args, 0);
   initMomentum =
      (fixed_t)(E_ArgAsInt(actor->state->args, 1, 0) * FRACUNIT / 8);

   // special defaults

   // default momentum of zero == 1/4 unit per tic
   if(!initMomentum)
      initMomentum = FRACUNIT >> 2;

   // randomize spawning coordinates within a 32-unit square
   x = actor->x + ((P_Random(pr_tglit) & 31) - 16) * FRACUNIT;
   y = actor->y + ((P_Random(pr_tglit) & 31) - 16) * FRACUNIT;

   z = actor->floorz;

   glitter = P_SpawnMobj(x, y, z, glitterType);

   // give it some upward momentum
   glitter->momz = initMomentum;

   if(CS_SERVER)
      SV_BroadcastActorSpawned(glitter);
}

//
// A_AccelGlitter
//
// Increases object's z momentum by 50%
//
void A_AccelGlitter(mobj_t *actor)
{
   actor->momz += actor->momz / 2;
}

//
// A_SpawnAbove
//
// Parameterized pointer to spawn a solid object above the one
// calling the pointer. Used by the key gizmos.
//
// args[0] -- thing type (DeHackEd num)
// args[1] -- state number (< 0 == no state transition)
// args[2] -- amount to add to z coordinate
//
void A_SpawnAbove(mobj_t *actor)
{
   int thingtype;
   int statenum;
   fixed_t zamt;
   mobj_t *mo;

   if(!serverside)
      return;

   thingtype = E_ArgAsThingNum(actor->state->args, 0);
   statenum  = E_ArgAsStateNumG0(actor->state->args, 1, actor);
   zamt      = (fixed_t)(E_ArgAsInt(actor->state->args, 2, 0) * FRACUNIT);

   mo = P_SpawnMobj(actor->x, actor->y, actor->z + zamt, thingtype);

   if(CS_SERVER)
      SV_BroadcastActorSpawned(mo);

   if(statenum >= 0 && statenum < NUMSTATES)
      P_SetMobjState(mo, statenum);
}

//=============================================================================
//
// Heretic Mummy
//

void A_MummyAttack(mobj_t *actor)
{
   if(!actor->target)
      return;

   S_StartSound(actor, actor->info->attacksound);

   if(P_CheckMeleeRange(actor))
   {
      int dmg = ((P_Random(pr_mumpunch)&7)+1)*2;
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
      S_StartSound(actor, sfx_mumat2);
      return;
   }

   S_StartSound(actor, sfx_mumat1);
}

void A_MummyAttack2(mobj_t *actor)
{
   mobj_t *mo;

   if(!actor->target)
      return;

   if(P_CheckMeleeRange(actor))
   {
      int dmg = ((P_Random(pr_mumpunch2)&7)+1)*2;
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
      return;
   }

   if(serverside)
   {
      mo = P_SpawnMissile(actor, actor->target,
                          E_SafeThingType(MT_MUMMYFX1),
                          actor->z + DEFAULTMISSILEZ);

      P_SetTarget<mobj_t>(&mo->tracer, actor->target);
      if(CS_SERVER)
         SV_BroadcastActorTarget(mo, CS_AT_TRACER);
   }
}

void A_MummySoul(mobj_t *actor)
{
   mobj_t *mo;
   static int soulType = -1;

   if(soulType == -1)
      soulType = E_SafeThingType(MT_MUMMYSOUL);

   if(serverside)
   {
      mo = P_SpawnMobj(actor->x, actor->y, actor->z + 10 * FRACUNIT, soulType);
      mo->momz = FRACUNIT;
      if(CS_SERVER)
         SV_BroadcastActorSpawned(mo);
   }
}

void P_HticDrop(mobj_t *actor, int special, mobjtype_t type)
{
   mobj_t *item;

   if(!serverside)
      return;

   item = P_SpawnMobj(
      actor->x, actor->y, actor->z + (actor->height >> 1), type
   );

   if(CS_SERVER)
      SV_BroadcastActorSpawned(item);

   item->momx = P_SubRandom(pr_hdropmom) << 8;
   item->momy = P_SubRandom(pr_hdropmom) << 8;
   item->momz = (P_Random(pr_hdropmom) << 10) + 5*FRACUNIT;

   /*
   // GROSS! We are not doing this in EE.
   if(special)
   {
      item->health = special;
   }
   */

   item->flags |= MF_DROPPED;
}

//
// A_HticDrop
//
// DEPRECATED! DO NOT USE!!!
//
// HTIC_FIXME / HTIC_TODO: Just get rid of this and make item
// drops a sole property of thing types, where it belongs.
//
// Parameterized code pointer, drops one or two items at random
//
// args[0] -- thing type 1 (0 == none)
// args[1] -- thing type 1 drop chance
// args[2] -- thing type 2 (0 == none)
// args[3] -- thing type 2 drop chance
//
void A_HticDrop(mobj_t *actor)
{
   int thingtype1, thingtype2, chance1, chance2;
   int drop1 = 0, drop2 = 0;

   thingtype1 = E_ArgAsThingNumG0(actor->state->args, 0);
   chance1    = E_ArgAsInt(actor->state->args,      1, 0);
   thingtype2 = E_ArgAsThingNumG0(actor->state->args, 2);
   chance2    = E_ArgAsInt(actor->state->args,      3, 0);

   // this is hackish, but it'll work
   /*
   if((dt = actor->state->args[4]))
   {
      drop1 = (int)(dt & 0x00007fff);
      drop2 = (int)((dt & 0x7fff0000) >> 16);
   }
   */

   A_Fall(actor);

   // haleyjd 07/05/03: adjusted for EDF
   if(thingtype1 >= 0 && thingtype1 < NUMMOBJTYPES)
   {
      if(P_Random(pr_hdrop1) <= chance1)
         P_HticDrop(actor, drop1, thingtype1);
   }

   if(thingtype2 >= 0 && thingtype2 < NUMMOBJTYPES)
   {
      if(P_Random(pr_hdrop2) <= chance2)
         P_HticDrop(actor, drop2, thingtype2);
   }
}

void P_HticTracer(mobj_t *actor, angle_t threshold, angle_t maxturn)
{
   angle_t exact, diff;
   fixed_t dist;
   mobj_t  *dest;
   boolean dir;

   // adjust direction
   dest = actor->tracer;

   if(!dest || dest->health <= 0)
      return;

   exact = P_PointToAngle(actor->x, actor->y, dest->x, dest->y);

   if(exact > actor->angle)
   {
      diff = exact - actor->angle;
      dir = true;
   }
   else
   {
      diff = actor->angle - exact;
      dir = false;
   }

   // if > 180, invert angle and direction
   if(diff > 0x80000000)
   {
      diff = 0xFFFFFFFF - diff;
      dir = !dir;
   }

   // apply limiting parameters
   if(diff > threshold)
   {
      diff >>= 1;
      if(diff > maxturn)
         diff = maxturn;
   }

   // turn clockwise or counterclockwise
   if(dir)
      actor->angle += diff;
   else
      actor->angle -= diff;

   // directly from above
   diff = actor->angle>>ANGLETOFINESHIFT;
   actor->momx = FixedMul(actor->info->speed, finecosine[diff]);
   actor->momy = FixedMul(actor->info->speed, finesine[diff]);

   // adjust z only when significant height difference exists
   if(actor->z + actor->height < dest->z ||
      dest->z  + dest->height  < actor->z)
   {
      // directly from above
      dist = P_AproxDistance(dest->x - actor->x, dest->y - actor->y);

      dist = dist / actor->info->speed;

      if(dist < 1)
         dist = 1;

      // momentum is set to equal slope rather than having some
      // added to it
      actor->momz = (dest->z - actor->z) / dist;
   }
}

//
// A_HticTracer
//
// Parameterized pointer for Heretic-style tracers. I wanted
// to merge this with A_GenTracer, but the logic looks incompatible
// no matter how I rewrite it.
//
// args[0]: threshold in degrees
// args[1]: maxturn in degrees
//
void A_HticTracer(mobj_t *actor)
{
   angle_t threshold, maxturn;

   threshold = (angle_t)(E_ArgAsInt(actor->state->args, 0, 0));
   maxturn   = (angle_t)(E_ArgAsInt(actor->state->args, 1, 0));

   // convert from integer angle to angle_t
   threshold = (angle_t)(((uint64_t)threshold << 32) / 360);
   maxturn   = (angle_t)(((uint64_t)maxturn << 32) / 360);

   P_HticTracer(actor, threshold, maxturn);
}

//
// A_ClinkAttack
//
// Sabreclaw's melee attack
//
void A_ClinkAttack(mobj_t *actor)
{
   int dmg;

   if(!actor->target)
      return;

   S_StartSound(actor, actor->info->attacksound);

   if(P_CheckMeleeRange(actor))
   {
      dmg = (P_Random(pr_clinkatk) % 7) + 3;
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
   }
}

//=============================================================================
//
// Disciple Actions
//

void A_WizardAtk1(mobj_t *actor)
{
   A_FaceTarget(actor);
   actor->flags3 &= ~MF3_GHOST;
}

void A_WizardAtk2(mobj_t *actor)
{
   A_FaceTarget(actor);
   actor->flags3 |= MF3_GHOST;
}

void A_WizardAtk3(mobj_t *actor)
{
   mobj_t *mo;
   angle_t angle;
   fixed_t momz;
   fixed_t z = actor->z + DEFAULTMISSILEZ;
   static int wizfxType = -1;

   if(wizfxType == -1)
      wizfxType = E_SafeThingType(MT_WIZFX1);

   actor->flags3 &= ~MF3_GHOST;
   if(!actor->target)
      return;

   S_StartSound(actor, actor->info->attacksound);

   if(P_CheckMeleeRange(actor))
   {
      int dmg = ((P_Random(pr_wizatk) & 7) + 1) * 4;
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
      return;
   }

   if(serverside)
   {
      mo = P_SpawnMissile(actor, actor->target, wizfxType, z);
      momz = mo->momz;
      angle = mo->angle;
      P_SpawnMissileAngle(actor, wizfxType, angle-(ANG45/8), momz, z);
      P_SpawnMissileAngle(actor, wizfxType, angle+(ANG45/8), momz, z);
   }
}

//=============================================================================
//
// Serpent Rider D'Sparil Actions
//

void A_Sor1Chase(mobj_t *actor)
{
   if(actor->counters[0])
   {
      // decrement fast walk timer
      actor->counters[0]--;
      actor->tics -= 3;
      // don't make tics less than 1
      if(actor->tics < 1)
         actor->tics = 1;
   }

   A_Chase(actor);
}

void A_Sor1Pain(mobj_t *actor)
{
   actor->counters[0] = 20; // Number of steps to walk fast
   A_Pain(actor);
}

void A_Srcr1Attack(mobj_t *actor)
{
   mobj_t *mo;
   fixed_t momz;
   angle_t angle;
   fixed_t mheight = actor->z + 48 * FRACUNIT;
   int dmg;
   static int srcrfxType = -1;

   if(!serverside)
      return;

   if(srcrfxType == -1)
      srcrfxType = E_SafeThingType(MT_SRCRFX1);

   if(!actor->target)
      return;

   S_StartSound(actor, actor->info->attacksound);

   // bite attack
   if(P_CheckMeleeRange(actor))
   {
      dmg = ((P_Random(pr_sorc1atk) & 7) + 1) * 8;
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
      return;
   }

   if(actor->health > (actor->info->spawnhealth * 2) / 3)
   {
      // regular attack, one fire ball
      P_SpawnMissile(actor, actor->target, srcrfxType, mheight);
   }
   else
   {
      // "limit break": 3 fire balls
      mo = P_SpawnMissile(actor, actor->target, srcrfxType, mheight);
      momz = mo->momz;
      angle = mo->angle;
      P_SpawnMissileAngle(
         actor, srcrfxType, angle - ANGLE_1 * 3, momz, mheight
      );
      P_SpawnMissileAngle(
         actor, srcrfxType, angle + ANGLE_1 * 3, momz, mheight
      );

      // desperation -- attack twice
      if(actor->health * 3 < actor->info->spawnhealth)
      {
         if(actor->counters[1])
            actor->counters[1] = 0;
         else
         {
            actor->counters[1] = 1;
            P_SetMobjState(actor, E_SafeState(S_SRCR1_ATK4));
         }
      }
   }
}

//
// A_SorcererRise
//
// Spawns the normal D'Sparil after the Chaos Serpent dies.
//
void A_SorcererRise(mobj_t *actor)
{
   mobj_t *mo;
   static int sorc2Type = -1;

   if(!serverside)
      return;

   if(sorc2Type == -1)
      sorc2Type = E_SafeThingType(MT_SORCERER2);

   actor->flags &= ~MF_SOLID;
   mo = P_SpawnMobj(actor->x, actor->y, actor->z, sorc2Type);
   mo->angle = actor->angle;

   if(CS_SERVER)
      SV_BroadcastActorSpawned(mo);

   // transfer friendliness
   mo->flags = (mo->flags & ~MF_FRIEND) | (actor->flags & MF_FRIEND);

   // add to appropriate thread
   mo->updateThinker();

   if(actor->target && !(mo->flags & MF_FRIEND))
   {
      P_SetTarget<mobj_t>(&mo->target, actor->target);
      if(CS_SERVER)
         SV_BroadcastActorTarget(mo, CS_AT_TARGET);
   }

   P_SetMobjState(mo, E_SafeState(S_SOR2_RISE1));
}

//=============================================================================
//
// Normal D'Sparil Actions
//

// haleyjd 11/19/02:
// Teleport spots for D'Sparil -- these are more or less identical
// to boss spots used for DOOM II MAP30, and I have generalized
// the code that looks for them so it can be used here (and elsewhere
// as needed). This automatically removes the Heretic boss spot
// limit, of course.

MobjCollection sorcspots;

void P_SpawnSorcSpots(void)
{
   static int spotType = -1;

   if(spotType == -1)
      spotType = E_ThingNumForDEHNum(MT_DSPARILSPOT);

   P_ReInitMobjCollection(&sorcspots, spotType);

   if(spotType == NUMMOBJTYPES)
      return;

   P_CollectThings(&sorcspots);
}

void A_Srcr2Decide(mobj_t *actor)
{
   int chance[] = { 192, 120, 120, 120, 64, 64, 32, 16, 0 };
   int index    = actor->health / (actor->info->spawnhealth / 8);

   if(!serverside)
      return;

   // if no spots, no teleportation
   if(P_CollectionIsEmpty(&sorcspots))
      return;

   if(P_Random(pr_sorctele1) < chance[index])
   {
      bossteleport_t bt;

      bt.mc          = &sorcspots;                       // use sorcspots
      bt.rngNum      = pr_sorctele2;                     // use this rng
      bt.boss        = actor;                            // teleport D'Sparil
      bt.state       = E_SafeState(S_SOR2_TELE1);        // set him to this state
      bt.fxtype      = E_SafeThingType(MT_SOR2TELEFADE); // spawn a DSparil TeleFade
      bt.zpamt       = 0;                                // add 0 to fx z coord
      bt.hereThere   = BOSSTELE_ORIG;                    // spawn fx only at origin
      bt.soundNum    = sfx_htelept;                      // use htic teleport sound
      bt.minDistance = 128 * FRACUNIT;                   // minimum distance

      P_BossTeleport(&bt);
   }
}

void A_Srcr2Attack(mobj_t *actor)
{
   int chance;
   fixed_t z = actor->z + DEFAULTMISSILEZ;
   static int sor2fx1Type = -1;
   static int sor2fx2Type = -1;

   if(!serverside)
      return;

   if(sor2fx1Type == -1)
   {
      sor2fx1Type = E_SafeThingType(MT_SOR2FX1);
      sor2fx2Type = E_SafeThingType(MT_SOR2FX2);
   }

   if(!actor->target)
      return;

   // haleyjd 10/01/08: use CHAN_WEAPON; D'Sparil never seems to cut off his
   // own sounds.
   S_StartSoundAtVolume(actor, actor->info->attacksound, 127, ATTN_NONE, CHAN_WEAPON);

   if(P_CheckMeleeRange(actor))
   {
      // ouch!
      int dmg = ((P_Random(pr_soratk1) & 7) + 1) * 20;
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
      return;
   }

   chance = (actor->health * 2 < actor->info->spawnhealth) ? 96 : 48;

   if(P_Random(pr_soratk2) < chance)
   {
      mobj_t *mo;

      // spawn wizards -- transfer friendliness
      mo = P_SpawnMissileAngle(actor, sor2fx2Type,
                               actor->angle - ANG45,
                               FRACUNIT / 2, z);
      mo->flags = (mo->flags & ~MF_FRIEND) | (actor->flags & MF_FRIEND);

      mo = P_SpawnMissileAngle(actor, sor2fx2Type,
                               actor->angle + ANG45,
                               FRACUNIT / 2, z);
      mo->flags = (mo->flags & ~MF_FRIEND) | (actor->flags & MF_FRIEND);
   }
   else
   {
      // shoot blue bolt
      P_SpawnMissile(actor, actor->target, sor2fx1Type, z);
   }
}

void A_BlueSpark(mobj_t *actor)
{
   int i;
   mobj_t *mo;
   static int sparkType = -1;

   if(!serverside)
      return;

   if(sparkType == -1)
      sparkType = E_SafeThingType(MT_SOR2FXSPARK);

   for(i = 0; i < 2; ++i)
   {
      mo = P_SpawnMobj(actor->x, actor->y, actor->z, sparkType);

      mo->momx = P_SubRandom(pr_bluespark) << 9;
      mo->momy = P_SubRandom(pr_bluespark) << 9;
      mo->momz = FRACUNIT + (P_Random(pr_bluespark) << 8);
      if(CS_SERVER)
         SV_BroadcastActorSpawned(mo);
   }
}

void A_GenWizard(mobj_t *actor)
{
   mobj_t *mo;
   mobj_t *fog;
   static int wizType = -1;
   static int fogType = -1;

   if(!serverside)
      return;

   if(wizType == -1)
   {
      wizType = E_SafeThingType(MT_WIZARD);
      fogType = E_SafeThingType(MT_HTFOG);
   }

   mo = P_SpawnMobj(
      actor->x, actor->y, actor->z-mobjinfo[wizType].height / 2, wizType
   );

   if(!P_CheckPosition(mo, mo->x, mo->y) ||
      (mo->z >
      (mo->subsector->sector->ceilingheight - mo->height)) ||
      (mo->z < mo->subsector->sector->floorheight))
   {
      // doesn't fit, so remove it immediately
      mo->removeThinker();
      return;
   }

   // transfer friendliness
   mo->flags = (mo->flags & ~MF_FRIEND) | (actor->flags & MF_FRIEND);

   // add to appropriate thread
   mo->updateThinker();

   // Check for movements.
   if(!P_TryMove(mo, mo->x, mo->y, false))
   {
      // can't move, remove it immediately
      mo->removeThinker();
      return;
   }

   if(CS_SERVER)
      SV_BroadcastActorSpawned(mo);

   // set this missile object to die
   actor->momx = actor->momy = actor->momz = 0;
   P_SetMobjState(actor, mobjinfo[actor->type].deathstate);
   actor->flags &= ~MF_MISSILE;

   // spawn a telefog
   fog = P_SpawnMobj(actor->x, actor->y, actor->z, fogType);
   S_StartSound(fog, sfx_htelept);

   if(CS_SERVER)
      SV_BroadcastActorSpawned(fog);
}

void A_Sor2DthInit(mobj_t *actor)
{
   actor->counters[0] = 7; // Animation loop counter

   // kill monsters early
   // kill only friends or enemies depending on friendliness
   P_Massacre((actor->flags & MF_FRIEND) ? 1 : 2);
}

void A_Sor2DthLoop(mobj_t *actor)
{
   if(--actor->counters[0])
   {
      // Need to loop
      P_SetMobjState(actor, E_SafeState(S_SOR2_DIE4));
   }
}

static const char *kwds_A_HticExplode[] =
{
   "default",       //    0
   "dsparilbspark", //    1
   "floorfire",     //    2
   "timebomb",      //    3
};

static argkeywd_t hticexpkwds =
{
   kwds_A_HticExplode,
   sizeof(kwds_A_HticExplode)/sizeof(const char *)
};

//
// A_HticExplode
//
// Parameterized pointer, enables several different Heretic
// explosion actions
//
void A_HticExplode(mobj_t *actor)
{
   int damage = 128;

   int action = E_ArgAsKwd(actor->state->args, 0, &hticexpkwds, 0);

   switch(action)
   {
   case 1: // 1 -- D'Sparil FX1 explosion, random damage
      damage = 80 + (P_Random(pr_sorfx1xpl) & 31);
      break;
   case 2: // 2 -- Maulotaur floor fire, constant 24 damage
      damage = 24;
      break;
   case 3: // 3 -- Time Bomb of the Ancients, special effects
      actor->z += 32*FRACUNIT;
      actor->translucency = FRACUNIT;
      break;
   default:
      break;
   }

   P_RadiusAttack(actor, actor->target, damage, actor->info->mod);

   if(actor->z <= actor->secfloorz + damage * FRACUNIT)
      E_HitWater(actor, actor->subsector->sector);
}

typedef struct boss_spec_htic_s
{
   unsigned int thing_flag;
   unsigned int level_flag;
   int flagfield;
} boss_spec_htic_t;

#define NUM_HBOSS_SPECS 5

static boss_spec_htic_t hboss_specs[NUM_HBOSS_SPECS] =
{
   { MF2_E1M8BOSS,   BSPEC_E1M8, 2 },
   { MF2_E2M8BOSS,   BSPEC_E2M8, 2 },
   { MF2_E3M8BOSS,   BSPEC_E3M8, 2 },
   { MF2_E4M8BOSS,   BSPEC_E4M8, 2 },
   { MF3_E5M8BOSS,   BSPEC_E5M8, 3 },
};

//
// A_HticBossDeath
//
// Heretic boss deaths
//
void A_HticBossDeath(mobj_t *actor)
{
   CThinker *th;
   line_t    junk;
   int       i;

   for(i = 0; i < NUM_HBOSS_SPECS; ++i)
   {
      unsigned int flags =
         hboss_specs[i].flagfield == 2 ? actor->flags2 : actor->flags3;

      // to activate a special, the thing must be a boss that triggers
      // it, and the map must have the special enabled.
      if((flags & hboss_specs[i].thing_flag) &&
         (LevelInfo.bossSpecs & hboss_specs[i].level_flag))
      {
         for(th = thinkercap.next; th != &thinkercap; th = th->next)
         {
            mobj_t *mo;
            if((mo = thinker_cast<mobj_t *>(th)))
            {
               unsigned int moflags =
                  hboss_specs[i].flagfield == 2 ? mo->flags2 : mo->flags3;
               if(mo != actor &&
                  (moflags & hboss_specs[i].thing_flag) &&
                  mo->health > 0)
                  return;         // other boss not dead
            }
         }

         // victory!
         switch(hboss_specs[i].level_flag)
         {
         default:
         case BSPEC_E2M8:
         case BSPEC_E3M8:
         case BSPEC_E4M8:
         case BSPEC_E5M8:
            // if a friendly boss dies, kill only friends
            // if an enemy boss dies, kill only enemies
            P_Massacre((actor->flags & MF_FRIEND) ? 1 : 2);

            // fall through
         case BSPEC_E1M8:
            junk.tag = 666;
            EV_DoFloor(&junk, lowerFloor);
            break;
         } // end switch
      } // end if
   } // end for
}

//=============================================================================
//
// Pods and Pod Generators
//

void A_PodPain(mobj_t *actor)
{
   int i;
   int count;
   int chance;
   mobj_t *goo;
   static int gooType = -1;

   if(!serverside)
      return;

   if(gooType == -1)
      gooType = E_SafeThingType(MT_PODGOO);

   chance = P_Random(pr_podpain);

   if(chance < 128)
      return;

   count = (chance > 240) ? 2 : 1;

   for(i = 0; i < count; ++i)
   {
      goo = P_SpawnMobj(actor->x, actor->y, actor->z + 48 * FRACUNIT, gooType);

      if(CS_SERVER)
         SV_BroadcastActorSpawned(goo);

      P_SetTarget<mobj_t>(&goo->target, actor);
      if(CS_SERVER)
         SV_BroadcastActorTarget(goo, CS_AT_TARGET);

      goo->momx = P_SubRandom(pr_podpain) << 9;
      goo->momy = P_SubRandom(pr_podpain) << 9;
      goo->momz = (FRACUNIT >> 1) + (P_Random(pr_podpain) << 9);
   }
}

void A_RemovePod(mobj_t *actor)
{
   // actor->tracer points to the generator that made this pod --
   // this method is save game safe and doesn't require any new
   // fields

   if(actor->tracer)
   {
      if(actor->tracer->counters[0] > 0)
         actor->tracer->counters[0]--;
   }
}

// Note on MAXGENPODS: unlike the limit on PE lost souls, this
// limit makes inarguable sense. If you remove it, areas with
// pod generators will become so crowded with pods that they'll
// begin flying around the map like mad. So, this limit isn't a
// good candidate for removal; it's a necessity.

#define MAXGENPODS 16

void A_MakePod(mobj_t *actor)
{
   angle_t angle;
   fixed_t move;
   mobj_t *mo;
   fixed_t x, y;

   if(!serverside)
      return;

   // limit pods per generator to avoid crowding, slow-down
   if(actor->counters[0] >= MAXGENPODS)
      return;

   x = actor->x;
   y = actor->y;

   mo = P_SpawnMobj(x, y, ONFLOORZ, E_SafeThingType(MT_POD));

   if(!P_CheckPosition(mo, x, y))
   {
      mo->removeThinker();
      return;
   }

   if(CS_SERVER)
      SV_BroadcastActorSpawned(mo);

   P_SetMobjState(mo, E_SafeState(S_POD_GROW1));
   S_StartSound(mo, sfx_newpod);

   // give the pod some random momentum
   angle = P_Random(pr_makepod) << 24;
   move  = 9 * FRACUNIT >> 1;

   P_ThrustMobj(mo, angle, move);

   // use tracer field to link pod to generator, and increment
   // generator's pod count
   P_SetTarget<mobj_t>(&mo->tracer, actor);
   if(CS_SERVER)
      SV_BroadcastActorTarget(mo, CS_AT_TRACER);

   actor->counters[0]++;
}

//=============================================================================
//
// Volcano Actions
//

//
// A_VolcanoBlast
//
// Called when a volcano is ready to erupt.
//
void A_VolcanoBlast(mobj_t *actor)
{
   static int ballType = -1;
   int i, numvolcballs;
   mobj_t *volcball;
   angle_t angle;

   if(!serverside)
      return;

   if(ballType == -1)
      ballType = E_SafeThingType(MT_VOLCANOBLAST);

   // spawn 1 to 3 volcano balls
   numvolcballs = (P_Random(pr_volcano) % 3) + 1;

   for(i = 0; i < numvolcballs; ++i)
   {
      volcball = P_SpawnMobj(
         actor->x, actor->y, actor->z + 44 * FRACUNIT, ballType
      );

      if(CS_SERVER)
         SV_BroadcastActorSpawned(volcball);

      P_SetTarget<mobj_t>(&volcball->target, actor);
      if(CS_SERVER)
         SV_BroadcastActorTarget(volcball, CS_AT_TARGET);

      S_StartSound(volcball, sfx_bstatk);

      // shoot at a random angle
      volcball->angle = P_Random(pr_volcano) << 24;
      angle = volcball->angle >> ANGLETOFINESHIFT;

      // give it some momentum
      volcball->momx = finecosine[angle];
      volcball->momy = finesine[angle];
      volcball->momz = (5 * FRACUNIT >> 1) + (P_Random(pr_volcano) << 10);

      // check if it hit something immediately
      P_CheckMissileSpawn(volcball);
   }
}

//
// A_VolcBallImpact
//
// Called when a volcano ball hits something.
//
void A_VolcBallImpact(mobj_t *actor)
{
   static int sballType = -1;
   int i;
   mobj_t *svolcball;
   angle_t angle;

   if(!serverside)
      return;

   if(sballType == -1)
      sballType = E_SafeThingType(MT_VOLCANOTBLAST);

   // if the thing hit the floor, move it up so that the little
   // volcano balls don't hit the floor immediately
   if(actor->z <= actor->floorz)
   {
      actor->flags |= MF_NOGRAVITY;
      actor->flags2 &= ~MF2_LOGRAV;
      actor->z += 28*FRACUNIT;
   }

   // do some radius damage
   P_RadiusAttack(actor, actor->target, 25, actor->info->mod);

   // spawn 4 little volcano balls
   for(i = 0; i < 4; ++i)
   {
      svolcball = P_SpawnMobj(actor->x, actor->y, actor->z, sballType);

      if(CS_SERVER)
         SV_BroadcastActorSpawned(svolcball);

      // pass on whatever shot the original volcano ball
      P_SetTarget<mobj_t>(&svolcball->target, actor->target);
      if(CS_SERVER)
         SV_BroadcastActorTarget(svolcball, CS_AT_TARGET);

      svolcball->angle = i * ANG90;
      angle = svolcball->angle >> ANGLETOFINESHIFT;

      // give them some momentum
      svolcball->momx = FixedMul(7 * FRACUNIT / 10, finecosine[angle]);
      svolcball->momy = FixedMul(7 * FRACUNIT / 10, finesine[angle]);
      svolcball->momz = FRACUNIT + (P_Random(pr_svolcano) << 9);

      // check if it hit something immediately
      P_CheckMissileSpawn(svolcball);
   }
}

//=============================================================================
//
// Knight Actions
//

//
// A_KnightAttack
//
// Shoots one of two missiles, depending on whether a Knight
// Ghost or some other object uses it.
//
void A_KnightAttack(mobj_t *actor)
{
   static int ghostType = -1, axeType = -1, redAxeType = -1;

   if(!serverside)
      return;

   // resolve thing types only once for max speed
   if(ghostType == -1)
   {
      ghostType  = E_ThingNumForDEHNum(MT_KNIGHTGHOST);
      axeType    = E_SafeThingType(MT_KNIGHTAXE);
      redAxeType = E_SafeThingType(MT_REDAXE);
   }

   if(!actor->target)
      return;

   if(P_CheckMeleeRange(actor))
   {
      int dmg = ((P_Random(pr_knightat1) & 7) + 1) * 3;
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
      S_StartSound(actor, sfx_kgtat2);
   }
   else
   {
      S_StartSound(actor, actor->info->attacksound);

      if(serverside)
      {
         if(actor->type == ghostType || P_Random(pr_knightat2) < 40)
         {
            P_SpawnMissile(
               actor, actor->target, redAxeType, actor->z + 36 * FRACUNIT
            );
         }
         else
         {
            P_SpawnMissile(
               actor, actor->target, axeType, actor->z + 36 * FRACUNIT
            );
         }
      }
   }
}

//
// A_DripBlood
//
// Throws some Heretic blood objects out from the source thing.
//
void A_DripBlood(mobj_t *actor)
{
   mobj_t *mo;
   fixed_t x, y;

   if(!serverside)
      return;

   x = actor->x + (P_SubRandom(pr_dripblood) << 11);
   y = actor->y + (P_SubRandom(pr_dripblood) << 11);

   mo = P_SpawnMobj(x, y, actor->z, E_SafeThingType(MT_HTICBLOOD));

   mo->momx = P_SubRandom(pr_dripblood) << 10;
   mo->momy = P_SubRandom(pr_dripblood) << 10;

   mo->flags2 |= MF2_LOGRAV;

   if(CS_SERVER)
      SV_BroadcastActorSpawned(mo);
}

//=============================================================================
//
// Beast Actions
//

void A_BeastAttack(mobj_t *actor)
{
   if(!serverside)
      return;

   if(!actor->target)
      return;

   S_StartSound(actor, actor->info->attacksound);

   if(P_CheckMeleeRange(actor))
   {
      int dmg = ((P_Random(pr_beastbite) & 7) + 1) * 3;
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
   }
   else
   {
      P_SpawnMissile(
         actor,
         actor->target,
         E_SafeThingType(MT_BEASTBALL),
         actor->z + DEFAULTMISSILEZ
      );
   }
}

static const char *kwds_A_BeastPuff[] =
{
   "momentum",       // 0
   "nomomentum",     // 1
};

static argkeywd_t beastkwds =
{
   kwds_A_BeastPuff,
   sizeof(kwds_A_BeastPuff) / sizeof(const char *)
};

void A_BeastPuff(mobj_t *actor)
{
   // 07/29/04: allow momentum to be disabled
   int momentumToggle = E_ArgAsKwd(actor->state->args, 0, &beastkwds, 0);

   if(!serverside)
      return;

   if(P_Random(pr_puffy) > 64)
   {
      fixed_t x, y, z;
      mobj_t *mo;

      // Note: this actually didn't work as intended in Heretic
      // because there, they gave it no momenta. A missile has
      // to be moving to inflict any damage. Doing this makes the
      // smoke a little dangerous. It also requires the missile's
      // target to be passed on, however, since otherwise the
      // Weredragon that shot this missile can get hurt by it.

      x = actor->x + (P_SubRandom(pr_puffy) << 10);
      y = actor->y + (P_SubRandom(pr_puffy) << 10);
      z = actor->z + (P_SubRandom(pr_puffy) << 10);

      mo = P_SpawnMobj(x, y, z, E_SafeThingType(MT_PUFFY));

      if(!momentumToggle)
      {
         mo->momx = -(actor->momx / 16);
         mo->momy = -(actor->momy / 16);
      }

      if(CS_SERVER)
         SV_BroadcastActorSpawned(mo);

      // pass on the beast so that it doesn't hurt itself
      P_SetTarget<mobj_t>(&mo->target, actor->target);
      if(CS_SERVER)
         SV_BroadcastActorTarget(mo, CS_AT_TARGET);
   }
}

//=============================================================================
//
// Ophidian Actions
//

void A_SnakeAttack(mobj_t *actor)
{
   if(serverside)
   {
      if(!actor->target)
      {
         // avoid going through other attack frames if target is gone
         P_SetMobjState(actor, actor->info->spawnstate);
         return;
      }
   }

   S_StartSound(actor, actor->info->attacksound);
   A_FaceTarget(actor);

   if(serverside)
   {
      P_SpawnMissile(
         actor,
         actor->target,
         E_SafeThingType(MT_SNAKEPRO_A),
         actor->z + DEFAULTMISSILEZ
      );
   }
}

void A_SnakeAttack2(mobj_t *actor)
{
   if(serverside)
   {
      if(!actor->target)
      {
         // avoid going through other attack frames if target is gone
         P_SetMobjState(actor, actor->info->spawnstate);
         return;
      }
   }

   S_StartSound(actor, actor->info->attacksound);
   A_FaceTarget(actor);

   if(serverside)
   {
      P_SpawnMissile(
         actor,
         actor->target,
         E_SafeThingType(MT_SNAKEPRO_B),
         actor->z + DEFAULTMISSILEZ
      );
   }
}

//=============================================================================
//
// Maulotaur Actions
//

//
// A_MinotaurAtk1
//
// Maulotaur melee attack. Big hammer, squishes player.
//
void A_MinotaurAtk1(mobj_t *actor)
{
   int dmg;
   player_t *player;

   if(!actor->target)
      return;

   S_StartSound(actor, sfx_stfpow);

   if(P_CheckMeleeRange(actor))
   {
      dmg = ((P_Random(pr_minatk1) & 7) + 1) * 4;
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);

      // if target is player, make the viewheight go down
      if((player = actor->target->player) != NULL)
         player->deltaviewheight = -16 * FRACUNIT;
   }
}

//
// P_CheckMntrCharge
//
// Returns true if the Maulotaur should do a charge attack.
//
d_inline static
boolean P_CheckMntrCharge(fixed_t dist, mobj_t *actor, mobj_t *target)
{
   return (target->z + target->height > actor->z &&          // check heights
           target->z + target->height < actor->z + actor->height &&
           dist > 64 * FRACUNIT && dist < 512 * FRACUNIT &&  // check distance
           P_Random(pr_mindist) < 150);                      // random factor
}

//
// P_CheckFloorFire
//
// Returns true if the Maulotaur should use floor fire.
//
d_inline static boolean P_CheckFloorFire(fixed_t dist, mobj_t *target)
{
   return (target->z == target->floorz &&   // target on floor?
           dist < 576 * FRACUNIT &&         // target in range?
           P_Random(pr_mindist) < 220);     // random factor
}

//
// A_MinotaurDecide
//
// Picks a Maulotaur attack.
//
void A_MinotaurDecide(mobj_t *actor)
{
   angle_t angle;
   mobj_t *target;
   int dist;

   if(!(target = actor->target))
      return;

   S_StartSound(actor, sfx_minsit);

#ifdef R_LINKEDPORTALS
   dist = P_AproxDistance(actor->x - getTargetX(actor),
                          actor->y - getTargetY(actor));
#else
   dist = P_AproxDistance(actor->x - target->x, actor->y - target->y);
#endif

   // charge attack
   if(P_CheckMntrCharge(dist, actor, target))
   {
      // set to charge state and start skull-flying
      P_SetMobjStateNF(actor, E_SafeState(S_MNTR_ATK4_1));
      actor->flags |= MF_SKULLFLY;
      A_FaceTarget(actor);

      // give him momentum
      angle = actor->angle >> ANGLETOFINESHIFT;
      actor->momx = FixedMul(13*FRACUNIT, finecosine[angle]);
      actor->momy = FixedMul(13*FRACUNIT, finesine[angle]);

      // set a timer
      actor->counters[0] = TICRATE >> 1;
   }
   else if(P_CheckFloorFire(dist, target))
   {
      // floor fire
      P_SetMobjState(actor, E_SafeState(S_MNTR_ATK3_1));
      actor->counters[1] = 0;
   }
   else
      A_FaceTarget(actor);

   // Fall through to swing attack
}

//
// A_MinotaurCharge
//
// Called while the Maulotaur is charging.
//
void A_MinotaurCharge(mobj_t *actor)
{
   static int puffType = -1;
   mobj_t *puff;

   if(!serverside)
      return;

   if(puffType == -1)
      puffType = E_SafeThingType(MT_PHOENIXPUFF);

   if(actor->counters[0]) // test charge timer
   {
      // spawn some smoke and count down the charge
      puff = P_SpawnMobj(actor->x, actor->y, actor->z, puffType);
      puff->momz = 2 * FRACUNIT;

      if(CS_SERVER)
         SV_BroadcastActorSpawned(puff);

      --actor->counters[0];
   }
   else
   {
      // end of the charge
      actor->flags &= ~MF_SKULLFLY;
      P_SetMobjState(actor, actor->info->seestate);
   }
}

//
// A_MinotaurAtk2
//
// Fireball attack for Maulotaur
//
void A_MinotaurAtk2(mobj_t *actor)
{
   int dmg;
   static int mntrfxType = -1;
   mobj_t *mo;
   angle_t angle;
   fixed_t momz;

   if(mntrfxType == -1)
      mntrfxType = E_SafeThingType(MT_MNTRFX1);

   if(!actor->target)
      return;

   S_StartSound(actor, sfx_minat2);

   if(!serverside)
      return;

   if(P_CheckMeleeRange(actor)) // hit directly
   {
      dmg = ((P_Random(pr_minatk2) & 7) + 1) * 5;
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
   }
   else // missile spread attack
   {
      fixed_t z = actor->z + 40*FRACUNIT;

      // shoot a missile straight
      mo = P_SpawnMissile(actor, actor->target, mntrfxType, z);
      S_StartSound(mo, sfx_minat2);

      // shoot 4 more missiles in a spread
      momz = mo->momz;
      angle = mo->angle;
      P_SpawnMissileAngle(actor, mntrfxType, angle - (ANG45/8),  momz, z);
      P_SpawnMissileAngle(actor, mntrfxType, angle + (ANG45/8),  momz, z);
      P_SpawnMissileAngle(actor, mntrfxType, angle - (ANG45/16), momz, z);
      P_SpawnMissileAngle(actor, mntrfxType, angle + (ANG45/16), momz, z);
   }
}

//
// A_MinotaurAtk3
//
// Performs floor fire attack, or melee if in range.
//
void A_MinotaurAtk3(mobj_t *actor)
{
   int dmg;
   static int mntrfxType = -1;
   mobj_t *mo;
   player_t *player;

   if(mntrfxType == -1)
      mntrfxType = E_SafeThingType(MT_MNTRFX2);

   if(!actor->target)
      return;

   if(P_CheckMeleeRange(actor))
   {
      dmg = ((P_Random(pr_minatk3) & 7) + 1) * 5;
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);

      // if target is player, decrease viewheight
      if((player = actor->target->player) != NULL)
         player->deltaviewheight = -16 * FRACUNIT;
   }
   else
   {
      // floor fire attack
      if(serverside)
         mo = P_SpawnMissile(actor, actor->target, mntrfxType, ONFLOORZ);
      S_StartSound(mo, sfx_minat1);
   }

   if(P_Random(pr_minatk3) < 192 && actor->counters[1] == 0)
   {
      P_SetMobjState(actor, E_SafeState(S_MNTR_ATK3_4));
      actor->counters[1] = 1;
   }
}

//
// A_MntrFloorFire
//
// Called by floor fire missile as it moves.
// Spawns small burning flames.
//
void A_MntrFloorFire(mobj_t *actor)
{
   static int mntrfxType = -1;
   mobj_t *mo;
   fixed_t x, y;

   if(!serverside)
      return;

   if(mntrfxType == -1)
      mntrfxType = E_SafeThingType(MT_MNTRFX3);

   // set actor to floor
   actor->z = actor->floorz;

   // determine spawn coordinates for small flame
   x = actor->x + (P_SubRandom(pr_mffire) << 10);
   y = actor->y + (P_SubRandom(pr_mffire) << 10);

   // spawn the flame
   mo = P_SpawnMobj(x, y, ONFLOORZ, mntrfxType);

   if(CS_SERVER)
      SV_BroadcastActorSpawned(mo);

   // pass on the Maulotaur as the source of damage
   P_SetTarget<mobj_t>(&mo->target, actor->target);
   if(CS_SERVER)
      SV_BroadcastActorTarget(mo, CS_AT_TARGET);

   // give it a bit of momentum and then check to see if it hit something
   mo->momx = 1;
   P_CheckMissileSpawn(mo);
}

//=============================================================================
//
// Iron Lich Actions
//

//
// A_LichFire
//
// Spawns a column of expanding fireballs. Called by A_LichAttack,
// but also available separately.
//
void A_LichFire(mobj_t *actor)
{
   int headfxType, frameNum;
   mobj_t *target, *baseFire, *fire;
   int i;

   headfxType = E_SafeThingType(MT_LICHFX3);
   frameNum   = E_SafeState(S_LICHFX3_4);

   if(!(target = actor->target))
      return;

   if(serverside)
   {
      // spawn the parent fireball
      baseFire = P_SpawnMissile(
         actor,
         target,
         headfxType,
         actor->z + DEFAULTMISSILEZ
      );
      // set it to S_HEADFX3_4 so that it doesn't grow
      P_SetMobjState(baseFire, frameNum);
   }

   S_StartSound(actor, sfx_hedat1);

   if(serverside)
   {
      for(i = 0; i < 5; ++i)
      {
         fire = P_SpawnMobj(baseFire->x, baseFire->y, baseFire->z, headfxType);
         if(CS_SERVER)
            SV_BroadcastActorSpawned(fire);

         // pass on the lich as the originator
         P_SetTarget<mobj_t>(&fire->target, baseFire->target);
         if(CS_SERVER)
            SV_BroadcastActorTarget(fire, CS_AT_TARGET);

         // inherit the motion properties of the parent fireball
         fire->angle = baseFire->angle;
         fire->momx  = baseFire->momx;
         fire->momy  = baseFire->momy;
         fire->momz  = baseFire->momz;

         // start out with zero damage
         fire->damage = 0;

         // set a counter for growth
         fire->counters[0] = (i + 1) << 1;

         P_CheckMissileSpawn(fire);
      }
   }
}

//
// A_LichWhirlwind
//
// Spawns a heat-seeking tornado. Called by A_LichAttack, but also
// available separately.
//
void A_LichWhirlwind(mobj_t *actor)
{
   static int wwType = -1;
   mobj_t *mo, *target;

   if(!(target = actor->target))
      return;

   if(serverside)
   {
      if(wwType == -1)
         wwType = E_SafeThingType(MT_WHIRLWIND);

      mo = P_SpawnMissile(actor, target, wwType, actor->z);

      // use mo->tracer to track target
      P_SetTarget<mobj_t>(&mo->tracer, target);
      if(CS_SERVER)
         SV_BroadcastActorTarget(mo, CS_AT_TRACER);
   
      mo->counters[0] = 20 * TICRATE; // duration
      mo->counters[1] = 50;           // timer for active sound
      mo->counters[2] = 60;           // explocount limit
   }

   S_StartSound(actor, sfx_hedat3);
}

//
// A_LichAttack
//
// Main Iron Lich attack logic.
//
void A_LichAttack(mobj_t *actor)
{
   int dmg;
   static int fxType = -1;
   mobj_t *target;
   int randAttack, dist;

   // Distance threshold = 512 units
   // Probabilities:
   // Attack       Close   Far
   // -----------------------------
   // Ice ball       20%   60%
   // Fire column    40%   20%
   // Whirlwind      40% : 20%

   if(fxType == -1)
      fxType = E_SafeThingType(MT_LICHFX1);

   if(!(target = actor->target))
      return;

   A_FaceTarget(actor);

   // hit directly when in melee range
   if(P_CheckMeleeRange(actor))
   {
      dmg = ((P_Random(pr_lichmelee) & 7) + 1) * 6;
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
      return;
   }

   // determine distance and use it to alter attack probabilities
#ifdef R_LINKEDPORTALS
   dist = (P_AproxDistance(actor->x - getTargetX(actor),
                           actor->y - getTargetY(actor)) > 512 * FRACUNIT);
#else
   dist = (
      P_AproxDistance(actor->x-target->x, actor->y-target->y) > 512 * FRACUNIT
   );
#endif

   randAttack = P_Random(pr_lichattack);

   if(randAttack < (dist ? 150 : 50))
   {
      // ice attack
      if(serverside)
         P_SpawnMissile(actor, target, fxType, actor->z + DEFAULTMISSILEZ);
      S_StartSound(actor, sfx_hedat2);
   }
   else if(randAttack < (dist ? 200 : 150))
      A_LichFire(actor);
   else
      A_LichWhirlwind(actor);
}

//
// A_WhirlwindSeek
//
// Special homing maintenance pointer for whirlwinds.
//
void A_WhirlwindSeek(mobj_t *actor)
{
   // decrement duration counter
   if((actor->counters[0] -= 3) < 0)
   {
      actor->momx = actor->momy = actor->momz = 0;
      P_SetMobjState(actor, actor->info->deathstate);
      actor->flags &= ~MF_MISSILE;
      return;
   }

   // decrement active sound counter
   if((actor->counters[1] -= 3) < 0)
   {
      actor->counters[1] = 58 + (P_Random(pr_whirlseek) & 31);
      S_StartSound(actor, sfx_hedat3);
   }

   if(!serverside)
      return;

   // test if tracer has become an invalid target
   if(!ancient_demo && actor->tracer &&
      (actor->tracer->flags3 & MF3_GHOST ||
       actor->tracer->health < 0))
   {
      mobj_t *originator = actor->target;
      mobj_t *origtarget = originator ? originator->target : NULL;

      // See if the Lich has a new target; if so, maybe chase it now.
      // This keeps the tornado from sitting around uselessly.
      if(originator && origtarget && actor->tracer != origtarget &&
         origtarget->health > 0 &&
         !(origtarget->flags3 & MF3_GHOST) &&
         !(originator->flags & origtarget->flags & MF_FRIEND))
      {
         P_SetTarget<mobj_t>(&actor->tracer, origtarget);
         if(CS_SERVER)
            SV_BroadcastActorTarget(actor, CS_AT_TRACER);
      }
      else
         return;
   }

   // follow the target
   P_HticTracer(actor, ANGLE_1 * 10, ANGLE_1 * 30);
}

//
// A_LichIceImpact
//
// Called when a Lich ice ball hits something. Shatters into
// shards that fly in all directions.
//
void A_LichIceImpact(mobj_t *actor)
{
   static int fxType = -1;
   int i;
   angle_t angle;
   mobj_t *shard;

   if(!serverside)
      return;

   if(fxType == -1)
      fxType = E_SafeThingType(MT_LICHFX2);

   for(i = 0; i < 8; ++i)
   {
      shard = P_SpawnMobj(actor->x, actor->y, actor->z, fxType);

      if(CS_SERVER)
         SV_BroadcastActorSpawned(shard);

      P_SetTarget<mobj_t>(&shard->target, actor->target);
      if(CS_SERVER)
         SV_BroadcastActorTarget(shard, CS_AT_TARGET);

      // send shards out every 45 degrees
      shard->angle = i * ANG45;

      // set momenta
      angle = shard->angle >> ANGLETOFINESHIFT;
      shard->momx = FixedMul(shard->info->speed, finecosine[angle]);
      shard->momy = FixedMul(shard->info->speed, finesine[angle]);
      shard->momz = -3 * FRACUNIT / 5;

      // check the spawn to see if it hit immediately
      P_CheckMissileSpawn(shard);
   }
}

//
// A_LichFireGrow
//
// Called by Lich fire pillar fireballs so that they can expand.
//
void A_LichFireGrow(mobj_t *actor)
{
   int frameNum = E_SafeState(S_LICHFX3_4);

   actor->z += 9 * FRACUNIT;

   if(--actor->counters[0] == 0) // count down growth timer
   {
      actor->damage = actor->info->damage; // restore normal damage
      P_SetMobjState(actor, frameNum);  // don't grow any more
   }
}

//=============================================================================
//
// Imp Actions
//

//
// A_ImpChargeAtk
//
// Almost identical to the Lost Soul's attack, but adds a frequent
// failure to attack so that the imps do not constantly charge.
// Note that this makes them nearly paralyzed in "Black Plague"
// skill level, however...
//
void A_ImpChargeAtk(mobj_t *actor)
{
   if(!actor->target || P_Random(pr_impcharge) > 64)
   {
      P_SetMobjState(actor, actor->info->seestate);
   }
   else
   {
      S_StartSound(actor, actor->info->attacksound);
      P_SkullFly(actor, 12 * FRACUNIT);
   }
}

//
// A_ImpMeleeAtk
//
void A_ImpMeleeAtk(mobj_t *actor)
{
   int dmg;

   if(!actor->target)
      return;

   S_StartSound(actor, actor->info->attacksound);

   if(P_CheckMeleeRange(actor))
   {
      dmg = 5 + (P_Random(pr_impmelee) & 7);
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
   }
}

//
// A_ImpMissileAtk
//
// Leader Imp's missile/melee attack
//
void A_ImpMissileAtk(mobj_t *actor)
{
   int dmg;
   static int fxType = -1;

   if(!actor->target)
      return;

   if(fxType == -1)
      fxType = E_SafeThingType(MT_IMPBALL);

   S_StartSound(actor, actor->info->attacksound);

   if(P_CheckMeleeRange(actor))
   {
      dmg = 5 + (P_Random(pr_impmelee2) & 7);
      P_DamageMobj(actor->target, actor, actor, dmg, MOD_HIT);
   }
   else if(serverside)
      P_SpawnMissile(actor, actor->target, fxType, actor->z + DEFAULTMISSILEZ);
}

//
// A_ImpDeath
//
// Called when the imp dies normally.
//
void A_ImpDeath(mobj_t *actor)
{
   actor->flags &= ~MF_SOLID;
   actor->flags2 |= MF2_FOOTCLIP;

   if(actor->z <= actor->floorz && actor->info->crashstate != NullStateNum)
   {
      actor->intflags |= MIF_CRASHED;
      P_SetMobjState(actor, actor->info->crashstate);
   }
}

//
// A_ImpXDeath1
//
// Called on imp extreme death. First half of action
//
void A_ImpXDeath1(mobj_t *actor)
{
   actor->flags &= ~MF_SOLID;
   actor->flags |= MF_NOGRAVITY;
   actor->flags2 |= MF2_FOOTCLIP;

   // set special1 so the crashstate goes to the
   // extreme crash death
   actor->counters[0] = 666;
}

//
// A_ImpXDeath2
//
// Called on imp extreme death. Second half of action.
//
void A_ImpXDeath2(mobj_t *actor)
{
   actor->flags &= ~MF_NOGRAVITY;

   if(actor->z <= actor->floorz && actor->info->crashstate != NullStateNum)
   {
      actor->intflags |= MIF_CRASHED;
      P_SetMobjState(actor, actor->info->crashstate);
   }
}

//
// A_ImpExplode
//
// Called from imp crashstate.
//
void A_ImpExplode(mobj_t *actor)
{
   int fxType1, fxType2, stateNum;
   mobj_t *mo;

   if(!serverside)
      return;

   // haleyjd 09/13/04: it's possible for an imp to enter its
   // crash state between calls to ImpXDeath1 and ImpXDeath2 --
   // if this happens, the NOGRAVITY flag must be cleared here,
   // or else it will remain indefinitely.

   actor->flags &= ~MF_NOGRAVITY;

   fxType1  = E_SafeThingType(MT_IMPCHUNK1);
   fxType2  = E_SafeThingType(MT_IMPCHUNK2);
   stateNum = E_SafeState(S_IMP_XCRASH1);

   mo = P_SpawnMobj(actor->x, actor->y, actor->z, fxType1);
   mo->momx = P_SubRandom(pr_impcrash) << 10;
   mo->momy = P_SubRandom(pr_impcrash) << 10;
   mo->momz = 9 * FRACUNIT;

   if(CS_SERVER)
      SV_BroadcastActorSpawned(mo);

   mo = P_SpawnMobj(actor->x, actor->y, actor->z, fxType2);
   mo->momx = P_SubRandom(pr_impcrash) << 10;
   mo->momy = P_SubRandom(pr_impcrash) << 10;
   mo->momz = 9 * FRACUNIT;

   if(CS_SERVER)
      SV_BroadcastActorSpawned(mo);

   // extreme death crash
   if(actor->counters[0] == 666)
      P_SetMobjState(actor, stateNum);
}

//=============================================================================
//
// Player Projectiles
//

//
// A_PhoenixPuff
//
// Parameters:
// * args[0] : thing type
//
void A_PhoenixPuff(mobj_t *actor)
{
   int thingtype;
   mobj_t *puff;
   angle_t angle;

   if(!serverside)
      return;

   thingtype = E_ArgAsThingNum(actor->state->args, 0);

   P_HticTracer(actor, ANGLE_1 * 5, ANGLE_1 * 10);

   puff = P_SpawnMobj(actor->x, actor->y, actor->z, thingtype);

   angle = actor->angle + ANG90;
   angle >>= ANGLETOFINESHIFT;
   puff->momx = FixedMul(13 * FRACUNIT / 10, finecosine[angle]);
   puff->momy = FixedMul(13 * FRACUNIT / 10, finesine[angle]);

   if(CS_SERVER)
      SV_BroadcastActorSpawned(puff);

   puff = P_SpawnMobj(actor->x, actor->y, actor->z, thingtype);

   angle = actor->angle - ANG90;
   angle >>= ANGLETOFINESHIFT;
   puff->momx = FixedMul(13 * FRACUNIT / 10, finecosine[angle]);
   puff->momy = FixedMul(13 * FRACUNIT / 10, finesine[angle]);

   if(CS_SERVER)
      SV_BroadcastActorSpawned(puff);
}

// EOF

