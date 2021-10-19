// Emacs style mode select   -*- C++ -*- 
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
// DESCRIPTION:
//  Heretic Intermission screens.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_gi.h"
#include "doomstat.h"
#include "e_fonts.h"
#include "e_string.h"
#include "g_game.h"
#include "in_lude.h"
#include "p_info.h"
#include "s_sound.h"
#include "v_block.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"

extern vfont_t *in_font;
extern vfont_t *in_bigfont;
extern vfont_t *in_bignumfont;

extern char gamemapname[9];

// Macros

#define HIS_NOWENTERING "NOW ENTERING:"
#define HIS_FINISHED    "FINISHED"
#define HIS_KILLS       "KILLS"
#define HIS_BONUS       "BONUS"
#define HIS_ITEMS       "ITEMS"
#define HIS_SECRETS     "SECRETS"
#define HIS_SECRET      "SECRET"
#define HIS_TIME        "TIME"
#define HIS_TOTAL       "TOTAL"
#define HIS_VICTIMS     "VICTIMS"
#define HIS_KILLERS     "K\0I\0L\0L\0E\0R\0S"

enum
{
   NUM_OVERWORLD_EPISODES = 3,
   NUM_LEVELS_PER_EPISODE = 9,
};

// Private Data

struct hipoint_t
{
   int x;
   int y;
};

static hipoint_t hipoints[NUM_OVERWORLD_EPISODES][NUM_LEVELS_PER_EPISODE] =
{
   {
      { 172, 78 },
      { 86,  90 },
      { 73,  66 },
      { 159, 95 },
      { 148, 126 },
      { 132, 54 },
      { 131, 74 },
      { 208, 138 },
      { 52,  101 }
   },
   {
      { 218, 57 },
      { 137, 81 },
      { 155, 124 },
      { 171, 68 },
      { 250, 86 },
      { 136, 98 },
      { 203, 90 },
      { 220, 140 },
      { 279, 106 }
   },
   {
      { 86,  99 },
      { 124, 103 },
      { 154, 79 },
      { 202, 83 },
      { 178, 59 },
      { 142, 58 },
      { 219, 66 },
      { 247, 57 },
      { 107, 80 }
   }
};

enum
{
   INTR_NONE = -1,
   INTR_STATS,
   INTR_LEAVING,
   INTR_GOING,
   INTR_WAITING,
};

static int interstate;

static int statetime;             // time until next state
static int countdown;             // count down to end
static bool flashtime = false;

static wbstartstruct_t hi_wbs;

// graphic patches
static patch_t *hi_interpic;
static patch_t *hi_exitpic;
static patch_t *hi_in_x;
static patch_t *hi_in_yah;

static int hi_faces[4];
static int hi_dead_faces[4];

// 03/27/05: EDF strings for intermission level names
static const char *mapName;
static const char *nextMapName;

// 04/25/09: deathmatch data
static fixed_t dSlideX[MAXPLAYERS];
static fixed_t dSlideY[MAXPLAYERS];
static int slaughterboy;

// 05/06/09: color block bg stuff
static byte blockcolor;
static int  blockfade;

// Private functions

//
// Helper to know if an episode index is for an overworld map
//
inline static bool overworld(int epid)
{
   return epid >= 0 && epid < NUM_OVERWORLD_EPISODES;
}

//
// Common handling of next-episode overworld and next enterpic.
//
inline static bool HI_haveEnteringPic()
{
   return overworld(hi_wbs.nextEpisode) || estrnonempty(hi_wbs.li_nextenterpic);
}
inline static bool HI_haveExitingPic()
{
   return overworld(hi_wbs.epsd) || estrnonempty(hi_wbs.li_lastexitpic);
}
inline static bool HI_singlePage()
{
   return !HI_haveExitingPic() && !HI_haveEnteringPic();
}

static void HI_DrawBackground(void);

static void HI_loadData(void)
{
   int i;
   char mapname[9];

   memset(mapname, 0, 9);

   // load interpic
   hi_interpic = nullptr;
   hi_exitpic = nullptr;

   if(estrnonempty(hi_wbs.li_lastexitpic))
      hi_exitpic = PatchLoader::CacheName(wGlobalDir, hi_wbs.li_lastexitpic, PU_STATIC);
   else if(overworld(hi_wbs.epsd))
   {
      snprintf(mapname, sizeof(mapname), "MAPE%d", hi_wbs.epsd + 1);
      hi_exitpic = PatchLoader::CacheName(wGlobalDir, mapname, PU_STATIC);
   }

   if(estrnonempty(hi_wbs.li_nextenterpic))
      hi_interpic = PatchLoader::CacheName(wGlobalDir, hi_wbs.li_nextenterpic, PU_STATIC);
   else if(overworld(hi_wbs.nextEpisode))
   {
      snprintf(mapname, sizeof(mapname), "MAPE%d", hi_wbs.nextEpisode + 1);
      hi_interpic = PatchLoader::CacheName(wGlobalDir, mapname, PU_STATIC);
   }

   // load positional indicators
   hi_in_x   = PatchLoader::CacheName(wGlobalDir, "IN_X",   PU_STATIC);
   hi_in_yah = PatchLoader::CacheName(wGlobalDir, "IN_YAH", PU_STATIC);

   // get lump numbers for faces
   for(i = 0; i < 4; i++)
   {
      char tempstr[9];

      memset(tempstr, 0, 9);

      snprintf(tempstr, sizeof(tempstr), "FACEA%.1d", i);
      hi_faces[i] = W_GetNumForName(tempstr);

      snprintf(tempstr, sizeof(tempstr), "FACEB%.1d", i);
      hi_dead_faces[i] = W_GetNumForName(tempstr);
   }

   // haleyjd 03/27/05: EDF-defined intermission map names
   mapName     = nullptr;
   nextMapName = nullptr;

   {
      char nameBuffer[24];
      const char *basename;
      edf_string_t *str;

      // set current map
      if(hi_wbs.li_lastlevelname && *hi_wbs.li_lastlevelname)
         mapName = hi_wbs.li_lastlevelname;
      else
      {
         psnprintf(nameBuffer, 24, "_IN_NAME_%s", gamemapname);
         if((str = E_StringForName(nameBuffer)))
            mapName = str->string;
      }

      if(hi_wbs.li_nextlevelname && *hi_wbs.li_nextlevelname)
      {
         nextMapName = hi_wbs.li_nextlevelname;
         return;
      }

      // are we going to a secret level?
      basename = hi_wbs.gotosecret ? LevelInfo.nextSecret : LevelInfo.nextLevel;

      // set next map
      if(*basename)
      {
         psnprintf(nameBuffer, 24, "_IN_NAME_%s", basename);

         if((str = E_StringForName(nameBuffer)))
            nextMapName = str->string;
      }
      else
      {
         // try ExMy and MAPxy defaults for normally-named maps
         if(isExMy(gamemapname))
         {
            psnprintf(nameBuffer, 24, "_IN_NAME_E%01dM%01d", 
                      hi_wbs.nextEpisode + 1, hi_wbs.next + 1);
            if((str = E_StringForName(nameBuffer)))
               nextMapName = str->string;
         }
         else if(isMAPxy(gamemapname))
         {
            psnprintf(nameBuffer, 24, "_IN_NAME_MAP%02d", hi_wbs.next + 1);
            if((str = E_StringForName(nameBuffer)))
               nextMapName = str->string;
         }
      }
   }
}

// Uncomment this define to see all players in single-player mode
//#define HI_DEBUG_ALLPLAYERS

//
// HI_playerInGame
//
// Returns true if the player is considered to be participating.
//
static bool HI_playerInGame(int playernum)
{
#ifdef HI_DEBUG_ALLPLAYERS
   return true;
#else
   return playeringame[playernum];
#endif
}

//
// HI_initStats
//
// Sets up deathmatch stuff.
//
static void HI_initStats(void)
{
   int i;
   int posnum = 0;
   int slaughterfrags;
   int slaughtercount;
   int playercount = 0;

   if(GameType != gt_dm)
      return;

   blockcolor = 144;
   blockfade  = -1;

   slaughterboy   = 0;
   slaughterfrags = D_MININT;
   slaughtercount = 0;

   for(i = 0; i < MAXPLAYERS; ++i)
   {
      if(HI_playerInGame(i))
      {
         dSlideX[i] = 43 * posnum * FRACUNIT / 20;
         dSlideY[i] = 36 * posnum * FRACUNIT / 20;
         ++posnum;
         ++playercount;

         // determine players with the most kills
         if(players[i].totalfrags > slaughterfrags)
         {
            slaughterboy   = 1 << i;
            slaughterfrags = players[i].totalfrags;
            slaughtercount = 1;
         }
         else if(players[i].totalfrags == slaughterfrags)
         {
            slaughterboy |= 1 << i;
            ++slaughtercount;
         }
      }
   }
   
   // no slaughterboy stuff if everyone is equal
   if(slaughtercount == playercount)
      slaughterboy = 0;
}

static void HI_Stop(void)
{
   if(hi_interpic)
      Z_ChangeTag(hi_interpic, PU_CACHE);
   if(hi_exitpic)
      Z_ChangeTag(hi_exitpic, PU_CACHE);

   Z_ChangeTag(hi_in_x, PU_CACHE);
   Z_ChangeTag(hi_in_yah, PU_CACHE);
}

// Drawing functions

//
// HI_drawNewLevelName
//
// Draws "NOW ENTERING:" and the destination level name centered,
// starting at the given y coordinate.
//
static void HI_drawNewLevelName(int y)
{
   int x;
   const char *thisLevelName;

   x = (SCREENWIDTH - V_FontStringWidth(in_font, HIS_NOWENTERING)) >> 1;
   V_FontWriteText(in_font, HIS_NOWENTERING, x, y, &subscreen43);

   if(nextMapName)
      thisLevelName = nextMapName;
   else
      thisLevelName = "new level";

   x = (SCREENWIDTH - V_FontStringWidth(in_bigfont, thisLevelName)) >> 1;
   V_FontWriteTextShadowed(in_bigfont, thisLevelName, x, y + 10, &subscreen43);
}

//
// HI_drawOldLevelName
//
// Draws previous level name and "FINISHED" centered,
// starting at the given y coordinate.
//
static void HI_drawOldLevelName(int y)
{
   int x;
   const char *oldLevelName;

   if(mapName)
      oldLevelName = mapName;
   else
      oldLevelName = "new level";

   x = (SCREENWIDTH - V_FontStringWidth(in_bigfont, oldLevelName)) / 2;
   V_FontWriteTextShadowed(in_bigfont, oldLevelName, x, y, &subscreen43);

   x = (SCREENWIDTH - V_FontStringWidth(in_font, HIS_FINISHED)) / 2;
   V_FontWriteText(in_font, HIS_FINISHED, x, y + 22, &subscreen43);
}

//
// HI_drawGoing
//
// Drawer function for the final intermission stage where the
// "now entering" is shown, and a pointer blinks at the next
// stage on the map.
//
static void HI_drawGoing()
{
   int i, previous;

   HI_drawNewLevelName(10);

   // don't proceed by drawing the location indicators if using "enterpic".
   // Keep them for a more advanced scripting.
   if(estrnonempty(hi_wbs.li_nextenterpic) || !overworld(hi_wbs.nextEpisode))
      return;
   
   previous = hi_wbs.last;

   // handle secret level
   if(previous == 8)
   {
      previous = hi_wbs.next - 1;
   }

   // draw patches on levels visited
   if(hi_wbs.nextEpisode == hi_wbs.epsd)
   {
      for(i = 0; i <= previous; i++)
      {
         if(i >= NUM_LEVELS_PER_EPISODE)
            break;
         V_DrawPatch(hipoints[hi_wbs.epsd][i].x,
                     hipoints[hi_wbs.epsd][i].y,
                     &subscreen43,
                     hi_in_x);
      }

      // draw patch on secret level
      if(hi_wbs.didsecret)
      {
         V_DrawPatch(hipoints[hi_wbs.epsd][8].x,
                     hipoints[hi_wbs.epsd][8].y,
                     &subscreen43,
                     hi_in_x);
      }
   }
   // blink destination arrow
   if(flashtime && hi_wbs.next >= 0 && hi_wbs.next < NUM_LEVELS_PER_EPISODE)
   {
      V_DrawPatch(hipoints[hi_wbs.nextEpisode][hi_wbs.next].x,
                  hipoints[hi_wbs.nextEpisode][hi_wbs.next].y,
                  &subscreen43,
                  hi_in_yah);
   }
}

//
// HI_drawLeaving
//
// Drawer function for first map stage, where "leaving..." is
// shown and the previous levels are all X'd out
//
static void HI_drawLeaving(void)
{
   int i;
   int lastlevel, thislevel;
   bool drawsecret;

   HI_drawOldLevelName(3);

   if(estrnonempty(hi_wbs.li_lastexitpic) || !overworld(hi_wbs.epsd))
      return;

   if(hi_wbs.last == 8)
   {
      // leaving secret level, handle specially
      lastlevel = hi_wbs.next;
      thislevel = 8;
      drawsecret = false;
   }
   else
   {
      lastlevel = thislevel = hi_wbs.last;
      drawsecret = hi_wbs.didsecret;
   }

   // draw all the previous levels
   for(i = 0; i < lastlevel; ++i)
   {
      if(i >= NUM_LEVELS_PER_EPISODE)
         break;
      V_DrawPatch(hipoints[hi_wbs.epsd][i].x,
                  hipoints[hi_wbs.epsd][i].y,
                  &subscreen43,
                  hi_in_x);
   }

   // draw the secret level if appropriate
   if(drawsecret)
   {
      V_DrawPatch(hipoints[hi_wbs.epsd][8].x, 
                  hipoints[hi_wbs.epsd][8].y,
                  &subscreen43,
                  hi_in_x);
   }

   // blink the level we're leaving
   if(flashtime && thislevel >= 0 && thislevel < NUM_LEVELS_PER_EPISODE)
   {
      V_DrawPatch(hipoints[hi_wbs.epsd][thislevel].x, 
                  hipoints[hi_wbs.epsd][thislevel].y,
                  &subscreen43,
                  hi_in_x);
   }   
}

//
// HI_drawLevelStat
//
// Draws a pair of "Font B" numbers separated by a slash
//
static void HI_drawLevelStat(int stat, int max, int x, int y)
{
   char str[16];

   sprintf(str, "%3d", stat);
   V_FontWriteTextShadowed(in_bignumfont, str, x, y, &subscreen43);
   
   V_FontWriteTextShadowed(in_bigfont, "/", x + 37, y, &subscreen43);

   sprintf(str, "%3d", max);
   V_FontWriteTextShadowed(in_bignumfont, str, x + 48, y, &subscreen43);
}

//
// HI_drawLevelStatPct
//
// Draws a single "Font B" number with a percentage sign.
//
static void HI_drawLevelStatPct(int stat, int x, int y, int pctx)
{
   char str[16];

   sprintf(str, "%3d", stat);
   V_FontWriteTextShadowed(in_bignumfont, str,  x,    y, &subscreen43);
   V_FontWriteTextShadowed(in_bigfont,    "%",  pctx, y, &subscreen43);
}

static void HI_drawFragCount(int count, int x, int y)
{
   char str[16];

   sprintf(str, "%3d", count);
   V_FontWriteTextShadowed(in_bignumfont, str, x, y, &subscreen43);
}

//
// HI_drawTime
//
// Draws a "Font B" HH:MM:SS time indication. Nothing is
// shown for hours if h is zero, but minutes and seconds
// are always shown no matter what. It looks best this way.
//
static void HI_drawTime(int h, int m, int s, int x, int y)
{
   char timestr[16];

   if(h)
   {
      sprintf(timestr, "%02d", h);
      V_FontWriteTextShadowed(in_bignumfont, timestr, x,      y, &subscreen43);
      V_FontWriteTextShadowed(in_bigfont,    ":",     x + 26, y, &subscreen43);
   }

   sprintf(timestr, "%02d", m);
   V_FontWriteTextShadowed(in_bignumfont, timestr, x + 34, y, &subscreen43);
   V_FontWriteTextShadowed(in_bigfont,    ":",     x + 60, y, &subscreen43);

   sprintf(timestr, "%02d", s);
   V_FontWriteTextShadowed(in_bignumfont, timestr, x + 68, y, &subscreen43);
}

//
// HI_drawSingleStats
//
// Drawer function for single-player mode stats stage.
// Shows kills, items, secrets, and either time or the next
// level name (depending on whether we're in a SOSR episode
// or not). Note Heretic has no par times.
//
static void HI_drawSingleStats(void)
{
   static int statstage = 0;

   V_FontWriteTextShadowed(in_bigfont, HIS_KILLS,   50,  65, &subscreen43);
   V_FontWriteTextShadowed(in_bigfont, HIS_ITEMS,   50,  90, &subscreen43);
   V_FontWriteTextShadowed(in_bigfont, HIS_SECRETS, 50, 115, &subscreen43);

   HI_drawOldLevelName(3);

   // prior to tic 30: draw nothing
   if(intertime < 30)
   {
      statstage = 0;
      return;
   }

   // tics 30 to 60: add kill count
   if(intertime > 30)
   {
      if(statstage == 0)
      {
         S_StartInterfaceSound(sfx_hdorcls);
         statstage = 1;
      }

      HI_drawLevelStat(players[consoleplayer].killcount,
                       hi_wbs.maxkills, 200, 65);
   }
   
   // tics 60 to 90: add item count
   if(intertime > 60)
   {
      if(statstage == 1)
      {
         S_StartInterfaceSound(sfx_hdorcls);
         statstage = 2;
      }

      HI_drawLevelStat(players[consoleplayer].itemcount,
                       hi_wbs.maxitems, 200, 90);
   }
   
   // tics 90 to 150: add secret count
   if(intertime > 90)
   {
      if(statstage == 2)
      {
         S_StartInterfaceSound(sfx_hdorcls);
         statstage = 3;
      }

      HI_drawLevelStat(players[consoleplayer].secretcount,
                       hi_wbs.maxsecret, 200, 115);
   }

   // 150 to end: show time or next level name
   // Note that hitting space earlier than 150 sets the ticker to
   // 150 so that we jump here.

   if(intertime > 150)
   {
      if(statstage == 3)
      {
         S_StartInterfaceSound(sfx_hdorcls);
         statstage = 4;
      }

      if(!HI_singlePage())
      {         
         int time, hours, minutes, seconds;
         
         time = hi_wbs.plyr[consoleplayer].stime / TICRATE;
         
         hours = time / 3600;
         time -= hours * 3600;
         
         minutes = time / 60;
         time -= minutes * 60;
         
         seconds = time;

         V_FontWriteTextShadowed(in_bigfont, HIS_TIME, 85, 160, &subscreen43);
         HI_drawTime(hours, minutes, seconds, 155, 160);
      }
      else
      {
         HI_drawNewLevelName(160);
         acceleratestage = false;
      }
   }
}

//
// HI_drawCoopStats
//
// Draws the cooperative-mode statistics matrix. Nothing fancy.
//
static void HI_drawCoopStats(void)
{
   int i, ypos;
   static int statstage;

   V_FontWriteTextShadowed(in_bigfont, HIS_KILLS,   95, 35, &subscreen43);
   V_FontWriteTextShadowed(in_bigfont, HIS_BONUS,  155, 35, &subscreen43);
   V_FontWriteTextShadowed(in_bigfont, HIS_SECRET, 232, 35, &subscreen43);

   HI_drawOldLevelName(3);

   ypos = 50;

   if(intertime < 40)
      statstage = 0;
   else if(intertime >= 40 && statstage == 0)
   {
      statstage = 1;
      S_StartInterfaceSound(sfx_hdorcls);
   }

   for(i = 0; i < MAXPLAYERS; ++i)
   {
      if(HI_playerInGame(i))
      {
         V_DrawPatchShadowed(
            25, ypos, &subscreen43,
            PatchLoader::CacheNum(wGlobalDir, hi_faces[i], PU_CACHE),
            nullptr, FRACUNIT
         );

         if(statstage == 1)
         {
            int kills = 0, items = 0, secrets = 0;

            if(hi_wbs.maxkills != 0)
               kills = hi_wbs.plyr[i].skills * 100 / hi_wbs.maxkills;
            if(hi_wbs.maxitems != 0)
               items = hi_wbs.plyr[i].sitems * 100 / hi_wbs.maxitems;
            if(hi_wbs.maxsecret != 0)
               secrets = hi_wbs.plyr[i].ssecret * 100 / hi_wbs.maxsecret;

            HI_drawLevelStatPct(kills,   85,  ypos + 10, 121);
            HI_drawLevelStatPct(items,   160, ypos + 10, 196);
            HI_drawLevelStatPct(secrets, 237, ypos + 10, 273);
         }

         ypos += 37;
      }
   }
}

static int dmstatstage;

//
// HI_dmFaceTLLevel
//
// Returns a translucency level at which to draw a face on the DM stats screen.
//
static fixed_t HI_dmFaceTLLevel(int playernum)
{
   fixed_t ret;

   if(intertime < 100 || !slaughterboy || (slaughterboy & (1 << playernum)))
      ret = FRACUNIT;
   else if(intertime < 120)
      ret = FRACUNIT - (intertime - 100) * ((FRACUNIT - HTIC_GHOST_TRANS) / 20);
   else
      ret = HTIC_GHOST_TRANS;

   return ret;
}

//
// HI_drawDMStats
//
// Draws deathmatch statistics.
//
static void HI_drawDMStats(void)
{
   int i;
   int xpos, ypos, kpos;
   const char *killers = HIS_KILLERS;

   V_FontWriteTextShadowed(in_bigfont, HIS_TOTAL, 265, 30, &subscreen43);
   V_FontWriteText(in_font, HIS_VICTIMS, 140, 8, &subscreen43);

   for(i = 0; i < 7; ++i)
      V_FontWriteText(in_font, killers + i*2, 10, 80 + 9 * i, &subscreen43);

   xpos = 90;
   ypos = 55;

   if(intertime <= 1)
      return;

   if(intertime < 20)
   {
      dmstatstage = 0;

      for(i = 0; i < MAXPLAYERS; ++i)
      {
         if(HI_playerInGame(i))
         {
            V_DrawPatchGeneral(40, 
               (ypos*FRACUNIT + dSlideY[i]*intertime)>>FRACBITS,
               &subscreen43,
               PatchLoader::CacheNum(wGlobalDir, hi_faces[i], PU_CACHE), 
               false);
            V_DrawPatchGeneral((xpos*FRACUNIT + dSlideX[i]*intertime)>>FRACBITS,
               18,
               &subscreen43,
               PatchLoader::CacheNum(wGlobalDir, hi_dead_faces[i], PU_CACHE),
               false);
         }
      }
   }

   if(intertime >= 20)
   {
      if(dmstatstage == 0)
      {
         S_StartInterfaceSound(sfx_hdorcls);
         dmstatstage = 1;
      }

      if(intertime >= 100)
      {
         if(dmstatstage == 1)
         {
            if(slaughterboy)
               S_StartInterfaceSound(sfx_hwpnup);
            dmstatstage = 2;
         }
      }

      for(i = 0; i < MAXPLAYERS; ++i)
      {
         if(HI_playerInGame(i))
         {
            int tllevel = HI_dmFaceTLLevel(i);

            if(i == consoleplayer)
            {
               V_ColorBlockTLScaled(&subscreen43, blockcolor, xpos, ypos + 3, 
                                    (263-(xpos-3)+1) + 48, 36 - 5, 
                                    FRACUNIT/3);
            }

            V_DrawPatchTL(
               40, ypos, &subscreen43,
               PatchLoader::CacheNum(wGlobalDir, hi_faces[i], PU_CACHE),
               nullptr, tllevel
            );
            V_DrawPatchTL(
               xpos, 18, &subscreen43,
               PatchLoader::CacheNum(wGlobalDir, hi_dead_faces[i], PU_CACHE),
               nullptr, tllevel
            );

            kpos = 86;

            for(int j = 0; j < MAXPLAYERS; ++j)
            {
               if(HI_playerInGame(j))
               {
                  HI_drawFragCount(players[i].frags[j], kpos, ypos + 10);
                  kpos += 43;
               }
            }

            if(slaughterboy & (1 << i)) // is this player a leader?
            {
               if(!(intertime & 16))
                  HI_drawFragCount(players[i].totalfrags, 263, ypos + 10);
            }
            else
               HI_drawFragCount(players[i].totalfrags, 263, ypos + 10);

            ypos += 36;
            xpos += 43;
         }
      }
   }
}

static void HI_Ticker(void)
{
   if(GameType == gt_dm)
   {      
      if(!(intertime % 3))
         blockcolor += blockfade;

      if(blockcolor == 144 && blockfade == 1)
         blockfade = -1;
      else if(blockcolor == 137 && blockfade == -1)
         blockfade = 1;
   }

   if(interstate == INTR_WAITING)
   {
      countdown--;
      
      if(!countdown)
      {
         HI_Stop();
         G_WorldDone();
      }

      return;
   }

   if(statetime < intertime)
   {
      interstate++;
      
      if(HI_singlePage() && interstate > INTR_STATS)
      {
         // extended episodes have no map screens
         interstate = INTR_WAITING;
      }

      switch(interstate)
      {
      case INTR_STATS:
         statetime = intertime + (HI_singlePage() ? 1200 : 300);
         break;
      case INTR_LEAVING:
         statetime = intertime + 200;
         HI_DrawBackground();             // change the background
         break;
      case INTR_GOING:
         statetime = D_MAXINT;
         break;
      case INTR_WAITING:
         countdown = 10;
         break;
      default:
         break;
      }
   }

   // update flashing graphics state
   flashtime = !(intertime & 16) || (interstate == INTR_WAITING);

   // see if we should skip ahead
   if(acceleratestage)
   {
      if(interstate == INTR_STATS && intertime < 150)
      {
         intertime = 150;
      }
      else if(interstate < INTR_GOING && !HI_singlePage())
      {
         interstate = INTR_GOING;
         HI_DrawBackground(); // force a background change
         S_StartInterfaceSound(sfx_hdorcls);
      }
      else
      {
         interstate = INTR_WAITING;
         countdown = 10;
         flashtime = true;

         S_StartInterfaceSound(sfx_hdorcls);
      }

      // clear accelerate flag
      acceleratestage = 0;
   }
}

//
// HI_DrawBackground
//
// Called when the background needs to be changed.
//
static void HI_DrawBackground(void)
{
   switch(interstate)
   {
      case INTR_LEAVING:
         if(hi_exitpic)
            V_DrawPatch(0, 0, &subscreen43, hi_exitpic);
         else
            V_DrawBackground("FLOOR16", &subscreen43);
         break;
      case INTR_GOING:
      case INTR_WAITING:
         if(hi_interpic)
            V_DrawPatch(0, 0, &subscreen43, hi_interpic);
         else
            V_DrawBackground("FLOOR16", &subscreen43);
         break;
      default:
         V_DrawBackground("FLOOR16", &subscreen43);
         break;
   }
}

static void HI_Drawer()
{
   static int oldinterstate;

   if(interstate == INTR_WAITING)
      return;
   
   if(oldinterstate != INTR_GOING && interstate == INTR_GOING)
      S_StartInterfaceSound(sfx_hpstop);

   // swap the background onto the screen
   IN_slamBackground();

   switch(interstate)
   {
   case INTR_STATS:
      switch(GameType)
      {
      case gt_single:
         HI_drawSingleStats();
         break;
      case gt_coop:
         HI_drawCoopStats();
         break;
      default:
         HI_drawDMStats();
         break;
      }
      break;

   case INTR_LEAVING:
      HI_drawLeaving();
      break;
   case INTR_GOING:
      HI_drawGoing();
      break;
   default:
      break;
   }

   oldinterstate = interstate;
}

static void HI_Start(wbstartstruct_t *wbstartstruct)
{
   acceleratestage = 0;
   statetime = -1;
   intertime = 0;
   interstate = INTR_NONE;

   hi_wbs = *wbstartstruct;

   HI_loadData();
   HI_initStats();
}

// Heretic Intermission object

interfns_t HticIntermission =
{
   HI_Ticker,
   HI_DrawBackground,
   HI_Drawer,
   HI_Start,
};

// EOF




