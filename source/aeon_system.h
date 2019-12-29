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
// Purpose: Aeon system
// Authors: Samuel Villarreal, Max Waine
//

#ifndef AEON_SYSTEM_H__
#define AEON_SYSTEM_H__

class  asIScriptContext;
class  asIScriptEngine;
class  asIScriptFunction;
class  asIScriptModule;
struct asSMessageInfo;

namespace Aeon
{
   //
   // Adapted from Powerslave EX's kexScriptManager into (effectively) a static class
   //
   class ScriptManager
   {
   public:
      ScriptManager() = delete;

      static void Init();
      static void Build();
      static void Shutdown();

      static asIScriptEngine  *Engine()  { return engine; }
      static asIScriptContext *Context() { return ctx;    }
      static asIScriptModule  *Module()  { return module; }

      static void PushState();
      static void PopState();
      static bool PrepareFunction(asIScriptFunction *function);
      static bool PrepareFunction(const char *function);
      static bool Execute();
   private:
      static void RegisterPrimitivePrintFuncs();
      static void RegisterTypedefs();
      static void RegisterHandleOnlyClasses();
      static void RegisterScriptObjs();
      static void RegisterGlobalProperties();
      static void MessageCallback(const asSMessageInfo *msg, void *param);

      static asIScriptEngine  *engine;
      static asIScriptContext *ctx;
      static asIScriptModule  *module;

      static int state;
   };
}

#endif

// EOF

