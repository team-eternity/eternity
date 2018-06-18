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
//      Action Pointer Functions
//      Argument structure
//
//-----------------------------------------------------------------------------

#ifndef A_ARGS_H__
#define A_ARGS_H__

struct arglist_t;
class  Mobj;
struct pspdef_t;

struct actionargs_t
{
   // activation type enumeration
   enum actiontype_e
   {
      MOBJFRAME,   // invoked from P_SetMobjState
      WEAPONFRAME, // invoked from P_SetPsprite
      ARTIFACT     // invoked from E_TryUseItem
   } actiontype;

   Mobj      *actor; // Actor for either type of invocation; use mo->player when needed
   pspdef_t  *pspr;  // psprite, only valid if actiontype is WEAPONFRAME
   arglist_t *args;  // EDF arguments list; potentially NULL, but all e_args funcs check.
};

#endif

// EOF

