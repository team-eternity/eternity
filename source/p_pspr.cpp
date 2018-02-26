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
//      Weapon sprite animation, weapon objects.
//      Action functions for weapons.
//
// NETCODE_FIXME -- DEMO_FIXME -- WEAPON_FIXME: Weapon changing, prefs,
// etc. need overhaul for all of these. See comments in other modules
// for why and how it needs to be changed. zdoom uses separate events
// outside of ticcmd_t to indicate weapon changes now, and so will not
// face the issue of being limited to 16 weapons.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

// Need gamepad hal
#include "hal/i_gamepads.h"

#include "a_args.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_mod.h"
#include "e_inventory.h"
#include "g_game.h"
#include "m_random.h"
#include "p_enemy.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_pspr.h"
#include "p_skin.h"
#include "p_tick.h"
#include "p_user.h"
#include "r_main.h"
#include "r_segs.h"
#include "r_things.h"
#include "s_sound.h"
#include "sounds.h"
#include "e_states.h"
#include "e_things.h"
#include "e_sound.h"
#include "d_dehtbl.h"
#include "p_info.h"
#include "d_gi.h"
#include "e_lib.h"
#include "e_args.h"
#include "e_player.h"

#define LOWERSPEED   (FRACUNIT*6)
#define RAISESPEED   (FRACUNIT*6)
#define WEAPONBOTTOM (FRACUNIT*128)
#define WEAPONTOP    (FRACUNIT*32)

#define BFGCELLS bfgcells        /* Ty 03/09/98 externalized in p_inter.c */

// The following array holds the recoil values         // phares
// haleyjd 08/18/08: recoil moved into weaponinfo

//=============================================================================

//
// P_SetPspritePtr
//
// haleyjd 06/22/13: Set psprite state from pspdef_t
//
void P_SetPspritePtr(player_t *player, pspdef_t *psp, statenum_t stnum)
{
   // haleyjd 06/22/13: rewrote again to use actionargs structure
   do
   {
      state_t *state;
      
      if(!stnum)
      {
         // object removed itself
         psp->state = NULL;
         break;
      }

      // WEAPON_FIXME: pre-beta bfg must become an alternate weapon under EDF

      // killough 7/19/98: Pre-Beta BFG
      if(stnum == E_StateNumForDEHNum(S_BFG1) && bfgtype == bfg_classic)
         stnum = E_SafeState(S_OLDBFG1); // Skip to alternative weapon frame

      state = states[stnum];
      psp->state = state;
      psp->tics = state->tics;        // could be 0

      if(state->misc1)
      {
         // coordinate set
         psp->sx = state->misc1 << FRACBITS;
         psp->sy = state->misc2 << FRACBITS;
      }

      // Call action routine.
      // Modified handling.
      if(state->action)
      {
         actionargs_t action;

         action.actiontype = actionargs_t::WEAPONFRAME;
         action.actor      = player->mo;
         action.args       = state->args;
         action.pspr       = psp;

         state->action(&action);
         
         if(!psp->state)
            break;
      }
      stnum = psp->state->nextstate;
   }
   while(!psp->tics);   // an initial state of 0 could cycle through
}

//
// P_SetPsprite
//
// haleyjd 03/31/06: Removed static.
//
void P_SetPsprite(player_t *player, int position, statenum_t stnum)
{
   P_SetPspritePtr(player, &player->psprites[position], stnum);
}

//
// P_BringUpWeapon
//
// Starts bringing the pending weapon up from the bottom of the screen.
// Uses player.
//
static void P_BringUpWeapon(player_t *player)
{
   statenum_t newstate;
   
   if(player->pendingweapon == wp_nochange)
      player->pendingweapon = player->readyweapon;
   
   weaponinfo_t *pendingweapon;
   if((pendingweapon = P_GetPendingWeapon(player)))
   {
      // haleyjd 06/28/13: weapon upsound
      if(pendingweapon->upsound)
         S_StartSound(player->mo, pendingweapon->upsound);
  
      newstate = pendingweapon->upstate;
  
      player->pendingweapon = wp_nochange;
   
      // killough 12/98: prevent pistol from starting visibly at bottom of screen:
      player->psprites[ps_weapon].sy = demo_version >= 203 ? 
         WEAPONBOTTOM+FRACUNIT*2 : WEAPONBOTTOM;
   
      P_SetPsprite(player, ps_weapon, newstate);
   }
}

//
// P_WeaponHasAmmo
//
// haleyjd 08/05/13: Test if a player has ammo for a weapon
//
bool P_WeaponHasAmmo(player_t *player, weaponinfo_t *weapon)
{
   itemeffect_t *ammoType = weapon->ammo;

   // a weapon without an ammotype has infinite ammo
   if(!ammoType)
      return true;

   // otherwise, read the inventory slot
   return (E_GetItemOwnedAmount(player, ammoType) >= weapon->ammopershot);
}

//
// P_NextWeapon
//
// haleyjd 05/31/14: Rewritten to use next and previous in cycle pointers
// in weaponinfo_t, for friendliness with future dynamic weapons system.
//
int P_NextWeapon(player_t *player)
{
   weaponinfo_t *currentweapon = P_GetReadyWeapon(player);
   weaponinfo_t *newweapon     = currentweapon;
   bool          ammototry;

   do
   {
      newweapon = newweapon->nextInCycle;
      ammototry = P_WeaponHasAmmo(player, newweapon);
   }
   while((!player->weaponowned[newweapon->id] || !ammototry) &&
         newweapon != currentweapon);

   return newweapon != currentweapon ? newweapon->id : wp_nochange;
}

//
// P_PrevWeapon
//
// haleyjd 03/06/09: Like the above.
//
int P_PrevWeapon(player_t *player)
{
   weaponinfo_t *currentweapon = P_GetReadyWeapon(player);
   weaponinfo_t *newweapon     = currentweapon;
   bool          ammototry;

   do
   {
      newweapon = newweapon->prevInCycle;
      ammototry = P_WeaponHasAmmo(player, newweapon);
   }
   while((!player->weaponowned[newweapon->id] || !ammototry) &&
         newweapon != currentweapon);

   return newweapon != currentweapon ? newweapon->id : wp_nochange;
}

// The first set is where the weapon preferences from             // killough,
// default.cfg are stored. These values represent the keys used   // phares
// in DOOM2 to bring up the weapon, i.e. 6 = plasma gun. These    //    |
// are NOT the wp_* constants.                                    //    V

// WEAPON_FIXME: retained for now as a hard-coded array. When EDF weapons are 
// complete, the preference order of weapons will be a modder-determined factor,
// or in other words, part of the game logic like it originally was meant to be,
// and not an end-user setting. It's impractical to maintain N preference 
// settings for an ever-constantly changing number of weapons, and for multiple
// potential sets of weapons, one per player class.

static int weapon_preferences[NUMWEAPONS+1] =
{
   6, 9, 4, 3, 2, 8, 5, 7, 1, 0
};

//
// P_SwitchWeapon
//
// Checks current ammo levels and gives you the most preferred weapon with ammo.
// It will not pick the currently raised weapon. When called from P_CheckAmmo 
// this won't matter, because the raised weapon has no ammo anyway. When called
// from G_BuildTiccmd you want to toggle to a different weapon regardless.
//
weapontype_t P_SwitchWeapon(player_t *player)
{
   int *prefer = weapon_preferences; // killough 3/22/98
   weapontype_t currentweapon = player->readyweapon;
   weapontype_t newweapon = currentweapon;
   int i = NUMWEAPONS + 1;   // killough 5/2/98   

   // killough 2/8/98: follow preferences and fix BFG/SSG bugs

   // haleyjd WEAPON_FIXME: makes assumptions about ammo per shot
   // haleyjd WEAPON_FIXME: makes assumptions about ammotypes used by weapons!
   // haleyjd WEAPON_FIXME: shareware-only must become EDF weapon property
   // haleyjd WEAPON_FIXME: must support arbitrary weapons
   // haleyjd WEAPON_FIXME: chainsaw/fist issues

   // INVENTORY_FIXME: This is COMPLETE AND TOTAL bullshit. Hardcoded for now...
   // How in the sweet fuck am I supposed to generalize this mess, AND remain
   // backwardly compatible with DeHackEd bullshit??
   int clips   = E_GetItemOwnedAmountName(player, "AmmoClip"); 
   int shells  = E_GetItemOwnedAmountName(player, "AmmoShell");
   int cells   = E_GetItemOwnedAmountName(player, "AmmoCell");
   int rockets = E_GetItemOwnedAmountName(player, "AmmoMissile");

   do
   {
      switch(*prefer++)
      {
      case 1:
         if(!player->powers[pw_strength])  // allow chainsaw override
            break;
      case 0:
         newweapon = wp_fist;
         break;
      case 2:
         if(clips)
            newweapon = wp_pistol;
         break;
      case 3:
         if(player->weaponowned[wp_shotgun] && shells)
            newweapon = wp_shotgun;
         break;
      case 4:
         if(player->weaponowned[wp_chaingun] && clips)
            newweapon = wp_chaingun;
         break;
      case 5:
         if(player->weaponowned[wp_missile] && rockets)
            newweapon = wp_missile;
         break;
      case 6:
         if(player->weaponowned[wp_plasma] && cells && GameModeInfo->id != shareware)
            newweapon = wp_plasma;
         break;
      case 7:
         if(player->weaponowned[wp_bfg] && GameModeInfo->id != shareware &&
            cells >= (demo_compatibility ? 41 : 40))
            newweapon = wp_bfg;
         break;
      case 8:
         if(player->weaponowned[wp_chainsaw])
            newweapon = wp_chainsaw;
         break;
      case 9:
         if(player->weaponowned[wp_supershotgun] && 
            enable_ssg &&
            shells >= (demo_compatibility ? 3 : 2))
            newweapon = wp_supershotgun;
         break;
      }
   }
   while(newweapon == currentweapon && --i);        // killough 5/2/98

   return newweapon;
}

//
// P_WeaponPreferred
//
// killough 5/2/98: whether consoleplayer prefers weapon w1 over weapon w2.
//
int P_WeaponPreferred(int w1, int w2)
{
  return
    (weapon_preferences[0] != ++w2 && (weapon_preferences[0] == ++w1 ||
    (weapon_preferences[1] !=   w2 && (weapon_preferences[1] ==   w1 ||
    (weapon_preferences[2] !=   w2 && (weapon_preferences[2] ==   w1 ||
    (weapon_preferences[3] !=   w2 && (weapon_preferences[3] ==   w1 ||
    (weapon_preferences[4] !=   w2 && (weapon_preferences[4] ==   w1 ||
    (weapon_preferences[5] !=   w2 && (weapon_preferences[5] ==   w1 ||
    (weapon_preferences[6] !=   w2 && (weapon_preferences[6] ==   w1 ||
    (weapon_preferences[7] !=   w2 && (weapon_preferences[7] ==   w1
   ))))))))))))))));
}

//
// P_CheckAmmo
//
// Returns true if there is enough ammo to shoot.
// If not, selects the next weapon to use.
// (only in demo_compatibility mode -- killough 3/22/98)
//
bool P_CheckAmmo(player_t *player)
{
   // Return if current ammunition sufficient.
   if(P_WeaponHasAmmo(player, P_GetReadyWeapon(player)))
      return true;
   
   // Out of ammo, pick a weapon to change to.
   //
   // killough 3/22/98: for old demos we do the switch here and now;
   // for Boom games we cannot do this, and have different player
   // preferences across demos or networks, so we have to use the
   // G_BuildTiccmd() interface instead of making the switch here.
   
   if(demo_compatibility)
   {
      player->pendingweapon = P_SwitchWeapon(player);      // phares
      // Now set appropriate weapon overlay.
      P_SetPsprite(player,ps_weapon,weaponinfo[player->readyweapon].downstate);
   }
     
   return false;
}

//
// P_SubtractAmmo
//
// haleyjd 03/08/07:
// Subtracts ammo from weapons in a uniform fashion. Unfortunately, this
// operation is complicated by compatibility issues and extra features.
//
void P_SubtractAmmo(player_t *player, int compat_amt)
{
   weaponinfo_t *weapon = P_GetReadyWeapon(player);

   if(player->cheats & CF_INFAMMO || !weapon->ammo)
      return;

   int amount = ((weapon->flags & WPF_ENABLEAPS) || compat_amt < 0) ? weapon->ammopershot : compat_amt;
   E_RemoveInventoryItem(player, weapon->ammo, amount);
}

int lastshottic; // killough 3/22/98

//
// P_FireWeapon.
//
static void P_FireWeapon(player_t *player)
{
   statenum_t newstate;
   weaponinfo_t *weapon;
   
   if(!P_CheckAmmo(player))
      return;

   weapon = P_GetReadyWeapon(player);

   P_SetMobjState(player->mo, player->mo->info->missilestate);
   newstate = weapon->atkstate;
   P_SetPsprite(player, ps_weapon, newstate);

   // haleyjd 04/06/03: silencer powerup
   // haleyjd 09/14/07: per-weapon silencer, always silent support
   if(!(weapon->flags & WPF_SILENCER && player->powers[pw_silencer]) &&
      !(weapon->flags & WPF_SILENT)) 
      P_NoiseAlert(player->mo, player->mo);

   lastshottic = gametic;                       // killough 3/22/98
}

//
// P_DropWeapon
//
// Player died, so put the weapon away.
//
void P_DropWeapon(player_t *player)
{
   P_SetPsprite(player, ps_weapon, weaponinfo[player->readyweapon].downstate);
}

//=============================================================================
//
// haleyjd 09/13/07
//
// New functions for dynamic weapons system
//

//
// P_GetReadyWeapon
//
// haleyjd 09/13/07: 
// Retrieves a pointer to the proper weaponinfo_t structure for the 
// readyweapon index stored in the player.
//
// WEAPON_TODO: Will need to change as system evolves.
// PCLASS_FIXME: weapons
//
weaponinfo_t *P_GetReadyWeapon(player_t *player)
{
   return &(weaponinfo[player->readyweapon]);
}

//
// P_GetPendingWeapon
//
// haleyjd 06/28/13:
// Retrieves a pointer to the proper weaponinfo_t structure for the
// pendingweapon index stored in the player.
//
// WEAPON_TODO: Will need to change as system evolves.
// PCLASS_FIXME: weapons
//
weaponinfo_t *P_GetPendingWeapon(player_t *player)
{
   return 
      (player->pendingweapon >= 0 && player->pendingweapon < NUMWEAPONS) ? 
       &(weaponinfo[player->pendingweapon]) : NULL;
}

//
// P_GetPlayerWeapon
//
// haleyjd 09/16/07:
// Gets weapon at given index for the given player.
// 
// WEAPON_TODO: must redirect through playerclass lookup
// PCLASS_FIXME: weapons
//
weaponinfo_t *P_GetPlayerWeapon(player_t *player, int index)
{
   // currently there is only one linear weaponinfo
   return &weaponinfo[index];
}

//
// P_WeaponSoundInfo
//
// Plays a sound originating from the player's weapon by sfxinfo_t *
//
static void P_WeaponSoundInfo(Mobj *mo, sfxinfo_t *sound)
{
   soundparams_t params;
   
   params.sfx = sound;
   params.setNormalDefaults(mo);

   if(mo->player && mo->player->powers[pw_silencer] &&
      P_GetReadyWeapon(mo->player)->flags & WPF_SILENCER)
      params.volumeScale = WEAPON_VOLUME_SILENCED;

   S_StartSfxInfo(params);
}

//
// P_WeaponSound
//
// Plays a sound originating from the player's weapon
//
static void P_WeaponSound(Mobj *mo, int sfx_id)
{
   int volume = 127;

   if(mo->player && mo->player->powers[pw_silencer] &&
      P_GetReadyWeapon(mo->player)->flags & WPF_SILENCER)
      volume = WEAPON_VOLUME_SILENCED;

   S_StartSoundAtVolume(mo, sfx_id, volume, ATTN_NORMAL, CHAN_AUTO);
}

//
// End dynamic weapon systems functions
//
//=============================================================================


//
// A_WeaponReady
//
// The player can fire the weapon
// or change to another weapon at this time.
// Follows after getting weapon up,
// or after previous attack/fire sequence.
//
void A_WeaponReady(actionargs_t *actionargs)
{
   Mobj     *mo = actionargs->actor;
   player_t *player;
   pspdef_t *psp;

   if(!(player = mo->player))
      return;

   if(!(psp = actionargs->pspr))
      return;

   // WEAPON_FIXME: chainsaw particulars (idle sound)

   // get out of attack state
   if(mo->state == states[mo->info->missilestate] || 
      mo->state == states[player->pclass->altattack])
   {
      P_SetMobjState(mo, mo->info->spawnstate);
   }

   if(player->readyweapon == wp_chainsaw && 
      psp->state == states[E_SafeState(S_SAW)])
   {
      S_StartSound(player->mo, sfx_sawidl);
      if(player == &players[consoleplayer])
         I_StartHaptic(HALHapticInterface::EFFECT_CONSTANT, 3, 108);
   }

   // check for change
   //  if player is dead, put the weapon away
   
   if(player->pendingweapon != wp_nochange || !player->health)
   {
      // change weapon (pending weapon should already be validated)
      statenum_t newstate = weaponinfo[player->readyweapon].downstate;
      P_SetPsprite(player, ps_weapon, newstate);
      return;
   }

   // check for fire
   //  the missile launcher and bfg do not auto fire
   
   if(player->cmd.buttons & BT_ATTACK)
   {
      if(!player->attackdown || 
         !(P_GetReadyWeapon(player)->flags & WPF_NOAUTOFIRE))
      {
         player->attackdown = true;
         P_FireWeapon(player);
         return;
      }
   }
   else
      player->attackdown = false;

   // bob the weapon based on movement speed
   int angle = (128*leveltime) & FINEMASK;
   psp->sx = FRACUNIT + FixedMul(player->bob, finecosine[angle]);
   angle &= FINEANGLES/2-1;
   psp->sy = WEAPONTOP + FixedMul(player->bob, finesine[angle]);
}

//
// A_ReFire
//
// The player can re-fire the weapon
// without lowering it entirely.
//
void A_ReFire(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;

   if(!player)
      return;

   // check for fire
   //  (if a weaponchange is pending, let it go through instead)
   
   if((player->cmd.buttons & BT_ATTACK)
      && player->pendingweapon == wp_nochange && player->health)
   {
      player->refire++;
      P_FireWeapon(player);
   }
   else
   {
      player->refire = 0;
      P_CheckAmmo(player);
   }
}

//
// A_CheckReload
//
// cph 2002/08/08 - In old Doom, P_CheckAmmo would start the weapon 
// lowering immediately. This was lost in Boom when the weapon 
// switching logic was rewritten. But we must tell Doom that we 
// don't need to complete the reload frames for the weapon here. 
// G_BuildTiccmd will set ->pendingweapon for us later on.
//
void A_CheckReload(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;

   if(!player)
      return;

   if(!P_CheckAmmo(player) && demo_version >= 331)
   {
      P_SetPsprite(player, ps_weapon,
                   weaponinfo[player->readyweapon].downstate);
   }
}

//
// A_Lower
//
// Lowers current weapon, and changes weapon at bottom.
//
void A_Lower(actionargs_t *actionargs)
{
   player_t *player;
   pspdef_t *psp;

   if(!(player = actionargs->actor->player))
      return;

   if(!(psp = actionargs->pspr))
      return;

   // WEAPON_FIXME: LOWERSPEED property of EDF weapons?
   psp->sy += LOWERSPEED;
   
   // Is already down.
   if(psp->sy < WEAPONBOTTOM)
      return;

   // Player is dead.
   if(player->playerstate == PST_DEAD)
   {
      psp->sy = WEAPONBOTTOM;
      return;      // don't bring weapon back up
   }

   // The old weapon has been lowered off the screen,
   // so change the weapon and start raising it
   
   if(!player->health)
   {      // Player is dead, so keep the weapon off screen.
      P_SetPsprite(player,  ps_weapon, NullStateNum);
      return;
   }

   // haleyjd 03/28/10: do not assume pendingweapon is valid
   if(player->pendingweapon < NUMWEAPONS)
      player->readyweapon = player->pendingweapon;

   P_BringUpWeapon(player);
}

//
// A_Raise
//
void A_Raise(actionargs_t *actionargs)
{
   statenum_t newstate;
   player_t *player;
   pspdef_t *psp;

   if(!(player = actionargs->actor->player))
      return;

   if(!(psp = actionargs->pspr))
      return;

   // WEAPON_FIXME: RAISESPEED property of EDF weapons?
   
   psp->sy -= RAISESPEED;
   
   if(psp->sy > WEAPONTOP)
      return;
   
   psp->sy = WEAPONTOP;
   
   // The weapon has been raised all the way,
   //  so change to the ready state.
   
   newstate = weaponinfo[player->readyweapon].readystate;
   
   P_SetPsprite(player, ps_weapon, newstate);
}

//
// P_WeaponRecoil
//
// haleyjd 12/11/12: Separated out of the below.
//
void P_WeaponRecoil(player_t *player)
{
   weaponinfo_t *readyweapon = P_GetReadyWeapon(player);

   // killough 3/27/98: prevent recoil in no-clipping mode
   if(!(player->mo->flags & MF_NOCLIP))
   {
      // haleyjd 08/18/08: added ALWAYSRECOIL weapon flag; recoil in weaponinfo
      if(readyweapon->flags & WPF_ALWAYSRECOIL ||
         (weapon_recoil && (demo_version >= 203 || !compatibility)))
      {
         P_Thrust(player, ANG180 + player->mo->angle, 0,
                  2048 * readyweapon->recoil); // phares
      }
   }

   // haleyjd 06/05/13: if weapon is flagged for it, do haptic recoil effect here.
   if(player == &players[consoleplayer] && (readyweapon->flags & WPF_HAPTICRECOIL))
      I_StartHaptic(HALHapticInterface::EFFECT_FIRE, readyweapon->hapticrecoil, readyweapon->haptictime);
}

// Weapons now recoil, amount depending on the weapon.              // phares
//                                                                  //   |
// The P_SetPsprite call in each of the weapon firing routines      //   V
// was moved here so the recoil could be synched with the
// muzzle flash, rather than the pressing of the trigger.
// The BFG delay caused this to be necessary.
//
// WEAPON_FIXME: "adder" parameter is not reliable for EDF editing
//
static void A_FireSomething(player_t* player, int adder)
{
   P_SetPsprite(player, ps_flash,
      weaponinfo[player->readyweapon].flashstate+adder);
   
   P_WeaponRecoil(player);
}

//
// A_GunFlash
//

void A_GunFlash(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;

   if(!player)
      return;

   P_SetMobjState(actionargs->actor, player->pclass->altattack);
   
   A_FireSomething(player, 0);                               // phares
}

//=============================================================================
//
// Autoaiming
//

//
// P_doAutoAim
//
// Does basic autoaiming logic and returns the slope value calculated by
// P_AimLineAttack.
//
static fixed_t P_doAutoAim(Mobj *mo, angle_t angle, fixed_t distance)
{
   if(demo_version >= 203)
   {
      // killough 8/2/98: make autoaiming prefer enemies
      fixed_t slope = P_AimLineAttack(mo, angle, distance, MF_FRIEND);

      // autoaiming is disabled?
      if(full_demo_version > make_full_version(340, 15) && !autoaim && mo->player)
         return P_PlayerPitchSlope(mo->player);

      if(clip.linetarget)
         return slope;
   }
   
   return P_AimLineAttack(mo, angle, distance, 0);
}

//=============================================================================
//
// WEAPON ATTACKS
//

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

   slope = P_doAutoAim(mo, angle, MELEERANGE);

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
   slope = P_doAutoAim(mo, angle, MELEERANGE + 1);
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

   if(!player)
      return;

   type = type1;

   // PCLASS_FIXME: second attack state
   
   // sf: make sure the player is in firing frame, or it looks silly
   if(demo_version > 300)
      P_SetMobjState(mo, player->pclass->altattack);
   
   // WEAPON_FIXME: recoil for classic BFG

   if(weapon_recoil && !(mo->flags & MF_NOCLIP))
      P_Thrust(player, ANG180 + mo->angle, 0,
               512*weaponinfo[wp_plasma].recoil);

   // WEAPON_FIXME: ammopershot for classic BFG
   auto weapon   = P_GetReadyWeapon(player);
   auto ammoType = weapon->ammo;   
   if(ammoType && !(player->cheats & CF_INFAMMO))
      E_RemoveInventoryItem(player, ammoType, 1);

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

static fixed_t bulletslope;

//
// P_BulletSlope
//
// Sets a slope so a near miss is at approximately
// the height of the intended target
//
void P_BulletSlope(Mobj *mo)
{
   angle_t an = mo->angle;    // see which target is to be aimed at
   
   // killough 8/2/98: make autoaiming prefer enemies
   int mask = demo_version < 203 ? 0 : MF_FRIEND;

   // haleyjd 08/09/11: allow autoaim disable
   if(full_demo_version > make_full_version(340, 15) && !autoaim && mo->player)
   {
      P_AimLineAttack(mo, an, 16*64*FRACUNIT, mask);
      bulletslope = P_PlayerPitchSlope(mo->player);
      return;
   }
   
   do
   {
      bulletslope = P_AimLineAttack(mo, an, 16*64*FRACUNIT, mask);
      if(!clip.linetarget)
         bulletslope = P_AimLineAttack(mo, an += 1<<26, 16*64*FRACUNIT, mask);
      if(!clip.linetarget)
         bulletslope = P_AimLineAttack(mo, an -= 2<<26, 16*64*FRACUNIT, mask);
   }
   while (mask && (mask=0, !clip.linetarget));  // killough 8/2/98
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

   if(!P_WeaponHasAmmo(player, P_GetReadyWeapon(player)))
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

void A_Light0(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;

   if(!mo->player)
      return;

   mo->player->extralight = 0;
}

void A_Light1(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;

   if(!mo->player)
      return;

   if(LevelInfo.useFullBright) // haleyjd
      mo->player->extralight = 1;
}

void A_Light2(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor;

   if(!mo->player)
      return;

   if(LevelInfo.useFullBright) // haleyjd
      mo->player->extralight = 2;
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

//
// P_SetupPsprites
//
// Called at start of level for each player.
//
void P_SetupPsprites(player_t *player)
{
   int i;
   
   // remove all psprites
   for(i = 0; i < NUMPSPRITES; ++i)
      player->psprites[i].state = NULL;
   
   // spawn the gun
   player->pendingweapon = player->readyweapon;
   P_BringUpWeapon(player);
}

//
// P_MovePsprites
//
// Called every tic by player thinking routine.
//
void P_MovePsprites(player_t *player)
{
   pspdef_t *psp = player->psprites;
   int i;

   // a null state means not active
   // drop tic count and possibly change state
   // a -1 tic count never changes
   
   for(i = 0; i < NUMPSPRITES; ++i, ++psp)
   {
      if(psp->state && psp->tics != -1 && !--psp->tics)
         P_SetPsprite(player, i, psp->state->nextstate);
   }
   
   player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
   player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
}

//===============================
//
// New Eternity Weapon Functions
//
//===============================

static const char *kwds_A_FireCustomBullets[] =
{
   "{DUMMY}",           // 0
   "always",            // 1
   "first",             // 2
   "never",             // 3
   "ssg",               // 4
   "monster",           // 5
};

static argkeywd_t fcbkwds =
{
   kwds_A_FireCustomBullets, 
   sizeof(kwds_A_FireCustomBullets) / sizeof(const char *)
};

//
// A_FireCustomBullets
//
// A parameterized player bullet weapon code pointer
// Parameters:
// args[0] : sound (dehacked num)
// args[1] : accuracy (always, first fire only, never, ssg, monster)
// args[2] : number of bullets to fire
// args[3] : damage factor of bullets
// args[4] : damage modulus of bullets
// args[5] : if not zero, set specific flash state; if < 0, don't change it.
//
void A_FireCustomBullets(actionargs_t *actionargs)
{
   Mobj      *mo   = actionargs->actor;
   arglist_t *args = actionargs->args;
   int i, accurate, numbullets, damage, dmgmod;
   int flashint, flashstate;
   sfxinfo_t *sfx;
   player_t *player;
   pspdef_t *psp;

   if(!(player = mo->player))
      return;

   if(!(psp = actionargs->pspr))
      return;

   sfx        = E_ArgAsSound(args, 0);
   accurate   = E_ArgAsKwd(args, 1, &fcbkwds, 0);
   numbullets = E_ArgAsInt(args, 2, 0);
   damage     = E_ArgAsInt(args, 3, 0);
   dmgmod     = E_ArgAsInt(args, 4, 0);
   
   flashint   = E_ArgAsInt(args, 5, 0);
   flashstate = E_ArgAsStateNum(args, 5, NULL);

   if(!accurate)
      accurate = 1;

   if(dmgmod < 1)
      dmgmod = 1;
   else if(dmgmod > 256)
      dmgmod = 256;

   P_WeaponSoundInfo(mo, sfx);

   P_SetMobjState(mo, player->pclass->altattack);

   // subtract ammo amount
   P_SubtractAmmo(player, -1);

   // have a flash state?
   if(flashint >= 0 && flashstate != NullStateNum)
      P_SetPsprite(player, ps_flash, flashstate);
   else if(flashint == 0) // zero means default behavior
      P_SetPsprite(player, ps_flash, P_GetReadyWeapon(player)->flashstate);

   P_WeaponRecoil(player);

   P_BulletSlope(mo);

   // loop on numbullets
   for(i = 0; i < numbullets; ++i)
   {
      int dmg = damage * (P_Random(pr_custombullets)%dmgmod + 1);
      angle_t angle = mo->angle;
      fixed_t slope = bulletslope; // haleyjd 01/03/07: bug fix
      
      if(accurate <= 3 || accurate == 5)
      {
         // if never accurate, monster accurate, or accurate only on 
         // refire and player is refiring, add some to the angle
         if(accurate == 3 || accurate == 5 || 
            (accurate == 2 && player->refire))
         {
            int aimshift = ((accurate == 5) ? 20 : 18);
            angle += P_SubRandom(pr_custommisfire) << aimshift;
         }

         P_LineAttack(mo, angle, MISSILERANGE, slope, dmg);
      }
      else if(accurate == 4) // ssg spread
      {
         angle += P_SubRandom(pr_custommisfire) << 19;
         slope += P_SubRandom(pr_custommisfire) << 5;

         P_LineAttack(mo, angle, MISSILERANGE, slope, dmg);
      }
   }
}

static const char *kwds_A_FirePlayerMissile[] =
{
   "normal",           //  0
   "homing",           //  1
};

static argkeywd_t seekkwds =
{
   kwds_A_FirePlayerMissile,
   sizeof(kwds_A_FirePlayerMissile) / sizeof(const char *)
};

//
// A_FirePlayerMissile
//
// A parameterized code pointer function for custom missiles
// Parameters:
// args[0] : thing type to shoot
// args[1] : whether or not to home at current autoaim target
//           (missile requires homing maintenance pointers, however)
//
void A_FirePlayerMissile(actionargs_t *actionargs)
{
   int thingnum;
   Mobj *actor = actionargs->actor;
   Mobj *mo;
   bool seek;
   player_t  *player;
   pspdef_t  *psp;
   arglist_t *args = actionargs->args;

   if(!(player = actor->player))
      return;

   if(!(psp = actionargs->pspr))
      return;

   thingnum = E_ArgAsThingNumG0(args, 0);
   seek     = !!E_ArgAsKwd(args, 1, &seekkwds, 0);

   // validate thingtype
   if(thingnum < 0/* || thingnum == -1*/)
      return;

   // decrement ammo if appropriate
   P_SubtractAmmo(player, -1);

   mo = P_SpawnPlayerMissile(actor, thingnum);

   if(mo && seek)
   {
      P_BulletSlope(actor);

      if(clip.linetarget)
         P_SetTarget<Mobj>(&mo->tracer, clip.linetarget);
   }
}

const char *kwds_A_CustomPlayerMelee[] =
{
   "{DUMMY}",          //  0
   "none",             //  1
   "punch",            //  2
   "chainsaw",         //  3
};

static argkeywd_t cpmkwds =
{
   kwds_A_CustomPlayerMelee,
   sizeof(kwds_A_CustomPlayerMelee) / sizeof(const char *)
};

//
// A_CustomPlayerMelee
//
// A parameterized melee attack codepointer function
// Parameters:
// args[0] : damage factor
// args[1] : damage modulus
// args[2] : berzerk multiplier
// args[3] : angle deflection type (none, punch, chainsaw)
// args[4] : sound to make (dehacked number)
//
void A_CustomPlayerMelee(actionargs_t *actionargs)
{
   angle_t angle;
   fixed_t slope;
   int damage, dmgfactor, dmgmod, berzerkmul, deftype;
   sfxinfo_t *sfx;
   Mobj      *mo = actionargs->actor;
   player_t  *player;
   pspdef_t  *psp;
   arglist_t *args = actionargs->args;

   if(!(player = mo->player))
      return;

   if(!(psp = actionargs->pspr))
      return;

   dmgfactor  = E_ArgAsInt(args, 0, 0);
   dmgmod     = E_ArgAsInt(args, 1, 0);
   berzerkmul = E_ArgAsInt(args, 2, 0);
   deftype    = E_ArgAsKwd(args, 3, &cpmkwds, 0);
   sfx        = E_ArgAsSound(args, 4);

   // adjust parameters

   if(dmgmod < 1)
      dmgmod = 1;
   else if(dmgmod > 256)
      dmgmod = 256;

   // calculate damage
   damage = dmgfactor * ((P_Random(pr_custompunch)%dmgmod) + 1);

   // apply berzerk multiplier
   if(player->powers[pw_strength])
      damage *= berzerkmul;

   // decrement ammo if appropriate
   P_SubtractAmmo(player, -1);
   
   angle = player->mo->angle;
   
   if(deftype == 2 || deftype == 3)
      angle += P_SubRandom(pr_custompunch) << 18;
   
   slope = P_doAutoAim(mo, angle, MELEERANGE);

   // WEAPON_FIXME: does this pointer fail to set the player into an attack state?
   // WEAPON_FIXME: check ALL new weapon pointers for this problem.
   
   P_LineAttack(mo, angle, MELEERANGE, slope, damage);
   
   if(!clip.linetarget)
   {
      // assume they want sawful on miss if sawhit specified
      if(sfx && sfx->dehackednum == sfx_sawhit)
         P_WeaponSound(mo, sfx_sawful);
      return;
   }

   // start sound
   P_WeaponSoundInfo(mo, sfx);
   
   // turn to face target   
   player->mo->angle = P_PointToAngle(mo->x, mo->y,
                                       clip.linetarget->x, clip.linetarget->y);

   // apply chainsaw deflection if selected
   if(deftype == 3)
   {
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
}

static const char *kwds_A_PlayerThunk1[] =
{
   "noface", // 0
   "face",   // 1
};

static argkeywd_t facekwds = { kwds_A_PlayerThunk1, 2 };

static const char *kwds_A_PlayerThunk2[] =
{
   "attacker",  // 0
   "aimtarget", // 1
};

static argkeywd_t targetkwds = { kwds_A_PlayerThunk2, 2};

static const char *kwds_A_PlayerThunk3[] =
{
   "nouseammo", // 0
   "useammo",   // 1
};

static argkeywd_t ammokwds = { kwds_A_PlayerThunk3, 2 };

//
// A_PlayerThunk
//
// Allows the player's weapons to use monster codepointers
// Parameters:
// args[0] : index of codepointer to call (is validated)
// args[1] : bool, 0 disables the FaceTarget cp for the player
// args[2] : dehacked num of a state to put the player in temporarily
// args[3] : bool, 1 == set player's target to autoaim target
// args[4] : bool, 1 == use ammo on current weapon if attack succeeds
//
void A_PlayerThunk(actionargs_t *actionargs)
{
   bool face;
   bool settarget;
   bool useammo;
   int cptrnum, statenum;
   state_t *oldstate = 0;
   Mobj *oldtarget = NULL, *localtarget = NULL;
   Mobj *mo = actionargs->actor;
   player_t  *player;
   pspdef_t  *psp;
   arglist_t *args = actionargs->args;

   if(!(player = mo->player))
      return;

   if(!(psp = actionargs->pspr))
      return;

   cptrnum   =   E_ArgAsBexptr(args, 0);
   face      = !!E_ArgAsKwd(args, 1, &facekwds, 0);
   statenum  =   E_ArgAsStateNumG0(args, 2, NULL);
   settarget = !!E_ArgAsKwd(args, 3, &targetkwds, 0);
   useammo   = !!E_ArgAsKwd(args, 4, &ammokwds, 0);

   // validate codepointer index
   if(cptrnum < 0)
      return;

   // set player's target to thing being autoaimed at if this
   // behavior is requested.
   if(settarget)
   {
      // record old target
      oldtarget = mo->target;
      P_BulletSlope(mo);
      if(clip.linetarget)
      {
         P_SetTarget<Mobj>(&(mo->target), clip.linetarget);
         localtarget = clip.linetarget;
      }
      else
         return;
   }

   // possibly disable the FaceTarget pointer using MIF_NOFACE
   if(!face)
      mo->intflags |= MIF_NOFACE;

   // If a state has been provided, place the player into it. This
   // allows use of parameterized codepointers.
   if(statenum >= 0 && statenum < NUMSTATES)
   {
      oldstate = mo->state;
      mo->state = states[statenum];
   }

   // if ammo should be used, subtract it now
   if(useammo)
      P_SubtractAmmo(player, -1);

   actionargs_t thunkargs;
   thunkargs.actiontype = actionargs_t::WEAPONFRAME;
   thunkargs.actor      = mo;
   thunkargs.args       = ESAFEARGS(mo);
   thunkargs.pspr       = actionargs->pspr;

   // execute the codepointer
   deh_bexptrs[cptrnum].cptr(&thunkargs);

   // remove MIF_NOFACE
   mo->intflags &= ~MIF_NOFACE;

   // restore player's old target if a new one was found & set
   if(settarget && localtarget)
   {
      P_SetTarget<Mobj>(&(mo->target), oldtarget);
   }

   // put player back into his normal state
   if(statenum >= 0 && statenum < NUMSTATES)
   {
      mo->state = oldstate;
   }
}

//----------------------------------------------------------------------------
//
// $Log: p_pspr.c,v $
// Revision 1.13  1998/05/07  00:53:36  killough
// Remove dependence on order of evaluation
//
// Revision 1.12  1998/05/05  16:29:17  phares
// Removed RECOIL and OPT_BOBBING defines
//
// Revision 1.11  1998/05/03  22:35:21  killough
// Fix weapons switch bug again, beautification, headers
//
// Revision 1.10  1998/04/29  10:01:55  killough
// Fix buggy weapons switch code
//
// Revision 1.9  1998/03/28  18:01:38  killough
// Prevent weapon recoil in no-clipping mode
//
// Revision 1.8  1998/03/23  03:28:29  killough
// Move weapons changes to G_BuildTiccmd()
//
// Revision 1.7  1998/03/10  07:14:47  jim
// Initial DEH support added, minus text
//
// Revision 1.6  1998/02/24  08:46:27  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.5  1998/02/17  05:59:41  killough
// Use new RNG calling sequence
//
// Revision 1.4  1998/02/15  02:47:54  phares
// User-defined keys
//
// Revision 1.3  1998/02/09  03:06:15  killough
// Add player weapon preference options
//
// Revision 1.2  1998/01/26  19:24:18  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:00  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

