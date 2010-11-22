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
//      DOOM action functions.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomstat.h"
#include "d_gi.h"
#include "d_mod.h"
#include "g_game.h"
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

#include "a_common.h"

//
// A_PosAttack
//
// Zombieman attack.
//
void A_PosAttack(mobj_t *actor)
{
   int angle, damage, slope;
   
   if(!actor->target)
      return;

   A_FaceTarget(actor);
   angle = actor->angle;
   slope = P_AimLineAttack(actor, angle, MISSILERANGE, 0); // killough 8/2/98
   S_StartSound(actor, sfx_pistol);
   
   // haleyjd 08/05/04: use new function
   angle += P_SubRandom(pr_posattack) << 20;

   damage = (P_Random(pr_posattack) % 5 + 1) * 3;
   P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

//
// A_SPosAttack
//
// Sergeant attack.
//
void A_SPosAttack(mobj_t* actor)
{
   int i, bangle, slope;
   
   if (!actor->target)
      return;
   
   S_StartSound(actor, sfx_shotgn);
   A_FaceTarget(actor);
   
   bangle = actor->angle;
   slope = P_AimLineAttack(actor, bangle, MISSILERANGE, 0); // killough 8/2/98
   
   for(i = 0; i < 3; ++i)
   {  
      // haleyjd 08/05/04: use new function
      int angle = bangle + (P_SubRandom(pr_sposattack) << 20);
      int damage = ((P_Random(pr_sposattack) % 5) + 1) * 3;
      P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
   }
}

//=============================================================================
//
// Chaingunner
//

//
// A_CPosAttack
//
// Heavy weapons dude attack.
//
void A_CPosAttack(mobj_t *actor)
{
   int angle, bangle, damage, slope;
   
   if (!actor->target)
      return;

   // haleyjd: restored to normal
   S_StartSound(actor, sfx_shotgn);
   A_FaceTarget(actor);
   
   bangle = actor->angle;
   slope = P_AimLineAttack(actor, bangle, MISSILERANGE, 0); // killough 8/2/98
   
   // haleyjd 08/05/04: use new function
   angle = bangle + (P_SubRandom(pr_cposattack) << 20);
   damage = ((P_Random(pr_cposattack) % 5) + 1) * 3;
   P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

//
// A_CPosRefire
//
// Line-of-sight checking for Chaingunner.
//
void A_CPosRefire(mobj_t *actor)
{
   // keep firing unless target got out of sight
   A_FaceTarget(actor);
   
   // killough 12/98: Stop firing if a friend has gotten in the way
   if(actor->flags & MF_FRIEND && P_HitFriend(actor))
   {
      P_SetMobjState(actor, actor->info->seestate);
      return;
   }
   
   // killough 11/98: prevent refiring on friends continuously
   if(P_Random(pr_cposrefire) < 40)
   {
      if(actor->target && (actor->flags & actor->target->flags & MF_FRIEND))
         P_SetMobjState(actor, actor->info->seestate);
      return;
   }

   if(!actor->target || actor->target->health <= 0 ||
      !P_CheckSight(actor, actor->target))
   {
      P_SetMobjState(actor, actor->info->seestate);
   }
}

//=============================================================================
//
// Basic Demons
//

//
// A_TroopAttack
//
// Imp attack.
//
void A_TroopAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   if(P_CheckMeleeRange(actor))
   {
      int damage;
      S_StartSound(actor, sfx_claw);
      damage = (P_Random(pr_troopattack)%8+1)*3;
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
      return;
   }
   // launch a missile
   P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_TROOPSHOT),
                  actor->z + DEFAULTMISSILEZ);
}

//
// A_SargAttack
//
// Demon attack.
//
void A_SargAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   if(P_CheckMeleeRange(actor))
   {
      int damage = ((P_Random(pr_sargattack)%10)+1)*4;
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
   }
}

//
// A_HeadAttack
//
// Cacodemon attack.
//
void A_HeadAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget (actor);
   if(P_CheckMeleeRange(actor))
   {
      int damage = (P_Random(pr_headattack)%6+1)*10;
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
      return;
   }
   // launch a missile
   P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_HEADSHOT),
                  actor->z + DEFAULTMISSILEZ);
}

//
// A_BruisAttack
//
// Baron of Hell attack.
//
void A_BruisAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   if(P_CheckMeleeRange(actor))
   {
      int damage;
      S_StartSound(actor, sfx_claw);
      damage = (P_Random(pr_bruisattack)%8+1)*10;
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
      return;
   }
   P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_BRUISERSHOT),
                  actor->z + DEFAULTMISSILEZ);  // launch a missile
}

//=============================================================================
//
// Spider Actions
//

//
// A_SpidRefire
//
// Spider Mastermind line-of-sight checking.
//
void A_SpidRefire(mobj_t* actor)
{
   // keep firing unless target got out of sight
   A_FaceTarget(actor);
   
   // killough 12/98: Stop firing if a friend has gotten in the way
   if(actor->flags & MF_FRIEND && P_HitFriend(actor))
   {
      P_SetMobjState(actor, actor->info->seestate);
      return;
   }
   
   if(P_Random(pr_spidrefire) < 10)
      return;

   // killough 11/98: prevent refiring on friends continuously
   if(!actor->target || actor->target->health <= 0    ||
      actor->flags & actor->target->flags & MF_FRIEND ||
      !P_CheckSight(actor, actor->target))
   {
      P_SetMobjState(actor, actor->info->seestate);
   }
}

//
// A_BspiAttack
//
// Arachnotron attack.
//
void A_BspiAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   // launch a missile
   P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_ARACHPLAZ), 
                  actor->z + DEFAULTMISSILEZ);
}

//
// A_BabyMetal
//
// Arachnotron chase w/sound.
//
void A_BabyMetal(mobj_t *mo)
{
   S_StartSound(mo, sfx_bspwlk);
   A_Chase(mo);
}

//=============================================================================
//
// Cyberdemon
//

//
// A_Hoof
//
// Cyberdemon chase pointer 1 with hoof sound.
//
void A_Hoof(mobj_t* mo)
{
   S_StartSound(mo, sfx_hoof);
   A_Chase(mo);
}

//
// A_Metal
//
// Cyberdemon chase pointer 2 with metal sound.
// Also used by Spider Mastermind.
//
void A_Metal(mobj_t *mo)
{
   S_StartSound(mo, sfx_metal);
   A_Chase(mo);
}

//
// A_CyberAttack
//
// Cyberdemon rocket attack.
//
void A_CyberAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   P_SpawnMissile(actor, actor->target, 
                  E_SafeThingType(MT_ROCKET),
                  actor->z + DEFAULTMISSILEZ);   
}

//=============================================================================
//
// Revenant
//

//
// A_SkelMissile
//
// Fire seeker missile.
//
void A_SkelMissile(mobj_t *actor)
{
   mobj_t *mo;
   
   if(!actor->target)
      return;
   
   A_FaceTarget (actor);
   actor->z += 16*FRACUNIT;      // so missile spawns higher
   mo = P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_TRACER),
                       actor->z + DEFAULTMISSILEZ);
   actor->z -= 16*FRACUNIT;      // back to normal
   
   mo->x += mo->momx;
   mo->y += mo->momy;
   P_SetTarget<mobj_t>(&mo->tracer, actor->target);  // killough 11/98
}

#define TRACEANGLE 0xc000000   /* killough 9/9/98: change to #define */

//
// A_Tracer
//
// (Accidentally?) randomized homing missile maintenance.
//
void A_Tracer(mobj_t *actor)
{
   angle_t       exact;
   fixed_t       dist;
   fixed_t       slope;
   mobj_t        *dest;
   mobj_t        *th;

   // killough 1/18/98: this is why some missiles do not have smoke
   // and some do. Also, internal demos start at random gametics, 
   // thus the bug in which revenants cause internal demos to go out 
   // of sync.
   //
   // killough 3/6/98: fix revenant internal demo bug by subtracting
   // levelstarttic from gametic.
   //
   // killough 9/29/98: use new "basetic" so that demos stay in sync
   // during pauses and menu activations, while retaining old demo 
   // sync.
   //
   // leveltime would have been better to use to start with in Doom, 
   // but since old demos were recorded using gametic, we must stick 
   // with it, and improvise around it (using leveltime causes desync 
   // across levels).

   if((gametic-basetic) & 3)
      return;

   // spawn a puff of smoke behind the rocket
   P_SpawnPuff(actor->x, actor->y, actor->z, 0, 3, false);
   
   th = P_SpawnMobj(actor->x - actor->momx,
                    actor->y - actor->momy,
                    actor->z, E_SafeThingType(MT_SMOKE));
  
   th->momz = FRACUNIT;
   th->tics -= P_Random(pr_tracer) & 3;
   if(th->tics < 1)
      th->tics = 1;
  
   // adjust direction
   dest = actor->tracer;
   
   if(!dest || dest->health <= 0)
      return;

   // change angle
   exact = P_PointToAngle(actor->x, actor->y, dest->x, dest->y);

   if(exact != actor->angle)
   {
      if(exact - actor->angle > 0x80000000)
      {
         actor->angle -= TRACEANGLE;
         if(exact - actor->angle < 0x80000000)
            actor->angle = exact;
      }
      else
      {
         actor->angle += TRACEANGLE;
         if(exact - actor->angle > 0x80000000)
            actor->angle = exact;
      }
   }

   exact = actor->angle>>ANGLETOFINESHIFT;
   actor->momx = FixedMul(actor->info->speed, finecosine[exact]);
   actor->momy = FixedMul(actor->info->speed, finesine[exact]);

   // change slope
   dist = P_AproxDistance(dest->x - actor->x, dest->y - actor->y);
   
   dist = dist / actor->info->speed;

   if(dist < 1)
      dist = 1;

   slope = (dest->z + 40*FRACUNIT - actor->z) / dist;
   
   if(slope < actor->momz)
      actor->momz -= FRACUNIT/8;
   else
      actor->momz += FRACUNIT/8;
}

//
// A_SkelWhoosh
//
// Fist swing sound for Revenant.
//
void A_SkelWhoosh(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   S_StartSound(actor, sfx_skeswg);
}

//
// A_SkelFist
//
// Revenant punch attack.
//
void A_SkelFist(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   if(P_CheckMeleeRange(actor))
   {
      int damage = ((P_Random(pr_skelfist) % 10) + 1) * 6;
      S_StartSound(actor, sfx_skepch);
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
   }
}

//=============================================================================
//
// Arch-Vile
//

static mobj_t* corpsehit;
static mobj_t* vileobj;
static fixed_t viletryx;
static fixed_t viletryy;

//
// PIT_VileCheck
//
// Detect a corpse that could be raised.
//
boolean PIT_VileCheck(mobj_t *thing)
{
   int maxdist;
   boolean check;
   static int vileType = -1;
   
   if(vileType == -1)
      vileType = E_SafeThingType(MT_VILE);
   
   if(!(thing->flags & MF_CORPSE))
      return true;        // not a monster
   
   if(thing->tics != -1)
      return true;        // not lying still yet
   
   if(thing->info->raisestate == NullStateNum)
      return true;        // monster doesn't have a raise state
   
   maxdist = thing->info->radius + mobjinfo[vileType].radius;
   
   if(D_abs(thing->x-viletryx) > maxdist ||
      D_abs(thing->y-viletryy) > maxdist)
      return true;                // not actually touching

   // Check to see if the radius and height are zero. If they are      // phares
   // then this is a crushed monster that has been turned into a       //   |
   // gib. One of the options may be to ignore this guy.               //   V

   // Option 1: the original, buggy method, -> ghost (compatibility)
   // Option 2: resurrect the monster, but not as a ghost
   // Option 3: ignore the gib

   //    if (Option3)                                                  //   ^
   //        if ((thing->height == 0) && (thing->radius == 0))         //   |
   //            return true;                                          // phares

   corpsehit = thing;
   corpsehit->momx = corpsehit->momy = 0;
   if(comp[comp_vile])
   {                                                              // phares
      corpsehit->height <<= 2;                                    //   V
      
      // haleyjd 11/11/04: this is also broken by Lee's change to
      // PIT_CheckThing when not in demo_compatibility.
      if(demo_version >= 331)
         corpsehit->flags |= MF_SOLID;

      check = P_CheckPosition(corpsehit, corpsehit->x, corpsehit->y);

      if(demo_version >= 331)
         corpsehit->flags &= ~MF_SOLID;
      
      corpsehit->height >>= 2;
   }
   else
   {
      int height,radius;
      
      height = corpsehit->height; // save temporarily
      radius = corpsehit->radius; // save temporarily
      corpsehit->height = P_ThingInfoHeight(corpsehit->info);
      corpsehit->radius = corpsehit->info->radius;
      corpsehit->flags |= MF_SOLID;
      check = P_CheckPosition(corpsehit,corpsehit->x,corpsehit->y);
      corpsehit->height = height; // restore
      corpsehit->radius = radius; // restore                      //   ^
      corpsehit->flags &= ~MF_SOLID;
   }                                                              //   |
                                                                  // phares
   if(!check)
      return true;              // doesn't fit here
   return false;               // got one, so stop checking
}

//
// A_VileChase
//
// Check for ressurecting a body
//
void A_VileChase(mobj_t *actor)
{
   int xl, xh;
   int yl, yh;
   int bx, by;

   if(actor->movedir != DI_NODIR)
   {
      // check for corpses to raise
      viletryx =
         actor->x + actor->info->speed*xspeed[actor->movedir];
      viletryy =
         actor->y + actor->info->speed*yspeed[actor->movedir];
      
      xl = (viletryx - bmaporgx - MAXRADIUS*2)>>MAPBLOCKSHIFT;
      xh = (viletryx - bmaporgx + MAXRADIUS*2)>>MAPBLOCKSHIFT;
      yl = (viletryy - bmaporgy - MAXRADIUS*2)>>MAPBLOCKSHIFT;
      yh = (viletryy - bmaporgy + MAXRADIUS*2)>>MAPBLOCKSHIFT;

      vileobj = actor;
      for(bx = xl; bx <= xh; ++bx)
      {
         for(by = yl; by <= yh; ++by)
         {
            // Call PIT_VileCheck to check
            // whether object is a corpse
            // that can be raised.
            if(!P_BlockThingsIterator(bx, by, PIT_VileCheck))
            {
               mobjinfo_t *info;
               
               // got one!
               mobj_t *temp = actor->target;
               actor->target = corpsehit;
               A_FaceTarget(actor);
               actor->target = temp;

               P_SetMobjState(actor, E_SafeState(S_VILE_HEAL1));
               S_StartSound(corpsehit, sfx_slop);
               info = corpsehit->info;

               // haleyjd 09/26/04: need to restore monster skins here
               // in case they were cleared by the thing being crushed
               if(info->altsprite != -1)
                  corpsehit->skin = P_GetMonsterSkin(info->altsprite);
               
               P_SetMobjState(corpsehit, info->raisestate);
               
               if(comp[comp_vile])
                  corpsehit->height <<= 2;                        // phares
               else                                               //   V
               {
                  // fix Ghost bug
                  corpsehit->height = P_ThingInfoHeight(info);
                  corpsehit->radius = info->radius;
               }                                                  // phares
               
               // killough 7/18/98: 
               // friendliness is transferred from AV to raised corpse
               corpsehit->flags = 
                  (info->flags & ~MF_FRIEND) | (actor->flags & MF_FRIEND);
               
               corpsehit->health = info->spawnhealth;
               P_SetTarget<mobj_t>(&corpsehit->target, NULL);  // killough 11/98

               if(demo_version >= 203)
               {         // kilough 9/9/98
                  P_SetTarget<mobj_t>(&corpsehit->lastenemy, NULL);
                  corpsehit->flags &= ~MF_JUSTHIT;
               }
               
               // killough 8/29/98: add to appropriate thread
               corpsehit->Update();
               
               return;
            }
         }
      }
   }
   A_Chase(actor);  // Return to normal attack.
}

//
// A_VileStart
//
// Play vilatk sound.
//
void A_VileStart(mobj_t *actor)
{
   S_StartSound(actor, sfx_vilatk);
}

//
// A_Fire
//
// Keep fire in front of player unless out of sight
//
void A_Fire(mobj_t *actor)
{
   angle_t an;
   mobj_t *dest = actor->tracer;
   
   if(!dest)
      return;
   
   // don't move it if the vile lost sight
   if(!P_CheckSight(actor->target, dest) )
      return;
   
   an = dest->angle >> ANGLETOFINESHIFT;
   
   P_UnsetThingPosition(actor);
   actor->x = dest->x + FixedMul(24*FRACUNIT, finecosine[an]);
   actor->y = dest->y + FixedMul(24*FRACUNIT, finesine[an]);
   actor->z = dest->z;
   P_SetThingPosition(actor);
}

//
// A_StartFire
//
// Start vile fire effect. This never does anything in vanilla DOOM however,
// because it is only in the fire's spawn state, and the pointer in any
// object's spawnstate is never executed the first time it spawns.
//
void A_StartFire(mobj_t *actor)
{
   S_StartSound(actor,sfx_flamst);
   A_Fire(actor);
}

//
// A_FireCrackle
//
// Play fire crackling sound.
//
void A_FireCrackle(mobj_t* actor)
{
   S_StartSound(actor,sfx_flame);
   A_Fire(actor);
}


//
// A_VileTarget
//
// Spawn the hellfire
//
void A_VileTarget(mobj_t *actor)
{
   mobj_t *fog;
   
   if(!actor->target)
      return;

   A_FaceTarget(actor);
   
   // killough 12/98: fix Vile fog coordinates
   fog = P_SpawnMobj(actor->target->x,
                     demo_version < 203 ? actor->target->x : actor->target->y,
                     actor->target->z,E_SafeThingType(MT_FIRE));
   
   P_SetTarget<mobj_t>(&actor->tracer, fog);   // killough 11/98
   P_SetTarget<mobj_t>(&fog->target, actor);
   P_SetTarget<mobj_t>(&fog->tracer, actor->target);
   A_Fire(fog);
}

//
// A_VileAttack
//
// Arch-vile attack.
//
void A_VileAttack(mobj_t *actor)
{
   mobj_t *fire;
   int    an;
   
   if(!actor->target)
      return;

   A_FaceTarget(actor);
   
   if(!P_CheckSight(actor, actor->target))
      return;

   S_StartSound(actor, sfx_barexp);
   P_DamageMobj(actor->target, actor, actor, 20, actor->info->mod);
   actor->target->momz = 1000*FRACUNIT/actor->target->info->mass;

   an = actor->angle >> ANGLETOFINESHIFT;
   
   fire = actor->tracer;
   
   if(!fire)
      return;

   // move the fire between the vile and the player
   fire->x = actor->target->x - FixedMul (24*FRACUNIT, finecosine[an]);
   fire->y = actor->target->y - FixedMul (24*FRACUNIT, finesine[an]);
   P_RadiusAttack(fire, actor, 70, actor->info->mod);
}

//=============================================================================
// 
// Mancubus
//

// Mancubus attack,
// firing three missiles (bruisers)
// in three different directions?
// Doesn't look like it.            - haleyjd: weird comment #345932

#define FATSPREAD       (ANG90/8)

//
// A_FatRaise
//
// Prepare to attack.
//
void A_FatRaise(mobj_t *actor)
{
   A_FaceTarget(actor);
   S_StartSound(actor, sfx_manatk);
}

static int FatShotType = -1;

//
// A_FatAttack1
//
// Mancubus attack 1.
//
void A_FatAttack1(mobj_t *actor)
{
   mobj_t *mo;
   int    an;
   fixed_t z = actor->z + DEFAULTMISSILEZ;
   
   // haleyjd: no crashing
   if(!actor->target)
      return;

   if(FatShotType == -1)
      FatShotType = E_SafeThingType(MT_FATSHOT);
   
   A_FaceTarget(actor);

   // Change direction  to ...
   actor->angle += FATSPREAD;
   
   P_SpawnMissile(actor, actor->target, FatShotType, z);
   
   mo = P_SpawnMissile(actor, actor->target, FatShotType, z);
   mo->angle += FATSPREAD;
   an = mo->angle >> ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_FatAttack2
//
// Mancubus attack 2.
//
void A_FatAttack2(mobj_t *actor)
{
   mobj_t *mo;
   int    an;
   fixed_t z = actor->z + DEFAULTMISSILEZ;

   // haleyjd: no crashing
   if(!actor->target)
      return;
   
   if(FatShotType == -1)
      FatShotType = E_SafeThingType(MT_FATSHOT);

   A_FaceTarget(actor);
   // Now here choose opposite deviation.
   actor->angle -= FATSPREAD;
   P_SpawnMissile(actor, actor->target, FatShotType, z);
   
   mo = P_SpawnMissile(actor, actor->target, FatShotType, z);
   mo->angle -= FATSPREAD*2;
   an = mo->angle >> ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_FatAttack3
//
// Mancubus attack 3.
//
void A_FatAttack3(mobj_t *actor)
{
   mobj_t *mo;
   int    an;
   fixed_t z = actor->z + DEFAULTMISSILEZ;

   // haleyjd: no crashing
   if(!actor->target)
      return;
   
   if(FatShotType == -1)
      FatShotType = E_SafeThingType(MT_FATSHOT);
   
   A_FaceTarget(actor);
   
   mo = P_SpawnMissile(actor, actor->target, FatShotType, z);
   mo->angle -= FATSPREAD/2;
   an = mo->angle >> ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy = FixedMul(mo->info->speed, finesine[an]);
   
   mo = P_SpawnMissile(actor, actor->target, FatShotType, z);
   mo->angle += FATSPREAD/2;
   an = mo->angle >> ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//=============================================================================
//
// Lost Soul and Pain Elemental
//

#define SKULLSPEED              (20*FRACUNIT)

//
// A_SkullAttack
//
// Fly at the player like a missile.
//
void A_SkullAttack(mobj_t *actor)
{   
   if(!actor->target)
      return;
   
   S_StartSound(actor, actor->info->attacksound);

   // haleyjd 08/07/04: use new P_SkullFly function
   P_SkullFly(actor, SKULLSPEED);
}

// sf: removed beta lost soul
// haleyjd: add back for mbf dehacked patch compatibility
//          might be a useful function to someone, anyway

//
// A_BetaSkullAttack()
//
// killough 10/98: this emulates the beta version's lost soul attacks
//
void A_BetaSkullAttack(mobj_t *actor)
{
   int damage;
   
   // haleyjd: changed to check if objects are the SAME type, not
   // for hard-coded lost soul
   if(!actor->target || actor->target->type == actor->type)
      return;
   
   S_StartSound(actor, actor->info->attacksound);
   
   A_FaceTarget(actor);
   damage = (P_Random(pr_skullfly)%8+1)*actor->damage;
   P_DamageMobj(actor->target, actor, actor, damage, actor->info->mod);
}

//
// A_Stop
//
// Remove all momentum from an object.
// POINTER-TODO: Move to a_common?
//
void A_Stop(mobj_t *actor)
{
   actor->momx = actor->momy = actor->momz = 0;
}

//
// A_PainShootSkull
//
// Spawn a lost soul and launch it at the target
//
void A_PainShootSkull(mobj_t *actor, angle_t angle)
{
   fixed_t       x,y,z;
   mobj_t        *newmobj;
   angle_t       an;
   int           prestep;
   static int    skullType = -1;
      
   if(skullType == -1)
      skullType = E_SafeThingType(MT_SKULL);

   // The original code checked for 20 skulls on the level,    // phares
   // and wouldn't spit another one if there were. If not in   // phares
   // compatibility mode, we remove the limit.                 // phares

   if(comp[comp_pain])  // killough 10/98: compatibility-optioned
   {
      // count total number of skulls currently on the level
      int count = 20;
      CThinker *th;
      for(th = thinkercap.next; th != &thinkercap; th = th->next)
      {
         mobj_t *mo;
         if((mo = dynamic_cast<mobj_t *>(th)) && mo->type == skullType)
         {
            if(--count < 0)         // killough 8/29/98: early exit
               return;
         }
      }
   }

   // okay, there's room for another one
   
   an = angle >> ANGLETOFINESHIFT;
   
   prestep = 4*FRACUNIT + 3*(actor->info->radius + mobjinfo[skullType].radius)/2;

   x = actor->x + FixedMul(prestep, finecosine[an]);
   y = actor->y + FixedMul(prestep, finesine[an]);
   z = actor->z + 8*FRACUNIT;
   
   if(comp[comp_skull])   // killough 10/98: compatibility-optioned
      newmobj = P_SpawnMobj(x, y, z, skullType);                    // phares
   else                                                             //   V
   {
      // Check whether the Lost Soul is being fired through a 1-sided
      // wall or an impassible line, or a "monsters can't cross" line.
      // If it is, then we don't allow the spawn. This is a bug fix, 
      // but it should be considered an enhancement, since it may 
      // disturb existing demos, so don't do it in compatibility mode.
      
      if (Check_Sides(actor,x,y))
         return;
      
      newmobj = P_SpawnMobj(x, y, z, skullType);
      
      // Check to see if the new Lost Soul's z value is above the
      // ceiling of its new sector, or below the floor. If so, kill it.
      
      if((newmobj->z >
         (newmobj->subsector->sector->ceilingheight - newmobj->height)) ||
         (newmobj->z < newmobj->subsector->sector->floorheight))
      {
         // kill it immediately
         P_DamageMobj(newmobj,actor,actor,10000,MOD_UNKNOWN);
         return;                                                    //   ^
      }                                                             //   |
   }                                                                // phares

   // killough 7/20/98: PEs shoot lost souls with the same friendliness
   newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (actor->flags & MF_FRIEND);

   // killough 8/29/98: add to appropriate thread
   newmobj->Update();

   // Check for movements.
   // killough 3/15/98: don't jump over dropoffs:

   if(!P_TryMove(newmobj, newmobj->x, newmobj->y, false))
   {
      // kill it immediately
      P_DamageMobj(newmobj, actor, actor, 10000, MOD_UNKNOWN);
      return;
   }
   
   P_SetTarget<mobj_t>(&newmobj->target, actor->target);
   A_SkullAttack(newmobj);
}

//
// A_PainAttack
//
// Spawn a lost soul and launch it at the target
//
void A_PainAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   A_PainShootSkull(actor, actor->angle);
}

//
// A_PainDie
//
// Normal fall logic plus 3 Lost Souls spawn.
//
void A_PainDie(mobj_t *actor)
{
   A_Fall(actor);
   A_PainShootSkull(actor, actor->angle + ANG90);
   A_PainShootSkull(actor, actor->angle + ANG180);
   A_PainShootSkull(actor, actor->angle + ANG270);
}

//=============================================================================
//
// Special Death Effects
//

typedef struct boss_spec_s
{
   unsigned int thing_flag;
   unsigned int level_flag;
} boss_spec_t;

#define NUM_BOSS_SPECS 7

static boss_spec_t boss_specs[NUM_BOSS_SPECS] =
{
   { MF2_MAP07BOSS1, BSPEC_MAP07_1 },
   { MF2_MAP07BOSS2, BSPEC_MAP07_2 },
   { MF2_E1M8BOSS,   BSPEC_E1M8 },
   { MF2_E2M8BOSS,   BSPEC_E2M8 },
   { MF2_E3M8BOSS,   BSPEC_E3M8 },
   { MF2_E4M6BOSS,   BSPEC_E4M6 },
   { MF2_E4M8BOSS,   BSPEC_E4M8 },
};

//
// A_BossDeath
//
// Possibly trigger special effects if on boss level
//
// haleyjd: enhanced to allow user specification of the thing types
//          allowed to trigger each special effect.
// haleyjd: 03/14/05 -- enhanced to allow actions on any map.
//
void A_BossDeath(mobj_t *mo)
{
   CThinker *th;
   line_t    junk;
   int       i;

   // make sure there is a player alive for victory
   for(i = 0; i < MAXPLAYERS; ++i)
   {
      if(playeringame[i] && players[i].health > 0)
         break;
   }
   
   // no one left alive, so do not end game
   if(i == MAXPLAYERS)
      return;

   for(i = 0; i < NUM_BOSS_SPECS; ++i)
   {
      // to activate a special, the thing must be a boss that triggers
      // it, and the map must have the special enabled.
      if((mo->flags2 & boss_specs[i].thing_flag) &&
         (LevelInfo.bossSpecs & boss_specs[i].level_flag))
      {
         // scan the remaining thinkers to see if all bosses are dead
         for(th = thinkercap.next; th != &thinkercap; th = th->next)
         {
            mobj_t *mo2;
            if((mo2 = dynamic_cast<mobj_t *>(th)))
            {
               if(mo2 != mo && 
                  (mo2->flags2 & boss_specs[i].thing_flag) && 
                  mo2->health > 0)
                  return;         // other boss not dead
            }
         }

         // victory!
         switch(boss_specs[i].level_flag)
         {
         case BSPEC_E1M8:
         case BSPEC_E4M8:
         case BSPEC_MAP07_1:
            // lower floors tagged 666 to lowest neighbor
            junk.tag = 666;
            EV_DoFloor(&junk, lowerFloorToLowest);
            break;
         case BSPEC_MAP07_2:
            // raise floors tagged 667 by shortest lower texture
            junk.tag = 667;
            EV_DoFloor(&junk, raiseToTexture);
            break;
         case BSPEC_E2M8:
         case BSPEC_E3M8:
            // exit map -- no use processing any further after this
            G_ExitLevel();
            return;
         case BSPEC_E4M6:
            // open sectors tagged 666 as blazing doors
            junk.tag = 666;
            EV_DoDoor(&junk, blazeOpen);
            break;
         default:
            break;
         } // end switch
      } // end if
   } // end for
}

//
// A_KeenDie
//
// DOOM II special, map 32.
// Uses special tag 666.
//
void A_KeenDie(mobj_t* mo)
{
   CThinker *th;
   line_t   junk;
   
   A_Fall(mo);

   // scan the remaining thinkers to see if all Keens are dead
   
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      mobj_t *mo2;
      if((mo2 = dynamic_cast<mobj_t *>(th)))
      {
         if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
            return;                           // other Keen not dead
      }
   }

   junk.tag = 666;
   EV_DoDoor(&junk,doorOpen);
}

//=============================================================================
//
// DOOM II Boss Brain
//
// "To win the game, you must kill me, John Romero"
//

// killough 2/7/98: Remove limit on icon landings:
// haleyjd 07/30/04: use new MobjCollection

static MobjCollection braintargets;

struct brain_s brain;   // killough 3/26/98: global state of boss brain

// killough 3/26/98: initialize icon landings at level startup,
// rather than at boss wakeup, to prevent savegame-related crashes

void P_SpawnBrainTargets(void)  // killough 3/26/98: renamed old function
{
   int BrainSpotType = E_ThingNumForDEHNum(MT_BOSSTARGET);

   // find all the target spots
   P_ReInitMobjCollection(&braintargets, BrainSpotType);

   brain.easy = 0;   // killough 3/26/98: always init easy to 0

   if(BrainSpotType == NUMMOBJTYPES)
      return;

   P_CollectThings(&braintargets);
}

// haleyjd 07/30/04: P_CollectThings moved to p_mobj.c

//
// A_BrainAwake
//
// Awaken the Boss Brain spawn shooter.
//
void A_BrainAwake(mobj_t *mo)
{
   S_StartSound(NULL,sfx_bossit); // killough 3/26/98: only generates sound now
}

//
// A_BrainPain
//
// Called when the Romero head is injured.
//
void A_BrainPain(mobj_t *mo)
{
   S_StartSound(NULL,sfx_bospn);
}

//
// A_BrainScream
//
// Romero head death effects.
//
void A_BrainScream(mobj_t *mo)
{
   int x;
   static int rocketType = -1;
   
   if(rocketType == -1)
      rocketType = E_SafeThingType(MT_ROCKET);

   for(x=mo->x - 196*FRACUNIT ; x< mo->x + 320*FRACUNIT ; x+= FRACUNIT*8)
   {
      int y = mo->y - 320*FRACUNIT;
      int z = 128 + P_Random(pr_brainscream)*2*FRACUNIT;
      mobj_t *th = P_SpawnMobj (x,y,z, rocketType);
      // haleyjd 02/21/05: disable particle events/effects for this thing
      th->intflags |= MIF_NOPTCLEVTS;
      th->effects = 0;
      th->momz = P_Random(pr_brainscream)*512;
      P_SetMobjState(th, E_SafeState(S_BRAINEXPLODE1));
      th->tics -= P_Random(pr_brainscream)&7;
      if(th->tics < 1)
         th->tics = 1;
   }
   S_StartSound(NULL,sfx_bosdth);
}

//
// A_BrainExplode
//
// More Romero head death effects.
//
void A_BrainExplode(mobj_t *mo)
{  
   // haleyjd 08/05/04: use new function
   int x = mo->x + P_SubRandom(pr_brainexp)*2048;
   int y = mo->y;
   int z = 128 + P_Random(pr_brainexp)*2*FRACUNIT;

   mobj_t *th = P_SpawnMobj(x, y, z, E_SafeThingType(MT_ROCKET));
   th->momz = P_Random(pr_brainexp)*512;
   // haleyjd 02/21/05: disable particle events/effects for this thing
   th->intflags |= MIF_NOPTCLEVTS;
   th->effects = 0;
   P_SetMobjState(th, E_SafeState(S_BRAINEXPLODE1));
   
   th->tics -= P_Random(pr_brainexp) & 7;
   if(th->tics < 1)
      th->tics = 1;
}

//
// A_BrainDie
//
// Romero head final death pointer.
//
void A_BrainDie(mobj_t *mo)
{
   G_ExitLevel();
}

//
// A_BrainSpit
//
// Spawn cube shooter attack.
//
void A_BrainSpit(mobj_t *mo)
{
   mobj_t *targ, *newmobj;
   static int SpawnShotType = -1;
   
   if(SpawnShotType == -1)
      SpawnShotType = E_SafeThingType(MT_SPAWNSHOT);
   
    // killough 4/1/98: ignore if no targets
   if(P_CollectionIsEmpty(&braintargets))
      return;

   brain.easy ^= 1;          // killough 3/26/98: use brain struct
   if(gameskill <= sk_easy && !brain.easy)
      return;

   // shoot a cube at current target
   targ = P_CollectionWrapIterator(&braintargets);

   // spawn brain missile
   newmobj = P_SpawnMissile(mo, targ, SpawnShotType, 
                            mo->z + DEFAULTMISSILEZ);
   P_SetTarget<mobj_t>(&newmobj->target, targ);
   newmobj->reactiontime = (int16_t)(((targ->y-mo->y)/newmobj->momy)/newmobj->state->tics);

   // killough 7/18/98: brain friendliness is transferred
   newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (mo->flags & MF_FRIEND);
   
   // killough 8/29/98: add to appropriate thread
   newmobj->Update();
   
   S_StartSound(NULL, sfx_bospit);
}

void A_SpawnFly(mobj_t *mo);

//
// A_SpawnSound
//
// travelling cube sound
//
void A_SpawnSound(mobj_t *mo)
{
   S_StartSound(mo,sfx_boscub);
   A_SpawnFly(mo);
}

// haleyjd 07/13/03: editable boss brain spawn types
// schepe: removed 11-type limit
int NumBossTypes;
int *BossSpawnTypes;
int *BossSpawnProbs;

//
// A_SpawnFly
//
// Called by spawn cubes in flight.
//
void A_SpawnFly(mobj_t *mo)
{
   int    i;         // schepe 
   mobj_t *newmobj;  // killough 8/9/98
   int    r;
   mobjtype_t type = 0;
   static int fireType = -1;
      
   mobj_t *fog;
   mobj_t *targ;

   // haleyjd 05/31/06: allow 0 boss types
   if(NumBossTypes == 0)
      return;

   if(fireType == -1)
      fireType = E_SafeThingType(MT_SPAWNFIRE);

   if(--mo->reactiontime)
      return;     // still flying

   // haleyjd: do not crash if target is null
   if(!(targ = mo->target))
      return;
   
   // First spawn teleport fog.
   fog = P_SpawnMobj(targ->x, targ->y, targ->z, fireType);
   
   S_StartSound(fog, sfx_telept);
   
   // Randomly select monster to spawn.
   r = P_Random(pr_spawnfly);

   // Probability distribution (kind of :), decreasing likelihood.
   // schepe:
   for(i = 0; i < NumBossTypes; ++i)
   {
      if(r < BossSpawnProbs[i])
      {
         type = BossSpawnTypes[i];
         break;
      }
   }

   newmobj = P_SpawnMobj(targ->x, targ->y, targ->z, type);
   
   // killough 7/18/98: brain friendliness is transferred
   newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (mo->flags & MF_FRIEND);

   // killough 8/29/98: add to appropriate thread
   newmobj->Update();
   
   if(P_LookForTargets(newmobj,true))      // killough 9/4/98
      P_SetMobjState(newmobj, newmobj->info->seestate);
   
   // telefrag anything in this spot
   P_TeleportMove(newmobj, newmobj->x, newmobj->y, true); // killough 8/9/98
   
   // remove self (i.e., cube).
   P_RemoveMobj(mo);
}

// EOF

