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
// Purpose: Gamemode information.
//  Stores all gamemode-dependent information in one location and
//  lends more structure to other modules.
//
// Authors: James Haley, Max Waine, Xaser Acheron, Derek MacDonald,
//  Simone Ivanish
//

#include "z_zone.h"
#include "i_system.h"

#include "autopalette.h"
#include "d_deh.h"
#include "d_gi.h"
#include "doomstat.h"
#include "doomdef.h"
#include "e_inventory.h"
#include "e_lib.h"
#include "e_things.h"
#include "f_finale.h"
#include "hi_stuff.h"
#include "info.h"
#include "mn_htic.h"
#include "mn_menus.h"
#include "p_info.h"
#include "p_skin.h"
#include "r_sky.h"
#include "r_textur.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "sounds.h"
#include "v_video.h"
#include "w_wad.h"
#include "wi_stuff.h"

// clang-format off

// definitions

// automap number mark format strings
constexpr const char DOOMMARKS[]    = "AMMNUM%d";
constexpr const char HTICMARKS[]    = "SMALLIN%d";

// menu background flats
constexpr const char DOOMMENUBACK[] = "FLOOR4_6";
constexpr const char HACXMENUBACK[] = "CONS1_5";
constexpr const char HTICMENUBACK[] = "FLOOR16";

// credit background flats
constexpr const char DOOMCREDITBK[] = "NUKAGE1";
constexpr const char DM2CREDITBK[]  = "SLIME05";
constexpr const char HACXCREDITBK[] = "SLIME01";
constexpr const char HTICCREDITBK[] = "FLTWAWA1";

// border flats
constexpr const char DOOMBRDRFLAT[] = "FLOOR7_2";
constexpr const char DM2BRDRFLAT[]  = "GRNROCK";
constexpr const char HREGBRDRFLAT[] = "FLAT513";
constexpr const char HSWBRDRFLAT[]  = "FLOOR04";

// Default sound names
// Note: these are lump names, not sound mnemonics
constexpr const char DOOMDEFSOUND[] = "DSPISTOL";
constexpr const char HTICDEFSOUND[] = "GLDHIT";

// Default console backdrops
// Used when no "CONSOLE" lump has been provided.
// Most gamemodes use the titlepic, which is darkened by code in c_io.c.
// Ultimate DOOM and DOOM II have a suitable graphic in the INTERPIC.
constexpr const char CONBACK_DEFAULT[] = "TITLEPIC";
constexpr const char CONBACK_HERETIC[] = "TITLE";

// Version names
constexpr const char VNAME_DOOM_SW[]   = "DOOM Shareware version";
constexpr const char VNAME_DOOM_REG[]  = "DOOM Registered version";
constexpr const char VNAME_DOOM_RET[]  = "Ultimate DOOM version";
constexpr const char VNAME_DOOM2[]     = "DOOM II version";
constexpr const char VNAME_TNT[]       = "Final DOOM: TNT - Evilution version";
constexpr const char VNAME_PLUT[]      = "Final DOOM: The Plutonia Experiment version";
constexpr const char VNAME_HACX[]      = "HACX - Twitch 'n Kill version";
constexpr const char VNAME_DISK[]      = "DOOM II disk version";
constexpr const char VNAME_PSX[]       = "DOOM: Custom PlayStation Edition version";
constexpr const char VNAME_HTIC_SW[]   = "Heretic Shareware version";
constexpr const char VNAME_HTIC_REG[]  = "Heretic Registered version";
constexpr const char VNAME_HTIC_BETA[] = "Heretic Beta version";
constexpr const char VNAME_HTIC_SOSR[] = "Heretic: Shadow of the Serpent Riders version";
constexpr const char VNAME_UNKNOWN[]   = "Unknown Game version. May not work.";

// FreeDoom/Blasphemer/Etc override names
constexpr const char FNAME_DOOM_SW[]   = "Freedoom Demo version";
constexpr const char FNAME_DOOM_R[]    = "Freedoom Phase 1 version";
constexpr const char FNAME_DOOM2[]     = "Freedoom Phase 2 version";

// BFG Edition override names
constexpr const char BFGNAME_DOOM[]    = "Ultimate DOOM - BFG Edition version";
constexpr const char BFGNAME_DOOM2[]   = "DOOM II - BFG Edition version";

// Startup banners
constexpr const char BANNER_DOOM_SW[]   = "DOOM Shareware Startup";
constexpr const char BANNER_DOOM_REG[]  = "DOOM Registered Startup";
constexpr const char BANNER_DOOM_RET[]  = "The Ultimate DOOM Startup";
constexpr const char BANNER_DOOM2[]     = "DOOM 2: Hell on Earth";
constexpr const char BANNER_TNT[]       = "DOOM 2: TNT - Evilution";
constexpr const char BANNER_PLUT[]      = "DOOM 2: Plutonia Experiment";
constexpr const char BANNER_HACX[]      = "HACX - Twitch 'n Kill";
constexpr const char BANNER_PSX[]       = "DOOM: Custom PlayStation Edition Startup";
constexpr const char BANNER_HTIC_SW[]   = "Heretic Shareware Startup";
constexpr const char BANNER_HTIC_REG[]  = "Heretic Registered Startup";
constexpr const char BANNER_HTIC_SOSR[] = "Heretic: Shadow of the Serpent Riders";
constexpr const char BANNER_UNKNOWN[]   = "Public DOOM";

// Default intermission pics
constexpr const char INTERPIC_DOOM[]    = "INTERPIC";

// Default finales caused by Teleport_EndGame special
#define DEF_DOOM_FINALE  FINALE_DOOM_CREDITS
#define DEF_HTIC_FINALE  FINALE_HTIC_CREDITS

// Default translations for MF4_AUTOTRANSLATE flag.
// Graphic is assumed to be in the DOOM palette.
constexpr const char DEFTL_HERETIC[]  = "TR_DINH";

// Default cast call text string coordinates
#define CAST_DEFTITLEY 20
#define CAST_DEFNAMEY  180

//
// Common Flags
//
// The following sets of GameModeInfo flags define the basic characteristics
// of the intrinsic gametypes.
//

#define DOOM_GIFLAGS \
    (GIF_PRBOOMTALLSKY | GIF_FLIGHTINERTIA | GIF_HASEXITSOUNDS | GIF_CLASSICMENUS | \
     GIF_SKILL5RESPAWN | GIF_SKILL5WARNING | GIF_HUDSTATBARNAME | GIF_DOOMWEAPONOFFSET)

#define HERETIC_GIFLAGS \
    (GIF_MNBIGFONT | GIF_SAVESOUND | GIF_HASADVISORY | GIF_SHADOWTITLES | \
     GIF_HASMADMELEE | GIF_CENTERHUDMSG | GIF_CHEATSOUND | GIF_CHASEFAST | GIF_WPNSWITCHSUPER)

#define FINALDOOM_MIFLAGS (MI_DEMOIFDEMO4 | MI_NOTELEPORTZ)

extern menu_t menu_episode, menu_episodeDoom2Stub, menu_hepisode;
// globals

// holds the address of the gamemodeinfo_t for the current gamemode,
// determined at startup
gamemodeinfo_t *GameModeInfo;

// data

//
// Menu Cursors
//

// doom menu skull cursor

static const char *skullCursorPatches[] = {
    "M_SKULL1",
    "M_SKULL2",
};

static gimenucursor_t giSkullCursor =
{
    8,                 // blinktime
    2,                 // numpatches
    skullCursorPatches // patches
};

// heretic menu arrow cursor

static const char *arrowCursorPatches[] = {
    "M_SLCTR1",
    "M_SLCTR2",
};

static gimenucursor_t giArrowCursor =
{
    16,                // blinktime
    2,                 // numpatches
    arrowCursorPatches // patch array
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
// PSprite global scales
//

static giscale_t giPsprNoScale  = { 1.0f, 1.0f      };
static giscale_t giPsprPSXScale = { 1.0f, 5.0f/6.0f };

//
// Menu Sounds
//

static int doomMenuSounds[MN_SND_NUMSOUNDS] = {
    sfx_swtchn, // activate
    sfx_swtchx, // deactivate
    sfx_pstop,  // key up/down
    sfx_pistol, // command
    sfx_swtchx, // previous menu
    sfx_stnmov, // key left/right
};

static int hticMenuSounds[MN_SND_NUMSOUNDS] = {
    sfx_hdorcls, // activate
    sfx_hdorcls, // deactivate
    sfx_hswitch, // key up/down
    sfx_hdorcls, // command
    sfx_hswitch, // previous menu
    sfx_keyup,   // key left/right
};

// gamemode-dependent menus (defined in mn_menus.c et al.)
extern menu_t menu_savegame;
extern menu_t menu_loadgame;
extern menu_t menu_newgame;
extern menu_t menu_hnewgame;

//
// Skin Sound Defaults
//

static const char *doom_skindefs[NUMSKINSOUNDS] = {
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
    "none",
};

static int doom_soundnums[NUMSKINSOUNDS] = {
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
    sfx_jump,
};

static const char *htic_skindefs[NUMSKINSOUNDS] = {
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
    "none",
    "none",
};

static int htic_soundnums[NUMSKINSOUNDS] = {
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
    sfx_hnoway,
    sfx_jump,
};

//
// Exit sounds
//

static int quitsounds[8] = {
    sfx_pldeth,
    sfx_dmpain,
    sfx_popain,
    sfx_slop,
    sfx_telept,
    sfx_posit1,
    sfx_posit3,
    sfx_sgtatk
};

static int quitsounds2[8] = {
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

// Empty exit rules set for gamemodes with no special default behaviors
static exitrule_t NullExitRules[] = {
    { -2 }
};

static exitrule_t DoomExitRules[] = {
    {  1,  9, 3, false }, // normal exit: E1M9 -> E1M4
    {  2,  9, 5, false }, // normal exit: E2M9 -> E2M6
    {  3,  9, 6, false }, // normal exit: E3M9 -> E3M7
    {  4,  9, 2, false }, // normal exit: E4M9 -> E4M3
    { -1, -1, 8, true  }, // secret exits: any map goes to ExM9
    { -2 }
};

static exitrule_t Doom2ExitRules[] = {
    {  1, 31, 15, false }, // normal exit: MAP31 -> MAP16
    {  1, 32, 15, false }, // normal exit: MAP32 -> MAP16
    {  1, 15, 30, true  }, // secret exit: MAP15 -> MAP31
    {  1, 31, 31, true  }, // secret exit: MAP31 -> MAP32
    { -2 }
};

static exitrule_t DiskExitRules[] = {
    {  1, 31, 15, false }, // normal exit: MAP31 -> MAP16
    {  1, 32, 15, false }, // normal exit: MAP32 -> MAP16
    {  1, 33,  2, false }, // normal exit: MAP33 -> MAP03
    {  1,  2, 32, true  }, // secret exit: MAP02 -> MAP33
    {  1, 15, 30, true  }, // secret exit: MAP15 -> MAP31
    {  1, 31, 31, true  }, // secret exit: MAP31 -> MAP32
    { -2 }
};

static exitrule_t HereticExitRules[] = {
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

static finalerule_t DoomFinaleRules[] = {
    {  1,  8, "BGFLATE1", "E1TEXT", FINALE_DOOM_CREDITS, false, false, true  },
    {  2,  8, "BGFLATE2", "E2TEXT", FINALE_DOOM_DEIMOS,  false, false, true  },
    {  3,  8, "BGFLATE3", "E3TEXT", FINALE_DOOM_BUNNY,   false, false, true  },
    {  4,  8, "BGFLATE4", "E4TEXT", FINALE_DOOM_MARINE,  false, false, true  },
    { -1,  8, "BGFLATE1", "E1TEXT", FINALE_DOOM_CREDITS, false, false, true  }, // ExM8 default
    { -1, -1, "BGFLATE1", nullptr,  FINALE_TEXT,         false, false, false }, // other default
    { -2 }
};

static finaledata_t DoomFinale =
{
   mus_victor,
   false,
   DoomFinaleRules
};

// DOOM 2

static finalerule_t Doom2FinaleRules[] = {
    {  1,  6, "BGFLAT06", "C1TEXT", FINALE_TEXT, false, false, false },
    {  1, 11, "BGFLAT11", "C2TEXT", FINALE_TEXT, false, false, false },
    {  1, 20, "BGFLAT20", "C3TEXT", FINALE_TEXT, false, false, false },
    {  1, 30, "BGFLAT30", "C4TEXT", FINALE_TEXT, true,  false, false }, // end of game
    {  1, 15, "BGFLAT15", "C5TEXT", FINALE_TEXT, false, true,  false }, // only after secret
    {  1, 31, "BGFLAT31", "C6TEXT", FINALE_TEXT, false, true,  false }, // only after secret
    { -1, -1, "BGFLAT06", nullptr,  FINALE_TEXT, false, false, false },
    { -2 }
};

static finaledata_t Doom2Finale = {
    mus_read_m,
    false,
    Doom2FinaleRules
};

// Final Doom: TNT

static finalerule_t TNTFinaleRules[] = {
    {  1,  6, "BGFLAT06", "T1TEXT", FINALE_TEXT, false, false, false },
    {  1, 11, "BGFLAT11", "T2TEXT", FINALE_TEXT, false, false, false },
    {  1, 20, "BGFLAT20", "T3TEXT", FINALE_TEXT, false, false, false },
    {  1, 30, "BGFLAT30", "T4TEXT", FINALE_TEXT, true,  false, false }, // end of game
    {  1, 15, "BGFLAT15", "T5TEXT", FINALE_TEXT, false, true,  false }, // only after secret
    {  1, 31, "BGFLAT31", "T6TEXT", FINALE_TEXT, false, true,  false }, // only after secret
    { -1, -1, "BGFLAT06", nullptr,  FINALE_TEXT, false, false, false },
    { -2 }
};

static finaledata_t TNTFinale =
{
    mus_read_m,
    false,
    TNTFinaleRules
};

// Final Doom: Plutonia

static finalerule_t PlutFinaleRules[] = {
    {  1,  6, "BGFLAT06", "P1TEXT", FINALE_TEXT, false, false, false },
    {  1, 11, "BGFLAT11", "P2TEXT", FINALE_TEXT, false, false, false },
    {  1, 20, "BGFLAT20", "P3TEXT", FINALE_TEXT, false, false, false },
    {  1, 30, "BGFLAT30", "P4TEXT", FINALE_TEXT, true,  false, false }, // end of game
    {  1, 15, "BGFLAT15", "P5TEXT", FINALE_TEXT, false, true,  false }, // only after secret
    {  1, 31, "BGFLAT31", "P6TEXT", FINALE_TEXT, false, true,  false }, // only after secret
    { -1, -1, "BGFLAT06", nullptr,  FINALE_TEXT, false, false, false },
    { -2 }
};

static finaledata_t PlutFinale =
{
   mus_read_m,
   false,
   PlutFinaleRules
};

// Heretic (Shareware / Registered / SoSR)

static finalerule_t HereticFinaleRules[] = {
    {  1,  8, "BGFLATHE1", "H1TEXT", FINALE_HTIC_CREDITS, false, false, true  },
    {  2,  8, "BGFLATHE2", "H2TEXT", FINALE_HTIC_WATER,   false, false, true  },
    {  3,  8, "BGFLATHE3", "H3TEXT", FINALE_HTIC_DEMON,   false, false, true  },
    {  4,  8, "BGFLATHE4", "H4TEXT", FINALE_HTIC_CREDITS, false, false, true  },
    {  5,  8, "BGFLATHE5", "H5TEXT", FINALE_HTIC_CREDITS, false, false, true  },
    { -1,  8, "BGFLATHE1", "H1TEXT", FINALE_HTIC_CREDITS, false, false, true  }, // ExM8 default
    { -1, -1, "BGFLATHE1", nullptr,  FINALE_TEXT,         false, false, false }, // other default
};

static finaledata_t HereticFinale =
{
    hmus_cptd,
    true,
    HereticFinaleRules
};

// Unknown gamemode

static finalerule_t UnknownFinaleRules[] = {
    { -1, -1, "F_SKY2", nullptr, FINALE_TEXT, false, false, false },
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

static skyrule_t DoomSkyRules[] = {
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

static skyflat_t DoomSkyFlats[] = {
    { "F_SKY1", nullptr, 0, 0, 0 },
    { "F_SKY2", nullptr, 0, 0, 0 },
    { nullptr,  nullptr, 0, 0, 0 }
};

// Doom II Gamemodes

static skyrule_t Doom2SkyRules[] = {
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

// PSX Mission

static skyrule_t PSXSkyRules[] = {
    { -1, "SKY01" },
    { -2 }
};

static skydata_t PSXSkyData =
{
   GI_SKY_IFMAPLESSTHAN,
   PSXSkyRules
};

static skyflat_t PSXSkyFlats[] = {
    { "F_SKY01", "SKY01", 0, 0, 0 },
    { "F_SKY02", "SKY02", 0, 0, 0 },
    { "F_SKY03", "SKY03", 0, 0, 0 },
    { "F_SKY04", "SKY04", 0, 0, 0 },
    { "F_SKY05", "SKY05", 0, 0, 0 },
    { "F_SKY06", "SKY06", 0, 0, 0 },
    { "F_SKY07", "SKY07", 0, 0, 0 },
    { "F_SKY09", "SKY09", 0, 0, 0 },
    { nullptr,   nullptr, 0, 0, 0 }
};

// Heretic Gamemodes

static skyrule_t HereticSkyRules[] = {
    {  1, "SKY1" },
    {  2, "SKY2" },
    {  3, "SKY3" },
    {  4, "SKY1" },
    {  5, "SKY3" },
    { -1, "SKY1" },
    { -2 }
};

static skydata_t HereticSkyData = {
    GI_SKY_IFEPISODE,
    HereticSkyRules
};

//
// Default boss spec rules
//

// Doom gamemodes

static bspecrule_t DoomBossSpecs[] = {
    {  1,  8, BSPEC_E1M8 },
    {  2,  8, BSPEC_E2M8 },
    {  3,  8, BSPEC_E3M8 },
    {  4,  6, BSPEC_E4M6 },
    {  4,  8, BSPEC_E4M8 },
    {  5,  8, BSPEC_E5M8 }, // for compatibility with earlier EE's
    { -1 }
};

// Doom II gamemodes

static bspecrule_t Doom2BossSpecs[] = {
    {  1,  7, BSPEC_MAP07_1 | BSPEC_MAP07_2 }, // DOOM II MAP07 has two specials
    { -1 }
};

// Heretic gamemodes

static bspecrule_t HereticBossSpecs[] = {
    {  1,  8, BSPEC_E1M8 },
    {  2,  8, BSPEC_E2M8 },
    {  3,  8, BSPEC_E3M8 },
    {  4,  6, BSPEC_E4M6 }, // for compatibility with earlier EE's
    {  4,  8, BSPEC_E4M8 },
    {  5,  8, BSPEC_E5M8 },
    { -1 }
};

//
// "Same Level" mappings
//
// Entirely for various versions of Heretic; these rules make you
// return to the same map you just finished when exiting the very
// last map in the IWAD.
//

// Heretic Registered
static samelevel_t hticRegSameLevels[] = {
    {  4,  2 },
    { -1, -1 }
};

// Heretic Beta
static samelevel_t hticBetaSameLevels[] = {
    {  1,  4 },
    { -1, -1 }
};

// Heretic SoSR
static samelevel_t hticSoSRSameLevels[] = {
    {  6,  4 },
    { -1, -1 }
};

//
// Key card name lookups
//
// These arrays are used by the status bar, HUD, cheats, etc. to map
// from old-style card numbers to the new artifact names, where it's
// still convenient to do this. Eventually we'll have some scripting
// solutions that take care of this kind of stuff.
//

// Key names in the original DOOM order, for use by HUD, status bar, cheats, etc.
static const char *DOOMCardNames[] = {
    ARTI_BLUECARD,
    ARTI_YELLOWCARD,
    ARTI_REDCARD,
    ARTI_BLUESKULL,
    ARTI_YELLOWSKULL,
    ARTI_REDSKULL
};

// Names for Heretic cards
static const char *HticCardNames[] = {
    ARTI_KEYBLUE,
    ARTI_KEYYELLOW,
    ARTI_KEYGREEN
};

//
// Default blood behavior for action arrays
//

static bloodtype_e bloodTypeForActionDOOM[NUMBLOODACTIONS] = {
    BLOODTYPE_DOOM,       // shoot
    BLOODTYPE_HERETIC,    // impact
    BLOODTYPE_HERETICRIP, // rip
    BLOODTYPE_CRUSH       // crush
};

static bloodtype_e bloodTypeForActionHtic[NUMBLOODACTIONS] = {
    BLOODTYPE_HERETIC,    // shoot
    BLOODTYPE_HERETIC,    // impact
    BLOODTYPE_HERETICRIP, // rip
    BLOODTYPE_CRUSH       // crush
};

// TODO: unused yet
/*
static bloodtype_e bloodTypeForActionHexen[NUMBLOODACTIONS] = {
    BLOODTYPE_HEXEN,    // shoot
    BLOODTYPE_HEXEN,    // impact
    BLOODTYPE_HEXENRIP, // rip
    BLOODTYPE_CRUSH     // crush
};

static bloodtype_e bloodTypeForActionStrife[NUMBLOODACTIONS] = {
    BLOODTYPE_STRIFE,     // shoot
    BLOODTYPE_HERETIC,    // impact
    BLOODTYPE_HERETICRIP, // rip
    BLOODTYPE_CRUSH       // crush
};
*/

//
// IWAD paths
//

// set by system config:
char *gi_path_doomsw;
char *gi_path_doomreg;
char *gi_path_doomu;
char *gi_path_doom2;
char *gi_path_bfgdoom2;
char *gi_path_tnt;
char *gi_path_plut;
char *gi_path_hacx;
char *gi_path_hticsw;
char *gi_path_hticreg;
char *gi_path_sosr;
char *gi_path_fdoom;
char *gi_path_fdoomu;
char *gi_path_freedm;
char *gi_path_rekkr;

char *gi_path_id24res;

//
// Default Override Objects
//
extern default_or_t HereticDefaultORs[];

//=============================================================================
//
// Mission Information Structures
//

//
// Doom
//
static missioninfo_t gmDoom = {
    doom,    // id
    0,       // flags
    "doom",  // gamePathName
    nullptr, // sameLevels

    0,       // addGMIFlags
    0,       // remGMIFlags
    nullptr, // versionNameOR
    nullptr, // startupBannerOR
    0,       // numEpisodesOR
    nullptr, // iwadPathOR
    nullptr, // finaleDataOR
};

//
// Doom 2
//
static missioninfo_t gmDoom2 =
{
    doom2,            // id
    MI_DOOM2MISSIONS, // flags
    "doom2",          // gamePathName
    nullptr,          // sameLevels

    0,       // addGMIFlags
    0,       // remGMIFlags
    nullptr, // versionNameOR
    nullptr, // startupBannerOR
    0,       // numEpisodesOR
    nullptr, // iwadPathOR
    nullptr, // finaleDataOR
};

//
// Final Doom - TNT: Evilution
//
static missioninfo_t gmFinalTNT = {
    pack_tnt,           // id
    FINALDOOM_MIFLAGS,  // flags
    "tnt",              // gamePathName
    nullptr,            // sameLevels

    GIF_LOSTSOULBOUNCE, // addGMIFlags
    0,                  // remGMIFlags
    VNAME_TNT,          // versionNameOR
    BANNER_TNT,         // startupBannerOR
    0,                  // numEpisodesOR
    &gi_path_tnt,       // iwadPathOR
    &TNTFinale,         // finaleDataOR
    nullptr,            // menuBackgroundOR
    nullptr,            // creditBackgroundOR
    demostates_udoom,   // demoStatesOR -- haleyjd 11/12/09: to play DEMO4
    nullptr,            // interPicOR
    nullptr,            // exitRulesOR
    mapnamest,          // levelNamesOR
};

//
// Final Doom - The Plutonia Experiment
//
static missioninfo_t gmFinalPlutonia = {
    pack_plut,          // id
    FINALDOOM_MIFLAGS,  // flags
    "plutonia",         // gamePathName
    nullptr,            // sameLevels

    GIF_LOSTSOULBOUNCE, // addGMIFlags
    0,                  // remGMIFlags
    VNAME_PLUT,         // versionNameOR
    BANNER_PLUT,        // startupBannerOR
    0,                  // numEpisodesOR
    &gi_path_plut,      // iwadPathOR
    &PlutFinale,        // finaleDataOR
    nullptr,            // menuBackgroundOR
    nullptr,            // creditBackgroundOR
    demostates_udoom,   // demoStatesOR -- haleyjd 11/12/09: to play DEMO4
    nullptr,            // interPicOR
    nullptr,            // exitRulesOR
    mapnamesp,          // levelNamesOR
};

// Special properties of BFG Edition mission:
// * DOOM II maps 31/32 rename themselves if not DEH or MAPINFO modified
// * Has MAP33: Betray w/secret exit on MAP02
// * Supports Doom 2 managed mission pack selection menu
// * Has a stupid M_GDHIGH lump
#define BFGMISSIONFLAGS (MI_WOLFNAMEHACKS|MI_HASBETRAY|MI_DOOM2MISSIONS|MI_NOGDHIGH)

//
// Disk version (Xbox and BFG Edition)
//
static missioninfo_t gmDisk = {
    pack_disk,          // id
    BFGMISSIONFLAGS,    // flags
    "doom2",            // gamePathName
    nullptr,            // sameLevels

    GIF_LOSTSOULBOUNCE, // addGMIFlags
    0,                  // remGMIFlags
    VNAME_DISK,         // versionNameOR
    nullptr,            // startupBannerOR
    0,                  // numEpisodesOR
    nullptr,            // iwadPathOR
    nullptr,            // finaleDataOR
    nullptr,            // menuBackgroundOR
    nullptr,            // creditBackgroundOR
    nullptr,            // demoStatesOR
    nullptr,            // interPicOR
    DiskExitRules,      // exitRulesOR
};

//
// HacX - Twitch 'n Kill Version 1.2
//
static missioninfo_t gmHacx = {
    pack_hacx,       // id
    0,               // flags
    "hacx",          // gamePathName
    nullptr,         // sameLevels

    0,               // addGMIFlags
    0,               // remGMIFlags
    VNAME_HACX,      // versionNameOR
    BANNER_HACX,     // startupBannerOR
    0,               // numEpisodesOR
    &gi_path_hacx,   // iwadPathOR
    nullptr,         // finaleDataOR
    HACXMENUBACK,    // menuBackgroundOR
    HACXCREDITBK,    // creditBackgroundOR
};

//
// PSX Doom, as generated by psxwadgen
//
static missioninfo_t gmPSX = {
    pack_psx,        // id
    0,               // flags
    "doom2",         // gamePathName - FIXME/TODO
    nullptr,         // sameLevels

    0,               // addGMIFlags
    0,               // remGMIFlags
    VNAME_PSX,       // versionNameOR
    BANNER_PSX,      // startupBannerOR
    0,               // numEpisodesOR
    nullptr,         // iwadPathOR
    nullptr,         // finaleDataOR
    nullptr,         // menuBackgroundOR
    nullptr,         // creditBackgroundOR
    nullptr,         // demoStatesOR
    nullptr,         // interPicOR
    NullExitRules,   // exitRulesOR
    nullptr,         // levelNamesOR
    0,               // randMusMaxOR
    &PSXSkyData,     // skyDataOR
    PSXSkyFlats,     // skyFlatsOR
    &giPsprPSXScale, // pspriteGlobalScaleOR
};

//
// Heretic
//
static missioninfo_t gmHeretic = {
    heretic,           // id
    0,                 // flags
    "heretic",         // gamePathName
    hticRegSameLevels, // sameLevels

    0,                 // addGMIFlags
    0,                 // remGMIFlags
    nullptr,           // versionNameOR
    nullptr,           // startupBannerOR
    0,                 // numEpisodesOR
    nullptr,           // iwadPathOR
    nullptr,           // finaleDataOR
};

//
// Heretic Beta Version
//
// Only difference here is that this mission only has three maps.
//
static missioninfo_t gmHereticBeta = {
    heretic,            // id
    0,                  // flags
    "heretic",          // gamePathName
    hticBetaSameLevels, // sameLevels

    0,                   // addGMIFlags
    0,                   // remGMIFlags
    VNAME_HTIC_BETA,     // versionNameOR
    nullptr,             // startupBannerOR
    0,                   // numEpisodesOR
    nullptr,             // iwadPathOR
    nullptr,             // finaleDataOR
    nullptr,             // menuBackgroundOR
    nullptr,             // creditBackgroundOR
    nullptr,             // demoStatesOR
    nullptr,             // interPicOR
    nullptr,             // exitRulesOR
    nullptr,             // levelNamesOR
    hmus_e1m3,           // randMusMaxOR
};

//
// Heretic: Shadow of the Serpent Riders
//
static missioninfo_t gmHereticSoSR = {
    hticsosr,           // id
    0,                  // flags
    "heretic",          // gamePathName
    hticSoSRSameLevels, // sameLevels

    0,                  // addGMIFlags
    0,                  // remGMIFlags
    VNAME_HTIC_SOSR,    // versionNameOR
    BANNER_HTIC_SOSR,   // startupBannerOR
    6,                  // numEpisodesOR
    &gi_path_sosr,      // iwadPathOR
    nullptr,            // finaleDataOR
};

//
// ???
//
static missioninfo_t gmUnknown = {
    none,           // id
    0,              // flags
    "doom",         // gamePathName
    nullptr,        // sameLevels

    0,              // addGMIFlags
    0,              // remGMIFlags
    VNAME_UNKNOWN,  // versionNameOR
    BANNER_UNKNOWN, // startupBannerOR
    0,              // numEpisodesOR
    nullptr,        // iwadPathOR
    &UnknownFinale, // finaleDataOR
};

// Mission info object array

missioninfo_t *MissionInfoObjects[NumGameMissions] = {
    &gmDoom,          // doom
    &gmDoom2,         // doom2
    &gmFinalTNT,      // pack_tnt
    &gmFinalPlutonia, // pack_plut
    &gmDisk,          // pack_disk
    &gmHacx,          // pack_hacx
    &gmPSX,           // pack_psx
    &gmHeretic,       // heretic
    &gmHereticBeta,   // hticbeta
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
static gamemodeinfo_t giDoomSW = {
    shareware,              // id
    Game_DOOM,              // type
    DOOM_GIFLAGS | GIF_SHAREWARE | GIF_NODIEHI, // flags

    VNAME_DOOM_SW,          // versionName
    FNAME_DOOM_SW,          // freeVerName
    nullptr,                // bfgEditionName
    BANNER_DOOM_SW,         // startupBanner
    &gi_path_doomsw,        // iwadPath

    demostates_doom,        // demoStates
    170,                    // titleTics
    0,                      // advisorTics
    11*TICRATE,             // pageTics
    mus_intro,              // titleMusNum

    DOOMMENUBACK,           // menuBackground
    DOOMCREDITBK,           // creditBackground
    20,                     // creditY
    12,                     // creditTitleStep
    &giSkullCursor,         // menuCursor
    &menu_main,             // mainMenu
    &menu_savegame,         // saveMenu
    &menu_loadgame,         // loadMenu
    &menu_newgame,          // newGameMenu
    &menu_episode,          // episodeMenu
    nullptr,                // menuStartMap
    doomMenuSounds,         // menuSounds
    S_TBALL1,               // transFrame
    sfx_shotgn,             // skvAtkSound
    CR_RED,                 // unselectColor
    CR_GRAY,                // selectColor
    CR_GREEN,               // variableColor
    CR_RED,                 // titleColor
    CR_GOLD,                // itemColor
    CR_RED,                 // bigFontItemColor
    0,                      // menuOffset
    MN_DoomNewGame,         // OnNewGame

    DOOMBRDRFLAT,           // borderFlat
    &giDoomBorder,          // border

    CR_RED,                 // defTextTrans
    CR_RED,                 // colorNormal
    CR_GRAY,                // colorHigh
    CR_GOLD,                // colorError
    40,                     // c_numCharsPerLine
    sfx_tink,               // c_BellSound
    sfx_tink,               // c_ChatSound
    CONBACK_DEFAULT,        // consoleBack
    0,                      // blackIndex
    4,                      // whiteIndex
    NUMCARDS,               // numHUDKeys
    DOOMCardNames,          // cardNames
    mapnames,               // levelNames
    P_DoomDefaultLevelName, // GetLevelName

    &DoomStatusBar,         // StatusBar

    DOOMMARKS,              // markNumFmt

    "M_PAUSE",              // pausePatch

    1,                      // numEpisodes
    DoomExitRules,          // exitRules
    "BulletPuff",           // puffType
    "DoomTeleFog",          // teleFogType
    0,                      // teleFogHeight
    sfx_telept,             // teleSound
    100,                    // thrustFactor
    GI_GIBFULLHEALTH,       // defaultGibHealth
    "DoomMarine",           // defPClassName
    nullptr,                // defTranslate
    DoomBossSpecs,          // bossRules
    LI_TYPE_DOOM,           // levelType
    "DoomBlood",            // bloodDefaultNormal
    "DoomBlood",            // bloodDefaultImpact
    "DoomBlood",            // bloodDefaultRIP
    "DoomBlood",            // bloodDefaultCrush
    bloodTypeForActionDOOM, // default behavior for action array
    "S_GIBS",               // defCrunchFrame
    2,                      // skillAmmoMultiplier
    meleecalc_doom,         // monsterMeleeRange
    8 * FRACUNIT,           // itemHeight
    nullptr,                // autoFlightArtifact
    45,                     // lookPitchUp
    45,                     // lookPitchDown

    INTERPIC_DOOM,          // interPic
    mus_inter,              // interMusNum
    P_DoomParTime,          // GetParTime
    &giDoomFText,           // fTextPos
    &DoomIntermission,      // interfuncs
    DEF_DOOM_FINALE,        // teleEndGameFinaleType
    &DoomFinale,            // finaleData
    CAST_DEFTITLEY,         // castTitleY
    CAST_DEFNAMEY,          // castNameY

    S_music,                // s_music
    S_MusicForMapDoom,      // MusicForMap
    S_DoomMusicCheat,       // MusicCheat
    mus_None,               // musMin
    NUMMUSIC,               // numMusic
    mus_e1m1,               // randMusMin
    mus_e1m9,               // randMusMax
    "D_",                   // musPrefix
    "e1m1",                 // defMusName
    DOOMDEFSOUND,           // defSoundName
    doom_skindefs,          // skinSounds
    doom_soundnums,         // playerSounds
    nullptr,                // titleMusName
    "DSSECRET",             // secretSoundName
    sfx_itmbk,              // defSecretSound

    1,                      // switchEpisode
    &DoomSkyData,           // skyData
    R_DoomTextureHacks,     // TextureHacks
    DoomSkyFlats,           // skyFlats
    &giPsprNoScale,         // pspriteGlobalScale

    nullptr,                // defaultORs

    "ENDOOM",               // endTextName
    quitsounds,             // exitSounds
};

//
// DOOM Registered
//
static gamemodeinfo_t giDoomReg = {
    registered,             // id
    Game_DOOM,              // type
    DOOM_GIFLAGS,           // flags

    VNAME_DOOM_REG,         // versionName
    FNAME_DOOM_R,           // freeVerName
    nullptr,                // bfgEditionName
    BANNER_DOOM_REG,        // startupBanner
    &gi_path_doomreg,       // iwadPath

    demostates_doom,        // demoStates
    170,                    // titleTics
    0,                      // advisorTics
    11*TICRATE,             // pageTics
    mus_intro,              // titleMusNum

    DOOMMENUBACK,           // menuBackground
    DOOMCREDITBK,           // creditBackground
    20,                     // creditY
    12,                     // creditTitleStep
    &giSkullCursor,         // menuCursor
    &menu_main,             // mainMenu
    &menu_savegame,         // saveMenu
    &menu_loadgame,         // loadMenu
    &menu_newgame,          // newGameMenu
    &menu_episode,          // episodeMenu
    nullptr,                // menuStartMap
    doomMenuSounds,         // menuSounds
    S_TBALL1,               // transFrame
    sfx_shotgn,             // skvAtkSound
    CR_RED,                 // unselectColor
    CR_GRAY,                // selectColor
    CR_GREEN,               // variableColor
    CR_RED,                 // titleColor
    CR_GOLD,                // itemColor
    CR_RED,                 // bigFontItemColor
    0,                      // menuOffset
    MN_DoomNewGame,         // OnNewGame

    DOOMBRDRFLAT,           // borderFlat
    &giDoomBorder,          // border

    CR_RED,                 // defTextTrans
    CR_RED,                 // colorNormal
    CR_GRAY,                // colorHigh
    CR_GOLD,                // colorError
    40,                     // c_numCharsPerLine
    sfx_tink,               // c_BellSound
    sfx_tink,               // c_ChatSound
    CONBACK_DEFAULT,        // consoleBack
    0,                      // blackIndex
    4,                      // whiteIndex
    NUMCARDS,               // numHUDKeys
    DOOMCardNames,          // cardNames
    mapnames,               // levelNames
    P_DoomDefaultLevelName, // GetLevelName

    &DoomStatusBar,         // StatusBar

    DOOMMARKS,              // markNumFmt

    "M_PAUSE",              // pausePatch

    3,                      // numEpisodes
    DoomExitRules,          // exitRules
    "BulletPuff",           // puffType
    "DoomTeleFog",          // teleFogType
    0,                      // teleFogHeight
    sfx_telept,             // teleSound
    100,                    // thrustFactor
    GI_GIBFULLHEALTH,       // defaultGibHealth
    "DoomMarine",           // defPClassName
    nullptr,                // defTranslate
    DoomBossSpecs,          // bossRules
    LI_TYPE_DOOM,           // levelType
    "DoomBlood",            // bloodDefaultNormal
    "DoomBlood",            // bloodDefaultImpact
    "DoomBlood",            // bloodDefaultRIP
    "DoomBlood",            // bloodDefaultCrush
    bloodTypeForActionDOOM, // default behavior for action array
    "S_GIBS",               // defCrunchFrame
    2,                      // skillAmmoMultiplier
    meleecalc_doom,         // monsterMeleeRange
    8 * FRACUNIT,           // itemHeight
    nullptr,                // autoFlightArtifact
    45,                     // lookPitchUp
    45,                     // lookPitchDown

    INTERPIC_DOOM,          // interPic
    mus_inter,              // interMusNum
    P_DoomParTime,          // GetParTime
    &giDoomFText,           // fTextPos
    &DoomIntermission,      // interfuncs
    DEF_DOOM_FINALE,        // teleEndGameFinaleType
    &DoomFinale,            // finaleData
    CAST_DEFTITLEY,         // castTitleY
    CAST_DEFNAMEY,          // castNameY

    S_music,                // s_music
    S_MusicForMapDoom,      // MusicForMap
    S_DoomMusicCheat,       // MusicCheat
    mus_None,               // musMin
    NUMMUSIC,               // numMusic
    mus_e1m1,               // randMusMin
    mus_e3m9,               // randMusMax
    "D_",                   // musPrefix
    "e1m1",                 // defMusName
    DOOMDEFSOUND,           // defSoundName
    doom_skindefs,          // skinSounds
    doom_soundnums,         // playerSounds
    nullptr,                // titleMusName
    "DSSECRET",             // secretSoundName
    sfx_itmbk,              // defSecretSound

    2,                      // switchEpisode
    &DoomSkyData,           // skyData
    R_DoomTextureHacks,     // TextureHacks
    DoomSkyFlats,           // skyFlats
    &giPsprNoScale,         // pspriteGlobalScale

    nullptr,                // defaultORs

    "ENDOOM",               // endTextName
    quitsounds,             // exitSounds
};

//
// The Ultimate DOOM (4 episodes)
//
static gamemodeinfo_t giDoomRetail = {
    retail,                 // id
    Game_DOOM,              // type
    DOOM_GIFLAGS | GIF_LOSTSOULBOUNCE | GIF_NOUPPEREPBOUND, // flags

    VNAME_DOOM_RET,         // versionName
    FNAME_DOOM_R,           // freeVerName
    BFGNAME_DOOM,           // bfgEditionName
    BANNER_DOOM_RET,        // startupBanner
    &gi_path_doomu,         // iwadPath

    demostates_udoom,       // demoStates
    170,                    // titleTics
    0,                      // advisorTics
    11*TICRATE,             // pageTics
    mus_intro,              // titleMusNum

    DOOMMENUBACK,           // menuBackground
    DOOMCREDITBK,           // creditBackground
    20,                     // creditY
    12,                     // creditTitleStep
    &giSkullCursor,         // menuCursor
    &menu_main,             // mainMenu
    &menu_savegame,         // saveMenu
    &menu_loadgame,         // loadMenu
    &menu_newgame,          // newGameMenu
    &menu_episode,          // episodeMenu
    nullptr,                // menuStartMap
    doomMenuSounds,         // menuSounds
    S_TBALL1,               // transFrame
    sfx_shotgn,             // skvAtkSound
    CR_RED,                 // unselectColor
    CR_GRAY,                // selectColor
    CR_GREEN,               // variableColor
    CR_RED,                 // titleColor
    CR_GOLD,                // itemColor
    CR_RED,                 // bigFontItemColor
    0,                      // menuOffset
    MN_DoomNewGame,         // OnNewGame

    DOOMBRDRFLAT,           // borderFlat
    &giDoomBorder,          // border

    CR_RED,                 // defTextTrans
    CR_RED,                 // colorNormal
    CR_GRAY,                // colorHigh
    CR_GOLD,                // colorError
    40,                     // c_numCharsPerLine
    sfx_tink,               // c_BellSound
    sfx_tink,               // c_ChatSound
    CONBACK_DEFAULT,        // consoleBack
    0,                      // blackIndex
    4,                      // whiteIndex
    NUMCARDS,               // numHUDKeys
    DOOMCardNames,          // cardNames
    mapnames,               // levelNames
    P_DoomDefaultLevelName, // GetLevelName

    &DoomStatusBar,         // StatusBar

    DOOMMARKS,              // markNumFmt

    "M_PAUSE",              // pausePatch

    4,                      // numEpisodes
    DoomExitRules,          // exitRules
    "BulletPuff",           // puffType
    "DoomTeleFog",          // teleFogType
    0,                      // teleFogHeight
    sfx_telept,             // teleSound
    100,                    // thrustFactor
    GI_GIBFULLHEALTH,       // defaultGibHealth
    "DoomMarine",           // defPClassName
    nullptr,                // defTranslate
    DoomBossSpecs,          // bossRules
    LI_TYPE_DOOM,           // levelType
    "DoomBlood",            // bloodDefaultNormal
    "DoomBlood",            // bloodDefaultImpact
    "DoomBlood",            // bloodDefaultRIP
    "DoomBlood",            // bloodDefaultCrush
    bloodTypeForActionDOOM, // default behavior for action array
    "S_GIBS",               // defCrunchFrame
    2,                      // skillAmmoMultiplier
    meleecalc_doom,         // monsterMeleeRange
    8 * FRACUNIT,           // itemHeight
    nullptr,                // autoFlightArtifact
    45,                     // lookPitchUp
    45,                     // lookPitchDown

    INTERPIC_DOOM,          // interPic
    mus_inter,              // interMusNum
    P_DoomParTime,          // GetParTime
    &giDoomFText,           // fTextPos
    &DoomIntermission,      // interfuncs
    DEF_DOOM_FINALE,        // teleEndGameFinaleType
    &DoomFinale,            // finaleData
    CAST_DEFTITLEY,         // castTitleY
    CAST_DEFNAMEY,          // castNameY

    S_music,                // s_music
    S_MusicForMapDoom,      // MusicForMap
    S_DoomMusicCheat,       // MusicCheat
    mus_None,               // musMin
    NUMMUSIC,               // numMusic
    mus_e1m1,               // randMusMin
    mus_e3m9,               // randMusMax
    "D_",                   // musPrefix
    "e1m1",                 // defMusName
    DOOMDEFSOUND,           // defSoundName
    doom_skindefs,          // skinSounds
    doom_soundnums,         // playerSounds
    nullptr,                // titleMusName
    "DSSECRET",             // secretSoundName
    sfx_itmbk,              // defSecretSound

    2,                      // switchEpisode
    &DoomSkyData,           // skyData
    R_DoomTextureHacks,     // TextureHacks
    DoomSkyFlats,           // skyFlats
    &giPsprNoScale,         // pspriteGlobalScale

    nullptr,                // defaultORs

    "ENDOOM",               // endTextName
    quitsounds,             // exitSounds
};

//
// DOOM II / Final DOOM
//
static gamemodeinfo_t giDoomCommercial = {
    commercial,              // id
    Game_DOOM,               // type
    DOOM_GIFLAGS | GIF_MAPXY | GIF_WOLFHACK | GIF_SETENDOFGAME, // flags

    VNAME_DOOM2,             // versionName
    FNAME_DOOM2,             // freeVerName
    BFGNAME_DOOM2,           // bfgEditionName
    BANNER_DOOM2,            // startupBanner
    &gi_path_doom2,          // iwadPath

    demostates_doom2,        // demoStates
    11*TICRATE,              // titleTics
    0,                       // advisorTics
    11*TICRATE,              // pageTics
    mus_dm2ttl,              // titleMusNum

    DOOMMENUBACK,            // menuBackground
    DM2CREDITBK,             // creditBackground
    20,                      // creditY
    12,                      // creditTitleStep
    &giSkullCursor,          // menuCursor
    &menu_main_doom2,        // mainMenu
    &menu_savegame,          // saveMenu
    &menu_loadgame,          // loadMenu
    &menu_newgame,           // newGameMenu
    &menu_episodeDoom2Stub,  // episodeMenu
    nullptr,                 // menuStartMap
    doomMenuSounds,          // menuSounds
    S_TBALL1,                // transFrame
    sfx_shotgn,              // skvAtkSound
    CR_RED,                  // unselectColor
    CR_GRAY,                 // selectColor
    CR_GREEN,                // variableColor
    CR_RED,                  // titleColor
    CR_GOLD,                 // itemColor
    CR_RED,                  // bigFontItemColor
    0,                       // menuOffset
    MN_Doom2NewGame,         // OnNewGame

    DM2BRDRFLAT,             // borderFlat
    &giDoomBorder,           // border

    CR_RED,                  // defTextTrans
    CR_RED,                  // colorNormal
    CR_GRAY,                 // colorHigh
    CR_GOLD,                 // colorError
    40,                      // c_numCharsPerLine
    sfx_tink,                // c_BellSound
    sfx_radio,               // c_ChatSound
    CONBACK_DEFAULT,         // consoleBack
    0,                       // blackIndex
    4,                       // whiteIndex
    NUMCARDS,                // numHUDKeys
    DOOMCardNames,           // cardNames
    mapnames2,               // levelNames
    P_Doom2DefaultLevelName, // GetLevelName

    &DoomStatusBar,          // StatusBar

    DOOMMARKS,               // markNumFmt

    "M_PAUSE",               // pausePatch

    1,                       // numEpisodes
    Doom2ExitRules,          // exitRules
    "BulletPuff",            // puffType
    "DoomTeleFog",           // teleFogType
    0,                       // teleFogHeight
    sfx_telept,              // teleSound
    100,                     // thrustFactor
    GI_GIBFULLHEALTH,        // defaultGibHealth
    "DoomMarine",            // defPClassName
    nullptr,                 // defTranslate
    Doom2BossSpecs,          // bossRules
    LI_TYPE_DOOM,            // levelType
    "DoomBlood",             // bloodDefaultNormal
    "DoomBlood",             // bloodDefaultImpact
    "DoomBlood",             // bloodDefaultRIP
    "DoomBlood",             // bloodDefaultCrush
    bloodTypeForActionDOOM,  // default behavior for action array
    "S_GIBS",                // defCrunchFrame
    2,                       // skillAmmoMultiplier
    meleecalc_doom,          // monsterMeleeRange
    8 * FRACUNIT,            // itemHeight
    nullptr,                 // autoFlightArtifact
    45,                      // lookPitchUp
    45,                      // lookPitchDown

    INTERPIC_DOOM,           // interPic
    mus_dm2int,              // interMusNum
    P_Doom2ParTime,          // GetParTime
    &giDoomFText,            // fTextPos
    &DoomIntermission,       // interfuncs
    FINALE_TEXT,             // teleEndGameFinaleType
    &Doom2Finale,            // finaleData
    CAST_DEFTITLEY,          // castTitleY
    CAST_DEFNAMEY,           // castNameY

    S_music,                 // s_music
    S_MusicForMapDoom2,      // MusicForMap
    S_Doom2MusicCheat,       // MusicCheat
    mus_None,                // musMin
    NUMMUSIC,                // numMusic
    mus_runnin,              // randMusMin
    mus_ultima,              // randMusMax
    "D_",                    // musPrefix
    "runnin",                // defMusName
    DOOMDEFSOUND,            // defSoundName
    doom_skindefs,           // skinSounds
    doom_soundnums,          // playerSounds
    nullptr,                 // titleMusName
    "DSSECRET",              // secretSoundName
    sfx_itmbk,               // defSecretSound

    3,                       // switchEpisode
    &Doom2SkyData,           // skyData
    R_Doom2TextureHacks,     // TextureHacks
    DoomSkyFlats,            // skyFlats
    &giPsprNoScale,          // pspriteGlobalScale

    nullptr,                 // defaultORs

    "ENDOOM",                // endTextName
    quitsounds2,             // exitSounds
};

//
// Heretic Shareware
//
static gamemodeinfo_t giHereticSW = {
    hereticsw,              // id
    Game_Heretic,           // type
    GIF_SHAREWARE | HERETIC_GIFLAGS, // flags

    VNAME_HTIC_SW,          // versionName
    nullptr,                // freeVerName
    nullptr,                // bfgEditionName
    BANNER_HTIC_SW,         // startupBanner
    &gi_path_hticsw,        // iwadPath

    demostates_hsw,         // demoStates
    210,                    // titleTics
    140,                    // advisorTics
    200,                    // pageTics
    hmus_titl,              // titleMusNum

    HTICMENUBACK,           // menuBackground
    HTICCREDITBK,           // creditBackground
    8,                      // creditY
    8,                      // creditTitleStep
    &giArrowCursor,         // menuCursor
    &menu_hmain,            // mainMenu
    &menu_savegame,         // saveMenu
    &menu_loadgame,         // loadMenu
    &menu_hnewgame,         // newGameMenu
    &menu_hepisode,         // episodeMenu
    nullptr,                // menuStartMap
    hticMenuSounds,         // menuSounds
    S_MUMMYFX1_1,           // transFrame
    sfx_gldhit,             // skvAtkSound
    CR_GRAY,                // unselectColor
    CR_RED,                 // selectColor
    CR_GREEN,               // variableColor
    CR_GREEN,               // titleColor
    CR_GOLD,                // itemColor
    CR_GREEN,               // bigFontItemColor
    4,                      // menuOffset
    MN_HticNewGame,         // OnNewGame

    HSWBRDRFLAT,            // borderFlat
    &giHticBorder,          // border

    CR_GRAY,                // defTextTrans
    CR_GRAY,                // colorNormal
    CR_GOLD,                // colorHigh
    CR_RED,                 // colorError
    40,                     // c_numCharsPerLine
    sfx_chat,               // c_BellSound
    sfx_chat,               // c_ChatSound
    CONBACK_HERETIC,        // consoleBack
    0,                      // blackIndex
    35,                     // whiteIndex
    3,                      // numHUDKeys
    HticCardNames,          // cardNames
    mapnamesh,              // levelNames
    P_HticDefaultLevelName, // GetLevelName

    &HticStatusBar,         // StatusBar

    HTICMARKS,              // markNumFmt

    "PAUSED",               // pausePatch

    1,                      // numEpisodes
    HereticExitRules,       // exitRules
    "HereticStaffPuff",     // puffType
    "HereticTeleFog",       // teleFogType
    32*FRACUNIT,            // teleFogHeight
    sfx_htelept,            // teleSound
    150,                    // thrustFactor
    GI_GIBHALFHEALTH,       // defaultGibHealth
    "Corvus",               // defPClassName
    DEFTL_HERETIC,          // defTranslate
    HereticBossSpecs,       // bossRules
    LI_TYPE_HERETIC,        // levelType
    "HereticBloodSplatter", // bloodDefaultNormal
    "HereticBloodSplatter", // bloodDefaultImpact
    "HereticBlood",         // bloodDefaultRIP
    "HereticBlood",         // bloodDefaultCrush
    bloodTypeForActionHtic, // default blood behavior for action array
    "",                     // defCrunchFrame
    1.5,                    // skillAmmoMultiplier
    meleecalc_raven,        // monsterMeleeRange
    32 * FRACUNIT,          // itemHeight
    "ArtiFly",              // autoFlightArtifact
    32,                     // lookPitchUp
    32,                     // lookPitchDown

    INTERPIC_DOOM,          // interPic
    hmus_intr,              // interMusNum
    P_NoParTime,            // GetParTime
    &giHticFText,           // fTextPos
    &HticIntermission,      // interfuncs
    DEF_HTIC_FINALE,        // teleEndGameFinaleType
    &HereticFinale,         // finaleData
    CAST_DEFTITLEY,         // castTitleY
    CAST_DEFNAMEY,          // castNameY

    H_music,                // s_music
    S_MusicForMapHtic,      // MusicForMap
    S_HereticMusicCheat,    // MusicCheat
    hmus_None,              // musMin
    NUMHTICMUSIC,           // numMusic
    hmus_e1m1,              // randMusMin
    hmus_e1m9,              // randMusMax
    "MUS_",                 // musPrefix
    "e1m1",                 // defMusName
    HTICDEFSOUND,           // defSoundName
    htic_skindefs,          // skinSounds
    htic_soundnums,         // playerSounds
    nullptr,                // titleMusName
    "DSSECRET",             // secretSoundName
    sfx_chat,               // defSecretSound

    1,                      // switchEpisode
    &HereticSkyData,        // skyData
    R_HticTextureHacks,     // TextureHacks
    DoomSkyFlats,           // skyFlats
    &giPsprNoScale,         // pspriteGlobalScale

    HereticDefaultORs,      // defaultORs

    "ENDTEXT",              // endTextName
    nullptr,                // exitSounds
};

//
// Heretic Registered / Heretic: Shadow of the Serpent Riders
//
// The only difference between registered and SoSR is the
// number of episodes, which is patched in this structure at
// runtime.
//
static gamemodeinfo_t giHereticReg = {
    hereticreg,             // id
    Game_Heretic,           // type
    HERETIC_GIFLAGS,        // flags

    VNAME_HTIC_REG,         // versionName
    nullptr,                // freeVerName
    nullptr,                // bfgEditionName
    BANNER_HTIC_REG,        // startupBanner
    &gi_path_hticreg,       // iwadPath

    demostates_hreg,        // demoStates
    210,                    // titleTics
    140,                    // advisorTics
    200,                    // pageTics
    hmus_titl,              // titleMusNum

    HTICMENUBACK,           // menuBackground
    HTICCREDITBK,           // creditBackground
    8,                      // creditY
    8,                      // creditTitleStep
    &giArrowCursor,         // menuCursor
    &menu_hmain,            // mainMenu
    &menu_savegame,         // saveMenu
    &menu_loadgame,         // loadMenu
    &menu_hnewgame,         // newGameMenu
    &menu_hepisode,         // episodeMenu
    nullptr,                // menuStartMap
    hticMenuSounds,         // menuSounds
    S_MUMMYFX1_1,           // transFrame
    sfx_gldhit,             // skvAtkSound
    CR_GRAY,                // unselectColor
    CR_RED,                 // selectColor
    CR_GREEN,               // variableColor
    CR_GREEN,               // titleColor
    CR_GOLD,                // itemColor
    CR_GREEN,               // bigFontItemColor
    4,                      // menuOffset
    MN_HticNewGame,         // OnNewGame

    HREGBRDRFLAT,           // borderFlat
    &giHticBorder,          // border

    CR_GRAY,                // defTextTrans
    CR_GRAY,                // colorNormal
    CR_GOLD,                // colorHigh
    CR_RED,                 // colorError
    40,                     // c_numCharsPerLine
    sfx_chat,               // c_BellSound
    sfx_chat,               // c_ChatSound
    CONBACK_HERETIC,        // consoleBack
    0,                      // blackIndex
    35,                     // whiteIndex
    3,                      // numHUDKeys
    HticCardNames,          // cardNames
    mapnamesh,              // levelNames
    P_HticDefaultLevelName, // GetLevelName

    &HticStatusBar,         // StatusBar

    HTICMARKS,              // markNumFmt

    "PAUSED",               // pausePatch

    4,                      // numEpisodes -- note 6 for SoSR gamemission
    HereticExitRules,       // exitRules
    "HereticStaffPuff",     // puffType
    "HereticTeleFog",       // teleFogType
    32*FRACUNIT,            // teleFogHeight
    sfx_htelept,            // teleSound
    150,                    // thrustFactor
    GI_GIBHALFHEALTH,       // defaultGibHealth
    "Corvus",               // defPClassName
    DEFTL_HERETIC,          // defTranslate
    HereticBossSpecs,       // bossRules
    LI_TYPE_HERETIC,        // levelType
    "HereticBloodSplatter", // bloodDefaultNormal
    "HereticBloodSplatter", // bloodDefaultImpact
    "HereticBlood",         // bloodDefaultRIP
    "HereticBlood",         // bloodDefaultCrush
    bloodTypeForActionHtic, // default blood behavior for action array
    "",                     // defCrunchFrame
    1.5,                    // skillAmmoMultiplier
    meleecalc_raven,        // monsterMeleeRange
    32 * FRACUNIT,          // itemHeight
    "ArtiFly",              // autoFlightArtifact
    32,                     // lookPitchUp
    32,                     // lookPitchDown

    INTERPIC_DOOM,          // interPic
    hmus_intr,              // interMusNum
    P_NoParTime,            // GetParTime
    &giHticFText,           // fTextPos
    &HticIntermission,      // interfuncs
    DEF_HTIC_FINALE,        // teleEndGameFinaleType
    &HereticFinale,         // finaleData
    CAST_DEFTITLEY,         // castTitleY
    CAST_DEFNAMEY,          // castNameY

    H_music,                // s_music
    S_MusicForMapHtic,      // MusicForMap
    S_HereticMusicCheat,    // MusicCheat
    hmus_None,              // musMin
    NUMHTICMUSIC,           // numMusic
    hmus_e1m1,              // randMusMin
    hmus_e3m3,              // randMusMax
    "MUS_",                 // musPrefix
    "e1m1",                 // defMusName
    HTICDEFSOUND,           // defSoundName
    htic_skindefs,          // skinSounds
    htic_soundnums,         // playerSounds
    nullptr,                // titleMusName
    "DSSECRET",             // secretSoundName
    sfx_chat,               // defSecretSound

    2,                      // switchEpisode
    &HereticSkyData,        // skyData
    R_HticTextureHacks,     // TextureHacks
    DoomSkyFlats,           // skyFlats
    &giPsprNoScale,         // pspriteGlobalScale

    HereticDefaultORs,      // defaultORs

    "ENDTEXT",              // endTextName
    nullptr,                // exitSounds
};

// clang-format on

// Game Mode Info Array
gamemodeinfo_t *GameModeInfoObjects[NumGameModes] = {
    &giDoomSW,         // shareware
    &giDoomReg,        // registered
    &giDoomCommercial, // commercial
    &giDoomRetail,     // retail
    &giHereticSW,      // hereticsw
    &giHereticReg,     // hereticreg
    &giDoomReg,        // indetermined - act like Doom, mostly.
};

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

    // apply GameModeInfo flag additions and removals
    gi->flags |= mi->addGMIFlags;
    gi->flags &= ~mi->remGMIFlags;

    // clang-format off

    // apply overrides
    OVERRIDE(versionName,        nullptr);
    OVERRIDE(startupBanner,      nullptr);
    OVERRIDE(numEpisodes,              0);
    OVERRIDE(iwadPath,           nullptr);
    OVERRIDE(finaleData,         nullptr);
    OVERRIDE(menuBackground,     nullptr);
    OVERRIDE(creditBackground,   nullptr);
    OVERRIDE(interPic,           nullptr);
    OVERRIDE(exitRules,          nullptr);
    OVERRIDE(levelNames,         nullptr);
    OVERRIDE(randMusMax,               0);
    OVERRIDE(skyData,            nullptr);
    OVERRIDE(skyFlats,           nullptr);
    OVERRIDE(pspriteGlobalScale, nullptr);

    // clang-format on

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
// 07/15/2012: Added runtime adjustment of blackIndex and whiteIndex.
//
void D_InitGMIPostWads()
{
    AutoPalette     pal(wGlobalDir);
    gamemodeinfo_t *gi = GameModeInfo;
    missioninfo_t  *mi = gi->missionInfo;

    // apply the demoStates override from the missioninfo conditionally:
    // * if MI_DEMOIFDEMO4 is not set, then always override.
    // * if MI_DEMOIFDEMO4 IS set, then only if DEMO4 actually exists.
    if(!(mi->flags & MI_DEMOIFDEMO4) || W_CheckNumForName("DEMO4") >= 0)
    {
        OVERRIDE(demoStates, nullptr);
    }

    GameModeInfo->blackIndex = V_FindBestColor(pal.get(), 0, 0, 0);
    GameModeInfo->whiteIndex = V_FindBestColor(pal.get(), 255, 255, 255);
}

// EOF
