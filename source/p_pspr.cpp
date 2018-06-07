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
#include "e_weapons.h"
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

// The following array holds the recoil values         // phares
// haleyjd 08/18/08: recoil moved into weaponinfo

//=============================================================================

//
// P_SetPspritePtr
//
// haleyjd 06/22/13: Set psprite state from pspdef_t
//
void P_SetPspritePtr(const player_t *player, pspdef_t *psp, statenum_t stnum)
{
   // haleyjd 06/22/13: rewrote again to use actionargs structure
   do
   {
      state_t *state;
      
      if(!stnum)
      {
         // object removed itself
         psp->state = nullptr;
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
   
   if(player->pendingweapon == nullptr)
   {
      player->pendingweapon = player->readyweapon;
      player->pendingweaponslot = player->readyweaponslot;
   }
   
   weaponinfo_t *pendingweapon;
   if((pendingweapon = player->pendingweapon))
   {
      // haleyjd 06/28/13: weapon upsound
      if(pendingweapon->upsound)
         S_StartSound(player->mo, pendingweapon->upsound);
  
      newstate = pendingweapon->upstate;
  
      player->pendingweapon = nullptr;
      player->pendingweaponslot = nullptr;
   
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
bool P_WeaponHasAmmo(const player_t *player, const weaponinfo_t *weapon)
{
   itemeffect_t *ammoType = weapon->ammo;

   // a weapon without an ammotype has infinite ammo
   if(!ammoType)
      return true;

   // otherwise, read the inventory slot
   return (E_GetItemOwnedAmount(player, ammoType) >= weapon->ammopershot);
}

//
// MaxW: 2018/01/03: Test if a player has alt ammo for a weapon
//
static bool P_WeaponHasAmmoAlt(player_t *player, weaponinfo_t *weapon)
{
   itemeffect_t *ammoType = weapon->ammo_alt;

   // a weapon without an ammotype has infinite ammo
   if(!ammoType)
      return true;

   // otherwise, read the inventory slot
   return (E_GetItemOwnedAmount(player, ammoType) >= weapon->ammopershot_alt);
}

static weaponslot_t *P_findFirstNonNullWeaponSlot(const player_t *player)
{
   for(int i = 0; i < NUMWEAPONSLOTS; i++)
   {
      if(player->pclass->weaponslots[i] != nullptr)
         return player->pclass->weaponslots[i];
   }

   return nullptr;
}

//
// P_NextWeapon
//
// haleyjd 05/31/14: Rewritten to use next and previous in cycle pointers
// in weaponinfo_t, for friendliness with future dynamic weapons system.
//
int P_NextWeapon(const player_t *player, uint8_t *slotindex)
{
   const weaponinfo_t             *currentweapon = player->readyweapon;
   const weaponinfo_t             *newweapon     = player->readyweapon;
   const weaponslot_t             *newweaponslot = player->readyweaponslot;
   const BDListItem<weaponslot_t> *newweaponlink;
   bool                            ammototry;

   if(newweaponslot == nullptr)
      newweaponslot = P_findFirstNonNullWeaponSlot(player);
   newweaponlink = &newweaponslot->links;

   do
   {
      newweaponlink = newweaponlink->bdPrev;
      newweapon = newweaponlink->bdObject->weapon;
      if(newweaponlink->isDummy())
      {
         const int slotindex = newweaponlink->bdObject->slotindex;
         bool firsttime = true;
         for(int i = (slotindex + 1) % NUMWEAPONSLOTS;
             i  != slotindex + 1 || firsttime; i = (i + 1) % NUMWEAPONSLOTS)
         {
            if(player->pclass->weaponslots[i] != nullptr)
            {
               newweaponlink = E_LastInSlot(player->pclass->weaponslots[i]);
               newweapon = newweaponlink->bdObject->weapon;
               break;
            }
            firsttime = false;
         }
      }
      ammototry = P_WeaponHasAmmo(player, newweapon);
   }
   while((!E_PlayerOwnsWeapon(player, newweapon) || !ammototry) &&
         newweapon->id != currentweapon->id);

   if(demo_version >= 401)
   {
      if(newweapon != currentweapon)
      {
         *slotindex = newweaponlink->bdObject->slotindex;
         return newweapon->id;
      }
      else
      {
         *slotindex = -1;
         return -1;
      }
   }
   else
      return newweapon != currentweapon ? newweapon->dehnum : wp_nochange;
}

//
// P_PrevWeapon
//
// haleyjd 03/06/09: Like the above.
//
int P_PrevWeapon(const player_t *player, uint8_t *slotindex)
{
   const weaponinfo_t             *currentweapon = player->readyweapon;
   const weaponinfo_t             *newweapon     = player->readyweapon;
   const weaponslot_t             *newweaponslot = player->readyweaponslot;
   const BDListItem<weaponslot_t> *newweaponlink;
   bool                            ammototry;

   if(newweaponslot == nullptr)
      newweaponslot = P_findFirstNonNullWeaponSlot(player);
   newweaponlink = &newweaponslot->links;

   do
   {
      newweaponlink = newweaponlink->bdNext;
      newweapon = newweaponlink->bdObject->weapon;
      if(newweaponlink->isDummy())
      {
         const int slotindex = newweaponlink->bdObject->slotindex;
         bool firsttime = true;
         for(int i = slotindex == 0 ? NUMWEAPONSLOTS - 1 : slotindex - 1;
             i  != slotindex - 1 || firsttime; i = i == 0 ? NUMWEAPONSLOTS - 1 : i - 1)
         {
            if(player->pclass->weaponslots[i] != nullptr)
            {
               newweaponlink = E_FirstInSlot(player->pclass->weaponslots[i]);
               newweapon = newweaponlink->bdObject->weapon;
               break;
            }
         }

      }
      ammototry = P_WeaponHasAmmo(player, newweapon);
   }
   while((!E_PlayerOwnsWeapon(player, newweapon) || !ammototry) &&
         newweapon->id != currentweapon->id);

   if(demo_version >= 401)
   {
      if(slotindex != nullptr)
         *slotindex = newweaponlink->bdObject->slotindex;
      return newweapon != currentweapon ? newweapon->id : -1;
   }
   else
      return newweapon != currentweapon ? newweapon->dehnum : wp_nochange;
}

// The first set is where the weapon preferences from             // killough,
// default.cfg are stored. These values represent the keys used   // phares
// in DOOM2 to bring up the weapon, i.e. 6 = plasma gun. These    //    |
// are NOT the wp_* constants.                                    //    V

// Exists for ye olde compat

static int weapon_preferences[NUMWEAPONS+1] =
{
   6, 9, 4, 3, 2, 8, 5, 7, 1, 0
};

//
// P_SwitchWeaponOld
//
// Checks current ammo levels and gives you the most preferred weapon with ammo.
// It will not pick the currently raised weapon. When called from P_CheckAmmo 
// this won't matter, because the raised weapon has no ammo anyway. When called
// from G_BuildTiccmd you want to toggle to a different weapon regardless.
//
weapontype_t P_SwitchWeaponOld(const player_t *player)
{
   int *prefer = weapon_preferences; // killough 3/22/98
   weapontype_t currentweapon = player->readyweapon->dehnum;
   weapontype_t newweapon = currentweapon;
   int i = NUMWEAPONS + 1;   // killough 5/2/98   

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
         if(E_PlayerOwnsWeaponForDEHNum(player, wp_shotgun) && shells)
            newweapon = wp_shotgun;
         break;
      case 4:
         if(E_PlayerOwnsWeaponForDEHNum(player, wp_chaingun) && clips)
            newweapon = wp_chaingun;
         break;
      case 5:
         if(E_PlayerOwnsWeaponForDEHNum(player, wp_missile) && rockets)
            newweapon = wp_missile;
         break;
      case 6:
         if(E_PlayerOwnsWeaponForDEHNum(player, wp_plasma) && cells &&
            GameModeInfo->id != shareware)
            newweapon = wp_plasma;
         break;
      case 7:
         if(E_PlayerOwnsWeaponForDEHNum(player, wp_bfg) && GameModeInfo->id != shareware &&
            cells >= (demo_compatibility ? 41 : 40))
            newweapon = wp_bfg;
         break;
      case 8:
         if(E_PlayerOwnsWeaponForDEHNum(player, wp_chainsaw))
            newweapon = wp_chainsaw;
         break;
      case 9:
         if(E_PlayerOwnsWeaponForDEHNum(player, wp_supershotgun) &&
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
   if(P_WeaponHasAmmo(player, player->readyweapon))
      return true;
   
   // Out of ammo, pick a weapon to change to.
   //
   // killough 3/22/98: for old demos we do the switch here and now;
   // for Boom games we cannot do this, and have different player
   // preferences across demos or networks, so we have to use the
   // G_BuildTiccmd() interface instead of making the switch here.
   
   if(demo_compatibility)
   {
      player->pendingweapon = E_WeaponForDEHNum(P_SwitchWeaponOld(player));      // phares
      // Now set appropriate weapon overlay.
      // WEAPON_FIXME
      P_SetPsprite(player,ps_weapon,player->readyweapon->downstate);
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
void P_SubtractAmmo(const player_t *player, int compat_amt)
{
   weaponinfo_t *weapon = player->readyweapon;
   itemeffect_t *ammo;
   int amount;

   // EDF_FEATURES_FIXME: Needs to fire the wimpy version's amount of ammo if in deathmatch, or at least
   // do it for Heretic.
   if(demo_version >= 401 && (player->attackdown & AT_ITEM))
      return;
   else if(demo_version >= 401 && (player->attackdown & AT_SECONDARY))
   {
      ammo = weapon->ammo_alt;
      amount = (!(weapon->flags & WPF_DISABLEAPS) || compat_amt < 0) ? weapon->ammopershot_alt :
                                                                       compat_amt;
   }
   else
   {
      ammo = weapon->ammo;
      amount = (!(weapon->flags & WPF_DISABLEAPS) || compat_amt < 0) ? weapon->ammopershot :
                                                                       compat_amt;
   }

   if(player->cheats & CF_INFAMMO || !ammo)
      return;

   
   E_RemoveInventoryItem(player, ammo, amount);
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

   weapon = player->readyweapon;

   P_SetMobjState(player->mo, player->mo->info->missilestate);
   newstate = player->refire && weapon->holdstate ? weapon->holdstate : weapon->atkstate;
   P_SetPsprite(player, ps_weapon, newstate);

   // haleyjd 04/06/03: silencer powerup
   // haleyjd 09/14/07: per-weapon silencer, always silent support
   if(!(weapon->flags & WPF_SILENCEABLE && player->powers[pw_silencer]) &&
      !(weapon->flags & WPF_SILENT)) 
      P_NoiseAlert(player->mo, player->mo);

   lastshottic = gametic;                       // killough 3/22/98
}

//
// Do a weapon's alt fire
//
static void P_fireWeaponAlt(player_t *player)
{
   statenum_t newstate;
   weaponinfo_t *weapon = player->readyweapon;

   if(!P_WeaponHasAmmoAlt(player, weapon) || !E_WeaponHasAltFire(weapon))
      return;

   P_SetMobjState(player->mo, player->mo->info->missilestate);
   newstate = player->refire && weapon->holdstate_alt ? weapon->holdstate_alt :
                                                        weapon->atkstate_alt;
   P_SetPsprite(player, ps_weapon, newstate);

   // haleyjd 04/06/03: silencer powerup
   // haleyjd 09/14/07: per-weapon silencer, always silent support
   if(!(weapon->flags & WPF_SILENCEABLE && player->powers[pw_silencer]) &&
      !(weapon->flags & WPF_SILENT))
      P_NoiseAlert(player->mo, player->mo);

   lastshottic = gametic;                       // killough 3/22/98
}

//
// Try to fire a weapon if the user inputs that.
// Returns true if a weapon is fired, and false otherwise.
//
static bool P_tryFireWeapon(player_t *player)
{
   if(player->cmd.buttons & BT_ATTACK)
   {
      if(!(player->attackdown & AT_ALL) || !(player->readyweapon->flags & WPF_NOAUTOFIRE))
      {
         player->attackdown = AT_PRIMARY;
         P_FireWeapon(player);
         return true;
      }
   }
   else if(player->cmd.buttons & BTN_ATTACK_ALT && E_WeaponHasAltFire(player->readyweapon))
   {
      if(!(player->attackdown & AT_ALL) || !(player->readyweapon->flags & WPF_NOAUTOFIRE))
      {
         player->attackdown = AT_SECONDARY;
         P_fireWeaponAlt(player);
         return true;
      }
   }
   else
      player->attackdown = AT_NONE;

   return false;
}

//
// P_DropWeapon
//
// Player died, so put the weapon away.
//
void P_DropWeapon(player_t *player)
{
   P_SetPsprite(player, ps_weapon, player->readyweapon->downstate);
}

//=============================================================================
//
// haleyjd 09/13/07
//
// New functions for dynamic weapons system
//

//
// P_GetPlayerWeapon
//
// haleyjd 09/16/07:
// Gets weapon at given index for the given player.
//
weaponinfo_t *P_GetPlayerWeapon(const player_t *player, int slot)
{
   if(demo_version < 401 && GameModeInfo->type == Game_DOOM)
      return E_WeaponForDEHNum(slot);

   if(!player->pclass->weaponslots[slot])
      return nullptr;

   // Backup demo compat code, in case something goes *really*
   // wrong with the DeHackEd num check.
   /*if(demo_version < 401 && GameModeInfo->type == Game_DOOM)
   {
      DLListItem<weaponslot_t> *weaponslot = E_FirstInSlot(player->pclass->weaponslots[slot]);
      while(!weaponslot->dllNext->isDummy())
         weaponslot = weaponslot->dllNext;
      return weaponslot->dllObject->weapon;
   }*/

   bool hit = false;
   BDListItem<weaponslot_t> *weaponslot, *baseslot;
   const auto *wp = E_IsPoweredVariant(player->readyweapon) ?
                    player->readyweapon->sisterWeapon : player->readyweapon;

   if(!weapon_hotkey_cycling)
   {
      weaponslot = E_LastInSlot(player->pclass->weaponslots[slot]);
      if(!E_PlayerOwnsWeapon(player, weaponslot->bdObject->weapon))
         return nullptr;
      else
         return weaponslot ? weaponslot->bdObject->weapon : nullptr;
   }

   // This initial call assures us that
   // player->pclass->weaponslots[slot]->bdNext is valid.
   baseslot = E_FirstInSlot(player->pclass->weaponslots[slot]);

   // Try finding the player's currently-equipped weapon.
   while(!baseslot->isDummy())
   {
      if(baseslot->bdObject->weapon->id == wp->id)
      {
         hit = true;
         break;
      }
      else
         baseslot = baseslot->bdNext;
   }

   // Reset starting point if we couldn't find player's weapon in this slot.
   if(baseslot->isDummy())
      baseslot = baseslot->bdNext;

   weaponslot = baseslot;

   // If we found the player's readyweapon in the current slot, or the player
   // doesn't own the first weapon in the slot, we'll need to iterate through.
   if(hit || !E_PlayerOwnsWeapon(player, weaponslot->bdObject->weapon))
   {
      // The next weapon we find that the player owns is what we're looking for.
      do
      {
         weaponslot = weaponslot->bdNext;
         // Start from the start the list if we get to the end.
         if(weaponslot->isDummy())
            weaponslot = weaponslot->bdNext;

      } while(weaponslot != baseslot &&
              !E_PlayerOwnsWeapon(player, weaponslot->bdObject->weapon));

      // If the player doesn't own weaponslot's weapon, don't allow the change.
      if(!E_PlayerOwnsWeapon(player, weaponslot->bdObject->weapon))
         weaponslot = nullptr;
   }

   return weaponslot ? weaponslot->bdObject->weapon : nullptr;
}

//
// P_WeaponSoundInfo
//
// Plays a sound originating from the player's weapon by sfxinfo_t *
//
void P_WeaponSoundInfo(Mobj *mo, sfxinfo_t *sound)
{
   soundparams_t params;
   
   params.sfx = sound;
   params.setNormalDefaults(mo);

   if(mo->player && mo->player->powers[pw_silencer] &&
      mo->player->readyweapon->flags & WPF_SILENCEABLE)
      params.volumeScale = WEAPON_VOLUME_SILENCED;

   S_StartSfxInfo(params);
}

//
// P_WeaponSound
//
// Plays a sound originating from the player's weapon
//
void P_WeaponSound(Mobj *mo, int sfx_id)
{
   int volume = 127;

   if(mo->player && mo->player->powers[pw_silencer] &&
      mo->player->readyweapon->flags & WPF_SILENCEABLE)
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

   // EDF_FEATURES_FIXME: Demo guard this as well?
   if(!player->pendingweapon && !E_PlayerOwnsWeapon(player, player->readyweapon) &&
      player->readyweapon->id != UnknownWeaponInfo)
   {
      player->pendingweapon = E_FindBestWeapon(player);
      player->pendingweaponslot = E_FindFirstWeaponSlot(player, player->pendingweapon);
   }

   // get out of attack state
   if(mo->state == states[mo->info->missilestate] || 
      mo->state == states[player->pclass->altattack])
   {
      P_SetMobjState(mo, mo->info->spawnstate);
   }


   // Play sound if the readyweapon has a sound to play and the current
   // state is the ready state, and do it only 50% of the time if the
   // according flag is set.
   if(player->readyweapon->readysound &&
      psp->state->index == player->readyweapon->readystate &&
      (!(player->readyweapon->flags & WPF_READYSNDHALF) || M_Random() < 128))
      S_StartSound(player->mo, player->readyweapon->readysound);

   // WEAPON_FIXME: chainsaw particulars (haptic feedback)
   if(E_WeaponIsCurrentDEHNum(player, wp_chainsaw) && psp->state == states[E_SafeState(S_SAW)])
   {
      if(player == &players[consoleplayer])
         I_StartHaptic(HALHapticInterface::EFFECT_CONSTANT, 3, 108);
   }

   // check for change
   //  if player is dead, put the weapon away
   
   if(player->pendingweapon != nullptr || !player->health)
   {
      // change weapon (pending weapon should already be validated)
      statenum_t newstate = player->readyweapon->downstate;
      P_SetPsprite(player, ps_weapon, newstate);
      return;
   }

   // check for fire
   // certain weapons do not auto fire
   if(demo_version >= 401 && P_tryFireWeapon(player))
      return;
   else if(player->cmd.buttons & BT_ATTACK)
   {
      if(!(player->attackdown & AT_PRIMARY) ||
         !(player->readyweapon->flags & WPF_NOAUTOFIRE))
      {
         player->attackdown = AT_PRIMARY;
         P_FireWeapon(player);
         return;
      }
   }
   else
      player->attackdown = AT_NONE;

   // bob the weapon based on movement speed
   int angle = (128*leveltime) & FINEMASK;
   psp->sx = FRACUNIT + FixedMul(player->bob, finecosine[angle]);
   angle &= FINEANGLES/2-1;
   psp->sy = WEAPONTOP + FixedMul(player->bob, finesine[angle]);
}

static void A_reFireNew(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;

   if(!player)
      return;

   // check for fire
   //  (if a weaponchange is pending, let it go through instead)

   if((player->cmd.buttons & BT_ATTACK)
      && player->pendingweapon == nullptr && player->health
      && !(player->attackdown & AT_SECONDARY))
   {
      player->refire++;
      P_FireWeapon(player);
   }
   else if((player->cmd.buttons & BTN_ATTACK_ALT)
           && player->pendingweapon == nullptr && player->health
           && !(player->attackdown & AT_PRIMARY))
   {
      player->refire++;
      P_fireWeaponAlt(player);
   }
   else
   {
      player->refire = 0;
      P_CheckAmmo(player);
   }
}

//
// A_ReFire
//
// The player can re-fire the weapon
// without lowering it entirely.
//
void A_ReFire(actionargs_t *actionargs)
{
   if(demo_version >= 401)
   {
      A_reFireNew(actionargs);
      return;
   }

   player_t *player = actionargs->actor->player;

   if(!player)
      return;

   // check for fire
   //  (if a weaponchange is pending, let it go through instead)
   
   if((player->cmd.buttons & BT_ATTACK)
      && player->pendingweapon == nullptr && player->health)
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
      P_SetPsprite(player, ps_weapon, player->readyweapon->downstate);
}

//
// A_Lower
//
// Lowers current weapon, and changes weapon at bottom.
//
void A_Lower(actionargs_t *actionargs)
{
   player_t     *player;
   pspdef_t     *psp;
   arglist_t    *args = actionargs->args;
   // WEAPON_FIXME: Default LOWERSPEED property of EDF weapons?
   const fixed_t lowerspeed = FRACUNIT * E_ArgAsInt(args, 1, LOWERSPEED);

   if(!(player = actionargs->actor->player))
      return;

   if(!(psp = actionargs->pspr))
      return;

   psp->sy += lowerspeed;
   
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
   if(player->pendingweapon != nullptr)
   {
      player->readyweapon = player->pendingweapon;
      player->readyweaponslot = player->pendingweaponslot;
   }

   P_BringUpWeapon(player);
}

//
// A_Raise
//
void A_Raise(actionargs_t *actionargs)
{
   statenum_t    newstate;
   player_t     *player;
   pspdef_t     *psp;
   arglist_t    *args = actionargs->args;
   // WEAPON_FIXME: Default RAISESPEED property of EDF weapons?
   const fixed_t raisespeed = FRACUNIT * E_ArgAsInt(args, 1, RAISESPEED);

   if(!(player = actionargs->actor->player))
      return;

   if(!(psp = actionargs->pspr))
      return;
   
   psp->sy -= raisespeed;
   
   if(psp->sy > WEAPONTOP)
      return;
   
   psp->sy = WEAPONTOP;
   
   // The weapon has been raised all the way,
   //  so change to the ready state.
   
   newstate = player->readyweapon->readystate;
   
   P_SetPsprite(player, ps_weapon, newstate);
}

//
// P_WeaponRecoil
//
// haleyjd 12/11/12: Separated out of the below.
//
void P_WeaponRecoil(player_t *player)
{
   weaponinfo_t *readyweapon = player->readyweapon;

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
void A_FireSomething(player_t* player, int adder)
{
   if(demo_version >= 401 && player->attackdown & AT_SECONDARY)
      P_SetPsprite(player, ps_flash, player->readyweapon->flashstate_alt);
   else
      P_SetPsprite(player, ps_flash, player->readyweapon->flashstate+adder);
 
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
// P_DoAutoAim
//
// Does basic autoaiming logic and returns the slope value calculated by
// P_AimLineAttack.
//
fixed_t P_DoAutoAim(Mobj *mo, angle_t angle, fixed_t distance)
{
   if(demo_version >= 203)
   {
      // killough 8/2/98: make autoaiming prefer enemies
      fixed_t slope = P_AimLineAttack(mo, angle, distance, true);

      // autoaiming is disabled?
      if(full_demo_version > make_full_version(340, 15) && !autoaim && mo->player)
         return P_PlayerPitchSlope(mo->player);

      if(clip.linetarget)
         return slope;
   }
   
   return P_AimLineAttack(mo, angle, distance, false);
}

//=============================================================================
//
// WEAPON ATTACKS
//

fixed_t bulletslope;

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
   int mask = demo_version < 203 ? false : true;

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
   while (mask && (mask=false, !clip.linetarget));  // killough 8/2/98
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
      player->psprites[i].state = nullptr;
   
   // spawn the gun
   player->pendingweapon = player->readyweapon;
   player->pendingweaponslot = player->readyweaponslot;
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

   player->psprites[ps_weapon].backupPosition();
   player->psprites[ps_flash].backupPosition();
   
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

enum custombulletaccuracy_e : uint8_t
{
   CBA_NONE,
   CBA_ALWAYS,
   CBA_FIRST,
   CBA_NEVER,
   CBA_SSG,
   CBA_MONSTER,
   CBA_CUSTOM,

   NUMCUSTOMBULLETACCURACIES
};

static const char *kwds_A_FireCustomBullets[NUMCUSTOMBULLETACCURACIES] =
{
   "{DUMMY}",           // 0
   "always",            // 1
   "first",             // 2
   "never",             // 3
   "ssg",               // 4
   "monster",           // 5
   "custom",            // 6
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
// args[6] : horizontal spread angle if args[1] is "custom"
// args[7] : vertical spread angle if args[1] is "custom"
// args[8] : optional pufftype ("" for default)
//
void A_FireCustomBullets(actionargs_t *actionargs)
{
   Mobj      *mo   = actionargs->actor;
   arglist_t *args = actionargs->args;
   int i, numbullets, damage, dmgmod;
   int flashint, flashstate;
   int horizontal, vertical;
   sfxinfo_t *sfx;
   player_t *player;
   pspdef_t *psp;
   custombulletaccuracy_e accurate;

   if(!(player = mo->player))
      return;

   if(!(psp = actionargs->pspr))
      return;

   sfx        = E_ArgAsSound(args, 0);
   accurate   = static_cast<custombulletaccuracy_e>(E_ArgAsKwd(args, 1, &fcbkwds, CBA_NONE));
   numbullets = E_ArgAsInt(args, 2, 0);
   damage     = E_ArgAsInt(args, 3, 0);
   dmgmod     = E_ArgAsInt(args, 4, 0);
   
   flashint   = E_ArgAsInt(args, 5, 0);
   flashstate = E_ArgAsStateNum(args, 5, nullptr, player);

   horizontal = E_ArgAsInt(args, 6, 0);
   vertical   = E_ArgAsInt(args, 7, 0);

   const char *pufftype = E_ArgAsString(args, 8, nullptr);

   if(accurate == CBA_NONE)
      accurate = CBA_ALWAYS;

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
      P_SetPsprite(player, ps_flash, player->readyweapon->flashstate);

   P_WeaponRecoil(player);

   P_BulletSlope(mo);

   // loop on numbullets
   for(i = 0; i < numbullets; ++i)
   {
      int dmg = damage * (P_Random(pr_custombullets)%dmgmod + 1);
      angle_t angle = mo->angle;
      fixed_t slope = bulletslope; // haleyjd 01/03/07: bug fix
      
      if(accurate == CBA_CUSTOM)
      {
         angle += P_SubRandomEx(pr_custommisfire, ANGLE_1) / 2 * horizontal;
         const angle_t pitch = (P_SubRandomEx(pr_custommisfire, ANGLE_1) / 2) *
                                vertical;
         // convert pitch to the same "unit" as slope, then add it on
         slope += finetangent[(ANG90 - pitch) >> ANGLETOFINESHIFT];

         P_LineAttack(mo, angle, MISSILERANGE, slope, dmg, pufftype);
      }
      else if(accurate <= CBA_NEVER || accurate == CBA_MONSTER)
      {
         // if never accurate, monster accurate, or accurate only on 
         // refire and player is refiring, add some to the angle
         if(accurate == CBA_NEVER || accurate == CBA_MONSTER ||
            (accurate == CBA_FIRST && player->refire))
         {
            int aimshift = ((accurate == CBA_MONSTER) ? 20 : 18);
            angle += P_SubRandom(pr_custommisfire) << aimshift;
         }

         P_LineAttack(mo, angle, MISSILERANGE, slope, dmg, pufftype);
      }
      else if(accurate == CBA_SSG) // ssg spread
      {
         angle += P_SubRandom(pr_custommisfire) << 19;
         slope += P_SubRandom(pr_custommisfire) << 5;

         P_LineAttack(mo, angle, MISSILERANGE, slope, dmg, pufftype);
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
// args[5] : pufftype
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
   const char *pufftype = E_ArgAsString(args, 5, nullptr);

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
   
   slope = P_DoAutoAim(mo, angle, MELEERANGE);

   // WEAPON_FIXME: does this pointer fail to set the player into an attack state?
   // WEAPON_FIXME: check ALL new weapon pointers for this problem.
   
   P_LineAttack(mo, angle, MELEERANGE, slope, damage, pufftype);
   
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
         if(angle - mo->angle < static_cast<angle_t>(-ANG90/20))
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
   Mobj *oldtarget = nullptr, *localtarget = nullptr;
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
   statenum  =   E_ArgAsStateNumG0(args, 2, nullptr, player);
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

