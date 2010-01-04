// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2009 James Haley
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
// DESCRIPTION:
//    "System" configuration file.
//    Sets some low-level stuff that must be read before the main config
//    file can be loaded.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomstat.h"
#include "d_main.h"
#include "m_misc.h"
#include "d_gi.h"

// External variables configured here:

extern int textmode_startup;
extern int realtic_clock_rate; // killough 4/13/98: adjustable timer
extern int screenshot_pcx;     // jff 3/30/98: option to output screenshot as pcx or bmp
extern int screenshot_gamma;   // haleyjd 11/16/04: allow disabling gamma correction in shots
extern boolean d_fastrefresh;  // haleyjd 01/04/10

#ifdef _SDL_VER
extern int waitAtExit;
extern int grabmouse;
extern int cpusaver;
extern int use_vsync;
extern boolean unicodeinput;
#endif

#if defined(_WIN32) || defined(HAVE_SCHED_SETAFFINITY)
extern unsigned int process_affinity_mask;
#endif

#if defined _MSC_VER
extern int disable_sysmenu;
#endif

// option name defines

#define ITEM_USE_DOOM_CONFIG    "use_doom_config"

#define ITEM_IWAD_DOOM_SW       "iwad_doom_shareware"
#define ITEM_IWAD_DOOM          "iwad_doom"
#define ITEM_IWAD_ULTIMATE_DOOM "iwad_ultimate_doom"
#define ITEM_IWAD_DOOM2         "iwad_doom2"
#define ITEM_IWAD_TNT           "iwad_tnt"
#define ITEM_IWAD_PLUTONIA      "iwad_plutonia"
#define ITEM_IWAD_HERETIC_SW    "iwad_heretic_shareware"
#define ITEM_IWAD_HERETIC       "iwad_heretic"
#define ITEM_IWAD_HERETIC_SOSR  "iwad_heretic_sosr"

// system defaults array

static default_t sysdefaults[] =
{
   DEFAULT_INT(ITEM_USE_DOOM_CONFIG, &use_doom_config, NULL, 0, 0, 1, wad_no,
               "1 to use base/doom/eternity.cfg for all DOOM gamemodes"),

   // IWAD paths

   DEFAULT_STR(ITEM_IWAD_DOOM_SW, &gi_path_doomsw, NULL, "", wad_no,
               "DOOM Shareware IWAD Path"),

   DEFAULT_STR(ITEM_IWAD_DOOM, &gi_path_doomreg, NULL, "", wad_no,
               "DOOM Registered IWAD Path"),

   DEFAULT_STR(ITEM_IWAD_ULTIMATE_DOOM, &gi_path_doomu, NULL, "", wad_no,
               "Ultimate DOOM IWAD Path"),

   DEFAULT_STR(ITEM_IWAD_DOOM2, &gi_path_doom2, NULL, "", wad_no,
               "DOOM 2 IWAD Path"),

   DEFAULT_STR(ITEM_IWAD_TNT, &gi_path_tnt, NULL, "", wad_no,
               "Final DOOM: TNT - Evilution IWAD Path"),

   DEFAULT_STR(ITEM_IWAD_PLUTONIA, &gi_path_plut, NULL, "", wad_no,
               "Final DOOM: The Plutonia Experiment IWAD Path"),

   DEFAULT_STR(ITEM_IWAD_HERETIC_SW, &gi_path_hticsw, NULL, "", wad_no,
               "Heretic Shareware IWAD Path"),

   DEFAULT_STR(ITEM_IWAD_HERETIC, &gi_path_hticreg, NULL, "", wad_no,
               "Heretic Registered IWAD Path"),

   DEFAULT_STR(ITEM_IWAD_HERETIC_SOSR, &gi_path_sosr, NULL, "", wad_no,
               "Heretic: Shadow of the Serpent Riders IWAD Path"),

   // 11/04/09: system-level options moved here from the main config

   DEFAULT_INT("textmode_startup", &textmode_startup, NULL, 0, 0, 1, wad_no,
               "Start up ETERNITY in text mode"),

   DEFAULT_INT("use_vsync", &use_vsync, NULL, 1, 0, 1, wad_no,
               "1 to enable wait for vsync to avoid display tearing"),

   DEFAULT_INT("realtic_clock_rate", &realtic_clock_rate, NULL, 100, 10, 1000, wad_no,
               "Percentage of normal speed (35 fps) realtic clock runs at"),

   // haleyjd 12/08/01
   DEFAULT_INT("force_flip_pan", &forceFlipPan, NULL, 0, 0, 1, wad_no,
               "Force reversal of stereo audio channels: 0 = normal, 1 = reverse"),

   // jff 3/30/98 add ability to take screenshots in BMP format
   DEFAULT_INT("screenshot_pcx", &screenshot_pcx, NULL, 1, 0, 2, wad_no,
               "screenshot format (0=BMP,1=PCX,2=TGA)"),
   
   DEFAULT_INT("screenshot_gamma", &screenshot_gamma, NULL, 1, 0, 1, wad_no,
               "1 to use gamma correction in screenshots"),

   DEFAULT_BOOL("d_fastrefresh", &d_fastrefresh, NULL, false, wad_no,
                "1 to refresh as fast as possible (uses high CPU)"),

#ifdef _SDL_VER
   // SoM
   DEFAULT_INT("powersaver", &cpusaver, NULL, 0, 0, 15, wad_no,
               "Value > 0 will call system delay function to reduce CPU usage"),

   DEFAULT_BOOL("unicodeinput", &unicodeinput, NULL, true, wad_no,
                "1 to use SDL Unicode input mapping (0 = DOS-like behavior)"),

   DEFAULT_INT("wait_at_exit",&waitAtExit, NULL, 0, 0, 1, wad_no,
               "Always wait for input at exit"),
   
   DEFAULT_INT("grabmouse",&grabmouse, NULL, 1, 0, 1, wad_no,
               "Toggle mouse input grabbing"),
#endif

#if defined(_WIN32) || defined(HAVE_SCHED_SETAFFINITY)
   DEFAULT_INT("process_affinity_mask", &process_affinity_mask, NULL, 0, 0, UL, wad_no, 
               "process affinity mask - warning: expert setting only!"),
#endif

#ifdef _MSC_VER
   DEFAULT_INT("disable_sysmenu", &disable_sysmenu, NULL, 0, 0, 1, wad_no,
               "1 to disable Windows system menu for alt+space compatibility"),
#endif

   { NULL }
};

//
// System Configuration File Object
//
static defaultfile_t sysdeffile =
{
   sysdefaults,
   sizeof(sysdefaults) / sizeof *sysdefaults - 1,
};

//
// M_LoadSysConfig
//
// Parses the system configuration file from the base folder.
//
void M_LoadSysConfig(const char *filename)
{
   startupmsg("M_LoadSysConfig", "Loading base/system.cfg");

   sysdeffile.fileName = strdup(filename);

   M_LoadDefaultFile(&sysdeffile);
}

//
// M_SaveSysConfig
//
// Saves values to the system configuration file in the base folder.
//
void M_SaveSysConfig(void)
{
   M_SaveDefaultFile(&sysdeffile);
}

// EOF

