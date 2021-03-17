//
// The Eternity Engine
// Copyright(C) 2021 James Haley, et al.
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
// Purpose: Save game-related menus
// Authors: Max Waine, Devin Acker
//

#if __cplusplus >= 201703L || _MSC_VER >= 1914
#include "hal/i_platform.h"
#if EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX
#include "hal/i_directory.h"
namespace fs = fsStopgap;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

#include "z_zone.h"

#include "c_runcmd.h"
#include "d_deh.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_main.h"
#include "d_net.h"
#include "doomstat.h"
#include "dstrings.h"
#include "g_bind.h"
#include "g_game.h"
#include "m_buffer.h"
#include "m_collection.h"
#include "m_qstr.h"
#include "mn_engin.h"
#include "mn_menus.h"
#include "mn_misc.h"
#include "m_utils.h"
#include "p_saveg.h"
#include "p_skin.h"
#include "r_patch.h"
#include "s_sound.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"

struct saveslot_t
{
   int     saveNum;
   qstring description;
   int     saveVersion;
   int     skill;
   qstring mapName;
   int     levelTime;
   time_t  fileTime;
   qstring fileTimeStr;
};

static Collection<saveslot_t> e_saveSlots;

#define SAVESTRINGSIZE 24

// DELETE START
// haleyjd: was 7
#define SAVESLOTS 8
char *savegamenames[SAVESLOTS];
// haleyjd: keep track of valid save slots
bool savegamepresent[SAVESLOTS];
// DELETE END

void MN_InitSaveGameMenus()
{
   // haleyjd: initialize via zone memory
   for(int i = 0; i < SAVESLOTS; i++)
   {
      savegamenames[i] = estrdup("");
      savegamepresent[i] = false;
   }
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

static void MN_SaveGame()
{
   int save_slot = static_cast<int>((char **)(Console.command->variable->variable) - savegamenames);

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
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_DEACTIVATE]);
}

//
// Read the strings from the savegame files
// Partially based on the "savemenu" branch from Devin Acker's EE fork
//
static void MN_readSaveStrings()
{
   int      dummy;
   uint64_t dummy64;
   bool     bdummy;

   e_saveSlots.clear();

   // test for failure
   if(std::error_code ec; !fs::is_directory(basesavegame, ec))
      return;

   const fs::directory_iterator itr(basesavegame);
   for(const fs::directory_entry &ent : itr)
   {
      size_t      len;
      char        description[SAVESTRINGSIZE + 1]; // sf
      char        vread[VERSIONSIZE];
      InBuffer    loadFile;
      SaveArchive arc(&loadFile);
      fs::path    savePath(ent.path());
      qstring     pathStr(savePath.generic_u8string().c_str());

      if(ent.is_directory() || !savePath.has_stem() || !savePath.has_extension() || savePath.extension() != ".dsg")
         continue;

      if(!loadFile.openFile(pathStr.constPtr(), InBuffer::NENDIAN))
         continue;

      qstring savename(savePath.stem().generic_u8string().c_str());
      if(savename.strNCaseCmp(savegamename, strlen(savegamename)))
         continue;

      savename.erase(0, strlen(savegamename));

      saveslot_t newSlot;
      newSlot.saveNum = savename.toInt();

      // file time
      struct stat statbuf;
      if(!stat(pathStr.constPtr(), &statbuf))
      {
         char timeStr[64 + 1];
         strftime(timeStr, sizeof(timeStr), "%a. %b %d %Y\n%r", localtime(&statbuf.st_mtime));
         newSlot.fileTime    = statbuf.st_mtime;
         newSlot.fileTimeStr = timeStr;
      }

      // description
      memset(description, 0, sizeof(description));
      arc.archiveCString(description, SAVESTRINGSIZE);
      newSlot.description = description;

      // version
      arc.archiveCString(vread, VERSIONSIZE);
      if(strncmp(vread, "EE", 2))
      {
         newSlot.saveVersion = 0;
         // don't try to load anything else...
         loadFile.close();
         e_saveSlots.add(newSlot);
         continue;
      }
      else
         arc << newSlot.saveVersion;

      // compatibility, skill level, manager dir, vanilla mode
      arc << dummy << newSlot.skill << dummy << bdummy;

      // map
      newSlot.mapName = qstring(9);
      for(int j = 0; j < 8; j++)
      {
         int8_t lvc;
         arc << lvc;
         newSlot.mapName[j] = static_cast<char>(lvc);
      }
      newSlot.mapName[8] = '\0';

      // skip managed directory stuff, checksum (if present), and players
      arc.archiveSize(len);
      loadFile.skip(len);
      if(arc.saveVersion() >= 4)
         arc << dummy64;
      for(int j = 0; j < MIN_MAXPLAYERS; j++)
         arc << bdummy;

      // music num, gametype
      arc << dummy << dummy;

      // skip options
      byte options[GAME_OPTION_SIZE];
      loadFile.read(options, sizeof(options));

      // level time
      arc << newSlot.levelTime;

      loadFile.close();
      e_saveSlots.add(newSlot);
   }
}

static void MN_drawSaveInfo(int savenum)
{
   const int x = 23 * 8;
   const int y = 40;
   const int h = 7 * 8;
   const int lineh = menu_font->cy;
   const int namew = MN_StringWidth("xxxxxx");

   V_DrawBox(x, y, SCREENWIDTH - x, h);

   if(savenum > e_saveSlots.getLength())
      return;

   const saveslot_t &saveSlot = e_saveSlots[savenum];

   MN_WriteTextColored(saveSlot.fileTimeStr.constPtr(), GameModeInfo->infoColor, x + 4, y + 4);

   if(saveSlot.saveVersion > 0)
   {
      char temp[32 + 1];
      const char *const mapName = saveSlot.mapName.constPtr();

      snprintf(temp, sizeof(temp), "%d", saveSlot.skill);
      MN_WriteTextColored("map:",   GameModeInfo->infoColor, x + 4,         y + (lineh * 3) + 4);
      MN_WriteTextColored(mapName,  GameModeInfo->infoColor, x + namew + 4, y + (lineh * 3) + 4);
      MN_WriteTextColored("skill:", GameModeInfo->infoColor, x + 4,         y + (lineh * 4) + 4);
      MN_WriteTextColored(temp,     GameModeInfo->infoColor, x + namew + 4, y + (lineh * 4) + 4);

      const int time     = saveSlot.levelTime / TICRATE;
      const int hours    = (time / 3600);
      const int minutes  = (time % 3600) / 60;
      const int seconds  = (time % 60);

      snprintf(temp, sizeof(temp), "%02d:%02d:%02d", hours, minutes, seconds);
      MN_WriteTextColored("time:", GameModeInfo->infoColor, x + 4,         y + (lineh * 5) + 4);
      MN_WriteTextColored(temp,    GameModeInfo->infoColor, x + namew + 4, y + (lineh * 5) + 4);
   }
   else
   {
      MN_WriteTextColored("warning:",         GameModeInfo->unselectColor, x + 4, y + (lineh * 3) + 4);
      MN_WriteTextColored("save is too old!", GameModeInfo->infoColor,     x + 4, y + (lineh * 4) + 4);
   }
}

// create the savegame console commands
void MN_CreateSaveCmds()
{
   for(int i = 0; i < SAVESLOTS; i++)  // haleyjd
   {
      command_t  *save_command;
      variable_t *save_variable;
      char tempstr[16];

      // create the variable first
      save_variable = estructalloc(variable_t, 1);
      save_variable->variable  = &savegamenames[i];
      save_variable->v_default = nullptr;
      save_variable->type      = vt_string;      // string value
      save_variable->min       = 0;
      save_variable->max       = SAVESTRINGSIZE;
      save_variable->defines   = nullptr;

      // now the command
      save_command = estructalloc(command_t, 1);

      sprintf(tempstr, "savegame_%i", i);
      save_command->name     = estrdup(tempstr);
      save_command->type     = ct_variable;
      save_command->flags    = 0;
      save_command->variable = save_variable;
      save_command->handler  = MN_SaveGame;
      save_command->netcmd   = 0;

      C_AddCommand(save_command); // hook into cmdlist
   }
}

static void MN_DrawSaveLoadBorder(int x, int y)
{
   patch_left  = PatchLoader::CacheName(wGlobalDir, "M_LSLEFT", PU_STATIC);
   patch_mid   = PatchLoader::CacheName(wGlobalDir, "M_LSCNTR", PU_STATIC);
   patch_right = PatchLoader::CacheName(wGlobalDir, "M_LSRGHT", PU_STATIC);

   V_DrawPatch(x - 8, y + 7, &subscreen43, patch_left);

   for(int i = 0; i < 24; i++)
   {
      V_DrawPatch(x, y + 7, &subscreen43, patch_mid);
      x += 8;
   }

   V_DrawPatch(x, y + 7, &subscreen43, patch_right);

   // haleyjd: make purgable
   Z_ChangeTag(patch_left,  PU_CACHE);
   Z_ChangeTag(patch_mid,   PU_CACHE);
   Z_ChangeTag(patch_right, PU_CACHE);
}

static void MN_loadGameOpen(menu_t *menu);
static void MN_loadGameDrawer();
static bool MN_loadGameResponder(event_t *ev, int action);

static int load_slot    = -1;
static int load_saveNum = -1;
static menuwidget_t load_selector = { MN_loadGameDrawer, MN_loadGameResponder, nullptr, true };

menu_t menu_loadgame =
{
   nullptr,
   nullptr, nullptr, nullptr,        // pages
   10, 44,                           // x, y
   0,                                // starting slot
   0,                                // no flags
   nullptr, nullptr, nullptr,        // drawer and content
   0,                                // gap override
   MN_loadGameOpen
};

static void MN_loadGameOpen(menu_t *menu)
{
   if(const size_t numSaveSlots = e_saveSlots.getLength(); numSaveSlots == 0)
   {
      load_slot    = -1;
      load_saveNum = -1;
   }
   else
   {
      if(load_slot == -1)
      {
         load_slot    = 0;
         load_saveNum = e_saveSlots[0].saveNum;
      }
      else
      {
         bool foundSave = false;
         for(size_t i = 0; i < numSaveSlots; i++)
         {
            if(e_saveSlots[i].saveNum == load_saveNum)
            {
               load_slot = int(i);
               foundSave = true;
            }
         }
         if(!foundSave)
         {
            load_slot    = 0;
            load_saveNum = e_saveSlots[0].saveNum;
         }
      }
   }
   MN_PushWidget(&load_selector);
}

static void MN_loadGameDrawer()
{
   int y = menu_loadgame.y;

   patch_t *patch = PatchLoader::CacheName(wGlobalDir, "M_LOADG", PU_CACHE);
   V_DrawPatch((SCREENWIDTH - patch->width) >> 1, 18, &subscreen43, patch);

   V_DrawBox(0, menu_loadgame.y - 4, 23 * 8, 8 * 16);
   for(int i = 0; i < e_saveSlots.getLength(); i++)
   {
      const int color = i == load_slot ? GameModeInfo->selectColor : GameModeInfo->unselectColor;
      MN_WriteTextColored(e_saveSlots[i].description.constPtr(), color, menu_loadgame.x, y);
      y += menu_font->cy;
   }
   MN_drawSaveInfo(load_slot);
}

static bool MN_loadGameResponder(event_t *ev, int action)
{
   int *menuSounds = GameModeInfo->menuSounds;

   if(action == ka_menu_toggle || action == ka_menu_previous)
   {
      if(menu_toggleisback || action == ka_menu_previous)
      {
         MN_PopWidget();
         MN_PrevMenu();
      }
      else
      {
         MN_ClearMenus();
         S_StartInterfaceSound(menuSounds[MN_SND_DEACTIVATE]);
      }
      return true;
   }

   // Nothing to interact with
   if(e_saveSlots.getLength() == 0)
      return false;

   if(action == ka_menu_down || action == ka_menu_up)
   {
      const int numSlots   = int(e_saveSlots.getLength());
      const int slotChange = action == ka_menu_down ? 1 : -1;
      load_slot    = (load_slot + slotChange + numSlots) % numSlots;
      load_saveNum = e_saveSlots[load_slot].saveNum;

      S_StartInterfaceSound(menuSounds[MN_SND_KEYUPDOWN]); // make sound

      return true;
   }

   if(action == ka_menu_confirm)
   {
      qstring loadCmd = qstring("mn_load ") << load_slot;
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_COMMAND]); // make sound
      Console.cmdtype = c_menu;
      C_RunTextCmd(loadCmd.constPtr());
      return true;
   }

   return false;
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
               "while recording a demo!\n\n" PRESSKEY);
      return;
   }

   MN_readSaveStrings();  // get savegame descriptions
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
   if(slot >= e_saveSlots.getLength())
   {
      MN_Alert("Attempted to load save that doesn't exist\n%s", DEH_String("PRESSKEY"));
      return;     // empty slot
   }

   len = M_StringAlloca(&name, 2, 26, basesavegame, savegamename);

   G_SaveGameName(name, len, slot);
   G_LoadGame(name, slot, false);

   MN_ClearMenus();

   // haleyjd 10/08/08: GIF_SAVESOUND flag
   if(GameModeInfo->flags & GIF_SAVESOUND)
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_DEACTIVATE]);
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
               "while recording a demo!\n\n" PRESSKEY);
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
   char *name = nullptr;     // killough 3/22/98
   size_t len;

   len = M_StringAlloca(&name, 2, 26, basesavegame, savegamename);

   G_SaveGameName(name, len, quickSaveSlot);
   G_LoadGame(name, quickSaveSlot, false);
}

/////////////////////////////////////////////////////////////////
//
// Save Game
//

static void MN_SaveGameDrawer();

// haleyjd: fixes continue here from 8-17 build

static menuitem_t mn_savegame_items[] =
{
   {it_variable, "",                          "savegame_0"},
   {it_variable, "",                          "savegame_1"},
   {it_variable, "",                          "savegame_2"},
   {it_variable, "",                          "savegame_3"},
   {it_variable, "",                          "savegame_4"},
   {it_variable, "",                          "savegame_5"},
   {it_variable, "",                          "savegame_6"},
   {it_variable, "",                          "savegame_7"},
   {it_end}
};

static void MN_saveGameOpen(menu_t *menu);
static void MN_saveGameDrawer();
static bool MN_saveGameResponder(event_t *ev, int action);

static int save_slot    = -1;
static int save_saveNum = -1;
static menuwidget_t save_selector = { MN_saveGameDrawer, MN_saveGameResponder, nullptr, true };


menu_t menu_savegame =
{
   nullptr,
   nullptr, nullptr, nullptr,        // pages
   10, 44,                           // x, y
   0,                                // starting slot
   0,                                // no flags
   nullptr, nullptr, nullptr,        // drawer and content
   0,                                // gap override
   MN_saveGameOpen
};

static void MN_saveGameOpen(menu_t *menu)
{
   if(const size_t numSaveSlots = e_saveSlots.getLength(); numSaveSlots == 0)
   {
      save_slot    = -1;
      save_saveNum = -1;
   }
   else
   {
      if(save_slot == -1)
         save_saveNum = -1;
      else
      {
         bool foundSave = false;
         for(size_t i = 0; i < numSaveSlots; i++)
         {
            if(e_saveSlots[i].saveNum == save_saveNum)
            {
               save_slot = int(i);
               foundSave = true;
            }
         }
         if(!foundSave)
         {
            save_slot    = -1;
            save_saveNum = -1;
         }
      }
   }
   MN_PushWidget(&save_selector);
}

static void MN_saveGameDrawer()
{
   int y = menu_savegame.y;

   patch_t *patch = PatchLoader::CacheName(wGlobalDir, "M_SAVEG", PU_CACHE);
   V_DrawPatch((SCREENWIDTH - patch->width) >> 1, 18, &subscreen43, patch);

   V_DrawBox(0, menu_savegame.y - 4, 23 * 8, 8 * 16);
   {
      const int color = save_slot == -1 ? GameModeInfo->selectColor : GameModeInfo->unselectColor;
      MN_WriteTextColored("<New Save Game>", color, menu_savegame.x, y);
      y += menu_font->cy;
   }
   for(int i = 0; i < e_saveSlots.getLength(); i++)
   {
      const int color = i == save_slot ? GameModeInfo->selectColor : GameModeInfo->unselectColor;
      MN_WriteTextColored(e_saveSlots[i].description.constPtr(), color, menu_savegame.x, y);
      y += menu_font->cy;
   }
   MN_drawSaveInfo(save_slot);
}

static bool MN_saveGameResponder(event_t *ev, int action)
{
   int *menuSounds = GameModeInfo->menuSounds;

   if(action == ka_menu_toggle || action == ka_menu_previous)
   {
      if(menu_toggleisback || action == ka_menu_previous)
      {
         MN_PopWidget();
         MN_PrevMenu();
      }
      else
      {
         MN_ClearMenus();
         S_StartInterfaceSound(menuSounds[MN_SND_DEACTIVATE]);
      }
      return true;
   }

   if(e_saveSlots.getLength() != 0 && (action == ka_menu_down || action == ka_menu_up))
   {
      const int numSlots   = int(e_saveSlots.getLength());
      const int slotChange = action == ka_menu_down ? 1 : -1;
      save_slot    = ((save_slot + slotChange + numSlots + 2) % (numSlots + 1)) - 1; // [-1, numSlots)
      save_saveNum = save_slot == -1 ? -1 : e_saveSlots[save_slot].saveNum;

      S_StartInterfaceSound(menuSounds[MN_SND_KEYUPDOWN]); // make sound

      return true;
   }

   if(action == ka_menu_confirm)
   {
      qstring saveCmd = qstring("mn_save ") << save_slot;
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_COMMAND]); // make sound
      Console.cmdtype = c_menu;
      C_RunTextCmd(saveCmd.constPtr());
      return true;
   }

   return false;
}

static void MN_SaveGameDrawer()
{
   int lumpnum = W_CheckNumForName("M_SGTTL");

   if(mn_classic_menus || lumpnum == -1)
      lumpnum = W_CheckNumForName("M_SAVEG");

   V_DrawPatch(72, 18, &subscreen43,
               PatchLoader::CacheNum(wGlobalDir, lumpnum, PU_CACHE));

   for(int i = 0; i < SAVESLOTS; i++)
      MN_DrawSaveLoadBorder(menu_savegame.x, menu_savegame.y + 16*i);
}

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

   MN_readSaveStrings();

   MN_StartMenu(GameModeInfo->saveMenu);
}

// haleyjd 02/23/02: Quick Save -- restored from MBF, converted to
// use console commands
CONSOLE_COMMAND(quicksave, 0)
{
   char tempstring[80];

   if(!usergame && (!demoplayback || netgame))  // killough 10/98
   {
      S_StartInterfaceSound(GameModeInfo->playerSounds[sk_oof]);
      return;
   }

   if(gamestate != GS_LEVEL)
      return;

   if(quickSaveSlot < 0)
   {
      quickSaveSlot = -2; // means to pick a slot now
      MN_readSaveStrings();
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

// EOF

