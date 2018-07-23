// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:  
//    Custom damage types, or "Means of Death" flags.
//
//-----------------------------------------------------------------------------

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"
#include "i_system.h"

#include "Confuse/confuse.h"

#include "e_lib.h"
#include "e_edf.h"
#include "e_mod.h"
#include "e_hash.h"

#include "d_dehtbl.h"
#include "d_io.h"
#include "doomtype.h"
#include "metaapi.h"

//
// damagetype options
//

#define ITEM_DAMAGETYPE_NUM        "num"
#define ITEM_DAMAGETYPE_OBIT       "obituary"
#define ITEM_DAMAGETYPE_SELFOBIT   "obituaryself"
#define ITEM_DAMAGETYPE_SOURCELESS "sourceless"
#define ITEM_DAMAGETYPE_ABSPUSH    "absolute.push"
#define ITEM_DAMAGETYPE_ABSHOP     "absolute.hop"

cfg_opt_t edf_dmgtype_opts[] =
{
   CFG_INT(ITEM_DAMAGETYPE_NUM,         -1,    CFGF_NONE),
   CFG_STR(ITEM_DAMAGETYPE_OBIT,        NULL,  CFGF_NONE),
   CFG_STR(ITEM_DAMAGETYPE_SELFOBIT,    NULL,  CFGF_NONE),
   CFG_BOOL(ITEM_DAMAGETYPE_SOURCELESS, false, CFGF_NONE),
   CFG_FLOAT(ITEM_DAMAGETYPE_ABSPUSH,   0,     CFGF_NONE),
   CFG_FLOAT(ITEM_DAMAGETYPE_ABSHOP,    0,     CFGF_NONE),
   CFG_END()
};

//
// Static Data
//

// hash tables

#define NUMMODCHAINS 67

static EHashTable<emod_t, ENCStringHashKey, 
                 &emod_t::name, &emod_t::namelinks> e_mod_namehash(NUMMODCHAINS);

// haleyjd 08/02/09: use new generic hash
static EHashTable<emod_t, EIntHashKey, 
                  &emod_t::num, &emod_t::numlinks> e_mod_numhash(NUMMODCHAINS);

// default damage type - "Unknown"
static emod_t unknown_mod;

// allocation starts at D_MAXINT and works toward 0
static int edf_alloc_modnum = D_MAXINT;

//
// Static Functions
//

//
// E_AddDamageTypeToNameHash
//
// Puts the emod_t into the name hash table.
//
static void E_AddDamageTypeToNameHash(emod_t *mod)
{
   e_mod_namehash.addObject(*mod);

   // cache dfKeyIndex for use in metatables
   mod->dfKeyIndex = 
      MetaTable::IndexForKey(E_ModFieldName("damagefactor", mod));
}

// need forward declaration for E_AutoAllocModNum
static void E_AddDamageTypeToNumHash(emod_t *mod);

//
// E_AutoAllocModNum
//
// Automatically assigns an unused damage type number to a damage type
// which was not given one by the author, to allow reference by name
// anywhere without the chore of number allocation.
//
static bool E_AutoAllocModNum(emod_t *mod)
{
   int num;

#ifdef RANGECHECK
   if(mod->num > 0)
      I_Error("E_AutoAllocModNum: called for emod_t with valid num\n");
#endif

   // cannot assign because we're out of dehnums?
   if(edf_alloc_modnum < 0)
      return false;

   do
   {
      num = edf_alloc_modnum--;
   } 
   while(num > 0 && E_DamageTypeForNum(num) != &unknown_mod);

   // ran out while looking for an unused number?
   if(num <= 0)
      return false;

   // assign it!
   mod->num = num;
   E_AddDamageTypeToNumHash(mod);

   return true;
}

//
// E_AddDamageTypeToNumHash
//
// Puts the emod_t into the numeric hash table.
//
static void E_AddDamageTypeToNumHash(emod_t *mod)
{
   // Auto-assign a numeric key to all damage types which don't have
   // a valid one explicitly specified. This avoids some gigantic, 
   // messy code rewrites by allowing mobjinfo to always store the 
   // numeric key.
   
   if(mod->num <= 0)
   {
      E_AutoAllocModNum(mod);
      return;
   }

   e_mod_numhash.addObject(mod);
}

//
// E_DelDamageTypeFromNumHash
//
// Removes the given emod_t from the numeric hash table.
// For overrides changing the numeric key of an existing damage type.
//
static void E_DelDamageTypeFromNumHash(emod_t *mod)
{
   e_mod_numhash.removeObject(mod);
}

//
// E_EDFDamageTypeForName
//
// Finds a damage type for the given name. If the name does not exist, this
// routine returns NULL rather than the Unknown type.
//
static emod_t *E_EDFDamageTypeForName(const char *name)
{
   return e_mod_namehash.objectForKey(name);
}

#define IS_SET(sec, name) (def || cfg_size(sec, name) > 0)

//
// E_ProcessDamageType
//
// Adds a single damage type.
//
static void E_ProcessDamageType(cfg_t *dtsec)
{
   emod_t *mod;
   const char *title, *obituary;
   bool def = true;
   int num;

   title = cfg_title(dtsec);
   num   = cfg_getint(dtsec, ITEM_DAMAGETYPE_NUM);

   // if one exists by this name already, modify it
   if((mod = E_EDFDamageTypeForName(title)))
   {
      // check numeric key
      if(mod->num != num)
      {
         // remove from numeric hash
         E_DelDamageTypeFromNumHash(mod);

         // change key
         mod->num = num;

         // add back to numeric hash
         E_AddDamageTypeToNumHash(mod);
      }

      // not a definition
      def = false;
   }
   else
   {
      // do not override the Unknown type
      if(!strcasecmp(title, "Unknown"))
      {
         E_EDFLoggedWarning(2, "Warning: attempt to override default Unknown "
                               "damagetype ignored\n");
         return;
      }

      // create a new mod
      mod = ecalloc(emod_t *, 1, sizeof(emod_t));

      mod->name = estrdup(title);
      mod->num  = num;

      // add to hash tables
      E_AddDamageTypeToNameHash(mod);
      E_AddDamageTypeToNumHash(mod);
   }

   if(IS_SET(dtsec, ITEM_DAMAGETYPE_OBIT))
   {
      obituary = cfg_getstr(dtsec, ITEM_DAMAGETYPE_OBIT);

      // if modifying, free any that already exists
      if(!def && mod->obituary)
      {
         efree(mod->obituary);
         mod->obituary = NULL;
      }

      if(obituary)
      {
         // determine if obituary string is a BEX string
         if(obituary[0] == '$' && strlen(obituary) > 1)
         {
            ++obituary;         
            mod->obitIsBexString = true;
         }
         else
            mod->obitIsBexString = false;

         mod->obituary = estrdup(obituary);
      }
   }

   // get self-obituary
   if(IS_SET(dtsec, ITEM_DAMAGETYPE_SELFOBIT))
   {
      obituary = cfg_getstr(dtsec, ITEM_DAMAGETYPE_SELFOBIT);

      // if modifying, free any that already exists
      if(!def && mod->selfobituary)
      {
         efree(mod->selfobituary);
         mod->selfobituary = NULL;
      }

      if(obituary)
      {
         // determine if obituary string is a BEX string
         if(obituary[0] == '$' && strlen(obituary) > 1)
         {
            ++obituary;         
            mod->selfObitIsBexString = true;
         }
         else
            mod->selfObitIsBexString = false;

         mod->selfobituary = estrdup(obituary);
      }
   }

   // process sourceless flag
   if(IS_SET(dtsec, ITEM_DAMAGETYPE_SOURCELESS))
      mod->sourceless = cfg_getbool(dtsec, ITEM_DAMAGETYPE_SOURCELESS);

   if(IS_SET(dtsec, ITEM_DAMAGETYPE_ABSPUSH))
   {
      mod->absolutePush = M_DoubleToFixed(cfg_getfloat(dtsec,
                                                       ITEM_DAMAGETYPE_ABSPUSH));
   }
   if(IS_SET(dtsec, ITEM_DAMAGETYPE_ABSHOP))
   {
      mod->absoluteHop = M_DoubleToFixed(cfg_getfloat(dtsec,
                                                      ITEM_DAMAGETYPE_ABSHOP));
   }

   E_EDFLogPrintf("\t\t%s damagetype %s\n", 
                  def ? "Defined" : "Modified", mod->name);
}

//
// E_initUnknownMod
//
// haleyjd 12/12/10: It's more convenient to do this at runtime now because of the
// POD objects that are contained in the emod_t structure.
//
static void E_initUnknownMod(void)
{
   static char name[] = "Unknown";
   static char obituary[] = "OB_DEFAULT";

   static bool firsttime = true;

   if(firsttime) // only needed once
   {
      firsttime = false;

      memset(&unknown_mod, 0, sizeof(emod_t));

      unknown_mod.name = name;
      unknown_mod.num  = 0;
      unknown_mod.obituary = obituary;
      unknown_mod.selfobituary = obituary;
      unknown_mod.obitIsBexString = true;
      unknown_mod.selfObitIsBexString = true;
      unknown_mod.sourceless = false;
   }
}

//
// Global Functions
//

//
// E_ProcessDamageTypes
//
// Adds all damage types in the given cfg_t.
//
void E_ProcessDamageTypes(cfg_t *cfg)
{
   unsigned int i;
   unsigned int nummods = cfg_size(cfg, EDF_SEC_MOD);

   E_EDFLogPrintf("\t* Processing damagetypes\n"
                  "\t\t%d damagetype(s) defined\n", nummods);

   // Initialized the "Unknown" damage type
   E_initUnknownMod();

   for(i = 0; i < nummods; ++i)
      E_ProcessDamageType(cfg_getnsec(cfg, EDF_SEC_MOD, i));
}

//
// E_DamageTypeForName
//
// Finds a damage type for the given name. If the name does not exist, the
// default "Unknown" damage type will be returned.
//
emod_t *E_DamageTypeForName(const char *name)
{
   emod_t *mod;

   if((mod = e_mod_namehash.objectForKey(name)) == NULL)
      mod = &unknown_mod;

   return mod;
}

//
// E_DamageTypeForNum
//
// Finds a damage type for the given number. If the number is not found, the
// default "Unknown" damage type will be returned.
//
emod_t *E_DamageTypeForNum(int num)
{
   emod_t *mod;
   
   if((mod = e_mod_numhash.objectForKey(num)) == NULL)
      mod = &unknown_mod;

   return mod;
}

//
// E_DamageTypeNumForName
//
// 01/01/09:
// Given a name, returns the corresponding damagetype number, or 0 if the
// requested type is not found by name.
//
int E_DamageTypeNumForName(const char *name)
{ 
   emod_t *mod = E_DamageTypeForName(name);

   return mod ? mod->num : 0;
}

// EOF

