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
//----------------------------------------------------------------------------
//
// Purpose: Aeon wrapper for player_t
// Authors: Max Waine
//

#include "aeon_common.h"
#include "aeon_math.h"
#include "aeon_player.h"
#include "aeon_system.h"
#include "d_player.h"
#include "e_things.h"
#include "e_weapons.h"
#include "m_qstr.h"
#include "p_mobj.h"
#include "p_user.h"

namespace Aeon
{
   static void thrust(player_t *plyr, Angle angle, Angle pitch, Fixed move)
   {
      P_Thrust(plyr, angle.value, pitch.value, move.value);
   }

   static void subtractAmmo(player_t *plyr, int amount = -1)
   {
      P_SubtractAmmo(plyr, amount);
   }

    static Mobj *spawnPlayerMissile(player_t *plyr, const qstring &name)
    {
        const mobjtype_t thingnum = E_GetThingNumForName(name.constPtr());

        return P_SpawnPlayerMissile(plyr->mo, thingnum);
    }

    static bool playerOwnsWeapon(player_t *plyr, const qstring &name)
    {
       weaponinfo_t *wp = E_WeaponForName(name.constPtr());

       return wp ? E_PlayerOwnsWeapon(plyr, wp) : false;
    }

   //
   // Sanity checked getter for plyr->weaponctrs->getIndexedCounterForPlayer(plyr, ctrnum)
   // Returns 0 on failure
   //
   static int getWeaponCounter(const int ctrnum, player_t *player)
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
   static void setWeaponCounter(const int ctrnum, const int val, player_t *player)
   {
      if(ctrnum >= 0 && ctrnum < NUMWEAPONTYPES)
          player->weaponctrs->setCounter(player, ctrnum, val);
      // TODO: else C_Printf warning?
   }

   static weaponinfo_t *getReadyWeapon(player_t *player)
   {
      return player->readyweapon;
   }

   static void setReadyWeapon(weaponinfo_t *wp, player_t *player)
   {
      if(wp == nullptr)
         return;

      player->readyweapon = wp;
      if(wp == player->pendingweapon)
         player->readyweaponslot = player->pendingweaponslot; // I dunno how else to do this
      else
         player->readyweaponslot = E_FindFirstWeaponSlot(player, wp);
   }

   static weaponinfo_t *getPendingWeapon(player_t *player)
   {
      return player->pendingweapon;
   }

   static void setPendingWeapon(const weaponinfo_t *wp, player_t *player)
   {
      if(!E_PlayerOwnsWeapon(player, player->readyweapon) && player->readyweapon->id != UnknownWeaponInfo)
      {
         player->pendingweapon     = E_FindBestWeapon(player);
         player->pendingweaponslot = E_FindFirstWeaponSlot(player, player->pendingweapon);
      }
   }

   static const aeonfuncreg_t playerFuncs[] =
   {
      { "void thrust(angle_t angle, angle_t pitch, fixed_t move)",   WRAP_OBJ_FIRST(thrust)             },
      { "void subtractAmmo(int amount = -1)",                        WRAP_OBJ_FIRST(subtractAmmo)       },
      { "Mobj @spawnMissile(const String &missileType) const",       WRAP_OBJ_FIRST(spawnPlayerMissile) },
      { "bool ownsWeapon(const String &weaponName) const",           WRAP_OBJ_FIRST(playerOwnsWeapon)   },
      { "bool checkAmmo() const",                                    WRAP_OBJ_FIRST(P_CheckAmmo)        },

      // Indexed property accessors (enables [] syntax for counters)
      { "int get_weaponcounters(const int ctrnum) const property",           WRAP_OBJ_LAST(getWeaponCounter)    },
      { "void set_weaponcounters(const int ctrnum, const int val) property", WRAP_OBJ_LAST(setWeaponCounter)    },

      // Getters and settings for ready and pending weapon
      { "Weapon @get_readyweapon() const property",                  WRAP_OBJ_LAST(getReadyWeapon)      },
      { "void set_readyweapon(Weapon @wp) property",                 WRAP_OBJ_LAST(setReadyWeapon)      },
      { "Weapon @get_pendingweapon() const property",                WRAP_OBJ_LAST(getPendingWeapon)    },
      { "void set_pendingweapon(Weapon @wp) property",               WRAP_OBJ_LAST(setPendingWeapon)    },
   };

   static const aeonpropreg_t playerProps[] =
   {
      { "int health",     asOFFSET(player_t, health) },

      { "int refire",     asOFFSET(player_t, refire) },

      { "Mobj @const mo", asOFFSET(player_t, mo)     },
   };

   static const aeonpropreg_t pspriteProps[] =
   {
      // I don't think state is required
      { "int tics",   asOFFSET(pspdef_t, tics) },
      { "fixed_t sx", asOFFSET(pspdef_t, sx)   },
      { "fixed_t sy", asOFFSET(pspdef_t, sy)   },
      // trans is never set anywhere, seemingly. The hell?
   };

   void ScriptObjPlayer::PreInit()
   {
      asIScriptEngine *const e = ScriptManager::Engine();

      e->SetDefaultNamespace("EE");

      e->RegisterObjectType("Player",  sizeof(player_t), asOBJ_REF | asOBJ_NOCOUNT);
      e->RegisterObjectType("Psprite", sizeof(pspdef_t), asOBJ_REF | asOBJ_NOCOUNT);

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

      for(const aeonpropreg_t &prop : pspriteProps)
         e->RegisterObjectProperty("Psprite", prop.declaration, prop.byteOffset);

      e->SetDefaultNamespace("");
   }
}


// EOF

