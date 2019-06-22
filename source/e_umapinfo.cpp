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

#include "z_zone.h"
#include "Confuse/confuse.h"
#include "e_edfmetatable.h"
#include "e_umapinfo.h"
#include "in_lude.h"

// metakey vocabulary
#define ITEM_UMAPINFO_LEVELNAME "levelname"
#define ITEM_UMAPINFO_LEVELPIC "levelpic"
#define ITEM_UMAPINFO_NEXT "next"
#define ITEM_UMAPINFO_NEXTSECRET "nextsecret"
#define ITEM_UMAPINFO_SKYTEXTURE "skytexture"
#define ITEM_UMAPINFO_MUSIC "music"
#define ITEM_UMAPINFO_EXITPIC "exitpic"
#define ITEM_UMAPINFO_ENTERPIC "enterpic"
#define ITEM_UMAPINFO_PARTIME "partime"
#define ITEM_UMAPINFO_ENDGAME "endgame"
#define ITEM_UMAPINFO_ENDPIC "endpic"
#define ITEM_UMAPINFO_ENDBUNNY "endbunny"
#define ITEM_UMAPINFO_ENDCAST "endcast"
#define ITEM_UMAPINFO_NOINTERMISSION "nointermission"
#define ITEM_UMAPINFO_INTERTEXT "intertext"
#define ITEM_UMAPINFO_INTERTEXTSECRET "intertextsecret"
#define ITEM_UMAPINFO_INTERBACKDROP "interbackdrop"
#define ITEM_UMAPINFO_INTERMUSIC "intermusic"
#define ITEM_UMAPINFO_EPISODE "episode"
#define ITEM_UMAPINFO_BOSSACTION "bossaction"

#define ITEM_EPISODE_PATCH "patch"
#define ITEM_EPISODE_NAME "name"
#define ITEM_EPISODE_KEY "key"

#define ITEM_BOSSACTION_THINGTYPE "thingtype"
#define ITEM_BOSSACTION_LINESPECIAL "linespecial"
#define ITEM_BOSSACTION_TAG "tag"

// Interned metatable keys
MetaKeyIndex keyUmapinfoLevelName(ITEM_UMAPINFO_LEVELNAME);
MetaKeyIndex keyUmapinfoLevelPic(ITEM_UMAPINFO_LEVELPIC);
MetaKeyIndex keyUmapinfoNext(ITEM_UMAPINFO_NEXT);
MetaKeyIndex keyUmapinfoNextSecret(ITEM_UMAPINFO_NEXTSECRET);
MetaKeyIndex keyUmapinfoSkyTexture(ITEM_UMAPINFO_SKYTEXTURE);
MetaKeyIndex keyUmapinfoMusic(ITEM_UMAPINFO_MUSIC);
MetaKeyIndex keyUmapinfoExitPic(ITEM_UMAPINFO_EXITPIC);
MetaKeyIndex keyUmapinfoEnterPic(ITEM_UMAPINFO_ENTERPIC);
MetaKeyIndex keyUmapinfoParTime(ITEM_UMAPINFO_PARTIME);
MetaKeyIndex keyUmapinfoEndGame(ITEM_UMAPINFO_ENDGAME);
MetaKeyIndex keyUmapinfoEndPic(ITEM_UMAPINFO_ENDPIC);
MetaKeyIndex keyUmapinfoEndBunny(ITEM_UMAPINFO_ENDBUNNY);
MetaKeyIndex keyUmapinfoEndCast(ITEM_UMAPINFO_ENDCAST);
MetaKeyIndex keyUmapinfoNoIntermission(ITEM_UMAPINFO_NOINTERMISSION);
MetaKeyIndex keyUmapinfoInterText(ITEM_UMAPINFO_INTERTEXT);
MetaKeyIndex keyUmapinfoInterTextSecret(ITEM_UMAPINFO_INTERTEXTSECRET);
MetaKeyIndex keyUmapinfoInterBackdrop(ITEM_UMAPINFO_INTERBACKDROP);
MetaKeyIndex keyUmapinfoInterMusic(ITEM_UMAPINFO_INTERMUSIC);
MetaKeyIndex keyUmapinfoEpisode(ITEM_UMAPINFO_EPISODE);
MetaKeyIndex keyUmapinfoBossAction(ITEM_UMAPINFO_BOSSACTION);

MetaKeyIndex keyUmapinfoEpisodePatch(ITEM_EPISODE_PATCH);
MetaKeyIndex keyUmapinfoEpisodeName(ITEM_EPISODE_NAME);
MetaKeyIndex keyUmapinfoEpisodeKey(ITEM_EPISODE_KEY);

MetaKeyIndex keyUmapinfoBossActionThingType(ITEM_BOSSACTION_THINGTYPE);
MetaKeyIndex keyUmapinfoBossActionLineSpecial(ITEM_BOSSACTION_LINESPECIAL);
MetaKeyIndex keyUmapinfoBossActionTag(ITEM_BOSSACTION_TAG);

static MetaTable e_umapinfoTable; // the pufftype metatable

//
// Episode entry
//
static cfg_opt_t episode_opts[] =
{
   CFG_STR(ITEM_EPISODE_PATCH, "", CFGF_NONE),
   CFG_STR(ITEM_EPISODE_NAME, "", CFGF_NONE),
   CFG_STR(ITEM_EPISODE_KEY, "", CFGF_NONE),
   CFG_END()
};

static cfg_opt_t bossaction_opts[] =
{
   CFG_STR(ITEM_BOSSACTION_THINGTYPE, "", CFGF_NONE),
   CFG_INT(ITEM_BOSSACTION_LINESPECIAL, 0, CFGF_NONE),
   CFG_INT(ITEM_BOSSACTION_TAG, 0, CFGF_NONE),
   CFG_END()
};

//
// The EDF UMAPINFO property
//
cfg_opt_t edf_umapinfo_opts[] =
{
   CFG_STR(ITEM_UMAPINFO_LEVELNAME, "", CFGF_NONE),
   CFG_STR(ITEM_UMAPINFO_LEVELPIC, "", CFGF_NONE),
   CFG_STR(ITEM_UMAPINFO_NEXT, "", CFGF_NONE),
   CFG_STR(ITEM_UMAPINFO_NEXTSECRET, "", CFGF_NONE),
   CFG_STR(ITEM_UMAPINFO_MUSIC, "", CFGF_NONE),
   CFG_STR(ITEM_UMAPINFO_EXITPIC, "", CFGF_NONE),
   CFG_STR(ITEM_UMAPINFO_ENTERPIC, "", CFGF_NONE),
   CFG_INT(ITEM_UMAPINFO_PARTIME, -1, CFGF_NONE),
   CFG_BOOL(ITEM_UMAPINFO_ENDGAME, false, CFGF_NONE),
   CFG_STR(ITEM_UMAPINFO_ENDPIC, "", CFGF_NONE),
   CFG_BOOL(ITEM_UMAPINFO_ENDBUNNY, false, CFGF_NONE),
   CFG_BOOL(ITEM_UMAPINFO_ENDCAST, false, CFGF_NONE),
   CFG_BOOL(ITEM_UMAPINFO_NOINTERMISSION, false, CFGF_NONE),
   CFG_STR(ITEM_UMAPINFO_INTERTEXT, 0, CFGF_LIST),
   CFG_STR(ITEM_UMAPINFO_INTERTEXTSECRET, 0, CFGF_LIST),
   CFG_STR(ITEM_UMAPINFO_INTERBACKDROP, "", CFGF_NONE),
   CFG_STR(ITEM_UMAPINFO_INTERMUSIC, "", CFGF_NONE),
   CFG_MVPROP(ITEM_UMAPINFO_EPISODE, episode_opts, CFGF_MULTI|CFGF_NOCASE),
   CFG_MVPROP(ITEM_UMAPINFO_BOSSACTION, bossaction_opts, CFGF_MULTI|CFGF_NOCASE),
   CFG_END()
};

//
// Process the UMAPINFO settings
//
void E_ProcessUmapinfo(cfg_t *cfg)
{
   E_BuildGlobalMetaTableFromEDF(cfg, EDF_SEC_UMAPINFO, nullptr, e_umapinfoTable);
}

//
// Get from name
//
const MetaTable *E_UmapinfoForName(const char *name)
{
   return e_umapinfoTable.getObjectKeyAndTypeEx<MetaTable>(name);
}

//
// Builds the intermission information from UMAPINFO
//
void E_BuildInterUmapinfo()
{
   MetaTable *level = nullptr;
   while((level = e_umapinfoTable.getNextTypeEx(level)))
   {
      intermapinfo_t &info = IN_GetMapInfo(level->getKey());

      const char *str = level->getString(keyUmapinfoLevelName, "");
      if(estrnonempty(str))
         info.levelname = str;

      str = level->getString(keyUmapinfoLevelPic, "");
      if(estrnonempty(str))
         info.levelpic = str;

      str = level->getString(keyUmapinfoEnterPic, "");
      if(estrnonempty(str))
         info.enterpic = str;

      str = level->getString(keyUmapinfoExitPic, "");
      if(estrnonempty(str))
         info.exitpic = str;
   }
}

// EOF
