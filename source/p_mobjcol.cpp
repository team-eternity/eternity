// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
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
//  Mobj Collections
//  These are used by stuff like Boss Spawn Spots and D'Sparil landings, and
//  evolved out of a generalization of the code Lee originally wrote to remove
//  the boss spot limit in BOOM. The core logic is now based on the generic
//  PODCollection class, however, and this just adds a bit of utility to it.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "e_hash.h"
#include "e_hashkeys.h"
#include "e_lib.h"
#include "e_things.h"
#include "p_mobj.h"
#include "p_mobjcol.h"

//
// MobjCollection::setMobjType
//
// Set the type of object this collection tracks.
//
void MobjCollection::setMobjType(const char *mt)
{
   E_ReplaceString(mobjType, estrdup(mt));
}

//
// MobjCollection::collectThings
//
// Collects pointers to all non-removed Mobjs on the level of the type stored
// in the collection.
//
void MobjCollection::collectThings()
{
   Thinker *th;
   int typenum;

   if(!enabled || !mobjType)
      return;

   if((typenum = E_ThingNumForName(mobjType)) < 0)
      return;

   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      Mobj *mo;

      if((mo = thinker_cast<Mobj *>(th)))
      {
         if(mo->type == typenum)
            add(mo);
      }
   }
}

//=============================================================================
//
// MobjCollectionSet Methods
//

// mobjCollectionSetPimpl - private implementation idiom object for the
// MobjCollectionSet class.

class mobjCollectionSetPimpl : public ZoneObject
{
public:
   EHashTable<MobjCollection,
              ENCStringHashKey,
              &MobjCollection::mobjType,
              &MobjCollection::hashLinks> collectionHash;

   mobjCollectionSetPimpl() : ZoneObject(), collectionHash(31) {}
};

//
// MobjCollectionSet constructor
//
MobjCollectionSet::MobjCollectionSet()
{
   pImpl = new mobjCollectionSetPimpl();
}

//
// MobjCollectionSet::collectionForName
//
// Tries to find a collection matching the thingtype with the given
// mnemonic. Returns NULL if not found or there's no such thing.
//
MobjCollection *MobjCollectionSet::collectionForName(const char *name)
{
   return pImpl->collectionHash.objectForKey(name);
}

//
// MobjCollection::addCollection
//
// Add a collection for a thingtype, if it doesn't exist already.
//
void MobjCollectionSet::addCollection(const char *mobjType)
{
   if(!pImpl->collectionHash.objectForKey(mobjType))
   {
      MobjCollection *newcol = new MobjCollection();
      newcol->setMobjType(mobjType);
      pImpl->collectionHash.addObject(newcol);
   }
}

//
// MobjCollectionSet::setCollectionEnabled
//
// Change the enable state of a collection
//
void MobjCollectionSet::setCollectionEnabled(const char *mobjType, bool enabled)
{
   MobjCollection *col = pImpl->collectionHash.objectForKey(mobjType);

   if(col)
      col->setEnabled(enabled);
}

//
// MobjCollectionSet::collectAllThings
//
// Called at the start of each level to collect all spawned mapthings into
// their corresponding collections.
//
void MobjCollectionSet::collectAllThings()
{
   MobjCollection *rover = NULL;

   while((rover = pImpl->collectionHash.tableIterator(rover)))
   {
      rover->clear();
      rover->collectThings();
   }
}

// The one and only global MobjCollectionSet
MobjCollectionSet MobjCollections;

// EOF
