//
// The Eternity Engine
// Copyright(C) 2019 James Haley, Max Waine, et al.
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
// Purpose: Aeon wrapper for player_t
// Authors: Max Waine
//

#include "aeon_common.h"
#include "aeon_player.h"
#include "aeon_system.h"
#include "d_player.h"

namespace Aeon
{
   void ScriptObjPlayer::PreInit()
   {
      asIScriptEngine *const e = ScriptManager::Engine();

      e->SetDefaultNamespace("EE");

      e->RegisterObjectType("Player", sizeof(player_t), asOBJ_REF | asOBJ_NOCOUNT);

      e->SetDefaultNamespace("");
   }

   void ScriptObjPlayer::Init()
   {
      asIScriptEngine *const e = ScriptManager::Engine();

      e->SetDefaultNamespace("EE");

      

      e->SetDefaultNamespace("");
   }
}


// EOF

