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
#include "e_mod.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_weapons.h"
#include "m_qstr.h"

#include "d_dehtbl.h"
#include "d_items.h"
#include "p_inter.h"

#include "e_player.h" // DO NOT MOVE. COMPILE FAILS IF MOVED BEFORE p_inter.h INCLUDE

// Weapon Keywords
// TODO: Currently in order of weaponinfo_t, reorder

//#define ITEM_WPN_NAME         "name"
#define ITEM_WPN_AMMO         "ammotype"
#define ITEM_WPN_UPSTATE      "upstate"
#define ITEM_WPN_DOWNSTATE    "downstate"
#define ITEM_WPN_READYSTATE   "readystate"
#define ITEM_WPN_ATKSTATE     "attackstate"
#define ITEM_WPN_FLASHSTATE   "flashstate"
#define ITEM_WPN_AMMOPERSHOT  "ammouse"

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


#define ITEM_WPN_INHERITS     "inherits"

// WeaponInfo Delta Keywords
#define ITEM_DELTA_NAME "name"



#define WEAPONINFO_FIELDS \
   CFG_STR(ITEM_WPN_AMMO,         "",       CFGF_NONE), \
   CFG_STR(ITEM_WPN_UPSTATE,      "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_WPN_DOWNSTATE,    "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_WPN_READYSTATE,   "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_WPN_ATKSTATE,     "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_WPN_FLASHSTATE,   "S_NULL", CFGF_NONE), \
   CFG_INT(ITEM_WPN_AMMOPERSHOT,  0,        CFGF_NONE), \
   CFG_STR(ITEM_WPN_NEXTINCYCLE,  "",       CFGF_NONE), \
   CFG_STR(ITEM_WPN_PREVINCYCLE,  "",       CFGF_NONE), \
   CFG_STR(ITEM_WPN_FLAGS,        "",       CFGF_NONE), \
   CFG_STR(ITEM_WPN_ADDFLAGS,     "",       CFGF_NONE), \
   CFG_STR(ITEM_WPN_REMFLAGS,     "",       CFGF_NONE), \
   CFG_STR(ITEM_WPN_MOD,          "",       CFGF_NONE), \
   CFG_INT(ITEM_WPN_RECOIL,       0,        CFGF_NONE), \
   CFG_INT(ITEM_WPN_HAPTICRECOIL, 0,        CFGF_NONE), \
   CFG_INT(ITEM_WPN_HAPTICTIME,   0,        CFGF_NONE), \
   CFG_STR(ITEM_WPN_UPSOUND,      "none",   CFGF_NONE), \
   CFG_END()

cfg_opt_t edf_wpninfo_opts[] =
{
   //CFG_TPROPS(wpninfo_tprops,       CFGF_NOCASE),
   CFG_STR(ITEM_WPN_INHERITS,  0, CFGF_NONE),
   WEAPONINFO_FIELDS
};

cfg_opt_t edf_wdelta_opts[] =
{
   CFG_STR(ITEM_DELTA_NAME, 0, CFGF_NONE),
   WEAPONINFO_FIELDS
};

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

#undef  IS_SET
#define IS_SET(name) ((def && !inherits) || cfg_size(weapon, (name)) > 0)

static void E_processWeaponInfo(int i, cfg_t *weapon, bool def)
{
   bool inherits = false;

   weaponinfo_t *weaponinfo = nullptr;
   weaponinfo = estructalloc(weaponinfo_t, 1);

   const char *tempstr;
   int tempint;

   weaponinfo->id = i;

   tempstr = cfg_title(weapon);
   weaponinfo->name = tempstr;

   if(IS_SET(ITEM_WPN_AMMO))
   {
      tempstr = cfg_getstr(weapon, ITEM_WPN_AMMO);
      weaponinfo->ammo = E_ItemEffectForName(tempstr);
   }

   if(IS_SET(ITEM_WPN_UPSTATE))
   {
      tempstr = cfg_getstr(weapon, ITEM_WPN_UPSTATE);
      weaponinfo->upstate = E_GetStateNumForName(tempstr);
   }

   if(IS_SET(ITEM_WPN_DOWNSTATE))
   {
      tempstr = cfg_getstr(weapon, ITEM_WPN_DOWNSTATE);
      weaponinfo->downstate = E_GetStateNumForName(tempstr);
   }
   if(IS_SET(ITEM_WPN_READYSTATE))
   {
      tempstr = cfg_getstr(weapon, ITEM_WPN_READYSTATE);
      weaponinfo->readystate = E_GetStateNumForName(tempstr);
   }
   if(IS_SET(ITEM_WPN_ATKSTATE))
   {
      tempstr = cfg_getstr(weapon, ITEM_WPN_ATKSTATE);
      weaponinfo->atkstate = E_GetStateNumForName(tempstr);
   }
   if(IS_SET(ITEM_WPN_FLASHSTATE))
   {
      tempstr = cfg_getstr(weapon, ITEM_WPN_FLASHSTATE);
      weaponinfo->flashstate = E_GetStateNumForName(tempstr);
   }
   else
      weaponinfo->flashstate = E_SafeState(S_NULL);

   if(IS_SET(ITEM_WPN_AMMOPERSHOT))
      weaponinfo->ammopershot = cfg_getint(weapon, ITEM_WPN_AMMOPERSHOT);

   if(IS_SET(ITEM_WPN_MOD))
   {
      tempstr = cfg_getstr(weapon, ITEM_WPN_MOD);
      weaponinfo->mod = E_DamageTypeNumForName(tempstr);
   }
   else
      weaponinfo->mod = 0; // MOD_UNKNOWN

   // 02/19/04: process combined flags first
   if(IS_SET(ITEM_WPN_FLAGS))
   {
      tempstr = cfg_getstr(weapon, ITEM_WPN_FLAGS);
      if(*tempstr == '\0')
      {
         weaponinfo->flags = 0;
      }
      else
      {
         unsigned int results = E_ParseFlags(tempstr, &e_weaponFlagSet);
         weaponinfo->flags = results;
      }
   }

   if(IS_SET(ITEM_WPN_RECOIL))
      weaponinfo->recoil = cfg_getint(weapon, ITEM_WPN_RECOIL);
   if(IS_SET(ITEM_WPN_HAPTICRECOIL))
      weaponinfo->hapticrecoil = cfg_getint(weapon, ITEM_WPN_HAPTICRECOIL);
   if(IS_SET(ITEM_WPN_HAPTICTIME))
      weaponinfo->haptictime = cfg_getint(weapon, ITEM_WPN_HAPTICTIME);

   if(IS_SET(ITEM_WPN_UPSOUND))
   {
      tempstr = ITEM_WPN_UPSOUND;
      sfxinfo_t *tempsfx = E_EDFSoundForName(tempstr);
      if(tempsfx)
         weaponinfo->upsound = E_EDFSoundForName(tempstr)->dehackednum;
   }

   e_WeaponIDHash.addObject(*weaponinfo);
   e_WeaponNameHash.addObject(*weaponinfo);
}

static void E_processWeaponCycle(cfg_t *weapon)
{
   const char *tempstr = cfg_title(weapon);
   if((tempstr = cfg_getstr(weapon, ITEM_WPN_NEXTINCYCLE)))
      weaponinfo->nextInCycle = E_WeaponForName(tempstr);
   if((tempstr = cfg_getstr(weapon, ITEM_WPN_PREVINCYCLE)))
      weaponinfo->prevInCycle = E_WeaponForName(tempstr);
}

void E_ProcessWeapons(cfg_t *cfg)
{
   unsigned int numWeapons = cfg_size(cfg, EDF_SEC_WEAPONINFO);

   E_EDFLogPuts("\t* Processing gameproperties\n");

   for(unsigned int i = 0; i < numWeapons; i++)
      E_processWeaponInfo(i, cfg_getnsec(cfg, EDF_SEC_WEAPONINFO, i), false);

   for(unsigned int i = 0; i < numWeapons; i++)
      E_processWeaponCycle(cfg_getnsec(cfg, EDF_SEC_WEAPONINFO, i));
}

// EOF

