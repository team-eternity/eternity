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
// Aeon Engine - JavaScript Native Interface Management ("Aeon API")
//
// By James Haley
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "m_qstr.h"

// Aeon Engine Includes
#include "ae_jsnatives.h"

//============================================================================
//
// AeonJS::Native
//
// Management of Aeon API native classes/objects/methods, for support of 
// memory- and time-efficient lazy property resolution.
//

typedef AeonJS::Native Native;

// Static hash table
Native *Native::chains[NUMCHAINS];

//
// AeonJS::Native::add
//
// Add an instance to the class's static hash table when it is constructed.
//
void Native::add()
{
   unsigned int hc = qstring::HashCodeCaseStatic(name) % NUMCHAINS;

   next = chains[hc];
   chains[hc] = this;
}

//
// AeonJS::Native::InitByName
//
// Lookup the native by name and then initialize it within a specific JS 
// context and object.
//
AeonJS::NativeInitCode Native::InitByName(const char *name, 
                                          JSContext *cx, JSObject *obj)
{
   unsigned int hc = qstring::HashCodeCaseStatic(name) % NUMCHAINS;
   Native *cur = chains[hc];

   while(cur && strcmp(cur->name, name))
      cur = cur->next;

   return cur ? cur->init(cx, obj) : NOSUCHPROPERTY;
}

// EOF

