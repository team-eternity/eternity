// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
#include "d_io.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "c_net.h"
#include "f_wipe.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "mn_engin.h"
#include "mn_misc.h"  // haleyjd
#include "m_shots.h"
#include "m_random.h"
#include "m_syscfg.h"
#include "p_inter.h"
#include "p_setup.h"
#include "w_wad.h"

#include "s_sound.h"  // haleyjd: restored exit sounds
#include "sounds.h"   // haleyjd: restored exit sounds
#include "p_partcl.h" // haleyjd: add particle event cmds
#include "d_gi.h"     // haleyjd: gamemode pertinent info
#include "e_player.h" // haleyjd: turbo cmd must alter playerclass info

extern void I_WaitVBL(int); // haleyjd: restored exit sounds

extern int automlook;
extern int invert_mouse;
extern int keylookspeed;

////////////////////////////////////////////////////////////////////////
//
// Game Commands
//

CONSOLE_COMMAND(i_error, 0)
{
   I_Error("%s", c_args);
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
void G_QuitDoom(void)
{
   // haleyjd: re-added code for playing random sound before exit
   extern int snd_card;
   
   if((!netgame || demoplayback) && !nosfxparm && snd_card &&
      GameModeInfo->flags & GIF_HASEXITSOUNDS)
   {
      S_StartSound(NULL, GameModeInfo->exitSounds[(gametic>>2)&7]);
      I_WaitVBL(105);
   }
   
   exit(0);
}

CONSOLE_COMMAND(quit, 0)
{
   G_QuitDoom();
}

CONSOLE_COMMAND(animshot, 0)
{
   if(!c_argc)
   {
      C_Printf(
         "animated screenshot.\n"
         "usage: animshot <frames>\n");
      return;
   }
   animscreenshot = atoi(c_argv[0]);
   C_InstaPopup();    // turn off console
}

CONSOLE_COMMAND(screenshot, 0)
{
   if(cmdtype != c_typed)
      C_InstaPopup();
   G_ScreenShot();
}

// particle stuff

// haleyjd 05/18/02: changed particle vars
// to INT variables with choice of sprite, particle, or both

char *particle_choices[] = { "sprites", "particles", "both" };

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

// horizontal mouse sensitivity

VARIABLE_INT(mouseSensitivity_horiz, NULL, 0, 1024, NULL);
CONSOLE_VARIABLE(sens_horiz, mouseSensitivity_horiz, 0) {}

// vertical mouse sensitivity

VARIABLE_INT(mouseSensitivity_vert, NULL, 0, 1024, NULL);
CONSOLE_VARIABLE(sens_vert, mouseSensitivity_vert, 0) {}

int mouseSensitivity_c;

// combined sensitivity (for old-style menu only)
VARIABLE_INT(mouseSensitivity_c, NULL, 0, 16, NULL);
CONSOLE_VARIABLE(sens_combined, mouseSensitivity_c, 0)
{
   mouseSensitivity_horiz = mouseSensitivity_vert = mouseSensitivity_c * 4;
}

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
   player_t *player = &players[cmdsrc];

   if((player->health > 0) || comp[comp_zombie])
      G_ExitLevel();
}

//////////////////////////////////////
//
// Demo Stuff
//

CONSOLE_COMMAND(playdemo, cf_notnet)
{
   if(W_CheckNumForName(c_argv[0]) == -1)
   {
      C_Printf("%s not found\n",c_argv[0]);
      return;
   }
   
   G_DeferedPlayDemo(c_argv[0]);
   singledemo = true;            // quit after one demo
}


CONSOLE_COMMAND(stopdemo, cf_notnet)
{
   G_StopDemo();
}

CONSOLE_COMMAND(timedemo, cf_notnet)
{
   if(c_argc != 2)
   {
      C_Printf("usage: timedemo demoname showmenu\n");
      return;
   }
   G_TimeDemo(c_argv[0], !!atoi(c_argv[1]));
}

// 'cool' demo

VARIABLE_BOOLEAN(cooldemo, NULL,            onoff);
CONSOLE_VARIABLE(cooldemo, cooldemo, 0) {}

///////////////////////////////////////////////////
//
// Wads
//

// load new wad
// buffered command: r_init during load

CONSOLE_COMMAND(addfile, cf_notnet|cf_buffered)
{
   if(GameModeInfo->flags & GIF_SHAREWARE)
   {
      C_Printf(FC_ERROR"command not available in shareware games\n");
      return;
   }
   D_AddNewFile(c_argv[0]);
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
   mobj_t *mobj;
   int playernum;
   
   playernum = cmdsrc;
   
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

   if(!c_argc)
   {
      C_Printf("usage: map <mapname>\n"
         "   or map <wadfile.wad>\n");
      return;
   }
   
   G_StopDemo();
   
   // check for .wad files
   // i'm not particularly a fan of this myself, but..

   // haleyjd 03/12/06: no .wad loading in netgames
   
   if(!netgame && strlen(c_argv[0]) > 4)
   {
      char *extension;
      extension = c_argv[0] + strlen(c_argv[0]) - 4;
      if(!strcmp(extension, ".wad"))
      {
         if(D_AddNewFile(c_argv[0]))
         {
            G_DeferedInitNew(gameskill, firstlevel);
         }
         return;
      }
   }

   // haleyjd 02/23/04: strict error checking
   lumpnum = W_CheckNumForName(c_argv[0]);

   if(lumpnum != -1 && P_CheckLevel(lumpnum) != LEVEL_FORMAT_INVALID)
   {   
      G_DeferedInitNew(gameskill, c_argv[0]);
   }
   else
      C_Printf(FC_ERROR"%s not found or is not a valid map\n", c_argv[0]);
}

        // player name
VARIABLE_STRING(default_name, NULL,             20);
CONSOLE_NETVAR(name, default_name, cf_handlerset, netcmd_name)
{
   int playernum;
   
   playernum = cmdsrc;
   
   strncpy(players[playernum].name, c_argv[0], 20);
   if(playernum == consoleplayer)
   {
      free(default_name);
      default_name = strdup(c_argv[0]);
   }
}

// screenshot type

char *str_pcx[] = { "bmp", "pcx", "tga" };
VARIABLE_INT(screenshot_pcx, NULL, 0, 2, str_pcx);
CONSOLE_VARIABLE(shot_type, screenshot_pcx, 0) {}

VARIABLE_BOOLEAN(screenshot_gamma, NULL, yesno);
CONSOLE_VARIABLE(shot_gamma, screenshot_gamma, 0) {}

// textmode startup

extern int textmode_startup;            // d_main.c
VARIABLE_BOOLEAN(textmode_startup, NULL,        onoff);
CONSOLE_VARIABLE(textmode_startup, textmode_startup, 0) {}

// demo insurance

extern int demo_insurance;
char *insure_str[]={"off", "on", "when recording"};
VARIABLE_INT(demo_insurance, &default_demo_insurance, 0, 2, insure_str);
CONSOLE_VARIABLE(demo_insurance, demo_insurance, cf_notnet) {}

extern int smooth_turning;
VARIABLE_BOOLEAN(smooth_turning, NULL,          onoff);
CONSOLE_VARIABLE(smooth_turning, smooth_turning, 0) {}


// SoM: mouse accel
char *accel_options[]={ "off", "linear", "choco" };
VARIABLE_INT(mouseAccel_type, NULL, 0, 2, accel_options);
CONSOLE_VARIABLE(mouse_accel, mouseAccel_type, 0) {}

VARIABLE_BOOLEAN(novert, NULL, onoff);
CONSOLE_VARIABLE(mouse_novert, novert, 0) {}

// haleyjd: new stuff

extern int map_point_coordinates;
VARIABLE_BOOLEAN(map_point_coordinates, NULL, onoff);
CONSOLE_VARIABLE(map_coords, map_point_coordinates, 0) {}

VARIABLE_INT(dogs, &default_dogs, 0, 3, NULL);
CONSOLE_VARIABLE(numhelpers, dogs, cf_notnet) {}

VARIABLE_BOOLEAN(dog_jumping, &default_dog_jumping, onoff);
CONSOLE_NETVAR(dogjumping, dog_jumping, cf_server, netcmd_dogjumping) {}

VARIABLE_BOOLEAN(startOnNewMap, NULL, yesno);
CONSOLE_VARIABLE(startonnewmap, startOnNewMap, 0) {}

VARIABLE_BOOLEAN(autorun, NULL, onoff);
CONSOLE_VARIABLE(autorun, autorun, 0) {}

VARIABLE_BOOLEAN(runiswalk, NULL, onoff);
CONSOLE_VARIABLE(runiswalk, runiswalk, 0) {}

// haleyjd 03/22/09: iwad cvars

VARIABLE_STRING(gi_path_doomsw,  NULL, UL);
VARIABLE_STRING(gi_path_doomreg, NULL, UL);
VARIABLE_STRING(gi_path_doomu,   NULL, UL);
VARIABLE_STRING(gi_path_doom2,   NULL, UL);
VARIABLE_STRING(gi_path_tnt,     NULL, UL);
VARIABLE_STRING(gi_path_plut,    NULL, UL);
VARIABLE_STRING(gi_path_hticsw,  NULL, UL);
VARIABLE_STRING(gi_path_hticreg, NULL, UL);
VARIABLE_STRING(gi_path_sosr,    NULL, UL);

static void G_TestIWADPath(char *path)
{
   M_NormalizeSlashes(path);

   // test for read access, and warn if non-existent
   if(access(path, R_OK))
   {
      if(menuactive)
         MN_ErrorMsg("Warning: cannot access filepath");
      else
         C_Printf(FC_ERROR "Warning: cannot access filepath\n");
   }
}

CONSOLE_VARIABLE(iwad_doom_shareware,    gi_path_doomsw,  0) 
{
   G_TestIWADPath(gi_path_doomsw);
}

CONSOLE_VARIABLE(iwad_doom,              gi_path_doomreg, 0) 
{
   G_TestIWADPath(gi_path_doomreg);
}

CONSOLE_VARIABLE(iwad_ultimate_doom,     gi_path_doomu,   0) 
{
   G_TestIWADPath(gi_path_doomu);
}

CONSOLE_VARIABLE(iwad_doom2,             gi_path_doom2,   0) 
{
   G_TestIWADPath(gi_path_doom2);
}

CONSOLE_VARIABLE(iwad_tnt,               gi_path_tnt,     0) 
{
   G_TestIWADPath(gi_path_tnt);
}

CONSOLE_VARIABLE(iwad_plutonia,          gi_path_plut,    0) 
{
   G_TestIWADPath(gi_path_plut);
}

CONSOLE_VARIABLE(iwad_heretic_shareware, gi_path_hticsw,  0) 
{
   G_TestIWADPath(gi_path_hticsw);
}

CONSOLE_VARIABLE(iwad_heretic,           gi_path_hticreg, 0) 
{
   G_TestIWADPath(gi_path_hticreg);
}

CONSOLE_VARIABLE(iwad_heretic_sosr,      gi_path_sosr,    0) 
{
   G_TestIWADPath(gi_path_sosr);
}

VARIABLE_BOOLEAN(use_doom_config, NULL, yesno);
CONSOLE_VARIABLE(use_doom_config, use_doom_config, 0) {}

CONSOLE_COMMAND(m_resetcomments, 0)
{
   M_ResetDefaultComments();
   M_ResetSysComments();
}

////////////////////////////////////////////////////////////////
//
// Chat Macros
//

void G_AddChatMacros(void)
{
   int i;
   
   for(i=0; i<10; i++)
   {
      variable_t *variable;
      command_t *command;
      char tempstr[32];

      memset(tempstr, 0, 32);
      
      // create the variable first
      variable = malloc(sizeof(*variable));
      variable->variable = &chat_macros[i];
      variable->v_default = NULL;
      variable->type = vt_string;      // string value
      variable->min = 0;
      variable->max = 128;
      variable->defines = NULL;
      
      // now the command
      command = malloc(sizeof(*command));
      
      sprintf(tempstr, "chatmacro%i", i);
      command->name = strdup(tempstr);
      command->type = ct_variable;
      command->flags = 0;
      command->variable = variable;
      command->handler = NULL;
      command->netcmd = 0;
      
      (C_AddCommand)(command); // hook into cmdlist
   }
}

///////////////////////////////////////////////////////////////
//
// Weapon Prefs
//

extern int weapon_preferences[2][NUMWEAPONS+1];                   

void G_SetWeapPref(int prefnum, int newvalue)
{
   int i;
   
   // find the pref which has the new value
   
   for(i=0; i<NUMWEAPONS; i++)
      if(weapon_preferences[0][i] == newvalue) break;
      
   weapon_preferences[0][i] = weapon_preferences[0][prefnum];
   weapon_preferences[0][prefnum] = newvalue;
}

char *weapon_str[NUMWEAPONS] =
{"fist", "pistol", "shotgun", "chaingun", "rocket launcher", "plasma gun",
 "bfg", "chainsaw", "double shotgun"};

void G_WeapPrefHandler(void)
{
   int prefnum = 
      (int *)c_command->variable->variable - weapon_preferences[0];
   G_SetWeapPref(prefnum, atoi(c_argv[0]));
}

void G_AddWeapPrefs(void)
{
   int i;
   
   for(i=0; i<NUMWEAPONS; i++)   // haleyjd
   {
      variable_t *variable;
      command_t *command;
      char tempstr[16]; // haleyjd: increased size -- bug fix!

      memset(tempstr, 0, 16);
      
      // create the variable first
      variable = malloc(sizeof(*variable));
      variable->variable = &weapon_preferences[0][i];
      variable->v_default = NULL;
      variable->type = vt_int;
      variable->min = 1;
      variable->max = NUMWEAPONS; // haleyjd
      variable->defines = weapon_str;  // use weapon string defines

      // now the command
      command = malloc(sizeof(*command));

      sprintf(tempstr, "weappref_%i", i+1);
      command->name = strdup(tempstr);
      command->type = ct_variable;
      command->flags = cf_handlerset;
      command->variable = variable;
      command->handler = G_WeapPrefHandler;
      command->netcmd = 0;

      (C_AddCommand)(command); // hook into cmdlist
   }
}

///////////////////////////////////////////////////////////////
//
// Autoloaded Files
//
// haleyjd 03/13/06
//

static char *autoload_names[] =
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

void G_AddAutoloadFiles(void)
{
   variable_t *variable;
   command_t *command;
   int i;
   
   for(i = 0; i < 6; ++i)
   {
      // create the variable first
      variable = malloc(sizeof(*variable));
      variable->variable = autoload_ptrs[i];
      variable->v_default = NULL;
      variable->type = vt_string;
      variable->min = 0;
      variable->max = 1024;
      variable->defines = NULL;
      
      // now the command
      command = malloc(sizeof(*command));
      command->name = autoload_names[i];
      command->type = ct_variable;
      command->flags = 0;
      command->variable = variable;
      command->handler = NULL;
      command->netcmd = 0;

      (C_AddCommand)(command); // hook into cmdlist
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
};

static void Handler_CompTHeights(void)
{
   P_ChangeThingHeights();
}

void G_AddCompat(void)
{
   int i;
   
   for(i = 0; i <= comp_special; i++)   // haleyjd: update this regularly
   {
      variable_t *variable;
      command_t *command;
      char tempstr[32];

      // create the variable first
      variable = malloc(sizeof(*variable));
      variable->variable = &comp[i];
      variable->v_default = &default_comp[i];
      variable->type = vt_int;      // string value
      variable->min = 0;
      variable->max = 1;
      variable->defines = yesno;
      
      // now the command
      command = malloc(sizeof(*command));
      
      psnprintf(tempstr, sizeof(tempstr), "comp_%s", comp_strings[i]);
      command->name = strdup(tempstr);
      command->type = ct_variable;

      switch(i)
      {
      case comp_theights:
         command->flags = cf_server | cf_netvar;
         command->handler = Handler_CompTHeights;
         break;
      default:
         command->flags = cf_server | cf_netvar;
         command->handler = NULL;
         break;
      }

      command->variable = variable;
      command->netcmd = netcmd_comp_0 + i;
      
      (C_AddCommand)(command); // hook into cmdlist
   }
}

void G_AddCommands(void)
{
   C_AddCommand(i_error);
   C_AddCommand(z_print);
   C_AddCommand(z_dumpcore);
   C_AddCommand(starttitle);
   C_AddCommand(endgame);
   C_AddCommand(pause);
   C_AddCommand(quit);
   C_AddCommand(animshot);
   C_AddCommand(screenshot);
   C_AddCommand(shot_type);
   C_AddCommand(shot_gamma);
   C_AddCommand(alwaysmlook);
   C_AddCommand(bobbing);
   C_AddCommand(doom_weapon_toggles);
   C_AddCommand(sens_vert);
   C_AddCommand(sens_horiz);
   C_AddCommand(sens_combined);
   C_AddCommand(mouse_accel);
   C_AddCommand(mouse_novert);
   C_AddCommand(invertmouse);
   C_AddCommand(turbo);
   C_AddCommand(playdemo);
   C_AddCommand(timedemo);
   C_AddCommand(cooldemo);
   C_AddCommand(stopdemo);
   C_AddCommand(exitlevel);
   C_AddCommand(addfile);
   C_AddCommand(listwads);
   C_AddCommand(rngseed);
   C_AddCommand(kill);
   C_AddCommand(map);
   C_AddCommand(name);
   C_AddCommand(textmode_startup);
   C_AddCommand(demo_insurance);
   C_AddCommand(smooth_turning);
   
   // haleyjd: new stuff
   C_AddCommand(map_coords);
   C_AddCommand(numhelpers);   
   C_AddCommand(dogjumping);
   C_AddCommand(draw_particles);
   C_AddCommand(bloodsplattype);
   C_AddCommand(bulletpufftype);
   C_AddCommand(rocket_trails);
   C_AddCommand(grenade_trails);
   C_AddCommand(bfg_cloud);
   C_AddCommand(startonnewmap);
   C_AddCommand(autorun);
   C_AddCommand(runiswalk);
   C_AddCommand(m_resetcomments);

   // haleyjd 03/22/09: iwad paths
   C_AddCommand(iwad_doom_shareware);
   C_AddCommand(iwad_doom);
   C_AddCommand(iwad_ultimate_doom);
   C_AddCommand(iwad_doom2);
   C_AddCommand(iwad_tnt);
   C_AddCommand(iwad_plutonia);
   C_AddCommand(iwad_heretic_shareware);
   C_AddCommand(iwad_heretic);
   C_AddCommand(iwad_heretic_sosr);
   C_AddCommand(use_doom_config);

   G_AddChatMacros();
   G_AddWeapPrefs();
   G_AddCompat();
   G_AddAutoloadFiles(); // haleyjd
   P_AddEventVars(); // haleyjd
}

// EOF
