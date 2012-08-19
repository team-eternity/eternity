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

#ifndef AE_JSENGINE_H__
#define AE_JSENGINE_H__

#ifdef EE_FEATURE_AEONJS

#include "ae_engine.h"

class JSCompiledScriptPimpl;
class JSValuePimpl;

class AeonJSEngine : public AeonEngine
{
public:
   ~AeonJSEngine() {}

   bool initEngine();
   void shutDown();

   bool evaluateString(const char *name, const char *script);
   bool evaluateFile(const char *filename);

   class CompiledScript : public AeonEngine::CompiledScript
   {
   private:
      friend class AeonJSEngine;
      JSCompiledScriptPimpl *pImpl;
   public:
      ~CompiledScript();
   };

   CompiledScript *compileString(const char *name, const char *script);
   CompiledScript *compileFile(const char *filename);

   bool executeCompiledScript(CompiledScript *cs);
   bool executeCompiledScriptWithResult(CompiledScript *cs, qstring &qstr);
   bool executeCompiledScriptWithResult(CompiledScript *cs, int &i);
   bool executeCompiledScriptWithResult(CompiledScript *cs, unsigned int &ui);
   bool executeCompiledScriptWithResult(CompiledScript *cs, double &d);
   bool executeCompiledScriptWithResult(CompiledScript *cs, bool &b);

   class Value : public AeonEngine::Value
   {
   private:
      friend class AeonJSEngine;
      JSValuePimpl *pImpl;
   public:
      ~Value();
   };
};

#endif

#endif

// EOF

