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
//   Classes for random spot spawns.
//
//-----------------------------------------------------------------------------

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

   virtual MetaObject *clone()    const { return new MetaCollectionSpawn(*this); }
   virtual const char *toString() const { return type.constPtr(); }
};

#endif

// EOF

