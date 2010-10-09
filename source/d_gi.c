// Emacs style mode select -*- C -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2002 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//----------------------------------------------------------------------------
//
// Gamemode information
//
// Stores all gamemode-dependent information in one location and 
// lends more structure to other modules.
//
// James Haley
//
//----------------------------------------------------------------------------

#include "z_zone.h"
#include "w_wad.h"
#include "doomstat.h"
#include "doomdef.h"
#include "sounds.h"
#include "hi_stuff.h"
#include "wi_stuff.h"
#include "d_gi.h"
#include "p_info.h"
#include "info.h"
#include "e_things.h"
#include "f_finale.h"

// definitions

// automap number mark format strings
#define DOOMMARKS    "AMMNUM%d"
#define HTICMARKS    "SMALLIN%d"

// menu background flats
#define DOOMMENUBACK "FLOOR4_6"
#define HACXMENUBACK "CONS1_5"
#define HTICMENUBACK "FLOOR16"

// credit background flats
#define DOOMCREDITBK "NUKAGE1"
#define DM2CREDITBK  "SLIME05"
#define HACXCREDITBK "SLIME01"
#define HTICCREDITBK "FLTWAWA1"

// border flats
#define DOOMBRDRFLAT "FLOOR7_2"
#define DM2BRDRFLAT  "GRNROCK"
#define HREGBRDRFLAT "FLAT513"
#define HSWBRDRFLAT  "FLOOR04"

// Default sound names
// Note: these are lump names, not sound mnemonics
#define DOOMDEFSOUND "DSPISTOL"
#define HTICDEFSOUND "GLDHIT"

// Default console backdrops
// Used when no "CONSOLE" lump has been provided.
// Most gamemodes use the titlepic, which is darkened by code in c_io.c.
// Ultimate DOOM and DOOM II have a suitable graphic in the INTERPIC.
#define CONBACK_DEFAULT "TITLEPIC"
#define CONBACK_COMRET  "INTERPIC"
#define CONBACK_DISK    "DMENUPIC"
#define CONBACK_HERETIC "TITLE" 

// Version names
#define VNAME_DOOM_SW   "DOOM Shareware version"
#define VNAME_DOOM_REG  "DOOM Registered version"
#define VNAME_DOOM_RET  "Ultimate DOOM version"
#define VNAME_DOOM2     "DOOM II version"
#define VNAME_TNT       "Final DOOM: TNT - Evilution version"
#define VNAME_PLUT      "Final DOOM: The Plutonia Experiment version"
#define VNAME_HACX      "HACX - Twitch 'n Kill version"
#define VNAME_DISK      "DOOM II disk version"
#define VNAME_HTIC_SW   "Heretic Shareware version"
#define VNAME_HTIC_REG  "Heretic Registered version"
#define VNAME_HTIC_SOSR "Heretic: Shadow of the Serpent Riders version"
#define VNAME_UNKNOWN   "Unknown Game version. May not work."

// FreeDoom/Blasphemer/Etc override names
#define FNAME_DOOM_SW   "FreeDoom demo version"
#define FNAME_DOOM_R    "FreeDoom version"
#define FNAME_DOOM2     "FreeDoom II version"

// Startup banners
#define BANNER_DOOM_SW   "DOOM Shareware Startup"
#define BANNER_DOOM_REG  "DOOM Registered Startup"
#define BANNER_DOOM_RET  "The Ultimate DOOM Startup"
#define BANNER_DOOM2     "DOOM 2: Hell on Earth"
#define BANNER_TNT       "DOOM 2: TNT - Evilution"
#define BANNER_PLUT      "DOOM 2: Plutonia Experiment"
#define BANNER_HACX      "HACX - Twitch 'n Kill"
#define BANNER_HTIC_SW   "Heretic Shareware Startup"
#define BANNER_HTIC_REG  "Heretic Registered Startup"
#define BANNER_HTIC_SOSR "Heretic: Shadow of the Serpent Riders"
#define BANNER_UNKNOWN   "Public DOOM"

// Default intermission pics
#define INTERPIC_DOOM    "INTERPIC"
#define INTERPIC_DISK    "DMENUPIC"

// Default finales caused by Teleport_EndGame special
#define DEF_DOOM_FINALE  FINALE_DOOM_CREDITS
#define DEF_HTIC_FINALE  FINALE_HTIC_CREDITS

// Default translations for MF4_AUTOTRANSLATE flag.
// Graphic is assumed to be in the DOOM palette.
#define DEFTL_HERETIC  "TR_DINH"

//
// Common Flags
//
// The following sets of GameModeInfo flags define the basic characteristics
// of the intrinsic gametypes.
//

#define DOOM_GIFLAGS \
   (GIF_HASDISK | GIF_HASEXITSOUNDS | GIF_CLASSICMENUS | GIF_SKILL5RESPAWN | \
    GIF_SKILL5WARNING | GIF_HUDSTATBARNAME)

#define HERETIC_GIFLAGS \
   (GIF_MNBIGFONT | GIF_SAVESOUND | GIF_HASADVISORY | GIF_SHADOWTITLES | \
    GIF_HASMADMELEE)

// globals

// holds the address of the gamemodeinfo_t for the current gamemode,
// determined at startup
gamemodeinfo_t *GameModeInfo;

// data

//
// Menu Cursors
//

// doom menu skull cursor
static gimenucursor_t giSkullCursor =
{
   "M_SKULL1",
   "M_SKULL2",
};

// heretic menu arrow cursor
static gimenucursor_t giArrowCursor =
{
   "M_SLCTR1",
   "M_SLCTR2",
};

//
// Display Borders
//

static giborder_t giDoomBorder =
{
   8, // offset
   8, // size

   "BRDR_TL", "BRDR_T", "BRDR_TR",
   "BRDR_L",            "BRDR_R",
   "BRDR_BL", "BRDR_B", "BRDR_BR",
};

static giborder_t giHticBorder =
{
   4,  // offset
   16, // size

   "BORDTL", "BORDT", "BORDTR",
   "BORDL",           "BORDR",
   "BORDBL", "BORDB", "BORDBR",
};

//
// Finale text locations
//

static giftextpos_t giDoomFText = { 10, 10 };
static giftextpos_t giHticFText = { 20, 5  };

//
// Menu Sounds
//

static int doomMenuSounds[MN_SND_NUMSOUNDS] =
{
   sfx_swtchn, // activate
   sfx_swtchx, // deactivate
   sfx_pstop,  // key up/down
   sfx_pistol, // command
   sfx_swtchx, // previous menu
   sfx_stnmov, // key left/right
};

static int hticMenuSounds[MN_SND_NUMSOUNDS] =
{
   sfx_hdorcls, // activate
   sfx_hdorcls, // deactivate
   sfx_hswitch, // key up/down
   sfx_hdorcls, // command
   sfx_hswitch, // previous menu
   sfx_keyup,   // key left/right
};

// gamemode-dependent menus (defined in mn_menus.c et al.)
extern menu_t menu_savegame;
extern menu_t menu_hsavegame;
extern menu_t menu_loadgame;
extern menu_t menu_hloadgame;
extern menu_t menu_newgame;
extern menu_t menu_hnewgame;

//
// Skin Sound Defaults
//

static const char *doom_skindefs[NUMSKINSOUNDS] =
{
   "plpain",
   "pdiehi",
   "oof",
   "slop",
   "punch",
   "radio",
   "pldeth",
   "plfall",
   "plfeet",
   "fallht",
   "none",
   "noway",
};

static int doom_soundnums[NUMSKINSOUNDS] =
{
   sfx_plpain,
   sfx_pdiehi,
   sfx_oof,
   sfx_slop,
   sfx_punch,
   sfx_radio,
   sfx_pldeth,
   sfx_plfall,
   sfx_plfeet,
   sfx_fallht,
   sfx_plwdth,
   sfx_noway,
};

static const char *htic_skindefs[NUMSKINSOUNDS] =
{
   "ht_plrpai",
   "ht_plrcdth",
   "ht_plroof",
   "ht_gibdth",
   "none",      // HTIC_FIXME
   "ht_chat",
   "ht_plrdth",
   "plfall",
   "plfeet",
   "fallht",
   "ht_plrwdth",
   "ht_plroof",
};

static int htic_soundnums[NUMSKINSOUNDS] =
{
   sfx_hplrpai,
   sfx_hplrcdth,
   sfx_hplroof,
   sfx_hgibdth,
   sfx_None,    // HTIC_FIXME
   sfx_chat,
   sfx_hplrdth,
   sfx_plfall,
   sfx_plfeet,
   sfx_fallht,
   sfx_hplrwdth,
   sfx_hplroof,
};

//
// Exit sounds
//

static int quitsounds[8] =
{
   sfx_pldeth,
   sfx_dmpain,
   sfx_popain,
   sfx_slop,
   sfx_telept,
   sfx_posit1,
   sfx_posit3,
   sfx_sgtatk
};

static int quitsounds2[8] =
{
   sfx_vilact,
   sfx_getpow,
   sfx_boscub,
   sfx_slop,
   sfx_skeswg,
   sfx_kntdth,
   sfx_bspact,
   sfx_sgtatk
};

//
// Exit Rule Sets
//
// haleyjd 07/03/09: replaces hard-coded functions in g_game.c
//

static exitrule_t DoomExitRules[] =
{
   {  1,  9, 3, false }, // normal exit: E1M9 -> E1M4
   {  2,  9, 5, false }, // normal exit: E2M9 -> E2M6
   {  3,  9, 6, false }, // normal exit: E3M9 -> E3M7
   {  4,  9, 2, false }, // normal exit: E4M9 -> E4M3
   { -1, -1, 8, true  }, // secret exits: any map goes to ExM9
   { -2 }
};

static exitrule_t Doom2ExitRules[] =
{
   {  1, 31, 15, false }, // normal exit: MAP31 -> MAP16
   {  1, 32, 15, false }, // normal exit: MAP32 -> MAP16
   {  1, 15, 30, true  }, // secret exit: MAP15 -> MAP31
   {  1, 31, 31, true  }, // secret exit: MAP31 -> MAP32
   { -2 }
};

static exitrule_t DiskExitRules[] =
{
   {  1, 31, 15, false }, // normal exit: MAP31 -> MAP16
   {  1, 32, 15, false }, // normal exit: MAP32 -> MAP16
   {  1, 33, 02, false }, // normal exit: MAP33 -> MAP03
   {  1, 02, 32, true  }, // secret exit: MAP02 -> MAP33
   {  1, 15, 30, true  }, // secret exit: MAP15 -> MAP31
   {  1, 31, 31, true  }, // secret exit: MAP31 -> MAP32
   { -2 }
};

static exitrule_t HereticExitRules[] =
{
   {  1,  9, 6, false }, // normal exit: E1M9 -> E1M7
   {  2,  9, 4, false }, // normal exit: E2M9 -> E2M5
   {  3,  9, 4, false }, // normal exit: E3M9 -> E3M5
   {  4,  9, 4, false }, // normal exit: E4M9 -> E4M5
   {  5,  9, 3, false }, // normal exit: E5M9 -> E5M4
   { -1, -1, 8, true  }, // secret exits: any map goes to ExM9
   { -2 }
};

//
// Default Finale Data
//

// Doom: Shareware, Registered, Retail

static finalerule_t DoomFinaleRules[] =
{
   {  1,  8, "BGFLATE1", "E1TEXT", FINALE_DOOM_CREDITS },
   {  2,  8, "BGFLATE2", "E2TEXT", FINALE_DOOM_DEIMOS  },
   {  3,  8, "BGFLATE3", "E3TEXT", FINALE_DOOM_BUNNY   },
   {  4,  8, "BGFLATE4", "E4TEXT", FINALE_DOOM_MARINE  },
   { -1,  8, "BGFLATE1", "E1TEXT", FINALE_DOOM_CREDITS }, // ExM8 default
   { -1, -1, "BGFLATE1", NULL,     FINALE_TEXT         }, // other default
   { -2 }
};

static finaledata_t DoomFinale =
{
   mus_victor,
   false,
   DoomFinaleRules
};

// DOOM 2

static finalerule_t Doom2FinaleRules[] =
{
   {  1,  6, "BGFLAT06", "C1TEXT", FINALE_TEXT },
   {  1, 11, "BGFLAT11", "C2TEXT", FINALE_TEXT },
   {  1, 20, "BGFLAT20", "C3TEXT", FINALE_TEXT },
   {  1, 30, "BGFLAT30", "C4TEXT", FINALE_TEXT, true        }, // end of game
   {  1, 15, "BGFLAT15", "C5TEXT", FINALE_TEXT, false, true }, // only after secret
   {  1, 31, "BGFLAT31", "C6TEXT", FINALE_TEXT, false, true }, // only after secret
   { -1, -1, "BGFLAT06", NULL,     FINALE_TEXT },
   { -2 }
};

static finaledata_t Doom2Finale =
{
   mus_read_m,
   false,
   Doom2FinaleRules
};

// Final Doom: TNT

static finalerule_t TNTFinaleRules[] =
{
   {  1,  6, "BGFLAT06", "T1TEXT", FINALE_TEXT },
   {  1, 11, "BGFLAT11", "T2TEXT", FINALE_TEXT },
   {  1, 20, "BGFLAT20", "T3TEXT", FINALE_TEXT },
   {  1, 30, "BGFLAT30", "T4TEXT", FINALE_TEXT, true        }, // end of game
   {  1, 15, "BGFLAT15", "T5TEXT", FINALE_TEXT, false, true }, // only after secret
   {  1, 31, "BGFLAT31", "T6TEXT", FINALE_TEXT, false, true }, // only after secret
   { -1, -1, "BGFLAT06", NULL,     FINALE_TEXT },
   { -2 }
};

static finaledata_t TNTFinale =
{
   mus_read_m,
   false,
   TNTFinaleRules
};

// Final Doom: Plutonia

static finalerule_t PlutFinaleRules[] =
{
   {  1,  6, "BGFLAT06", "P1TEXT", FINALE_TEXT },
   {  1, 11, "BGFLAT11", "P2TEXT", FINALE_TEXT },
   {  1, 20, "BGFLAT20", "P3TEXT", FINALE_TEXT },
   {  1, 30, "BGFLAT30", "P4TEXT", FINALE_TEXT, true        }, // end of game
   {  1, 15, "BGFLAT15", "P5TEXT", FINALE_TEXT, false, true }, // only after secret
   {  1, 31, "BGFLAT31", "P6TEXT", FINALE_TEXT, false, true }, // only after secret
   { -1, -1, "BGFLAT06", NULL,     FINALE_TEXT },
   { -2 }
};

static finaledata_t PlutFinale =
{
   mus_read_m,
   false,
   PlutFinaleRules
};

// Heretic (Shareware / Registered / SoSR)

static finalerule_t HereticFinaleRules[] =
{
   {  1,  8, "BGFLATHE1", "H1TEXT", FINALE_HTIC_CREDITS },
   {  2,  8, "BGFLATHE2", "H2TEXT", FINALE_HTIC_WATER   },
   {  3,  8, "BGFLATHE3", "H3TEXT", FINALE_HTIC_DEMON   },
   {  4,  8, "BGFLATHE4", "H4TEXT", FINALE_HTIC_CREDITS },
   {  5,  8, "BGFLATHE5", "H5TEXT", FINALE_HTIC_CREDITS },
   { -1,  8, "BGFLATHE1", "H1TEXT", FINALE_HTIC_CREDITS }, // ExM8 default
   { -1, -1, "BGFLATHE1", NULL,     FINALE_TEXT         }, // other default
   { -2 }
};

static finaledata_t HereticFinale =
{
   hmus_cptd,
   true,
   HereticFinaleRules
};

// Unknown gamemode

static finalerule_t UnknownFinaleRules[] =
{
   { -1, -1, "F_SKY2", NULL, FINALE_TEXT },
   { -2 }
};

static finaledata_t UnknownFinale =
{
   mus_None, // special case
   false,
   UnknownFinaleRules
};

//
// Default Sky Data
//

// Doom Gamemodes

static skyrule_t DoomSkyRules[] =
{
   {  1, "SKY1" },
   {  2, "SKY2" },
   {  3, "SKY3" },
   {  4, "SKY4" },
   { -1, "SKY4" },
   { -2 }
};

static skydata_t DoomSkyData =
{
   GI_SKY_IFEPISODE,
   DoomSkyRules
};

// Doom II Gamemodes

static skyrule_t Doom2SkyRules[] =
{
   { 12, "SKY1" },
   { 21, "SKY2" },
   { -1, "SKY3" },
   { -2 }
};

static skydata_t Doom2SkyData =
{
   GI_SKY_IFMAPLESSTHAN,
   Doom2SkyRules
};

// Heretic Gamemodes

static skyrule_t HereticSkyRules[] =
{
   {  1, "SKY1" },
   {  2, "SKY2" },
   {  3, "SKY3" },
   {  4, "SKY1" },
   {  5, "SKY3" },
   { -1, "SKY1" },
   { -2 }
};

static skydata_t HereticSkyData =
{
   GI_SKY_IFEPISODE,
   HereticSkyRules
};

//
// Default boss spec rules
//

// Doom gamemodes

static bspecrule_t DoomBossSpecs[] =
{
   {  1,  8, BSPEC_E1M8 },
   {  2,  8, BSPEC_E2M8 },
   {  3,  8, BSPEC_E3M8 },
   {  4,  6, BSPEC_E4M6 },
   {  4,  8, BSPEC_E4M8 },
   {  5,  8, BSPEC_E5M8 }, // for compatibility with earlier EE's
   { -1 }
};

// Doom II gamemodes

static bspecrule_t Doom2BossSpecs[] =
{
   {  1,  7, BSPEC_MAP07_1 | BSPEC_MAP07_2 }, // DOOM II MAP07 has two specials
   { -1 }
};

// Heretic gamemodes

static bspecrule_t HereticBossSpecs[] =
{
   {  1,  8, BSPEC_E1M8 },
   {  2,  8, BSPEC_E2M8 },
   {  3,  8, BSPEC_E3M8 },
   {  4,  6, BSPEC_E4M6 }, // for compatibility with earlier EE's
   {  4,  8, BSPEC_E4M8 },
   {  5,  8, BSPEC_E5M8 }, 
   { -1 }
};

//
// Music Selection Routines - defined in s_sound.c
//

extern int S_MusicForMapDoom(void);
extern int S_MusicForMapDoom2(void);
extern int S_MusicForMapHtic(void);

//
// IWAD paths
//

// set by system config:
char *gi_path_doomsw;
char *gi_path_doomreg;
char *gi_path_doomu;
char *gi_path_doom2;
char *gi_path_tnt;
char *gi_path_plut;
char *gi_path_hacx;
char *gi_path_hticsw;
char *gi_path_hticreg;
char *gi_path_sosr;

//
// Default Override Objects
//
extern default_or_t HereticDefaultORs;

//=============================================================================
//
// Mission Information Structures
//

//
// Doom
//
static missioninfo_t gmDoom =
{
   doom,   // id
   0,      // flags
   "doom", // gamePathName

   NULL,   // versionNameOR
   NULL,   // startupBannerOR
   0,      // numEpisodesOR
   NULL,   // iwadPathOR
   NULL,   // finaleDataOR
};

//
// Doom 2
//
static missioninfo_t gmDoom2 =
{
   doom2,   // id
   0,       // flags
   "doom2", // gamePathName

   NULL,    // versionNameOR
   NULL,    // startupBannerOR
   0,       // numEpisodesOR
   NULL,    // iwadPathOR
   NULL,    // finaleDataOR
};

//
// Final Doom - TNT
//
static missioninfo_t gmFinalTNT =
{
   pack_tnt,          // id
   MI_DEMOIFDEMO4,    // flags
  "tnt",              // gamePathName

   VNAME_TNT,         // versionNameOR
   BANNER_TNT,        // startupBannerOR
   0,                 // numEpisodesOR
   &gi_path_tnt,      // iwadPathOR
   &TNTFinale,        // finaleDataOR
   NULL,              // mainMenuOR
   NULL,              // menuBackgroundOR
   NULL,              // creditBackgroundOR
   NULL,              // consoleBackOR
   demostates_udoom,  // demoStatesOR -- haleyjd 11/12/09: to play DEMO4
};

//
// Final Doom - Plutonia
//
static missioninfo_t gmFinalPlutonia =
{
   pack_plut,         // id
   MI_DEMOIFDEMO4,    // flags
   "plutonia",        // gamePathName

   VNAME_PLUT,        // versionNameOR
   BANNER_PLUT,       // startupBannerOR
   0,                 // numEpisodesOR
   &gi_path_plut,     // iwadPathOR
   &PlutFinale,       // finaleDataOR
   NULL,              // mainMenuOR
   NULL,              // menuBackgroundOR
   NULL,              // creditBackgroundOR
   NULL,              // consoleBackOR
   demostates_udoom,  // demoStatesOR -- haleyjd 11/12/09: to play DEMO4
};

//
// Disk version
//
static missioninfo_t gmDisk =
{
   pack_disk,       // id
   MI_CONBACKTITLE, // flags
   "doom2",         // gamePathName

   VNAME_DISK,      // versionNameOR
   NULL,            // startupBannerOR
   0,               // numEpisodesOR
   NULL,            // iwadPathOR
   NULL,            // finaleDataOR
   NULL,            // mainMenuOR
   NULL,            // menuBackgroundOR
   NULL,            // creditBackgroundOR
   CONBACK_DISK,    // consoleBackOR
   NULL,            // demoStatesOR
   INTERPIC_DISK,   // interPicOR
   DiskExitRules,   // exitRulesOR
};

//
// HacX Stand-alone version
//
static missioninfo_t gmHacx =
{
   pack_hacx,       // id
   0,               // flags
   "hacx",          // gamePathName

   VNAME_HACX,      // versionNameOR
   BANNER_HACX,     // startupBannerOR
   0,               // numEpisodesOR
   &gi_path_hacx,   // iwadPathOR
   NULL,            // finaleDataOR
   &menu_old_main,  // mainMenuOR
   HACXMENUBACK,    // menuBackgroundOR
   HACXCREDITBK,    // creditBackgroundOR
   CONBACK_DEFAULT, // consoleBackOR
};

//
// Heretic
//
static missioninfo_t gmHeretic =
{
   heretic,   // id
   0,         // flags
   "heretic", // gamePathName

   NULL,      // versionNameOR
   NULL,      // startupBannerOR
   0,         // numEpisodesOR
   NULL,      // iwadPathOR
   NULL,      // finaleDataOR
};

//
// Heretic: Shadow of the Serpent Riders
//
static missioninfo_t gmHereticSoSR =
{
   hticsosr,         // id
   0,                // flags
   "heretic",        // gamePathName

   VNAME_HTIC_SOSR,  // versionNameOR
   BANNER_HTIC_SOSR, // startupBannerOR
   6,                // numEpisodesOR
   &gi_path_sosr,    // iwadPathOR
   NULL,             // finaleDataOR
};

//
// ???
//
static missioninfo_t gmUnknown =
{
   none,           // id
   0,              // flags
   "doom",         // gamePathName

   VNAME_UNKNOWN,  // versionNameOR
   BANNER_UNKNOWN, // startupBannerOR
   0,              // numEpisodesOR
   NULL,           // iwadPathOR
   &UnknownFinale, // finaleDataOR
};

// Mission info object array

missioninfo_t *MissionInfoObjects[NumGameMissions] =
{
   &gmDoom,          // doom
   &gmDoom2,         // doom2
   &gmFinalTNT,      // pack_tnt
   &gmFinalPlutonia, // pack_plut
   &gmDisk,          // pack_disk
   &gmHacx,          // pack_hacx
   &gmHeretic,       // heretic
   &gmHereticSoSR,   // hticsosr
   &gmUnknown,       // none - ???
};

//=============================================================================
//
// Game Mode Information Structures
//

//
// DOOM Shareware
//
static gamemodeinfo_t giDoomSW =
{
   shareware,                    // id
   Game_DOOM,                    // type
   GIF_SHAREWARE | DOOM_GIFLAGS, // flags
   
   VNAME_DOOM_SW,    // versionName
   FNAME_DOOM_SW,    // freeVerName
   BANNER_DOOM_SW,   // startupBanner
   &gi_path_doomsw,  // iwadPath
   
   demostates_doom,  // demoStates
   170,              // titleTics
   0,                // advisorTics
   11*TICRATE,       // pageTics
   mus_intro,        // titleMusNum

   DOOMMENUBACK,     // menuBackground
   DOOMCREDITBK,     // creditBackground
   20,               // creditY
   12,               // creditTitleStep
   &giSkullCursor,   // menuCursor
   &menu_main,       // mainMenu
   &menu_savegame,   // saveMenu
   &menu_loadgame,   // loadMenu
   &menu_newgame,    // newGameMenu
   doomMenuSounds,   // menuSounds
   S_TBALL1,         // transFrame
   sfx_shotgn,       // skvAtkSound
   CR_RED,           // unselectColor
   CR_GRAY,          // selectColor
   CR_GREEN,         // variableColor
   0,                // menuOffset

   DOOMBRDRFLAT,     // borderFlat
   &giDoomBorder,    // border

   &cr_red,          // defTextTrans
   CR_RED,           // colorNormal
   CR_GRAY,          // colorHigh
   CR_GOLD,          // colorError
   40,               // c_numCharsPerLine
   sfx_tink,         // c_BellSound
   sfx_tink,         // c_ChatSound
   CONBACK_DEFAULT,  // consoleBack
   0,                // blackIndex
   4,                // whiteIndex
   NUMCARDS,         // numHUDKeys

   &DoomStatusBar,   // StatusBar

   DOOMMARKS,        // markNumFmt

   "M_PAUSE",        // pausePatch

   1,                // numEpisodes
   DoomExitRules,    // exitRules
   MT_TFOG,          // teleFogType
   0,                // teleFogHeight
   sfx_telept,       // teleSound
   100,              // thrustFactor
   "DoomMarine",     // defPClassName
   NULL,             // defTranslate
   DoomBossSpecs,    // bossRules

   INTERPIC_DOOM,     // interPic
   mus_inter,         // interMusNum
   &giDoomFText,      // fTextPos
   &DoomIntermission, // interfuncs
   DEF_DOOM_FINALE,   // teleEndGameFinaleType
   &DoomFinale,       // finaleData

   S_music,           // s_music
   S_MusicForMapDoom, // MusicForMap
   mus_None,          // musMin
   NUMMUSIC,          // numMusic
   "d_",              // musPrefix
   "e1m1",            // defMusName
   DOOMDEFSOUND,      // defSoundName
   doom_skindefs,     // skinSounds
   doom_soundnums,    // playerSounds

   1,                // switchEpisode
   &DoomSkyData,     // skyData

   NULL,             // defaultORs

   "ENDOOM",         // endTextName
   quitsounds,       // exitSounds
};

//
// DOOM Registered
//
static gamemodeinfo_t giDoomReg =
{
   registered,       // id
   Game_DOOM,        // type
   DOOM_GIFLAGS,     // flags
   
   VNAME_DOOM_REG,   // versionName
   FNAME_DOOM_R,     // freeVerName
   BANNER_DOOM_REG,  // startupBanner
   &gi_path_doomreg, // iwadPath
   
   demostates_doom,  // demoStates
   170,              // titleTics
   0,                // advisorTics
   11*TICRATE,       // pageTics
   mus_intro,        // titleMusNum

   DOOMMENUBACK,     // menuBackground
   DOOMCREDITBK,     // creditBackground
   20,               // creditY
   12,               // creditTitleStep
   &giSkullCursor,   // menuCursor
   &menu_main,       // mainMenu
   &menu_savegame,   // saveMenu
   &menu_loadgame,   // loadMenu
   &menu_newgame,    // newGameMenu
   doomMenuSounds,   // menuSounds
   S_TBALL1,         // transFrame
   sfx_shotgn,       // skvAtkSound
   CR_RED,           // unselectColor
   CR_GRAY,          // selectColor
   CR_GREEN,         // variableColor
   0,                // menuOffset

   DOOMBRDRFLAT,     // borderFlat
   &giDoomBorder,    // border

   &cr_red,          // defTextTrans
   CR_RED,           // colorNormal
   CR_GRAY,          // colorHigh
   CR_GOLD,          // colorError
   40,               // c_numCharsPerLine
   sfx_tink,         // c_BellSound
   sfx_tink,         // c_ChatSound
   CONBACK_DEFAULT,  // consoleBack
   0,                // blackIndex
   4,                // whiteIndex
   NUMCARDS,         // numHUDKeys

   &DoomStatusBar,   // StatusBar

   DOOMMARKS,        // markNumFmt

   "M_PAUSE",        // pausePatch

   3,                // numEpisodes
   DoomExitRules,    // exitRules
   MT_TFOG,          // teleFogType
   0,                // teleFogHeight
   sfx_telept,       // teleSound
   100,              // thrustFactor
   "DoomMarine",     // defPClassName
   NULL,             // defTranslate
   DoomBossSpecs,    // bossRules

   INTERPIC_DOOM,     // interPic
   mus_inter,         // interMusNum
   &giDoomFText,      // fTextPos
   &DoomIntermission, // interfuncs
   DEF_DOOM_FINALE,   // teleEndGameFinaleType
   &DoomFinale,       // finaleData

   S_music,           // s_music
   S_MusicForMapDoom, // MusicForMap
   mus_None,          // musMin
   NUMMUSIC,          // numMusic
   "d_",              // musPrefix
   "e1m1",            // defMusName
   DOOMDEFSOUND,      // defSoundName
   doom_skindefs,     // skinSounds
   doom_soundnums,    // playerSounds

   2,                // switchEpisode
   &DoomSkyData,     // skyData

   NULL,             // defaultORs

   "ENDOOM",         // endTextName
   quitsounds,       // exitSounds
};

//
// The Ultimate DOOM (4 episodes)
//
static gamemodeinfo_t giDoomRetail =
{
   retail,           // id
   Game_DOOM,        // type
   DOOM_GIFLAGS,     // flags
   
   VNAME_DOOM_RET,   // versionName
   FNAME_DOOM_R,     // freeVerName
   BANNER_DOOM_RET,  // startupBanner
   &gi_path_doomu,   // iwadPath
   
   demostates_udoom, // demoStates
   170,              // titleTics
   0,                // advisorTics
   11*TICRATE,       // pageTics
   mus_intro,        // titleMusNum

   DOOMMENUBACK,     // menuBackground
   DOOMCREDITBK,     // creditBackground
   20,               // creditY
   12,               // creditTitleStep
   &giSkullCursor,   // menuCursor
   &menu_main,       // mainMenu
   &menu_savegame,   // saveMenu
   &menu_loadgame,   // loadMenu
   &menu_newgame,    // newGameMenu
   doomMenuSounds,   // menuSounds
   S_TBALL1,         // transFrame
   sfx_shotgn,       // skvAtkSound
   CR_RED,           // unselectColor
   CR_GRAY,          // selectColor
   CR_GREEN,         // variableColor
   0,                // menuOffset

   DOOMBRDRFLAT,     // borderFlat
   &giDoomBorder,    // border

   &cr_red,          // defTextTrans
   CR_RED,           // colorNormal
   CR_GRAY,          // colorHigh
   CR_GOLD,          // colorError
   40,               // c_numCharsPerLine
   sfx_tink,         // c_BellSound
   sfx_tink,         // c_ChatSound
   CONBACK_COMRET,   // consoleBack
   0,                // blackIndex
   4,                // whiteIndex
   NUMCARDS,         // numHUDKeys

   &DoomStatusBar,   // StatusBar

   DOOMMARKS,        // markNumFmt

   "M_PAUSE",        // pausePatch

   4,                // numEpisodes
   DoomExitRules,    // exitRules
   MT_TFOG,          // teleFogType
   0,                // teleFogHeight
   sfx_telept,       // teleSound
   100,              // thrustFactor
   "DoomMarine",     // defPClassName
   NULL,             // defTranslate
   DoomBossSpecs,    // bossRules

   INTERPIC_DOOM,     // interPic
   mus_inter,         // interMusNum
   &giDoomFText,      // fTextPos
   &DoomIntermission, // interfuncs
   DEF_DOOM_FINALE,   // teleEndGameFinaleType
   &DoomFinale,       // finaleData

   S_music,           // s_music
   S_MusicForMapDoom, // MusicForMap
   mus_None,          // musMin
   NUMMUSIC,          // numMusic
   "d_",              // musPrefix
   "e1m1",            // defMusName
   DOOMDEFSOUND,      // defSoundName
   doom_skindefs,     // skinSounds
   doom_soundnums,    // playerSounds

   2,                // switchEpisode
   &DoomSkyData,     // skyData

   NULL,             // defaultORs

   "ENDOOM",         // endTextName
   quitsounds,       // exitSounds
};

//
// DOOM II / Final DOOM
//
static gamemodeinfo_t giDoomCommercial =
{
   commercial,       // id
   Game_DOOM,        // type
   DOOM_GIFLAGS | GIF_MAPXY | GIF_WOLFHACK | GIF_SETENDOFGAME, // flags

   VNAME_DOOM2,      // versionName
   FNAME_DOOM2,      // freeVerName
   BANNER_DOOM2,     // startupBanner
   &gi_path_doom2,   // iwadPath

   demostates_doom2, // demoStates
   11*TICRATE,       // titleTics
   0,                // advisorTics
   11*TICRATE,       // pageTics
   mus_dm2ttl,       // titleMusNum

   DOOMMENUBACK,     // menuBackground
   DM2CREDITBK,      // creditBackground
   20,               // creditY
   12,               // creditTitleStep
   &giSkullCursor,   // menuCursor
   &menu_main,       // mainMenu
   &menu_savegame,   // saveMenu
   &menu_loadgame,   // loadMenu
   &menu_newgame,    // newGameMenu
   doomMenuSounds,   // menuSounds
   S_TBALL1,         // transFrame
   sfx_shotgn,       // skvAtkSound
   CR_RED,           // unselectColor
   CR_GRAY,          // selectColor
   CR_GREEN,         // variableColor
   0,                // menuOffset

   DM2BRDRFLAT,      // borderFlat
   &giDoomBorder,    // border

   &cr_red,          // defTextTrans
   CR_RED,           // colorNormal
   CR_GRAY,          // colorHigh
   CR_GOLD,          // colorError
   40,               // c_numCharsPerLine
   sfx_tink,         // c_BellSound
   sfx_radio,        // c_ChatSound
   CONBACK_COMRET,   // consoleBack
   0,                // blackIndex
   4,                // whiteIndex
   NUMCARDS,         // numHUDKeys

   &DoomStatusBar,   // StatusBar

   DOOMMARKS,        // markNumFmt

   "M_PAUSE",        // pausePatch

   1,                // numEpisodes
   Doom2ExitRules,   // exitRules
   MT_TFOG,          // teleFogType
   0,                // teleFogHeight
   sfx_telept,       // teleSound
   100,              // thrustFactor
   "DoomMarine",     // defPClassName
   NULL,             // defTranslate
   Doom2BossSpecs,   // bossRules

   INTERPIC_DOOM,     // interPic
   mus_dm2int,        // interMusNum
   &giDoomFText,      // fTextPos
   &DoomIntermission, // interfuncs
   FINALE_TEXT,       // teleEndGameFinaleType
   &Doom2Finale,      // finaleData

   S_music,            // s_music
   S_MusicForMapDoom2, // MusicForMap
   mus_None,           // musMin
   NUMMUSIC,           // numMusic
   "d_",               // musPrefix
   "runnin",           // defMusName
   DOOMDEFSOUND,       // defSoundName
   doom_skindefs,      // skinSounds
   doom_soundnums,     // playerSounds

   3,                // switchEpisode
   &Doom2SkyData,    // skyData

   NULL,             // defaultORs

   "ENDOOM",         // endTextName
   quitsounds2,      // exitSounds
};

//
// Heretic Shareware
//
static gamemodeinfo_t giHereticSW =
{
   hereticsw,        // id
   Game_Heretic,     // type
   GIF_SHAREWARE | HERETIC_GIFLAGS, // flags

   VNAME_HTIC_SW,    // versionName
   NULL,             // freeVerName
   BANNER_HTIC_SW,   // startupBanner
   &gi_path_hticsw,  // iwadPath

   demostates_hsw,   // demoStates
   210,              // titleTics
   140,              // advisorTics
   200,              // pageTics
   hmus_titl,        // titleMusNum

   HTICMENUBACK,     // menuBackground
   HTICCREDITBK,     // creditBackground
   8,                // creditY
   8,                // creditTitleStep
   &giArrowCursor,   // menuCursor
   &menu_hmain,      // mainMenu
   &menu_hsavegame,  // saveMenu
   &menu_hloadgame,  // loadMenu
   &menu_hnewgame,   // newGameMenu
   hticMenuSounds,   // menuSounds
   S_MUMMYFX1_1,     // transFrame
   sfx_gldhit,       // skvAtkSound
   CR_GRAY,          // unselectColor
   CR_RED,           // selectColor
   CR_GREEN,         // variableColor
   4,                // menuOffset

   HSWBRDRFLAT,      // borderFlat
   &giHticBorder,    // border

   &cr_gray,         // defTextTrans
   CR_GRAY,          // colorNormal
   CR_GOLD,          // colorHigh
   CR_RED,           // colorError
   40,               // c_numCharsPerLine
   sfx_chat,         // c_BellSound
   sfx_chat,         // c_ChatSound
   CONBACK_HERETIC,  // consoleBack
   0,                // blackIndex
   35,               // whiteIndex
   3,                // numHUDKeys

   &HticStatusBar,   // StatusBar

   HTICMARKS,        // markNumFmt

   "PAUSED",         // pausePatch

   1,                // numEpisodes
   HereticExitRules, // exitRules
   MT_HTFOG,         // teleFogType
   32*FRACUNIT,      // teleFogHeight
   sfx_htelept,      // teleSound
   150,              // thrustFactor
   "Corvus",         // defPClassName
   DEFTL_HERETIC,    // defTranslate
   HereticBossSpecs, // bossRules

   INTERPIC_DOOM,     // interPic
   hmus_intr,         // interMusNum
   &giHticFText,      // fTextPos
   &HticIntermission, // interfuncs
   DEF_HTIC_FINALE,   // teleEndGameFinaleType
   &HereticFinale,    // finaleData

   H_music,           // s_music
   S_MusicForMapHtic, // MusicForMap
   hmus_None,         // musMin
   NUMHTICMUSIC,      // numMusic
   "mus_",            // musPrefix
   "e1m1",            // defMusName
   HTICDEFSOUND,      // defSoundName
   htic_skindefs,     // skinSounds
   htic_soundnums,    // playerSounds

   1,                // switchEpisode
   &HereticSkyData,  // skyData

   &HereticDefaultORs, // defaultORs

   "ENDTEXT",        // endTextName
   NULL,             // exitSounds
};

//
// Heretic Registered / Heretic: Shadow of the Serpent Riders
//
// The only difference between registered and SoSR is the
// number of episodes, which is patched in this structure at
// runtime.
//
static gamemodeinfo_t giHereticReg =
{
   hereticreg,       // id
   Game_Heretic,     // type   
   HERETIC_GIFLAGS,  // flags
   
   VNAME_HTIC_REG,   // versionName
   NULL,             // freeVerName
   BANNER_HTIC_REG,  // startupBanner
   &gi_path_hticreg, // iwadPath

   demostates_hreg,  // demoStates
   210,              // titleTics
   140,              // advisorTics
   200,              // pageTics
   hmus_titl,        // titleMusNum

   HTICMENUBACK,     // menuBackground
   HTICCREDITBK,     // creditBackground
   8,                // creditY
   8,                // creditTitleStep
   &giArrowCursor,   // menuCursor
   &menu_hmain,      // mainMenu
   &menu_hsavegame,  // saveMenu
   &menu_hloadgame,  // loadMenu
   &menu_hnewgame,   // newGameMenu
   hticMenuSounds,   // menuSounds
   S_MUMMYFX1_1,     // transFrame
   sfx_gldhit,       // skvAtkSound
   CR_GRAY,          // unselectColor
   CR_RED,           // selectColor
   CR_GREEN,         // variableColor
   4,                // menuOffset

   HREGBRDRFLAT,     // borderFlat
   &giHticBorder,    // border

   &cr_gray,         // defTextTrans
   CR_GRAY,          // colorNormal
   CR_GOLD,          // colorHigh
   CR_RED,           // colorError
   40,               // c_numCharsPerLine
   sfx_chat,         // c_BellSound
   sfx_chat,         // c_ChatSound
   CONBACK_HERETIC,  // consoleBack
   0,                // blackIndex
   35,               // whiteIndex
   3,                // numHUDKeys

   &HticStatusBar,   // StatusBar

   HTICMARKS,        // markNumFmt

   "PAUSED",         // pausePatch

   4,                // numEpisodes -- note 6 for SoSR gamemission
   HereticExitRules, // exitRules
   MT_HTFOG,         // teleFogType
   32*FRACUNIT,      // teleFogHeight
   sfx_htelept,      // teleSound
   150,              // thrustFactor
   "Corvus",         // defPClassName
   DEFTL_HERETIC,    // defTranslate
   HereticBossSpecs, // bossRules

   INTERPIC_DOOM,     // interPic
   hmus_intr,         // interMusNum
   &giHticFText,      // fTextPos
   &HticIntermission, // interfuncs
   DEF_HTIC_FINALE,   // teleEndGameFinaleType
   &HereticFinale,    // finaleData

   H_music,           // s_music
   S_MusicForMapHtic, // MusicForMap
   hmus_None,         // musMin
   NUMHTICMUSIC,      // numMusic
   "mus_",            // musPrefix
   "e1m1",            // defMusName
   HTICDEFSOUND,      // defSoundName
   htic_skindefs,     // skinSounds
   htic_soundnums,    // playerSounds

   2,                 // switchEpisode
   &HereticSkyData,   // skyData
   
   &HereticDefaultORs, // defaultORs

   "ENDTEXT",          // endTextName
   NULL,               // exitSounds
};

// Game Mode Info Array
gamemodeinfo_t *GameModeInfoObjects[NumGameModes] =
{
   &giDoomSW,         // shareware
   &giDoomReg,        // registered
   &giDoomCommercial, // commercial
   &giDoomRetail,     // retail
   &giHereticSW,      // hereticsw
   &giHereticReg,     // hereticreg
   &giDoomReg,        // indetermined - act like Doom, mostly.
};

//
// D_InitGameInfo
//
// Called after GameModeInfo is set to normalize some fields.
// EDF makes this necessary.
//
void D_InitGameInfo(void)
{
#ifdef RANGECHECK
   if(!GameModeInfo)
      I_Error("D_InitGameInfo: called before GameModeInfo set\n");
#endif

   GameModeInfo->teleFogType = E_SafeThingType(GameModeInfo->teleFogType);
}

//
// OVERRIDE macro
//
// Keeps me from typing this over and over :)
//
#define OVERRIDE(a, b) \
   if(mi-> a ## OR != b) \
      gi-> a = mi-> a ## OR

//
// D_SetGameModeInfo
//
// Sets GameModeInfo, sets the missionInfo pointer, and then overrides any
// data in GameModeInfo for which the missioninfo object has a replacement
// value. This prevents checking for overrides throughout the source.
//
void D_SetGameModeInfo(GameMode_t mode, GameMission_t mission)
{
   gamemodeinfo_t *gi;
   missioninfo_t  *mi;

   GameModeInfo = gi = GameModeInfoObjects[mode];

   // If gamemode == indetermined, change the id in the structure.
   // (We will be using object giDoomReg in that case).
   if(mode == indetermined)
      gi->id = indetermined;

   // set the missioninfo pointer
   gi->missionInfo = mi = MissionInfoObjects[mission];

   // If gamemode == indetermined, we want to use the unknown gamemission
   // information to set overrides, but *not* to actually set the mission
   // information. Turns out the "none" value of gamemission has never 
   // been used, and some code in the engine might crash if gamemission is
   // actually set to "none".
   if(mode == indetermined)
      mi = &gmUnknown;

   // apply overrides
   OVERRIDE(versionName,      NULL);
   OVERRIDE(startupBanner,    NULL);
   OVERRIDE(numEpisodes,         0);
   OVERRIDE(iwadPath,         NULL);
   OVERRIDE(finaleData,       NULL);
   OVERRIDE(mainMenu,         NULL);
   OVERRIDE(menuBackground,   NULL);
   OVERRIDE(creditBackground, NULL);
   OVERRIDE(consoleBack,      NULL);
   OVERRIDE(interPic,         NULL);
   OVERRIDE(exitRules,        NULL);
   
   // Note: demostates are not overridden here, see below.
}

//
// D_InitGMIPostWads
//
// haleyjd 11/12/09: Conditionally applies missioninfo overrides which are
// contingent upon the presence of certain lumps. This must be called after
// W_InitMultipleFiles (and is done so immediately afterward), in order to
// account for any PWADs loaded.
//
void D_InitGMIPostWads(void)
{
   gamemodeinfo_t *gi = GameModeInfo;
   missioninfo_t  *mi = gi->missionInfo;

   // apply the demoStates override from the missioninfo conditionally:
   // * if MI_DEMOIFDEMO4 is not set, then always override.
   // * if MI_DEMOIFDEMO4 IS set, then only if DEMO4 actually exists.
   if(!(mi->flags & MI_DEMOIFDEMO4) || W_CheckNumForName("DEMO4") >= 0)
      OVERRIDE(demoStates, NULL);
}

// EOF
