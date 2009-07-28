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

static default_t sysdefaults[] =
{
   DEFAULT_INT(ITEM_USE_DOOM_CONFIG, &use_doom_config, NULL, 0, 0, 1, wad_no,
               "1 to use base/doom/eternity.cfg for all DOOM gamemodes"),

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

