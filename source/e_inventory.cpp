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

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"
#include "i_system.h"

#include "Confuse/confuse.h"
#include "e_edf.h"
#include "e_hash.h"
#include "e_inventory.h"
#include "e_lib.h"
#include "e_sprite.h"

#include "d_player.h"
#include "doomstat.h"
#include "info.h"
#include "m_collection.h"
#include "metaapi.h"

//=============================================================================
//
// Effect Classes
//

// Item Effect Type names
static const char *e_ItemEffectTypeNames[NUMITEMFX] =
{
   "None",
   "Health",
   "Armor",
   "Power",
   "Artifact"
};

//
// E_EffectTypeForName
//
// Look up an effect type index based on its name. ITEMFX_NONE is returned
// if an invalid name is provided.
//
itemeffecttype_t E_EffectTypeForName(const char *name)
{
   itemeffecttype_t fx;
   
   if((fx = E_StrToNumLinear(e_ItemEffectTypeNames, NUMITEMFX, name)) == NUMITEMFX)
      fx = ITEMFX_NONE;

   return fx;
}

//=============================================================================
//
// Effects
//
// An effect applies when an item is collected, used, or dropped.
//

// The effects table contains as properties metatables for each effect.
static MetaTable e_effectsTable;

//
// E_addItemEffect
//
// Add an item effect from a cfg_t definition.
//
static itemeffect_t *E_addItemEffect(cfg_t *cfg)
{
   itemeffect_t *table;
   const char   *name = cfg_title(cfg);

   if(!(table = E_ItemEffectForName(name)))
      e_effectsTable.addObject((table = new itemeffect_t(name)));

   E_MetaTableFromCfg(cfg, table);

   return table;
}

//
// E_ItemEffectForName
//
// Find an item effect by name.
//
itemeffect_t *E_ItemEffectForName(const char *name)
{
   return runtime_cast<itemeffect_t *>(e_effectsTable.getObject(name));
}

//
// E_GetItemEffects
//
// Get the whole effects table, for the few places it is needed externally
// (mainly for console debugging features).
//
MetaTable *E_GetItemEffects()
{
   return &e_effectsTable;
}

//=============================================================================
//
// Effect Processing
//

// metakey vocabulary
#define KEY_ADDITIVETIME   "additivetime"
#define KEY_ALWAYSPICKUP   "alwayspickup"
#define KEY_AMOUNT         "amount"
#define KEY_ARTIFACTTYPE   "artifacttype"
#define KEY_BONUS          "bonus"
#define KEY_CLASS          "class"
#define KEY_CLASSNAME      "classname"
#define KEY_DURATION       "duration"
#define KEY_FULLAMOUNTONLY "fullamountonly"
#define KEY_ICON           "icon"
#define KEY_INTERHUBAMOUNT "interhubamount"
#define KEY_INVBAR         "invbar"
#define KEY_ITEMID         "itemid"
#define KEY_KEEPDEPLETED   "keepdepleted"
#define KEY_LOWMESSAGE     "lowmessage"
#define KEY_MAXAMOUNT      "maxamount"
#define KEY_MAXSAVEAMOUNT  "maxsaveamount"
#define KEY_SAVEAMOUNT     "saveamount"
#define KEY_SAVEDIVISOR    "savedivisor"
#define KEY_SAVEFACTOR     "savefactor"
#define KEY_SORTORDER      "sortorder"
#define KEY_TYPE           "type"
#define KEY_UNDROPPABLE    "undroppable"
#define KEY_USEEFFECT      "useeffect"
#define KEY_USESOUND       "usesound"

// Interned metatable keys
static MetaKeyIndex keyAmount        (KEY_AMOUNT        );
static MetaKeyIndex keyArtifactType  (KEY_ARTIFACTTYPE  );
static MetaKeyIndex keyClass         (KEY_CLASS         );
static MetaKeyIndex keyClassName     (KEY_CLASSNAME     );
static MetaKeyIndex keyFullAmountOnly(KEY_FULLAMOUNTONLY);
static MetaKeyIndex keyItemID        (KEY_ITEMID        );
static MetaKeyIndex keyKeepDepleted  (KEY_KEEPDEPLETED  );
static MetaKeyIndex keyMaxAmount     (KEY_MAXAMOUNT     );
static MetaKeyIndex keySortOrder     (KEY_SORTORDER     );

// Health fields
cfg_opt_t edf_healthfx_opts[] =
{
   CFG_INT(KEY_AMOUNT,     0,  CFGF_NONE), // amount to recover
   CFG_INT(KEY_MAXAMOUNT,  0,  CFGF_NONE), // max that can be recovered
   CFG_STR(KEY_LOWMESSAGE, "", CFGF_NONE), // message if health < amount
   
   CFG_FLAG(KEY_ALWAYSPICKUP, 0,  CFGF_SIGNPREFIX), // if +, always pick up
   
   CFG_END()
};

// Armor fields
cfg_opt_t edf_armorfx_opts[] =
{
   CFG_INT(KEY_SAVEAMOUNT,    0,  CFGF_NONE), // amount of armor given
   CFG_INT(KEY_SAVEFACTOR,    1,  CFGF_NONE), // numerator of save percentage
   CFG_INT(KEY_SAVEDIVISOR,   3,  CFGF_NONE), // denominator of save percentage
   CFG_INT(KEY_MAXSAVEAMOUNT, 0,  CFGF_NONE), // max save amount, for bonuses
   
   CFG_FLAG(KEY_ALWAYSPICKUP, 0, CFGF_SIGNPREFIX), // if +, always pick up
   CFG_FLAG(KEY_BONUS,        0, CFGF_SIGNPREFIX), // if +, is a bonus (adds to current armor type)

   CFG_END()
};

// Powerup fields
cfg_opt_t edf_powerfx_opts[] =
{
   CFG_INT(KEY_DURATION,  -1, CFGF_NONE), // length of time to last
   CFG_STR(KEY_TYPE,      "", CFGF_NONE), // name of powerup effect to give

   CFG_FLAG(KEY_ADDITIVETIME, 0, CFGF_SIGNPREFIX), // if +, adds to current duration

   // TODO: support HUBPOWER and PERSISTENTPOWER properties, etc.

   CFG_END()
};

// Artifact subtype names
static const char *artiTypeNames[NUMARTITYPES] =
{
   "NORMAL",   // an ordinary artifact
   "AMMO",     // ammo type
   "BACKPACK", // backpack token
   "PUZZLE",   // puzzle item
   "POWER",    // powerup token
   "WEAPON",   // weapon token
   "QUEST"     // quest token
};

//
// E_artiTypeCB
//
// Value parsing callback for artifact type
//
static int E_artiTypeCB(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result)
{
   int res;

   if((res = E_StrToNumLinear(artiTypeNames, NUMARTITYPES, value)) == NUMARTITYPES)
      res = ARTI_NORMAL;

   *(int *)result = res;

   return 0;
}

// Artifact fields
cfg_opt_t edf_artifact_opts[] =
{
   CFG_INT(KEY_AMOUNT,         0,  CFGF_NONE), // amount gained with one pickup
   CFG_INT(KEY_MAXAMOUNT,      0,  CFGF_NONE), // max amount that can be carried in inventory
   CFG_INT(KEY_INTERHUBAMOUNT, 0,  CFGF_NONE), // amount carryable between hubs (or levels)
   CFG_INT(KEY_SORTORDER,      0,  CFGF_NONE), // relative ordering within inventory
   CFG_STR(KEY_ICON,           "", CFGF_NONE), // icon used on inventory bars
   CFG_STR(KEY_USESOUND,       "", CFGF_NONE), // sound to play when used
   CFG_STR(KEY_USEEFFECT,      "", CFGF_NONE), // effect to activate when used

   CFG_FLAG(KEY_UNDROPPABLE,    0, CFGF_SIGNPREFIX), // if +, cannot be dropped
   CFG_FLAG(KEY_INVBAR,         0, CFGF_SIGNPREFIX), // if +, appears in inventory bar
   CFG_FLAG(KEY_KEEPDEPLETED,   0, CFGF_SIGNPREFIX), // if +, remains in inventory if amount is 0
   CFG_FLAG(KEY_FULLAMOUNTONLY, 0, CFGF_SIGNPREFIX), // if +, pick up for full amount only

   CFG_INT_CB(KEY_ARTIFACTTYPE, ARTI_NORMAL, CFGF_NONE, E_artiTypeCB), // artifact sub-type
   
   CFG_END()
};

static const char *e_ItemSectionNames[NUMITEMFX] =
{
   "",
   EDF_SEC_HEALTHFX,
   EDF_SEC_ARMORFX,
   EDF_SEC_POWERFX,
   EDF_SEC_ARTIFACT
};

//
// E_processItemEffects
//
static void E_processItemEffects(cfg_t *cfg)
{
   // process each type of effect section
   for(int i = ITEMFX_HEALTH; i < NUMITEMFX; i++)
   {
      const char   *cfgSecName  = e_ItemSectionNames[i];
      const char   *className   = e_ItemEffectTypeNames[i];
      unsigned int  numSections = cfg_size(cfg, cfgSecName);

      E_EDFLogPrintf("\t* Processing %s item effects (%u defined)\n", 
                     className, numSections);

      // process each section of the current type
      for(unsigned int secNum = 0; secNum < numSections; secNum++)
      {
         auto newEffect = E_addItemEffect(cfg_getnsec(cfg, cfgSecName, secNum));

         // add the item effect type and name as properties
         newEffect->setInt(keyClass, i);
         newEffect->setConstString(keyClassName, className);

         E_EDFLogPrintf("\t\t* Processed item '%s'\n", newEffect->getKey());
      }
   }
}

//=============================================================================
//
// Effect Bindings
//
// Effects can be bound to sprites.
//

// Sprite pick-up effects
#define ITEM_PICKUPFX "effect"

// sprite-based pickup items
cfg_opt_t edf_pickup_opts[] =
{
   CFG_STR(ITEM_PICKUPFX, "PFX_NONE", CFGF_NONE),
   CFG_END()
};

// pickup variables

// pickup effect names (these are currently searched linearly)
// matching enum values are defined in e_edf.h

// INVENTORY_FIXME: make this a "compatibility name" for the effect
const char *pickupnames[PFX_NUMFX] =
{
   "PFX_NONE",
   "PFX_GREENARMOR",
   "PFX_BLUEARMOR",
   "PFX_POTION",
   "PFX_ARMORBONUS",
   "PFX_SOULSPHERE",
   "PFX_MEGASPHERE",
   "PFX_BLUEKEY",
   "PFX_YELLOWKEY",
   "PFX_REDKEY",
   "PFX_BLUESKULL",
   "PFX_YELLOWSKULL",
   "PFX_REDSKULL",
   "PFX_STIMPACK",
   "PFX_MEDIKIT",
   "PFX_INVULNSPHERE",
   "PFX_BERZERKBOX",
   "PFX_INVISISPHERE",
   "PFX_RADSUIT",
   "PFX_ALLMAP",
   "PFX_LIGHTAMP",
   "PFX_CLIP",
   "PFX_CLIPBOX",
   "PFX_ROCKET",
   "PFX_ROCKETBOX",
   "PFX_CELL",
   "PFX_CELLPACK",
   "PFX_SHELL",
   "PFX_SHELLBOX",
   "PFX_BACKPACK",
   "PFX_BFG",
   "PFX_CHAINGUN",
   "PFX_CHAINSAW",
   "PFX_LAUNCHER",
   "PFX_PLASMA",
   "PFX_SHOTGUN",
   "PFX_SSG",
   "PFX_HGREENKEY",
   "PFX_HBLUEKEY",
   "PFX_HYELLOWKEY",
   "PFX_HPOTION",
   "PFX_SILVERSHIELD",
   "PFX_ENCHANTEDSHIELD",
   "PFX_BAGOFHOLDING",
   "PFX_HMAP",
   "PFX_GWNDWIMPY",
   "PFX_GWNDHEFTY",
   "PFX_MACEWIMPY",
   "PFX_MACEHEFTY",
   "PFX_CBOWWIMPY",
   "PFX_CBOWHEFTY",
   "PFX_BLSRWIMPY",
   "PFX_BLSRHEFTY",
   "PFX_PHRDWIMPY",
   "PFX_PHRDHEFTY",
   "PFX_SKRDWIMPY",
   "PFX_SKRDHEFTY",
   "PFX_TOTALINVIS",
};

// pickupfx lookup table used in P_TouchSpecialThing (is allocated
// with size NUMSPRITES)
int *pickupfx = NULL;

//
// E_processPickupItems
//
// Allocates the pickupfx array used in P_TouchSpecialThing,
// and loads all pickupitem definitions, using the sprite hash
// table to resolve what sprite owns the specified effect.
//
static void E_processPickupItems(cfg_t *cfg)
{
   static int oldnumsprites;
   int i, numnew, numpickups;

   E_EDFLogPuts("\t* Processing pickup items\n");

   // allocate and initialize pickup effects array
   // haleyjd 11/21/11: allow multiple runs
   numnew = NUMSPRITES - oldnumsprites;
   if(numnew > 0)
   {
      pickupfx = erealloc(int *, pickupfx, NUMSPRITES * sizeof(int));
      for(i = oldnumsprites; i < NUMSPRITES; i++)
         pickupfx[i] = PFX_NONE;
      oldnumsprites = NUMSPRITES;
   }

   // sanity check
   if(!pickupfx)
      E_EDFLoggedErr(2, "E_ProcessItems: no sprites defined!?\n");
   
   // load pickupfx
   numpickups = cfg_size(cfg, EDF_SEC_PICKUPFX);
   E_EDFLogPrintf("\t\t%d pickup item(s) defined\n", numpickups);
   for(i = 0; i < numpickups; ++i)
   {
      int fxnum, sprnum;
      cfg_t *sec = cfg_getnsec(cfg, EDF_SEC_PICKUPFX, i);
      const char *title = cfg_title(sec);
      const char *pfx = cfg_getstr(sec, ITEM_PICKUPFX);

      // validate the sprite name given in the section title and
      // resolve to a sprite number (hashed)
      sprnum = E_SpriteNumForName(title);

      if(sprnum == -1)
      {
         // haleyjd 05/31/06: downgraded to warning, substitute blanksprite
         E_EDFLoggedWarning(2,
            "Warning: invalid sprite mnemonic for pickup item: '%s'\n",
            title);
         sprnum = blankSpriteNum;
      }

      // find the proper pickup effect number (linear search)
      fxnum = E_StrToNumLinear(pickupnames, PFX_NUMFX, pfx);
      if(fxnum == PFX_NUMFX)
      {
         E_EDFLoggedErr(2, "E_ProcessItems: invalid pickup effect: '%s'\n", pfx);
      }
      
      E_EDFLogPrintf("\t\tSet sprite %s(#%d) to pickup effect %s(#%d)\n",
                     title, sprnum, pfx, fxnum);

      pickupfx[sprnum] = fxnum;
   }
}

//=============================================================================
//
// Inventory Items
//
// Inventory items represent a holdable item that can take up a slot in an 
// inventory.
//

// Lookup table of inventory item types by assigned ID number
static PODCollection<itemeffect_t *> e_InventoryItemsByID;
static inventoryitemid_t e_maxitemid;

//
// E_allocateInventoryItemIDs
//
// First, if the item ID table has already been built, we need to wipe it out.
// Then, regardless, build the artifact ID table by scanning the effects table
// for items of classes which enter the inventory, and assign each one a unique
// artifact ID. The ID will be added to the object.
//
static void E_allocateInventoryItemIDs()
{
   itemeffect_t *item = NULL;

   // empty the table if it was already created before
   e_InventoryItemsByID.clear();
   e_maxitemid = 0;

   // scan the effects table and add artifacts to the table
   while((item = runtime_cast<itemeffect_t *>(e_effectsTable.tableIterator(item))))
   {
      itemeffecttype_t fxtype = item->getInt(keyClass, ITEMFX_NONE);
      
      // only interested in effects that are recorded in the inventory
      if(fxtype == ITEMFX_ARTIFACT)
      {
         // add it to the table
         e_InventoryItemsByID.add(item);

         // add the ID to the artifact definition
         item->setInt(keyItemID, e_maxitemid++);
      }
   }
}

//
// E_allocatePlayerInventories
//
// Allocate inventory arrays for the player_t structures.
//
static void E_allocatePlayerInventories()
{
   for(int i = 0; i < MAXPLAYERS; i++)
   {
      if(players[i].inventory)
         efree(players[i].inventory);

      players[i].inventory = estructalloc(inventoryslot_t, e_maxitemid);

      for(inventoryindex_t idx = 0; idx < e_maxitemid; idx++)
         players[i].inventory[idx].item = -1;
   }
}

//
// E_EffectForInventoryItemID
//
// Return the effect definition associated with an inventory item ID.
//
itemeffect_t *E_EffectForInventoryItemID(inventoryitemid_t id)
{
   return (id >= 0 && id < e_maxitemid) ? e_InventoryItemsByID[id] : NULL;
}

//
// E_EffectForInventoryIndex
//
// Get the item effect for a particular index in a given player's inventory.
//
itemeffect_t *E_EffectForInventoryIndex(player_t *player, inventoryindex_t idx)
{
   return (idx >= 0 && idx < e_maxitemid) ? 
      E_EffectForInventoryItemID(player->inventory[idx].item) : NULL;
}

//
// E_InventorySlotForItemID
//
// Find the slot being used by an item in the player's inventory, if one exists.
// NULL is returned if the item is not in the player's inventory.
//
inventoryslot_t *E_InventorySlotForItemID(player_t *player, inventoryitemid_t id)
{
   inventory_t inventory = player->inventory;

   for(inventoryindex_t idx = 0; idx < e_maxitemid; idx++)
   {
      if(inventory[idx].item == id)
         return &inventory[idx];
   }

   return NULL;
}

//
// E_InventorySlotForItem
//
// Find the slot bieng used by an item in the player's inventory, by pointer,
// if one exists. NULL is returned if the item is not in the player's 
// inventory.
//
inventoryslot_t *E_InventorySlotForItem(player_t *player, itemeffect_t *effect)
{
   inventoryitemid_t id;

   if((id = effect->getInt(keyItemID, -1)) >= 0)
      return E_InventorySlotForItemID(player, id);
   else
      return NULL;
}

//
// E_InventorySlotForItemName
//
// Find the slot being used by an item in the player's inventory, by name,
// if one exists. NULL is returned if the item is not in the player's 
// inventory.
//
inventoryslot_t *E_InventorySlotForItemName(player_t *player, const char *name)
{
   itemeffect_t *namedEffect;

   if((namedEffect = E_ItemEffectForName(name)))
      return E_InventorySlotForItem(player, namedEffect);
   else
      return NULL;
}

//
// E_findInventorySlot
//
// Finds the first unused inventory slot index.
//
static inventoryindex_t E_findInventorySlot(inventory_t inventory)
{
   // find the first unused slot and return it
   for(inventoryindex_t idx = 0; idx < e_maxitemid; idx++)
   {
      if(inventory[idx].item == -1)
         return idx;
   }

   // should be unreachable
   return -1;
}

//
// E_sortInventory
//
// After a new slot is added to the inventory, it needs to be placed into its
// proper sorted position based on the item effects' sortorder fields.
//
static void E_sortInventory(player_t *player, inventoryindex_t newIndex, int sortorder)
{
   inventory_t inventory = player->inventory;
   inventoryslot_t tempSlot = inventory[newIndex];

   for(inventoryindex_t idx = 0; idx < newIndex; idx++)
   {
      itemeffect_t *effect;

      if((effect = E_EffectForInventoryIndex(player, idx)))
      {
         int thatorder = effect->getInt(keySortOrder, 0);
         if(thatorder < sortorder)
            continue;
         else
         {
            // shift everything up
            for(inventoryindex_t up = newIndex; up > idx; up--)
               inventory[up] = inventory[up - 1];

            // put the saved slot into its proper place
            inventory[idx] = tempSlot;
            return;
         }
      }
   }
}

//
// E_GiveInventoryItem
//
// Place an artifact effect into the player's inventory, if it will fit.
//
bool E_GiveInventoryItem(player_t *player, itemeffect_t *artifact)
{
   itemeffecttype_t  fxtype = artifact->getInt(keyClass, ITEMFX_NONE);
   inventoryitemid_t itemid = artifact->getInt(keyItemID, -1);

   // Not an artifact??
   if(fxtype != ITEMFX_ARTIFACT || itemid < 0)
      return false;
   
   inventoryindex_t newSlot = -1;
   int amountToGive = artifact->getInt(keyAmount,    0);
   int maxAmount    = artifact->getInt(keyMaxAmount, 0);

   // Does the player already have this item?
   inventoryslot_t *slot = E_InventorySlotForItemID(player, itemid);

   // If not, make a slot for it
   if(!slot)
   {
      if((newSlot = E_findInventorySlot(player->inventory)) < 0)
         return false; // internal error, actually... shouldn't happen
      slot = &player->inventory[newSlot];
   }
   
   // If must collect full amount, but it won't fit, return now.
   if(artifact->getInt(keyFullAmountOnly, 0) && 
      slot->amount + amountToGive > maxAmount)
      return false;

   // set the item type in case the slot is new, and increment the amount owned
   // by the amount this item gives, capping to the maximum allowed amount
   slot->item    = itemid;
   slot->amount += amountToGive;
   if(slot->amount > maxAmount)
      slot->amount = maxAmount;

   // sort if needed
   if(newSlot > 0)
      E_sortInventory(player, newSlot, artifact->getInt(keySortOrder, 0));

   return true;
}

//
// E_removeInventorySlot
//
// Remove a slot from the player's inventory.
//
static void E_removeInventorySlot(player_t *player, inventoryslot_t *slot)
{
   inventory_t inventory = player->inventory;

   for(inventoryindex_t idx = 0; idx < e_maxitemid; idx++)
   {
      if(slot == &inventory[idx])
      {
         // shift everything down
         for(inventoryindex_t down = e_maxitemid - 1; down > idx; down--)
            inventory[down - 1] = inventory[down];

         // clear the top slot
         inventory[e_maxitemid - 1].item   = -1;
         inventory[e_maxitemid - 1].amount =  0;

         return;
      }
   }
}

//
// E_RemoveInventoryItem
//
// Remove some amount of a specific item from the player's inventory, if
// possible.
//
bool E_RemoveInventoryItem(player_t *player, itemeffect_t *artifact, int amount)
{
   inventoryslot_t *slot = E_InventorySlotForItem(player, artifact);

   // don't have that item at all?
   if(!slot)
      return false;

   // don't own that many?
   if(slot->amount < amount)
      return false;

   // subtract the amount requested
   slot->amount -= amount;

   // are they used up?
   if(slot->amount == 0)
   {
      // check for "keep depleted" flag to see if item stays even when we have
      // a zero amount of it.
      if(!artifact->getInt(keyKeepDepleted, 0))
      {
         // otherwise, we need to remove that item and collapse the player's 
         // inventory
         E_removeInventorySlot(player, slot);
      }
   }

   return true;
}

//=============================================================================
//
// Global Processing
//

//
// E_ProcessInventory
//
// Main global function for performing all inventory-related EDF processing.
//
void E_ProcessInventory(cfg_t *cfg)
{
   // process item effects
   E_processItemEffects(cfg);

   // allocate inventory item IDs
   E_allocateInventoryItemIDs();

   // allocate player inventories
   E_allocatePlayerInventories();

   // process pickup item bindings
   E_processPickupItems(cfg);

   // TODO: MOAR
}

// EOF

