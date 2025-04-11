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
// EDF DECORATE States Implementation
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_DSTATE_H__
#define E_DSTATE_H__

// Required for statenum_t:
#include "info.h"

//
// DECORATE state output structure 1:
//
// DECORATE state. This is a label plus a pointer to the state_t to which it 
// refers (which is already a member of the states array). This can be assigned
// to either a native state or a metastate by the calling code.
//
struct edecstate_t
{
   char    *label; // native state name or metastate key (allocated)
   state_t *state; // pointer to state
};

//
// DECORATE state output structure 2:
//
// Goto record. This is a goto destination label which requires external
// resolution by the caller because it refers to an ancestral definition
// (super state, or a label not defined within the current state block).
// The data consists of the original jump label text, offset, and a pointer
// to the nextstate field of the state_t which needs to have its nextstate 
// set.
//
struct egoto_t
{
   char       *label;     // label text (allocated)
   int         offset;    // offset, if non-zero
   statenum_t *nextstate; // pointer to field needing resolved value
};

//
// DECORATE state output structure 3:
//
// Kill state. This is a state which needs to be removed from the object
// which is receiving this set of DECORATE states.
//
struct ekillstate_t
{
   char *killname; // native state name or metastate key to nullify/delete
};

//
// Aggregate DECORATE state parsing output
//
struct edecstateout_t
{
   edecstate_t  *states;     // states to create
   egoto_t      *gotos;      // gotos to resolve
   ekillstate_t *killstates; // states to kill
   int numstates;            // number of states to add
   int numgotos;             // number of gotos to resolve
   int numkillstates;        // number of states to kill
   int numstatesalloc;       // number of state objects allocated
   int numgotosalloc;        // number of goto objects allocated
   int numkillsalloc;        // number of kill states allocated
};

edecstateout_t *E_ParseDecorateStates(const char *owner, const char *input, 
                                      const char *firststate);
void E_FreeDSO(edecstateout_t *dso);

#endif

// EOF

