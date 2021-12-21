// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//
//   MetaQString adapter class for storage of qstrings as metatable properties.
//
//-----------------------------------------------------------------------------

#ifndef METAQSTRING_H__
#define METAQSTRING_H__

#include "metaapi.h"
#include "m_collection.h"
#include "m_qstr.h"

class MetaQString : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaQString, MetaObject)

public:
   qstring value;

   MetaQString() : Super(), value()
   {
   }

   explicit MetaQString(const char *key) : Super(key), value()
   {
   }

   MetaQString(const char *key, const qstring &initValue)
      : Super(key), value(initValue)
   {
   }

   MetaQString(const char *key, const char *ccvalue)
      : Super(key), value(ccvalue)
   {
   }

   MetaQString(const MetaQString &other)
      : Super(other), value(other.value)
   {
   }

   // Virtuals
   MetaObject *clone()    const override { return new MetaQString(*this); }
   const char *toString() const override { return value.constPtr(); }
};

#endif

// EOF


