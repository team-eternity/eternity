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
#include "e_hash.h"
#include "e_lib.h"
#include "m_argv.h"
#include "m_qstr.h"
#include "m_qstrkeys.h"

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
   static bool firsttime = true;

   if(!firsttime) // one time only.
      return;

   firsttime = false;

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

   // make sure log system is initialized
   InitOutput();
   
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

   // make sure log system is initialized
   InitOutput();

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

//=============================================================================
//
// Evaluation Contexts
//
// The particulars of this class are implemented in each Aeon engine, but 
// certain base functionalities such as lookup of contexts by name (with or 
// without qualification by the parent AeonEngine object) are implemented here.
//

IMPLEMENT_RTTI_TYPE(AeonEngine::EvalContext)

//
// Private Implementation Class
//
class Aeon::EvalContextPimpl : public ZoneObject
{
public:
   // Instance vars
   AeonEngine::EvalContext *parent;
   qstring name;
   DLListItem<EvalContextPimpl> links;

   EvalContextPimpl(AeonEngine::EvalContext *pParent) 
      : parent(pParent), name(), links() 
   {
   }
};

// Global hash table of evaluation context objects
static EHashTable<Aeon::EvalContextPimpl, EQStrHashKey, 
                  &Aeon::EvalContextPimpl::name, &Aeon::EvalContextPimpl::links>
                  GlobalContextHash(131);

//
// Default Constructor
//
AeonEngine::EvalContext::EvalContext()
   : Super(), engine(NULL)
{
   pImpl = new PimplType(this);
}

//
// Initializing Constructor
//
AeonEngine::EvalContext::EvalContext(const char *pName, AeonEngine *pEngine)
   : Super(), engine(pEngine)
{
   pImpl = new PimplType(this);
   pImpl->name = pName;
}

//
// AeonEngine::EvalContext::Find
//
// Find an already existent evaluation context.
//
AeonEngine::EvalContext *
AeonEngine::EvalContext::Find(const char *name, AeonEngine *e)
{
   EvalContext *ret  = NULL;
   PimplType   *eval = NULL;

   while((eval = GlobalContextHash.keyIterator(eval, name)))
   {
      if(!e || e == eval->parent->engine)
      {
         ret = eval->parent;
         break;
      }
   }

   return ret;
}

//
// AeonEngine::EvalContext::Define
//
// Define a new evaluation context.
//
AeonEngine::EvalContext *
AeonEngine::EvalContext::Define(const char *name, AeonEngine *e)
{
   EvalContext *newCtx = NULL;

   if(!(newCtx = Find(name, e)))
   {
      newCtx = e->evalContextRTTIType()->newObject();
      newCtx->pImpl->name = name;
      newCtx->engine = e;

      GlobalContextHash.addObject(newCtx->pImpl);
   }

   return newCtx;
}

// EOF

