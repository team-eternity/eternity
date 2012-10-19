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
#include "ae_jsapi.h"
#include "ae_jsnatives.h"

//============================================================================
//
// AeonJS::Native
//
// Management of Aeon API native classes/objects/methods, for support of 
// memory- and time-efficient lazy property resolution.
//

// Static hash table
AeonJS::Native *AeonJS::Native::chains[NUMCHAINS];

//
// AeonJS::Native::add
//
// Add an instance to the class's static hash table when it is constructed.
//
void AeonJS::Native::add()
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
AeonJS::NativeInitCode AeonJS::Native::InitByName(const char *name, 
                                                  JSContext *cx, JSObject *obj)
{
   unsigned int hc = qstring::HashCodeCaseStatic(name) % NUMCHAINS;
   Native *cur = chains[hc];

   while(cur && strcmp(cur->name, name))
      cur = cur->next;

   return cur ? cur->init(cx, obj) : NOSUCHPROPERTY;
}

//
// AeonJS::Native::EnumerateAll
//
// Add all lazy-resolved natives to the context/object that haven't
// already been added, "just-in-time" for enumeration of the context
// globals.
//
AeonJS::NativeInitCode AeonJS::Native::EnumerateAll(JSContext *cx, JSObject *obj)
{
   NativeInitCode ret = RESOLVED;

   // Run down each hash chain
   for(int i = 0; i < NUMCHAINS; i++)
   {
      Native *cur = chains[i];

      while(cur)
      {
         JSBool found = JS_FALSE;
         if(!JS_AlreadyHasOwnProperty(cx, obj, cur->name, &found))
         {
            ret = RESOLUTIONERROR; // JSAPI error
            break;
         }
         else if(!found) // Not already defined?
         {
            NativeInitCode initCode = cur->init(cx, obj);
            if(initCode == RESOLUTIONERROR)
            {
               ret = initCode; // JSAPI error in init callback
               break;
            }
         } 
         cur = cur->next;
      }
   }

   return ret;
}

// EOF

