//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: EDF switch definitions.
// Authors: Ioan Chera
//

#ifndef E_SWITCH_H_
#define E_SWITCH_H_

#include "Confuse/confuse.h"
#include "m_collection.h"
#include "m_dllist.h"
#include "m_qstr.h"

constexpr const char EDF_SEC_SWITCH[] = "switch";

struct cfg_t;

//
// EDF switch definition
//
class ESwitchDef : public ZoneObject
{
public:
   ESwitchDef() : episode(), link()
   {
   }

   //
   // Call reset on an unlinked definition
   //
   void reset();
   bool emptyDef() const
   {
      return onpic.empty() && onsound.empty() && offsound.empty();
   }

   qstring offpic;
   qstring onpic;
   qstring onsound;
   qstring offsound;
   int episode;
   DLListItem<ESwitchDef> link;
};

extern cfg_opt_t edf_switch_opts[];
extern PODCollection<ESwitchDef *> eswitches;

void E_ProcessSwitches(cfg_t *cfg);
void E_AddSwitchDef(const ESwitchDef &def);
const ESwitchDef *E_SwitchForName(const char *name);

#endif

// EOF

