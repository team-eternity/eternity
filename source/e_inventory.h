// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley
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
//   Inventory
//
//-----------------------------------------------------------------------------

#ifndef E_INVENTORY_H__
#define E_INVENTORY_H__

class MetaTable;

// Inventory item ID is just an integer. The inventory item type can be looked
// up using this.
typedef int inventoryitemid_t;

// Inventory Slot
// Every slot in an inventory is composed of this structure. It references the
// type of item in the slot via ID number, and tells how many of the item is
// being carried. That's all it does.
struct inventoryslot_t
{
   inventoryitemid_t item;   // The item.
   int               amount; // Amount possessed.
};

// Inventory
// An inventory is an array of inventoryslot structures, allocated at the max
// number of inventory items it is possible to carry (ie. the number of distinct
// inventory items defined).
typedef inventoryslot_t * inventory_t;

// Effect Types
enum
{
   ITEMFX_NONE,   // has no effect
   ITEMFX_HEALTH, // an immediate-use health pickup
   NUMITEMFX
};
typedef int itemeffecttype_t;

//
// Item Effect
// 
// An item effect is a MetaTable. The properties in the table depend on the type
// of section that instantiated the effect (and therefore what its purpose is).
//
typedef MetaTable itemeffect_t;

//
// Functions
//

// Get an item effect type number by name
itemeffecttype_t E_EffectTypeForName(const char *name);

// Find an item effect by name
itemeffect_t *E_ItemEffectForName(const char *name);

// Get the item effects table
MetaTable *E_GetItemEffects();

#ifdef NEED_EDF_DEFINITIONS

// Section Names
#define EDF_SEC_HEALTHFX "healtheffect"

// Section Defs
extern cfg_opt_t edf_healthfx_opts[];

// Functions
void E_ProcessInventory(cfg_t *cfg);

#endif

#endif

// EOF

