//
// The Eternity Engine
// Copyright(C) 2017 James Haley, Max Waine, et al.
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//----------------------------------------------------------------------------
//
// Purpose: Doom weapon action functions
// Authors: Max Waine
//

#include "z_zone.h"

#include "hal/i_gamepads.h"

#include "a_args.h"
#include "d_player.h"
#include "doomstat.h"
#include "d_gi.h"
#include "d_mod.h"
#include "e_inventory.h"
#include "e_player.h"
#include "e_states.h"
#include "e_things.h"
#include "e_weapons.h"
#include "m_random.h"
#include "p_mobj.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_maputl.h"
#include "p_skin.h"
#include "p_user.h"
#include "s_sound.h"

#include "p_map.h"

#define BFGCELLS bfgcells        /* Ty 03/09/98 externalized in p_inter.c */

//
// A_Punch
//
void A_Punch(actionargs_t *actionargs)
{
   angle_t angle;
   fixed_t slope;
   int damage = (P_Random(pr_punch) % 10 + 1) << 1;
   Mobj     *mo     = actionargs->actor;
   player_t *player = mo->player;

   if(!player)
      return;
   
   if(player->powers[pw_strength])
      damage *= 10;
   
   angle = mo->angle;

   // haleyjd 08/05/04: use new function
   angle += P_SubRandom(pr_punchangle) << 18;

   slope = P_DoAutoAim(mo, angle, MELEERANGE);

   P_LineAttack(mo, angle, MELEERANGE, slope, damage);

   if(!clip.linetarget)
      return;

   P_WeaponSound(mo, GameModeInfo->playerSounds[sk_punch]);

   // turn to face target
   mo->angle = P_PointToAngle(mo->x, mo->y, clip.linetarget->x, clip.linetarget->y);
}

//
// A_Saw
//
void A_Saw(actionargs_t *actionargs)
{
   fixed_t slope;
   int     damage = 2 * (P_Random(pr_saw) % 10 + 1);
   Mobj   *mo    = actionargs->actor;
   angle_t angle = mo->angle;
   
   // haleyjd 08/05/04: use new function
   angle += P_SubRandom(pr_saw) << 18;

   // Use meleerange + 1 so that the puff doesn't skip the flash
   slope = P_DoAutoAim(mo, angle, MELEERANGE + 1);
   P_LineAttack(mo, angle, MELEERANGE+1, slope, damage);
   
   I_StartHaptic(HALHapticInterface::EFFECT_CONSTANT, 4, 108);   
   
   if(!clip.linetarget)
   {
      P_WeaponSound(mo, sfx_sawful);
      return;
   }

   P_WeaponSound(mo, sfx_sawhit);
   I_StartHaptic(HALHapticInterface::EFFECT_RUMBLE, 5, 108);
   
   // turn to face target
   angle = P_PointToAngle(mo->x, mo->y, clip.linetarget->x, clip.linetarget->y);

   if(angle - mo->angle > ANG180)
   {
      if(angle - mo->angle < -ANG90/20)
         mo->angle = angle + ANG90/21;
      else
         mo->angle -= ANG90/20;
   }
   else
   {
      if(angle - mo->angle > ANG90/20)
         mo->angle = angle - ANG90/21;
      else
         mo->angle += ANG90/20;
   }

   mo->flags |= MF_JUSTATTACKED;
}

//
// A_FireMissile
//
void A_FireMissile(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;

   if(!player)
      return;

   P_SubtractAmmo(player, 1);

   P_SpawnPlayerMissile(player->mo, E_SafeThingType(MT_ROCKET));
}

//
// A_FireBFG
//

#define BFGBOUNCE 16

void A_FireBFG(actionargs_t *actionargs)
{
   Mobj *mo;
   player_t *player = actionargs->actor->player;

   if(!player)
      return;

   P_SubtractAmmo(player, BFGCELLS);
   
   mo = P_SpawnPlayerMissile(actionargs->actor, E_SafeThingType(MT_BFG));
   mo->extradata.bfgcount = BFGBOUNCE;   // for bouncing bfg - redundant
}

//
// A_FireOldBFG
//
// This function emulates Doom's Pre-Beta BFG
// By Lee Killough 6/6/98, 7/11/98, 7/19/98, 8/20/98
//
// This code may not be used in other mods without appropriate credit given.
// Code leeches will be telefragged.
//
void A_FireOldBFG(actionargs_t *actionargs)
{
   Mobj *mo  = actionargs->actor;
   int type1 = E_SafeThingType(MT_PLASMA1);
   int type2 = E_SafeThingType(MT_PLASMA2);
   int type;
   player_t *player = mo->player;
   weaponinfo_t *wp;

   if(!player)
      return;

   // FIXME: This is ugly and bad, but I can't figure out how to
   // dynamically have the player switch different versions of the BFG
   wp = player->readyweapon->dehnum == wp_bfg && bfgtype == bfg_classic ?
                                       E_WeaponForName("OldBFG") : player->readyweapon;

   if(!wp)
      return;

   type = type1;

   // PCLASS_FIXME: second attack state
   
   // sf: make sure the player is in firing frame, or it looks silly
   if(demo_version > 300)
      P_SetMobjState(mo, player->pclass->altattack);
   
   // WEAPON_FIXME: recoil for classic BFG

   if(weapon_recoil && !(mo->flags & MF_NOCLIP))
      P_Thrust(player, ANG180 + mo->angle, 0, 512 * wp->recoil);

   auto weapon   = player->readyweapon;
   auto ammoType = weapon->ammo;   
   if(ammoType && !(player->cheats & CF_INFAMMO))
      E_RemoveInventoryItem(player, ammoType, wp->ammopershot);

   if(LevelInfo.useFullBright) // haleyjd
      player->extralight = 2;

   do
   {
      Mobj *th;
      angle_t an = mo->angle;
      angle_t an1 = ((P_Random(pr_bfg) & 127) - 64) * (ANG90 / 768) + an;
      angle_t an2 = ((P_Random(pr_bfg) & 127) - 64) * (ANG90 / 640) + ANG90;
      extern int autoaim;
      fixed_t slope;

      if(autoaim)
      {
         // killough 8/2/98: make autoaiming prefer enemies
         int mask = MF_FRIEND;
         do
         {
            slope = P_AimLineAttack(mo, an, 16*64*FRACUNIT, mask);
            if(!clip.linetarget)
               slope = P_AimLineAttack(mo, an += 1<<26, 16*64*FRACUNIT, mask);
            if(!clip.linetarget)
               slope = P_AimLineAttack(mo, an -= 2<<26, 16*64*FRACUNIT, mask);
            if(!clip.linetarget) // sf: looking up/down
            {
               slope = finetangent[(ANG90-player->pitch)>>ANGLETOFINESHIFT];
               an = mo->angle;
            }
         }
         while(mask && (mask=0, !clip.linetarget));     // killough 8/2/98
         an1 += an - mo->angle;
         // sf: despite killough's infinite wisdom.. even
         // he is prone to mistakes. seems negative numbers
         // won't survive a bitshift!
         if(slope < 0 && demo_version >= 303)
            an2 -= tantoangle[-slope >> DBITS];
         else
            an2 += tantoangle[slope >> DBITS];
      }
      else
      {
         slope = finetangent[(ANG90-player->pitch)>>ANGLETOFINESHIFT];
         if(slope < 0 && demo_version >= 303)
            an2 -= tantoangle[-slope >> DBITS];
         else
            an2 += tantoangle[slope >> DBITS];
      }

      th = P_SpawnMobj(mo->x, mo->y,
                       mo->z + 62*FRACUNIT - player->psprites[ps_weapon].sy,
                       type);

      P_SetTarget<Mobj>(&th->target, mo);
      th->angle = an1;
      th->momx = finecosine[an1>>ANGLETOFINESHIFT] * 25;
      th->momy = finesine[an1>>ANGLETOFINESHIFT] * 25;
      th->momz = finetangent[an2>>ANGLETOFINESHIFT] * 25;
      P_CheckMissileSpawn(th);
   }
   while((type != type2) && (type = type2)); //killough: obfuscated!
}

//
// A_FirePlasma
//
void A_FirePlasma(actionargs_t *actionargs)
{
   Mobj     *mo     = actionargs->actor;
   player_t *player = mo->player;

   if(!player)
      return;

   P_SubtractAmmo(player, 1);

   A_FireSomething(player, P_Random(pr_plasma) & 1);
   
   // sf: removed beta
   P_SpawnPlayerMissile(mo, E_SafeThingType(MT_PLASMA));
}

//
// P_GunShot
//
void P_GunShot(Mobj *mo, bool accurate)
{
   int damage = 5 * (P_Random(pr_gunshot) % 3 + 1);
   angle_t angle = mo->angle;
   
   if(!accurate)
      angle += P_SubRandom(pr_misfire) << 18;
   
   P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage);
}

//
// A_FirePistol
//
void A_FirePistol(actionargs_t *actionargs)
{
   Mobj     *mo     = actionargs->actor;
   player_t *player = mo->player;

   if(!player)
      return;

   P_WeaponSound(mo, sfx_pistol);
   
   // PCLASS_FIXME: attack state two

   P_SetMobjState(mo, player->pclass->altattack);

   P_SubtractAmmo(player, 1);
   
   A_FireSomething(player, 0); // phares
   P_BulletSlope(mo);
   P_GunShot(mo, !player->refire);
}

//
// A_FireShotgun
//
void A_FireShotgun(actionargs_t *actionargs)
{
   int i;
   Mobj     *mo     = actionargs->actor;
   player_t *player = mo->player;

   if(!player)
      return;

   P_WeaponSound(mo, sfx_shotgn);
   P_SetMobjState(mo, player->pclass->altattack);
   
   P_SubtractAmmo(player, 1);
   
   A_FireSomething(player, 0); // phares
   
   P_BulletSlope(mo);
   
   for(i = 0; i < 7; ++i)
      P_GunShot(mo, false);
}

//
// A_FireShotgun2
//
void A_FireShotgun2(actionargs_t *actionargs)
{
   int i;
   Mobj     *mo     = actionargs->actor;
   player_t *player = mo->player;

   if(!player)
      return;

   P_WeaponSound(mo, sfx_dshtgn);
   P_SetMobjState(mo, player->pclass->altattack);

   P_SubtractAmmo(player, 2);
   
   A_FireSomething(player, 0); // phares
   
   P_BulletSlope(mo);
   
   for(i = 0; i < 20; i++)
   {
      int damage = 5 * (P_Random(pr_shotgun) % 3 + 1);
      angle_t angle = mo->angle;

      angle += P_SubRandom(pr_shotgun) << 19;
      
      P_LineAttack(mo, angle, MISSILERANGE, bulletslope +
                   (P_SubRandom(pr_shotgun) << 5), damage);
   }
}

// MaxW: 2018/01/04: moved all the Doom codepointers here!
// haleyjd 04/05/07: moved all SSG codepointers here

void A_OpenShotgun2(actionargs_t *actionargs)
{
   P_WeaponSound(actionargs->actor, sfx_dbopn);
}

void A_LoadShotgun2(actionargs_t *actionargs)
{
   P_WeaponSound(actionargs->actor, sfx_dbload);
}

void A_CloseShotgun2(actionargs_t *actionargs)
{
   P_WeaponSound(actionargs->actor, sfx_dbcls);
   A_ReFire(actionargs);
}

//
// A_FireCGun
//
void A_FireCGun(actionargs_t *actionargs)
{
   Mobj     *mo = actionargs->actor;
   player_t *player;
   pspdef_t *psp;

   if(!(player = mo->player))
      return;

   if(!(psp = actionargs->pspr))
      return;

   P_WeaponSound(mo, sfx_chgun);

   if(!P_WeaponHasAmmo(player, player->readyweapon))
      return;
   
   P_SetMobjState(mo, player->pclass->altattack);
   
   P_SubtractAmmo(player, 1);

   // haleyjd 08/28/03: this is not safe for DeHackEd/EDF, so it
   // needs some modification to be safer
   // haleyjd WEAPON_FIXME: hackish and dangerous for EDF, needs fix.
   if(demo_version < 331 || 
      (psp->state->index >= E_StateNumForDEHNum(S_CHAIN1) &&
       psp->state->index < E_StateNumForDEHNum(S_CHAIN3)))
   {      
      // phares
      A_FireSomething(player, psp->state->index - states[E_SafeState(S_CHAIN1)]->index);
   }
   else
      A_FireSomething(player, 0); // new default behavior
   
   P_BulletSlope(mo);
   
   P_GunShot(mo, !player->refire);
}

void A_BouncingBFG(actionargs_t *actionargs);
void A_BFG11KHit(actionargs_t *actionargs);
void A_BFGBurst(actionargs_t *actionargs); // haleyjd

//
// A_BFGSpray
//
// Spawn a BFG explosion on every monster in view
//
void A_BFGSpray(actionargs_t *actionargs)
{
   // WEAPON_FIXME: BFG type stuff
   switch(bfgtype)
   {
   case bfg_11k:
      A_BFG11KHit(actionargs);
      return;
   case bfg_bouncing:
      A_BouncingBFG(actionargs);
      return;
   case bfg_burst:
      A_BFGBurst(actionargs);
      return;
   default:
      break;
   }

   Mobj *mo = actionargs->actor;
   
   for(int i = 0; i < 40; i++)  // offset angles from its attack angle
   {
      int j, damage;
      angle_t an = mo->angle - ANG90/2 + ANG90/40*i;
      
      // mo->target is the originator (player) of the missile
      
      // killough 8/2/98: make autoaiming prefer enemies
      if(demo_version < 203 || 
         (P_AimLineAttack(mo->target, an, 16*64*FRACUNIT, MF_FRIEND), 
         !clip.linetarget))
         P_AimLineAttack(mo->target, an, 16*64*FRACUNIT, 0);
      
      if(!clip.linetarget)
         continue;
      
      P_SpawnMobj(clip.linetarget->x, clip.linetarget->y,
                  clip.linetarget->z + (clip.linetarget->height>>2), 
                  E_SafeThingType(MT_EXTRABFG));
      
      for(damage = j = 0; j < 15; j++)
         damage += (P_Random(pr_bfg)&7) + 1;
      
      P_DamageMobj(clip.linetarget, mo->target, mo->target, damage,
                   MOD_BFG_SPLASH);
   }
}

//
// A_BouncingBFG
//
// haleyjd: The bouncing BFG from SMMU, but fixed to work better.
//
void A_BouncingBFG(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;
   Mobj *newmo;
   
   if(!mo->extradata.bfgcount)
      return;
   
   for(int i = 0 ; i < 40 ; i++)  // offset angles from its attack angle
   {
      angle_t an2, an = (ANG360/40)*i;
      int dist;
      
      P_AimLineAttack(mo, an, 16*64*FRACUNIT,0);
      
      // haleyjd: track last target with mo->tracer, don't fire
      // at same target more than one time in a row
      if(!clip.linetarget || (mo->tracer && mo->tracer == clip.linetarget))
         continue;
      if(an/6 == mo->angle/6) continue;
      
      // don't aim for shooter, or for friends of shooter
      if(clip.linetarget == mo->target ||
         (clip.linetarget->flags & mo->target->flags & MF_FRIEND))
         continue; 
      
      P_SpawnMobj(clip.linetarget->x, clip.linetarget->y,
                  clip.linetarget->z + (clip.linetarget->height>>2),
                  E_SafeThingType(MT_EXTRABFG));

      // spawn new bfg      
      // haleyjd: can't use P_SpawnMissile here
      newmo = P_SpawnMobj(mo->x, mo->y, mo->z, E_SafeThingType(MT_BFG));
      S_StartSound(newmo, newmo->info->seesound);
      P_SetTarget<Mobj>(&newmo->target, mo->target); // pass on the player

      // ioanch 20151230: make portal aware
      fixed_t ltx = getThingX(newmo, clip.linetarget);
      fixed_t lty = getThingY(newmo, clip.linetarget);
      fixed_t ltz = getThingZ(newmo, clip.linetarget);

      an2 = P_PointToAngle(newmo->x, newmo->y, ltx, lty);
      newmo->angle = an2;
      
      an2 >>= ANGLETOFINESHIFT;
      newmo->momx = FixedMul(newmo->info->speed, finecosine[an2]);
      newmo->momy = FixedMul(newmo->info->speed, finesine[an2]);

      dist = P_AproxDistance(ltx - newmo->x, lty - newmo->y);
      dist = dist / newmo->info->speed;
      
      if(dist < 1)
         dist = 1;
      
      newmo->momz = (ltz + (clip.linetarget->height>>1) - newmo->z) / dist;

      newmo->extradata.bfgcount = mo->extradata.bfgcount - 1; // count down
      P_SetTarget<Mobj>(&newmo->tracer, clip.linetarget); // haleyjd: track target

      P_CheckMissileSpawn(newmo);

      mo->remove(); // remove the old one

      break; //only spawn 1
   }
}

//
// A_BFG11KHit
//
// Explosion pointer for SMMU BFG11k.
//
void A_BFG11KHit(actionargs_t *actionargs)
{
   int i = 0;
   int j, damage;
   int origdist;
   Mobj *mo = actionargs->actor;

   if(!mo->target)
      return;
   
   // check the originator and hurt them if too close
   origdist = P_AproxDistance(mo->x - getTargetX(mo), mo->y - getTargetY(mo));
   
   if(origdist < 96*FRACUNIT)
   {
      // decide on damage
      // damage decreases with distance
      for(damage = j = 0; j < 48 - (origdist/(FRACUNIT*2)); j++)
         damage += (P_Random(pr_bfg)&7) + 1;
      
      // flash
      P_SpawnMobj(mo->target->x, mo->target->y,
                  mo->target->z + (mo->target->height>>2), 
                  E_SafeThingType(MT_EXTRABFG));
      
      P_DamageMobj(mo->target, mo, mo->target, damage, 
                   MOD_BFG11K_SPLASH);
   }
   
   // now check everyone else
   
   for(i = 0 ; i < 40 ; i++)  // offset angles from its attack angle
   {
      angle_t an = (ANG360/40)*i;
      
      // mo->target is the originator (player) of the missile
      
      P_AimLineAttack(mo, an, 16*64*FRACUNIT,0);
      
      if(!clip.linetarget) continue;
      if(clip.linetarget == mo->target)
         continue;
      
      // decide on damage
      for(damage = j = 0; j < 20; j++)
         damage += (P_Random(pr_bfg)&7) + 1;
      
      // dumbass flash
      P_SpawnMobj(clip.linetarget->x, clip.linetarget->y,
                  clip.linetarget->z + (clip.linetarget->height>>2), 
                  E_SafeThingType(MT_EXTRABFG));
      
      P_DamageMobj(clip.linetarget, mo->target, mo->target, damage,
                   MOD_BFG_SPLASH);
   }
}

//
// A_BFGBurst
//
// This implements the projectile burst that was featured
// in Cephaler's unreleased MagDoom port. Much thanks to my
// long lost friend for giving me the source to his port back
// when he stopped working on it. This is a tribute to his bold
// spirit ^_^
//
void A_BFGBurst(actionargs_t *actionargs)
{
   int      a;
   angle_t  an = 0;
   Mobj    *mo = actionargs->actor;
   Mobj    *th;
   int      plasmaType = E_SafeThingType(MT_PLASMA3);

   for(a = 0; a < 40; a++)
   {
      an += ANG90 / 10;

      th = P_SpawnMobj(mo->x, mo->y, mo->z, plasmaType);
      P_SetTarget<Mobj>(&th->target, mo->target);

      th->angle = an;
      th->momx = finecosine[an >> ANGLETOFINESHIFT] << 4;
      th->momy = finesine[an >> ANGLETOFINESHIFT] << 4;
      th->momz = FRACUNIT * ((16 - P_Random(pr_bfg)) >> 5);

      P_CheckMissileSpawn(th);      
   }
}

//
// A_BFGsound
//
void A_BFGsound(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;

   P_WeaponSound(mo, sfx_bfg);

   if(mo->player == &players[consoleplayer])
      I_StartHaptic(HALHapticInterface::EFFECT_RAMPUP, 5, 850);
}


// EOF

