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
#include "e_args.h"
#include "e_things.h"
#include "e_states.h"


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

// EOF

