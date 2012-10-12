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

class qstring;
class JSCompiledScriptPimpl;

namespace AeonJS
{
   bool InitEngine();
   void ShutDown();

   bool EvaluateString(const char *name, const char *script);
   bool EvaluateStringLogResult(const char *name, const char *script);
   bool EvaluateFile(const char *filename);

   /*
   bool EvaluateInContext(EvalContext &ctx, const char *name, const char *script);
   bool EvaluateInContext(EvalContext &ctx, const char *filename);
   */

   // JS Compiled Script
   class CompiledScript : public ZoneObject
   {
   private:
      JSCompiledScriptPimpl *pImpl;

   public:
      CompiledScript();
      ~CompiledScript();
      bool execute();
      bool executeWithResult(qstring &qstr);
      bool executeWithResult(int &i);
      bool executeWithResult(unsigned int &ui);
      bool executeWithResult(double &d);
      bool executeWithResult(bool &b);

      static CompiledScript *CompileString(const char *name, const char *script);
      static CompiledScript *CompileFile(const char *filename);
   };
}

#endif

#endif

// EOF

