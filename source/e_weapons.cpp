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

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"
#include "i_system.h"

#include "Confuse/confuse.h"
#include "e_edf.h"
#include "e_hash.h"
#include "e_lib.h"
#include "e_weapons.h"

#include "d_dehtbl.h"
#include "d_items.h"

//=============================================================================
//
// Globals
//

//
// Weapon Slots
//
// There are up to 16 possible slots for weapons to stack in, but any number
// of weapons can stack in each slot. The correlation to the weapon action 
// binding used to cycle through weapons in that slot is direct. The order of
// weapons in the slot is determined by their relative priorities.
//
weaponslot_t *weaponslots[NUMWEAPONSLOTS];

//=============================================================================
//
// Weapon Flags
//

//static dehflags_t e_weaponFlags[] =
//{
//   { "NOTHRUST",     WPF_NOTHRUST     },
//   { "NOHITGHOSTS",  WPF_NOHITGHOSTS  },
//   { "NOTSHAREWARE", WPF_NOTSHAREWARE },
//   { "SILENCER",     WPF_SILENCER     },
//   { "SILENT",       WPF_SILENT       },
//   { "NOAUTOFIRE",   WPF_NOAUTOFIRE   },
//   { "FLEEMELEE",    WPF_FLEEMELEE    },
//   { "ALWAYSRECOIL", WPF_ALWAYSRECOIL },
//   { "HAPTICRECOIL", WPF_HAPTICRECOIL },
//   { NULL,           0                }
//};

//static dehflagset_t e_weaponFlagSet =
//{
//   e_weaponFlags, // flags
//   0              // mode
//};

//=============================================================================
//
// Weapon Hash Tables
//

static 
   EHashTable<weaponinfo_t, EIntHashKey, &weaponinfo_t::id, &weaponinfo_t::idlinks> 
   e_WeaponIDHash;

static
   EHashTable<weaponinfo_t, ENCStringHashKey, &weaponinfo_t::name, &weaponinfo_t::namelinks>
   e_WeaponNameHash;

//
// E_WeaponForID
//
// Obtain a weaponinfo_t structure for its ID number.
//
weaponinfo_t *E_WeaponForID(int id)
{
   return e_WeaponIDHash.objectForKey(id);
}

//
// E_WeaponForName
//
// Obtain a weaponinfo_t structure by name.
//
weaponinfo_t *E_WeaponForName(const char *name)
{
   return e_WeaponNameHash.objectForKey(name);
}

// EOF

