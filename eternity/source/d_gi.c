// Emacs style mode select -*- C++ -*-
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

#include "doomstat.h"
#include "doomdef.h"
#include "sounds.h"
#include "hi_stuff.h"
#include "wi_stuff.h"
#include "d_gi.h"
#include "p_info.h"
#include "info.h"
#include "e_things.h"

// definitions

// resource wad format strings
#define DOOMRESWAD   "%s/%s.wad"
#define HTICRESWAD   "%s/%.4shtic.wad"

// automap number mark format strings
#define DOOMMARKS    "AMMNUM%d"
#define HTICMARKS    "SMALLIN%d"

// menu background flats
#define DOOMMENUBACK "FLOOR4_6"
#define HTICMENUBACK "FLOOR04"

// credit background flats
#define DOOMCREDITBK "NUKAGE1"
#define DM2CREDITBK  "SLIME05"
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

// globals

// holds the address of the gameinfo_t for the current gamemode,
// determined at startup
gameinfo_t *gameModeInfo;

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
// V-Font metrics
//

static gitextmetric_t giDoomVText =
{
   0, 0, // x,y (not used)
   8,    // cy
   4,    // space
   0,    // dw
   8,    // absh
};

static gitextmetric_t giHticVText =
{
   0, 0, // x,y (not used)
   9,    // cy
   5,    // space
   1,    // dw
   10,   // absh
};

//
// Finale font metrics
//

static gitextmetric_t giDoomFText =
{
   10, 10, // x,y
   11,     // cy
   4,      // space
   0,      // dw
};

static gitextmetric_t giHticFText =
{
   20, 5, // x,y
   9,     // cy
   5,     // space
   0,     // dw
};

//
// Big Font metrics
//

static gitextmetric_t giDoomBigText =
{
   0, 0, // x, y, (not used)
   12,   // cy
   8,    // space
   1,    // dw
   12,   // absh
};

static gitextmetric_t giHticBigText = 
{
   0, 0, // x, y (not used)
   20,   // cy
   8,    // space
   1,    // dw
   20,   // absh -- FIXME: may not be correct
};

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
   sfx_hdorcls, // previous menu
   sfx_keyup,   // key left/right
};

extern menu_t menu_savegame;
extern menu_t menu_hsavegame;
extern menu_t menu_loadgame;
extern menu_t menu_hloadgame;
extern menu_t menu_newgame;
extern menu_t menu_hnewgame;

//
// Game Mode Information Structures
//

/*
//
// DOOM Shareware
//
gameinfo_t giDoomSW =
{
   Game_DOOM,        // type
   GIF_SHAREWARE,    // flags

   "doom",           // gameDir
   DOOMRESWAD,       // resourceFmt

   170,              // titleTics
   0,                // advisorTics
   false,            // hasAdvisory
   11*TICRATE,       // pageTics
   mus_intro,        // titleMusNum

   DOOMMENUBACK,     // menuBackground
   DOOMCREDITBK,     // creditBackground
   17,               // creditY
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
   false,            // shadowTitles

   DOOMBRDRFLAT,     // borderFlat
   &giDoomBorder,    // border
   
   &cr_red,          // defTextTrans
   CR_RED,           // colorNormal
   CR_GRAY,          // colorHigh
   CR_GOLD,          // colorError
   40,               // c_numCharsPerLine
   sfx_tink,         // c_BellSound
   sfx_tink,         // c_ChatSound
   &giDoomVText,     // vtextinfo
   &giDoomBigText,   // btextinfo
   0,                // blackIndex
   4,                // whiteIndex
   NUMCARDS,         // numHUDKeys

   &DoomStatusBar,   // StatusBar

   DOOMMARKS,        // markNumFmt

   "M_PAUSE",        // pausePatch
   
   1,                // numEpisodes
   MT_TFOG,          // teleFogType
   0,                // teleFogHeight
   sfx_telept,       // teleSound
   100,              // thrustFactor
   false,            // hasMadMelee

   mus_inter,        // interMusNum
   &giDoomFText,     // ftextinfo
   &DoomIntermission,// interfuncs

   S_music,          // s_music
   mus_None,         // musMin
   NUMMUSIC,         // numMusic
   "d_",             // musPrefix
   DOOMDEFSOUND,     // defSoundName

   "ENDOOM",         // endTextName
};
*/

//
// DOOM Registered
//
// Note: since all the data is currently the same, I've eliminated the separate
// structures for DOOM SW and Ultimate Doom, and I patch the episode number and
// gamemode flags in d_main.c at startup, as I do for Heretic:SoSR. I'm keeping
// the commented-out structures though in case more extensive differences show
// up later as more things are moved into this module.
//
gameinfo_t giDoomReg =
{
   Game_DOOM,        // type
   GIF_HASDISK,      // flags -- note: patched for shareware DOOM

   "doom",           // gameDir
   DOOMRESWAD,       // resourceFmt
   
   170,              // titleTics
   0,                // advisorTics
   false,            // hasAdvisory
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
   false,            // shadowTitles   

   DOOMBRDRFLAT,     // borderFlat
   &giDoomBorder,    // border

   &cr_red,          // defTextTrans
   CR_RED,           // colorNormal
   CR_GRAY,          // colorHigh
   CR_GOLD,          // colorError
   40,               // c_numCharsPerLine
   sfx_tink,         // c_BellSound
   sfx_tink,         // c_ChatSound
   &giDoomVText,     // vtextinfo
   &giDoomBigText,   // btextinfo
   0,                // blackIndex
   4,                // whiteIndex
   NUMCARDS,         // numHUDKeys

   &DoomStatusBar,   // StatusBar

   DOOMMARKS,        // markNumFmt

   "M_PAUSE",        // pausePatch

   3,                // numEpisodes -- note 1 for SW DOOM, 4 for Ultimate
   MT_TFOG,          // teleFogType
   0,                // teleFogHeight
   sfx_telept,       // teleSound
   100,              // thrustFactor
   false,            // hasMadMelee

   mus_inter,        // interMusNum
   &giDoomFText,     // ftextinfo
   &DoomIntermission,// interfuncs

   S_music,          // s_music
   mus_None,         // musMin
   NUMMUSIC,         // numMusic
   "d_",             // musPrefix
   DOOMDEFSOUND,     // defSoundName

   "ENDOOM",         // endTextName
};

/*
//
// The Ultimate DOOM (4 episodes)
//
gameinfo_t giDoomRetail =
{
   Game_DOOM,        // type
   0,                // flags

   "doom",           // gameDir
   DOOMRESWAD,       // resourceFmt

   170,              // titleTics
   0,                // advisorTics
   false,            // hasAdvisory
   11*TICRATE,       // pageTics
   mus_intro,        // titleMusNum

   DOOMMENUBACK,     // menuBackground
   DOOMCREDITBK,     // creditBackground
   17,               // creditY
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
   false,            // shadowTitles   

   DOOMBRDRFLAT,     // borderFlat
   &giDoomBorder,    // border

   &cr_red,          // defTextTrans
   CR_RED,           // colorNormal
   CR_GRAY,          // colorHigh
   CR_GOLD,          // colorError
   40,               // c_numCharsPerLine
   sfx_tink,         // c_BellSound
   sfx_tink,         // c_ChatSound
   &giDoomVText,     // vtextinfo
   &giDoomBigText,   // btextinfo
   0,                // blackIndex
   4,                // whiteIndex
   NUMCARDS,         // numHUDKeys

   &DoomStatusBar,   // StatusBar

   DOOMMARKS,        // markNumFmt

   "M_PAUSE",        // pausePatch

   4,                // numEpisodes
   MT_TFOG,          // teleFogType
   0,                // teleFogHeight
   sfx_telept,       // teleSound
   100,              // thrustFactor
   false,            // hasMadMelee

   mus_inter,        // interMusNum
   &giDoomFText,     // ftextinfo
   &DoomIntermission,// interfuncs

   S_music,          // s_music
   mus_None,         // musMin
   NUMMUSIC,         // numMusic
   "d_",             // musPrefix
   DOOMDEFSOUND,     // defSoundName

   "ENDOOM",         // endTextName
};
*/

//
// DOOM II / Final DOOM
//
gameinfo_t giDoomCommercial =
{
   Game_DOOM,        // type
   GIF_HASDISK,      // flags

   "doom",           // gameDir
   DOOMRESWAD,       // resourceFmt

   11*TICRATE,       // titleTics
   0,                // advisorTics
   false,            // hasAdvisory
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
   false,            // shadowTitles   

   DM2BRDRFLAT,      // borderFlat
   &giDoomBorder,    // border

   &cr_red,          // defTextTrans
   CR_RED,           // colorNormal
   CR_GRAY,          // colorHigh
   CR_GOLD,          // colorError
   40,               // c_numCharsPerLine
   sfx_tink,         // c_BellSound
   sfx_radio,        // c_ChatSound
   &giDoomVText,     // vtextinfo
   &giDoomBigText,   // btextinfo
   0,                // blackIndex
   4,                // whiteIndex
   NUMCARDS,         // numHUDKeys

   &DoomStatusBar,   // StatusBar

   DOOMMARKS,        // markNumFmt

   "M_PAUSE",        // pausePatch

   1,                // numEpisodes
   MT_TFOG,          // teleFogType
   0,                // teleFogHeight
   sfx_telept,       // teleSound
   100,              // thrustFactor
   false,            // hasMadMelee

   mus_dm2int,       // interMusNum
   &giDoomFText,     // ftextinfo
   &DoomIntermission,// interfuncs

   S_music,          // s_music
   mus_None,         // musMin
   NUMMUSIC,         // numMusic
   "d_",             // musPrefix
   DOOMDEFSOUND,     // defSoundName

   "ENDOOM",         // endTextName
};

//
// Heretic Shareware
//
gameinfo_t giHereticSW =
{
   Game_Heretic,     // type
   GIF_SHAREWARE | GIF_MNBIGFONT, // flags

   "heretic",        // gameDir
   HTICRESWAD,       // resourceFmt

   210,              // titleTics
   140,              // advisorTics
   true,             // hasAdvisory
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
   true,             // shadowTitles   

   HSWBRDRFLAT,      // borderFlat
   &giHticBorder,    // border

   &cr_gray,         // defTextTrans
   CR_GRAY,          // colorNormal
   CR_GOLD,          // colorHigh
   CR_RED,           // colorError
   43,               // c_numCharsPerLine
   sfx_chat,         // c_BellSound
   sfx_chat,         // c_ChatSound
   &giHticVText,     // vtextinfo
   &giHticBigText,   // btextinfo
   0,                // blackIndex
   35,               // whiteIndex
   3,                // numHUDKeys

   &HticStatusBar,   // StatusBar

   HTICMARKS,        // markNumFmt

   "PAUSED",         // pausePatch

   1,                // numEpisodes
   MT_HTFOG,         // teleFogType
   32*FRACUNIT,      // teleFogHeight
   sfx_htelept,      // teleSound
   150,              // thrustFactor
   true,             // hasMadMelee

   hmus_intr,        // interMusNum
   &giHticFText,     // ftextinfo
   &HticIntermission,// interfuncs

   H_music,          // s_music
   hmus_None,        // musMin
   NUMHTICMUSIC,     // numMusic
   "mus_",           // musPrefix
   HTICDEFSOUND,     // defSoundName

   "ENDTEXT",        // endTextName
};

//
// Heretic Registered / Heretic: Shadow of the Serpent Riders
//
// The only difference between registered and SoSR is the
// number of episodes, which is patched in this structure at
// runtime.
//
gameinfo_t giHereticReg =
{
   Game_Heretic,     // type   
   GIF_MNBIGFONT,    // flags

   "heretic",        // gameDir
   HTICRESWAD,       // resourceFmt

   210,              // titleTics
   140,              // advisorTics
   true,             // hasAdvisory
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
   true,             // shadowTitles   

   HREGBRDRFLAT,     // borderFlat
   &giHticBorder,    // border

   &cr_gray,         // defTextTrans
   CR_GRAY,          // colorNormal
   CR_GOLD,          // colorHigh
   CR_RED,           // colorError
   43,               // c_numCharsPerLine
   sfx_chat,         // c_BellSound
   sfx_chat,         // c_ChatSound
   &giHticVText,     // vtextinfo
   &giHticBigText,   // btextinfo
   0,                // blackIndex
   35,               // whiteIndex
   3,                // numHUDKeys

   &HticStatusBar,   // StatusBar

   HTICMARKS,        // markNumFmt

   "PAUSED",         // pausePatch

   4,                // numEpisodes -- note 6 for SoSR gamemission
   MT_HTFOG,         // teleFogType
   32*FRACUNIT,      // teleFogHeight
   sfx_htelept,      // teleSound
   150,              // thrustFactor
   true,             // hasMadMelee

   hmus_intr,        // interMusNum
   &giHticFText,     // ftextinfo
   &HticIntermission,// interfuncs

   H_music,          // s_music
   hmus_None,        // musMin
   NUMHTICMUSIC,     // numMusic
   "mus_",           // musPrefix
   HTICDEFSOUND,     // defSoundName

   "ENDTEXT",        // endTextName
};

//
// D_InitGameInfo
//
// Called after gameModeInfo is set to normalize some fields.
// EDF makes this necessary.
//
void D_InitGameInfo(void)
{
#ifdef RANGECHECK
   if(!gameModeInfo)
      I_Error("D_InitGameInfo: called before gameModeInfo set\n");
#endif

   gameModeInfo->teleFogType = E_SafeThingType(gameModeInfo->teleFogType);
}

// EOF
