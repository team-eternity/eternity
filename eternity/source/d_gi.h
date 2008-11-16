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

#ifndef __D_GI_H__
#define __D_GI_H__

#include "mn_engin.h"
#include "mn_menus.h"
#include "mn_htic.h"
#include "sounds.h"
#include "v_video.h"
#include "st_stuff.h"
#include "in_lude.h"

// inspired by, but not taken from, zdoom

// Menu cursor -- a 2 patch alternating graphic
typedef struct gimenucursor_s
{
   char *patch1;
   char *patch2;
} gimenucursor_t;

// Screen border used to fill backscreen for small screen sizes
typedef struct giborder_s
{
   int offset;
   int size;
   char *c_tl;
   char *top;
   char *c_tr;
   char *left;
   char *right;
   char *c_bl;
   char *bottom;
   char *c_br;
} giborder_t;

typedef struct gitextmetric_s
{
   int x, y;   // initial coordinates (for finale)
   int cy;     // step amount for \n
   int space;  // blank character step
   int dw;     // amount to subtract from character width
   int absh;   // absolute maximum height of any character
} gitextmetric_t;

//
// enum for menu sounds
//
enum
{
   MN_SND_ACTIVATE,
   MN_SND_DEACTIVATE,
   MN_SND_KEYUPDOWN,
   MN_SND_COMMAND,
   MN_SND_PREVIOUS,
   MN_SND_KEYLEFTRIGHT,
   MN_SND_NUMSOUNDS,
};

// Game Mode Flags

#define GIF_HASDISK      0x00000001 // has flashing io disk
#define GIF_SHAREWARE    0x00000002 // shareware game (no -file)
#define GIF_MNBIGFONT    0x00000004 // uses big font for menu titles
#define GIF_MAPXY        0x00000008 // gamemode uses MAPxy maps by default
#define GIF_SAVESOUND    0x00000010 // makes a sound in save & load menus
#define GIF_HASADVISORY  0x00000020 // displays advisory popup on title screen
#define GIF_SHADOWTITLES 0x00000040 // shadows titles in menus
#define GIF_HASMADMELEE  0x00000080 // has mad melee when player dies in SP

// Game mode handling - identify IWAD version
//  to handle IWAD dependent animations etc.
typedef enum 
{
  shareware,    // DOOM 1 shareware, E1, M9
  registered,   // DOOM 1 registered, E3, M27
  commercial,   // DOOM 2 retail, E1, M34
  retail,       // DOOM 1 retail, E4, M36
  hereticsw,    // Heretic shareware
  hereticreg,   // Heretic full
  indetermined, // Incomplete or corrupted IWAD
  NumGameModes
} GameMode_t;

// Mission packs
typedef enum
{
  doom,         // DOOM 1
  doom2,        // DOOM 2
  pack_tnt,     // TNT mission pack
  pack_plut,    // Plutonia pack
  heretic,      // Heretic
  hticsosr,     // Heretic - Shadow of the Serpent Riders
  none,
  NumGameMissions
} GameMission_t;

//
// Game Mode Types
//
// Some things have to vary only on the game-type being played, be it
// Doom or Heretic (or eventually, Hexen or Strife). These are directly
// tied to the state of the Enable system in EDF.
//
enum
{
   Game_DOOM,
   Game_Heretic,
   NumGameModeTypes
};

//
// missioninfo_t
//
// This structure holds mission-dependent information for the
// gamemodeinfo_t structure. The gamemode info structure must be
// assigned a missioninfo_t object during startup in d_main.c:IdentifyVersion.
//
typedef struct missioninfo_s
{
   GameMission_t id;            // mission id - replaces "gamemission" variable
   
   // override data - information here overrides that contained in the
   // gamemodeinfo_t that uses this missioninfo object.

   const char *versionNameOR;   // if not NULL, overrides name of the gamemode
   const char *startupBannerOR; // if not NULL, overrides the startup banner 
   int         numEpisodesOR;   // if not 0, overrides the number of episodes
} missioninfo_t;

//
// gamemodeinfo_t
//
// This structure holds just about all the gamemode-pertinent
// information that could easily be pulled out of the rest of
// the source. This approach, as mentioned above, was inspired
// by zdoom, but I've taken it and really run with it.
//
typedef struct gamemodeinfo_s
{
   GameMode_t id;             // id      - replaces "gamemode" variable
   int type;                  // main game mode type: doom, heretic, etc.
   int flags;                 // game mode flags
   
   // startup stuff
   const char *versionName;   // descriptive version name
   const char *startupBanner; // startup banner text
   
   // demo state information
   int titleTics;             // length of time to show title
   int advisorTics;           // for Heretic, len. to show advisory
   int pageTics;              // length of general demo state pages
   int titleMusNum;           // music number to use for title

   // menu stuff
   const char *menuBackground;   // name of menu background flat
   const char *creditBackground; // name of dynamic credit bg flat
   int   creditY;             // y coord for credit text
   int   creditTitleStep;     // step-down from credit title
   gimenucursor_t *menuCursor;   // pointer to the big menu cursor
   menu_t *mainMenu;          // pointer to main menu structure
   menu_t *saveMenu;          // pointer to save menu structure
   menu_t *loadMenu;          // pointer to load menu structure
   menu_t *newGameMenu;       // pointer to new game menu structure
   int *menuSounds;           // menu sound indices
   int transFrame;            // frame DEH # used on video menu
   int skvAtkSound;           // skin viewer attack sound
   int unselectColor;         // color of unselected menu item text
   int selectColor;           // color of selected menu item text
   int variableColor;         // color of variable text

   // border stuff
   const char *borderFlat;    // name of flat to fill backscreen
   giborder_t *border;        // pointer to a game border

   // HU / Video / Console stuff
   char **defTextTrans;       // default text color rng for msgs
   int colorNormal;           // color # for normal messages
   int colorHigh;             // color # for highlighted messages
   int colorError;            // color # for error messages
   int c_numCharsPerLine;     // number of chars per line in console
   int c_BellSound;           // sound used for \a in console
   int c_ChatSound;           // sound used by say command
   const char *consoleBack;   // lump to use for default console backdrop
   gitextmetric_t *vtextinfo; // v_font text info
   gitextmetric_t *btextinfo; // big font text info
   unsigned char blackIndex;  // palette index for black {0,0,0}
   unsigned char whiteIndex;  // palette index for white {255,255,255}
   int numHUDKeys;            // number of keys to show in HUD

   // Status bar
   stbarfns_t *StatusBar;     // status bar function set

   // Automap
   const char *markNumFmt;    // automap mark number format string

   // Miscellaneous graphics
   const char *pausePatch;    // name of patch to show when paused

   // Game interaction stuff
   int numEpisodes;           // number of game episodes
   int teleFogType;           // DeHackEd number of telefog object
   fixed_t teleFogHeight;     // amount to add to telefog z coord
   int teleSound;             // sound id for teleportation
   short thrustFactor;        // damage thrust factor
   const char *defPClassName; // default playerclass name

   // Intermission and Finale stuff
   int interMusNum;           // intermission music number
   gitextmetric_t *ftextinfo; // finale text info
   interfns_t *interfuncs;    // intermission function pointers

   // Sound
   musicinfo_t *s_music;      // pointer to musicinfo_t (sounds.h)
   int musMin;                // smallest music index value (0)
   int numMusic;              // maximum music index value
   const char *musPrefix;     // "D_" for DOOM, "MUS_" for Heretic
   const char *defMusName;    // default music name
   const char *defSoundName;  // default sound if one is missing
   const char **skinSounds;   // default skin sound mnemonics array
   int *playerSounds;         // player sound dehnum indirection

   // Renderer stuff
   int switchEpisode;         // "episode" number for switch texture defs

   // Miscellaneous stuff
   const char *endTextName;   // name of end text resource

   // Internal fields - these are set at runtime, so keep them last.
   missioninfo_t *missionInfo; // gamemission-dependent info

} gamemodeinfo_t;

extern missioninfo_t  *MissionInfoObjects[NumGameMissions];
extern gamemodeinfo_t *GameModeInfoObjects[NumGameModes];

extern gamemodeinfo_t *GameModeInfo;

void D_SetGameModeInfo(GameMode_t, GameMission_t);
void D_InitGameInfo(void);

#endif

// EOF
