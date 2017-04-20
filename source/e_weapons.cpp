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
#include "doomdef.h"
#include "e_dstate.h"
#include "e_edf.h"
#include "e_hash.h"
#include "e_lib.h"
#include "e_metastate.h"
#include "e_mod.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h" // TODO: Move E_SplitTypeAndState to e_states and remove this include?
#include "metaapi.h"

#include "d_dehtbl.h"
#include "d_items.h"
#include "p_inter.h"

#include "e_weapons.h"
#include "e_player.h" // DO NOT MOVE. COMPILE FAILS IF MOVED BEFORE p_inter.h INCLUDE

static weaponinfo_t **weaponinfo = nullptr;
static int NUMWEAPONTYPES = 0;

// track generations
static int edf_weapon_generation = 1;

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
#define ITEM_WPN_READYSOUND   "readysound"

#define ITEM_WPN_TRACKER      "tracker"

#define ITEM_WPN_FIRSTDECSTATE "firstdecoratestate"

// DECORATE state block
#define ITEM_WPN_STATES        "states"

#define ITEM_WPN_INHERITS     "inherits"

// WeaponInfo Delta Keywords
#define ITEM_DELTA_NAME "name"

// Title properties 
#define ITEM_WPN_TITLE_SUPER     "superclass" 

cfg_opt_t wpninfo_tprops[] =
{
   CFG_STR(ITEM_WPN_TITLE_SUPER,      0, CFGF_NONE),
   CFG_END()
};

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
   CFG_STR(ITEM_WPN_READYSOUND,   "none",    CFGF_NONE), \
   CFG_STR(ITEM_WPN_TRACKER,      "",       CFGF_NONE), \
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
// of weapons can stack in each slot. The correlation to the weaponsec action 
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
   { "READYSNDHALF", WPF_READYSNDHALF },
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

//
// Returns a thing type index given its name. Returns -1
// if a thing type is not found.
//
int E_WeaponNumForName(const char *name)
{
   weaponinfo_t *info = nullptr;
   int ret = -1;

   if((info = e_WeaponNameHash.objectForKey(name)))
      ret = info->id;

   return ret;
}

//
// As above, but causes a fatal error if the thing type isn't found.
//
int E_GetWeaponNumForName(const char *name)
{
   int weaponnum = E_WeaponNumForName(name);

   if(weaponnum == -1)
      I_Error("E_GetThingNumForName: bad thing type %s\n", name);

   return weaponnum;
}

// 
// Processing Functions
//

// TODO: I dunno, something about the below comment
// Warning: likely to get lost as there's no module for metastates currently!
//IMPLEMENT_RTTI_TYPE(MetaState)

//
// Adds a state to the mobjinfo metatable.
//
static void E_AddMetaState(weaponinfo_t *wi, state_t *state, const char *name)
{
   wi->meta->addObject(new MetaState(name, state));
}

static const char *nativeWepStateLabels[] =
{
   "Select",   // upstate
   "Deselect", // downstate
   "Ready",    // readystate
   "Fire",     // atkstate
   "Flash",    // flashstate
   "Hold",     // holdstate
};

enum wepstatetypes_e
{
   WSTATE_SELECT,
   WSTATE_DESELECT,
   WSTATE_READY,
   WSTATE_FIRE,
   WSTATE_FLASH,
   WSTATE_HOLD
};

#define NUMNATIVEWSTATES earrlen(nativeWepStateLabels)

//
// Gets a state that is stored inside an weaponinfo metatable.
// Returns null if no such object exists.
//
static MetaState *E_GetMetaState(weaponinfo_t *wi, const char *name)
{
   return wi->meta->getObjectKeyAndTypeEx<MetaState>(name);
}

//
// Returns a pointer to an weaponinfo's native state field if the given name
// is a match for that field's corresponding DECORATE label. Returns null
// if the name is not a match for a native state field.
//
int *E_GetNativeWepStateLoc(weaponinfo_t *wi, const char *label)
{
   int nativenum = E_StrToNumLinear(nativeWepStateLabels, NUMNATIVEWSTATES, label);
   int *ret = nullptr;

   switch(nativenum)
   {
   case WSTATE_SELECT:   ret = &wi->upstate;    break;
   case WSTATE_DESELECT: ret = &wi->downstate;  break;
   case WSTATE_READY:    ret = &wi->readystate; break;
   case WSTATE_FIRE:     ret = &wi->atkstate;   break;
   case WSTATE_FLASH:    ret = &wi->flashstate; break;
   case WSTATE_HOLD:     ret = &wi->holdstate;  break;
   default:
      break;
   }

   return ret;
}

//
// Retrieves any state for an weaponinfo, either native or metastate.
// Returns null if no such state can be found. Note that the null state is
// not considered a valid state.
//
state_t *E_GetStateForWeaponInfo(weaponinfo_t *wi, const char *label)
{
   MetaState *ms;
   state_t *ret = nullptr;
   int *nativefield = nullptr;

   // check metastates
   if((ms = E_GetMetaState(wi, label)))
      ret = ms->state;
   else if((nativefield = E_GetNativeWepStateLoc(wi, label)))
   {
      // only if not S_NULL
      if(*nativefield != NullStateNum)
         ret = states[*nativefield];
   }

   return ret;
}

//
// Returns an weaponinfo_t * if the given weaponinfo inherits from the given type 
// by name. Returns null otherwise. Self-identity is *not* considered 
// inheritance.
//
weaponinfo_t *E_IsWeaponInfoDescendantOf(weaponinfo_t *wi, const char *type)
{
   weaponinfo_t *curwi = wi->parent;
   int targettype = E_WeaponNumForName(type);

   while(curwi)
   {
      if(curwi->id == targettype)
         break; // found it

                // walk up the inheritance tree
      curwi = curwi->parent;
   }

   return curwi;
}

//
// Deal with unresolved goto entries in the DECORATE state object.
//
static void E_processDecorateWepGotos(weaponinfo_t *wi, edecstateout_t *dso)
{
   int i;

   for(i = 0; i < dso->numgotos; ++i)
   {
      weaponinfo_t *type = nullptr;
      state_t *state;
      statenum_t statenum;
      char *statename = nullptr;

      // see if the label contains a colon, and if so, it may be an
      // access to an inherited state
      char *colon = strchr(dso->gotos[i].label, ':');

      if(colon)
      {
         char *typestr = nullptr;

         E_SplitTypeAndState(dso->gotos[i].label, &typestr, &statename);

         if(!(typestr && statename))
         {
            // error, set to null state
            *(dso->gotos[i].nextstate) = NullStateNum;
            continue;
         }

         // if "super" this means find the state in the parent type;
         // otherwise, check if the indicated type inherits from this one
         if(!strcasecmp(typestr, "super") && wi->parent)
            type = wi->parent;
         else
            type = E_IsWeaponInfoDescendantOf(wi, typestr);
      }
      else
      {
         // otherwise this is a name to resolve within the scope of the local
         // thingtype - it may be a state acquired through inheritance, for
         // example, and is thus not defined within the thingtype's state block.
         type = wi;
         statename = dso->gotos[i].label;
      }

      if(!type)
      {
         // error; set to null state and continue
         *(dso->gotos[i].nextstate) = NullStateNum;
         continue;
      }

      // find the state in the proper type
      if(!(state = E_GetStateForWeaponInfo(type, statename)))
      {
         // error; set to null state and continue
         *(dso->gotos[i].nextstate) = NullStateNum;
         continue;
      }

      statenum = state->index + dso->gotos[i].offset;

      if(statenum < 0 || statenum >= NUMSTATES)
      {
         // error; set to null state and continue
         *(dso->gotos[i].nextstate) = NullStateNum;
         continue;
      }

      // resolve the goto
      *(dso->gotos[i].nextstate) = statenum;
   }
}

//
// Add all labeled states from a DECORATE state block to the given weaponinfo.
//
static void E_processDecorateWepStates(weaponinfo_t *wi, edecstateout_t *dso)
{
   int i;

   for(i = 0; i < dso->numstates; ++i)
   {
      int *nativefield;

      // first see if this is a native state
      if((nativefield = E_GetNativeWepStateLoc(wi, dso->states[i].label)))
         *nativefield = dso->states[i].state->index;
      else
      {
         MetaState *msnode;

         // there is not a matching native field, so add the state as a 
         // metastate
         if((msnode = E_GetMetaState(wi, dso->states[i].label)))
            msnode->state = dso->states[i].state;
         else
            E_AddMetaState(wi, dso->states[i].state, dso->states[i].label);
      }
   }
}


//
// Processes the DECORATE state list in a weapon
//
static void E_ProcessDecorateWepStateList(weaponinfo_t *wi, const char *str,
                                       const char *firststate, bool recursive)
{
   edecstateout_t *dso;

   if(!(dso = E_ParseDecorateStates(str, firststate)))
   {
      E_EDFLoggedWarning(2, "Warning: couldn't attach DECORATE states to weapon '%s'.\n",
                         wi->name);
      return;
   }

   // first deal with any gotos that still need resolution
   if(dso->numgotos)
      E_processDecorateWepGotos(wi, dso);

   // add all labeled states from the block to the weaponinfo
   if(dso->numstates && !recursive)
      E_processDecorateWepStates(wi, dso);


   // free the DSO object
   E_FreeDSO(dso);
}

//
// Process DECORATE-format states
//
static void E_ProcessDecorateWepStatesRecursive(cfg_t *weaponsec, int wnum, bool recursive)
{
   cfg_t *displaced;

   // 01/02/12: Process displaced sections recursively first.
   if((displaced = cfg_displaced(weaponsec)))
      E_ProcessDecorateWepStatesRecursive(displaced, wnum, true);

   // haleyjd 06/22/10: Process DECORATE state block
   if(cfg_size(weaponsec, ITEM_WPN_STATES) > 0)
   {
      // 01/01/12: allow use of pre-existing reserved states; they must be
      // defined consecutively in EDF and should be flagged +decorate in order
      // for values inside them to be overridden by the DECORATE state block.
      // If this isn't being done, firststate will be null.
      const char *firststate = cfg_getstr(weaponsec, ITEM_WPN_FIRSTDECSTATE);
      const char *tempstr = cfg_getstr(weaponsec, ITEM_WPN_STATES);

      // recursion should process states only if firststate is valid
      if(!recursive || firststate)
         E_ProcessDecorateWepStateList(weaponinfo[wnum], tempstr, firststate, recursive);
   }

}

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
   unsigned int i;
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
      for(i = firstnewweapon; i < unsigned int(NUMWEAPONTYPES); i++)
      {
         weaponinfo[i] = &newWeapon[i - firstnewweapon];
         weaponinfo[i]->id = i;
         weaponinfo[i]->meta = new MetaTable("weaponinfo");
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

      // This is a new weaponinfo, whether or not one already exists by this name
      // in the hash table. For subsequent addition of EDF weapontypes at runtime,
      // the hash table semantics of "find newest first" take care of overriding,
      // while not breaking objects that depend on the original definition of
      // the weapontype for inheritance purposes.
      weaponinfo_t *wi = weaponinfo[curnewweapon++];

      // initialize name
      wi->name = estrdup(name);

      // add to name & ID hash
      e_WeaponIDHash.addObject(wi);
      e_WeaponNameHash.addObject(wi);

      // TODO: dehnum stuff maybe at some point?
      /*// check for titleprops definition first
      if(cfg_size(weaponcfg, "#title") > 0)
      {
         titleprops = cfg_gettitleprops(weaponcfg);
         if(titleprops)
            dehnum = cfg_getint(titleprops, ITEM_WPN_TITLE_DEHNUM)
      }

      // If undefined, check the legacy value inside the section
      if(dehnum == -1)
         dehnum = cfg_getint(weaponnum, ITEM_WPN_DEHNUM);*/


      // set generation
      wi->generation = edf_weapon_generation;
   }

   // first-time-only events
   if(firsttime)
   {
      // check that at least one weaponinfo was defined
      if(!NUMWEAPONTYPES)
         E_EDFLoggedErr(2, "E_CollectWeapons: no weaponinfo defined.\n");

      // TODO: verify the existance of the Unknown weapon type?
      /*UnknownWeaponInfo = E_WeaponNumForName("Unknown");
      if(UnknownWeaponInfo < 0)
         E_EDFLoggedErr(2, "E_CollectWeapons: 'Unknown' weaponinfo must be defined.\n");*/

      firsttime = false;
   }
}

// weapon_hitlist: keeps track of what thingtypes are initialized
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
static void E_AddWeaponToPStack(int num)
{
   // Overflow shouldn't happen since it would require cyclic
   // inheritance as well, but I'll guard against it anyways.

   if(weapon_pindex >= NUMWEAPONTYPES)
      E_EDFLoggedErr(2, "E_AddWeaponToPStack: max inheritance depth exceeded\n");

   weapon_pstack[weapon_pindex++] = num;
}

//
// Resets the weapontype inheritance stack, setting all the pstack
// values to -1, and setting pindex back to zero.
//
static void E_ResetWeaponPStack()
{
   for(int i = 0; i < NUMWEAPONTYPES; i++)
      weapon_pstack[i] = -1;

   weapon_pindex = 0;
}

//
// Copies one weaponinfo into another.
//
static void E_CopyWeapon(int num, int pnum)
{
   weaponinfo_t *this_mi;
   DLListItem<weaponinfo_t> idlinks, namelinks;
   const char *name;
   MetaTable  *meta;
   int         id;
   int         generation;
   weaponinfo_t *nextInCycle, *prevInCycle;

   this_mi = weaponinfo[num];

   // must save the following fields in the destination thing:
   idlinks = this_mi->idlinks;
   namelinks = this_mi->namelinks;
   name = this_mi->name;
   meta = this_mi->meta;
   id = this_mi->id;
   generation = this_mi->generation;
   nextInCycle = this_mi->nextInCycle;
   prevInCycle = this_mi->prevInCycle;

   // copy from source to destination
   memcpy(this_mi, weaponinfo[pnum], sizeof(weaponinfo_t));

   // normalize special fields?
   // FIXME: IDK

   // copy metatable
   meta->copyTableFrom(weaponinfo[pnum]->meta);

   // restore metatable pointer
   this_mi->meta = meta;

   // must restore name and dehacked num data
   this_mi->namelinks = namelinks;
   this_mi->idlinks = idlinks;
   this_mi->name = name;
   this_mi->id = id;
   this_mi->generation = generation;
   this_mi->nextInCycle = nextInCycle;
   this_mi->prevInCycle = prevInCycle;

   // other fields not inherited:

   // force tracker of inheriting type to nullptr
   this_mi->tracker = nullptr;
}


static const char *E_getWeaponTitleProps(cfg_t *weaponsec, bool def)
{
   cfg_t *titleprops;

   if(def && cfg_size(weaponsec, "#title") > 0 &&
      (titleprops = cfg_gettitleprops(weaponsec)))
      return cfg_getstr(titleprops, ITEM_WPN_TITLE_SUPER);
   else
      return nullptr;
}

//
// Get the weaponinfo index for the thing's superclass weapontype.
//
static int E_resolveParentWeapon(cfg_t *thingsec, const char *tprop)
{
   int pnum = -1;

   // check title props first
   if(tprop)
   {
      // "Weapon" is currently a dummy value and means it is just a plain 
      // thing not inheriting from anything else. Maybe in the future it
      // could designate specialized native subclasses of Weapon as well?
      if(strcasecmp(tprop, "Weapon"))
         pnum = E_GetWeaponNumForName(tprop);
   }
   else // resolve parent thingtype through legacy "inherits" field
      pnum = E_GetWeaponNumForName(cfg_getstr(thingsec, ITEM_WPN_INHERITS));

   return pnum;
}

#undef  IS_SET
#define IS_SET(name) ((def && !inherits) || cfg_size(weaponsec, (name)) > 0)

//
// Process a single weaponinfo, or weapondelta
//
static void E_processWeapon(int i, cfg_t *weaponsec, cfg_t *pcfg, bool def)
{
   double tempfloat;
   int tempint;
   const char *tempstr;
   bool inherits = false;
   bool cflags   = false;
   const char *tprop;

   // if weaponsec is null, we are in the situation of inheriting from a thing
   // that was processed in a previous EDF generation, so no processing is
   // required; return immediately.
   if(!weaponsec)
      return;

   // Retrieve title properties
   tprop = E_getWeaponTitleProps(weaponsec, def);

   // inheritance -- not in deltas
   if(def)
   {
      int pnum = -1;

      // if this weapon is already processed via recursion due to
      // inheritance, don't process it again
      if(weapon_hitlist[i])
         return;

      if(tprop || cfg_size(weaponsec, ITEM_WPN_INHERITS) > 0)
         pnum = E_resolveParentWeapon(weaponsec, tprop);

      if(pnum >= 0)
      {
         cfg_t *parent_tngsec;
         int pnum = E_resolveParentWeapon(weaponsec, tprop); // TODO: Remove this line?

         // check against cyclic inheritance
         if(!E_CheckWeaponInherit(pnum))
         {
            E_EDFLoggedErr(2,
               "E_processWeapon: cyclic inheritance detected in weaponinfo '%s'\n",
               weaponinfo[i]->name);
         }

         // add to inheritance stack
         E_AddWeaponToPStack(pnum);

         // process parent recursively
         // must use cfg_gettsec; note can return null
         parent_tngsec = cfg_gettsec(pcfg, EDF_SEC_WEAPONINFO, weaponinfo[pnum]->name);
         E_processWeapon(pnum, parent_tngsec, pcfg, true);

         // copy parent to this thing
         E_CopyWeapon(i, pnum);

         // keep track of parent explicitly
         weaponinfo[i]->parent = weaponinfo[pnum];

         // we inherit, so treat defaults as no value
         inherits = true;
      }
   }

   tempstr = cfg_title(weaponsec);
   if(IS_SET(ITEM_WPN_NEXTINCYCLE))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_NEXTINCYCLE);
      weaponinfo[i]->nextInCycle = E_WeaponForName(tempstr);
   }
   if(IS_SET(ITEM_WPN_PREVINCYCLE))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_PREVINCYCLE);
      weaponinfo[i]->prevInCycle = E_WeaponForName(tempstr);
   }

   if(IS_SET(ITEM_WPN_TRACKER))
   {
      const char *tempeffectname = cfg_getstr(weaponsec, ITEM_WPN_TRACKER);
      itemeffect_t *tempeffect;
      if((tempeffect = E_ItemEffectForName(tempeffectname)))
         weaponinfo[i]->tracker = tempeffect;
      else // A weaponsec is worthless without its tracker
         E_EDFLoggedErr(2, "Tracker \"%s\" in weaponinfo[i] \"%s\" is undefined\n", tempeffectname, tempstr);
   }
   else if(def) // Don't check tracker, since it was already checked in initial definition
      E_EDFLoggedErr(2, "No tracker defined for weaponsec \"%s\"\n", tempstr);

   if(IS_SET(ITEM_WPN_AMMO))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_AMMO);
      weaponinfo[i]->ammo = E_ItemEffectForName(tempstr);
   }

   if(IS_SET(ITEM_WPN_UPSTATE))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_UPSTATE);
      weaponinfo[i]->upstate = E_GetStateNumForName(tempstr);
   }

   if(IS_SET(ITEM_WPN_DOWNSTATE))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_DOWNSTATE);
      weaponinfo[i]->downstate = E_GetStateNumForName(tempstr);
   }
   if(IS_SET(ITEM_WPN_READYSTATE))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_READYSTATE);
      weaponinfo[i]->readystate = E_GetStateNumForName(tempstr);
   }
   if(IS_SET(ITEM_WPN_ATKSTATE))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_ATKSTATE);
      weaponinfo[i]->atkstate = E_GetStateNumForName(tempstr);
   }
   if(IS_SET(ITEM_WPN_FLASHSTATE))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_FLASHSTATE);
      weaponinfo[i]->flashstate = E_GetStateNumForName(tempstr);
   }
   else
      weaponinfo[i]->flashstate = E_SafeState(S_NULL);

   if(IS_SET(ITEM_WPN_AMMOPERSHOT))
      weaponinfo[i]->ammopershot = cfg_getint(weaponsec, ITEM_WPN_AMMOPERSHOT);

   if(IS_SET(ITEM_WPN_MOD))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_MOD);
      weaponinfo[i]->mod = E_DamageTypeNumForName(tempstr);
   }
   else
      weaponinfo[i]->mod = 0; // MOD_UNKNOWN

   // 02/19/04: process combined flags first
   if(IS_SET(ITEM_WPN_FLAGS))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_FLAGS);
      if(*tempstr == '\0')
      {
         weaponinfo[i]->flags = 0;
      }
      else
      {
         unsigned int results = E_ParseFlags(tempstr, &e_weaponFlagSet);
         weaponinfo[i]->flags = results;
      }
   }

   if(IS_SET(ITEM_WPN_RECOIL))
      weaponinfo[i]->recoil = cfg_getint(weaponsec, ITEM_WPN_RECOIL);
   if(IS_SET(ITEM_WPN_HAPTICRECOIL))
      weaponinfo[i]->hapticrecoil = cfg_getint(weaponsec, ITEM_WPN_HAPTICRECOIL);
   if(IS_SET(ITEM_WPN_HAPTICTIME))
      weaponinfo[i]->haptictime = cfg_getint(weaponsec, ITEM_WPN_HAPTICTIME);

   if(IS_SET(ITEM_WPN_UPSOUND))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_UPSOUND);
      sfxinfo_t *tempsfx = E_EDFSoundForName(tempstr);
      if(tempsfx)
         weaponinfo[i]->upsound = tempsfx->dehackednum;
   }

   if(IS_SET(ITEM_WPN_READYSOUND))
   {
      tempstr = cfg_getstr(weaponsec, ITEM_WPN_READYSOUND);
      sfxinfo_t *tempsfx = E_EDFSoundForName(tempstr);
      if(tempsfx)
         weaponinfo[i]->readysound = tempsfx->dehackednum;
   }

   // Process DECORATE state block
   E_ProcessDecorateWepStatesRecursive(weaponsec, i, false);
}

//
// Process a single weapon cycle, done after processing all weaponinfos
//
static void E_processWeaponCycle(cfg_t *weapon)
{
   const char *tempstr = cfg_title(weapon);
   weaponinfo_t *weaponinfo = E_WeaponForName(tempstr);
   if((tempstr = cfg_getstr(weapon, ITEM_WPN_NEXTINCYCLE)))
      weaponinfo->nextInCycle = E_WeaponForName(tempstr);
   if((tempstr = cfg_getstr(weapon, ITEM_WPN_PREVINCYCLE)))
      weaponinfo->prevInCycle = E_WeaponForName(tempstr);
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

   for(unsigned int i = 0; i < unsigned int(NUMWEAPONTYPES); i++)
   {
      if(weaponinfo[i]->generation != edf_weapon_generation)
         weapon_hitlist[i] = 1;
   }

   for(unsigned int i = 0; i < numWeapons; i++)
   {
      cfg_t *weaponsec = cfg_getnsec(cfg, EDF_SEC_WEAPONINFO, i);
      const char *name = cfg_title(weaponsec);
      int weaponnum = E_WeaponNumForName(name);

      // reset the inheritance stack
      E_ResetWeaponPStack();

      // add this weapon to the stack
      E_AddWeaponToPStack(weaponnum);

      E_processWeapon(weaponnum, weaponsec, cfg, true);

      E_EDFLogPrintf("\t\tFinished thingtype %s (#%d)\n",
         weaponinfo[weaponnum]->name, weaponnum);
   }

   // free tables
   efree(weapon_hitlist);
   efree(weapon_pstack);

   // increment generation count
   ++edf_weapon_generation;
}

void E_ProcessWeaponDeltas(cfg_t *cfg)
{
   E_EDFLogPuts("\t* Processing weapondelta data\n");

   const unsigned int numDeltas = cfg_size(cfg, EDF_SEC_WPNDELTA);

   for(unsigned int i = 0; i < numDeltas; i++)
   {
      const char *name;
      int weaponNum;
      cfg_t *deltasec = cfg_getnsec(cfg, EDF_SEC_WPNDELTA, i);
      // get thingtype to edit
      if(!cfg_size(deltasec, ITEM_DELTA_NAME))
         E_EDFLoggedErr(2, "E_ProcessWeaponDeltas: weapondelta requires name field\n");

      name = cfg_getstr(deltasec, ITEM_DELTA_NAME);
      weaponNum = E_WeaponNumForName(name);

      E_processWeapon(weaponNum, deltasec, cfg, false);

      E_EDFLogPrintf("\t\tApplied thingdelta #%d to %s(#%d)\n",
                     i, weaponinfo[weaponNum]->name, weaponNum);
   }
}

//=============================================================================
//
// Weapon Utiliy Functions
//

//
// TODO: This should lose almost all applicability at some point
//
weaponinfo_t *E_WeaponForSlot(int slot)
{
   switch(slot)
   {
   case wp_fist:
      return E_WeaponForName(WEAPNAME_FIST);
   case wp_pistol:
      return E_WeaponForName(WEAPNAME_PISTOL);
   case wp_shotgun:
      return E_WeaponForName(WEAPNAME_SHOTGUN);
   case wp_chaingun:
      return E_WeaponForName(WEAPNAME_CHAINGUN);
   case wp_missile:
      return E_WeaponForName(WEAPNAME_MISSILE);
   case wp_plasma:
      return E_WeaponForName(WEAPNAME_PLASMA);
   case wp_bfg:
      return E_WeaponForName(WEAPNAME_BFG9000);
   case wp_chainsaw:
      return E_WeaponForName(WEAPNAME_CHAINSAW);
   case wp_supershotgun:
      return E_WeaponForName(WEAPNAME_SSG);
   default:
      return nullptr;
   }
}

//
// TODO: This should also hopefully become worthless at some point
//
int E_SlotForWeapon(weaponinfo_t *weapon)
{
   if(!strcmp(weapon->name, WEAPNAME_FIST))
      return wp_fist;
   if(!strcmp(weapon->name, WEAPNAME_PISTOL))
      return wp_pistol;
   if(!strcmp(weapon->name, WEAPNAME_SHOTGUN))
      return wp_shotgun;
   if(!strcmp(weapon->name, WEAPNAME_CHAINGUN))
      return wp_chaingun;
   if(!strcmp(weapon->name, WEAPNAME_MISSILE))
      return wp_missile;
   if(!strcmp(weapon->name, WEAPNAME_PLASMA))
      return wp_plasma;
   if(!strcmp(weapon->name, WEAPNAME_BFG9000))
      return wp_bfg;
   if(!strcmp(weapon->name, WEAPNAME_CHAINSAW))
      return wp_chainsaw;
   if(!strcmp(weapon->name, WEAPNAME_SSG))
      return wp_supershotgun;

   return wp_nochange;
}

//
// Check if the named weaponsec is currently equipped
//
bool E_WeaponIsCurrent(const player_t *player, const char *name)
{
   weaponinfo_t *weapon = E_WeaponForName(name);
   if(!weapon)
      return false;
   return player->readyweaponnew->id == weapon->id;
}

//
// Check if the weaponsec in the slotnum is currently equipped
// TODO: This should also become redundant
//
bool E_WeaponIsCurrentNum(player_t *player, const int num)
{
   return player->readyweaponnew->id == E_WeaponForSlot(num)->id;
}

//
// Convenience function to check if a player owns a weapon
//
bool E_PlayerOwnsWeapon(player_t *player, weaponinfo_t *weapon)
{
   return E_GetItemOwnedAmount(player, weapon->tracker);
}

//
// TODO: Remove this?
//
bool E_PlayerOwnsWeaponForSlot(player_t *player, int slot)
{
   return E_PlayerOwnsWeapon(player, E_WeaponForSlot(slot));
}

//
// Checks if a player owns a weapon in the provided slot, useful for things like the
// Doom weapon-number widget
// TODO: Consider global "weaponslots" variable
//
bool E_PlayerOwnsWeaponInSlot(player_t *player, int slot)
{
   if(!player->pclass->weaponslots[slot])
      return false;

   DLListItem<weaponslot_t> *weaponslot = &player->pclass->weaponslots[slot]->links;
   while(weaponslot)
   {
      if(E_PlayerOwnsWeapon(player, weaponslot->dllObject->weapon))
         return true;
      weaponslot = weaponslot->dllNext;
   }
   return false;
}

//
// Give the player ALL weapons indiscriminately
//
void E_GiveAllWeapons(player_t *player)
{
   weaponinfo_t *weapon;
   int i = 0;
   while((weapon = E_WeaponForID(i++)))
   {
      if(!E_PlayerOwnsWeapon(player, weapon))
         E_GiveInventoryItem(player, weapon->tracker, 1);
   }
}

//
// Give the player all class weapons
// TODO: Consider the global "weaponslots" variable
//
void E_GiveAllClassWeapons(player_t *player)
{
   for(int i = 0; i < NUMWEAPONSLOTS; i++)
   {
      if(!player->pclass->weaponslots[i])
         continue;

      DLListItem<weaponslot_t> *weaponslot = &player->pclass->weaponslots[i]->links;
      while(weaponslot)
      {
         if(!E_PlayerOwnsWeapon(player, weaponslot->dllObject->weapon))
            E_GiveInventoryItem(player, weaponslot->dllObject->weapon->tracker, 1);
         weaponslot = weaponslot->dllNext;
      }
   }
}

// EOF

