//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

#if (EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS)
#define START_UTF8() setlocale(LC_ALL, ".65001")
#define END_UTF8()   setlocale(LC_ALL, "C")
#else
#define START_UTF8()
#define END_UTF8()
#endif

#include "z_zone.h"

#include "c_runcmd.h"
#include "d_deh.h"
#include "d_dehtbl.h"
#include "d_event.h"
#include "d_gi.h"
#include "d_main.h"
#include "d_net.h"
#include "doomstat.h"
#include "dstrings.h"
#include "g_bind.h"
#include "g_game.h"
#include "m_buffer.h"
#include "m_collection.h"
#include "m_compare.h"
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

#define DSPROMPT "Delete save named\n\n'%s'?\n\n" PRESSYN

static constexpr int SAVESTRINGSIZE     = 24;
static constexpr int NUMSAVEBOXLINES    = 15;
static constexpr int SAVEBOXUNIT        = 8;
static constexpr int SAVEBOXWIDTH       = (SAVESTRINGSIZE - 1) * SAVEBOXUNIT;
static constexpr int MAXSAVESTRINGWIDTH = SAVEBOXWIDTH - SAVEBOXUNIT;

struct saveID_t
{
   int slot;
   int fileNum;
};

struct saveSlotTimePair_t
{
   int    slot;
   time_t fileTime;
};

struct saveslot_t
{
   int     fileNum;
   qstring description;
   int     saveVersion;
   int     skill;
   qstring mapName;
   int     levelTime;
   time_t  fileTime;
   qstring fileTimeStr;
};

static void MN_deleteSave();

static Collection<saveslot_t> e_saveSlots;

static qstring desc_buffer;
static bool    typing_save_desc = false;

static saveID_t quickID = { -1, -1 };
static saveID_t loadID  = { -1, -1 };
static saveID_t saveID  = { -1, -1 };

// load/save box patches
patch_t *patch_left, *patch_mid, *patch_right;

/////////////////////////////////////////////////////////////////
//
// Save & Load Game
//

static int WrapSlot(int slot, const int delta, const int min, const int max)
{
   const int mod = max - min;
   slot += delta - min;
   slot += (1 - (slot / mod)) * mod;
   slot  = (slot % mod) + min;
   return slot;
}

static void MN_drawHereticHeader(const char *title)
{
   V_FontWriteText(
      menu_font_big, title,
      160 - V_FontStringWidth(menu_font_big, title) / 2, 10,
      &subscreen43
   );
}

bool MN_handleNavigationInput(saveID_t &saveID, const int action, const int min)
{
   int slotChange;
   switch(action)
   {
   case ka_menu_down:     slotChange =  1;               break;
   case ka_menu_up:       slotChange = -1;               break;
   case ka_menu_pagedown: slotChange =  NUMSAVEBOXLINES; break;
   case ka_menu_pageup:   slotChange = -NUMSAVEBOXLINES; break;
   default:               slotChange =  0;               break;
   }

   if(slotChange)
   {
      const int numSlots   = int(e_saveSlots.getLength());
      saveID.slot          = WrapSlot(saveID.slot, slotChange, min, numSlots);
      saveID.fileNum       = saveID.slot == -1 ? -1 : e_saveSlots[saveID.slot].fileNum;;

      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_KEYUPDOWN]); // make sound

      return true; // eat input
   }

   return false;
}

bool MN_handleMenuCloseInput(const int action)
{
   if(action == ka_menu_toggle || action == ka_menu_previous)
   {
      if(menu_toggleisback || action == ka_menu_previous)
      {
         MN_PopWidget(consumeText_e::NO);
         MN_PrevMenu();
      }
      else
      {
         MN_ClearMenus();
         S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_DEACTIVATE]);
      }
      return true; // eat input
   }

   return false;
}

bool MN_handleSaveDeleteInput(saveID_t &id, const event_t *const ev, const int action)
{
   if(ev->type == ev_keydown && ev->data1 == KEYD_DEL && id.fileNum != -1)
   {
      char tempstring[80];
      psnprintf(tempstring, sizeof(tempstring), DSPROMPT, e_saveSlots[id.slot].description.constPtr());

      MN_QuestionFunc(tempstring, MN_deleteSave);
      return true; // eat input
   }

   return false;
}

static void MN_updateSaveID(saveID_t &id, const int minimum)
{
   if(const size_t numSaveSlots = e_saveSlots.getLength(); numSaveSlots == 0)
      id = { -1, -1 };
   else
   {
      if(id.slot == -1)
      {
         id.slot    = minimum;
         id.fileNum = minimum == -1 ? -1 : e_saveSlots[0].fileNum;
      }
      else
      {
         bool foundSave = false;
         for(size_t i = 0; i < numSaveSlots; i++)
         {
            if(e_saveSlots[i].fileNum == id.fileNum)
            {
               id.slot = int(i);
               foundSave   = true;
               break;
            }
         }
         if(!foundSave)
         {
            id.slot    = minimum;
            id.fileNum = minimum == -1 ? -1 : e_saveSlots[0].fileNum;
         }
      }
   }
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
      char        description[64]; // sf
      InBuffer    loadFile;
      SaveArchive arc(&loadFile);
      fs::path    savePath(ent.path());
      qstring     pathStr(reinterpret_cast<const char *>(savePath.generic_u8string().c_str())); // C++20_FIXME: Cast to make C++20 builds compile

      if(ent.is_directory() || !savePath.has_stem() || !savePath.has_extension() || savePath.extension() != ".dsg")
         continue;

      START_UTF8();
      const bool fileLoaded = !loadFile.openFile(pathStr.constPtr(), InBuffer::NENDIAN);
      END_UTF8();
      if(fileLoaded)
         continue;

      qstring savename(reinterpret_cast<const char *>(savePath.stem().generic_u8string().c_str())); // C++20_FIXME: Cast to make C++20 builds compile
      if(savename.strNCaseCmp(savegamename, strlen(savegamename)))
         continue;

      savename.erase(0, strlen(savegamename));

      saveslot_t newSlot;
      newSlot.fileNum = savename.toInt();

      // file time
      START_UTF8();
      struct stat statbuf;
      if(!stat(pathStr.constPtr(), &statbuf))
      {
         char timeStr[64 + 1];
         strftime(timeStr, sizeof(timeStr), "%a. %b %d %Y\n%r", localtime(&statbuf.st_mtime));
         newSlot.fileTime    = statbuf.st_mtime;
         newSlot.fileTimeStr = timeStr;
      }
      END_UTF8();

      // description
      memset(description, 0, sizeof(description));
      arc.archiveCString(description, SAVESTRINGSIZE);
      newSlot.description = description;

      // version
      if(!arc.readSaveVersion())
      {
         newSlot.saveVersion = 0;
         // don't try to load anything else...
         loadFile.close();
         e_saveSlots.add(newSlot);
         continue;
      }
      newSlot.saveVersion = arc.saveVersion();

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
      {
         int numwadfiles;
         arc << dummy64;
         arc << numwadfiles;
         for(int i = 0; i < numwadfiles; i++)
         {
            arc.archiveSize(len);
            loadFile.skip(len);
         }
      }
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

   // Sort the slots from most recent to least recent
   if(const size_t numSlots = e_saveSlots.getLength(); numSlots != 0)
   {
      Collection<saveslot_t> tempSlots;
      saveSlotTimePair_t *slotTimes = estructalloc(saveSlotTimePair_t, numSlots);

      for(int i = 0; i < numSlots; i++)
         slotTimes[i] = { i, e_saveSlots[i].fileTime };

      qsort(
         slotTimes, numSlots, sizeof(saveSlotTimePair_t),
         [](const void *i1, const void *i2) {
            return int(static_cast<const saveSlotTimePair_t *>(i2)->fileTime - static_cast<const saveSlotTimePair_t *>(i1)->fileTime);
         }
      );

      for(int i = 0; i < numSlots; i++)
         tempSlots.add(e_saveSlots[slotTimes[i].slot]);
      e_saveSlots = std::move(tempSlots);

      efree(slotTimes);
   }

   MN_updateSaveID(loadID,  0);
   MN_updateSaveID(saveID, -1);
   if(quickID.slot >= 0)
      MN_updateSaveID(quickID, -1);
}

//
// Draw the currently selected save's info (map name, skill, date saved, and if it's too old to load)
//
static void MN_drawSaveInfo(int slotIndex)
{
   const int x = SAVEBOXWIDTH;
   const int y = 40;
   const int lineh = menu_font->cy;
   const int h     = 7 * lineh;
   const int namew = MN_StringWidth("xxxxxx");

   V_DrawBox(x, y, SCREENWIDTH - x, h);

   if(slotIndex > e_saveSlots.getLength())
      return;

   const saveslot_t &saveSlot = e_saveSlots[slotIndex];

   MN_WriteTextColored(saveSlot.fileTimeStr.constPtr(), GameModeInfo->infoColor, x + 4, y + 4);

   if(saveSlot.saveVersion > 0)
   {
      char temp[32 + 1];
      const char *const mapName = saveSlot.mapName.constPtr();

      snprintf(temp, sizeof(temp), "%d", saveSlot.skill + 1); // Accursed people and their 1-indexing
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

//
// Get the minimum and maximum slot indices to display based on the current select slot,
// minimum total slot, and number of slots
//
static void MN_getMinAndMaxSlot(int &min, int &max, const int slot, const int minSlot, const int numSlots)
{
   min = slot - NUMSAVEBOXLINES / 2;
   if(min < minSlot)
      min = minSlot;
   max = min + NUMSAVEBOXLINES - 1;
   if(max >= numSlots)
   {
      max = numSlots - 1;
      min = max - NUMSAVEBOXLINES + 1;
      if(min < minSlot)
         min = minSlot;
   }
}

enum class promptQuickSave_e : bool
{
   no  = false,
   yes = true
};

//
// Try to quicksave, either with or without a confirmation prompt
//
static void MN_quickSave(const promptQuickSave_e prompt)
{
   static auto performQuickSave = []() {
      G_SaveGame(e_saveSlots[quickID.slot].fileNum, e_saveSlots[quickID.slot].description.constPtr());
   };

   char tempstring[80];

   if(!usergame && (!demoplayback || netgame))  // killough 10/98
   {
      S_StartInterfaceSound(GameModeInfo->playerSounds[sk_oof]);
      return;
   }

   if(gamestate != GS_LEVEL)
      return;

   MN_readSaveStrings();
   if(quickID.slot < 0)
   {
      quickID.slot = -2; // means to pick a slot now
      MN_StartMenu(GameModeInfo->saveMenu);
      return;
   }

   if(prompt == promptQuickSave_e::yes)
   {
      psnprintf(tempstring, sizeof(tempstring), s_QSPROMPT,
                e_saveSlots[quickID.slot].description.constPtr());
      MN_QuestionFunc(tempstring, performQuickSave);
   }
   else
      performQuickSave();
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

static void MN_loadGameOpen(menu_t *menu);
static void MN_loadGameDrawer();
static bool MN_loadGameResponder(event_t *ev, int action);

static menuwidget_t load_selector = { MN_loadGameDrawer, MN_loadGameResponder, nullptr, true };

menu_t menu_loadgame =
{
   nullptr,
   nullptr, nullptr, nullptr,        // pages
   4, 44,                            // x, y
   0,                                // starting slot
   0,                                // no flags
   nullptr, nullptr, nullptr,        // drawer and content
   0,                                // gap override
   MN_loadGameOpen
};

static void MN_loadGameOpen(menu_t *menu)
{
   MN_PushWidget(&load_selector);
}

static void MN_loadGameDrawer()
{
   int min, max;
   const int numSlots = int(e_saveSlots.getLength());
   const int lheight  = menu_font->cy;
   int y = menu_loadgame.y;

   if(GameModeInfo->type == Game_Heretic)
      MN_drawHereticHeader("Load Game");
   else
   {
      patch_t *patch = PatchLoader::CacheName(wGlobalDir, "M_LOADG", PU_CACHE);
      V_DrawPatch((SCREENWIDTH - patch->width) >> 1, 18, &subscreen43, patch);
   }

   V_DrawBox(0, menu_loadgame.y - 4, SAVEBOXWIDTH, lheight * (NUMSAVEBOXLINES + 1));

   MN_getMinAndMaxSlot(min, max, loadID.slot, 0, numSlots);

   for(int i = min; i <= max; i++)
   {
      int     color;
      qstring text;

      if((i == min && min > 0) || (i == max && max < numSlots - 1))
      {
         color = CR_GOLD;
         text = "More...";
      }
      else
      {
         text = e_saveSlots[i].description;
         if(loadID.slot == i)
         {
            const int skull_x = menu_loadgame.x + 1 + MN_StringWidth(text.constPtr());
            MN_DrawSmallPtr(skull_x, y);
            color = GameModeInfo->selectColor;
         }
         else
            color = GameModeInfo->unselectColor;
      }

      MN_WriteTextColored(text.constPtr(), color, menu_loadgame.x, y);
      y += lheight;
   }
   MN_drawSaveInfo(loadID.slot);
}

static bool MN_loadGameResponder(event_t *ev, int action)
{
   if(MN_handleSaveDeleteInput(loadID, ev, action))
      return true;

   if(MN_handleMenuCloseInput(action))
      return true;

   // Nothing to interact with
   if(e_saveSlots.getLength() == 0)
      return false;

   if(MN_handleNavigationInput(loadID, action, 0))
      return true;

   if(action == ka_menu_confirm)
   {
      qstring loadCmd = qstring("mn_load ") << loadID.slot;
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
   int fileNum;
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

   fileNum = e_saveSlots[slot].fileNum;

   len = M_StringAlloca(&name, 2, 26, basesavegame, savegamename);

   G_SaveGameName(name, len, fileNum);
   G_LoadGame(name, fileNum, false);

   if(quickID.slot == -2)
      quickID = loadID;

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

   MN_readSaveStrings();
   if(quickID.slot < 0)
   {
      quickID.slot = -2; // means to pick a slot now
      MN_StartMenu(GameModeInfo->loadMenu);
      return;
   }

   psnprintf(tempstring, sizeof(tempstring), s_QLPROMPT,
             e_saveSlots[quickID.slot].description.constPtr());
   MN_Question(tempstring, "qload");
}

CONSOLE_COMMAND(qload, cf_hidden)
{
   char *name = nullptr;     // killough 3/22/98
   size_t len;
   int fileNum;

   len = M_StringAlloca(&name, 2, 26, basesavegame, savegamename);

   fileNum = e_saveSlots[quickID.slot].fileNum;

   G_SaveGameName(name, len, fileNum);
   G_LoadGame(name, fileNum, false);
}

/////////////////////////////////////////////////////////////////
//
// Save Game
//

static void MN_saveGameOpen(menu_t *menu);
static void MN_saveGameDrawer();
static bool MN_saveGameResponder(event_t *ev, int action);

static menuwidget_t save_selector = { MN_saveGameDrawer, MN_saveGameResponder, nullptr, true };

menu_t menu_savegame =
{
   nullptr,
   nullptr, nullptr, nullptr,        // pages
   4, 44,                            // x, y
   0,                                // starting slot
   0,                                // no flags
   nullptr, nullptr, nullptr,        // drawer and content
   0,                                // gap override
   MN_saveGameOpen
};

static void MN_saveGameOpen(menu_t *menu)
{
   MN_PushWidget(&save_selector);
}

static void MN_saveGameDrawer()
{
   int min, max;
   int minOffset      = -1;
   const int numSlots = int(e_saveSlots.getLength());
   const int lheight  = menu_font->cy;
   int y = menu_savegame.y;

   if(GameModeInfo->type == Game_Heretic)
      MN_drawHereticHeader("Save Game");
   else
   {
      int lumpnum = W_CheckNumForName("M_SGTTL");
      if(mn_classic_menus || lumpnum == -1)
         lumpnum = W_CheckNumForName("M_SAVEG");
      V_DrawPatch(72, 18, &subscreen43, PatchLoader::CacheNum(wGlobalDir, lumpnum, PU_CACHE));

   }

   V_DrawBox(0, menu_savegame.y - 4, SAVEBOXWIDTH, lheight * (NUMSAVEBOXLINES + 1));

   MN_getMinAndMaxSlot(min, max, saveID.slot, -1, numSlots);

   if(min == -1)
   {
      int         color = GameModeInfo->unselectColor;
      qstring     text("(New Save Game)");
      if(saveID.slot == -1)
      {
         color = GameModeInfo->selectColor;
         if(typing_save_desc)
         {
            text = desc_buffer;
            if(text.length() < SAVESTRINGSIZE &&
               V_FontStringWidth(menu_font, desc_buffer.constPtr()) + V_FontMinWidth(menu_font) <= MAXSAVESTRINGWIDTH)
               text += '_';
         }
         const int skull_x = menu_loadgame.x + 1 + MN_StringWidth(text.constPtr());
         MN_DrawSmallPtr(skull_x, y);
      }
      MN_WriteTextColored(text.constPtr(), color, menu_savegame.x, y);
      y += lheight;
      min++;
      minOffset++;
   }
   for(int i = min; i <= max; i++)
   {
      int    color;
      qstring text;

      if((i == min && min > minOffset) || (i == max && max < numSlots - 1))
      {
         color = CR_GOLD;
         text  = "More...";
      }
      else
      {
         text  = e_saveSlots[i].description;
         if(saveID.slot == i)
         {
            color = GameModeInfo->selectColor;
            if(typing_save_desc)
            {
               text = desc_buffer;
               if(text.length() < SAVESTRINGSIZE &&
                  V_FontStringWidth(menu_font, desc_buffer.constPtr()) + V_FontMinWidth(menu_font) <= MAXSAVESTRINGWIDTH)
                  text += '_';
            }
            const int skull_x = menu_loadgame.x + 1 + MN_StringWidth(text.constPtr());
            MN_DrawSmallPtr(skull_x, y);
         }
         else
            color = GameModeInfo->unselectColor;
      }

      MN_WriteTextColored(text.constPtr(), color, menu_savegame.x, y);
      y += lheight;
   }
   MN_drawSaveInfo(saveID.slot);
}

static bool MN_saveGameResponder(event_t *ev, int action)
{
   if(ev->type == ev_keydown && typing_save_desc)
   {
      if(action == ka_menu_toggle) // cancel input
         typing_save_desc = false;
      else if(action == ka_menu_confirm)
      {
         if(desc_buffer.length())
         {
            qstring saveCmd = qstring("mn_save ") << saveID.slot;
            S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_COMMAND]); // make sound
            Console.cmdtype = c_menu;
            C_RunTextCmd(saveCmd.constPtr());

            typing_save_desc = false;
         }
         return true;
      }
      else if(ev->data1 == KEYD_BACKSPACE)
      {
         desc_buffer.Delc();
         return true;
      }

      return true;
   }
   else if(ev->type == ev_text && typing_save_desc)
   {
      // just a normal character
      const unsigned char ich = static_cast<unsigned char>(ev->data1);
      const char          uch = static_cast<char>(ich);

      if(ectype::isPrint(ich) && desc_buffer.length() < SAVESTRINGSIZE &&
         V_FontStringWidth(menu_font, desc_buffer.constPtr()) + V_FontCharWidth(menu_font, uch) <= MAXSAVESTRINGWIDTH)
         desc_buffer += uch;

      return true;
   }
   else if(!typing_save_desc && MN_handleSaveDeleteInput(saveID, ev, action))
      return true;

   if(MN_handleMenuCloseInput(action))
      return true;

   if(e_saveSlots.getLength() != 0 && MN_handleNavigationInput(saveID, action, -1))
      return true;

   if(action == ka_menu_confirm)
   {
      if(saveID.slot == -1)
         desc_buffer.clear();
      else
         desc_buffer = e_saveSlots[saveID.slot].description;

      typing_save_desc = true;
      return true;
   }

   return false;
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

CONSOLE_COMMAND(mn_save, 0)
{
   const size_t numSlots = e_saveSlots.getLength();

   if(Console.argc < 1)
      return;

   saveID.slot = Console.argv[0]->toInt();

   if(gamestate != GS_LEVEL)
      return; // only save in level

   if(saveID.slot < -1 || saveID.slot >= int(numSlots))
      return; // sanity check

   if(numSlots == 0)
      saveID = { 0, 0 };
   else if(saveID.slot == -1)
   {
      saveID_t *saveIDs = estructalloc(saveID_t, numSlots);

      for(int i = 0; i < numSlots; i++)
         saveIDs[i] = { i, e_saveSlots[i].fileNum };

      // Sort the save IDs from most lowest to highest file number
      qsort(
         saveIDs, numSlots, sizeof(saveID_t),
         [](const void *i1, const void *i2) {
            return static_cast<const saveID_t *>(i1)->fileNum - static_cast<const saveID_t *>(i2)->fileNum;
         }
      );

      saveID_t lowestSaveID = { -1, -1 };
      int      lastSaveNum  = -1;
      for(int i = 0; i < numSlots; i++)
      {
         if(saveIDs[i].fileNum > lastSaveNum + 1)
         {
            lowestSaveID = { i, lastSaveNum + 1 };
            break;
         }
         lastSaveNum = saveIDs[i].fileNum;
      }
      if(lowestSaveID.fileNum == -1)
         saveID = { int(numSlots), int(numSlots) };
      else
         saveID = lowestSaveID;

      efree(saveIDs);
   }
   else
      saveID.fileNum = e_saveSlots[saveID.slot].fileNum;

   G_SaveGame(saveID.fileNum, desc_buffer.constPtr());
   MN_ClearMenus();

   // haleyjd 02/23/02: restored from MBF
   if(quickID.slot == -2)
      quickID = saveID;

   // haleyjd 10/08/08: GIF_SAVESOUND flag
   if(GameModeInfo->flags & GIF_SAVESOUND)
      S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_DEACTIVATE]);
}

// haleyjd 02/23/02: Quick Save -- restored from MBF, converted to
// use console commands
CONSOLE_COMMAND(quicksave, 0)
{
   MN_quickSave(promptQuickSave_e::yes);
}

CONSOLE_COMMAND(qsave, cf_hidden)
{
   MN_quickSave(promptQuickSave_e::no);
}

//VARIABLE_STRING(save_game_desc, nullptr, SAVESTRINGSIZE);
//CONSOLE_VARIABLE(mn_savegamedesc, save_game_desc, 0) {}

static void MN_deleteSave()
{
   char *name;
   size_t len;
   int fileNum;

   if(!menuactive)
      return;

   if(current_menu == &menu_loadgame)
      fileNum = loadID.fileNum;
   else if(current_menu == &menu_savegame)
      fileNum = saveID.fileNum;
   else
      return;

   len = M_StringAlloca(&name, 2, 26, basesavegame, savegamename);

   G_SaveGameName(name, len, fileNum);
   remove(name);
   MN_readSaveStrings();
}

// EOF

