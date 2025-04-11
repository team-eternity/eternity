//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
//
// Copyright (C) 2013 Simon Howard
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
// By Popular demand :)
// Hubs.
//
// Hubs are where there is more than one level, and links between them:
// you can freely travel between all the levels in a hub and when you
// return, the level should be exactly as it previously was.
// As in Quake2/Half life/Hexen etc.
//
// By Simon Howard
//
// haleyjd:
// This code is not currently accessible and needs a complete ground-up
// rewrite so that it works properly, taking a cue from Hexen on the
// storage and indexing of hub save files, level transfers, and the
// proper handling of scripts.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "d_event.h"
#include "doomstat.h"
#include "d_io.h"       // SoM 3/14/2002: strncasecmp
#include "g_game.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_saveg.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_state.h"

#define MAXHUBLEVELS 128

struct hublevel_t
{
   char levelname[8];
   char *tmpfile;        // temporary file holding the saved level
};

extern char gamemapname[9];

// sf: set when we are changing to
//  another level in the hub
bool hub_changelevel = false;  

hublevel_t hub_levels[MAXHUBLEVELS];
int num_hub_levels;

// sf: my own tmpnam (djgpp one doesn't work as i want it)

char *temp_hubfile(void)
{
   static int tmpfilenum = 0;
   char *new_tmpfilename;
   
   new_tmpfilename = emalloc(char *, 10);
   
   snprintf(new_tmpfilename, 10, "smmu%i.tmp", tmpfilenum++);
   
   return new_tmpfilename;  
}

void P_ClearHubs(void)
{
   int i;
   
   for(i=0; i<num_hub_levels; i++)
   {
      if(hub_levels[i].tmpfile)
         remove(hub_levels[i].tmpfile);
   }
   
   num_hub_levels = 0;

#if 0
   // clear the hub_script
   T_ClearHubScript();
#endif
}

// seperate function: ensure that atexit is not set twice

static void P_ClearHubsAtExit(void)
{
   static bool atexit_set = false;
   
   if(atexit_set) return;   // already set
   
   I_AtExit(P_ClearHubs);
   
   atexit_set = true;
}

void P_InitHubs(void)
{
   num_hub_levels = 0;
   
   P_ClearHubsAtExit();    // set P_ClearHubs to be called at exit
}

static hublevel_t *HublevelForName(char *name)
{
   int i;
   
   for(i=0; i<num_hub_levels; i++)
   {
      if(!strncasecmp(name, hub_levels[i].levelname, 8))
         return &hub_levels[i];
   }
   
   return nullptr;  // not found
}

static hublevel_t *AddHublevel(char *levelname)
{
   strncpy(hub_levels[num_hub_levels].levelname, levelname, 8);
   hub_levels[num_hub_levels].tmpfile = nullptr;
   
   return &hub_levels[num_hub_levels++];
}

// save the current level in the hub

static void SaveHubLevel(void)
{
   static char hubdesc[] = "smmu hubs";
   hublevel_t *hublevel;
   
   hublevel = HublevelForName(levelmapname);
   
   // create new hublevel if not been there yet
   if(!hublevel)
      hublevel = AddHublevel(levelmapname);
   
   // allocate a temp. filename for save
   if(!hublevel->tmpfile)
      hublevel->tmpfile = temp_hubfile();
   
   P_SaveCurrentLevel(hublevel->tmpfile, hubdesc);
}

static void LoadHubLevel(char *levelname)
{
   hublevel_t *hublevel;
   
   hublevel = HublevelForName(levelname);
   
   if(!hublevel)
   {
      // load level normally
      G_SetGameMapName(levelname);
      gameaction = ga_loadlevel;
   }
   else
   {
      // found saved level: reload
      G_LoadGame(hublevel->tmpfile, 0, 0);
      hub_changelevel = true;
   }
   
   wipegamestate = gamestate;
}

void P_HubChangeLevel(char *levelname)
{
   hub_changelevel = true;
   
   SaveHubLevel();
   LoadHubLevel(levelname);
}

void P_HubReborn(void)
{
   // called when player is reborn when using hubs
   LoadHubLevel(levelmapname);
}

void P_DumpHubs(void)
{
   /*
   int i;
   char tempbuf[10];

   for(i=0; i<num_hub_levels; i++)
   {
      strncpy(tempbuf, hub_levels[i].levelname, 8);
   }
   */
}

static fixed_t  save_xoffset;
static fixed_t  save_yoffset;
static Mobj   save_mobj;
static int      save_sectag;
static player_t *save_player;
static pspdef_t save_psprites[NUMPSPRITES];

// save a player's position relative to a particular sector
void P_SavePlayerPosition(player_t *player, int sectag)
{
   sector_t *sec;
   int secnum;
   
   save_player = player;
   
   // save psprites whatever happens
   
   memcpy(save_psprites, player->psprites, sizeof(player->psprites));

   // save sector x,y offset
   
   save_sectag = sectag;
   
   if((secnum = P_FindSectorFromTag(sectag, -1)) < 0)
   {
      // invalid: sector not found
      save_sectag = -1;
      return;
   }
  
   sec = &sectors[secnum];
   
   // use soundorg x and y as 'centre' of sector
   
   save_xoffset = player->mo->x - sec->soundorg.x;
   save_yoffset = player->mo->y - sec->soundorg.y;
   
   // save mobj so we can restore various bits of data
   // haleyjd 02/04/13: not legit for C++; must replace if rehabilitated
#if 0   
   memcpy(&save_mobj, player->mo, sizeof(Mobj));
#endif
}

// restore the players position -- sector must be the same shape
void P_RestorePlayerPosition(void)
{
   sector_t *sec;
   int secnum;
   
   // we always save and restore the psprites
   
   memcpy(save_player->psprites, save_psprites, sizeof(save_player->psprites));
   
   // restore player position from x,y offset
   
   if(save_sectag == -1) return;      // no sector relativeness
   
   if((secnum = P_FindSectorFromTag(save_sectag, -1)) < 0)
   {
      // invalid: sector not found
      return;
   }
   
   sec = &sectors[secnum];
   
   // restore position
   
   P_UnsetThingPosition(save_player->mo);
   
   save_player->mo->x = sec->soundorg.x + save_xoffset;
   save_player->mo->y = sec->soundorg.y + save_yoffset;
   
   // restore various other things
   save_player->mo->angle = save_mobj.angle;
   save_player->mo->momx = save_mobj.momx;    // keep momentum
   save_player->mo->momy = save_mobj.momy;
   
   P_SetThingPosition(save_player->mo);
}

// EOF

