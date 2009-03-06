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

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"

#include "Confuse/confuse.h"

#include "doomstat.h"
#include "e_lib.h"
#include "d_gi.h"

#define ITEM_USE_DOOM_CONFIG "use_doom_config"

#define ITEM_IWAD_DOOM_SW       "iwad_doom_shareware"
#define ITEM_IWAD_DOOM          "iwad_doom"
#define ITEM_IWAD_ULTIMATE_DOOM "iwad_ultimate_doom"
#define ITEM_IWAD_DOOM2         "iwad_doom2"
#define ITEM_IWAD_TNT           "iwad_tnt"
#define ITEM_IWAD_PLUTONIA      "iwad_plutonia"
#define ITEM_IWAD_HERETIC_SW    "iwad_heretic_shareware"
#define ITEM_IWAD_HERETIC       "iwad_heretic"
#define ITEM_IWAD_HERETIC_SOSR  "iwad_heretic_sosr"

static cfg_opt_t sys_opts[] =
{
   CFG_BOOL(ITEM_USE_DOOM_CONFIG, cfg_false, CFGF_NONE),
   
   CFG_STR(ITEM_IWAD_DOOM_SW,       NULL, CFGF_NONE),
   CFG_STR(ITEM_IWAD_DOOM,          NULL, CFGF_NONE),
   CFG_STR(ITEM_IWAD_ULTIMATE_DOOM, NULL, CFGF_NONE),
   CFG_STR(ITEM_IWAD_DOOM2,         NULL, CFGF_NONE),
   CFG_STR(ITEM_IWAD_TNT,           NULL, CFGF_NONE),
   CFG_STR(ITEM_IWAD_PLUTONIA,      NULL, CFGF_NONE),
   CFG_STR(ITEM_IWAD_HERETIC_SW,    NULL, CFGF_NONE),
   CFG_STR(ITEM_IWAD_HERETIC,       NULL, CFGF_NONE),
   CFG_STR(ITEM_IWAD_HERETIC_SOSR,  NULL, CFGF_NONE),
   
   CFG_END()
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
};

static const char *iwadPathItems[] =
{
   ITEM_IWAD_DOOM_SW,
   ITEM_IWAD_DOOM,
   ITEM_IWAD_ULTIMATE_DOOM,
   ITEM_IWAD_DOOM2,
   ITEM_IWAD_TNT,
   ITEM_IWAD_PLUTONIA,
   ITEM_IWAD_HERETIC_SW,
   ITEM_IWAD_HERETIC,
   ITEM_IWAD_HERETIC_SOSR,
   NULL
};

static void syscfg_error(cfg_t *cfg, const char *foo, va_list ap) {}

void M_LoadSysConfig(const char *filename)
{
   cfg_t *cfg;
   int i;

   cfg = cfg_init(sys_opts, CFGF_NOCASE);

   cfg_set_error_function(cfg, E_ErrorCB);

   if(cfg_parse(cfg, filename))
   {
      printf("M_LoadSysConfig: no system config found, using defaults.\n");
      cfg_free(cfg);
      return; // Oh well. Variables will default.
   }

   // process use_doom_config
   use_doom_config = 
      cfg_getbool(cfg, ITEM_USE_DOOM_CONFIG) == cfg_true ? true : false;

   // process iwads
   for(i = 0; iwadPathItems[i] != NULL; ++i)
   {
      char *str = cfg_getstr(cfg, iwadPathItems[i]);

      if(str)
         *iwadPaths[i] = strdup(str);
   }

   // done.
   cfg_free(cfg);

   printf("M_LoadSysConfig: system defaults loaded.\n");
}