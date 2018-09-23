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
#include "p_mobj.h"
#include "s_sound.h"

#include "p_map.h"

namespace Aeon
{
   static Mobj *MobjFactory()
   {
      return new Mobj();
   }

   static Mobj *MobjFactoryFromOther(const Mobj &in)
   {
      return new Mobj(in);
   }

   //
   // Sanity checked getter for mo->counters[ctrnum]
   // Returns 0 on failure
   //
   static int GetMobjCounter(const unsigned int ctrnum, Mobj *mo)
   {
      if(ctrnum >= 0 && ctrnum < NUMMOBJCOUNTERS)
         return mo->counters[ctrnum];
      else
         return 0; // TODO: C_Printf warning?
   }

   //
   // Sanity checked getter for mo->counters[ctrnum]
   // Doesn't set on failure
   //
   static void SetMobjCounter(const unsigned int ctrnum, const int val, Mobj *mo)
   {
      if(ctrnum >= 0 && ctrnum < NUMMOBJCOUNTERS)
         mo->counters[ctrnum] = val;
      // TODO: else C_Printf warning?
   }

   Fixed FloatBobOffsets(const int in)
   {
      static constexpr int NUMFLOATBOBOFFSETS = earrlen(::FloatBobOffsets);
      if(in < 0 || in > NUMFLOATBOBOFFSETS)
         return 0;
      return ::FloatBobOffsets[in];
   }

   static void MobjStartSound(const PointThinker *origin, sfxinfo_t *sfxinfo)
   {
      if(sfxinfo)
         S_StartSound(origin, sfxinfo->dehackednum);
   }

   static const aeonfuncreg_t mobjFuncs[]
   {
      // Native Mobj methods
      { "void backupPosition()",              WRAP_MFN(Mobj, backupPosition)                  },
      { "void copyPosition(Mobj @other)",     WRAP_MFN(Mobj, copyPosition)                    },
      { "int getModifiedSpawnHealth() const", WRAP_MFN(Mobj, getModifiedSpawnHealth)          },

      // Non-methods that are used like methods in Aeon
      { "bool tryMove(fixed_t x, fixed_t y, int dropoff)",     WRAP_OBJ_FIRST(P_TryMove)      },
      { "void startSound(EE::Sound @sound)",                   WRAP_OBJ_FIRST(MobjStartSound) },

      // Indexed property accessors (enables [] syntax for counters)
      { "int get_counters(const uint ctrnum) const",           WRAP_OBJ_LAST(GetMobjCounter)  },
      { "void set_counters(const uint ctrnum, const int val)", WRAP_OBJ_LAST(SetMobjCounter)  },

      // Statics
      "fixed_t FloatBobOffsets(const int index)", WRAP_FN(FloatBobOffsets)
   };

   #define DECLAREMOBJFLAGS(x) \
      e->RegisterEnum("EnumMobjFlags" #x); \
      e->RegisterObjectProperty("Mobj", "EnumMobjFlags" #x " flags" #x, asOFFSET(Mobj, flags ##x));

   void ScriptObjMobj::Init()
   {
      extern dehflags_t deh_mobjflags[];
      asIScriptEngine *e = ScriptManager::Engine();

      e->SetDefaultNamespace("EE");

      e->RegisterObjectType("Mobj", sizeof(Mobj), asOBJ_REF);

      e->RegisterObjectBehaviour("Mobj", asBEHAVE_FACTORY, "Mobj @f()",
                                 WRAP_FN(MobjFactory), asCALL_GENERIC);
      e->RegisterObjectBehaviour("Mobj", asBEHAVE_FACTORY, "Mobj @f(const Mobj &in)",
                                 WRAP_FN_PR(MobjFactoryFromOther, (const Mobj &), Mobj *),
                                 asCALL_GENERIC);
      e->RegisterObjectBehaviour("Mobj", asBEHAVE_ADDREF, "void f()",
                                 WRAP_MFN(Mobj, addReference), asCALL_GENERIC);
      e->RegisterObjectBehaviour("Mobj", asBEHAVE_RELEASE, "void f()",
                                 WRAP_MFN(Mobj, delReference), asCALL_GENERIC);

      for(const aeonfuncreg_t &fn : mobjFuncs)
         e->RegisterObjectMethod("Mobj", fn.declaration, fn.funcPointer, asCALL_GENERIC);

      e->RegisterObjectProperty("Mobj", "fixed_t x",      asOFFSET(Mobj, x));
      e->RegisterObjectProperty("Mobj", "fixed_t y",      asOFFSET(Mobj, y));
      e->RegisterObjectProperty("Mobj", "fixed_t z",      asOFFSET(Mobj, z));
      e->RegisterObjectProperty("Mobj", "angle_t angle",  asOFFSET(Mobj, angle));

      e->RegisterObjectProperty("Mobj", "fixed_t radius", asOFFSET(Mobj, radius));
      e->RegisterObjectProperty("Mobj", "fixed_t height", asOFFSET(Mobj, height));
      e->RegisterObjectProperty("Mobj", "vector_t mom",   asOFFSET(Mobj, mom));

      e->RegisterObjectProperty("Mobj", "int health",     asOFFSET(Mobj, health));

      e->RegisterObjectProperty("Mobj", "Mobj @target",   asOFFSET(Mobj, target));
      e->RegisterObjectProperty("Mobj", "Mobj @tracer",   asOFFSET(Mobj, tracer));

      DECLAREMOBJFLAGS();
      DECLAREMOBJFLAGS(2);
      DECLAREMOBJFLAGS(3);
      DECLAREMOBJFLAGS(4);
      for(dehflags_t *flag = deh_mobjflags; flag->name != nullptr; flag++)
      {
         qstring type("EnumMobjFlags");
         qstring name = qstring("MF");
         if(flag->index > 0)
         {
            name << (flag->index + 1);
            type <<  (flag->index + 1);
         }

         name << "_" << flag->name;
         e->RegisterEnumValue(type.constPtr(), name.constPtr(), flag->value);
      }

      // It's Mobj-related, OK?
      e->SetDefaultNamespace("EE::Mobj");
      e->RegisterGlobalFunction("fixed_t FloatBobOffsets(const int index)",
                                WRAP_FN(FloatBobOffsets), asCALL_GENERIC);

      e->SetDefaultNamespace("");
   }
}


// EOF

