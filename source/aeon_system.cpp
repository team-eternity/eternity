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
// Purpose: Aeon initialisation
// Authors: James Haley, Max Waine
//

#include "aeon_common.h"
#include "aeon_fixed.h"
#include "aeon_string.h"
#include "c_io.h"
#include "d_dwfile.h"
#include "doomstat.h"
#include "i_system.h"
#include "m_utils.h"
#include "w_wad.h"

static asIScriptEngine *engine;

//
// AngelScript message callback
//
static void MessageCallback(const asSMessageInfo *msg, void *param)
{
   const char *type = "ERR ";
   if(msg->type == asMSGTYPE_WARNING)
      type = "WARN";
   else if(msg->type == asMSGTYPE_INFORMATION)
      type = "INFO";
   printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

//
// Register typedefs
//
static void RegisterTypedefs(asIScriptEngine *e)
{
   e->RegisterTypedef("char", "int8");
   e->RegisterTypedef("uchar", "uint8");
   //e->RegisterTypedef("angle", "uint32");
}

static void RegisterPrintFuncs(asIScriptEngine *e)
{
   e->RegisterGlobalFunction("void print(int)",
                             WRAP_FN_PR(ASPrint, (int), void),
                             asCALL_GENERIC);
   e->RegisterGlobalFunction("void print(uint)",
                             WRAP_FN_PR(ASPrint, (unsigned int), void),
                             asCALL_GENERIC);
   e->RegisterGlobalFunction("void print(float)",
                             WRAP_FN_PR(ASPrint, (float), void),
                             asCALL_GENERIC);
}

int Aeon_Init()
{
   // create AngelScript engine instance
   engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
   if(!engine)
      I_Error("Aeon_Init: Could not create AngelScript engine\n");

   // set message callback
   if(engine->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL) < 0)
      I_Error("Aeon_Init: Could not set AngelScript message callback\n");

   // set engine properties
   engine->SetEngineProperty(asEP_SCRIPT_SCANNER,         0); // ASCII
   engine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, 1); // allow 'c' to be a char

   // Register typedefs
   RegisterTypedefs(engine);

   // Register print functions for primitive types
   RegisterPrintFuncs(engine);

   // Register fixed-point number type
   AeonScriptObjFixed::Init(engine);

   // Register qstring type
   AeonScriptObjQString::Init(engine);

   // FIXME: Below is temporary gross hacks
   DWFILE dwfile;
   dwfile.openFile(M_SafeFilePath(basepath, "test.asc"), "rb");
   char *buf = ecalloc(char *, dwfile.fileLength(), sizeof(char));
   for(char *temp = buf; !dwfile.atEof(); temp++)
      *temp = dwfile.getChar();

   asIScriptModule *asmodule = engine->GetModule("core", asGM_CREATE_IF_NOT_EXISTS);
   if(!asmodule)
      I_Error("Aeon_Init: Could not create module\n");

   if(asmodule->AddScriptSection("section", buf, dwfile.fileLength(), 0) < 0)
      I_Error("Aeon_Init: Could not add code to module\n");

    if(asmodule->Build() < 0)
      I_Error("Aeon_Init: Could not build module\n");

   // call main function
   auto func = asmodule->GetFunctionByDecl("void main()");
   if(!func)
      I_Error("Aeon_Init: Could not find main function in script\n");

   // create execution context
   auto ctx = engine->CreateContext();
   if(!ctx)
      I_Error("Aeon_Init: Could not create execution context\n");

   // prepare
   if(ctx->Prepare(func) < 0)
      I_Error("Aeon_Init: Prepare failed\n");

   // execute
   int r = ctx->Execute();
   if(r != asEXECUTION_FINISHED)
   {
      if(r == asEXECUTION_EXCEPTION)
         C_Printf("An exception '%s' occurred.\n", ctx->GetExceptionString());
   }

      // shut down
      ctx->Release();
      engine->Release();

   return 0;
}

// EOF

