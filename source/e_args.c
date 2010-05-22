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
boolean E_AddArgToList(arglist_t *al, const char *value)
{
   boolean added = false;
   
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
boolean E_SetArg(arglist_t *al, int index, const char *value)
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
boolean E_SetArgFromNumber(arglist_t *al, int index, int value)
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


//
// E_ArgAsStateNum
//
// Gets the arg value at index i as a state number, if such argument exists.
// The evaluated value will be cached so that it can be returned on subsequent
// calls. If the arg does not exist, the null state is returned instead.
//
int E_ArgAsStateNum(arglist_t *al, int index)
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

      eval->type = EVALTYPE_STATENUM;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(pos && *pos != '\0')
      {
         // it is a name
         eval->value.i = E_SafeStateName(al->args[index]);
      }
      else
      {
         // it is a DeHackEd number
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
// calls. If the arg does not exist, NUMSTATES is returned.
//
// NI == No Invalid, because invalid states are not converted to the null state.
//
int E_ArgAsStateNumNI(arglist_t *al, int index)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return NUMSTATES;

   eval = &(al->values[index]);

   if(eval->type != EVALTYPE_STATENUM)
   {
      char *pos = NULL;
      long num;

      eval->type = EVALTYPE_STATENUM;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(pos && *pos != '\0')
      {
         // it is a name
         eval->value.i = E_StateNumForName(al->args[index]);
      }
      else
      {
         // it is a DeHackEd number
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
int E_ArgAsStateNumG0(arglist_t *al, int index)
{
   evalcache_t *eval;

   // if the arglist doesn't exist or doesn't hold this many arguments,
   // return the default value.
   if(!al || index >= al->numargs)
      return NUMSTATES;

   eval = &(al->values[index]);

   if(eval->type != EVALTYPE_STATENUM)
   {
      char *pos = NULL;
      long num;

      eval->type = EVALTYPE_STATENUM;

      // see if this is a string or an integer
      num = strtol(al->args[index], &pos, 0);

      if(pos && *pos != '\0')
      {
         // it is a name
         eval->value.i = E_StateNumForName(al->args[index]);
      }
      else
      {
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

