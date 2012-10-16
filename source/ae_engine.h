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

class qstring;

//
// AeonEngine Namespace
//
// Utilities for Aeon scripting engines
//
namespace AeonEngine
{
   void LogPrintf(const char *message, ...);
   void LogPuts(const char *message);

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

   int LogOutputMode();

   void InitEngines(); // Initialize all scripting engines

   // ConsoleHook - base class for scripting engine console hooks
   class ConsoleHook
   {
   public:
      ConsoleHook() {}

      virtual void activateHook() {}
      virtual void addInputLine(const qstring &inputLine) {}
      virtual void getInputPrompt(qstring &prompt)        {}
      virtual void exitHook(); // Call your parent!
   };
}

#endif

// EOF

