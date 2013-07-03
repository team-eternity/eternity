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

#include "metaapi.h"

//=============================================================================
//
// Effect Classes
//

// Item Effect Type names
static const char *e_ItemEffectTypeNames[NUMITEMFX] =
{
   "None",
   "Health"
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

//=============================================================================
//
// Effect Processing
//

// Health fields
#define ITEM_HEALTH_AMOUNT     "amount"
#define ITEM_HEALTH_MAXAMOUNT  "maxamount"
#define ITEM_HEALTH_LOWMESSAGE "lowmessage"

cfg_opt_t edf_healthfx_opts[] =
{
   CFG_INT(ITEM_HEALTH_AMOUNT,     0,  CFGF_NONE), // amount to recover
   CFG_INT(ITEM_HEALTH_MAXAMOUNT,  0,  CFGF_NONE), // max that can be recovered
   CFG_STR(ITEM_HEALTH_LOWMESSAGE, "", CFGF_NONE), // message if health < amount
   CFG_END()
};

static const char *e_ItemSectionNames[NUMITEMFX] =
{
   "",
   EDF_SEC_HEALTHFX
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
         newEffect->setInt("class", i);
         newEffect->setString("classname", className);

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

//=============================================================================
//
// Inventory Items
//
// Inventory items represent a holdable item that can take up a slot in an 
// inventory.
//

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

   // TODO: MOAR
}

// EOF

