// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
//
// EDF Thing Types Module
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_THINGS_H__
#define E_THINGS_H__

#include "m_collection.h"

struct emod_t;
struct mobjinfo_t;
struct state_t;
class  Mobj;
enum   bloodaction_e : int;

// Global Data
extern int UnknownThingType;

#ifdef NEED_EDF_DEFINITIONS

// Section Names
constexpr const char EDF_SEC_THING[]      = "thingtype";
constexpr const char EDF_SEC_TNGDELTA[]   = "thingdelta";
constexpr const char EDF_SEC_THINGGROUP[] = "thinggroup";

// Section Options
extern cfg_opt_t edf_thing_opts[];
extern cfg_opt_t edf_tdelta_opts[];
extern cfg_opt_t edf_tgroup_opts[];

#endif

//
// Thinggroup flags
//
enum
{
   TGF_PROJECTILEALLIANCE = 1,   // things in group are immune to their projectiles
   TGF_DAMAGEIGNORE = 2,         // things in group don't react to being damaged
   TGF_INHERITED = 4,            // make sure to also apply these to inheriting objects
   TGF_NOSPLASHDAMAGE = 8,       // things in group immune to splash damage
};

// Global Functions

// For EDF Only:

#ifdef NEED_EDF_DEFINITIONS
void E_CollectThings(cfg_t *cfg);
void E_ProcessThing(int i, cfg_t *thingsec, cfg_t *pcfg, bool def);
void E_ProcessThings(cfg_t *cfg);
void E_ProcessThingPickups(cfg_t *cfg);
void E_ProcessThingDeltas(cfg_t *cfg);
void E_ProcessThingGroups(cfg_t *cfg);
bool E_AutoAllocThingDEHNum(int thingnum);
void E_SetThingDefaultSprites(void);
#endif

// For Game Engine:
int E_GetAddThingNumForDEHNum(int dehnum, bool forceAdd);
int E_ThingNumForDEHNum(int dehnum);           // dehnum lookup
int E_GetThingNumForDEHNum(int dehnum);        //   fatal error version
int E_SafeThingType(int dehnum);               //   fallback version
int E_ThingNumForName(const char *name);       // mnemonic lookup
int E_GetThingNumForName(const char *name);    //   fatal error version
int E_SafeThingName(const char *name);         //   fallback version
int E_ThingNumForCompatName(const char *name); //   ACS compat version

void E_SetDropItem(mobjinfo_t *mi, const int itemnum);

// setup default gibhealth
void E_ThingDefaultGibHealth(mobjinfo_t *mi);

// thingtype custom-damagetype pain/death states
state_t *E_StateForMod(const mobjinfo_t *mi, const char *base,
                       const emod_t *mod);
state_t *E_StateForModNum(const mobjinfo_t *mi, const char *base, int num);

void     E_SplitTypeAndState(char *src, char **type, char **state);
int     *E_GetNativeStateLoc(mobjinfo_t *mi, const char *label);
inline static const int *E_GetNativeStateLoc(const mobjinfo_t *mi,
                                             const char *label)
{
   return E_GetNativeStateLoc(const_cast<mobjinfo_t *>(mi), label);
}
state_t *E_GetStateForMobjInfo(const mobjinfo_t *mi, const char *label);
state_t *E_GetStateForMobj(const Mobj *mo, const char *label);

// Thing groups
bool E_ThingPairValid(int t1, int t2, unsigned flags);
void E_AddToMBF21ThingGroup(int idnum, unsigned flag, int type, bool inclusive);
void E_RemoveFromExistingThingPairs(int type, unsigned flag);
const PODCollection<int> *E_GetThingsFromGroup(const char *name);

// ioanch 20160220: metastate key names used throughout the code. They also
// work as DECORATE state label names.
#define METASTATE_HEAL "Heal"
#define METASTATE_CRUNCH "Crunch"

// blood types
enum bloodtype_e : int
{
   BLOODTYPE_DOOM, 
   BLOODTYPE_HERETIC, 
   BLOODTYPE_HERETICRIP, 
   BLOODTYPE_HEXEN,
   BLOODTYPE_HEXENRIP,
   BLOODTYPE_STRIFE,
   BLOODTYPE_CRUSH,
   BLOODTYPE_CUSTOM,

   BLOODTYPE_MAX // must be last
};

int E_BloodTypeForThing(const Mobj *mo, bloodaction_e action);
bloodtype_e E_GetBloodBehaviorForAction(mobjinfo_t *info, bloodaction_e action);

void E_ForEachMobjInfoWithAnyFlags2(unsigned flags,
   bool (*func)(const mobjinfo_t &info, void *context), void *context);

int E_GetCrunchFrame(const Mobj *mo);

#endif

// EOF

