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
//      DOOM action functions.
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
#include "g_game.h"
#include "m_bbox.h"  // ioanch 20160108: portal aware block iterator
#include "p_enemy.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobjcol.h"
#include "p_mobj.h"
#include "p_portalcross.h"
#include "p_pspr.h"
#include "p_setup.h"
#include "p_skin.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_defs.h"
#include "r_main.h"  // ioanch 20160106: portal correct access
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "v_misc.h"

//
// A_PosAttack
//
// Zombieman attack.
//
void A_PosAttack(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   int angle, damage, slope;
   
   if(!actor->target)
      return;

   A_FaceTarget(actionargs);
   angle = actor->angle;
   slope = P_AimLineAttack(actor, angle, MISSILERANGE, false); // killough 8/2/98
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
void A_SPosAttack(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   int i, bangle, slope;
   
   if (!actor->target)
      return;
   
   S_StartSound(actor, sfx_shotgn);
   A_FaceTarget(actionargs);
   
   bangle = actor->angle;
   slope = P_AimLineAttack(actor, bangle, MISSILERANGE, false); // killough 8/2/98
   
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
void A_CPosAttack(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   int angle, bangle, damage, slope;
   
   if(!actor->target)
      return;

   // haleyjd: restored to normal
   S_StartSound(actor, sfx_shotgn);
   A_FaceTarget(actionargs);
   
   bangle = actor->angle;
   slope = P_AimLineAttack(actor, bangle, MISSILERANGE, false); // killough 8/2/98
   
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
void A_CPosRefire(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   // keep firing unless target got out of sight
   A_FaceTarget(actionargs);
   
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
void A_TroopAttack(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   if(!actor->target)
      return;

   A_FaceTarget(actionargs);
   
   if(P_CheckMeleeRange(actor))
   {
      int damage;
      S_StartSound(actor, sfx_claw);
      damage = (P_Random(pr_troopattack)%8+1)*3;
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
   }
   else
   {
      // launch a missile
      P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_TROOPSHOT),
                     actor->z + DEFAULTMISSILEZ);
   }
}

//
// A_SargAttack
//
// Demon attack.
//
void A_SargAttack(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   if(!actor->target)
      return;
   
   A_FaceTarget(actionargs);
   
   if(P_CheckMeleeRange(actor))
   {
      int damage = ((P_Random(pr_sargattack)%10)+1)*4;
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
   }
}

//
// A_SargAttack12
//
// DOOM 1.2 Demon attack
// haleyjd 07/29/14
//
void A_SargAttack12(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   int damage, mod, mul;

   if(!actor->target)
      return;

   mod = E_ArgAsInt(actionargs->args, 0, 10);
   mul = E_ArgAsInt(actionargs->args, 1,  4);
   // ioanch: also pufftype
   const char *pufftype = E_ArgAsString(actionargs->args, 2, nullptr);

   A_FaceTarget(actionargs);
   damage = ((P_Random(pr_sargattack) % mod) + 1) * mul;
   P_LineAttack(actor, actor->angle, MELEERANGE, 0, damage, pufftype);
}

//
// A_HeadAttack
//
// Cacodemon attack.
//
void A_HeadAttack(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   if(!actor->target)
      return;
   
   A_FaceTarget(actionargs);
   
   if(P_CheckMeleeRange(actor))
   {
      int damage = (P_Random(pr_headattack)%6+1)*10;
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
   }
   else
   {
      // launch a missile
      P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_HEADSHOT),
                     actor->z + DEFAULTMISSILEZ);
   }
}

//
// A_BruisAttack
//
// Baron of Hell attack.
//
void A_BruisAttack(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   if(!actor->target)
      return;
   
   if(P_CheckMeleeRange(actor))
   {
      int damage;
      S_StartSound(actor, sfx_claw);
      damage = (P_Random(pr_bruisattack)%8+1)*10;
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
   }
   else
   {
      // launch a missile
      P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_BRUISERSHOT),
                     actor->z + DEFAULTMISSILEZ);  
   }
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
void A_SpidRefire(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   // keep firing unless target got out of sight
   A_FaceTarget(actionargs);
   
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
void A_BspiAttack(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   if(!actor->target)
      return;
   
   A_FaceTarget(actionargs);
   
   // launch a missile
   P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_ARACHPLAZ), 
                  actor->z + DEFAULTMISSILEZ);
}

//
// A_BabyMetal
//
// Arachnotron chase w/sound.
//
void A_BabyMetal(actionargs_t *actionargs)
{
   S_StartSound(actionargs->actor, sfx_bspwlk);
   A_Chase(actionargs);
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
void A_Hoof(actionargs_t *actionargs)
{
   S_StartSound(actionargs->actor, sfx_hoof);
   A_Chase(actionargs);
}

//
// A_Metal
//
// Cyberdemon chase pointer 2 with metal sound.
// Also used by Spider Mastermind.
//
void A_Metal(actionargs_t *actionargs)
{
   S_StartSound(actionargs->actor, sfx_metal);
   A_Chase(actionargs);
}

//
// A_CyberAttack
//
// Cyberdemon rocket attack.
//
void A_CyberAttack(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   if(!actor->target)
      return;
   
   A_FaceTarget(actionargs);
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
void A_SkelMissile(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   Mobj *mo;
   
   if(!actor->target)
      return;
   
   A_FaceTarget(actionargs);
   actor->z += 16*FRACUNIT;      // so missile spawns higher
   mo = P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_TRACER),
                       actor->z + DEFAULTMISSILEZ);
   actor->z -= 16*FRACUNIT;      // back to normal
   
   mo->x += mo->momx;
   mo->y += mo->momy;
   mo->backupPosition();
   P_SetTarget<Mobj>(&mo->tracer, actor->target);  // killough 11/98
}

#define TRACEANGLE 0xc000000   /* killough 9/9/98: change to #define */

//
// A_Tracer
//
// (Accidentally?) randomized homing missile maintenance.
// ioanch 20151230: fixed to be portal-aware
//
void A_Tracer(actionargs_t *actionargs)
{
   angle_t  exact;
   fixed_t  dist;
   fixed_t  slope;
   Mobj    *actor = actionargs->actor;
   Mobj    *dest;
   Mobj    *th;

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
   P_SpawnPuff(actor->x, actor->y, actor->z, 0, 3, false, actor->target);
   
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

   fixed_t dx = getThingX(actor, dest);
   fixed_t dy = getThingY(actor, dest);
   fixed_t dz = getThingZ(actor, dest);

   // change angle
   exact = P_PointToAngle(actor->x, actor->y, dx, dy);

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
   dist = P_AproxDistance(dx - actor->x, dy - actor->y);
   
   dist = dist / actor->info->speed;

   if(dist < 1)
      dist = 1;

   slope = (dz + 40*FRACUNIT - actor->z) / dist;
   
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
void A_SkelWhoosh(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   if(!actor->target)
      return;
   A_FaceTarget(actionargs);
   S_StartSound(actor, sfx_skeswg);
}

//
// A_SkelFist
//
// Revenant punch attack.
//
void A_SkelFist(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   
   if(!actor->target)
      return;
   
   A_FaceTarget(actionargs);
   
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

static Mobj    *corpsehit;
static Mobj    *vileobj;
static fixed_t  viletryx;
static fixed_t  viletryy;

//
// PIT_VileCheck
//
// Detect a corpse that could be raised.
//
static bool PIT_VileCheck(Mobj *thing, void *context)
{
   int maxdist;
   int vileType = E_SafeThingType(MT_VILE);
   
   // ioanch 20160221: call functions
   if(!P_ThingIsCorpse(thing))
      return true;
   
   maxdist = thing->info->radius + mobjinfo[vileType]->radius;
   
   if(D_abs(getThingX(vileobj, thing) - viletryx) > maxdist ||
      D_abs(getThingY(vileobj, thing) - viletryy) > maxdist)
      return true;                // not actually touching

   // ioanch 20160108: since now it's portal aware, make sure that corpses
   // from different group ids are seen. This restriction doesn't apply for
   // single layer areas.
   if(vileobj->groupid != R_NOGROUP && thing->groupid != R_NOGROUP &&
      vileobj->groupid != thing->groupid && !P_CheckSight(vileobj, thing))
   {
      return true;
   }

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

   // Exit (return false) if the function returns true!
   return !P_CheckCorpseRaiseSpace(corpsehit);
}

//
// A_VileChase
//
// Check for ressurecting a body
//
void A_VileChase(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   if(actor->movedir != DI_NODIR)
   {
      // check for corpses to raise
      viletryx =
         actor->x + actor->info->speed*xspeed[actor->movedir];
      viletryy =
         actor->y + actor->info->speed*yspeed[actor->movedir];

      // ioanch 20160108: make resurrection portal aware
      fixed_t bbox[4];
      bbox[BOXLEFT] = viletryx - MAXRADIUS*2;
      bbox[BOXBOTTOM] = viletryy - MAXRADIUS*2;
      bbox[BOXRIGHT] = viletryx + MAXRADIUS*2;
      bbox[BOXTOP] = viletryy + MAXRADIUS*2;
      vileobj = actor;

      if(!P_TransPortalBlockWalker(bbox, actor->groupid, true, actionargs, 
         [](int x, int y, int groupid, void *data) -> bool
      {
         // get the variables back
         auto actionargs = static_cast<actionargs_t *>(data);
         Mobj *actor = actionargs->actor;

         if(!P_BlockThingsIterator(x, y, groupid, PIT_VileCheck))
         {
            // got one!
            Mobj *temp = actor->target;
            actor->target = corpsehit;
            A_FaceTarget(actionargs);
            actor->target = temp;

            // ioanch 20160220: allow custom state
            const state_t *healstate = E_GetStateForMobjInfo(actor->info, 
                                                               METASTATE_HEAL);
            if(healstate && healstate->index != NullStateNum)
               P_SetMobjState(actor, healstate->index);
            else
               P_SetMobjState(actor, S_VILE_HEAL1);   // Doom behaviour

            // ioanch 20160221: call function
            P_RaiseCorpse(corpsehit, actor);
            return false;
         }
         return true;
      }))
         return;  // if the block returned false, exit
   }
   A_Chase(actionargs);  // Return to normal attack.
}

//
// A_VileStart
//
// Play vilatk sound.
//
void A_VileStart(actionargs_t *actionargs)
{
   S_StartSound(actionargs->actor, sfx_vilatk);
}

//
// A_Fire
//
// Keep fire in front of player unless out of sight
//
void A_Fire(actionargs_t *actionargs)
{
   angle_t an;
   Mobj *actor = actionargs->actor;
   Mobj *dest  = actor->tracer;
   
   if(!dest)
      return;
   
   // don't move it if the vile lost sight
   if(!P_CheckSight(actor->target, dest) )
      return;
   
   an = dest->angle >> ANGLETOFINESHIFT;
   
   P_UnsetThingPosition(actor);
   // IOANCH 20160106: correct line portal behaviour
   v2fixed_t pos = P_LinePortalCrossing(*dest,
                                        FixedMul(24*FRACUNIT, finecosine[an]),
                                        FixedMul(24*FRACUNIT, finesine[an]));
   actor->x = pos.x;
   actor->y = pos.y;
   actor->z = dest->z;
   actor->backupPosition();
   P_SetThingPosition(actor);
}

//
// A_StartFire
//
// Start vile fire effect. This never does anything in vanilla DOOM however,
// because it is only in the fire's spawn state, and the pointer in any
// object's spawnstate is never executed the first time it spawns.
//
void A_StartFire(actionargs_t *actionargs)
{
   S_StartSound(actionargs->actor, sfx_flamst);
   A_Fire(actionargs);
}

//
// A_FireCrackle
//
// Play fire crackling sound.
//
void A_FireCrackle(actionargs_t *actionargs)
{
   S_StartSound(actionargs->actor, sfx_flame);
   A_Fire(actionargs);
}


//
// A_VileTarget
//
// Spawn the hellfire
//
void A_VileTarget(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   Mobj *fog;
   
   if(!actor->target)
      return;

   A_FaceTarget(actionargs);
   
   // killough 12/98: fix Vile fog coordinates
   fog = P_SpawnMobj(actor->target->x,
                     demo_version < 203 ? actor->target->x : actor->target->y,
                     actor->target->z,E_SafeThingType(MT_FIRE));
   
   P_SetTarget<Mobj>(&actor->tracer, fog);   // killough 11/98
   P_SetTarget<Mobj>(&fog->target, actor);
   P_SetTarget<Mobj>(&fog->tracer, actor->target);
   
   actionargs_t fogaction;

   fogaction.actiontype = actionargs_t::MOBJFRAME;
   fogaction.actor      = fog;
   fogaction.args       = ESAFEARGS(fog);
   fogaction.pspr       = nullptr;
   
   A_Fire(&fogaction);
}

//
// A_VileAttack
//
// Arch-vile attack.
//
void A_VileAttack(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   Mobj *fire;
   int   an;
   
   if(!actor->target)
      return;

   A_FaceTarget(actionargs);
   
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
   // ioanch 20160106: correct fire position based on portals
   v2fixed_t pos = P_LinePortalCrossing(*actor->target, 
                                        -FixedMul(24 * FRACUNIT, finecosine[an]),
                                        -FixedMul(24 * FRACUNIT, finesine[an]));
   fire->x = pos.x;
   fire->y = pos.y;
   
   // ioanch: set the correct group ID now
   if(full_demo_version >= make_full_version(340, 48))
      fire->groupid = R_PointInSubsector(pos)->sector->groupid;
   
   P_RadiusAttack(fire, actor, 70, 70, actor->info->mod, 0);
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
void A_FatRaise(actionargs_t *actionargs)
{
   A_FaceTarget(actionargs);
   S_StartSound(actionargs->actor, sfx_manatk);
}

//
// A_FatAttack1
//
// Mancubus attack 1.
// * args[0] = type to spawn
//
void A_FatAttack1(actionargs_t *actionargs)
{
   Mobj   *actor = actionargs->actor;
   Mobj   *mo;
   int     an;
   fixed_t z = actor->z + DEFAULTMISSILEZ;
   int thingType = E_ArgAsThingNumG0(actionargs->args, 0);
   if(thingType < 0)
      thingType = E_SafeThingType(MT_FATSHOT);

   // haleyjd: no crashing
   if(!actor->target)
      return;

   A_FaceTarget(actionargs);

   // Change direction  to ...
   actor->angle += FATSPREAD;

   P_SpawnMissile(actor, actor->target, thingType, z);

   mo = P_SpawnMissile(actor, actor->target, thingType, z);
   mo->angle += FATSPREAD;
   an = mo->angle >> ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_FatAttack2
//
// Mancubus attack 2.
// * args[0] = type to spawn
//
void A_FatAttack2(actionargs_t *actionargs)
{
   Mobj   *actor = actionargs->actor;
   Mobj   *mo;
   int     an;
   fixed_t z = actor->z + DEFAULTMISSILEZ;
   int thingType = E_ArgAsThingNumG0(actionargs->args, 0);
   if(thingType < 0)
      thingType = E_SafeThingType(MT_FATSHOT);

   // haleyjd: no crashing
   if(!actor->target)
      return;

   A_FaceTarget(actionargs);

   // Now here choose opposite deviation.
   actor->angle -= FATSPREAD;
   P_SpawnMissile(actor, actor->target, thingType, z);

   mo = P_SpawnMissile(actor, actor->target, thingType, z);
   mo->angle -= FATSPREAD*2;
   an = mo->angle >> ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// A_FatAttack3
//
// Mancubus attack 3.
// * args[0] = type to spawn
//
void A_FatAttack3(actionargs_t *actionargs)
{
   Mobj   *actor = actionargs->actor;
   Mobj   *mo;
   int     an;
   fixed_t z = actor->z + DEFAULTMISSILEZ;
   int thingType = E_ArgAsThingNumG0(actionargs->args, 0);
   if(thingType < 0)
      thingType = E_SafeThingType(MT_FATSHOT);
   // haleyjd: no crashing
   if(!actor->target)
      return;

   A_FaceTarget(actionargs);

   mo = P_SpawnMissile(actor, actor->target, thingType, z);
   mo->angle -= FATSPREAD/2;
   an = mo->angle >> ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy = FixedMul(mo->info->speed, finesine[an]);

   mo = P_SpawnMissile(actor, actor->target, thingType, z);
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
void A_SkullAttack(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   if(!actor->target)
      return;
   
   S_StartSound(actor, actor->info->attacksound);

   // haleyjd 08/07/04: use new P_SkullFly function
   P_SkullFly(actor, SKULLSPEED, false);
}

//
// A_BetaSkullAttack()
//
// killough 10/98: this emulates the beta version's lost soul attacks
//
void A_BetaSkullAttack(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   int   damage;
   
   // haleyjd: changed to check if objects are the SAME type, not
   // for hard-coded lost soul
   if(!actor->target || actor->target->type == actor->type)
      return;
   
   S_StartSound(actor, actor->info->attacksound);
   
   A_FaceTarget(actionargs);
   damage = (P_Random(pr_skullfly)%8+1)*actor->damage;
   P_DamageMobj(actor->target, actor, actor, damage, actor->info->mod);
}

//
// A_Stop
//
// Remove all momentum from an object.
// POINTER-TODO: Move to a_common?
//
void A_Stop(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   actor->momx = actor->momy = actor->momz = 0;
}

enum painattackflags_e : unsigned int
{
   PAF_NOSKULLATTACK = 0x00000001,
   PAF_AIMFACING     = 0x00000002,
   PAF_NOTARGET      = 0x00000004,
};

//
// Spawn a lost soul and launch it at the target
//
static void A_painShootSkull(Mobj *actor, const angle_t angle, int thingType,
                             const unsigned int flags = 0, int count = -1)
{
   fixed_t  x, y, z;
   Mobj    *newmobj;
   angle_t  an;
   int      prestep;

   // The original code checked for 20 skulls on the level,    // phares
   // and wouldn't spit another one if there were. If not in   // phares
   // compatibility mode, we remove the limit.                 // phares

   if((getComp(comp_pain) && count < 0) || count > 0)  // killough 10/98: compatibility-optioned
   {
      // count total number of skulls currently on the level
      if(count < 0)
         count = 20;
      else
         count--;

      Thinker *th;
      for(th = thinkercap.next; th != &thinkercap; th = th->next)
      {
         Mobj *mo;
         if((mo = thinker_cast<Mobj *>(th)) && mo->type == thingType)
         {
            if(--count < 0)         // killough 8/29/98: early exit
               return;
         }
      }
   }

   // okay, there's room for another one
   
   an = angle >> ANGLETOFINESHIFT;
   
   prestep = 4*FRACUNIT + 3*(actor->info->radius + mobjinfo[thingType]->radius)/2;

   // ioanch 20160107: spawn at the correct position if there's a line portal
   // between monster and intended position.
   // Also keep track of the "relative" position: the one without portal trans-
   // lation. Needed for Check_Sides
   v2fixed_t relpos = { actor->x + FixedMul(prestep, finecosine[an]),
                        actor->y + FixedMul(prestep, finesine[an]) };
   v2fixed_t pos = P_LinePortalCrossing(*actor, relpos - v2fixed_t{ actor->x, actor->y });
   x = pos.x;
   y = pos.y;
   z = actor->z + 8*FRACUNIT;
   
   if(getComp(comp_skull))   // killough 10/98: compatibility-optioned
      newmobj = P_SpawnMobj(x, y, z, thingType);                    // phares
   else                                                             //   V
   {
      // Check whether the Lost Soul is being fired through a 1-sided
      // wall or an impassible line, or a "monsters can't cross" line.
      // If it is, then we don't allow the spawn. This is a bug fix, 
      // but it should be considered an enhancement, since it may 
      // disturb existing demos, so don't do it in compatibility mode.

      // ioanch 20160107: check sides against the non-translated position. This 
      // way the two coordinates will be in valid range and it will only check
      // sides against the passable portal line
      if(Check_Sides(actor, relpos.x, relpos.y, thingType))
         return;
      
      newmobj = P_SpawnMobj(x, y, z, thingType);
      
      // Check to see if the new Lost Soul's z value is above the
      // ceiling of its new sector, or below the floor. If so, kill it.

      // ioanch 20160107: check against the floor or ceiling sector behind any
      // portals
      if(!P_CheckFloorCeilingForSpawning(*newmobj))
      {
         // kill it immediately
         P_DamageMobj(newmobj,actor,actor,GOD_BREACH_DAMAGE,MOD_UNKNOWN);
         return;                                                    //   ^
      }                                                             //   |
   }                                                                // phares

   // killough 7/20/98: PEs shoot lost souls with the same friendliness
   newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (actor->flags & MF_FRIEND);

   // killough 8/29/98: add to appropriate thread
   newmobj->updateThinker();

   // Check for movements.
   // killough 3/15/98: don't jump over dropoffs:

   if(!P_TryMove(newmobj, newmobj->x, newmobj->y, false))
   {
      // kill it immediately
      P_DamageMobj(newmobj, actor, actor, GOD_BREACH_DAMAGE, MOD_UNKNOWN);
      return;
   }
   
   if(!(flags & PAF_NOTARGET))
      P_SetTarget<Mobj>(&newmobj->target, actor->target);

   if(!(flags & PAF_NOSKULLATTACK))
   {
      actionargs_t skullaction;

      skullaction.actiontype = actionargs_t::MOBJFRAME;
      skullaction.actor      = newmobj;
      skullaction.args       = ESAFEARGS(newmobj);
      skullaction.pspr       = nullptr;

      A_SkullAttack(&skullaction);
   }
}

static dehflags_t painattack_flaglist[] =
{
   { "NOSKULLATTACK", PAF_NOSKULLATTACK },
   { "AIMFACING",     PAF_AIMFACING     },
   { "NOTARGET",      PAF_NOTARGET      },
   { nullptr,         0                 }
};

static dehflagset_t painattack_flagset =
{
   painattack_flaglist, // flaglist
   0,                   // mode
};

//
// Spawn a thing and launch it at the target
// * args[0] = type to spawn
// * args[1] = angle to add to spawned thing
// * args[2] = flags
// * args[3] = maximum amount allowed on map (infinite if 0, 21 if negative)
//
void A_PainAttack(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   if(!actor->target)
      return;

   int thingType = E_ArgAsThingNumG0(actionargs->args, 0);
   if(thingType < 0)
      thingType = E_SafeThingType(MT_SKULL);
   const angle_t      angle = FixedToAngle(E_ArgAsFixed(actionargs->args, 1, 0));
   const unsigned int flags = E_ArgAsFlags(actionargs->args, 2, &painattack_flagset);
   const int          count = E_ArgAsInt(actionargs->args, 3, -1);

   if(!(flags & PAF_NOTARGET))
      A_FaceTarget(actionargs);
   A_painShootSkull(actor, actor->angle + angle, thingType, flags, count);
}

//
// Normal fall logic plus 3 things spawn.
// * args[0] = type to spawn
//
void A_PainDie(actionargs_t *actionargs)
{
   Mobj *actor     = actionargs->actor;
   int   thingType = E_ArgAsThingNumG0(actionargs->args, 0);
   if(thingType < 0)
      thingType = E_SafeThingType(MT_SKULL);
   A_Fall(actionargs);
   A_painShootSkull(actor, actor->angle + ANG90,  thingType);
   A_painShootSkull(actor, actor->angle + ANG180, thingType);
   A_painShootSkull(actor, actor->angle + ANG270, thingType);
}

//=============================================================================
//
// Special Death Effects
//

struct boss_spec_t
{
   unsigned int thing_flag;
   unsigned int level_flag;
};

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
void A_BossDeath(actionargs_t *actionargs)
{
   Mobj    *mo = actionargs->actor;
   Thinker *th;
   line_t   junk;
   int      i;

   // make sure there is a player alive for victory
   for(i = 0; i < MAXPLAYERS; i++)
   {
      if(playeringame[i] && players[i].health > 0)
         break;
   }

   // no one left alive, so do not end game
   if(i == MAXPLAYERS)
      return;

   for(boss_spec_t &boss_spec : boss_specs)
   {
      // to activate a special, the thing must be a boss that triggers
      // it, and the map must have the special enabled.
      if((mo->flags2 & boss_spec.thing_flag) &&
         (LevelInfo.bossSpecs & boss_spec.level_flag))
      {
         // scan the remaining thinkers to see if all bosses are dead
         for(th = thinkercap.next; th != &thinkercap; th = th->next)
         {
            Mobj *mo2;
            if((mo2 = thinker_cast<Mobj *>(th)))
            {
               if(mo2 != mo &&
                  (mo2->flags2 & boss_spec.thing_flag) &&
                  mo2->health > 0)
                  return;         // other boss not dead
            }
         }

         // victory!
         switch(boss_spec.level_flag)
         {
         case BSPEC_E1M8:
         case BSPEC_E4M8:
         case BSPEC_MAP07_1:
            // lower floors tagged 666 to lowest neighbor
            junk.args[0] = junk.tag = 666;
            EV_DoFloor(&junk, lowerFloorToLowest);
            break;
         case BSPEC_MAP07_2:
            // raise floors tagged 667 by shortest lower texture
            junk.args[0] = junk.tag = 667;
            EV_DoFloor(&junk, raiseToTexture);
            break;
         case BSPEC_E2M8:
         case BSPEC_E3M8:
            // exit map -- no use processing any further after this
            G_ExitLevel();
            return;
         case BSPEC_E4M6:
            // open sectors tagged 666 as blazing doors
            EV_DoDoor(666, blazeOpen);
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
void A_KeenDie(actionargs_t *actionargs)
{
   Mobj    *mo = actionargs->actor;
   Thinker *th;

   A_Fall(actionargs);

   // scan the remaining thinkers to see if all Keens are dead
   
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      Mobj *mo2;
      if((mo2 = thinker_cast<Mobj *>(th)))
      {
         if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
            return;                           // other Keen not dead
      }
   }

   EV_DoDoor(666, doorOpen);
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

void P_SpawnBrainTargets()  // killough 3/26/98: renamed old function
{
   // find all the target spots
   braintargets.mobjType = "BossSpawnSpot";
   braintargets.makeEmpty();

   brain.easy = 0;   // killough 3/26/98: always init easy to 0

   braintargets.collectThings();
}

// haleyjd 07/30/04: P_CollectThings moved to p_mobj.c

//
// A_BrainAwake
//
// Awaken the Boss Brain spawn shooter.
//
void A_BrainAwake(actionargs_t *actionargs)
{
   S_StartSound(nullptr, sfx_bossit); // killough 3/26/98: only generates sound now
}

//
// A_BrainPain
//
// Called when the Romero head is injured.
//
void A_BrainPain(actionargs_t *actionargs)
{
   S_StartSound(nullptr, sfx_bospn);
}

//
// A_BrainScream
//
// Romero head death effects.
//
void A_BrainScream(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;
   int   x;
   int   rocketType = E_SafeThingType(MT_ROCKET);

   for(x = mo->x - 196*FRACUNIT; x < mo->x + 320*FRACUNIT; x += FRACUNIT*8)
   {
      int   y  = mo->y - 320 * FRACUNIT;
      int   z  = 128 + P_Random(pr_brainscream) * 2 * FRACUNIT;      
      Mobj *th = P_SpawnMobj(x, y, z, rocketType);
      // haleyjd 02/21/05: disable particle events/effects for this thing
      th->intflags |= MIF_NOPTCLEVTS;
      th->effects = 0;
      th->momz = P_Random(pr_brainscream) * 512;
      P_SetMobjState(th, E_SafeState(S_BRAINEXPLODE1));
      th->tics -= P_Random(pr_brainscream) & 7;
      if(th->tics < 1)
         th->tics = 1;
   }
   S_StartSound(nullptr, sfx_bosdth);
}

//
// A_BrainExplode
//
// More Romero head death effects.
//
void A_BrainExplode(actionargs_t *actionargs)
{  
   Mobj *mo = actionargs->actor;

   // haleyjd 08/05/04: use new function
   int x = mo->x + P_SubRandom(pr_brainexp)*2048;
   int y = mo->y;
   int z = 128 + P_Random(pr_brainexp)*2*FRACUNIT;

   Mobj *th = P_SpawnMobj(x, y, z, E_SafeThingType(MT_ROCKET));
   th->momz = P_Random(pr_brainexp) * 512;
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
void A_BrainDie(actionargs_t *actionargs)
{
   G_ExitLevel();
}

//
// A_BrainSpit
//
// Spawn cube shooter attack.
//
void A_BrainSpit(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;
   Mobj *targ, *newmobj;
   int SpawnShotType = E_SafeThingType(MT_SPAWNSHOT);
   
    // killough 4/1/98: ignore if no targets
   if(braintargets.isEmpty())
      return;

   brain.easy ^= 1;          // killough 3/26/98: use brain struct
   if(gameskill <= sk_easy && !brain.easy)
      return;

   // shoot a cube at current target
   targ = braintargets.wrapIterator();

   // spawn brain missile
   newmobj = P_SpawnMissile(mo, targ, SpawnShotType, mo->z + DEFAULTMISSILEZ);
   P_SetTarget<Mobj>(&newmobj->target, targ);
   if(!newmobj->state->tics)
   {
      newmobj->remove();
      doom_printf(FC_ERROR "BrainSpit: invalid 0 tic spawn state");
      return;
   }
   newmobj->reactiontime = (int16_t)(((targ->y-mo->y)/newmobj->momy)/newmobj->state->tics);

   // killough 7/18/98: brain friendliness is transferred
   newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (mo->flags & MF_FRIEND);
   
   // killough 8/29/98: add to appropriate thread
   newmobj->updateThinker();
   
   S_StartSound(nullptr, sfx_bospit);
}

void A_SpawnFly(actionargs_t *actionargs);

//
// A_SpawnSound
//
// travelling cube sound
//
void A_SpawnSound(actionargs_t *actionargs)
{
   S_StartSound(actionargs->actor, sfx_boscub);
   A_SpawnFly(actionargs);
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
void A_SpawnFly(actionargs_t *actionargs)
{
   int   i;         // schepe 
   int   r;
   mobjtype_t type = 0;
   int fireType = E_SafeThingType(MT_SPAWNFIRE); 

   Mobj *mo = actionargs->actor;
   Mobj *fog;
   Mobj *targ;
   Mobj *newmobj;  // killough 8/9/98

   // haleyjd 05/31/06: allow 0 boss types
   if(NumBossTypes == 0)
      return;

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
   newmobj->updateThinker();
   
   if(P_LookForTargets(newmobj, true))      // killough 9/4/98
      P_SetMobjState(newmobj, newmobj->info->seestate);
   
   // telefrag anything in this spot
   P_TeleportMove(newmobj, newmobj->x, newmobj->y, true); // killough 8/9/98
   
   // remove self (i.e., cube).
   mo->remove();
}

// EOF

