// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2009 James Haley
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//
//   MetaObject descendants for adaptation of external objects to storage
//   inside MetaTables.
//
//-----------------------------------------------------------------------------

#ifndef METAADAPTER_H__
#define METAADAPTER_H__

#include "metaapi.h"

//
// MetaAdapter<T>
//
// Primary class for adapting non-MetaObject-descendant objects to behave as
// metatable properties. The only requirement exacted on type T is that it
// be directly copyable - either a POD, or an object with a copy constructor.
//
// The primary means of external object adaptation is forwarding of method 
// calls and operators through the star and arrow operator overloads, which 
// make this class behave similar to a smart pointer. Via use of operator ->,
// it is possible to delegate directly all messages to the contained object
// without forcing an is-a relationship that would otherwise lead to multiple 
// inheritance for many types such as qstring.
//
template<typename T> class MetaAdapter : public MetaObject
{
protected:
   T containedObject;

public:
   MetaAdapter(metatypename_t className, const char *key)
      : MetaObject(className, key), containedObject()
   {
   }

   MetaAdapter(metatypename_t className, const char *key, const T &initValue)
      : MetaObject(className, key), containedObject(initValue)
   {
   }

   MetaAdapter(const MetaAdapter<T> &other)
      : MetaObject(other), containedObject(other.containedObject)
   {
   }

   // NB: You MUST override this yourself if you subclass.
   MetaObject *clone() const { return new MetaAdapter<T>(*this); }

   // NB: Inheriting classes should override MetaObject::toString if
   // they are interested; there is no implementation in this class.

   // Operators
   T *operator -> () { return &containedObject; }
   T &operator *  () { return  containedObject; }
};

#endif

// EOF

