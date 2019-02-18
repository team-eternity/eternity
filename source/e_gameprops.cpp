// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//----------------------------------------------------------------------------
//
// EDF Gamemode Properties
//
// Allows editing of select properties from the GameModeInfo structure.
//
// James Haley
//
//----------------------------------------------------------------------------

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"

#include "z_zone.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "f_finale.h"
#include "info.h"

#include "e_edf.h"
#include "e_gameprops.h"
#include "e_lib.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h"

//=============================================================================
//
// Data Tables
//

#define ITEM_GPROP_FLAGSADD    "flags.add"
#define ITEM_GPROP_FLAGSREM    "flags.remove"
#define ITEM_GPROP_MFLAGSADD   "missionflags.add"
#define ITEM_GPROP_MFLAGSREM   "missionflags.remove"
#define ITEM_GPROP_TITLETICS   "demo.titletics"
#define ITEM_GPROP_ADVISORTICS "demo.advisortics"
#define ITEM_GPROP_PAGETICS    "demo.pagetics"
#define ITEM_GPROP_MENUBKGND   "menu.background"
#define ITEM_GPROP_TRANSFRAME  "menu.transframe"
#define ITEM_GPROP_MENUSKVASND "menu.skvattacksound"
#define ITEM_GPROP_MENUOFFSET  "menu.offset"
#define ITEM_GPROP_MENUPTR1    "menu.pointer1"
#define ITEM_GPROP_MENUPTR2    "menu.pointer2"
#define ITEM_GPROP_BORDERFLAT  "border.flat"
#define ITEM_GPROP_BORDERTL    "border.topleft"
#define ITEM_GPROP_BORDERTOP   "border.top"
#define ITEM_GPROP_BORDERTR    "border.topright"
#define ITEM_GPROP_BORDERLEFT  "border.left"
#define ITEM_GPROP_BORDERRIGHT "border.right"
#define ITEM_GPROP_BORDERBL    "border.bottomleft"
#define ITEM_GPROP_BORDERBOTT  "border.bottom"
#define ITEM_GPROP_BORDERBR    "border.bottomright"
#define ITEM_GPROP_CCHARSPERLN "console.charsperline"
#define ITEM_GPROP_CBELLSOUND  "console.bellsound"
#define ITEM_GPROP_CCHATSOUND  "console.chatsound"
#define ITEM_GPROP_CBACKDROP   "console.backdrop"
#define ITEM_GPROP_PAUSEPATCH  "hud.pausepatch"
#define ITEM_GPROP_PUFFTYPE    "game.pufftype"
#define ITEM_GPROP_TELEFOGTYPE "game.telefogtype"
#define ITEM_GPROP_TELEFOGHT   "game.telefogheight"
#define ITEM_GPROP_TELESOUND   "game.telesound"
#define ITEM_GPROP_THRUSTFACTR "game.thrustfactor"
#define ITEM_GPROP_DEFPCLASS   "game.defpclass"
#define ITEM_GPROP_FINTYPE     "game.endgamefinaletype"
#define ITEM_GPROP_SKILLMUL    "game.skillammomultiplier"
#define ITEM_GPROP_MELEECALC   "game.monstermeleerange"
#define ITEM_GPROP_ITEMHEIGHT  "game.itemheight"
#define ITEM_GPROP_FINALEX     "finale.text.x"
#define ITEM_GPROP_FINALEY     "finale.text.y"
#define ITEM_GPROP_CASTTITLEY  "castcall.title.y"
#define ITEM_GPROP_CASTNAMEY   "castcall.name.y"
#define ITEM_GPROP_INTERPIC    "intermission.pic"
#define ITEM_GPROP_DEFMUSNAME  "sound.defaultmusname"
#define ITEM_GPROP_DEFSNDNAME  "sound.defaultsndname"
#define ITEM_GPROP_CREDITBKGND "credit.background"
#define ITEM_GPROP_CREDITY     "credit.y"
#define ITEM_GPROP_CREDITTSTEP "credit.titlestep"
#define ITEM_GPROP_ENDTEXTNAME "exit.endtextname"
#define ITEM_GPROP_BLOODNORM   "blood.defaultnormal"
#define ITEM_GPROP_BLOODRIP    "blood.defaultrip"
#define ITEM_GPROP_BLOODCRUSH  "blood.defaultcrush"

// Dynamic string numbers
enum
{
   GI_STR_MENUBKGND,
   GI_STR_MENUPTR1,
   GI_STR_MENUPTR2,
   GI_STR_BORDERFLAT,
   GI_STR_BORDERTL,
   GI_STR_BORDERTOP,
   GI_STR_BORDERTR,
   GI_STR_BORDERLEFT,
   GI_STR_BORDERRIGHT,
   GI_STR_BORDERBL,
   GI_STR_BORDERBOTT,
   GI_STR_BORDERBR,
   GI_STR_CBACKDROP,
   GI_STR_PAUSEPATCH,
   GI_STR_DEFPCLASS,
   GI_STR_PUFFTYPE,
   GI_STR_TELEFOGTYPE,
   GI_STR_INTERPIC,
   GI_STR_DEFMUSNAME,
   GI_STR_DEFSNDNAME,
   GI_STR_CREDITBKGND,
   GI_STR_ENDTEXTNAME,
   GI_STR_BLOODNORM,
   GI_STR_BLOODRIP,
   GI_STR_BLOODCRUSH,

   GI_STR_NUMSTRS
};

// Keep track of the dynamic string values assigned into GameModeInfo
static char *dynamicStrings[GI_STR_NUMSTRS];

// Finale type strings
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
   "psx_doom2"
};

// GameModeInfo flags
static dehflags_t gmi_flags[] =
{
   { "MNBIGFONT",      GIF_MNBIGFONT      },
   { "MAPXY",          GIF_MAPXY          },
   { "SAVESOUND",      GIF_SAVESOUND      },
   { "HASADVISORY",    GIF_HASADVISORY    },
   { "SHADOWTITLES",   GIF_SHADOWTITLES   },
   { "HASMADMELEE",    GIF_HASMADMELEE    },
   { "WOLFHACK",       GIF_WOLFHACK       },
   { "SETENDOFGAME",   GIF_SETENDOFGAME   },
   { "SKILL5RESPAWN",  GIF_SKILL5RESPAWN  },
   { "SKILL5WARNING",  GIF_SKILL5WARNING  },
   { "HUDSTATBARNAME", GIF_HUDSTATBARNAME },
   { "CENTERHUDMSG",   GIF_CENTERHUDMSG   },
   { "NODIEHI",        GIF_NODIEHI        },
   { "LOSTSOULBOUNCE", GIF_LOSTSOULBOUNCE },
   { "IMPACTBLOOD",    GIF_IMPACTBLOOD    },
   { "CHEATSOUND",     GIF_CHEATSOUND     },
   { "CHASEFAST",      GIF_CHASEFAST      },
   { NULL,             0                  }
};

static dehflagset_t gmi_flagset =
{
   gmi_flags, // flaglist
   0,         // mode
};

// missionInfo flags
static dehflags_t mission_flags[] =
{
   { "DEMOIFDEMO4",    MI_DEMOIFDEMO4    },
   { "CONBACKTITLE",   MI_CONBACKTITLE   },
   { "WOLFNAMEHACKS",  MI_WOLFNAMEHACKS  },
   { "HASBETRAY",      MI_HASBETRAY      },
   { "DOOM2MISSIONS",  MI_DOOM2MISSIONS  },
   { "NOTELEPORTZ",    MI_NOTELEPORTZ    },
   { "NOGDHIGH",       MI_NOGDHIGH       },
   { "ALLOWEXITTAG",   MI_ALLOWEXITTAG   },
   { "ALLOWSECRETTAG", MI_ALLOWSECRETTAG },
   { NULL,             0                 }
};

static dehflagset_t mission_flagset =
{
   mission_flags,
   0,
};

const char *meleeCalcStrings[meleecalc_NUM] =
{
   "doom",
   "raven"
};

// Main options for the gameproperties block
cfg_opt_t edf_game_opts[] =
{
   CFG_STR(ITEM_GPROP_FLAGSADD,    "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_FLAGSREM,    "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_MFLAGSADD,   "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_MFLAGSREM,   "",   CFGF_NONE),
   CFG_INT(ITEM_GPROP_TITLETICS,   0,    CFGF_NONE),
   CFG_INT(ITEM_GPROP_ADVISORTICS, 0,    CFGF_NONE),
   CFG_INT(ITEM_GPROP_PAGETICS,    0,    CFGF_NONE),
   CFG_STR(ITEM_GPROP_MENUBKGND,   "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_TRANSFRAME,  "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_MENUSKVASND, "",   CFGF_NONE),
   CFG_INT(ITEM_GPROP_MENUOFFSET,  0,    CFGF_NONE),
   CFG_STR(ITEM_GPROP_MENUPTR1,    "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_MENUPTR2,    "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_BORDERFLAT,  "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_BORDERTL,    "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_BORDERTOP,   "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_BORDERTR,    "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_BORDERLEFT,  "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_BORDERRIGHT, "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_BORDERBL,    "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_BORDERBOTT,  "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_BORDERBR,    "",   CFGF_NONE),
   CFG_INT(ITEM_GPROP_CCHARSPERLN, 0,    CFGF_NONE),
   CFG_STR(ITEM_GPROP_CBELLSOUND,  "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_CCHATSOUND,  "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_CBACKDROP,   "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_PAUSEPATCH,  "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_PUFFTYPE,    "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_TELEFOGTYPE, "",   CFGF_NONE),
   CFG_INT(ITEM_GPROP_TELEFOGHT,   0,    CFGF_NONE),
   CFG_STR(ITEM_GPROP_TELESOUND,   "",   CFGF_NONE),
   CFG_INT(ITEM_GPROP_THRUSTFACTR, 0,    CFGF_NONE),
   CFG_STR(ITEM_GPROP_DEFPCLASS,   "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_FINTYPE,     "",   CFGF_NONE),
   CFG_FLOAT(ITEM_GPROP_SKILLMUL,  0,    CFGF_NONE),
   CFG_STR(ITEM_GPROP_MELEECALC,   "",   CFGF_NONE),
   CFG_FLOAT(ITEM_GPROP_ITEMHEIGHT, 0,   CFGF_NONE),
   CFG_INT(ITEM_GPROP_FINALEX,     0,    CFGF_NONE),
   CFG_INT(ITEM_GPROP_FINALEY,     0,    CFGF_NONE),
   CFG_INT(ITEM_GPROP_CASTTITLEY,  0,    CFGF_NONE),
   CFG_INT(ITEM_GPROP_CASTNAMEY,   0,    CFGF_NONE),
   CFG_STR(ITEM_GPROP_INTERPIC,    "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_DEFMUSNAME,  "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_DEFSNDNAME,  "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_CREDITBKGND, "",   CFGF_NONE),
   CFG_INT(ITEM_GPROP_CREDITY,     0,    CFGF_NONE),
   CFG_INT(ITEM_GPROP_CREDITTSTEP, 0,    CFGF_NONE),
   CFG_STR(ITEM_GPROP_ENDTEXTNAME, "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_BLOODNORM,   "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_BLOODRIP,    "",   CFGF_NONE),
   CFG_STR(ITEM_GPROP_BLOODCRUSH,  "",   CFGF_NONE),
   CFG_END()
};

//=============================================================================
//
// Processing
//

//
// E_setDynamicString
//
// Set a dynamic string value inside of GameModeInfo.
//
static void E_setDynamicString(const char *&dest, int index, const char *value)
{
   if(dynamicStrings[index])
      efree(dynamicStrings[index]);

   dynamicStrings[index] = estrdup(value);

   dest = dynamicStrings[index];
}

#define IS_SET(name) (cfg_size(props, name) > 0)

//
// E_processGamePropsBlock
//
// Process a single gameproperties block.
//
static void E_processGamePropsBlock(cfg_t *props)
{
   // Flags

   if(IS_SET(ITEM_GPROP_FLAGSADD))
   {
      const char *flagstr = cfg_getstr(props, ITEM_GPROP_FLAGSADD);

      GameModeInfo->flags |= E_ParseFlags(flagstr, &gmi_flagset);
   }

   if(IS_SET(ITEM_GPROP_FLAGSREM))
   {
      const char *flagstr = cfg_getstr(props, ITEM_GPROP_FLAGSREM);
      unsigned int curFlags = GameModeInfo->flags;

      GameModeInfo->flags &= ~E_ParseFlags(flagstr, &gmi_flagset);

      GameModeInfo->flags |= (curFlags & GIF_SHAREWARE); // nice try, bitch.
   }

   if(IS_SET(ITEM_GPROP_MFLAGSADD))
   {
      const char *flagstr = cfg_getstr(props, ITEM_GPROP_MFLAGSADD);

      GameModeInfo->missionInfo->flags |= E_ParseFlags(flagstr, &mission_flagset);
   }

   if(IS_SET(ITEM_GPROP_MFLAGSREM))
   {
      const char *flagstr = cfg_getstr(props, ITEM_GPROP_MFLAGSREM);

      GameModeInfo->missionInfo->flags &= ~E_ParseFlags(flagstr, &mission_flagset);
   }

   // Demo Loop Properties

   if(IS_SET(ITEM_GPROP_TITLETICS))
      GameModeInfo->titleTics = cfg_getint(props, ITEM_GPROP_TITLETICS);
   
   if(IS_SET(ITEM_GPROP_ADVISORTICS))
      GameModeInfo->advisorTics = cfg_getint(props, ITEM_GPROP_ADVISORTICS);
   
   if(IS_SET(ITEM_GPROP_PAGETICS))
      GameModeInfo->pageTics = cfg_getint(props, ITEM_GPROP_PAGETICS);

   // Menu Properties
   if(IS_SET(ITEM_GPROP_MENUBKGND))
   {
      E_setDynamicString(GameModeInfo->menuBackground, GI_STR_MENUBKGND, 
                         cfg_getstr(props, ITEM_GPROP_MENUBKGND));
   }

   if(IS_SET(ITEM_GPROP_TRANSFRAME))
   {
      int stateNum = E_StateNumForName(cfg_getstr(props, ITEM_GPROP_TRANSFRAME));

      if(stateNum >= 0 &&
         (states[stateNum]->dehnum >= 0 || E_AutoAllocStateDEHNum(stateNum)))
      {
         GameModeInfo->transFrame = states[stateNum]->dehnum;
      }
   }

   if(IS_SET(ITEM_GPROP_MENUSKVASND))
   {
      sfxinfo_t *snd = E_SoundForName(cfg_getstr(props, ITEM_GPROP_MENUSKVASND));

      if(snd && (snd->dehackednum >= 0 || E_AutoAllocSoundDEHNum(snd)))
         GameModeInfo->skvAtkSound = snd->dehackednum;
   }

   if(IS_SET(ITEM_GPROP_MENUOFFSET))
      GameModeInfo->menuOffset = cfg_getint(props, ITEM_GPROP_MENUOFFSET);

   if(IS_SET(ITEM_GPROP_MENUPTR1) && GameModeInfo->menuCursor->numpatches >= 1)
   {
      E_setDynamicString(GameModeInfo->menuCursor->patches[0], GI_STR_MENUPTR1,
                         cfg_getstr(props, ITEM_GPROP_MENUPTR1));
   }

   if(IS_SET(ITEM_GPROP_MENUPTR2) && GameModeInfo->menuCursor->numpatches >= 2)
   {
      E_setDynamicString(GameModeInfo->menuCursor->patches[1], GI_STR_MENUPTR2,
                         cfg_getstr(props, ITEM_GPROP_MENUPTR2));
   }

   // Border Properties

   if(IS_SET(ITEM_GPROP_BORDERFLAT))
   {
      E_setDynamicString(GameModeInfo->borderFlat, GI_STR_BORDERFLAT,
                         cfg_getstr(props, ITEM_GPROP_BORDERFLAT));
   }

   if(IS_SET(ITEM_GPROP_BORDERTL))
   {
      E_setDynamicString(GameModeInfo->border->c_tl, GI_STR_BORDERTL, 
                         cfg_getstr(props, ITEM_GPROP_BORDERTL));
   }

   if(IS_SET(ITEM_GPROP_BORDERTOP))
   {
      E_setDynamicString(GameModeInfo->border->top, GI_STR_BORDERTOP, 
                         cfg_getstr(props, ITEM_GPROP_BORDERTOP));
   }

   if(IS_SET(ITEM_GPROP_BORDERTR))
   {
      E_setDynamicString(GameModeInfo->border->c_tr, GI_STR_BORDERTR, 
                         cfg_getstr(props, ITEM_GPROP_BORDERTR));
   }

   if(IS_SET(ITEM_GPROP_BORDERLEFT))
   {
      E_setDynamicString(GameModeInfo->border->left, GI_STR_BORDERLEFT, 
                         cfg_getstr(props, ITEM_GPROP_BORDERLEFT));
   }

   if(IS_SET(ITEM_GPROP_BORDERRIGHT))
   {
      E_setDynamicString(GameModeInfo->border->right, GI_STR_BORDERRIGHT, 
                         cfg_getstr(props, ITEM_GPROP_BORDERRIGHT));
   }

   if(IS_SET(ITEM_GPROP_BORDERBL))
   {
      E_setDynamicString(GameModeInfo->border->c_bl, GI_STR_BORDERBL, 
                         cfg_getstr(props, ITEM_GPROP_BORDERBL));
   }

   if(IS_SET(ITEM_GPROP_BORDERBOTT))
   {
      E_setDynamicString(GameModeInfo->border->bottom, GI_STR_BORDERBOTT, 
                         cfg_getstr(props, ITEM_GPROP_BORDERBOTT));
   }

   if(IS_SET(ITEM_GPROP_BORDERBR))
   {
      E_setDynamicString(GameModeInfo->border->c_br, GI_STR_BORDERBR, 
                         cfg_getstr(props, ITEM_GPROP_BORDERBR));
   }

   // Console Properties

   if(IS_SET(ITEM_GPROP_CCHARSPERLN))
      GameModeInfo->c_numCharsPerLine = cfg_getint(props, ITEM_GPROP_CCHARSPERLN);

   if(IS_SET(ITEM_GPROP_CBELLSOUND))
   {
      sfxinfo_t *snd = E_SoundForName(cfg_getstr(props, ITEM_GPROP_CBELLSOUND));

      if(snd && (snd->dehackednum >= 0 || E_AutoAllocSoundDEHNum(snd)))
         GameModeInfo->c_BellSound = snd->dehackednum;
   }

   if(IS_SET(ITEM_GPROP_CCHATSOUND))
   {
      sfxinfo_t *snd = E_SoundForName(cfg_getstr(props, ITEM_GPROP_CCHATSOUND));

      if(snd && (snd->dehackednum >= 0 || E_AutoAllocSoundDEHNum(snd)))
         GameModeInfo->c_ChatSound = snd->dehackednum;
   }

   if(IS_SET(ITEM_GPROP_CBACKDROP))
   {
      E_setDynamicString(GameModeInfo->consoleBack, GI_STR_CBACKDROP,
                         cfg_getstr(props, ITEM_GPROP_CBACKDROP));
   }

   // HUD Properties

   if(IS_SET(ITEM_GPROP_PAUSEPATCH))
   {
      E_setDynamicString(GameModeInfo->pausePatch, GI_STR_PAUSEPATCH,
                         cfg_getstr(props, ITEM_GPROP_PAUSEPATCH));
   }

   // Gamesim Properties

   if(IS_SET(ITEM_GPROP_PUFFTYPE))
   {
      const char *name = cfg_getstr(props, ITEM_GPROP_PUFFTYPE);

      if(E_ThingNumForName(name) >= 0)
         E_setDynamicString(GameModeInfo->puffType, GI_STR_PUFFTYPE, name);
   }

   if(IS_SET(ITEM_GPROP_TELEFOGTYPE))
   {
      const char *name = cfg_getstr(props, ITEM_GPROP_TELEFOGTYPE);

      if(E_ThingNumForName(name) >= 0)
         E_setDynamicString(GameModeInfo->teleFogType, GI_STR_TELEFOGTYPE, name);
   }

   if(IS_SET(ITEM_GPROP_TELEFOGHT))
   {
      int num = cfg_getint(props, ITEM_GPROP_TELEFOGHT);

      GameModeInfo->teleFogHeight = num * FRACUNIT;
   }

   if(IS_SET(ITEM_GPROP_TELESOUND))
   {
      sfxinfo_t *snd = E_SoundForName(cfg_getstr(props, ITEM_GPROP_TELESOUND));

      if(snd && (snd->dehackednum >= 0 || E_AutoAllocSoundDEHNum(snd)))
         GameModeInfo->teleSound = snd->dehackednum;
   }

   if(IS_SET(ITEM_GPROP_THRUSTFACTR))
      GameModeInfo->thrustFactor = (int16_t)cfg_getint(props, ITEM_GPROP_THRUSTFACTR);

   if(IS_SET(ITEM_GPROP_DEFPCLASS))
   {
      E_setDynamicString(GameModeInfo->defPClassName, GI_STR_DEFPCLASS,
                         cfg_getstr(props, ITEM_GPROP_DEFPCLASS));
   }

   // Determines behavior of the Teleport_EndGame line special when LevelInfo
   // has not provided a specific finale type for the level.
   if(IS_SET(ITEM_GPROP_FINTYPE))
   {
      int finaleType = E_StrToNumLinear(finaleTypeStrs, FINALE_NUMFINALES, 
                                        cfg_getstr(props, ITEM_GPROP_FINTYPE));

      if(finaleType >= 0 && finaleType < FINALE_NUMFINALES)
         GameModeInfo->teleEndGameFinaleType = finaleType;
   }

   if(IS_SET(ITEM_GPROP_SKILLMUL))
      GameModeInfo->skillAmmoMultiplier = cfg_getfloat(props, ITEM_GPROP_SKILLMUL);
   if(IS_SET(ITEM_GPROP_MELEECALC))
   {
      int meleetype = E_StrToNumLinear(meleeCalcStrings, meleecalc_NUM,
                                       cfg_getstr(props, ITEM_GPROP_MELEECALC));
      if(meleetype >= 0 && meleetype < meleecalc_NUM)
         GameModeInfo->monsterMeleeRange = static_cast<meleecalc_e>(meleetype);
   }
   if(IS_SET(ITEM_GPROP_ITEMHEIGHT))
      GameModeInfo->itemHeight = M_DoubleToFixed(cfg_getfloat(props, ITEM_GPROP_ITEMHEIGHT));

   // Finale Properties

   if(IS_SET(ITEM_GPROP_FINALEX))
      GameModeInfo->fTextPos->x = cfg_getint(props, ITEM_GPROP_FINALEX);

   if(IS_SET(ITEM_GPROP_FINALEY))
      GameModeInfo->fTextPos->y = cfg_getint(props, ITEM_GPROP_FINALEY);

   if(IS_SET(ITEM_GPROP_CASTTITLEY))
      GameModeInfo->castTitleY = cfg_getint(props, ITEM_GPROP_CASTTITLEY);

   if(IS_SET(ITEM_GPROP_CASTNAMEY))
      GameModeInfo->castNameY = cfg_getint(props, ITEM_GPROP_CASTNAMEY);

   // Intermission Properties

   if(IS_SET(ITEM_GPROP_INTERPIC))
   {
      E_setDynamicString(GameModeInfo->interPic, GI_STR_INTERPIC,
                         cfg_getstr(props, ITEM_GPROP_INTERPIC));
   }

   // Sound Properties

   if(IS_SET(ITEM_GPROP_DEFMUSNAME))
   {
      E_setDynamicString(GameModeInfo->defMusName, GI_STR_DEFMUSNAME,
                         cfg_getstr(props, ITEM_GPROP_DEFMUSNAME));
   }

   if(IS_SET(ITEM_GPROP_DEFSNDNAME))
   {
      E_setDynamicString(GameModeInfo->defSoundName, GI_STR_DEFSNDNAME,
                         cfg_getstr(props, ITEM_GPROP_DEFSNDNAME));
   }
   
   // Credit Screen Properties

   if(IS_SET(ITEM_GPROP_CREDITBKGND))
   {
      E_setDynamicString(GameModeInfo->creditBackground, GI_STR_CREDITBKGND,
                         cfg_getstr(props, ITEM_GPROP_CREDITBKGND));
   }

   if(IS_SET(ITEM_GPROP_CREDITY))
      GameModeInfo->creditY = cfg_getint(props, ITEM_GPROP_CREDITY);

   if(IS_SET(ITEM_GPROP_CREDITTSTEP))
      GameModeInfo->creditTitleStep = cfg_getint(props, ITEM_GPROP_CREDITTSTEP);
   
   // Exit Properties
   
   if(IS_SET(ITEM_GPROP_ENDTEXTNAME))
   {
      E_setDynamicString(GameModeInfo->endTextName, GI_STR_ENDTEXTNAME,
                         cfg_getstr(props, ITEM_GPROP_ENDTEXTNAME));
   }

   // Blood Properties
   if(IS_SET(ITEM_GPROP_BLOODNORM))
   {
      E_setDynamicString(GameModeInfo->bloodDefaultNormal, GI_STR_BLOODNORM,
                         cfg_getstr(props, ITEM_GPROP_BLOODNORM));
   }

   if(IS_SET(ITEM_GPROP_BLOODRIP))
   {
      E_setDynamicString(GameModeInfo->bloodDefaultRIP, GI_STR_BLOODRIP,
                         cfg_getstr(props, ITEM_GPROP_BLOODRIP));
}

   if(IS_SET(ITEM_GPROP_BLOODCRUSH))
   {
      E_setDynamicString(GameModeInfo->bloodDefaultCrush, GI_STR_BLOODCRUSH,
                         cfg_getstr(props, ITEM_GPROP_BLOODCRUSH));
   }
}

//
// E_ProcessGameProperties
//
// Main routine for this module. Process all gameproperties blocks.
//
void E_ProcessGameProperties(cfg_t *cfg)
{
   unsigned int numProps = cfg_size(cfg, EDF_SEC_GAMEPROPS);

   E_EDFLogPuts("\t* Processing gameproperties\n");

   for(unsigned int i = 0; i < numProps; i++)
      E_processGamePropsBlock(cfg_getnsec(cfg, EDF_SEC_GAMEPROPS, i));
}

// EOF

