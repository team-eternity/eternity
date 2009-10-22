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

#ifndef E_ARGS_H__
#define E_ARGS_H__

// needed for MAXFLAGFIELDS:
#include "d_dehtbl.h"

// 16 arguments ought to be enough for anybody.
#define EMAXARGS 16

typedef enum
{
   EVALTYPE_NONE,      // not cached
   EVALTYPE_INT,       // evaluated to an integer
   EVALTYPE_FIXED,     // evaluated to a fixed_t
   EVALTYPE_DOUBLE,    // evaluated to a double
   EVALTYPE_THINGNUM,  // evaluated to a thing number
   EVALTYPE_STATENUM,  // evaluated to a state number
   EVALTYPE_THINGFLAG, // evaluated to a thing flag bitmask
   EVALTYPE_NUMTYYPES
} evaltype_e;

typedef struct evalcache_s
{
   evaltype_e type;
   union evalue_s
   {
      int     i;
      fixed_t x;
      double  d;
      int     flags[MAXFLAGFIELDS];
   } value;
} evalcache_t;

typedef struct arglist_s
{
   const char *args[EMAXARGS];   // argument strings stored from EDF
   evalcache_t values[EMAXARGS]; // if type != EVALTYPE_NONE, cached value
   int numargs;                  // number of arguments
} arglist_t;

#endif

// EOF

