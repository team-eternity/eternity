//
// The Eternity Engine
// Copyright (C) 2016 James Haley et al.
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
// Purpose: EDF Argument Management
// Authors: James Haley
//

#include "z_zone.h"

#include "d_items.h"
#include "d_player.h"
#include "e_args.h"
#include "e_lib.h"
#include "e_mod.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_string.h"
#include "e_things.h"
#include "e_weapons.h"
#include "info.h"
#include "m_utils.h"
#include "p_mobj.h"
#include "p_maputl.h"

// haleyjd 05/21/10: a static empty string, to avoid allocating tons of memory 
// for single-byte strings.
static char e_argemptystr[] = "";

// test if a string is empty
#define ISEMPTY(s) (*(s) == '\0')

// test if a string is the global arg empty string
#define ISARGEMPTYSTR(s) ((s) == e_argemptystr)

//
// Adds an argument to the end of an argument list, if possible.
// Returns false if the operation fails.
//
bool E_AddArgToList(arglist_t *al, const char *value)
{
   bool added = false;
   
   if(al->numargs < EMAXARGS)
   {
      if(ISEMPTY(value))
         al->args[al->numargs] = e_argemptystr;
      else
         al->args[al->numargs] = estrdup(value);
      
      al->numargs++;
      added = true;
   }

   return added;
}

//
// Sets the argument at the given index to a new value. If that argument
// does not exist already, empty arguments will be added until that index 
// is valid.
//
bool E_SetArg(arglist_t *al, int index, const char *value)
{
   if(index >= EMAXARGS)
      return false;

   while(index >= al->numargs)
   {
      if(!E_AddArgToList(al, ""))
         return false;
   }

   // dispose of old argument and set new value
   if(!ISARGEMPTYSTR(al->args[index]))
      efree(al->args[index]);

   if(ISEMPTY(value))
      al->args[index] = e_argemptystr;
   else
      al->args[index] = estrdup(value);

   // any cached evaluation is now invalid
   al->values[index].type = EVALTYPE_NONE;

   return true;
}

//
// Calls E_SetArg after performing an itoa operation on the argument.
// This is for convenience in DeHackEd, which is not very smart about 
// setting arguments.
//
bool E_SetArgFromNumber(arglist_t *al, int index, int value)
{
   char numbuffer[33];

   M_Itoa(value, numbuffer, 10);

   return E_SetArg(al, index, numbuffer);
}

//
// Resets an arglist_t to its initial state.
//
void E_DisposeArgs(arglist_t *al)
{
   for(int i = 0; i < al->numargs; i++)
   {
      if(!ISARGEMPTYSTR(al->args[i]))
         efree(al->args[i]);
   }

   memset(al, 0, sizeof(arglist_t));
}

//
// Marks an arg (if it exists) as unevaluated.
//
void E_ResetArgEval(arglist_t *al, int index)
{
   if(al && index < al->numargs)
      al->values[index].type = EVALTYPE_NONE;
}

//
// haleyjd 03/28/10: Reset the evaluation state of all state argument lists,
// to flush the state of the system after a runtime modification to EDF.
//
void E_ResetAllArgEvals()
{
   for(int stnum = 0; stnum < NUMSTATES; stnum++)
   {
      arglist_t *args = states[stnum]->args;

      if(args)
      {
         for(int argnum = 0; argnum < args->numargs; argnum++)
            args->values[argnum].type = EVALTYPE_NONE;
      }
   }
}

//
// This is just a safe method to get the argument string at the given
// index. If the argument doesn't exist, defvalue is returned.
//
const char *E_ArgAsString(const arglist_t *al, int index, const char *defvalue)
{
   return (al && index < al->numargs) ? al->args[index] : defvalue;
}

//
// Gets the arg value at index i as an integer, if such argument exists.
// The evaluated value will be cached so that it can be returned on
// subsequent calls. If the argument does not exist, the value passed in
// the "defvalue" argument will be returned.
//
int E_ArgAsInt(arglist_t *al, int index, int defvalue)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return defvalue;

   evalcache_t &eval = al->values[index];

   // if the value is cached, return the cached value
   if(eval.type != EVALTYPE_INT)
   {
      // calculate the value and cache it
      eval.type    = EVALTYPE_INT;
      eval.value.i = int(strtol(al->args[index], nullptr, 0));
   }

   return eval.value.i;
}

//
// Gets the arg value at index i as a fixed_t, if such argument exists.
// The evaluated value will be cached so that it can be returned on
// subsequent calls. If the argument does not exist, the value passed in
// the "defvalue" argument will be returned.
//
fixed_t E_ArgAsFixed(arglist_t *al, int index, fixed_t defvalue)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return defvalue;

   evalcache_t &eval = al->values[index];

   // if the value is cached, return the cached value
   if(eval.type != EVALTYPE_FIXED)
   {
      // calculate the value and cache it
      eval.type    = EVALTYPE_FIXED;
      eval.value.x = M_DoubleToFixed(strtod(al->args[index], nullptr));
   }

   return eval.value.x;
}

//
// Gets the arg value at index i as a double, if such argument exists.
// The evaluated value will be cached so that it can be returned on
// subsequent calls. If the argument does not exist, the value passed in
// the "defvalue" argument will be returned.
//
double E_ArgAsDouble(arglist_t *al, int index, double defvalue)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return defvalue;

   evalcache_t &eval = al->values[index];

   // if the value is cached, return the cached value
   if(eval.type != EVALTYPE_DOUBLE)
   {
      // calculate the value and cache it
      eval.type    = EVALTYPE_DOUBLE;
      eval.value.d = strtod(al->args[index], nullptr);
   }

   return eval.value.d;
}

//
// Gets the arg value at index i as an angle_t, if such argument exists.
// The evaluated value will be cached so that it can be returned on
// subsequent calls. If the argument does not exist, the value passed in
// the "defvalue" argument will be returned.
//
angle_t E_ArgAsAngle(arglist_t *al, int index, angle_t defvalue)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return defvalue;

   evalcache_t &eval = al->values[index];

   // if the value is cached, return the cached value
   if(eval.type != EVALTYPE_ANGLE)
   {
      // calculate the value and cache it
      eval.type    = EVALTYPE_ANGLE;
      eval.value.a = P_DoubleToAngle(strtod(al->args[index], nullptr));
   }

   return eval.value.a;
}

//
// Gets the arg value at index i as a thingtype number, if such 
// argument exists. The evaluated value will be cached so that it can
// be returned on subsequent calls. If the arg does not exist, the 
// Unknown thingtype is returned instead.
//
int E_ArgAsThingNum(arglist_t *al, int index)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return UnknownThingType;

   evalcache_t &eval = al->values[index];

   if(eval.type != EVALTYPE_THINGNUM)
   {
      char *pos = nullptr;
      long num;

      eval.type = EVALTYPE_THINGNUM;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);
      if(estrnonempty(pos))
      {
         // it is a name
         eval.value.i = E_SafeThingName(al->args[index]);
      }
      else
      {
         // it is a DeHackEd number
         eval.value.i = E_SafeThingType((int)num);
      }
   }

   return eval.value.i;
}

//
// Only converts numbers to things if the number is greater than zero.
// G0 == "greater than zero"
//
int E_ArgAsThingNumG0(arglist_t *al, int index)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return -1;

   evalcache_t &eval = al->values[index];

   if(eval.type != EVALTYPE_THINGNUM)
   {
      char *pos = nullptr;
      long num;

      eval.type = EVALTYPE_THINGNUM;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(estrnonempty(pos))
      {
         // it is a name
         eval.value.i = E_ThingNumForName(al->args[index]);
      }
      else
      {
         // it is a DeHackEd number
         if(num > 0)
            eval.value.i = E_ThingNumForDEHNum((int)num);
         else
            eval.value.i = -1;
      }
   }

   return eval.value.i;
}

//=============================================================================
//
// State Evaluators
//
// haleyjd 07/17/10: With the advent of DECORATE state support, the need for
// more featured evaluation of state arguments has developed. When functions
// which traditionally took global state names do not find a state of the given
// name, their behavior should be to subsequently look for a DECORATE state of
// that name.
//
// For pointers implemented for DECORATE state compatibility, the evaluation 
// order should be opposite - DECORATE state labels have priority over global
// state names, and for some pointers, global state names are not allowed.
//

//
// Returns the target state given an initial type and label text. Assuming that
// the text does not contain a :: operator, the information returned will be 
// the ordinary plain state named by the input string. Otherwise, both the mobjinfo
// and statename may be redirected.
//
state_t *E_GetJumpInfo(const mobjinfo_t *mi, const char *arg)
{
   char *temparg = Z_Strdupa(arg);
   char *colon   = strchr(temparg, ':');

   char *statename = nullptr, *type = nullptr;

   // if the statename does not contain a colon, there is no potential for 
   // redirection.
   if(!colon)
      return E_GetStateForMobjInfo(mi, arg);

   // split temparg at the :: operator
   E_SplitTypeAndState(temparg, &type, &statename);

   // if both are not valid, we can't do this sort of operation
   if(!(type && statename))
      return E_GetStateForMobjInfo(mi, arg);

   // Check for super::, which is an explicit reference to the parent type;
   // Otherwise, treat the left side as a thingtype EDF class name.
   if(!strcasecmp(type, "super") && mi->parent)
      mi = mi->parent;
   else
   {
      int thingtype = E_ThingNumForName(type);
      
      // non-existent thingtype is an error, no jump will happen
      if(thingtype == -1)
         return nullptr;
      else
         mi = mobjinfo[thingtype];
   }

   return E_GetStateForMobjInfo(mi, statename);
}

//
// Returns the target state given an initial type and label text. Assuming that
// the text does not contain a :: operator, the information returned will be 
// the ordinary plain state named by the input string. Otherwise, both the weaponinfo
// and statename may be redirected.
//
state_t *E_GetWpnJumpInfo(const weaponinfo_t *wi, const char *arg)
{
   char *temparg = Z_Strdupa(arg);
   char *colon   = strchr(temparg, ':');

   char *statename = nullptr, *type = nullptr;

   // if the statename does not contain a colon, there is no potential for 
   // redirection.
   if(!colon)
      return E_GetStateForWeaponInfo(wi, arg);

   // split temparg at the :: operator
   E_SplitTypeAndState(temparg, &type, &statename);

   // if both are not valid, we can't do this sort of operation
   if(!(type && statename))
      return E_GetStateForWeaponInfo(wi, arg);

   // Check for super::, which is an explicit reference to the parent type;
   // Otherwise, treat the left side as a weaponinfo EDF class name.
   if(!strcasecmp(type, "super") && wi->parent)
      wi = wi->parent;
   else
   {
      int weapontype = E_WeaponNumForName(type);
      
      // non-existent weaponinfo is an error, no jump will happen
      if(weapontype == -1)
         return nullptr;
      else
         wi = E_WeaponForID(weapontype);
   }

   return E_GetStateForWeaponInfo(wi, statename);
}

//
// This evaluator only allows DECORATE state labels or numbers, and will not 
// make reference to global states. Because evaluation of this type of argument
// is relative to the mobjinfo, this evaluation is never cached.
//
state_t *E_ArgAsStateLabel(const Mobj *mo, const arglist_t *al, int index)
{
   const char *arg;
   char       *end   = nullptr;
   state_t    *state = mo->state;
   long        num;

   if(!al || index >= al->numargs)
      return nullptr;

   arg = al->args[index];

   num = strtol(arg, &end, 0);

   // if not a number, this is a state label
   if(estrnonempty(end))
      return E_GetJumpInfo(mo->info, arg);
   else
   {
      long idx = state->index + num;

      return (idx >= 0 && idx < NUMSTATES) ? states[idx] : nullptr;
   }
}

//
// This evaluator only allows DECORATE state labels or numbers, and will not
// make reference to global states. Because evaluation of this type of argument
// is relative to the player, this evaluation is never cached.
//
state_t *E_ArgAsStateLabel(const player_t *player, const arglist_t *al, int index)
{
   const weaponinfo_t *wi = player->readyweapon;
   const char *arg;
   char       *end   = nullptr;
   const state_t *state = player->psprites->state;
   long        num;

   if(!al || index >= al->numargs)
      return nullptr;

   arg = al->args[index];

   num = strtol(arg, &end, 0);

   // if not a number, this is a state label
   if(estrnonempty(end))
      return E_GetWpnJumpInfo(wi, arg);
   else
   {
      long idx = state->index + num;

      return (idx >= 0 && idx < NUMSTATES) ? states[idx] : nullptr;
   }
}

//
// Gets the arg value at index i as a state number, if such argument exists.
// The evaluated value will be cached so that it can be returned on subsequent
// calls. If the arg does not exist, the null state is returned instead.
//
template<typename T> inline int E_argAsStateNum(arglist_t *al, int index, const T *mop)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return NullStateNum;

   evalcache_t &eval = al->values[index];

   if(eval.type != EVALTYPE_STATENUM)
   {
      char *pos = nullptr;
      long num;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(estrnonempty(pos))
      {
         // it is a name
         int statenum;

         // haleyjd 07/18/10: Try EDF state name first; if this turns out as
         // invalid, check to see if it's a DECORATE state label before returning
         // NullStateNum.
         if((statenum = E_StateNumForName(al->args[index])) >= 0)
         {
            // it was a valid EDF state mnemonic
            eval.type = EVALTYPE_STATENUM;
            eval.value.i = statenum;
         }
         else
         {
            // see if it is a valid DECORATE state label
            // if it is valid, it is NOT cached, because DECORATE label
            // resolution is "virtual" (ie relative to the calling thingtype).
            state_t *state;
            if(mop && (state = E_ArgAsStateLabel(mop, al, index)))
               return state->index;
            else
            {
               // whatever it is, we dunno of it.
               eval.type = EVALTYPE_STATENUM;
               eval.value.i = NullStateNum;
            }
         }
      }
      else
      {
         // it is a DeHackEd number
         eval.type = EVALTYPE_STATENUM;
         eval.value.i = E_SafeState((int)num);
      }
   }

   return eval.value.i;
}

//
// Gets the arg value at index i as a state number, if such argument exists.
// The evaluated value will be cached so that it can be returned on subsequent
// calls. If the arg does not exist, -1 is returned.
//
// NI == No Invalid, because invalid states are not converted to the null state.
//
template<typename T> inline int E_argAsStateNumNI(arglist_t *al, int index, const T *mop)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return -1;

   evalcache_t &eval = al->values[index];

   if(eval.type != EVALTYPE_STATENUM)
   {
      char *pos = nullptr;
      long num;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(estrnonempty(pos))
      {
         // it is a name
         int statenum;

         // haleyjd 07/18/10: Try EDF state name first; if this turns out as
         // invalid, check to see if it's a DECORATE state label before returning
         // NullStateNum.
         if((statenum = E_StateNumForName(al->args[index])) >= 0)
         {
            // it was a valid EDF state mnemonic
            eval.type = EVALTYPE_STATENUM;
            eval.value.i = statenum;
         }
         else
         {
            // see if it is a valid DECORATE state label
            // if it is valid, it is NOT cached, because DECORATE label
            // resolution is "virtual" (ie relative to the calling thingtype).
            state_t *state;
            if(mop && (state = E_ArgAsStateLabel(mop, al, index)))
               return state->index;
            else
            {
               // whatever it is, we dunno of it.
               eval.type = EVALTYPE_STATENUM;
               eval.value.i = -1;
            }
         }
      }
      else
      {
         // it is a DeHackEd number
         eval.type = EVALTYPE_STATENUM;
         eval.value.i = E_StateNumForDEHNum((int)num);
      }
   }

   return eval.value.i;
}

//
// Only converts numbers to states if the number is greater than or
// equal to zero.
// G0 == "greater than or equal to zero"
//
template<typename T> inline int E_argAsStateNumG0(arglist_t *al, int index, const T *mop)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return -1;

   evalcache_t &eval = al->values[index];

   if(eval.type != EVALTYPE_STATENUM)
   {
      char *pos = nullptr;
      long num;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(estrnonempty(pos))
      {
         // it is a name
         int statenum;

         // haleyjd 07/18/10: Try EDF state name first; if this turns out as
         // invalid, check to see if it's a DECORATE state label before returning
         // NullStateNum.
         if((statenum = E_StateNumForName(al->args[index])) >= 0)
         {
            // it was a valid EDF state mnemonic
            eval.type = EVALTYPE_STATENUM;
            eval.value.i = statenum;
         }
         else
         {
            // see if it is a valid DECORATE state label
            // if it is valid, it is NOT cached, because DECORATE label
            // resolution is "virtual" (ie relative to the calling thingtype).
            state_t *state;
            if(mop && (state = E_ArgAsStateLabel(mop, al, index)))
               return state->index;
            else
            {
               // whatever it is, we dunno of it.
               eval.type = EVALTYPE_STATENUM;
               eval.value.i = -1;
            }
         }
      }
      else
      {
         eval.type = EVALTYPE_STATENUM;

         // it is a DeHackEd number if it is >= 0
         if(num >= 0)
            eval.value.i = E_StateNumForDEHNum((int)num);
         else
            eval.value.i = (int)num;
      }
   }

   return eval.value.i;
}

//
// All the overloads for the various argument-as-state-number functions
//
int E_ArgAsStateNum(arglist_t *al, int index, const Mobj *mo)
{
   return E_argAsStateNum<Mobj>(al, index, mo);
}
int E_ArgAsStateNum(arglist_t *al, int index, const player_t *player)
{
   return E_argAsStateNum<player_t>(al, index, player);
}
int E_ArgAsStateNumNI(arglist_t *al, int index, const Mobj *mo)
{
   return E_argAsStateNumNI<Mobj>(al, index, mo);
}
int E_ArgAsStateNumNI(arglist_t *al, int index, const player_t *player)
{
   return E_argAsStateNumNI<player_t>(al, index, player);
}
int E_ArgAsStateNumG0(arglist_t *al, int index, const Mobj *mo)
{
   return E_argAsStateNumG0<Mobj>(al, index, mo);
}
int E_ArgAsStateNumG0(arglist_t *al, int index, const player_t *player)
{
   return E_argAsStateNumG0<player_t>(al, index, player);
}

//
// Gets the arg value at index i as a set of thing flag masks, if such argument 
// exists. The evaluated value will be cached so that it can be returned on 
// subsequent calls. If the arg does not exist, nullptr is returned.
//
unsigned int *E_ArgAsThingFlags(arglist_t *al, int index)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return nullptr;

   evalcache_t &eval = al->values[index];

   if(eval.type != EVALTYPE_THINGFLAG)
   {
      eval.type = EVALTYPE_THINGFLAG;

      // empty string is zero
      if(*(al->args[index]) != '\0')
      {
         unsigned int *flagvals = deh_ParseFlagsCombined(al->args[index]);

         memcpy(eval.value.flags, flagvals, MAXFLAGFIELDS * sizeof(unsigned int));
      }
      else
         memset(eval.value.flags, 0, MAXFLAGFIELDS * sizeof(unsigned int));
   }

   return eval.value.flags;
}

//
// Gets the arg value at index i as a flag mask, if such argument  exists,
// using the specified flagset. The evaluated value will be cached
// so that it can be returned on subsequent calls. If the arg does not
// exist, nullptr is returned.
//
unsigned int E_ArgAsFlags(arglist_t *al, int index, dehflagset_t *flagset)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return 0;

   evalcache_t &eval = al->values[index];

   if(eval.type != EVALTYPE_THINGFLAG)
   {
      eval.type = EVALTYPE_THINGFLAG;
      memset(eval.value.flags, 0, MAXFLAGFIELDS * sizeof(unsigned int));

      // empty string is zero
      if(*(al->args[index]) != '\0')
      {
         eval.value.flags[0] = E_ParseFlags(al->args[index], flagset);
      }
   }

   return eval.value.flags[0];
}

//
// Gets the arg value at index i as a sound, if such argument exists.
// The evaluated value will be cached so that it can be returned on subsequent
// calls. If the arg does not exist, nullptr is returned.
//
sfxinfo_t *E_ArgAsSound(arglist_t *al, int index)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return nullptr;

   evalcache_t &eval = al->values[index];

   if(eval.type != EVALTYPE_SOUND)
   {
      char *pos = nullptr;
      long num;

      eval.type = EVALTYPE_SOUND;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(estrnonempty(pos))
      {
         // it is a name
         eval.value.s = E_SoundForName(al->args[index]);
      }
      else
      {
         // it is a DeHackEd number
         eval.value.s = E_SoundForDEHNum((int)num);
      }
   }

   return eval.value.s;
}

//
// Gets the arg value at index i as a bexptr index, if such argument exists.
// The evaluated value will be cached so that it can be returned on subsequent
// calls. If the arg does not exist, -1 is returned.
//
int E_ArgAsBexptr(arglist_t *al, int index)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return -1;

   evalcache_t &eval = al->values[index];

   if(eval.type != EVALTYPE_BEXPTR)
   {
      deh_bexptr *ptr = D_GetBexPtr(al->args[index]);

      eval.type    = EVALTYPE_BEXPTR;
      eval.value.i = ptr ? eindex(ptr - deh_bexptrs) : -1;
   }

   return eval.value.i;
}

//
// Gets the arg value at index i as an EDF string, if such argument exists.
// The evaluated value will be cached so that it can be returned on subsequent
// calls. If the arg does not exist, nullptr is returned.
//
edf_string_t *E_ArgAsEDFString(arglist_t *al, int index)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return nullptr;

   evalcache_t &eval = al->values[index];

   if(eval.type != EVALTYPE_EDFSTRING)
   {
      char *pos = nullptr;
      long num;

      eval.type = EVALTYPE_EDFSTRING;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(estrnonempty(pos))
      {
         // it is a name
         eval.value.estr = E_StringForName(al->args[index]);
      }
      else
      {
         // it is a string number
         eval.value.estr = E_StringForNum((int)num);
      }
   }

   return eval.value.estr;
}

//
// Gets the arg value at index i as an EDF damage type, if such argument exists.
// The evaluated value will be cached so that it can be returned on subsequent
// calls. If the arg does not exist, the default value will be looked up. If
// that value does not exist, the "Unknown" damagetype is returned courtesy of
// EDF logic.
//
emod_t *E_ArgAsDamageType(arglist_t *al, int index, int defvalue)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return E_DamageTypeForNum(defvalue);

   evalcache_t &eval = al->values[index];

   if(eval.type != EVALTYPE_MOD)
   {
      char *pos = nullptr;
      long num;

      eval.type = EVALTYPE_MOD;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(estrnonempty(pos))
      {
         // it is a name
         eval.value.mod = E_DamageTypeForName(al->args[index]);
      }
      else
      {
         // it is a number
         eval.value.mod = E_DamageTypeForNum((int)num);
      }
   }

   return eval.value.mod;
}

//
// Gets the argument at index i as the corresponding enumeration value for a
// keyword string, given the provided keyword set. Returns the default value
// if the argument doesn't exist, or the keyword is not matched.
//
int E_ArgAsKwd(arglist_t *al, int index, const argkeywd_t *kw, int defvalue)
{
   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return defvalue;

      evalcache_t &eval = al->values[index];

   if(eval.type != EVALTYPE_KEYWORD)
   {
      char *pos = nullptr;
      long num;

      eval.type = EVALTYPE_KEYWORD;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(estrnonempty(pos))
      {
         // it is a name
         eval.value.i = E_StrToNumLinear(kw->keywords, kw->numkeywords, al->args[index]);

         if(eval.value.i == kw->numkeywords)
            eval.value.i = defvalue;
      }
      else
      {
         // it is just a number
         eval.value.i = (int)num;
      }
   }

   return eval.value.i;
}

// EOF

