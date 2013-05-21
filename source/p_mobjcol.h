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

#ifndef P_MOBJCOL_H__
#define P_MOBJCOL_H__

#include "m_collection.h"
#include "m_dllist.h"

class Mobj;
class mobjCollectionSetPimpl;

class MobjCollection : public PODCollection<Mobj *>
{
protected:
   DLListItem<MobjCollection> hashLinks; // links for MobjCollectionSet hash
   char *mobjType;
   bool  enabled;

   friend class mobjCollectionSetPimpl;

public:
   MobjCollection()
      : PODCollection<Mobj *>(), hashLinks(), mobjType(NULL), enabled(true)
   {
   }

   const char *getMobjType() const  { return mobjType; }
   void setMobjType(const char *mt);
   bool isEnabled() const   { return enabled;  }
   void setEnabled(bool en) { enabled = en;    }

   void collectThings();
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
