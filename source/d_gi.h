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
// Purpose: Gamemode information
//  Stores all gamemode-dependent information in one location and 
//  lends more structure to other modules.
//
// Authors: James Haley
//

#ifndef D_GI_H__
#define D_GI_H__

#include "m_fixed.h"

struct default_or_t;
struct interfns_t;
struct levelnamedata_t;
struct menu_t;
struct musicinfo_t;
struct skyflat_t;
struct stbarfns_t;
struct texture_t;
enum   bloodtype_e   : int;

// inspired by, but not taken from, zdoom

// Menu cursor
struct gimenucursor_t
{
   int blinktime;
   int numpatches;
   const char **patches;
};

// Screen border used to fill backscreen for small screen sizes
struct giborder_t
{
   int offset;
   int size;
   const char *c_tl;
   const char *top;
   const char *c_tr;
   const char *left;
   const char *right;
   const char *c_bl;
   const char *bottom;
   const char *c_br;
};

struct giftextpos_t
{
   int x, y;   // initial coordinates (for finale)
};

// scaling data
struct giscale_t
{
   float x;
   float y;
};

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

// haleyjd 07/02/09: moved demostate_t to header for GameModeInfo

typedef void (*dsfunc_t)(const char *);

struct demostate_t
{
   dsfunc_t func;
   const char *name;
};

extern const demostate_t demostates_doom[];
extern const demostate_t demostates_doom2[];
extern const demostate_t demostates_udoom[];
extern const demostate_t demostates_hsw[];
extern const demostate_t demostates_hreg[];

//
// Exit Rule Sets
//

struct exitrule_t
{
   int gameepisode; // current episode (1 for games like DOOM 2, 
                    //   -1 if doesn't matter, -2 to terminate array)
   int gamemap;     // current map the game is on (1-based, -1 if doesn't matter)   
   int destmap;     // destination map (0-based for wminfo)
   bool isSecret;   // this rule applies for secret exits   
};

//
// MapInfo-related Stuff
//

// Default finale data

struct finalerule_t
{
   int gameepisode;       // episode where applies, or -1 to match all; -2 ends.
   int gamemap;           // map where applies, or -1 to match all
   const char *backDrop;  // BEX mnemonic of background graphic string
   const char *interText; // BEX mnemonic of intertext string
   int finaleType;        // transferred to LevelInfo.finaleType
   bool endOfGame;        // if true, LevelInfo.endOfGame is set
   bool secretOnly;       // if true, LevelInfo.finaleSecretOnly is set
   bool early;            // if true, LevelInfo.finaleEarly is set
};

struct finaledata_t
{
   int musicnum;        // index into gamemodeinfo_t::s_music
   bool killStatsHack;  // kill stats if !GIF_SHAREWARE && episode >= numEpisodes
   finalerule_t *rules; // rules array
};

// Default sky data

// sky ops
enum
{
   GI_SKY_IFEPISODE,     // rules test episode #'s
   GI_SKY_IFMAPLESSTHAN  // rules test map #'s with <
};

struct skyrule_t
{
   int data;               // data for rule; -1 == match any; -2 == end
   const char *skyTexture; // name of default sky texture
};

struct skydata_t
{
   int testtype;     // if tests maps or episodes
   skyrule_t *rules; // rules array
};

struct samelevel_t
{
   int episode; // episode number
   int map;     // map number
};

// Default boss specs

struct bspecrule_t
{
   int episode;
   int map;
   unsigned int flags;
};

// Default gibhealth behaviors
enum
{
   GI_GIBFULLHEALTH, // DOOM/Strife behavior (gib at -spawnhealth)
   GI_GIBHALFHEALTH  // Heretic/Hexen behavior (gib at -spawnhealth/2)   
};

//
// Monster melee calculation
//
enum meleecalc_e
{
   meleecalc_doom,
   meleecalc_raven,
   // FIXME: how to classify Strife's own z-clipping rule?
   meleecalc_NUM
};

//
// Game Mode Flags
//
enum
{
   GIF_SHAREWARE      = 0x00000002, // shareware game (no -file)
   GIF_MNBIGFONT      = 0x00000004, // uses big font for menu titles
   GIF_MAPXY          = 0x00000008, // gamemode uses MAPxy maps by default
   GIF_SAVESOUND      = 0x00000010, // makes a sound in save & load menus
   GIF_HASADVISORY    = 0x00000020, // displays advisory popup on title screen
   GIF_SHADOWTITLES   = 0x00000040, // shadows titles in menus
   GIF_HASMADMELEE    = 0x00000080, // has mad melee when player dies in SP
   GIF_HASEXITSOUNDS  = 0x00000100, // has sounds at exit
   GIF_WOLFHACK       = 0x00000200, // is subject to German-edition restriction
   GIF_SETENDOFGAME   = 0x00000400, // Teleport_EndGame sets LevelInfo.endOfGame
   GIF_CLASSICMENUS   = 0x00000800, // supports classic/traditional menu emulation
   GIF_SKILL5RESPAWN  = 0x00001000, // monsters respawn by default on skill 5
   GIF_SKILL5WARNING  = 0x00002000, // display menu warning for skill 5
   GIF_HUDSTATBARNAME = 0x00004000, // HUD positions level name above status bar
   GIF_CENTERHUDMSG   = 0x00008000, // HUD messages are centered by default
   GIF_NODIEHI        = 0x00010000, // never plays PDIEHI sound
   GIF_LOSTSOULBOUNCE = 0x00020000, // gamemode or mission normally fixes Lost Soul bouncing
   GIF_IMPACTBLOOD    = 0x00040000, // blood is spawned when actors are impacted by projectiles
   GIF_CHEATSOUND     = 0x00080000, // make menu open sound when cheating
   GIF_CHASEFAST      = 0x00100000, // A_Chase shortens tics like in Raven games
   GIF_NOUPPEREPBOUND = 0x00200000, // Don't clamp down gameepisode if > numEpisodes
   // Weapon frame X offset must be nonzero for both XY offsets to be enabled. Needed for DeHackEd
   // compatibility.
   GIF_DOOMWEAPONOFFSET = 0x00400000,
   GIF_INVALWAYSOPEN  = 0x00800000, // Inventory is always open (like Strife, but not Heretic)
   GIF_BERZERKISPENTA = 0x01000000, // Berzerk is actually penta damage

   // TODO: make this public for EDF gameprops (in a good public form)
   GIF_FLIGHTINERTIA  = 0x02000000, // player flight retains some inertia
   // TODO: make this public for EDF gameprops (in a good public form)
   GIF_WPNSWITCHSUPER = 0x04000000, // only switch to superior weapon when picking up
   GIF_PRBOOMTALLSKY  = 0x08000000, // PrBoom tall sky draw compatibility (do not raise for mlook)
};

// Game mode handling - identify IWAD version
//  to handle IWAD dependent animations etc.
enum GameMode_t
{
   shareware,    // DOOM 1 shareware, E1, M9
   registered,   // DOOM 1 registered, E3, M27
   commercial,   // DOOM 2 retail, E1, M34
   retail,       // DOOM 1 retail, E4, M36
   hereticsw,    // Heretic shareware
   hereticreg,   // Heretic full
   indetermined, // Incomplete or corrupted IWAD
   NumGameModes
};

// Mission packs
enum GameMission_t
{
   doom,         // DOOM 1
   doom2,        // DOOM 2
   pack_tnt,     // TNT mission pack
   pack_plut,    // Plutonia pack
   pack_disk,    // Disk version
   pack_hacx,    // HacX stand-alone IWAD
   pack_psx,     // PSX Doom
   heretic,      // Heretic
   hticbeta,     // Heretic Beta Version
   hticsosr,     // Heretic - Shadow of the Serpent Riders
   none,
   NumGameMissions
};

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

// haleyjd 10/05/05: level translation types
enum
{
   LI_TYPE_DOOM,
   LI_TYPE_HERETIC,
   LI_TYPE_HEXEN,
   LI_TYPE_STRIFE,
   LI_TYPE_NUMTYPES
};

// mission flags
enum
{
   MI_DEMOIFDEMO4    = 0x00000001, // use demoStates override iff DEMO4 exists
   MI_CONBACKTITLE   = 0x00000002, // use console backdrop instead of titlepic
   MI_WOLFNAMEHACKS  = 0x00000004, // overrides Wolf level names if not replaced
   MI_HASBETRAY      = 0x00000008, // has Betray secret MAP33 level
   MI_DOOM2MISSIONS  = 0x00000010, // supports Doom 2 mission packs
   MI_NOTELEPORTZ    = 0x00000020, // teleporters don't set z height in old demos
   MI_NOGDHIGH       = 0x00000040, // M_GDHIGH lump is stupid
   MI_ALLOWEXITTAG   = 0x00000080, // mission allows tagged normal exit lines
   MI_ALLOWSECRETTAG = 0x00000100  // mission allows tagged secret exit lines
};

//
// missioninfo_t
//
// This structure holds mission-dependent information for the
// gamemodeinfo_t structure. The gamemode info structure must be
// assigned a missioninfo_t object during startup in d_main.c:IdentifyVersion.
//
struct missioninfo_t
{
   GameMission_t  id;            // mission id - replaces "gamemission" variable
   unsigned int   flags;         // missioninfo flags
   const char    *gamePathName;  // name of base/game folder used for this mission
   samelevel_t   *sameLevels;    // pointer to samelevel structures
   
   // override data - information here overrides that contained in the
   // gamemodeinfo_t that uses this missioninfo object.

   unsigned int addGMIFlags;        // flags to add to the base GameModeInfo->flags
   unsigned int remGMIFlags;        // flags to remove from base GameModeInfo->flags
   const char *versionNameOR;       // if not nullptr, overrides name of the gamemode
   const char *startupBannerOR;     // if not nullptr, overrides the startup banner 
   int numEpisodesOR;               // if not       0, overrides number of episodes
   char **iwadPathOR;               // if not nullptr, overrides iwadPath
   finaledata_t *finaleDataOR;      // if not nullptr, overrides finaleData
   const char *menuBackgroundOR;    // if not nullptr, overrides menuBackground
   const char *creditBackgroundOR;  // if not nullptr, overrides creditBackground
   const demostate_t *demoStatesOR; // if not nullptr, overrides demostates
   const char *interPicOR;          // if not nullptr, overrides interPic
   exitrule_t *exitRulesOR;         // if not nullptr, overrides exitRules
   const char **levelNamesOR;       // if not nullptr, overrides levelNames
   int randMusMaxOR;                // if not       0, overrides randMusMax
   skydata_t *skyDataOR;            // if not nullptr, overrides skyData
   skyflat_t *skyFlatsOR;           // if not nullptr, overrides skyFlats
   giscale_t *pspriteGlobalScaleOR; // if not nullptr, overrides pspriteGlobalScale
};

// function pointer types
typedef int  (*gimusformapfn_t)();
typedef int  (*gimuscheatfn_t )(const char *);
typedef void (*ginewgamefn_t  )();
typedef int  (*gipartimefn_t  )(int, int);
typedef void (*gilevelnamefn_t)(levelnamedata_t &);
typedef void (*gitexhackfn_t  )(texture_t *);

//
// gamemodeinfo_t
//
// This structure holds just about all the gamemode-pertinent
// information that could easily be pulled out of the rest of
// the source. This approach, as mentioned above, was inspired
// by zdoom, but I've taken it and really run with it.
//
struct gamemodeinfo_t
{
   GameMode_t id;                 // id      - replaces "gamemode" variable
   int type;                      // main game mode type: doom, heretic, etc.
   unsigned int flags;            // game mode flags
   
   // startup stuff
   const char *versionName;       // descriptive version name
   const char *freeVerName;       // FreeDoom override name, if such exists
   const char *bfgEditionName;    // BFG Edition override name, if exists
   const char *startupBanner;     // startup banner text   
   char **iwadPath;               // iwad path variable
   
   // demo state information
   const demostate_t *demoStates; // demostates table
   int titleTics;                 // length of time to show title
   int advisorTics;               // for Heretic, len. to show advisory
   int pageTics;                  // length of general demo state pages
   int titleMusNum;               // music number to use for title

   // menu stuff
   const char *menuBackground;    // name of menu background flat
   const char *creditBackground;  // name of dynamic credit bg flat
   int   creditY;                 // y coord for credit text
   int   creditTitleStep;         // step-down from credit title
   gimenucursor_t *menuCursor;    // pointer to the big menu cursor
   menu_t *mainMenu;              // pointer to main menu structure
   menu_t *saveMenu;              // pointer to save menu structure
   menu_t *loadMenu;              // pointer to load menu structure
   menu_t *newGameMenu;           // pointer to new game menu structure
   const menu_t *episodeMenu;     // pointer to the episode menu (for UMAPINFO)
   const char *menuStartMap;      // new game map lump for skill selection
   int *menuSounds;               // menu sound indices
   int transFrame;                // frame DEH # used on video menu
   int skvAtkSound;               // skin viewer attack sound
   int unselectColor;             // color of unselected menu item text
   int selectColor;               // color of selected menu item text
   int variableColor;             // color of variable text
   int titleColor;                // color of title strings
   int infoColor;                 // color of it_info items
   int bigFontItemColor;          // color of selectable items using big font
   int menuOffset;                // an amount to subtract from menu y coords
   ginewgamefn_t OnNewGame;       // mn_newgame routine

   // border stuff
   const char *borderFlat;        // name of flat to fill backscreen
   giborder_t *border;            // pointer to a game border

   // HU / Video / Console stuff
   int defTextTrans;              // default text color rng for msgs
   int colorNormal;               // color # for normal messages
   int colorHigh;                 // color # for highlighted messages
   int colorError;                // color # for error messages
   int c_numCharsPerLine;         // number of chars per line in console
   int c_BellSound;               // sound used for \a in console
   int c_ChatSound;               // sound used by say command
   const char *consoleBack;       // lump to use for default console backdrop
   unsigned char blackIndex;      // palette index for black {0,0,0}
   unsigned char whiteIndex;      // palette index for white {255,255,255}
   int numHUDKeys;                // number of keys to show in HUD
   const char **cardNames;        // names of default key card artifact types
   const char **levelNames;       // default level name BEX mnemonic array, if any
   gilevelnamefn_t GetLevelName;  // default level name function

   // Status bar
   stbarfns_t *StatusBar;         // status bar function set

   // Automap
   const char *markNumFmt;        // automap mark number format string

   // Miscellaneous graphics
   const char *pausePatch;        // name of patch to show when paused

   // Game interaction stuff
   int numEpisodes;                // number of game episodes
   exitrule_t *exitRules;          // exit rule set
   const char *puffType;           // Name of default puff object
   const char *teleFogType;        // Name of telefog object
   fixed_t teleFogHeight;          // amount to add to telefog z coord
   int teleSound;                  // sound id for teleportation
   int16_t thrustFactor;           // damage thrust factor
   int defaultGibHealth;           // default gibhealth behavior
   const char *defPClassName;      // default playerclass name
   const char *defTranslate;       // default translation for AUTOTRANSLATE
   bspecrule_t *bossRules;         // default boss specials
   int levelType;                  // level translation type
   const char *bloodDefaultNormal; // thingtype of the blood shown when thing is hit by hitscan
   const char *bloodDefaultImpact; // thingtype of the blood shown when thing is hit by projectile
   const char *bloodDefaultRIP;    // thingtype of blood shown when thing is impcated by inflictor with "RIP" flag
   const char *bloodDefaultCrush;  // thingtype of blood shown when thing is crushed
   bloodtype_e *defBloodBehaviors; // default blood behavior for action array
   const char *defCrunchFrame;     // default frame name for when things are smashed by crushers
   double skillAmmoMultiplier;     // how much more ammo to give on baby and nightmare
   meleecalc_e monsterMeleeRange;  // how monster melee range is calculated
   fixed_t itemHeight;             // item pick-up height (independent of thing height)
   const char *autoFlightArtifact; // name of artifact to trigger when commanding to fly
   int lookPitchUp;
   int lookPitchDown;

   // Intermission and Finale stuff
   const char *interPic;          // default intermission backdrop
   int interMusNum;               // intermission music number
   gipartimefn_t GetParTime;      // partime retrieval function for LevelInfo
   giftextpos_t *fTextPos;        // finale text info
   interfns_t *interfuncs;        // intermission function pointers
   int teleEndGameFinaleType;     // Teleport_EndGame causes this finale by default
   finaledata_t *finaleData;      // Default finale data for MapInfo
   int castTitleY;                // Y coord of cast call title
   int castNameY;                 // Y coord of cast call names


   // Sound
   musicinfo_t *s_music;          // pointer to musicinfo_t (sounds.h)
   gimusformapfn_t MusicForMap;   // pointer to S_MusicForMap* routine
   gimuscheatfn_t  MusicCheat;    // pointer to music cheat routine
   int musMin;                    // smallest music index value (0)
   int numMusic;                  // maximum music index value
   int randMusMin;                // beginning of randomizable music 
   int randMusMax;                // end of randomizable music
   const char *musPrefix;         // "D_" for DOOM, "MUS_" for Heretic
   const char *defMusName;        // default music name
   const char *defSoundName;      // default sound if one is missing
   const char **skinSounds;       // default skin sound mnemonics array
   int *playerSounds;             // player sound dehnum indirection
   const char *titleMusName;      // [XA] title music override, for EDF
   const char *secretSoundName;   // name of the secret lump (DSSECRET for non-Strife games)
   int         defSecretSound;    // dehnum of default secret sound

   // Renderer stuff
   int switchEpisode;             // "episode" number for switch texture defs
   skydata_t *skyData;            // default sky data for MapInfo
   gitexhackfn_t TextureHacks;    // texture hacks function
   skyflat_t *skyFlats;           // list of supported sky flats
   giscale_t *pspriteGlobalScale; // psprite global scaling info

   // Configuration
   default_or_t *defaultORs;      // default overrides for configuration file

   // Miscellaneous stuff
   const char *endTextName;       // name of end text resource
   int *exitSounds;               // exit sounds array, if GIF_HASEXITSOUNDS is set

   // Internal fields - these are set at runtime, so keep them last.
   missioninfo_t *missionInfo;    // gamemission-dependent info
};

extern missioninfo_t  *MissionInfoObjects[NumGameMissions];
extern gamemodeinfo_t *GameModeInfoObjects[NumGameModes];

extern gamemodeinfo_t *GameModeInfo;

// set by system config:
extern char *gi_path_doomsw;
extern char *gi_path_doomreg;
extern char *gi_path_doomu;
extern char *gi_path_doom2;
extern char *gi_path_bfgdoom2;
extern char *gi_path_tnt;
extern char *gi_path_plut;
extern char *gi_path_hacx;
extern char *gi_path_hticsw;
extern char *gi_path_hticreg;
extern char *gi_path_sosr;
extern char *gi_path_fdoom;
extern char *gi_path_fdoomu;
extern char *gi_path_freedm;
extern char *gi_path_rekkr;

extern char *gi_path_id24res;


void D_SetGameModeInfo(GameMode_t, GameMission_t);
void D_InitGMIPostWads();

#endif

// EOF
