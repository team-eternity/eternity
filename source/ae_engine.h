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
// Aeon Engine - Core Interface Module
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef AE_ENGINE_H__
#define AE_ENGINE_H__

#include "z_zone.h"

class qstring;

class AeonEngine
{
protected:
   static int outputMode;
   static FILE *outStream;

public:
   virtual ~AeonEngine() {}

   virtual bool initEngine() = 0;
   virtual void shutDown() = 0;

   virtual bool evaluateString(const char *name, const char *script) = 0;
   virtual bool evaluateFile(const char *filename) = 0;

   class CompiledScript : public ZoneObject
   {
   public:
      virtual ~CompiledScript() {}
      virtual bool execute() = 0;
      virtual bool executeWithResult(qstring &qstr) = 0;
      virtual bool executeWithResult(int &i) = 0;
      virtual bool executeWithResult(unsigned int &ui) = 0;
      virtual bool executeWithResult(double &d) = 0;
      virtual bool executeWithResult(bool &b) = 0;
   };

   virtual CompiledScript *compileString(const char *name, const char *script) = 0;
   virtual CompiledScript *compileFile(const char *filename) = 0;

   static void InitOutput();
   static void LogPrintf(const char *message, ...);
   static void LogPuts(const char *message);

   // Error/Warning output modes
   enum
   {
      OUTPUT_CONSOLE, // use game console
      OUTPUT_STDOUT,  // use stdout stream
      OUTPUT_STDERR,  // use stderr stream
      OUTPUT_FILE,    // use a log file
      OUTPUT_SILENT,  // no output
      OUTPUT_NUMMODES
   };
};

#endif

// EOF

