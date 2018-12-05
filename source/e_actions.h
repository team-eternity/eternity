//
// Copyright (C) 2018 James Haley, Max Waine, et al.
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
// Purpose: EDF Aeon Actions
// Authors: Samuel Villarreal, Max Waine
//

#ifndef E_ACTIONS_H__
#define E_ACTIONS_H__

// Required for EMAXARGS
#include "e_args.h"

// Required for DLListItem
#include "m_dllist.h"

// Required for qstring
#include "m_qstr.h"

struct action_t;
struct actionargs_t;
struct cfg_t;

enum actionargtype_e : int8_t
{
   AAT_INVALID = -1,
   AAT_INTEGER =  0,
   AAT_FIXED,
   AAT_STRING,
   AAT_SOUND,
};

enum actioncalltype_e : uint8_t
{
   ACT_ACTIONARGS,
   ACT_MOBJ,
   ACT_PLAYER,
   ACT_PLAYER_W_PSPRITE,
};

// AngelScript (Aeon) action
struct actiondef_t
{
   const char      *name;
   actionargtype_e  argTypes[EMAXARGS];
   void             *defaultArgs[EMAXARGS];
   int              numArgs;
   actioncalltype_e callType;

   DLListItem<actiondef_t> links;  // hash by name
};

#ifdef NEED_EDF_DEFINITIONS

#define EDF_SEC_ACTION "action"

extern cfg_opt_t edf_action_opts[];

#endif

actiondef_t *E_AeonActionForName(const char *name);
action_t *E_GetAction(const char *name);

void E_CollectActions(cfg_t *cfg);
void E_ProcessActions(cfg_t *cfg);

#endif

// EOF

