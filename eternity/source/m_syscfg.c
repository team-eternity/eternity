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
//    "System" configuration file, read-only.
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
   {
      ITEM_USE_DOOM_CONFIG,
      &use_doom_config, NULL,
      0, {0,1}, dt_number, ss_none, wad_no,
      "1 to use base/doom/eternity.cfg for all DOOM gamemodes"
   },

   {
      ITEM_IWAD_DOOM_SW,
      (int *)&gi_path_doomsw, NULL,
      (int) "", {0}, dt_string, ss_none, wad_no,
      "Doom Shareware IWAD Path"
   },

   {
      ITEM_IWAD_DOOM,
      (int *)&gi_path_doomreg, NULL,
      (int) "", {0}, dt_string, ss_none, wad_no,
      "Doom Registered IWAD Path"
   },

   {
      ITEM_IWAD_ULTIMATE_DOOM,
      (int *)&gi_path_doomu, NULL,
      (int) "", {0}, dt_string, ss_none, wad_no,
      "Ultimate Doom IWAD Path"
   },

   {
      ITEM_IWAD_DOOM2,
      (int *)&gi_path_doom2, NULL,
      (int) "", {0}, dt_string, ss_none, wad_no,
      "Doom 2 IWAD Path"
   },

   {
      ITEM_IWAD_TNT,
      (int *)&gi_path_tnt, NULL,
      (int) "", {0}, dt_string, ss_none, wad_no,
      "Final DOOM: TNT - Evilution IWAD Path"
   },

   {
      ITEM_IWAD_PLUTONIA,
      (int *)&gi_path_plut, NULL,
      (int) "", {0}, dt_string, ss_none, wad_no,
      "Final DOOM: The Plutonia Experiment IWAD Path"
   },

   {
      ITEM_IWAD_HERETIC_SW,
      (int *)&gi_path_hticsw, NULL,
      (int) "", {0}, dt_string, ss_none, wad_no,
      "Heretic Shareware IWAD Path"
   },

   {
      ITEM_IWAD_HERETIC,
      (int *)&gi_path_hticreg, NULL,
      (int) "", {0}, dt_string, ss_none, wad_no,
      "Heretic Registered IWAD Path"
   },

   {
      ITEM_IWAD_HERETIC_SOSR,
      (int *)&gi_path_doomsw, NULL,
      (int) "", {0}, dt_string, ss_none, wad_no,
      "Heretic: Shadow of the Serpent Riders IWAD Path"
   },

   { NULL }
};

static defaultfile_t sysdeffile =
{
   sysdefaults,
   sizeof(sysdefaults) / sizeof *sysdefaults - 1,
};

static char **iwadPaths[] =
{
   &gi_path_doomsw,
   &gi_path_doomreg,
   &gi_path_doomu,
   &gi_path_doom2,
   &gi_path_tnt,
   &gi_path_plut,
   &gi_path_hticsw,
   &gi_path_hticreg,
   &gi_path_sosr,
   NULL
};

void M_LoadSysConfig(const char *filename)
{
   int i;

   startupmsg("M_LoadSysConfig", "Loading base/system.cfg");

   sysdeffile.fileName = strdup(filename);

   M_LoadDefaultFile(&sysdeffile);

   // process iwads
   for(i = 0; iwadPaths[i] != NULL; ++i)
   {
      char *str = *iwadPaths[i];

      // if iwads are set to empty strings, free them and set them to NULL.
      if(str && *str == '\0')
      {
         free(str);
         *iwadPaths[i] = NULL;
      }
   }
}

void M_SaveSysConfig(void)
{
   int i;

   // process iwads
   for(i = 0; iwadPaths[i] != NULL; ++i)
   {
      // if iwads are NULL, set to empty string
      if(*iwadPaths[i] == NULL)
         *iwadPaths[i] = "";
   }

   M_SaveDefaultFile(&sysdeffile);
}

// EOF

