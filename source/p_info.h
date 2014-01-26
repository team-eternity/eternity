// Emacs style mode select -*- C++ -*-
//---------------------------------------------------------------------------
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
//--------------------------------------------------------------------------

#ifndef P_INFO_H__
#define P_INFO_H__

#include "doomdef.h"

void P_LoadLevelInfo(int lumpnum, const char *lvname);

void P_CreateMetaInfo(int map, const char *levelname, int par, const char *mus, 
                      int next, int secr, bool finale, const char *intertext,
                      int mission, const char *interpic);

void P_AddSndInfoMusic(int mapnum, const char *lumpname);
const char *P_GetSndInfoMusic(int mapnum);
void P_AddMusInfoMusic(const char *mapname, int number, const char *lump);
const char *P_GetMusInfoMusic(const char *mapname, int number);

// boss special flags
enum
{
   BSPEC_MAP07_1 = 0x00000001,
   BSPEC_MAP07_2 = 0x00000002,
   BSPEC_E1M8    = 0x00000004,
   BSPEC_E2M8    = 0x00000008,
   BSPEC_E3M8    = 0x00000010,
   BSPEC_E4M6    = 0x00000020,
   BSPEC_E4M8    = 0x00000040,
   BSPEC_E5M8    = 0x00000080,
};

//
// LevelInfo Field Enumeration
//
// haleyjd 06/21/10: This is needed to keep track of what fields are stored in
// a LevelInfo prototype. The order does not have to match that in the 
// structure, but there must be one enumeration value for every structure 
// field.
//
enum
{
   LI_FIELD_LEVELTYPE,
   LI_FIELD_BOSSSPECS,
   LI_FIELD_PARTIME,
   LI_FIELD_INTERPIC,
   LI_FIELD_INTERTEXT,
   LI_FIELD_INTERTEXTLUMP,
   LI_FIELD_BACKDROP,
   LI_FIELD_INTERMUSIC,
   LI_FIELD_LEVELPIC,
   LI_FIELD_NEXTLEVELPIC,
   LI_FIELD_NEXTSECRETPIC,
   LI_FIELD_FINALETYPE,
   LI_FIELD_KILLSTATS,
   LI_FIELD_KILLFINALE,
   LI_FIELD_FINALESECRETONLY,
   LI_FIELD_ENDOFGAME,
   LI_FIELD_USEEDFINTERNAME,
   LI_FIELD_NEXTLEVEL,
   LI_FIELD_NEXTSECRET,
   LI_FIELD_LEVELNAME,
   LI_FIELD_MUSICNAME,
   LI_FIELD_COLORMAP,
   LI_FIELD_OUTDOORFOG,
   LI_FIELD_USEFULLBRIGHT,
   LI_FIELD_UNEVENLIGHT,
   LI_FIELD_SKYNAME,
   LI_FIELD_ALTSKYNAME,
   LI_FIELD_SKY2NAME,
   LI_FIELD_DOUBLESKY,
   LI_FIELD_HASLIGHTNING,
   LI_FIELD_SKYDELTA,
   LI_FIELD_SKY2DELTA,
   LI_FIELD_GRAVITY,
   LI_FIELD_CREATOR,
   LI_FIELD_HASSCRIPT,
   LI_FIELD_SCRIPTLUMP,
   LI_FIELD_ACSSCRIPTLUMP,
   LI_FIELD_EXTRADATA,
   LI_FIELD_SOUNDSWTCHN,
   LI_FIELD_SOUNDSWTCHX,
   LI_FIELD_SOUNDSTNMOV,
   LI_FIELD_SOUNDPSTOP,
   LI_FIELD_SOUNDBDCLS,
   LI_FIELD_SOUNDBDOPN,
   LI_FIELD_SOUNDDOROPN,
   LI_FIELD_SOUNDDORCLS,
   LI_FIELD_SOUNDPSTART,
   LI_FIELD_SOUNDFCMOVE,
   LI_FIELD_NOAUTOSEQUENCES,
   LI_FIELD_DEFENVIRONMENT,
   
   // count of fields
   LI_FIELD_NUMFIELDS
};

//
// LevelInfo_t
//
// haleyjd: This structure is a singleton and now contains all of what were
// previously a bunch of messy global variables.
//
struct LevelInfo_t
{
   // map type selection
   int mapFormat;            // format of the map (Doom or Hexen)
   int levelType;            // level type used for conversions, etc.

   // specials: lines, sectors, etc.
   unsigned int bossSpecs;  // boss special flags for BossDeath, HticBossDeath

   // intermission and finale stuff
   int  partime;              // intermission par time in seconds
   const char *interPic;      // intermission background
   const char *interText;     // presence of this determines if a finale will occur
   const char *interTextLump; // this can be set in the file
   const char *backDrop;      // pic used during text finale
   const char *interMusic;    // text finale music
   const char *levelPic;      // intermission level name graphics lump
   const char *nextLevelPic;  // level name lump for NEXT level 06/17/06
   const char *nextSecretPic; // level name lump for SECRET level 06/17/06
   int  finaleType;           // type of finale sequence -- haleyjd 05/26/06
   bool killStats;            // level has no statistics intermission
   bool killFinale;           // level has no finale even if text is given
   bool finaleSecretOnly;     // level only has finale if secret exit
   bool endOfGame;            // DOOM II: last map, trigger cast call
   bool useEDFInterName;      // use an intermission map name from EDF

   // level transfer stuff
   const char *nextLevel;     // name of next map for normal exit
   const char *nextSecret;    // name of next map for secret exit

   // level variables
   const char *levelName;     // name used in automap
   const char *musicName;     // name of music to play during level

   // color map stuff
   const char *colorMap;      // global colormap replacement
   const char *outdoorFog;    // outdoor fogmap -- 03/04/07
   bool useFullBright;        // use fullbright on this map?
   bool unevenLight;          // use uneven wall lighting?

   // sky stuff
   const char *skyName;       // normal sky name (F_SKY1 or top of double)
   const char *altSkyName;    // alt sky - replaces skyName during lightning
   const char *sky2Name;      // secondary sky (F_SKY2 or bottom of double)
   bool doubleSky;            // use hexen-style double skies?
   bool hasLightning;         // map has lightning flashes?
   int  skyDelta;             // double-sky scroll speeds (units/tic)
   int  sky2Delta;

   // misc
   int gravity;               // gravity factor
   const char *creator;       // creator: name of who made this map

   // attached scripts
   char *acsScriptLump;       // name of ACS script lump, for DOOM-format maps
   char *extraData;           // name of ExtraData lump

   // per-level sound replacements
   const char *sound_swtchn;  // switch on
   const char *sound_swtchx;  // switch off
   const char *sound_stnmov;  // plane move
   const char *sound_pstop;   // plat stop
   const char *sound_bdcls;   // blazing door close
   const char *sound_bdopn;   // blazing door open
   const char *sound_doropn;  // normal door open
   const char *sound_dorcls;  // normal door close
   const char *sound_pstart;  // plat start
   const char *sound_fcmove;  // floor/ceiling move

   // sound sequences
   bool noAutoSequences;      // auto sequence behavior

   // sound environments
   int  defaultEnvironment;   // ID of default sound environment
};

// the one and only LevelInfo object
extern LevelInfo_t LevelInfo;

extern int default_weaponowned[NUMWEAPONS];

// killough 5/2/98: moved from d_deh.c:
// Par times (new item with BOOM)
extern int pars[][10];  // hardcoded array size
extern int cpars[];     // hardcoded array size

int P_DoomParTime(int episode, int map);
int P_Doom2ParTime(int episode, int map);
int P_NoParTime(int episode, int map);

struct levelnamedata_t;

void P_DoomDefaultLevelName(levelnamedata_t &lnd);
void P_Doom2DefaultLevelName(levelnamedata_t &lnd);
void P_HticDefaultLevelName(levelnamedata_t &lnd);

#endif

// EOF

