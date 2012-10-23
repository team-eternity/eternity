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

#ifndef __PYMOD_LEVEL_H__
#define __PYMOD_LEVEL_H__

#include "doomstat.h"
#include "ae_pyinter.h"

class SaveArchive;
class Mobj;

// CLASSES ====================================================================

//-------------------------------------
//
// MapObject
//
//-------------------------------------

class MobjProxy : public ZoneObject, public PyObject
{
   PyPP_Header;
   
public:
   // Instance variables
   Mobj *owner;
   PyObject *attributedict;

   // C methods
   MobjProxy (Mobj *owner);
   ~MobjProxy (void);

   virtual void serialize (SaveArchive &arc);

   // Python methods
   static void      PyDestructor (PyObject  *self);
   static int       SetAttribute (PyObject  *self, char *attr_name, PyObject *value);
   static PyObject* GetAttribute (PyObject  *self, char *attr_name);
   static int       DelAttribute (PyObject  *self, char *attr_name);
};

//-------------------------------------
//
// MapObjectIterator
//
//-------------------------------------

class MobjProxyIterator : public ZoneObject, public PyObject
{
   PyPP_Header;
   
public:
   // Instance variables
   Thinker *current;

   // C methods
   MobjProxyIterator (void)
      : current (&thinkercap)
   {
      PyObject_Init ((PyObject*) this, &MobjProxyIterator::Type);
   };
   ~MobjProxyIterator (void) {};

   // Python methods
   static PyObject *PyConstructor (PyTypeObject *T, PyObject *args, PyObject *keywords);
   static void      PyDestructor  (PyObject *obj);
   static PyObject *ReturnSelf (PyObject *obj); // I can't believe I need this.
   static PyObject *Next (PyObject *self);
};

// MODULE FUNCTIONS ===========================================================

PyObject* init_AeonMod_level (void);

#endif // __PYMOD_LEVEL_H__

// EOF
