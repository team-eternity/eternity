// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2009 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//----------------------------------------------------------------------------
//
// EDF Argument Management
//
// By James Haley
//
//----------------------------------------------------------------------------

#include "z_zone.h"
#include "info.h"
#include "m_misc.h"
#include "e_lib.h"
#include "e_mod.h"
#include "e_states.h"
#include "e_things.h"
#include "e_sound.h"
#include "e_args.h"

// haleyjd 05/21/10: an empty string, to avoid allocating tons of memory for
// single-byte strings.
static char e_argemptystr[] = "";

// test if a string is empty
#define ISEMPTY(s) (*(s) == '\0')

// test if a string is the global arg empty string
#define ISARGEMPTYSTR(s) ((s) == e_argemptystr)

//
// E_AddArgToList
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
         al->args[al->numargs] = strdup(value);
      
      al->numargs++;
      added = true;
   }

   return added;
}

//
// E_SetArg
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
      free(al->args[index]);

   if(ISEMPTY(value))
      al->args[index] = e_argemptystr;
   else
      al->args[index] = strdup(value);

   // any cached evaluation is now invalid
   al->values[index].type = EVALTYPE_NONE;

   return true;
}

//
// E_SetArgFromNumber
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
// E_DisposeArgs
//
// Resets an arglist_t to its initial state.
//
void E_DisposeArgs(arglist_t *al)
{
   int i;

   for(i = 0; i < al->numargs; ++i)
   {
      if(!ISARGEMPTYSTR(al->args[i]))
         free(al->args[i]);
   }

   memset(al, 0, sizeof(arglist_t));
}

//
// E_ResetArgEval
//
// Marks an arg (if it exists) as unevaluated.
//
void E_ResetArgEval(arglist_t *al, int index)
{
   if(al && index < al->numargs)
      al->values[index].type = EVALTYPE_NONE;
}

//
// E_ResetAllArgEvals
//
// haleyjd 03/28/10: Reset the evaluation state of all state argument lists,
// to flush the state of the system after a runtime modification to EDF.
//
void E_ResetAllArgEvals(void)
{
   int stnum;

   for(stnum = 0; stnum < NUMSTATES; ++stnum)
   {
      arglist_t *args = states[stnum]->args;

      if(args)
      {
         int argnum;

         for(argnum = 0; argnum < args->numargs; ++argnum)
            args->values[argnum].type = EVALTYPE_NONE;
      }
   }
}

//
// E_ArgAsString
//
// This is just a safe method to get the argument string at the given
// index. If the argument doesn't exist, defvalue is returned.
//
const char *E_ArgAsString(arglist_t *al, int index, const char *defvalue)
{
   return (al && index < al->numargs) ? al->args[index] : defvalue;
}

//
// E_ArgAsInt
//
// Gets the arg value at index i as an integer, if such argument exists.
// The evaluated value will be cached so that it can be returned on
// subsequent calls. If the argument does not exist, the value passed in
// the "defvalue" argument will be returned.
//
int E_ArgAsInt(arglist_t *al, int index, int defvalue)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return defvalue;

   eval = &(al->values[index]);

   // if the value is cached, return the cached value
   if(eval->type != EVALTYPE_INT)
   {
      // calculate the value and cache it
      eval->type    = EVALTYPE_INT;
      eval->value.i = (int)(strtol(al->args[index], NULL, 0));
   }

   return eval->value.i;
}

//
// E_ArgAsFixed
//
// Gets the arg value at index i as a fixed_t, if such argument exists.
// The evaluated value will be cached so that it can be returned on
// subsequent calls. If the argument does not exist, the value passed in
// the "defvalue" argument will be returned.
//
fixed_t E_ArgAsFixed(arglist_t *al, int index, fixed_t defvalue)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return defvalue;

   eval = &(al->values[index]);

   // if the value is cached, return the cached value
   if(eval->type != EVALTYPE_FIXED)
   {
      // calculate the value and cache it
      eval->type    = EVALTYPE_FIXED;
      eval->value.x = M_DoubleToFixed(strtod(al->args[index], NULL));
   }

   return eval->value.x;
}

//
// E_ArgAsDouble
//
// Gets the arg value at index i as a double, if such argument exists.
// The evaluated value will be cached so that it can be returned on
// subsequent calls. If the argument does not exist, the value passed in
// the "defvalue" argument will be returned.
//
double E_ArgAsDouble(arglist_t *al, int index, double defvalue)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return defvalue;

   eval = &(al->values[index]);

   // if the value is cached, return the cached value
   if(eval->type != EVALTYPE_DOUBLE)
   {
      // calculate the value and cache it
      eval->type    = EVALTYPE_DOUBLE;
      eval->value.d = strtod(al->args[index], NULL);
   }

   return eval->value.d;
}

//
// E_ArgAsThingNum
//
// Gets the arg value at index i as a thingtype number, if such 
// argument exists. The evaluated value will be cached so that it can
// be returned on subsequent calls. If the arg does not exist, the 
// Unknown thingtype is returned instead.
//
int E_ArgAsThingNum(arglist_t *al, int index)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return UnknownThingType;

   eval = &(al->values[index]);

   if(eval->type != EVALTYPE_THINGNUM)
   {
      char *pos = NULL;
      long num;

      eval->type = EVALTYPE_THINGNUM;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(pos && *pos != '\0')
      {
         // it is a name
         eval->value.i = E_SafeThingName(al->args[index]);
      }
      else
      {
         // it is a DeHackEd number
         eval->value.i = E_SafeThingType((int)num);
      }
   }

   return eval->value.i;
}

//
// E_ArgAsThingNumG0
//
// Only converts numbers to things if the number is greater than zero.
// G0 == "greater than zero"
//
int E_ArgAsThingNumG0(arglist_t *al, int index)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return NUMMOBJTYPES;

   eval = &(al->values[index]);

   if(eval->type != EVALTYPE_THINGNUM)
   {
      char *pos = NULL;
      long num;

      eval->type = EVALTYPE_THINGNUM;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(pos && *pos != '\0')
      {
         // it is a name
         eval->value.i = E_ThingNumForName(al->args[index]);
      }
      else
      {
         // it is a DeHackEd number
         if(num > 0)
            eval->value.i = E_ThingNumForDEHNum((int)num);
         else
            eval->value.i = -1;
      }
   }

   return eval->value.i;
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

// static utilities

typedef struct jumpinfo_s
{
   mobjinfo_t *mi;  // mobjinfo the jump is relative to
   char *statename; // state name or label
} jumpinfo_t;

//
// E_getJumpInfo
//
// Returns the target mobjtype and jump label/state name, given an initial
// type and label text. Assuming that the text does not contain a :: operator,
// the information returned will be the same information passed in. Otherwise,
// both the mobjinfo and statename may be redirected.
//
static jumpinfo_t E_getJumpInfo(mobjinfo_t *mi, const char *arg)
{
   jumpinfo_t ji;
   char *temparg = Z_Strdupa(arg);
   char *colon   = strchr(temparg, ':');

   char *statename = NULL, *type = NULL;

   // if the statename does not contain a colon, there is no potential for 
   // redirection.
   if(!colon)
   {
      ji.mi = mi;
      ji.statename = temparg;
      return ji;
   }

   // split temparg at the :: operator
   E_SplitTypeAndState(temparg, &type, &statename);

   // if both are not valid, we can't do this sort of operation
   if(!(type && statename))
   {
      ji.mi = mi;
      ji.statename = temparg;
      return ji;
   }

   // Check for super::, which is an explicit reference to the parent type;
   // Otherwise, treat the left side as a thingtype EDF class name.
   if(!strcasecmp(type, "super") && mi->parent)
      ji.mi = mi->parent;
   else
   {
      int thingtype = E_ThingNumForName(type);
      
      // non-existent thingtype is an error, no jump will happen
      if(thingtype == NUMMOBJTYPES)
      {
         ji.mi = mi;
         ji.statename = "";
         return ji;
      }
      else
         ji.mi = &mobjinfo[thingtype];
   }

   ji.statename = statename;

   return ji;
}

//
// E_ArgAsStateLabel
//
// This evaluator only allows DECORATE state labels or numbers, and will not 
// make reference to global states. Because evaluation of this type of argument
// is relative to the mobjinfo, this evaluation is never cached.
//
state_t *E_ArgAsStateLabel(Mobj *mo, int index)
{
   const char *arg;
   char       *end   = NULL;
   state_t    *state = mo->state;
   arglist_t  *al    = state->args;
   long        num;

   if(!al || index >= al->numargs)
      return NULL;

   arg = al->args[index];

   num = strtol(arg, &end, 0);

   // if not a number, this is a state label
   if(end && *end != '\0')
   {
      jumpinfo_t ji = E_getJumpInfo(mo->info, arg);

      return E_GetStateForMobjInfo(ji.mi, ji.statename);
   }
   else
   {
      long idx = state->index + num;

      return (idx >= 0 && idx < NUMSTATES) ? states[idx] : NULL;
   }
}

//
// E_ArgAsStateNum
//
// Gets the arg value at index i as a state number, if such argument exists.
// The evaluated value will be cached so that it can be returned on subsequent
// calls. If the arg does not exist, the null state is returned instead.
//
int E_ArgAsStateNum(arglist_t *al, int index, Mobj *mo)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return NullStateNum;

   eval = &(al->values[index]);

   if(eval->type != EVALTYPE_STATENUM)
   {
      char *pos = NULL;
      long num;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(pos && *pos != '\0')
      {
         // it is a name
         int statenum;
         
         // haleyjd 07/18/10: Try EDF state name first; if this turns out as
         // invalid, check to see if it's a DECORATE state label before returning
         // NullStateNum.
         if((statenum = E_StateNumForName(al->args[index])) >= 0)
         {
            // it was a valid EDF state mnemonic
            eval->type = EVALTYPE_STATENUM;
            eval->value.i = statenum;
         }
         else
         {
            // see if it is a valid DECORATE state label
            // if it is valid, it is NOT cached, because DECORATE label
            // resolution is "virtual" (ie relative to the calling thingtype).
            state_t *state;
            if(mo && (state = E_ArgAsStateLabel(mo, index)))
               return state->index;
            else
            {
               // whatever it is, we dunno of it.
               eval->type = EVALTYPE_STATENUM;
               eval->value.i = NullStateNum;
            }
         }
      }
      else
      {
         // it is a DeHackEd number
         eval->type = EVALTYPE_STATENUM;
         eval->value.i = E_SafeState((int)num);
      }
   }

   return eval->value.i;
}

//
// E_ArgAsStateNumNI
//
// Gets the arg value at index i as a state number, if such argument exists.
// The evaluated value will be cached so that it can be returned on subsequent
// calls. If the arg does not exist, -1 is returned.
//
// NI == No Invalid, because invalid states are not converted to the null state.
//
int E_ArgAsStateNumNI(arglist_t *al, int index, Mobj *mo)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return -1;

   eval = &(al->values[index]);

   if(eval->type != EVALTYPE_STATENUM)
   {
      char *pos = NULL;
      long num;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(pos && *pos != '\0')
      {
         // it is a name
         int statenum;
         
         // haleyjd 07/18/10: Try EDF state name first; if this turns out as
         // invalid, check to see if it's a DECORATE state label before returning
         // NullStateNum.
         if((statenum = E_StateNumForName(al->args[index])) >= 0)
         {
            // it was a valid EDF state mnemonic
            eval->type = EVALTYPE_STATENUM;
            eval->value.i = statenum;
         }
         else
         {
            // see if it is a valid DECORATE state label
            // if it is valid, it is NOT cached, because DECORATE label
            // resolution is "virtual" (ie relative to the calling thingtype).
            state_t *state;
            if(mo && (state = E_ArgAsStateLabel(mo, index)))
               return state->index;
            else
            {
               // whatever it is, we dunno of it.
               eval->type = EVALTYPE_STATENUM;
               eval->value.i = -1;
            }
         }      
      }
      else
      {
         // it is a DeHackEd number
         eval->type = EVALTYPE_STATENUM;
         eval->value.i = E_StateNumForDEHNum((int)num);
      }
   }

   return eval->value.i;
}

//
// E_ArgAsStateNumG0
//
// Only converts numbers to states if the number is greater than or
// equal to zero.
// G0 == "greater than or equal to zero"
//
int E_ArgAsStateNumG0(arglist_t *al, int index, Mobj *mo)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return -1;

   eval = &(al->values[index]);

   if(eval->type != EVALTYPE_STATENUM)
   {
      char *pos = NULL;
      long num;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(pos && *pos != '\0')
      {
         // it is a name
         int statenum;
       
         // haleyjd 07/18/10: Try EDF state name first; if this turns out as
         // invalid, check to see if it's a DECORATE state label before returning
         // NullStateNum.
         if((statenum = E_StateNumForName(al->args[index])) >= 0)
         {
            // it was a valid EDF state mnemonic
            eval->type = EVALTYPE_STATENUM;
            eval->value.i = statenum;
         }
         else
         {
            // see if it is a valid DECORATE state label
            // if it is valid, it is NOT cached, because DECORATE label
            // resolution is "virtual" (ie relative to the calling thingtype).
            state_t *state;
            if(mo && (state = E_ArgAsStateLabel(mo, index)))
               return state->index;
            else
            {
               // whatever it is, we dunno of it.
               eval->type = EVALTYPE_STATENUM;
               eval->value.i = -1;
            }
         }      
      }
      else
      {
         eval->type = EVALTYPE_STATENUM;
         
         // it is a DeHackEd number if it is >= 0
         if(num >= 0)
            eval->value.i = E_StateNumForDEHNum((int)num);
         else
            eval->value.i = (int)num;
      }
   }

   return eval->value.i;
}

//
// E_ArgAsThingFlags
//
// Gets the arg value at index i as a set of thing flag masks, if such argument 
// exists. The evaluated value will be cached so that it can be returned on 
// subsequent calls. If the arg does not exist, NULL is returned.
//
int *E_ArgAsThingFlags(arglist_t *al, int index)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return NULL;

   eval = &(al->values[index]);

   if(eval->type != EVALTYPE_THINGFLAG)
   {
      eval->type = EVALTYPE_THINGFLAG;

      // empty string is zero
      if(*(al->args[index]) != '\0')
      {
         int *flagvals = deh_ParseFlagsCombined(al->args[index]);

         memcpy(eval->value.flags, flagvals, MAXFLAGFIELDS * sizeof(int));
      }
      else
         memset(eval->value.flags, 0, MAXFLAGFIELDS * sizeof(int));
   }

   return eval->value.flags;
}

//
// E_ArgAsSound
//
// Gets the arg value at index i as a sound, if such argument exists.
// The evaluated value will be cached so that it can be returned on subsequent
// calls. If the arg does not exist, NULL is returned.
//
sfxinfo_t *E_ArgAsSound(arglist_t *al, int index)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return NULL;

   eval = &(al->values[index]);

   if(eval->type != EVALTYPE_SOUND)
   {
      char *pos = NULL;
      long num;

      eval->type = EVALTYPE_SOUND;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(pos && *pos != '\0')
      {
         // it is a name
         eval->value.s = E_SoundForName(al->args[index]);
      }
      else
      {
         // it is a DeHackEd number
         eval->value.s = E_SoundForDEHNum((int)num);
      }
   }

   return eval->value.s;
}

//
// E_ArgAsBexptr
//
// Gets the arg value at index i as a bexptr index, if such argument exists.
// The evaluated value will be cached so that it can be returned on subsequent
// calls. If the arg does not exist, -1 is returned.
//
int E_ArgAsBexptr(arglist_t *al, int index)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return -1;

   eval = &(al->values[index]);

   if(eval->type != EVALTYPE_BEXPTR)
   {
      deh_bexptr *ptr = D_GetBexPtr(al->args[index]);

      eval->type    = EVALTYPE_BEXPTR;
      eval->value.i = ptr ? ptr - deh_bexptrs : -1;
   }

   return eval->value.i;
}

//
// E_ArgAsEDFString
//
// Gets the arg value at index i as an EDF string, if such argument exists.
// The evaluated value will be cached so that it can be returned on subsequent
// calls. If the arg does not exist, NULL is returned.
//
edf_string_t *E_ArgAsEDFString(arglist_t *al, int index)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return NULL;

   eval = &(al->values[index]);

   if(eval->type != EVALTYPE_EDFSTRING)
   {
      char *pos = NULL;
      long num;

      eval->type = EVALTYPE_EDFSTRING;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(pos && *pos != '\0')
      {
         // it is a name
         eval->value.estr = E_StringForName(al->args[index]);
      }
      else
      {
         // it is a string number
         eval->value.estr = E_StringForNum((int)num);
      }
   }

   return eval->value.estr;
}

//
// E_ArgAsKwd
//
// Gets the argument at index i as the corresponding enumeration value for a
// keyword string, given the provided keyword set. Returns the default value
// if the argument doesn't exist, or the keyword is not matched.
//
int E_ArgAsKwd(arglist_t *al, int index, argkeywd_t *kw, int defvalue)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return defvalue;

   eval = &(al->values[index]);

   if(eval->type != EVALTYPE_KEYWORD)
   {
      char *pos = NULL;
      long num;

      eval->type = EVALTYPE_KEYWORD;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(pos && *pos != '\0')
      {
         // it is a name
         eval->value.i = E_StrToNumLinear(kw->keywords, 
                                          kw->numkeywords,
                                          al->args[index]);

         if(eval->value.i == kw->numkeywords)
            eval->value.i = defvalue;
      }
      else
      {
         // it is just a number
         eval->value.i = (int)num;
      }
   }

   return eval->value.i;
}

// EOF

