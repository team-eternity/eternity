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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//  Mobj Collections
//  These are used by stuff like Boss Spawn Spots and D'Sparil landings, and
//  evolved out of a generalization of the code Lee originally wrote to remove
//  the boss spot limit in BOOM. The core logic is now based on the generic
//  PODCollection class, however, and this just adds a bit of utility to it.
//
//-----------------------------------------------------------------------------

#ifndef P_MOBJCOL_H__
#define P_MOBJCOL_H__

#include "m_collection.h"
#include "m_dllist.h"
#include "m_qstr.h"
#include "m_random.h"

class Mobj;
class mobjCollectionSetPimpl;

class MobjCollection : public PODCollection<Mobj *>
{
protected:
   DLListItem<MobjCollection> hashLinks; // links for MobjCollectionSet hash

   friend class mobjCollectionSetPimpl;

public:
   MobjCollection() 
      : PODCollection<Mobj *>(), hashLinks(), mobjType(), enabled(true) 
   {
   }

   // public properties
   qstring mobjType;
   bool    enabled;

   void collectThings();
   bool spawnAtRandom(const char *type, pr_class_t prnum, 
                      int spchance, int coopchance, int dmchance);
   bool startupSpawn();
   void moveToRandom(Mobj *actor);
};

// MobjCollectionSet maintains a global hash of MobjCollection objects that
// are created via EDF and will probably eventually be accessible through
// Aeon scripting too.

class MobjCollectionSet
{
private:
   mobjCollectionSetPimpl *pImpl; // yet another private implementation idiom

public:
   MobjCollectionSet();
   MobjCollection *collectionForName(const char *name);
   void addCollection(const char *mobjType);
   void setCollectionEnabled(const char *mobjType, bool enabled);
   void collectAllThings();
};

extern MobjCollectionSet MobjCollections;

#endif

// EOF

