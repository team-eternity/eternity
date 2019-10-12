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
//   Generalized line action system - Bindings
//
//-----------------------------------------------------------------------------

#ifndef EV_BINDINGS_H__
#define EV_BINDINGS_H__

#include "ev_specials.h"

struct ev_lockdef_t
{
   int special; // special number
   int lockID;  // lockdef ID number
};

#define EV_LOCKDEF_NULL        0
#define EV_LOCKDEF_REDCARD     1
#define EV_LOCKDEF_BLUECARD    2
#define EV_LOCKDEF_YELLOWCARD  3
#define EV_LOCKDEF_REDSKULL    4
#define EV_LOCKDEF_BLUESKULL   5
#define EV_LOCKDEF_YELLOWSKULL 6
#define EV_LOCKDEF_ANYKEY      100
#define EV_LOCKDEF_ALL6        101
#define EV_LOCKDEF_REDGREEN    129
#define EV_LOCKDEF_BLUE        130
#define EV_LOCKDEF_YELLOW      131
#define EV_LOCKDEF_ALL3        229

extern ev_action_t  NullAction;
extern ev_action_t  BoomGenAction;

extern ev_binding_t DOOMBindings[];
extern ev_binding_t HereticBindings[];
extern ev_binding_t HexenBindings[];
extern ev_binding_t PSXBindings[];
extern ev_binding_t UDMFEternityBindings[];
extern ev_binding_t ACSBindings[];

extern const size_t DOOMBindingsLen;
extern const size_t HereticBindingsLen;
extern const size_t HexenBindingsLen;
extern const size_t PSXBindingsLen;
extern const size_t UDMFEternityBindingsLen;
extern const size_t ACSBindingsLen;

extern ev_lockdef_t DOOMLockDefs[];
extern ev_lockdef_t HereticLockDefs[];

extern const size_t DOOMLockDefsLen;
extern const size_t HereticLockDefsLen;

void EV_InitUDMFToExtraDataLookup();

#endif

// EOF

