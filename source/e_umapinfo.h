//
// The Eternity Engine
// Copyright (C) 2019 James Haley et al.
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
// Purpose: UMAPINFO in EDF
// Authors: Ioan Chera
//

#ifndef E_UMAPINFO_H_
#define E_UMAPINFO_H_

#include "metaapi.h"

#define EDF_SEC_UMAPINFO "map"

struct cfg_opt_t;
struct cfg_t;

extern MetaKeyIndex keyUmapinfoLevelName;
extern MetaKeyIndex keyUmapinfoLevelPic;
extern MetaKeyIndex keyUmapinfoNext;
extern MetaKeyIndex keyUmapinfoNextSecret;
extern MetaKeyIndex keyUmapinfoSkyTexture;
extern MetaKeyIndex keyUmapinfoMusic;
extern MetaKeyIndex keyUmapinfoExitPic;
extern MetaKeyIndex keyUmapinfoEnterPic;
extern MetaKeyIndex keyUmapinfoParTime;
extern MetaKeyIndex keyUmapinfoEndGame;
extern MetaKeyIndex keyUmapinfoEndPic;
extern MetaKeyIndex keyUmapinfoEndBunny;
extern MetaKeyIndex keyUmapinfoEndCast;
extern MetaKeyIndex keyUmapinfoNoIntermission;
extern MetaKeyIndex keyUmapinfoInterText;
extern MetaKeyIndex keyUmapinfoInterTextSecret;
extern MetaKeyIndex keyUmapinfoInterBackdrop;
extern MetaKeyIndex keyUmapinfoInterMusic;
extern MetaKeyIndex keyUmapinfoEpisode;
extern MetaKeyIndex keyUmapinfoBossAction;

extern MetaKeyIndex keyUmapinfoEpisodePatch;
extern MetaKeyIndex keyUmapinfoEpisodeName;
extern MetaKeyIndex keyUmapinfoEpisodeKey;

extern MetaKeyIndex keyUmapinfoBossActionThingType;
extern MetaKeyIndex keyUmapinfoBossActionLineSpecial;
extern MetaKeyIndex keyUmapinfoBossActionTag;

extern cfg_opt_t edf_umapinfo_opts[];

void E_ProcessUmapinfo(cfg_t *cfg);
const MetaTable *E_UmapinfoForName(const char *name);
void E_BuildInterUmapinfo();

#endif
// EOF
