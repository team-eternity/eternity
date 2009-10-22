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
#include "e_lib.h"
#include "e_mod.h"
#include "e_states.h"
#include "e_things.h"
#include "e_sound.h"
#include "e_string.h"
#include "e_args.h"


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
      al->args[al->numargs++] = strdup(value);
      added = true;
   }

   return added;
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
      eval->type       = EVALTYPE_EDFSTRING;
      eval->value.estr = E_StringForName(al->args[index]);
   }

   return eval->value.estr;
}

//
// E_ArgAsKeywordValue
//
// Gets the argument at index i as the corresponding enumeration value for a
// keyword string, given the provided keyword set. Returns the default value
// if the argument doesn't exist, or the keyword is not matched.
//
int E_ArgAsKeywordValue(arglist_t *al, int index, const char **keywords, 
                        int numkwds, int defvalue)
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
         eval->value.i = E_StrToNumLinear(keywords, numkwds, al->args[index]);

         if(eval->value.i == numkwds)
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

