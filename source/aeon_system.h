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
// Purpose: Aeon system
// Authors: Samuel Villarreal, Max Waine
//

#ifndef AEON_INIT_H__
#define AEON_INIT_H__

#include "angelscript.h"

//
// Adapted from Powerslave EX's kexScriptManager into (effectively) a static class
//
class AeonScriptManager
{
public:
   AeonScriptManager() = delete;

   static void Init();
   static void Shutdown();

   static asIScriptEngine  *Engine()  { return engine; }
   static asIScriptContext *Context() { return ctx;    }
   static asIScriptModule  *Module()  { return module; }
private:
   static void RegisterTypedefs();
   static void RegisterPrimitivePrintFuncs();
   static void MessageCallback(const asSMessageInfo *msg, void *param);

   static asIScriptEngine  *engine;
   static asIScriptContext *ctx;
   static asIScriptModule  *module;
};

#endif

// EOF

