// Emacs style mode select   -*- C++ -*- 
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:  
//    EDF Inventory
//
//-----------------------------------------------------------------------------

#ifndef E_INVENTORY_H__
#define E_INVENTORY_H__

#include "m_dllist.h"

class MetaTable;

// Inventory flags
enum
{
   INVF_AUTOACTIVATE    = 0x00000001, // Activates immediately when acquired
   INVF_UNDROPPABLE     = 0x00000002, // Can't be dropped
   INVF_INVBAR          = 0x00000004, // Is displayed in inventory bar
   INVF_HUBPOWER        = 0x00000008, // Power lasts for current cluster
   INVF_PERSISTENTPOWER = 0x00000010, // Power is kept between levels
   INVF_ALWAYSPICKUP    = 0x00000020, // Item is picked up even if unneeded
   INVF_PICKUPIFANY     = 0x00000040, // Item picked up if any one effect passes
   INVF_KEEPDEPLETED    = 0x00000080, // Item remains in inventory with amount 0
   INVF_ADDITIVETIME    = 0x00000100, // Power timer is increased w/each collect
   INVF_UNTOSSABLE      = 0x00000200, // Can't be removed even w/console command
   INVF_LOUDPICKUPSOUND = 0x00000400  // Pickup sound is played with ATTN_NONE

   // Possible TODOs:
   // INV_BIGPOWERUP  - redundant with flags value
   // INV_IGNORESKILL - would rather have more flexible skill props...
};

// Inventory classes
enum
{
   INV_CLASS_NONE,       // A token, functionless inventory item
   INV_CLASS_HEALTH,     // Collectable health powerup
   INV_CLASS_NUMCLASSES
};

//
// inventory_t
//
// This is the primary inventory definition structure.
//
struct inventory_t
{
   // hash links
   DLListItem<inventory_t> namelinks;
   DLListItem<inventory_t> compatlinks;
   DLListItem<inventory_t> numlinks;

   // basic properties
   int   amount;         // amount given on pickup
   int   maxAmount;      // maximum amount that can be carried
   int   interHubAmount; // amount that persists between hubs or non-hub levels
   char *icon;           // name of icon patch graphic
   char *pickUpMessage;  // message given when picked up (BEX if starts with $)
   char *pickUpSound;    // name of pickup sound, if any
   char *pickUpFlash;    // name of pickup flash actor type
   char *useSound;       // name of use sound, if any
   int   respawnTics;    // length of time it takes to respawn w/item respawn on
   int   giveQuest;      // quest flag # given, if non-zero

   unsigned int flags;   // basic inventory flags

   // fields needed for EDF identification and hashing
   char *name;           // buffer for name   
   int   numkey;         // ID number
   int   compatNum;      // for pickupitem compatibility
   inventory_t *parent;  // parent record for inheritance
   bool  processed;      // if true, has been processed
   MetaTable *meta;      // metatable
};

// Lookup functions
inventory_t *E_InventoryForID(int idnum);
inventory_t *E_GetInventoryForID(int idnum);
inventory_t *E_InventoryForName(const char *name);
inventory_t *E_GetInventoryForName(const char *name);
inventory_t *E_InventoryForCompatNum(int pickupeffect);

#ifdef NEED_EDF_DEFINITIONS

#define EDF_SEC_INVENTORY "inventory"
#define EDF_SEC_INVDELTA  "inventorydelta"

extern cfg_opt_t edf_inv_opts[];
extern cfg_opt_t edf_invdelta_opts[];

void E_CollectInventory(cfg_t *cfg);
void E_ProcessInventoryDefs(cfg_t *cfg);
void E_ProcessInventoryDeltas(cfg_t *cfg);

// Sprite pick-up effects (compatibility only)
#define SEC_PICKUPFX  "pickupitem"
#define ITEM_PICKUPFX "effect"
void E_ProcessItems(cfg_t *cfg);

#endif

// Old stuff, strictly for compatibility

// pickup effects enumeration

enum
{
   PFX_NONE,
   PFX_GREENARMOR,
   PFX_BLUEARMOR,
   PFX_POTION,
   PFX_ARMORBONUS,
   PFX_SOULSPHERE,
   PFX_MEGASPHERE,
   PFX_BLUEKEY,
   PFX_YELLOWKEY,
   PFX_REDKEY,
   PFX_BLUESKULL,
   PFX_YELLOWSKULL,
   PFX_REDSKULL,
   PFX_STIMPACK,
   PFX_MEDIKIT,
   PFX_INVULNSPHERE,
   PFX_BERZERKBOX,
   PFX_INVISISPHERE,
   PFX_RADSUIT,
   PFX_ALLMAP,
   PFX_LIGHTAMP,
   PFX_CLIP,
   PFX_CLIPBOX,
   PFX_ROCKET,
   PFX_ROCKETBOX,
   PFX_CELL,
   PFX_CELLPACK,
   PFX_SHELL,
   PFX_SHELLBOX,
   PFX_BACKPACK,
   PFX_BFG,
   PFX_CHAINGUN,
   PFX_CHAINSAW,
   PFX_LAUNCHER,
   PFX_PLASMA,
   PFX_SHOTGUN,
   PFX_SSG,
   PFX_HGREENKEY,
   PFX_HBLUEKEY,
   PFX_HYELLOWKEY,
   PFX_HPOTION,
   PFX_SILVERSHIELD,
   PFX_ENCHANTEDSHIELD,
   PFX_BAGOFHOLDING,
   PFX_HMAP,
   PFX_GWNDWIMPY,
   PFX_GWNDHEFTY,
   PFX_MACEWIMPY,
   PFX_MACEHEFTY,
   PFX_CBOWWIMPY,
   PFX_CBOWHEFTY,
   PFX_BLSRWIMPY,
   PFX_BLSRHEFTY,
   PFX_PHRDWIMPY,
   PFX_PHRDHEFTY,
   PFX_SKRDWIMPY,
   PFX_SKRDHEFTY,
   PFX_TOTALINVIS,
   PFX_NUMFX
};

extern inventory_t **pickupfx;

#endif

// EOF

