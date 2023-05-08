// Emacs style mode select -*- C++ -*-
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
// Heretic-specific menu code
//
// By James Haley
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_deh.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "doomdef.h"
#include "doomstat.h"
#include "dstrings.h"
#include "e_fonts.h"
#include "mn_engin.h"
#include "mn_misc.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"

extern int start_episode;
extern char *start_mapname;

extern menu_t *mn_episode_override;

#define NUM_HSKULL 18

int HSkullLumpNums[NUM_HSKULL];

//
// Heretic-Only Menus
//
// Only the menus that need specific restructuring for Heretic
// are here; everything that could be shared has been.
//

// Main Heretic Menu

static void MN_HMainMenuDrawer();

static menuitem_t mn_hmain_items[] =
{
   // 'heretic' title and skulls drawn by the drawer
   
   { it_runcmd, "New Game",  "mn_newgame",  nullptr },
   { it_runcmd, "Options",   "mn_options",  nullptr },
   { it_runcmd, "Load Game", "mn_loadgame", nullptr },
   { it_runcmd, "Save Game", "mn_savegame", nullptr },
   { it_runcmd, "Quit Game", "mn_quit",     nullptr },
   { it_end }
};

menu_t menu_hmain =
{
   mn_hmain_items,            // menu items
   nullptr, nullptr, nullptr, // pages
   100, 56,                   // x, y offsets
   0,                         // start with 'new game' selected
   mf_skullmenu | mf_bigfont, // a skull menu
   MN_HMainMenuDrawer         // special drawer
};

void MN_HInitSkull()
{
   char tempstr[10];

   for(int i = 0; i < NUM_HSKULL; i++)
   {
      snprintf(tempstr, sizeof(tempstr), "M_SKL%.2d", i);
      HSkullLumpNums[i] = W_GetNumForName(tempstr);         
   }
}

static void MN_HMainMenuDrawer()
{
   int skullIndex;

   // draw M_HTIC
   V_DrawPatch(88, 0, &subscreen43, 
      PatchLoader::CacheName(wGlobalDir, "M_HTIC", PU_CACHE));

   // draw spinning skulls
   skullIndex = (menutime / 3) % NUM_HSKULL;

   V_DrawPatch(40, 10, &subscreen43,
      PatchLoader::CacheNum(wGlobalDir, HSkullLumpNums[17-skullIndex], PU_CACHE));

   V_DrawPatch(232, 10, &subscreen43,
      PatchLoader::CacheNum(wGlobalDir, HSkullLumpNums[skullIndex], PU_CACHE));
}

static menuitem_t mn_hepisode_items[] =
{
   { it_info,   "Which Episode?",       nullptr,      nullptr },
   { it_gap },
   { it_runcmd, "City of the Damned",   "mn_hepis 1", nullptr },
   { it_runcmd, "Hell's Maw",           "mn_hepis 2", nullptr },
   { it_runcmd, "The Dome of D'Sparil", "mn_hepis 3", nullptr },
   { it_runcmd, "The Ossuary",          "mn_hepis 4", nullptr },
   { it_runcmd, "The Stagnant Demesne", "mn_hepis 5", nullptr },
   { it_end }
};

menu_t menu_hepisode =
{
   mn_hepisode_items,         // menu items
   nullptr, nullptr, nullptr, // pages
   38, 26,                    // x,y offsets
   2,                         // starting item: city of the damned
   mf_skullmenu | mf_bigfont, // is a skull menu
};

//
// MN_HticNewGame
//
// GameModeInfo function for starting a new game in Heretic modes
//
void MN_HticNewGame()
{
   // chop off SoSR episodes if not present
   if(GameModeInfo->numEpisodes < 6)
      menu_hepisode.menuitems[5].type = it_end;
   
   MN_StartMenu(&menu_hepisode);
}

static menuitem_t mn_hnewgame_items[] =
{
   { it_info,   "Choose Skill Level",          nullptr,     nullptr },
   { it_gap },
   { it_runcmd, "Thou needeth a wet nurse",    "newgame 0", nullptr },
   { it_runcmd, "Yellowbellies-r-us",          "newgame 1", nullptr },
   { it_runcmd, "Bringest them oneth",         "newgame 2", nullptr },
   { it_runcmd, "Thou art a smite-meister",    "newgame 3", nullptr },
   { it_runcmd, "Black plague possesses thee", "newgame 4", nullptr },
   { it_end }
};

menu_t menu_hnewgame =
{
   mn_hnewgame_items,         // menu items
   nullptr, nullptr, nullptr, // pages
   38, 26,                    // x,y offsets
   4,                         // starting item: bringest them oneth
   mf_skullmenu | mf_bigfont, // is a skull menu
};

CONSOLE_COMMAND(mn_hepis, cf_notnet)
{
   if(!Console.argc)
   {
      C_Printf("usage: mn_hepis <epinum>\n");
      return;
   }
   
   start_episode = Console.argv[0]->toInt();
   
   if((GameModeInfo->flags & GIF_SHAREWARE) && start_episode > 1)
   {
      MN_Alert("Only available in the registered version.");
      return;
   }
   
   MN_StartMenu(&menu_hnewgame);
}

// EOF
