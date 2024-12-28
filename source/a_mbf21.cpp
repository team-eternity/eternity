//
// The Eternity Engine
// Copyright(C) 2021 James Haley, Max Waine, et al.
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
// Purpose: MBF21 action functions
// All of this code is derived from dsda-doom,
// by Ryan Krafnick and Xaser Acheron, used under terms of the GPLv3.
// Authors: Max Waine
//

#include "z_zone.h"

#include "a_args.h"
#include "a_common.h"
#include "a_doom.h"
#include "d_event.h"
#include "d_gi.h"
#include "d_mod.h"
#include "d_player.h"
#include "doomstat.h"
#include "e_args.h"
#include "e_inventory.h"
#include "e_player.h"
#include "m_vector.h"
#include "p_enemy.h"
#include "p_inter.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_portalcross.h"
#include "r_main.h"
#include "s_sound.h"

//=============================================================================
//
// Actor codepointers
//

//
// Generic actor spawn function.
//
// args[0] -- type (dehnum) of actor to spawn
// args[1] -- angle (degrees), relative to calling actor's angle
// args[2] -- x (forward/back) spawn position offset
// args[3] -- y (left/right) spawn position offset
// args[4] -- z (up/down) spawn position offset
// args[5] -- x (forward/back) velocity
// args[6] -- y (left/right) velocity
// args[7] -- z (up/down) velocity
//
void A_SpawnObject(actionargs_t *actionargs)
{
   arglist_t *args  = actionargs->args;
   Mobj      *actor = actionargs->actor;
   int        thingtype;
   fixed_t    angle, fan;
   fixed_t    dx, dy;
   angle_t    an;
   v3fixed_t  ofs, vel;
   Mobj      *mo;

   thingtype = E_ArgAsThingNumG0(args, 0);
   if(!mbf21_demo || thingtype == -1)
      return;

   angle = E_ArgAsFixed(args, 1, 0);
   ofs.x = E_ArgAsFixed(args, 2, 0);
   ofs.y = E_ArgAsFixed(args, 3, 0);
   ofs.z = E_ArgAsFixed(args, 4, 0);
   vel.x = E_ArgAsFixed(args, 5, 0);
   vel.y = E_ArgAsFixed(args, 6, 0);
   vel.z = E_ArgAsFixed(args, 7, 0);

   // calculate position offsets
   an  = actor->angle + angle_t((int64_t(angle) << FRACBITS) / 360);
   fan = an >> ANGLETOFINESHIFT;
   dx  = FixedMul(ofs.x, finecosine[fan]) - FixedMul(ofs.y, finesine[fan]  );
   dy  = FixedMul(ofs.x, finesine[fan]  ) + FixedMul(ofs.y, finecosine[fan]);

   // calculate final horizontal position (portal aware)
   const v2fixed_t hpos = P_LinePortalCrossing(*actor, dx, dy);

   // spawn it, yo
   mo = P_SpawnMobj(hpos.x, hpos.y, actor->z + ofs.z, thingtype);
   if(!mo)
      return;

   // angle dangle
   mo->angle = an;

   // set velocity
   mo->momx = FixedMul(vel.x, finecosine[fan]) - FixedMul(vel.y, finesine[fan]  );
   mo->momy = FixedMul(vel.x, finesine[fan]  ) + FixedMul(vel.y, finecosine[fan]);
   mo->momz = vel.z;

   // if spawned object is a missile, set target+tracer
   if(mo->info->flags & (MF_MISSILE | MF_BOUNCES))
   {
      // if spawner is also a missile, copy 'em
      if(actor->info->flags & (MF_MISSILE | MF_BOUNCES))
      {
         P_SetTarget(&mo->target, actor->target);
         P_SetTarget(&mo->tracer, actor->tracer);
      }
      // otherwise, set 'em as if a monster fired 'em
      else
      {
         P_SetTarget(&mo->target, actor);
         P_SetTarget(&mo->tracer, actor->target);
      }
   }

   // [XA] don't bother with the dont-inherit-friendliness hack
   // that exists in A_Spawn, 'cause WTF is that about anyway?
}

//
// Generic monster projectile attack.
//
// args[0] -- type (dehnum) of actor to spawn
// args[1] -- angle (degrees), relative to calling actor's angle
// args[2] -- pitch (degrees), relative to calling actor's pitch
// args[3] -- horizontal spawn offset, relative to calling actor's angle
// args[4] -- vertical spawn offset, relative to actor's default projectile fire height
//
void A_MonsterProjectile(actionargs_t *actionargs)
{
   arglist_t *args  = actionargs->args;
   Mobj      *actor = actionargs->actor;
   int        thingtype;
   fixed_t    angle, pitch;
   fixed_t    spawnofs_xy, spawnofs_z;
   angle_t    an;
   Mobj      *mo;

   thingtype = E_ArgAsThingNumG0(args, 0);
   if(!mbf21_demo || !actor->target || thingtype == -1)
      return;

   angle       = E_ArgAsFixed(args, 1, 0);
   pitch       = E_ArgAsFixed(args, 2, 0);
   spawnofs_xy = E_ArgAsFixed(args, 3, 0);
   spawnofs_z  = E_ArgAsFixed(args, 4, 0);

   A_FaceTarget(actionargs);
   mo = P_SpawnMissile(actor, actor->target, thingtype, actor->z + actor->info->missileheight);
   if(!mo)
      return;

   // adjust angle
   mo->angle += angle_t((int64_t(angle) << 16) / 360);
   an         = mo->angle >> ANGLETOFINESHIFT;
   mo->momx   = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy   = FixedMul(mo->info->speed, finesine[an]);

   // adjust pitch (approximated, using Doom's ye olde
   // finetangent table; same method as monster aim)
   mo->momz += FixedMul(mo->info->speed, DegToSlope(pitch));

   // adjust position
   an     = (actor->angle - ANG90) >> ANGLETOFINESHIFT;
   mo->x += FixedMul(spawnofs_xy, finecosine[an]);
   mo->y += FixedMul(spawnofs_xy, finesine[an]);
   mo->z += spawnofs_z;

   // always set the 'tracer' field, so this pointer
   // can be used to fire seeker missiles at will.
   P_SetTarget(&mo->tracer, actor->target);
}

//
// Generic monster bullet attack.
//
// args[0] -- horizontal spread (degrees, in fixed point)
// args[1] -- vertical spread (degrees, in fixed point)
// args[2] -- number of bullets to fire; if not set, defaults to 1
// args[3] -- base damage of attack; if not set, defaults to 3
// args[4] -- attack damage random multiplier; if not set, defaults to 5
//
void A_MonsterBulletAttack(actionargs_t *actionargs)
{
   arglist_t *args  = actionargs->args;
   Mobj      *actor = actionargs->actor;
   fixed_t    hspread, vspread;
   int        numbullets, damagebase, damagemod;
   fixed_t    aimslope, slope;
   angle_t    angle;
   int        damage;

   if(!mbf21_demo || !actor->target)
      return;

   hspread    = E_ArgAsFixed(args, 0, 0);
   vspread    = E_ArgAsFixed(args, 1, 0);
   numbullets = E_ArgAsInt(args, 2, 1);
   damagebase = E_ArgAsInt(args, 3, 3);
   damagemod  = E_ArgAsInt(args, 4, 5);
   
   if(numbullets >= 1 && !damagemod)
   {
      doom_warningf("%s: invalid damage random multiplier 0", __func__);
      return;
   }

   A_FaceTarget(actionargs);
   S_StartSound(actor, actor->info->attacksound);

   aimslope = P_AimLineAttack(actor, actor->angle, MISSILERANGE, 0);

   for(int i = 0; i < numbullets; i++)
   {
      damage = (P_Random(pr_mbf21) % damagemod + 1) * damagebase;
      angle  = int(actor->angle) + P_RandomHitscanAngle(pr_mbf21, hspread);
      slope  = aimslope + P_RandomHitscanSlope(pr_mbf21, vspread);

      P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
   }
}


//
// Generic monster melee attack.
//
// args[0] -- base damage of attack; if not set, defaults to 3
// args[1] -- attack damage random multiplier; if not set, defaults to 8
// args[2] -- sound to play if attack hits
// args[3] -- attack range; if not set, defaults to calling actor's melee range property
//
void A_MonsterMeleeAttack(actionargs_t *actionargs)
{
   arglist_t *args  = actionargs->args;
   Mobj      *actor = actionargs->actor;
   int       damagebase, damagemod;
   sfxinfo_t *hitsound;
   fixed_t   range;
   int       damage;

   if(!mbf21_demo || !actor->target)
      return;

   damagebase = E_ArgAsInt(args, 0, 3);
   damagemod  = E_ArgAsInt(args, 1, 8);
   hitsound   = E_ArgAsSound(args, 2);
   range      = E_ArgAsFixed(args, 3, actor->info->meleerange);

   if (GameModeInfo->monsterMeleeRange == meleecalc_doom)
      range += actor->target->info->radius - 20 * FRACUNIT;

   A_FaceTarget(actionargs);
   if(!P_CheckRange(actor, range))
      return;

   if(hitsound)
      S_StartSound(actor, hitsound->dehackednum);

   damage = (P_Random(pr_mbf21) % damagemod + 1) * damagebase;
   P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
}

//
// Generic explosion.
//
// args[0] -- max explosion damge
// args[1] -- explosion radius, in map units
//
void A_RadiusDamage(actionargs_t *actionargs)
{
   arglist_t *args  = actionargs->args;
   Mobj      *actor = actionargs->actor;
   int        damage, radius;

   if(!mbf21_demo)
      return;

   damage = E_ArgAsInt(args, 0, 0);
   radius = E_ArgAsInt(args, 1, 0);

   P_RadiusAttack(actor, actor->target, damage, radius, actor->info->mod, 0);
}

//
// Alerts monsters within sound-travel distance of the calling actor's target.
//
void A_NoiseAlert(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   if(!mbf21_demo || !actor->target)
      return;

   P_NoiseAlert(actor->target, actor);
}

//
// Generic A_VileChase.
// Chases the player and resurrects any corpses with a Raise state when bumping into them.
//
// args[0] -- state to jump to on the calling actor when resurrecting a corpse
// args[1] -- sound to play when resurrecting a corpse
//
void A_HealChase(actionargs_t *actionargs)
{
   arglist_t *args = actionargs->args;
   Mobj      *actor = actionargs->actor;
   int state, sound;
   sfxinfo_t *sfxinfo;

   state = E_ArgAsStateNumNI(args, 0, actor);
   if(!mbf21_demo || state < 0)
      return;

   sfxinfo = E_ArgAsSound(args, 1);
   sound   = sfxinfo ? sfxinfo->dehackednum : 0;


   if(!P_HealCorpse(actionargs, actor->info->radius, state, sound))
      A_Chase(actionargs);
}

//
// Generic seeker missile function..
//
// args[0] -- if angle to target is lower than this, missile will 'snap' directly to face the target
// args[1] -- maximum angle a missile will turn towards the target if angle is above the threshold
//
void A_SeekTracer(actionargs_t *actionargs)
{
   arglist_t *args  = actionargs->args;
   Mobj      *actor = actionargs->actor;
   fixed_t    threshold, maxturnangle;

   if(!mbf21_demo)
      return;

   threshold    = FixedToAngle(E_ArgAsFixed(args, 0, 0));
   maxturnangle = FixedToAngle(E_ArgAsFixed(args, 1, 0));

   P_SeekerMissile(actor, threshold, maxturnangle, seekcenter_e::yes);
}

//
// Searches for a valid tracer (seek target), if the calling actor doesn't already have one.
// Particularly useful for player missiles.
//
// args[0] -- field-of-view, relative to calling actor's angle, to search for targets in.
//            if zero, the search will occur in all directions
// args[1] -- distance to search, in map blocks (128 units); if not set, defaults to 10
//
void A_FindTracer(actionargs_t *actionargs)
{
   arglist_t *args  = actionargs->args;
   Mobj      *actor = actionargs->actor;
   angle_t    fov;
   fixed_t    range;

   if(!mbf21_demo || actor->tracer)
      return;

   fov   = FixedToAngle(E_ArgAsFixed(args, 0, 0));
   range = E_ArgAsInt(args, 1, 10) * MAPBLOCKSIZE;

   if(fov == 0)
      fov = ANG360;

   // Code based on A_BouncingBFG
   for(int i = 1; i < 40; i++)  // offset angles from its attack angle
   {
      // Angle fans outward, preferring nearer angles
      angle_t an = actor->angle;
      if(i & 2)
         an += (fov / 40) * (i / 2);
      else
         an -= (fov / 40) * (i / 2);

      P_AimLineAttack(actor, an, range, true);

      // don't aim for shooter, or for friends of shooter
      if(clip.linetarget)
      {
         if(clip.linetarget == actor->target ||
            (clip.linetarget->flags & actor->target->flags & MF_FRIEND))
            continue;

         if(clip.linetarget)
         {
            P_SetTarget<Mobj>(&actor->tracer, clip.linetarget);
            return;
         }
      }
   }
}

//
// Clears the calling actor's tracer (seek target) field.
//
void A_ClearTracer(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   if(!mbf21_demo)
      return;

   P_ClearTarget(actor->tracer);
}

//
// Jumps to a state if caller's health is below the specified threshold.
//
// args[0] -- state to jump to
// args[1] -- health to check
//
void A_JumpIfHealthBelow(actionargs_t *actionargs)
{
   arglist_t *args = actionargs->args;
   Mobj      *actor = actionargs->actor;
   int state, health;

   state = E_ArgAsStateNumNI(args, 0, actor);
   if(!mbf21_demo || state < 0)
      return;

   health = E_ArgAsInt(args, 1, 0);

   if(actor->health < health)
      P_SetMobjState(actor, state);
}

//
// Generalised A_JumpIfTargetInSight and A_JumpIfTracerInSight
//
template<Mobj *Mobj::*field>
static void A_jumpIfMobjInSight(actionargs_t *actionargs)
{
   arglist_t *args  = actionargs->args;
   Mobj      *actor = actionargs->actor;
   Mobj      *mobj  = actor->*field;
   int state;

   if(!mbf21_demo || !mobj)
      return;

   fixed_t ffov = E_ArgAsFixed(args, 1, 0);

   // check fov if one is specified
   if(ffov > 0)
   {
      const angle_t fov    = FixedToAngle(ffov);
      const angle_t tang   = P_PointToAngle(actor->x, actor->y, getThingX(actor, mobj), getThingY(actor, mobj));
      const angle_t minang = actor->angle - FixedToAngle(fov) / 2;
      const angle_t maxang = actor->angle + FixedToAngle(fov) / 2;

      // if the angles are backward, compare differently
      if((minang > maxang) ? tang < minang && tang > maxang
                           : tang < minang || tang > maxang)
      {
         return;
      }
   }

   // check line of sight
   if(!P_CheckSight(actor, mobj))
      return;

   // prepare to jump!
   if(state = E_ArgAsStateNumNI(args, 0, actor); state < 0)
      return;

   P_SetMobjState(actor, state);
}

//
// Generalised A_JumpIfTargetCloser and A_JumpIfTracerCloser
//
template<Mobj *Mobj::*field>
static void A_jumpIfMobjCloser(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   Mobj      *actor  = actionargs->actor;
   Mobj      *mobj   = actor->*field;
   int     state;
   fixed_t distance;


   state = E_ArgAsStateNumNI(args, 0, actor);
   if(!mbf21_demo || !mobj || state < 0)
      return;

   distance = E_ArgAsFixed(args, 1, 0);

   const fixed_t dx = actor->x - getThingX(actor, mobj);
   const fixed_t dy = actor->y - getThingY(actor, mobj);
   if(distance > P_AproxDistance(dx, dy))
      P_SetMobjState(actor, state);
}

//
// Jumps to a state if caller's target is in line-of-sight.
//
// args[0] -- state to jump to
// args[1] -- field-of-view, relative to calling actor's angle, to check for target in.
//            if zero, the check will occur in all directions
//
void A_JumpIfTargetInSight(actionargs_t *actionargs)
{
   A_jumpIfMobjInSight<&Mobj::target>(actionargs);
}

//
// Jumps to a state if caller's target is closer than the specified distance.
//
// args[0] -- state to jump to
// args[1] -- distance threshold, in map units
//
void A_JumpIfTargetCloser(actionargs_t *actionargs)
{
   A_jumpIfMobjCloser<&Mobj::target>(actionargs);
}

//
// Jumps to a state if caller's tracer (seek target) is in line-of-sight.
//
// args[0] -- state to jump to
// args[1] -- field-of-view, relative to calling actor's angle, to check for target in.
//            if zero, the check will occur in all directions
//
void A_JumpIfTracerInSight(actionargs_t *actionargs)
{
   A_jumpIfMobjInSight<&Mobj::tracer>(actionargs);
}

//
// Jumps to a state if caller's tracer (seek target) is closer than the specified distance.
//
// args[0] -- state to jump to
// args[1] -- distance threshold, in map units
//
void A_JumpIfTracerCloser(actionargs_t *actionargs)
{
   A_jumpIfMobjCloser<&Mobj::tracer>(actionargs);

}

//
// Jumps to a state if caller has the specified thing flags set.
//
// args[0] -- state to jump to
// args[1] -- standard actor flag(s) to check
// args[2] -- MBF21 actor flag(s) to check
//
void A_JumpIfFlagsSet(actionargs_t *actionargs)
{
   arglist_t *args  = actionargs->args;
   Mobj      *actor = actionargs->actor;
   int           state;
   unsigned int  flags;
   unsigned int *mbf21flags;

   state = E_ArgAsStateNumNI(args, 0, actor);
   if(!mbf21_demo || !actor || state < 0)
      return;

   flags      = E_ArgAsInt(args, 1, 0);
   mbf21flags = E_ArgAsMBF21ThingFlags(args, 2);

   if((actor->flags & flags) == flags &&
      (
         !mbf21flags ||
         (
            (actor->flags2 & mbf21flags[DEHFLAGS_MODE2]) == mbf21flags[DEHFLAGS_MODE2] &&
            (actor->flags3 & mbf21flags[DEHFLAGS_MODE3]) == mbf21flags[DEHFLAGS_MODE3] &&
            (actor->flags4 & mbf21flags[DEHFLAGS_MODE4]) == mbf21flags[DEHFLAGS_MODE4] &&
            (actor->flags5 & mbf21flags[DEHFLAGS_MODE5]) == mbf21flags[DEHFLAGS_MODE5]
         )
      )
   )
      P_SetMobjState(actor, state);

}

//
// Adds the specified thing flags to the caller.
//
// args[0] -- standard actor flag(s) to add
// args[1] -- MBF21 actor flag(s) to add
//
void A_AddFlags(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   Mobj      *actor  = actionargs->actor;
   unsigned int  flags;
   unsigned int *mbf21flags;

   if(!mbf21_demo)
      return;

   flags      = E_ArgAsInt(args, 0, 0);
   mbf21flags = E_ArgAsMBF21ThingFlags(args, 1);

   unsigned preflags = actor->flags;
   actor->flags  |= flags;
   if(mbf21flags)
   {
      actor->flags2 |= mbf21flags[DEHFLAGS_MODE2];
      actor->flags3 |= mbf21flags[DEHFLAGS_MODE3];
      actor->flags4 |= mbf21flags[DEHFLAGS_MODE4];
      actor->flags5 |= mbf21flags[DEHFLAGS_MODE5];
   }
   // Must do it after the change, to avoid nested functions interpreting the current state.
   if(!(preflags & MF_NOSECTOR) && flags & MF_NOSECTOR)
      P_UnsetThingSectorLink(actor, false);
   if(!(preflags & MF_NOBLOCKMAP) && flags & MF_NOBLOCKMAP)
      P_UnsetThingBlockLink(actor);
}

//
// Removes the specified thing flags from the caller.
//
// args[0] -- standard actor flag(s) to remove
// args[1] -- MBF21 actor flag(s) to remove
//
void A_RemoveFlags(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   Mobj      *actor  = actionargs->actor;
   unsigned int  flags;
   unsigned int *mbf21flags;

   if(!mbf21_demo)
      return;

   flags      = E_ArgAsInt(args, 0, 0);
   mbf21flags = E_ArgAsMBF21ThingFlags(args, 1);

   unsigned preflags = actor->flags;
   actor->flags &= ~flags;
   if(mbf21flags)
   {
      actor->flags2 &= ~mbf21flags[DEHFLAGS_MODE2];
      actor->flags3 &= ~mbf21flags[DEHFLAGS_MODE3];
      actor->flags4 &= ~mbf21flags[DEHFLAGS_MODE4];
      actor->flags5 &= ~mbf21flags[DEHFLAGS_MODE5];
   }

   if(preflags & MF_NOSECTOR && flags & MF_NOSECTOR)
      P_SetThingSectorLink(actor, nullptr);
   if(preflags & MF_NOBLOCKMAP && flags & MF_NOBLOCKMAP)
      P_SetThingBlockLink(actor);
}

//=============================================================================
//
// Weapon codepointers
//

//
// Generic weapon projectile attack.
//
// args[0] -- type (dehnum) of actor to spawn
// args[1] -- angle (degrees), relative to player's angle
// args[2] -- pitch (degrees), relative to player's pitch
// args[3] -- horizontal spawn offset, relative to player's angle
// args[4] -- vertical spawn offset, relative to player's default projectile fire height
//
void A_WeaponProjectile(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   Mobj      *actor  = actionargs->actor;
   player_t  *player = actor->player;
   int        thingtype;
   fixed_t    angle, pitch;
   fixed_t    spawnofs_xy, spawnofs_z;
   angle_t    an;
   Mobj      *mo;

   thingtype = E_ArgAsThingNumG0(args, 0);
   if(!mbf21_demo || !player || thingtype == -1)
      return;

   angle       = E_ArgAsFixed(args, 1, 0);
   pitch       = E_ArgAsFixed(args, 2, 0);
   spawnofs_xy = E_ArgAsFixed(args, 3, 0);
   spawnofs_z  = E_ArgAsFixed(args, 4, 0);

   mo = P_SpawnPlayerMissile(actor, thingtype);
   if(!mo)
      return;

   // adjust angle
   mo->angle += angle_t((int64_t(angle) << 16) / 360);
   an         = mo->angle >> ANGLETOFINESHIFT;
   mo->momx   = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy   = FixedMul(mo->info->speed, finesine[an]);

   // adjust pitch (approximated, using Doom's ye olde
   // finetangent table; same method as autoaim)
   mo->momz += FixedMul(mo->info->speed, DegToSlope(pitch));

   // adjust position
   an     = (actor->angle - ANG90) >> ANGLETOFINESHIFT;
   mo->x += FixedMul(spawnofs_xy, finecosine[an]);
   mo->y += FixedMul(spawnofs_xy, finesine[an]);
   mo->z += spawnofs_z;

   // always set the 'tracer' field, so this pointer
   // can be used to fire seeker missiles at will.
   P_SetTarget(&mo->tracer, clip.linetarget);
}

//
// Generic weapon bullet attack.
//
// args[0] -- horizontal spread (degrees, in fixed point)
// args[1] -- vertical spread (degrees, in fixed point)
// args[2] -- number of bullets to fire; if not set, defaults to 1
// args[3] -- base damage of attack; if not set, defaults to 5
// args[4] -- attack damage random multiplier; if not set, defaults to 3
//
void A_WeaponBulletAttack(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   Mobj      *actor  = actionargs->actor;
   player_t  *player = actor->player;
   fixed_t    hspread, vspread;
   int        numbullets, damagebase, damagemod;
   fixed_t    slope;
   angle_t    angle;
   int        damage;

   if(!mbf21_demo || !player)
      return;

   hspread    = E_ArgAsFixed(args, 0, 0);
   vspread    = E_ArgAsFixed(args, 1, 0);
   numbullets = E_ArgAsInt(args, 2, 1);
   damagebase = E_ArgAsInt(args, 3, 5);
   damagemod  = E_ArgAsInt(args, 4, 3);

   P_BulletSlope(actor);

   for(int i = 0; i < numbullets; i++)
   {
      damage = (P_Random(pr_mbf21) % damagemod + 1) * damagebase;
      angle  = int(actor->angle) + P_RandomHitscanAngle(pr_mbf21, hspread);
      slope  = bulletslope + P_RandomHitscanSlope(pr_mbf21, vspread);

      P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
   }
}

//
// Generic weapon melee attack.
//
// args[0] -- base damage of attack; if not set, defaults to 2
// args[1] -- attack damage random multiplier; if not set, defaults to 10
// args[2] -- berserk damage multiplier; if not set, defaults to 1.0
// args[3] -- sound index to play if attack hits
// args[4] -- attack range; if not set, defaults to player mobj's melee range property
//
void A_WeaponMeleeAttack(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   Mobj      *actor  = actionargs->actor;
   player_t  *player = actor->player;
   int       damagebase, damagedice, zerkfactor;
   sfxinfo_t *hitsound;
   fixed_t   range;
   int       damage;
   angle_t   angle;
   fixed_t   slope;

   if(!mbf21_demo || !player)
      return;

   damagebase = E_ArgAsInt(args, 0, 2);
   damagedice = E_ArgAsInt(args, 1, 10);
   zerkfactor = E_ArgAsFixed(args, 2, FRACUNIT);
   hitsound   = E_ArgAsSound(args, 3);
   range      = E_ArgAsFixed(args, 4, 0);

   if(range == 0)
      range = player->mo->info->meleerange;

   damage = (P_Random(pr_mbf21) % damagedice + 1) * damagebase;
   if(player->powers[pw_strength].isActive())
      damage = (damage * zerkfactor) >> FRACBITS;


   // slight randomization; weird vanillaism here. :P
   angle = player->mo->angle;
   angle += P_SubRandom(pr_mbf21) << 18;

   // make autoaim prefer enemies
   slope = P_AimLineAttack(player->mo, angle, range, true);
   if(!clip.linetarget)
      slope = P_AimLineAttack(player->mo, angle, range, 0);

   // attack, dammit!
   P_LineAttack(player->mo, angle, range, slope, damage);

   // missed? ah, welp.
   if(!clip.linetarget)
      return;

   if(hitsound)
      P_WeaponSoundInfo(actor, hitsound);

   // turn to face target
   actor->angle = R_PointToAngle2(actor->x, actor->y, clip.linetarget->x, clip.linetarget->y);

}

//
// Generic playsound for weapons.
//
// args[0] -- sound index to play
// args[1] -- if nonzero, play this sound at full volume across the entire map
//
void A_WeaponSound(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   Mobj      *mo     = actionargs->actor;
   player_t  *player = mo->player;
   sfxinfo_t *sfxinfo;
   bool fullVol;

   if(!mbf21_demo || !player)
      return;

   sfxinfo = E_ArgAsSound(args, 0);
   fullVol = E_ArgAsInt(args, 1, 0);
   const int sound = sfxinfo ? sfxinfo->dehackednum : 0;
   S_StartSound(fullVol ? nullptr : mo, sound);
}

//
// Random state jump for weapons.
//
// args[0] -- state index to jump to
// args[1] -- chance out of 256 to perform the jump. 0 (or below) never jumps, 256 (or higher) always jumps
//
void A_WeaponJump(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   player_t  *player = actionargs->actor->player;
   pspdef_t  *pspr   = actionargs->pspr;
   int state;

   if(!mbf21_demo || !player || !pspr)
      return;

   if(state = E_ArgAsStateNumNI(args, 0, player); state < 0)
      return;

   if(P_Random(pr_mbf21) < E_ArgAsInt(args, 1, 0))
      P_SetPspritePtr(*player, pspr, state);
}

//
// Subtracts ammo from the currently-selected weapon's ammo pool.
//
// args[0] -- amount of ammo to subtract. if zero, will default to the current weapon's ammopershot value
//
void A_ConsumeAmmo(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   player_t  *player = actionargs->actor->player;
   int amount;

   if(!player)
      return;

   amount = E_ArgAsInt(args, 0, 0);
   if(amount == 0)
   {
      if(player->readyweapon->flags & WPF_DISABLEAPS)
         amount = 1; // default to 1 if ammo-per-shot was never set
      else
         amount = INT_MIN;
   }

   P_SubtractAmmoAmount(player, amount);
}

//
// Jumps to state if ammo is below amount.
//
// args[0] -- state index to jump to.
// args[1] -- amount of ammo to check. if zero, will default to the current weapon's ammopershot value
//
void A_CheckAmmo(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   player_t  *player = actionargs->actor->player;
   pspdef_t  *pspr   = actionargs->pspr;
   int state;
   int checkamount;
   const weaponinfo_t *weapon;
   int                 ammoamount;
   const itemeffect_t *ammotype;

   if(!mbf21_demo || !player || !pspr)
      return;

   weapon = player->readyweapon;

   if(state = E_ArgAsStateNumNI(args, 0, player); state < 0)
      return;

   if(player->attackdown & AT_SECONDARY)
   {
      checkamount = E_ArgAsInt(args, 1, weapon->ammopershot_alt);
      ammotype = weapon->ammo_alt;
   }
   else
   {
      checkamount = E_ArgAsInt(args, 1, weapon->ammopershot);
      ammotype = weapon->ammo;
   }

   if(!ammotype) // no-ammo weapon?
      return;

   ammoamount = E_GetItemOwnedAmount(*player, ammotype);

   if(ammoamount < checkamount)
      P_SetPspritePtr(*player, pspr, state);
}

//
// Jumps to state if the fire button is currently being pressed and the weapon has enough ammo to fire.
//
// args[0] -- state index to jump to
// args[1] -- if nonzero, skip the ammo check
//
void A_RefireTo(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   player_t  *player = actionargs->actor->player;
   pspdef_t  *pspr   = actionargs->pspr;
   int  state;
   bool noammocheck;

   if(!mbf21_demo || !player || !pspr)
      return;

   if(state = E_ArgAsStateNumNI(args, 0, player); state < 0)
      return;

   noammocheck = !!E_ArgAsInt(args, 1, 0);

   if(player->pendingweapon == nullptr && player->health)
   {
      if((player->cmd.buttons & BT_ATTACK) && !(player->attackdown & AT_SECONDARY))
      {
         if(noammocheck || P_WeaponHasAmmo(*player, player->readyweapon))
            P_SetPspritePtr(*player, pspr, state);
      }
      else if((player->cmd.buttons & BTN_ATTACK_ALT) && !(player->attackdown & AT_PRIMARY))
      {
         if(noammocheck || P_WeaponHasAmmoAlt(*player, player->readyweapon))
            P_SetPspritePtr(*player, pspr, state);
      }
   }
}

//
// Generic weapon muzzle flash.
//
// args[0] -- state index to set the flash psprite to
// args[1] -- if nonzero, do not change the 3rd-person player sprite to the player muzzleflash state
//
void A_GunFlashTo(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   player_t  *player = actionargs->actor->player;
   pspdef_t  *pspr   = actionargs->pspr;
   int  state;

   if(!mbf21_demo || !player || !pspr)
      return;

   if(state = E_ArgAsStateNumNI(args, 0, player); state < 0)
      return;

   if(!E_ArgAsInt(args, 1, 0))
      P_SetMobjState(actionargs->actor, player->pclass->altattack);

   P_SetPsprite(*player, ps_flash, state);
}

//
// Alerts monsters within sound-travel distance of the player's presence.
// Useful for weapons with the WPF_SILENT flag set.
//
void A_WeaponAlert(actionargs_t *actionargs)
{
   Mobj      *mo     = actionargs->actor;
   player_t  *player = mo->player;

   if(!mbf21_demo || !player)
      return;

   P_NoiseAlert(mo, mo);
}

// EOF

