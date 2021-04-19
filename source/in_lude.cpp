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
//
// DESCRIPTION:
// 
// Shared intermission code
//
// haleyjd: This code has been moved here from wi_stuff.c to provide a
// consistent interface to intermission code and to enable better code reuse.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "d_event.h"
#include "d_gi.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_fonts.h"
#include "e_hash.h"
#include "e_things.h" 
#include "g_game.h"
#include "in_lude.h"
#include "m_random.h"
#include "p_chase.h"
#include "p_enemy.h"
#include "p_info.h"
#include "p_mobjcol.h"
#include "p_tick.h"
#include "r_main.h"
#include "s_sndseq.h"
#include "s_sound.h"
#include "v_video.h"

// Globals

// used for timing
int intertime;

// used to accelerate or skip a stage
int acceleratestage; // killough 3/28/98: made global

static interfns_t *InterFuncs = nullptr;

vfont_t *in_font;
vfont_t *in_bigfont;
vfont_t *in_bignumfont;
char *in_fontname;
char *in_bigfontname;
char *in_bignumfontname;

//
// Intermission Camera
//

// is this just some boring picture, or a view of the level?
static int realbackdrop = 1;
camera_t   intercam;

// haleyjd: mobj collection object for intermission cameras
MobjCollection camerathings;

// chosen camera
Mobj *wi_camera;

//
// Intermission map info
//
static EHashTable<intermapinfo_t, ENCStringHashKey,
&intermapinfo_t::lumpname, &intermapinfo_t::link> in_mapinfo;

//
// IN_AddCameras
//
// haleyjd: new solution for intermission cameras. Uses
// P_CollectThings to remove limit, and actually uses spawned
// things of type 5003 rather than just storing the mapthing_t's.
//
void IN_AddCameras()
{
   camerathings.mobjType = "SMMUCameraSpot";
   camerathings.makeEmpty();

   // no camera view if we're in an older demo.
   if(demo_version < 331)
      return;
   
   camerathings.collectThings();
}

//
// IN_StartCamera
//
// Set up the intermissions camera
//
static void IN_StartCamera()
{
   if(!camerathings.isEmpty())
   {
      realbackdrop = 1;

      // pick a camera at random
      wi_camera = camerathings.getRandom(pr_misc);

#ifdef UNSAFE_BACKDROP
      // remove the player mobjs (look silly in camera view)
      for(int i = 0; i < MAXPLAYERS; ++i)
      {
         if(!playeringame[i])
            continue;
         // this is strange. the monsters can still see the player Mobj, (and
         // even kill it!) even tho it has been removed from the level. I make
         // it unshootable first so they lose interest.
         players[i].mo->flags &= ~MF_SHOOTABLE;
         players[i].mo->remove();
      }
#endif
            
      intercam.x = wi_camera->x;
      intercam.y = wi_camera->y;
      intercam.angle = wi_camera->angle;
      intercam.pitch = 0;

      subsector_t *subsec = R_PointInSubsector(intercam.x, intercam.y);
      intercam.z = subsec->sector->srf.floor.height + 41*FRACUNIT;

      intercam.backupPosition();
      
      // FIXME: does this bite the player's setting for the next map?
      R_SetViewSize(11);     // force fullscreen
   }
   else            // no camera, boring interpic
   {
      realbackdrop = 0;
      wi_camera = nullptr;
      S_StopAllSequences(); // haleyjd 06/06/06
      S_StopLoopedSounds(); // haleyjd 10/06/06
   }
}

// ====================================================================
// IN_slamBackground
// Purpose: Put the full-screen background up prior to patches
// Args:    none
// Returns: void
//
void IN_slamBackground()
{
   if(realbackdrop)
      R_RenderPlayerView(players+displayplayer, &intercam);
   else
      IN_DrawBackground();
}

// ====================================================================
// IN_checkForAccelerate
// Purpose: See if the player has hit either the attack or use key
//          or mouse button.  If so we set acceleratestage to 1 and
//          all those display routines above jump right to the end.
// Args:    none
// Returns: void
//
void IN_checkForAccelerate()
{
   int   i;
   player_t  *player;
   
   // check for button presses to skip delays
   for(i = 0, player = players ; i < MAXPLAYERS ; i++, player++)
   {
      if(playeringame[i])
      {
         if(player->cmd.buttons & BT_ATTACK)
         {
            if(!(player->attackdown & AT_PRIMARY))
               acceleratestage = 1;
            player->attackdown = AT_PRIMARY;
         }
         else
            player->attackdown = AT_NONE;
         
         if (player->cmd.buttons & BT_USE)
         {
            if(!player->usedown)
               acceleratestage = 1;
            player->usedown = true;
         }
         else
            player->usedown = false;
      }
   }
}

//
// IN_Ticker
//
// Runs intermission timer, starts music, checks for acceleration
// when user presses a key, and runs the level if an intermission
// camera is active. Gamemode-specific ticker is called.
//
void IN_Ticker()
{
   // counter for general background animation
   intertime++;  
   
   // intermission music
   if(intertime == 1)
      S_ChangeMusicNum(GameModeInfo->interMusNum, true);

   IN_checkForAccelerate();

   InterFuncs->Ticker();

#ifdef UNSAFE_BACKDROP
   // keep the level running when using an intermission camera
   if(realbackdrop)
      P_Ticker();
#endif
}

//
// IN_Drawer
//
// Calls the gamemode-specific intermission drawer.
//
void IN_Drawer()
{
   InterFuncs->Drawer();
}

//
// IN_DrawBackground
//
// Calls the gamemode-specific background drawer. This doesn't
// use the static global InterFuncs on the off chance that the video 
// mode could change while the game is in intermission mode, but the 
// InterFuncs variable hasn't been initialized yet.
// Called from system-specific code when the video mode changes.
//
void IN_DrawBackground()
{
   GameModeInfo->interfuncs->DrawBackground();
}

//
// IN_Start
//
// Sets up intermission cameras, sets the game to use the proper
// intermission object for the current gamemode, and then calls the
// gamemode's start function.
//
void IN_Start(wbstartstruct_t *wbstartstruct)
{
   // haleyjd 03/24/05: allow skipping stats intermission
   if(LevelInfo.killStats)
   {
      G_WorldDone();
      return;
   }

   if(!in_font)
   {
      if(!(in_font = E_FontForName(in_fontname)))
         I_Error("IN_Start: bad EDF font name %s\n", in_fontname);
      if(!(in_bigfont = E_FontForName(in_bigfontname)))
         I_Error("IN_Start: bad EDF font name %s\n", in_bigfontname);
      if(!(in_bignumfont = E_FontForName(in_bignumfontname)))
         I_Error("IN_Start: bad EDF font name %s\n", in_bignumfontname);
   }

   IN_StartCamera();  //set up camera

   InterFuncs = GameModeInfo->interfuncs;

   InterFuncs->Start(wbstartstruct);
}

//
// Gets the map info based on lump name. If none is there, then create and
// return it.
//
intermapinfo_t &IN_GetMapInfo(const char *lumpname)
{
   intermapinfo_t *info;
   if(!(info = in_mapinfo.objectForKey(lumpname)))
   {
      info = estructalloc(intermapinfo_t, 1);
      info->lumpname = estrdup(lumpname);
      in_mapinfo.addObject(info);
   }
   return *info;
}

// EOF

