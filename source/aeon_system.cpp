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
// A lot of this code is based on Quasar's astest.
//
// Purpose: Aeon system
// Authors: Samuel Villarreal, James Haley, Max Waine
//

#include "scriptarray.h"

#include "aeon_common.h"
#include "aeon_math.h"
#include "aeon_mobj.h"
#include "aeon_string.h"
#include "aeon_system.h"
#include "c_io.h"
#include "d_dwfile.h"
#include "doomstat.h"
#include "i_system.h"
#include "m_utils.h"
#include "sounds.h"
#include "w_wad.h"

namespace Aeon
{
   asIScriptEngine  *ScriptManager::engine = nullptr;
   asIScriptContext *ScriptManager::ctx    = nullptr;
   asIScriptModule  *ScriptManager::module = nullptr;
   int               ScriptManager::state  = asEXECUTION_UNINITIALIZED;

   void ASPrint(int i)
   {
      C_Printf("%d\n", i);
   }

   void ASPrint(unsigned int u)
   {
      C_Printf("%u\n", u);
   }

   void ASPrint(float f)
   {
      C_Printf("%f\n", f);
   }

   void ScriptManager::RegisterPrimitivePrintFuncs()
   {
      engine->RegisterGlobalFunction("void print(int)",
                                     WRAP_FN_PR(ASPrint, (int), void),
                                     asCALL_GENERIC);
      engine->RegisterGlobalFunction("void print(uint)",
                                     WRAP_FN_PR(ASPrint, (unsigned int), void),
                                     asCALL_GENERIC);
      engine->RegisterGlobalFunction("void print(float)",
                                     WRAP_FN_PR(ASPrint, (float), void),
                                     asCALL_GENERIC);
   }

   void ScriptManager::RegisterTypedefs()
   {
      engine->RegisterTypedef("char",    "int8");
      engine->RegisterTypedef("uchar",   "uint8");

      engine->RegisterTypedef("int8_t",   "int8");
      engine->RegisterTypedef("int16_t",  "int16");
      engine->RegisterTypedef("int32_t",  "int32");
      engine->RegisterTypedef("int64_t",  "int64");
      engine->RegisterTypedef("uint8_t",  "uint8");
      engine->RegisterTypedef("uint16_t", "uint16");
      engine->RegisterTypedef("uint32_t", "uint32");
      engine->RegisterTypedef("uint64_t", "uint64");
   }

   void ScriptManager::RegisterHandleOnlyClasses()
   {
      engine->SetDefaultNamespace("EE");
      engine->RegisterObjectType("Sound", sizeof(sfxinfo_t), asOBJ_REF | asOBJ_NOCOUNT);
      engine->SetDefaultNamespace("");
   }

   void ScriptManager::MessageCallback(const asSMessageInfo *msg, void *param)
   {
      const char *type = "ERR ";
      if(msg->type == asMSGTYPE_WARNING)
         type = "WARN";
      else if(msg->type == asMSGTYPE_INFORMATION)
         type = "INFO";
      printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
   }

   void ScriptManager::Init()
   {
      puts("Aeon::ScriptManager::Init: Setting up AngelScript.");

      // create AngelScript engine instance
      if(!(engine = asCreateScriptEngine(ANGELSCRIPT_VERSION)))
         I_Error("Aeon::ScriptManager::Init: Could not create AngelScript engine\n");

      // set message callback
      if(engine->SetMessageCallback(asFUNCTION(ScriptManager::MessageCallback),
                                    nullptr, asCALL_CDECL) < 0)
         I_Error("Aeon::ScriptManager::Init: Could not set AngelScript message callback\n");

      // set engine properties
      engine->SetEngineProperty(asEP_SCRIPT_SCANNER,         0); // ASCII
      engine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, 1); // allow 'c' to be a char

      RegisterPrimitivePrintFuncs();
      RegisterTypedefs();
      RegisterHandleOnlyClasses();
      RegisterScriptArray(engine, true);

      ScriptObjString::Init();

      ScriptObjFixed::Init();
      ScriptObjAngle::Init();
      ScriptObjVector::Init();
      ScriptObjMath::Init();

      ScriptObjMobj::Init();

      if(!(module = engine->GetModule("core", asGM_CREATE_IF_NOT_EXISTS)))
         I_Error("Aeon::ScriptManager::Init: Could not create module\n");

      // create execution context
      if(!(ctx = engine->CreateContext()))
         I_Error("Aeon::ScriptManager::Init: Could not create execution context\n");

      atexit(Shutdown);
   }

   void ScriptManager::Shutdown()
   {
      ctx->Release();
      engine->Release();
   }

   void ScriptManager::PushState()
   {
       state = ctx->GetState();

       if(state == asEXECUTION_ACTIVE)
           ctx->PushState();
   }

   void ScriptManager::PopState()
   {
       if(state == asEXECUTION_ACTIVE)
           ctx->PopState();
   }

   bool ScriptManager::PrepareFunction(asIScriptFunction *function)
   {
      if(function == nullptr)
         return false;
      if(ctx->GetState() == asEXECUTION_SUSPENDED)
         return false;

      PushState();

      if(ctx->Prepare(function) != 0)
      {
         PopState();
         return false;
      }

      return true;
   }

   bool ScriptManager::PrepareFunction(const char *function)
   {
       return PrepareFunction(module->GetFunctionByName(function));
   }

   bool ScriptManager::Execute()
   {
       if(ctx->Execute() == asEXECUTION_EXCEPTION)
       {
           PopState();
           I_Error("AeonScriptManager::Execute: %s\n", ctx->GetExceptionString());
           return false;
       }

       PopState();
       return true;
   }
}

// EOF

