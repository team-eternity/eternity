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
//   Base "eternity" module, houses internal functions and the built-in
//   submodules.
//
//-----------------------------------------------------------------------------

#include "c_io.h"
#include "doomstat.h"
#include "py_inter.h"
#include "py_module_base.h"
#include "v_misc.h"

typedef PODCollection<qstring> QStringCollection;

QStringCollection SplitDotNames (qstring &name)
{
   qstring* path = new qstring (name);
   QStringCollection pathList;

   size_t pos = name.findFirstOf ('.');

   while (pos != qstring::npos)
   {
      qstring* left = new qstring ();
      left->copy (*path);
      left->truncate (pos);
      pathList.add (*left);

      pos = name.findFirstOf ('.', pos + 1);
   }
   pathList.add (*path);

   return pathList;
}

// This is used by TextPrinter for buffering. Returns a two-item list
// (left, right), or NULL if middle is not present in str.

QStringCollection* Partition (qstring& str, char middle)
{
   QStringCollection *pair = NULL;
   qstring *left, *right;

   size_t pos = str.findFirstOf (middle);

   if (pos != qstring::npos)
   {
      pos++; // Stick the middle on the left side of the string.

      pair = new QStringCollection (2);

      if (pos < (str.length () - 1))
      {
         left = new qstring ();
         left->copy (str);
         left->truncate (pos);
         pair->add (*left);

         right = new qstring (str.getBuffer () + pos);
         pair->add (*right);
      }
      else
      {
         pair->add (str);

         qstring *dummy = new qstring ();
         pair->add (*dummy);
      }
   }

   return pair;
}


// CLASSES ====================================================================

// TextPrinter

PyTypeObject TextPrinter::Type = {
   PyObject_HEAD_INIT(NULL)
   "TextPrinter",         // name
   sizeof(TextPrinter),   // basic size
   0,                     // item size

   // methods //
   PyDestructor,          // del
   NULL,                  // print
   NULL,                  // getattr
   NULL,                  // setattr
   NULL,                  // compare
   NULL,                  // repr
   NULL,                  // as number
   NULL,                  // as sequence
   NULL,                  // as mapping
   NULL,                  // hash
   NULL,                  // call
   NULL,                  // str
   NULL,                  // getattr (pyobject*)
   NULL,                  // setattr (pyobject*)
   NULL,                  // as buffer

   // misc // 
                        // flags
   Py_TPFLAGS_DEFAULT |
   Py_TPFLAGS_BASETYPE,
                        // doc
   "Helper class for writing to the console. This one writes for\n\
   stdout.",
   NULL,                  // traverse
   NULL,                  // clear
   NULL,                  // rich compare
   NULL,                  // weak list offset
   NULL,                  // iter
   NULL,                  // next

   // attributes //
   TextPrinter::Methods,        // methods
   NULL,                        // members
   TextPrinter::GetSetMethods,  // getset
};

PyMethodDef TextPrinter::Methods[] = {
   {"write", (PyCFunction) TextPrinter::write, METH_VARARGS,
    "Prints to the console."},
   {NULL, NULL, 0, NULL} // Sentinel
};

PyGetSetDef TextPrinter::GetSetMethods[] = { {NULL} };

PyObject* TextPrinter::write (PyObject *pyself, PyObject *args)
{
   TextPrinter* self = (TextPrinter*) pyself;
   const char* str = NULL;

   if(!PyArg_ParseTuple (args, "s", &str))
   {
      return NULL;
   }

   self->buffer->concat (str);

   QStringCollection* cx = Partition (*self->buffer, '\n');

   if (cx)
   {
      qstring l = cx->at (0);

      if (in_textmode)
         printf ("%s", l.constPtr ());
      else
         C_Printf ("%s%s", self->color, l.constPtr ());

      self->buffer->copy (cx->at (1));

      delete cx;
   }


   Py_RETURN_NONE;
}

// MODULE FUNCTIONS ===========================================================

static PyObject* Import (PyObject* module, PyObject* args, PyObject* kwargs)
{
   // We ignore globals, locals, and level for now since those aren't relevant
   // to us.
   const char* name;
   PyObject* globals;
   PyObject* locals;
   PyObject* fromlist;
   int level;

   if (!PyArg_ParseTuple (args, "sOOOi", &name, &globals, &locals, &fromlist, &level)) return NULL;

   PyObject* base = NULL;
   PyObject* sub = NULL;

   QStringCollection modules = SplitDotNames (qstring (name));

   qstring moduleName;
   PyObject* currentModule;

   for (size_t i = 0; i < modules.getLength (); i++)
   {
      moduleName = modules[i];

      currentModule = AeonInterpreter::LumpImport (moduleName.constPtr ());

      if (base == NULL)
      {
         // Remember the base module
         base = currentModule;
      }
      else if (sub == NULL)
      {
         PyObject_SetAttrString (base, moduleName.constPtr (), currentModule);
         sub = currentModule;
      }
      else
      {
         PyObject_SetAttrString (sub, moduleName.constPtr (), currentModule);
         sub = currentModule;
      }
   }

   if (fromlist == Py_None)
      return base;
   else
   {
      if (sub)
         return sub;
      else
         return base;
   }
}

// MODULE DEFINITION ==========================================================

static struct PyMethodDef AeonMod_Base_Methods[] = {
   {"import", (PyCFunction) Import, METH_VARARGS,
    "Override for the default import mechanism."},
   {NULL, NULL, 0, NULL} // Sentinel
};

static struct PyModuleDef AeonMod_Base_ModuleDef = {
   PyModuleDef_HEAD_INIT,
   "eternity",
   "The base package for built-in Aeon modules.",
   0,
   AeonMod_Base_Methods,
   NULL,
   NULL,
   NULL,
   NULL
};

PyObject* init_AeonMod_base ()
{
   PyObject *module = PyModule_Create(&AeonMod_Base_ModuleDef);

   if (!module)
      return NULL;

   if (PyType_Ready(&TextPrinter::Type) < 0)
      return NULL;

   Py_INCREF(&TextPrinter::Type);
   PyModule_AddObject(module, "TextPrinter", (PyObject *) &TextPrinter::Type);

   PyModule_AddObject(module, "__path__", PyList_New (0));

   return module;
}

// EOF
