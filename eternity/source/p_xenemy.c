// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2007 James Haley
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
//
// Hexen- and ZDoom-inspired action functions
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "a_small.h"
#include "doomstat.h"
#include "e_things.h"
#include "e_states.h"
#include "p_enemy.h"
#include "p_xenemy.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_tick.h"
#include "e_sound.h"
#include "s_sound.h"
#include "e_lib.h"

// action routines from other modules
void A_Look(mobj_t *actor);
void A_Chase(mobj_t *actor);
void A_Pain(mobj_t *actor);
void A_FaceTarget(mobj_t *actor);

// extern data
extern int FloatBobOffsets[];

//
// T_QuakeThinker
//
// Earthquake effect thinker.
//
void T_QuakeThinker(quakethinker_t *qt)
{
   int i, tics;
   mobj_t *soundorg = (mobj_t *)&qt->origin;
   sfxinfo_t *quakesound;
   
   // quake is finished?
   if(qt->duration == 0)
   {
      S_StopSound(soundorg, CHAN_ALL);
      P_RemoveThinker(&(qt->origin.thinker));
      return;
   }
   
   quakesound = E_SoundForName("Earthquake");

   // loop quake sound
   if(quakesound && !S_CheckSoundPlaying(soundorg, quakesound))
      S_StartSfxInfo(soundorg, quakesound, 127, ATTN_NORMAL, true, CHAN_AUTO);

   tics = qt->duration--;

   // do some rumbling
   for(i = 0; i < MAXPLAYERS; ++i)
   {
      if(playeringame[i])
      {
         player_t *p  = &players[i];
         mobj_t   *mo = p->mo;
         fixed_t  dst = P_AproxDistance(qt->origin.x - mo->x, 
                                        qt->origin.y - mo->y);

         // test if player is in quake radius
         // haleyjd 04/16/07: only set p->quake when qt->intensity is greater;
         // this way, the strongest quake in effect always wins out
         if(dst < qt->quakeRadius && p->quake < qt->intensity)
            p->quake = qt->intensity;

         // every 2 tics, the player may be damaged
         if(!(tics & 1))
         {
            angle_t  thrustangle;   
            
            // test if in damage radius and on floor
            if(dst < qt->damageRadius && mo->z <= mo->floorz)
            {
               if(P_Random(pr_quake) < 50)
               {
                  P_DamageMobj(mo, NULL, NULL, P_Random(pr_quakedmg) % 8 + 1,
                               MOD_QUAKE);
               }
               thrustangle = (359 * P_Random(pr_quake) / 255) * ANGLE_1;
               P_ThrustMobj(mo, thrustangle, qt->intensity * FRACUNIT / 2);
            }
         }
      }
   }
}

//
// P_StartQuake
//
// Starts an earthquake at each object with the tid in args[4]
//
boolean P_StartQuake(int *args)
{
   mobj_t *mo  = NULL;
   boolean ret = false;

   while((mo = P_FindMobjFromTID(args[4], mo, NULL)))
   {
      quakethinker_t *qt;
      ret = true;

      qt = Z_Malloc(sizeof(*qt), PU_LEVSPEC, NULL);
      qt->origin.thinker.function = T_QuakeThinker;

      P_AddThinker(&(qt->origin.thinker));

      qt->intensity    = args[0];
      qt->duration     = args[1];
      qt->damageRadius = args[2] * (64 * FRACUNIT);
      qt->quakeRadius  = args[3] * (64 * FRACUNIT);

      qt->origin.x       = mo->x;
      qt->origin.y       = mo->y;
      qt->origin.z       = mo->z;
#ifdef R_LINKEDPORTALS
      qt->origin.groupid = mo->groupid;
#endif
   }

   return ret;
}

//
// A_FadeIn
//
// ZDoom-inspired action function, implemented using wiki docs.
//
// args[0] : alpha step
//
void A_FadeIn(mobj_t *mo)
{
   mo->translucency += mo->state->args[0];
   
   if(mo->translucency < 0)
      mo->translucency = 0;
   else if(mo->translucency > FRACUNIT)
      mo->translucency = FRACUNIT;
}

//
// A_FadeOut
//
// ZDoom-inspired action function, implemented using wiki docs.
//
// args[0] : alpha step
//
void A_FadeOut(mobj_t *mo)
{
   mo->translucency -= mo->state->args[0];
   
   if(mo->translucency < 0)
      mo->translucency = 0;
   else if(mo->translucency > FRACUNIT)
      mo->translucency = FRACUNIT;
}

//
// A_ClearSkin
//
// Codepointer needed for Heretic/Hexen/Strife support.
//
void A_ClearSkin(mobj_t *mo)
{
   mo->skin   = NULL;
   mo->sprite = mo->state->sprite;
}

E_Keyword_t kwds_A_PlaySoundEx[] =
{
   { "chan_auto",   0 },
   { "chan_weapon", 1 },
   { "chan_voice",  2 },
   { "chan_item",   3 },
   { "chan_body",   4 },
   { "attn_normal", 0 },
   { "attn_idle",   1 },
   { "attn_static", 2 },
   { "attn_none",   3 },
   { NULL }
};

//
// A_PlaySoundEx
//
// ZDoom-inspired action function, implemented using wiki docs.
//
// args[0] : sound
// args[1] : subchannel
// args[2] : loop?
// args[3] : attenuation
// args[4] : EE extension - volume
//
void A_PlaySoundEx(mobj_t *mo)
{
   int sound    =   mo->state->args[0];
   int channel  =   mo->state->args[1];
   boolean loop = !!mo->state->args[2];
   int attn     =   mo->state->args[3];
   int volume   =   mo->state->args[4];
   sfxinfo_t *sfx = NULL;

   if(!(sfx = E_SoundForDEHNum(sound)))
      return;

   // rangechecking
   if(attn < 0 || attn >= ATTN_NUM)
      attn = ATTN_NORMAL;

   if(channel < CHAN_AUTO || channel >= NUMSCHANNELS)
      channel = CHAN_AUTO;

   // note: volume 0 == 127, for convenience
   if(volume <= 0 || volume > 127)
      volume = 127;

   S_StartSfxInfo(mo, sfx, volume, attn, loop, channel);
}

//
// A_DetonateEx
//
// Equivalent to ZDoom A_Explode extensions.
//
// args[0] : damage
// args[1] : radius
// args[2] : hurt self?
// args[3] : alert?
// args[4] : full damage radius
//
void A_DetonateEx(mobj_t *actor)
{
}

//==============================================================================
// 
// Hexen Codepointers
//

//
// A_SetInvulnerable
//
// Not strictly necessary for EE but a convenience routine nonetheless.
//
void A_SetInvulnerable(mobj_t *actor)
{
   actor->flags2 |= MF2_INVULNERABLE;
}

void A_UnSetInvulnerable(mobj_t *actor)
{
   actor->flags2 &= ~MF2_INVULNERABLE;
}

//
// A_SetReflective
//
// As above.
//
void A_SetReflective(mobj_t *actor)
{
   actor->flags2 |= MF2_REFLECTIVE;

   // note: no special case for Centaurs; use SetFlags pointer instead
}

void A_UnSetReflective(mobj_t *actor)
{
   actor->flags2 &= ~MF2_REFLECTIVE;
}

//
// P_UpdateMorphedMonster
//
boolean P_UpdateMorphedMonster(mobj_t *actor, int tics)
{
   // HEXEN_TODO
   return false;
}

//
// A_PigLook
//
void A_PigLook(mobj_t *actor)
{
   if(P_UpdateMorphedMonster(actor, 10))
      return;

   A_Look(actor);
}

//
// A_PigChase
//
void A_PigChase(mobj_t *actor)
{
   if(P_UpdateMorphedMonster(actor, 3))
      return;

   A_Chase(actor);
}

//
// A_PigAttack
//
void A_PigAttack(mobj_t *actor)
{
   if(P_UpdateMorphedMonster(actor, 18))
      return;

   if(!actor->target)
      return;

   /*
   HEXEN_TODO
   if(P_CheckMeleeRange(actor))
   {
      P_DamageMobj(actor->target, actor, actor, 2+(P_Random()&1));
      S_StartSound(actor, SFX_PIG_ATTACK);
   }
   */
}

//
// A_PigPain
//
void A_PigPain(mobj_t *actor)
{
   A_Pain(actor);

   if(actor->z <= actor->floorz)
      actor->momz = 7 * FRACUNIT / 2;
}

// HEXEN_TODO: Dark Servant Maulotaur variation

//
// A_HexenExplode
//
// Hexen explosion effects pointer
//
void A_HexenExplode(mobj_t *actor)
{
   // HEXEN_TODO
   // Note: requires new blast radius semantics
}

// HEXEN_TODO: A_FreeTargMobj needed?

//
// A_SerpentUnHide
//
void A_SerpentUnHide(mobj_t *actor)
{
   actor->flags2    &= ~MF2_DONTDRAW;
   actor->floorclip  = 24 * FRACUNIT;
}

//
// A_SerpentHide
//
void A_SerpentHide(mobj_t *actor)
{
   actor->flags2    |= MF2_DONTDRAW;
   actor->floorclip  = 0;
}

//
// A_SerpentChase
//
void A_SerpentChase(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_RaiseFloorClip
//
// Note: was A_SerpentRaiseHump in Hexen
// Parameters:
// * args[0] == amount to reduce floorclip in eighths of a unit
//
void A_RaiseFloorClip(mobj_t *actor)
{
   fixed_t amt = actor->state->args[0] * FRACUNIT / 8;

   actor->floorclip -= amt;
}

//
// A_LowerFloorClip
//
// Note: was A_SerpentLowerHump in Hexen
// Parameters:
// * args[0] == amount to increase floorclip in eighths of a unit
//
void A_LowerFloorClip(mobj_t *actor)
{
   fixed_t amt = actor->state->args[0] * FRACUNIT / 8;

   actor->floorclip += amt;
}

//
// A_SerpentHumpDecide
//
void A_SerpentHumpDecide(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_SerpentWalk
//
void A_SerpentWalk(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_SerpentCheckForAttack
//
void A_SerpentCheckForAttack(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_SerpentChooseAttack
//
void A_SerpentChooseAttack(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_SerpentMeleeAttack
//
void A_SerpentMeleeAttack(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_SerpentMissileAttack
//
void A_SerpentMissileAttack(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_SerpentSpawnGibs
//
void A_SerpentSpawnGibs(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_SubTics
//
// Note: was A_DelayGib in Hexen
// Parameters:
// * args[0] : mode select (0 = random amt, 1 = use args[1])
// * args[1] : if mode == 0, shift amt; if mode == 1, amount to subtract
//
void A_SubTics(mobj_t *actor)
{
   int mode = (int)(actor->state->args[0]);
   int amt  = (int)(actor->state->args[1]);

   switch(mode)
   {
   case 0:
      actor->tics -= P_Random(pr_subtics) >> amt;
      break;
   case 1:
      actor->tics -= amt;
      break;
   default:
      break;
   }

   // cannot set tics to zero, to avoid state cycles.
   if(actor->tics <= 0)
      actor->tics = 1;
}

//
// A_SerpentHeadCheck
//
void A_SerpentHeadCheck(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_CentaurAttack
//
void A_CentaurAttack(mobj_t *actor)
{
   if(!actor->target)
      return;

   if(P_CheckMeleeRange(actor))
   {
      P_DamageMobj(actor->target, actor, actor, 
                   P_Random(pr_centauratk) % 7 + 3, MOD_HIT);
   }
}

//
// A_CentaurAttack2
//
void A_CentaurAttack2(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// P_TossEquipmentItem
//
// Throws an object away from the source.
//
static void P_TossEquipmentItem(mobj_t *mo, angle_t angle, int momshift, 
                                fixed_t baseZMom)
{
   mo->momx = FixedMul(((P_Random(pr_dropequip) - 128) << momshift) + FRACUNIT,
                       finecosine[angle >> ANGLETOFINESHIFT]);

   mo->momy = FixedMul(((P_Random(pr_dropequip) - 128) << momshift) + FRACUNIT,
                       finesine[angle >> ANGLETOFINESHIFT]);
   
   mo->momz = baseZMom + (P_Random(pr_dropequip) << (momshift - 1));
}

//
// A_DropEquipment
//
// Note: was A_CentaurDropStuff in Hexen
// Parameters:
// * args[0] : thing type 1
// * args[1] : thing type 2
// * args[2] : x/y/z momentum shift (z = x/y - 1)
// * args[3] : base z momentum
// * args[4] : z height
//
void A_DropEquipment(mobj_t *actor)
{
   int moType1    = (int)(actor->state->args[0]);
   int moType2    = (int)(actor->state->args[1]);
   int xyMomShift = (int)(actor->state->args[2]);
   
   fixed_t baseZMom = (int)(actor->state->args[3]) * FRACUNIT;
   fixed_t zHeight  = (int)(actor->state->args[4]) * FRACUNIT;

   mobj_t  *mo;

   mo = P_SpawnMobj(actor->x, actor->y, actor->z + zHeight,
                    E_SafeThingType(moType1));

   P_TossEquipmentItem(mo, actor->angle + ANG90, xyMomShift, baseZMom);
   P_SetTarget(&mo->target, actor);

   mo = P_SpawnMobj(actor->x, actor->y, actor->z + zHeight,
                    E_SafeThingType(moType2));

   P_TossEquipmentItem(mo, actor->angle - ANG90, xyMomShift, baseZMom);
   P_SetTarget(&mo->target, actor);
}

//
// A_CentaurDefend
//
void A_CentaurDefend(mobj_t *actor)
{
   if(!actor->target)
      return;

   A_FaceTarget(actor);

   if(P_CheckMeleeRange(actor) && P_Random(pr_centaurdef) < 32)
   {
      actor->flags2 &= ~(MF2_INVULNERABLE | MF2_REFLECTIVE);
      P_SetMobjState(actor, actor->info->meleestate);
   }
}

//
// A_BishopAttack
//
void A_BishopAttack(mobj_t *actor)
{
   if(!actor->target)
      return;

   S_StartSound(actor, actor->info->attacksound);
   
   if(P_CheckMeleeRange(actor))
   {
      P_DamageMobj(actor->target, actor, actor, 
                   ((P_Random(pr_bishop1) & 7) + 1) * 4, MOD_HIT);
   }
   else
      actor->counters[0] = (P_Random(pr_bishop1) & 3) + 5;
}

void A_BishopAttack2(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_BishopMissileWeave
//
// I'm not even going to pretend to understand how it works.
//
void A_BishopMissileWeave(mobj_t *actor)
{
   fixed_t newX, newY;
   int weaveXY, weaveZ;
   int angle;
   
   weaveXY = actor->counters[1] >> 16;
   weaveZ  = actor->counters[1] & 0xFFFF;
   angle   = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
   
   newX     = actor->x - FixedMul(finecosine[angle], FloatBobOffsets[weaveXY] << 1);
   newY     = actor->y - FixedMul(finesine[angle],   FloatBobOffsets[weaveXY] << 1);   
   weaveXY  = (weaveXY + 2) & 63;   
   newX    += FixedMul(finecosine[angle], FloatBobOffsets[weaveXY] << 1);
   newY    += FixedMul(finesine[angle],   FloatBobOffsets[weaveXY] << 1);
   
   P_TryMove(actor, newX, newY, false);
   
   actor->z -= FloatBobOffsets[weaveZ];
   weaveZ    = (weaveZ + 2) & 63;   
   actor->z += FloatBobOffsets[weaveZ];
   
   actor->counters[1] = weaveZ + (weaveXY << 16);
}

//
// A_BishopDoBlur
//
void A_BishopDoBlur(mobj_t *actor)
{
   actor->counters[0] = (P_Random(pr_bishop2) & 3) + 3; // Random number of blurs

   if(P_Random(pr_bishop2) < 120)
      P_ThrustMobj(actor, actor->angle + ANG90, 11*FRACUNIT); // Thrust left
   else if(P_Random(pr_bishop3) > 125)
      P_ThrustMobj(actor, actor->angle - ANG90, 11*FRACUNIT); // Thrust right
   else
      P_ThrustMobj(actor, actor->angle, 11*FRACUNIT);         // Thrust forward

   // HEXEN_TODO:
   // S_StartSound(actor, SFX_BISHOP_BLUR);
}

//
// A_SpawnBlur
//
// Note: was A_BishopSpawnBlur in Hexen
// Parameters:
// * args[0] : walk state
// * args[1] : attack state
// * args[2] : thing type
//
void A_SpawnBlur(mobj_t *actor)
{
   mobj_t *mo;
   int walkState = E_SafeState(actor->state->args[0]);
   int atkState  = E_SafeState(actor->state->args[1]);
   int thingType = E_SafeThingType(actor->state->args[2]);
   
   if(!--actor->counters[0])
   {
      actor->momx = 0;
      actor->momy = 0;

      if(walkState != NullStateNum && P_Random(pr_spawnblur) > 96)
         P_SetMobjState(actor, walkState);
      else if(atkState != NullStateNum)
         P_SetMobjState(actor, atkState);
   }

   mo = P_SpawnMobj(actor->x, actor->y, actor->z, thingType);
   mo->angle = actor->angle;
}

//
// A_BishopChase
//
void A_BishopChase(mobj_t *actor)
{
   actor->z -= FloatBobOffsets[actor->counters[1]] >> 1;
   actor->counters[1] = (actor->counters[1] + 4) & 63;
   actor->z += FloatBobOffsets[actor->counters[1]] >> 1;
}

//
// A_BishopPainBlur
//
void A_BishopPainBlur(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_DragonInitFlight
//
void A_DragonInitFlight(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_DragonFlight
//
void A_DragonFlight(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_DragonFlap
//
void A_DragonFlap(mobj_t *actor)
{
   A_DragonFlight(actor);

   /*
   HEXEN_TODO:
   S_StartSound(actor, 
      P_Random() < 240 ? SFX_DRAGON_WINGFLAP : actor->info->activesound);
   */
}

//
// A_DragonFX2
//
// Parameters:
// * args[0] : thingtype to spawn
//
void A_DragonFX2(mobj_t *actor)
{
   int thingType = (int)(actor->state->args[0]);
   mobj_t *mo;
   int i;
   int delay;
   
   delay = 16 + (P_Random(pr_dragonfx) >> 3);
   
   for(i = 1 + (P_Random(pr_dragonfx) & 3); i; i--)
   {
      mo = P_SpawnMobj(actor->x + ((P_Random(pr_dragonfx) - 128) << 14), 
                       actor->y + ((P_Random(pr_dragonfx) - 128) << 14), 
                       actor->z + ((P_Random(pr_dragonfx) - 128) << 12),
                       E_SafeThingType(thingType));

      mo->tics = delay + (P_Random(pr_dragonfx) & 3) * i * 2;
      P_SetTarget(&mo->target, actor->target);
   } 
}

//
// A_PainCounterBEQ
//
// Note: was A_DragonPain in Hexen
// Calls A_Pain and branches to specified state if the specified
// thing counter is zero.
// Parameters:
// * args[0] : state to branch into
// * args[1] : counter to examine
//
void A_PainCounterBEQ(mobj_t *actor)
{
   int stateNum   = E_SafeState(actor->state->args[0]);
   int counterNum = (int)(actor->state->args[1]);
   int *counter;

   A_Pain(actor);

   if(counterNum < 0 || counterNum >= NUMMOBJCOUNTERS)
      return; // invalid

   counter = &(actor->counters[counterNum]);

   if(!*counter)
      P_SetMobjState(actor, stateNum);
}

// HEXEN_TODO: A_DragonCheckCrash needed?

//
// A_DemonAttack1
//
void A_DemonAttack1(mobj_t *actor)
{
   if(!actor->target)
      return;

   if(P_CheckMeleeRange(actor))
   {
      P_DamageMobj(actor->target, actor, actor, 
                   ((P_Random(pr_chaosbite) & 7) + 1) * 2, MOD_HIT);
   }
}

//
// A_DemonAttack2
//
void A_DemonAttack2(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_DemonDeath
//
void A_DemonDeath(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_Demon2Death
//
void A_Demon2Death(mobj_t *actor)
{
}

static boolean P_SinkMobj(mobj_t *actor)
{
   // HEXEN_TODO
   return true;
}

static boolean P_RaiseMobj(mobj_t *actor)
{
   // HEXEN_TODO
   return true;
}

//
// A_WraithInit
//
void A_WraithInit(mobj_t *actor)
{
   actor->z += 48 * FRACUNIT;
   actor->counters[0] = 0;         // index into floatbob
}

//
// A_WraithRaiseInit
//
void A_WraithRaiseInit(mobj_t *actor)
{
   actor->flags2 &= ~MF2_DONTDRAW;
   // HEXEN_TODO: ???
   // actor->flags2 &= ~MF2_NONSHOOTABLE;
   actor->flags |= (MF_SHOOTABLE | MF_SOLID);
   actor->floorclip = actor->info->height;
}

//
// A_WraithRaise
//
void A_WraithRaise(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_WraithMelee
//
void A_WraithMelee(mobj_t *actor)
{
   int amount;

   if(!actor->target)
      return;
   
   // Steal health from target and give to actor
   if(P_CheckMeleeRange(actor) && (P_Random(pr_wraithm) < 220))
   {
      amount = ((P_Random(pr_wraithd) & 7) + 1) * 2;
      
      P_DamageMobj(actor->target, actor, actor, amount, MOD_HIT);
      
      actor->health += amount;
   }
}

//
// A_WraithMissile
//
void A_WraithMissile(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_WraithFX2
//
// Spawns sparkle tail of missile
// Parameters:
// * args[0] : thingtype
//
void A_WraithFX2(mobj_t *actor)
{
   int thingType = E_SafeThingType(actor->state->args[0]);
   mobj_t *mo;
   angle_t angle;
   int i;
   
   for(i = 0; i < 2; ++i)
   {
      mo = P_SpawnMobj(actor->x, actor->y, actor->z, thingType);
      
      if(P_Random(pr_wraithfx2) < 128)
         angle = actor->angle + (P_Random(pr_wraithfx2) << 22);
      else
         angle = actor->angle - (P_Random(pr_wraithfx2) << 22);

      mo->momz = 0;
      mo->momx = FixedMul((P_Random(pr_wraithfx2) << 7) + FRACUNIT,
                          finecosine[angle >> ANGLETOFINESHIFT]);
      mo->momy = FixedMul((P_Random(pr_wraithfx2) << 7) + FRACUNIT, 
                          finesine[angle >> ANGLETOFINESHIFT]);
      P_SetTarget(&mo->target, actor);
      mo->floorclip = 10*FRACUNIT;
   }
}

//
// A_WraithFX3
//
// Spawn an FX3 around the actor during attacks
// Parameters:
// * args[0] : thing type
//
void A_WraithFX3(mobj_t *actor)
{
   int thingType = E_SafeThingType(actor->state->args[0]);
   mobj_t *mo;
   int numdropped = P_Random(pr_wraithfx3) % 15;
   int i;
   
   for(i = 0; i < numdropped; ++i)
   {
      mo = P_SpawnMobj(actor->x, actor->y, actor->z, thingType);
      
      mo->x += (P_Random(pr_wraithfx3) - 128) << 11;
      mo->y += (P_Random(pr_wraithfx3) - 128) << 11;
      mo->z += (P_Random(pr_wraithfx3) << 10);
      P_SetTarget(&mo->target, actor);
   }
}

enum
{
   WFX4_SPAWN_TYPE1 = 0x01,
   WFX4_SPAWN_TYPE2 = 0x02,
};

//
// A_WraithFX4
//
// Parameters:
// * args[0] : thing type 1
// * args[1] : thing type 2
//
void A_WraithFX4(mobj_t *actor)
{
   int thingType1 = E_SafeThingType(actor->state->args[0]);
   int thingType2 = E_SafeThingType(actor->state->args[1]);
   mobj_t *mo;
   int chance = P_Random(pr_wraithfx4a);
   int spawnflags;
   
   if(chance < 10)
      spawnflags = WFX4_SPAWN_TYPE1;
   else if(chance < 20)
      spawnflags = WFX4_SPAWN_TYPE2;
   else if(chance < 25)
      spawnflags = WFX4_SPAWN_TYPE1 | WFX4_SPAWN_TYPE2;
   else
      spawnflags = 0;
   
   if(spawnflags & WFX4_SPAWN_TYPE1)
   {
      mo = P_SpawnMobj(actor->x, actor->y, actor->z, thingType1);
      mo->x += (P_Random(pr_wraithfx4b) - 128) << 12;
      mo->y += (P_Random(pr_wraithfx4b) - 128) << 12;
      mo->z += (P_Random(pr_wraithfx4b) << 10);
      P_SetTarget(&mo->target, actor);
   }
   if(spawnflags & WFX4_SPAWN_TYPE2)
   {
      mo = P_SpawnMobj(actor->x, actor->y, actor->z, thingType2);
      mo->x += (P_Random(pr_wraithfx4c) - 128) << 11;
      mo->y += (P_Random(pr_wraithfx4c) - 128) << 11;
      mo->z += (P_Random(pr_wraithfx4c) << 10);
      P_SetTarget(&mo->target, actor);
   }
}

//
// A_WraithLook
//
void A_WraithLook(mobj_t *actor)
{
   // HEXEN_TODO: reimplement?
   // A_WraithFX4(actor); // too expensive
   A_Look(actor);
}

//
// A_WraithChase
//
void A_WraithChase(mobj_t *actor)
{
   int weaveindex = actor->counters[0];
   actor->z += FloatBobOffsets[weaveindex];
   actor->counters[0] = (weaveindex + 2) & 63;

   A_Chase(actor);
   A_WraithFX4(actor);
}

//
// A_EttinAttack
//
void A_EttinAttack(mobj_t *actor)
{
   if(P_CheckMeleeRange(actor))
   {
      P_DamageMobj(actor->target, actor, actor, 
                   ((P_Random(pr_ettin) & 7) + 1) * 2, MOD_HIT);
   }
}

//
// A_DropMace
//
// Parameters:
// * args[0] : thing type
// * args[1] : x/y/z momentum shift (z = x/y - 1)
// * args[2] : base z momentum
//
void A_DropMace(mobj_t *actor)
{
   int thingType    = E_SafeThingType(actor->state->args[0]);
   int momShift     = (int)(actor->state->args[1]);
   fixed_t baseMomZ = (int)(actor->state->args[2]) * FRACUNIT;
   mobj_t *mo;
   
   mo = P_SpawnMobj(actor->x, actor->y, actor->z + (actor->height >> 1), 
                    thingType);

   mo->momx = (P_Random(pr_dropmace) - 128) << momShift;
   mo->momy = (P_Random(pr_dropmace) - 128) << momShift;
   mo->momz = baseMomZ + (P_Random(pr_dropmace) << (momShift - 1));
   P_SetTarget(&mo->target, actor);
}

//
// A_AffritSpawnRock
//
// Parameters:
// args[0] - args[4]: thingtypes
//
void A_AffritSpawnRock(mobj_t *actor)
{
   mobj_t *mo;
   int x, y, z, i;
   int rtype;
   int thingTypes[5];

   for(i = 0; i < 5; ++i)
      thingTypes[i] = E_SafeThingType(actor->state->args[i]);
   
   rtype = thingTypes[P_Random(pr_affritrock) % 5];
   
   x = actor->x + ((P_Random(pr_affritrock) - 128) << 12);
   y = actor->y + ((P_Random(pr_affritrock) - 128) << 12);
   z = actor->z + (P_Random(pr_affritrock) << 11);
   
   mo = P_SpawnMobj(x, y, z, rtype);
   
   P_SetTarget(&mo->target, actor);

   mo->momx = (P_Random(pr_affritrock) - 128) << 10;
   mo->momy = (P_Random(pr_affritrock) - 128) << 10;
   mo->momz = (P_Random(pr_affritrock) << 10);
   
   mo->counters[0] = 2; // Number bounces
   
   // Initialize fire demon
   actor->counters[1]  = 0;
   actor->flags       &= ~MF_JUSTATTACKED;
}

//
// A_AffritRocks
//
void A_AffritRocks(mobj_t *actor)
{
   A_AffritSpawnRock(actor);
   A_AffritSpawnRock(actor);
   A_AffritSpawnRock(actor);
   A_AffritSpawnRock(actor);
   A_AffritSpawnRock(actor);
}

//
// A_SmBounce
//
void A_SmBounce(mobj_t *actor)
{
   // give some more momentum (x,y,&z)
   actor->z    = actor->floorz + FRACUNIT;
   actor->momz = 2*FRACUNIT + (P_Random(pr_smbounce) << 10);
   actor->momx = P_Random(pr_smbounce) % 3 << FRACBITS;
   actor->momy = P_Random(pr_smbounce) % 3 << FRACBITS;
}

#define FIREDEMON_ATTACK_RANGE	64*8*FRACUNIT

void A_AffritChase(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_AffritSplotch
//
// Parameters:
// * args[0] : thingtype 1
// * args[1] : thingtype 2
//
void A_AffritSplotch(mobj_t *actor)
{
   int thingType1 = E_SafeThingType(actor->state->args[0]);
   int thingType2 = E_SafeThingType(actor->state->args[1]);
   mobj_t *mo;
   
   mo = P_SpawnMobj(actor->x, actor->y, actor->z, thingType1);
   
   mo->momx = (P_Random(pr_affrits) - 128) << 11;
   mo->momy = (P_Random(pr_affrits) - 128) << 11;
   mo->momz = 3*FRACUNIT + (P_Random(pr_affrits) << 10);

   mo = P_SpawnMobj(actor->x, actor->y, actor->z, thingType2);
   
   mo->momx = (P_Random(pr_affrits) - 128) << 11;
   mo->momy = (P_Random(pr_affrits) - 128) << 11;
   mo->momz = 3*FRACUNIT + (P_Random(pr_affrits)<<10);
}

//
// A_IceGuyLook
//
// Parameters:
// * args[0] : thingtype 1
// * args[1] : thingtype 2
//
void A_IceGuyLook(mobj_t *actor)
{
   int thingType1 = E_SafeThingType(actor->state->args[0]);
   int thingType2 = E_SafeThingType(actor->state->args[1]);
   fixed_t dist;
   fixed_t an;
   
   A_Look(actor);

   if(P_Random(pr_icelook) < 64)
   {
      dist = ((P_Random(pr_icelook2) - 128) * actor->radius) >> 7;
      an   = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
      
      P_SpawnMobj(actor->x + FixedMul(dist, finecosine[an]),
                  actor->y + FixedMul(dist, finesine[an]), 
                  actor->z + 60*FRACUNIT,
                  (P_Random(pr_icelook2) & 1) ? thingType2 : thingType1);
   }
}

//
// A_IceGuyChase
//
// Parameters:
// * args[0] : thingtype 1
// * args[1] : thingtype 2
//
void A_IceGuyChase(mobj_t *actor)
{
   int thingType1 = E_SafeThingType(actor->state->args[0]);
   int thingType2 = E_SafeThingType(actor->state->args[1]);
   fixed_t dist;
   fixed_t an;
   mobj_t *mo;
   
   A_Chase(actor);

   if(P_Random(pr_icechase) < 128)
   {
      dist = ((P_Random(pr_icechase2) - 128) * actor->radius) >> 7;
      an   = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
      
      mo = P_SpawnMobj(actor->x + FixedMul(dist, finecosine[an]),
                       actor->y + FixedMul(dist, finesine[an]), 
                       actor->z + 60*FRACUNIT,
                       (P_Random(pr_icechase2) & 1) ? thingType2 : thingType1);
      mo->momx = actor->momx;
      mo->momy = actor->momy;
      mo->momz = actor->momz;
      P_SetTarget(&mo->target, actor);
   }
}

//
// A_IceGuyAttack
//
void A_IceGuyAttack(mobj_t *actor)
{
   // HEXEN_TODO
}

void A_IceGuyDie(mobj_t *actor)
{
   // HEXEN_TODO
   //void A_FreezeDeathChunks(mobj_t *actor);
   
   actor->momx = 0;
   actor->momy = 0;
   actor->momz = 0;
   actor->height <<= 2;
   
   // HEXEN_TODO
   //A_FreezeDeathChunks(actor);
}

void A_IceGuyMissileExplode(mobj_t *actor)
{
   // HEXEN_TODO
}

// HEXEN_TODO: HERESIARCH >:O

// HEXEN_TODO: Class Bosses

//
// A_CheckFloor
//
void A_CheckFloor(mobj_t *actor)
{
   if(actor->z <= actor->floorz)
   {
      actor->z = actor->floorz;
      actor->flags2 &= ~MF2_LOGRAV;
      P_SetMobjState(actor, actor->info->deathstate);
   }
}

//
// A_FreezeDeath
//
void A_FreezeDeath(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_IceSetTics
//
void A_IceSetTics(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_IceCheckHeadDone
//
void A_IceCheckHeadDone(mobj_t *actor)
{
   // HEXEN_TODO
}

//
// A_FreezeDeathChunks
//
void A_FreezeDeathChunks(mobj_t *actor)
{
   // HEXEN_TODO
}

// HEXEN_TODO: KORAX >:O >:O >:O

// EOF

