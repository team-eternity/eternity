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
#include "metaapi.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"
#include "e_edf.h"
#include "e_hash.h"
#include "e_lib.h"
#include "e_inventory.h"

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
   { "KEEPDEPLETED",    INVF_KEEPDEPLETED    },
   { "ADDITIVETIME",    INVF_ADDITIVETIME    },
   { "UNTOSSABLE",      INVF_UNTOSSABLE      },
   { NULL,              0                    }
};

static dehflagset_t inventory_flagset =
{
   inventoryflags, // flaglist
   0,              // mode
};

// Frame section keywords
#define ITEM_INVENTORY_INHERITS       "inherits"
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

#define ITEM_DELTA_NAME               "name"

//
// Inventory Options
//

#define INVENTORY_FIELDS \
   CFG_INT(ITEM_INVENTORY_AMOUNT,          0, CFGF_NONE), \
   CFG_INT(ITEM_INVENTORY_MAXAMOUNT,       0, CFGF_NONE), \
   CFG_INT(ITEM_INVENTORY_INTERHUBAMOUNT,  0, CFGF_NONE), \
   CFG_STR(ITEM_INVENTORY_ICON,           "", CFGF_NONE), \
   CFG_STR(ITEM_INVENTORY_PICKUPMESSAGE,  "", CFGF_NONE), \
   CFG_STR(ITEM_INVENTORY_PICKUPSOUND,    "", CFGF_NONE), \
   CFG_STR(ITEM_INVENTORY_PICKUPFLASH,    "", CFGF_NONE), \
   CFG_STR(ITEM_INVENTORY_USESOUND,       "", CFGF_NONE), \
   CFG_INT(ITEM_INVENTORY_RESPAWNTICS,     0, CFGF_NONE), \
   CFG_INT(ITEM_INVENTORY_GIVEQUEST,      -1, CFGF_NONE), \
   CFG_STR(ITEM_INVENTORY_FLAGS,          "", CFGF_NONE), \
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

//=============================================================================
//
// Inventory Hashing 
//

#define NUMINVCHAINS 307

// hash by name
static EHashTable<inventory_t, ENCStringHashKey,
                  &inventory_t::name, &inventory_t::namelinks> inv_namehash;

// hash by ID number
static EHashTable<inventory_t, EIntHashKey,
                  &inventory_t::numkey, &inventory_t::numlinks> inv_numhash;


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

   // initialize hash tables if needed
   if(!inv_namehash.isInitialized())
   {
      inv_namehash.Initialize(NUMINVCHAINS);
      inv_numhash.Initialize(NUMINVCHAINS);
   }

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
   
   // cycle through the thingtypes defined in the cfg
   for(i = 0; i < numInventory; i++)
   {
      cfg_t *invcfg = cfg_getnsec(cfg, EDF_SEC_INVENTORY, i);
      const char *name = cfg_title(invcfg);

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

      // create ID number and add to hash table
      inv->numkey = currentID++;
      inv_numhash.addObject(inv);
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
   DLListItem<inventory_t> namelinks = child->namelinks;
   DLListItem<inventory_t> numlinks  = child->numlinks;
   char *name      = child->name;
   int   numkey    = child->numkey;
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
   child->namelinks = namelinks;
   child->numlinks  = numlinks;
   child->name      = name;
   child->numkey    = numkey;
   child->processed = processed;
   child->meta      = meta;
   
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
         inventory_t *parent = E_GetInventoryForName(cfg_getstr(invsec, ITEM_INVENTORY_INHERITS));

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
         inv->giveQuest = 0;
      else
         inv->giveQuest = E_ParseFlags(tempstr, &inventory_flagset);
   }

   // TODO: addflags/remflags
   // TODO: effects/"child class" fields
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

// EOF

