//
// Copyright (C) 2018 James Haley, Max Waine, et al.
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
//----------------------------------------------------------------------------
//
// Purpose: Inventory
// Authors: James Haley, Max Waine
//

#ifndef E_INVENTORY_H__
#define E_INVENTORY_H__

// Basic inventory definitions are now in d_player.h, so that the entire engine
// doesn't rebuild if you modify this header.
#include "d_player.h"

class MetaTable;

extern inventoryindex_t e_maxvisiblesortorder;

// Effect Types
enum
{
   ITEMFX_NONE,        // has no effect
   ITEMFX_HEALTH,      // an immediate-use health item
   ITEMFX_ARMOR,       // an immediate-use armor item
   ITEMFX_AMMO,        // an ammo giving item; not to be confused with an ammo type
   ITEMFX_POWER,       // a power-up giver
   ITEMFX_WEAPONGIVER, // gives a weapon
   ITEMFX_ARTIFACT,    // an item that enters the inventory, for later use or tracking
   NUMITEMFX
};
typedef int itemeffecttype_t;

// Artifact sub-types
enum
{
   ARTI_NORMAL,   // an ordinary artifact
   ARTI_AMMO,     // ammo type
   ARTI_KEY,      // key
   ARTI_PUZZLE,   // puzzle item
   ARTI_POWER,    // powerup token
   ARTI_WEAPON,   // weapon token
   ARTI_QUEST,    // quest token
   NUMARTITYPES
};
typedef int artitype_t;

// Hard-coded names for specially treated items (needed in DeHackEd, etc.)
#define ITEMNAME_BERSERKHEALTH "BerserkHealth" // The health
#define ITEMNAME_HEALTHBONUS   "HealthBonus"
#define ITEMNAME_MEDIKIT       "Medikit"
#define ITEMNAME_MEGASPHERE    "MegasphereHealth"
#define ITEMNAME_SOULSPHERE    "Soulsphere"
#define ITEMNAME_STIMPACK      "Stimpack"
#define ITEMNAME_GREENARMOR    "GreenArmor"
#define ITEMNAME_BLUEARMOR     "BlueArmor"
#define ITEMNAME_IDFAARMOR     "IDFAArmor"
#define ITEMNAME_ARMORBONUS    "ArmorBonus"
#define ITEMNAME_RAMBOARMOR    "RAMBOArmor"

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

// Hard-coded names for the (currently) 11 specially treated powers in powertype_t.
// These are listed in ascending order of enumeration, as opposed to alphabetical.
#define POWER_INVULNERABLE "PowerInvulnerable"      // pw_invulnerability
#define POWER_STRENGTH     "PowerStrength"          // pw_strength (Berserk)
#define POWER_PARTIALINVIS "PowerPartialInvisibility" // pw_invisibility
#define POWER_IRONFEET     "PowerIronFeet"          // pw_ironfeet (Env. protection)
#define POWER_ALLMAP       "PowerAllmap"            // pw_allmap
#define POWER_INFRARED     "PowerLightAmp"          // pw_infrared (Fullbright)
#define POWER_TOTALINVIS   "PowerTotalInvisibility" // pw_totalinvis
#define POWER_GHOST        "PowerGhost"             // pw_ghost
#define POWER_SILENT       "PowerSilent"            // pw_silencer
#define POWER_FLIGHT       "PowerFlight"            // pw_flight
#define POWER_TORCH        "PowerTorch"             // pw_torch (Fullbright w/ flicker)
#define POWER_WEAPONLEVEL2 "PowerWeaponLevel2"      // pw_weaponlevel2

extern const char *powerStrings[NUMPOWERS];

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

enum pickupflags_e : unsigned int
{
   PXFX_NONE              = 0x00000000,
   PFXF_ALWAYSPICKUP      = 0x00000001, // item is picked up even if not useful
   PFXF_LEAVEINMULTI      = 0x00000002, // item is left in multiplayer games
   PFXF_NOSCREENFLASH     = 0x00000004, // does not cause bonuscount increment
   PFXF_SILENTNOBENEFIT   = 0x00000008, // no pickup effects if picked up without benefit
   PXFX_COMMERCIALONLY    = 0x00000010, // can only be picked up in commercial gamemodes
   PFXF_GIVESBACKPACKAMMO = 0x00000020, // gives backpack ammo
};

struct e_pickupfx_t
{
   char          *name;         // name
   char          *compatname;   // compat name, if any
   spritenum_t    sprnum;       // sprite number, -1 if not set
   unsigned int   numEffects;   // number of effects
   itemeffect_t **effects;      // item given, if any
   weaponinfo_t  *changeweapon; // weapon to change to, if any
   char          *message;      // message, if any
   char          *sound;        // sound, if any
   pickupflags_e  flags;        // pickup flags

   // EDF Hashing
   DLListItem<e_pickupfx_t> namelinks;   // hash by name
   DLListItem<e_pickupfx_t> cnamelinks;  // hash by compat name
   DLListItem<e_pickupfx_t> sprnumlinks; // hash by sprite num
};

extern inventoryitemid_t e_maxitemid;   // IOANCH: no longer static

//
// Functions
//

// Get an item effect type number by name
itemeffecttype_t E_EffectTypeForName(const char *name);

// Remove an item effect from the table.
void E_RemoveItemEffect(itemeffect_t *effect);

// Find an item effect by name
itemeffect_t *E_ItemEffectForName(const char *name);

// Get the item effects table
MetaTable *E_GetItemEffects();

// Get the number of ammo type artifacts defined.
size_t E_GetNumAmmoTypes();

// enum for E_GiveAllAmmo behavior
enum giveallammo_e
{
   GAA_MAXAMOUNT,      // give max amount
   GAA_CUSTOM          // use amount argument
};

// Give the player a certain amount of all ammo types
void E_GiveAllAmmo(player_t *player, giveallammo_e op, int amount = -1);

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
bool E_PlayerCanUnlock(const player_t *player, int lockID, bool remote, bool mute = false);

// Get the automap color for a lockdef
int E_GetLockDefColor(int lockID);

// Tries to move the inventory cursor 'amount' right.
bool E_MoveInventoryCursor(player_t *player, int amount, int &cursor);

// Says if a player possesses at least one item w/ +invbar
bool E_PlayerHasVisibleInvItem(player_t *player);

// Tries to use the currently selected item.
void E_TryUseItem(player_t *player, inventoryitemid_t ID);

// Obtain an item effect definition for its inventory item ID
itemeffect_t *E_EffectForInventoryItemID(inventoryitemid_t id);

// Obtain an item effect definition for a player's inventory index
itemeffect_t *E_EffectForInventoryIndex(player_t *player, inventoryindex_t idx);

// Get the slot being used for a particular inventory item, by ID, if one
// exists. Returns NULL if the item isn't in the player's inventory.
inventoryslot_t *E_InventorySlotForItemID(const player_t *player, inventoryitemid_t id);

// Get the slot being used for a particular inventory item, by item pointer, if
// one exists. Returns NULL if the item isn't in the player's inventory.
inventoryslot_t *E_InventorySlotForItem(const player_t *player,
                                        const itemeffect_t *effect);

// Get the slot being used for a particular inventory item, by name, if one 
// exists. Returns NULL if the item isn't in the player's inventory.
inventoryslot_t *E_InventorySlotForItemName(const player_t *player,
                                            const char *name);

// Special function to test for player backpack.
bool E_PlayerHasBackpack(const player_t *player);

// Special function to give the player a backpack.
bool E_GiveBackpack(player_t *player);

// Special function to remove backpack.
bool E_RemoveBackpack(player_t *player);

// Lookup the maximum amount a player can carry of a specific artifact type.
int E_GetMaxAmountForArtifact(const player_t *player,
                              const itemeffect_t *artifact);

// Get amount of an item owned for a specific artifact type
int E_GetItemOwnedAmount(const player_t *player, const itemeffect_t *artifact);

// Get amount of an item owned by name
int E_GetItemOwnedAmountName(const player_t *player, const char *name);

// Place an item into a player's inventory. 
bool E_GiveInventoryItem(player_t *player, const itemeffect_t *artifact, int amount = -1);

e_pickupfx_t *E_PickupFXForName(const char *name);
e_pickupfx_t *E_PickupFXForSprNum(spritenum_t sprnum);

pickupflags_e E_PickupFlagsForStr(const char *flagstr);

// return value enumeration for E_RemoveInventoryItem
enum itemremoved_e
{
   INV_NOTREMOVED, // could not remove
   INV_REMOVED,    // removed requested number but left slot
   INV_REMOVEDSLOT // removed number AND slot
};

// Remove an item from a player's inventory.
itemremoved_e E_RemoveInventoryItem(player_t *player, const itemeffect_t *artifact, int amount);

// Call at the end of a hub, or a level that isn't part of a hub, to clear
// out items that don't persist.
void E_InventoryEndHub(player_t *player);

// Call to completely clear a player's inventory.
void E_ClearInventory(player_t *player);

// Get allocated size of player inventory arrays
int E_GetInventoryAllocSize();

// IOANCH: needed by bot
void E_GetInventoryItemDetails(const itemeffect_t *artifact,
                               itemeffecttype_t &fxtype,
                               inventoryitemid_t &itemid,
                               int &amountToGive,
                               int &fullAmount);

//
// EDF-Only Definitions
//

#ifdef NEED_EDF_DEFINITIONS

// Section Names
#define EDF_SEC_HEALTHFX "healtheffect"
#define EDF_SEC_ARMORFX  "armoreffect"
#define EDF_SEC_AMMOFX   "ammoeffect"
#define EDF_SEC_POWERFX  "powereffect"
#define EDF_SEC_WEAPGFX  "weapongiver"
#define EDF_SEC_ARTIFACT "artifact"
#define EDF_SEC_SPRPKUP  "pickupitem"
#define EDF_SEC_PICKUPFX "pickupeffect"
#define EDF_SEC_LOCKDEF  "lockdef"

// Section Defs
extern cfg_opt_t edf_healthfx_opts[];
extern cfg_opt_t edf_armorfx_opts[];
extern cfg_opt_t edf_ammofx_opts[];
extern cfg_opt_t edf_powerfx_opts[];
extern cfg_opt_t edf_weapgfx_opts[];
extern cfg_opt_t edf_artifact_opts[];
extern cfg_opt_t edf_sprpkup_opts[];
extern cfg_opt_t edf_pkupfx_opts[];
extern cfg_opt_t edf_lockdef_opts[];

// Functions
void E_ProcessPickups(cfg_t *cfg);
void E_ProcessInventory(cfg_t *cfg);

#endif

#endif

// EOF

