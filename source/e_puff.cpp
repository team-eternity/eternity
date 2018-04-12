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
#define ITEM_PUFF_THINGTYPE "thingtype"
#define ITEM_PUFF_SOUND "sound"
#define ITEM_PUFF_ALTDAMAGEPUFF "altdamagepuff"
#define ITEM_PUFF_UPSPEED "upspeed"
#define ITEM_PUFF_BLOODCHANCE "bloodchance"
#define ITEM_PUFF_PUNCHSTATE "punchstate"

#define ITEM_PUFF_RANDOMTICS "RANDOMTICS"
#define ITEM_PUFF_RANDOMZ "RANDOMZ"
#define ITEM_PUFF_PUFFHIT "PUFFHIT"
#define ITEM_PUFF_SMOKEPARTICLES "SMOKEPARTICLES"

// Interned metatable keys
MetaKeyIndex keyPuffThingType(ITEM_PUFF_THINGTYPE);
MetaKeyIndex keyPuffSound(ITEM_PUFF_SOUND);
MetaKeyIndex keyPuffAltDamagePuff(ITEM_PUFF_ALTDAMAGEPUFF);
MetaKeyIndex keyPuffUpSpeed(ITEM_PUFF_UPSPEED);
MetaKeyIndex keyPuffBloodChance(ITEM_PUFF_BLOODCHANCE);
MetaKeyIndex keyPuffPunchState(ITEM_PUFF_PUNCHSTATE);
MetaKeyIndex keyPuffRandomTics(ITEM_PUFF_RANDOMTICS);
MetaKeyIndex keyPuffRandomZ(ITEM_PUFF_RANDOMZ);
MetaKeyIndex keyPuffPuffHit(ITEM_PUFF_PUFFHIT);
MetaKeyIndex keyPuffSmokeParticles(ITEM_PUFF_SMOKEPARTICLES);

#define PUFF_CONFIGS \
CFG_STR(ITEM_PUFF_THINGTYPE, "", CFGF_NONE),            \
CFG_STR(ITEM_PUFF_SOUND, "none", CFGF_NONE),            \
CFG_STR(ITEM_PUFF_ALTDAMAGEPUFF, "", CFGF_NONE),        \
CFG_FLOAT(ITEM_PUFF_UPSPEED, 0, CFGF_NONE),             \
CFG_FLOAT(ITEM_PUFF_BLOODCHANCE, 1, CFGF_NONE),         \
CFG_STR(ITEM_PUFF_PUNCHSTATE, "", CFGF_NONE),           \
CFG_FLAG(ITEM_PUFF_RANDOMTICS, 0, CFGF_SIGNPREFIX),     \
CFG_FLAG(ITEM_PUFF_RANDOMZ, 0, CFGF_SIGNPREFIX),        \
CFG_FLAG(ITEM_PUFF_PUFFHIT, 0, CFGF_SIGNPREFIX),        \
CFG_FLAG(ITEM_PUFF_SMOKEPARTICLES, 0, CFGF_SIGNPREFIX), \

//
// EDF puff options
//
cfg_opt_t edf_puff_opts[] =
{
   CFG_TPROPS(edf_generic_tprops, CFGF_NOCASE),
   PUFF_CONFIGS
   CFG_END()
};

cfg_opt_t edf_puff_delta_opts[] =
{
   CFG_STR("name", 0, CFGF_NONE),
   PUFF_CONFIGS
   CFG_END()
};

static MetaTable e_puffTable; // the pufftype metatable

//
// Process the pufftype settings
//
void E_ProcessPuffs(cfg_t *cfg)
{
   E_BuildGlobalMetaTableFromEDF(cfg, EDF_SEC_PUFFTYPE, EDF_SEC_PUFFDELTA,
                                 e_puffTable);
}

//
// Get a puff for name
//
const MetaTable *E_SafePuffForName(const char *name)
{
   auto puff = e_puffTable.getObjectKeyAndTypeEx<MetaTable>(name);
   if(!puff)
      return e_puffTable.getObjectKeyAndTypeEx<MetaTable>(GameModeInfo->puffType);
   return puff;
}

const MetaTable *E_SafePuffForIndex(size_t index)
{
   auto puff = e_puffTable.getObjectKeyAndTypeEx<MetaTable>(index);
   if(!puff)
      return e_puffTable.getObjectKeyAndTypeEx<MetaTable>(GameModeInfo->puffType);
   return puff;
}

// EOF
