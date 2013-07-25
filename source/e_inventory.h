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

class  MetaTable;
struct player_t;

// Inventory item ID is just an integer. The inventory item type can be looked
// up using this.
typedef int inventoryitemid_t;

// Inventory index is an index into a player's inventory_t array. It is NOT
// the same as the item ID. You get from one to the other by using:
//   inventory[index].item
typedef int inventoryindex_t;

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
   ITEMFX_NONE,     // has no effect
   ITEMFX_HEALTH,   // an immediate-use health item
   ITEMFX_ARMOR,    // an immediate-use armor item
   ITEMFX_POWER,    // a power-up giver
   ITEMFX_ARTIFACT, // an item that enters the inventory, for later use or tracking
   NUMITEMFX
};
typedef int itemeffecttype_t;

// Artifact sub-types
enum
{
   ARTI_NORMAL,   // an ordinary artifact
   ARTI_AMMO,     // ammo type
   ARTI_BACKPACK, // backpack token
   ARTI_KEY,      // key
   ARTI_PUZZLE,   // puzzle item
   ARTI_POWER,    // powerup token
   ARTI_WEAPON,   // weapon token
   ARTI_QUEST,    // quest token
   NUMARTITYPES
};
typedef int artitype_t;

// Hard-coded names for specially treated artifact types
#define ARTI_BACKPACKITEM "BackpackItem"
#define ARTI_BLUECARD     "BlueCard"
#define ARTI_BLUESKULL    "BlueSkull"
#define ARTI_REDCARD      "RedCard"
#define ARTI_REDSKULL     "RedSkull"
#define ARTI_YELLOWCARD   "YellowCard"
#define ARTI_YELLOWSKULL  "YellowSkull"
#define ARTI_KEYGREEN     "KeyGreen"
#define ARTI_KEYYELLOW    "KeyYellow"
#define ARTI_KEYBLUE      "KeyBlue"

//
// Item Effect
// 
// An item effect is a MetaTable. The properties in the table depend on the type
// of section that instantiated the effect (and therefore what its purpose is).
//
typedef MetaTable itemeffect_t;

//
// Effect Bindings
//

enum pickupflags_e
{
   PFXF_ALWAYSPICKUP    = 0x00000001, // item is picked up even if not useful
   PFXF_LEAVEINMULTI    = 0x00000002, // item is left in multiplayer games
   PFXF_NOSCREENFLASH   = 0x00000004, // does not cause bonuscount increment
   PFXF_SILENTNOBENEFIT = 0x00000008, // no pickup effects if picked up without benefit
};

struct e_pickupfx_t
{
   itemeffect_t *effect;  // item given, if any
   char         *message; // message, if any
   char         *sound;   // sound, if any
   unsigned int  flags;   // pickup flags

   int tempeffect;        // INVENTORY_FIXME: temporary transitional field
};

// INVENTORY_TODO: alter as needed
extern e_pickupfx_t *pickupfx;

//
// Functions
//

// Get an item effect type number by name
itemeffecttype_t E_EffectTypeForName(const char *name);

// Find an item effect by name
itemeffect_t *E_ItemEffectForName(const char *name);

// Get the item effects table
MetaTable *E_GetItemEffects();

// Get the number of ammo type artifacts defined.
size_t E_GetNumAmmoTypes();

// Get an ammo type for its index in the fast lookup table.
itemeffect_t *E_AmmoTypeForIndex(size_t idx);

// Get the number of key type artifacts defined.
size_t E_GetNumKeyItems();

// Get a key item for its index in the fast lookup table.
itemeffect_t *E_KeyItemForIndex(size_t idx);

// Give all "key" type artifacts to a player
int E_GiveAllKeys(player_t *player);

// Take all "key" type artifacts from a player
int E_TakeAllKeys(player_t *player);

// Check if a player is able to unlock a lock, by its lock ID.
bool E_PlayerCanUnlock(player_t *player, int lockID, bool remote);

// Obtain an item effect definition for its inventory item ID
itemeffect_t *E_EffectForInventoryItemID(inventoryitemid_t id);

// Obtain an item effect definition for a player's inventory index
itemeffect_t *E_EffectForInventoryIndex(player_t *player, inventoryindex_t idx);

// Get the slot being used for a particular inventory item, by ID, if one
// exists. Returns NULL if the item isn't in the player's inventory.
inventoryslot_t *E_InventorySlotForItemID(player_t *player, inventoryitemid_t id);

// Get the slot being used for a particular inventory item, by item pointer, if
// one exists. Returns NULL if the item isn't in the player's inventory.
inventoryslot_t *E_InventorySlotForItem(player_t *player, itemeffect_t *effect);

// Get the slot being used for a particular inventory item, by name, if one 
// exists. Returns NULL if the item isn't in the player's inventory.
inventoryslot_t *E_InventorySlotForItemName(player_t *player, const char *name);

// Special function to test for player backpack.
bool E_PlayerHasBackpack(player_t *player);

// Lookup the maximum amount a player can carry of a specific artifact type.
int E_GetMaxAmountForArtifact(player_t *player, itemeffect_t *artifact);

// Place an item into a player's inventory. 
bool E_GiveInventoryItem(player_t *player, itemeffect_t *artifact);

// Remove an item from a player's inventory.
bool E_RemoveInventoryItem(player_t *player, itemeffect_t *artifact, int amount);

// Call at the end of a hub, or a level that isn't part of a hub, to clear
// out items that don't persist.
void E_InventoryEndHub(player_t *player);

// Call to completely clear a player's inventory.
void E_ClearInventory(player_t *player);

// Get allocated size of player inventory arrays
int E_GetInventoryAllocSize();

//
// EDF-Only Definitions
//

#ifdef NEED_EDF_DEFINITIONS

// Section Names
#define EDF_SEC_HEALTHFX "healtheffect"
#define EDF_SEC_ARMORFX  "armoreffect"
#define EDF_SEC_POWERFX  "powereffect"
#define EDF_SEC_ARTIFACT "artifact"
#define EDF_SEC_PICKUPFX "pickupitem"
#define EDF_SEC_LOCKDEF  "lockdef"

// Section Defs
extern cfg_opt_t edf_healthfx_opts[];
extern cfg_opt_t edf_armorfx_opts[];
extern cfg_opt_t edf_powerfx_opts[];
extern cfg_opt_t edf_artifact_opts[];
extern cfg_opt_t edf_pickup_opts[];
extern cfg_opt_t edf_lockdef_opts[];

// Functions
void E_ProcessInventory(cfg_t *cfg);

#endif

#endif

// EOF

