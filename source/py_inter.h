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

#ifndef __PY_INTER_H__
#define __PY_INTER_H__

#define PY_SSIZE_T_CLEAN
#include "Python.h"

#include "Confuse/confuse.h"
#include "e_edf.h"
#include "e_hash.h"
#include "metaapi.h"
#include "m_collection.h"
#include "m_hash.h"
#include "m_qstr.h"

// Base definitions for all Python-related objects

// This is used for PyObject-inherited classes.
#define PyPP_Header \
public: \
    static PyTypeObject   Type; \
    static PyMethodDef    Methods[]; \
    static PyGetSetDef    GetSetMethods[]; \
    virtual PyTypeObject *GetType(void) {return &Type;};\
    void PyINCREF(void) {Py_INCREF(this);};\
    void PyDECREF(void) {Py_DECREF(this);};

// MetaTable objects for some common Python types.

// TODO: Constants (None, Ellipsis)

// Boolean objects (True and False)
class MetaBoolean : public MetaObject
{
protected:
   bool value;

public:
   MetaBoolean(const char *key, bool t)
      : MetaObject(key), value(t) {};
   MetaBoolean(const MetaBoolean &other)
      : MetaObject(other)
   {
      this->value = other.value;
   };

   // Virtual Methods
   virtual MetaObject *clone() const { return new MetaBoolean (*this); };
   virtual const char *toString() const
   {
      if (value)
         return "True";
      else
         return "False";
   };

   // Accessors
   bool getValue() const { return value; };
   void setValue (bool t) { value = t; };

   friend class MetaTable;
};

// The class for the Python interpreter itself. All methods and functions are
// static, so don't worry about creating any instances, there will never be
// any.

#ifdef NEED_EDF_DEFINITIONS
#define ITEM_AEON_MODS "scriptmodule"
#define ITEM_AEON_MOD_LUMP "lump"
#define ITEM_AEON_MOD_NAME "modulename"

extern cfg_opt_t edf_script_opts[];

void E_ProcessScriptNames (cfg_t*);
#endif

class AeonScriptMapping : public ZoneObject
{
public:
   AeonScriptMapping (const char* modulename, const char* lumpname)
      : modulename (modulename), lumpname (lumpname)
   {}

   DLListItem<AeonScriptMapping> links;

   const char* modulename;
   const char* lumpname;
};

class AeonCachedModule : public ZoneObject
{
public:
   AeonCachedModule (const char* modname, PyObject* module)
      : modname (modname), module (module)
   {}

   DLListItem<AeonCachedModule> links;

   const char* modname;
   PyObject* module;
};

typedef EHashTable<AeonScriptMapping, EStringHashKey, &AeonScriptMapping::modulename, &AeonScriptMapping::links> EAeonScriptMap;
typedef EHashTable<AeonCachedModule,  EStringHashKey, &AeonCachedModule::modname,     &AeonCachedModule::links>  EAeonModuleCache;

class AeonInterpreter
{
   static PyObject* Builtins;        // Module

   static PyObject* Globals;         // Dict
   static PyObject* ConsoleGlobals;  // Dict

   static EAeonScriptMap ScriptMap;
   static Collection<HashData> SeenInits;

   static EAeonModuleCache ModuleCache;

   friend void Handler_py (void);
   friend void Handler_pyfile (void);
   friend PyObject* init_AeonMod_base (void);

public:
   // Initialization
   static void Initialize (const char*);
   static void ProcessScriptMappings (cfg_t *cfg);

   // Scripts
   static bool RunScriptLump (int, const char*);
   static void RunInitScripts ();
   static bool CheckMarkedScript (int);

   static PyObject* LumpImport (const char*);
};

#endif // __PY_INTER_H__
