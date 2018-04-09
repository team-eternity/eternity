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

#include "z_zone.h"
#include "Confuse/confuse.h"
#include "e_puff.h"

// Differences we have so far:
// thingtype
// random Z?
// sound
// alternate pufftype for hitting
// vertical speed
// whether it appears on hit target
// chance to splat bloot on hit
// override state for small range (just use "melee")
// particle support? How many?

#define ITEM_PUFF_THINGTYPE "thingtype"
#define ITEM_PUFF_SOUND "sound"
#define ITEM_PUFF_DAMAGEPUFFTYPE "damagepufftype"
#define ITEM_PUFF_UPSPEED "upspeed"
#define ITEM_PUFF_BLOODCHANCE "bloodchance"
#define ITEM_PUFF_PUNCHSTATE "punchstate"

#define ITEM_PUFF_RANDOMTICS "RANDOMTICS"
#define ITEM_PUFF_RANDOMZ "RANDOMZ"
#define ITEM_PUFF_PUFFHIT "PUFFHIT"
#define ITEM_PUFF_SMOKEPARTICLES "SMOKEPARTICLES"

//
// EDF puff options
//
cfg_opt_t edf_puff_opts[] =
{
   CFG_STR(ITEM_PUFF_THINGTYPE, "", CFGF_NONE),
   CFG_STR(ITEM_PUFF_SOUND, "", CFGF_NONE),
   CFG_STR(ITEM_PUFF_DAMAGEPUFFTYPE, "", CFGF_NONE),
   CFG_FLOAT(ITEM_PUFF_UPSPEED, 0, CFGF_NONE),
   CFG_FLOAT(ITEM_PUFF_BLOODCHANCE, 1, CFGF_NONE), // TODO: support percent
   CFG_STR(ITEM_PUFF_PUNCHSTATE, "", CFGF_NONE),

   CFG_FLAG(ITEM_PUFF_RANDOMTICS, 0, CFGF_SIGNPREFIX),
   CFG_FLAG(ITEM_PUFF_RANDOMZ, 0, CFGF_SIGNPREFIX),
   CFG_FLAG(ITEM_PUFF_PUFFHIT, 0, CFGF_SIGNPREFIX),
   CFG_FLAG(ITEM_PUFF_SMOKEPARTICLES, 0, CFGF_SIGNPREFIX)
};

// EOF
