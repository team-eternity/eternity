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

#include "z_zone.h"

#include "ae_engine.h"
#include "c_io.h"
#include "doomstat.h"
#include "e_lib.h"
#include "m_argv.h"
#include "m_qstr.h"

//============================================================================
//
// Warning / Error Logging
//

int   AeonEngine::outputMode = AeonEngine::OUTPUT_CONSOLE;
FILE *AeonEngine::outStream;

static const char *outputModeNames[] =
{
   "console",
   "stdout",
   "stderr",
   "file",
   "silent"
};

//
// AeonEngine::InitOutput
//
// Sets up the logging system for scripting warnings and errors.
//
void AeonEngine::InitOutput()
{
   // check for command line argument
   int p;
   qstring filename;

   if((p = M_CheckParm("-aeonout")) && p < myargc - 1)
   {
      outputMode = E_StrToNumLinear(outputModeNames, OUTPUT_NUMMODES, 
                                    myargv[p + 1]);
      if(outputMode == OUTPUT_NUMMODES)
         outputMode = OUTPUT_CONSOLE;
   }

   switch(outputMode)
   {
   case OUTPUT_CONSOLE:
   case OUTPUT_SILENT:
      // nothing special is required for these
      break;
   case OUTPUT_STDOUT:
      outStream = stdout;
      break;
   case OUTPUT_STDERR:
      outStream = stderr;
      break;
   case OUTPUT_FILE:
      filename = userpath;
      filename.pathConcatenate("aeonlog.txt");
      if(!(outStream = fopen(filename.constPtr(), "a")))
         outStream = stderr; // fall back to stderr if open fails
      break;
   }
}

//
// AeonEngine::LogPrintf
//
// Formatted printing to the Aeon log
//
void AeonEngine::LogPrintf(const char *message, ...)
{
   va_list va;

   va_start(va, message);
   
   switch(outputMode)
   {
   case OUTPUT_STDOUT:
   case OUTPUT_STDERR:
   case OUTPUT_FILE:
      vfprintf(outStream, message, va);
      break;
   case OUTPUT_CONSOLE:
      C_VPrintf(message, va);
      break;
   default:
      break;
   }

   va_end(va);
}

//
// AeonEngine::LogPuts
//
// Put a simple message into the Aeon log.
// When writing to a FILE stream, a line end will be appended if the message
// does not already end with one.
//
void AeonEngine::LogPuts(const char *message)
{
   size_t len = 0;

   switch(outputMode)
   {
   case OUTPUT_STDOUT:
   case OUTPUT_STDERR:
   case OUTPUT_FILE:
      fputs(message, outStream);
      len = strlen(message);
      if(len > 0 && message[len - 1] != '\n')
         fputc('\n', outStream);
      break;
   case OUTPUT_CONSOLE:
      C_Puts(message);
      break;
   default:
      break;
   }
}

// EOF

