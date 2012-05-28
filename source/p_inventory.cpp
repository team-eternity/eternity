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
//    Inventory and Item Effects
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "e_inventory.h"
#include "e_metaitems.h"
#include "metaapi.h"
#include "metaalgorithms.h"
#include "p_inventory.h"
#include "p_mobj.h"

// Inventory class singletons
static InventoryGeneric inventoryGenericObj;
static InventoryHealth  inventoryHealthObj;

// Inventory classtype to instance lookup
static InventoryGeneric *InstanceForInventoryClass[INV_CLASS_NUMCLASSES] =
{
   &inventoryGenericObj, // INV_CLASS_NONE
   &inventoryHealthObj,  // INV_CLASS_HEALTH
};

//=============================================================================
//
// InventoryGeneric
//
// Base class for all inventory effects
//

//
// InventoryGeneric::canCollect
//
// Called for each inventory classtype in the metatable.
//
bool InventoryGeneric::canCollect(pickupdata_t &params)
{
   // TODO: any default logic here?
   return true;
}

class CheckFunctor
{
public:
   InventoryGeneric::pickupdata_t &params;

   CheckFunctor(InventoryGeneric::pickupdata_t &pParams) : params(pParams)
   {
   }

   //
   // checkClass
   //
   // Call the inventory class object's canCollect method to see if the
   // player needs this effect or not.
   //
   bool checkClass(MetaInteger &metaInt)
   {
      // We've been passed one of the classtypes from the MetaTable
      int classType = metaInt.getValue(); 
      return InstanceForInventoryClass[classType]->canCollect(params);
   }
};

//
// InventoryGeneric::CheckForCollect
//
// Checks to see if the collector can gain the given or picked-up inventory
// item based on the qualities of each implemented class and depending upon
// the inventory flags pertaining to whether or not the item must have 
// needed effects before it'll be picked up.
//
bool InventoryGeneric::CheckForCollect(pickupdata_t &params)
{
   bool result = false;
   MetaTable *invMeta = params.inv->meta;
   CheckFunctor cf(params);

   if(params.inv->flags & INVF_ALWAYSPICKUP)
      result = true;
   else if(params.inv->flags & INVF_PICKUPIFANY)
      result = MetaPredicateAny(invMeta, "classtype", &cf, &CheckFunctor::checkClass);
   else
      result = MetaPredicateAll(invMeta, "classtype", &cf, &CheckFunctor::checkClass);

   return result;
}

//
// InventoryGeneric::GiveItem
//
// Called to directly give an item to an Mobj.
//
bool InventoryGeneric::GiveItem(Mobj *collector, inventory_t *item)
{
   bool result = false;


   return result;
}

//
// InventoryGeneric::TouchItem
//
// Called from P_TouchSpecialThing when an Mobj makes contact with another
// object bearing the MF_SPECIAL flag.
//
bool InventoryGeneric::TouchItem(Mobj *collector, Mobj *item)
{
   bool        result   = false;
   MetaTable  *itemMeta = item->info->meta;
   MetaObject *obj;

   if((obj = itemMeta->getObjectKeyAndType("giveitem", RTTI(MetaGiveItem))))
   {
      inventory_t *inventory = static_cast<MetaGiveItem *>(obj)->getValue();
      pickupdata_t data;

      data.collector = collector;
      data.item      = item;
      data.inv       = inventory;

      // Check for player class restrictions. The item may change its behavior
      // if the collector does not fall within them (or it may deny pickup
      // altogether). We'll record the findings of this operation in the
      // parameter structure for reference below.
      /*TODO*/;

      // Check for basic collection capability, ie.:
      // * If it's ALWAYSPICKUP - it may not help, but we'll still grab it
      // * If it is instant-activate, and the player needs one of its effects
      // * If it goes into the inventory, and there is room for it
      if(CheckForCollect(data))
      {
         // If fell within class restrictions, apply full effects
         // Else, apply restricted effects
         /*TODO*/;

         // Provided collection is successful, do general item stuff:
         // * Palette flash
         // * Message
         // * COUNTITEM flag
         // * GiveQuest
         // * Remove the item?
         /*TODO*/;
      }
   }

   return result;
}

// EOF

