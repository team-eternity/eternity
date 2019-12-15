//
// The Eternity Engine
// Copyright(C) 2018 James Haley, Max Waine, et al.
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
// Purpose: Aeon wrapper for Mobj
// Authors: Max Waine
//

#include "aeon_common.h"
#include "aeon_math.h"
#include "aeon_mobj.h"
#include "aeon_system.h"
#include "d_dehtbl.h"
#include "m_qstr.h"
#include "p_enemy.h"
#include "p_mobj.h"
#include "s_sound.h"

#include "p_map.h"

namespace Aeon
{
   static Mobj *mobjFactory()
   {
      return new Mobj();
   }

   static Mobj *mobjFactoryFromOther(const Mobj &in)
   {
      return new Mobj(in);
   }

   //
   // Sanity checked getter for mo->counters[ctrnum]
   // Returns 0 on failure
   //
   static int getMobjCounter(const unsigned int ctrnum, Mobj *mo)
   {
      if(ctrnum >= 0 && ctrnum < NUMMOBJCOUNTERS)
         return mo->counters[ctrnum];
      else
         return 0; // TODO: C_Printf warning?
   }

   //
   // Sanity checked setter for mo->counters[ctrnum]
   // Doesn't set on failure
   //
   static void setMobjCounter(const unsigned int ctrnum, const int val, Mobj *mo)
   {
      if(ctrnum >= 0 && ctrnum < NUMMOBJCOUNTERS)
         mo->counters[ctrnum] = val;
      // TODO: else C_Printf warning?
   }

   static Fixed floatBobOffsets(const int in)
   {
      static constexpr int NUMFLOATBOBOFFSETS = earrlen(FloatBobOffsets);
      if(in < 0 || in > NUMFLOATBOBOFFSETS)
         return 0;
      return FloatBobOffsets[in];
   }

   static void startSound(const PointThinker *origin, const sfxinfo_t &sfxinfo)
   {
      S_StartSound(origin, sfxinfo.dehackednum);
   }

   static Fixed aimLineAttack(Mobj *t1, Angle angle, Fixed distance, int flags)
   {
      return Fixed(P_AimLineAttack(t1, angle.value, distance.value,
                                   flags & 1 ? true : false));
   }

   static void lineAttack(Mobj *t1, Angle angle, Fixed distance, Fixed slope,
                              int damage, const qstring &pufftype)
   {
      P_LineAttack(t1, angle.value, distance.value, slope.value, damage, pufftype.constPtr());
   }

   static const aeonfuncreg_t mobjFuncs[] =
   {
      // Native Mobj methods
      { "void backupPosition()",              WRAP_MFN(Mobj, backupPosition)                  },
      { "void copyPosition(Mobj @other)",     WRAP_MFN(Mobj, copyPosition)                    },
      { "int getModifiedSpawnHealth() const", WRAP_MFN(Mobj, getModifiedSpawnHealth)          },

      // Non-methods that are used like methods in Aeon
      { "bool checkSight(Mobj @other)",                        WRAP_OBJ_FIRST(P_CheckSight)   },
      { "bool hitFriend()",                                    WRAP_OBJ_FIRST(P_HitFriend)    },
      { "void startSound(const EE::Sound &in)",                WRAP_OBJ_FIRST(startSound)     },
      { "bool tryMove(fixed_t x, fixed_t y, int dropoff = 0)", WRAP_OBJ_FIRST(P_TryMove)      },
      {
         "fixed_t aimLineAttack(angle_t angle, fixed_t distance, alaflags_e flags = 0)",
         WRAP_OBJ_FIRST(aimLineAttack)
      },
      {
         "void lineAttack(angle_t angle, fixed_t distance, fixed_t slope, int damage, const String &pufftype)",
         WRAP_OBJ_FIRST(lineAttack)
      },

      // Indexed property accessors (enables [] syntax for counters)
      { "int get_counters(const uint ctrnum) const property",           WRAP_OBJ_LAST(getMobjCounter)  },
      { "void set_counters(const uint ctrnum, const int val) property", WRAP_OBJ_LAST(setMobjCounter)  },
   };

   static const aeonpropreg_t mobjProps[] =
   {
      { "fixed_t x",            asOFFSET(Mobj, x)      },
      { "fixed_t y",            asOFFSET(Mobj, y)      },
      { "fixed_t z",            asOFFSET(Mobj, z)      },
      { "angle_t angle",        asOFFSET(Mobj, angle)  },

      { "fixed_t radius",       asOFFSET(Mobj, radius) },
      { "fixed_t height",       asOFFSET(Mobj, height) },
      { "vector_t mom",         asOFFSET(Mobj, mom)    },

      { "int health",           asOFFSET(Mobj, health) },

      { "Mobj @target",         asOFFSET(Mobj, target) },
      { "Mobj @tracer",         asOFFSET(Mobj, tracer) },

      { "Player @const player", asOFFSET(Mobj, player) },
   };

   #define DECLAREMOBJFLAGS(x) \
      e->RegisterEnum("mobjflags" #x "_e"); \
      e->RegisterObjectProperty("Mobj", "mobjflags" #x "_e flags" #x, asOFFSET(Mobj, flags ##x));

   void ScriptObjMobj::PreInit()
   {
      asIScriptEngine *const e = ScriptManager::Engine();

      e->SetDefaultNamespace("EE");

      e->RegisterObjectType("Mobj", sizeof(Mobj), asOBJ_REF);

      e->SetDefaultNamespace("");
   }

   void ScriptObjMobj::Init()
   {
      extern dehflags_t deh_mobjflags[];
      asIScriptEngine *const e = ScriptManager::Engine();

      e->SetDefaultNamespace("EE");

      e->RegisterObjectBehaviour("Mobj", asBEHAVE_FACTORY, "Mobj @f()",
                                 WRAP_FN(mobjFactory), asCALL_GENERIC);
      e->RegisterObjectBehaviour("Mobj", asBEHAVE_FACTORY, "Mobj @f(const Mobj &in)",
                                 WRAP_FN_PR(mobjFactoryFromOther, (const Mobj &), Mobj *),
                                 asCALL_GENERIC);
      e->RegisterObjectBehaviour("Mobj", asBEHAVE_ADDREF, "void f()",
                                 WRAP_MFN(Mobj, addReference), asCALL_GENERIC);
      e->RegisterObjectBehaviour("Mobj", asBEHAVE_RELEASE, "void f()",
                                 WRAP_MFN(Mobj, delReference), asCALL_GENERIC);

      for(const aeonpropreg_t &prop : mobjProps)
         e->RegisterObjectProperty("Mobj", prop.declaration, prop.byteOffset);

      DECLAREMOBJFLAGS();
      DECLAREMOBJFLAGS(2);
      DECLAREMOBJFLAGS(3);
      DECLAREMOBJFLAGS(4);
      for(dehflags_t *flag = deh_mobjflags; flag->name != nullptr; flag++)
      {
         qstring type("mobjflags");
         qstring name("MF");
         if(flag->index > 0)
         {
            name << (flag->index + 1);
            type << (flag->index + 1);
         }
         type << "_e";

         name << "_" << flag->name;
         e->RegisterEnumValue(type.constPtr(), name.constPtr(), flag->value);
      }

      // Flags used by EE::Mobj::aimLineAttack
      e->RegisterEnum("alaflags_e");
      e->RegisterEnumValue("alaflags_e", "ALF_SKIPFRIEND", 1);

      // Register all Aeon Mobj methods
      for(const aeonfuncreg_t &fn : mobjFuncs)
         e->RegisterObjectMethod("Mobj", fn.declaration, fn.funcPointer, asCALL_GENERIC);


      // It's Mobj-related, OK?
      e->SetDefaultNamespace("EE::Mobj");
      e->RegisterGlobalFunction("fixed_t FloatBobOffsets(const int index)",
                                WRAP_FN(floatBobOffsets), asCALL_GENERIC);
      e->SetDefaultNamespace("");
   }
}


// EOF

