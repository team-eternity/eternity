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
#include "m_qstr.h"

// JSAPI Configuration
#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
#include "ae_jscfgw32.h"
#elif EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX
#include "ae_jscfglinux.h"
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

static JSBool Aeon_JS_globalEnumerate(JSContext *cx, JSObject *obj)
{
   return JS_EnumerateStandardClasses(cx, obj);
}

static JSBool Aeon_JS_globalResolve(JSContext *cx, JSObject *obj, jsval id,
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
   Aeon_JS_globalEnumerate,                    // enumerate
   (JSResolveOp)Aeon_JS_globalResolve,         // resolve
   JS_ConvertStub,                             // convert
   JS_FinalizeStub,                            // finalize
   JSCLASS_NO_OPTIONAL_MEMBERS                 // getObjectOps etc.
};

static JSFunctionSpec global_funcs[] =
{
   JS_FS_END
};

//
// Aeon_JS_evaluateFile
//
// Load and compile a file in one shot.
//
static JSBool Aeon_JS_evaluateFile(JSContext *cx, JSObject *obj,
                                   const char *filename, jsval *rval)
{
   JSScript *script = NULL;
   JSObject *gcroot = NULL;
   JSBool ok = JS_FALSE;
   uint32 oldopts;

   oldopts = JS_GetOptions(cx);
   JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO);

   if((script = JS_CompileFile(cx, obj, filename)))
   {
      if((gcroot = JS_NewScriptObject(cx, script)))
      {
         if(JS_AddNamedRoot(cx, &gcroot, "Aeon_JS_evaluateFile"))
         {
            ok = JS_ExecuteScript(cx, obj, script, rval);
            JS_RemoveRoot(cx, &gcroot);
         }
      }
   }

   JS_SetOptions(cx, oldopts);

   return ok;
}

//============================================================================
//
// AeonJSEngine::CompiledScript
//
// Proxy object for a unit of compiled bytecode, which can be cached and run
// repeatedly.
//

//
// JSCompiledScriptPimpl
//
// Private implementation details for AeonJSEngine::CompiledScript
//
class JSCompiledScriptPimpl : public ZoneObject
{
public:
   JSContext *cx;
   JSObject  *obj;
   JSScript  *script;
   JSCompiledScriptPimpl() : cx(NULL), obj(NULL), script(NULL) {}
   ~JSCompiledScriptPimpl();
   void init(JSContext *pcx, JSScript *pScript);

   bool execute(jsval *rval);
};

//
// JSCompiledScriptPimpl Destructor
//
// Remove the GC root for the compiled script so that it can be garbage
// collected.
//
JSCompiledScriptPimpl::~JSCompiledScriptPimpl()
{
   if(cx && obj)
      JS_RemoveRoot(cx, &obj);

   cx     = NULL;
   obj    = NULL;
   script = NULL;
}

//
// JSCompiledScriptPimpl::init
//
// Initialize a compiled script by adding its script object and making
// it a GC root.
//
void JSCompiledScriptPimpl::init(JSContext *pcx, JSScript *pScript)
{
   cx     = pcx;
   script = pScript;
   if((obj = JS_NewScriptObject(cx, script)))
      JS_AddNamedRoot(cx, &obj, "JSCompiledScriptPimpl::init");
}

#define ValidCompiledScript(s) ((s)->obj && (s)->script)

//
// JSCompiledScriptPimpl::execute
//
// Run the compiled script.
//
bool JSCompiledScriptPimpl::execute(jsval *rval)
{
   bool result = false;

   if(ValidCompiledScript(this))
   {
      if(JS_ExecuteScript(cx, gGlobal, script, rval) == JS_TRUE)
         result = true;
   }

   return result;
}

//
// AeonJSEngine::CompiledScript Constructor
//
AeonJSEngine::CompiledScript::CompiledScript() : AeonEngine::CompiledScript()
{
   pImpl = new JSCompiledScriptPimpl;
}

//
// AeonJSEngine::CompiledScript Destructor
//
AeonJSEngine::CompiledScript::~CompiledScript()
{
   if(pImpl)
   {
      delete pImpl;
      pImpl = NULL;
   }
}

//
// AeonJSEngine::CompiledScript::execute
//
bool AeonJSEngine::CompiledScript::execute()
{
   jsval rval;

   return pImpl->execute(&rval);
}

//
// AeonJSEngine::CompiledScript::executeWithResult
//
bool AeonJSEngine::CompiledScript::executeWithResult(qstring &qstr)
{
   bool result = false;
   jsval rval;

   if(pImpl->execute(&rval))
   {
      JSString *jstr = JS_ValueToString(pImpl->cx, rval);
      if(jstr)
      {
         qstr = JS_GetStringBytes(jstr);
         result = true;
      }
   }

   return result;
}

//
// AeonJSEngine::CompiledScript::executeWithResult
//
bool AeonJSEngine::CompiledScript::executeWithResult(int &i)
{
   bool result = false;
   jsval rval;

   if(pImpl->execute(&rval))
   {
      int32 res = 0;
      if(JS_ValueToECMAInt32(pImpl->cx, rval, &res))
      {
         i = static_cast<int>(res);
         result = true;
      }
   }

   return result;   
}

//
// AeonJSEngine::CompiledScript::executeWithResult
//
bool AeonJSEngine::CompiledScript::executeWithResult(unsigned int &ui)
{
   bool result = false;
   jsval rval;

   if(pImpl->execute(&rval))
   {
      uint32 res = 0;
      if(JS_ValueToECMAUint32(pImpl->cx, rval, &res))
      {
         ui = static_cast<unsigned int>(res);
         result = true;
      }
   }

   return result;   
}

//
// AeonJSEngine::CompiledScript::executeWithResult
//
bool AeonJSEngine::CompiledScript::executeWithResult(double &d)
{
   bool result = false;
   jsval rval;

   if(pImpl->execute(&rval))
   {
      jsdouble res = 0.0;
      if(JS_ValueToNumber(pImpl->cx, rval, &res))
      {
         d = static_cast<double>(res);
         result = true;
      }
   }

   return result;   
}

//
// AeonJSEngine::CompiledScript::executeWithResult
//
bool AeonJSEngine::CompiledScript::executeWithResult(bool &b)
{
   bool result = false;
   jsval rval;

   if(pImpl->execute(&rval))
   {
      JSBool res = JS_FALSE;
      if(JS_ValueToBoolean(pImpl->cx, rval, &res))
      {
         b = (res == JS_TRUE);
         result = true;
      }
   }

   return result;   
}

//============================================================================
//
// AeonJSEngine
//
// Concrete implementation of AeonEngine interfacing with SpiderMonkey 1.8.0,
// to provide support for ECMAScript 3 (aka JavaScript).
//

//
// AeonJSEngine::initEngine
//
// Initialize core components of the JSAPI
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

//
// AeonJSEngine::shutDown
//
// Destroy and shutdown the js32 library.
//
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

//
// AeonJSEngine::evaluateString
//
// Evaluate a string as a one-shot script.
//
bool AeonJSEngine::evaluateString(const char *name, const char *script)
{
   jsval  rval;
   JSBool result;

   result = JS_EvaluateScript(gContext, gGlobal, script, strlen(script), name,
                              0, &rval);
   
   return (result == JS_TRUE);
}

//
// AeonJSEngine::evaluateFile
//
// Evaluate a file as a one-shot script.
//
bool AeonJSEngine::evaluateFile(const char *filename)
{
   jsval  rval;
   JSBool result;

   result = Aeon_JS_evaluateFile(gContext, gGlobal, filename, &rval);

   return (result == JS_TRUE);
}

//
// AeonJSEngine::compileString
//
// Compile the provided string data as a script and return it wrapped in a 
// persistent CompiledScript object. This object can then be executed 
// repeatedly. Returns NULL on failure.
//
AeonJSEngine::CompiledScript *AeonJSEngine::compileString(const char *name, 
                                                          const char *script)
{
   CompiledScript *ret = NULL;
   JSScript *s;

   if((s = JS_CompileScript(gContext, gGlobal, script, strlen(script), name, 0)))
   {
      ret = new CompiledScript;
      ret->pImpl->init(gContext, s);
   }

   return ret;
}

//
// AeonJSEngine::compileFile
//
// Compile the provided disk file as a script and return it wrapped in a 
// persistent CompiledScript object. This object can then be executed 
// repeatedly. Returns NULL on failure.
//
AeonJSEngine::CompiledScript *AeonJSEngine::compileFile(const char *filename)
{
   CompiledScript *ret = NULL;
   JSScript *s;

   if((s = JS_CompileFile(gContext, gGlobal, filename)))
   {
      ret = new CompiledScript;
      ret->pImpl->init(gContext, s);
   }

   return ret;
}

#endif // EE_FEATURE_AEONJS

// EOF

