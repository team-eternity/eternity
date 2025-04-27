//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Max Waine, et al.
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
// Purpose: EDF inventory.
// Authors: James Haley, Max Waine
//

#ifndef E_INVENTORY_H__
#define E_INVENTORY_H__

// Basic inventory definitions are now in d_player.h, so that the entire engine
// doesn't rebuild if you modify this header.
#include "d_player.h"
#include "m_collection.h"

class MetaKeyIndex;
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
using itemeffecttype_t = int;

// Artifact sub-types
enum
{
    ARTI_NORMAL, // an ordinary artifact
    ARTI_AMMO,   // ammo type
    ARTI_KEY,    // key
    ARTI_PUZZLE, // puzzle item
    ARTI_POWER,  // powerup token
    ARTI_WEAPON, // weapon token
    ARTI_QUEST,  // quest token
    NUMARTITYPES
};
using artitype_t = int;

// Autouse health types
enum class AutoUseHealthMode : int
{
    none,
    heretic,
    // TODO: add this when we support strife
    //   strife,
    MAX
};

// Hard-coded names for specially treated items (needed in DeHackEd, etc.)
constexpr const char ITEMNAME_BERSERKHEALTH[] = "BerserkHealth"; // The health
constexpr const char ITEMNAME_HEALTHBONUS[]   = "HealthBonus";
constexpr const char ITEMNAME_MEDIKIT[]       = "Medikit";
constexpr const char ITEMNAME_MEGASPHERE[]    = "MegasphereHealth";
constexpr const char ITEMNAME_SOULSPHERE[]    = "Soulsphere";
constexpr const char ITEMNAME_STIMPACK[]      = "Stimpack";
constexpr const char ITEMNAME_GREENARMOR[]    = "GreenArmor";
constexpr const char ITEMNAME_BLUEARMOR[]     = "BlueArmor";
constexpr const char ITEMNAME_IDFAARMOR[]     = "IDFAArmor";
constexpr const char ITEMNAME_ARMORBONUS[]    = "ArmorBonus";
constexpr const char ITEMNAME_RAMBOARMOR[]    = "RAMBOArmor";

// Hard-coded names for specially treated artifact types
constexpr const char ARTI_BACKPACKITEM[] = "BackpackItem";
constexpr const char ARTI_BLUECARD[]     = "BlueCard";
constexpr const char ARTI_BLUESKULL[]    = "BlueSkull";
constexpr const char ARTI_REDCARD[]      = "RedCard";
constexpr const char ARTI_REDSKULL[]     = "RedSkull";
constexpr const char ARTI_YELLOWCARD[]   = "YellowCard";
constexpr const char ARTI_YELLOWSKULL[]  = "YellowSkull";
constexpr const char ARTI_KEYGREEN[]     = "KeyGreen";
constexpr const char ARTI_KEYYELLOW[]    = "KeyYellow";
constexpr const char ARTI_KEYBLUE[]      = "KeyBlue";

// Hard-coded names for the (currently) 11 specially treated powers in powertype_t.
// These are listed in ascending order of enumeration, as opposed to alphabetical.
constexpr const char POWER_INVULNERABLE[] = "PowerInvulnerable";        // pw_invulnerability
constexpr const char POWER_STRENGTH[]     = "PowerStrength";            // pw_strength (Berserk)
constexpr const char POWER_PARTIALINVIS[] = "PowerPartialInvisibility"; // pw_invisibility
constexpr const char POWER_IRONFEET[]     = "PowerIronFeet";            // pw_ironfeet (Env. protection)
constexpr const char POWER_ALLMAP[]       = "PowerAllmap";              // pw_allmap
constexpr const char POWER_INFRARED[]     = "PowerLightAmp";            // pw_infrared (Fullbright)
constexpr const char POWER_TOTALINVIS[]   = "PowerTotalInvisibility";   // pw_totalinvis
constexpr const char POWER_GHOST[]        = "PowerGhost";               // pw_ghost
constexpr const char POWER_SILENT[]       = "PowerSilent";              // pw_silencer
constexpr const char POWER_FLIGHT[]       = "PowerFlight";              // pw_flight
constexpr const char POWER_TORCH[]        = "PowerTorch";               // pw_torch (Fullbright w/ flicker)
constexpr const char POWER_WEAPONLEVEL2[] = "PowerWeaponLevel2";        // pw_weaponlevel2

extern const char *powerStrings[NUMPOWERS];

//
// Item Effect
//
// An item effect is a MetaTable. The properties in the table depend on the type
// of section that instantiated the effect (and therefore what its purpose is).
//
using itemeffect_t = MetaTable;

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
    PFXF_COMMERCIALONLY    = 0x00000010, // can only be picked up in commercial gamemodes
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

//
// Autouse health items
//
struct e_autouseid_t
{
    int                 amount;      // heal amount
    unsigned            restriction; // restriction, if any
    const itemeffect_t *artifact;    // the inventory artifact effect
};

//
// Functions
//

// Get an item effect type number by name
itemeffecttype_t E_EffectTypeForName(const char *name);

// Remove an item effect from the table.
void E_SafeDeleteItemEffect(itemeffect_t *effect);

// Find an item effect by name
itemeffect_t *E_ItemEffectForName(const char *name);

// Get the item effects table
MetaTable *E_GetItemEffects();

// Get the number of ammo type artifacts defined.
size_t E_GetNumAmmoTypes();

// enum for E_GiveAllAmmo behavior
enum giveallammo_e
{
    GAA_MAXAMOUNT, // give max amount
    GAA_CUSTOM     // use amount argument
};

// Give the player a certain amount of all ammo types
void E_GiveAllAmmo(player_t &player, giveallammo_e op, int amount = -1);

// Get an ammo type for its index in the fast lookup table.
itemeffect_t *E_AmmoTypeForIndex(size_t idx);

// Get the number of key type artifacts defined.
size_t E_GetNumKeyItems();

// Get a key item for its index in the fast lookup table.
itemeffect_t *E_KeyItemForIndex(size_t idx);

// Give all "key" type artifacts to a player
int E_GiveAllKeys(player_t &player);

// Take all "key" type artifacts from a player
int E_TakeAllKeys(const player_t &player);

// Upon morphing a player, move all original class weapons to a secondary inventory
void E_StashOriginalMorphWeapons(player_t &player);

// After returning to normal form, restore all weapons. Delete existing weapons
void E_UnstashWeaponsForUnmorphing(player_t &player);

// Check if a player is able to unlock a lock, by its lock ID.
bool E_PlayerCanUnlock(const player_t &player, int lockID, bool remote);

// Get the automap color for a lockdef
int E_GetLockDefColor(int lockID);

// Tries to move the inventory cursor 'amount' right
bool E_MoveInventoryCursor(const player_t &player, const int amount, int &cursor);

// Checks if inventory cursor can be moved 'amount' right without mutating cursor
bool E_CanMoveInventoryCursor(const player_t *player, const int amount, const int cursor);

// Says if a player possesses at least one item w/ +invbar
bool E_PlayerHasVisibleInvItem(const player_t &player);

// Tries to use the currently selected item.
void E_TryUseItem(player_t &player, inventoryitemid_t ID);

// Obtain an item effect definition for its inventory item ID
itemeffect_t *E_EffectForInventoryItemID(inventoryitemid_t id);

// Obtain an item effect definition for a player's inventory index
itemeffect_t *E_EffectForInventoryIndex(const player_t &player, inventoryindex_t idx);

// Get the slot being used for a particular inventory item, by ID, if one
// exists. Returns nullptr if the item isn't in the player's inventory.
inventoryslot_t *E_InventorySlotForItemID(const player_t &player, inventoryitemid_t id);

// Get the slot being used for a particular inventory item, by item pointer, if
// one exists. Returns nullptr if the item isn't in the player's inventory.
inventoryslot_t *E_InventorySlotForItem(const player_t &player, const itemeffect_t *effect);

// Get the slot being used for a particular inventory item, by name, if one
// exists. Returns nullptr if the item isn't in the player's inventory.
inventoryslot_t *E_InventorySlotForItemName(const player_t &player, const char *name);

// Returns the item ID for a given item effect name. Returns -1 if not found.
int E_ItemIDForName(const char *name);

// Special function to test for player backpack.
bool E_PlayerHasBackpack(const player_t &player);

// Special function to give the player a backpack.
bool E_GiveBackpack(player_t &player);

// Special function to remove backpack.
bool E_RemoveBackpack(const player_t &player);

// Lookup the maximum amount a player can carry of a specific artifact type.
int E_GetMaxAmountForArtifact(const player_t &player, const itemeffect_t *artifact);

// Get amount of an item owned for a specific artifact type
int E_GetItemOwnedAmount(const player_t &player, const itemeffect_t *artifact);

// Get amount of an item owned by name
int E_GetItemOwnedAmountName(const player_t &player, const char *name);

bool E_PlayerHasPowerName(const player_t &player, const char *name);

// Place an item into a player's inventory.
bool E_GiveInventoryItem(player_t &player, const itemeffect_t *artifact, int amount = -1);

e_pickupfx_t *E_PickupFXForName(const char *name);
e_pickupfx_t *E_PickupFXForSprNum(spritenum_t sprnum);

pickupflags_e E_PickupFlagsForStr(const char *flagstr);

const PODCollection<e_autouseid_t> &E_GetAutouseList();

// return value enumeration for E_RemoveInventoryItem
enum itemremoved_e
{
    INV_NOTREMOVED, // could not remove
    INV_REMOVED,    // removed requested number but left slot
    INV_REMOVEDSLOT // removed number AND slot
};

enum autousehealthrestrict_flags : unsigned
{
    AHR_BABY       = 1,
    AHR_DEATHMATCH = 2,
};

// Remove an item from a player's inventory.
itemremoved_e E_RemoveInventoryItem(const player_t &player, const itemeffect_t *artifact, int amount);

// Call at the end of a hub, or a level that isn't part of a hub, to clear
// out items that don't persist.
void E_InventoryEndHub(const player_t *player);

// Call to completely clear a player's inventory.
void E_ClearInventory(player_t *player);

// Get allocated size of player inventory arrays
int E_GetInventoryAllocSize();

int E_GetPClassHealth(const itemeffect_t &effect, size_t keyIndex, const playerclass_t &pclass, int def);
int E_GetPClassHealth(const itemeffect_t &effect, const char *key, const playerclass_t &pclass, int def);

extern MetaKeyIndex keyAmount;
extern MetaKeyIndex keyBackpackAmount;
extern MetaKeyIndex keyClass;
extern MetaKeyIndex keyClassName;
extern MetaKeyIndex keyItemID;
extern MetaKeyIndex keyMaxAmount;
extern MetaKeyIndex keyBackpackMaxAmt;
extern MetaKeyIndex keyInvBar;
extern MetaKeyIndex keyAmmoGiven;
extern MetaKeyIndex keyAutouseHealthRestrict;
extern MetaKeyIndex keyAutouseHealthMode;

//
// EDF-Only Definitions
//

#ifdef NEED_EDF_DEFINITIONS

// Section Names
constexpr const char EDF_SEC_HEALTHFX[] = "healtheffect";
constexpr const char EDF_SEC_ARMORFX[]  = "armoreffect";
constexpr const char EDF_SEC_AMMOFX[]   = "ammoeffect";
constexpr const char EDF_SEC_POWERFX[]  = "powereffect";
constexpr const char EDF_SEC_WEAPGFX[]  = "weapongiver";
constexpr const char EDF_SEC_ARTIFACT[] = "artifact";

constexpr const char EDF_SEC_HEALTHFXDELTA[] = "healthdelta";
constexpr const char EDF_SEC_ARMORFXDELTA[]  = "armordelta";
constexpr const char EDF_SEC_AMMOFXDELTA[]   = "ammodelta";
constexpr const char EDF_SEC_POWERFXDELTA[]  = "powerdelta";
constexpr const char EDF_SEC_WEAPGFXDELTA[]  = "weapongiverdelta";
constexpr const char EDF_SEC_ARTIFACTDELTA[] = "artifactdelta";

constexpr const char EDF_SEC_SPRPKUP[]  = "pickupitem";
constexpr const char EDF_SEC_PICKUPFX[] = "pickupeffect";
constexpr const char EDF_SEC_LOCKDEF[]  = "lockdef";

// Section Defs
extern cfg_opt_t edf_healthfx_opts[];
extern cfg_opt_t edf_armorfx_opts[];
extern cfg_opt_t edf_ammofx_opts[];
extern cfg_opt_t edf_powerfx_opts[];
extern cfg_opt_t edf_weapgfx_opts[];
extern cfg_opt_t edf_artifact_opts[];

extern cfg_opt_t edf_healthfx_delta_opts[];
extern cfg_opt_t edf_armorfx_delta_opts[];
extern cfg_opt_t edf_ammofx_delta_opts[];
extern cfg_opt_t edf_powerfx_delta_opts[];
extern cfg_opt_t edf_weapgfx_delta_opts[];
extern cfg_opt_t edf_artifact_delta_opts[];

extern cfg_opt_t edf_sprpkup_opts[];
extern cfg_opt_t edf_pkupfx_opts[];
extern cfg_opt_t edf_lockdef_opts[];

// Functions
void E_ProcessPickups(cfg_t *cfg);
void E_ProcessInventory(cfg_t *cfg);

#endif

#endif

// EOF

