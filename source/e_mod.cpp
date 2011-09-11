// Emacs style mode select   -*- C -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2008 James Haley
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

//
// damagetype options
//

#define ITEM_DAMAGETYPE_NUM        "num"
#define ITEM_DAMAGETYPE_OBIT       "obituary"
#define ITEM_DAMAGETYPE_SELFOBIT   "obituaryself"
#define ITEM_DAMAGETYPE_SOURCELESS "sourceless"

cfg_opt_t edf_dmgtype_opts[] =
{
   CFG_INT(ITEM_DAMAGETYPE_NUM,         -1,        CFGF_NONE),
   CFG_STR(ITEM_DAMAGETYPE_OBIT,        NULL,      CFGF_NONE),
   CFG_STR(ITEM_DAMAGETYPE_SELFOBIT,    NULL,      CFGF_NONE),
   CFG_BOOL(ITEM_DAMAGETYPE_SOURCELESS, cfg_false, CFGF_NONE),
   CFG_END()
};

//
// Static Data
//

// hash tables

#define NUMMODCHAINS 67

static ehash_t e_mod_namehash;

E_KEYFUNC(emod_t, name)
E_LINKFUNC(emod_t, namelinks)

// haleyjd 08/02/09: use new generic hash
static ehash_t e_mod_numhash;

E_KEYFUNC(emod_t, num)

// default damage type - "Unknown"

static emod_t unknown_mod =
{
   { NULL, NULL }, // numlinks
   { NULL, NULL }, // namelinks

   "Unknown",      // name
   0,              // num
   "OB_DEFAULT",   // obituary
   "OB_DEFAULT",   // selfobituary
   true,           // obitIsBexString
   true,           // selfObitIsBexString
   false,          // sourceless
};

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
   if(!e_mod_namehash.isinit)
   {
      E_NCStrHashInit(&e_mod_namehash, NUMMODCHAINS, E_KEYFUNCNAME(emod_t, name),
                      E_LINKFUNCNAME(emod_t, namelinks));
   }
   
   E_HashAddObject(&e_mod_namehash, mod);
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
static boolean E_AutoAllocModNum(emod_t *mod)
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
   if(!e_mod_numhash.isinit)
   {
      E_SintHashInit(&e_mod_numhash, NUMMODCHAINS, 
                     E_KEYFUNCNAME(emod_t, num), NULL);
   }

   // Auto-assign a numeric key to all damage types which don't have
   // a valid one explicitly specified. This avoids some gigantic, 
   // messy code rewrites by allowing mobjinfo to always store the 
   // numeric key.
   
   if(mod->num <= 0)
   {
      E_AutoAllocModNum(mod);
      return;
   }

   E_HashAddObject(&e_mod_numhash, mod);
}

//
// E_DelDamageTypeFromNumHash
//
// Removes the given emod_t from the numeric hash table.
// For overrides changing the numeric key of an existing damage type.
//
static void E_DelDamageTypeFromNumHash(emod_t *mod)
{
   E_HashRemoveObject(&e_mod_numhash, mod);
}

//
// E_EDFDamageTypeForName
//
// Finds a damage type for the given name. If the name does not exist, this
// routine returns NULL rather than the Unknown type.
//
static emod_t *E_EDFDamageTypeForName(const char *name)
{
   return (emod_t *)(E_HashObjectForKey(&e_mod_namehash, &name));
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
   boolean def = true;
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
      mod = (emod_t *)(calloc(1, sizeof(emod_t)));

      mod->name = strdup(title);
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
         free(mod->obituary);
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

         mod->obituary = strdup(obituary);
      }
   }

   // get self-obituary
   if(IS_SET(dtsec, ITEM_DAMAGETYPE_SELFOBIT))
   {
      obituary = cfg_getstr(dtsec, ITEM_DAMAGETYPE_SELFOBIT);

      // if modifying, free any that already exists
      if(!def && mod->selfobituary)
      {
         free(mod->selfobituary);
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

         mod->selfobituary = strdup(obituary);
      }
   }

   // process sourceless flag
   if(IS_SET(dtsec, ITEM_DAMAGETYPE_SOURCELESS))
      mod->sourceless = (cfg_getbool(dtsec, ITEM_DAMAGETYPE_SOURCELESS) == cfg_true);

   E_EDFLogPrintf("\t\t%s damagetype %s\n", 
                  def ? "Defined" : "Modified", mod->name);
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

   if((mod = (emod_t *)(E_HashObjectForKey(&e_mod_namehash, &name))) == NULL)
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
   
   if((mod = (emod_t *)(E_HashObjectForKey(&e_mod_numhash, &num))) == NULL)
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

