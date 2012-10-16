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

#include "c_io.h"
#include "c_runcmd.h"
#include "m_qstr.h"
#include "v_misc.h"

// AeonJS modules
#include "ae_engine.h"
#include "ae_jsapi.h"
#include "ae_jsengine.h"
#include "ae_jsprivate.h"
#include "ae_jsutils.h"

using namespace AeonJS;

//============================================================================
//
// Global Data
//

// RTTI Proxy for AeonJS::PrivateData
IMPLEMENT_RTTI_TYPE(AeonJS::PrivateData)

AeonJS::ConsoleHook *AeonJS::ConsoleHook::curJSHook;

//============================================================================
//
// Private Routines and Data
//

#define AEON_JS_RUNTIME_HEAP_SIZE 64L * 1024L * 1024L
#define AEON_JS_STACK_CHUNK_SIZE  8192

static JSRuntime   *gRuntime;
static EvalContext *gEvalContext;

//=============================================================================
//
// JS Evaluation Context
//
// An evaluation context consists of a JSContext and a JSObject instance which
// serves as that context's global object.
//

//
// JavaScript sandbox class
//

// Enumeration callback
static JSBool AeonJS_sandboxEnumerate(JSContext *cx, JSObject *obj)
{
   jsval v;
   JSBool b;

   if(!JS_GetProperty(cx, obj, "lazy", &v) || !JS_ValueToBoolean(cx, v, &b))
      return JS_FALSE;

   return !b || JS_EnumerateStandardClasses(cx, obj);
}

// Resolution callback
static JSBool AeonJS_sandboxResolve(JSContext *cx, JSObject *obj, jsval id, 
                                    uintN flags, JSObject **objp)
{
   jsval v;
   JSBool b, resolved;

   if(!JS_GetProperty(cx, obj, "lazy", &v) || !JS_ValueToBoolean(cx, v, &b))
      return JS_FALSE;
   
   if(b && !(flags & JSRESOLVE_ASSIGNING)) 
   {
      if(!JS_ResolveStandardClass(cx, obj, id, &resolved))
         return JS_FALSE;

      if(resolved) 
      {
         *objp = obj;
         return JS_TRUE;
      }
   }

   *objp = NULL;
   return JS_TRUE;
}

// Sandbox JSClass
static JSClass sandbox_class =
{
   "sandbox",
   JSCLASS_NEW_RESOLVE,
   JS_PropertyStub,
   JS_PropertyStub,
   JS_PropertyStub,
   JS_PropertyStub,
   AeonJS_sandboxEnumerate,
   (JSResolveOp)AeonJS_sandboxResolve,
   JS_ConvertStub,
   JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

// EvalContext - Private implementation object
class JSEvalContextPimpl : public ZoneObject
{
public:
   AutoContext  autoContext;
   JSObject    *global;

   JSEvalContextPimpl()
      : ZoneObject(), autoContext(gRuntime, AEON_JS_STACK_CHUNK_SIZE), 
        global(NULL)
   {
   }
};

//
// EvalContext Constructor
//
AeonJS::EvalContext::EvalContext() : ZoneObject()
{
   pImpl = new JSEvalContextPimpl();
}

//
// EvalContext Destructor
//
AeonJS::EvalContext::~EvalContext()
{
   if(pImpl)
   {
      delete pImpl;
      pImpl = NULL;
   }
}

//
// EvalContext::setGlobal
//
// Instantiate an object of the indicated JSClass as the global object of the
// contained JSContext.
//
bool AeonJS::EvalContext::setGlobal(JSClass *globalClass)
{
   JSContext *ctx = pImpl->autoContext.ctx;

   if(ctx && (pImpl->global = JS_NewObject(ctx, globalClass, NULL, NULL)))
   {
      JS_SetGlobalObject(ctx, pImpl->global);
      return true;
   }
   else
      return false;
}

//
// EvalContext::getContext
//
// Get the JSContext for this evaluation context.
//
JSContext *AeonJS::EvalContext::getContext() const
{
   return pImpl->autoContext.ctx;
}

//
// EvalContext::getGlobal
//
// Get a pointer to the global object instance for this evaluation context.
//
JSObject *AeonJS::EvalContext::getGlobal() const
{
   return pImpl->global;
}

//=============================================================================
//
// JS Global Object
//

//
// Aeon_JS_globalEnumerate
//
// Lazy enumeration for the ECMA standard classes.
// Doing this is said to lower memory usage.
//
static JSBool Aeon_JS_globalEnumerate(JSContext *cx, JSObject *obj)
{
   return JS_EnumerateStandardClasses(cx, obj);
}

//
// Aeon_JS_globalResolve
//
// Lazy resolution for the ECMA standard classes.
//
static JSBool Aeon_JS_globalResolve(JSContext *cx, JSObject *obj, jsval id,
                                    uintN flags, JSObject **objp)
{
   if((flags & JSRESOLVE_ASSIGNING) == 0)
   {
      JSBool resolved = JS_FALSE;

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

//
// global_class
//
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

//
// Aeon_JS_evaluateFile
//
// Compile and execute a file in one shot.
//
static JSBool Aeon_JS_evaluateFile(JSContext *cx, JSObject *obj,
                                   const char *filename, jsval *rval)
{
   JSScript *script = NULL;
   void     *gcroot = NULL;
   JSBool ok = JS_FALSE;
   uint32 oldopts;

   oldopts = JS_GetOptions(cx);
   JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO);

   if((script = JS_CompileFile(cx, obj, filename)))
   {
      if((gcroot = JS_NewScriptObject(cx, script)))
      {
         AutoNamedRoot root(cx, &gcroot, "Aeon_JS_evaluateFile");
         
         if(root.isValid())
            ok = JS_ExecuteScript(cx, obj, script, rval);
      }
   }

   JS_SetOptions(cx, oldopts);

   return ok;
}

//============================================================================
//
// Aeon Namespace Natives
//
// These native methods are for manipulating the state of the API itself.
//

//
// AeonJS_GC
//
// Signal an unconditional garbage collection pass.
//
static JSBool AeonJS_GC(JSContext *cx, uintN argc, jsval *vp)
{
   JS_GC(cx);

   JS_SET_RVAL(cx, vp, JSVAL_VOID);
   return JS_TRUE;
}

//
// AeonJS_MaybeGC
//
// Perform garbage collection only if it is needed.
//
static JSBool AeonJS_MaybeGC(JSContext *cx, uintN argc, jsval *vp)
{
   JS_MaybeGC(cx);

   JS_SET_RVAL(cx, vp, JSVAL_VOID);
   return JS_TRUE;
}

static JSBool AeonJS_LogPrint(JSContext *cx, uintN argc, jsval *vp)
{
   jsval *argv = JS_ARGV(cx, vp);

   if(argc >= 1)
      AeonEngine::LogPuts(AeonJS::SafeGetStringBytes(cx, argv[0], &argv[0]));

   JS_SET_RVAL(cx, vp, JSVAL_VOID);
   return JS_TRUE;
}

static JSBool AeonJS_ExitShell(JSContext *cx, uintN argc, jsval *vp)
{
   // Must be in console hook.
   if(!AeonJS::ConsoleHook::curJSHook)
   {
      JSString *str = 
         JS_NewStringCopyZ(cx, "Aeon Exception: exitShell: not in hook");
      JS_SetPendingException(cx, STRING_TO_JSVAL(str));
      return JS_FALSE;
   }

   AeonJS::ConsoleHook::curJSHook->exitHook();
   
   JS_SET_RVAL(cx, vp, JSVAL_VOID);
   return JS_TRUE;
}

static JSClass aeonJSClass =
{
   "Aeon",
   0,
   JS_PropertyStub,
   JS_PropertyStub,
   JS_PropertyStub,
   JS_PropertyStub,
   JS_EnumerateStub,
   JS_ResolveStub,
   JS_ConvertStub,
   JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec aeonJSMethods[] =
{
   JS_FN("GC",        AeonJS_GC,        0, 0, 0),
   JS_FN("maybeGC",   AeonJS_MaybeGC,   0, 0, 0),
   JS_FN("logPrint",  AeonJS_LogPrint,  1, 0, 0),
   JS_FN("exitShell", AeonJS_ExitShell, 0, 0, 0),
   JS_FS_END
};

//
// AeonJS_CreateAeonObject
//
// Create the Aeon object as a property of the global.
//
bool AeonJS_CreateAeonObject(EvalContext *evalCtx)
{
   JSContext *cx     = evalCtx->getContext();
   JSObject  *global = evalCtx->getGlobal();
   JSObject  *obj;

   if(!(obj = JS_DefineObject(cx, global, "Aeon", &aeonJSClass, NULL, JSPROP_PERMANENT)))
      return false;

   if(!JS_DefineFunctions(cx, obj, aeonJSMethods))
      return false;

   return true;
}

//============================================================================
//
// AeonJS::CompiledScript
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
   EvalContext  *evalCtx;
   JSObject     *obj;
   JSScript     *script;
   AutoNamedRoot root;

   JSCompiledScriptPimpl() : evalCtx(NULL), obj(NULL), script(NULL), root() {}

   bool execute(jsval *rval);
};

//
// JSCompiledScriptPimpl::execute
//
// Run the compiled script.
//
bool JSCompiledScriptPimpl::execute(jsval *rval)
{
   JSContext *cx     = evalCtx->getContext();
   JSObject  *global = evalCtx->getGlobal();
   bool       result = false;

   if(cx && script)
   {
      if(JS_ExecuteScript(cx, global, script, rval) == JS_TRUE)
         result = true;
   }

   return result;
}

//
// AeonJS::CompiledScript Constructor
//
AeonJS::CompiledScript::CompiledScript() : ZoneObject()
{
   pImpl = new JSCompiledScriptPimpl;

   // Default to the global evaluation context
   pImpl->evalCtx = gEvalContext;
}

//
// AeonJS::CompiledScript Destructor
//
AeonJS::CompiledScript::~CompiledScript()
{
   if(pImpl)
   {
      delete pImpl;
      pImpl = NULL;
   }
}

//
// CompiledScript::changeContext
//
// Change the script to use a different context.
//
void AeonJS::CompiledScript::changeContext(EvalContext *pEvalCtx)
{
   pImpl->evalCtx = pEvalCtx;
}

//
// CompiledScript::setScript
//
// Protected method; initialize a compiled script by adding its
// script object and making it a GC root.
//
void AeonJS::CompiledScript::setScript(JSScript *pScript)
{
   JSContext *cx = pImpl->evalCtx->getContext();
   pImpl->script = pScript;
      
   if((pImpl->obj = JS_NewScriptObject(cx, pImpl->script)))
      pImpl->root.init(cx, pImpl->obj, "JSCompiledScriptPimpl::init");
}

//
// AeonJS::CompiledScript::execute
//
bool AeonJS::CompiledScript::execute()
{
   jsval rval;

   return pImpl->execute(&rval);
}

//
// AeonJS::CompiledScript::executeWithResult
//
bool AeonJS::CompiledScript::executeWithResult(qstring &qstr)
{
   bool result = false;
   jsval rval;

   if(pImpl->execute(&rval))
   {
      AutoNamedRoot root;
      qstr   = SafeGetStringBytes(pImpl->evalCtx->getContext(), rval, root);
      result = true;
   }

   return result;
}

//
// AeonJS::CompiledScript::executeWithResult
//
bool AeonJS::CompiledScript::executeWithResult(int &i)
{
   bool result = false;
   jsval rval;

   if(pImpl->execute(&rval))
   {
      int32 res = 0;
      if(JS_ValueToECMAInt32(pImpl->evalCtx->getContext(), rval, &res))
      {
         i = static_cast<int>(res);
         result = true;
      }
   }

   return result;   
}

//
// AeonJS::CompiledScript::executeWithResult
//
bool AeonJS::CompiledScript::executeWithResult(unsigned int &ui)
{
   bool result = false;
   jsval rval;

   if(pImpl->execute(&rval))
   {
      uint32 res = 0;
      if(JS_ValueToECMAUint32(pImpl->evalCtx->getContext(), rval, &res))
      {
         ui = static_cast<unsigned int>(res);
         result = true;
      }
   }

   return result;   
}

//
// AeonJS::CompiledScript::executeWithResult
//
bool AeonJS::CompiledScript::executeWithResult(double &d)
{
   bool result = false;
   jsval rval;

   if(pImpl->execute(&rval))
   {
      jsdouble res = 0.0;
      if(JS_ValueToNumber(pImpl->evalCtx->getContext(), rval, &res))
      {
         d = static_cast<double>(res);
         result = true;
      }
   }

   return result;   
}

//
// AeonJS::CompiledScript::executeWithResult
//
bool AeonJS::CompiledScript::executeWithResult(bool &b)
{
   bool result = false;
   jsval rval;

   if(pImpl->execute(&rval))
   {
      JSBool res = JS_FALSE;
      if(JS_ValueToBoolean(pImpl->evalCtx->getContext(), rval, &res))
      {
         b = (res == JS_TRUE);
         result = true;
      }
   }

   return result;   
}

//
// CompiledScript::CompileString
//
// Compile the provided string data as a script and return it wrapped in a 
// persistent CompiledScript object. This object can then be executed 
// repeatedly. Returns NULL on failure.
//
AeonJS::CompiledScript *
AeonJS::CompiledScript::CompileString(const char *name, const char *script, 
                                      EvalContext *evalCtx)
{
   CompiledScript *ret = NULL;
   JSScript *s;

   if(!evalCtx)
      evalCtx = gEvalContext;

   if((s = JS_CompileScript(evalCtx->getContext(), evalCtx->getGlobal(), 
                            script, strlen(script), name, 0)))
   {
      ret = new CompiledScript();
      ret->changeContext(evalCtx);
      ret->setScript(s);
   }

   return ret;
}

//
// CompiledScript::CompileFile
//
// Compile the provided disk file as a script and return it wrapped in a 
// persistent CompiledScript object. This object can then be executed 
// repeatedly. Returns NULL on failure.
//
CompiledScript *AeonJS::CompiledScript::CompileFile(const char *filename, 
                                                    EvalContext *evalCtx)
{
   CompiledScript *ret = NULL;
   JSScript *s;

   if(!evalCtx)
      evalCtx = gEvalContext;

   if((s = JS_CompileFile(evalCtx->getContext(), evalCtx->getGlobal(), filename)))
   {
      ret = new CompiledScript();
      ret->changeContext(evalCtx);
      ret->setScript(s);
   }

   return ret;
}

//============================================================================
//
// AeonJS Engine
//
// Interfacing with SpiderMonkey 1.8.0 to provide support for ECMAScript 3 
// (aka JavaScript).
//

static void AeonJS_ErrorReporter(JSContext *cx, const char *message, 
                                JSErrorReport *report)
{
   qstring msg;
   bool    addedFile = false;

   // Use error color if outputting to console
   if(AeonEngine::LogOutputMode() == AeonEngine::OUTPUT_CONSOLE)
      msg = FC_ERROR; 

   if(!report)
   {
      msg << message;
      AeonEngine::LogPuts(msg.constPtr());
      return;
   }
   
   if(report->filename)
   {
      msg << "In file " << report->filename;
      addedFile = true;

      if(report->lineno)
         msg << " @ line " << static_cast<int>(report->lineno);
   }
   if(addedFile)
      msg << ":\n";
   if(JSREPORT_IS_WARNING(report->flags))
      msg << "Warning: ";
   else
      msg << "Error: ";
   
   msg << message;

   if(report->linebuf)
      msg << "\nContext: " << report->linebuf;

   AeonEngine::LogPuts(msg.constPtr());
}

//
// AeonJS_ContextCallback
//
// Callback function to be invoked whenever a new context is created by the
// jsapi. Sets the error reporting function and ensures that the engine is
// configured to use the latest supported ECMA spec.
//
static JSBool AeonJS_ContextCallback(JSContext *cx, uintN contextOp)
{
   if(contextOp == JSCONTEXT_NEW)
   {
      JS_SetErrorReporter(cx, AeonJS_ErrorReporter);
      JS_SetVersion(cx, JSVERSION_LATEST);
   }

   return JS_TRUE;
}

//
// AeonJS::InitEngine
//
// Initialize core components of the jsapi
//
bool AeonJS::InitEngine()
{
   // Create the JSRuntime
   if(!(gRuntime = JS_NewRuntime(AEON_JS_RUNTIME_HEAP_SIZE)))
      return false;

   // Set context callback
   JS_SetContextCallback(gRuntime, AeonJS_ContextCallback);
   
   // Create the default global execution context
   gEvalContext = new EvalContext();

   // Create the JavaScript global object and initialize it
   if(!gEvalContext->setGlobal(&global_class))
      return false;

   // Create Aeon object
   if(!AeonJS_CreateAeonObject(gEvalContext))
      return false;

   return true;
}

//
// AeonJS::ShutDown
//
// Destroy and shutdown the js32 library.
//
void AeonJS::ShutDown()
{
   if(gEvalContext)
   {
      delete gEvalContext;
      gEvalContext = NULL;
   }

   if(gRuntime)
   {
      JS_DestroyRuntime(gRuntime);
      gRuntime = NULL;
      JS_ShutDown();
   }
}

//
// AeonJS::EvaluateString
//
// Evaluate a string as a one-shot script.
//
bool AeonJS::EvaluateString(const char *name, const char *script, 
                            EvalContext *ctx)
{
   jsval  rval;
   JSBool result;

   if(!ctx)
      ctx = gEvalContext;

   result = JS_EvaluateScript(ctx->getContext(), ctx->getGlobal(), script, 
                              strlen(script), name, 0, &rval);
   
   return (result == JS_TRUE);
}

//
// AeonJS::EvaluateStringLogResult
//
// Evaluate a string as a one-shot script, and echo the result to the Aeon log.
//
bool AeonJS::EvaluateStringLogResult(const char *name, const char *script, 
                                     EvalContext *ctx)
{
   JSContext *cx;
   jsval  rval;
   JSBool result;
   AutoNamedRoot root;

   if(!ctx)
      ctx = gEvalContext;

   cx = ctx->getContext();

   result = JS_EvaluateScript(cx, ctx->getGlobal(), 
                              script, strlen(script), name, 0, &rval);

   AeonEngine::LogPuts(AeonJS::SafeGetStringBytes(cx, rval, root));

   return (result == JS_TRUE);
}

//
// AeonJS::EvaluateFile
//
// Evaluate a file as a one-shot script.
//
bool AeonJS::EvaluateFile(const char *filename, EvalContext *ctx)
{
   jsval  rval;
   JSBool result;

   if(!ctx)
      ctx = gEvalContext;

   result = Aeon_JS_evaluateFile(ctx->getContext(), ctx->getGlobal(), 
                                 filename, &rval);

   return (result == JS_TRUE);
}

//
// AeonJS::EvaluateInSandbox
//
// Execute a string as a one-shot script inside a sandbox.
//
bool AeonJS::EvaluateInSandbox(const char *name, const char *script)
{
   EvalContext sandbox;

   if(!sandbox.setGlobal(&sandbox_class))
      return false;

   return EvaluateString(name, script, &sandbox);
}

//
// AeonJS::EvaluateInSandbox
//
// Disk file overload.
//
bool AeonJS::EvaluateInSandbox(const char *filename)
{
   EvalContext sandbox;

   if(!sandbox.setGlobal(&sandbox_class))
      return false;

   return EvaluateFile(filename, &sandbox);
}

//=============================================================================
//
// JavaScript Console Hook
//
// Allows interactive input of JS at the console.
//

// Private implementation
class JSConsoleHookPimpl : public ZoneObject
{
public:
   int     lineno;    // current line number
   int     startline; // starting line number
   qstring buffer;    // input buffer

   JSConsoleHookPimpl()
      : lineno(0), startline(0), buffer()
   {
   }
};

//
// ConsoleHook Constructor
//
AeonJS::ConsoleHook::ConsoleHook()
   : AeonEngine::ConsoleHook()
{
   pImpl = new JSConsoleHookPimpl();
}

//
// ConsoleHook::activateHook
//
// Boot the JS console hook.
//
void AeonJS::ConsoleHook::activateHook()
{
   pImpl->lineno = pImpl->startline = 0;
   pImpl->buffer.clear();

   curJSHook = this;
}

//
// ConsoleHook::exitHook
//
// Leaving the JS console hook.
//
void AeonJS::ConsoleHook::exitHook()
{
   AeonEngine::ConsoleHook::exitHook();
   curJSHook = NULL;
}

//
// ConsoleHook::addInputLine
//
// Receive and digest a line of input from the console. If the buffer has
// accumulated into a compilable unit of code, it will be executed.
//
void AeonJS::ConsoleHook::addInputLine(const qstring &inputLine)
{
   pImpl->buffer << inputLine << '\n';
   ++pImpl->lineno;

   JSContext *cx     = gEvalContext->getContext();
   JSObject  *global = gEvalContext->getGlobal();

   if(JS_BufferIsCompilableUnit(cx, global, pImpl->buffer.constPtr(), 
                                pImpl->buffer.length()))
   {
      CompiledScript *cs;

      // Clear any exception
      JS_ClearPendingException(gEvalContext->getContext());

      if((cs = CompiledScript::CompileString("console", pImpl->buffer.constPtr())))
      {
         qstring result;
         if(cs->executeWithResult(result))
         {
            if(result != "undefined")
               C_Printf(FC_BRICK "%s", result.constPtr());
         }
         delete cs;
      }

      // Clear any exception
      JS_ClearPendingException(gEvalContext->getContext());

      pImpl->lineno = pImpl->startline = 0;
      pImpl->buffer.clear();
   }
}

//
// ConsoleHook::getInputPrompt
//
// Return a string to the console to use as the console command prompt in place
// of the usual prompt, while the hook is active.
//
void AeonJS::ConsoleHook::getInputPrompt(qstring &prompt)
{
   prompt = (pImpl->lineno == pImpl->startline) ? FC_HI "js> " FC_NORMAL : "";
}

#endif // EE_FEATURE_AEONJS

// EOF

