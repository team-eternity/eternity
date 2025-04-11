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
//  Mobj Collections
//  These are used by stuff like Boss Spawn Spots and D'Sparil landings, and
//  evolved out of a generalization of the code Lee originally wrote to remove
//  the boss spot limit in BOOM. The core logic is now based on the generic
//  PODCollection class, however, and this just adds a bit of utility to it.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomstat.h"
#include "e_hash.h"
#include "e_lib.h"
#include "e_things.h"
#include "metaapi.h"
#include "metaspawn.h"
#include "m_qstrkeys.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_mobjcol.h"

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

   if(!enabled || mobjType.empty())
      return;

   if((typenum = E_ThingNumForName(mobjType.constPtr())) < 0)
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

//
// MobjCollection::spawnAtRandom
//
// Spawn an Mobj of the indicated type at a random spot in the collection.
// The three chance parameters allow for the spawn itself to be randomized,
// as if a RNG call falls outside the threshold specified for the current
// game type, the spawn won't take place at all.
//
bool MobjCollection::spawnAtRandom(const char *type, pr_class_t prnum,
                                   int spchance, int coopchance, int dmchance)
{
   int chance;

   // need spots to spawn at.
   if(isEmpty())
      return false;

   // check for failure to spawn
   switch(GameType)
   {
   case gt_single:
      chance = spchance;
      break;
   case gt_coop:
      chance = coopchance;
      break;
   case gt_dm:
      chance = dmchance;
      break;
   default:
      chance = 0;
      break;
   }

   if(P_Random(prnum) < chance)
      return false;

   // get a random spot
   Mobj *spot;
   
   spot = getRandom(prnum);

   // spawn the object there
   P_SpawnMobj(spot->x, spot->y, spot->z, E_SafeThingName(type));
   return true;
}

//
// MobjCollection::startupSpawn
//
// Handle a potential startup random spawn associated with a collection.
// The data necessary for the spawn consists of meta fields on the 
// collection's thingtype's mobjinfo.
//
bool MobjCollection::startupSpawn()
{
   int type = E_ThingNumForName(mobjType.constPtr());
   if(type < 0)
      return false;

   MetaTable  *meta = mobjinfo[type]->meta;
   auto        mcs  = meta->getObjectTypeEx<MetaCollectionSpawn>();
   bool        res  = false;

   if(mcs)
   {
      res = spawnAtRandom(mcs->type.constPtr(), pr_spotspawn,
                          mcs->spchance, mcs->coopchance, mcs->dmchance);
   }

   return res;
}

//
// MobjCollection::moveToRandom
//
// Move an object to a random spot in the collection.
//
void MobjCollection::moveToRandom(Mobj *actor)
{
   if(isEmpty())
      return;

   P_UnsetThingPosition(actor);
   actor->copyPosition(getRandom(pr_moverandom));
   P_SetThingPosition(actor);
   P_AdjustFloorClip(actor);
   actor->backupPosition();
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
              ENCQStrHashKey, 
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
// mnemonic. Returns nullptr if not found or there's no such thing.
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
      newcol->mobjType = mobjType;
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
      col->enabled = enabled;
}

//
// MobjCollectionSet::collectAllThings
//
// Called at the start of each level to collect all spawned mapthings into
// their corresponding collections.
//
void MobjCollectionSet::collectAllThings()
{
   MobjCollection *rover = nullptr;

   while((rover = pImpl->collectionHash.tableIterator(rover)))
   {
      rover->clear();
      rover->collectThings();
      rover->startupSpawn();
   }
}

// The one and only global MobjCollectionSet
MobjCollectionSet MobjCollections;

// EOF

