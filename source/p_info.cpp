// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley, Simon Howard, et al.
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
//----------------------------------------------------------------------------
//
// Level info.
//
// Under SMMU, level info is stored in the level marker: ie. "mapxx"
// or "exmx" lump. This contains new info such as: the level name, music
// lump to be played, par time etc.
//
// By Simon Howard
//
// Eternity Modifications:
// -----------------------
// As of 07/22/04, Eternity can now support global MapInfo lumps in
// addition to MapInfo placed in level headers. Header MapInfo always
// overrides any global lump data. The global lumps cascade in such
// a way that a map will get its global MapInfo from the newest 
// "EMAPINFO" lump that contains a block with the map's name.
//
// I have also moved much of the initialization code from many modules
// into this file, making the LevelInfo structure the default place to
// get map information, rather than an alternative to many other
// sources as it was previously. This simplifies code outside this
// module and encapsulates more level-dependent decisions here.
//
// -- haleyjd
//
//-----------------------------------------------------------------------------

/* includes ************************/

#include "z_zone.h"
#include "i_system.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_deh.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_io.h"
#include "doomstat.h"
#include "doomdef.h"
#include "dstrings.h"
#include "e_lib.h"
#include "e_hash.h"
#include "e_sound.h"
#include "f_finale.h"
#include "g_game.h"
#include "m_collection.h"
#include "m_dllist.h"
#include "m_qstr.h"
#include "m_qstrkeys.h"
#include "m_misc.h"
#include "metaapi.h"
#include "p_info.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_levels.h"
#include "w_wad.h"
#include "xl_mapinfo.h"

// need wad iterators
#include "w_iterator.h"

extern char gamemapname[9];

// haleyjd: moved everything into the LevelInfo struct
LevelInfo_t LevelInfo;

static void P_ParseLevelInfo(WadDirectory *dir, int lumpnum);

static int  P_ParseInfoCmd(qstring *line);
static void P_ParseLevelVar(qstring *cmd);

static void P_ClearLevelVars();
static void P_InitWeapons();

// Hexen MAPINFO
static void P_applyHexenMapInfo();

// post-processing routine prototypes
static void P_LoadInterTextLumps();
static void P_SetSky2Texture();
static void P_SetParTime();
static void P_SetInfoSoundNames();
static void P_SetOutdoorFog();

static char *P_openWadTemplate(const char *wadfile, int *len);
static char *P_findTextInTemplate(char *text, int len, int titleOrAuthor);

// haleyjd 05/30/10: struct for info read from a metadata file
struct metainfo_t
{
   int         level;      // level to use on
   const char *levelname;  // name
   int         partime;    // par time
   const char *musname;    // music name
   int         nextlevel;  // next level #, only used if non-0
   int         nextsecret; // next secret #, only used if non-0
   bool        finale;     // if true, sets LevelInfo.endOfGame
   const char *intertext;  // only used if finale is true
   const char *interpic;   // interpic, if not NULL
   int         mission;    // if non-zero, only applies during a mission pack
};

static int nummetainfo, nummetainfoalloc;
static metainfo_t *metainfo;
static metainfo_t *curmetainfo;

static metainfo_t *P_GetMetaInfoForLevel(int mapnum);

static enum lireadtype_e
{
   RT_LEVELINFO,
   RT_OTHER,
} readtype;

static enum limode_e
{
   LI_MODE_GLOBAL, // global: we're reading a global MapInfo lump
   LI_MODE_LEVEL,  // level:  we're reading a level header
   LI_NUMMODES
} limode;

static bool foundEEMapInfo;

// haleyjd: flag set for boss specials
static dehflags_t boss_spec_flags[] =
{
   { "MAP07_1", BSPEC_MAP07_1 },
   { "MAP07_2", BSPEC_MAP07_2 },
   { "E1M8",    BSPEC_E1M8 },
   { "E2M8",    BSPEC_E2M8 },
   { "E3M8",    BSPEC_E3M8 },
   { "E4M6",    BSPEC_E4M6 },
   { "E4M8",    BSPEC_E4M8 },
   { "E5M8",    BSPEC_E5M8 },
   { NULL,      0 }
};

static dehflagset_t boss_flagset =
{
   boss_spec_flags, // flaglist
   0,               // mode: single flags word
};

struct textvals_t
{
   const char **vals;
   int numvals;
   int defaultval;
};

static const char *finaleTypeStrs[] =
{
   "text",
   "doom_credits",
   "doom_deimos",
   "doom_bunny",
   "doom_marine",
   "htic_credits",
   "htic_water",
   "htic_demon",
};

static textvals_t finaleTypeVals =
{
   finaleTypeStrs,
   FINALE_NUMFINALES,
   0
};

//
// P_LoadLevelInfo
//
// Parses global MapInfo lumps looking for the first entry that
// matches this map, then parses the map's own header MapInfo if
// it has any. This is the main external function of this module.
// Called from P_SetupLevel.
//
// lvname, if non-null, is the filepath of the wad that contains
// the level when it has been loaded from a managed wad directory.
//
void P_LoadLevelInfo(int lumpnum, const char *lvname)
{
   // set all the level defaults
   P_ClearLevelVars();

   // override with info from text file?
   if(lvname)
   {
      int len = 0;
      char *text = P_openWadTemplate(lvname, &len);
      char *str;

      if(text && len > 0)
      {
         // look for title
         if((str = P_findTextInTemplate(text, len, 0)))
            LevelInfo.levelName = str;

         // look for author
         if((str = P_findTextInTemplate(text, len, 1)))
            LevelInfo.creator = str;
      }
   }

   // parse global lumps
   limode = LI_MODE_GLOBAL;
   foundEEMapInfo = false;

   // run down the hash chain for EMAPINFO
   WadChainIterator wci(wGlobalDir, "EMAPINFO");
   for(wci.begin(); wci.current(); wci.next())
   {
      if(wci.testLump(lumpinfo_t::ns_global))
      {
         // reset the parser state
         readtype = RT_OTHER;
         P_ParseLevelInfo(&wGlobalDir, (*wci)->selfindex);
         if(foundEEMapInfo) // parsed an entry for this map, so stop
            break;
      }
   }

   // parse level lump
   limode   = LI_MODE_LEVEL;
   readtype = RT_OTHER;
   P_ParseLevelInfo(&wGlobalDir, lumpnum);

   // haleyjd 01/26/14: if no EE map information was specified for this map,
   // defer to any defined Hexen MAPINFO data now.
   if(!foundEEMapInfo)
      P_applyHexenMapInfo();
   
   // haleyjd: call post-processing routines
   P_LoadInterTextLumps();
   P_SetSky2Texture();
   P_SetParTime();
   P_SetInfoSoundNames();
   P_SetOutdoorFog();

   P_InitWeapons();
}

//
// P_ParseLevelInfo
//
// Parses one individual MapInfo lump.
//
static void P_ParseLevelInfo(WadDirectory *dir, int lumpnum)
{
   qstring line;
   char *lump, *rover;
   int  size;

   // haleyjd 03/12/05: seriously restructured to eliminate last line
   // problem and to use qstring to buffer lines
   
   // if lump is zero size, we are done
   if(!(size = dir->lumpLength(lumpnum)))
      return;

   // allocate lump buffer with size + 2 to allow for termination
   size += 2;
   lump = (char *)(Z_Malloc(size, PU_STATIC, NULL));
   dir->readLump(lumpnum, lump);

   // terminate lump data with a line break and null character;
   // this makes uniform parsing much easier
   lump[size - 2] = '\n';
   lump[size - 1] = '\0';

   rover = lump;

   while(*rover)
   {
      if(*rover == '\n') // end of line
      {
         // hack for global MapInfo: if P_ParseInfoCmd returns -1,
         // we can break out of parsing early
         if(P_ParseInfoCmd(&line) == -1)
            break;
         line.clear(); // clear line buffer
      }
      else
         line += *rover; // add char to line buffer

      ++rover;
   }

   // free the lump
   Z_Free(lump);
}

//
// P_ParseInfoCmd
//
// Parses a single line of a MapInfo lump.
//
static int P_ParseInfoCmd(qstring *line)
{
   unsigned int len;
   const char *label = NULL;

   line->replace("\t\r\n", ' '); // erase any control characters
   line->toLower();              // make everything lowercase
   line->lstrip(' ');            // strip spaces at beginning
   line->rstrip(' ');            // strip spaces at end
   
   if(!(len = line->length()))   // ignore totally empty lines
      return 0;

   // detect comments at beginning
   if(line->charAt(0) == '#' || line->charAt(0) == ';' || 
      (len > 1 && line->charAt(0) == '/' && line->charAt(1) == '/'))
      return 0;
     
   if((label = line->strChr('[')))  // a new section separator
   {
      ++label;

      if(limode == LI_MODE_GLOBAL)
      {
         // when in global mode, returning -1 will make
         // P_ParseLevelInfo break out early, saving time
         if(foundEEMapInfo)
            return -1;

         if(!strncasecmp(label, gamemapname, strlen(gamemapname)))
         {
            foundEEMapInfo = true;
            readtype = RT_LEVELINFO;
         }
      }
      else
      {
         if(!strncmp(label, "level info", 10))
         {
            foundEEMapInfo = true;
            readtype = RT_LEVELINFO;
         }
      }

      // go to next line immediately
      return 0;
   }
  
   switch(readtype)
   {
   case RT_LEVELINFO:
      P_ParseLevelVar(line);
      break;

   case RT_OTHER:
      break;
   }

   return 0;
}

//
//  Level vars: level variables in the [level info] section.
//
//  Takes the form:
//     [variable name] = [value]
//
//  '=' sign is optional
//

enum
{
   IVT_STRING,
   IVT_STRNUM,
   IVT_INT,
   IVT_BOOLEAN,
   IVT_FLAGS,
   IVT_ENVIRONMENT,
   IVT_END
};

struct levelvar_t
{
   int         type;
   const char *name;
   void       *variable;
   void       *extra;
};


//
// levelvars field prototype macros
//
// These provide a mapping between keywords and the fields of the LevelInfo
// structure.
//
#define LI_STRING(name, field) \
   { IVT_STRING, name, (void *)(&(LevelInfo . field)), NULL }

#define LI_STRNUM(name, field, extra) \
   { IVT_STRNUM, name, (void *)(&(LevelInfo . field)), &(extra) }

#define LI_INTEGR(name, field) \
   { IVT_INT, name, &(LevelInfo . field), NULL }

#define LI_BOOLNF(name, field) \
   { IVT_BOOLEAN, name, &(LevelInfo . field), NULL }

#define LI_FLAGSF(name, field, extra) \
   { IVT_FLAGS, name, &(LevelInfo . field), &(extra) }

#define LI_ENVIRO(name, field) \
   { IVT_ENVIRONMENT, name, &(LevelInfo . field), NULL }

#define LI_END() \
   { IVT_END, NULL, NULL }

levelvar_t levelvars[]=
{
   LI_STRING("acsscript",          acsScriptLump),
   LI_STRING("altskyname",         altSkyName),
   LI_FLAGSF("boss-specials",      bossSpecs,         boss_flagset),
   LI_STRING("colormap",           colorMap),
   LI_STRING("creator",            creator),
   LI_ENVIRO("defaultenvironment", defaultEnvironment),
   LI_BOOLNF("doublesky",          doubleSky),
   LI_BOOLNF("edf-intername",      useEDFInterName),
   LI_BOOLNF("endofgame",          endOfGame),
   LI_STRING("extradata",          extraData),
   LI_BOOLNF("finale-normal",      finaleNormalOnly),
   LI_BOOLNF("finale-secret",      finaleSecretOnly), 
   LI_BOOLNF("finale-early",       finaleEarly),
   LI_STRNUM("finaletype",         finaleType,        finaleTypeVals),
   LI_STRNUM("finalesecrettype",   finaleSecretType,  finaleTypeVals),
   LI_BOOLNF("fullbright",         useFullBright),
   LI_INTEGR("gravity",            gravity),
   LI_STRING("inter-backdrop",     backDrop),
   LI_STRING("intermusic",         interMusic),
   LI_STRING("interpic",           interPic),
   LI_STRING("intertext",          interTextLump),
   LI_STRING("intertext-secret",   interTextSLump),
   LI_BOOLNF("killfinale",         killFinale),
   LI_BOOLNF("killstats",          killStats),
   LI_STRING("levelname",          levelName),
   LI_STRING("levelpic",           levelPic),
   LI_STRING("levelpicnext",       nextLevelPic),
   LI_STRING("levelpicsecret",     nextSecretPic),
   LI_BOOLNF("lightning",          hasLightning),
   LI_STRING("music",              musicName),
   LI_STRING("nextlevel",          nextLevel),
   LI_STRING("nextsecret",         nextSecret),
   LI_BOOLNF("noautosequences",    noAutoSequences),
   LI_STRING("outdoorfog",         outdoorFog),
   LI_INTEGR("partime",            partime),
   LI_INTEGR("skydelta",           skyDelta),
   LI_INTEGR("sky2delta",          sky2Delta),
   LI_STRING("skyname",            skyName),
   LI_STRING("sky2name",           sky2Name),
   LI_STRING("sound-swtchn",       sound_swtchn),
   LI_STRING("sound-swtchx",       sound_swtchx),
   LI_STRING("sound-stnmov",       sound_stnmov),
   LI_STRING("sound-pstop",        sound_pstop),
   LI_STRING("sound-bdcls",        sound_bdcls),
   LI_STRING("sound-bdopn",        sound_bdopn),
   LI_STRING("sound-dorcls",       sound_dorcls),
   LI_STRING("sound-doropn",       sound_doropn),
   LI_STRING("sound-pstart",       sound_pstart),
   LI_STRING("sound-fcmove",       sound_fcmove),
   LI_BOOLNF("unevenlight",        unevenLight),

   //{ IVT_STRING,  "defaultweapons", &info_weapons },
   
   LI_END() // must be last
};

// lexer state enumeration for P_ParseLevelVar
enum
{
   STATE_VAR,
   STATE_BETWEEN,
   STATE_VAL
};

//
// P_parseLevelString
//
// Parse a plain string value.
//
static void P_parseLevelString(levelvar_t *var, const qstring &value)
{
   *(char**)var->variable = value.duplicate(PU_LEVEL);
}

//
// P_parseLevelStrNum
//
// Parse an enumerated keyword value into its corresponding integral value.
//
static void P_parseLevelStrNum(levelvar_t *var, const qstring &value)
{
   textvals_t *tv = (textvals_t *)var->extra;
   int val = E_StrToNumLinear(tv->vals, tv->numvals, value.constPtr());

   if(val >= tv->numvals)
      val = tv->defaultval;

   *(int *)var->variable = val;
}

//
// P_parseLevelInt
//
// Parse an integer value.
//
static void P_parseLevelInt(levelvar_t *var, const qstring &value)
{
   *(int*)var->variable = value.toInt();
}

//
// P_parseLevelBool
//
// Parse a boolean true/false option.
//
static void P_parseLevelBool(levelvar_t *var, const qstring &value)
{
   *(bool *)var->variable = !value.strCaseCmp("true");
}

//
// P_parseLevelFlags
//
// Parse a BEX-style flags string.
//
static void P_parseLevelFlags(levelvar_t *var, const qstring &value)
{
   dehflagset_t *flagset = (dehflagset_t *)var->extra;
   *(unsigned int *)var->variable = E_ParseFlags(value.constPtr(), flagset);
}

enum
{
   ENV_STATE_SCANSTART,
   ENV_STATE_INID1,
   ENV_STATE_SCANBETWEEN,
   ENV_STATE_INID2,
   ENV_STATE_END
};

//
// P_parseLevelEnvironment
//
// Parse a sound environment ID.
//
static void P_parseLevelEnvironment(levelvar_t *var, const qstring &value)
{
   int state = ENV_STATE_SCANSTART;
   qstring id1, id2;
   const char *rover = value.constPtr();

   // parse with a small state machine
   while(state != ENV_STATE_END)
   {
      char c = *rover++;
      int isnum = ectype::isDigit(c);
      
      if(!c)
         break;

      switch(state)
      {
      case ENV_STATE_SCANSTART:
         if(isnum)
         {
            id1 += c;
            state = ENV_STATE_INID1;
         }
         break;
      case ENV_STATE_INID1:
         if(isnum)
            id1 += c;
         else
            state = ENV_STATE_SCANBETWEEN;
         break;
      case ENV_STATE_SCANBETWEEN:
         if(isnum)
         {
            id2 += c;
            state = ENV_STATE_INID2;
         }
         break;
      case ENV_STATE_INID2:
         if(isnum)
            id2 += c;
         else
            state = ENV_STATE_END;
         break;
      }
   }

   *(int *)var->variable = (id1.toInt() << 8) | id2.toInt();
}

typedef void (*varparserfn_t)(levelvar_t *, const qstring &);
static varparserfn_t infoVarParsers[IVT_END] =
{
   P_parseLevelString,
   P_parseLevelStrNum,
   P_parseLevelInt,
   P_parseLevelBool,
   P_parseLevelFlags,
   P_parseLevelEnvironment
};

//
// P_ParseLevelVar
//
// Tokenizes the line parsed by P_ParseInfoCmd and then sets
// any appropriate matching MapInfo variable to the retrieved
// value.
//
static void P_ParseLevelVar(qstring *cmd)
{
   int state = 0;
   qstring var, value;
   char c;
   const char *rover = cmd->constPtr();
   levelvar_t *current = levelvars;

   // haleyjd 03/12/05: seriously restructured to remove possible
   // overflow of static buffer and bad kludges used to separate
   // the variable and value tokens -- now uses qstring.

   while((c = *rover++))
   {
      switch(state)
      {
      case STATE_VAR: // in variable -- read until whitespace or = is hit
         if(c == ' ' || c == '=')
            state = STATE_BETWEEN;
         else
            var += c;
         continue;
      case STATE_BETWEEN: // between -- skip whitespace or =
         if(c != ' ' && c != '=')
         {
            state = STATE_VAL;
            value += c;
         }
         continue;
      case STATE_VAL: // value -- everything else goes here
         value += c;
         continue;
      default:
         I_Error("P_ParseLevelVar: undefined state in lexer\n");
      }
   }

   // detect some syntax errors
   if(state != STATE_VAL || !var.length() || !value.length())
      return;

   // TODO: improve linear search? fixed small set, so may not matter
   while(current->type != IVT_END)
   {
      if(!var.strCaseCmp(current->name))
         infoVarParsers[current->type](current, value);
      current++;
   }
}

//
// Default Setup and Post-Processing Routines
//

enum SynthType_e
{
   SYNTH_NEWLEVEL,    // "new level"
   SYNTH_HIDDENLEVEL, // "hidden level"
   SYNTH_NUMTYPES
};

static const char *synthNames[SYNTH_NUMTYPES] =
{
   "%s: New Level",
   "%s: Hidden Level"
};

struct levelnamedata_t
{
   const char  *bexName;   // BEX mnemonic to use
   SynthType_e  synthType; // synthesis type to use
};

//
// SynthLevelName
//
// Makes up a level name for new maps and Heretic secrets.
// Moved here from hu_stuff.c
//
static void SynthLevelName(SynthType_e levelNameType)
{
   // haleyjd 12/14/01: halved size of this string, max length
   // is deterministic since gamemapname is 8 chars long
   static char newlevelstr[25];

   sprintf(newlevelstr, synthNames[levelNameType], gamemapname);
   
   LevelInfo.levelName = newlevelstr;
}

//
// P_BFGNameHacks
//
// haleyjd 04/29/13: This will run once when we're in pack_disk, 
// and will check to see if HUSTR 31 or 32 have been DEH/BEX modified. 
// If so they're left alone. Otherwise, we replace those strings in the
// BEX string table with different values.
//
static void P_BFGNameHacks()
{
   static bool firsttime = true;

   // Only applies to DOOM II BFG Edition
   if(!(GameModeInfo->missionInfo->flags & MI_WOLFNAMEHACKS))
      return;

   if(firsttime)
   {
      firsttime = false;
      if(!DEH_StringChanged("HUSTR_31"))
         DEH_ReplaceString("HUSTR_31", BFGHUSTR_31);
      if(!DEH_StringChanged("HUSTR_32"))
         DEH_ReplaceString("HUSTR_32", BFGHUSTR_32);
   }
}

//
// P_DoomDefaultLevelName
//
// GameModeInfo routine for DOOM gamemode default level names.
//
void P_DoomDefaultLevelName(levelnamedata_t &lnd)
{
   if(isExMy(gamemapname) &&
      gameepisode > 0 && gameepisode <= GameModeInfo->numEpisodes &&
      gamemap > 0 && gamemap <= 9)
   {
      lnd.bexName = GameModeInfo->levelNames[(gameepisode-1)*9+gamemap-1];
   }
}

//
// P_Doom2DefaultLevelName
//
// GameModeInfo routine for DOOM II default level names.
//
void P_Doom2DefaultLevelName(levelnamedata_t &lnd)
{
   int d2upperbound = 32;

   // haleyjd 04/29/13: a couple hacks for BFG Edition authenticity
   P_BFGNameHacks();

   // haleyjd 11/03/12: in pack_disk, we have 33 map names.
   // This is also allowed for subsitution through BEX mnemonic HUSTR_33.
   if(DEH_StringChanged("HUSTR_33") ||
      (GameModeInfo->missionInfo->flags & MI_HASBETRAY))
      d2upperbound = 33;

   if(isMAPxy(gamemapname) && gamemap > 0 && gamemap <= d2upperbound)
      lnd.bexName = GameModeInfo->levelNames[gamemap-1];   
}

//
// P_HticDefaultLevelName
//
// GameModeInfo routine for Heretic default level names.
//
void P_HticDefaultLevelName(levelnamedata_t &lnd)
{
   int maxEpisode = GameModeInfo->numEpisodes;

   if(isExMy(gamemapname) &&
      gameepisode > 0 && gameepisode <= maxEpisode &&
      gamemap > 0 && gamemap <= 9)
   {
      // For Heretic, the last episode isn't "real" and contains
      // "secret" levels -- a name is synthesized for those
      // 10/10/05: don't catch shareware here
      if(maxEpisode == 1 || gameepisode < maxEpisode)
         lnd.bexName = GameModeInfo->levelNames[(gameepisode-1)*9+gamemap-1];
      else
         lnd.synthType = SYNTH_HIDDENLEVEL; // put "hidden level"
   }
}

//
// P_InfoDefaultLevelName
//
// Figures out the name to use for this map.
// Moved here from hu_stuff.c
//
static void P_InfoDefaultLevelName()
{
   levelnamedata_t lnd = { NULL, SYNTH_NEWLEVEL };
   bool deh_modified = false;

   // if we have a current metainfo, use its level name
   if(curmetainfo)
   {
      LevelInfo.levelName = curmetainfo->levelname;
      return;
   }

   // invoke gamemode-specific retrieval function
   GameModeInfo->GetLevelName(lnd);

   if(lnd.bexName)
      deh_modified = DEH_StringChanged(lnd.bexName);

   if((newlevel && !deh_modified) || !lnd.bexName)
      SynthLevelName(lnd.synthType);
   else
      LevelInfo.levelName = DEH_String(lnd.bexName);
}

#define NUMMAPINFOSOUNDS 10

static const char *DefSoundNames[NUMMAPINFOSOUNDS] =
{
   "EE_DoorOpen",
   "EE_DoorClose",
   "EE_BDoorOpen",
   "EE_BDoorClose",
   "EE_SwitchOn",
   "EE_SwitchEx",
   "EE_PlatStart",
   "EE_PlatStop",
   "EE_PlatMove",
   "EE_FCMove",
};

static sfxinfo_t *DefSoundAliases[NUMMAPINFOSOUNDS][2];

// Yes, an array of pointers-to-pointers. Your eyes do not deceive you.
// It's just easiest to do it this way.

static const char **infoSoundPtrs[NUMMAPINFOSOUNDS] =
{
   &LevelInfo.sound_doropn,
   &LevelInfo.sound_dorcls,
   &LevelInfo.sound_bdopn,
   &LevelInfo.sound_bdcls,
   &LevelInfo.sound_swtchn,
   &LevelInfo.sound_swtchx,
   &LevelInfo.sound_pstart,
   &LevelInfo.sound_pstop,
   &LevelInfo.sound_stnmov,
   &LevelInfo.sound_fcmove,
};

//
// P_applyHexenMapInfo
//
// haleyjd 01/26/14: Applies data from Hexen MAPINFO
//
static void P_applyHexenMapInfo()
{
   MetaTable *xlmi;
   const char *s;
   int i;

   if(!(xlmi = XL_MapInfoForMapName(gamemapname)))
      return;

   LevelInfo.levelName = xlmi->getString("name", "");
   
   // sky textures
   if((s = xlmi->getString("sky1", NULL)))
   {
      LevelInfo.skyName  = s;
      LevelInfo.skyDelta = xlmi->getInt("sky1delta", 0);
   }
   if((s = xlmi->getString("sky2", NULL)))
   {
      LevelInfo.sky2Name  = s;
      LevelInfo.sky2Delta = xlmi->getInt("sky2delta", 0);
   }

   // double skies
   if((i = xlmi->getInt("doublesky", -1)) >= 0)
      LevelInfo.doubleSky = !!i;

   // lightning
   if((i = xlmi->getInt("lightning", -1)) >= 0)
      LevelInfo.hasLightning = !!i;

   // colormap
   if((s = xlmi->getString("fadetable", NULL)))
      LevelInfo.colorMap = s;

   // TODO: cluster, warptrans

   // next map
   if((s = xlmi->getString("next", NULL)))
      LevelInfo.nextLevel = s;

   // next secret
   if((s = xlmi->getString("secretnext", NULL)))
      LevelInfo.nextSecret = s;

   // titlepatch for intermission
   if((s = xlmi->getString("titlepatch", NULL)))
      LevelInfo.levelPic = s;

   // TODO: cdtrack

   // par time
   if((i = xlmi->getInt("par", -1)) >= 0)
      LevelInfo.partime = i;

   if((s = xlmi->getString("music", NULL)))
      LevelInfo.musicName = s;

   // flags
   if((i = xlmi->getInt("nointermission", -1)) >= 0)
      LevelInfo.killStats = !!i;
   if((i = xlmi->getInt("evenlighting", -1)) >= 0)
      LevelInfo.unevenLight = !i;
   if((i = xlmi->getInt("noautosequences", -1)) >= 0)
      LevelInfo.noAutoSequences = !!i;

   /*
   Stuff with "Unfinished Business":
   qstring name;
   qstring next;
   qstring secretnext;
   qstring titlepatch;
   */
}

//
// P_InfoDefaultSoundNames
//
// Restores the alias fields of the sounds used by the sound sequence engine
// for default effects.
//
static void P_InfoDefaultSoundNames()
{
   static bool firsttime = true;
   int i;

   // if first time, save pointers to the sounds and their aliases
   if(firsttime)
   {
      firsttime = false;

      for(i = 0; i < NUMMAPINFOSOUNDS; i++)
      {
         sfxinfo_t *sfx = E_SoundForName(DefSoundNames[i]);

         DefSoundAliases[i][0] = sfx;
         DefSoundAliases[i][1] = sfx ? sfx->alias : NULL;
      }
   }
   else
   {
      // restore defaults
      for(i = 0; i < NUMMAPINFOSOUNDS; i++)
      {
         if(DefSoundAliases[i][0])
            DefSoundAliases[i][0]->alias = DefSoundAliases[i][1];
      }
   }

   // set sound names to defaults
   for(i = 0; i < NUMMAPINFOSOUNDS; i++)
   {
      if(DefSoundAliases[i][0] && DefSoundAliases[i][0]->alias)
         *infoSoundPtrs[i] = DefSoundAliases[i][0]->alias->mnemonic;
      else
         *infoSoundPtrs[i] = "none";
   }
}

//
// P_SetInfoSoundNames
//
// Post-processing routine.
//
// Changes aliases in sounds used by the sound sequence engine for the default
// sequences to point at the sounds defined by MapInfo.
//
static void P_SetInfoSoundNames()
{
   int i;

   for(i = 0; i < NUMMAPINFOSOUNDS; i++)
   {
      const char *name = *infoSoundPtrs[i];

      if(DefSoundAliases[i][0])
         DefSoundAliases[i][0]->alias = E_SoundForName(name);
   }
}

//
// P_loadTextLump
//
// Load a text lump as a null-terminated C string with PU_LEVEL cache tag.
//
static char *P_loadTextLump(const char *lumpname)
{
   int lumpNum, lumpLen;
   char *str;

   lumpNum = W_GetNumForName(lumpname);
   lumpLen = W_LumpLength(lumpNum);

   str = emalloctag(char *, lumpLen + 1, PU_LEVEL, NULL);

   wGlobalDir.readLump(lumpNum, str);

   // null-terminate the string
   str[lumpLen] = '\0';

   return str;
}

//
// P_LoadInterTextLumps
//
// Post-processing routine.
//
// If the intertext lump name was set via MapInfo, this loads the
// lump and sets LevelInfo.interText to it.
// Moved here from f_finale.c
//
static void P_LoadInterTextLumps()
{
   if(LevelInfo.interTextLump)
      LevelInfo.interText = P_loadTextLump(LevelInfo.interTextLump);

   if(LevelInfo.interTextSLump)
      LevelInfo.interTextSecret = P_loadTextLump(LevelInfo.interTextSLump);
}

//
// P_InfoDefaultFinale
//
// Sets up default MapInfo values related to f_finale.c code.
// Moved here from f_finale.c and altered for new features, etc.
//
// haleyjd: rewritten 07/03/09 to tablify info through GameModeInfo.
// haleyjd: merged finaleType here 01/09/10
//
static void P_InfoDefaultFinale()
{
   finaledata_t *fdata   = GameModeInfo->finaleData;
   finalerule_t *rule    = fdata->rules;
   finalerule_t *theRule = NULL;

   // universal defaults
   LevelInfo.interTextLump    = NULL;
   LevelInfo.interTextSLump   = NULL;
   LevelInfo.finaleNormalOnly = false;
   LevelInfo.finaleSecretOnly = false;
   LevelInfo.finaleEarly      = false;
   LevelInfo.killFinale       = false;
   LevelInfo.killStats        = false;
   LevelInfo.endOfGame        = false;
   LevelInfo.useEDFInterName  = false;

   // set data from the finaledata object
   if(fdata->musicnum != mus_None)
      LevelInfo.interMusic = (GameModeInfo->s_music[fdata->musicnum]).name;
   else
      LevelInfo.interMusic = NULL;

   // check killStats flag - a bit of a hack, this is for Heretic's "hidden"
   // levels in episode 6, which have no statistics intermission.
   if(fdata->killStatsHack)
   {
      if(!(GameModeInfo->flags & GIF_SHAREWARE) && 
         gameepisode >= GameModeInfo->numEpisodes)
         LevelInfo.killStats = true;
   }

   // look for a rule that applies to this level in particular
   for(; rule->gameepisode != -2; rule++)
   {
      if((rule->gameepisode == -1 || rule->gameepisode == gameepisode) &&
         (rule->gamemap == -1 || rule->gamemap == gamemap))
      {
         theRule = rule;
         break;
      }
   }

   if(theRule)
   {
      // set backdrop graphic, intertext, and finale type
      LevelInfo.backDrop   = DEH_String(rule->backDrop);
      LevelInfo.interText  = DEH_String(rule->interText);
      LevelInfo.finaleType = rule->finaleType;

      // check for endOfGame flag
      if(rule->endOfGame)
         LevelInfo.endOfGame = true;

      // check for secretOnly flag
      if(rule->secretOnly)
         LevelInfo.finaleSecretOnly = true;

      // check for early flag
      if(rule->early)
         LevelInfo.finaleEarly = true;

      // allow metainfo overrides
      if(curmetainfo)
      {
         if(curmetainfo->finale)
         {
            LevelInfo.interText = curmetainfo->intertext;
            LevelInfo.endOfGame = true;
         }
         else
            LevelInfo.interText = NULL; // disable other levels
      }
   }
   else
   {
      // This shouldn't really happen but I'll be proactive about it.
      // The finale data sets all have rules to cover defaulted maps.

      // haleyjd: note -- this cannot use F_SKY1 like it did in BOOM
      // because heretic.wad contains an invalid F_SKY1 flat. This 
      // caused crashes during development of Heretic support, so now
      // it uses the F_SKY2 flat which is provided in eternity.wad.
      LevelInfo.backDrop   = "F_SKY2";
      LevelInfo.interText  = NULL;
      LevelInfo.finaleType = FINALE_TEXT;
   }

   // finale type for secret exits starts unspecified; if left that way and a
   // finale occurs for a secret exit anyway, the normal finale will be used.
   LevelInfo.finaleSecretType = FINALE_UNSPECIFIED;
   LevelInfo.interTextSecret  = NULL;
}


//
// P_InfoDefaultSky
//
// Sets the default sky texture for the level.
// Moved here from r_sky.c
//
static void P_InfoDefaultSky()
{
   skydata_t *sd      = GameModeInfo->skyData;
   skyrule_t *rule    = sd->rules;
   skyrule_t *theRule = NULL;

   // DOOM determines the sky texture to be used
   // depending on the current episode, and the game version.
   // haleyjd 01/07/10: use GameModeInfo ruleset

   for(; rule->data != -2; rule++)
   {
      switch(sd->testtype)
      {
      case GI_SKY_IFEPISODE:
         if(rule->data == gameepisode || rule->data == -1)
            theRule = rule;
         break;
      case GI_SKY_IFMAPLESSTHAN:
         if(gamemap < rule->data || rule->data == -1)
            theRule = rule;
         break;
      default:
         I_Error("P_InfoDefaultSky: bad microcode %d in skyrule set\n", sd->testtype);
      }

      if(theRule) // break out if we've found one
         break;
   }

   // we MUST have a default sky rule
   if(theRule)
      LevelInfo.skyName = theRule->skyTexture;
   else
      I_Error("P_InfoDefaultSky: no default rule in skyrule set\n");

   // set sky2Name to NULL now, we'll test it later
   LevelInfo.sky2Name = NULL;
   LevelInfo.doubleSky = false; // double skies off by default

   // sky deltas for Hexen-style double skies - default to 0
   LevelInfo.skyDelta  = 0;
   LevelInfo.sky2Delta = 0;

   // altSkyName -- this is used for lightning flashes --
   // starts out NULL to indicate none.
   LevelInfo.altSkyName = NULL;
}

//
// P_InfoDefaultBossSpecials
//
// haleyjd 03/14/05: Sets the default boss specials for DOOM, DOOM II,
// and Heretic. These are the actions triggered for various maps by the
// BossDeath and HticBossDeath codepointers. They can now be enabled for
// any map by using the boss-specials variable.
//
static void P_InfoDefaultBossSpecials()
{
   bspecrule_t *rule = GameModeInfo->bossRules;

   // most maps have no specials, so set to zero here
   LevelInfo.bossSpecs = 0;

   // look for a level-specific default
   for(; rule->episode != -1; rule++)
   {
      if(gameepisode == rule->episode && gamemap == rule->map)
      {
         LevelInfo.bossSpecs = rule->flags;
         return;
      }
   }
}

//
// P_InfoDefaultMusic
//
// haleyjd 12/22/11: Considers various sources for a default value of musicname
// for the current map.
//
static void P_InfoDefaultMusic(metainfo_t *curmetainfo)
{
   const char *sndInfoLump;

   // Default to undefined
   LevelInfo.musicName = ""; 

   // Is there a diskfile metainfo?
   if(curmetainfo)
      LevelInfo.musicName = curmetainfo->musname;

   // Is there a Hexen SNDINFO definition?
   if((sndInfoLump = P_GetSndInfoMusic(gamemap)))
      LevelInfo.musicName = sndInfoLump;
}

//
// P_SetSky2Texture
//
// Post-processing routine.
//
// Sets the sky2Name, if it is still NULL, to whatever 
// skyName has ended up as. This may be the default, or 
// the value from MapInfo.
//
static void P_SetSky2Texture()
{
   if(!LevelInfo.sky2Name)
      LevelInfo.sky2Name = LevelInfo.skyName;
}

// DOOM Par Times
int pars[4][10] =
{
   { 0 },                                             // dummy row
   { 0,  30,  75, 120,  90, 165, 180, 180,  30, 165 }, // episode 1
   { 0,  90,  90,  90, 120,  90, 360, 240,  30, 170 }, // episode 2
   { 0,  90,  45,  90, 150,  90,  90, 165,  30, 135 }  // episode 3
};

// DOOM II Par Times
int cpars[34] = 
{
    30,  90, 120, 120,  90, 150, 120, 120, 270,  90,  //  1-10
   210, 150, 150, 150, 210, 150, 420, 150, 210, 150,  // 11-20
   240, 150, 180, 150, 150, 300, 330, 420, 300, 180,  // 21-30
   120,  30,  30,  30                                 // 31-34
};

//
// P_DoomParTime
//
// GameModeInfo routine to get partimes for Doom gamemodes
//
int P_DoomParTime(int episode, int map)
{
   if(episode >= 1 && episode <= 3 && map >= 1 && map <= 9)
      return TICRATE * pars[episode][map];
   else
      return -1;
}

//
// P_Doom2ParTime
//
// GameModeInfo routine to get partimes for Doom II gamemodes
//
int P_Doom2ParTime(int episode, int map)
{
   if(map >= 1 && map <= 34)
      return TICRATE * cpars[map - 1];
   else
      return -1;
}

//
// P_NoParTime
//
// GameModeInfo routine for gamemodes which do not have partimes.
// Includes Heretic, Hexen, and Strife.
//
int P_NoParTime(int episode, int map)
{
   return 0;
}

//
// P_SetParTime
//
// Post-processing routine.
//
// Sets the map's par time, allowing for DeHackEd replacement and
// status as a "new" level. Moved here from g_game.c
//
static void P_SetParTime()
{
   if(LevelInfo.partime == -1) // if not set via MapInfo already
   {
      if(!newlevel || deh_pars) // if not a new map, OR deh pars loaded
         LevelInfo.partime = GameModeInfo->GetParTime(gameepisode, gamemap);
   }
   else
      LevelInfo.partime *= TICRATE; // multiply MapInfo value by TICRATE
}

//
// P_SetOutdoorFog
//
// Post-processing routine
//
// If the global fog map has not been set, set it the same as the global
// colormap for the level.
//
static void P_SetOutdoorFog()
{
   if(LevelInfo.outdoorFog == NULL)
      LevelInfo.outdoorFog = LevelInfo.colorMap;
}

//
// P_ClearLevelVars
//
// Clears all the level variables so that none are left over from a
// previous level. Calls all the default construction functions
// defined above. Post-processing routines are called *after* the
// MapInfo processing is complete, and thus some of the values set
// here are used to detect whether or not MapInfo set a value.
//
static void P_ClearLevelVars()
{
   static char nextlevel[10];
   static char nextsecret[10];

   // find a metainfo for the level if one exists
   curmetainfo = P_GetMetaInfoForLevel(gamemap);

   // set default level type depending on game mode
   LevelInfo.levelType = GameModeInfo->levelType;

   LevelInfo.levelPic        = NULL;
   LevelInfo.nextLevelPic    = NULL;
   LevelInfo.nextSecretPic   = NULL;
   LevelInfo.creator         = "unknown";

   if(curmetainfo) // metadata-overridable intermission defaults
   {
      LevelInfo.interPic = curmetainfo->interpic;
      LevelInfo.partime  = curmetainfo->partime;
   }
   else
   {
      LevelInfo.interPic = GameModeInfo->interPic;
      LevelInfo.partime  = -1;
   }

   LevelInfo.colorMap        = "COLORMAP";
   LevelInfo.outdoorFog      = NULL;
   LevelInfo.useFullBright   = true;
   LevelInfo.unevenLight     = true;
   
   LevelInfo.hasLightning    = false;
   LevelInfo.nextSecret      = "";
   //info_weapons            = "";
   LevelInfo.gravity         = DEFAULTGRAVITY;
   LevelInfo.acsScriptLump   = NULL;
   LevelInfo.extraData       = NULL;
   
   // Hexen TODO: will be true for Hexen maps by default
   LevelInfo.noAutoSequences = false;

   // haleyjd: default sound environment zone
   LevelInfo.defaultEnvironment = 0;

   // haleyjd: construct defaults
   P_InfoDefaultLevelName();
   P_InfoDefaultSoundNames();
   P_InfoDefaultFinale();
   P_InfoDefaultSky();
   P_InfoDefaultBossSpecials();
   P_InfoDefaultMusic(curmetainfo);
   
   // special handling for ExMy maps under DOOM II
   if(GameModeInfo->id == commercial && isExMy(levelmapname))
   {
      LevelInfo.nextLevel = nextlevel;
      
      // set the next episode
      strcpy(nextlevel, levelmapname);
      nextlevel[3]++;
      if(nextlevel[3] > '9')  // next episode
      {
         nextlevel[3] = '1';
         nextlevel[1]++;
      }
      LevelInfo.musicName = levelmapname;
   }
   else
   {
      // allow metainfo override for nextlevel
      if(curmetainfo && curmetainfo->nextlevel)
      {
         memset(nextlevel, 0, sizeof(nextlevel));
         psnprintf(nextlevel, sizeof(nextlevel), "MAP%02d", curmetainfo->nextlevel);
         LevelInfo.nextLevel = nextlevel;
      }
      else
         LevelInfo.nextLevel = "";
   }

   // allow metainfo override for nextsecret
   if(curmetainfo && curmetainfo->nextsecret)
   {
      memset(nextsecret, 0, sizeof(nextsecret));
      psnprintf(nextsecret, sizeof(nextsecret), "MAP%02d", curmetainfo->nextsecret);
      LevelInfo.nextSecret = nextsecret;
   }

   // haleyjd 08/31/12: Master Levels mode hacks
   if(inmanageddir == MD_MASTERLEVELS && GameModeInfo->type == Game_DOOM)
      LevelInfo.interPic = "INTRMLEV";
}

int default_weaponowned[NUMWEAPONS];

// haleyjd: note -- this is considered deprecated and is a
// candidate for replacement/rewrite
// WEAPON_FIXME: mapinfo weapons.

static void P_InitWeapons()
{
#if 0
   char *s;
#endif
   
   memset(default_weaponowned, 0, sizeof(default_weaponowned));
#if 0   
   s = info_weapons;
   
   while(*s)
   {
      switch(*s)
      {
      case '3': default_weaponowned[wp_shotgun] = true; break;
      case '4': default_weaponowned[wp_chaingun] = true; break;
      case '5': default_weaponowned[wp_missile] = true; break;
      case '6': default_weaponowned[wp_plasma] = true; break;
      case '7': default_weaponowned[wp_bfg] = true; break;
      case '8': default_weaponowned[wp_supershotgun] = true; break;
      default: break;
      }
      s++;
   }
#endif
}

//=============================================================================
//
// Meta Info
//
// Meta info is only loaded from diskfiles and may override LevelInfo defaults.
// 

//
// P_GetMetaInfoForLevel
//
// Finds a metainfo object for the given map number, if one exists (returns 
// NULL otherwise).
//
static metainfo_t *P_GetMetaInfoForLevel(int mapnum)
{
   metainfo_t *mi = NULL;

   for(int i = 0; i < nummetainfo; i++)
   {
      // Check for map number match, and mission match, if a mission
      // is required to be active in order to use this metadata.
      if(metainfo[i].level == mapnum && metainfo[i].mission == inmanageddir)
      {
         mi = &metainfo[i];
         break;
      }
   }

   return mi;
}

//
// P_CreateMetaInfo
//
// Creates a metainfo object for a given map with all of the various data that
// can be defined in metadata.txt files. This is called from some code in 
// d_main.c that deals with special ".disk" files that contain an IWAD and
// possible PWAD(s) that originate from certain console versions of DOOM.
//
void P_CreateMetaInfo(int map, const char *levelname, int par, const char *mus, 
                      int next, int secr, bool finale, const char *intertext,
                      int mission, const char *interpic)
{
   metainfo_t *mi;

   if(nummetainfo >= nummetainfoalloc)
   {
      nummetainfoalloc = nummetainfoalloc ? nummetainfoalloc * 2 : 10;
      metainfo = erealloc(metainfo_t *, metainfo, nummetainfoalloc * sizeof(metainfo_t));
   }

   mi = &metainfo[nummetainfo];

   mi->level      = map;
   mi->levelname  = levelname;
   mi->partime    = par;
   mi->musname    = mus;
   mi->nextlevel  = next;
   mi->nextsecret = secr;
   mi->finale     = finale;
   mi->intertext  = intertext;
   mi->mission    = mission;
   mi->interpic   = interpic;

   ++nummetainfo;
}

//=============================================================================
//
// SNDINFO Music Definitions
//

struct sndinfomus_t
{
   DLListItem<sndinfomus_t> links;
   int   mapnum;
   char *lumpname;
};

static EHashTable<sndinfomus_t, EIntHashKey, 
                  &sndinfomus_t::mapnum, &sndinfomus_t::links> sndInfoMusHash(101);

//
// P_AddSndInfoMusic
//
// Adds a music definition from a Hexen SNDINFO lump.
//
void P_AddSndInfoMusic(int mapnum, const char *lumpname)
{
   sndinfomus_t *newmus;

   // If one already exists, modify it. Otherwise create a new one.
   if((newmus = sndInfoMusHash.objectForKey(mapnum)))
   {
      efree(newmus->lumpname);
      newmus->lumpname = estrdup(lumpname);
   }
   else
   {
      newmus = estructalloc(sndinfomus_t, 1);

      newmus->mapnum   = mapnum;
      newmus->lumpname = estrdup(lumpname);

      sndInfoMusHash.addObject(newmus);
   }

   // Make sure that this music appears in the music selection menu by asking
   // for a musicinfo_t from the sound engine. This'll add a musicinfo structure
   // for it right now, rather than waiting for the music to actually be played.
   S_MusicForName(newmus->lumpname);
}

// 
// P_GetSndInfoMusic
//
// If a Hexen SNDINFO music definition exists for the passed-in map number,
// the lumpname to use will be passed in. Otherwise, NULL is returned.
//
const char *P_GetSndInfoMusic(int mapnum)
{
   const char   *lumpname = NULL;
   sndinfomus_t *music    = NULL;

   if((music = sndInfoMusHash.objectForKey(mapnum)))
      lumpname = music->lumpname;

   return lumpname;
}

//=============================================================================
//
// MUSINFO Music Definitions
//

struct musinfomap_t
{
   int   num;
   char *lump;
};

class MusInfoMusic : public ZoneObject
{
public:
   DLListItem<MusInfoMusic>    links;
   PODCollection<musinfomap_t> maps;
   qstring mapname;
};

static EHashTable<MusInfoMusic, ENCQStrHashKey, 
                  &MusInfoMusic::mapname, &MusInfoMusic::links> musInfoMusHash(101);

//
// P_AddMusInfoMusic
//
// Add a single music definition from a Risen3D MUSINFO lump.
//
void P_AddMusInfoMusic(const char *mapname, int number, const char *lump)
{
   MusInfoMusic *music = NULL;

   // Does it exist already?
   if((music = musInfoMusHash.objectForKey(mapname)))
   {
      int nummaps = music->maps.getLength();
      bool foundnum = false;

      // Does it have an entry for this number already?
      for(int i = 0; i < nummaps; i++)
      {
         if(music->maps[i].num == number)
         {
            E_ReplaceString(music->maps[i].lump, estrdup(lump));
            foundnum = true;
            break;
         }
      }

      if(!foundnum) // Add a new one.
      {
         musinfomap_t newmap;
         newmap.num  = number;
         newmap.lump = estrdup(lump);
         music->maps.add(newmap);
      }
   }
   else
   {
      // Create a new MUSINFO entry.
      MusInfoMusic *newMusInfo = new MusInfoMusic();
      newMusInfo->mapname = mapname;

      // Create a new subentry.
      musinfomap_t newmap;
      newmap.num = number;
      newmap.lump = estrdup(lump);
      newMusInfo->maps.add(newmap);

      // Add it to the hash table.
      musInfoMusHash.addObject(newMusInfo);
   }
}

// 
// P_GetMusInfoMusic
//
// If a Risen3D MUSINFO music definition exists for the passed-in map name,
// the lumpname to use will be passed in. Otherwise, NULL is returned.
//
const char *P_GetMusInfoMusic(const char *mapname, int number)
{
   const char   *lumpname = NULL;
   MusInfoMusic *music    = NULL;

   if((music = musInfoMusHash.objectForKey(mapname)))
   {
      size_t nummaps = music->maps.getLength();

      for(size_t i = 0; i < nummaps; i++)
      {
         if(music->maps[i].num == number)
         {
            lumpname = music->maps[i].lump;
            break;
         }
      }
   }

   return lumpname;
}

//=============================================================================
//
// Wad Template Parsing
//
// haleyjd 06/16/10: For situations where we can get information from a wad
// file's corresponding template text file.
//

//
// P_openWadTemplate
//
// Input:  The filepath to the wad file for the current level.
// Output: The buffered text from the corresponding wad template file, if the
//         file was found. NULL otherwise.
//
static char *P_openWadTemplate(const char *wadfile, int *len)
{
   char *fn = Z_Strdupa(wadfile);
   char *dotloc = NULL;
   byte *buffer = NULL;

   // find an extension if it has one, and see that it is ".wad"
   if((dotloc = strrchr(fn, '.')) && !strcasecmp(dotloc, ".wad"))
   {
      strcpy(dotloc, ".txt");    // try replacing .wad with .txt

      if(access(fn, R_OK))       // no?
      {
         strcpy(dotloc, ".TXT"); // try with .TXT (for You-neeks systems 9_9)
         if(access(fn, R_OK))
            return NULL;         // oh well, tough titties.
      }
   }

   return (*len = M_ReadFile(fn, &buffer)) < 0 ? NULL : (char *)buffer;
}

// template parsing states
enum
{
   TMPL_STATE_START,   // at beginning
   TMPL_STATE_QUOTE,   // saw a quote first off (for Sverre levels)
   TMPL_STATE_TITLE,   // reading in "Title" identifier
   TMPL_STATE_SPACE,   // skipping spaces and/or ':' characters
   TMPL_STATE_INTITLE, // parsing out the title
   TMPL_STATE_DONE     // finished successfully
};

typedef struct tmplpstate_s
{
   char *text;
   int i;
   int len;
   int state;
   qstring *tokenbuf;
   int spacecount;
   int titleOrAuthor; // if non-zero, looking for Author, not title
} tmplpstate_t;

static void TmplStateStart(tmplpstate_t *state)
{
   char c = state->text[state->i];

   switch(c)
   {
   case '"':
      if(!state->titleOrAuthor) // only when looking for title...
      {
         state->spacecount = 0;
         state->state = TMPL_STATE_QUOTE; // this is a hack for Sverre levels
      }
      break;
   case 'T':
   case 't':
      if(state->titleOrAuthor)
         break;
      *state->tokenbuf += c;
      state->state = TMPL_STATE_TITLE; // start reading out "Title"
      break;
   case 'A':
   case 'a':
      if(state->titleOrAuthor)
      {
         *state->tokenbuf += c;
         state->state = TMPL_STATE_TITLE; // start reading out "Author"
      }
      break;
   default:
      break; // stay in same state
   }
}

static void TmplStateQuote(tmplpstate_t *state)
{
   char c = state->text[state->i];

   switch(c)
   {
   case ' ':
      if(++state->spacecount == 2)
      {
         *state->tokenbuf += ' ';
         state->spacecount = 0;
      }
      break;
   case '\n':
   case '\r':
   case '"': // done
      state->state = TMPL_STATE_DONE;
      break;
   default:
      state->spacecount = 0;
      *state->tokenbuf += c;
      break;
   }
}

static void TmplStateTitle(tmplpstate_t *state)
{
   char c = state->text[state->i];

   switch(c)
   {
   case ' ':
   case '\n':
   case '\r':
   case '\t':
   case ':':
      // whitespace - check to see if we have "Title" or "Author" in the token buffer
      if(!state->tokenbuf->strCaseCmp(state->titleOrAuthor ? "Author" : "Title"))
      {
         state->tokenbuf->clear();
         state->state = TMPL_STATE_SPACE;
      }
      else
      {
         state->tokenbuf->clear();
         state->state = TMPL_STATE_START; // start over
      }
      break;
   default:
      *state->tokenbuf += c;
      break;
   }
}

static void TmplStateSpace(tmplpstate_t *state)
{
   char c = state->text[state->i];

   switch(c)
   {
   case ' ':
   case ':':
   case '\n':
   case '\r':
   case '\t':
   case '"':
      break; // stay in same state
   default:
      // should be start of title
      *state->tokenbuf += c;
      state->state = TMPL_STATE_INTITLE;
      break;
   }
}

static void TmplStateInTitle(tmplpstate_t *state)
{
   char c = state->text[state->i];

   switch(c)
   {
   case '"':
      if(state->titleOrAuthor) // for authors, quotes are allowed
      {
         *state->tokenbuf += c;
         break;
      }
      // intentional fall-through for titles (end of title)
   case '\n':
   case '\r':
      // done
      state->state = TMPL_STATE_DONE;
      break;
   default:
      *state->tokenbuf += c;
      break;
   }
}

typedef void (*tmplstatefunc_t)(tmplpstate_t *state);

// state functions
static tmplstatefunc_t statefuncs[] =
{
   TmplStateStart,  // TMPL_STATE_START
   TmplStateQuote,  // TMPL_STATE_QUOTE
   TmplStateTitle,  // TMPL_STATE_TITLE
   TmplStateSpace,  // TMPL_STATE_SPACE
   TmplStateInTitle // TMPL_STATE_INTITLE
};

//
// P_findTextInTemplate
//
// haleyjd 06/16/10: Find the title or author of the wad in the template.
//
static char *P_findTextInTemplate(char *text, int len, int titleOrAuthor)
{
   tmplpstate_t state;
   qstring tokenbuffer;
   char *ret = NULL;

   state.text          = text;
   state.len           = len;
   state.i             = 0;
   state.state         = TMPL_STATE_START;
   state.tokenbuf      = &tokenbuffer;
   state.titleOrAuthor = titleOrAuthor;

   while(state.i != len && state.state != TMPL_STATE_DONE)
   {
      statefuncs[state.state](&state);
      state.i++;
   }

   // valid termination states are DONE or INTITLE (though it's pretty unlikely
   // that we would hit EOF in the title string, I'll allow for it)
   if(state.state == TMPL_STATE_DONE || state.state == TMPL_STATE_INTITLE)
      ret = tokenbuffer.duplicate(PU_LEVEL);

   return ret;
}

// EOF

