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

#ifndef __PY_MOD_BASE_H__
#define __PY_MOD_BASE_H__

#include "ae_pyinter.h"
#include "v_misc.h"

PyObject* init_AeonMod_base ();

class TextPrinter : public PyObject, public ZoneObject
{
   PyPP_Header;

   const char* color;
   qstring *buffer;

   TextPrinter (const char* color = FC_NORMAL, PyTypeObject* T = &Type)
   {
      this->ob_type = T;
      _Py_NewReference(this);

      buffer = new qstring ();
      this->color = color;
   };

   virtual ~TextPrinter() { delete buffer; };
   static void PyDestructor(PyObject *obj)
   {
      TextPrinter* me = (TextPrinter*) obj;
      delete me;
   };

public:
   static PyObject* write (PyObject*, PyObject*);
};

#endif

// EOF
