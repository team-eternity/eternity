//
// The Eternity Engine
// Copyright (C) 2018 James Haley et al.
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
// Purpose: EDF hitscan puffs
// Authors: Ioan Chera
//

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"

#include "Confuse/confuse.h"
#include "d_gi.h"
#include "e_edfmetatable.h"
#include "e_puff.h"
#include "e_states.h"
#include "e_things.h"

// Differences we have so far between the various puffs:
// thingtype
// random Z?
// sound
// alternate pufftype for hitting
// vertical speed
// whether it appears on hit target
// chance to splat bloot on hit
// override state for small range (just use "melee")
// particle support? How many?

// metakey vocabulary
constexpr const char ITEM_PUFF_THINGTYPE[]       = "thingtype";
constexpr const char ITEM_PUFF_SOUND[]           = "sound";
constexpr const char ITEM_PUFF_HITSOUND[]        = "hitsound";
constexpr const char ITEM_PUFF_HITPUFFTYPE[]     = "hitpufftype";
constexpr const char ITEM_PUFF_NOBLOODPUFFTYPE[] = "nobloodpufftype";
constexpr const char ITEM_PUFF_UPSPEED[]         = "upspeed";
constexpr const char ITEM_PUFF_ZSPREAD[]         = "zspread";
constexpr const char ITEM_PUFF_PUNCHHACK[]       = "punchhack";
constexpr const char ITEM_PUFF_PARTICLES[]       = "particles";

constexpr const char ITEM_PUFF_PUFFONACTORS[]  = "PUFFONACTORS";
constexpr const char ITEM_PUFF_BLOODLESS[]     = "BLOODLESS";
constexpr const char ITEM_PUFF_LOCALTHRUST[]   = "LOCALTHRUST";
constexpr const char ITEM_PUFF_RANDOMTICS[]    = "RANDOMTICS";
constexpr const char ITEM_PUFF_TARGETSHOOTER[] = "TARGETSHOOTER";

// Interned metatable keys
MetaKeyIndex keyPuffThingType      (ITEM_PUFF_THINGTYPE      );
MetaKeyIndex keyPuffSound          (ITEM_PUFF_SOUND          );
MetaKeyIndex keyPuffHitSound       (ITEM_PUFF_HITSOUND       );
MetaKeyIndex keyPuffHitPuffType    (ITEM_PUFF_HITPUFFTYPE    );
MetaKeyIndex keyPuffNoBloodPuffType(ITEM_PUFF_NOBLOODPUFFTYPE);
MetaKeyIndex keyPuffUpSpeed        (ITEM_PUFF_UPSPEED        );
MetaKeyIndex keyPuffZSpread        (ITEM_PUFF_ZSPREAD        );
MetaKeyIndex keyPuffPunchHack      (ITEM_PUFF_PUNCHHACK      );
MetaKeyIndex keyPuffParticles      (ITEM_PUFF_PARTICLES      );
MetaKeyIndex keyPuffPuffOnActors   (ITEM_PUFF_PUFFONACTORS   );
MetaKeyIndex keyPuffBloodless      (ITEM_PUFF_BLOODLESS      );
MetaKeyIndex keyPuffLocalThrust    (ITEM_PUFF_LOCALTHRUST    );
MetaKeyIndex keyPuffRandomTics     (ITEM_PUFF_RANDOMTICS     );
MetaKeyIndex keyPuffTargetShooter  (ITEM_PUFF_TARGETSHOOTER  );

#define PUFF_CONFIGS \
   CFG_STR(ITEM_PUFF_THINGTYPE,       "",                 CFGF_NONE),       \
   CFG_STR(ITEM_PUFF_SOUND,           "none",             CFGF_NONE),       \
   CFG_STR(ITEM_PUFF_HITSOUND,        "none",             CFGF_NONE),       \
   CFG_STR(ITEM_PUFF_HITPUFFTYPE,     "",                 CFGF_NONE),       \
   CFG_STR(ITEM_PUFF_NOBLOODPUFFTYPE, "",                 CFGF_NONE),       \
   CFG_FLOAT(ITEM_PUFF_UPSPEED,       0,                  CFGF_NONE),       \
   CFG_FLOAT(ITEM_PUFF_ZSPREAD,       puffZSpreadDefault, CFGF_NONE),       \
   CFG_STR(ITEM_PUFF_PUNCHHACK,       "",                 CFGF_NONE),       \
   CFG_INT(ITEM_PUFF_PARTICLES,       0,                  CFGF_NONE),       \
   CFG_FLAG(ITEM_PUFF_PUFFONACTORS,   0,                  CFGF_SIGNPREFIX), \
   CFG_FLAG(ITEM_PUFF_BLOODLESS,      0,                  CFGF_SIGNPREFIX), \
   CFG_FLAG(ITEM_PUFF_LOCALTHRUST,    0,                  CFGF_SIGNPREFIX), \
   CFG_FLAG(ITEM_PUFF_RANDOMTICS,     0,                  CFGF_SIGNPREFIX), \
   CFG_FLAG(ITEM_PUFF_TARGETSHOOTER,  0,                  CFGF_SIGNPREFIX), \
   CFG_END()

//
// EDF puff options
//
cfg_opt_t edf_puff_opts[] =
{
   CFG_TPROPS(edf_generic_tprops, CFGF_NOCASE),
   PUFF_CONFIGS
};

cfg_opt_t edf_puff_delta_opts[] =
{
   CFG_STR("name", nullptr, CFGF_NONE),
   PUFF_CONFIGS
};

static MetaTable e_puffTable; // the pufftype metatable

//
// Replaces thing names with mobjtypes
//
static void E_postprocessThingType(MetaTable *table)
{
   const char *name = table->getString(keyPuffThingType, nullptr);
   if(!name)
      return;
   table->addInt(keyPuffThingType, E_SafeThingName(name));
   table->removeStringNR(keyPuffThingType);
}

//
// Replaces punch state with actual state number
//
static void E_postprocessPunchState(MetaTable *table)
{
   const char *sname = table->getString(keyPuffPunchHack, nullptr);
   if(!sname)
      return;
   table->addInt(keyPuffPunchHack, E_SafeStateName(sname));
   table->removeStringNR(keyPuffPunchHack);
}

//
// Process the pufftype settings
//
void E_ProcessPuffs(cfg_t *cfg)
{
   E_BuildGlobalMetaTableFromEDF(cfg, EDF_SEC_PUFFTYPE, EDF_SEC_PUFFDELTA,
                                 e_puffTable);

   // Now perform some postprocessing
   // Replace all alternate object names with tables, if available
   MetaObject *object = nullptr;
   while((object = e_puffTable.tableIterator(object)))
   {
      auto table = runtime_cast<MetaTable *>(object);
      if(!table)
         continue;
      E_postprocessThingType(table);
      E_postprocessPunchState(table);
   }
}

//
// Get a puff for name
//
const MetaTable *E_PuffForName(const char *name)
{
   auto puff = e_puffTable.getObjectKeyAndTypeEx<MetaTable>(name);
   if(!puff)
      return e_puffTable.getObjectKeyAndTypeEx<MetaTable>(GameModeInfo->puffType);
   return puff;
}

const MetaTable *E_PuffForIndex(size_t index)
{
   auto puff = e_puffTable.getObjectKeyAndTypeEx<MetaTable>(index);
   if(!puff)
      return e_puffTable.getObjectKeyAndTypeEx<MetaTable>(GameModeInfo->puffType);
   return puff;
}

// EOF
