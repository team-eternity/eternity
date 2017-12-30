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
#include "d_player.h"
#include "e_edf.h"
#include "e_hash.h"
#include "e_lib.h"
#include "e_weapons.h"

#include "d_dehtbl.h"
#include "d_items.h"

//static weaponinfo_t **weaponinfo = nullptr;
int NUMWEAPONTYPES = 0;

// track generations
static int edf_weapon_generation = 1;

// The "Unknown" weapon info, which is required, has its type
// number resolved in E_CollectWeaponinfo
weapontype_t UnknownWeaponInfo;

// Weapon Keywords
// TODO: Reorder
#define ITEM_WPN_DEHNUM       "dehackednum"

#define ITEM_WPN_AMMO         "ammotype"
#define ITEM_WPN_UPSTATE      "upstate"
#define ITEM_WPN_DOWNSTATE    "downstate"
#define ITEM_WPN_READYSTATE   "readystate"
#define ITEM_WPN_ATKSTATE     "attackstate"
#define ITEM_WPN_FLASHSTATE   "flashstate"
#define ITEM_WPN_HOLDSTATE    "holdstate"
#define ITEM_WPN_AMMOPERSHOT  "ammouse"

#define ITEM_WPN_AMMO_ALT        "ammotype2"
#define ITEM_WPN_ATKSTATE_ALT    "attackstate2"
#define ITEM_WPN_FLASHSTATE_ALT  "flashstate2"
#define ITEM_WPN_HOLDSTATE_ALT   "holdstate2"
#define ITEM_WPN_AMMOPERSHOT_ALT "ammouse2"

#define ITEM_WPN_SELECTORDER  "selectionorder"
#define ITEM_WPN_SISTERWEAPON "sisterweapon"

#define ITEM_WPN_NEXTINCYCLE  "nextincycle"
#define ITEM_WPN_PREVINCYCLE  "previncycle"

#define ITEM_WPN_FLAGS        "flags"
#define ITEM_WPN_ADDFLAGS     "addflags"
#define ITEM_WPN_REMFLAGS     "remflags"
#define ITEM_WPN_MOD          "mod"
#define ITEM_WPN_RECOIL       "recoil"
#define ITEM_WPN_HAPTICRECOIL "hapticrecoil"
#define ITEM_WPN_HAPTICTIME   "haptictime"
#define ITEM_WPN_UPSOUND      "upsound"
#define ITEM_WPN_READYSOUND   "readysound"

#define ITEM_WPN_FIRSTDECSTATE "firstdecoratestate"

// DECORATE state block
#define ITEM_WPN_STATES        "states"

#define ITEM_WPN_INHERITS     "inherits"

// WeaponInfo Delta Keywords
#define ITEM_DELTA_NAME "name"

// Title properties 
#define ITEM_WPN_TITLE_SUPER     "superclass" 
#define ITEM_WPN_TITLE_DEHNUM    "dehackednum"


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

static dehflags_t e_weaponFlags[] =
{
   { "NOTHRUST",     WPF_NOTHRUST     },
   { "NOHITGHOSTS",  WPF_NOHITGHOSTS  },
   { "NOTSHAREWARE", WPF_NOTSHAREWARE },
   { "SILENCER",     WPF_SILENCER     },
   { "SILENT",       WPF_SILENT       },
   { "NOAUTOFIRE",   WPF_NOAUTOFIRE   },
   { "FLEEMELEE",    WPF_FLEEMELEE    },
   { "ALWAYSRECOIL", WPF_ALWAYSRECOIL },
   { "HAPTICRECOIL", WPF_HAPTICRECOIL },
   { NULL,           0                }
};

static dehflagset_t e_weaponFlagSet =
{
   e_weaponFlags, // flags
   0              // mode
};

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

static 
   EHashTable<weaponinfo_t, EIntHashKey, &weaponinfo_t::dehnum, &weaponinfo_t::dehlinks>
   e_WeaponDehHash;

//
// Obtain a weaponinfo_t structure for its ID number.
//
weaponinfo_t *E_WeaponForID(int id)
{
   return e_WeaponIDHash.objectForKey(id);
}

//
// Obtain a weaponinfo_t structure by name.
//
weaponinfo_t *E_WeaponForName(const char *name)
{
   return e_WeaponNameHash.objectForKey(name);
}

//
// Obtain a weaponinfo_t structure by name.
//
weaponinfo_t *E_WeaponForDEHNum(int dehnum)
{
   return e_WeaponDehHash.objectForKey(dehnum);
}

//
// Check if the weaponsec in the slotnum is currently equipped
// DON'T CALL THIS IN NEW CODE, IT EXISTS SOLELY FOR COMPAT.
//
bool E_WeaponIsCurrentDEHNum(player_t *player, const int dehnum)
{
   const weaponinfo_t *weapon = E_WeaponForDEHNum(dehnum);
   return weapon ? player->readyweapon->id == weapon->id : false;
}

//
// Convenience function to check if a player owns a weapon
//
bool E_PlayerOwnsWeapon(player_t *player, weaponinfo_t *weapon)
{
   return player->weaponowned[weapon->id];
}

//
// Convenience function to check if a player owns the weapon specific by dehnum
//
bool E_PlayerOwnsWeaponForDEHNum(player_t *player, int dehnum)
{
   return E_PlayerOwnsWeapon(player, E_WeaponForDEHNum(dehnum));
}

//
// EXTREME HACKS
//
void E_AddHardCodedWeaponsToHash()
{
   for(int i = 0; i < NUMWEAPONS; i++)
   {
      e_WeaponIDHash.addObject(weaponinfo[i]);
      e_WeaponNameHash.addObject(weaponinfo[i]);
      e_WeaponDehHash.addObject(weaponinfo[i]);
   }
}

// EOF

