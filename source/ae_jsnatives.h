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

#ifndef AE_JSNATIVES_H__
#define AE_JSNATIVES_H__

struct JSContext;
struct JSObject;

namespace AeonJS
{
   typedef enum
   {
      NOSUCHPROPERTY,  // Can't find that as a native
      RESOLUTIONERROR, // A JSAPI error occurred while resolving
      RESOLVED         // Successful resolution/init
   } NativeInitCode;

   typedef NativeInitCode (*NativeInitFn)(JSContext *, JSObject *);

   //
   // This class represents an Aeon JS native, be it a class to register,
   // or a property or method of the context global. Instances are self-
   // registering at construction and can provide any arbitrary function
   // for initialization within a given context via NativeInitFn.
   //
   class Native
   {
   protected:
      // Static hash table
      enum { NUMCHAINS = 257 };
      static Native *chains[NUMCHAINS];
      
      // Instance variables
      
      Native       *next; // Next on hash chain
      NativeInitFn  init; // Init function, to instantiate within a context
      const char   *name; // Name, for lookup 

      void add(); // Called when object is constructing

   public:
      Native(const char *pName, NativeInitFn pInit)
         : name(pName), init(pInit)
      {
         // Add self to class's static hash of all instances
         add();
      }

      // Initialize a native by name within the given context and object.
      static NativeInitCode InitByName(const char *name, 
                                       JSContext *cx, JSObject *obj);

      static NativeInitCode EnumerateAll(JSContext *cx, JSObject *obj);
   };
}

#endif

// EOF

