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

#ifndef E_PUFF_H_
#define E_PUFF_H_

#define EDF_SEC_PUFFTYPE "pufftype"
#define EDF_SEC_PUFFDELTA "puffdelta"

#include "metaapi.h"

struct cfg_opt_t;
struct cfg_t;
class MetaTable;

static const double puffZSpreadDefault = 4;

// NOTE: some of these properties are post-processes from the EDF appearance.
extern MetaKeyIndex keyPuffThingType;
extern MetaKeyIndex keyPuffSound;
extern MetaKeyIndex keyPuffAltDamagePuff;
extern MetaKeyIndex keyPuffUpSpeed;
extern MetaKeyIndex keyPuffZSpread;
extern MetaKeyIndex keyPuffPunchHack;
extern MetaKeyIndex keyPuffRandomTics;
extern MetaKeyIndex keyPuffPuffHit;
extern MetaKeyIndex keyPuffParticles;

extern cfg_opt_t edf_puff_opts[];
extern cfg_opt_t edf_puff_delta_opts[];

void E_ProcessPuffs(cfg_t *cfg);
const MetaTable *E_PuffForName(const char *name);
const MetaTable *E_PuffForIndex(size_t index);

#endif

// EOF
