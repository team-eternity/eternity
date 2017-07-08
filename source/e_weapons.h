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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:  
//   Dynamic Weapons System
//
//-----------------------------------------------------------------------------

#ifndef E_WEAPONS_H__
#define E_WEAPONS_H__

#include "m_dllist.h"

struct weaponinfo_t;

// Hard-coded names for specially treated weapons (needed in DeHackEd, etc.)
// INVENTORY_TODO: many may become unneeded when P_TouchSpecialThing is finished
#define WEAPNAME_FIST     "Fist"
#define WEAPNAME_PISTOL   "Pistol"
#define WEAPNAME_SHOTGUN  "Shotgun"
#define WEAPNAME_CHAINGUN "Chaingun"
#define WEAPNAME_MISSILE  "MissileLauncher"
#define WEAPNAME_PLASMA   "PlasmaRifle"
#define WEAPNAME_BFG9000  "BFG9000"
#define WEAPNAME_CHAINSAW "Chainsaw"
#define WEAPNAME_SSG      "SuperShotgun"

#ifdef NEED_EDF_DEFINITIONS

// Section Names
#define EDF_SEC_WEAPONINFO "weaponinfo"
#define EDF_SEC_WPNDELTA   "weapondelta"

// Section Options
extern cfg_opt_t edf_wpninfo_opts[];
extern cfg_opt_t edf_wdelta_opts[];

#endif

// Structures
struct cfg_t;
struct weaponslot_t
{
   weaponinfo_t *weapon;           // weapon in the slot
   DLListItem<weaponslot_t> links; // link to next weapon in the same slot
};

#define NUMWEAPONSLOTS 16

extern weaponslot_t *weaponslots[NUMWEAPONSLOTS];

// Global Functions
weaponinfo_t *E_WeaponForID(int id);
weaponinfo_t *E_WeaponForName(const char *name);
weaponinfo_t *E_WeaponForDEHNum(int dehnum);

// FIXME: Reorder to be cleaner
void E_CollectWeapons(cfg_t *cfg);



void E_ProcessWeaponInfo(cfg_t *cfg);
void E_ProcessWeaponDeltas(cfg_t *cfg);

bool E_WeaponIsCurrent(const player_t *player, const char *name);
bool E_WeaponIsCurrentDEHNum(player_t *player, const int num);
bool E_PlayerOwnsWeapon(player_t *player, weaponinfo_t *weapon);
bool E_PlayerOwnsWeaponForDEHNum(player_t *player, int slot);
bool E_PlayerOwnsWeaponInSlot(player_t *player, int slot);

void E_GiveAllWeapons(player_t *player);
void E_GiveAllClassWeapons(player_t *player);

#endif

// EOF

