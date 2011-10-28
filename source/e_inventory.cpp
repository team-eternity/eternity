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
#include "d_dehtbl.h" // for dehflags parsing
#include "doomtype.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"
#include "e_edf.h"
#include "e_lib.h"
#include "e_inventory.h"

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
#define ITEM_INVENTORY_ID             "id"
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
   CFG_INT(ITEM_INVENTORY_ID,             -1, CFGF_NONE), \
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

// inv_hitlist: keeps track of what inventory defs are initialized
static byte *inv_hitlist = NULL;

// inv_pstack: used by recursive E_ProcessInventory to track inheritance
static int  *inv_pstack  = NULL;
static int   inv_pindex  = 0;

// global vars
int NumInventoryDefs;

//
// E_CheckInventoryInherit
//
// Makes sure the inventory definition being inherited from has not already
// been inherited during the current inheritance chain. Returns false if the
// check fails, and true if it succeeds.
//
static bool E_CheckInventoryInherit(int pnum)
{
   int i;

   for(i = 0; i < NumInventoryDefs; i++)
   {
      // circular inheritance
      if(inv_pstack[i] == pnum)
         return false;

      // found end of list
      if(inv_pstack[i] == -1)
         break;
   }

   return true;
}

//
// E_AddInventoryToPStack
//
// Adds a type number to the inheritance stack.
//
static void E_AddInventoryToPStack(int num)
{
   // Overflow shouldn't happen since it would require cyclic inheritance as 
   // well, but I'll guard against it anyways.
   
   if(inv_pindex >= NumInventoryDefs)
      E_EDFLoggedErr(2, "E_AddInventoryToPStack: max inheritance depth exceeded\n");

   inv_pstack[inv_pindex++] = num;
}

//
// E_ResetInventoryPStack
//
// Resets the inventory inheritance stack, setting all the pstack
// values to -1, and setting pindex back to zero.
//
static void E_ResetInventoryPStack(void)
{
   int i;

   for(i = 0; i < NumInventoryDefs; i++)
      inv_pstack[i] = -1;

   inv_pindex = 0;
}

//
// E_CopyInventory
//
// Copies one inventory definition into another.
//
static void E_CopyInventory(int num, int pnum)
{
   // TODO: save out fields that shouldn't be overwritten

   // TODO: copy from source to destination

   // TODO: normalize special fields
   // * duplicate any mallocs
   // * copy any metatables
   
   // TODO: restore saved fields

   // TODO: reset any uninherited fields to defaults
}

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
static void E_ProcessInventory(int i, cfg_t *invsec, cfg_t *pcfg, bool def)
{
   //int tempint;
   //const char *tempstr;
   bool inherits = false;

   // Process inheritance (not in deltas)
   if(def)
   {
      // if this inventory is already processed via recursion due to
      // inheritance, don't process it again
      if(inv_hitlist[i])
         return;
      
      if(cfg_size(invsec, ITEM_INVENTORY_INHERITS) > 0)
      {
         cfg_t *parent_invsec;
         
         // TODO: resolve parent inventory def
         int pnum = -1; //E_GetInventoryNumForName(cfg_getstr(invsec, ITEM_INVENTORY_INHERITS));

         // check against cyclic inheritance
         if(!E_CheckInventoryInherit(pnum))
         {
            E_EDFLoggedErr(2, 
               "E_ProcessInventory: cyclic inheritance detected in inventory '%s'\n",
               /*TODO: mobjinfo[i].name*/ "");
         }
         
         // add to inheritance stack
         E_AddInventoryToPStack(pnum);

         // process parent recursively
         parent_invsec = cfg_getnsec(pcfg, EDF_SEC_INVENTORY, pnum);
         E_ProcessInventory(pnum, parent_invsec, pcfg, true);
         
         // copy parent to this thing
         E_CopyInventory(i, pnum);

         // TODO: keep track of parent explicitly
         // mobjinfo[i].parent = &mobjinfo[pnum];
         
         // we inherit, so treat defaults as no value
         inherits = true;
      }
      else
        ; // TODO: mobjinfo[i].parent = NULL; // no parent.

      // mark this inventory def as processed
      inv_hitlist[i] = 1;
   }

   // TODO: field processing

   // output end message if processing a definition
   if(def)
      E_EDFLogPrintf("\t\tFinished inventory %s(#%d)\n", ""/*TODO:mobjinfo[i].name*/, i);
}

//
// E_ProcessInventoryDefs
//
// Resolves and loads all information for the inventory structures.
//
void E_ProcessInventoryDefs(cfg_t *cfg)
{
   int i;

   E_EDFLogPuts("\t* Processing inventory data\n");

   // allocate inheritance stack and hitlist
   inv_hitlist = ecalloc(byte *, NumInventoryDefs, sizeof(byte));
   inv_pstack  = ecalloc(int  *, NumInventoryDefs, sizeof(int));

   // TODO: Be sure to fully support additive defs like states.
   for(i = 0; i < NumInventoryDefs; ++i)
   {
      cfg_t *invsec = cfg_getnsec(cfg, EDF_SEC_INVENTORY, i);

      // reset the inheritance stack
      E_ResetInventoryPStack();

      // add this def to the stack
      E_AddInventoryToPStack(i);

      E_ProcessInventory(i, invsec, cfg, true);
   }

   // free tables
   efree(inv_hitlist);
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
      const char *tempstr;
      //int mobjType;
      cfg_t *deltasec = cfg_getnsec(cfg, EDF_SEC_INVDELTA, i);

      // get thingtype to edit
      if(!cfg_size(deltasec, ITEM_DELTA_NAME))
         E_EDFLoggedErr(2, "E_ProcessInventoryDeltas: inventorydelta requires name field\n");

      tempstr = cfg_getstr(deltasec, ITEM_DELTA_NAME);
      // TODO:mobjType = E_GetThingNumForName(tempstr);

      E_ProcessInventory(0/*TODO:mobjType*/, deltasec, cfg, false);

      E_EDFLogPrintf("\t\tApplied inventorydelta #%d to %s(#%d)\n",
                     i, ""/*TODO:mobjinfo[mobjType].name*/, 0/*TODO:mobjType*/);
   }
}

// EOF

