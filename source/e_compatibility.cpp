//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Max Waine, Ioan Chera et al.
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
#include "metaapi.h"
#include "st_stuff.h"

constexpr const char ITEM_COMPATIBILITY_HASHES[]  = "hashes";
constexpr const char ITEM_COMPATIBILITY_ENABLE[]  = "on";
constexpr const char ITEM_COMPATIBILITY_DISABLE[] = "off";


// The EDF-set table
static MetaTable ontable;
static MetaTable offtable;

//
// The fields
//
cfg_opt_t edf_compatibility_opts[] =
{
   CFG_STR(ITEM_COMPATIBILITY_HASHES,  nullptr, CFGF_LIST),
   CFG_STR(ITEM_COMPATIBILITY_ENABLE,  nullptr, CFGF_LIST),
   CFG_STR(ITEM_COMPATIBILITY_DISABLE, nullptr, CFGF_LIST),

   CFG_END()
};

static int s_overridden[NUM_overridableSetting];
static bool s_overrideEnabled[NUM_overridableSetting];
static int *const sk_overrideTarget[NUM_overridableSetting] =
{
   &sts_traditional_keys
};

//
// Processes a compatibility item
//
static void E_processCompatibility(cfg_t *cfg, cfg_t* compatibility)
{
   unsigned hashCount = cfg_size(compatibility, ITEM_COMPATIBILITY_HASHES);
   unsigned onsettingCount = cfg_size(compatibility, ITEM_COMPATIBILITY_ENABLE);
   unsigned offsettingCount = cfg_size(compatibility, ITEM_COMPATIBILITY_DISABLE);

   if(!hashCount || (!onsettingCount && !offsettingCount))
      return;

   for(unsigned hashIndex = 0; hashIndex < hashCount; ++hashIndex)
   {
      const char *hash = cfg_getnstr(compatibility, ITEM_COMPATIBILITY_HASHES, hashIndex);
      for(unsigned settingIndex = 0; settingIndex < onsettingCount; ++settingIndex)
      {
         const char *setting = cfg_getnstr(compatibility, ITEM_COMPATIBILITY_ENABLE, settingIndex);

         bool found = false;
         MetaString *metaSetting = nullptr;
         while((metaSetting = ontable.getNextKeyAndTypeEx(metaSetting, hash)))
         {
            if(!strcasecmp(metaSetting->getValue(), setting))
            {
               found = true;
               break;
            }
         }

         if(!found)
            ontable.addString(hash, setting);
      }
      for(unsigned settingIndex = 0; settingIndex < offsettingCount; ++settingIndex)
      {
         const char *setting = cfg_getnstr(compatibility, ITEM_COMPATIBILITY_DISABLE, settingIndex);

         bool found = false;
         MetaString *metaSetting = nullptr;
         while((metaSetting = offtable.getNextKeyAndTypeEx(metaSetting, hash)))
         {
            if(!strcasecmp(metaSetting->getValue(), setting))
            {
               found = true;
               break;
            }
         }

         if(!found)
            offtable.addString(hash, setting);
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
// Restore any pushed compatibilities
//
void E_RestoreCompatibilities()
{
   memset(level_compat_comp, 0, sizeof(level_compat_comp));
   memset(level_compat_compactive, 0, sizeof(level_compat_compactive));
   memset(s_overridden, 0, sizeof(s_overridden));
   memset(s_overrideEnabled, 0, sizeof(s_overrideEnabled));
}

//
// Look into settings and set them.
//
// IMPORTANT: all the comp_ and gameplay-affecting settings will require demo_version 401.
// Others are tolerated on -vanilla too
//
static void E_setItem(const char *name, bool enable)
{
   extern const char *comp_strings[];

   if(!strcasecmp(name, "sts_traditional_keys"))
   {
      s_overrideEnabled[overridableSetting_stsTraditionalKeys] = true;
      s_overridden[overridableSetting_stsTraditionalKeys] = enable ? 1 : 0;
      return;
   }

   // Look in the list of comp strings
   if(demo_version >= 401 && !strncasecmp(name, "comp_", 5))
   {
      for(int i = 0; i < COMP_NUM_USED; ++i)
      {
         if(!strcasecmp(name + 5, comp_strings[i]))
         {
            level_compat_compactive[i] = true;
            level_compat_comp[i] = enable ? 1 : 0;
            return;
         }
      }
      if(!strcasecmp(name, "comp_jump"))  // the only badly named entry
      {
         level_compat_compactive[comp_jump] = true;
         level_compat_comp[comp_jump] = enable ? 1 : 0;
      }
   }
}

//
// Apply compatibility given a digest
//
void E_ApplyCompatibility(const char *digest)
{
   E_RestoreCompatibilities();

   MetaString *metaSetting = nullptr;
   while((metaSetting = ontable.getNextKeyAndTypeEx(metaSetting, digest)))
      E_setItem(metaSetting->getValue(), true);
   metaSetting = nullptr;
   while((metaSetting = offtable.getNextKeyAndTypeEx(metaSetting, digest)))
      E_setItem(metaSetting->getValue(), false);
}

//
// Gets the overridable setting depending on compatibility setup
//
int E_Get(overridableSetting_e setting)
{
   return s_overrideEnabled[setting] ? s_overridden[setting] : *sk_overrideTarget[setting];
}

// EOF
