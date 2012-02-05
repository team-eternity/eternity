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
//   MetaQString adapter class for storage of qstrings as metatable properites.
//
//-----------------------------------------------------------------------------

#ifndef METAQSTRING_H__
#define METAQSTRING_H__

#include "metaadapter.h"
#include "m_qstr.h"

class MetaQString : public MetaAdapter<qstring>
{
public:
   MetaQString(const char *key) : MetaAdapter<qstring>("MetaQString", key)
   {
   }

   MetaQString(const char *key, const qstring &initValue)
      : MetaAdapter<qstring>("MetaQString", key, initValue)
   {
   }

   MetaQString(const char *key, const char *value)
      : MetaAdapter<qstring>("MetaQString", key)
   {
      containedObject = value;
   }

   MetaQString(const MetaQString &other) : MetaAdapter<qstring>(other) {}

   // Virtuals
   MetaObject *clone()    const { return new MetaQString(*this);     }
   const char *toString() const { return containedObject.constPtr(); }
};

#endif

// EOF


