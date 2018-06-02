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
//--------------------------------------------------------------------------
//
// Game console commands
//
// console commands controlling the game functions.
//
// By Simon Howard
//
//---------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_files.h"
#include "d_gi.h"     // haleyjd: gamemode pertinent info
#include "d_io.h"
#include "d_mod.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_main.h"
#include "e_player.h" // haleyjd: turbo cmd must alter playerclass info
#include "f_wipe.h"
#include "g_game.h"
#include "hal/i_timer.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "i_video.h"
#include "m_misc.h"
#include "m_shots.h"
#include "m_random.h"
#include "m_syscfg.h"
#include "m_utils.h"
#include "mn_engin.h"
#include "mn_misc.h"  // haleyjd
#include "p_inter.h"
#include "p_mobj.h"
#include "p_partcl.h" // haleyjd: add particle event cmds
#include "p_setup.h"
#include "p_user.h"
#include "s_sound.h"  // haleyjd: restored exit sounds
#include "sounds.h"   // haleyjd: restored exit sounds
#include "v_misc.h"
#include "v_video.h"
#include "w_levels.h"
#include "w_wad.h"

extern int automlook;
extern int invert_mouse;
extern int keylookspeed;

////////////////////////////////////////////////////////////////////////
//
// Game Commands
//

CONSOLE_COMMAND(i_exitwithmessage, 0)
{
   I_ExitWithMessage("%s\n", Console.args.constPtr());
}

CONSOLE_COMMAND(i_fatalerror, 0)
{
   I_FatalError(I_ERR_KILL, "%s\n", Console.args.constPtr());
}

CONSOLE_COMMAND(i_error, 0)
{
   I_Error("%s\n", Console.args.constPtr());
}

CONSOLE_COMMAND(z_print, cf_hidden)
{
   Z_PrintZoneHeap();
}

CONSOLE_COMMAND(z_dumpcore, cf_hidden)
{
   Z_DumpCore();
}

CONSOLE_COMMAND(starttitle, cf_notnet)
{
   // haleyjd 04/18/03
   if(demorecording || singledemo)
      G_CheckDemoStatus();

   MN_ClearMenus();         // put menu away
   D_StartTitle();
}

CONSOLE_COMMAND(endgame, cf_notnet)
{
   C_SetConsole();
}

CONSOLE_COMMAND(pause, cf_server)
{
   sendpause = true;
}

//
// haleyjd: Restoration of original exit behavior
//
static void G_QuitDoom()
{
   // haleyjd: re-added code for playing random sound before exit
   extern int snd_card;

   if((!netgame || demoplayback) && !nosfxparm && snd_card &&
      GameModeInfo->flags & GIF_HASEXITSOUNDS)
   {
      S_StartInterfaceSound(GameModeInfo->exitSounds[(gametic>>2)&7]);
      i_haltimer.Sleep(1500);
   }

   exit(0);
}

CONSOLE_COMMAND(quit, 0)
{
   G_QuitDoom();
}

CONSOLE_COMMAND(animshot, 0)
{
   if(!Console.argc)
   {
      C_Printf(
         "animated screenshot.\n"
         "usage: animshot <frames>\n");
      return;
   }
   animscreenshot = Console.argv[0]->toInt();
   C_InstaPopup();    // turn off console
}

CONSOLE_COMMAND(screenshot, 0)
{
   if(Console.cmdtype != c_typed)
      C_InstaPopup();
   G_ScreenShot();
}

// particle stuff

// haleyjd 05/18/02: changed particle vars
// to INT variables with choice of sprite, particle, or both

const char *particle_choices[] = { "sprites", "particles", "both" };

VARIABLE_BOOLEAN(drawparticles, NULL, onoff);
CONSOLE_VARIABLE(draw_particles, drawparticles, 0) {}

VARIABLE_INT(bloodsplat_particle, NULL, 0, 2, particle_choices);
CONSOLE_VARIABLE(bloodsplattype, bloodsplat_particle, 0) {}

VARIABLE_INT(bulletpuff_particle, NULL, 0, 2, particle_choices);
CONSOLE_VARIABLE(bulletpufftype, bulletpuff_particle, 0) {}

VARIABLE_BOOLEAN(drawrockettrails, NULL, onoff);
CONSOLE_VARIABLE(rocket_trails, drawrockettrails, 0) {}

VARIABLE_BOOLEAN(drawgrenadetrails, NULL, onoff);
CONSOLE_VARIABLE(grenade_trails, drawgrenadetrails, 0) {}

VARIABLE_BOOLEAN(drawbfgcloud, NULL, onoff);
CONSOLE_VARIABLE(bfg_cloud, drawbfgcloud, 0) {}

// always mlook

VARIABLE_BOOLEAN(automlook, NULL,           onoff);
CONSOLE_VARIABLE(alwaysmlook, automlook, 0) {}

// invert mouse

VARIABLE_BOOLEAN(invert_mouse, NULL,        onoff);
CONSOLE_VARIABLE(invertmouse, invert_mouse, 0) {}

VARIABLE_BOOLEAN(invert_padlook, NULL, onoff);
CONSOLE_VARIABLE(invert_padlook, invert_padlook, 0) {}

// horizontal mouse sensitivity

VARIABLE_FLOAT(mouseSensitivity_horiz, NULL, 0.0, 1024.0);
CONSOLE_VARIABLE(sens_horiz, mouseSensitivity_horiz, 0) {}

// vertical mouse sensitivity

VARIABLE_FLOAT(mouseSensitivity_vert, NULL, 0.0, 1024.0);
CONSOLE_VARIABLE(sens_vert, mouseSensitivity_vert, 0) {}

int mouseSensitivity_c;

// combined sensitivity (for old-style menu only)
VARIABLE_INT(mouseSensitivity_c, NULL, 0, 16, NULL);
CONSOLE_VARIABLE(sens_combined, mouseSensitivity_c, 0)
{
   mouseSensitivity_horiz = mouseSensitivity_vert = mouseSensitivity_c * 4;
}

// [CG] 01/20/12: Create "vanilla" and "raw" mouse sensitivities.
bool default_vanilla_mouse_sensitivity = true;
VARIABLE_TOGGLE(mouseSensitivity_vanilla, &default_vanilla_mouse_sensitivity,
                yesno);
CONSOLE_VARIABLE(sens_vanilla, mouseSensitivity_vanilla, 0) {}

// player bobbing -- haleyjd: altered to read default, use netcmd

VARIABLE_BOOLEAN(player_bobbing, &default_player_bobbing, onoff);
CONSOLE_NETVAR(bobbing, player_bobbing, cf_server, netcmd_bobbing) {}

VARIABLE_BOOLEAN(doom_weapon_toggles, NULL, onoff);
CONSOLE_VARIABLE(doom_weapon_toggles, doom_weapon_toggles, 0) {}

// turbo scale

int turbo_scale = 100;
VARIABLE_INT(turbo_scale, NULL,         10, 400, NULL);
CONSOLE_VARIABLE(turbo, turbo_scale, 0)
{
   C_Printf("turbo scale: %i%%\n",turbo_scale);
   E_ApplyTurbo(turbo_scale);
}

CONSOLE_NETCMD(exitlevel, cf_server|cf_level, netcmd_exitlevel)
{
   // haleyjd 09/04/02: prevent exit if dead, unless comp flag on
   player_t *player = &players[Console.cmdsrc];

   if((player->health > 0) || comp[comp_zombie])
      G_ExitLevel();
}

//=============================================================================
//
// Demo Stuff
//

CONSOLE_COMMAND(playdemo, cf_notnet)
{
   if(Console.argc < 1)
   {
      C_Printf("usage: playdemo demoname\n");
      return;
   }

   // haleyjd 02/15/10: check in both ns_demos and ns_global
   if(wGlobalDir.checkNumForNameNSG(Console.argv[0]->constPtr(), lumpinfo_t::ns_demos) < 0)
   {
      C_Printf(FC_ERROR "%s not found\n", Console.argv[0]->constPtr());
      return;
   }

   G_DeferedPlayDemo(Console.argv[0]->constPtr());
   singledemo = true;            // quit after one demo
}


CONSOLE_COMMAND(stopdemo, cf_notnet)
{
   G_StopDemo();
}

CONSOLE_COMMAND(timedemo, cf_notnet)
{
   if(Console.argc != 2)
   {
      C_Printf("usage: timedemo demoname showmenu\n");
      return;
   }
   G_TimeDemo(Console.argv[0]->constPtr(), !!Console.argv[1]->toInt());
}

// 'cool' demo

const char *cooldemo_modes[] =
{
   "off",
   "random",
   "follow"
};

VARIABLE_INT(cooldemo, NULL, 0, 2, cooldemo_modes);
CONSOLE_VARIABLE(cooldemo, cooldemo, 0) {}

//=============================================================================
//
// Wads
//

// load new wad
// buffered command: r_init during load

CONSOLE_COMMAND(addfile, cf_notnet|cf_buffered)
{
   if(GameModeInfo->flags & GIF_SHAREWARE)
   {
      C_Printf(FC_ERROR "command not available in shareware games\n");
      return;
   }
   D_AddNewFile(Console.argv[0]->constPtr());
}

// list loaded wads

CONSOLE_COMMAND(listwads, 0)
{
   D_ListWads();
}

// random seed

CONST_INT(rngseed);
CONSOLE_CONST(rngseed, rngseed);

// suicide

CONSOLE_NETCMD(kill, cf_level, netcmd_kill)
{
   Mobj *mobj;
   int playernum;

   playernum = Console.cmdsrc;

   mobj = players[playernum].mo;
   P_DamageMobj(mobj, NULL, NULL,
                2*(players[playernum].health+players[playernum].armorpoints),
                MOD_SUICIDE);
   mobj->momx = mobj->momy = mobj->momz = 0;
   players[playernum].momx = players[playernum].momy = 0;
}

// change level

CONSOLE_NETCMD(map, cf_server, netcmd_map)
{
   int lumpnum;

   if(!Console.argc)
   {
      C_Printf("usage: map <mapname>\n");
      return;
   }

   G_StopDemo();

   // check for .wad files
   // i'm not particularly a fan of this myself, but..

   // haleyjd 03/12/06: no .wad loading in netgames
   // haleyjd 05/24/13: or in shareware!

   if(!netgame && !(GameModeInfo->flags & GIF_SHAREWARE) && Console.argv[0]->length() > 4)
   {
      const char *extension;
      extension = Console.argv[0]->bufferAt(Console.argv[0]->length() - 4);
      if(!strcasecmp(extension, ".wad"))
      {
         if(D_AddNewFile(Console.argv[0]->constPtr()))
         {
            C_Popup();
            D_StartTitle();
         }
         return;
      }
   }

   // haleyjd 02/23/04: strict error checking
   lumpnum = W_CheckNumForName(Console.argv[0]->constPtr());

   if(lumpnum != -1 && P_CheckLevel(&wGlobalDir, lumpnum) != LEVEL_FORMAT_INVALID)
      G_DeferedInitNew(gameskill, Console.argv[0]->constPtr());
   else
      C_Printf(FC_ERROR "%s not found or is not a valid map\n", Console.argv[0]->constPtr());
}

// restart map (shorthand for doing the map command to the same level)

CONSOLE_NETCMD(restartmap, cf_server, netcmd_restartmap)
{
   G_DeferedInitNew(gameskill, gamemapname);
}

        // player name
VARIABLE_STRING(default_name, NULL,             20);
CONSOLE_NETVAR(name, default_name, cf_handlerset, netcmd_name)
{
   int playernum;

   if(Console.argc < 1)
      return;

   playernum = Console.cmdsrc;

   Console.argv[0]->copyInto(players[playernum].name, 20);

   if(playernum == consoleplayer)
   {
      efree(default_name);
      default_name = Console.argv[0]->duplicate(PU_STATIC);
   }
}

// screenshot type

const char *str_pcx[] = { "bmp", "pcx", "tga", "png" };
VARIABLE_INT(screenshot_pcx, NULL, 0, 3, str_pcx);
CONSOLE_VARIABLE(shot_type, screenshot_pcx, 0) {}

VARIABLE_BOOLEAN(screenshot_gamma, NULL, yesno);
CONSOLE_VARIABLE(shot_gamma, screenshot_gamma, 0) {}

// textmode startup

extern int textmode_startup;            // d_main.c
VARIABLE_BOOLEAN(textmode_startup, NULL,        onoff);
CONSOLE_VARIABLE(textmode_startup, textmode_startup, 0) {}

// demo insurance

extern int demo_insurance;
const char *insure_str[]={"off", "on", "when recording"};
VARIABLE_INT(demo_insurance, &default_demo_insurance, 0, 2, insure_str);
CONSOLE_VARIABLE(demo_insurance, demo_insurance, cf_notnet) {}

extern int smooth_turning;
VARIABLE_BOOLEAN(smooth_turning, NULL,          onoff);
CONSOLE_VARIABLE(smooth_turning, smooth_turning, 0) {}

// SoM: mouse accel
int default_mouse_accel_type = ACCELTYPE_NONE;
const char *accel_options[]={ "off", "linear", "choco", "custom" };
VARIABLE_INT(mouseAccel_type, &default_mouse_accel_type,
             ACCELTYPE_NONE, ACCELTYPE_MAX, accel_options);
CONSOLE_VARIABLE(mouse_accel_type, mouseAccel_type, ACCELTYPE_NONE) {}

// [CG] 01/20/12: Custom mouse acceleration (threshold & value).
int default_mouse_accel_threshold = 10;
VARIABLE_INT(mouseAccel_threshold, &default_mouse_accel_threshold, 0, 1024,
             NULL);
CONSOLE_VARIABLE(mouse_accel_threshold, mouseAccel_threshold, 0) {}

double default_mouse_accel_value = 2.0;
VARIABLE_FLOAT(mouseAccel_value, &default_mouse_accel_value, 0.0, 100.0);
CONSOLE_VARIABLE(mouse_accel_value, mouseAccel_value, 0) {}

VARIABLE_BOOLEAN(novert, NULL, onoff);
CONSOLE_VARIABLE(mouse_novert, novert, 0) {}

VARIABLE_INT(mouseb_dblc1, NULL, -1, 2, NULL);
CONSOLE_VARIABLE(mouseb_dblc1, mouseb_dblc1, 0) {}

VARIABLE_INT(mouseb_dblc2, NULL, -1, 2, NULL);
CONSOLE_VARIABLE(mouseb_dblc2, mouseb_dblc2, 0) {}

// haleyjd: new stuff

extern int map_point_coordinates;
VARIABLE_BOOLEAN(map_point_coordinates, NULL, onoff);
CONSOLE_VARIABLE(map_coords, map_point_coordinates, 0) {}

extern int map_secret_after;
VARIABLE_BOOLEAN(map_secret_after, NULL, yesno);
CONSOLE_VARIABLE(map_secret_after, map_secret_after, 0) {}

VARIABLE_INT(dogs, &default_dogs, 0, 3, NULL);
CONSOLE_VARIABLE(numhelpers, dogs, cf_notnet) {}

VARIABLE_BOOLEAN(dog_jumping, &default_dog_jumping, onoff);
CONSOLE_NETVAR(dogjumping, dog_jumping, cf_server, netcmd_dogjumping) {}

VARIABLE_BOOLEAN(autorun, NULL, onoff);
CONSOLE_VARIABLE(autorun, autorun, 0) {}

VARIABLE_BOOLEAN(runiswalk, NULL, onoff);
CONSOLE_VARIABLE(runiswalk, runiswalk, 0) {}

// haleyjd 03/22/09: iwad cvars

VARIABLE_STRING(gi_path_doomsw,   NULL, UL);
VARIABLE_STRING(gi_path_doomreg,  NULL, UL);
VARIABLE_STRING(gi_path_doomu,    NULL, UL);
VARIABLE_STRING(gi_path_doom2,    NULL, UL);
VARIABLE_STRING(gi_path_bfgdoom2, NULL, UL);
VARIABLE_STRING(gi_path_tnt,      NULL, UL);
VARIABLE_STRING(gi_path_plut,     NULL, UL);
VARIABLE_STRING(gi_path_hacx,     NULL, UL);
VARIABLE_STRING(gi_path_hticsw,   NULL, UL);
VARIABLE_STRING(gi_path_hticreg,  NULL, UL);
VARIABLE_STRING(gi_path_sosr,     NULL, UL);
VARIABLE_STRING(gi_path_fdoom,    NULL, UL);
VARIABLE_STRING(gi_path_fdoomu,   NULL, UL);
VARIABLE_STRING(gi_path_freedm,   NULL, UL);

VARIABLE_STRING(w_masterlevelsdirname, NULL, UL);
VARIABLE_STRING(w_norestpath,          NULL, UL);

static bool G_TestIWADPath(char *path)
{
   M_NormalizeSlashes(path);

   // test for read access, and warn if non-existent
   if(access(path, R_OK))
   {
      if(menuactive)
         MN_ErrorMsg("Warning: cannot access filepath");
      else
         C_Printf(FC_ERROR "Warning: cannot access filepath\n");

      return false;
   }

   return true;
}

CONSOLE_VARIABLE(iwad_doom_shareware,    gi_path_doomsw,   cf_allowblank)
{
   G_TestIWADPath(gi_path_doomsw);
}

CONSOLE_VARIABLE(iwad_doom,              gi_path_doomreg,  cf_allowblank)
{
   G_TestIWADPath(gi_path_doomreg);
}

CONSOLE_VARIABLE(iwad_ultimate_doom,     gi_path_doomu,    cf_allowblank)
{
   G_TestIWADPath(gi_path_doomu);
}

CONSOLE_VARIABLE(iwad_doom2,             gi_path_doom2,    cf_allowblank)
{
   G_TestIWADPath(gi_path_doom2);
}

CONSOLE_VARIABLE(iwad_bfgdoom2,          gi_path_bfgdoom2, cf_allowblank)
{
   G_TestIWADPath(gi_path_bfgdoom2);
}

CONSOLE_VARIABLE(iwad_tnt,               gi_path_tnt,      cf_allowblank)
{
   G_TestIWADPath(gi_path_tnt);
}

CONSOLE_VARIABLE(iwad_plutonia,          gi_path_plut,     cf_allowblank)
{
   G_TestIWADPath(gi_path_plut);
}

CONSOLE_VARIABLE(iwad_hacx,              gi_path_hacx,     cf_allowblank)
{
   G_TestIWADPath(gi_path_hacx);
}

CONSOLE_VARIABLE(iwad_heretic_shareware, gi_path_hticsw,   cf_allowblank)
{
   G_TestIWADPath(gi_path_hticsw);
}

CONSOLE_VARIABLE(iwad_heretic,           gi_path_hticreg,  cf_allowblank)
{
   G_TestIWADPath(gi_path_hticreg);
}

CONSOLE_VARIABLE(iwad_heretic_sosr,      gi_path_sosr,     cf_allowblank)
{
   G_TestIWADPath(gi_path_sosr);
}

CONSOLE_VARIABLE(iwad_freedoom,          gi_path_fdoom,    cf_allowblank)
{
   G_TestIWADPath(gi_path_fdoom);
}

CONSOLE_VARIABLE(iwad_freedoomu,         gi_path_fdoomu,   cf_allowblank)
{
   G_TestIWADPath(gi_path_fdoomu);
}

CONSOLE_VARIABLE(iwad_freedm,            gi_path_freedm,   cf_allowblank)
{
   G_TestIWADPath(gi_path_freedm);
}

CONSOLE_VARIABLE(master_levels_dir, w_masterlevelsdirname, cf_allowblank)
{
   if(G_TestIWADPath(w_masterlevelsdirname))
      W_EnumerateMasterLevels(true);
}

CONSOLE_VARIABLE(w_norestpath, w_norestpath, cf_allowblank)
{
   G_TestIWADPath(w_norestpath);
}

VARIABLE_BOOLEAN(use_doom_config, NULL, yesno);
CONSOLE_VARIABLE(use_doom_config, use_doom_config, 0) {}

CONSOLE_COMMAND(spectate_prev, 0)
{
   int i = displayplayer - 1;

   if((gamestate != GS_LEVEL) || ((!demoplayback) && (GameType == gt_dm)))
      return;

   for(; i != displayplayer; i--)
   {
      if(i == -1)
         i = (MAXPLAYERS - 1);

      if(playeringame[i])
         break;
   }

   P_SetDisplayPlayer(i);
}

CONSOLE_COMMAND(spectate_next, 0)
{
   int i = displayplayer + 1;

   if((gamestate != GS_LEVEL) || ((!demoplayback) && (GameType == gt_dm)))
      return;

   for(; i != displayplayer; i++)
   {
      if(i >= MAXPLAYERS)
         i = 0;

      if(playeringame[i])
         break;
   }

   P_SetDisplayPlayer(i);
}

CONSOLE_COMMAND(spectate_self, 0)
{
   if((gamestate != GS_LEVEL) || ((!demoplayback) && (GameType == gt_dm)))
      return;

   P_SetDisplayPlayer(consoleplayer);
}

////////////////////////////////////////////////////////////////
//
// Chat Macros
//

void G_AddChatMacros()
{
   for(int i = 0; i < 10; i++)
   {
      variable_t *variable;
      command_t  *command;
      char tempstr[32];

      memset(tempstr, 0, 32);

      // create the variable first
      variable = estructalloc(variable_t, 1);
      variable->variable  = &chat_macros[i];
      variable->v_default = NULL;
      variable->type      = vt_string;      // string value
      variable->min       = 0;
      variable->max       = 128;
      variable->defines   = NULL;

      // now the command
      command = estructalloc(command_t, 1);

      sprintf(tempstr, "chatmacro%i", i);
      command->name     = estrdup(tempstr);
      command->type     = ct_variable;
      command->flags    = 0;
      command->variable = variable;
      command->handler  = NULL;
      command->netcmd   = 0;

      C_AddCommand(command); // hook into cmdlist
   }
}

///////////////////////////////////////////////////////////////
//
// Autoloaded Files
//
// haleyjd 03/13/06
//

static const char *autoload_names[] =
{
   "auto_wad_1",
   "auto_wad_2",
   "auto_deh_1",
   "auto_deh_2",
   "auto_csc_1",
   "auto_csc_2",
};

extern char *wad_files[];
extern char *deh_files[];
extern char *csc_files[];

static char **autoload_ptrs[] =
{
   &wad_files[0],
   &wad_files[1],
   &deh_files[0],
   &deh_files[1],
   &csc_files[0],
   &csc_files[1],
};

void G_AddAutoloadFiles()
{
   variable_t *variable;
   command_t  *command;

   for(int i = 0; i < 6; i++)
   {
      // create the variable first
      variable = estructalloc(variable_t, 1);
      variable->variable  = autoload_ptrs[i];
      variable->v_default = NULL;
      variable->type      = vt_string;
      variable->min       = 0;
      variable->max       = 1024;
      variable->defines   = NULL;

      // now the command
      command = estructalloc(command_t, 1);
      command->name     = autoload_names[i];
      command->type     = ct_variable;
      command->flags    = cf_allowblank;
      command->variable = variable;
      command->handler  = NULL;
      command->netcmd   = 0;

      C_AddCommand(command); // hook into cmdlist
   }
}

///////////////////////////////////////////////////////////////
//
// Compatibility vectors
//

// names given to cmds
const char *comp_strings[] =
{
  "telefrag",
  "dropoff",
  "vile",
  "pain",
  "skull",
  "blazing",
  "doorlight",
  "model",
  "god",
  "falloff",
  "floors",
  "skymap",
  "pursuit",
  "doorstuck",
  "staylift",
  "zombie",
  "stairs",
  "infcheat",
  "zerotags",
  "terrain",    // haleyjd: TerrainTypes
  "respawnfix", //          Nightmare respawn location fix
  "fallingdmg", //          Players take falling damage
  "soul",       //          Lost soul bouncing
  "theights",   //          Thing heights fix
  "overunder",  //          10/19/02: z checking
  "planeshoot", //          09/22/07: plane shooting
  "special",    //          08/29/09: special failure behavior
  "ninja",      //          04/18/10: ninja spawn
  "aircontrol"
};

static void Handler_CompTHeights()
{
   P_ChangeThingHeights();
}

void G_AddCompat()
{
   for(int i = 0; i < COMP_NUM_USED; i++)   // haleyjd: update this regularly
   {
      variable_t *variable;
      command_t  *command;
      char tempstr[32];

      // create the variable first
      variable = estructalloc(variable_t, 1);
      variable->variable  = &comp[i];
      variable->v_default = &default_comp[i];
      variable->type      = vt_int;
      variable->min       = 0;
      variable->max       = 1;
      variable->defines   = yesno;

      // now the command
      command = estructalloc(command_t, 1);

      psnprintf(tempstr, sizeof(tempstr), "comp_%s", comp_strings[i]);
      command->name = estrdup(tempstr);
      command->type = ct_variable;

      switch(i)
      {
      case comp_theights:
         command->flags   = cf_server | cf_netvar;
         command->handler = Handler_CompTHeights;
         break;
      default:
         command->flags   = cf_server | cf_netvar;
         command->handler = NULL;
         break;
      }

      command->variable = variable;
      command->netcmd   = NETCMD_COMP_0 + i;

      C_AddCommand(command); // hook into cmdlist
   }
}

// EOF
