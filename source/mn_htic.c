// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2002 James Haley
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
// Heretic-specific menu code
//
// By James Haley
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "d_deh.h"
#include "doomdef.h"
#include "doomstat.h"
#include "dstrings.h"
#include "c_io.h"
#include "d_gi.h"
#include "c_runcmd.h"
#include "mn_engin.h"
#include "mn_misc.h"
#include "v_video.h"
#include "v_misc.h"
#include "w_wad.h"

extern int start_episode;

#define NUM_HSKULL 18

int HSkullLumpNums[NUM_HSKULL];

//
// Heretic-Only Menus
//
// Only the menus that need specific restructuring for Heretic
// are here; everything that could be shared has been.
//

// Main Heretic Menu

void MN_HMainMenuDrawer(void);

static menuitem_t mn_hmain_items[] =
{
   // 'heretic' title and skulls drawn by the drawer
   
   {it_runcmd, "new game",  "mn_hnewgame", NULL, MENUITEM_BIGFONT },
   {it_runcmd, "options",   "mn_options",  NULL, MENUITEM_BIGFONT },
   {it_runcmd, "load game", "mn_loadgame", NULL, MENUITEM_BIGFONT },
   {it_runcmd, "save game", "mn_savegame", NULL, MENUITEM_BIGFONT },
   {it_runcmd, "end game",  "mn_endgame",  NULL, MENUITEM_BIGFONT },
   {it_runcmd, "quit game", "mn_quit",     NULL, MENUITEM_BIGFONT },
   {it_end}
};

menu_t menu_hmain =
{
   mn_hmain_items,        // menu items
   NULL, NULL,            // pages
   100, 56,               // x, y offsets
   0,                     // start with 'new game' selected
   mf_skullmenu,          // a skull menu
   MN_HMainMenuDrawer     // special drawer
};

void MN_HInitSkull(void)
{
   int i;
   char tempstr[10];

   for(i = 0; i < NUM_HSKULL; i++)
   {
      sprintf(tempstr, "M_SKL%.2d", i);
      HSkullLumpNums[i] = W_GetNumForName(tempstr);         
   }
}

void MN_HMainMenuDrawer(void)
{
   int skullIndex;

   // draw M_HTIC
   V_DrawPatch(88, 0, &vbscreen, W_CacheLumpName("M_HTIC", PU_CACHE));

   // draw spinning skulls
   skullIndex = (menutime / 3) % NUM_HSKULL;

   V_DrawPatch(40, 10, &vbscreen,
      W_CacheLumpNum(HSkullLumpNums[17-skullIndex], PU_CACHE));

   V_DrawPatch(232, 10, &vbscreen,
      W_CacheLumpNum(HSkullLumpNums[skullIndex], PU_CACHE));
}

static menuitem_t mn_hepisode_items[] =
{
   {it_info,   "which episode?",       NULL,         NULL, MENUITEM_BIGFONT},
   {it_gap},
   {it_runcmd, "city of the damned",   "mn_hepis 1", NULL, MENUITEM_BIGFONT},
   {it_runcmd, "hell's maw",           "mn_hepis 2", NULL, MENUITEM_BIGFONT},
   {it_runcmd, "the dome of d'sparil", "mn_hepis 3", NULL, MENUITEM_BIGFONT},
   {it_runcmd, "the ossuary",          "mn_hepis 4", NULL, MENUITEM_BIGFONT},
   {it_runcmd, "the stagnant demesne", "mn_hepis 5", NULL, MENUITEM_BIGFONT},
   {it_end}
};

menu_t menu_hepisode =
{
   mn_hepisode_items,    // menu items
   NULL, NULL,           // pages
   38, 26,               // x,y offsets
   2,                    // starting item: city of the damned
   mf_skullmenu,         // is a skull menu
};

CONSOLE_COMMAND(mn_hnewgame, 0)
{
   if(netgame && !demoplayback)
   {
      MN_Alert(s_NEWGAME);
      return;
   }

   // chop off SoSR episodes if not present
   if(gamemission != hticsosr)
      menu_hepisode.menuitems[5].type = it_end;
   
   MN_StartMenu(&menu_hepisode);
}

static menuitem_t mn_hnewgame_items[] =
{
   {it_info,   "choose skill level",          NULL,        NULL, MENUITEM_BIGFONT},
   {it_gap},
   {it_runcmd, "thou needeth a wet nurse",    "newgame 0", NULL, MENUITEM_BIGFONT},
   {it_runcmd, "yellowbellies-r-us",          "newgame 1", NULL, MENUITEM_BIGFONT},
   {it_runcmd, "bringest them oneth",         "newgame 2", NULL, MENUITEM_BIGFONT},
   {it_runcmd, "thou art a smite-meister",    "newgame 3", NULL, MENUITEM_BIGFONT},
   {it_runcmd, "black plague possesses thee", "newgame 4", NULL, MENUITEM_BIGFONT},
   {it_end}
};

menu_t menu_hnewgame =
{
   mn_hnewgame_items,    // menu items
   NULL, NULL,           // pages
   38, 26,               // x,y offsets
   4,                    // starting item: bringest them oneth
   mf_skullmenu,         // is a skull menu
};

CONSOLE_COMMAND(mn_hepis, cf_notnet)
{
   if(!c_argc)
   {
      C_Printf("usage: mn_hepis <epinum>\n");
      return;
   }
   
   start_episode = atoi(c_argv[0]);
   
   if((gameModeInfo->flags & GIF_SHAREWARE) && start_episode > 1)
   {
      MN_Alert("only available in the registered version");
      return;
   }
   
   MN_StartMenu(&menu_hnewgame);
}

static menuitem_t mn_hsavegame_items[] =
{
   {it_variable, "", "savegame_0" },
   {it_gap},
   {it_variable, "", "savegame_1" },
   {it_gap},
   {it_variable, "", "savegame_2" },
   {it_gap},
   {it_variable, "", "savegame_3" },
   {it_gap},
   {it_variable, "", "savegame_4" },
   {it_gap},
   {it_variable, "", "savegame_5" },
   {it_gap},
   {it_variable, "", "savegame_6" },
   {it_gap},
   {it_variable, "", "savegame_7" },
   {it_end}
};

static void MN_HSaveDrawer(void);

#define HSAVEGAME_BOX_X 70
#define HSAVEGAME_BOX_Y 30
#define HSAVEGAME_X (HSAVEGAME_BOX_X + 5)
#define HSAVEGAME_Y (HSAVEGAME_BOX_Y + 5)

menu_t menu_hsavegame =
{
   mn_hsavegame_items,            // items
   NULL, NULL,                    // pages
   HSAVEGAME_X, HSAVEGAME_Y,      // x, y
   0,                             // starting index
   mf_skullmenu | mf_leftaligned, // flags
   MN_HSaveDrawer,                // drawer
   NULL, NULL,                    // contents stuff
   11                             // gap size override
};

static void MN_HSaveDrawer(void)
{
   int x, y, i;
   const char *title = "save game";

   V_WriteTextBig(title, 160 - V_StringWidthBig(title) / 2, 10);

   x = HSAVEGAME_BOX_X;
   y = HSAVEGAME_BOX_Y;

   for(i = 0; i < 8; ++i)
   {
      V_DrawPatch(x, y, &vbscreen, W_CacheLumpName("M_FSLOT", PU_CACHE));
      y += 20;
   }
}

static menuitem_t mn_hloadgame_items[] =
{
   {it_runcmd, "", "mn_load 0"},
   {it_gap},
   {it_runcmd, "", "mn_load 1"},
   {it_gap},
   {it_runcmd, "", "mn_load 2"},
   {it_gap},
   {it_runcmd, "", "mn_load 3"},
   {it_gap},
   {it_runcmd, "", "mn_load 4"},
   {it_gap},
   {it_runcmd, "", "mn_load 5"},
   {it_gap},
   {it_runcmd, "", "mn_load 6"},
   {it_gap},
   {it_runcmd, "", "mn_load 7"},
   {it_end}
};

static void MN_HLoadDrawer(void);

#define HLOADGAME_BOX_X 70
#define HLOADGAME_BOX_Y 30
#define HLOADGAME_X (HLOADGAME_BOX_X + 5)
#define HLOADGAME_Y (HLOADGAME_BOX_Y + 5)

menu_t menu_hloadgame =
{
   mn_hloadgame_items,            // items
   NULL, NULL,                    // pages
   HLOADGAME_X, HLOADGAME_Y,      // x, y
   0,                             // starting index
   mf_skullmenu | mf_leftaligned, // flags
   MN_HLoadDrawer,                // drawer
   NULL, NULL,                    // contents stuff
   11                             // gap size override
};

#define SAVESLOTS 8

extern char *savegamenames[];

static void MN_HLoadDrawer(void)
{
   int x, y, i;
   const char *title = "load game";

   V_WriteTextBig(title, 160 - V_StringWidthBig(title) / 2, 10);

   x = HLOADGAME_BOX_X;
   y = HLOADGAME_BOX_Y;

   for(i = 0; i < 8; ++i)
   {
      V_DrawPatch(x, y, &vbscreen, W_CacheLumpName("M_FSLOT", PU_CACHE));
      y += 20;
   }

   // this is lame
   for(i = 0, y = 0; i < SAVESLOTS; ++i, y += 2)
   {
      menu_hloadgame.menuitems[y].description =
         savegamenames[i] ? savegamenames[i] : s_EMPTYSTRING;
   }
}

void MN_AddHMenus(void)
{
   C_AddCommand(mn_hnewgame);
   C_AddCommand(mn_hepis);
}

// EOF
