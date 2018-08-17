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
#include "aeon_mobj.h"
#include "aeon_system.h"
#include "d_dehtbl.h"
#include "m_qstr.h"
#include "p_mobj.h"

#include "p_map.h"

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

static aeonfuncreg_t mobjFuncs[]
{
   { "int getModifiedSpawnHealth() const", WRAP_MFN(Mobj, getModifiedSpawnHealth) },

   // TODO: Test if WRAP_OBJ_FIRST works. If so use that instead
   { "bool tryMove(eFixed x, eFixed y, int dropoff)", // WRAP_OBJ_FIRST(P_TryMove) },
      WRAP_OBJ_FIRST_PR(P_TryMove, (Mobj *, fixed_t, fixed_t, int), bool) },

   // Indexed property accessors (enables [] syntax for counters)
   { "int get_counters(const uint ctrnum) const",           WRAP_OBJ_LAST(GetMobjCounter)},
   { "void set_counters(const uint ctrnum, const int val)", WRAP_OBJ_LAST(SetMobjCounter)},
};

#define DECLAREMOBJFLAGS(x) \
   e->RegisterEnum("EnumMobjFlags" #x); \
   e->RegisterObjectProperty("eMobj", "EnumMobjFlags" #x " flags" #x, asOFFSET(Mobj, flags ##x));

void AeonScriptObjMobj::Init()
{
   extern dehflags_t deh_mobjflags[];
   asIScriptEngine *e = AeonScriptManager::Engine();

   e->RegisterObjectType("eMobj", sizeof(Mobj), asOBJ_REF);

   e->RegisterObjectBehaviour("eMobj", asBEHAVE_FACTORY, "eMobj @f()",
                              WRAP_FN(MobjFactory), asCALL_GENERIC);
   e->RegisterObjectBehaviour("eMobj", asBEHAVE_FACTORY, "eMobj @f(const eMobj &in)",
                              WRAP_FN_PR(MobjFactoryFromOther, (const Mobj &), Mobj *),
                              asCALL_GENERIC);
   e->RegisterObjectBehaviour("eMobj", asBEHAVE_ADDREF, "void f()",
                              WRAP_MFN(Mobj, addReference), asCALL_GENERIC);
   e->RegisterObjectBehaviour("eMobj", asBEHAVE_RELEASE, "void f()",
                              WRAP_MFN(Mobj, delReference), asCALL_GENERIC);

   for(const aeonfuncreg_t &fn : mobjFuncs)
      e->RegisterObjectMethod("eMobj", fn.declaration, fn.funcPointer, asCALL_GENERIC);

   e->RegisterObjectProperty("eMobj", "eFixed x", asOFFSET(Mobj, x));
   e->RegisterObjectProperty("eMobj", "eFixed y", asOFFSET(Mobj, y));
   e->RegisterObjectProperty("eMobj", "eFixed z", asOFFSET(Mobj, z));
   e->RegisterObjectProperty("eMobj", "eAngle angle", asOFFSET(Mobj, angle));

   e->RegisterObjectProperty("eMobj", "eFixed radius", asOFFSET(Mobj, radius));
   e->RegisterObjectProperty("eMobj", "eFixed height", asOFFSET(Mobj, height));

   // FIXME? Change to property accessors
   e->RegisterObjectProperty("eMobj", "eVector mom", asOFFSET(Mobj, momx));

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
}


// EOF

