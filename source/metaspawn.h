//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Classes for random spot spawns.
// Authors: James Haley
//

#ifndef METASPAWN_H__
#define METASPAWN_H__

#include "metaapi.h"
#include "m_qstr.h"

//
// MetaCollectionSpawn
//
// Tracks data for a collected spot thingtype on the type of object it may
// (possibly randomly) spawn at one of the spots at level start up.
//
class MetaCollectionSpawn : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaCollectionSpawn, MetaObject)

public:
   // public data
   qstring type;
   int     spchance;
   int     coopchance;
   int     dmchance;

   // Constructors

   MetaCollectionSpawn() 
      : Super(), type(), spchance(0), coopchance(0), dmchance(0)
   {
   }

   MetaCollectionSpawn(const char *key, const char *pType, 
                       int pSpchance, int pCoopchance, int pDmchance)
      : Super(key), type(pType), spchance(pSpchance), coopchance(pCoopchance),
        dmchance(pDmchance)
   {
   }

   MetaCollectionSpawn(const MetaCollectionSpawn &other)
      : Super(other), type(other.type), spchance(other.spchance),
        coopchance(other.coopchance), dmchance(other.dmchance)
   {
   }

   virtual MetaObject *clone()    const override { return new MetaCollectionSpawn(*this); }
   virtual const char *toString() const override { return type.constPtr(); }
};

#endif

// EOF

