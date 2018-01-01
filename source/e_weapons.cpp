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
#include "d_mod.h"
#include "d_player.h"
#include "e_edf.h"
#include "e_hash.h"
#include "e_inventory.h"
#include "e_lib.h"
#include "e_mod.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_weapons.h"

#include "d_dehtbl.h"
#include "d_items.h"

static weaponinfo_t **weaponinfo = nullptr;
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

cfg_opt_t wpninfo_tprops[] =
{
   CFG_STR(ITEM_WPN_TITLE_SUPER,      0, CFGF_NONE),
   CFG_INT(ITEM_WPN_TITLE_DEHNUM,    -1, CFGF_NONE),
   CFG_END()
};

#define WEAPONINFO_FIELDS \
   CFG_INT(ITEM_WPN_DEHNUM,       -1,       CFGF_NONE), \
   CFG_STR(ITEM_WPN_AMMO,         "",       CFGF_NONE), \
   CFG_STR(ITEM_WPN_UPSTATE,      "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_WPN_DOWNSTATE,    "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_WPN_READYSTATE,   "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_WPN_ATKSTATE,     "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_WPN_FLASHSTATE,   "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_WPN_HOLDSTATE,    "S_NULL", CFGF_NONE), \
   CFG_INT(ITEM_WPN_AMMOPERSHOT,  0,        CFGF_NONE), \
   CFG_STR(ITEM_WPN_AMMO_ALT,        "",       CFGF_NONE), \
   CFG_STR(ITEM_WPN_ATKSTATE_ALT,    "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_WPN_FLASHSTATE_ALT,  "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_WPN_HOLDSTATE_ALT,   "S_NULL", CFGF_NONE), \
   CFG_INT(ITEM_WPN_AMMOPERSHOT_ALT, 0,        CFGF_NONE), \
   CFG_INT(ITEM_WPN_SELECTORDER,  -1,       CFGF_NONE), \
   CFG_STR(ITEM_WPN_SISTERWEAPON, "",       CFGF_NONE), \
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
   CFG_STR(ITEM_WPN_READYSOUND,   "none",   CFGF_NONE), \
   CFG_STR(ITEM_WPN_FIRSTDECSTATE, nullptr, CFGF_NONE), \
   CFG_STR(ITEM_WPN_STATES,        0,       CFGF_NONE), \
   CFG_END()

cfg_opt_t edf_wpninfo_opts[] =
{
   CFG_TPROPS(wpninfo_tprops,       CFGF_NOCASE),
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
   { "NOTHRUST",       WPF_NOTHRUST       },
   { "NOHITGHOSTS",    WPF_NOHITGHOSTS    },
   { "NOTSHAREWARE",   WPF_NOTSHAREWARE   },
   { "ENABLEAPS",      WPF_ENABLEAPS      },
   { "SILENCER",       WPF_SILENCER       },
   { "SILENT",         WPF_SILENT         },
   { "NOAUTOFIRE",     WPF_NOAUTOFIRE     },
   { "FLEEMELEE",      WPF_FLEEMELEE      },
   { "ALWAYSRECOIL",   WPF_ALWAYSRECOIL   },
   { "HAPTICRECOIL",   WPF_HAPTICRECOIL   },
   { "READYSNDHALF",   WPF_READYSNDHALF   },
   { "AUTOSWITCHFROM", WPF_AUTOSWITCHFROM },
   { NULL,             0                  }
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
// Returns a weapon type index given its name. Returns -1
// if a weapon type is not found.
//
static weapontype_t E_weaponNumForName(const char *name)
{
   weaponinfo_t *info = nullptr;
   weapontype_t ret = -1;

   if((info = e_WeaponNameHash.objectForKey(name)))
      ret = info->id;

   return ret;
}

//
// As above, but causes a fatal error if the weapon type isn't found.
//
static weapontype_t E_getWeaponNumForName(const char *name)
{
   weapontype_t weaponnum = E_weaponNumForName(name);

   if(weaponnum == -1)
      I_Error("E_GetWeaponNumForName: bad weapon type %s\n", name);

   return weaponnum;
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
   return player->weaponowned[weapon->dehnum];
}

//
// Convenience function to check if a player owns the weapon specific by dehnum
//
bool E_PlayerOwnsWeaponForDEHNum(player_t *player, int dehnum)
{
   return E_PlayerOwnsWeapon(player, E_WeaponForDEHNum(dehnum));
}

//=============================================================================
//
// Weapon Processing
//

//
// Function to reallocate the weaponinfo array safely.
//
static void E_ReallocWeapons(int numnewweapons)
{
   static int numweaponsalloc = 0;

   // only realloc when needed
   if(!numweaponsalloc || (NUMWEAPONTYPES < numweaponsalloc + numnewweapons))
   {
      int i;

      // First time, just allocate the requested number of weaponinfo.
      // Afterward:
      // * If the number of weaponinfo requested is small, add 2 times as many
      //   requested, plus a small constant amount.
      // * If the number is large, just add that number.

      if(!numweaponsalloc)
         numweaponsalloc = numnewweapons;
      else if(numnewweapons <= 50)
         numweaponsalloc += numnewweapons * 2 + 32;
      else
         numweaponsalloc += numnewweapons;

      // reallocate weaponinfo[]
      weaponinfo = erealloc(weaponinfo_t **, weaponinfo, numweaponsalloc *
                            sizeof(weaponinfo_t *));

      for(i = NUMWEAPONTYPES; i < numweaponsalloc; i++)
         weaponinfo[i] = nullptr;
   }

   NUMWEAPONTYPES += numnewweapons;
}

//
// Pre-creates and hashes by name the weaponinfo, for purpose 
// of mutual and forward references.
//
void E_CollectWeapons(cfg_t *cfg)
{
   weapontype_t i;
   unsigned int numweapons;        // number of weaponinfo defined by the cfg
   unsigned int curnewweapon = 0;  // index of current new weaponinfo being used
   weaponinfo_t *newWeapon = nullptr;
   static bool firsttime = true;

   // get number of weaponinfos defined by the cfg
   numweapons = cfg_size(cfg, EDF_SEC_WEAPONINFO);

   // echo counts
   E_EDFLogPrintf("\t\t%u weaponinfo defined\n", numweapons);

   if(numweapons)
   {
      unsigned int firstnewweapon = 0; // index of first new weaponinfo
      // allocate weaponinfo_t structures for the new weapons
      newWeapon = estructalloc(weaponinfo_t, numweapons);

      // add space to the weaponinfo array
      curnewweapon = firstnewweapon = NUMWEAPONTYPES;

      E_ReallocWeapons(int(numweapons));

      // set pointers in weaponinfo[] to the proper structures;
      // also set self-referential index member, and allocate a
      // metatable.
      for(i = firstnewweapon; i < weapontype_t(NUMWEAPONTYPES); i++)
      {
         weaponinfo[i] = &newWeapon[i - firstnewweapon];
         weaponinfo[i]->id = i;
         //weaponinfo[i]->meta = new MetaTable("weaponinfo");
      }
   }

   // build hash tables
   E_EDFLogPuts("\t\tBuilding weapon hash tables\n");

   // cycle through the weaponinfo defined in the cfg
   for(i = 0; i < numweapons; i++)
   {
      cfg_t *weaponcfg  = cfg_getnsec(cfg, EDF_SEC_WEAPONINFO, i);
      const char *name  = cfg_title(weaponcfg);
      cfg_t *titleprops = nullptr;
      int dehnum = -1;

      // This is a new weaponinfo, whether or not one already exists by this name
      // in the hash table. For subsequent addition of EDF weaponinfo at runtime,
      // the hash table semantics of "find newest first" take care of overriding,
      // while not breaking objects that depend on the original definition of
      // the weaponinfo for inheritance purposes.
      weaponinfo_t *wi = weaponinfo[curnewweapon++];

      // initialize name
      wi->name = estrdup(name);

      // add to name & ID hash
      e_WeaponIDHash.addObject(wi);
      e_WeaponNameHash.addObject(wi);

      // check for titleprops definition first
      if(cfg_size(weaponcfg, "#title") > 0)
      {
         titleprops = cfg_gettitleprops(weaponcfg);
         if(titleprops)
            dehnum = cfg_getint(titleprops, ITEM_WPN_TITLE_DEHNUM);
      }
      
      // If undefined, check the legacy value inside the section
      if(dehnum == -1)
         dehnum = cfg_getint(weaponcfg, ITEM_WPN_DEHNUM);

      // process dehackednum and add thing to dehacked hash table,
      // if appropriate
      if((wi->dehnum = dehnum) >= 0)
         e_WeaponDehHash.addObject(wi);


      // set generation
      //wi->generation = edf_weapon_generation;
   }

   // first-time-only events
   if(firsttime)
   {
      // check that at least one weaponinfo was defined
      if(!NUMWEAPONTYPES)
         E_EDFLoggedErr(2, "E_CollectWeapons: no weaponinfo defined.\n");

      // TODO: verify the existance of the Unknown weapon type?
      //UnknownWeaponInfo = E_weaponNumForName("Unknown");
      //if(UnknownWeaponInfo < 0)
      //   E_EDFLoggedErr(2, "E_CollectWeapons: 'Unknown' weaponinfo must be defined.\n");

      firsttime = false;
   }
}

// weapon_hitlist: keeps track of what weaponinfo are initialized
static byte *weapon_hitlist = nullptr;

// weapon_pstack: used by recursive E_ProcessWeapon to track inheritance
static int  *weapon_pstack = nullptr;
static int   weapon_pindex = 0;

//
// Makes sure the weapon type being inherited from has not already
// been inherited during the current inheritance chain. Returns
// false if the check fails, and true if it succeeds.
//
static bool E_CheckWeaponInherit(int pnum)
{
   for(int i = 0; i < NUMWEAPONTYPES; i++)
   {
      // circular inheritance
      if(weapon_pstack[i] == pnum)
         return false;

      // found end of list
      if(weapon_pstack[i] == -1)
         break;
   }

   return true;
}

//
// Adds a type number to the inheritance stack.
//
static void E_AddWeaponToPStack(weapontype_t num)
{
   // Overflow shouldn't happen since it would require cyclic
   // inheritance as well, but I'll guard against it anyways.

   if(weapon_pindex >= NUMWEAPONTYPES)
      E_EDFLoggedErr(2, "E_AddWeaponToPStack: max inheritance depth exceeded\n");

   weapon_pstack[weapon_pindex++] = num;
}

//
// Resets the weaponinfo inheritance stack, setting all the pstack
// values to -1, and setting pindex back to zero.
//
static void E_ResetWeaponPStack()
{
   for(int i = 0; i < NUMWEAPONTYPES; i++)
      weapon_pstack[i] = -1;

   weapon_pindex = 0;
}
struct weapontitleprops_t
{
   const char *superclass;
   int dehackednum;
};

static void E_getWeaponTitleProps(cfg_t *weaponsec, weapontitleprops_t &props, bool def)
{
   cfg_t *titleprops;

   if(def && cfg_size(weaponsec, "#title") > 0 && (titleprops = cfg_gettitleprops(weaponsec)))
   {
      props.superclass  = cfg_getstr(titleprops, ITEM_WPN_TITLE_SUPER);
      props.dehackednum = cfg_getint(titleprops, ITEM_WPN_TITLE_DEHNUM);
   }
   else
   {
      props.superclass  = nullptr;
      props.dehackednum = -1;
   }
}

#undef  IS_SET
#define IS_SET(name) ((def && !inherits) || cfg_size(weaponsec, (name)) > 0)

//
// Process a single weaponinfo, or weapondelta
//
static void E_processWeapon(weapontype_t i, cfg_t *weaponsec, cfg_t *pcfg, bool def)
{
   int tempint;
   const char *tempstr;
   bool inherits = false;
   weapontitleprops_t titleprops;
   weaponinfo_t &wp = *weaponinfo[i];

   // if weaponsec is null, we are in the situation of inheriting from a weapon
   // that was processed in a previous EDF generation, so no processing is
   // required; return immediately.
   if(!weaponsec)
      return;

   // Retrieve title properties
   E_getWeaponTitleProps(weaponsec, titleprops, def);

   /*// inheritance -- not in deltas
   if(def)
   {
      int pnum = -1;

      // if this weapon is already processed via recursion due to
      // inheritance, don't process it again
      if(weapon_hitlist[i])
         return;

      if(titleprops.superclass || cfg_size(weaponsec, ITEM_WPN_INHERITS) > 0)
         pnum = E_resolveParentWeapon(weaponsec, titleprops);

      if(pnum >= 0)
      {
         cfg_t *parent_tngsec;
         weapontype_t pnum = E_resolveParentWeapon(weaponsec, titleprops); // Why's this here?

         // check against cyclic inheritance
         if(!E_CheckWeaponInherit(pnum))
         {
            E_EDFLoggedErr(2, "E_processWeapon: cyclic inheritance "
                               "detected in weaponinfo '%s'\n", wp.name);
         }

         // add to inheritance stack
         E_AddWeaponToPStack(pnum);

         // process parent recursively
         // must use cfg_gettsec; note can return null
         parent_tngsec = cfg_gettsec(pcfg, EDF_SEC_WEAPONINFO, weaponinfo[pnum]->name);
         E_processWeapon(pnum, parent_tngsec, pcfg, true);

         // copy parent to this weapon
         E_CopyWeapon(i, pnum);

         // keep track of parent explicitly
         wp.parent = weaponinfo[pnum];

         // we inherit, so treat defaults as no value
         inherits = true;
      }
   }*/

   if(IS_SET(ITEM_WPN_SELECTORDER))
   {
      /*if((tempint = cfg_getint(weaponsec, ITEM_WPN_SELECTORDER)) >= 0)
      {
         E_insertSelectOrderNode(tempint, &wp, !def);
         wp.sortorder = tempint;
      }*/
   }
   if(IS_SET(ITEM_WPN_SISTERWEAPON))
   {
      
   }

   if(IS_SET(ITEM_WPN_NEXTINCYCLE))
      wp.nextInCycle = E_WeaponForName(cfg_getstr(weaponsec, ITEM_WPN_NEXTINCYCLE));
   if(IS_SET(ITEM_WPN_PREVINCYCLE))
      wp.prevInCycle = E_WeaponForName(cfg_getstr(weaponsec, ITEM_WPN_PREVINCYCLE));

   // Attack properties
   if(IS_SET(ITEM_WPN_AMMO))
      wp.ammo = E_ItemEffectForName(cfg_getstr(weaponsec, ITEM_WPN_AMMO));

   if(IS_SET(ITEM_WPN_UPSTATE))
      wp.upstate = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_UPSTATE));
   if(IS_SET(ITEM_WPN_DOWNSTATE))
      wp.downstate = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_DOWNSTATE));
   if(IS_SET(ITEM_WPN_READYSTATE))
      wp.readystate = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_READYSTATE));
   if(IS_SET(ITEM_WPN_ATKSTATE))
      wp.atkstate = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_ATKSTATE));
   if(IS_SET(ITEM_WPN_FLASHSTATE))
      wp.flashstate = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_FLASHSTATE));
   //if(IS_SET(ITEM_WPN_HOLDSTATE))
   //   wp.holdstate = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_HOLDSTATE));

   if(IS_SET(ITEM_WPN_AMMOPERSHOT))
      wp.ammopershot = cfg_getint(weaponsec, ITEM_WPN_AMMOPERSHOT);


   // Alt attack properties
   /*if(IS_SET(ITEM_WPN_AMMO_ALT))
      wp.ammo_alt = E_ItemEffectForName(cfg_getstr(weaponsec, ITEM_WPN_AMMO_ALT));

   if(IS_SET(ITEM_WPN_ATKSTATE_ALT))
      wp.atkstate_alt = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_ATKSTATE_ALT));
   if(IS_SET(ITEM_WPN_FLASHSTATE_ALT))
      wp.flashstate_alt = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_FLASHSTATE_ALT));
   if(IS_SET(ITEM_WPN_HOLDSTATE_ALT))
      wp.holdstate_alt = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_HOLDSTATE_ALT));

   if(IS_SET(ITEM_WPN_AMMOPERSHOT_ALT))
      wp.ammopershot_alt = cfg_getint(weaponsec, ITEM_WPN_AMMOPERSHOT_ALT);*/

   if(IS_SET(ITEM_WPN_MOD))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_MOD);
      wp.mod = E_DamageTypeNumForName(tempstr);
   }
   else
      wp.mod = MOD_UNKNOWN;

   // process combined flags first
   if(IS_SET(ITEM_WPN_FLAGS))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_FLAGS);
      if(estrempty(tempstr))
         wp.flags = 0;
      else
      {
         unsigned int results = E_ParseFlags(tempstr, &e_weaponFlagSet);
         wp.flags = results;
      }
   }

   // process addflags and remflags modifiers

   if(cfg_size(weaponsec, ITEM_WPN_ADDFLAGS) > 0)
   {      
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_ADDFLAGS);

      unsigned int results = E_ParseFlags(tempstr, &e_weaponFlagSet);
      wp.flags |= results;
   }

   if(cfg_size(weaponsec, ITEM_WPN_REMFLAGS) > 0)
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_REMFLAGS);

      unsigned int results = E_ParseFlags(tempstr, &e_weaponFlagSet);
      wp.flags &= ~results;
   }

   if(IS_SET(ITEM_WPN_RECOIL))
      wp.recoil = cfg_getint(weaponsec, ITEM_WPN_RECOIL);
   if(IS_SET(ITEM_WPN_HAPTICRECOIL))
      wp.hapticrecoil = cfg_getint(weaponsec, ITEM_WPN_HAPTICRECOIL);
   if(IS_SET(ITEM_WPN_HAPTICTIME))
      wp.haptictime = cfg_getint(weaponsec, ITEM_WPN_HAPTICTIME);

   if(IS_SET(ITEM_WPN_UPSOUND))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_UPSOUND);
      sfxinfo_t *tempsfx = E_EDFSoundForName(tempstr);
      if(tempsfx)
         wp.upsound = tempsfx->dehackednum;
   }

   /*if(IS_SET(ITEM_WPN_READYSOUND))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_READYSOUND);
      sfxinfo_t *tempsfx = E_EDFSoundForName(tempstr);
      if(tempsfx)
         wp.readysound = tempsfx->dehackednum;
   }*/

   // Process DECORATE state block
   //E_ProcessDecorateWepStatesRecursive(weaponsec, i, false);
}

//
// Process all weapons!
//
void E_ProcessWeaponInfo(cfg_t *cfg)
{
   E_EDFLogPuts("\t* Processing weaponinfo data\n");

   const unsigned int numWeapons = cfg_size(cfg, EDF_SEC_WEAPONINFO);

   // allocate inheritance stack and hitlist
   weapon_hitlist = ecalloc(byte *, NUMWEAPONTYPES, sizeof(byte));
   weapon_pstack  = ecalloc(int *, NUMWEAPONTYPES, sizeof(int));

   for(weapontype_t i = 0; i < NUMWEAPONTYPES; i++)
   {
      if(weaponinfo[i]->generation != edf_weapon_generation)
         weapon_hitlist[i] = 1;
   }

   for(unsigned int i = 0; i < numWeapons; i++)
   {
      cfg_t *weaponsec = cfg_getnsec(cfg, EDF_SEC_WEAPONINFO, i);
      const char *name = cfg_title(weaponsec);
      weapontype_t weaponnum = E_weaponNumForName(name);

      // reset the inheritance stack
      E_ResetWeaponPStack();

      // add this weapon to the stack
      E_AddWeaponToPStack(weaponnum);

      E_processWeapon(weaponnum, weaponsec, cfg, true);

      E_EDFLogPrintf("\t\tFinished weaponinfo %s (#%d)\n",
                     weaponinfo[weaponnum]->name, weaponnum);
   }

   // free tables
   efree(weapon_hitlist);
   efree(weapon_pstack);

   // increment generation count
   ++edf_weapon_generation;
}

// EOF

