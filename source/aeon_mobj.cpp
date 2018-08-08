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

static Mobj *MobjFactory()
{
   return new Mobj();
}

static Mobj *MobjFactoryFromOther(const Mobj &in)
{
   return new Mobj(in);
}

static aeonfuncreg_t mobjFuncs[]
{
   { "int getModifiedSpawnHealth() const", WRAP_MFN(Mobj, getModifiedSpawnHealth) },
};

#define REGISTERMOBJFLAG(flag) e->RegisterEnumValue("EnumMobjFlags", #flag, flag);

extern dehflags_t deh_mobjflags[];

void AeonScriptObjMobj::Init()
{
   asIScriptEngine *e = AeonScriptManager::Engine();

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

   e->RegisterObjectProperty("Mobj", "fixed x", asOFFSET(Mobj, x));
   e->RegisterObjectProperty("Mobj", "fixed y", asOFFSET(Mobj, y));
   e->RegisterObjectProperty("Mobj", "fixed z", asOFFSET(Mobj, z));

   for(const aeonfuncreg_t &fn : mobjFuncs)
      e->RegisterObjectMethod("Mobj", fn.declaration, fn.funcPointer, asCALL_GENERIC);

   e->RegisterEnum("EnumMobjFlags");
   e->RegisterEnum("EnumMobjFlags2");
   e->RegisterEnum("EnumMobjFlags3");
   e->RegisterEnum("EnumMobjFlags4");
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

