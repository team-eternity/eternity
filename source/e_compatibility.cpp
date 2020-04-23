//
// Copyright (C) 2020 James Haley, Max Waine, Ioan Chera et al.
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
// Purpose: EDF compatibility WAD support
// Authors: Ioan Chera
//

#include "z_zone.h"

#include "Confuse/confuse.h"
#include "c_io.h"
#include "doomstat.h"
#include "e_compatibility.h"
#include "m_collection.h"
#include "m_misc.h"
#include "metaapi.h"

#define ITEM_COMPATIBILITY_HASHES "hashes"

extern const char *comp_strings[];

//
// When a setting is changed, store its previous value
//
struct changedsetting_t
{
   int index;
   int previousValue;
};

// The EDF-set table
static MetaTable table;

static PODCollection<changedsetting_t> changedSettings;

//
// Dynamically produces the compatibility options
//
static cfg_opt_t *E_buildCompatibilityOpts()
{
   static PODCollection<cfg_opt_t> opts;
   opts.add(CFG_STR(ITEM_COMPATIBILITY_HASHES, 0, CFGF_LIST));
   for(int i = 0; i < COMP_NUM_USED; ++i)
   {
      char tempstr[32];
      snprintf(tempstr, sizeof(tempstr), "comp_%s", comp_strings[i]);
      opts.add(CFG_BOOL(tempstr, false, CFGF_NONE));
      opts.add(CFG_END());
   }

   return &opts[0];
}

//
// The fields
//
cfg_opt_t *edf_compatibility_opts = E_buildCompatibilityOpts();

//
// Processes a compatibility item
//
static void E_processCompatibility(cfg_t *cfg, cfg_t* compatibility)
{
   unsigned hashCount = cfg_size(compatibility, ITEM_COMPATIBILITY_HASHES);

   if(!hashCount)
      return;

   for(unsigned hashIndex = 0; hashIndex < hashCount; ++hashIndex)
   {
      const char *hash = cfg_getnstr(compatibility, ITEM_COMPATIBILITY_HASHES, hashIndex);

      MetaTable *complist = new MetaTable(hash);
      table.addObject(complist);

      for(int i = 0; i < COMP_NUM_USED; ++i)
      {
         char tempstr[32];
         snprintf(tempstr, sizeof(tempstr), "comp_%s", comp_strings[i]);

         if(cfg_size(compatibility, tempstr))
         {
            bool value = cfg_getbool(compatibility, tempstr);
            complist->setInt(tempstr, value ? 1 : 0);
         }
      }
   }
}

//
// Process WAD compatibility
//
void E_ProcessCompatibilities(cfg_t* cfg)
{
   unsigned count = cfg_size(cfg, EDF_SEC_COMPATIBILITY);
   for(unsigned i = 0; i < count; ++i)
   {
      cfg_t* compatibility = cfg_getnsec(cfg, EDF_SEC_COMPATIBILITY, i);
      E_processCompatibility(cfg, compatibility);
   }
}

//
// Apply compatibility given a digest
//
void E_ApplyCompatibility(const char *digest)
{
   MetaString *comp = nullptr;
   MetaTable *complist = table.getMetaTable(digest, nullptr);
   if(!complist)
      return;

   for(int i = 0; i < COMP_NUM_USED; ++i)
   {
      char tempstr[32];
      snprintf(tempstr, sizeof(tempstr), "comp_%s", comp_strings[i]);

      int value = complist->getInt(tempstr, -1);
      if(value == -1)
         continue;
      changedSettings.add(changedsetting_t { i, g_opts.comp[i] } );
      g_opts.comp[i] = !!value;
   }
}

// EOF
