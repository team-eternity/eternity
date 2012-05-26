// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
//
// Classes for storage of inventory item properties in mobjinfo MetaTables
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_METAITEMS_H__
#define E_METAITEMS_H__

#include "metaapi.h"
#include "e_inventory.h"

//
// MetaGiveItem
//
// This class represents an item to be given to the player through contact
// with an item bearing the MF_SPECIAL flag.
//
class MetaGiveItem : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaGiveItem, MetaObject)

protected:
   inventory_t *inventory;

public:
   // Default constructor
   MetaGiveItem() : Super(), inventory(NULL)
   {
   }

   // Parameterized constructor
   MetaGiveItem(const char *key, inventory_t *pInv) 
      : Super(key), inventory(pInv)
   {
   }

   // Copy constructor
   MetaGiveItem(const MetaGiveItem &other) : Super(other)
   {
      this->inventory = other.inventory;
   }
   
   // Accessors
   inventory_t *getValue() const   { return inventory; }
   void setValue(inventory_t *inv) { inventory = inv;  }

   // Clone - virtual copy constructor
   virtual MetaObject *clone() const { return new MetaGiveItem(*this); }

   // toString - virtual method for nice display of metastate properties.
   virtual const char *toString() const { return inventory->name; }
};

#endif

// EOF

