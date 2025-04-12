//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Eternity EMAPINFO lump processing.
// Authors: Ioan Chera
//

#include "z_zone.h"

#include "d_dehtbl.h"
#include "doomstat.h"
#include "e_lib.h"
#include "e_things.h"
#include "ev_specials.h"
#include "f_finale.h"
#include "m_qstr.h"
#include "metaapi.h"
#include "p_info.h"

//=============================================================================
//
// EMAPINFO Processing
//
// EMAPINFO and level header [level info] data (for SMMU compatibility) is
// now parsed by a finite state automaton parser in the XLParser family.
// Data from EMAPINFO is loaded up for us at startup and any time an archive
// is runtime loaded. Level header info has to be dealt with here if it exists.
// Our job here is to get the MetaTable from the XL system and process the
// fields that are defined in it into the LevelInfo structure.
//


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
   { nullptr,   0 }
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

static const char *finaleTypeStrs[FINALE_NUMFINALES] =
{
   "text",
   "doom_credits",
   "doom_deimos",
   "doom_bunny",
   "doom_marine",
   "htic_credits",
   "htic_water",
   "htic_demon",
   "psx_udoom",
   "psx_doom2",
   "endpic",
};

static textvals_t finaleTypeVals =
{
   finaleTypeStrs,
   FINALE_NUMFINALES,
   0
};

static const char *sectorColormapStrs[] =
{
   "normal",
   "Boom",
   "SMMU"
};

static textvals_t sectorColormapVals =
{
   sectorColormapStrs,
   earrlen(sectorColormapStrs),
   0
};

// levelvar types
enum
{
   IVT_STRING,      // data is stored as a straight string value
   IVT_STRNUM,      // data is a string in EMAPINFO that is converted to an enum value
   IVT_INT,         // data is converted to integer
   IVT_BOOLEAN,     // data is boolean true or false
   IVT_FLAGS,       // data is a BEX-style flags string
   IVT_ENVIRONMENT, // data is a pair of sound environment ID numbers
   IVT_LEVELACTION, // data is a level action string
   IVT_DOUBLEFIXED, // data is floating-point and is loaded as fixed_t
   IVT_END
};

//
// levelvar_t flags
//
enum
{
   LV_MULTI = 1,  // allows multiple definitions
   LV_PLAYSIM = 2,   // influences the playsim, so don't apply it to classic port demos
};

// levelvar structure maps from keys to LevelInfo structure fields
struct levelvar_t
{
   int         type;     // determines how MetaTable value should be parsed
   const char *name;     // key of the MetaTable value
   void       *variable; // pointer to destination LevelInfo field
   void       *extra;    // pointer to any "extra" info needed for this parsing
   unsigned    flags;    // the flags
};

//
// levelvars field prototype macros
//
// These provide a mapping between keywords and the fields of the LevelInfo
// structure.
//
#define LI_STRING(name, field, flags) \
   { IVT_STRING, name, (void *)(&(LevelInfo . field)), nullptr, flags }

#define LI_STRNUM(name, field, extra, flags) \
   { IVT_STRNUM, name, (void *)(&(LevelInfo . field)), &(extra), flags }

#define LI_INTEGR(name, field, flags) \
   { IVT_INT, name, &(LevelInfo . field), nullptr, flags }

#define LI_DOUBLE(name, field) \
   { IVT_DOUBLE, name, &(LevelInfo . field), nullptr, 0 }

#define LI_BOOLNF(name, field, flags) \
   { IVT_BOOLEAN, name, &(LevelInfo . field), nullptr, flags }

#define LI_FLAGSF(name, field, extra, flags) \
   { IVT_FLAGS, name, &(LevelInfo . field), &(extra), flags }

#define LI_ENVIRO(name, field) \
   { IVT_ENVIRONMENT, name, &(LevelInfo . field), nullptr, 0 }

#define LI_ACTION(name, field) \
   { IVT_LEVELACTION, name, &(LevelInfo . field), nullptr, LV_MULTI | LV_PLAYSIM }

#define LI_DBLFIX(name, field, flags) \
   { IVT_DOUBLEFIXED, name, &(LevelInfo . field), nullptr, flags }

static levelvar_t levelvars[]=
{
   LI_BOOLNF("acsopendelay",       acsOpenDelay, LV_PLAYSIM),
   LI_STRING("acsscript",          acsScriptLump, LV_PLAYSIM),
   LI_INTEGR("aircontrol",         airControl, LV_PLAYSIM),
   LI_INTEGR("airfriction",        airFriction, LV_PLAYSIM),
   LI_BOOLNF("allowexittags",      allowExitTags, LV_PLAYSIM),
   LI_BOOLNF("allowsecrettags",    allowSecretTags, LV_PLAYSIM),
   LI_STRING("altskyname",         altSkyName, 0),
   LI_FLAGSF("boss-specials",      bossSpecs,         boss_flagset, LV_PLAYSIM),
   LI_STRING("colormap",           colorMap, 0),
   LI_STRING("creator",            creator, 0),
   LI_ENVIRO("defaultenvironment", defaultEnvironment),
   LI_BOOLNF("disable-jump",       disableJump, LV_PLAYSIM),
   LI_BOOLNF("doublesky",          doubleSky, 0),
   LI_BOOLNF("edf-intername",      useEDFInterName, 0),
   LI_BOOLNF("endofgame",          endOfGame, LV_PLAYSIM),
   LI_STRING("endpic",             endPic, 0),  // not playsim by itself, needs "finaletype endpic"
   LI_STRING("extradata",          extraData, LV_PLAYSIM),
   LI_BOOLNF("finale-normal",      finaleNormalOnly, LV_PLAYSIM),
   LI_BOOLNF("finale-secret",      finaleSecretOnly, LV_PLAYSIM),
   LI_BOOLNF("finale-early",       finaleEarly, LV_PLAYSIM),
   LI_STRNUM("finaletype",         finaleType,        finaleTypeVals, LV_PLAYSIM),
   LI_STRNUM("finalesecrettype",   finaleSecretType,  finaleTypeVals, LV_PLAYSIM),
   LI_BOOLNF("fullbright",         useFullBright, 0),
   LI_INTEGR("gravity",            gravity, LV_PLAYSIM),
   LI_STRING("inter-backdrop",     backDrop, 0),
   LI_STRING("inter-levelname",    interLevelName, 0),
   LI_STRING("intermusic",         interMusic, 0),
   LI_STRING("interpic",           interPic, 0),
   LI_STRING("intertext",          interTextLump, LV_PLAYSIM),
   LI_STRING("intertext-secret",   interTextSLump, LV_PLAYSIM),
   LI_BOOLNF("killfinale",         killFinale, LV_PLAYSIM),
   LI_BOOLNF("killstats",          killStats, LV_PLAYSIM),
   LI_ACTION("levelaction",        actions),
   LI_ACTION("levelaction-bossdeath", actions),
   LI_STRING("levelname",          levelName, 0),
   LI_STRING("levelpic",           levelPic, 0),
   LI_STRING("levelpicnext",       nextLevelPic, 0),
   LI_STRING("levelpicsecret",     nextSecretPic, 0),
   LI_BOOLNF("lightning",        hasLightning, LV_PLAYSIM),  // sadly uses P_Random, so it's playsim
   LI_STRING("music",              musicName, 0),
   LI_STRING("nextlevel",          nextLevel, LV_PLAYSIM),
   LI_STRING("nextsecret",         nextSecret, LV_PLAYSIM),
   LI_BOOLNF("noautosequences",    noAutoSequences, 0),
   LI_STRING("outdoorfog",         outdoorFog, 0),
   LI_INTEGR("partime",            partime, LV_PLAYSIM), // we can't be sure of timing
   LI_STRNUM("sector-colormaps",   sectorColormaps,   sectorColormapVals, 0),
   LI_DBLFIX("skydelta",           skyDelta, 0),
   LI_DBLFIX("sky2delta",          sky2Delta, 0),
   LI_STRING("skyname",            skyName, 0),
   LI_STRING("sky2name",           sky2Name, 0),
   LI_INTEGR("skyrowoffset",       skyRowOffset, 0),
   LI_INTEGR("sky2rowoffset",      sky2RowOffset, 0),
   LI_STRING("sound-swtchn",       sound_swtchn, 0),
   LI_STRING("sound-swtchx",       sound_swtchx, 0),
   LI_STRING("sound-stnmov",       sound_stnmov, 0),
   LI_STRING("sound-pstop",        sound_pstop, 0),
   LI_STRING("sound-bdcls",        sound_bdcls, 0),
   LI_STRING("sound-bdopn",        sound_bdopn, 0),
   LI_STRING("sound-dorcls",       sound_dorcls, 0),
   LI_STRING("sound-doropn",       sound_doropn, 0),
   LI_STRING("sound-pstart",       sound_pstart, 0),
   LI_STRING("sound-fcmove",       sound_fcmove, 0),
   LI_BOOLNF("unevenlight",        unevenLight, 0),

   //{ IVT_STRING,  "defaultweapons", &info_weapons },
};

//
// P_parseLevelString
//
// Parse a plain string value.
//
static void P_parseLevelString(levelvar_t *var, const qstring &value)
{
   *(char**)var->variable = value.duplicate(PU_LEVEL);

   // Hack for sky compatibility
   if(var->variable == &LevelInfo.skyName || var->variable == &LevelInfo.sky2Name)
      LevelInfo.enableBoomSkyHack = false;
}

//
// P_parseLevelStrNum
//
// Parse an enumerated keyword value into its corresponding integral value.
//
static void P_parseLevelStrNum(levelvar_t *var, const qstring &value)
{
   const textvals_t *tv = static_cast<textvals_t *>(var->extra);
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
// Parses a double-precision value and converts it to fixed-point internally
//
static void P_parseLevelDoubleFixed(levelvar_t *var, const qstring &value)
{
   *static_cast<fixed_t *>(var->variable) = M_DoubleToFixed(value.toDouble(nullptr));
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
   dehflagset_t *flagset = static_cast<dehflagset_t *>(var->extra);
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

enum
{
   LVACT_STATE_EXPECTMOBJ,
   LVACT_STATE_INMOBJ,
   LVACT_STATE_EXPECTSPECIAL,
   LVACT_STATE_INSPECIAL,
   LVACT_STATE_EXPECTARGS,
   LVACT_STATE_INARG,
   LVACT_STATE_END
};

//
// P_parseLevelAction
//
// Parse a level action value.
//
static void P_parseLevelAction(levelvar_t *var, const qstring &value)
{
   int state = LVACT_STATE_EXPECTMOBJ;
   const char *rover = value.constPtr();
   qstring mobjName, lineSpec, argStr[5];
   qstring *curArg = nullptr;
   int argcount = 0;
   int args[NUMLINEARGS] = { 0, 0, 0, 0, 0 };
   int mobjType;
   ev_binding_t *binding;

   // parse with a small state machine
   while(state != LVACT_STATE_END)
   {
      char c = *rover++;

      if(!c)
         break;

      switch(state)
      {
      case LVACT_STATE_EXPECTMOBJ:
         if(c == ' ' || c == '\t') // skip whitespace
            continue;
         else                      // start of mobj type name
         {
            mobjName.Putc(c);
            state = LVACT_STATE_INMOBJ;
         }
         break;
      case LVACT_STATE_INMOBJ:
         if(c == ' ' || c == '\t') // end of mobj name
            state = LVACT_STATE_EXPECTSPECIAL;
         else
            mobjName += c;
         break;
      case LVACT_STATE_EXPECTSPECIAL:
         if(c == ' ' || c == '\t') // skip whitespace
            continue;
         else                      // start of special name
         {
            lineSpec.Putc(c);
            state = LVACT_STATE_INSPECIAL;
         }
         break;
      case LVACT_STATE_INSPECIAL:
         if(c == ' ' || c == '\t') // end of special
            state = LVACT_STATE_EXPECTARGS;
         else
            lineSpec += c;
         break;
      case LVACT_STATE_EXPECTARGS:
         if(argcount == NUMLINEARGS) // only up to five arguments allowed
            state = LVACT_STATE_END;
         else if(c == ' ' || c == '\t') // skip whitespace
            continue;
         else
         {
            curArg = &argStr[argcount];
            curArg->Putc(c);
            state = LVACT_STATE_INARG;
         }
         break;
      case LVACT_STATE_INARG:
         if(c == ' ' || c == '\t') // end of argument
         {
            ++argcount;
            state = LVACT_STATE_EXPECTARGS;
         }
         else
            *curArg += c;
         break;
      }
   }

   // lookup thing and special; both must be valid
   if((mobjType = E_ThingNumForName(mobjName.constPtr())) == -1)
      return;
   if(!(binding = EV_UDMFEternityBindingForName(lineSpec.constPtr())))
      return;

   // translate arguments
   for(int i = 0; i < NUMLINEARGS; i++)
      args[i] = argStr[i].toInt();

   // create a new levelaction structure
   auto newAction = estructalloctag(levelaction_t, 1, PU_LEVEL);
   newAction->mobjtype = mobjType;
   newAction->special  = binding->actionNumber;
   memcpy(newAction->args, args, sizeof(args));

   if(!strcasecmp(var->name, "levelaction-bossdeath"))
      newAction->flags |= levelaction_t::BOSS_ONLY;

   // put it onto the linked list in LevelInfo
   newAction->next = LevelInfo.actions;
   LevelInfo.actions = newAction;
}

using varparserfn_t = void (*)(levelvar_t *, const qstring &);
static varparserfn_t infoVarParsers[IVT_END] =
{
   P_parseLevelString,
   P_parseLevelStrNum,
   P_parseLevelInt,
   P_parseLevelBool,
   P_parseLevelFlags,
   P_parseLevelEnvironment,
   P_parseLevelAction,
   P_parseLevelDoubleFixed
};

//
// P_processEMapInfo
//
// Call the proper processing routine for every MetaString object in the
// MapInfo MetaTable.
//
void P_ProcessEMapInfo(MetaTable *info)
{
   for(size_t i = 0; i < earrlen(levelvars); i++)
   {
      levelvar_t *levelvar  = &levelvars[i];
      MetaString *str       = nullptr;

      // Do not process playsim changers in MBF or less mode. Allow cosmetic changes to happen.
      if(levelvar->flags & LV_PLAYSIM && demo_version <= 203)
         continue;

      while((str = info->getNextKeyAndTypeEx<MetaString>(str, levelvar->name)))
      {
         infoVarParsers[levelvar->type](levelvar, qstring(str->getValue()));
         if(!(levelvar->flags & LV_MULTI))
            break;
      }
   }
}

// EOF
