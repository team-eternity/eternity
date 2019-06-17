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
#include "e_weapons.h"
#include "p_mobj.h"

namespace Aeon
{
   //
   // Sanity checked getter for plyr->weaponctrs->getIndexedCounterForPlayer(plyr, ctrnum)
   // Returns 0 on failure
   //
   static int getWeaponCounter(const unsigned int ctrnum, player_t *player)
   {
      if(ctrnum >= 0 && ctrnum < NUMWEAPONTYPES)
         return *player->weaponctrs->getIndexedCounterForPlayer(player, ctrnum);
      else
         return 0; // TODO: C_Printf warning?
   }

   //
   // Sanity checked setter for plyr->weaponctrs->setCounter(plyr, ctrnum, val)
   // Doesn't set on failure
   //
   static void setWeaponCounter(const unsigned int ctrnum, const int val, player_t *player)
   {
      if(ctrnum >= 0 && ctrnum < NUMWEAPONTYPES)
          player->weaponctrs->setCounter(player, ctrnum, val);
      // TODO: else C_Printf warning?
   }

   static const aeonfuncreg_t playerFuncs[] =
   {

      // Indexed property accessors (enables [] syntax for counters)
      { "int get_weaponcounters(const uint ctrnum) const",           WRAP_OBJ_LAST(getWeaponCounter)  },
      { "void set_weaponcounters(const uint ctrnum, const int val)", WRAP_OBJ_LAST(setWeaponCounter)  },
   };

   static const aeonpropreg_t playerProps[] =
   {
      { "int refire",     asOFFSET(player_t, refire) },

      { "Mobj @const mo", asOFFSET(player_t, mo)     },
   };

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

      // Register all Aeon Player properties
      for(const aeonpropreg_t &prop : playerProps)
         e->RegisterObjectProperty("Player", prop.declaration, prop.byteOffset);

      // Register all Aeon Player methods
      for(const aeonfuncreg_t &fn : playerFuncs)
         e->RegisterObjectMethod("Player", fn.declaration, fn.funcPointer, asCALL_GENERIC);

      e->SetDefaultNamespace("");
   }
}


// EOF

