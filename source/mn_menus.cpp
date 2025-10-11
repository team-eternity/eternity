//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Simon Howard et al.
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
//------------------------------------------------------------------------------
//
// Purpose: The actual menus: Structs and handler functions (if any),
//  console commands to activate each menu.
//
// Authors: Simon Howard, James Haley, Max Waine, Devin Acker
//

#include "z_zone.h"

#include "hal/i_gamepads.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_deh.h"
#include "d_dehtbl.h"
#include "d_event.h"
#include "d_files.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_iwad.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "dhticstr.h" // haleyjd
#include "dstrings.h"
#include "e_fonts.h"
#include "e_states.h"
#include "g_bind.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "hu_over.h"
#include "hu_stuff.h" // haleyjd
#include "i_system.h"
#include "i_video.h"
#include "m_random.h"
#include "m_swap.h"
#include "m_utils.h"
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
#include "r_main.h"
#include "r_patch.h"
#include "r_state.h"
#include "s_sound.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_levels.h"
#include "w_wad.h"

// need wad iterators
#include "w_iterator.h"

// menus: all in this file (not really extern)
extern menu_t menu_newgame;
extern menu_t menu_newmission;
extern menu_t menu_main;
extern menu_t menu_episode;
extern menu_t menu_d2episode;

int screenSize; // screen size

char *mn_demoname; // demo to play

// haleyjd: moved these up here to fix Z_Free error

char *mn_start_mapname;

static void MN_InitCustomMenu();
static void MN_InitSearchStr();
static bool MN_tweakD2EpisodeMenu();

void MN_InitMenus()
{
    mn_demoname      = estrdup("demo1");
    mn_wadname       = estrdup("");
    mn_start_mapname = estrdup(""); // haleyjd 05/14/06

    MN_InitCustomMenu(); // haleyjd 03/14/06
    MN_InitSearchStr();  // haleyjd 03/15/06
}

//=============================================================================
//
// THE MENUS
//
//=============================================================================

//=============================================================================
//
// Main Menu
//

// Note: The main menu is modified dynamically to point to
// the rest of the old menu system when appropriate.

static void MN_MainMenuDrawer()
{
    // hack for m_doom compatibility
    V_DrawPatch(94, 2, &subscreen43, PatchLoader::CacheName(wGlobalDir, "M_DOOM", PU_CACHE));
}

static menuitem_t mn_main_items[] = {
    { it_runcmd, "New Game",   "mn_newgame",  "M_NGAME"  },
    { it_runcmd, "Options",    "mn_options",  "M_OPTION" },
    { it_runcmd, "Load Game",  "mn_loadgame", "M_LOADG"  },
    { it_runcmd, "Save Game",  "mn_savegame", "M_SAVEG"  },
    { it_runcmd, "Read This!", "credits",     "M_RDTHIS" },
    { it_runcmd, "Quit",       "mn_quit",     "M_QUITG"  },
    { it_end,    nullptr,      nullptr,       nullptr    }
};

menu_t menu_main = {
    mn_main_items,              // menu items
    nullptr,                    // prev page
    nullptr,                    // next page
    nullptr,                    // rootpage
    97,                         // x offset
    64,                         // y offset
    0,                          // first selectable
    mf_skullmenu | mf_emulated, // 08/30/06: use emulated flag
    MN_MainMenuDrawer,          // no drawer
};

static menuitem_t mn_main_doom2_items[] = {
    { it_runcmd, "New Game",  "mn_newgame",  "M_NGAME"  },
    { it_runcmd, "Options",   "mn_options",  "M_OPTION" },
    { it_runcmd, "Load Game", "mn_loadgame", "M_LOADG"  },
    { it_runcmd, "Save Game", "mn_savegame", "M_SAVEG"  },
    { it_runcmd, "Quit",      "mn_quit",     "M_QUITG"  },
    { it_end,    nullptr,     nullptr,       nullptr    }
};

menu_t menu_main_doom2 = {
    mn_main_doom2_items,        // menu items
    nullptr,                    // prev page
    nullptr,                    // next page
    nullptr,                    // rootpage
    97,                         // x offset
    72,                         // y offset
    0,                          // first selectable
    mf_skullmenu | mf_emulated, // 08/30/06: use emulated flag
    MN_MainMenuDrawer,          // no drawer
};

// haleyjd 05/14/06: moved these up here
int start_episode;

char *start_mapname; // local copy of ptr to cvar value

extern menu_t *mn_episode_override;

VARIABLE_STRING(mn_start_mapname, nullptr, 9);
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

//
// MN_DoomNewGame
//
// GameModeInfo function for starting a new game in Doom modes.
//
void MN_DoomNewGame()
{
    // hack -- cut off thy flesh consumed if not retail
    if(GameModeInfo->numEpisodes < 4)
        menu_episode.menuitems[3].type = it_end;

    MN_StartMenu(&menu_episode);
}

//
// MN_Doom2NewGame
//
// GameModeInfo function for starting a new game in Doom II modes.
//
void MN_Doom2NewGame()
{
    if(MN_tweakD2EpisodeMenu())
        MN_StartMenu(&menu_d2episode);
    else
        MN_StartMenu(&menu_newgame);
}

//
// mn_newgame
//
// called from main menu:
// starts menu according to menuStartMap, gametype and modifiedgame
//
CONSOLE_COMMAND(mn_newgame, 0)
{
    if(netgame && !demoplayback)
    {
        MN_Alert("%s", DEH_String("NEWGAME"));
        return;
    }

    // haleyjd 05/14/06: reset episode/level selection variables
    start_episode = 1;
    start_mapname = nullptr;

    // haleyjd 05/14/06: check for episode menu override now
    if(mn_episode_override)
    {
        int episodeIndex = -1;

        // Figure out if we should skip the episode menu override due to it containing only a
        // single episode, and nothing else
        for(int i = 0; mn_episode_override->menuitems[i].type != it_end; i++)
        {
            menuitem_t *item = &mn_episode_override->menuitems[i];
            if(item->type == it_runcmd)
            {
                if(!strncasecmp("mn_start_mapname", item->data, strlen("mn_start_mapname")) && episodeIndex == -1)
                    episodeIndex = i;
                else
                {
                    episodeIndex = -1;
                    break;
                }
            }
            else if(item->type != it_title && item->type != it_gap && item->type != it_info)
            {
                episodeIndex = -1;
                break;
            }
        }

        if(episodeIndex >= 0)
            C_RunTextCmd(mn_episode_override->menuitems[episodeIndex].data);
        else
            MN_StartMenu(mn_episode_override);
    }
    else if(GameModeInfo->menuStartMap && *GameModeInfo->menuStartMap &&
            W_CheckNumForName(GameModeInfo->menuStartMap) >= 0)
    {
        G_DeferedInitNew(defaultskill, GameModeInfo->menuStartMap);
        MN_ClearMenus();
    }
    else
        GameModeInfo->OnNewGame();
}

// menu item to quit doom:
// pop up a quit message as in the original

void MN_QuitDoom()
{
    int         quitmsgnum;
    char        quitmsg[128];
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

//=============================================================================
//
// Episode Selection
//

static void MN_EpisodeDrawer()
{
    V_DrawPatch(54, 38, &subscreen43, PatchLoader::CacheName(wGlobalDir, "M_EPISOD", PU_CACHE));
}

static menuitem_t mn_episode_items[] = {
    { it_runcmd, "Knee Deep in the Dead", "mn_episode 1", "M_EPI1" },
    { it_runcmd, "The Shores of Hell",    "mn_episode 2", "M_EPI2" },
    { it_runcmd, "Inferno",               "mn_episode 3", "M_EPI3" },
    { it_runcmd, "Thy Flesh Consumed",    "mn_episode 4", "M_EPI4" },
    { it_end,    nullptr,                 nullptr,        nullptr  }
};

menu_t menu_episode = {
    mn_episode_items,           // menu items
    nullptr,                    // prev page
    nullptr,                    // next page
    nullptr,                    // rootpage
    48,                         // x offset
    63,                         // y offset
    0,                          // select episode 1
    mf_skullmenu | mf_emulated, // skull menu
    MN_EpisodeDrawer,           // drawer
};

//
// Stub episode menu (for UMAPINFO)
//
static menuitem_t mn_episode_itemsStub[] = { { it_end } };

menu_t menu_episodeDoom2Stub = {
    mn_episode_itemsStub,       // menu items
    nullptr,                    // prev page
    nullptr,                    // next page
    nullptr,                    // rootpage
    48,                         // x offset
    63,                         // y offset
    0,                          // first selectable
    mf_skullmenu | mf_emulated, // skull menu
    MN_EpisodeDrawer,           // no drawer
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

//=============================================================================
//
// BFG Edition No Rest for the Living Special Support
//

static int mn_missionNo;

// Skill level items for missions
static menuitem_t mn_newmission_items[] = {
    { it_runcmd, "I'm too young to die", "mn_startmission 0", "M_JKILL" },
    { it_runcmd, "Hey, not too rough",   "mn_startmission 1", "M_ROUGH" },
    { it_runcmd, "Hurt me plenty.",      "mn_startmission 2", "M_HURT"  },
    { it_runcmd, "Ultra-Violence",       "mn_startmission 3", "M_ULTRA" },
    { it_runcmd, "Nightmare!",           "mn_startmission 4", "M_NMARE" },
    { it_end,    nullptr,                nullptr,             nullptr   }
};

//
// mn_startmission
//
// Console command to start a managed mission pack from the menus.
// Skill is provided as a parameter; the mission pack number was
// decided earlier by mn_mission.
//
CONSOLE_COMMAND(mn_startmission, cf_hidden | cf_notnet)
{
    if(Console.argc < 1)
        return;
    int skill = Console.argv[0]->toInt();

    switch(mn_missionNo)
    {
    case 1: // NR4TL
        W_DoNR4TLStart(skill);
        break;
    case 2: // Master Levels
        W_DoMasterLevels(true, skill);
        break;
    default: //
        C_Printf(FC_ERROR "Unknown mission number %d\a\n", mn_missionNo);
        break;
    }
}

//
// mn_mission
//
// Managed mission pack selection command for the menus. Remember the
// mission number chosen, for mn_startmission on the next menu.
//
CONSOLE_COMMAND(mn_mission, cf_hidden | cf_notnet)
{
    if(Console.argc < 1)
        return;
    mn_missionNo = Console.argv[0]->toInt();
    MN_StartMenu(&menu_newmission);
}

static void MN_D2EpisodeDrawer()
{
    V_DrawPatch(54, 38, &subscreen43, PatchLoader::CacheName(wGlobalDir, "M_BFGEPI", PU_CACHE));
}

static menuitem_t mn_item_nr4tl = { it_runcmd, "No Rest for the Living", "mn_mission 1", "M_BFGEP2" };

static menuitem_t mn_item_masterlevs = { it_runcmd, "Master Levels", "mn_mission 2", "M_BFGEP3" };

static menuitem_t mn_dm2ep_items[] = {
    { it_runcmd, "Hell on Earth", "mn_episode 1", "M_BFGEP1" },
    { it_end,    nullptr,         nullptr,        nullptr    }, // reserved
    { it_end,    nullptr,         nullptr,        nullptr    }, // reserved
    { it_end,    nullptr,         nullptr,        nullptr    }
};

menu_t menu_d2episode = {
    mn_dm2ep_items,             // menu items
    nullptr,                    // prev page
    nullptr,                    // next page
    nullptr,                    // rootpage
    48,                         // x offset
    63,                         // y offset
    0,                          // first selectable
    mf_skullmenu | mf_emulated, // skull menu
    MN_D2EpisodeDrawer,         // no drawer
};

//
// MN_tweakD2EpisodeMenu
//
// haleyjd 05/14/13: We build the managed mission pack selection menu based on
// the available expansions. Returns true if any expansions were added.
//
static bool MN_tweakD2EpisodeMenu()
{
    int curitem = 1;

    // Not for TNT, Plutonia, HacX
    if(!(GameModeInfo->missionInfo->flags & MI_DOOM2MISSIONS))
        return false;

    // Also not when playing PWADs
    if(modifiedgame)
        return false;

    // start out with nothing enabled
    menu_d2episode.menuitems[1].type = it_end;
    menu_d2episode.menuitems[2].type = it_end;

    if(w_norestpath && *w_norestpath) // Have NR4TL?
    {
        menu_d2episode.menuitems[curitem] = mn_item_nr4tl;
        ++curitem;
    }

    if(w_masterlevelsdirname && *w_masterlevelsdirname) // Have Master levels?
    {
        menu_d2episode.menuitems[curitem] = mn_item_masterlevs;
        ++curitem;
    }

    // If none are configured, we're not going to even show this menu.
    return (curitem > 1);
}

//=============================================================================
//
// New Game Menu: Skill Level Selection
//

//
// MN_openNewGameMenu
//
// haleyjd 11/12/09: special initialization method for the newgame menu.
//
static void MN_openNewGameMenu(menu_t *menu)
{
    // start on defaultskill setting
    menu->selected = defaultskill - 1;
}

//
// MN_DrawNewGame
//
static void MN_DrawNewGame()
{
    V_DrawPatch(96, 14, &subscreen43, PatchLoader::CacheName(wGlobalDir, "M_NEWG", PU_CACHE));
    V_DrawPatch(54, 38, &subscreen43, PatchLoader::CacheName(wGlobalDir, "M_SKILL", PU_CACHE));
}

static menuitem_t mn_newgame_items[] = {
    { it_runcmd, "I'm too young to die", "newgame 0", "M_JKILL" },
    { it_runcmd, "Hey, not too rough",   "newgame 1", "M_ROUGH" },
    { it_runcmd, "Hurt me plenty.",      "newgame 2", "M_HURT"  },
    { it_runcmd, "Ultra-Violence",       "newgame 3", "M_ULTRA" },
    { it_runcmd, "Nightmare!",           "newgame 4", "M_NMARE" },
    { it_end,    nullptr,                nullptr,     nullptr   }
};

menu_t menu_newgame = {
    mn_newgame_items,           // menu items
    nullptr,                    // prev page
    nullptr,                    // next page
    nullptr,                    // rootpage
    48,                         // x offset
    63,                         // y offset
    0,                          // starting item (overridden by open method)
    mf_skullmenu | mf_emulated, // is a skull menu
    MN_DrawNewGame,             // no drawer
    nullptr,                    // TOC names
    nullptr,                    // TOC pages
    0,                          // gap override
    MN_openNewGameMenu,         // open method
};

menu_t menu_newmission = {
    mn_newmission_items,        // menu items
    nullptr,                    // prev page
    nullptr,                    // next page
    nullptr,                    // rootpage
    48,                         // x offset
    63,                         // y offset
    0,                          // starting item
    mf_skullmenu | mf_emulated, // is a skull menu
    MN_DrawNewGame,             // no drawer
    nullptr,                    // TOC names
    nullptr,                    // TOC pages
    0,                          // gap override
    MN_openNewGameMenu,         // open method
};

static void MN_DoNightmare()
{
    // haleyjd 05/14/06: check for mn_episode_override
    if(mn_episode_override)
    {
        if(start_mapname) // if set, use name, else use episode num as usual
            G_DeferedInitNew(sk_nightmare, start_mapname);
        else
            G_DeferedInitNewNum(sk_nightmare, start_episode, 1);
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
    else
    {
        // start on first level of selected episode
        G_DeferedInitNewNum((skill_t)skill, start_episode, 1);
    }

    MN_ClearMenus();
}

////////////////////////////////////////////////
//
// Demos Menu
//
// Allow Demo playback and (recording),
// also access to camera angles
//

static menuitem_t mn_demos_items[] = {
    { it_title,    "Demos",             nullptr,                                "m_demos" },
    { it_gap,      nullptr,             nullptr,                                nullptr   },
    { it_info,     "Play Demo",         nullptr,                                nullptr   },
    { it_variable, "Demo name",         "mn_demoname",                          nullptr   },
    { it_runcmd,   "Play demo...",      "mn_clearmenus; playdemo %mn_demoname", nullptr   },
    { it_runcmd,   "Stop demo...",      "mn_clearmenus; stopdemo",              nullptr   },
    { it_gap,      nullptr,             nullptr,                                nullptr   },
    { it_info,     "Sync",              nullptr,                                nullptr   },
    { it_toggle,   "Demo insurance",    "demo_insurance",                       nullptr   },
    { it_gap,      nullptr,             nullptr,                                nullptr   },
    { it_info,     "Cameras",           nullptr,                                nullptr   },
    { it_toggle,   "Viewpoint changes", "cooldemo",                             nullptr   },
    { it_toggle,   "Walkcam",           "walkcam",                              nullptr   },
    { it_toggle,   "Chasecam",          "chasecam",                             nullptr   },
    { it_variable, "Chasecam height",   "chasecam_height",                      nullptr   },
    { it_variable, "Chasecam speed %",  "chasecam_speed",                       nullptr   },
    { it_variable, "Chasecam distance", "chasecam_dist",                        nullptr   },
    { it_end,      nullptr,             nullptr,                                nullptr   }
};

menu_t menu_demos = {
    mn_demos_items, // menu items
    nullptr,        // prev page
    nullptr,        // next page
    nullptr,        // rootpage
    200,            // x offset
    15,             // y offset
    3,              // start item
    mf_background,  // fullscreen
};

VARIABLE_STRING(mn_demoname, nullptr, 12);
CONSOLE_VARIABLE(mn_demoname, mn_demoname, 0) {}

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

static const char *mn_wad_names[] = {
    "File Selection / Master Levels",        "Autoloads", "IWAD Paths - DOOM", "IWAD Paths - Raven",
    "IWAD Paths - Freedoom / Mission Packs", nullptr
};

static menu_t *mn_wad_pages[] = {
    &menu_loadwad,   //
    &menu_wadmisc,   //
    &menu_wadiwads1, //
    &menu_wadiwads2, //
    &menu_wadiwads3, //
    nullptr          //
};

static menuitem_t mn_loadwad_items[] = {
    { it_title,    "Load Wad",              nullptr,             "M_WAD", 0                 },
    { it_gap,      nullptr,                 nullptr,             nullptr, 0                 },
    { it_info,     "File Selection",        nullptr,             nullptr, MENUITEM_CENTERED },
    { it_gap,      nullptr,                 nullptr,             nullptr, 0                 },
    { it_variable, "Wad name:",             "mn_wadname",        nullptr, MENUITEM_LALIGNED },
    { it_variable, "Wad directory:",        "wad_directory",     nullptr, MENUITEM_LALIGNED },
    { it_runcmd,   "Select wad...",         "mn_selectwad",      nullptr, MENUITEM_LALIGNED },
    { it_gap,      nullptr,                 nullptr,             nullptr, 0                 },
    { it_runcmd,   "Load wad",              "mn_loadwaditem",    nullptr, MENUITEM_CENTERED },
    { it_gap,      nullptr,                 nullptr,             nullptr, 0                 },
    { it_info,     "Master Levels",         nullptr,             nullptr, MENUITEM_CENTERED },
    { it_gap,      nullptr,                 nullptr,             nullptr, 0                 },
    { it_variable, "Master Levels dir:",    "master_levels_dir", nullptr, MENUITEM_LALIGNED },
    { it_runcmd,   "Play Master Levels...", "w_masterlevels",    nullptr, MENUITEM_LALIGNED },
    { it_end,      nullptr,                 nullptr,             nullptr, 0                 },
};

static menuitem_t mn_wadmisc_items[] = {
    { it_title,    "Wad Options",      nullptr,      "M_WADOPT", 0                 },
    { it_gap,      nullptr,            nullptr,      nullptr,    0                 },
    { it_info,     "Autoloaded Files", nullptr,      nullptr,    MENUITEM_CENTERED },
    { it_gap,      nullptr,            nullptr,      nullptr,    0                 },
    { it_variable, "WAD file 1:",      "auto_wad_1", nullptr,    MENUITEM_LALIGNED },
    { it_variable, "WAD file 2:",      "auto_wad_2", nullptr,    MENUITEM_LALIGNED },
    { it_variable, "DEH file 1:",      "auto_deh_1", nullptr,    MENUITEM_LALIGNED },
    { it_variable, "DEH file 2:",      "auto_deh_2", nullptr,    MENUITEM_LALIGNED },
    { it_variable, "CSC file 1:",      "auto_csc_1", nullptr,    MENUITEM_LALIGNED },
    { it_variable, "CSC file 2:",      "auto_csc_2", nullptr,    MENUITEM_LALIGNED },
    { it_end,      nullptr,            nullptr,      nullptr,    0                 },
};

static menuitem_t mn_wadiwad1_items[] = {
    { it_title,    "Wad Options",       nullptr,               "M_WADOPT", 0                 },
    { it_gap,      nullptr,             nullptr,               nullptr,    0                 },
    { it_info,     "IWAD Paths - DOOM", nullptr,               nullptr,    MENUITEM_CENTERED },
    { it_gap,      nullptr,             nullptr,               nullptr,    0                 },
    { it_variable, "DOOM (SW):",        "iwad_doom_shareware", nullptr,    MENUITEM_LALIGNED },
    { it_variable, "DOOM (Reg):",       "iwad_doom",           nullptr,    MENUITEM_LALIGNED },
    { it_variable, "Ultimate DOOM:",    "iwad_ultimate_doom",  nullptr,    MENUITEM_LALIGNED },
    { it_variable, "DOOM II:",          "iwad_doom2",          nullptr,    MENUITEM_LALIGNED },
    { it_variable, "Evilution:",        "iwad_tnt",            nullptr,    MENUITEM_LALIGNED },
    { it_variable, "Plutonia:",         "iwad_plutonia",       nullptr,    MENUITEM_LALIGNED },
    { it_variable, "HACX:",             "iwad_hacx",           nullptr,    MENUITEM_LALIGNED },
    { it_gap,      nullptr,             nullptr,               nullptr,    0                 },
    { it_variable, "ID24 Resources:",   "pwad_id24res",        nullptr,    MENUITEM_LALIGNED },
    { it_end,      nullptr,             nullptr,               nullptr,    0                 }
};

static menuitem_t mn_wadiwad2_items[] = {
    { it_title,    "Wad Options",        nullptr,                  "M_WADOPT", 0                 },
    { it_gap,      nullptr,              nullptr,                  nullptr,    0                 },
    { it_info,     "IWAD Paths - Raven", nullptr,                  nullptr,    MENUITEM_CENTERED },
    { it_gap,      nullptr,              nullptr,                  nullptr,    0                 },
    { it_variable, "Heretic (SW):",      "iwad_heretic_shareware", nullptr,    MENUITEM_LALIGNED },
    { it_variable, "Heretic (Reg):",     "iwad_heretic",           nullptr,    MENUITEM_LALIGNED },
    { it_variable, "Heretic SoSR:",      "iwad_heretic_sosr",      nullptr,    MENUITEM_LALIGNED },
    { it_end,      nullptr,              nullptr,                  nullptr,    0                 }
};

static menuitem_t mn_wadiwad3_items[] = {
    { it_title,    "Wad Options",             nullptr,          "M_WADOPT", 0                 },
    { it_gap,      nullptr,                   nullptr,          nullptr,    0                 },
    { it_info,     "IWAD Paths - Freedoom",   nullptr,          nullptr,    MENUITEM_CENTERED },
    { it_gap,      nullptr,                   nullptr,          nullptr,    0                 },
    { it_variable, "Freedoom Phase 1:",       "iwad_freedoomu", nullptr,    MENUITEM_LALIGNED },
    { it_variable, "Freedoom Phase 2:",       "iwad_freedoom",  nullptr,    MENUITEM_LALIGNED },
    { it_variable, "FreeDM:",                 "iwad_freedm",    nullptr,    MENUITEM_LALIGNED },
    { it_gap,      nullptr,                   nullptr,          nullptr,    0                 },
    { it_info,     "Mission Packs",           nullptr,          nullptr,    MENUITEM_CENTERED },
    { it_gap,      nullptr,                   nullptr,          nullptr,    0                 },
    { it_variable, "No Rest for the Living:", "w_norestpath",   nullptr,    MENUITEM_LALIGNED },
    { it_end,      nullptr,                   nullptr,          nullptr,    0                 }
};

menu_t menu_loadwad = {
    mn_loadwad_items, // menu items
    nullptr,          // prev page
    &menu_wadmisc,    // next page
    &menu_loadwad,    // rootpage
    120,              // x offset
    15,               // y offset
    4,                // starting item
    mf_background,    // fullscreen
    nullptr,          // no drawer
    mn_wad_names,     // TOC names
    mn_wad_pages,     // TOC pages
};

menu_t menu_wadmisc = {
    mn_wadmisc_items, // menu items
    &menu_loadwad,    // prev page
    &menu_wadiwads1,  // next page
    &menu_loadwad,    // rootpage
    200,              // x offset
    15,               // y offset
    4,                // starting item
    mf_background,    // fullscreen
    nullptr,          // no drawer
    mn_wad_names,     // TOC names
    mn_wad_pages,     // TOC pages
};

menu_t menu_wadiwads1 = {
    mn_wadiwad1_items, // menu items
    &menu_wadmisc,     // prev page
    &menu_wadiwads2,   // next page
    &menu_loadwad,     // rootpage
    200,               // x offset
    15,                // y offset
    4,                 // starting item
    mf_background,     // fullscreen
    nullptr,           // no drawer
    mn_wad_names,      // TOC names
    mn_wad_pages,      // TOC pages
};

menu_t menu_wadiwads2 = {
    mn_wadiwad2_items, // menu items
    &menu_wadiwads1,   // prev page
    &menu_wadiwads3,   // next page
    &menu_loadwad,     // rootpage
    200,               // x offset
    15,                // y offset
    4,                 // starting item
    mf_background,     // fullscreen
    nullptr,           // no drawer
    mn_wad_names,      // TOC names
    mn_wad_pages,      // TOC pages
};

menu_t menu_wadiwads3 = {
    mn_wadiwad3_items, // menu items
    &menu_wadiwads2,   // prev page
    nullptr,           // next page
    &menu_loadwad,     // rootpage
    200,               // x offset
    15,                // y offset
    4,                 // starting item
    mf_background,     // fullscreen
    nullptr,           // no drawer
    mn_wad_names,      // TOC names
    mn_wad_pages,      // TOC pages
};

CONSOLE_COMMAND(mn_loadwad, cf_notnet)
{
    MN_StartMenu(&menu_loadwad);
}

//=============================================================================
//
// Multiplayer Game settings
//

static const char *mn_gset_names[] = {
    "general / dm auto-exit", "monsters / boom features", "deathmatch flags", "chat macros", nullptr,
};

extern menu_t menu_gamesettings;
extern menu_t menu_advanced;
extern menu_t menu_dmflags;
extern menu_t menu_chatmacros;

static menu_t *mn_gset_pages[] = {
    &menu_gamesettings,
    &menu_advanced,
    &menu_dmflags,
    &menu_chatmacros,
    nullptr, //
};

static menuitem_t mn_gamesettings_items[] = {
    { it_title,    "Game Settings",  nullptr,      "M_GSET" },
    { it_gap,      nullptr,          nullptr,      nullptr  },
    { it_info,     "General",        nullptr,      nullptr  },
    { it_toggle,   "game type",      "gametype",   nullptr  },
    { it_variable, "starting level", "startlevel", nullptr  },
    { it_toggle,   "skill level",    "skill",      nullptr  },
    { it_gap,      nullptr,          nullptr,      nullptr  },
    { it_info,     "DM Auto-Exit",   nullptr,      nullptr  },
    { it_variable, "time limit",     "timelimit",  nullptr  },
    { it_variable, "frag limit",     "fraglimit",  nullptr  },
    { it_end,      nullptr,          nullptr,      nullptr  }
};

menu_t menu_gamesettings = {
    mn_gamesettings_items, // menu items
    nullptr,               // prev page
    &menu_advanced,        // next page
    &menu_gamesettings,    // rootpage
    164,                   // x offset
    15,                    // y offset
    3,                     // starting item
    mf_background,         // fullscreen
    nullptr,               // no drawer
    mn_gset_names,         // TOC names
    mn_gset_pages,         // TOC pages
};

// level to start on
VARIABLE_STRING(startlevel, nullptr, 9);
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
        startlevel = Z_Strdup(newvalue, PU_STATIC, nullptr);
    }
}

CONSOLE_COMMAND(mn_gset, 0) // just setting options from menu
{
    MN_StartMenu(&menu_gamesettings);
}

/////////////////////////////////////////////////////////////////
//
// Multiplayer Game settings
// Advanced menu
//

static menuitem_t mn_advanced_items[] = {
    { it_title,  "Game Settings",       nullptr,       "M_GSET" },
    { it_gap,    nullptr,               nullptr,       nullptr  },
    { it_info,   "Monsters",            nullptr,       nullptr  },
    { it_toggle, "no monsters",         "nomonsters",  nullptr  },
    { it_toggle, "fast monsters",       "fast",        nullptr  },
    { it_toggle, "respawning monsters", "respawn",     nullptr  },
    { it_gap,    nullptr,               nullptr,       nullptr  },
    { it_info,   "BOOM Features",       nullptr,       nullptr  },
    { it_toggle, "variable friction",   "varfriction", nullptr  },
    { it_toggle, "boom push effects",   "pushers",     nullptr  },
    { it_toggle, "nukage enabled",      "nukage",      nullptr  },
    { it_end,    nullptr,               nullptr,       nullptr  }
};

menu_t menu_advanced = {
    mn_advanced_items,  // menu items
    &menu_gamesettings, // prev page
    &menu_dmflags,      // next page
    &menu_gamesettings, // rootpage
    200,                // x offset
    15,                 // y offset
    3,                  // starting item
    mf_background,      // fullscreen
    nullptr,            // no drawer
    mn_gset_names,      // TOC names
    mn_gset_pages,      // TOC pages
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

static void MN_DMFlagsDrawer();

enum
{
    DMF_MENU_TITLE,
    DMF_MENU_TITLEGAP,
    DMF_MENU_ITEMSRESPAWN,
    DMF_MENU_WEAPONSSTAY,
    DMF_MENU_RESPAWNBARRELS,
    DMF_MENU_PLAYERDROP,
    DMF_MENU_SUPERITEMS,
    DMF_MENU_INSTAGIB,
    DMF_MENU_KEEPITEMS,
    DMF_MENU_FLAGS_GAP,
    DMF_MENU_DMFLAGS,
    DMF_END,
    DMF_NUMITEMS
};

static menuitem_t mn_dmflags_items[DMF_NUMITEMS] = {
    { it_title,  "Deathmatch Flags",       nullptr,          "M_DMFLAG", 0                 },
    { it_gap,    nullptr,                  nullptr,          nullptr,    0                 },
    { it_runcmd, "Items respawn",          "mn_dfitem",      nullptr,    0                 },
    { it_runcmd, "Weapons stay",           "mn_dfweapstay",  nullptr,    0                 },
    { it_runcmd, "Respawning barrels",     "mn_dfbarrel",    nullptr,    0                 },
    { it_runcmd, "Players drop items",     "mn_dfplyrdrop",  nullptr,    0                 },
    { it_runcmd, "Respawning super items", "mn_dfrespsupr",  nullptr,    0                 },
    { it_runcmd, "Instagib",               "mn_dfinstagib",  nullptr,    0                 },
    { it_runcmd, "Keep items on respawn",  "mn_dfkeepitems", nullptr,    0                 },
    { it_gap,    nullptr,                  nullptr,          nullptr,    0                 },
    { it_info,   "dmflags =",              nullptr,          nullptr,    MENUITEM_CENTERED },
    { it_end,    nullptr,                  nullptr,          nullptr,    0                 }
};

menu_t menu_dmflags = {
    mn_dmflags_items,   // menu items
    &menu_advanced,     // prev page
    &menu_chatmacros,   // next page
    &menu_gamesettings, // rootpage
    200,                // x offset
    15,                 // y offset
    2,                  // starting item
    mf_background,      // fullscreen
    MN_DMFlagsDrawer,   // no drawer
    mn_gset_names,      // TOC names
    mn_gset_pages,      // TOC pages
};

// haleyjd 04/14/03: dmflags menu drawer (a big hack, mostly)

static void MN_DMFlagsDrawer()
{
    int         i;
    char        buf[64];
    const char *values[2] = { "on", "off" };
    menuitem_t *menuitem;

    // don't draw anything before the menu has been initialized
    if(!(menu_dmflags.menuitems[9].flags & MENUITEM_POSINIT))
        return;

    for(i = DMF_MENU_ITEMSRESPAWN; i < DMF_MENU_FLAGS_GAP; i++)
    {
        menuitem = &(menu_dmflags.menuitems[i]);

        V_FontWriteTextColored(menu_font, values[!(dmflags & (1 << (i - 2)))],
                               (i == menu_dmflags.selected) ? GameModeInfo->selectColor : GameModeInfo->variableColor,
                               menuitem->x + 20, menuitem->y, &subscreen43);
    }

    menuitem = &(menu_dmflags.menuitems[DMF_MENU_DMFLAGS]);
    // draw dmflags value
    psnprintf(buf, sizeof(buf), "%c%u", GameModeInfo->infoColor, dmflags);
    V_FontWriteText(menu_font, buf, menuitem->x + 4, menuitem->y, &subscreen43);
}

static void toggle_dm_flag(unsigned int flag)
{
    char cmdbuf[64];
    dmflags ^= flag;
    psnprintf(cmdbuf, sizeof(cmdbuf), "dmflags %u", dmflags);
    C_RunTextCmd(cmdbuf);
}

CONSOLE_COMMAND(mn_dfitem, cf_server | cf_hidden)
{
    toggle_dm_flag(DM_ITEMRESPAWN);
}

CONSOLE_COMMAND(mn_dfweapstay, cf_server | cf_hidden)
{
    toggle_dm_flag(DM_WEAPONSTAY);
}

CONSOLE_COMMAND(mn_dfbarrel, cf_server | cf_hidden)
{
    toggle_dm_flag(DM_BARRELRESPAWN);
}

CONSOLE_COMMAND(mn_dfplyrdrop, cf_server | cf_hidden)
{
    toggle_dm_flag(DM_PLAYERDROP);
}

CONSOLE_COMMAND(mn_dfrespsupr, cf_server | cf_hidden)
{
    toggle_dm_flag(DM_RESPAWNSUPER);
}

CONSOLE_COMMAND(mn_dfinstagib, cf_server | cf_hidden)
{
    toggle_dm_flag(DM_INSTAGIB);
}

CONSOLE_COMMAND(mn_dfkeepitems, cf_server | cf_hidden)
{
    toggle_dm_flag(DM_KEEPITEMS);
}

/////////////////////////////////////////////////////////////////
//
// Chat Macros
//

static menuitem_t mn_chatmacros_items[] = {
    { it_title,    "Chat Macros", nullptr,      "M_CHATM" },
    { it_gap,      nullptr,       nullptr,      nullptr   },
    { it_variable, "0:",          "chatmacro0", nullptr   },
    { it_variable, "1:",          "chatmacro1", nullptr   },
    { it_variable, "2:",          "chatmacro2", nullptr   },
    { it_variable, "3:",          "chatmacro3", nullptr   },
    { it_variable, "4:",          "chatmacro4", nullptr   },
    { it_variable, "5:",          "chatmacro5", nullptr   },
    { it_variable, "6:",          "chatmacro6", nullptr   },
    { it_variable, "7:",          "chatmacro7", nullptr   },
    { it_variable, "8:",          "chatmacro8", nullptr   },
    { it_variable, "9:",          "chatmacro9", nullptr   },
    { it_end,      nullptr,       nullptr,      nullptr   }
};

menu_t menu_chatmacros = {
    mn_chatmacros_items, // menu items
    &menu_dmflags,       // prev page
    nullptr,             // next page
    &menu_gamesettings,  // rootpage
    25,                  // x offset
    15,                  // y offset
    2,                   // chatmacro0 at start
    mf_background,       // fullscreen
    nullptr,             // no drawer
    mn_gset_names,       // TOC names
    mn_gset_pages,       // TOC pages
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

static void MN_PlayerDrawer(void);

static menuitem_t mn_player_items[] = {
    { it_title,    "Player Setup",   nullptr,      "M_PLAYER" },
    { it_gap,      nullptr,          nullptr,      nullptr    },
    { it_variable, "player name",    "name",       nullptr    },
    { it_toggle,   "player color",   "colour",     nullptr    },
    { it_toggle,   "player skin",    "skin",       nullptr    },
    { it_runcmd,   "skin viewer...", "skinviewer", nullptr    },
    { it_gap,      nullptr,          nullptr,      nullptr    },
    { it_toggle,   "handedness",     "lefthanded", nullptr    },
    { it_end,      nullptr,          nullptr,      nullptr    }
};

menu_t menu_player = {
    mn_player_items, // menu items
    nullptr,         // previous page
    nullptr,         // next page
    nullptr,         // rootpage
    180,             // x offset
    5,               // y offset
    2,               // player name at start
    mf_background,   // fullscreen menu
    MN_PlayerDrawer, // player sprite drawer
};

#define SPRITEBOX_X 200
#define SPRITEBOX_Y (menu_player.menuitems[7].y + 16)

void MN_PlayerDrawer()
{
    int            lump, w, h, toff, loff;
    spritedef_t   *sprdef;
    spriteframe_t *sprframe;
    patch_t       *patch;

    if(!(menu_player.menuitems[7].flags & MENUITEM_POSINIT))
        return;

    sprdef = &sprites[players[consoleplayer].skin->sprite];

    // haleyjd 08/15/02
    if(!(sprdef->spriteframes))
        return;

    sprframe = &sprdef->spriteframes[0];
    lump     = sprframe->lump[1];

    patch = PatchLoader::CacheNum(wGlobalDir, lump + firstspritelump, PU_CACHE);

    w    = patch->width;
    h    = patch->height;
    toff = patch->topoffset;
    loff = patch->leftoffset;

    V_DrawBox(SPRITEBOX_X, SPRITEBOX_Y, w + 16, h + 16);

    // haleyjd 01/12/04: changed translation handling
    V_DrawPatchTranslated(
        SPRITEBOX_X + 8 + loff, SPRITEBOX_Y + 8 + toff, &subscreen43, patch,
        players[consoleplayer].colormap ? translationtables[(players[consoleplayer].colormap - 1)] : nullptr, false);
}

CONSOLE_COMMAND(mn_player, 0)
{
    MN_StartMenu(&menu_player);
}

/////////////////////////////////////////////////////////////////
//
// Options Menu
//
// Massively re-organised from the original version
//

extern menu_t menu_options;
extern menu_t menu_optionsp2;

static const char *mn_optionpg_names[] = { "io / game options / widgets", "multiplayer / files / menus / info",
                                           nullptr };

static menu_t *mn_options_pages[] = {
    &menu_options,   //
    &menu_optionsp2, //
    nullptr          //
};

static menuitem_t mn_options_items[] = {
    { it_title,  "Options",         nullptr,       "M_OPTION" },
    { it_gap,    nullptr,           nullptr,       nullptr    },
    { it_info,   "Input/Output",    nullptr,       nullptr    },
    { it_runcmd, "Key bindings",    "mn_bindings", nullptr    },
    { it_runcmd, "Mouse Options",   "mn_mouse",    nullptr    },
    { it_runcmd, "Gamepad Options", "mn_gamepad",  nullptr    },
    { it_runcmd, "Video Options",   "mn_video",    nullptr    },
    { it_runcmd, "Sound Options",   "mn_sound",    nullptr    },
    { it_gap,    nullptr,           nullptr,       nullptr    },
    { it_info,   "Game Options",    nullptr,       nullptr    },
    { it_runcmd, "Compatibility",   "mn_compat",   nullptr    },
    { it_runcmd, "Enemies",         "mn_enemies",  nullptr    },
    { it_runcmd, "Weapons",         "mn_weapons",  nullptr    },
    { it_gap,    nullptr,           nullptr,       nullptr    },
    { it_info,   "Game Widgets",    nullptr,       nullptr    },
    { it_runcmd, "HUD Settings",    "mn_hud",      nullptr    },
    { it_runcmd, "Status Bar",      "mn_status",   nullptr    },
    { it_runcmd, "Automap",         "mn_automap",  nullptr    },
    { it_end,    nullptr,           nullptr,       nullptr    }
};

static menuitem_t mn_optionsp2_items[] = {
    { it_title,  "Options",        nullptr,                  "M_OPTION" },
    { it_gap,    nullptr,          nullptr,                  nullptr    },
    { it_info,   "Multiplayer",    nullptr,                  nullptr    },
    { it_runcmd, "Player Setup",   "mn_player",              nullptr    },
    { it_runcmd, "Game Settings",  "mn_gset",                nullptr    },
    { it_gap,    nullptr,          nullptr,                  nullptr    },
    { it_info,   "Game Files",     nullptr,                  nullptr    },
    { it_runcmd, "WAD Options",    "mn_loadwad",             nullptr    },
    { it_runcmd, "Demo Settings",  "mn_demos",               nullptr    },
    { it_runcmd, "Configuration",  "mn_config",              nullptr    },
    { it_gap,    nullptr,          nullptr,                  nullptr    },
    { it_info,   "Menus",          nullptr,                  nullptr    },
    { it_runcmd, "Menu Options",   "mn_menus",               nullptr    },
    { it_runcmd, "Custom Menu",    "mn_dynamenu _MN_Custom", nullptr    },
    { it_gap,    nullptr,          nullptr,                  nullptr    },
    { it_info,   "Information",    nullptr,                  nullptr    },
    { it_runcmd, "About Eternity", "credits",                nullptr    },
    { it_end,    nullptr,          nullptr,                  nullptr    }
};

menu_t menu_options = {
    mn_options_items,                 // menu items
    nullptr,                          // prev page
    &menu_optionsp2,                  // next page
    &menu_options,                    // rootpage
    100,                              // x offset
    15,                               // y offset
    3,                                // starting item: first selectable
    mf_background | mf_centeraligned, // draw background: not a skull menu
    nullptr,                          // no drawer
    mn_optionpg_names,                // TOC names
    mn_options_pages,                 // TOC pages
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
static void MN_InitCustomMenu()
{
    if(!MN_DynamicMenuForName("_MN_Custom"))
    {
        mn_optionsp2_items[13].type        = it_info;
        mn_optionsp2_items[13].description = FC_BRICK "custom menu";
    }
}

menu_t menu_optionsp2 = {
    mn_optionsp2_items,               // menu items
    &menu_options,                    // prev page
    nullptr,                          // next page
    &menu_options,                    // rootpage
    100,                              // x offset
    15,                               // y offset
    3,                                // starting item
    mf_background | mf_centeraligned, // draw background: not a skull menu
    nullptr,                          // no drawer
    mn_optionpg_names,                // TOC names
    mn_options_pages,                 // TOC pages
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

static const char *aspect_ratio_desc[] = { "Legacy", "5:4", "4:3", "3:2", "16:10", "5:3", "WSVGA", "16:9", "21:9" };

VARIABLE_INT(mn_favaspectratio, nullptr, 0, AR_NUMASPECTRATIOS - 1, aspect_ratio_desc);
CONSOLE_VARIABLE(mn_favaspectratio, mn_favaspectratio, 0) {}

// haleyjd 06/19/11: user's favored fullscreen/window setting
int mn_favscreentype;

static const char *screen_type_desc[] = { "windowed", "fs desktop", "exclusive fs" };

VARIABLE_INT(mn_favscreentype, nullptr, 0, MN_NUMSCREENTYPES - 1, screen_type_desc);
CONSOLE_VARIABLE(mn_favscreentype, mn_favscreentype, 0) {}

// Video mode lists, per aspect ratio

// Legacy settings, w/aspect-corrected variants
static const char *legacyModes[] = {
    "320x200",  // Mode Y (16:10 logical, 4:3 physical)
    "320x240",  // QVGA
    "640x400",  // VESA Extension (same as 320x200)
    "640x480",  // VGA
    "960x600",  // x3
    "960x720",  // x3 with aspect ratio correction
    "1280x960", // x4 with aspect ratio correction
    nullptr,
};

// 5:4 modes (1.25 / 0.8)
static const char *fiveFourModes[] = {
    "320x256",   // NES resolution, IIRC
    "640x512",   //
    "800x640",   //
    "960x768",   //
    "1280x1024", // Most common 5:4 mode (name?)
    "1400x1120", //
    "1600x1280", //
    nullptr,
};

// 4:3 modes (1.333... / 0.75)
static const char *fourThreeModes[] = {
    "768x576",   // PAL
    "800x600",   // SVGA
    "1024x768",  // XGA
    "1152x864",  // XGA+
    "1400x1050", // SXGA+
    "1600x1200", // UGA
    "2048x1536", // QXGA
    nullptr,
};

// 3:2 modes (1.5 / 0.666...)
static const char *threeTwoModes[] = {
    "720x480", // NTSC
    "768x512", "960x640", "1152x768", "1280x854", "1440x960", "1680x1120", nullptr,
};

// 16:10 modes (1.6 / 0.625)
static const char *sixteenTenModes[] = {
    "1280x800",  // WXGA
    "1440x900",  //
    "1680x1050", // WSXGA+
    "1920x1200", // WUXGA
    "2560x1600", // WQXGA
    "2880x1800", // Apple Retina Display (current max resolution)
    nullptr,
};

// 5:3 modes (1.666... / 0.6)
static const char *fiveThreeModes[] = {
    "640x384",
    "800x480", // WVGA
    "960x576",
    "1280x768",  // WXGA
    "1400x840",  //
    "1600x960",  //
    "2560x1536", //
    nullptr,
};

// "WSVGA" modes (128:75 or 16:9.375: 1.70666... / 0.5859375)
static const char *wsvgaModes[] = {
    "1024x600", // WSVGA (common netbook resolution)
    nullptr,
};

// 16:9 modes (1.777... / 0.5625)
static const char *sixteenNineModes[] = {
    "854x480",   // WVGA
    "1280x720",  // HD720
    "1360x768",  //
    "1440x810",  //
    "1600x900",  // HD+
    "1920x1080", // HD1080
    "2048x1152",
    "2880x1620", //
    nullptr,
};

// 21:9 modes (2.333... / 0.428571...)
static const char *twentyoneNineModes[] = {
    "1720x720",
    "2560x1080",
    "3440x1440",
    nullptr,
};

// TODO: Not supported as menu choices yet:
// 17:9  (1.888... / 0.5294117647058823...) ex: 2048x1080
// 32:15, or 16:7.5 (2.1333... / 0.46875)   ex: 1280x600

static const char **resListForAspectRatio[AR_NUMASPECTRATIOS] = {
    legacyModes,        // Low-res 16:10, 4:3 modes and their multiples
    fiveFourModes,      // 5:4 - very square
    fourThreeModes,     // 4:3 (standard CRT)
    threeTwoModes,      // 3:2 (similar to European TV)
    sixteenTenModes,    // 16:10, common LCD widescreen monitors
    fiveThreeModes,     // 5:3
    wsvgaModes,         // 128:75 (or 16:9.375), common netbook resolution
    sixteenNineModes,   // 16:9, consumer HD widescreen TVs/monitors
    twentyoneNineModes, // 21:9, consumder HD ultrawide TVs/monitors
};

static int    mn_vidmode_num;
static char **mn_vidmode_desc;
static char **mn_vidmode_cmds;

enum class tableType_e
{
    VIDEO_MODE,
    RESOLUTION
};

//
// haleyjd 06/19/11: Resurrected and restructured to allow choosing modes
// from the precomposited lists above based on the user's favorite aspect
// ratio and fullscreen/windowed settings.
//
static void MN_BuildVidmodeTables(const tableType_e type)
{
    int          useraspect = mn_favaspectratio;
    int          userfs     = mn_favscreentype;
    const char **reslist    = nullptr;
    int          offset     = 0;

    if(mn_vidmode_desc)
    {
        for(int i = 0; i < mn_vidmode_num; i++)
            efree(mn_vidmode_desc[i]);
        efree(mn_vidmode_desc);
        mn_vidmode_desc = nullptr;
    }
    if(mn_vidmode_cmds)
    {
        for(int i = 0; i < mn_vidmode_num; i++)
            efree(mn_vidmode_cmds[i]);
        efree(mn_vidmode_cmds);
        mn_vidmode_cmds = nullptr;
    }

    // pick the list of resolutions to use
    if(useraspect >= 0 && useraspect < AR_NUMASPECTRATIOS)
        reslist = resListForAspectRatio[useraspect];
    else
        reslist = resListForAspectRatio[AR_LEGACY]; // A safe pick.

    // count the modes on that list
    mn_vidmode_num = 0;
    while(reslist[mn_vidmode_num])
        mn_vidmode_num++;

    if(type == tableType_e::RESOLUTION)
    {
        mn_vidmode_num++;
        offset = 1;
    }

    // allocate arrays
    mn_vidmode_desc = ecalloc(char **, mn_vidmode_num + 1, sizeof(const char *));
    mn_vidmode_cmds = ecalloc(char **, mn_vidmode_num + 1, sizeof(const char *));

    if(type == tableType_e::RESOLUTION)
    {
        mn_vidmode_desc[0] = estrdup("native");
        mn_vidmode_cmds[0] = estrdup("i_resolution native");
    }

    const char *cmdstr = (type == tableType_e::RESOLUTION) ? "i_resolution" : "i_videomode";
    for(int i = offset; i < mn_vidmode_num; i++)
    {
        qstring cmd, description;

        description = reslist[i - offset];
        if(type == tableType_e::VIDEO_MODE)
        {
            switch(userfs)
            {
            case MN_FULLSCREEN: //
                description += 'f';
                break;
            case MN_FULLSCREEN_DESKTOP: //
                description += 'd';
                break;
            case MN_WINDOWED:
            default: //
                description += 'w';
                break;
            }
        }

        // set the mode description
        mn_vidmode_desc[i] = description.duplicate();

        cmd << cmdstr << " " << description;

        mn_vidmode_cmds[i] = cmd.duplicate();
    }

    // null-terminate the lists
    mn_vidmode_desc[mn_vidmode_num] = nullptr;
    mn_vidmode_cmds[mn_vidmode_num] = nullptr;
}

CONSOLE_COMMAND(mn_resolution, cf_hidden)
{
    MN_BuildVidmodeTables(tableType_e::RESOLUTION);

    MN_SetupBoxWidget("Choose a Resolution", (const char **)mn_vidmode_desc, boxwidget_command, nullptr,
                      (const char **)mn_vidmode_cmds);
    MN_ShowBoxWidget();
}

CONSOLE_COMMAND(mn_vidmode, cf_hidden)
{
    MN_BuildVidmodeTables(tableType_e::VIDEO_MODE);

    MN_SetupBoxWidget("Choose a Video Mode", (const char **)mn_vidmode_desc, boxwidget_command, nullptr,
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
extern menu_t menu_vidadv;

static const char *mn_vidpage_names[] = { "Mode / Rendering",  "Framerate / Screenshots / Screen Wipe",
                                          "System / Console",  "Particles",
                                          "Advanced / OpenGL", nullptr };

static menu_t *mn_vidpage_menus[] = {
    &menu_video,     //
    &menu_sysvideo,  //
    &menu_video_pg2, //
    &menu_particles, //
    &menu_vidadv,    //
    nullptr,         //
};

static menuitem_t mn_video_items[] = {
    { it_title,    "Video Options",          nullptr,             "m_video" },
    { it_gap,      nullptr,                  nullptr,             nullptr   },
    { it_info,     "Mode",                   nullptr,             nullptr   },
    { it_runcmd,   "Choose a mode...",       "mn_vidmode",        nullptr   },
    { it_variable, "Video mode",             "i_videomode",       nullptr   },
    { it_runcmd,   "Choose a resolution...", "mn_resolution",     nullptr   },
    { it_variable, "Renderer resolution",    "i_resolution",      nullptr   },
    { it_toggle,   "Favorite aspect ratio",  "mn_favaspectratio", nullptr   },
    { it_gap,      nullptr,                  nullptr,             nullptr   },
    { it_info,     "Display Properties",     nullptr,             nullptr   },
    { it_toggle,   "Favorite screen mode",   "mn_favscreentype",  nullptr   },
    { it_toggle,   "Display number",         "displaynum",        nullptr   },
    { it_toggle,   "Vertical sync",          "v_retrace",         nullptr   },
    { it_slider,   "Gamma correction",       "gamma",             nullptr   },
    { it_end,      nullptr,                  nullptr,             nullptr   }
};

menu_t menu_video = {
    mn_video_items,   // menu items
    nullptr,          // prev page
    &menu_sysvideo,   // next page
    &menu_video,      // rootpage
    200,              // x offset
    15,               // y offset
    3,                // start on first selectable
    mf_background,    // fullscreen
    nullptr,          // no drawer
    mn_vidpage_names, // TOC names
    mn_vidpage_menus, // TOC pages
};

CONSOLE_COMMAND(mn_video, 0)
{
    MN_StartMenu(&menu_video);
}

static menuitem_t mn_sysvideo_items[] = {
    { it_title,    "Video Options",           nullptr,          "m_video" },
    { it_gap,      nullptr,                   nullptr,          nullptr   },
    { it_info,     "Rendering",               nullptr,          nullptr   },
    { it_slider,   "Screen size",             "screensize",     nullptr   },
    { it_toggle,   "Renderer threads",        "r_numcontexts",  nullptr   },
    { it_toggle,   "HOM detector flashes",    "r_homflash",     nullptr   },
    { it_toggle,   "Translucency",            "r_trans",        nullptr   },
    { it_variable, "Opacity percentage",      "r_tranpct",      nullptr   },
    { it_toggle,   "Stock Doom object style", "r_tlstyle",      nullptr   },
    { it_toggle,   "Sprite projection style", "r_sprprojstyle", nullptr   },
    { it_gap,      nullptr,                   nullptr,          nullptr   },
    { it_info,     "Framerate",               nullptr,          nullptr   },
    { it_toggle,   "Uncapped framerate",      "d_fastrefresh",  nullptr   },
    { it_toggle,   "Interpolation",           "d_interpolate",  nullptr   },
    { it_gap,      nullptr,                   nullptr,          nullptr   },
    { it_info,     "Screen Wipe",             nullptr,          nullptr   },
    { it_toggle,   "Wipe style",              "wipetype",       nullptr   },
    { it_toggle,   "Game waits for wipe",     "wipewait",       nullptr   },
    { it_end,      nullptr,                   nullptr,          nullptr   }
};

static void MN_SysVideoModeDrawer();

menu_t menu_sysvideo = {
    mn_sysvideo_items,     // menu items
    &menu_video,           // prev page
    &menu_video_pg2,       // next page
    &menu_video,           // rootpage
    200,                   // x offset
    15,                    // y offset
    3,                     // start on first selectable
    mf_background,         // fullscreen
    MN_SysVideoModeDrawer, // drawer
    mn_vidpage_names,      // TOC names
    mn_vidpage_menus,      // TOC pages
};

static void MN_SysVideoModeDrawer()
{
    int            lump, y;
    patch_t       *patch;
    spritedef_t   *sprdef;
    spriteframe_t *sprframe;
    int            frame = E_SafeState(GameModeInfo->transFrame);

    // draw an imp fireball

    // don't draw anything before the menu has been initialized
    if(!(menu_sysvideo.menuitems[5].flags & MENUITEM_POSINIT))
        return;

    sprdef = &sprites[states[frame]->sprite];
    // haleyjd 08/15/02
    if(!(sprdef->spriteframes))
        return;
    sprframe = &sprdef->spriteframes[0];
    lump     = sprframe->lump[0];

    patch = PatchLoader::CacheNum(wGlobalDir, lump + firstspritelump, PU_CACHE);

    // approximately center box on "translucency" item in menu
    y = menu_video.menuitems[5].y - 5;
    V_DrawBox(270, y, 20, 20);
    if(r_tlstyle == R_TLSTYLE_BOOM)
        V_DrawPatchTL(282, y + 12, &subscreen43, patch, nullptr, FTRANLEVEL);
    else if(r_tlstyle == R_TLSTYLE_NEW)
        V_DrawPatchAdd(282, y + 12, &subscreen43, patch, nullptr, FTRANLEVEL);
    else
        V_DrawPatch(282, y + 12, &subscreen43, patch);
}

static menuitem_t mn_video_page2_items[] = {
    { it_title,    "Video Options",       nullptr,            "m_video" },
    { it_gap,      nullptr,               nullptr,            nullptr   },
    { it_info,     "Screenshots",         nullptr,            nullptr   },
    { it_toggle,   "Screenshot format",   "shot_type",        nullptr   },
    { it_toggle,   "Gamma correct shots", "shot_gamma",       nullptr   },
    { it_gap,      nullptr,               nullptr,            nullptr   },
    { it_info,     "System",              nullptr,            nullptr   },
    { it_toggle,   "DOS-like startup",    "textmode_startup", nullptr   },
#ifdef _SDL_VER
    { it_toggle,   "Wait at exit",        "i_waitatexit",     nullptr   },
    { it_toggle,   "Show ENDOOM",         "i_showendoom",     nullptr   },
    { it_variable, "ENDOOM delay",        "i_endoomdelay",    nullptr   },
#endif
    { it_gap,      nullptr,               nullptr,            nullptr   },
    { it_info,     "Console",             nullptr,            nullptr   },
    { it_variable, "Dropdown speed",      "c_speed",          nullptr   },
    { it_variable, "Console size",        "c_height",         nullptr   },
    { it_end,      nullptr,               nullptr,            nullptr   }
};

menu_t menu_video_pg2 = {
    mn_video_page2_items, // menu items
    &menu_sysvideo,       // prev page
    &menu_particles,      // next page
    &menu_video,          // rootpage
    200,                  // x offset
    15,                   // y offset
    3,                    // start on first selectable
    mf_background,        // fullscreen
    nullptr,              // no drawer
    mn_vidpage_names,     // TOC names
    mn_vidpage_menus,     // TOC pages
};

static menuitem_t mn_particles_items[] = {
    { it_title,  "Video Options",            nullptr,          "m_video" },
    { it_gap,    nullptr,                    nullptr,          nullptr   },
    { it_info,   "Particles",                nullptr,          nullptr   },
    { it_toggle, "Render particle effects",  "draw_particles", nullptr   },
    { it_toggle, "Particle translucency",    "r_ptcltrans",    nullptr   },
    { it_gap,    nullptr,                    nullptr,          nullptr   },
    { it_info,   "Effects",                  nullptr,          nullptr   },
    { it_toggle, "Blood splat type",         "bloodsplattype", nullptr   },
    { it_toggle, "Bullet puff type",         "bulletpufftype", nullptr   },
    { it_toggle, "Enable rocket trails",     "rocket_trails",  nullptr   },
    { it_toggle, "Enable grenade trails",    "grenade_trails", nullptr   },
    { it_toggle, "Enable bfg cloud",         "bfg_cloud",      nullptr   },
    { it_toggle, "Enable rocket explosions", "pevt_rexpl",     nullptr   },
    { it_toggle, "Enable bfg explosions",    "pevt_bfgexpl",   nullptr   },
    { it_end,    nullptr,                    nullptr,          nullptr   }
};

menu_t menu_particles = {
    mn_particles_items, // menu items
    &menu_video_pg2,    // prev page
    &menu_vidadv,       // next page
    &menu_video,        // rootpage
    200,                // x offset
    15,                 // y offset
    3,                  // start on first selectable
    mf_background,      // fullscreen
    nullptr,            // no drawer
    mn_vidpage_names,   // TOC names
    mn_vidpage_menus,   // TOC pages
};

CONSOLE_COMMAND(mn_particle, 0)
{
    MN_StartMenu(&menu_particles);
}

// Advanced video option items
static menuitem_t mn_vidadv_items[] = {
    { it_title,    "Video Options",        nullptr,              "m_video" },
    { it_gap,      nullptr,                nullptr,              nullptr   },
    { it_info,     "Advanced",             nullptr,              nullptr   },
    { it_toggle,   "Video driver",         "i_videodriverid",    nullptr   },
    { it_gap,      nullptr,                nullptr,              nullptr   },
    { it_info,     "OpenGL",               nullptr,              nullptr   },
    { it_variable, "GL color depth",       "gl_colordepth",      nullptr   },
    { it_toggle,   "Texture filtering",    "gl_filter_type",     nullptr   },
    { it_toggle,   "Use extensions",       "gl_use_extensions",  nullptr   },
    { it_toggle,   "Use ARB pixelbuffers", "gl_arb_pixelbuffer", nullptr   },
    { it_end,      nullptr,                nullptr,              nullptr   }
};

// Advanced video options
menu_t menu_vidadv = {
    mn_vidadv_items,  // menu items
    &menu_particles,  // prev page
    nullptr,          // next page
    &menu_video,      // rootpage
    200,              // x offset
    15,               // y offset
    3,                // start on first selectable
    mf_background,    // fullscreen
    nullptr,          // no drawer
    mn_vidpage_names, // TOC names
    mn_vidpage_menus, // TOC pages
};

/////////////////////////////////////////////////////////////////
//
// Sound Options
//

extern menu_t menu_sound;
extern menu_t menu_soundeq;

static const char *mn_sndpage_names[] = { "Volume / Setup / Misc", "Equalizer", nullptr };

static menu_t *mn_sndpage_menus[] = {
    &menu_sound,   //
    &menu_soundeq, //
    nullptr        //
};

static menuitem_t mn_sound_items[] = {
    { it_title,  "Sound Options",        nullptr,           "m_sound" },
    { it_gap,    nullptr,                nullptr,           nullptr   },
    { it_info,   "Volume",               nullptr,           nullptr   },
    { it_slider, "Sfx volume",           "sfx_volume",      nullptr   },
    { it_slider, "Music volume",         "music_volume",    nullptr   },
    { it_gap,    nullptr,                nullptr,           nullptr   },
    { it_info,   "Setup",                nullptr,           nullptr   },
    { it_toggle, "Sound driver",         "snd_card",        nullptr   },
    { it_toggle, "Music driver",         "mus_card",        nullptr   },
    { it_toggle, "MIDI device",          "snd_mididevice",  nullptr   },
    { it_toggle, "Max sound channels",   "snd_channels",    nullptr   },
    { it_toggle, "Force reverse stereo", "s_flippan",       nullptr   },
    { it_toggle, "OPL3 Emulator",        "snd_oplemulator", nullptr   },
    { it_toggle, "OPL3 Chip Count",      "snd_numchips",    nullptr   },
    { it_runcmd, "Set OPL3 Bank...",     "snd_selectbank",  nullptr   },
    { it_gap,    nullptr,                nullptr,           nullptr   },
    { it_info,   "Misc",                 nullptr,           nullptr   },
    { it_toggle, "Precache sounds",      "s_precache",      nullptr   },
    { it_toggle, "Pitched sounds",       "s_pitched",       nullptr   },
    { it_end,    nullptr,                nullptr,           nullptr   }
};

menu_t menu_sound = {
    mn_sound_items,   // menu items
    nullptr,          // prev page
    &menu_soundeq,    // next page
    &menu_sound,      // root page
    180,              // x offset
    15,               // y offset
    3,                // first selectable
    mf_background,    // fullscreen
    nullptr,          // no drawer
    mn_sndpage_names, // TOC names
    mn_sndpage_menus, // TOC pages
};

CONSOLE_COMMAND(mn_sound, 0)
{
    MN_StartMenu(&menu_sound);
}

static menuitem_t mn_soundeq_items[] = {
    { it_title,    "Sound Options",    nullptr,      "m_sound" },
    { it_gap,      nullptr,            nullptr,      nullptr   },
    { it_info,     "Equalizer",        nullptr,      nullptr   },
    { it_slider,   "Low band gain",    "s_lowgain",  nullptr   },
    { it_slider,   "Midrange gain",    "s_midgain",  nullptr   },
    { it_slider,   "High band gain",   "s_highgain", nullptr   },
    { it_slider,   "Preamp",           "s_eqpreamp", nullptr   },
    { it_variable, "Low pass cutoff",  "s_lowfreq",  nullptr   },
    { it_variable, "High pass cutoff", "s_highfreq", nullptr   },
    { it_end,      nullptr,            nullptr,      nullptr   }
};

menu_t menu_soundeq = {
    mn_soundeq_items, // menu items
    &menu_sound,      // prev page
    nullptr,          // next page
    &menu_sound,      // root page
    180,              // x offset
    15,               // y offset
    3,                // first selectable
    mf_background,    // fullscreen
    nullptr,          // no drawer
    mn_sndpage_names, // TOC names
    mn_sndpage_menus, // TOC pages
};

/////////////////////////////////////////////////////////////////
//
// Mouse Options
//

static const char *mn_mouse_names[] = { "Mouse Settings", "Acceleration / Mouselook", nullptr };

extern menu_t menu_mouse;
extern menu_t menu_mouse_accel_and_mlook;

static menu_t *mn_mouse_pages[] = {
    &menu_mouse,                 //
    &menu_mouse_accel_and_mlook, //
    nullptr                      //
};

static menuitem_t mn_mouse_items[] = {
    { it_title,    "Mouse Settings",          nullptr,          "M_MOUSE" },
    { it_gap,      nullptr,                   nullptr,          nullptr   },
    { it_toggle,   "Enable mouse",            "i_usemouse",     nullptr   },
    { it_gap,      nullptr,                   nullptr,          nullptr   },
    { it_info,     "Sensitivity",             nullptr,          nullptr   },
    { it_variable, "Horizontal",              "sens_horiz",     nullptr   },
    { it_variable, "Vertical",                "sens_vert",      nullptr   },
    { it_toggle,   "Vanilla sensitivity",     "sens_vanilla",   nullptr   },
    { it_gap,      nullptr,                   nullptr,          nullptr   },
    { it_info,     "Miscellaneous",           nullptr,          nullptr   },
    { it_toggle,   "Invert mouse",            "invertmouse",    nullptr   },
    { it_toggle,   "Smooth turning",          "smooth_turning", nullptr   },
    { it_toggle,   "Vertical mouse movement", "mouse_vert",     nullptr   },
#ifdef _SDL_VER
    { it_toggle,   "Window grabs mouse",      "i_grabmouse",    nullptr   },
#endif
    { it_end,      nullptr,                   nullptr,          nullptr   }
};

menu_t menu_mouse = {
    mn_mouse_items,              // menu items
    nullptr,                     // prev page
    &menu_mouse_accel_and_mlook, // next page
    &menu_mouse,                 // rootpage
    200,                         // x offset
    15,                          // y offset
    2,                           // first selectable
    mf_background,               // fullscreen
    nullptr,                     // no drawer
    mn_mouse_names,              // TOC names
    mn_mouse_pages,              // TOC pages
};

CONSOLE_COMMAND(mn_mouse, 0)
{
    MN_StartMenu(&menu_mouse);
}

static menuitem_t mn_mouse_accel_and_mlook_items[] = {
    { it_title,    "Mouse Settings",     nullptr,                 "M_MOUSE" },
    { it_gap,      nullptr,              nullptr,                 nullptr   },
    { it_info,     "Acceleration",       nullptr,                 nullptr   },
    { it_toggle,   "Type",               "mouse_accel_type",      nullptr   },
    { it_variable, "Custom threshold",   "mouse_accel_threshold", nullptr   },
    { it_variable, "Custom amount",      "mouse_accel_value",     nullptr   },
    { it_gap,      nullptr,              nullptr,                 nullptr   },
    { it_info,     "Mouselook",          nullptr,                 nullptr   },
    { it_toggle,   "Enable mouselook",   "allowmlook",            nullptr   },
    { it_toggle,   "Always mouselook",   "alwaysmlook",           nullptr   },
    { it_binding,  "Bind mouselook key", "mlook",                 nullptr   },
    { it_end,      nullptr,              nullptr,                 nullptr   }
};

menu_t menu_mouse_accel_and_mlook = {
    mn_mouse_accel_and_mlook_items, // menu items
    &menu_mouse,                    // prev page
    nullptr,                        // next page
    &menu_mouse,                    // rootpage
    200,                            // x offset
    15,                             // y offset
    3,                              // firest selectable
    mf_background,                  // fullscreen
    nullptr,                        // no drawer
    mn_mouse_names,                 // TOC names
    mn_mouse_pages,                 // TOC pages
};

/////////////////////////////////////////////////////////////////
//
// Gamepad Options
//

extern menu_t menu_gamepad;
extern menu_t menu_gamepad_axes;

static const char *mn_gamepad_names[] = { "Gamepad Settings", "Gamepad Axis Settings", nullptr };

static menu_t *mn_gamepad_pages[] = {
    &menu_gamepad,      //
    &menu_gamepad_axes, //
    nullptr             //
};

//------------------------------------------------------------------------
//
// Gamepad Configuration Menu
//

static const char **mn_js_desc;
static const char **mn_js_cmds;

static void MN_BuildJSTables()
{
    static bool menu_built = false;

    // don't build multiple times
    if(!menu_built)
    {
        qstring tempstr;
        size_t  numpads = I_GetNumGamePads();

        // allocate arrays
        mn_js_desc = ecalloc(const char **, (numpads + 2), sizeof(char *));
        mn_js_cmds = ecalloc(const char **, (numpads + 2), sizeof(char *));

        // first item is "none", for disabling gamepad input altogether
        mn_js_desc[0] = "none";
        mn_js_cmds[0] = "i_joystick -1";

        // add every active gamepad
        for(size_t jsnum = 0; jsnum < numpads; jsnum++)
        {
            HALGamePad *pad = I_GetGamePad(jsnum);

            mn_js_desc[jsnum + 1] = pad->name.duplicate(PU_STATIC);

            tempstr.Printf(0, "i_joystick %i", pad->num);
            mn_js_cmds[jsnum + 1] = tempstr.duplicate(PU_STATIC);
        }

        // add a null terminator to the end of the list
        mn_js_desc[numpads + 1] = nullptr;
        mn_js_cmds[numpads + 1] = nullptr;

        menu_built = true;
    }
}

CONSOLE_COMMAND(mn_joysticks, cf_hidden)
{
    static char title[256];
    const char *drv_name;
    HALGamePad *pad;
    MN_BuildJSTables();

    if((pad = I_GetActivePad()))
        drv_name = pad->name.constPtr();
    else
        drv_name = "none";

    psnprintf(title, sizeof(title), "Choose a Gamepad\n\nCurrent device:\n  %s", drv_name);

    MN_SetupBoxWidget(title, mn_js_desc, boxwidget_command, nullptr, mn_js_cmds);
    MN_ShowBoxWidget();
}

static const char **mn_prof_desc;
static const char **mn_prof_cmds;

static void MN_buildProfileTables()
{
    static bool menu_built = false;

    // don't build multiple times
    if(!menu_built)
    {
        int numProfiles = wGlobalDir.getNamespace(lumpinfo_t::ns_pads).numLumps;

        if(!numProfiles)
            return;

        // allocate arrays - +1 size for nullptr termination
        mn_prof_desc = ecalloc(const char **, (numProfiles + 1), sizeof(char *));
        mn_prof_cmds = ecalloc(const char **, (numProfiles + 1), sizeof(char *));

        int                  i = 0;
        WadNamespaceIterator wni(wGlobalDir, lumpinfo_t::ns_pads);

        for(wni.begin(); wni.current(); wni.next(), i++)
        {
            qstring     fullname;
            qstring     base;
            qstring     desc;
            lumpinfo_t *lump = wni.current();

            if(lump->lfn)
                fullname = lump->lfn;
            else
                fullname = lump->name;

            size_t dot;
            if((dot = fullname.find(".txt")) != qstring::npos)
                fullname.truncate(dot);

            fullname.extractFileBase(base);

            desc = base;
            desc.replace("_", ' ');

            mn_prof_desc[i] = desc.duplicate();

            base.makeQuoted();
            base.insert("g_padprofile ", 0);
            mn_prof_cmds[i] = base.duplicate();
        }

        mn_prof_desc[numProfiles] = nullptr;
        mn_prof_cmds[numProfiles] = nullptr;

        menu_built = true;
    }
}

CONSOLE_COMMAND(mn_profiles, cf_hidden)
{
    MN_buildProfileTables();

    if(!mn_prof_desc)
    {
        MN_ErrorMsg("No profiles available");
        return;
    }

    MN_SetupBoxWidget("Choose a Profile: ", mn_prof_desc, boxwidget_command, nullptr, mn_prof_cmds);
    MN_ShowBoxWidget();
}

static menuitem_t mn_gamepad_items[] = {
    { it_title,    "Gamepad Settings",           nullptr,                  nullptr },
    { it_gap,      nullptr,                      nullptr,                  nullptr },
    { it_info,     "Devices",                    nullptr,                  nullptr },
    { it_runcmd,   "Select gamepad...",          "mn_joysticks",           nullptr },
    { it_runcmd,   "Test gamepad...",            "mn_padtest",             nullptr },
    { it_gap,      nullptr,                      nullptr,                  nullptr },
    { it_info,     "Settings",                   nullptr,                  nullptr },
    { it_runcmd,   "Load profile...",            "mn_profiles",            nullptr },
    { it_variable, "Turn sensitivity",           "i_joyturnsens",          nullptr },
    { it_variable, "Gamepad l. stick dead zone", "i_joy_deadzone_left",    nullptr },
    { it_variable, "Gamepad r. stick dead zone", "i_joy_deadzone_right",   nullptr },
    { it_variable, "Gamepad trigger dead zone",  "i_joy_deadzone_trigger", nullptr },
    { it_toggle,   "Force feedback",             "i_forcefeedback",        nullptr },
    { it_end,      nullptr,                      nullptr,                  nullptr }
};

menu_t menu_gamepad = {
    mn_gamepad_items,   // menu items
    nullptr,            // prev page
    &menu_gamepad_axes, // next page
    &menu_gamepad,      // rootpage
    200,                // x offset
    15,                 // y offset
    3,                  // first selectable
    mf_background,      // fullscreen
    nullptr,            // no drawer
    mn_gamepad_names,   // TOC names
    mn_gamepad_pages,   // TOC pages
};

CONSOLE_COMMAND(mn_gamepad, 0)
{
    MN_StartMenu(&menu_gamepad);
}

//-----------------------------------------------------------------------------
//
// Pad Test
//

//
// Data for padtest widget.
//
struct mn_padtestdata_t
{
    int   buttonLengths[HALGamePad::MAXBUTTONS];
    bool  buttonStates[HALGamePad::MAXBUTTONS];
    float axisStates[HALGamePad::MAXAXES];
    int   numButtons;
    int   numAxes;
};

static mn_padtestdata_t mn_padtestdata;

//
// MN_padTestDrawer
//
// Drawer for padtest widget.
// Draw active buttons and axes.
//
static void MN_padTestDrawer()
{
    qstring     qstr;
    const char *title      = "Gamepad Test";
    int         x          = 160 - V_FontStringWidth(menu_font_big, title) / 2;
    int         y          = 8;
    int         lineHeight = MN_StringHeight("Mg") + 4;

    // draw background
    V_DrawBackground(mn_background_flat, &vbscreen);

    // draw title
    if(GameModeInfo->flags & GIF_SHADOWTITLES)
        V_FontWriteTextShadowed(menu_font_big, title, x, y, &subscreen43, GameModeInfo->titleColor);
    else
        V_FontWriteTextColored(menu_font_big, title, GameModeInfo->titleColor, x, y, &subscreen43);

    y += V_FontStringHeight(menu_font_big, title) + 16;

    // draw axes
    x = 8;
    MN_WriteText("Axes:", x, y);
    y += lineHeight;
    for(int i = 0; i < mn_padtestdata.numAxes && i < HALGamePad::MAXAXES; i++)
    {
        int color = mn_padtestdata.axisStates[i] > 0 ? GameModeInfo->selectColor :
                    mn_padtestdata.axisStates[i] < 0 ? GameModeInfo->unselectColor :
                                                       GameModeInfo->infoColor;

        qstr.Printf(16, "A%d=%.2f", i + 1, mn_padtestdata.axisStates[i]);
        MN_WriteTextColored(qstr.constPtr(), color, x, y);
        x += MN_StringWidth(qstr.constPtr()) + 8;

        if(x > 300 && i != mn_padtestdata.numAxes - 1)
        {
            x  = 8;
            y += lineHeight;
        }
    }

    // draw buttons
    y += 2 * lineHeight;
    x  = 8;
    MN_WriteText("Buttons:", x, y);
    y += MN_StringHeight("Buttons:") + 4;
    for(int i = 0; i < mn_padtestdata.numButtons && HALGamePad::MAXBUTTONS; i++)
    {
        int color = mn_padtestdata.buttonStates[i] ? GameModeInfo->selectColor : GameModeInfo->infoColor;

        qstr.clear() << "B" << (i + 1);
        MN_WriteTextColored(qstr.constPtr(), color, x, y);
        x += MN_StringWidth(qstr.constPtr()) + 8;

        if(x > 300 && i != mn_padtestdata.numButtons - 1)
        {
            x  = 8;
            y += lineHeight;
        }
    }

    const char *help1  = "Press ESC on keyboard or hold any";
    x                  = 160 - MN_StringWidth(help1) / 2;
    y                 += 2 * lineHeight;
    MN_WriteTextColored(help1, GameModeInfo->infoColor, x, y);
    const char *help2  = "controller button 5 seconds to exit";
    x                  = 160 - MN_StringWidth(help2) / 2;
    y                 += lineHeight;
    MN_WriteTextColored(help2, GameModeInfo->infoColor, x, y);
}

//
// MN_padTestTicker
//
// Ticker for padtest widget.
// Refresh the input state by reading from the active gamepad.
//
static void MN_padTestTicker()
{
    HALGamePad *gamepad = I_GetActivePad();
    if(!gamepad)
    {
        mn_padtestdata.numAxes    = 0;
        mn_padtestdata.numButtons = 0;
        return; // woops!?
    }

    mn_padtestdata.numAxes    = gamepad->numAxes;
    mn_padtestdata.numButtons = gamepad->numButtons;

    for(int i = 0; i < mn_padtestdata.numAxes && i < HALGamePad::MAXAXES; i++)
        mn_padtestdata.axisStates[i] = gamepad->state.axes[i];
    for(int i = 0; i < mn_padtestdata.numButtons && i < HALGamePad::MAXBUTTONS; i++)
    {
        mn_padtestdata.buttonStates[i] = gamepad->state.buttons[i];

        // kill the widget if any button is held for 5 seconds
        if(mn_padtestdata.buttonStates[i])
        {
            mn_padtestdata.buttonLengths[i]++;
            if(mn_padtestdata.buttonLengths[i] > TICRATE * 5)
            {
                S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_DEACTIVATE]);
                MN_PopWidget();
            }
        }
        else
            mn_padtestdata.buttonLengths[i] = 0;
    }
}

//
// MN_padTestResponder
//
static bool MN_padTestResponder(event_t *ev, int action)
{
    // only interested in keydown events
    if(ev->type != ev_keydown)
        return false;

    if(ev->data1 == KEYD_ESCAPE) // Must be keyboard ESC
    {
        // kill the widget
        S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_DEACTIVATE]);
        MN_PopWidget();
    }

    // eat other events
    return true;
}

static menuwidget_t padtest_widget = {
    MN_padTestDrawer,    //
    MN_padTestResponder, //
    MN_padTestTicker,    //
    true                 //
};

CONSOLE_COMMAND(mn_padtest, 0)
{
    if(!I_GetActivePad())
    {
        MN_ErrorMsg("No active gamepad");
        return;
    }

    for(int i = 0; i < HALGamePad::MAXBUTTONS; i++)
        mn_padtestdata.buttonLengths[i] = 0;

    MN_PushWidget(&padtest_widget);
}

//------------------------------------------------------------------------
//
// Gamepad Axis Configuration Menu
//

//
// The menu content. NOTE: this is only for show and keeping content; otherwise it gets dynamically
// updated
//
static menuitem_t mn_gamepad_axes_placeholder[] = {
    { it_title, "Gamepad Axis Settings", nullptr, nullptr, 0                 },
    { it_gap,   nullptr,                 nullptr, nullptr, 0                 },
    { it_info,  "Not configured",        nullptr, nullptr, MENUITEM_CENTERED }, // DUMMY
    { it_end,   nullptr,                 nullptr, nullptr, 0                 },
};

menu_t menu_gamepad_axes = {
    mn_gamepad_axes_placeholder, // menu items
    &menu_gamepad,               // prev page
    nullptr,                     // next page
    &menu_gamepad,               // rootpage
    200,                         // x offset
    15,                          // y offset
    2,                           // first selectable
    mf_background,               // fullscreen
    nullptr,                     // no drawer
    mn_gamepad_names,            // TOC names
    mn_gamepad_pages,            // TOC pages
};

//
// Called when the current gamepad is changed
//
void MN_UpdateGamepadMenus()
{
    struct menuentry_t
    {
        char label[32];
        char variable[32];
    };

    static const menuitem_t title    = { it_title, "Gamepad Axis Settings" };
    static const menu_t     basemenu = {
        nullptr,          // menu items
        &menu_gamepad,    // prev page
        nullptr,          // next page
        &menu_gamepad,    // rootpage
        200,              // x offset
        15,               // y offset
        2,                // first selectable
        mf_background,    // fullscreen
        nullptr,          // no drawer
        mn_gamepad_names, // TOC names
        mn_gamepad_pages, // TOC pages
    };

    HALGamePad *pad = I_GetActivePad();
    if(!pad || !pad->numAxes)
    {
        static menuitem_t noitems[] = {
            title,
            { it_gap,  nullptr,                nullptr, nullptr, 0                 },
            { it_info, "No gamepad selected.", nullptr, nullptr, MENUITEM_CENTERED },
            { it_end,  nullptr,                nullptr, nullptr, 0                 }
        };

        menu_gamepad_axes           = basemenu;
        menu_gamepad_axes.menuitems = noitems;
        return;
    }

    // All collections need to be static
    static PODCollection<menuentry_t> entries;
    entries.makeEmpty();

    for(int i = 0; i < pad->numAxes; ++i)
    {
        menuentry_t &entry = entries.addNew();
        snprintf(entry.label, sizeof(entry.label), "%s action", pad->getAxisName(i));
        snprintf(entry.variable, sizeof(entry.variable), "g_axisaction%d", i + 1);
    }
    for(int i = 0; i < pad->numAxes; ++i)
    {
        menuentry_t &entry = entries.addNew();
        snprintf(entry.label, sizeof(entry.label), "%s orientation", pad->getAxisName(i));
        snprintf(entry.variable, sizeof(entry.variable), "g_axisorientation%d", i + 1);
    }
    // TODO: add the options from Descent Rebirth

    enum
    {
        PER_PAGE = 16
    };

    struct menuitemtable_t
    {
        menuitem_t items[PER_PAGE + 3];
    };

    static PODCollection<menu_t>          pages;
    static PODCollection<menuitemtable_t> pageitems;
    pages.clear();
    pageitems.clear();

    int numpages = (static_cast<int>(entries.getLength()) - 1) / PER_PAGE + 1;
    for(int i = 0; i < numpages; ++i)
    {
        menu_t          &page    = pages.addNew();
        menuitemtable_t &content = pageitems.addNew();

        content.items[0] = title;
        content.items[1] = { it_gap };
        for(int j = 0; j < PER_PAGE; ++j)
        {
            int entryindex = i * PER_PAGE + j;
            if(entryindex >= static_cast<int>(entries.getLength()))
                break;
            content.items[2 + j] = { it_toggle, entries[entryindex].label, entries[entryindex].variable };
        }
        content.items[PER_PAGE + 2] = { it_end };

        // Defer menuitems/prevpage/nextpage for after collection finish
        page = basemenu;
    }
    for(int i = 0; i < numpages; ++i)
        pages[i].menuitems = pageitems[i].items;
    pages[0].prevpage = &menu_gamepad;
    for(int i = 1; i < numpages; ++i)
        pages[i].prevpage = &pages[i - 1];
    for(int i = 0; i < numpages - 1; ++i)
        pages[i].nextpage = &pages[i + 1];

    // HACK: just copy the first page into menu_gamepad_axes. It will be a duplicate, yes, but
    // the TOC looks for this global object, and we don't want to mess with the C-like TOC structure
    // here
    menu_gamepad_axes = pages[0];
}

//=============================================================================
//
// HUD Settings
//

static void MN_HUDPg2Drawer(void);

static const char *mn_hud_names[] = { "messages / BOOM HUD", "crosshair / automap / misc", nullptr };

extern menu_t menu_hud;
extern menu_t menu_hud_pg2;

static menu_t *mn_hud_pages[] = {
    &menu_hud,     //
    &menu_hud_pg2, //
    nullptr        //
};

static menuitem_t mn_hud_items[] = {
    { it_title,    "HUD Settings",        nullptr,               "m_hud" },
    { it_gap,      nullptr,               nullptr,               nullptr },
    { it_info,     "Message Options",     nullptr,               nullptr },
    { it_toggle,   "Messages",            "hu_messages",         nullptr },
    { it_toggle,   "Message alignment",   "hu_messagealignment", nullptr },
    { it_toggle,   "Message color",       "hu_messagecolor",     nullptr },
    { it_toggle,   "Messages scroll",     "hu_messagescroll",    nullptr },
    { it_toggle,   "Message lines",       "hu_messagelines",     nullptr },
    { it_variable, "Message time (ms)",   "hu_messagetime",      nullptr },
    { it_toggle,   "Obituaries",          "hu_obituaries",       nullptr },
    { it_toggle,   "Obituary color",      "hu_obitcolor",        nullptr },
    { it_gap,      nullptr,               nullptr,               nullptr },
    { it_info,     "HUD Overlay options", nullptr,               nullptr },
    { it_toggle,   "Overlay type",        "hu_overlayid",        nullptr },
    { it_toggle,   "Overlay layout",      "hu_overlaystyle",     nullptr },
    { it_toggle,   "Hide stats",          "hu_hidestats",        nullptr },
    { it_toggle,   "Hide secrets",        "hu_hidesecrets",      nullptr },
    { it_end,      nullptr,               nullptr,               nullptr }
};

static menuitem_t mn_hud_pg2_items[] = {
    { it_title,  "HUD Settings",             nullptr,               "m_hud" },
    { it_gap,    nullptr,                    nullptr,               nullptr },
    { it_info,   "Crosshair Options",        nullptr,               nullptr },
    { it_toggle, "Crosshair type",           "hu_crosshair",        nullptr },
    { it_toggle, "Monster highlighting",     "hu_crosshair_hilite", nullptr },
    { it_toggle, "Crosshair scaling",        "hu_crosshair_scale",  nullptr },
    { it_gap,    nullptr,                    nullptr,               nullptr },
    { it_info,   "Automap Options",          nullptr,               nullptr },
    { it_toggle, "Show coords widget",       "hu_showcoords",       nullptr },
    { it_toggle, "Coords follow pointer",    "map_coords",          nullptr },
    { it_toggle, "Coords color",             "hu_coordscolor",      nullptr },
    { it_toggle, "Show level time widget",   "hu_showtime",         nullptr },
    { it_toggle, "Level time color",         "hu_timecolor",        nullptr },
    { it_toggle, "Level name color",         "hu_levelnamecolor",   nullptr },
    { it_gap,    nullptr,                    nullptr,               nullptr },
    { it_info,   "Miscellaneous",            nullptr,               nullptr },
    { it_toggle, "Show frags in deathmatch", "show_scores",         nullptr },
    { it_end,    nullptr,                    nullptr,               nullptr }
};

menu_t menu_hud = {
    mn_hud_items,  // menu items
    nullptr,       // prev page
    &menu_hud_pg2, // next page
    &menu_hud,     // rootpage
    200,           // x offset
    15,            // y offset
    3,             // first selectable
    mf_background, // fullscreen
    nullptr,       // no drawer
    mn_hud_names,  // TOC names
    mn_hud_pages,  // TOC pages
};

menu_t menu_hud_pg2 = {
    mn_hud_pg2_items,
    &menu_hud, // prev page
    nullptr,   // next page
    &menu_hud, // rootpage
    200,
    15, // x,y offset
    3,
    mf_background,
    MN_HUDPg2Drawer, // drawer
    mn_hud_names,    // TOC stuff
    mn_hud_pages,
};

extern int crosshairs[CROSSHAIRS];

static void MN_HUDPg2Drawer(void)
{
    int      y;
    patch_t *patch    = nullptr;
    int      xhairnum = crosshairnum - 1;

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
        VBuffer &buffer = crosshair_scale ? subscreen43 : vbscreenfullres;

        const int16_t w  = patch->width;
        const int16_t h  = patch->height;
        const int16_t to = patch->topoffset;
        const int16_t lo = patch->leftoffset;

        const int crossX = buffer.mapXFromOther(270 + 12, subscreen43) - (w >> 1) + lo;
        const int crossY = buffer.mapYFromOther(y + 12, subscreen43) - (h >> 1) + to;

        V_DrawPatchTL(crossX, crossY, &buffer, patch, colrngs[CR_RED], FTRANLEVEL);
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

static menuitem_t mn_statusbar_items[] = {
    { it_title,    "Status Bar",             nullptr,         "M_STATUS" },
    { it_gap,      nullptr,                  nullptr,         nullptr    },
    { it_toggle,   "Numbers like DOOM",      "st_rednum",     nullptr    },
    { it_toggle,   "Percent sign grey",      "st_graypct",    nullptr    },
    { it_toggle,   "Single key display",     "st_singlekey",  nullptr    },
    { it_gap,      nullptr,                  nullptr,         nullptr    },
    { it_info,     "Status Bar Colors",      nullptr,         nullptr    },
    { it_variable, "Ammo ok percentage",     "ammo_yellow",   nullptr    },
    { it_variable, "Ammo low percentage",    "ammo_red",      nullptr    },
    { it_gap,      nullptr,                  nullptr,         nullptr    },
    { it_variable, "Armor high percentage",  "armor_green",   nullptr    },
    { it_variable, "Armor ok percentage",    "armor_yellow",  nullptr    },
    { it_variable, "Armor low percentage",   "armor_red",     nullptr    },
    { it_gap,      nullptr,                  nullptr,         nullptr    },
    { it_variable, "Health high percentage", "health_green",  nullptr    },
    { it_variable, "Health ok percentage",   "health_yellow", nullptr    },
    { it_variable, "Health low percentage",  "health_red",    nullptr    },
    { it_end,      nullptr,                  nullptr,         nullptr    }
};

menu_t menu_statusbar = {
    mn_statusbar_items, // menu items
    nullptr,            // prev page
    nullptr,            // next page
    nullptr,            // rootpage
    200,                // x offset
    15,                 // y offset
    2,                  // first selectable
    mf_background,      // fullscreen
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

static const char *mn_automap_names[] = {
    "background and lines",    //
    "floor and ceiling lines", //
    "sprites",                 //
    "portals",                 //
    nullptr                    //
};

static menu_t *mn_automap_pages[] = {
    &menu_automapcol1, //
    &menu_automapcol2, //
    &menu_automapcol3, //
    &menu_automap4,    //
    nullptr            //
};

static menuitem_t mn_automapcolbgl_items[] = {
    { it_title,       "Automap",              nullptr,         "m_auto", 0                 },
    { it_gap,         nullptr,                nullptr,         nullptr,  0                 },
    { it_toggle,      "Overlay automap",      "am_overlay",    nullptr,  0                 },
    { it_gap,         nullptr,                nullptr,         nullptr,  0                 },
    { it_info,        "Background and Lines", nullptr,         nullptr,  MENUITEM_CENTERED },
    { it_gap,         nullptr,                nullptr,         nullptr,  0                 },
    { it_automap,     "Background color",     "mapcolor_back", nullptr,  0                 },
    { it_automap,     "Crosshair",            "mapcolor_hair", nullptr,  0                 },
    { it_automap,     "Grid",                 "mapcolor_grid", nullptr,  0                 },
    { it_automap,     "One-sided walls",      "mapcolor_wall", nullptr,  0                 },
    { it_automap_opt, "Teleporter",           "mapcolor_tele", nullptr,  0                 },
    { it_automap_opt, "Secret area boundary", "mapcolor_secr", nullptr,  0                 },
    { it_automap_opt, "Exit",                 "mapcolor_exit", nullptr,  0                 },
    { it_automap,     "Unseen line",          "mapcolor_unsn", nullptr,  0                 },
    { it_end,         nullptr,                nullptr,         nullptr,  0                 }
};

static menuitem_t mn_automapcoldoor_items[] = {
    { it_title,       "Automap",                  nullptr,         "m_auto", 0                 },
    { it_gap,         nullptr,                    nullptr,         nullptr,  0                 },
    { it_info,        "Floor and Ceiling Lines",  nullptr,         nullptr,  MENUITEM_CENTERED },
    { it_gap,         nullptr,                    nullptr,         nullptr,  0                 },
    { it_automap,     "Change in floor height",   "mapcolor_fchg", nullptr,  0                 },
    { it_automap,     "Change in ceiling height", "mapcolor_cchg", nullptr,  0                 },
    { it_automap_opt, "Floor and ceiling equal",  "mapcolor_clsd", nullptr,  0                 },
    { it_automap_opt, "No change in height",      "mapcolor_flat", nullptr,  0                 },
    { it_automap_opt, "Red locked door",          "mapcolor_rdor", nullptr,  0                 },
    { it_automap_opt, "Yellow locked door",       "mapcolor_ydor", nullptr,  0                 },
    { it_automap_opt, "Blue locked door",         "mapcolor_bdor", nullptr,  0                 },
    { it_end,         nullptr,                    nullptr,         nullptr,  0                 }
};

static menuitem_t mn_automapcolsprite_items[] = {
    { it_title,   "Automap",        nullptr,         "m_auto", 0                 },
    { it_gap,     nullptr,          nullptr,         nullptr,  0                 },
    { it_info,    "Sprites",        nullptr,         nullptr,  MENUITEM_CENTERED },
    { it_gap,     nullptr,          nullptr,         nullptr,  0                 },
    { it_automap, "Normal objects", "mapcolor_sprt", nullptr,  0                 },
    { it_automap, "Friends",        "mapcolor_frnd", nullptr,  0                 },
    { it_automap, "Player arrow",   "mapcolor_sngl", nullptr,  0                 },
    { it_automap, "Red key",        "mapcolor_rkey", nullptr,  0                 },
    { it_automap, "Yellow key",     "mapcolor_ykey", nullptr,  0                 },
    { it_automap, "Blue key",       "mapcolor_bkey", nullptr,  0                 },
    { it_end,     nullptr,          nullptr,         nullptr,  0                 }
};

static menuitem_t mn_automapportal_items[] = {
    { it_title,       "Automap",                nullptr,             "m_auto", 0                 },
    { it_gap,         nullptr,                  nullptr,             nullptr,  0                 },
    { it_info,        "Portals",                nullptr,             nullptr,  MENUITEM_CENTERED },
    { it_gap,         nullptr,                  nullptr,             nullptr,  0                 },
    { it_toggle,      "Overlay linked portals", "mapportal_overlay", nullptr,  0                 },
    { it_automap_opt, "Overlay line color",     "mapcolor_prtl",     nullptr,  0                 },
    { it_end,         nullptr,                  nullptr,             nullptr,  0                 },
};

menu_t menu_automapcol1 = {
    mn_automapcolbgl_items, // menu items
    nullptr,                // prev page
    &menu_automapcol2,      // next page
    &menu_automapcol1,      // rootpage
    200,                    // x offset
    15,                     // y offset
    2,                      // starting item
    mf_background,          // fullscreen
    nullptr,                // no drawer
    mn_automap_names,       // TOC names
    mn_automap_pages,       // TOC pages
};

menu_t menu_automapcol2 = {
    mn_automapcoldoor_items, // menu items
    &menu_automapcol1,       // prev page
    &menu_automapcol3,       // next page
    &menu_automapcol1,       // rootpage
    200,                     // x offset
    15,                      // y offset
    4,                       // starting item
    mf_background,           // fullscreen
    nullptr,                 // no drawer
    mn_automap_names,        // TOC names
    mn_automap_pages,        // TOC pages
};

menu_t menu_automapcol3 = {
    mn_automapcolsprite_items, // menu items
    &menu_automapcol2,         // prev page
    &menu_automap4,            // next page
    &menu_automapcol1,         // rootpage
    200,                       // x offset
    15,                        // y offset
    4,                         // starting item
    mf_background,             // fullscreen
    nullptr,                   // no drawer
    mn_automap_names,          // TOC names
    mn_automap_pages,          // TOC pages
};

menu_t menu_automap4 = {
    mn_automapportal_items, // menu items
    &menu_automapcol3,      // prev page
    nullptr,                // next page
    &menu_automapcol1,      // rootpage
    200,                    // x offset
    15,                     // y offset
    4,                      // starting item
    mf_background,          // fullscreen
    nullptr,                // no drawer
    mn_automap_names,       // TOC names
    mn_automap_pages,       // TOC pages
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

static const char *mn_weapons_names[] = {
    "options", //
    nullptr    //
};

static menu_t *mn_weapons_pages[] = {
    &menu_weapons, //
    nullptr        //
};

static menuitem_t mn_weapons_items[] = {
    { it_title,  "Weapons",                nullptr,                 "M_WEAP", 0                 },
    { it_gap,    nullptr,                  nullptr,                 nullptr,  0                 },
    { it_info,   "Options",                nullptr,                 nullptr,  MENUITEM_CENTERED },
    { it_gap,    nullptr,                  nullptr,                 nullptr,  0                 },
    { it_toggle, "Bfg type",               "bfgtype",               nullptr,  0                 },
    { it_toggle, "Bobbing",                "bobbing",               nullptr,  0                 },
    { it_toggle, "Center when firing",     "r_centerfire",          nullptr,  0                 },
    { it_toggle, "Recoil",                 "recoil",                nullptr,  0                 },
    { it_toggle, "Weapon hotkey cycling",  "weapon_hotkey_cycling", nullptr,  0                 },
    { it_toggle, "Cycle when holding key", "weapon_hotkey_holding", nullptr,  0                 },
    { it_toggle, "Autoaiming",             "autoaim",               nullptr,  0                 },
    { it_gap,    nullptr,                  nullptr,                 nullptr,  0                 },
    { it_end,    nullptr,                  nullptr,                 nullptr,  0                 },
};

menu_t menu_weapons = {
    mn_weapons_items, // menu items
    nullptr,          // prev page
    nullptr,          // next page
    &menu_weapons,    // rootpage
    200,              // x offset
    15,               // y offset
    4,                // starting item
    mf_background,    // fullscreen
    nullptr,          // no drawer
    mn_weapons_names, // TOC names
    mn_weapons_pages, // TOC pages
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

// table of contents arrays

static const char *mn_compat_contents[] = {
    "players / monster ai", //
    "simulation",           //
    "maps",                 //
    nullptr                 //
};

static menu_t *mn_compat_pages[] = {
    &menu_compat1, //
    &menu_compat2, //
    &menu_compat3, //
    nullptr        //
};

static menuitem_t mn_compat1_items[] = {
    { it_title,  "Compatibility",                     nullptr,           "m_compat", 0                 },
    { it_gap,    nullptr,                             nullptr,           nullptr,    0                 },
    { it_info,   "Players",                           nullptr,           nullptr,    MENUITEM_CENTERED },
    { it_toggle, "God mode is not absolute",          "comp_god",        nullptr,    0                 },
    { it_toggle, "Powerup cheats are time limited",   "comp_infcheat",   nullptr,    0                 },
    { it_toggle, "Sky is normal when invulnerable",   "comp_skymap",     nullptr,    0                 },
    { it_toggle, "Zombie players can exit levels",    "comp_zombie",     nullptr,    0                 },
    { it_toggle, "Disable unintended player jumping", "comp_aircontrol", nullptr,    0                 },
    { it_gap,    nullptr,                             nullptr,           nullptr,    0                 },
    { it_info,   "Monster AI",                        nullptr,           nullptr,    MENUITEM_CENTERED },
    { it_toggle, "Arch-viles can create ghosts",      "comp_vile",       nullptr,    0                 },
    { it_toggle, "Pain elemental lost souls limited", "comp_pain",       nullptr,    0                 },
    { it_toggle, "Lost souls get stuck in walls",     "comp_skull",      nullptr,    0                 },
    { it_toggle, "Lost souls never bounce on floors", "comp_soul",       nullptr,    0                 },
    { it_toggle, "Monsters randomly walk off lifts",  "comp_staylift",   nullptr,    0                 },
    { it_toggle, "Monsters get stuck on door tracks", "comp_doorstuck",  nullptr,    0                 },
    { it_toggle, "Monsters give up pursuit",          "comp_pursuit",    nullptr,    0                 },
    { it_end,    nullptr,                             nullptr,           nullptr,    0                 }
};

static menuitem_t mn_compat2_items[] = {
    { it_title,  "Compatibility",                       nullptr,           "m_compat", 0                 },
    { it_gap,    nullptr,                               nullptr,           nullptr,    0                 },
    { it_info,   "Simulation",                          nullptr,           nullptr,    MENUITEM_CENTERED },
    { it_toggle, "Actors get stuck over dropoffs",      "comp_dropoff",    nullptr,    0                 },
    { it_toggle, "Actors never fall off ledges",        "comp_falloff",    nullptr,    0                 },
    { it_toggle, "Spawncubes telefrag on MAP30 only",   "comp_telefrag",   nullptr,    0                 },
    { it_toggle, "Monsters can respawn outside map",    "comp_respawnfix", nullptr,    0                 },
    { it_toggle, "Disable terrain types",               "comp_terrain",    nullptr,    0                 },
    { it_toggle, "Disable falling damage",              "comp_fallingdmg", nullptr,    0                 },
    { it_toggle, "Actors have infinite height",         "comp_overunder",  nullptr,    0                 },
    { it_toggle, "Doom actor heights are inaccurate",   "comp_theights",   nullptr,    0                 },
    { it_toggle, "Bullets never hit floors & ceilings", "comp_planeshoot", nullptr,    0                 },
    { it_toggle, "Respawns are sometimes silent in DM", "comp_ninja",      nullptr,    0                 },
    { it_end,    nullptr,                               nullptr,           nullptr,    0                 }
};

static menuitem_t mn_compat3_items[] = {
    { it_title,  "Compatibility",                       nullptr,          "m_compat", 0                 },
    { it_gap,    nullptr,                               nullptr,          nullptr,    0                 },
    { it_info,   "Maps",                                nullptr,          nullptr,    MENUITEM_CENTERED },
    { it_toggle, "Turbo doors make two closing sounds", "comp_blazing",   nullptr,    0                 },
    { it_toggle, "Disable tagged door light fading",    "comp_doorlight", nullptr,    0                 },
    { it_toggle, "Use DOOM stairbuilding method",       "comp_stairs",    nullptr,    0                 },
    { it_toggle, "Use DOOM floor motion behavior",      "comp_floors",    nullptr,    0                 },
    { it_toggle, "Use DOOM linedef trigger model",      "comp_model",     nullptr,    0                 },
    { it_toggle, "Line effects work on sector tag 0",   "comp_zerotags",  nullptr,    0                 },
    { it_toggle, "One-time line effects can break",     "comp_special",   nullptr,    0                 },
    { it_end,    nullptr,                               nullptr,          nullptr,    0                 }
};

menu_t menu_compat1 = {
    mn_compat1_items,   // menu items
    nullptr,            // prev page
    &menu_compat2,      // next page
    &menu_compat1,      // rootpage
    270,                // x offset
    5,                  // y offset
    3,                  // starting item
    mf_background,      // fullscreen
    nullptr,            // no drawer
    mn_compat_contents, // TOC names
    mn_compat_pages,    // TOC pages
};

menu_t menu_compat2 = {
    mn_compat2_items,   // menu items
    &menu_compat1,      // prev page
    &menu_compat3,      // next page
    &menu_compat1,      // rootpage
    270,                // x offset
    5,                  // y offset
    3,                  // starting item
    mf_background,      // fullscreen
    nullptr,            // no drawer
    mn_compat_contents, // TOC names
    mn_compat_pages,    // TOC pages
};

menu_t menu_compat3 = {
    mn_compat3_items,   // menu items
    &menu_compat2,      // prev page
    nullptr,            // next page
    &menu_compat1,      // rootpage
    270,                // x offset
    5,                  // y offset
    3,                  // starting item
    mf_background,      // fullscreen
    nullptr,            // no drawer
    mn_compat_contents, // TOC names
    mn_compat_pages,    // TOC pages
};

CONSOLE_COMMAND(mn_compat, 0)
{
    MN_StartMenu(&menu_compat1);
}

/////////////////////////////////////////////////////////////////
//
// Enemies
//

static menuitem_t mn_enemies_items[] = {
    { it_title,    "Enemies",                  nullptr,           "m_enem" },
    { it_gap,      nullptr,                    nullptr,           nullptr  },
    { it_info,     "Monster options",          nullptr,           nullptr  },
    { it_toggle,   "Monsters remember target", "mon_remember",    nullptr  },
    { it_toggle,   "Monster infighting",       "mon_infight",     nullptr  },
    { it_toggle,   "Monsters back out",        "mon_backing",     nullptr  },
    { it_toggle,   "Monsters avoid hazards",   "mon_avoid",       nullptr  },
    { it_toggle,   "Affected by friction",     "mon_friction",    nullptr  },
    { it_toggle,   "Climb tall stairs",        "mon_climb",       nullptr  },
    { it_gap,      nullptr,                    nullptr,           nullptr  },
    { it_info,     "MBF Friend Options",       nullptr,           nullptr  },
    { it_variable, "Friend distance",          "mon_distfriend",  nullptr  },
    { it_toggle,   "Rescue dying friends",     "mon_helpfriends", nullptr  },
    { it_variable, "Number of helper dogs",    "numhelpers",      nullptr  },
    { it_toggle,   "Dogs can jump down",       "dogjumping",      nullptr  },
    { it_end,      nullptr,                    nullptr,           nullptr  }
};

menu_t menu_enemies = {
    mn_enemies_items, // menu items
    nullptr,          // prev page
    nullptr,          // next page
    nullptr,          // rootpage
    220,              // x offset
    15,               // y offset
    3,                // starting item
    mf_background     // fullscreen
};

CONSOLE_COMMAND(mn_enemies, 0)
{
    MN_StartMenu(&menu_enemies);
}

//------------------------------------------------------------------------
//
// haleyjd 10/15/05: table of contents for key bindings menu pages
//

static const char *mn_binding_contentnames[] = { "basic movement", "advanced movement", "weapon keys",
                                                 "environment",    "game functions",    "menu keys",
                                                 "automap keys",   "console keys",      nullptr };

// all in this file, but require a forward prototype
extern menu_t menu_movekeys;
extern menu_t menu_advkeys;
extern menu_t menu_weaponbindings;
extern menu_t menu_envinvbindings;
extern menu_t menu_funcbindings;
extern menu_t menu_menukeys;
extern menu_t menu_automapkeys;
extern menu_t menu_consolekeys;

static menu_t *mn_binding_contentpages[] = {
    &menu_movekeys,       //
    &menu_advkeys,        //
    &menu_weaponbindings, //
    &menu_envinvbindings, //
    &menu_funcbindings,   //
    &menu_menukeys,       //
    &menu_automapkeys,    //
    &menu_consolekeys,    //
    nullptr,              //
};

static menuitem_t menu_bindings_items[] = {
    { it_title,  "Key Bindings",              nullptr,          "M_KEYBND" },
    { it_gap,    nullptr,                     nullptr,          nullptr    },
    { it_info,   "Gameplay",                  nullptr,          nullptr    },
    { it_runcmd, "Basic Movement",            "mn_movekeys",    nullptr    },
    { it_runcmd, "Advanced Movement",         "mn_advkeys",     nullptr    },
    { it_runcmd, "Weapons",                   "mn_weaponkeys",  nullptr    },
    { it_runcmd, "Inventory and Environment", "mn_envkeys",     nullptr    },
    { it_gap,    nullptr,                     nullptr,          nullptr    },
    { it_info,   "System",                    nullptr,          nullptr    },
    { it_runcmd, "Functions",                 "mn_gamefuncs",   nullptr    },
    { it_runcmd, "Menus",                     "mn_menukeys",    nullptr    },
    { it_runcmd, "Maps",                      "mn_automapkeys", nullptr    },
    { it_runcmd, "Console",                   "mn_consolekeys", nullptr    },
    { it_end,    nullptr,                     nullptr,          nullptr    }
};

menu_t menu_bindings = {
    menu_bindings_items,              // menu items
    nullptr,                          // prev page
    nullptr,                          // next page
    &menu_bindings,                   // rootpage
    160,                              // x offset
    15,                               // y offset
    3,                                // starting item
    mf_background | mf_centeraligned, // fullscreen and centred
    nullptr,                          // no drawer
    mn_binding_contentnames,          // TOC names
    mn_binding_contentpages,          // TOC pages
};

CONSOLE_COMMAND(mn_bindings, 0)
{
    MN_StartMenu(&menu_bindings);
}

//------------------------------------------------------------------------
//
// Key Bindings: basic movement keys
//

static menuitem_t mn_movekeys_items[] = {
    { it_title,   "Basic Movement",  nullptr     },
    { it_gap,     nullptr,           nullptr     },
    { it_binding, "Move forward",    "forward"   },
    { it_binding, "Move backward",   "backward"  },
    { it_binding, "Run",             "speed"     },
    { it_binding, "Turn left",       "left"      },
    { it_binding, "Turn right",      "right"     },
    { it_binding, "Strafe on",       "strafe"    },
    { it_binding, "Strafe left",     "moveleft"  },
    { it_binding, "Strafe right",    "moveright" },
    { it_binding, "180 degree turn", "flip"      },
    { it_binding, "Use",             "use"       },
    { it_binding, "Attack/fire",     "attack"    },
    { it_binding, "Alt-attack/fire", "altattack" },
    { it_binding, "Toggle autorun",  "autorun"   },
    { it_end,     nullptr,           nullptr     }
};

static menuitem_t mn_advkeys_items[] = {
    { it_title,   "Advanced Movement", nullptr     },
    { it_gap,     nullptr,             nullptr     },
    { it_binding, "Weapon Reload",     "reload"    },
    { it_binding, "Weapon Zoom",       "zoom"      },
    { it_binding, "Weapon State 1",    "user1"     },
    { it_binding, "Weapon State 2",    "user2"     },
    { it_binding, "Weapon State 3",    "user3"     },
    { it_binding, "Weapon State 4",    "user4"     },
    { it_binding, "Jump",              "jump"      },
    { it_binding, "MLook on",          "mlook"     },
    { it_binding, "Look up",           "lookup"    },
    { it_binding, "Look down",         "lookdown"  },
    { it_binding, "Center view",       "center"    },
    { it_binding, "Fly up",            "flyup"     },
    { it_binding, "Fly down",          "flydown"   },
    { it_binding, "Land",              "flycenter" },
    { it_end,     nullptr,             nullptr     }
};

menu_t menu_movekeys = {
    mn_movekeys_items,       // menu items
    nullptr,                 // prev page
    &menu_advkeys,           // next page
    nullptr,                 // rootpage
    150,                     // x offset
    15,                      // y offset
    2,                       // starting item
    mf_background,           // draw background: not a skull menu
    nullptr,                 // no drawer
    mn_binding_contentnames, // TOC names
    mn_binding_contentpages, // TOC pages
};

menu_t menu_advkeys = {
    mn_advkeys_items,        // menu items
    &menu_movekeys,          // prev page
    &menu_weaponbindings,    // next page
    nullptr,                 // rootpage
    150,                     // x offset
    15,                      // y offset
    2,                       // starting item
    mf_background,           // draw background: not a skull menu
    nullptr,                 // no drawer
    mn_binding_contentnames, // TOC names
    mn_binding_contentpages, // TOC pages
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

static menuitem_t mn_weaponbindings_items[] = {
    { it_title,   "Weapon Keys",      nullptr      },
    { it_gap,     nullptr,            nullptr      },
    { it_binding, "Weapon 1",         "weapon1"    },
    { it_binding, "Weapon 2",         "weapon2"    },
    { it_binding, "Weapon 3",         "weapon3"    },
    { it_binding, "Weapon 4",         "weapon4"    },
    { it_binding, "Weapon 5",         "weapon5"    },
    { it_binding, "Weapon 6",         "weapon6"    },
    { it_binding, "Weapon 7",         "weapon7"    },
    { it_binding, "Weapon 8",         "weapon8"    },
    { it_binding, "Weapon 9",         "weapon9"    },
    { it_binding, "Next best weapon", "nextweapon" },
    { it_binding, "Next weapon",      "weaponup"   },
    { it_binding, "Previous weapon",  "weapondown" },
    { it_end,     nullptr,            nullptr      }
};

menu_t menu_weaponbindings = {
    mn_weaponbindings_items, // menu items
    &menu_advkeys,           // prev page
    &menu_envinvbindings,    // next page
    nullptr,                 // rootpage
    150,                     // x offset
    15,                      // y offset
    2,                       // starting item
    mf_background,           // draw background: not a skull menu
    nullptr,                 // no drawer
    mn_binding_contentnames, // TOC names
    mn_binding_contentpages, // TOC pages
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

static menuitem_t mn_envinvbindings_items[] = {
    { it_title,   "Inventory/Environment", nullptr,           nullptr, 0                 },
    { it_gap,     nullptr,                 nullptr,           nullptr, 0                 },
    { it_info,    "Inventory Bindings",    nullptr,           nullptr, MENUITEM_CENTERED },
    { it_gap,     nullptr,                 nullptr,           nullptr, 0                 },
    { it_binding, "Inventory left",        "inventory_left",  nullptr, 0                 },
    { it_binding, "Inventory right",       "inventory_right", nullptr, 0                 },
    { it_binding, "Use inventory",         "inventory_use",   nullptr, 0                 },
    { it_binding, "Drop inventory",        "inventory_drop",  nullptr, 0                 },
    { it_gap,     nullptr,                 nullptr,           nullptr, 0                 },
    { it_info,    "Environment",           nullptr,           nullptr, MENUITEM_CENTERED },
    { it_gap,     nullptr,                 nullptr,           nullptr, 0                 },
    { it_binding, "Screen size up",        "screensize +",    nullptr, 0                 },
    { it_binding, "Screen size down",      "screensize -",    nullptr, 0                 },
    { it_binding, "Take screenshot",       "screenshot",      nullptr, 0                 },
    { it_binding, "Spectate prev",         "spectate_prev",   nullptr, 0                 },
    { it_binding, "Spectate next",         "spectate_next",   nullptr, 0                 },
    { it_binding, "Spectate self",         "spectate_self",   nullptr, 0                 },
    { it_end,     nullptr,                 nullptr,           nullptr, 0                 }
};

menu_t menu_envinvbindings = {
    mn_envinvbindings_items, // menu items
    &menu_weaponbindings,    // prev page
    &menu_funcbindings,      // next page
    nullptr,                 // rootpage
    150,                     // x offset
    15,                      // y offset
    4,                       // starting item
    mf_background,           // draw background: not a skull menu
    nullptr,                 // no drawer
    mn_binding_contentnames, // TOC names
    mn_binding_contentpages, // TOC pages
};

CONSOLE_COMMAND(mn_envkeys, 0)
{
    MN_StartMenu(&menu_envinvbindings);
}

//------------------------------------------------------------------------
//
// Key Bindings: Game Function Keys
//

static menuitem_t mn_function_items[] = {
    { it_title,   "Game Functions",   nullptr             },
    { it_gap,     nullptr,            nullptr             },
    { it_binding, "Save game",        "mn_savegame"       },
    { it_binding, "Load game",        "mn_loadgame"       },
    { it_binding, "Volume",           "mn_sound"          },
    { it_binding, "Toggle hud",       "hu_overlaystyle /" },
    { it_binding, "Quick save",       "quicksave"         },
    { it_binding, "End game",         "mn_endgame"        },
    { it_binding, "Toggle messages",  "hu_messages /"     },
    { it_binding, "Quick load",       "quickload"         },
    { it_binding, "Quit",             "mn_quit"           },
    { it_binding, "Gamma correction", "gamma /"           },
    { it_gap,     nullptr,            nullptr             },
    { it_binding, "Join demo",        "joindemo"          },
    { it_end,     nullptr,            nullptr             }
};

menu_t menu_funcbindings = {
    mn_function_items,       // menu items
    &menu_envinvbindings,    // prev page
    &menu_menukeys,          // next page
    nullptr,                 // rootpage
    150,                     // x offset
    15,                      // y offset
    2,                       // starting item
    mf_background,           // draw background: not a skull menu
    nullptr,                 // no drawer
    mn_binding_contentnames, // TOC names
    mn_binding_contentpages, // TOC pages
};

CONSOLE_COMMAND(mn_gamefuncs, 0)
{
    MN_StartMenu(&menu_funcbindings);
}

//------------------------------------------------------------------------
//
// Key Bindings: Menu Keys
//

static menuitem_t mn_menukeys_items[] = {
    { it_title,   "Menu Keys",      nullptr         },
    { it_gap,     nullptr,          nullptr         },
    { it_binding, "Toggle menus",   "menu_toggle"   },
    { it_binding, "Previous menu",  "menu_previous" },
    { it_binding, "Next item",      "menu_down"     },
    { it_binding, "Previous item",  "menu_up"       },
    { it_binding, "Next value",     "menu_right"    },
    { it_binding, "Previous value", "menu_left"     },
    { it_binding, "Confirm",        "menu_confirm"  },
    { it_binding, "Display help",   "menu_help"     },
    { it_binding, "Display setup",  "menu_setup"    },
    { it_binding, "Previous page",  "menu_pageup"   },
    { it_binding, "Next page",      "menu_pagedown" },
    { it_binding, "Show contents",  "menu_contents" },
    { it_end,     nullptr,          nullptr         }
};

menu_t menu_menukeys = {
    mn_menukeys_items,       // menu items
    &menu_funcbindings,      // prev page
    &menu_automapkeys,       // next page
    nullptr,                 // rootpage
    150,                     // x offset
    15,                      // y offset
    2,                       // starting item
    mf_background,           // draw background: not a skull menu
    nullptr,                 // no drawer
    mn_binding_contentnames, // TOC names
    mn_binding_contentpages, // TOC pages
};

CONSOLE_COMMAND(mn_menukeys, 0)
{
    MN_StartMenu(&menu_menukeys);
}

//------------------------------------------------------------------------
//
// Key Bindings: Automap Keys
//

static menuitem_t mn_automapkeys_items[] = {
    { it_title,   "Automap",       nullptr       },
    { it_gap,     nullptr,         nullptr       },
    { it_binding, "Toggle map",    "map_toggle"  },
    { it_binding, "Move up",       "map_up"      },
    { it_binding, "Move down",     "map_down"    },
    { it_binding, "Move left",     "map_left"    },
    { it_binding, "Move right",    "map_right"   },
    { it_binding, "Zoom in",       "map_zoomin"  },
    { it_binding, "Zoom out",      "map_zoomout" },
    { it_binding, "Show full map", "map_gobig"   },
    { it_binding, "Follow mode",   "map_follow"  },
    { it_binding, "Mark spot",     "map_mark"    },
    { it_binding, "Clear spots",   "map_clear"   },
    { it_binding, "Show grid",     "map_grid"    },
    { it_binding, "Overlay mode",  "map_overlay" },
    { it_end,     nullptr,         nullptr       }
};

menu_t menu_automapkeys = {
    mn_automapkeys_items,    // menu items
    &menu_menukeys,          // prev page
    &menu_consolekeys,       // next page
    nullptr,                 // rootpage
    150,                     // x offset
    15,                      // y offset
    2,                       // starting item
    mf_background,           // draw background: not a skull menu
    nullptr,                 // no drawer
    mn_binding_contentnames, // TOC names
    mn_binding_contentpages, // TOC pages
};

CONSOLE_COMMAND(mn_automapkeys, 0)
{
    MN_StartMenu(&menu_automapkeys);
}

//------------------------------------------------------------------------
//
// Key Bindings: Console Keys
//

static menuitem_t mn_consolekeys_items[] = {
    { it_title,   "Console",          nullptr             },
    { it_gap,     nullptr,            nullptr             },
    { it_binding, "Toggle",           "console_toggle"    },
    { it_binding, "Enter command",    "console_enter"     },
    { it_binding, "Complete command", "console_tab"       },
    { it_binding, "Previous command", "console_up"        },
    { it_binding, "Next command",     "console_down"      },
    { it_binding, "Backspace",        "console_backspace" },
    { it_binding, "Page up",          "console_pageup"    },
    { it_binding, "Page down",        "console_pagedown"  },
    { it_end,     nullptr,            nullptr             }
};

menu_t menu_consolekeys = {
    mn_consolekeys_items,    // menu items
    &menu_automapkeys,       // prev page
    nullptr,                 // next page
    nullptr,                 // rootpage
    150,                     // x offset
    15,                      // y offset
    2,                       // starting item
    mf_background,           // draw background: not a skull menu
    nullptr,                 // no drawer
    mn_binding_contentnames, // TOC names
    mn_binding_contentpages, // TOC pages
};

CONSOLE_COMMAND(mn_consolekeys, 0)
{
    MN_StartMenu(&menu_consolekeys);
}

//----------------------------------------------------------------------------
//
// "Menus" menu -- for menu options related to the menus themselves :)
//
// haleyjd 03/14/06
//

static char       *mn_searchstr;
static menuitem_t *lastMatch;

VARIABLE_STRING(mn_searchstr, nullptr, 32);
CONSOLE_VARIABLE(mn_searchstr, mn_searchstr, 0)
{
    // any change to mn_searchstr resets the search index
    lastMatch = nullptr;
}

static void MN_InitSearchStr()
{
    mn_searchstr = estrdup("");
}

// haleyjd: searchable menus
extern menu_t menu_movekeys;
extern menu_t menu_mouse;
extern menu_t menu_gamepad;
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

static menu_t *mn_search_menus[] = {
    &menu_movekeys,     //
    &menu_mouse,        //
    &menu_gamepad,      //
    &menu_video,        //
    &menu_sound,        //
    &menu_compat1,      //
    &menu_enemies,      //
    &menu_weapons,      //
    &menu_hud,          //
    &menu_statusbar,    //
    &menu_automapcol1,  //
    &menu_player,       //
    &menu_gamesettings, //
    &menu_loadwad,      //
    &menu_demos,        //
    nullptr,            //
};

CONSOLE_COMMAND(mn_search, 0)
{
    int     i = 0;
    menu_t *curMenu;
    bool    pastLast;

    // if lastMatch is set, set pastLast to false so that we'll seek
    // forward until we pass the last item that was matched
    pastLast = !lastMatch;

    if(!mn_searchstr || !mn_searchstr[0])
    {
        MN_ErrorMsg("Invalid search string");
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
            int         j = 0;
            menuitem_t *item;

            // run through items
            while((item = &(curPage->menuitems[j++])))
            {
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

                qstring desc(item->description);
                desc.toLower();

                // found a match
                if(desc.findSubStr(mn_searchstr))
                {
                    // go to it
                    lastMatch = item;
                    MN_StartMenu(curPage);
                    MN_ErrorMsg("Found: %s", desc.constPtr());
                    if(!is_a_gap(item))
                        curPage->selected = j - 1;
                    return;
                }
            }

            curPage = curPage->nextpage;
        }
    }

    if(lastMatch) // if doing a valid search, reset it now
    {
        lastMatch = nullptr;
        MN_ErrorMsg("Reached end of search");
    }
    else
        MN_ErrorMsg("No match found for '%s'", mn_searchstr);
}

static menuitem_t mn_menus_items[] = {
    { it_title,    "Menu Options",          nullptr,            "M_MENUS" },
    { it_gap,      nullptr,                 nullptr,            nullptr   },
    { it_info,     "General",               nullptr,            nullptr   },
    { it_toggle,   "Toggle key backs up",   "mn_toggleisback",  nullptr   },
    { it_runcmd,   "Set background...",     "mn_selectflat",    nullptr   },
    { it_gap,      nullptr,                 nullptr,            nullptr   },
    { it_info,     "Compatibility",         nullptr,            nullptr   },
    { it_toggle,   "Emulate all old menus", "mn_classic_menus", nullptr   },
    { it_gap,      nullptr,                 nullptr,            nullptr   },
    { it_info,     "Utilities",             nullptr,            nullptr   },
    { it_variable, "Search string:",        "mn_searchstr",     nullptr   },
    { it_runcmd,   "Search in menus...",    "mn_search",        nullptr   },
    { it_end,      nullptr,                 nullptr,            nullptr   },
};

menu_t menu_menuopts = {
    mn_menus_items, // menu items
    nullptr,        // prev page
    nullptr,        // next page
    nullptr,        // rootpage
    200,            // x offset
    15,             // y offset
    3,              // starting item
    mf_background,  // draw background: not a skull menu
    nullptr,        // no drawer
};

CONSOLE_COMMAND(mn_menus, 0)
{
    MN_StartMenu(&menu_menuopts);
}

//
// Config menu - configuring the configuration files O_o
//

static menuitem_t mn_config_items[] = {
    { it_title,  "Configuration",        nullptr           },
    { it_gap,    nullptr,                nullptr           },
    { it_info,   "Doom Game Modes",      nullptr           },
    { it_toggle, "Use user/doom config", "use_doom_config" },
    { it_end,    nullptr,                nullptr           }
};

menu_t menu_config = {
    mn_config_items, // menu items
    nullptr,         // prev page
    nullptr,         // next page
    nullptr,         // rootpage
    200,             // x offset
    15,              // y offset
    3,               // starting item
    mf_background,   // draw background: not a skull menu
    nullptr,         // no drawer
};

CONSOLE_COMMAND(mn_config, 0)
{
    MN_StartMenu(&menu_config);
}

//
// Skin Viewer Command
//

extern void MN_InitSkinViewer();

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

// haleyjd 05/16/04: traditional DOOM main menu support

int mn_classic_menus;

//
// MN_LinkClassicMenus
//
// haleyjd 08/31/06: This function is called to turn classic menu system support
// on or off. When it's on, the main menu is patched to point to the old classic
// menus.
//
void MN_LinkClassicMenus(int link)
{
    if(!(GameModeInfo->flags & GIF_CLASSICMENUS))
        return;

    if(link) // turn on classic menus
    {
        GameModeInfo->mainMenu->menuitems[1].data = "mn_old_options";
    }
    else // turn off classic menus
    {
        GameModeInfo->mainMenu->menuitems[1].data = "mn_options";
    }
}

VARIABLE_BOOLEAN(mn_classic_menus, nullptr, yesno);
CONSOLE_VARIABLE(mn_classic_menus, mn_classic_menus, 0)
{
    MN_LinkClassicMenus(mn_classic_menus);
}

// Original Options Menu

static menuitem_t mn_old_option_items[] = {
    { it_runcmd,    "end game",       "mn_endgame",    "M_ENDGAM" },
    { it_runcmd,    "messages",       "hu_messages /", "M_MESSG"  },
    { it_runcmd,    "graphic detail", "echo No.",      "M_DETAIL" },
    { it_bigslider, "screen size",    "screensize",    "M_SCRNSZ" },
    { it_gap,       nullptr,          nullptr,         nullptr    },
    { it_bigslider, "mouse sens.",    "sens_combined", "M_MSENS"  },
    { it_gap,       nullptr,          nullptr,         nullptr    },
    { it_runcmd,    "sound volume",   "mn_old_sound",  "M_SVOL"   },
    { it_end,       nullptr,          nullptr,         nullptr    }
};

static char detailNames[2][9] = { "M_GDHIGH", "M_GDLOW" };
static char msgNames[2][9]    = { "M_MSGOFF", "M_MSGON" };

static void MN_OldOptionsDrawer()
{
    V_DrawPatch(108, 15, &subscreen43, PatchLoader::CacheName(wGlobalDir, "M_OPTTTL", PU_CACHE));

    V_DrawPatch(60 + 120, 37 + EMULATED_ITEM_SIZE, &subscreen43,
                PatchLoader::CacheName(wGlobalDir, msgNames[showMessages], PU_CACHE));

    const char *detailstr;
    if(GameModeInfo->missionInfo->flags & MI_NOGDHIGH) // stupid BFG Edition.
        detailstr = msgNames[1];
    else
        detailstr = detailNames[0];

    V_DrawPatch(60 + 175, 37 + EMULATED_ITEM_SIZE * 2, &subscreen43,
                PatchLoader::CacheName(wGlobalDir, detailstr, PU_CACHE));
}

menu_t menu_old_options = {
    mn_old_option_items,        // menu items
    nullptr,                    // previous page
    nullptr,                    // next page
    nullptr,                    // rootpage
    60,                         // x offset
    37,                         // y offset
    0,                          // starting item
    mf_skullmenu | mf_emulated, // is a skull menu
    MN_OldOptionsDrawer         // drawer
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

static menuitem_t mn_old_sound_items[] = {
    { it_bigslider, "sfx volume",   "sfx_volume",   "M_SFXVOL" },
    { it_gap,       nullptr,        nullptr,        nullptr    },
    { it_bigslider, "music volume", "music_volume", "M_MUSVOL" },
    { it_end,       nullptr,        nullptr,        nullptr    }
};

static void MN_OldSoundDrawer()
{
    V_DrawPatch(60, 38, &subscreen43, PatchLoader::CacheName(wGlobalDir, "M_SVOL", PU_CACHE));
}

menu_t menu_old_sound = {
    mn_old_sound_items,         // menu items
    nullptr,                    // previous page
    nullptr,                    // next page
    nullptr,                    // rootpage
    80,                         // x offset
    64,                         // y offset
    0,                          // starting item
    mf_skullmenu | mf_emulated, // is a skull menu
    MN_OldSoundDrawer           // drawer
};

CONSOLE_COMMAND(mn_old_sound, 0)
{
    MN_StartMenu(&menu_old_sound);
}

// TODO: Draw skull cursor on Credits pages?

// EOF
