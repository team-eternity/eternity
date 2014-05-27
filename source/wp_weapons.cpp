// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2014 James Haley et al.
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
// 
// Weapon Engine base class.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "a_args.h"
#include "d_player.h"
#include "e_inventory.h"
#include "p_mobj.h"
#include "s_sound.h"
#include "wp_weapons.h"

//
// WeaponEngine::setPspritePtr
//
// haleyjd 06/22/13: Set psprite state from pspdef_t
//
void WeaponEngine::setPspritePtr(player_t *player, pspdef_t *psp, statenum_t stnum)
{
   do
   {
      state_t *state;
      
      if(!stnum)
      {
         // object removed itself
         psp->state = nullptr;
         break;
      }

      // Allow subclasses to redirect particular states (aka, dirty hacks)
      stnum = redirectStateOnSet(stnum);

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

/*
Code for DoomWeaponEngine::redirectStateOnSet:
      ...
      // killough 7/19/98: Pre-Beta BFG
      if(stnum == E_StateNumForDEHNum(S_BFG1) && bfgtype == bfg_classic)
         stnum = E_SafeState(S_OLDBFG1); // Skip to alternative weapon frame
      ...
*/

// 
// WeaponEngine::setPsprite
//
// Set player's psprite from psprite index in position
//
void WeaponEngine::setPsprite(player_t *player, int position, statenum_t stnum)
{
   setPspritePtr(player, &player->psprites[position], stnum);
}

//
// WeaponEngine::getReadyWeapon
//
// haleyjd 09/13/07: 
// Retrieves a pointer to the proper weaponinfo_t structure for the 
// readyweapon index stored in the player.
//
weaponinfo_t *WeaponEngine::getReadyWeapon(player_t *player)
{
   return &(data.weaponinfo[player->readyweapon]);
}


//
// WeaponEngine::getPendingWeapon
//
// haleyjd 06/28/13:
// Retrieves a pointer to the proper weaponinfo_t structure for the
// pendingweapon index stored in the player.
//
weaponinfo_t *WeaponEngine::getPendingWeapon(player_t *player)
{
   return
      (player->pendingweapon >= 0 && player->pendingweapon < data.numweapons) ?
       &(data.weaponinfo[player->pendingweapon]) : nullptr;
}

//
// WeaponEngine::getPlayerWeapon
//
// haleyjd 09/16/07:
// Gets weapon at given index for the given player.
//
weaponinfo_t *WeaponEngine::getPlayerWeapon(player_t *player, int index)
{
   return &(data.weaponinfo[index]);
}

//
// WeaponEngine::bringUpWeapon
//
// Protected method.
// Starts bringing the pending weapon up from the bottom of the screen.
//
void WeaponEngine::bringUpWeapon(player_t *player)
{
   statenum_t newstate;

   if(player->pendingweapon == data.nochange)
      player->pendingweapon = player->readyweapon;

   weaponinfo_t *pendingweapon;
   if((pendingweapon = getPendingWeapon(player)))
   {
      // haleyjd 06/28/13: weapon upsound
      if(pendingweapon->upsound)
         S_StartSound(player->mo, pendingweapon->upsound);
      
      newstate = pendingweapon->upstate;
  
      player->pendingweapon = data.nochange;
   
      player->psprites[ps_weapon].sy = data.weaponbottom;
   
      setPsprite(player, ps_weapon, newstate);
   }
}

// Needed for BOOM/MBF/EE weapon engine static data:
      /*
      // killough 12/98: prevent pistol from starting visibly at bottom of screen:
      player->psprites[ps_weapon].sy = demo_version >= 203 ? 
         WEAPONBOTTOM+FRACUNIT*2 : WEAPONBOTTOM;
      */

//
// WeaponEngine::weaponHasAmmo
//
// haleyjd 08/05/13: Test if a player has ammo for a weapon
//
bool WeaponEngine::weaponHasAmmo(player_t *player, weaponinfo_t *weapon)
{
   itemeffect_t *ammoType = weapon->ammo;

   // a weapon without an ammotype has infinite ammo
   if(!ammoType)
      return true;

   // otherwise, read the inventory slot
   return (E_GetItemOwnedAmount(player, ammoType) >= weapon->ammopershot);
}

// EOF

