// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//----------------------------------------------------------------------------
//
// Aeon Engine - SpiderMonkey JavaScript Engine Interface
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifdef EE_FEATURE_AEONJS

#include "z_zone.h"

#include "ae_jsengine.h"
#include "hal/i_platform.h"

// JSAPI Configuration
#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
#include "ae_jscfgw32.h"
#endif

// JSAPI
#include "../js/src/jsapi.h"

//============================================================================
//
// Private Routines and Data
//

#define AEON_JS_RUNTIME_HEAP_SIZE 64L * 1024L * 1024L
#define AEON_JS_STACK_CHUNK_SIZE  8192

static JSRuntime *gRuntime;
static JSContext *gContext;
static JSObject  *gGlobal;

//
// JS Global Object
//

static JSBool global_enumerate(JSContext *cx, JSObject *obj)
{
   return JS_EnumerateStandardClasses(cx, obj);
}

static JSBool global_resolve(JSContext *cx, JSObject *obj, jsval id,
                             uintN flags, JSObject **objp)
{
   if((flags & JSRESOLVE_ASSIGNING) == 0)
   {
      JSBool resolved;

      if(!JS_ResolveStandardClass(cx, obj, id, &resolved))
         return JS_FALSE;

      if(resolved)
      {
         *objp = obj;
         return JS_TRUE;
      }
   }

   return JS_TRUE;
}

static JSClass global_class =
{
   "global",                                   // name
   JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS, // flags
   JS_PropertyStub,                            // addProperty
   JS_PropertyStub,                            // delProperty
   JS_PropertyStub,                            // getProperty
   JS_PropertyStub,                            // setProperty
   global_enumerate,                           // enumerate
   (JSResolveOp)global_resolve,                // resolve
   JS_ConvertStub,                             // convert
   JS_FinalizeStub,                            // finalize
   JSCLASS_NO_OPTIONAL_MEMBERS                 // getObjectOps etc.
};

static JSFunctionSpec global_funcs[] =
{
   JS_FS_END
};

//============================================================================
//
// AeonJSEngine
//
// Concrete implementation of AeonEngine interfacing with SpiderMonkey 1.8.0,
// to provide support for ECMAScript 3 (aka JavaScript).
//

bool AeonJSEngine::initEngine()
{
   if(!(gRuntime = JS_NewRuntime(AEON_JS_RUNTIME_HEAP_SIZE)))
      return false;

   if(!(gContext = JS_NewContext(gRuntime, AEON_JS_STACK_CHUNK_SIZE)))
      return false;

   if(!(gGlobal = JS_NewObject(gContext, &global_class, NULL, NULL)))
      return false;

   JS_SetGlobalObject(gContext, gGlobal);
   if(!JS_DefineFunctions(gContext, gGlobal, global_funcs))
      return false;

   return true;
}

void AeonJSEngine::shutDown()
{
   if(gContext)
   {
      JS_DestroyContext(gContext);
      gContext = NULL;
   }

   if(gRuntime)
   {
      JS_DestroyRuntime(gRuntime);
      gRuntime = NULL;
      JS_ShutDown();
   }
}

bool AeonJSEngine::evaluateString(const char *name, const char *script)
{
   return false;
}

bool AeonJSEngine::evaluateFile(const char *filename)
{
   return false;
}

AeonJSEngine::CompiledScript *AeonJSEngine::compileString(const char *name, const char *script)
{
   return NULL;
}

AeonJSEngine::CompiledScript *AeonJSEngine::compileFile(const char *filename)
{
   return NULL;
}

bool AeonJSEngine::executeCompiledScript(CompiledScript *cs)
{
   return false;
}

bool AeonJSEngine::executeCompiledScriptWithResult(CompiledScript *cs, qstring &qstr)
{
   return false;
}

bool AeonJSEngine::executeCompiledScriptWithResult(CompiledScript *cs, int &i)
{
   return false;
}

bool AeonJSEngine::executeCompiledScriptWithResult(CompiledScript *cs, unsigned int &ui)
{
   return false;
}

bool AeonJSEngine::executeCompiledScriptWithResult(CompiledScript *cs, double &d)
{
   return false;
}

bool AeonJSEngine::executeCompiledScriptWithResult(CompiledScript *cs, bool &b)
{
   return false;
}

#endif // EE_FEATURE_AEONJS

// EOF

