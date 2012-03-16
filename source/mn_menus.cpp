// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 Simon Howard
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
// Menus
//
// the actual menus: structs and handler functions (if any)
// console commands to activate each menu
//
// By Simon Howard
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_deh.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "dhticstr.h" // haleyjd
#include "dstrings.h"
#include "e_fonts.h"
#include "e_states.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "hu_over.h"
#include "hu_stuff.h" // haleyjd
#include "i_video.h"
#include "m_misc.h"
#include "m_random.h"
#include "m_swap.h"
#include "mn_htic.h"
#include "mn_engin.h"
#include "mn_emenu.h"
#include "mn_menus.h"
#include "mn_misc.h"
#include "mn_files.h"
#include "p_setup.h"
#include "p_skin.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_patch.h"
#include "r_state.h"
#include "s_sound.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"

// menus: all in this file (not really extern)
extern menu_t menu_newgame;
extern menu_t menu_main;
extern menu_t menu_episode;
extern menu_t menu_startmap;

// This is the original cfg variable, now reintroduced.
int traditional_menu;

// Blocky mode, has default, 0 = high, 1 = normal
//int     detailLevel;    obsolete -- killough
int screenSize;      // screen size

char *mn_phonenum;           // phone number to dial
char *mn_demoname;           // demo to play
char *mn_wadname;            // wad to load

// haleyjd: moved these up here to fix Z_Free error

// haleyjd: was 7
#define SAVESLOTS 8

char *savegamenames[SAVESLOTS];

char *mn_start_mapname;

// haleyjd: keep track of valid save slots
bool savegamepresent[SAVESLOTS];

static void MN_PatchOldMainMenu(void);
static void MN_InitCustomMenu(void);
static void MN_InitSearchStr(void);

void MN_InitMenus(void)
{
   int i; // haleyjd
   
   mn_phonenum = Z_Strdup("555-1212", PU_STATIC, 0);
   mn_demoname = Z_Strdup("demo1", PU_STATIC, 0);
   mn_wadname  = Z_Strdup("", PU_STATIC, 0);
   mn_start_mapname = Z_Strdup("", PU_STATIC, 0); // haleyjd 05/14/06
   
   // haleyjd: initialize via zone memory
   for(i = 0; i < SAVESLOTS; ++i)
   {
      savegamenames[i] = Z_Strdup("", PU_STATIC, 0);
      savegamepresent[i] = false;
   }

   if(GameModeInfo->id == commercial)
      MN_PatchOldMainMenu(); // haleyjd 05/16/04

   MN_InitCustomMenu();      // haleyjd 03/14/06
   MN_InitSearchStr();       // haleyjd 03/15/06
}

//////////////////////////////////////////////////////////////////////////
//
// THE MENUS
//
//////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////
//
// Main Menu
//

void MN_MainMenuDrawer(void);

static menuitem_t mn_main_items[] =
{
   // 'doom' title drawn by the drawer
   
   {it_runcmd, "new game",  "mn_newgame",  "M_NGAME"},
   {it_runcmd, "options",   "mn_options",  "M_OPTION"},
   {it_runcmd, "load game", "mn_loadgame", "M_LOADG"},
   {it_runcmd, "save game", "mn_savegame", "M_SAVEG"},
   {it_runcmd, "end game",  "mn_endgame",  "M_ENDGAM"},
   {it_runcmd, "quit",      "mn_quit",     "M_QUITG"},
   {it_end}
};

menu_t menu_main =
{
   mn_main_items,          // menu items
   NULL, NULL, NULL,       // pages
   100, 65,                // x, y offsets
   0,                      // start with 'new game' selected
   mf_skullmenu,           // a skull menu
   MN_MainMenuDrawer       // special drawer
};

void MN_MainMenuDrawer(void)
{
   // hack for m_doom compatibility
   V_DrawPatch(94, 2, &vbscreen, 
               PatchLoader::CacheName(wGlobalDir, "M_DOOM", PU_CACHE));
}

// haleyjd 05/14/06: moved these up here
int start_episode;

char *start_mapname; // local copy of ptr to cvar value

extern menu_t *mn_episode_override;

VARIABLE_STRING(mn_start_mapname,    NULL,   9);
CONSOLE_VARIABLE(mn_start_mapname, mn_start_mapname, cf_handlerset)
{
   int lumpnum;

   if(!Console.argc)
      return;

   lumpnum = W_CheckNumForName(Console.argv[0]->constPtr());
   
   if(lumpnum == -1 || P_CheckLevel(&wGlobalDir, lumpnum) == LEVEL_FORMAT_INVALID)   
      C_Printf(FC_ERROR "level not found\a\n");
   else
   {
      if(mn_start_mapname)
         efree(mn_start_mapname);
      start_mapname = mn_start_mapname = Console.argv[0]->duplicate(PU_STATIC);
   }

   if(menuactive)
      MN_StartMenu(GameModeInfo->newGameMenu);
}

// mn_newgame called from main menu:
// goes to start map OR
// starts menu
// according to use_startmap, gametype and modifiedgame

CONSOLE_COMMAND(mn_newgame, 0)
{
   if(netgame && !demoplayback)
   {
      MN_Alert("%s", DEH_String("NEWGAME"));
      return;
   }

   // haleyjd 05/14/06: reset episode/level selection variables
   start_episode = 1;
   start_mapname = NULL;

   // haleyjd 05/14/06: check for episode menu override now
   if(mn_episode_override)
   {
      MN_StartMenu(mn_episode_override);
      return;
   }
   
   if(GameModeInfo->id == commercial)
   {
      // determine startmap presence and origin
      int startMapLump = W_CheckNumForName("START");
      bool mapPresent = true;
      lumpinfo_t **lumpinfo = wGlobalDir.GetLumpInfo();

      // if lump not found or the game is modified and the
      // lump comes from the first loaded wad, consider it not
      // present -- FIXME: this assumes the resource wad is loaded first.
      if(startMapLump < 0 || 
         (modifiedgame && 
          lumpinfo[startMapLump]->source == WadDirectory::ResWADSource))
         mapPresent = false;


      // dont use new game menu if not needed
      if(!(modifiedgame && startOnNewMap) && use_startmap && mapPresent)
      {
         if(use_startmap == -1)              // not asked yet
            MN_StartMenu(&menu_startmap);
         else
         {  
            // use start map 
            G_DeferedInitNew((skill_t)(defaultskill - 1), "START");
            MN_ClearMenus();
         }
      }
      else
         MN_StartMenu(&menu_newgame);
   }
   else
   {
      // hack -- cut off thy flesh consumed if not retail
      if(GameModeInfo->id != retail)
         menu_episode.menuitems[5].type = it_end;
      
      MN_StartMenu(&menu_episode);
   }
}

// menu item to quit doom:
// pop up a quit message as in the original

void MN_QuitDoom(void)
{
   int quitmsgnum;
   char quitmsg[128];
   const char *source;
   const char *str = DEH_String("QUITMSG");

   quitmsgnum = M_Random() % 14;

   // sf: use QUITMSG if it has been replaced in a dehacked file

   if(strcmp(str, "")) // support dehacked replacement
      source = str;
   else if(GameModeInfo->type == Game_Heretic) // haleyjd: heretic support
      source = HTICQUITMSG;
   else // normal doom messages
      source = endmsg[quitmsgnum];

   psnprintf(quitmsg, sizeof(quitmsg), "%s\n\n%s", source, DEH_String("DOSY"));
   
   MN_Question(quitmsg, "quit");
}

CONSOLE_COMMAND(mn_quit, 0)
{
   if(Console.cmdtype != c_menu && menuactive)
      return;

   MN_QuitDoom();
}

/////////////////////////////////////////////////////////
//
// Episode Selection
//

static menuitem_t mn_episode_items[] =
{
   {it_title,  "which episode?",            NULL,            "M_EPISOD"},
   {it_gap},
   {it_runcmd, "knee deep in the dead",     "mn_episode 1",  "M_EPI1"},
   {it_runcmd, "the shores of hell",        "mn_episode 2",  "M_EPI2"},
   {it_runcmd, "inferno!",                  "mn_episode 3",  "M_EPI3"},
   {it_runcmd, "thy flesh consumed",        "mn_episode 4",  "M_EPI4"},
   {it_end}
};

menu_t menu_episode =
{
   mn_episode_items,    // menu items
   NULL, NULL, NULL,    // pages
   40, 30,              // x, y offsets
   2,                   // select episode 1
   mf_skullmenu,        // skull menu
};

// console command to select episode

CONSOLE_COMMAND(mn_episode, cf_notnet)
{
   if(!Console.argc)
   {
      C_Printf("usage: mn_episode <epinum>\n");
      return;
   }
   
   start_episode = Console.argv[0]->toInt();
   
   if(GameModeInfo->flags & GIF_SHAREWARE && start_episode > 1)
   {
      MN_Alert("%s", DEH_String("SWSTRING"));
      return;
   }
   
   MN_StartMenu(&menu_newgame);
}

//////////////////////////////////////////////////////////
//
// New Game Menu: Skill Level Selection
//

//
// MN_openNewGameMenu
//
// haleyjd 11/12/09: special initialization method for the newgame menu.
//
static void MN_openNewGameMenu(void)
{
   extern menu_t menu_newgame; // actually just right below...
   
   // start on defaultskill setting
   menu_newgame.selected = (defaultskill - 1) + 4;
}

static menuitem_t mn_newgame_items[] =
{
   {it_title,  "new game",             NULL,                "M_NEWG"},
   {it_gap},
   {it_info,   "choose skill level",   NULL,                "M_SKILL"},
   {it_gap},
   {it_runcmd, "i'm too young to die", "newgame 0",         "M_JKILL"},
   {it_runcmd, "hey, not too rough",   "newgame 1",         "M_ROUGH"},
   {it_runcmd, "hurt me plenty.",      "newgame 2",         "M_HURT"},
   {it_runcmd, "ultra-violence",       "newgame 3",         "M_ULTRA"},
   {it_runcmd, "nightmare!",           "newgame 4",         "M_NMARE"},
   {it_end}
};

menu_t menu_newgame =
{
   mn_newgame_items,   // menu items
   NULL, NULL, NULL,   // pages
   40, 15,             // x,y offsets
   6,                  // starting item (overridden by open method)
   mf_skullmenu,       // is a skull menu
   NULL,               // drawer method
   NULL, NULL,         // toc
   0,                  // gap override
   MN_openNewGameMenu, // open method
};

static void MN_DoNightmare(void)
{
   // haleyjd 05/14/06: check for mn_episode_override
   if(mn_episode_override)
   {
      if(start_mapname) // if set, use name, else use episode num as usual
         G_DeferedInitNew(sk_nightmare, start_mapname);
      else
         G_DeferedInitNewNum(sk_nightmare, start_episode, 1);
   }
   else if(GameModeInfo->id == commercial && modifiedgame && startOnNewMap)
   {
      // start on newest level from wad
      G_DeferedInitNew(sk_nightmare, firstlevel);
   }
   else
   {
      // start on first level of selected episode
      G_DeferedInitNewNum(sk_nightmare, start_episode, 1);
   }
   
   MN_ClearMenus();
}

CONSOLE_COMMAND(newgame, cf_notnet)
{
   int skill = gameskill;
   
   // skill level is argv 0
   
   if(Console.argc)
      skill = Console.argv[0]->toInt();

   // haleyjd 07/27/05: restored nightmare behavior
   if(GameModeInfo->flags & GIF_SKILL5WARNING && skill == sk_nightmare)
   {
      MN_QuestionFunc(DEH_String("NIGHTMARE"), MN_DoNightmare);
      return;
   }

   // haleyjd 05/14/06: check for mn_episode_override
   if(mn_episode_override)
   {
      if(start_mapname) // if set, use name, else use episode num as usual
         G_DeferedInitNew((skill_t)skill, start_mapname);
      else
         G_DeferedInitNewNum((skill_t)skill, start_episode, 1);
   }
   else if(GameModeInfo->id == commercial && modifiedgame && startOnNewMap)
   {  
      // haleyjd 03/02/03: changed to use startOnNewMap config variable
      // start on newest level from wad
      G_DeferedInitNew((skill_t)skill, firstlevel);
   }
   else
   {
      // start on first level of selected episode
      G_DeferedInitNewNum((skill_t)skill, start_episode, 1);
   }
   
   MN_ClearMenus();
}

//////////////////////////////////////////////////
//
// First-time Query menu to use start map
//

static menuitem_t mn_startmap_items[] =
{
   {it_title,  "new game",          NULL,                  "M_NEWG"},
   {it_gap},
   {it_info,   "Eternity includes a 'start map' to let"},
   {it_info,   "you start new games from in a level."},
   {it_gap},
   {it_info,   FC_GOLD "in the future would you rather:"},
   {it_gap},
   {it_runcmd, "use the start map", "use_startmap 1; mn_newgame"},
   {it_runcmd, "use the menu",      "use_startmap 0; mn_newgame"},
   {it_end}
};

menu_t menu_startmap =
{
   mn_startmap_items,    // menu items
   NULL, NULL, NULL,     // pages
   40, 15,               // x,y offsets
   7,                    // starting item: start map
   mf_leftaligned | mf_background, 
};

const char *str_startmap[] = {"ask", "no", "yes"};
VARIABLE_INT(use_startmap, NULL, -1, 1, str_startmap);
CONSOLE_VARIABLE(use_startmap, use_startmap, 0) {}


////////////////////////////////////////////////
//
// Demos Menu
//
// Allow Demo playback and (recording),
// also access to camera angles
//

static const char *mn_demo_names[] =
{
   "demos",
   "client/server demos",
   NULL
};

extern menu_t menu_demos;
extern menu_t menu_cs_demos;

static menu_t *mn_demo_pages[] =
{
   &menu_demos,
   &menu_cs_demos,
   NULL
};

static menuitem_t mn_demos_items[] =
{
   {it_title,      FC_GOLD "demos",          NULL,             "m_demos"},
   {it_gap},
   {it_info,       FC_GOLD "play demo"},
   {it_variable,   "demo name",              "mn_demoname"},
   {it_runcmd,     "play demo...",           "mn_clearmenus; playdemo %mn_demoname"},
   {it_runcmd,     "stop demo...",           "mn_clearmenus; stopdemo"},
   {it_gap},
   {it_info,       FC_GOLD "sync"},
   {it_toggle,     "demo insurance",         "demo_insurance"},
   {it_gap},
   {it_info,       FC_GOLD "cameras"},
   {it_toggle,     "viewpoint changes",      "cooldemo"},
   {it_toggle,     "walkcam",                "walkcam"},
   {it_toggle,     "chasecam",               "chasecam"},
   {it_variable,   "chasecam height",        "chasecam_height"},
   {it_variable,   "chasecam speed %",       "chasecam_speed"},
   {it_variable,   "chasecam distance",      "chasecam_dist"},
   {it_end}
};

static menuitem_t mn_cs_demos_items[] =
{
   {it_title,    FC_GOLD"demos",               NULL, "m_demos"},
   {it_gap},
   {it_info,     FC_GOLD"client/server demos", NULL, NULL, MENUITEM_CENTERED },
   {it_variable,        "demo folder",         "cs_demo_folder_path"},
   {it_variable,        "compression level",   "cs_demo_compression_level"},
   {it_end}
};

menu_t menu_demos = 
{
   mn_demos_items,    // menu items
   NULL,              // previous page
   &menu_cs_demos,    // next page
   &menu_demos,       // rootpage
   200, 15,           // x,y
   3,                 // start item
   mf_background,     // full screen
   NULL,              // no drawer
   mn_demo_names,     // TOC stuff
   mn_demo_pages,
};

menu_t menu_cs_demos = 
{
   mn_cs_demos_items, // menu items
   &menu_demos,       // previous page
   NULL,              // next page
   &menu_demos,       // rootpage
   200, 15,           // x,y
   0,                 // start item
   mf_background,     // full screen
   NULL,              // no drawer
   mn_demo_names,     // TOC stuff
   mn_demo_pages,
};

VARIABLE_STRING(mn_demoname,     NULL,           12);
CONSOLE_VARIABLE(mn_demoname,    mn_demoname,     0) {}

CONSOLE_COMMAND(mn_demos, cf_notnet)
{
   MN_StartMenu(&menu_demos);
}

//=============================================================================
//
// Load new pwad menu
//
// Using SMMU dynamic wad loading
//

extern menu_t menu_loadwad;
extern menu_t menu_wadmisc;
extern menu_t menu_wadiwads1;
extern menu_t menu_wadiwads2;
extern menu_t menu_wadiwads3;

static const char *mn_wad_names[] =
{
   "File Selection",
   "Misc Settings",
   "IWAD Paths - DOOM",
   "IWAD Paths - Raven",
   "IWAD Paths - Freedoom",
   NULL
};

static menu_t *mn_wad_pages[] =
{
   &menu_loadwad,
   &menu_wadmisc,
   &menu_wadiwads1,
   &menu_wadiwads2,
   &menu_wadiwads3,
   NULL
};

static menuitem_t mn_loadwad_items[] =
{
   {it_title,    FC_GOLD "Load Wad",       NULL,               "M_WAD"},
   {it_gap},
   {it_info,     FC_GOLD "File Selection", NULL,                NULL, MENUITEM_CENTERED },
   {it_gap},
   {it_variable, "Wad name:",              "mn_wadname" },
   {it_variable, "Wad directory:",         "wad_directory" },
   {it_runcmd,   "Select wad...",          "mn_selectwad" },
   {it_gap},
   {it_runcmd,   "Load wad",               "mn_loadwaditem",    NULL, MENUITEM_CENTERED },
   {it_end},
};

static menuitem_t mn_wadmisc_items[] =
{
   {it_title,    FC_GOLD "Wad Options", NULL,                   "M_WADOPT"},
   {it_gap},
   {it_info,     FC_GOLD "Misc Settings", NULL,                 NULL, MENUITEM_CENTERED },
   {it_gap},
   {it_toggle,   "Use start map",        "use_startmap" },
   {it_toggle,   "Start on 1st new map", "startonnewmap" },
   {it_gap},
   {it_info,     FC_GOLD "Autoloaded Files", NULL,              NULL, MENUITEM_CENTERED },
   {it_gap},
   {it_variable, "WAD file 1:",         "auto_wad_1",           NULL, MENUITEM_LALIGNED },
   {it_variable, "WAD file 2:",         "auto_wad_2",           NULL, MENUITEM_LALIGNED },
   {it_variable, "DEH file 1:",         "auto_deh_1",           NULL, MENUITEM_LALIGNED },
   {it_variable, "DEH file 2:",         "auto_deh_2",           NULL, MENUITEM_LALIGNED },
   {it_variable, "CSC file 1:",         "auto_csc_1",           NULL, MENUITEM_LALIGNED },
   {it_variable, "CSC file 2:",         "auto_csc_2",           NULL, MENUITEM_LALIGNED },
   {it_end},
};

static menuitem_t mn_wadiwad1_items[] =
{
   {it_title,    FC_GOLD "Wad Options", NULL,                     "M_WADOPT"},
   {it_gap},
   {it_info,     FC_GOLD "IWAD Paths - DOOM", NULL,               NULL, MENUITEM_CENTERED },
   {it_gap}, 
   {it_variable, "DOOM (SW):",          "iwad_doom_shareware",    NULL, MENUITEM_LALIGNED },
   {it_variable, "DOOM (Reg):",         "iwad_doom",              NULL, MENUITEM_LALIGNED },
   {it_variable, "Ultimate DOOM:",      "iwad_ultimate_doom",     NULL, MENUITEM_LALIGNED },
   {it_variable, "DOOM II:",            "iwad_doom2",             NULL, MENUITEM_LALIGNED },
   {it_variable, "Evilution:",          "iwad_tnt",               NULL, MENUITEM_LALIGNED },
   {it_variable, "Plutonia:",           "iwad_plutonia",          NULL, MENUITEM_LALIGNED },
   {it_variable, "HACX:",               "iwad_hacx",              NULL, MENUITEM_LALIGNED },
   {it_end}
};

static menuitem_t mn_wadiwad2_items[] =
{
   {it_title,    FC_GOLD "Wad Options", NULL,                     "M_WADOPT"},
   {it_gap},
   {it_info,     FC_GOLD "IWAD Paths - Raven", NULL,              NULL, MENUITEM_CENTERED },
   {it_gap}, 
   {it_variable, "Heretic (SW):",       "iwad_heretic_shareware", NULL, MENUITEM_LALIGNED },
   {it_variable, "Heretic (Reg):",      "iwad_heretic",           NULL, MENUITEM_LALIGNED },
   {it_variable, "Heretic SoSR:",       "iwad_heretic_sosr",      NULL, MENUITEM_LALIGNED },
   {it_end}
};

static menuitem_t mn_wadiwad3_items[] =
{
   {it_title,    FC_GOLD "Wad Options", NULL,           "M_WADOPT"},
   {it_gap},
   {it_info,     FC_GOLD "IWAD Paths - Freedoom", NULL,  NULL, MENUITEM_CENTERED },
   {it_gap}, 
   {it_variable, "Freedoom:",          "iwad_freedoom",  NULL, MENUITEM_LALIGNED },
   {it_variable, "Ultimate Freedoom:", "iwad_freedoomu", NULL, MENUITEM_LALIGNED },
   {it_variable, "FreeDM:",            "iwad_freedm",    NULL, MENUITEM_LALIGNED },
   {it_end}
};

menu_t menu_loadwad =
{
   mn_loadwad_items,            // menu items
   NULL, 
   &menu_wadmisc,               // pages
   &menu_loadwad,               // rootpage
   120, 15,                     // x,y offsets
   4,                           // starting item
   mf_background,               // full screen 
   NULL,
   mn_wad_names,
   mn_wad_pages,
};

menu_t menu_wadmisc =
{
   mn_wadmisc_items,
   &menu_loadwad,
   &menu_wadiwads1,
   &menu_loadwad, // rootpage
   200, 15,
   4,
   mf_background,
   NULL,
   mn_wad_names,
   mn_wad_pages,
};

menu_t menu_wadiwads1 =
{
   mn_wadiwad1_items,
   &menu_wadmisc,
   &menu_wadiwads2,
   &menu_loadwad, // rootpage
   200, 15,
   4,
   mf_background,
   NULL,
   mn_wad_names,
   mn_wad_pages,
};

menu_t menu_wadiwads2 =
{
   mn_wadiwad2_items,
   &menu_wadiwads1,
   &menu_wadiwads3,
   &menu_loadwad, // rootpage
   200, 15,
   4,
   mf_background,
   NULL,
   mn_wad_names,
   mn_wad_pages,
};

menu_t menu_wadiwads3 =
{
   mn_wadiwad3_items,
   &menu_wadiwads2,
   NULL,
   &menu_loadwad,
   200, 15,
   4,
   mf_background,
   NULL,
   mn_wad_names,
   mn_wad_pages,
};

VARIABLE_STRING(mn_wadname,  NULL,       UL);
CONSOLE_VARIABLE(mn_wadname, mn_wadname,  0) {}

CONSOLE_COMMAND(mn_loadwad, cf_notnet)
{   
   MN_StartMenu(&menu_loadwad);
}

CONSOLE_COMMAND(mn_loadwaditem, cf_notnet|cf_hidden)
{
   char *filename = NULL;

   // haleyjd 03/12/06: this is much more resilient than the 
   // chain of console commands that was used by SMMU

   // haleyjd: generalized to all shareware modes
   if(GameModeInfo->flags & GIF_SHAREWARE)
   {
      MN_Alert("You must purchase the full version\n"
               "to load external wad files.\n"
               "\n"
               "%s", DEH_String("PRESSKEY"));
      return;
   }

   if(!mn_wadname || strlen(mn_wadname) == 0)
   {
      MN_ErrorMsg("Invalid wad file name");
      return;
   }

   filename = M_SafeFilePath(wad_directory, mn_wadname);

   if(D_AddNewFile(filename))
   {
      MN_ClearMenus();
      G_DeferedInitNew(gameskill, firstlevel);
   }
   else
      MN_ErrorMsg("Failed to load wad file");
}

/////////////////////////////////////////////////////////////////
//
// Multiplayer Game settings
//

static const char *mn_gset_names[] =
{
   "general / dm auto-exit",
   "monsters / boom features",
   "deathmatch flags",
   "chat macros",
   NULL,
};

extern menu_t menu_gamesettings;
extern menu_t menu_advanced;
extern menu_t menu_dmflags;
extern menu_t menu_chatmacros;

static menu_t *mn_gset_pages[] =
{
   &menu_gamesettings,
   &menu_advanced,
   &menu_dmflags,
   &menu_chatmacros,
   NULL,
};

static menuitem_t mn_gamesettings_items[] =
{
   {it_title,    FC_GOLD "game settings",        NULL,       "M_GSET"},
   {it_gap},
   {it_info,     FC_GOLD "general"},
   {it_toggle,   "game type",                  "gametype"},
   {it_variable, "starting level",             "startlevel"},
   {it_toggle,   "skill level",                "skill"},
   {it_gap},
   {it_info,     FC_GOLD "dm auto-exit"},
   {it_variable, "time limit",                 "timelimit"},
   {it_variable, "frag limit",                 "fraglimit"},
   {it_end}
};

menu_t menu_gamesettings =
{
   mn_gamesettings_items,
   NULL, 
   &menu_advanced,               // pages
   &menu_gamesettings,           // rootpage
   164, 15,
   3,                            // start
   mf_background,                // full screen
   NULL,                         // no drawer
   mn_gset_names,                // TOC stuff
   mn_gset_pages,
};

        // level to start on
VARIABLE_STRING(startlevel,    NULL,   9);
CONSOLE_VARIABLE(startlevel, startlevel, cf_handlerset)
{
   const char *newvalue;
   
   if(!Console.argc)
      return;
   
   newvalue = Console.argv[0]->constPtr();
   
   // check for a valid level
   if(W_CheckNumForName(newvalue) == -1)
      MN_ErrorMsg("level not found");
   else
   {
      if(startlevel)
         Z_Free(startlevel);
      startlevel = Z_Strdup(newvalue, PU_STATIC, 0);
   }
}

CONSOLE_COMMAND(mn_gset, 0)            // just setting options from menu
{
   MN_StartMenu(&menu_gamesettings);
}

/////////////////////////////////////////////////////////////////
//
// Multiplayer Game settings
// Advanced menu
//

static menuitem_t mn_advanced_items[] =
{
   {it_title,    FC_GOLD "game settings",           NULL,             "M_GSET"},
   {it_gap},
   {it_info,     FC_GOLD "monsters"},
   {it_toggle,   "no monsters",                "nomonsters"},
   {it_toggle,   "fast monsters",              "fast"},
   {it_toggle,   "respawning monsters",        "respawn"},
   {it_gap},
   {it_info,     FC_GOLD "BOOM features"},
   {it_toggle,   "variable friction",          "varfriction"},
   {it_toggle,   "boom push effects",          "pushers"},
   {it_toggle,   "nukage enabled",             "nukage"},
   {it_end}
};

menu_t menu_advanced =
{
   mn_advanced_items,
   &menu_gamesettings, 
   &menu_dmflags,                // pages
   &menu_gamesettings,           // rootpage
   200, 15,
   3,                            // start
   mf_background,                // full screen
   NULL,                         // no drawer
   mn_gset_names,                // TOC stuff
   mn_gset_pages,
};

/*
CONSOLE_COMMAND(mn_advanced, cf_server)
{
   MN_StartMenu(&menu_advanced);
}
*/

//
// Deathmatch Flags Menu
//

//
// NETCODE_FIXME -- CONSOLE_FIXME: Dm flags may require special treatment
//

static void MN_DMFlagsDrawer(void);

static menuitem_t mn_dmflags_items[] =
{
   {it_title,    FC_GOLD "deathmatch flags",   NULL,            "M_DMFLAG"},
   {it_gap},
   {it_runcmd,   "items respawn",              "mn_dfitem"},
   {it_runcmd,   "weapons stay",               "mn_dfweapstay"},
   {it_runcmd,   "respawning barrels",         "mn_dfbarrel"},
   {it_runcmd,   "players drop items",         "mn_dfplyrdrop"},
   {it_runcmd,   "respawning super items",     "mn_dfrespsupr"},
   {it_runcmd,   "instagib",                   "mn_dfinstagib"},
   {it_gap},
   {it_info,     FC_GOLD "dmflags =",          NULL,            NULL, MENUITEM_CENTERED},
   {it_end}
};

menu_t menu_dmflags =
{
   mn_dmflags_items,
   &menu_advanced, 
   &menu_chatmacros,   // pages
   &menu_gamesettings, // rootpage
   200, 15,
   2,
   mf_background,     // full screen
   MN_DMFlagsDrawer,
   mn_gset_names,     // TOC stuff
   mn_gset_pages,
};

/*
CONSOLE_COMMAND(mn_dmflags, cf_server)
{
   MN_StartMenu(&menu_dmflags);
}
*/

// haleyjd 04/14/03: dmflags menu drawer (a big hack, mostly)

extern vfont_t *menu_font;
extern vfont_t *menu_font_normal;

static void MN_DMFlagsDrawer(void)
{
   int i;
   char buf[64];
   const char *values[2] = { "on", "off" };
   menuitem_t *menuitem;

   // don't draw anything before the menu has been initialized
   if(!(menu_dmflags.menuitems[9].flags & MENUITEM_POSINIT))
      return;

   for(i = 2; i < 8; i++)
   {
      menuitem = &(menu_dmflags.menuitems[i]);
                  
      V_FontWriteTextColored
        (
         menu_font,
         values[!(dmflags & (1<<(i-2)))],
         (i == menu_dmflags.selected) ? 
            GameModeInfo->selectColor : GameModeInfo->variableColor,
         menuitem->x + 20, menuitem->y
        );
   }

   menuitem = &(menu_dmflags.menuitems[9]);
   // draw dmflags value
   psnprintf(buf, sizeof(buf), FC_GOLD "%lu", dmflags);
   V_FontWriteText(menu_font, buf, menuitem->x + 4, menuitem->y);
}

static void toggle_dm_flag(unsigned int flag)
{
   char cmdbuf[64];
   dmflags ^= flag;
   psnprintf(cmdbuf, sizeof(cmdbuf), "dmflags %lu", dmflags);
   C_RunTextCmd(cmdbuf);
}

CONSOLE_COMMAND(mn_dfitem, cf_server|cf_hidden)
{
   toggle_dm_flag(DM_ITEMRESPAWN);
}

CONSOLE_COMMAND(mn_dfweapstay, cf_server|cf_hidden)
{
   toggle_dm_flag(DM_WEAPONSTAY);
}

CONSOLE_COMMAND(mn_dfbarrel, cf_server|cf_hidden)
{
   toggle_dm_flag(DM_BARRELRESPAWN);
}

CONSOLE_COMMAND(mn_dfplyrdrop, cf_server|cf_hidden)
{
   toggle_dm_flag(DM_PLAYERDROP);
}

CONSOLE_COMMAND(mn_dfrespsupr, cf_server|cf_hidden)
{
   toggle_dm_flag(DM_RESPAWNSUPER);
}

CONSOLE_COMMAND(mn_dfinstagib, cf_server|cf_hidden)
{
   toggle_dm_flag(DM_INSTAGIB);
}

/////////////////////////////////////////////////////////////////
//
// Chat Macros
//

static menuitem_t mn_chatmacros_items[] =
{
   {it_title,    FC_GOLD "chat macros", NULL,         "M_CHATM"},
   {it_gap},
   {it_variable, "0:",                  "chatmacro0"},
   {it_variable, "1:",                  "chatmacro1"},
   {it_variable, "2:",                  "chatmacro2"},
   {it_variable, "3:",                  "chatmacro3"},
   {it_variable, "4:",                  "chatmacro4"},
   {it_variable, "5:",                  "chatmacro5"},
   {it_variable, "6:",                  "chatmacro6"},
   {it_variable, "7:",                  "chatmacro7"},
   {it_variable, "8:",                  "chatmacro8"},
   {it_variable, "9:",                  "chatmacro9"},
   {it_end}
};

menu_t menu_chatmacros =
{
   mn_chatmacros_items,
   &menu_dmflags, 
   NULL,                                 // pages
   &menu_gamesettings,                   // rootpage
   25, 15,                               // x, y offset
   2,                                    // chatmacro0 at start
   mf_background,                        // full-screen
   NULL,                                 // no drawer
   mn_gset_names,                        // TOC stuff
   mn_gset_pages,
};

/*
CONSOLE_COMMAND(mn_chatmacros, 0)
{
   MN_StartMenu(&menu_chatmacros);
}
*/

/////////////////////////////////////////////////////////////////
//
// Player Setup
//

void MN_PlayerDrawer(void);

static menuitem_t mn_player_items[] =
{
   {it_title,  FC_GOLD "player setup",         NULL,         "M_PLAYER"},
   {it_gap},
   {it_variable,       "player name",          "name"},
   {it_toggle,         "player colour",        "colour"},
   {it_toggle,         "handedness",           "lefthanded"},
   {it_toggle,         "packet buffer",        "packet_buffer"},
   {it_variable,       "packet buffer size",   "packet_buffer_size"},
   {it_toggle,         "reliable commands",    "reliable_commands"},
   {it_toggle,         "player skin",          "skin"},
   {it_runcmd,         "skin viewer...",       "skinviewer"},
   {it_gap},
   {it_end}
};

menu_t menu_player =
{
   mn_player_items,
   NULL, NULL, NULL,                     // pages
   150, 5,                               // x, y offset
   2,                                    // chatmacro0 at start
   mf_background,                        // full-screen
   MN_PlayerDrawer
};

#define SPRITEBOX_X 200
#define SPRITEBOX_Y (menu_player.menuitems[7].y + 16)

void MN_PlayerDrawer(void)
{
   int lump, w, h, toff, loff;
   spritedef_t *sprdef;
   spriteframe_t *sprframe;
   patch_t *patch;

   if(!(menu_player.menuitems[7].flags & MENUITEM_POSINIT))
      return;

   sprdef = &sprites[players[consoleplayer].skin->sprite];

   // haleyjd 08/15/02
   if(!(sprdef->spriteframes))
      return;
  
   sprframe = &sprdef->spriteframes[0];
   lump = sprframe->lump[1];
   
   patch = PatchLoader::CacheNum(wGlobalDir, lump + firstspritelump, PU_CACHE);

   w    = patch->width;
   h    = patch->height;
   toff = patch->topoffset;
   loff = patch->leftoffset;
   
   V_DrawBox(SPRITEBOX_X, SPRITEBOX_Y, w + 16, h + 16);

   // haleyjd 01/12/04: changed translation handling
   V_DrawPatchTranslated
      (
       SPRITEBOX_X + 8 + loff,
       SPRITEBOX_Y + 8 + toff,
       &vbscreen,
       patch,
       players[consoleplayer].colormap ?
          translationtables[(players[consoleplayer].colormap - 1)] :
          NULL,
       false
      );
}

CONSOLE_COMMAND(mn_player, 0)
{
   MN_StartMenu(&menu_player);
}


/////////////////////////////////////////////////////////////////
//
// Load Game
//

//
// NETCODE_FIXME: Ensure that loading/saving are handled properly in
// netgames when it comes to the menus. Some deficiencies have already
// been caught in the past, so some may still exist.
//

// haleyjd: numerous fixes here from 8-17 version of SMMU

#define SAVESTRINGSIZE  24

// load/save box patches
patch_t *patch_left, *patch_mid, *patch_right;

void MN_SaveGame(void)
{
   int save_slot = 
      (char **)(Console.command->variable->variable) - savegamenames;
   
   if(gamestate != GS_LEVEL) 
      return; // only save in level
   
   if(save_slot < 0 || save_slot >= SAVESLOTS)
      return; // sanity check
   
   G_SaveGame(save_slot, savegamenames[save_slot]);
   MN_ClearMenus();
   
   // haleyjd 02/23/02: restored from MBF
   if(quickSaveSlot == -2)
      quickSaveSlot = save_slot;
   
   // haleyjd: keep track of valid saveslots
   savegamepresent[save_slot] = true;

   // haleyjd 10/08/08: GIF_SAVESOUND flag
   if(GameModeInfo->flags & GIF_SAVESOUND)
      S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_DEACTIVATE]);
}

// create the savegame console commands
void MN_CreateSaveCmds(void)
{
   // haleyjd: something about the way these commands are being created
   //          is causing the console code to free a ptr with no zone id...
   //          08/08/00: fixed -- was trying to Z_Free a string on the C
   //                    heap - see initializers above in MN_InitMenus

   int i;
   
   for(i=0; i<SAVESLOTS; i++)  // haleyjd
   {
      command_t *save_command;
      variable_t *save_variable;
      char tempstr[16];

      // create the variable first
      save_variable = (variable_t *)(Z_Malloc(sizeof(*save_variable), PU_STATIC, 0)); // haleyjd
      save_variable->variable = &savegamenames[i];
      save_variable->v_default = NULL;
      save_variable->type = vt_string;      // string value
      save_variable->min = 0;
      save_variable->max = SAVESTRINGSIZE;
      save_variable->defines = NULL;
      
      // now the command
      save_command = (command_t *)(Z_Malloc(sizeof(*save_command), PU_STATIC, 0)); // haleyjd
      
      sprintf(tempstr, "savegame_%i", i);
      save_command->name = estrdup(tempstr);
      save_command->type = ct_variable;
      save_command->flags = 0;
      save_command->variable = save_variable;
      save_command->handler = MN_SaveGame;
      save_command->netcmd = 0;
      
      (C_AddCommand)(save_command); // hook into cmdlist
   }
}


//
// MN_ReadSaveStrings
//  read the strings from the savegame files
// based on the mbf sources
//
void MN_ReadSaveStrings(void)
{
   int i;
   
   for(i = 0; i < SAVESLOTS; i++)
   {
      char *name = NULL;    // killough 3/22/98
      size_t len;
      char description[SAVESTRINGSIZE]; // sf
      FILE *fp;  // killough 11/98: change to use stdio

      len = M_StringAlloca(&name, 2, 26, basesavegame, savegamename);

      G_SaveGameName(name, len, i);

      // haleyjd: fraggle got rid of this - perhaps cause of the crash?
      //          I've re-implemented it below to try to resolve the
      //          zoneid check error -- bingo, along with new init code.
      // if(savegamenames[i])
      //  Z_Free(savegamenames[i]);

      fp = fopen(name,"rb");
      if(!fp)
      {   // Ty 03/27/98 - externalized:
         // haleyjd
         if(savegamenames[i])
            Z_Free(savegamenames[i]);
         savegamenames[i] = Z_Strdup(DEH_String("EMPTYSTRING"), PU_STATIC, 0);
         continue;
      }

      if(fread(description, SAVESTRINGSIZE, 1, fp) < 1)
         doom_printf("%s", FC_ERROR "Warning: savestring read failed");
      if(savegamenames[i])
         Z_Free(savegamenames[i]);
      savegamenames[i] = Z_Strdup(description, PU_STATIC, 0);  // haleyjd
      savegamepresent[i] = true;
      fclose(fp);
   }
}

void MN_DrawLoadBox(int x, int y)
{
   int i;
   
   patch_left  = PatchLoader::CacheName(wGlobalDir, "M_LSLEFT", PU_STATIC);
   patch_mid   = PatchLoader::CacheName(wGlobalDir, "M_LSCNTR", PU_STATIC);
   patch_right = PatchLoader::CacheName(wGlobalDir, "M_LSRGHT", PU_STATIC);

   V_DrawPatch(x, y, &vbscreen, patch_left);
   x += patch_left->width;
   
   for(i=0; i<24; i++)
   {
      V_DrawPatch(x, y, &vbscreen, patch_mid);
      x += patch_mid->width;
   }
   
   V_DrawPatch(x, y, &vbscreen, patch_right);

   // haleyjd: make purgable
   Z_ChangeTag(patch_left,  PU_CACHE);
   Z_ChangeTag(patch_mid,   PU_CACHE);
   Z_ChangeTag(patch_right, PU_CACHE);
}

void MN_LoadGameDrawer(void);

// haleyjd: all saveslot names changed to be consistent

static menuitem_t mn_loadgame_items[] =
{
   {it_title,  FC_GOLD "load game", NULL,         "M_LGTTL"},
   {it_gap},
   {it_runcmd, "save slot 0",       "mn_load 0"},
   {it_gap},
   {it_runcmd, "save slot 1",       "mn_load 1"},
   {it_gap},
   {it_runcmd, "save slot 2",       "mn_load 2"},
   {it_gap},
   {it_runcmd, "save slot 3",       "mn_load 3"},
   {it_gap},
   {it_runcmd, "save slot 4",       "mn_load 4"},
   {it_gap},
   {it_runcmd, "save slot 5",       "mn_load 5"},
   {it_gap},
   {it_runcmd, "save slot 6",       "mn_load 6"},
   {it_gap},
   {it_runcmd, "save slot 7",       "mn_load 7"},
   {it_end}
};

menu_t menu_loadgame =
{
   mn_loadgame_items,
   NULL, NULL, NULL,                 // pages
   50, 15,                           // x, y
   2,                                // starting slot
   mf_skullmenu | mf_leftaligned,    // skull menu
   MN_LoadGameDrawer,
};


void MN_LoadGameDrawer(void)
{
   static char *emptystr = NULL;
   int i, y;

   if(!emptystr)
      emptystr = estrdup(DEH_String("EMPTYSTRING"));
   
   for(i = 0, y = 46; i < SAVESLOTS; ++i, y += 16) // haleyjd
   {
      MN_DrawLoadBox(45, y);
   }
   
   // this is lame
   for(i = 0, y = 2; i < SAVESLOTS; ++i, y += 2)  // haleyjd
   {
      menu_loadgame.menuitems[y].description =
         savegamenames[i] ? savegamenames[i] : emptystr;
   }
}

CONSOLE_COMMAND(mn_loadgame, 0)
{
   if(netgame && !demoplayback)
   {
      MN_Alert("%s", DEH_String("LOADNET"));
      return;
   }
   
   // haleyjd 02/23/02: restored from MBF
   if(demorecording) // killough 5/26/98: exclude during demo recordings
   {
      MN_Alert("you can't load a game\n"
               "while recording a demo!\n\n"PRESSKEY);
      return;
   }
   
   MN_ReadSaveStrings();  // get savegame descriptions
   MN_StartMenu(GameModeInfo->loadMenu);
}

CONSOLE_COMMAND(mn_load, 0)
{
   char *name;     // killough 3/22/98
   int slot;
   size_t len;
   
   if(Console.argc < 1)
      return;
   
   slot = Console.argv[0]->toInt();
   
   // haleyjd 08/25/02: giant bug here
   if(!savegamepresent[slot])
   {
      MN_Alert("You can't load an empty game!\n%s", DEH_String("PRESSKEY"));
      return;     // empty slot
   }

   len = M_StringAlloca(&name, 2, 26, basesavegame, savegamename);
   
   G_SaveGameName(name, len, slot);
   G_LoadGame(name, slot, false);
   
   MN_ClearMenus();

   // haleyjd 10/08/08: GIF_SAVESOUND flag
   if(GameModeInfo->flags & GIF_SAVESOUND)
      S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_DEACTIVATE]);
}

// haleyjd 02/23/02: Quick Load -- restored from MBF and converted
// to use console commands
CONSOLE_COMMAND(quickload, 0)
{
   char tempstring[80];
   
   if(netgame && !demoplayback)
   {
      MN_Alert("%s", DEH_String("QLOADNET"));
      return;
   }

   if(demorecording)
   {
      MN_Alert("you can't quickload\n"
               "while recording a demo!\n\n"PRESSKEY);
      return;
   }

   if(quickSaveSlot < 0)
   {
      MN_Alert("%s", DEH_String("QSAVESPOT"));
      return;
   }
   
   psnprintf(tempstring, sizeof(tempstring), s_QLPROMPT, 
             savegamenames[quickSaveSlot]);
   MN_Question(tempstring, "qload");
}

CONSOLE_COMMAND(qload, cf_hidden)
{
   char *name = NULL;     // killough 3/22/98
   size_t len;

   len = M_StringAlloca(&name, 2, 26, basesavegame, savegamename);
   
   G_SaveGameName(name, len, quickSaveSlot);
   G_LoadGame(name, quickSaveSlot, false);
}

/////////////////////////////////////////////////////////////////
//
// Save Game
//

// haleyjd: fixes continue here from 8-17 build

void MN_SaveGameDrawer(void)
{
   int i, y;
   
   for(i = 0, y = 46; i < SAVESLOTS; i++, y += 16) // haleyjd
   {
      MN_DrawLoadBox(45, y);
   }
}

static menuitem_t mn_savegame_items[] =
{
   {it_title,  FC_GOLD "save game",           NULL,              "M_SGTTL"},
   {it_gap},
   {it_variable, "",                          "savegame_0"},
   {it_gap},
   {it_variable, "",                          "savegame_1"},
   {it_gap},
   {it_variable, "",                          "savegame_2"},
   {it_gap},
   {it_variable, "",                          "savegame_3"},
   {it_gap},
   {it_variable, "",                          "savegame_4"},
   {it_gap},
   {it_variable, "",                          "savegame_5"},
   {it_gap},
   {it_variable, "",                          "savegame_6"},
   {it_gap},
   {it_variable, "",                          "savegame_7"},
   {it_end}
};

menu_t menu_savegame = 
{
   mn_savegame_items,
   NULL, NULL, NULL,                 // pages
   50, 15,                           // x, y
   2,                                // starting slot
   mf_skullmenu | mf_leftaligned,    // skull menu
   MN_SaveGameDrawer,
};

CONSOLE_COMMAND(mn_savegame, 0)
{
   // haleyjd 02/23/02: restored from MBF
   // killough 10/6/98: allow savegames during single-player demo 
   // playback
   
   if(!usergame && (!demoplayback || netgame))
   {
      MN_Alert("%s", DEH_String("SAVEDEAD")); // Ty 03/27/98 - externalized
      return;
   }
   
   if(gamestate != GS_LEVEL)
      return;    // only save in levels
   
   MN_ReadSaveStrings();

   MN_StartMenu(GameModeInfo->saveMenu);
}

// haleyjd 02/23/02: Quick Save -- restored from MBF, converted to
// use console commands
CONSOLE_COMMAND(quicksave, 0)
{
   char tempstring[80];

   if(!usergame && (!demoplayback || netgame))  // killough 10/98
   {
      S_StartSound(NULL, GameModeInfo->playerSounds[sk_oof]);
      return;
   }
   
   if(gamestate != GS_LEVEL)
      return;
  
   if(quickSaveSlot < 0)
   {
      quickSaveSlot = -2; // means to pick a slot now
      MN_ReadSaveStrings();
      MN_StartMenu(GameModeInfo->saveMenu);
      return;
   }
   
   psnprintf(tempstring, sizeof(tempstring), s_QSPROMPT, 
             savegamenames[quickSaveSlot]);
   MN_Question(tempstring, "qsave");
}

CONSOLE_COMMAND(qsave, cf_hidden)
{
   G_SaveGame(quickSaveSlot, savegamenames[quickSaveSlot]);
}

/////////////////////////////////////////////////////////////////
//
// Options Menu
//
// Massively re-organised from the original version
//

extern menu_t menu_options;
extern menu_t menu_optionsp2;

static const char *mn_optionpg_names[] =
{
   "io / game options / widgets",
   "multiplayer / files / menus / info",
   NULL
};

static menu_t *mn_options_pages[] =
{
   &menu_options,
   &menu_optionsp2,
   NULL
};

static menuitem_t mn_options_items[] =
{
   {it_title,  FC_GOLD "options",       NULL,             "M_OPTION"},
   {it_gap},
   {it_info,   FC_GOLD "input/output"},
   {it_runcmd, "key bindings",          "mn_movekeys" },
   {it_runcmd, "mouse / gamepad",       "mn_mouse"    },
   {it_runcmd, "video options",         "mn_video"    },
   {it_runcmd, "sound options",         "mn_sound"    },
   {it_gap},
   {it_info,   FC_GOLD "game options"},
   {it_runcmd, "compatibility",         "mn_compat"   },
   {it_runcmd, "enemies",               "mn_enemies"  },
   {it_runcmd, "weapons",               "mn_weapons"  },
   {it_gap},
   {it_info,   FC_GOLD "game widgets"},
   {it_runcmd, "hud settings",          "mn_hud"      },
   {it_runcmd, "status bar",            "mn_status"   },
   {it_runcmd, "automap",               "mn_automap"  },
   {it_end}
};

static menuitem_t mn_optionsp2_items[] =
{
   {it_title,  FC_GOLD "options",        NULL,             "M_OPTION"},
   {it_gap},
   {it_info,   FC_GOLD "multiplayer"},
   {it_runcmd, "player setup",           "mn_player"  },
   {it_runcmd, "game settings",          "mn_gset"    },
   {it_gap},
   {it_info,   FC_GOLD "game files"},
   {it_runcmd, "wad options",            "mn_loadwad" },
   {it_runcmd, "demo settings",          "mn_demos"   },
   {it_runcmd, "configuration",          "mn_config"  },
   {it_gap},
   {it_info,   FC_GOLD "menus"},
   {it_runcmd, "menu options",           "mn_menus"   },
   {it_runcmd, "custom menu",            "mn_dynamenu _MN_Custom" },
   {it_gap},
   {it_info,   FC_GOLD "information"},
   {it_runcmd, "about eternity",         "credits"    },
   {it_end}
};

menu_t menu_options =
{
   mn_options_items,
   NULL, 
   &menu_optionsp2,                      // pages
   &menu_options,                        // rootpage
   100, 15,                              // x,y offsets
   3,                                    // starting item: first selectable
   mf_background|mf_centeraligned,       // draw background: not a skull menu
   NULL,                                 // no drawer
   mn_optionpg_names,                    // TOC stuff
   mn_options_pages
};

CONSOLE_COMMAND(mn_options, 0)
{
   MN_StartMenu(&menu_options);
}

//
// MN_InitCustomMenu
//
// Checks to see if the user has defined a _MN_Custom menu.
// If not, the "custom menu" item on the second page of the
// options menu will be disabled.
//
static void MN_InitCustomMenu(void)
{
   if(!MN_DynamicMenuForName("_MN_Custom"))
   {
      mn_optionsp2_items[13].type        = it_info;
      mn_optionsp2_items[13].description = FC_BRICK "custom menu";
   }
}

menu_t menu_optionsp2 =
{
   mn_optionsp2_items,
   &menu_options,
   NULL,
   &menu_options, // rootpage
   100, 15,
   3,
   mf_background|mf_centeraligned,
   NULL,
   mn_optionpg_names,
   mn_options_pages
};

CONSOLE_COMMAND(mn_endgame, 0)
{
   // haleyjd 04/18/03: restored netgame behavior from MBF
   if(netgame)
   {
      MN_Alert("%s", DEH_String("NETEND"));
      return;
   }

   MN_Question(DEH_String("ENDGAME"), "starttitle");
}

//=============================================================================
//
// Set video mode
//

// haleyjd 06/19/11: user's favorite aspect ratio
int mn_favaspectratio;

static const char *aspect_ratio_desc[] =
{
   "Legacy", "5:4", "4:3", "3:2", "16:10", "5:3", "WSVGA", "16:9"
};

VARIABLE_INT(mn_favaspectratio, NULL, 0, AR_NUMASPECTRATIOS-1, aspect_ratio_desc);
CONSOLE_VARIABLE(mn_favaspectratio, mn_favaspectratio, 0) {}

// haleyjd 06/19/11: user's favored fullscreen/window setting
int mn_favscreentype;

static const char *screen_type_desc[] = { "windowed", "fullscreen" };

VARIABLE_INT(mn_favscreentype, NULL, 0, MN_NUMSCREENTYPES-1, screen_type_desc);
CONSOLE_VARIABLE(mn_favscreentype, mn_favscreentype, 0) {}

// Video mode lists, per aspect ratio

// Legacy settings, w/aspect-corrected variants
static const char *legacyModes[] =
{
   "320x200",  // Mode 13h (16:10 logical, 4:3 physical)
   "320x240",  // QVGA
   "640x400",  // VESA Extension (same as 320x200)
   "640x480",  // VGA
   "960x600",  // x3
   "960x720",  // x3 with aspect ratio correction
   "1280x960", // x4 with aspect ratio correction  
   NULL
};

// 5:4 modes (1.25 / 0.8)
static const char *fiveFourModes[] =
{
   "320x256",   // NES resolution, IIRC
   "640x512",
   "800x640",
   "960x768",
   "1280x1024", // Most common 5:4 mode (name?)
   "1400x1120",
   "1600x1280",
   NULL
};

// 4:3 modes (1.333... / 0.75)
static const char *fourThreeModes[] =
{
   "768x576",   // PAL
   "800x600",   // SVGA
   "1024x768",  // XGA
   "1152x864",  // XGA+
   "1400x1050", // SXGA+
   "1600x1200", // UGA
   "2048x1536", // QXGA
   NULL
};

// 3:2 modes (1.5 / 0.666...)
static const char *threeTwoModes[] =
{
   "720x480",  // NTSC
   "768x512",
   "960x640",
   "1152x768",
   "1280x854",
   "1440x960",
   "1680x1120",
   NULL
};

// 16:10 modes (1.6 / 0.625)
static const char *sixteenTenModes[] =
{
   "1280x800",  // WXGA
   "1440x900",
   "1680x1050", // WSXGA+
   "1920x1200", // WUXGA
   "2560x1600", // WQXGA (This is the current max resolution)
   NULL
};

// 5:3 modes (1.666... / 0.6)
static const char *fiveThreeModes[] =
{
   "640x384",
   "800x480",  // WVGA
   "960x576",
   "1280x768", // WXGA
   "1400x840",
   "1600x960",
   "2560x1536",
   NULL
};

// "WSVGA" modes (128:75 or 16:9.375: 1.70666... / 0.5859375)
static const char *wsvgaModes[] =
{
   "1024x600", // WSVGA (common netbook resolution)
   NULL
};

// 16:9 modes (1.777... / 0.5625)
static const char *sixteenNineModes[] =
{
   "854x480",   // WVGA
   "1280x720",  // HD720
   "1360x768",  
   "1440x810",
   "1600x900",  // HD+
   "1920x1080", // HD1080
   "2048x1152",
   NULL
};

// FIXME/TODO: Not supported as menu choices yet:
// 17:9  (1.888... / 0.5294117647058823...) ex: 2048x1080 
// 32:15, or 16:7.5 (2.1333... / 0.46875)   ex: 1280x600
// These are not choices here because EE doesn't support them properly yet.
// Weapons will float above the status bar in these aspect ratios.

static const char **resListForAspectRatio[AR_NUMASPECTRATIOS] =
{
   legacyModes,      // Low-res 16:10, 4:3 modes and their multiples
   fiveFourModes,    // 5:4 - very square
   fourThreeModes,   // 4:3 (standard CRT)
   threeTwoModes,    // 3:2 (similar to European TV)
   sixteenTenModes,  // 16:10, common LCD widescreen monitors
   fiveThreeModes,   // 5:3 
   wsvgaModes,       // 128:75 (or 16:9.375), common netbook resolution
   sixteenNineModes  // 16:9, consumer HD widescreen TVs/monitors
};

static int mn_vidmode_num;
static char **mn_vidmode_desc;
static char **mn_vidmode_cmds;

//
// MN_BuildVidmodeTables
//
// haleyjd 06/19/11: Resurrected and restructured to allow choosing modes
// from the precomposited lists above based on the user's favorite aspect
// ratio and fullscreen/windowed settings.
//
static void MN_BuildVidmodeTables(void)
{
   int useraspect = mn_favaspectratio;
   int userfs     = mn_favscreentype;
   const char **reslist = NULL;
   int i = 0;
   int nummodes;
   qstring description;
   qstring cmd;

   if(mn_vidmode_desc)
   {
      for(i = 0; i < mn_vidmode_num; i++)
         efree(mn_vidmode_desc[i]);
      efree(mn_vidmode_desc);
      mn_vidmode_desc = NULL;
   }
   if(mn_vidmode_cmds)
   {
      for(i = 0; i < mn_vidmode_num; i++)
         efree(mn_vidmode_cmds[i]);
      efree(mn_vidmode_cmds);
      mn_vidmode_cmds = NULL;
   }

   // pick the list of resolutions to use
   if(useraspect >= 0 && useraspect < AR_NUMASPECTRATIOS)
      reslist = resListForAspectRatio[useraspect];
   else
      reslist = resListForAspectRatio[AR_LEGACY]; // A safe pick.

   // count the modes on that list
   while(reslist[i])
      ++i;

   nummodes = i;

   // allocate arrays
   mn_vidmode_desc = ecalloc(char **, i+1, sizeof(const char *));
   mn_vidmode_cmds = ecalloc(char **, i+1, sizeof(const char *));

   for(i = 0; i < nummodes; i++)
   {
      description = reslist[i];
      switch(userfs)
      {
      case MN_FULLSCREEN:
         description += 'f';
         break;
      case MN_WINDOWED:
      default:
         description += 'w';
         break;
      }

      // set the mode description
      mn_vidmode_desc[i] = description.duplicate(PU_STATIC);
      
      cmd  = "i_videomode ";
      cmd += description;
      
      mn_vidmode_cmds[i] = cmd.duplicate(PU_STATIC);
   }

   // null-terminate the lists
   mn_vidmode_desc[nummodes] = NULL;
   mn_vidmode_cmds[nummodes] = NULL;
}

CONSOLE_COMMAND(mn_vidmode, cf_hidden)
{
   MN_BuildVidmodeTables();

   MN_SetupBoxWidget("Choose a Video Mode", 
                     (const char **)mn_vidmode_desc, 1, NULL, 
                     (const char **)mn_vidmode_cmds);
   MN_ShowBoxWidget();
}

//=============================================================================
//
// Video Options
//

extern menu_t menu_video;
extern menu_t menu_sysvideo;
extern menu_t menu_video_pg2;
extern menu_t menu_particles;

static const char *mn_vidpage_names[] =
{
   "Mode / Rendering / Misc",
   "System / Console / Screenshots",
   "Screen Wipe",
   "Particles",
   NULL
};

static menu_t *mn_vidpage_menus[] =
{
   &menu_video,
   &menu_sysvideo,
   &menu_video_pg2,
   &menu_particles,
   NULL
};

void MN_VideoModeDrawer(void);

static menuitem_t mn_video_items[] =
{
   {it_title,        FC_GOLD "Video Options",           NULL, "m_video"},
   {it_gap},
   {it_info,         FC_GOLD "Mode"                                    },
   {it_runcmd,       "choose a mode...",        "mn_vidmode"           },
   {it_variable,     "video mode",              "i_videomode"          },
   {it_toggle,       "favorite aspect ratio",   "mn_favaspectratio"    },
   {it_toggle,       "favorite screen mode",    "mn_favscreentype"     },
   //{it_runcmd,       "make default video mode", "i_default_videomode"  },
   {it_toggle,       "vertical sync",           "v_retrace"            },
   {it_slider,       "gamma correction",        "gamma"                },   
   {it_gap},
   {it_info,         FC_GOLD "Rendering"                               },
   {it_slider,       "screen size",             "screensize"           },
   {it_toggle,       "hom detector flashes",    "r_homflash"           },
   {it_toggle,       "translucency",            "r_trans"              },
   {it_variable,     "translucency percentage", "r_tranpct"            },
   {it_end}
};

menu_t menu_video =
{
   mn_video_items,
   NULL,                 // prev page
   &menu_sysvideo,       // next page
   &menu_video,          // rootpage
   200, 15,              // x,y offset
   3,                    // start on first selectable
   mf_background,        // full-screen menu
   MN_VideoModeDrawer,
   mn_vidpage_names,
   mn_vidpage_menus
};

void MN_VideoModeDrawer(void)
{
   int lump, y;
   patch_t *patch;
   spritedef_t *sprdef;
   spriteframe_t *sprframe;
   int frame = E_SafeState(GameModeInfo->transFrame);

   // draw an imp fireball

   // don't draw anything before the menu has been initialized
   if(!(menu_video.menuitems[14].flags & MENUITEM_POSINIT))
      return;
   
   sprdef = &sprites[states[frame]->sprite];
   // haleyjd 08/15/02
   if(!(sprdef->spriteframes))
      return;
   sprframe = &sprdef->spriteframes[0];
   lump = sprframe->lump[0];
   
   patch = PatchLoader::CacheNum(wGlobalDir, lump + firstspritelump, PU_CACHE);
   
   // approximately center box on "translucency" item in menu
   y = menu_video.menuitems[13].y - 5;
   V_DrawBox(270, y, 20, 20);
   V_DrawPatchTL(282, y + 12, &vbscreen, patch, NULL, FTRANLEVEL);
}

CONSOLE_COMMAND(mn_video, 0)
{
   MN_StartMenu(&menu_video);
}

static menuitem_t mn_sysvideo_items[] =
{
   {it_title,    FC_GOLD "Video Options",   NULL, "m_video"},
   {it_gap},
   {it_info,     FC_GOLD "System"},
   {it_toggle,   "textmode startup",        "textmode_startup"},
#ifdef _SDL_VER
   {it_toggle,   "wait at exit",            "i_waitatexit"},
   {it_toggle,   "show endoom",             "i_showendoom"},
   {it_variable, "endoom delay",            "i_endoomdelay"},
#endif
   {it_gap},
   {it_info,     FC_GOLD "Console"},
   {it_variable, "console speed",           "c_speed"},
   {it_variable, "console height",          "c_height"},
   {it_gap},
   {it_info,     FC_GOLD "Screenshots"},
   {it_toggle,   "screenshot format",       "shot_type"},
   {it_toggle,   "gamma correct shots",     "shot_gamma"},
   {it_end}
};

menu_t menu_sysvideo =
{
   mn_sysvideo_items,
   &menu_video,          // prev page
   &menu_video_pg2,      // next page
   &menu_video,          // rootpage
   200, 15,              // x,y offset
   3,                    // start on first selectable
   mf_background,        // full-screen menu
   NULL,
   mn_vidpage_names,
   mn_vidpage_menus
};

static menuitem_t mn_video_page2_items[] =
{
   {it_title,   FC_GOLD "Video Options",    NULL, "m_video"},
   {it_gap},
   {it_info,    FC_GOLD "Screen Wipe"},
   {it_toggle,  "wipe type",                "wipetype"},
   {it_toggle,  "game waits",               "wipewait"},
   {it_gap},
   {it_info,    FC_GOLD "Misc."},
   {it_toggle,  "loading disk icon",       "v_diskicon"},
   {it_slider,  "damage screen intensity", "damage_screen_cap"},
   {it_end}
};

menu_t menu_video_pg2 =
{
   mn_video_page2_items,
   &menu_sysvideo,       // prev page
   &menu_particles,      // next page
   &menu_video,          // rootpage
   200, 15,              // x,y offset
   3,                    // start on first selectable
   mf_background,        // full-screen menu
   NULL,
   mn_vidpage_names,
   mn_vidpage_menus
};

static menuitem_t mn_particles_items[] =
{
   {it_title,  FC_GOLD "Video Options", NULL,              "m_video"},
   {it_gap},
   {it_info,         FC_GOLD "Particles"},
   {it_toggle, "render particle effects",  "draw_particles"},
   {it_toggle, "particle translucency",    "r_ptcltrans"},
   {it_gap},
   {it_info,         FC_GOLD "Effects"},
   {it_toggle, "blood splat type",         "bloodsplattype"},
   {it_toggle, "bullet puff type",         "bulletpufftype"},
   {it_toggle, "enable rocket trail",      "rocket_trails"},
   {it_toggle, "enable grenade trail",     "grenade_trails"},
   {it_toggle, "enable bfg cloud",         "bfg_cloud"},
   {it_toggle, "enable rocket explosion",  "pevt_rexpl"},
   {it_toggle, "enable bfg explosion",     "pevt_bfgexpl"},
   {it_end}
};

menu_t menu_particles =
{
   mn_particles_items,   // menu items
   &menu_video_pg2,      // previous page
   NULL,                 // next page
   &menu_video,          // rootpage
   200, 15,              // x,y offset
   3,                    // start on first selectable
   mf_background,        // full-screen menu
   NULL,
   mn_vidpage_names,
   mn_vidpage_menus
};


CONSOLE_COMMAND(mn_particle, 0)
{
   MN_StartMenu(&menu_particles);
}


/////////////////////////////////////////////////////////////////
//
// Sound Options
//

extern menu_t menu_sound;
extern menu_t menu_soundeq;

static const char *mn_sndpage_names[] =
{
   "Volume / Setup / Misc",
   "Equalizer",
   NULL
};

static menu_t *mn_sndpage_menus[] =
{
   &menu_sound,
   &menu_soundeq,
   NULL
};

static menuitem_t mn_sound_items[] =
{
   {it_title,      FC_GOLD "Sound Options",       NULL, "m_sound"},
   {it_gap},
   {it_info,       FC_GOLD "Volume"},
   {it_slider,     "Sfx volume",                   "sfx_volume"},
   {it_slider,     "Music volume",                 "music_volume"},
   {it_gap},
   {it_info,       FC_GOLD "Setup"},
   {it_toggle,     "Sound card",                   "snd_card"},
   {it_toggle,     "Music card",                   "mus_card"},
   {it_toggle,     "Sound channels",               "snd_channels"},
   {it_toggle,     "Force reverse stereo",         "s_flippan"},
   {it_gap},
   {it_info,       FC_GOLD "Misc"},
   {it_toggle,     "Precache sounds",              "s_precache"},
   {it_toggle,     "Pitched sounds",               "s_pitched"},
   {it_toggle,     "Announcer",                    "announcer_type"},
   {it_end}
};

menu_t menu_sound =
{
   mn_sound_items,
   NULL,                 // previous page
   &menu_soundeq,        // next page
   &menu_sound,          // root page
   180, 15,              // x, y offset
   3,                    // first selectable
   mf_background,        // full-screen menu
   NULL,
   mn_sndpage_names,
   mn_sndpage_menus
};

CONSOLE_COMMAND(mn_sound, 0)
{
   MN_StartMenu(&menu_sound);
}

static menuitem_t mn_soundeq_items[] =
{
   { it_title,      FC_GOLD "Sound Options",  NULL, "m_sound" },
   { it_gap },
   { it_info,       FC_GOLD "Equalizer"                       },
   { it_toggle,     "Enable equalizer",       "s_equalizer"   },
   { it_slider,     "Low band gain",          "s_lowgain"     },
   { it_slider,     "Midrange gain",          "s_midgain"     },
   { it_slider,     "High band gain",         "s_highgain"    },
   { it_slider,     "Preamp",                 "s_eqpreamp"    },
   { it_variable,   "Low pass cutoff",        "s_lowfreq"     },
   { it_variable,   "High pass cutoff",       "s_highfreq"    },
   { it_end }
};

menu_t menu_soundeq =
{
   mn_soundeq_items,
   &menu_sound,          // previous page
   NULL,                 // next page
   &menu_sound,          // root page
   180, 15,              // x, y offset
   3,                    // first selectable
   mf_background,        // full-screen menu
   NULL,
   mn_sndpage_names,
   mn_sndpage_menus
};

/////////////////////////////////////////////////////////////////
//
// Mouse & Joystick Options
//

static const char *mn_mousejoy_names[] =
{
   "mouse",
   "mouselook",
#ifdef _SDL_VER
   "joystick",
#endif
   NULL
};

extern menu_t menu_mouse;
extern menu_t menu_mouse_accel_and_mlook;
extern menu_t menu_joystick;

static menu_t *mn_mousejoy_pages[] =
{
   &menu_mouse,
   &menu_mouse_accel_and_mlook,
#ifdef _SDL_VER
   &menu_joystick,
#endif
   NULL
};

static menuitem_t mn_mouse_items[] =
{
   {it_title,      FC_GOLD "mouse settings",       NULL,   "m_mouse"},
   {it_gap},
   {it_toggle,     "enable mouse",                  "i_usemouse"},
   {it_gap},
   {it_info,       FC_GOLD "sensitivity"},
   {it_variable,   "horizontal",                    "sens_horiz"},
   {it_variable,   "vertical",                      "sens_vert"},
   {it_toggle,     "vanilla sensitivity",           "sens_vanilla"},
   {it_gap},
   {it_info,       FC_GOLD "misc."},
   {it_toggle,     "invert mouse",                  "invertmouse"},
   {it_toggle,     "smooth turning",                "smooth_turning"},
   {it_toggle,     "novert emulation",              "mouse_novert"},
#ifdef _SDL_VER
   {it_toggle,     "window grabs mouse",            "i_grabmouse"},
#endif
   {it_end}
};

menu_t menu_mouse =
{
   mn_mouse_items,               // menu items
   NULL,                         // previous page
   &menu_mouse_accel_and_mlook,  // next page
   &menu_mouse,                  // rootpage
   200, 15,                      // x, y offset
   2,                            // first selectable
   mf_background,                // full-screen menu
   NULL,                         // no drawer
   mn_mousejoy_names,            // TOC stuff
   mn_mousejoy_pages,
};

CONSOLE_COMMAND(mn_mouse, 0)
{
   MN_StartMenu(&menu_mouse);
}

static menuitem_t mn_mouse_accel_and_mlook_items[] =
{
   {it_title,      FC_GOLD "Mouse Settings",       NULL,   "m_mouse"},
   {it_gap},
   {it_info,       FC_GOLD"acceleration"},
   {it_toggle,     "type",                "mouse_accel_type"},
   {it_variable,   "custom threshold",    "mouse_accel_threshold"},
   {it_variable,   "custom amount",       "mouse_accel_value"},
   {it_gap},
   {it_info,       FC_GOLD"mouselook"},
   {it_toggle,     "enable mouselook",    "allowmlook" },
   {it_toggle,     "always mouselook",    "alwaysmlook"},
   {it_toggle,     "stretch short skies", "r_stretchsky"},
   {it_end}
};

menu_t menu_mouse_accel_and_mlook =
{
   mn_mouse_accel_and_mlook_items, // menu items
   &menu_mouse,                    // previous page
#ifdef _SDL_VER
   &menu_joystick,                 // next page
#else
   NULL,
#endif
   &menu_mouse,                    // rootpage
   200, 15,                        // x, y offset
   3,                              // first selectable
   mf_background,                  // full-screen menu
   NULL,                           // no drawer
   mn_mousejoy_names,              // TOC stuff
   mn_mousejoy_pages,
};

//------------------------------------------------------------------------
//
// SDL-specific joystick configuration menu
//

#ifdef _SDL_VER

static const char **mn_js_desc;
static const char **mn_js_cmds;

extern int numJoysticks;

static void MN_BuildJSTables(void)
{
   static bool menu_built = false;
   
   // don't build multiple times
   if(!menu_built)
   {
      int jsnum;
      char tempstr[20];

      // allocate arrays
      mn_js_desc = (const char **)(Z_Malloc((numJoysticks + 2) * sizeof(char *), 
                                            PU_STATIC, NULL));
      mn_js_cmds = (const char **)(Z_Malloc((numJoysticks + 2) * sizeof(char *),
                                            PU_STATIC, NULL));
      
      mn_js_desc[0] = "none";
      mn_js_cmds[0] = "i_joystick -1";
            
      for(jsnum = 0; jsnum < numJoysticks; ++jsnum)
      {
         mn_js_desc[jsnum+1] = joysticks[jsnum].description;
         sprintf(tempstr, "i_joystick %i", jsnum);
         mn_js_cmds[jsnum+1] = estrdup(tempstr);
      }
      mn_js_desc[numJoysticks + 1] = NULL;
      mn_js_cmds[numJoysticks + 1] = NULL;
          
      menu_built = true;
   }
}

CONSOLE_COMMAND(mn_joysticks, cf_hidden)
{
   const char *drv_name;
   static char title[256];

   MN_BuildJSTables();

   if(i_SDLJoystickNum != -1)
      drv_name = joysticks[i_SDLJoystickNum].description;
   else
      drv_name = "none";

   psnprintf(title, sizeof(title), 
             "choose a joystick\n\n  current device:\n  %s",
             drv_name);

   MN_SetupBoxWidget(title, mn_js_desc, 1, NULL, mn_js_cmds);
   MN_ShowBoxWidget();
}

static menuitem_t mn_joystick_items[] =
{
   {it_title,        FC_GOLD "joystick settings", NULL, "M_JOYSET" },
   {it_gap},
   {it_toggle,       "enable joystick",           "i_usejoystick"},
   {it_runcmd,       "select joystick...",        "mn_joysticks" },
   {it_gap},
   {it_info,         FC_GOLD "sensitivity"},
   {it_variable,     "horizontal",                "joySens_x" },
   {it_variable,     "vertical",                  "joySens_y" },
   {it_end}
};

menu_t menu_joystick =
{
   mn_joystick_items,
   &menu_mouse_accel_and_mlook,    // previous page
   NULL,                           // next page
   &menu_mouse,                    // rootpage
   200, 15,                        // x,y offset
   2,                              // start on first selectable
   mf_background,                  // full-screen menu
   NULL,                           // no drawer
   mn_mousejoy_names,              // TOC stuff
   mn_mousejoy_pages,
};

CONSOLE_COMMAND(mn_joymenu, 0)
{   
   MN_StartMenu(&menu_joystick);
}

#endif // _SDL_VER

/////////////////////////////////////////////////////////////////
//
// HUD Settings
//

static void MN_HUDPg2Drawer(void);

static const char *mn_hud_names[] =
{
   "messages",
   "BOOM HUD / crosshair / misc",
   "automap",
   NULL
};

extern menu_t menu_hud;
extern menu_t menu_hud_pg2;
extern menu_t menu_hud_pg3;

static menu_t *mn_hud_pages[] =
{
   &menu_hud,
   &menu_hud_pg2,
   &menu_hud_pg3,
   NULL
};

static menuitem_t mn_hud_items[] =
{
   {it_title,      FC_GOLD "hud settings",         NULL,      "m_hud"},
   {it_gap},
   {it_info,       FC_GOLD "message options"},
   {it_toggle,     "messages",                     "hu_messages"},
   {it_toggle,     "message colour",               "hu_messagecolor"},
   {it_toggle,     "messages scroll",              "hu_messagescroll"},
   {it_toggle,     "message lines",                "hu_messagelines"},
   {it_variable,   "message time (ms)",            "hu_messagetime"},
   {it_toggle,     "obituaries",                   "hu_obituaries"},
   {it_toggle,     "obituary colour",              "hu_obitcolor"},
   {it_toggle,     "center message color",         "hu_center_mess_color"},
   {it_toggle,     "enlarge center message",       "hu_center_mess_large"},
   {it_toggle,     "show spree messages",          "show_sprees"},
   {it_end}
};

static menuitem_t mn_hud_pg2_items[] =
{
   {it_title,      FC_GOLD "hud settings",         NULL,      "m_hud"},
   {it_gap},
   {it_info,       FC_GOLD "crosshair options"},
   {it_toggle,     "crosshair type",               "hu_crosshair"},
   {it_toggle,     "monster highlighting",         "hu_crosshair_hilite"},
   {it_toggle,     "display target names",         "display_target_names"},
   {it_gap},
   {it_info,       FC_GOLD "BOOM HUD options"},
   {it_toggle,     "display type",                 "hu_overlay"},
   {it_toggle,     "hide secrets",                 "hu_hidesecrets"},
   {it_gap},
   {it_info,       FC_GOLD "miscellaneous"},
   {it_toggle,     "show frags in deathmatch",     "show_scores"},
   {it_toggle,     "show timer",                   "show_timer"},
   {it_toggle,     "show netstats",                "show_netstats"},
   {it_end}
};

static menuitem_t mn_hud_pg3_items[] =
{
   {it_title,      FC_GOLD "hud settings",         NULL,      "m_hud"},
   {it_gap},
   {it_info,       FC_GOLD "automap options"},
   {it_toggle,     "show coords widget",           "hu_showcoords"},
   {it_toggle,     "coords follow pointer",        "map_coords"},
   {it_toggle,     "coords color",                 "hu_coordscolor"},
   {it_toggle,     "show level time widget",       "hu_showtime"},
   {it_toggle,     "level time color",             "hu_timecolor"},
   {it_toggle,     "level name color",             "hu_levelnamecolor"},
   {it_end}
};

menu_t menu_hud =
{
   mn_hud_items,
   NULL, 
   &menu_hud_pg2,         // next page
   &menu_hud,             // rootpage
   200, 15,               // x,y offset
   3,
   mf_background,
   NULL,                  // no drawer
   mn_hud_names,          // TOC stuff
   mn_hud_pages,
};

menu_t menu_hud_pg2 =
{
   mn_hud_pg2_items,
   &menu_hud,             // previous page
   &menu_hud_pg3,         // next page
   &menu_hud,             // rootpage
   200, 15,               // x,y offset
   3,
   mf_background,
   MN_HUDPg2Drawer,       // drawer
   mn_hud_names,          // TOC stuff
   mn_hud_pages,
};

menu_t menu_hud_pg3 =
{
   mn_hud_pg3_items,
   &menu_hud_pg2,         // previous page
   NULL,                  // next page
   &menu_hud,             // rootpage
   200, 15,               // x,y offset
   3,
   mf_background,
   NULL,                  // no drawer
   mn_hud_names,          // TOC stuff
   mn_hud_pages,
};

extern int crosshairs[CROSSHAIRS];

static void MN_HUDPg2Drawer(void)
{
   int y;
   patch_t *patch = NULL;
   int xhairnum = crosshairnum - 1;

   // draw the player's crosshair

   // don't draw anything before the menu has been initialized
   if(!(menu_hud_pg2.menuitems[3].flags & MENUITEM_POSINIT))
      return;

   if(xhairnum >= 0 && crosshairs[xhairnum] != -1)
      patch = PatchLoader::CacheNum(wGlobalDir, crosshairs[xhairnum], PU_CACHE);
  
   // approximately center box on "crosshair" item in menu
   y = menu_hud_pg2.menuitems[3].y - 5;
   V_DrawBox(270, y, 24, 24);

   if(patch)
   {
      int16_t w  = patch->width;
      int16_t h  = patch->height;
      int16_t to = patch->topoffset;
      int16_t lo = patch->leftoffset;

      V_DrawPatchTL(270 + 12 - (w >> 1) + lo, 
                    y + 12 - (h >> 1) + to, 
                    &vbscreen, patch, colrngs[CR_RED], FTRANLEVEL);
   }
}

CONSOLE_COMMAND(mn_hud, 0)
{
   MN_StartMenu(&menu_hud);
}


/////////////////////////////////////////////////////////////////
//
// Status Bar Settings
//

static menuitem_t mn_statusbar_items[] =
{
   {it_title,      FC_GOLD "status bar",           NULL,           "m_stat"},
   {it_gap},
   {it_toggle,     "numbers always red",           "st_rednum"},
   {it_toggle,     "percent sign grey",            "st_graypct"},
   {it_toggle,     "single key display",           "st_singlekey"},
   {it_gap},
   {it_info,       FC_GOLD "status bar colours"},
   {it_variable,   "ammo ok percentage",           "ammo_yellow"},
   {it_variable,   "ammo low percentage",          "ammo_red"},
   {it_gap},
   {it_variable,   "armour high percentage",       "armor_green"},
   {it_variable,   "armour ok percentage",         "armor_yellow"},
   {it_variable,   "armour low percentage",        "armor_red"},
   {it_gap},
   {it_variable,   "health high percentage",       "health_green"},
   {it_variable,   "health ok percentage",         "health_yellow"},
   {it_variable,   "health low percentage",        "health_red"},
   {it_end}
};

menu_t menu_statusbar =
{
   mn_statusbar_items,
   NULL, NULL, NULL,       // pages
   200, 15,
   2,
   mf_background,
};

CONSOLE_COMMAND(mn_status, 0)
{
   MN_StartMenu(&menu_statusbar);
}


/////////////////////////////////////////////////////////////////
//
// Automap colours
//

extern menu_t menu_automapcol1;
extern menu_t menu_automapcol2;
extern menu_t menu_automapcol3;
extern menu_t menu_automap4;

static const char *mn_automap_names[] =
{
   "background and lines",
   "floor and ceiling lines",
   "sprites",
   "portals",
   NULL
};

static menu_t *mn_automap_pages[] =
{
   &menu_automapcol1,
   &menu_automapcol2,
   &menu_automapcol3,
   &menu_automap4,
   NULL
};

static menuitem_t mn_automapcolbgl_items[] =
{
   {it_title,    FC_GOLD "automap",              NULL, "m_auto"},
   {it_gap},
   {it_info,     FC_GOLD "background and lines", NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_automap,  "background colour",            "mapcolor_back"},
   {it_automap,  "crosshair",                    "mapcolor_hair"},
   {it_automap,  "grid",                         "mapcolor_grid"},
   {it_automap,  "one-sided walls",              "mapcolor_wall"},
   {it_automap,  "teleporter",                   "mapcolor_tele"},
   {it_automap,  "secret area boundary",         "mapcolor_secr"},
   {it_automap,  "exit",                         "mapcolor_exit"},
   {it_automap,  "unseen line",                  "mapcolor_unsn"},
   {it_end}
};

static menuitem_t mn_automapcoldoor_items[] =
{
   {it_title,    FC_GOLD "automap",        NULL, "m_auto"},
   {it_gap},
   {it_info,     FC_GOLD "floor and ceiling lines", NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_automap,  "change in floor height",   "mapcolor_fchg"},
   {it_automap,  "change in ceiling height", "mapcolor_cchg"},
   {it_automap,  "floor and ceiling equal",  "mapcolor_clsd"},
   {it_automap,  "no change in height",      "mapcolor_flat"},
   {it_automap,  "red locked door",          "mapcolor_rdor"},
   {it_automap,  "yellow locked door",       "mapcolor_ydor"},
   {it_automap,  "blue locked door",         "mapcolor_bdor"},
   {it_end}
};

static menuitem_t mn_automapcolsprite_items[] =
{
   {it_title,    FC_GOLD "automap", NULL, "m_auto"},
   {it_gap},
   {it_info,     FC_GOLD "sprites", NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_automap,  "normal objects",  "mapcolor_sprt"},
   {it_automap,  "friends",         "mapcolor_frnd"},
   {it_automap,  "player arrow",    "mapcolor_sngl"},
   {it_automap,  "red key",         "mapcolor_rkey"},
   {it_automap,  "yellow key",      "mapcolor_ykey"},
   {it_automap,  "blue key",        "mapcolor_bkey"},
   {it_end}
};

static menuitem_t mn_automapportal_items[] =
{
   {it_title,    FC_GOLD "automap", NULL, "m_auto"},
   {it_gap},
   {it_info,     FC_GOLD "portals", NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_toggle,   "overlay linked portals", "mapportal_overlay"},
   {it_automap,  "overlay line color",     "mapcolor_prtl"},
   {it_end},
};

menu_t menu_automapcol1 = 
{
   mn_automapcolbgl_items,
   NULL,                   // previous page
   &menu_automapcol2,      // next page
   &menu_automapcol1,      // rootpage
   200, 15,                // x,y
   4,                      // starting item
   mf_background,          // fullscreen
   NULL,
   mn_automap_names,       // TOC stuff
   mn_automap_pages,
};

menu_t menu_automapcol2 = 
{
   mn_automapcoldoor_items,
   &menu_automapcol1,       // previous page
   &menu_automapcol3,       // next page
   &menu_automapcol1,       // rootpage
   200, 15,                 // x,y
   4,                       // starting item
   mf_background,           // fullscreen
   NULL,
   mn_automap_names,        // TOC stuff
   mn_automap_pages,
};

menu_t menu_automapcol3 = 
{
   mn_automapcolsprite_items,
   &menu_automapcol2,         // previous page
   &menu_automap4,            // next page
   &menu_automapcol1,         // rootpage
   200, 15,                   // x,y
   4,                         // starting item
   mf_background,             // fullscreen
   NULL,
   mn_automap_names,          // TOC stuff
   mn_automap_pages,
};

menu_t menu_automap4 = 
{
   mn_automapportal_items,
   &menu_automapcol3,         // previous page
   NULL,                      // next page
   &menu_automapcol1,         // rootpage
   200, 15,                   // x,y
   4,                         // starting item
   mf_background,             // fullscreen
   NULL,
   mn_automap_names,          // TOC stuff
   mn_automap_pages,
};

CONSOLE_COMMAND(mn_automap, 0)
{
   MN_StartMenu(&menu_automapcol1);
}


/////////////////////////////////////////////////////////////////
//
// Weapon Options
//

//
// NETCODE_FIXME -- WEAPONS_FIXME
//
// Weapon prefs and related values may need to change the way they
// work. See other notes for info about bfg type and autoaiming.
//

extern menu_t menu_weapons;
extern menu_t menu_weapons_pref;

static const char *mn_weapons_names[] =
{
   "options",
   "preferences",
   NULL
};

static menu_t *mn_weapons_pages[] =
{
   &menu_weapons,
   &menu_weapons_pref,
   NULL
};

/*
YSHEAR_FIXME: this feature may return after EDF for weapons
{it_toggle,   "allow mlook with bfg",       "bfglook"},
*/

static menuitem_t mn_weapons_items[] =
{
   {it_title,      FC_GOLD "weapons",                NULL, "M_WEAP"},
   {it_gap},
   {it_info,       FC_GOLD "options", NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_toggle,     "bfg type",                       "bfgtype"},
   {it_toggle,     "bobbing",                        "bobbing"},
   {it_toggle,     "recoil",                         "recoil"},
   {it_toggle,     "fist/ssg toggle",                "doom_weapon_toggles"},
   {it_toggle,     "autoaiming",                     "autoaim"},
   {it_variable,   "change time",                    "weapspeed"},
   {it_gap},
   {it_end},
};

menu_t menu_weapons =
{
   mn_weapons_items,
   NULL, 
   &menu_weapons_pref,                  // next page
   &menu_weapons,                       // rootpage
   200, 15,                             // x,y offset
   4,                                   // starting item
   mf_background,                       // full screen
   NULL,                                // no drawer
   mn_weapons_names,                    // TOC stuff
   mn_weapons_pages,
};

static menuitem_t mn_weapons_pref_items[] =
{
   {it_title,      FC_GOLD "weapons",       NULL, "M_WEAP"},
   {it_gap},
   {it_info, FC_GOLD "preferred weapon order", NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_variable, "1", "weappref_1"},
   {it_variable, "2", "weappref_2"},
   {it_variable, "3", "weappref_3"},
   {it_variable, "4", "weappref_4"},
   {it_variable, "5", "weappref_5"},
   {it_variable, "6", "weappref_6"},
   {it_variable, "7", "weappref_7"},
   {it_variable, "8", "weappref_8"},
   {it_variable, "9", "weappref_9"},
   {it_end},
};

menu_t menu_weapons_pref =
{
   mn_weapons_pref_items,         // items
   &menu_weapons,                 // previous page
   NULL,                          // next page
   &menu_weapons,                 // rootpage
   88, 15,                        // coords
   4,                             // first item
   mf_background|mf_leftaligned,  // flags
   NULL,                          // no drawer
   mn_weapons_names,              // TOC stuff
   mn_weapons_pages,
};

CONSOLE_COMMAND(mn_weapons, 0)
{
   MN_StartMenu(&menu_weapons);
}

/////////////////////////////////////////////////////////////////
//
// Compatibility vectors
//

extern menu_t menu_compat1;
extern menu_t menu_compat2;
extern menu_t menu_compat3;
extern menu_t menu_compat4;

// table of contents arrays

static const char *mn_compat_contents[] =
{
   "players / monster ai",
   "simulation",
   "maps",
   "explosions",
   NULL
};

static menu_t *mn_compat_pages[] =
{
   &menu_compat1,
   &menu_compat2,
   &menu_compat3,
   &menu_compat4,
   NULL
};


static menuitem_t mn_compat1_items[] =
{
   { it_title,  FC_GOLD "Compatibility", NULL, "m_compat" },
   { it_gap },   
   { it_info,   FC_GOLD "Players", NULL, NULL, MENUITEM_CENTERED        },
   { it_toggle, "God mode is not absolute",            "comp_god"       },
   { it_toggle, "Powerup cheats are time limited",     "comp_infcheat"  },
   { it_toggle, "Sky is normal when invulnerable",     "comp_skymap"    },
   { it_toggle, "Zombie players can exit levels",      "comp_zombie"    },
   { it_gap },
   { it_info,   FC_GOLD "Monster AI", NULL, NULL, MENUITEM_CENTERED     },
   { it_toggle, "Arch-viles can create ghosts",        "comp_vile"      },
   { it_toggle, "Pain elemental lost souls limited",   "comp_pain"      },
   { it_toggle, "Lost souls get stuck in walls",       "comp_skull"     },
   { it_toggle, "Lost souls never bounce on floors",   "comp_soul"      },
   { it_toggle, "Monsters randomly walk off lifts",    "comp_staylift"  },
   { it_toggle, "Monsters get stuck on door tracks",   "comp_doorstuck" },
   { it_toggle, "Monsters give up pursuit",            "comp_pursuit"   },
   { it_end }

};
 
static menuitem_t mn_compat2_items[] =
{ 
   { it_title,  FC_GOLD "Compatibility", NULL, "m_compat" },
   { it_gap },   
   { it_info,   FC_GOLD "Simulation",    NULL, NULL, MENUITEM_CENTERED   },
   { it_toggle, "Actors get stuck over dropoffs",      "comp_dropoff"    },
   { it_toggle, "Actors never fall off ledges",        "comp_falloff"    },
   { it_toggle, "Spawncubes telefrag on MAP30 only",   "comp_telefrag"   },
   { it_toggle, "Monsters can respawn outside map",    "comp_respawnfix" },
   { it_toggle, "Disable terrain types",               "comp_terrain"    },
   { it_toggle, "Disable falling damage",              "comp_fallingdmg" },
   { it_toggle, "Actors have infinite height",         "comp_overunder"  },
   { it_toggle, "Doom actor heights are inaccurate",   "comp_theights"   },
   { it_toggle, "Bullets never hit floors & ceilings", "comp_planeshoot" },
   { it_toggle, "Respawns are sometimes silent in DM", "comp_ninja"      },
   { it_toggle, "Use smaller mouselook range",         "comp_mouselook"  },
   { it_end }
};

static menuitem_t mn_compat3_items[] =
{
   { it_title,  FC_GOLD "Compatibility", NULL, "m_compat" },
   { it_gap },
   { it_info,   FC_GOLD "Maps",       NULL, NULL, MENUITEM_CENTERED      },
   { it_toggle, "Turbo doors make two closing sounds",  "comp_blazing"   },
   { it_toggle, "Disable tagged door light fading",     "comp_doorlight" },
   { it_toggle, "Use DOOM stairbuilding method",        "comp_stairs"    },
   { it_toggle, "Use DOOM floor motion behavior",       "comp_floors"    },
   { it_toggle, "Use DOOM linedef trigger model",       "comp_model"     },
   { it_toggle, "Line effects work on sector tag 0",    "comp_zerotags"  },
   { it_toggle, "One-time line effects can break",      "comp_special"   },
   { it_end }
};

static menuitem_t mn_compat4_items[] =
{
   { it_title,  FC_GOLD "Compatibility", NULL, "m_compat" },
   { it_gap },
   { it_info,     FC_GOLD "Explosions", NULL, NULL, MENUITEM_CENTERED         },
   { it_toggle,   "Explosions only thrust in 2D",  "comp_2dradatk"            },
   { it_variable, "Explosion damage factor",      "radial_attack_damage"      },
   { it_variable, "Explosion self-damage factor", "radial_attack_self_damage" },
   { it_variable, "Explosion lift factor",        "radial_attack_lift"        },
   { it_variable, "Explosion self-lift factor",   "radial_attack_self_lift"   },
   { it_end }
};

menu_t menu_compat1 =
{
   mn_compat1_items,    // items
   NULL, &menu_compat2, // pages
   &menu_compat1,       // rootpage
   270, 5,              // x,y
   3,                   // starting item
   mf_background,       // full screen
   NULL,                // no drawer
   mn_compat_contents,  // TOC arrays
   mn_compat_pages,
};

menu_t menu_compat2 =
{
   mn_compat2_items,    // items
   &menu_compat1,       // prev page
   &menu_compat3,       // next page
   &menu_compat1,       // rootpage
   270, 5,              // x,y
   3,                   // starting item
   mf_background,       // full screen
   NULL,                // no drawer
   mn_compat_contents,  // TOC arrays
   mn_compat_pages,
};

menu_t menu_compat3 =
{
   mn_compat3_items,    // items
   &menu_compat2,       // prev page
   &menu_compat4,       // next page
   &menu_compat1,       // rootpage
   270, 5,              // x, y
   3,                   // starting item
   mf_background,       // full screen
   NULL,                // no drawer
   mn_compat_contents,  // TOC arrays
   mn_compat_pages,
};

menu_t menu_compat4 =
{
   mn_compat4_items,    // items
   &menu_compat3, NULL, // pages
   &menu_compat1,       // rootpage
   200, 5,              // x, y
   3,                   // starting item
   mf_background,       // full screen
   NULL,                // no drawer
   mn_compat_contents,  // TOC arrays
   mn_compat_pages,
};

CONSOLE_COMMAND(mn_compat, 0)
{
   MN_StartMenu(&menu_compat1);
}

/////////////////////////////////////////////////////////////////
//
// Enemies
//

static menuitem_t mn_enemies_items[] =
{
   {it_title,      FC_GOLD "enemies",              NULL,      "m_enem"},
   {it_gap},
   {it_info,       FC_GOLD "monster options"},
   {it_toggle,     "monsters remember target",     "mon_remember"},
   {it_toggle,     "monster infighting",           "mon_infight"},
   {it_toggle,     "monsters back out",            "mon_backing"},
   {it_toggle,     "monsters avoid hazards",       "mon_avoid"},
   {it_toggle,     "affected by friction",         "mon_friction"},
   {it_toggle,     "climb tall stairs",            "mon_climb"},
   {it_gap},
   {it_info,       FC_GOLD "mbf friend options"},
   {it_variable,   "friend distance",              "mon_distfriend"},
   {it_toggle,     "rescue dying friends",         "mon_helpfriends"},
   {it_variable,   "number of helper dogs",        "numhelpers"},
   {it_toggle,     "dogs can jump down",           "dogjumping"},
   {it_end}
};

menu_t menu_enemies =
{
   mn_enemies_items,
   NULL, NULL, NULL,      // pages
   220,15,                // x,y offset
   3,                     // starting item
   mf_background          // full screen
};

CONSOLE_COMMAND(mn_enemies, 0)
{
   MN_StartMenu(&menu_enemies);
}

//------------------------------------------------------------------------
//
// haleyjd 10/15/05: table of contents for key bindings menu pages
//

static const char *mn_binding_contentnames[] =
{
   "basic movement",
   "advanced movement",
   "weapon keys",
   "environment",
   "game functions",
   "menu keys",
   "automap keys",
   "console keys",
   "client/server keys",
   "client/server demo keys",
   NULL
};

// all in this file, but require a forward prototype
extern menu_t menu_movekeys;
extern menu_t menu_advkeys;
extern menu_t menu_weaponbindings;
extern menu_t menu_envbindings;
extern menu_t menu_funcbindings;
extern menu_t menu_menukeys;
extern menu_t menu_automapkeys;
extern menu_t menu_consolekeys;
extern menu_t menu_clientserverkeys;
extern menu_t menu_clientserverdemokeys;

static menu_t *mn_binding_contentpages[] =
{
   &menu_movekeys,
   &menu_advkeys,
   &menu_weaponbindings,
   &menu_envbindings,
   &menu_funcbindings,
   &menu_menukeys,
   &menu_automapkeys,
   &menu_consolekeys,
   &menu_clientserverkeys,
   &menu_clientserverdemokeys,
   NULL
};

//------------------------------------------------------------------------
//
// Key Bindings: basic movement keys
//

static menuitem_t mn_movekeys_items[] =
{
   {it_title, FC_GOLD "key bindings",   NULL, "M_KEYBND"},
   {it_gap},
   {it_info,  FC_GOLD "basic movement", NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_binding,      "move forward",    "forward"},
   {it_binding,      "move backward",   "backward"},
   {it_binding,      "run",             "speed"},
   {it_binding,      "turn left",       "left"},
   {it_binding,      "turn right",      "right"},
   {it_binding,      "strafe on",       "strafe"},
   {it_binding,      "strafe left",     "moveleft"},
   {it_binding,      "strafe right",    "moveright"},
   {it_binding,      "180 degree turn", "flip"},
   {it_binding,      "use",             "use"},
   {it_binding,      "attack/fire",     "attack"},
   {it_binding,      "toggle autorun",  "autorun"},
   {it_end}
};

static menuitem_t mn_advkeys_items[] =
{
   {it_title, FC_GOLD "key bindings",      NULL, "M_KEYBND"},
   {it_gap},
   {it_info,  FC_GOLD "advanced movement", NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_binding,      "jump",            "jump"},
   {it_gap},
   {it_binding,      "mlook on",        "mlook"},
   {it_binding,      "look up",         "lookup"},
   {it_binding,      "look down",       "lookdown"},
   {it_binding,      "center view",     "center"},
   {it_end}
};

menu_t menu_movekeys =
{
   mn_movekeys_items,
   NULL,                    // previous page
   &menu_advkeys,           // next page
   &menu_movekeys,          // rootpage
   150, 15,                 // x,y offsets
   4,
   mf_background,           // draw background: not a skull menu
   NULL,                    // no drawer
   mn_binding_contentnames, // table of contents arrays
   mn_binding_contentpages,
};

menu_t menu_advkeys =
{
   mn_advkeys_items,
   &menu_movekeys,          // previous page
   &menu_weaponbindings,    // next page
   &menu_movekeys,          // rootpage
   150, 15,                 // x,y offsets
   4,
   mf_background,           // draw background: not a skull menu
   NULL,                    // no drawer
   mn_binding_contentnames, // table of contents arrays
   mn_binding_contentpages,
};

CONSOLE_COMMAND(mn_movekeys, 0)
{
   MN_StartMenu(&menu_movekeys);
}

CONSOLE_COMMAND(mn_advkeys, 0)
{
   MN_StartMenu(&menu_advkeys);
}

//------------------------------------------------------------------------
//
// Key Bindings: weapon keys
//

static menuitem_t mn_weaponbindings_items[] =
{
   {it_title,   FC_GOLD "key bindings", NULL, "M_KEYBND"},
   {it_gap},
   {it_info,    FC_GOLD "weapon keys",  NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_binding, "weapon 1",             "weapon1"},
   {it_binding, "weapon 2",             "weapon2"},
   {it_binding, "weapon 3",             "weapon3"},
   {it_binding, "weapon 4",             "weapon4"},
   {it_binding, "weapon 5",             "weapon5"},
   {it_binding, "weapon 6",             "weapon6"},
   {it_binding, "weapon 7",             "weapon7"},
   {it_binding, "weapon 8",             "weapon8"},
   {it_binding, "weapon 9",             "weapon9"},
   {it_binding, "next best weapon",     "nextweapon"},
   {it_binding, "next weapon",          "weaponup"},
   {it_binding, "previous weapon",      "weapondown"},
   {it_end}
};

menu_t menu_weaponbindings =
{
   mn_weaponbindings_items,
   &menu_advkeys,           // previous page
   &menu_envbindings,       // next page
   &menu_movekeys,          // rootpage
   150, 15,                 // x,y offsets
   4,
   mf_background,           // draw background: not a skull menu
   NULL,                    // no drawer
   mn_binding_contentnames, // table of contents arrays
   mn_binding_contentpages,
};

CONSOLE_COMMAND(mn_weaponkeys, 0)
{
   MN_StartMenu(&menu_weaponbindings);
}

//------------------------------------------------------------------------
//
// Key Bindings: Environment
//

// haleyjd 06/24/02: added quicksave/load

static menuitem_t mn_envbindings_items[] =
{
   {it_title,   FC_GOLD "key bindings", NULL, "M_KEYBND"},
   {it_gap},
   {it_info,    FC_GOLD "environment",  NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_binding, "screen size up",       "screensize +"},
   {it_binding, "screen size down",     "screensize -"},
   {it_binding, "take screenshot",      "screenshot"},
   {it_binding, "spectate prev",        "spectate_prev"},
   {it_binding, "spectate next",        "spectate_next"},
   {it_binding, "spectate self",        "spectate_self"},
   {it_end}
};

menu_t menu_envbindings =
{
   mn_envbindings_items,
   &menu_weaponbindings,    // previous page
   &menu_funcbindings,      // next page
   &menu_movekeys,          // rootpage
   150, 15,                 // x,y offsets
   4,
   mf_background,           // draw background: not a skull menu
   NULL,                    // no drawer
   mn_binding_contentnames, // table of contents arrays
   mn_binding_contentpages,
};

CONSOLE_COMMAND(mn_envkeys, 0)
{
   MN_StartMenu(&menu_envbindings);
}

//------------------------------------------------------------------------
//
// Key Bindings: Game Function Keys
//

static menuitem_t mn_function_items[] =
{
   {it_title,   FC_GOLD "key bindings", NULL, "M_KEYBND"},
   {it_gap},
   {it_info,    FC_GOLD "game functions",  NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_binding, "save game",            "mn_savegame"}, 
   {it_binding, "load game",            "mn_loadgame"},
   {it_binding, "volume",               "mn_sound"},
   {it_binding, "toggle hud",           "hu_overlay /"},
   {it_binding, "quick save",           "quicksave"},
   {it_binding, "end game",             "mn_endgame"},
   {it_binding, "toggle messages",      "hu_messages /"},
   {it_binding, "quick load",           "quickload"},
   {it_binding, "quit",                 "mn_quit"},
   {it_binding, "gamma correction",     "gamma /"},
   {it_end}
};

menu_t menu_funcbindings =
{
   mn_function_items,
   &menu_envbindings,       // previous page
   &menu_menukeys,          // next page
   &menu_movekeys,          // rootpage
   150, 15,                 // x,y offsets
   4,
   mf_background,           // draw background: not a skull menu
   NULL,                    // no drawer
   mn_binding_contentnames, // table of contents arrays
   mn_binding_contentpages,
};

CONSOLE_COMMAND(mn_gamefuncs, 0)
{
   MN_StartMenu(&menu_funcbindings);
}

//------------------------------------------------------------------------
//
// Key Bindings: Menu Keys
//

static menuitem_t mn_menukeys_items[] =
{
   {it_title,   FC_GOLD "key bindings", NULL, "M_KEYBND"},
   {it_gap},
   {it_info,    FC_GOLD "menu keys", NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_binding, "toggle menus",         "menu_toggle"},
   {it_binding, "previous menu",        "menu_previous"},
   {it_binding, "next item",            "menu_down"},
   {it_binding, "previous item",        "menu_up"},
   {it_binding, "next value",           "menu_right"},
   {it_binding, "previous value",       "menu_left"},
   {it_binding, "confirm",              "menu_confirm"},
   {it_binding, "display help",         "menu_help"},
   {it_binding, "display setup",        "menu_setup"},
   {it_binding, "previous page",        "menu_pageup"},
   {it_binding, "next page",            "menu_pagedown"},
   {it_binding, "show contents",        "menu_contents"},
   {it_end}
};

menu_t menu_menukeys =
{
   mn_menukeys_items,
   &menu_funcbindings,      // previous page
   &menu_automapkeys,       // next page
   &menu_movekeys,          // rootpage
   150, 15,                 // x,y offsets
   4,
   mf_background,           // draw background: not a skull menu
   NULL,                    // no drawer
   mn_binding_contentnames, // table of contents arrays
   mn_binding_contentpages,
};

CONSOLE_COMMAND(mn_menukeys, 0)
{
   MN_StartMenu(&menu_menukeys);
}

//------------------------------------------------------------------------
//
// Key Bindings: Automap Keys
//

static menuitem_t mn_automapkeys_items[] =
{
   {it_title,   FC_GOLD "key bindings", NULL, "M_KEYBND"},
   {it_gap},
   {it_info,    FC_GOLD "automap",      NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_binding, "toggle map",           "map_toggle"},
   {it_binding, "move up",              "map_up"},
   {it_binding, "move down",            "map_down"},
   {it_binding, "move left",            "map_left"},
   {it_binding, "move right",           "map_right"},
   {it_binding, "zoom in",              "map_zoomin"},
   {it_binding, "zoom out",             "map_zoomout"},
   {it_binding, "show full map",        "map_gobig"},
   {it_binding, "follow mode",          "map_follow"},
   {it_binding, "mark spot",            "map_mark"},
   {it_binding, "clear spots",          "map_clear"},
   {it_binding, "show grid",            "map_grid"},
   {it_end}
};

menu_t menu_automapkeys =
{
   mn_automapkeys_items,
   &menu_menukeys,          // previous page
   &menu_consolekeys,       // next page
   &menu_movekeys,          // rootpage
   150, 15,                 // x,y offsets
   4,
   mf_background,           // draw background: not a skull menu
   NULL,                    // no drawer
   mn_binding_contentnames, // table of contents arrays
   mn_binding_contentpages,
};

CONSOLE_COMMAND(mn_automapkeys, 0)
{
   MN_StartMenu(&menu_automapkeys);
}

//------------------------------------------------------------------------
//
// Key Bindings: Console Keys
//

static menuitem_t mn_consolekeys_items[] =
{
   {it_title,   FC_GOLD "key bindings", NULL, "M_KEYBND"},
   {it_gap},
   {it_info,    FC_GOLD "console",      NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_binding, "toggle",               "console_toggle"},
   {it_binding, "enter command",        "console_enter"},
   {it_binding, "complete command",     "console_tab"},
   {it_binding, "previous command",     "console_up"},
   {it_binding, "next command",         "console_down"},
   {it_binding, "backspace",            "console_backspace"},
   {it_binding, "page up",              "console_pageup"},
   {it_binding, "page down",            "console_pagedown"},
   {it_end}
};

menu_t menu_consolekeys =
{
   mn_consolekeys_items,
   &menu_automapkeys,       // previous page
   &menu_clientserverkeys,  // next page
   &menu_movekeys,          // rootpage
   150, 15,                 // x,y offsets
   4,
   mf_background,           // draw background: not a skull menu
   NULL,                    // no drawer
   mn_binding_contentnames, // table of contents arrays
   mn_binding_contentpages,
};

CONSOLE_COMMAND(mn_consolekeys, 0)
{
   MN_StartMenu(&menu_consolekeys);
}

//------------------------------------------------------------------------
//
// Key Bindings: Client/Server Keys
//

static menuitem_t mn_clientserverkeys_items[] =
{
   {it_title,   FC_GOLD "key bindings", NULL, "M_KEYBND"},
   {it_gap},
   {it_info,    FC_GOLD "client/server", NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_binding, "message all",          "message_all"},
   {it_binding, "message team",         "message_team"},
   {it_binding, "message player",       "message_player"},
   {it_binding, "message server",       "message_server"},
   {it_binding, "rcon",                 "message_rcon"},
   {it_binding, "show scoreboard",      "scoreboard"},
   {it_binding, "vote yes",             "vote_yes"},
   {it_binding, "vote no",              "vote_no"},
   {it_end}
};

menu_t menu_clientserverkeys =
{
   mn_clientserverkeys_items,
   &menu_consolekeys,          // previous page
   &menu_clientserverdemokeys, // next page
   &menu_movekeys,             // rootpage
   150, 15,                    // x,y offsets
   4,
   mf_background,              // draw background: not a skull menu
   NULL,                       // no drawer
   mn_binding_contentnames,    // table of contents arrays
   mn_binding_contentpages,
};

CONSOLE_COMMAND(mn_clientserverkeys, 0)
{
    MN_StartMenu(&menu_clientserverkeys);
}

//------------------------------------------------------------------------
//
// Key Bindings: Client/Server Demo Keys
//

static menuitem_t mn_clientserverdemokeys_items[] =
{
   {it_title,   FC_GOLD "key bindings", NULL, "M_KEYBND"},
   {it_gap},
   {it_info,    FC_GOLD "client/server demos", NULL, NULL, MENUITEM_CENTERED},
   {it_gap},
   {it_binding, "start recording",          "csdemorecord"            },
   {it_binding, "stop playing/recording",   "csdemostop"              },
   {it_binding, "pause/resume playback",    "csdemopauseresume"       },
   {it_binding, "speed up",                 "csdemospeedup"           },
   {it_binding, "slow down",                "csdemoslowdown"          },
   {it_binding, "save checkpoint",          "csdemosavecheckpoint"    },
   {it_binding, "load previous checkpoint", "csdemopreviouscheckpoint"},
   {it_binding, "load next checkpoint",     "csdemonextcheckpoint"    },
   {it_binding, "rewind 15 seconds",        "csdemoback15"            },
   {it_binding, "rewind 30 seconds",        "csdemoback30"            },
   {it_binding, "rewind 60 seconds",        "csdemoback60"            },
   {it_binding, "forward 15 seconds",       "csdemoforward15"         },
   {it_binding, "forward 30 seconds",       "csdemoforward30"         },
   {it_binding, "forward 60 seconds",       "csdemoforward60"         },
   {it_end}
};

menu_t menu_clientserverdemokeys =
{
   mn_clientserverdemokeys_items,
   &menu_clientserverkeys,  // previous page
   NULL,                    // next page
   &menu_movekeys,          // rootpage
   170, 15,                 // x,y offsets
   4,
   mf_background,           // draw background: not a skull menu
   NULL,                    // no drawer
   mn_binding_contentnames, // table of contents arrays
   mn_binding_contentpages,
};

CONSOLE_COMMAND(mn_clientserverdemokeys, 0)
{
    MN_StartMenu(&menu_clientserverdemokeys);
}

//----------------------------------------------------------------------------
//
// "Menus" menu -- for menu options related to the menus themselves :)
//
// haleyjd 03/14/06
//

static char *mn_searchstr;
static menuitem_t *lastMatch;


VARIABLE_STRING(mn_searchstr, NULL, 32);
CONSOLE_VARIABLE(mn_searchstr, mn_searchstr, 0)
{
   // any change to mn_searchstr resets the search index
   lastMatch = NULL;
}

static void MN_InitSearchStr(void)
{
   if(mn_searchstr)
       efree(mn_searchstr);

   mn_searchstr = estrdup("");
}

// haleyjd: searchable menus
extern menu_t menu_movekeys;
extern menu_t menu_mouse;
extern menu_t menu_video;
extern menu_t menu_sound;
extern menu_t menu_compat1;
extern menu_t menu_enemies;
extern menu_t menu_weapons;
extern menu_t menu_hud;
extern menu_t menu_statusbar;
extern menu_t menu_automapcol1;
extern menu_t menu_player;
extern menu_t menu_gamesettings;
extern menu_t menu_loadwad;
extern menu_t menu_demos;

static menu_t *mn_search_menus[] =
{
   &menu_movekeys,
   &menu_mouse,
   &menu_video,
   &menu_sound,
   &menu_compat1,
   &menu_enemies,
   &menu_weapons,
   &menu_hud,
   &menu_statusbar,
   &menu_automapcol1,
   &menu_player,
   &menu_gamesettings,
   &menu_loadwad,
   &menu_demos,
   NULL
};

CONSOLE_COMMAND(mn_search, 0)
{
   int i = 0;
   menu_t *curMenu;
   bool pastLast;

   // if lastMatch is set, set pastLast to false so that we'll seek
   // forward until we pass the last item that was matched
   pastLast = !lastMatch;

   if(!mn_searchstr || !mn_searchstr[0])
   {
      MN_ErrorMsg("invalid search string");
      return;
   }

   M_Strlwr(mn_searchstr);

   // run through every menu in the search list...
   while((curMenu = mn_search_menus[i++]))
   {
      menu_t *curPage = curMenu;

      // run through every page of the menu
      while(curPage)
      {
         int j = 0;
         menuitem_t *item; 

         // run through items
         while((item = &(curPage->menuitems[j++])))
         {
            char *desc;

            if(item->type == it_end)
               break;

            if(!item->description)
               continue;

            // keep seeking until we pass the last item found
            if(item == lastMatch)
            {
               pastLast = true;
               continue;
            }            
            if(!pastLast)
               continue;

            desc = M_Strlwr(estrdup(item->description));

            // found a match
            if(strstr(desc, mn_searchstr))
            {
               // go to it
               lastMatch = item;
               MN_StartMenu(curPage);
               MN_ErrorMsg("found: %s", desc);
               if(!is_a_gap(item))
                  curPage->selected = j - 1;
               efree(desc);
               return;
            }
            efree(desc);
         }

         curPage = curPage->nextpage;
      }
   }

   if(lastMatch) // if doing a valid search, reset it now
   {
      lastMatch = NULL;
      MN_ErrorMsg("reached end of search");
   }
   else
      MN_ErrorMsg("no match found for '%s'", mn_searchstr);
}

static menuitem_t mn_menus_items[] =
{
   {it_title,    FC_GOLD "menu options",   NULL, "M_MENUS"},
   {it_gap},
   {it_info,     FC_GOLD "general"},
   {it_toggle,   "toggle key backs up",    "mn_toggleisback"},
   {it_runcmd,   "set background...",      "mn_selectflat"},
   {it_gap},
   {it_info,     FC_GOLD "compatibility"},
   {it_toggle,   "use doom's main menu",   "use_traditional_menu"},
   {it_toggle,   "emulate all old menus",  "mn_classic_menus"},
   {it_gap},
   {it_info,     FC_GOLD "utilities"},
   {it_variable, "search string:",         "mn_searchstr"},
   {it_runcmd,   "search in menus...",     "mn_search"},
   {it_end},
};

menu_t menu_menuopts =
{
   mn_menus_items,
   NULL,                    // prev page
   NULL,                    // next page
   NULL,                    // root page
   200, 15,                 // x,y offsets
   3,                       // first item
   mf_background,           // draw background: not a skull menu
   NULL,                    // no drawer
};

CONSOLE_COMMAND(mn_menus, 0)
{
   MN_StartMenu(&menu_menuopts);
}

//
// Config menu - configuring the configuration files O_o
//

static menuitem_t mn_config_items[] =
{
   {it_title,    FC_GOLD "Configuration" },
   {it_gap},
   {it_info,     FC_GOLD "Doom Game Modes"},
   {it_toggle,   "use base/doom config",     "use_doom_config" },
   {it_end}
};

menu_t menu_config =
{
   mn_config_items,
   NULL,
   NULL,
   NULL,
   200, 15,
   3,
   mf_background,
   NULL,
};

CONSOLE_COMMAND(mn_config, 0)
{
   MN_StartMenu(&menu_config);
}

//
// Skin Viewer Command
//

extern void MN_InitSkinViewer(void);

CONSOLE_COMMAND(skinviewer, 0)
{
   MN_InitSkinViewer();
}

//==============================================================================
//
// DOOM Traditional Menu System
//
// haleyjd 08/29/06: On August 19 of this year, the DOOM community lost one of
// its well-known members, Dylan James McIntosh, AKA "Toke," a purist and
// old-school deathmatcher by trade and creator of a series of popular DM maps
// whose style was decidedly minimalistic. We met once in real life to discuss a
// business deal involving programming, which unfortunately never took off. For
// personal reasons as well as to represent the community, I attended his
// funeral the following Friday afternoon in Tulsa and met his parents there. I
// considered him a friend, and I have experienced a lot of pain due to his
// death.
//
// A description of his services and the messages from his many online friends
// can be viewed at this Doomworld Forums thread:
// http://www.doomworld.com/vb/showthread.php?s=&threadid=37900
//
// He also helped out with several source ports, including ZDaemon, Chocolate
// DOOM, and yes, Eternity. He didn't do coding, but rather helped by pointing
// out bugs and compatibility issues. Not very many people do this, and I have
// always appreciated those who do. He was also a champion for universal
// multiplayer standards.
//
// This feature I had planned for quite some time, but never seemed to find the
// drive to work on it. Because Toke appreciated everything old-school, I 
// thought it would be good to introduce this feature in the 3.33.50 version 
// which is dedicated to his memory. This is a complete emulation of DOOM's 
// original, minimal menu system without any of the fluff or access to new 
// features that the normal menus provide.
//
// To Toke: We still miss ya, buddy, but I know you're somewhere having fun and
// maybe having a laugh at our expense ;)  Keep DOOMing forever, my friend.
//

#define EMULATED_ITEM_SIZE 16

// haleyjd 05/16/04: traditional menu support
VARIABLE_BOOLEAN(traditional_menu, NULL, yesno);
CONSOLE_VARIABLE(use_traditional_menu, traditional_menu, 0) {}

// haleyjd 05/16/04: traditional DOOM main menu support

// Note: the main menu emulation can still be enabled separately from the
// rest of the purist menu system. It is modified dynamically to point to
// the rest of the old menu system when appropriate.

static menuitem_t mn_old_main_items[] =
{
   { it_runcmd, "new game",   "mn_newgame",  "M_NGAME"  },
   { it_runcmd, "options",    "mn_options",  "M_OPTION" },
   { it_runcmd, "save game",  "mn_savegame", "M_SAVEG"  },
   { it_runcmd, "load game",  "mn_loadgame", "M_LOADG"  },
   { it_runcmd, "read this!", "credits",     "M_RDTHIS" },
   { it_runcmd, "quit",       "mn_quit",     "M_QUITG"  },
   { it_end }
};

menu_t menu_old_main =
{
   mn_old_main_items,
   NULL, NULL, NULL,           // pages
   97, 64,
   0,
   mf_skullmenu | mf_emulated, // 08/30/06: use emulated flag
   MN_MainMenuDrawer
};

//
// MN_PatchOldMainMenu
//
// haleyjd 05/16/04: patches the old main menu for DOOM II, for full
// compatibility.
//
static void MN_PatchOldMainMenu(void)
{
   // turn "Read This!" into "Quit Game" and move down 8 pixels
   menu_old_main.menuitems[4] = menu_old_main.menuitems[5];
   menu_old_main.menuitems[5].type = it_end;
   menu_old_main.y += 8;
}

int mn_classic_menus;

//
// MN_LinkClassicMenus
//
// haleyjd 08/31/06: This function is called to turn classic menu system support
// on or off. When it's on, the old main menu above is patched to point to the
// other old menus.
//
void MN_LinkClassicMenus(int link)
{
   if(link) // turn on classic menus
   {
      menu_old_main.menuitems[1].data = "mn_old_options";
   }
   else // turn off classic menus
   {
      menu_old_main.menuitems[1].data = "mn_options";
   }
}

VARIABLE_BOOLEAN(mn_classic_menus, NULL, yesno);
CONSOLE_VARIABLE(mn_classic_menus, mn_classic_menus, 0) 
{
   MN_LinkClassicMenus(mn_classic_menus);
}

// Original Options Menu

static menuitem_t mn_old_option_items[] =
{
   { it_runcmd,    "end game",       "mn_endgame",    "M_ENDGAM" },
   { it_runcmd,    "messages",       "hu_messages /", "M_MESSG"  },
   { it_runcmd,    "graphic detail", "echo No.",      "M_DETAIL" },
   { it_bigslider, "screen size",    "screensize",    "M_SCRNSZ" },
   { it_gap },
   { it_bigslider, "mouse sens.",    "sens_combined", "M_MSENS"  },
   { it_gap },
   { it_runcmd,    "sound volume",   "mn_old_sound",  "M_SVOL"   },
   { it_end }
};

static char detailNames[2][9] = { "M_GDHIGH", "M_GDLOW" };
static char msgNames[2][9]    = { "M_MSGOFF", "M_MSGON" };

static void MN_OldOptionsDrawer(void)
{
   V_DrawPatch(108, 15, &vbscreen,
               PatchLoader::CacheName(wGlobalDir, "M_OPTTTL", PU_CACHE));

   V_DrawPatch(60 + 120, 37 + EMULATED_ITEM_SIZE, &vbscreen,
               PatchLoader::CacheName(wGlobalDir, msgNames[showMessages], PU_CACHE));

   V_DrawPatch(60 + 175, 37 + EMULATED_ITEM_SIZE*2, &vbscreen,
               PatchLoader::CacheName(wGlobalDir, detailNames[0], PU_CACHE));
}

menu_t menu_old_options =
{
   mn_old_option_items,
   NULL, NULL, NULL,           // pages
   60, 37,
   0,
   mf_skullmenu | mf_emulated,
   MN_OldOptionsDrawer
};

extern int mouseSensitivity_c;

CONSOLE_COMMAND(mn_old_options, 0)
{
   // propagate horizontal mouse sensitivity to combined setting
   mouseSensitivity_c = (int)(mouseSensitivity_horiz / 4.0);

   // bound to max 16
   if(mouseSensitivity_c > 16)
      mouseSensitivity_c = 16;

   MN_StartMenu(&menu_old_options);
}

// Original Sound Menu w/ Large Sliders

static menuitem_t mn_old_sound_items[] =
{
   { it_bigslider, "sfx volume",   "sfx_volume",   "M_SFXVOL" },
   { it_gap },
   { it_bigslider, "music volume", "music_volume", "M_MUSVOL" },
   { it_end }
};

static void MN_OldSoundDrawer(void)
{
   V_DrawPatch(60, 38, &vbscreen, 
               PatchLoader::CacheName(wGlobalDir, "M_SVOL", PU_CACHE));
}

menu_t menu_old_sound =
{
   mn_old_sound_items,
   NULL, NULL, NULL,
   80, 64,
   0,
   mf_skullmenu | mf_emulated,
   MN_OldSoundDrawer
};

CONSOLE_COMMAND(mn_old_sound, 0)
{
   MN_StartMenu(&menu_old_sound);
}

// TODO: Original Save and Load Menus?
// TODO: Draw skull cursor on Credits pages?

//
// MN_AddMenus
//
// Adds all menu system commands to the console command chains.
// 
void MN_AddMenus(void)
{
   C_AddCommand(mn_newgame);
   C_AddCommand(mn_episode);
   C_AddCommand(startlevel);
   C_AddCommand(use_startmap);
   C_AddCommand(mn_start_mapname); // haleyjd 05/14/06
   
   C_AddCommand(mn_loadgame);
   C_AddCommand(mn_load);
   C_AddCommand(mn_savegame);

   C_AddCommand(mn_loadwad);
   C_AddCommand(mn_loadwaditem);
   C_AddCommand(mn_wadname);
   C_AddCommand(mn_demos);
   C_AddCommand(mn_demoname);
   C_AddCommand(mn_player);

   // haleyjd: dmflags
   C_AddCommand(mn_dfitem);
   C_AddCommand(mn_dfweapstay);
   C_AddCommand(mn_dfbarrel);
   C_AddCommand(mn_dfplyrdrop);
   C_AddCommand(mn_dfrespsupr);
   C_AddCommand(mn_dfinstagib);
   
   // different connect types
   C_AddCommand(mn_gset);
   
   C_AddCommand(mn_options);
   C_AddCommand(mn_mouse);
   C_AddCommand(mn_video);
   C_AddCommand(mn_particle);  // haleyjd: particle options menu
   C_AddCommand(mn_vidmode);
   C_AddCommand(mn_favaspectratio);
   C_AddCommand(mn_favscreentype);
   C_AddCommand(mn_sound);
   C_AddCommand(mn_weapons);
   C_AddCommand(mn_compat);
   C_AddCommand(mn_enemies);
   C_AddCommand(mn_hud);
   C_AddCommand(mn_status);
   C_AddCommand(mn_automap);

   C_AddCommand(mn_movekeys);
   C_AddCommand(mn_advkeys);
   C_AddCommand(mn_weaponkeys);
   C_AddCommand(mn_envkeys);
   C_AddCommand(mn_gamefuncs);
   C_AddCommand(mn_menukeys);
   C_AddCommand(mn_automapkeys);
   C_AddCommand(mn_consolekeys);
   C_AddCommand(mn_clientserverkeys);
   C_AddCommand(mn_clientserverdemokeys);
   C_AddCommand(newgame);
   
   // prompt messages
   C_AddCommand(mn_quit);
   C_AddCommand(mn_endgame);

   // haleyjd 03/15/06: the "menu" menu
   C_AddCommand(mn_menus);
   C_AddCommand(mn_searchstr);
   C_AddCommand(mn_search);

   C_AddCommand(mn_config);
   
   // haleyjd: quicksave, quickload
   C_AddCommand(quicksave);
   C_AddCommand(quickload);
   C_AddCommand(qsave);
   C_AddCommand(qload);
   
   // haleyjd 04/15/02: SDL joystick devices
#ifdef _SDL_VER
   C_AddCommand(mn_joysticks);
   C_AddCommand(mn_joymenu);
#endif

   C_AddCommand(skinviewer);
   C_AddCommand(use_traditional_menu);

   // haleyjd: "old" menus
   C_AddCommand(mn_classic_menus);
   C_AddCommand(mn_old_options);
   C_AddCommand(mn_old_sound);
   
   // haleyjd: add Heretic-specific menus (in mn_htic.c)
   MN_AddHMenus();
   
   MN_CreateSaveCmds();

   // haleyjd 03/11/06: file dialog cmds
   MN_File_AddCommands();
}

// EOF
