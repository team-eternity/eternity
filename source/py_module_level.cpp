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
//   Module for level interaction.
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "e_things.h"
#include "py_inter.h"
#include "py_module_level.h"
#include "p_mobj.h"
#include "p_saveg.h"

#include "marshal.h" // Python marshal module

// CLASSES ====================================================================

//-------------------------------------
//
// MapObject
//
//-------------------------------------

// C methods

MobjProxy::MobjProxy (Mobj* owner)
   : owner (owner), attributedict (NULL)
{
   attributedict = PyDict_New ();
   PyObject_Init ((PyObject*) this, &MobjProxy::Type);
}

MobjProxy::~MobjProxy ()
{
   Py_DECREF (attributedict);
}

void MobjProxy::serialize (SaveArchive &arc)
{
   if (arc.isSaving ())
   {
      PyObject *Bytes = PyMarshal_WriteObjectToString (attributedict, Py_MARSHAL_VERSION);
      Py_ssize_t size = PyBytes_GET_SIZE (Bytes);
      char *string;

      if (PyBytes_AsStringAndSize (Bytes, &string, &size) == -1)
         I_Error ("Save error attempting to serialize Python properties!");

      size_t psize = size;
      arc.ArchiveLString (string, psize);

      Py_DECREF (Bytes);
   }
   else
   {
      char *outstring;
      size_t outlen;
      PyObject *newdict;

      arc.ArchiveLString (outstring, outlen);

      newdict = PyMarshal_ReadObjectFromString (outstring, (Py_ssize_t) outlen);
      if (!newdict)
         I_Error ("Load error attempting to deserialize Python properties!");

      Py_DECREF (attributedict);
      efree (outstring);

      attributedict = newdict;
   }
}

// Python methods

void MobjProxy::PyDestructor (PyObject *obj)
{
   delete (MobjProxy *) obj;
}

int MobjProxy::SetAttribute (PyObject *obj, char *attr_name, PyObject *value)
{
   MobjProxy *self = (MobjProxy *) obj;

   long int nvalue;
   double dvalue;

   if (value == NULL) return DelAttribute (self, attr_name);

   if (!self->owner)
   {
      PyErr_SetString (PyExc_ReferenceError, "Map object no longer exists");
      return NULL;
   }

   if (!strcasecmp (attr_name, "classname"))
   {
      PyErr_SetString (PyExc_ValueError, "Attribute is read-only");
      return -1;
   }

   else if (!strcasecmp (attr_name, "angle"))
   {
      dvalue = PyFloat_AsDouble (value);

      if (dvalue > 360.0 || dvalue < 0.0)
      {
         PyErr_SetString (PyExc_ValueError, "Value is out of bounds");
         return -1;
      }
      self->owner->angle = FixedToAngle (M_DoubleToFixed (dvalue));
      return 0;
   }

   else if (!strcasecmp (attr_name, "health"))
   {
      nvalue = PyLong_AsLong (value);

      if (nvalue > INT_MAX || nvalue < INT_MIN)
      {
         PyErr_SetString (PyExc_ValueError, "Value would truncate");
         return -1;
      }
      self->owner->health = (int) nvalue;
      return 0;
   }

   else
   {
      return PyDict_SetItemString (self->attributedict, attr_name, value);
   }
}

int MobjProxy::DelAttribute (PyObject *obj, char *attr_name)
{
   MobjProxy *self = (MobjProxy *) obj;

   if (!self->owner)
   {
      PyErr_SetString (PyExc_ReferenceError, "Map object no longer exists");
      return NULL;
   }

   PyObject *s = PyUnicode_FromString (attr_name);
   if (PyDict_Contains (self->attributedict, s))
   {
      PyDict_DelItem (self->attributedict, s);
      Py_DECREF (s);
      return 0;
   }
   PyErr_SetObject (PyExc_AttributeError, s);
   Py_DECREF (s);
   return -1;
}

PyObject* MobjProxy::GetAttribute (PyObject *obj, char *attr_name)
{
   MobjProxy *self = (MobjProxy *) obj;

   if (!self->owner)
   {
      PyErr_SetString (PyExc_ReferenceError, "Map object no longer exists");
      return NULL;
   }

   if (!strcasecmp (attr_name, "classname"))
   {
      return Py_BuildValue ("s", self->owner->info->name);
   }
   else if (!strcasecmp (attr_name, "angle"))
   {
      return Py_BuildValue ("d", M_FixedToDouble (AngleToFixed (self->owner->angle)));
   }
   else if (!strcasecmp (attr_name, "health"))
   {
      return Py_BuildValue ("i", self->owner->health);
   }
   else
   {
      PyObject* result = PyDict_GetItemString (self->attributedict, attr_name);
      if (!result)
      {
         PyErr_SetString (PyExc_AttributeError, attr_name);
         return NULL;
      }
      return result;
   }
}

// Python definition

PyTypeObject MobjProxy::Type = {
   PyObject_HEAD_INIT(NULL)
   "MapObject",         // name
   sizeof(MobjProxy),   // basic size
   0,                   // item size

   // methods //
   (destructor) MobjProxy::PyDestructor,  // del
   NULL,                                  // print
   (getattrfunc) MobjProxy::GetAttribute, // getattr
   (setattrfunc) MobjProxy::SetAttribute, // setattr
   NULL,                      // compare
   NULL,                      // repr
   NULL,                      // as number
   NULL,                      // as sequence
   NULL,                      // as mapping
   NULL,                      // hash
   NULL,                      // call
   NULL,                      // str
   NULL,                      // getattr (pyobject*)
   NULL,                      // setattr (pyobject*)
   NULL,                      // as buffer

   // misc // 
                           // flags
   Py_TPFLAGS_DEFAULT |
   Py_TPFLAGS_BASETYPE,
                           // doc
   "TODO: Description.",
   NULL,                   // traverse
   NULL,                   // clear
   NULL,                   // rich compare
   NULL,                   // weak list offset
   NULL,                   // iter
   NULL,                   // next

   // attributes //
   MobjProxy::Methods,  // methods
   NULL,                // members
   NULL,                // getset
};

PyMethodDef MobjProxy::Methods[] = {
   //{"write", (PyCFunction) TextPrinter::write, METH_VARARGS,
   // "Prints to the console."},
   {NULL, NULL, 0, NULL} // Sentinel
};

PyGetSetDef MobjProxy::GetSetMethods[] = { {NULL} };


//-------------------------------------
//
// MapObjectIterator
//
//-------------------------------------

// C methods

void MobjProxyIterator::PyDestructor (PyObject *obj)
{
   MobjProxyIterator *me = (MobjProxyIterator *) obj;
   delete me;
}

// Python methods

PyObject *MobjProxyIterator::PyConstructor (PyTypeObject *T, PyObject *args, PyObject *keywords)
{
   MobjProxyIterator *newobj = new MobjProxyIterator ();
   return newobj;
}

PyObject *MobjProxyIterator::Next (PyObject *obj)
{
   MobjProxyIterator *self = (MobjProxyIterator *) obj;

   self->current = self->current->next; 
   while (self->current != &thinkercap)
   {
      if (self->current->isDescendantOf (RTTIObject::Type::FindType("Mobj")) && !self->current->isRemoved ())
      {
         Mobj *thing = (Mobj *) self->current;
         Py_INCREF (thing->aeonproxy);
         return thing->aeonproxy;
      }

      self->current = self->current->next;
   }

   PyErr_SetNone (PyExc_StopIteration);
   return NULL;
}

PyObject *MobjProxyIterator::ReturnSelf (PyObject *obj)
{
   Py_INCREF (obj);
   return obj;
}

// Python definition

PyTypeObject MobjProxyIterator::Type = {
   PyObject_HEAD_INIT(NULL)
   "MapObjectIterator",       // name
   sizeof(MobjProxyIterator), // basic size
   0,                         // item size

   // methods //
   MobjProxyIterator::PyDestructor,   // del
   NULL,                      // print
   NULL,                      // getattr
   NULL,                      // setattr
   NULL,                      // compare
   NULL,                      // repr
   NULL,                      // as number
   NULL,                      // as sequence
   NULL,                      // as mapping
   NULL,                      // hash
   NULL,                      // call
   NULL,                      // str
   NULL,                      // getattr (pyobject*)
   NULL,                      // setattr (pyobject*)
   NULL,                      // as buffer

   // misc // 
                           // flags
   Py_TPFLAGS_DEFAULT |
   Py_TPFLAGS_BASETYPE,
                           // doc
   "TODO: Description.",
   NULL,                            // traverse
   NULL,                            // clear
   NULL,                            // rich compare
   NULL,                            // weak list offset
   MobjProxyIterator::ReturnSelf,   // iter
   MobjProxyIterator::Next,         // next

   // attributes //
   NULL,                         // methods
   NULL,                         // members
   NULL,                         // getset
};

// Unused
PyMethodDef MobjProxyIterator::Methods[] = {
   {NULL, NULL, 0, NULL} // Sentinel
};

PyGetSetDef MobjProxyIterator::GetSetMethods[] = { {NULL} };

// MODULE FUNCTIONS ===========================================================

PyObject* Aeon_GetLevelTime (PyObject *dummy, PyObject *args)
{
   if (gamestate != GS_LEVEL)
   {
      PyErr_SetString (PyExc_RuntimeError, "getLevelTime called outside of a level");
      return NULL;
   }
   return Py_BuildValue ("i", leveltime);
}

PyObject* Aeon_SpawnMobj (PyObject *dummy, PyObject *args)
{
   const char *thingname;
   mobjtype_t thingtype;
   double x, y, z;
   Mobj *result = NULL;

   if (gamestate != GS_LEVEL)
   {
      PyErr_SetString (PyExc_RuntimeError, "spawnMapObject called outside of a level");
      return NULL;
   }

   if (!PyArg_ParseTuple (args, "(ddd)s", &x, &y, &z, &thingname))
      return NULL;

   if ((thingtype = E_ThingNumForName(thingname)) == -1)
   {
      PyErr_SetString (PyExc_ValueError, "Invalid thing type");
      return NULL;
   }

   result = P_SpawnMobj (M_DoubleToFixed (x), M_DoubleToFixed (y), M_DoubleToFixed (z), thingtype);

   if (!result)
   {
      PyErr_SetString (PyExc_RuntimeError, "Could not spawn thing");
      return NULL;
   }

   Py_INCREF (result->aeonproxy);
   return (PyObject *) result->aeonproxy;
}

// MODULE DEFINITION ==========================================================

static struct PyMethodDef AeonMod_Level_Methods[] = {
   {"getLevelTime",     (PyCFunction) Aeon_GetLevelTime, METH_NOARGS,
    "Returns the current level time in 1/35ths of a second."},
   {"spawnMapObject",   (PyCFunction) Aeon_SpawnMobj,    METH_VARARGS,
    "Attempts to spawn a new MapObject and returns the reference if successful."},
   {NULL, NULL, 0, NULL} // Sentinel
};

static struct PyModuleDef AeonMod_Level_ModuleDef = {
   PyModuleDef_HEAD_INIT,
   "level",
   "This module contains all level interaction classes and functions.",
   0,
   AeonMod_Level_Methods,
   NULL,
   NULL,
   NULL,
   NULL
};

PyObject* init_AeonMod_level ()
{
    PyObject *module = PyModule_Create(&AeonMod_Level_ModuleDef);

    if (!module)
        return NULL;

    MobjProxyIterator::Type.tp_new = MobjProxyIterator::PyConstructor;

    if (PyType_Ready(&MobjProxy::Type) < 0)
        return NULL;

    if (PyType_Ready(&MobjProxyIterator::Type) < 0)
        return NULL;

    Py_INCREF(&MobjProxy::Type);
    PyModule_AddObject(module, "MapObject", (PyObject *) &MobjProxy::Type);
    Py_INCREF(&MobjProxyIterator::Type);
    PyModule_AddObject(module, "MapObjectIterator", (PyObject *) &MobjProxyIterator::Type);

    return module;
}
