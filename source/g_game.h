// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
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

#ifndef G_GAME_H__
#define G_GAME_H__

// Required for byte
#include "doomdef.h"
#include "doomtype.h"

struct event_t;
struct player_t;
class  Mobj;
class  WadDirectory;

//
// GAME
//

char *G_GetNameForMap(int episode, int map);
int   G_GetMapForName(const char *name, int &episode);

bool G_Responder(const event_t *ev);
bool G_CheckDemoStatus();
void G_DeathMatchSpawnPlayer(int playernum);
void G_DeQueuePlayerCorpse(const Mobj *mo);
void G_ClearPlayerCorpseQueue();
void G_DeferedInitNewNum(skill_t skill, int episode, int map);
void G_DeferedInitNew(skill_t skill, const char *levelname);
void G_DeferedInitNewFromDir(skill_t skill, const char *levelname, WadDirectory *dir);
void G_DeferedPlayDemo(const char *demo);
void G_TimeDemo(const char *name, bool showmenu);
void G_LoadGame(const char *name, int slot, bool is_command); // killough 5/15/98
void G_ForcedLoadGame();                      // killough 5/15/98: forced loadgames
void G_LoadGameErr(const char *msg);
void G_SaveGame(int slot, const char *description); // Called by M_Responder.
void G_RecordDemo(const char *name);                // Only called by startup code.
void G_RecordDemoContinue(const char *in, const char *name);
void G_SetOldDemoOptions();
void G_BeginRecording();
void G_StopDemo();
void G_ScrambleRand();
void G_ExitLevel(int destmap = 0);
void G_SecretExitLevel(int destmap = 0);
void G_WorldDone();
void G_ForceFinale();
void G_Ticker();
void G_ScreenShot();
void G_ReloadDefaults();                // killough 3/01/98: loads game defaults
void G_SaveGameName(char *,size_t,int); // killough 3/22/98: sets savegame filename
void G_SetFastParms(int);               // killough 4/10/98: sets -fast parameters
void G_DoNewGame();
void G_DoReborn(int playernum);
void G_DoLoadLevel();
byte *G_ReadOptions(byte *demoptr);         // killough 3/1/98
byte *G_WriteOptions(byte *demoptr);        // killough 3/1/98
void G_PlayerReborn(int player);
void G_InitNewNum(skill_t skill, int episode, int map);
void G_InitNew(skill_t skill, const char*);
void G_SetGameMapName(const char *s); // haleyjd
void G_SetGameMap();
void G_SpeedSetAddThing(int thingtype, int nspeed, int fspeed); // haleyjd
uint64_t G_Signature(const WadDirectory *dir);
void G_DoPlayDemo();

void R_InitPortals();

int G_TotalKilledMonsters();
int G_TotalFoundItems();
int G_TotalFoundSecrets();

// killough 1/18/98: Doom-style printf;   killough 4/25/98: add gcc attributes
void doom_printf(E_FORMAT_STRING(const char *), ...) E_PRINTF(1, 2);

        // sf: player_printf
void player_printf(const player_t *player, E_FORMAT_STRING(const char *s), ...) E_PRINTF(2, 3);

// killough 5/2/98: moved from m_misc.c:

extern int  key_escape;                                             // phares
extern int  key_autorun;
extern int  key_chat;
extern int  key_help;
extern int  key_pause;
extern int  destination_keys[MAXPLAYERS];
extern int  autorun;           // always running?                   // phares
extern int  runiswalk;
extern int  automlook;
extern int  invert_mouse;
extern int  invert_padlook;
extern int mouseb_dblc1;  // double-clicking either of these buttons
extern int mouseb_dblc2;  // causes a use action

//extern angle_t consoleangle;

extern skill_t defaultskill;     // jff 3/24/98 default skill
extern bool haswolflevels;    // jff 4/18/98 wolf levels present
extern bool demorecording;    // killough 12/98
extern bool forced_loadgame;
extern bool command_loadgame;
extern char gamemapname[9];

extern int  bodyquesize, default_bodyquesize; // killough 2/8/98, 10/98
extern int  animscreenshot;       // animated screenshots

//
// Cool demo setting
//
enum class CoolDemo: int
{
   off,
   random,
   follow
};

extern CoolDemo cooldemo;
extern bool hub_changelevel;

extern bool scriptSecret;   // haleyjd

extern bool sendpause;

extern int mouse_vert; // haleyjd
extern int smooth_turning;

#define VERSIONSIZE   16

// killough 2/22/98: version id string format for savegames
#define VERSIONID "EE"

extern WadDirectory *g_dir;
extern WadDirectory *d_dir;

// killough 2/28/98: A ridiculously large number
// of players, the most you'll ever need in a demo
// or savegame. This is used to prevent problems, in
// case more players in a game are supported later.

#define MIN_MAXPLAYERS 32

#endif

//----------------------------------------------------------------------------
//
// $Log: g_game.h,v $
// Revision 1.10  1998/05/16  09:17:02  killough
// Make loadgame checksum friendlier
//
// Revision 1.9  1998/05/06  15:15:59  jim
// Documented IWAD routines
//
// Revision 1.8  1998/05/03  22:15:50  killough
// Add all external declarations in g_game.c
//
// Revision 1.7  1998/04/27  02:00:53  killough
// Add gcc __attribute__ to check doom_printf() format string
//
// Revision 1.6  1998/04/10  06:34:35  killough
// Fix -fast parameter bugs
//
// Revision 1.5  1998/03/23  03:15:02  killough
// Add G_SaveGameName()
//
// Revision 1.4  1998/03/16  12:29:53  killough
// Remember savegame slot when loading
//
// Revision 1.3  1998/03/02  11:28:46  killough
// Add G_ReloadDefaults() prototype
//
// Revision 1.2  1998/01/26  19:26:51  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:55  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
