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

#include "z_zone.h"
#include "i_system.h"

#include "d_dehtbl.h" // for dehflags parsing
#include "doomtype.h"
#include "info.h"
#include "metaapi.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"
#include "e_edf.h"
#include "e_hash.h"
#include "e_inventory.h"
#include "e_lib.h"
#include "e_metaitems.h"
#include "e_sprite.h"

static unsigned int numInventoryDefs;

// basic inventory flag values and mnemonics

static dehflags_t inventoryflags[] =
{
   { "AUTOACTIVATE",    INVF_AUTOACTIVATE    },
   { "UNDROPPABLE",     INVF_UNDROPPABLE     },
   { "INVBAR",          INVF_INVBAR          },
   { "HUBPOWER",        INVF_HUBPOWER        },
   { "PERSISTENTPOWER", INVF_PERSISTENTPOWER },
   { "ALWAYSPICKUP",    INVF_ALWAYSPICKUP    },
   { "PICKUPIFANY",     INVF_PICKUPIFANY     },
   { "KEEPDEPLETED",    INVF_KEEPDEPLETED    },
   { "ADDITIVETIME",    INVF_ADDITIVETIME    },
   { "UNTOSSABLE",      INVF_UNTOSSABLE      },
   { "LOUDPICKUPSOUND", INVF_LOUDPICKUPSOUND },
   { NULL,              0                    }
};

static dehflagset_t inventory_flagset =
{
   inventoryflags, // flaglist
   0,              // mode
};

static const char *inventoryClassNames[INV_CLASS_NUMCLASSES] =
{
   "None",
   "Health"
};

// Frame section keywords
#define ITEM_INVENTORY_INHERITS       "inherits"
#define ITEM_INVENTORY_CLASS          "class"
#define ITEM_INVENTORY_AMOUNT         "amount"
#define ITEM_INVENTORY_MAXAMOUNT      "maxamount"
#define ITEM_INVENTORY_INTERHUBAMOUNT "interhubamount"
#define ITEM_INVENTORY_ICON           "icon"
#define ITEM_INVENTORY_PICKUPMESSAGE  "pickupmessage"
#define ITEM_INVENTORY_PICKUPSOUND    "pickupsound"
#define ITEM_INVENTORY_PICKUPFLASH    "pickupflash"
#define ITEM_INVENTORY_USESOUND       "usesound"
#define ITEM_INVENTORY_RESPAWNTICS    "respawntics"
#define ITEM_INVENTORY_GIVEQUEST      "givequest"
#define ITEM_INVENTORY_FLAGS          "flags"
#define ITEM_INVENTORY_RESTRICTEDTO   "restrictedto"
#define ITEM_INVENTORY_FORBIDDENTO    "forbiddento"
#define ITEM_INVENTORY_COMPATNAME     "compatname"

// Delta section name field
#define ITEM_DELTA_NAME               "name"

// Class-specific fields

// Health
#define ITEM_HEALTH_AMOUNT            "health.amount"
#define ITEM_HEALTH_MAXAMOUNT         "health.maxamount"
#define ITEM_HEALTH_LOWMESSAGE        "health.lowmessage"
#define ITEM_LOWMSG_VALUE             "value"
#define ITEM_LOWMSG_MESSAGE           "message"

//
// lowmessage mvprop options
//
static cfg_opt_t lowmsg_opts[] =
{
   CFG_INT(ITEM_LOWMSG_VALUE,    0, CFGF_NONE),
   CFG_STR(ITEM_LOWMSG_MESSAGE, "", CFGF_NONE),
   CFG_END()
};

//
// Inventory Options
//

#define INVENTORY_FIELDS \
   CFG_STR(ITEM_INVENTORY_CLASS,           "None", CFGF_MULTI), \
   CFG_INT(ITEM_INVENTORY_AMOUNT,               0, CFGF_NONE ), \
   CFG_INT(ITEM_INVENTORY_MAXAMOUNT,            0, CFGF_NONE ), \
   CFG_INT(ITEM_INVENTORY_INTERHUBAMOUNT,       0, CFGF_NONE ), \
   CFG_STR(ITEM_INVENTORY_ICON,                "", CFGF_NONE ), \
   CFG_STR(ITEM_INVENTORY_PICKUPMESSAGE,       "", CFGF_NONE ), \
   CFG_STR(ITEM_INVENTORY_PICKUPSOUND,         "", CFGF_NONE ), \
   CFG_STR(ITEM_INVENTORY_PICKUPFLASH,         "", CFGF_NONE ), \
   CFG_STR(ITEM_INVENTORY_USESOUND,            "", CFGF_NONE ), \
   CFG_INT(ITEM_INVENTORY_RESPAWNTICS,          0, CFGF_NONE ), \
   CFG_INT(ITEM_INVENTORY_GIVEQUEST,           -1, CFGF_NONE ), \
   CFG_STR(ITEM_INVENTORY_FLAGS,               "", CFGF_NONE ), \
   CFG_STR(ITEM_INVENTORY_COMPATNAME,        NULL, CFGF_NONE ), \
   CFG_INT(ITEM_HEALTH_AMOUNT,                  0, CFGF_NONE ), \
   CFG_INT(ITEM_HEALTH_MAXAMOUNT,               0, CFGF_NONE ), \
   CFG_MVPROP(ITEM_HEALTH_LOWMESSAGE, lowmsg_opts, CFGF_NONE ), \
   CFG_END()

cfg_opt_t edf_inv_opts[] =
{
   CFG_STR(ITEM_INVENTORY_INHERITS, NULL, CFGF_NONE),
   INVENTORY_FIELDS
};

cfg_opt_t edf_invdelta_opts[] =
{
   CFG_STR(ITEM_DELTA_NAME, NULL, CFGF_NONE),
   INVENTORY_FIELDS
};

// Inventory Inheritance

// inv_pstack: used by recursive E_ProcessInventory to track inheritance
static inventory_t **inv_pstack  = NULL;
static unsigned int inv_pindex  = 0;

// Properties for meta item classes
IMPLEMENT_RTTI_TYPE(MetaGiveItem)

//
// Compatibility stuff
//

// pickup effect names (these are currently searched linearly)
// matching enum values are defined in e_inventory.h

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

//=============================================================================
//
// Inventory Hashing 
//

#define NUMINVCHAINS 257

// hash by name
static EHashTable<inventory_t, ENCStringHashKey,
                  &inventory_t::name, 
                  &inventory_t::namelinks> inv_namehash(NUMINVCHAINS);

// hash by ID number
static EHashTable<inventory_t, EIntHashKey,
                  &inventory_t::numkey, 
                  &inventory_t::numlinks> inv_numhash(NUMINVCHAINS);

// hash for pickupitem compatibility
static EHashTable<inventory_t, EIntHashKey,
                  &inventory_t::compatNum, 
                  &inventory_t::compatlinks> inv_comphash(NUMINVCHAINS);

//
// E_InventoryForID
//
// Get an inventory item by ID number.
//
inventory_t *E_InventoryForID(int idnum)
{
   return inv_numhash.objectForKey(idnum);
}

//
// E_GetInventoryForID
//
// As above, but causes a fatal error if an inventory def is not found.
//
inventory_t *E_GetInventoryForID(int idnum)
{
   inventory_t *inv = E_InventoryForID(idnum);

   if(!inv)
      I_Error("E_GetInventoryForID: invalid inventory ID %d\n", idnum);

   return inv;
}

//
// E_InventoryForName
//
// Returns an inventory definition given its name. Returns NULL
// if not found.
//
inventory_t *E_InventoryForName(const char *name)
{
   return inv_namehash.objectForKey(name);
}

//
// E_GetInventoryForName
//
// As above, but causes a fatal error if the inventory def isn't found.
//
inventory_t *E_GetInventoryForName(const char *name)
{
   inventory_t *inv = E_InventoryForName(name);

   if(!inv)
      I_Error("E_GetInventoryForName: bad inventory item %s\n", name);

   return inv;
}

//
// E_InventoryForCompatNum
//
// Returns an inventory definition given a pickupitem compatibility number.
//
inventory_t *E_InventoryForCompatNum(int pickupeffect)
{
   return inv_comphash.objectForKey(pickupeffect);
}

//=============================================================================
//
// Pre-Processing
//

//
// E_CollectInventory
//
// Pre-creates and hashes by name the inventory definitions, for purpose 
// of mutual and forward references.
//
void E_CollectInventory(cfg_t *cfg)
{
   static int currentID = 1;
   unsigned int i;
   unsigned int numInventory;    // number of inventory defs defined by the cfg
   inventory_t *newInvDefs  = NULL;

   // get number of inventory definitions defined by the cfg
   numInventory = cfg_size(cfg, EDF_SEC_INVENTORY);

   // echo counts
   E_EDFLogPrintf("\t\t%u inventory items defined\n", numInventory);

   if(numInventory)
   {
      // allocate inventory structures for the new thingtypes
      newInvDefs = estructalloc(inventory_t, numInventory);

      numInventoryDefs += numInventory;

      // create metatables
      for(i = 0; i < numInventory; i++)
         newInvDefs[i].meta = new MetaTable("inventory");
   }

   // build hash tables
   E_EDFLogPuts("\t\tBuilding inventory hash tables\n");
   
   // cycle through the inventory items defined in the cfg
   for(i = 0; i < numInventory; i++)
   {
      cfg_t *invcfg = cfg_getnsec(cfg, EDF_SEC_INVENTORY, i);
      const char *name       = cfg_title(invcfg);
      const char *compatName = cfg_getstr(invcfg, ITEM_INVENTORY_COMPATNAME);
      int compatNum = E_StrToNumLinear(pickupnames, PFX_NUMFX, compatName);

      // This is a new inventory, whether or not one already exists by this name
      // in the hash table. For subsequent addition of EDF inventory defs at 
      // runtime, the hash table semantics of "find newest first" take care of 
      // overriding, while not breaking objects that depend on the original 
      // definition of the inventory type for inheritance purposes.
      inventory_t *inv = &newInvDefs[i];

      // initialize name
      inv->name = estrdup(name);

      // add to name hash
      inv_namehash.addObject(inv);

      // create unique ID number and add to hash table
      inv->numkey = currentID++;
      inv_numhash.addObject(inv);

      // check for compatibility number
      if(compatNum > 0 && compatNum < PFX_NUMFX)
      {
         inv->compatNum = compatNum;
         inv_comphash.addObject(inv);
      }
   }
}

//=============================================================================
//
// Inheritance
//

//
// E_CheckInventoryInherit
//
// Makes sure the inventory definition being inherited from has not already
// been inherited during the current inheritance chain. Returns false if the
// check fails, and true if it succeeds.
//
static bool E_CheckInventoryInherit(inventory_t *inv)
{
   for(unsigned int i = 0; i < numInventoryDefs; i++)
   {
      // circular inheritance
      if(inv_pstack[i] == inv)
         return false;

      // found end of list
      if(inv_pstack[i] == NULL)
         break;
   }

   return true;
}

//
// E_AddInventoryToPStack
//
// Adds an inventory definition to the inheritance stack.
//
static void E_AddInventoryToPStack(inventory_t *inv)
{
   // Overflow shouldn't happen since it would require cyclic inheritance as 
   // well, but I'll guard against it anyways.
   
   if(inv_pindex >= numInventoryDefs)
      E_EDFLoggedErr(2, "E_AddInventoryToPStack: max inheritance depth exceeded\n");

   inv_pstack[inv_pindex++] = inv;
}

//
// E_ResetInventoryPStack
//
// Resets the inventory inheritance stack, setting all the pstack
// values to -1, and setting pindex back to zero.
//
static void E_ResetInventoryPStack()
{
   for(unsigned int i = 0; i < numInventoryDefs; i++)
      inv_pstack[i] = NULL;

   inv_pindex = 0;
}

//
// E_CopyInventory
//
// Copies one inventory definition into another.
//
static void E_CopyInventory(inventory_t *child, inventory_t *parent)
{
   // save out fields that shouldn't be overwritten
   DLListItem<inventory_t> namelinks   = child->namelinks;
   DLListItem<inventory_t> numlinks    = child->numlinks;
   DLListItem<inventory_t> compatlinks = child->compatlinks;
   char *name      = child->name;
   int   numkey    = child->numkey;
   int   compatNum = child->compatNum;
   bool  processed = child->processed;
   MetaTable *meta = child->meta;

   // wipe out any dynamically allocated strings in inheriting inventory
   if(child->icon)
      efree(child->icon);
   if(child->pickUpMessage)
      efree(child->pickUpMessage);
   if(child->pickUpSound)
      efree(child->pickUpSound);
   if(child->pickUpFlash)
      efree(child->pickUpFlash);
   if(child->useSound)
      efree(child->useSound);

   // copy from source to destination
   memcpy(child, parent, sizeof(inventory_t));

   // restore saved fields
   child->namelinks   = namelinks;
   child->numlinks    = numlinks;
   child->compatlinks = compatlinks;
   child->name        = name;
   child->numkey      = numkey;
   child->compatNum   = compatNum;
   child->processed   = processed;
   child->meta        = meta;
   
   // normalize special fields
   
   // * duplicate mallocs
   if(child->icon)
      child->icon = estrdup(child->icon);
   if(child->pickUpMessage)
      child->pickUpMessage = estrdup(child->pickUpMessage);
   if(child->pickUpSound)
      child->pickUpSound = estrdup(child->pickUpSound);
   if(child->pickUpFlash)
      child->pickUpFlash = estrdup(child->pickUpFlash);
   if(child->useSound)
      child->useSound = estrdup(child->useSound);

   // * copy metatable
   child->meta->copyTableFrom(parent->meta);
}

//=============================================================================
//
// Processing
//

// IS_SET: this macro tests whether or not a particular field should
// be set. When applying deltas, we should not retrieve defaults.
// 01/27/04: Also, if inheritance is taking place, we should not
// retrieve defaults.

#undef  IS_SET
#define IS_SET(name) ((def && !inherits) || cfg_size(invsec, (name)) > 0)

//
// Inventory Class Processing Routines
//

//
// None
//
// For the default inventory class type; does nothing special.
//
static void E_processNone(inventory_t *inv, cfg_t *invsec, bool def, bool inherits)
{
   // Do nothing.
}

//
// Health
//
// Health inventory items are collected immediately and will add to the 
// collector's health, up to the maxamount value.
//
static void E_processHealthProperties(inventory_t *inv, cfg_t *invsec, 
                                      bool def, bool inherits)
{
   // "Hard-coded" initial properties for Health class:
   if(def && !inherits)
      inv->flags |= INVF_AUTOACTIVATE;

   // amount - amount of health given by the pickup
   if(IS_SET(ITEM_HEALTH_AMOUNT))
      E_MetaIntFromCfgInt(inv->meta, invsec, ITEM_HEALTH_AMOUNT);

   // maxamount - limit of healing ability for this item
   if(IS_SET(ITEM_HEALTH_MAXAMOUNT))
      E_MetaIntFromCfgInt(inv->meta, invsec, ITEM_HEALTH_MAXAMOUNT);

   if(IS_SET(ITEM_HEALTH_LOWMESSAGE))
   {
      cfg_t *lowsec = cfg_getmvprop(invsec, ITEM_HEALTH_LOWMESSAGE);

      E_MetaIntFromCfgInt(inv->meta, lowsec, ITEM_LOWMSG_VALUE);
      E_MetaStringFromCfgString(inv->meta, invsec, ITEM_LOWMSG_MESSAGE);
   }
}

typedef void (*ClassFuncPtr)(inventory_t *, cfg_t *, bool, bool);

static ClassFuncPtr inventoryClasses[INV_CLASS_NUMCLASSES] = 
{
   E_processNone,
   E_processHealthProperties
};

//
// E_addMetaClass
//
// Add a class to the metatable, provided it hasn't been added already.
//
static void E_addMetaClass(inventory_t *inv, int classtype)
{
   MetaObject *obj = NULL;

   while((obj = inv->meta->getNextKeyAndType(obj, "classtype", RTTI(MetaInteger))))
   {
      MetaInteger *mInt = static_cast<MetaInteger *>(obj);
      if(mInt->getValue() == classtype)
         return;
   }

   inv->meta->addInt("classtype", classtype);
}

//
// E_applyInventoryClasses
//
// For every class the inventory implements, fields will be added by the above
// processing methods.
//
static void E_applyInventoryClasses(inventory_t *inv, cfg_t *invsec, bool def, 
                                    bool inherits)
{
   unsigned int numClasses = cfg_size(invsec, ITEM_INVENTORY_CLASS);

   for(unsigned int i = 0; i < numClasses; i++)
   {
      const char *classname = cfg_getnstr(invsec, ITEM_INVENTORY_CLASS, i);
      int classtype = E_StrToNumLinear(inventoryClassNames, INV_CLASS_NUMCLASSES, classname);

      if(classtype == INV_CLASS_NUMCLASSES)
      {
         E_EDFLoggedWarning(2, "Warning: unknown inventory class '%s'\n", classname);
         classtype = INV_CLASS_NONE;
      }

#ifdef RANGECHECK
      if(classtype < 0 || classtype >= INV_CLASS_NUMCLASSES)
      {
         // internal sanity check
         E_EDFLoggedErr(2, "E_ProcessInventory: internal error - bad classtype %d\n", 
                        classtype);
      }
#endif

      // record this class in the metatable
      E_addMetaClass(inv, classtype);

      // process fields for this class
      inventoryClasses[classtype](inv, invsec, def, inherits);
   }
}

//
// E_ProcessInventory
//
// Generalized code to process the data for a single inventory definition
// structure. Doubles as code for inventory and inventorydelta.
//
static void E_ProcessInventory(inventory_t *inv, cfg_t *invsec, cfg_t *pcfg, bool def)
{
   //int tempint;
   const char *tempstr;
   bool inherits = false;

   // possible when inheriting from an inventory def of a previous EDF generation
   if(!invsec) 
      return;

   // Process inheritance (not in deltas)
   if(def)
   {
      // if this inventory is already processed via recursion due to
      // inheritance, don't process it again
      if(inv->processed)
         return;
      
      if(cfg_size(invsec, ITEM_INVENTORY_INHERITS) > 0)
      {
         cfg_t *parent_invsec;
         
         // resolve parent inventory def
         inventory_t *parent = 
            E_GetInventoryForName(cfg_getstr(invsec, ITEM_INVENTORY_INHERITS));

         // check against cyclic inheritance
         if(!E_CheckInventoryInherit(parent))
         {
            E_EDFLoggedErr(2, 
               "E_ProcessInventory: cyclic inheritance detected in inventory '%s'\n",
               inv->name);
         }
         
         // add to inheritance stack
         E_AddInventoryToPStack(parent);

         // process parent recursively
         parent_invsec = cfg_gettsec(pcfg, EDF_SEC_INVENTORY, parent->name);
         E_ProcessInventory(parent, parent_invsec, pcfg, true);
         
         // copy parent to this thing
         E_CopyInventory(inv, parent);

         // keep track of parent explicitly
         inv->parent = parent;
         
         // we inherit, so treat defaults as no value
         inherits = true;
      }
      else
         inv->parent = NULL; // no parent.

      // mark this inventory def as processed
      inv->processed = true;
   }

   // field processing
   
   if(cfg_size(invsec, ITEM_INVENTORY_CLASS) > 0)
      E_applyInventoryClasses(inv, invsec, def, inherits);

   if(IS_SET(ITEM_INVENTORY_AMOUNT))
      inv->amount = cfg_getint(invsec, ITEM_INVENTORY_AMOUNT);

   if(IS_SET(ITEM_INVENTORY_MAXAMOUNT))
      inv->maxAmount = cfg_getint(invsec, ITEM_INVENTORY_MAXAMOUNT);

   if(IS_SET(ITEM_INVENTORY_INTERHUBAMOUNT))
      inv->interHubAmount = cfg_getint(invsec, ITEM_INVENTORY_INTERHUBAMOUNT);

   if(IS_SET(ITEM_INVENTORY_ICON))
      E_ReplaceString(inv->icon, cfg_getstrdup(invsec, ITEM_INVENTORY_ICON));

   if(IS_SET(ITEM_INVENTORY_PICKUPMESSAGE))
      E_ReplaceString(inv->pickUpMessage, cfg_getstrdup(invsec, ITEM_INVENTORY_PICKUPMESSAGE));

   if(IS_SET(ITEM_INVENTORY_PICKUPSOUND))
      E_ReplaceString(inv->pickUpSound, cfg_getstrdup(invsec, ITEM_INVENTORY_PICKUPSOUND));

   if(IS_SET(ITEM_INVENTORY_PICKUPFLASH))
      E_ReplaceString(inv->pickUpFlash, cfg_getstrdup(invsec, ITEM_INVENTORY_PICKUPFLASH));

   if(IS_SET(ITEM_INVENTORY_USESOUND))
      E_ReplaceString(inv->useSound, cfg_getstrdup(invsec, ITEM_INVENTORY_USESOUND));

   if(IS_SET(ITEM_INVENTORY_RESPAWNTICS))
      inv->respawnTics = cfg_getint(invsec, ITEM_INVENTORY_RESPAWNTICS);

   if(IS_SET(ITEM_INVENTORY_GIVEQUEST))
      inv->giveQuest = cfg_getint(invsec, ITEM_INVENTORY_GIVEQUEST);

   if(IS_SET(ITEM_INVENTORY_FLAGS))
   {
      tempstr = cfg_getstr(invsec, ITEM_INVENTORY_FLAGS);
      if(*tempstr == '\0')
         inv->flags = 0;
      else
         inv->flags = E_ParseFlags(tempstr, &inventory_flagset);
   }
   
   // TODO: addflags/remflags

   // TODO: player class restrictions
}

//
// E_ProcessInventoryDefs
//
// Resolves and loads all information for the inventory structures.
//
void E_ProcessInventoryDefs(cfg_t *cfg)
{
   unsigned int i, numInventory;

   E_EDFLogPuts("\t* Processing inventory data\n");

   if(!(numInventory = cfg_size(cfg, EDF_SEC_INVENTORY)))
      return;

   // allocate inheritance stack and hitlist
   inv_pstack = ecalloc(inventory_t **, numInventoryDefs, sizeof(inventory_t *));

   // TODO: any first-time-only processing?

   for(i = 0; i < numInventory; i++)
   {
      cfg_t       *invsec = cfg_getnsec(cfg, EDF_SEC_INVENTORY, i);
      const char  *name   = cfg_title(invsec);
      inventory_t *inv    = E_InventoryForName(name);

      // reset the inheritance stack
      E_ResetInventoryPStack();

      // add this def to the stack
      E_AddInventoryToPStack(inv);

      E_ProcessInventory(inv, invsec, cfg, true);

      E_EDFLogPrintf("\t\tFinished inventory %s(#%d)\n", inv->name, inv->numkey);
   }

   // free tables
   efree(inv_pstack);
}

//
// E_ProcessInventoryDeltas
//
// Does processing for inventorydelta sections, which allow cascading editing
// of existing inventory items. The inventorydelta shares most of its fields
// and processing code with the thingtype section.
//
void E_ProcessInventoryDeltas(cfg_t *cfg)
{
   int i, numdeltas;

   E_EDFLogPuts("\t* Processing inventory deltas\n");

   numdeltas = cfg_size(cfg, EDF_SEC_INVDELTA);

   E_EDFLogPrintf("\t\t%d inventorydelta(s) defined\n", numdeltas);

   for(i = 0; i < numdeltas; i++)
   {
      cfg_t       *deltasec = cfg_getnsec(cfg, EDF_SEC_INVDELTA, i);
      inventory_t *inv;

      // get thingtype to edit
      if(!cfg_size(deltasec, ITEM_DELTA_NAME))
         E_EDFLoggedErr(2, "E_ProcessInventoryDeltas: inventorydelta requires name field\n");

      inv = E_GetInventoryForName(cfg_getstr(deltasec, ITEM_DELTA_NAME));

      E_ProcessInventory(inv, deltasec, cfg, false);

      E_EDFLogPrintf("\t\tApplied inventorydelta #%d to %s(#%d)\n",
                     i, inv->name, inv->numkey);
   }
}

//=============================================================================
//
// Compatibility Stuff
//

// pickupfx lookup table used in P_TouchSpecialThing (is allocated
// with size NUMSPRITES)
inventory_t **pickupfx = NULL;

//
// E_ProcessItems
//
// Allocates the pickupfx array used in P_TouchSpecialThing,
// and loads all pickupitem definitions, using the sprite hash
// table to resolve what sprite owns the specified effect.
//
void E_ProcessItems(cfg_t *cfg)
{
   static int oldnumsprites;
   int i, numnew, numpickups;

   E_EDFLogPuts("\t* Processing compatibility pickup items\n");

   // allocate and initialize pickup effects array
   // haleyjd 11/21/11: allow multiple runs
   numnew = NUMSPRITES - oldnumsprites;
   if(numnew > 0)
   {
      pickupfx = erealloc(inventory_t **, pickupfx, NUMSPRITES * sizeof(inventory_t *));
      for(i = oldnumsprites; i < NUMSPRITES; i++)
         pickupfx[i] = NULL;
      oldnumsprites = NUMSPRITES;
   }

   // sanity check
   if(!pickupfx)
      E_EDFLoggedErr(2, "E_ProcessItems: no sprites defined!?\n");
   
   // load pickupfx
   numpickups = cfg_size(cfg, SEC_PICKUPFX);
   E_EDFLogPrintf("\t\t%d compatibility pickup item(s) defined\n", numpickups);

   for(i = 0; i < numpickups; ++i)
   {
      int fxnum, sprnum;
      cfg_t *sec = cfg_getnsec(cfg, SEC_PICKUPFX, i);
      const char *title = cfg_title(sec);
      const char *pfx = cfg_getstr(sec, ITEM_PICKUPFX);
      inventory_t *inv;

      // validate the sprite name given in the section title and
      // resolve to a sprite number (hashed)
      sprnum = E_SpriteNumForName(title);

      if(sprnum == -1)
      {
         // haleyjd 05/31/06: downgraded to warning, substitute blanksprite
         E_EDFLoggedWarning(2,
            "Warning: invalid sprite mnemonic for pickup item: '%s'\n",
            title);
         continue;
      }

      // find the proper pickup effect number (linear search)
      fxnum = E_StrToNumLinear(pickupnames, PFX_NUMFX, pfx);

      // find the proper inventory item to associate with it
      inv = E_InventoryForCompatNum(fxnum);

      if(inv)
      {
         E_EDFLogPrintf("\t\tSet sprite %s(#%d) to pickup effect %s(#%d)\n",
                        title, sprnum, pfx, fxnum);
         pickupfx[sprnum] = inv;
      }
      else
      {
         E_EDFLogPrintf("\t\tClearing pickup effect for sprite %s(#%d)\n",
                        title, sprnum);
         pickupfx[sprnum] = NULL;
      }
   }
}

// EOF

