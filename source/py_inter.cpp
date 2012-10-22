// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 Kate Stone
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//
//   Controller class for the Python interpreter; maintains various context
//   objects, runs scripts, and executes and evaluates code.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>

#define NEED_EDF_DEFINITIONS

#include "c_io.h"
#include "c_runcmd.h"
#include "py_inter.h"
#include "py_module_base.h"
#include "py_module_level.h"
#include "w_wad.h"

// Helper functions
wchar_t* charToWChar (char* string)
{
   size_t memorySize = mbstowcs (NULL, string, 0);

   wchar_t* newstr = emalloc (wchar_t*, (memorySize + 1) * sizeof (wchar_t));
   // What do I do if this fails?
   size_t size = mbstowcs (newstr, string, memorySize + 1);

   return newstr;
}

// AeonInterpreter

PyObject* AeonInterpreter::Builtins;

PyObject* AeonInterpreter::Globals;
PyObject* AeonInterpreter::ConsoleGlobals;

EAeonScriptMap AeonInterpreter::ScriptMap;

const char* builtin_mods[] =
{
   // Aeon built-ins
   "eternity",
   "level",

   // Python built-ins
   "itertools",
   "math",
   "operator",
   "time",

   NULL
};

void AeonInterpreter::Initialize (const char* basepath)
{
   // Initialize our hash tables.
   ModuleCache.initialize (389);
   ScriptMap.initialize (389);

   // Point to *our* standard library
   qstring libpath = qstring ();
   libpath.Printf (0, "%s/stdlib.zip", basepath);
   libpath.normalizeSlashes ();

   wchar_t* wbasepath = charToWChar (libpath.getBuffer ());
   Py_SetPath (const_cast<const wchar_t*> (wbasepath));
   efree (wbasepath);

   //wchar_t* CommandLineOptions[3] = { L"-E", L"-S", L"-B" };
   //PySys_SetArgvEx(3, CommandLineOptions, 0);

   // Built-in modules need to be added to the initialization tab.
   PyImport_AppendInittab("eternity",  init_AeonMod_base);
   PyImport_AppendInittab("level",     init_AeonMod_level);

   // It is a fatal error if this fails, but it shouldn't as long as there is
   // a Python33.dll in the current directory, or in the system.
   Py_Initialize ();

   // Print the exact version so we know what we're initializing for debugging
   // later if required.
   printf ("Python %s\n", Py_GetVersion ());

   // Initialize our built-in modules
   PyObject* internalModule;
   
   for (int i = 0; builtin_mods[i] != NULL; i++)
   {
      internalModule = PyImport_ImportModuleLevel (const_cast<char*>(builtin_mods[i]), Globals, Globals, Py_None, 0);

      if (!internalModule)
      {
         PyErr_Print ();
         I_Error ("Python error encountered! See above for more details.");
      }

      ModuleCache.addObject (new AeonCachedModule (builtin_mods[i], internalModule));
   }

   internalModule = ModuleCache.objectForKey ("eternity")->module;

   // Built-in functions
   PyObject* internalBuiltins = PyEval_GetBuiltins ();

   // Do not allow scripts access to files
   PyDict_DelItemString (internalBuiltins, "open");

   // Use our import mechanism
   PyDict_SetItemString (internalBuiltins, "__import__",
      PyObject_GetAttrString (internalModule, "import")
   );

   // Initialize globals
   Globals = PyDict_New ();
   ConsoleGlobals = PyDict_New ();

   PyDict_SetItemString (Globals, "__builtins__", internalBuiltins);
   PyDict_SetItemString (ConsoleGlobals, "__builtins__", internalBuiltins);

   // Sys: Use our custom stdout object
   PySys_SetObject ("stdout", (PyObject *) new TextPrinter (FC_NORMAL));
   PySys_SetObject ("stderr", (PyObject *) new TextPrinter (FC_ERROR));
}

Collection<HashData> AeonInterpreter::SeenInits;

bool AeonInterpreter::CheckMarkedScript (int lumpnum)
{
   int scriptLength = wGlobalDir.lumpLength(lumpnum);
   char* scriptString = emalloc (char*, (scriptLength + 1) * sizeof (char));
   qstring script;

   wGlobalDir.readLump (lumpnum, scriptString);
   script.copy (scriptString);
   efree (scriptString);

   HashData scriptsha = HashData(HashData::SHA1, (const uint8_t *) script.getBuffer (), (uint32_t) script.length ());
   size_t totalseen = SeenInits.getLength();

   for(size_t n = 0; n < totalseen; n++)
   {
      if(scriptsha == SeenInits[n])
         return true;
   }

   SeenInits.add(scriptsha);
   return false;
}

EAeonModuleCache AeonInterpreter::ModuleCache;

PyObject* AeonInterpreter::LumpImport (const char* name)
{
   PyObject* module;
   int lumpnum;
   int scriptLength;
   char* script;
   qstring formattedScript;
   PyObject* compiledScript;

   AeonCachedModule *entry;
   if ((entry = ModuleCache.objectForKey (name)) != NULL)
   {
      return entry->module;
   }

   AeonScriptMapping *mapping;
   if ((mapping = ScriptMap.objectForKey (name)) == NULL)
   {
      qstring errmsg = qstring ();
      errmsg.Printf (0, "No module named %s", name);
      PyErr_SetString (PyExc_ImportError, errmsg.getBuffer ());
      return NULL;
   }

   lumpnum = wGlobalDir.getNumForName (mapping->lumpname);
   scriptLength = wGlobalDir.lumpLength(lumpnum);
   script = emalloc (char*, (scriptLength + 1) * sizeof (char));

   wGlobalDir.readLump (lumpnum, script);
   formattedScript.copy (script);
   efree (script);

   if (!formattedScript.length ())
   {
      // Just return an empty module
      module = PyImport_AddModule (name);
      ModuleCache.addObject (new AeonCachedModule (name, module));
      return module;
   }

   // Normalize line endings
   formattedScript.replace ("\r\n", '\n');
   formattedScript.replace ("\r", '\n');

   compiledScript = Py_CompileString (formattedScript.getBuffer (), name, Py_file_input);

   // Errors are already set by Py_CompileString and PyImport_ExecCodeModule.
   if (!compiledScript) return NULL;

   // For some reason, this function takes a char* even though it does not
   // write to it, so I assure you this is safe.
   module = PyImport_ExecCodeModule(const_cast<char*> (name), compiledScript);
   Py_DECREF (compiledScript);

   if (!module) return NULL;

   // Cache it like Python does, so we don't process it multiple times.
   ModuleCache.addObject (AeonCachedModule (name, module));
   return module;
}

// Runs a script in the top-level namespace. Returns true if the script
// executed and exited without an error, false otherwise.

bool AeonInterpreter::RunScriptLump (int lumpnum, const char* name)
{
   int scriptLength = wGlobalDir.lumpLength(lumpnum);
   char* script = emalloc (char*, (scriptLength + 1) * sizeof (char));
   qstring formattedScript = qstring ();

   wGlobalDir.readLump (lumpnum, script);
   formattedScript.copy (script);
   efree (script);

   if (!formattedScript.length ())
      // Script is empty so executing it is pointless.
      return false;

   // Normalize line endings
   formattedScript.replace ("\r\n", '\n');
   formattedScript.replace ("\r", '\n');

   PyObject* compiledScript = Py_CompileString (formattedScript.getBuffer (), name, Py_file_input);

   if (!compiledScript)
   {
      PyErr_Print ();
      PyErr_Clear ();
      return false;
   }

   PyObject* result = PyEval_EvalCode (compiledScript, Globals, Globals);
   Py_DECREF (compiledScript);

   if (!result)
   {
      PyErr_Print ();
      PyErr_Clear ();
      return false;
   }

   Py_DECREF (result);
   return true;
}

// Runs AEONINIT scripts to set up the Python interpreter environment.

void AeonInterpreter::RunInitScripts ()
{
   if(W_CheckNumForName("AEONINIT") == -1)
   {
      printf ("No initialization scripts.\n");
      return;
   }

   int count = 0;
   lumpinfo_t* root = W_GetLumpNameChain("AEONINIT");
   lumpinfo_t** lumpinfo = wGlobalDir.getLumpInfo();
   int i = root->index;

   do
   {
      if(!strncasecmp(lumpinfo[i]->name, "AEONINIT", 8) && 
         lumpinfo[i]->li_namespace == lumpinfo_t::ns_global)
      {
         // ID it first and return if we already saw it
         if (CheckMarkedScript (i))
            continue;
         if (RunScriptLump (i, "AEONINIT"))
            count++;
      }
      i = lumpinfo[i]->next;
   }
   while (i > -1);

   printf ("%d startup script%s processed.\n", count, (count == 1)? "" : "s");
}

// EDF parsing ----------------------------------------------------------------

// multi-value property options for scriptmodule

cfg_opt_t edf_script_opts[] =
{
   CFG_STR(ITEM_AEON_MOD_LUMP, "", CFGF_NONE),
   CFG_STR(ITEM_AEON_MOD_NAME, "", CFGF_NONE),
   CFG_END()
};

// Maps script lumps to names for use with the import statement.

void AeonInterpreter::ProcessScriptMappings (cfg_t *cfg)
{
   unsigned int i;
   unsigned int numScripts = cfg_size(cfg, ITEM_AEON_MODS);

   E_EDFLogPuts ("\t* Processing script names\n");

   for(i = 0; i < numScripts; ++i)
   {
      cfg_t *props = cfg_getnmvprop(cfg, ITEM_AEON_MODS, i);
      AeonScriptMapping* map = new AeonScriptMapping
         (cfg_getstr(props, ITEM_AEON_MOD_NAME),
          cfg_getstr(props, ITEM_AEON_MOD_LUMP));

      // scriptmodule "test" = lump is 'test', module is 'test'.
      if (!strlen (map->modulename))
         map->modulename = map->lumpname;

      if (!strlen (map->lumpname))
         E_EDFLoggedErr (2, "Error: \"scriptmodule\" with no lump name given.");

      if (W_CheckNumForName (map->lumpname) == -1)
      {
         E_EDFLoggedErr (2, "Error: Lump \"%s\" for module \"%s\" not found!", map->lumpname, map->modulename);
      }
      E_EDFLogPrintf ("\t\tAdding module \"%s\"\n", map->modulename);

      ScriptMap.addObject (map);
   }

   E_EDFLogPrintf ("\t\t%d script%s.\n", numScripts, (numScripts == 1)? "" : "s");
}

// Console support ------------------------------------------------------------

CONSOLE_COMMAND (py, cf_notnet)
{
   if (Console.argc != 1)
   {
      C_Puts ("Usage: py \"<code>\"");
      return;
   }

   PyObject* compiledScript = Py_CompileString (Console.argv[0]->constPtr (), "<console>", Py_single_input);

   if (!compiledScript)
   {
      PyErr_Print ();
      PyErr_Clear ();
      return;
   }

   PyObject* result = PyEval_EvalCode (compiledScript, AeonInterpreter::ConsoleGlobals, AeonInterpreter::ConsoleGlobals);
   Py_DECREF (compiledScript);

   if (!result)
   {
      PyErr_Print ();
      PyErr_Clear ();
      return;
   }

   Py_DECREF (result);
}

CONSOLE_COMMAND (pyfile, cf_notnet)
{
   if (Console.argc != 1)
   {
      C_Puts ("Usage: pyfile <file>");
      return;
   }

   FILE* fileptr = fopen (Console.argv[0]->constPtr (), "r+b");

   if (fileptr == NULL)
   {
      C_Printf ("Unable to open file \"%s\"", Console.argv[0]->constPtr ());
      return;
   }

   fseek (fileptr, 0, SEEK_END);
   size_t filesize = ftell (fileptr);
   rewind (fileptr);

   char* buffer = emalloc (char*, (sizeof (char) * filesize) + 1);

   if (buffer == NULL)
   {
      C_Printf ("Not enough memory to load file");
      return;
   }

   int fileread = fread (buffer, 1, filesize, fileptr);

   if (fileread != filesize)
   {
      C_Printf ("Error reading file");
      return;
   }

   buffer[filesize] = '\0';

   qstring* formattedScript = new qstring (const_cast<const char*>(buffer));

   efree (buffer);

   // Normalize line endings
   formattedScript->replace ("\r", ' ');

   PyObject* compiledScript = Py_CompileString (formattedScript->constPtr (), Console.argv[0]->constPtr (), Py_file_input);

   delete formattedScript;

   if (!compiledScript)
   {
      PyErr_Print ();
      PyErr_Clear ();
      return;
   }

   PyObject* result = PyEval_EvalCode (compiledScript,
      AeonInterpreter::ConsoleGlobals,
      AeonInterpreter::ConsoleGlobals);
   Py_DECREF (compiledScript);

   if (!result)
   {
      PyErr_Print ();
      PyErr_Clear ();
      return;
   }

   Py_DECREF (result);
}

void Py_AddCommands ()
{
   C_AddCommand(py);
   C_AddCommand(pyfile);
}