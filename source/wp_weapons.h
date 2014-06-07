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

#ifndef WP_WEAPONS_H__
#define WP_WEAPONS_H__

// Needed for statenum_t
#include "info.h"

//
// weaponenginedata_t
//
// Static data for a WeaponEngine instance.
//
struct weaponenginedata_t
{
   const char   *name;         // name of the weapon engine, ie., for EDF
   weaponinfo_t *weaponinfo;   // pointer to weaponinfo table
   int           numweapons;   // number of weapons
   int           nochange;     // "nochange" value (calculated as numweapons+1)
   fixed_t       weaponbottom; // bottom-of-screen coordinate for lower/bringup
};

//
// WeaponEngine
// 
// Base class for all other weapon engines. Contains generic logic adapted from
// p_pspr code minus various game-specific hacks, which are relegated to classes
// deriving from this one.
//
class WeaponEngine
{
protected:
   const weaponenginedata_t &data; // static engine data

   // Protected methods

   // Bring up weapon from bottom of screen to normal position
   void bringUpWeapon(player_t *player);

   // Protected overridables

   // Override to redirect states during setPsprite methods
   virtual statenum_t redirectStateOnSet(statenum_t stnum) { return stnum; }

public:
   WeaponEngine(const weaponenginedata_t &edata) : data(edata)
   {
   }

   // Set player's psprite from a pointer value
   void setPspritePtr(player_t *player, pspdef_t *psp, statenum_t stnum);

   // Set player's psprite from psprite index in position
   void setPsprite(player_t *player, int position, statenum_t stnum);

   // Get a player's currently selected weapon definition
   weaponinfo_t *getReadyWeapon(player_t *player);

   // Get a player's pending weapon definition
   weaponinfo_t *getPendingWeapon(player_t *player);

   // Get a player's weapon at a particular index in the weaponinfo table
   weaponinfo_t *getPlayerWeapon(player_t *player, int index);

   // Check if a weapon has ammo available in the player's inventory
   bool weaponHasAmmo(player_t *player, weaponinfo_t *weapon);
};

#endif

// EOF

