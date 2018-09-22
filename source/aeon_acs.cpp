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
// Purpose: Aeon bindings for interop with ACS
// Authors: Max Waine
//

#include "scriptarray.h"

#include "z_zone.h"

#include "acs_intr.h"
#include "aeon_acs.h"
#include "aeon_common.h"
#include "aeon_system.h"
#include "doomtype.h"
#include "m_qstr.h"

namespace Aeon
{
   static int GetStringIndex(qstring &str)
   {
      return ~ACSenv.getString(str.constPtr())->idx;
   }

   static bool StartACSScriptS(Mobj *mo, const qstring &name,
                               const uint32_t mapnum, const CScriptArray *args)
   {
      const uint32_t argc = args ? args->GetSize() : 0;
      uint32_t *argv = nullptr;

      if(argc > 0)
      {
         argv = ecalloc(uint32_t *, argc, sizeof(uint32_t));
         for(uint32_t i = 0; i < argc; i++)
            argv[i] = *static_cast<const uint32_t *>(args->At(i));
      }

      const bool ret = ACS_ExecuteScriptS(name.constPtr(), mapnum, argv, argc, mo,
                                          nullptr, 0, nullptr);
      if(argv)
         efree(argv);
      return ret;
   }

   static bool StartACSScriptSAlways(Mobj *mo, const qstring &name,
                                     const uint32_t mapnum, const CScriptArray *args)
   {
      const uint32_t argc = args ? args->GetSize() : 0;
      uint32_t *argv = nullptr;

      if(argc > 0)
      {
         argv = ecalloc(uint32_t *, argc, sizeof(uint32_t));
         for(uint32_t i = 0; i < argc; i++)
            argv[i] = *static_cast<const uint32_t *>(args->At(i));
      }

      const bool ret = ACS_ExecuteScriptSAlways(name.constPtr(), mapnum, argv, argc, mo,
                                                nullptr, 0, nullptr);
      if(argv)
         efree(argv);
      return ret;
   }

   static uint32_t StartACSScriptSResult(Mobj *mo, const qstring &name, const CScriptArray *args)
   {
      const uint32_t argc = args ? args->GetSize() : 0;
      uint32_t *argv = nullptr;

      if(argc > 0)
      {
         argv = ecalloc(uint32_t *, argc, sizeof(uint32_t));
         for(uint32_t i = 0; i < argc; i++)
            argv[i] = *static_cast<const uint32_t *>(args->At(i));
      }

      const uint32_t ret = ACS_ExecuteScriptSResult(name.constPtr(), argv, argc,
                                                    mo, nullptr, 0, nullptr);
      if(argv)
         efree(argv);
      return ret;
   }

   static bool StartACSScriptI(Mobj *mo, const int scriptnum,
                               const uint32_t mapnum, const CScriptArray *args)
   {
      const uint32_t argc = args ? args->GetSize() : 0;
      uint32_t *argv = nullptr;

      if(argc > 0)
      {
         argv = ecalloc(uint32_t *, argc, sizeof(uint32_t));
         for(uint32_t i = 0; i < argc; i++)
            argv[i] = *static_cast<const uint32_t *>(args->At(i));
      }

      const bool ret = ACS_ExecuteScriptI(scriptnum, mapnum, argv, argc, mo,
                                          nullptr, 0, nullptr);
      if(argv)
         efree(argv);
      return ret;
   }

   static bool StartACSScriptIAlways(Mobj *mo, const int scriptnum,
                                     const uint32_t mapnum, const CScriptArray *args)
   {
      const uint32_t argc = args ? args->GetSize() : 0;
      uint32_t *argv = nullptr;

      if(argc > 0)
      {
         argv = ecalloc(uint32_t *, argc, sizeof(uint32_t));
         for(uint32_t i = 0; i < argc; i++)
            argv[i] = *static_cast<const uint32_t *>(args->At(i));
      }

      const bool ret = ACS_ExecuteScriptIAlways(scriptnum, mapnum, argv, argc, mo,
                                                nullptr, 0, nullptr);
      if(argv)
         efree(argv);
      return ret;
   }

   static uint32_t StartACSScriptIResult(Mobj *mo, const int scriptnum, const CScriptArray *args)
   {
      const uint32_t argc = args ? args->GetSize() : 0;
      uint32_t *argv = nullptr;

      if(argc > 0)
      {
         argv = ecalloc(uint32_t *, argc, sizeof(uint32_t));
         for(uint32_t i = 0; i < argc; i++)
            argv[i] = *static_cast<const uint32_t *>(args->At(i));
      }

      const uint32_t ret = ACS_ExecuteScriptIResult(scriptnum, argv, argc, mo,
                                                    nullptr, 0, nullptr);
      if(argv)
         efree(argv);
      return ret;
   }

   // ACS script starting function that requires map number as a parameter
   #define MAPSIG(name, idarg) "bool " name " (EE::Mobj @mo, " idarg ","        \
                                              "const uint32 mapnum,"            \
                                              "const array<int> @args = null)"
   // ACS script starting function that returns a result (and has no mapnum parameter)
   #define RSLTSIG(name, idarg) "uint32 " name "(EE::Mobj @mo, " idarg ","      \
                                               "const array<int> @args = null)"

   static aeonfuncreg_t acsFuncs[] =
   {
      { "int GetStringIndex(String &str)",                   WRAP_FN(GetStringIndex)        },
      { MAPSIG("StartScript",       "const String &name"),   WRAP_FN(StartACSScriptS)       },
      { MAPSIG("StartScriptAlways", "const String &name"),   WRAP_FN(StartACSScriptSAlways) },
      { MAPSIG("StartScript",       "const int scriptnum"),  WRAP_FN(StartACSScriptI)       },
      { MAPSIG("StartScriptAlways", "const int scriptnum"),  WRAP_FN(StartACSScriptIAlways) },
      { RSLTSIG("StartScriptResult", "const String &name"),  WRAP_FN(StartACSScriptSResult) },
      { RSLTSIG("StartScriptResult", "const int scriptnum"), WRAP_FN(StartACSScriptSResult) },
   };

   void ScriptObjACS::Init()
   {
      asIScriptEngine *e = ScriptManager::Engine();

      e->SetDefaultNamespace("EE::ACS");

      for(const aeonfuncreg_t &fn : acsFuncs)
         e->RegisterGlobalFunction(fn.declaration, fn.funcPointer, asCALL_GENERIC);

      e->SetDefaultNamespace("");
   }
}

// EOF

