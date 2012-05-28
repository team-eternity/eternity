// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//
//    Metatable generic algorithms
//
//-----------------------------------------------------------------------------

#ifndef METAALGORITHMS_H__
#define METAALGORITHMS_H__

#include "metaapi.h"

//
// MetaPredicateAll
//
// Calls the passed method on the indicated object for each object in the
// MetaTable that matches the MetaType template argument type and the passed-in
// key, until a method call returns false.
//
template<typename T, typename MetaType> 
bool MetaPredicateAll(MetaTable *mt, const char *key, 
                      T *object, bool (T::*method)(MetaType &))
{
   MetaObject *metaObj = NULL;

   while((metaObj = mt->getNextKeyAndType(metaObj, key, RTTI(MetaType))))
   {
      if(!(object->*method)(static_cast<MetaType &>(*metaObj)))
         return false; // can short-circuit after first failure
   }

   return true; // all passed
}

//
// MetaPredicateAny
//
// Calls the passed method on the indicated object for each object in the
// MetaTable that matches the MetaType template argument type and the passed-in
// key, until a method call returns true.
//
template<typename T, typename MetaType>
bool MetaPredicateAny(MetaTable *mt, const char *key, 
                      T *object, bool (T::*method)(MetaType &))
{
   MetaObject *metaObj = NULL;

   while((metaObj = mt->getNextKeyAndType(metaObj, key, RTTI(MetaType))))
   {
      if((object->*method)(static_cast<MetaType &>(*metaObj)))
         return true; // can short-circuit after first success
   }

   return false; // none passed
}

//
// MetaForEach
//
// Unconditionally calls the passed method on the indicated object for every
// object in the MetaTable matching the key and type.
//
template<typename T, typename MetaType>
void MetaForEach(MetaTable *mt, const char *key,
                 T *object, void (T::*method)(MetaType &))
{
   MetaObject *metaObj = NULL;

   while((metaObj = mt->getNextKeyAndType(metaObj, key, RTTI(MetaType))))
      (object->*method)(static_cast<MetaType &>(*metaObj));
}

#endif

// EOF

