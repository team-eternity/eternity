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
struct cfg_t;

#ifdef NEED_EDF_DEFINITIONS

// Section Names
#define EDF_SEC_WEAPONINFO "weaponinfo"

// Section Options
extern cfg_opt_t edf_wpninfo_opts[];
extern cfg_opt_t edf_wdelta_opts[];

#endif

// Structures
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

bool E_WeaponIsCurrentDEHNum(player_t *player, const int dehnum);

bool E_PlayerOwnsWeapon(player_t *player, weaponinfo_t *weapon);
bool E_PlayerOwnsWeaponForDEHNum(player_t *player, int dehnum);


void E_CollectWeapons(cfg_t *cfg);

void E_ProcessWeaponInfo(cfg_t *cfg);

#endif

// EOF

