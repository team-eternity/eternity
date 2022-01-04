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
#include "d_mod.h"
#include "doomstat.h"
#include "e_args.h"
#include "m_vector.h"
#include "p_enemy.h"
#include "p_inter.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_portalcross.h"
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
   if(!mbf21_temp || thingtype == -1)
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
   if(!mbf21_temp || !actor->target || thingtype == -1)
      return;

   angle       = E_ArgAsFixed(args, 1, 0);
   pitch       = E_ArgAsFixed(args, 2, 0);
   spawnofs_xy = E_ArgAsFixed(args, 3, 0);
   spawnofs_z  = E_ArgAsFixed(args, 4, 0);

   A_FaceTarget(actionargs);
   mo = P_SpawnMissile(actor, actor->target, thingtype, DEFAULTMISSILEZ);
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

   if(!mbf21_temp || !actor->target)
      return;

   hspread    = E_ArgAsFixed(args, 0, 0);
   vspread    = E_ArgAsFixed(args, 1, 0);
   numbullets = E_ArgAsInt(args, 2, 1);
   damagebase = E_ArgAsInt(args, 3, 3);
   damagemod  = E_ArgAsInt(args, 4, 5);

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

   if(!mbf21_temp || !actor->target)
      return;

   damagebase = E_ArgAsInt(args, 0, 3);
   damagemod  = E_ArgAsInt(args, 1, 8);
   hitsound   = E_ArgAsSound(args, 2);
   range      = E_ArgAsFixed(args, 3, actor->info->meleerange);

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

   if(!mbf21_temp || !actor->state)
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

   if(!mbf21_temp || !actor->target)
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

   if(!mbf21_temp)
      return;

   state   = E_ArgAsStateNum(args, 0, actor);
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
   // TODO
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
   // TODO
}

//
// Clears the calling actor's tracer (seek target) field.
//
void A_ClearTracer(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;

   if(!mbf21_temp)
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

   if(!mbf21_temp)
      return;

   state  = E_ArgAsStateNum(args, 0, actor);
   health = E_ArgAsInt(args, 1, 0);

   if(actor->health < health)
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
   // TODO
}

//
// Jumps to a state if caller's target is closer than the specified distance.
//
// args[0] -- state to jump to
// args[1] -- distance threshold, in map units
//
void A_JumpIfTargetCloser(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   Mobj      *actor  = actionargs->actor;
   Mobj      *target = actor->target;
   int     state;
   fixed_t distance;

   if(!mbf21_temp || !target)
      return;

   state    = E_ArgAsStateNum(args, 0, actor);
   distance = E_ArgAsFixed(args, 1, 0);

   if(distance > P_AproxDistance(actor->x - getTargetX(actor), actor->y - getTargetY(actor)))
      P_SetMobjState(actor, state);
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
   // TODO
}

//
// Jumps to a state if caller's tracer (seek target) is closer than the specified distance.
//
// args[0] -- state to jump to
// args[1] -- distance threshold, in map units
//
void A_JumpIfTracerCloser(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   Mobj      *actor  = actionargs->actor;
   Mobj      *tracer = actor->tracer;
   int     state;
   fixed_t distance;

   if(!mbf21_temp || !tracer)
      return;

   state    = E_ArgAsStateNum(args, 0, actor);
   distance = E_ArgAsFixed(args, 1, 0);

   const fixed_t dx = actor->x - getThingX(actor, actor->tracer);
   const fixed_t dy = actor->y - getThingY(actor, actor->tracer);
   if(distance > P_AproxDistance(dx, dy))
      P_SetMobjState(actor, state);

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

   if(!mbf21_temp || !actor)
      return;

   state      = E_ArgAsStateNum(args, 0, actor);
   flags      = E_ArgAsInt(args, 0, 0);
   mbf21flags = E_ArgAsMBF21ThingFlags(args, 1);

   if((actor->flags & flags)                      == flags &&
      (actor->flags & mbf21flags[DEHFLAGS_MODE2]) == mbf21flags[DEHFLAGS_MODE2] &&
      (actor->flags & mbf21flags[DEHFLAGS_MODE3]) == mbf21flags[DEHFLAGS_MODE3] &&
      (actor->flags & mbf21flags[DEHFLAGS_MODE4]) == mbf21flags[DEHFLAGS_MODE4] &&
      (actor->flags & mbf21flags[DEHFLAGS_MODE5]) == mbf21flags[DEHFLAGS_MODE5])
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

   if(!mbf21_temp || !actor)
      return;

   flags      = E_ArgAsInt(args, 0, 0);
   mbf21flags = E_ArgAsMBF21ThingFlags(args, 1);

   actor->flags  |= flags;
   actor->flags2 |= mbf21flags[DEHFLAGS_MODE2];
   actor->flags3 |= mbf21flags[DEHFLAGS_MODE3];
   actor->flags4 |= mbf21flags[DEHFLAGS_MODE4];
   actor->flags5 |= mbf21flags[DEHFLAGS_MODE5];
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

   if(!mbf21_temp || !actor)
      return;

   flags      = E_ArgAsInt(args, 0, 0);
   mbf21flags = E_ArgAsMBF21ThingFlags(args, 1);

   actor->flags  &= ~flags;
   actor->flags2 &= ~mbf21flags[DEHFLAGS_MODE2];
   actor->flags3 &= ~mbf21flags[DEHFLAGS_MODE3];
   actor->flags4 &= ~mbf21flags[DEHFLAGS_MODE4];
   actor->flags5 &= ~mbf21flags[DEHFLAGS_MODE5];
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
   // TODO
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
   // TODO
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
   // TODO
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

   if(!mbf21_temp || !player)
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
   // TODO
}

//
// Subtracts ammo from the currently-selected weapon's ammo pool.
//
// args[0] -- amount of ammo to subtract. if zero, will default to the current weapon's ammopershot value
//
void A_ConsumeAmmo(actionargs_t *actionargs)
{
   // TODO
}

//
// Jumps to state if ammo is below amount.
//
// args[0] -- state index to jump to.
// args[1] -- amount of ammo to check. if zero, will default to the current weapon's ammopershot value
//
void A_CheckAmmo(actionargs_t *actionargs)
{
   // TODO
}

//
// Jumps to state if the fire button is currently being pressed and the weapon has enough ammo to fire.
//
// args[0] -- state index to jump to
// args[1] -- if nonzero, skip the ammo check
//
void A_RefireTo(actionargs_t *actionargs)
{
   // TODO
}

//
// Generic weapon muzzle flash.
//
// args[0] -- state index to set the flash psprite to
// args[1] -- if nonzero, do not change the 3rd-person player sprite to the player muzzleflash state
//
void A_GunFlashTo(actionargs_t *actionargs)
{
   // TODO
}

//
// Alerts monsters within sound-travel distance of the player's presence.
// Useful for weapons with the WPF_SILENT flag set.
//
void A_WeaponAlert(actionargs_t *actionargs)
{
   arglist_t *args   = actionargs->args;
   Mobj      *mo     = actionargs->actor;
   player_t  *player = mo->player;

   if(!mbf21_temp || !player)
      return;

   P_NoiseAlert(mo, mo);
}

// EOF

