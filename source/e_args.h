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
// EDF Argument Management
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_ARGS_H__
#define E_ARGS_H__

// needed for MAXFLAGFIELDS:
#include "d_dehtbl.h"
#include "m_fixed.h"

struct edf_string_t;
struct emod_t;
class  Mobj;
struct mobjinfo_t;
struct player_t;
struct sfxinfo_t;
struct state_t;
struct weaponinfo_t;

// 16 arguments ought to be enough for anybody.
#define EMAXARGS 16

// Get an arglist safely from an Mobj * even if the pointer or the 
// Mobj's state is NULL (NULL is returned in either case).
#define ESAFEARGS(mo) ((mo && mo->state) ? mo->state->args : NULL)

typedef enum
{
   EVALTYPE_NONE,      // not cached
   EVALTYPE_INT,       // evaluated to an integer
   EVALTYPE_FIXED,     // evaluated to a fixed_t
   EVALTYPE_DOUBLE,    // evaluated to a double
   EVALTYPE_THINGNUM,  // evaluated to a thing number
   EVALTYPE_STATENUM,  // evaluated to a state number
   EVALTYPE_THINGFLAG, // evaluated to a thing flag bitmask
   EVALTYPE_SOUND,     // evaluated to a sound
   EVALTYPE_BEXPTR,    // evaluated to a bexptr
   EVALTYPE_EDFSTRING, // evaluated to an edf string
   EVALTYPE_KEYWORD,   // evaluated to a keyword enumeration value
   EVALTYPE_MOD,       // evaluated to a damagetype/means of damage
   EVALTYPE_NUMTYPES
} evaltype_e;

typedef struct evalcache_s
{
   evaltype_e type;
   union evalue_s
   {
      int           i;
      fixed_t       x;
      double        d;
      sfxinfo_t    *s;
      edf_string_t *estr;
      emod_t       *mod;
      unsigned int  flags[MAXFLAGFIELDS];
   } value;
} evalcache_t;

struct arglist_t
{
   char *args[EMAXARGS];         // argument strings stored from EDF
   evalcache_t values[EMAXARGS]; // if type != EVALTYPE_NONE, cached value
   int numargs;                  // number of arguments
};

typedef struct argkeywd_s
{
   const char **keywords;
   int        numkeywords;
} argkeywd_t;

bool          E_AddArgToList(arglist_t *al, const char *value);
inline int    E_GetArgCount(const arglist_t *al) { return al ? al->numargs : 0; }
bool          E_SetArg(arglist_t *al, int index, const char *value);
bool          E_SetArgFromNumber(arglist_t *al, int index, int value);
void          E_DisposeArgs(arglist_t *al);
void          E_ResetArgEval(arglist_t *al, int index);
void          E_ResetAllArgEvals();

const char   *E_ArgAsString(const arglist_t *al, int index, const char *defvalue);
int           E_ArgAsInt(arglist_t *al, int index, int defvalue);
fixed_t       E_ArgAsFixed(arglist_t *al, int index, fixed_t defvalue);
double        E_ArgAsDouble(arglist_t *al, int index, double defvalue);
int           E_ArgAsThingNum(arglist_t *al, int index);
int           E_ArgAsThingNumG0(arglist_t *al, int index);
state_t      *E_ArgAsStateLabel(const Mobj *mo, const arglist_t *al, int index);
state_t      *E_ArgAsStateLabel(const player_t *player, const arglist_t *al,
                                   int index);
int           E_ArgAsStateNum(  arglist_t *al, int index, const Mobj     *mo);
int           E_ArgAsStateNum(  arglist_t *al, int index, const player_t *player);
int           E_ArgAsStateNumNI(arglist_t *al, int index, const Mobj     *mo);
int           E_ArgAsStateNumNI(arglist_t *al, int index, const player_t *player);
int           E_ArgAsStateNumG0(arglist_t *al, int index, const Mobj     *mo);
int           E_ArgAsStateNumG0(arglist_t *al, int index, const player_t *player);
unsigned int *E_ArgAsThingFlags(arglist_t *al, int index);
sfxinfo_t    *E_ArgAsSound(arglist_t *al, int index);
int           E_ArgAsBexptr(arglist_t *al, int index);
edf_string_t *E_ArgAsEDFString(arglist_t *al, int index);
emod_t       *E_ArgAsDamageType(arglist_t *al, int index, int defvalue);
int           E_ArgAsKwd(arglist_t *al, int index, const argkeywd_t *kw,
                         int defvalue);

state_t      *E_GetJumpInfo(const mobjinfo_t *mi, const char *arg);
state_t      *E_GetWpnJumpInfo(const weaponinfo_t *wi, const char *arg);

#endif

// EOF

