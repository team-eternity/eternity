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
//  DOOM Intermission screens.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "am_map.h"
#include "c_io.h"
#include "d_deh.h"
#include "d_gi.h"
#include "doomstat.h"
#include "e_fonts.h"
#include "e_string.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "m_qstr.h"
#include "m_random.h"
#include "m_swap.h"
#include "p_info.h"
#include "p_mobj.h"
#include "p_tick.h"
#include "r_main.h"
#include "r_patch.h"
#include "s_sound.h"
#include "sounds.h"
#include "st_stuff.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_levels.h"
#include "w_wad.h"
#include "wi_stuff.h"

extern vfont_t *in_bigfont;

extern char gamemapname[9];

// Ty 03/17/98: flag that new par times have been loaded in d_deh
// haleyjd: moved to header

//
// Data needed to add patches to full screen intermission pics.
// Patches are statistics messages, and animations.
// Loads of by-pixel layout and placement, offsets etc.
//

//
// Different between registered DOOM (1994) and
//  Ultimate DOOM - Final edition (retail, 1995?).
// This is supposedly ignored for commercial
//  release (aka DOOM II), which had 34 maps
//  in one episode. So there.
#define NUMEPISODES 4
#define NUMMAPS     9

// Not used
// in tics
//U #define PAUSELEN    (TICRATE*2) 
//U #define SCORESTEP    100
//U #define ANIMPERIOD    32
// pixel distance from "(YOU)" to "PLAYER N"
//U #define STARDIST  10 
//U #define WK 1

// GLOBAL LOCATIONS
#define WI_TITLEY      2
#define WI_SPACINGY   33

// SINGLE-PLAYER STUFF
#define SP_STATSX     50
#define SP_STATSY     50

#define SP_TIMEX      16
#define SP_TIMEY      (SCREENHEIGHT-32)

// NET GAME STUFF
#define NG_STATSY     50
#define NG_STATSX     (32 + star->width/2 + 32*!dofrags)

#define NG_SPACINGX   64

// Used to display the frags matrix at endgame
// DEATHMATCH STUFF
#define DM_MATRIXX    42
#define DM_MATRIXY    68

#define DM_SPACINGX   40

#define DM_TOTALSX   269

#define DM_KILLERSX   10
#define DM_KILLERSY  100
#define DM_VICTIMSX    5
#define DM_VICTIMSY   50

enum
{
   NUM_OVERWORLD_EPISODES = 3,
   NUM_LEVELS_PER_EPISODE = 9,
};

// These animation variables, structures, etc. are used for the
// DOOM/Ultimate DOOM intermission screen animations.  This is
// totally different from any sprite or texture/flat animations
typedef enum
{
   ANIM_ALWAYS,   // determined by patch entry
   ANIM_RANDOM,   // occasional
   ANIM_LEVEL     // continuous
} animenum_t;

struct point_t
{
   int   x;       // x/y coordinate pair structure
   int   y;
};

//
// Animation.
// There is another anim_t used in p_spec.
//
struct anim_t
{
   animenum_t  type;

   // period in tics between animations
   int   period;

   // number of animation frames
   int   nanims;

   // location of animation
   point_t loc;

   // ALWAYS: n/a,
   // RANDOM: period deviation (<256),
   // LEVEL: level
   int   data1;

   // ALWAYS: n/a,
   // RANDOM: random base period,
   // LEVEL: n/a
   int   data2;

   // actual graphics for frames of animations
   patch_t*  p[3];

   // following must be initialized to zero before use!

   // next value of intertime (used in conjunction with period)
   int   nexttic;

   // last drawn animation frame
   int   lastdrawn;

   // next frame number to animate
   int   ctr;

   // used by RANDOM and LEVEL when animating
   int   state;
};

static point_t lnodes[NUMEPISODES][NUMMAPS] =
{
   // Episode 0 World Map
   {
      { 185, 164 }, // location of level 0 (CJ)
      { 148, 143 }, // location of level 1 (CJ)
      { 69, 122 },  // location of level 2 (CJ)
      { 209, 102 }, // location of level 3 (CJ)
      { 116, 89 },  // location of level 4 (CJ)
      { 166, 55 },  // location of level 5 (CJ)
      { 71, 56 },   // location of level 6 (CJ)
      { 135, 29 },  // location of level 7 (CJ)
      { 71, 24 }    // location of level 8 (CJ)
   },
  
   // Episode 1 World Map should go here
   {
      { 254, 25 },  // location of level 0 (CJ)
      { 97, 50 },   // location of level 1 (CJ)
      { 188, 64 },  // location of level 2 (CJ)
      { 128, 78 },  // location of level 3 (CJ)
      { 214, 92 },  // location of level 4 (CJ)
      { 133, 130 }, // location of level 5 (CJ)
      { 208, 136 }, // location of level 6 (CJ)
      { 148, 140 }, // location of level 7 (CJ)
      { 235, 158 }  // location of level 8 (CJ)
   },
      
   // Episode 2 World Map should go here
   {
      { 156, 168 }, // location of level 0 (CJ)
      { 48, 154 },  // location of level 1 (CJ)
      { 174, 95 },  // location of level 2 (CJ)
      { 265, 75 },  // location of level 3 (CJ)
      { 130, 48 },  // location of level 4 (CJ)
      { 279, 23 },  // location of level 5 (CJ)
      { 198, 48 },  // location of level 6 (CJ)
      { 140, 25 },  // location of level 7 (CJ)
      { 281, 136 }  // location of level 8 (CJ)
   }
};

//
// Animation locations for episode 0 (1).
// Using patches saves a lot of space,
//  as they replace 320x200 full screen frames.
//
static anim_t epsd0animinfo[] =
{
   { ANIM_ALWAYS, TICRATE/3, 3, { 224, 104 } },
   { ANIM_ALWAYS, TICRATE/3, 3, { 184, 160 } },
   { ANIM_ALWAYS, TICRATE/3, 3, { 112, 136 } },
   { ANIM_ALWAYS, TICRATE/3, 3, { 72, 112 } },
   { ANIM_ALWAYS, TICRATE/3, 3, { 88, 96 } },
   { ANIM_ALWAYS, TICRATE/3, 3, { 64, 48 } },
   { ANIM_ALWAYS, TICRATE/3, 3, { 192, 40 } },
   { ANIM_ALWAYS, TICRATE/3, 3, { 136, 16 } },
   { ANIM_ALWAYS, TICRATE/3, 3, { 80, 16 } },
   { ANIM_ALWAYS, TICRATE/3, 3, { 64, 24 } }
};

static anim_t epsd1animinfo[] =
{
   { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 1 },
   { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 2 },
   { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 3 },
   { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 4 },
   { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 5 },
   { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 6 },
   { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 7 },
   { ANIM_LEVEL,  TICRATE/3, 3, { 192, 144 }, 8 },
   { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 8 }
};

static anim_t epsd2animinfo[] =
{
   { ANIM_ALWAYS, TICRATE/3, 3, { 104, 168 } },
   { ANIM_ALWAYS, TICRATE/3, 3, { 40, 136 } },
   { ANIM_ALWAYS, TICRATE/3, 3, { 160, 96 } },
   { ANIM_ALWAYS, TICRATE/3, 3, { 104, 80 } },
   { ANIM_ALWAYS, TICRATE/3, 3, { 120, 32 } },
   { ANIM_ALWAYS, TICRATE/4, 3, { 40, 0 } }
};

static int NUMANIMS[NUMEPISODES] =
{
   sizeof(epsd0animinfo)/sizeof(anim_t),
   sizeof(epsd1animinfo)/sizeof(anim_t),
   sizeof(epsd2animinfo)/sizeof(anim_t)
};

static anim_t *anims[NUMEPISODES] =
{
   epsd0animinfo,
   epsd1animinfo,
   epsd2animinfo
};

//
// GENERAL DATA
//

//
// Locally used stuff.
//
#define FB 0

// States for single-player
#define SP_KILLS    0
#define SP_ITEMS    2
#define SP_SECRET   4
#define SP_FRAGS    6 
#define SP_TIME     8 
#define SP_PAR      ST_TIME

#define SP_PAUSE    1

// in seconds
#define SHOWNEXTLOCDELAY  4
//#define SHOWLASTLOCDELAY  SHOWNEXTLOCDELAY

// wbs->pnum
static int    me;

// specifies current state
static stateenum_t  state;

// contains information passed into intermission
static wbstartstruct_t* wbs;

static wbplayerstruct_t* plrs;  // wbs->plyr[]

// used for general timing
static int    cnt;  

// signals to refresh everything for one frame
static int    firstrefresh; 

static int    cnt_kills[MAXPLAYERS];
static int    cnt_items[MAXPLAYERS];
static int    cnt_secret[MAXPLAYERS];
static int    cnt_time;
static int    cnt_par;
static int    cnt_pause;

//
//  GRAPHICS
//

// You Are Here graphic
static patch_t*   yah[2]; 

// splat
static patch_t*   splat;

// %, : graphics
static patch_t*   percent;
static patch_t*   colon;

// 0-9 graphic
static patch_t*   num[10];

// minus sign
static patch_t*   wiminus;

// "Finished!" graphics
static patch_t*   finished;

// "Entering" graphic
static patch_t*   entering; 

// "secret"
static patch_t*   sp_secret;

// "Kills", "Scrt", "Items", "Frags"
static patch_t*   kills;
static patch_t*   secret;
static patch_t*   items;
static patch_t*   frags;

// Time sucks.
static patch_t*   time_patch;
static patch_t*   par;
static patch_t*   sucks;

// "killers", "victims"
static patch_t*   killers;
static patch_t*   victims; 

// "Total", your face, your dead face
static patch_t*   total;
static patch_t*   star;
static patch_t*   bstar;

// "red P[1..MAXPLAYERS]"
static patch_t*   p[MAXPLAYERS];

// "gray P[1..MAXPLAYERS]"
static patch_t*   bp[MAXPLAYERS];

// Name graphics of each level (centered)

// haleyjd 06/17/06: cache only the patches needed
static patch_t *wi_lname_this;
static patch_t *wi_lname_next;

// haleyjd: counter based on wi_pause_time
static int cur_pause_time;

// haleyjd: whether decision to fade bg graphics has been made yet
static bool fade_applied = false;

// haleyjd 03/27/05: EDF-defined intermission map names
static const char *mapName;
static const char *nextMapName;

// globals

// haleyjd 02/02/05: intermission pause time -- see EDF
int wi_pause_time = 0;

// haleyjd 02/02/05: color fade
// Allows darkening the background after optional pause expires.
// This was inspired by Contra III and is used by CQIII. See EDF.
int     wi_fade_color = -1;
fixed_t wi_tl_level   =  0;

// forward declaration
static void WI_OverlayBackground();

//
// CODE
//

//
// Helper to know if an episode index is for an overworld map
//
inline static bool overworld(int epid)
{
   return epid >= 0 && epid < NUM_OVERWORLD_EPISODES;
}

// ====================================================================
// WI_drawLF
// Purpose: Draw the "Finished" level name before showing stats
// Args:    none
// Returns: void
//
static void WI_drawLF()
{
   int y = WI_TITLEY;
   patch_t *patch = nullptr;
   
   // haleyjd 07/08/04: fixed to work for any map
   // haleyjd 03/27/05: added string functionality
   // haleyjd 06/17/06: only the needed patch is now cached

   if(wbs->li_lastlevelpic)
      patch = PatchLoader::CacheName(wGlobalDir, wbs->li_lastlevelpic, PU_STATIC);
   else
      patch = wi_lname_this;

   if(patch || mapName)
   {
      // draw <LevelName> 
      if(mapName)
      {
         V_FontWriteText(in_bigfont, mapName, 
            (SCREENWIDTH - V_FontStringWidth(in_bigfont, mapName)) / 2, y,
            &subscreen43);
         y += (5 * V_FontStringHeight(in_bigfont, mapName)) / 4;
      }
      else
      {
         V_DrawPatch((SCREENWIDTH - patch->width)/2,
                     y, &subscreen43, patch);
         y += (5 * patch->height) / 4;
      }
      
      // draw "Finished!"
      V_DrawPatch((SCREENWIDTH - finished->width)/2,
                  y, &subscreen43, finished);
   }

   // haleyjd 06/17/06: set PU_CACHE level here
   if(LevelInfo.levelPic)
      Z_ChangeTag(patch, PU_CACHE);
}


// ====================================================================
// WI_drawEL
// Purpose: Draw introductory "Entering" and level name
// Args:    none
// Returns: void
//
static void WI_drawEL()
{
   int y = WI_TITLEY;
   patch_t *patch = nullptr;
   bool loadedInfoPatch = false;

   // haleyjd 10/24/10: Don't draw "Entering" when in Master Levels mode
   if(inmanageddir == MD_MASTERLEVELS)
      return;

   // haleyjd 06/17/06: support spec. of next map/next secret map pics
   if(wbs->gotosecret)
   {
      if(LevelInfo.nextSecretPic) // watch out; can't merge these if's
      {
         patch = PatchLoader::CacheName(wGlobalDir, LevelInfo.nextSecretPic, PU_STATIC);
         loadedInfoPatch = true;
      }
   }
   else if(LevelInfo.nextLevelPic)
   {
      patch = PatchLoader::CacheName(wGlobalDir, LevelInfo.nextLevelPic, PU_STATIC);
      loadedInfoPatch = true;
   }

   if(!patch && wbs->li_nextlevelpic && *wbs->li_nextlevelpic)
   {
      patch = PatchLoader::CacheName(wGlobalDir, wbs->li_nextlevelpic,
                                     PU_STATIC);
      loadedInfoPatch = true;
   }

   // if no MapInfo patch was loaded, try the default (may also be nullptr)
   if(!patch)
      patch = wi_lname_next;

   if(patch || nextMapName)
   {
      // draw "Entering"
      V_DrawPatch((SCREENWIDTH - entering->width)/2,
                  y, &subscreen43, entering);

      // haleyjd: corrected to use height of entering, not map name
      y += (5 * entering->height)/4;

      // draw level
      if(nextMapName)
      {
         V_FontWriteText(in_bigfont, nextMapName,
            (SCREENWIDTH - V_FontStringWidth(in_bigfont, nextMapName)) / 2,
            y, &subscreen43);
      }
      else
      {
         V_DrawPatch((SCREENWIDTH - patch->width)/2,
                     y, &subscreen43, patch);
      }
   }

   // haleyjd 06/17/06: set any loaded MapInfo patch to PU_CACHE here
   if(loadedInfoPatch)
      Z_ChangeTag(patch, PU_CACHE);
}


// ====================================================================
// WI_drawOnLnode
// Purpose: Draw patches at a location based on episode/map
// Args:    n   -- index to map# within episode
//          c[] -- array of patches to be drawn
//          numpatches -- haleyjd 04/12/03: bug fix - number of patches
// Returns: void
//
// draw stuff at a location by episode/map#
//
static void WI_drawOnLnode(int n, patch_t *c[], int numpatches)
{
   int  i;
   int  left;
   int  top;
   int  right;
   int  bottom;
   bool fits = false;

   if(n < 0 || n >= NUM_LEVELS_PER_EPISODE)
      return;
   
   i = 0;
   do
   {
      left   = lnodes[wbs->epsd][n].x - c[i]->leftoffset;
      top    = lnodes[wbs->epsd][n].y - c[i]->topoffset;
      right  = left + c[i]->width;
      bottom = top  + c[i]->height;
      
      if(left >= 0
         && right < SCREENWIDTH
         && top >= 0
         && bottom < SCREENHEIGHT)
         fits = true;
      else
         i++;
   } 
   while(!fits && i != numpatches); // haleyjd: bug fix

   if(fits && i < numpatches) // haleyjd: bug fix
   {
      V_DrawPatch(lnodes[wbs->epsd][n].x, lnodes[wbs->epsd][n].y,
                  &subscreen43, c[i]);
   }
   else
   {
      // haleyjd: changed printf to C_Printf
      C_Printf(FC_ERROR "Could not place patch on level %d\n", n+1);
   }
}


// ====================================================================
// WI_initAnimatedBack
// Purpose: Initialize pointers and styles for background animation
// Args:    none
// Returns: void
//
static void WI_initAnimatedBack(bool entering)
{
   int   i;
   anim_t* a;

   if(estrnonempty(wbs->li_lastexitpic))
      return;
   if(estrnonempty(wbs->li_nextenterpic) && entering)
      return;
   if(GameModeInfo->id == commercial)  // no animation for DOOM2
      return;

   if(!overworld(wbs->epsd))
      return;

   for(i = 0; i < NUMANIMS[wbs->epsd]; i++)
   {
      a = &anims[wbs->epsd][i];
      
      // init variables
      a->ctr = -1;
  
      // specify the next time to draw it
      switch(a->type)
      {
      case ANIM_ALWAYS:
         a->nexttic = intertime + 1 + (M_Random() % a->period);
         break;
      case ANIM_RANDOM:
         a->nexttic = intertime + 1 + a->data2 + (M_Random() % a->data1);
         break;
      case ANIM_LEVEL:
         a->nexttic = intertime + 1;
         break;
      }
   }
}


// ====================================================================
// WI_updateAnimatedBack
// Purpose: Figure out what animation we do on this iteration
// Args:    none
// Returns: void
//
static void WI_updateAnimatedBack()
{
   int     i;
   anim_t *a;

   if(estrnonempty(wbs->li_lastexitpic))
      return;
   if(estrnonempty(wbs->li_nextenterpic) && state != StatCount)
      return;
   if(GameModeInfo->id == commercial)
      return;

   if(!overworld(wbs->epsd))
      return;

   for(i = 0; i < NUMANIMS[wbs->epsd]; i++)
   {
      a = &anims[wbs->epsd][i];
      
      if(intertime == a->nexttic)
      {
         switch(a->type)
         {
         case ANIM_ALWAYS:
            if(++a->ctr >= a->nanims) 
               a->ctr = 0;
            a->nexttic = intertime + a->period;
            break;

         case ANIM_RANDOM:
            a->ctr++;
            if(a->ctr == a->nanims)
            {
               a->ctr = -1;
               a->nexttic = intertime + a->data2 + (M_Random() % a->data1);
            }
            else 
               a->nexttic = intertime + a->period;
            break;
    
         case ANIM_LEVEL:
            // gawd-awful hack for level anims
            if(!(state == StatCount && i == 7)
               && wbs->next == a->data1)
            {
               a->ctr++;
               if(a->ctr == a->nanims)
                  a->ctr--;
               a->nexttic = intertime + a->period;
            }
            break;
         }
      }
   }
}


// ====================================================================
// WI_drawAnimatedBack
// Purpose: Actually do the animation (whew!)
// Args:    none
// Returns: void
//
static void WI_drawAnimatedBack()
{
   int     i;
   anim_t *a;

   if(estrnonempty(wbs->li_lastexitpic))
      return;
   if(estrnonempty(wbs->li_nextenterpic) && state != StatCount)
      return;
   if(GameModeInfo->id == commercial) //jff 4/25/98 Someone forgot commercial an enum
      return;

   if(!overworld(wbs->epsd))
      return;
   
   for(i = 0; i < NUMANIMS[wbs->epsd]; i++)
   {
      a = &anims[wbs->epsd][i];
      
      if(a->ctr >= 0)
         V_DrawPatch(a->loc.x, a->loc.y, &subscreen43, a->p[a->ctr]);
   }
}


// ====================================================================
// WI_drawNum
// Purpose: Draws a number.  If digits > 0, then use that many digits
//          minimum, otherwise only use as many as necessary
// Args:    x, y   -- location
//          n      -- the number to be drawn
//          digits -- number of digits minimum or zero
// Returns: new x position after drawing (note we are going to the left)
//
static int WI_drawNum(int x, int y, int n, int digits)
{
   int neg, temp, fontwidth = num[0]->width;

   // if non-number, do not draw it
   if(n == INT_MIN)
      return 0;

   neg = n < 0;    // killough 11/98: move up to here, for /= 10 division below
   if(neg)
      n = -n;

   if(digits < 0)
   {
      if(!n)
      {
         // make variable-length zeros 1 digit long
         digits = 1;
      }
      else
      {
         // figure out # of digits in #
         digits = 0;
         temp = n;

         while(temp)
         {
            temp /= 10;
            ++digits;
         }
      }
   }

   // draw the new number
   while(digits--)
   {
      x -= fontwidth;
      V_DrawPatch(x, y, &subscreen43, num[ n % 10 ]);
      n /= 10;
   }

   // draw a minus sign if necessary
   if(neg)
      V_DrawPatch(x -= 8, y, &subscreen43, wiminus);

   return x;
}


// ====================================================================
// WI_drawPercent
// Purpose: Draws a percentage, really just a call to WI_drawNum 
//          after putting a percent sign out there
// Args:    x, y   -- location
//          pct    -- the percentage value to be drawn, no negatives
// Returns: void
//
static void WI_drawPercent(int x, int y, int pct )
{
   if(pct < 0)
      return;
   
   V_DrawPatch(x, y, &subscreen43, percent);
   WI_drawNum(x, y, pct, -1);
}


// ====================================================================
// WI_drawTime
// Purpose: Draws the level completion time or par time, or "Sucks"
//          if 1 hour or more
// Args:    x, y   -- location
//          t      -- the time value to be drawn
// Returns: void
//
static void WI_drawTime(int x, int y, int t)
{  
   int div, n;
   
   if(t < 0)
      return;
   
   if(t <= 61*59)  // otherwise known as 60*60 -1 == 3599
   {
      div = 1;
      
      do
      {
         n = (t / div) % 60;
         x = WI_drawNum(x, y, n, 2) - colon->width;
         div *= 60;
         
         // draw
         if(div == 60 || t / div)
            V_DrawPatch(x, y, &subscreen43, colon);
         
      } while(t / div);
   }
   else
   {
      // "sucks"
      V_DrawPatch(x - sucks->width, y, &subscreen43, sucks); 
   }
}

// ====================================================================
// WI_unloadData
// Purpose: Free up the space allocated during WI_loadData
// Args:    none
// Returns: void
//
static void WI_unloadData()
{
   int i, j;
   
   Z_ChangeTag(wiminus, PU_CACHE);
   
   for(i = 0; i < 10; i++)
      Z_ChangeTag(num[i], PU_CACHE);

   // haleyjd 06/17/06: unload the two level name patches if they exist
   if(wi_lname_this)
      Z_ChangeTag(wi_lname_this, PU_CACHE);
   if(wi_lname_next)
      Z_ChangeTag(wi_lname_next, PU_CACHE);
   
   if(GameModeInfo->id != commercial)
   {
      Z_ChangeTag(yah[0], PU_CACHE);
      Z_ChangeTag(yah[1], PU_CACHE);
      
      Z_ChangeTag(splat, PU_CACHE);
      
      if(overworld(wbs->epsd))
      {
         for(j = 0; j < NUMANIMS[wbs->epsd]; j++)
         {
            if(wbs->epsd != 1 || j != 8)
               for(i = 0; i < anims[wbs->epsd][j].nanims; i++)
                  Z_ChangeTag(anims[wbs->epsd][j].p[i], PU_CACHE);
         }
      }
   }
    
   //  Z_Free(lnames);    // killough 4/26/98: this free is too early!!!
   
   Z_ChangeTag(percent, PU_CACHE);
   Z_ChangeTag(colon, PU_CACHE);
   Z_ChangeTag(finished, PU_CACHE);
   Z_ChangeTag(entering, PU_CACHE);
   Z_ChangeTag(kills, PU_CACHE);
   Z_ChangeTag(secret, PU_CACHE);
   Z_ChangeTag(sp_secret, PU_CACHE);
   Z_ChangeTag(items, PU_CACHE);
   Z_ChangeTag(frags, PU_CACHE);
   Z_ChangeTag(time_patch, PU_CACHE);
   Z_ChangeTag(sucks, PU_CACHE);
   Z_ChangeTag(par, PU_CACHE);

   Z_ChangeTag(victims, PU_CACHE);
   Z_ChangeTag(killers, PU_CACHE);
   Z_ChangeTag(total, PU_CACHE);
  
   for(i = 0; i < MAXPLAYERS; i++)
      Z_ChangeTag(p[i], PU_CACHE);

   for(i = 0; i < MAXPLAYERS; i++)
      Z_ChangeTag(bp[i], PU_CACHE);
}


// ====================================================================
// WI_End
// Purpose: Unloads data structures (inverse of WI_Start)
// Args:    none
// Returns: void
//
static void WI_End()
{
   WI_unloadData();
}


// ====================================================================
// WI_initNoState
// Purpose: Clear state, ready for end of level activity
// Args:    none
// Returns: void
//
static void WI_initNoState()
{
   state = NoState;
   acceleratestage = 0;
   cnt = 10;
}


// ====================================================================
// WI_updateNoState
// Purpose: Cycle until end of level activity is done
// Args:    none
// Returns: void
//
static void WI_updateNoState() 
{
   WI_updateAnimatedBack();
   
   if(!--cnt)
   {
      WI_End();
      G_WorldDone();

      // haleyjd 03/16/06: bug found by fraggle: WI_End sets the 
      // intermission resources to PU_CACHE. Most of the time this is 
      // fine, but if any other memory operations occur before the time 
      // G_DoWorldDone runs, the game may crash.

      // Fix: made a new state that draws nothing.
      state = IntrEnding;
   }
}

static bool snl_pointeron = false;


// ====================================================================
// WI_initShowNextLoc
// Purpose: Prepare to show the next level's location 
// Args:    none
// Returns: void
//
static void WI_initShowNextLoc()
{
   state = ShowNextLoc;
   acceleratestage = 0;
   cnt = SHOWNEXTLOCDELAY * TICRATE;
   
   WI_initAnimatedBack(true);
}


// ====================================================================
// WI_updateShowNextLoc
// Purpose: Prepare to show the next level's location
// Args:    none
// Returns: void
//
static void WI_updateShowNextLoc()
{
   WI_updateAnimatedBack();
   
   if(!--cnt || acceleratestage)
      WI_initNoState();
   else
      snl_pointeron = (cnt & 31) < 20;
}


// ====================================================================
// WI_drawShowNextLoc
// Purpose: Show the next level's location on animated backgrounds
// Args:    none
// Returns: void
//
static void WI_drawShowNextLoc()
{
   int   i, last;

   IN_slamBackground();
   
   // draw animated background
   WI_drawAnimatedBack();

   if(estrnonempty(wbs->li_lastexitpic) ||
      (estrnonempty(wbs->li_nextenterpic) && state != StatCount))
   {
      WI_drawEL();
      return;
   }

   if(GameModeInfo->id != commercial)
   {
      if(!overworld(wbs->epsd))
      {
         WI_drawEL();  // "Entering..." if not E1 or E2 or E3
         return;
      }
  
      last = (wbs->last == 8) ? wbs->next - 1 : wbs->last;
      
      // draw a splat on taken cities.
      for(i = 0; i <= last; i++)
         WI_drawOnLnode(i, &splat, 1); // haleyjd 04/12/03: bug fix

      // splat the secret level?
      if(wbs->didsecret)
         WI_drawOnLnode(8, &splat, 1);

      // draw flashing ptr
      if(snl_pointeron)
         WI_drawOnLnode(wbs->next, yah, 2); 
   }

   // draws which level you are entering..
   // check for end of game -- haleyjd 07/08/04: use map info
   if((GameModeInfo->id != commercial) || !LevelInfo.endOfGame)
      WI_drawEL();  
}

// ====================================================================
// WI_drawNoState
// Purpose: Draw the pointer and next location
// Args:    none
// Returns: void
//
static void WI_drawNoState()
{
   snl_pointeron = true;
   WI_drawShowNextLoc();
}

// ====================================================================
// WI_fragSum
// Purpose: Calculate frags for this player based on the current totals
//          of all the other players.  Subtract self-frags.
// Args:    playernum -- the player to be calculated
// Returns: the total frags for this player
//
static int WI_fragSum(int playernum)
{
   int i, numfrags = 0;
    
   for(i = 0; i < MAXPLAYERS; i++)
   {
      if(playeringame[i]  // is this player playing?
         && i != playernum) // and it's not the player we're calculating
      {
         numfrags += plrs[playernum].frags[i];  
      }
   }
      
   // JDC hack - negative frags.
   numfrags -= plrs[playernum].frags[playernum];
   // UNUSED if (frags < 0)
   //  frags = 0;
   
   return numfrags;
}

static int dm_state;
static int dm_frags[MAXPLAYERS][MAXPLAYERS];  // frags matrix
static int dm_totals[MAXPLAYERS];  // totals by player

// ====================================================================
// WI_initDeathmatchStats
// Purpose: Set up to display DM stats at end of level.  Calculate 
//          frags for all players.
// Args:    none
// Returns: void
//
static void WI_initDeathmatchStats()
{
   int i, j; // looping variables
   
   state = StatCount;  // We're doing stats
   acceleratestage = 0;
   dm_state = 1;  // count how many times we've done a complete stat

   cnt_pause = TICRATE;
   
   for(i = 0; i < MAXPLAYERS; i++)
   {
      if(playeringame[i])
      { 
         for(j = 0; j < MAXPLAYERS; j++)
         {
            if(playeringame[j])
               dm_frags[i][j] = 0;  // set all counts to zero
         }
         
         dm_totals[i] = 0;
      }
   }
   WI_initAnimatedBack(false);
}


// ====================================================================
// WI_updateDeathmatchStats
// Purpose: Update numbers for deathmatch intermission. Lots of noise 
//          and drama around the presentation.
// Args:    none
// Returns: void
//
static void WI_updateDeathmatchStats()
{
   int  i, j;    
   bool stillticking;
   
   WI_updateAnimatedBack();

   if(cur_pause_time > 0)
   {
      if(acceleratestage)
      {
         cur_pause_time = 0;
         acceleratestage = 0;
      }
      else
      {
         --cur_pause_time;
         return;
      }
   }
   
   if(acceleratestage && dm_state != 4)  // still ticking
   {
      acceleratestage = 0;
      
      for(i = 0; i < MAXPLAYERS; i++)
      {
         if(playeringame[i])
         {
            for(j = 0; j < MAXPLAYERS; j++)
            {
               if(playeringame[j])
                  dm_frags[i][j] = plrs[i].frags[j];
            }
            
            dm_totals[i] = WI_fragSum(i);
         }
      }
  
      S_StartInterfaceSound(sfx_barexp);  // bang
      dm_state = 4;  // we're done with all 4 (or all we have to do)
   }
    
   if(dm_state == 2)
   {
      if(!(intertime&3))
         S_StartInterfaceSound(sfx_pistol);  // noise while counting
  
      stillticking = false;
      
      for(i = 0; i < MAXPLAYERS; i++)
      {
         if(playeringame[i])
         {
            for(j = 0; j < MAXPLAYERS; j++)
            {
               if(playeringame[j] && dm_frags[i][j] != plrs[i].frags[j])
               {
                  if(plrs[i].frags[j] < 0)
                     dm_frags[i][j]--;
                  else
                     dm_frags[i][j]++;

                  if(dm_frags[i][j] > 999) // Ty 03/17/98 3-digit frag count
                     dm_frags[i][j] = 999;
                  
                  if(dm_frags[i][j] < -999)
                     dm_frags[i][j] = -999;
                  
                  stillticking = true;
               }
            }
            dm_totals[i] = WI_fragSum(i);
            
            if(dm_totals[i] > 999)
               dm_totals[i] = 999;
            
            if(dm_totals[i] < -999)
               dm_totals[i] = -999;  // Ty 03/17/98 end 3-digit frag count
         }
      }

      if(!stillticking)
      {
         S_StartInterfaceSound(sfx_barexp);
         dm_state++;
      }
   }
   else if(dm_state == 4)
   {
      if(acceleratestage)
      {   
         S_StartInterfaceSound(sfx_slop);

         if(GameModeInfo->id == commercial && estrempty(wbs->li_nextenterpic))
         {
            WI_initNoState();
         }
         else
            WI_initShowNextLoc();
      }
   }
   else if(dm_state & 1)
   {
      if(!--cnt_pause)
      {
         dm_state++;
         cnt_pause = TICRATE;
      }
   }
}


// ====================================================================
// WI_drawDeathmatchStats
// Purpose: Draw the stats on the screen in a matrix
// Args:    none
// Returns: void
//
static void WI_drawDeathmatchStats()
{
   int   i, j, x, y, w;  

   if(!(fade_applied || cur_pause_time))
   {
      if(wi_fade_color != -1)
         WI_OverlayBackground();
      fade_applied = true;
   }
   
   IN_slamBackground();
  
   // draw animated background
   WI_drawAnimatedBack();

   if(cur_pause_time > 0)
      return;

   WI_drawLF();

   // draw stat titles (top line)
   V_DrawPatch(DM_TOTALSX-total->width/2,
               DM_MATRIXY-WI_SPACINGY+10,
               &subscreen43,
               total);
  
   V_DrawPatch(DM_KILLERSX, DM_KILLERSY, &subscreen43, killers);
   V_DrawPatch(DM_VICTIMSX, DM_VICTIMSY, &subscreen43, victims);

   // draw P?
   x = DM_MATRIXX + DM_SPACINGX;
   y = DM_MATRIXY;

   for(i = 0; i < MAXPLAYERS; i++)
   {
      if (playeringame[i])
      {
         V_DrawPatch(x-p[i]->width/2,
                     DM_MATRIXY - WI_SPACINGY,
                     &subscreen43,
                     p[i]);
      
         V_DrawPatch(DM_MATRIXX-p[i]->width/2,
                     y,
                     &subscreen43,
                     p[i]);

         if(i == me)
         {
            V_DrawPatch(x-p[i]->width/2,
                        DM_MATRIXY - WI_SPACINGY,
                        &subscreen43,
                        bstar);

            V_DrawPatch(DM_MATRIXX-p[i]->width/2,
                        y,
                        &subscreen43,
                        star);
         }
      }
      x += DM_SPACINGX;
      y += WI_SPACINGY;
   }

   // draw stats
   y = DM_MATRIXY + 10;
   w = num[0]->width;

   for(i = 0; i < MAXPLAYERS; i++)
   {
      x = DM_MATRIXX + DM_SPACINGX;
      
      if(playeringame[i])
      {
         for(j = 0; j < MAXPLAYERS; j++)
         {
            if(playeringame[j])
               WI_drawNum(x+w, y, dm_frags[i][j], 2);
            
            x += DM_SPACINGX;
         }
         WI_drawNum(DM_TOTALSX+w, y, dm_totals[i], 2);
      }
      y += WI_SPACINGY;
   }
}


//
// Note: The term "Netgame" means a coop game
// (Or sometimes a DM game too, depending on context -- killough)
// (haleyjd: not any more since I cleaned that up using GameType)
//
static int cnt_frags[MAXPLAYERS];
static int dofrags;
static int ng_state;


// ====================================================================
// WI_initNetgameStats
// Purpose: Prepare for coop game stats
// Args:    none
// Returns: void
//
static void WI_initNetgameStats()
{
   int i;
   
   state = StatCount;
   acceleratestage = 0;
   ng_state = 1;

   cnt_pause = TICRATE;
   
   for(i = 0; i < MAXPLAYERS; i++)
   {
      if(!playeringame[i])
         continue;
      
      cnt_kills[i] = cnt_items[i] = cnt_secret[i] = cnt_frags[i] = 0;
      
      dofrags += WI_fragSum(i);
   }
   
   dofrags = !!dofrags; // set to true or false - did we have frags?
   
   WI_initAnimatedBack(false);
}


// ====================================================================
// WI_updateNetgameStats
// Purpose: Calculate coop stats as we display them with noise and fury
// Args:    none
// Returns: void
// Comment: This stuff sure is complicated for what it does
//
static void WI_updateNetgameStats()
{
   int i, fsum;    
   bool stillticking;
   
   WI_updateAnimatedBack();

   if(cur_pause_time > 0)
   {
      if(acceleratestage)
      {
         cur_pause_time = 0;
         acceleratestage = 0;
      }
      else
      {
         --cur_pause_time;
         return;
      }
   }
   
   if(acceleratestage && ng_state != 10)
   {
      acceleratestage = 0;
      
      for(i = 0; i < MAXPLAYERS; i++)
      {
         if(!playeringame[i])
            continue;
         
         cnt_kills[i] = (plrs[i].skills * 100) / wbs->maxkills;
         cnt_items[i] = (plrs[i].sitems * 100) / wbs->maxitems;
         
         // killough 2/22/98: Make secrets = 100% if maxsecret = 0:
         cnt_secret[i] = wbs->maxsecret ? 
            (plrs[i].ssecret * 100) / wbs->maxsecret : 100;
         if(dofrags)
            cnt_frags[i] = WI_fragSum(i);  // we had frags
      }
      S_StartInterfaceSound(sfx_barexp);  // bang
      ng_state = 10;
   }
   
   if(ng_state == 2)
   {
      if(!(intertime & 3))
         S_StartInterfaceSound(sfx_pistol);  // pop
      
      stillticking = false;
      
      for(i = 0; i < MAXPLAYERS; i++)
      {
         if(!playeringame[i])
            continue;
         
         cnt_kills[i] += 2;
         
         if(cnt_kills[i] >= (plrs[i].skills * 100) / wbs->maxkills)
            cnt_kills[i] = (plrs[i].skills * 100) / wbs->maxkills;
         else
            stillticking = true; // still got stuff to tally
      }
      
      if(!stillticking)
      {
         S_StartInterfaceSound(sfx_barexp); 
         ng_state++;
      }
   }
   else if(ng_state == 4)
   {
      if(!(intertime & 3))
         S_StartInterfaceSound(sfx_pistol);
      
      stillticking = false;
      
      for(i = 0; i < MAXPLAYERS; i++)
      {
         if(!playeringame[i])
            continue;
         
         cnt_items[i] += 2;
         if(cnt_items[i] >= (plrs[i].sitems * 100) / wbs->maxitems)
            cnt_items[i] = (plrs[i].sitems * 100) / wbs->maxitems;
         else
            stillticking = true;
      }
      
      if(!stillticking)
      {
         S_StartInterfaceSound(sfx_barexp);
         ng_state++;
      }
   }
   else if(ng_state == 6)
   {
      if(!(intertime & 3))
         S_StartInterfaceSound(sfx_pistol);
      
      stillticking = false;
      
      for(i = 0; i < MAXPLAYERS; i++)
      {
         if(!playeringame[i])
            continue;
         
         cnt_secret[i] += 2;
         
         // killough 2/22/98: Make secrets = 100% if maxsecret = 0:
         
         if(cnt_secret[i] >= (wbs->maxsecret ? (plrs[i].ssecret * 100) / wbs->maxsecret : 100))
            cnt_secret[i] = wbs->maxsecret ? (plrs[i].ssecret * 100) / wbs->maxsecret : 100;
         else
            stillticking = true;
      }
      
      if(!stillticking)
      {
         S_StartInterfaceSound(sfx_barexp);
         ng_state += 1 + 2*!dofrags;
      }
   }
   else if(ng_state == 8)
   {
      if(!(intertime & 3))
         S_StartInterfaceSound(sfx_pistol);
      
      stillticking = false;
      
      for(i = 0; i < MAXPLAYERS; i++)
      {
         if(!playeringame[i])
            continue;
         
         cnt_frags[i] += 1;
         
         if(cnt_frags[i] >= (fsum = WI_fragSum(i)))
            cnt_frags[i] = fsum;
         else
            stillticking = true;
      }
      
      if(!stillticking)
      {
         S_StartInterfaceSound(sfx_pldeth);
         ng_state++;
      }
   }
   else if(ng_state == 10)
   {
      if(acceleratestage)
      {
         S_StartInterfaceSound(sfx_sgcock);
         if(GameModeInfo->id == commercial && estrempty(wbs->li_nextenterpic))
            WI_initNoState();
         else
            WI_initShowNextLoc();
      }
   }
   else if(ng_state & 1)
   {
      if(!--cnt_pause)
      {
         ng_state++;
         cnt_pause = TICRATE;
      }
   }
}


// ====================================================================
// WI_drawNetgameStats
// Purpose: Put the coop stats on the screen
// Args:    none
// Returns: void
//
static void WI_drawNetgameStats()
{
   int i, x, y, pwidth = percent->width;

   if(!(fade_applied || cur_pause_time))
   {
      if(wi_fade_color != -1)
         WI_OverlayBackground();
      fade_applied = true;
   }

   IN_slamBackground();
   
   // draw animated background
   WI_drawAnimatedBack(); 

   if(cur_pause_time > 0)
      return;
   
   WI_drawLF();

   // draw stat titles (top line)
   V_DrawPatch(NG_STATSX+NG_SPACINGX-kills->width,
               NG_STATSY, &subscreen43, kills);

   V_DrawPatch(NG_STATSX+2*NG_SPACINGX-items->width,
               NG_STATSY, &subscreen43, items);

   V_DrawPatch(NG_STATSX+3*NG_SPACINGX-secret->width,
               NG_STATSY, &subscreen43, secret);
  
   if(dofrags)
      V_DrawPatch(NG_STATSX+4*NG_SPACINGX-frags->width,
                  NG_STATSY, &subscreen43, frags);

   // draw stats
   y = NG_STATSY + kills->height;

   for(i = 0; i < MAXPLAYERS; i++)
   {
      if(!playeringame[i])
         continue;
      
      x = NG_STATSX;
      V_DrawPatch(x-p[i]->width, y, &subscreen43, p[i]);
      
      if(i == me)
         V_DrawPatch(x-p[i]->width, y, &subscreen43, star);
      
      x += NG_SPACINGX;
      WI_drawPercent(x-pwidth, y+10, cnt_kills[i]); x += NG_SPACINGX;
      WI_drawPercent(x-pwidth, y+10, cnt_items[i]); x += NG_SPACINGX;
      WI_drawPercent(x-pwidth, y+10, cnt_secret[i]);  x += NG_SPACINGX;

      if(dofrags)
         WI_drawNum(x, y+10, cnt_frags[i], -1);
      
      y += WI_SPACINGY;
   }
}

static int sp_state;

// ====================================================================
// WI_initStats
// Purpose: Get ready for single player stats
// Args:    none
// Returns: void
// Comment: Seems like we could do all these stats in a more generic
//          set of routines that weren't duplicated for dm, coop, sp
//
static void WI_initStats()
{
   state = StatCount;
   acceleratestage = 0;
   sp_state = 1;
   cnt_kills[0] = cnt_items[0] = cnt_secret[0] = -1;
   cnt_time = cnt_par = -1;
   cnt_pause = TICRATE;

   WI_initAnimatedBack(false);
}

// ====================================================================
// WI_updateStats
// Purpose: Calculate solo stats
// Args:    none
// Returns: void
//
static void WI_updateStats()
{
   WI_updateAnimatedBack();

   // haleyjd 02/02/05: allow an initial intermission pause
   if(cur_pause_time > 0)
   {
      if(acceleratestage)
      {
         cur_pause_time = 0;
         acceleratestage = 0;
      }
      else
      {
         --cur_pause_time;
         return;
      }
   }
   
   if(acceleratestage && sp_state != 10)
   {
      acceleratestage = 0;
      cnt_kills[0] = (plrs[me].skills * 100) / wbs->maxkills;
      cnt_items[0] = (plrs[me].sitems * 100) / wbs->maxitems;
      
      // killough 2/22/98: Make secrets = 100% if maxsecret = 0:
      cnt_secret[0] = (wbs->maxsecret ? 
                       (plrs[me].ssecret * 100) / wbs->maxsecret : 100);

      cnt_time = plrs[me].stime / TICRATE;
      cnt_par = wbs->partime==-1 ? 0 : wbs->partime/TICRATE;
      S_StartInterfaceSound(sfx_barexp);
      sp_state = 10;
   }

   if(sp_state == 2)
   {
      cnt_kills[0] += 2;
      
      if(!(intertime & 3))
         S_StartInterfaceSound(sfx_pistol);

      if(cnt_kills[0] >= (plrs[me].skills * 100) / wbs->maxkills)
      {
         cnt_kills[0] = (plrs[me].skills * 100) / wbs->maxkills;
         S_StartInterfaceSound(sfx_barexp);
         sp_state++;
      }
   }
   else if(sp_state == 4)
   {
      cnt_items[0] += 2;
      
      if(!(intertime & 3))
         S_StartInterfaceSound(sfx_pistol);
      
      if(cnt_items[0] >= (plrs[me].sitems * 100) / wbs->maxitems)
      {
         cnt_items[0] = (plrs[me].sitems * 100) / wbs->maxitems;
         S_StartInterfaceSound(sfx_barexp);
         sp_state++;
      }
   }
   else if(sp_state == 6)
   {
      cnt_secret[0] += 2;
      
      if(!(intertime & 3))
         S_StartInterfaceSound(sfx_pistol);

      // killough 2/22/98: Make secrets = 100% if maxsecret = 0:
      if(cnt_secret[0] >= (wbs->maxsecret ? 
         (plrs[me].ssecret * 100) / wbs->maxsecret : 100))
      {
         cnt_secret[0] = (wbs->maxsecret ? 
            (plrs[me].ssecret * 100) / wbs->maxsecret : 100);
         S_StartInterfaceSound(sfx_barexp);
         sp_state++;
      }
   }
   else if(sp_state == 8)
   {
      if(!(intertime & 3))
         S_StartInterfaceSound(sfx_pistol);
      
      cnt_time += 3;
      
      if(cnt_time >= plrs[me].stime / TICRATE)
         cnt_time = plrs[me].stime / TICRATE;
      
      cnt_par += 3;
      
      if(cnt_par >= wbs->partime / TICRATE)
      {
         cnt_par = wbs->partime / TICRATE;
         
         if (cnt_time >= plrs[me].stime / TICRATE)
         {
            S_StartInterfaceSound(sfx_barexp);
            sp_state++;
         }
      }
      if(wbs->partime == -1) 
         cnt_par = 0;
   }
   else if(sp_state == 10)
   {
      if(acceleratestage)
      {
         S_StartInterfaceSound(sfx_sgcock);
         
         if(GameModeInfo->id == commercial && estrempty(wbs->li_nextenterpic))
            WI_initNoState();
         else
            WI_initShowNextLoc();
      }
   }
   else if(sp_state & 1)
   {
      if(!--cnt_pause)
      {
         sp_state++;
         cnt_pause = TICRATE;
      }
   }
}

// ====================================================================
// WI_drawStats
// Purpose: Put the solo stats on the screen
// Args:    none
// Returns: void
//
static void WI_drawStats()
{
   // line height
   int lh; 
   
   lh = (3*num[0]->height)/2;

   if(!(fade_applied || cur_pause_time))
   {
      if(wi_fade_color != -1)
         WI_OverlayBackground();
      fade_applied = true;
   }

   IN_slamBackground();
   
   // draw animated background
   WI_drawAnimatedBack();

   // haleyjd 02/02/05: intermission pause
   if(cur_pause_time > 0)
      return;
   
   WI_drawLF();

   V_DrawPatch(SP_STATSX, SP_STATSY, &subscreen43, kills);
   WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY, cnt_kills[0]);
   
   V_DrawPatch(SP_STATSX, SP_STATSY+lh, &subscreen43, items);
   WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY+lh, cnt_items[0]);
   
   V_DrawPatch(SP_STATSX, SP_STATSY+2*lh, &subscreen43, sp_secret);
   WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY+2*lh, cnt_secret[0]);
   
   V_DrawPatch(SP_TIMEX, SP_TIMEY, &subscreen43, time_patch);
   WI_drawTime(SCREENWIDTH/2 - SP_TIMEX, SP_TIMEY, cnt_time);

   // Ty 04/11/98: redid logic: should skip only if with pwad but 
   // without deh patch
   // killough 2/22/98: skip drawing par times on pwads
   // Ty 03/17/98: unless pars changed with deh patch
   // sf: cleverer: only skips on _new_ non-iwad levels
   //   new logic in g_game.c

   if(wbs->partime != -1)
   {
      if(overworld(wbs->epsd))
      {
         V_DrawPatch(SCREENWIDTH/2 + SP_TIMEX, SP_TIMEY, &subscreen43, par);
         WI_drawTime(SCREENWIDTH - SP_TIMEX, SP_TIMEY, cnt_par);
      }
   }
}


// ====================================================================
// WI_Ticker
// Purpose: Do various updates every gametic, for stats, animation,
//          checking that intermission music is running, etc.
// Args:    none
// Returns: void
//
static void WI_Ticker()
{
   switch(state)
   {
   case StatCount:
      if(GameType == gt_dm) 
         WI_updateDeathmatchStats();
      else if(GameType == gt_coop) 
         WI_updateNetgameStats();
      else 
         WI_updateStats();
      break;
      
   case ShowNextLoc:
      WI_updateShowNextLoc();
      break;
      
   case NoState:
      WI_updateNoState();
      break;

   case IntrEnding: // haleyjd 03/16/06
      break;
   }
}

extern void V_ColorBlockTL(VBuffer *, byte, int, int, int, int, int);

//
// WI_OverlayBackground
//
// haleyjd 02/02/05: function to allow the background to be overlaid
// with a translucent color. Assumes WI_DrawBackground already called.
//
static void WI_OverlayBackground()
{
   V_ColorBlockTL(&subscreen43, (byte)wi_fade_color, 
                  0, 0, backscreen1.width, backscreen1.height, 
                  wi_tl_level);
}

// killough 11/98:
// Moved to separate function so that i_video.c could call it
// haleyjd: i_video now calls IN_DrawBackground, which calls the
//          appropriate gamemode's bg drawer.

static void WI_DrawBackground()
{
   char  name[9];  // limited to 8 characters

   if(state != StatCount && estrnonempty(wbs->li_nextenterpic))
   {
      strncpy(name, wbs->li_nextenterpic, sizeof(name));
      name[sizeof(name) - 1] = 0;
   }
   else if(estrnonempty(wbs->li_lastexitpic) ||
      GameModeInfo->id == commercial || (GameModeInfo->id == retail &&
                                         !overworld(wbs->epsd)))
   {
      // Use LevelInfo's interpic here: it handles cases where li_lastexitpic IS
      // empty. By the help of logic, the intermapinfo_t last exitpic must be
      // equivalent to currently exited level's interPic.
      strncpy(name, LevelInfo.interPic, sizeof(name));
      name[sizeof(name) - 1] = 0;
   }
   else
      snprintf(name, sizeof(name), "WIMAP%d", wbs->epsd);

   // background
   V_DrawFSBackground(&subscreen43, wGlobalDir.checkNumForName(name));

   // re-fade if we were called due to video mode reset
   if(fade_applied && wi_fade_color != -1)
      WI_OverlayBackground();
}

// ====================================================================
// WI_loadData
// Purpose: Initialize intermission data such as background graphics,
//          patches, map names, etc.
// Args:    none
// Returns: void
//
static void WI_loadData()
{
   int   i, j;
   char name[9];

   if(GameModeInfo->id == commercial)
   {
      // haleyjd 06/17/06: only load the patches that are needed, and also
      // allow them to be loaded for ANY valid MAPxy map, not just 1 - 32
      if(isMAPxy(gamemapname))
      {
         int lumpnum;

         psnprintf(name, sizeof(name), "CWILV%2.2d", wbs->last);

         if((lumpnum = g_dir->checkNumForName(name)) != -1)
            wi_lname_this = PatchLoader::CacheNum(*g_dir, lumpnum, PU_STATIC);
         else
            wi_lname_this = nullptr;

         psnprintf(name, sizeof(name), "CWILV%2.2d", wbs->next);

         if((lumpnum = g_dir->checkNumForName(name)) != -1)
            wi_lname_next = PatchLoader::CacheNum(*g_dir, lumpnum, PU_STATIC);
         else
            wi_lname_next = nullptr;
      }
   }
   else
   {
      // haleyjd 06/17/06: as above, but for ExMy maps
      if(isExMy(gamemapname))
      {
         int lumpnum;

         psnprintf(name, sizeof(name), "WILV%d%d", wbs->epsd, wbs->last);

         if((lumpnum = g_dir->checkNumForName(name)) != -1)
            wi_lname_this = PatchLoader::CacheNum(*g_dir, lumpnum, PU_STATIC);
         else
            wi_lname_this = nullptr;

         psnprintf(name, sizeof(name), "WILV%d%d", wbs->epsd, wbs->next);

         if((lumpnum = g_dir->checkNumForName(name)) != -1)
            wi_lname_next = PatchLoader::CacheNum(*g_dir, lumpnum, PU_STATIC);
         else
            wi_lname_next = nullptr;
      }
      
      // you are here
      yah[0] = PatchLoader::CacheName(wGlobalDir, "WIURH0", PU_STATIC);
      
      // you are here (alt.)
      yah[1] = PatchLoader::CacheName(wGlobalDir, "WIURH1", PU_STATIC);

      // splat
      splat = PatchLoader::CacheName(wGlobalDir, "WISPLAT", PU_STATIC);
      
      if(overworld(wbs->epsd))
      {
         for(j = 0; j < NUMANIMS[wbs->epsd]; j++)
         {
            anim_t *a = &anims[wbs->epsd][j];
            for(i = 0; i < a->nanims; i++)
            {
               // MONDO HACK!
               if(wbs->epsd != 1 || j != 8) 
               {
                  // animations
                  snprintf(name, sizeof(name), "WIA%d%.2d%.2d", wbs->epsd, j, i);
                  a->p[i] = PatchLoader::CacheName(wGlobalDir, name, PU_STATIC);
               }
               else
               {
                  // HACK ALERT!
                  a->p[i] = anims[1][4].p[i]; 
               }
            } // end for
         } // end for
      } // end if
   } // end else (!commercial)

   // More hacks on minus sign.
   wiminus = PatchLoader::CacheName(wGlobalDir, "WIMINUS", PU_STATIC); 

   for(i = 0; i < 10; i++)
   {
      // numbers 0-9
      snprintf(name, sizeof(name), "WINUM%d", i);
      num[i] = PatchLoader::CacheName(wGlobalDir, name, PU_STATIC);
   }

   // percent sign
   percent = PatchLoader::CacheName(wGlobalDir, "WIPCNT", PU_STATIC);
   
   // "finished"
   finished = PatchLoader::CacheName(wGlobalDir, "WIF", PU_STATIC);
   
   // "entering"
   entering = PatchLoader::CacheName(wGlobalDir, "WIENTER", PU_STATIC);
   
   // "kills"
   kills = PatchLoader::CacheName(wGlobalDir, "WIOSTK", PU_STATIC);   

   // "scrt"
   secret = PatchLoader::CacheName(wGlobalDir, "WIOSTS", PU_STATIC);
   
   // "secret"
   sp_secret = PatchLoader::CacheName(wGlobalDir, "WISCRT2", PU_STATIC);

   // Yuck. // Ty 03/27/98 - got that right :)  
   // french is an enum=1 always true.
   // haleyjd: removed old crap

   items = PatchLoader::CacheName(wGlobalDir, "WIOSTI", PU_STATIC);
   
   // "frgs"
   frags = PatchLoader::CacheName(wGlobalDir, "WIFRGS", PU_STATIC);    
   
   // ":"
   colon = PatchLoader::CacheName(wGlobalDir, "WICOLON", PU_STATIC); 
   
   // "time"
   time_patch = PatchLoader::CacheName(wGlobalDir, "WITIME", PU_STATIC);   

   // "sucks"
   sucks = PatchLoader::CacheName(wGlobalDir, "WISUCKS", PU_STATIC);  
   
   // "par"
   par = PatchLoader::CacheName(wGlobalDir, "WIPAR", PU_STATIC);   
   
   // "killers" (vertical)
   killers = PatchLoader::CacheName(wGlobalDir, "WIKILRS", PU_STATIC);
  
   // "victims" (horiz)
   victims = PatchLoader::CacheName(wGlobalDir, "WIVCTMS", PU_STATIC);
   
   // "total"
   total = PatchLoader::CacheName(wGlobalDir, "WIMSTT", PU_STATIC);   
   
   // your face
   star = PatchLoader::CacheName(wGlobalDir, "STFST01", PU_STATIC);
   
   // dead face
   bstar = PatchLoader::CacheName(wGlobalDir, "STFDEAD0", PU_STATIC);    

   for(i = 0; i < MAXPLAYERS; i++)
   {
      // "1,2,3,4"
      snprintf(name, sizeof(name), "STPB%d", i);
      p[i] = PatchLoader::CacheName(wGlobalDir, name, PU_STATIC);
      
      // "1,2,3,4"
      snprintf(name, sizeof(name), "WIBP%d", i+1);
      bp[i] = PatchLoader::CacheName(wGlobalDir, name, PU_STATIC);
   }
}

// ====================================================================
// WI_Drawer
// Purpose: Call the appropriate stats drawing routine depending on
//          what kind of game is being played (DM, coop, solo)
// Args:    none
// Returns: void
//
static void WI_Drawer()
{
   switch(state)
   {
   case StatCount:
      if(GameType == gt_dm)
         WI_drawDeathmatchStats();
      else if(GameType == gt_coop)
         WI_drawNetgameStats();
      else
         WI_drawStats();
      break;
      
   case ShowNextLoc:
      WI_drawShowNextLoc();
      break;
      
   case NoState:
      WI_drawNoState();
      break;

   case IntrEnding: // haleyjd 03/16/06
      break;
   }
}


// ====================================================================
// WI_initVariables
// Purpose: Initialize the intermission information structure
//          Note: wbstartstruct_t is defined in d_player.h
// Args:    wbstartstruct -- pointer to the structure with the data
// Returns: void
//
static void WI_initVariables(wbstartstruct_t *wbstartstruct)
{
   wbs = wbstartstruct;

   // haleyjd 02/02/05: pause and fade features
   cur_pause_time = wi_pause_time;
   fade_applied   = false;

   acceleratestage = 0;
   cnt = intertime = 0;
   firstrefresh = 1;
   me = wbs->pnum;
   plrs = wbs->plyr;

   if(!wbs->maxkills)
      wbs->maxkills = 1;  // probably only useful in MAP30
   
   if(!wbs->maxitems)
      wbs->maxitems = 1;

   // killough 2/22/98: Keep maxsecret=0 if it's zero, so
   // we can detect 0/0 as as a special case and print 100%.
   //
   //    if (!wbs->maxsecret)
   //  wbs->maxsecret = 1;

   if(GameModeInfo->id != retail)
   {
      // IOANCH: What was this?!
//      if(wbs->epsd > 2)
//         wbs->epsd -= 3;
   }

   // haleyjd 03/27/05: EDF-defined intermission map names
   mapName     = nullptr;
   nextMapName = nullptr;

   // NOTE: in UMAPINFO, level-pic has priority
   if((!wbs->li_lastlevelpic || !*wbs->li_lastlevelpic) &&
      wbs->li_lastlevelname && *wbs->li_lastlevelname)
   {
      mapName = wbs->li_lastlevelname;
   }
   if((!wbs->li_nextlevelpic || !*wbs->li_nextlevelpic) &&
      wbs->li_nextlevelname && *wbs->li_nextlevelname)
   {
      nextMapName = wbs->li_nextlevelname;
   }

   if(LevelInfo.useEDFInterName || inmanageddir)
   {
      char nameBuffer[24];
      const char *basename;

      // set current map
      if(!mapName)
      {
         if(inmanageddir == MD_MASTERLEVELS)
         {
            // haleyjd 08/31/12: In Master Levels mode, synthesize one here.
            qstring buffer;
            qstring lvname;

            lvname << FC_ABSCENTER << FC_GOLD << LevelInfo.levelName;

            V_FontFitTextToRect(in_bigfont, lvname, 0, 0, 320, 200);

            buffer << "{EE_MLEV_" << lvname << "}";
            mapName = E_CreateString(lvname.constPtr(), buffer.constPtr(), -1)->string;
         }
         else
         {
            edf_string_t *str;
            psnprintf(nameBuffer, sizeof(nameBuffer), "_IN_NAME_%s", gamemapname);
            if((str = E_StringForName(nameBuffer)))
               mapName = str->string;
         }
      }

      // are we going to a secret level?
      if(!nextMapName)
      {
         basename = wbs->gotosecret ? LevelInfo.nextSecret : LevelInfo.nextLevel;

         // set next map
         if(*basename)
         {
            edf_string_t *str;
            psnprintf(nameBuffer, 24, "_IN_NAME_%s", basename);

            if((str = E_StringForName(nameBuffer)))
               nextMapName = str->string;
         }
         else
         {
            // try ExMy and MAPxy defaults for normally-named maps
            if(isExMy(gamemapname))
            {
               edf_string_t *str;
               psnprintf(nameBuffer, 24, "_IN_NAME_E%01dM%01d",
                         wbs->epsd + 1, wbs->next + 1);
               if((str = E_StringForName(nameBuffer)))
                  nextMapName = str->string;
            }
            else if(isMAPxy(gamemapname))
            {
               edf_string_t *str;
               psnprintf(nameBuffer, 24, "_IN_NAME_MAP%02d", wbs->next + 1);
               if((str = E_StringForName(nameBuffer)))
                  nextMapName = str->string;
            }
         }
      }
   }
}

// ====================================================================
// WI_Start
// Purpose: Call the various init routines
//          Note: wbstartstruct_t is defined in d_player.h
// Args:    wbstartstruct -- pointer to the structure with the 
//          intermission data
// Returns: void
//
static void WI_Start(wbstartstruct_t *wbstartstruct)
{
   WI_initVariables(wbstartstruct);
   WI_loadData();
   
   if(GameType == gt_dm)
      WI_initDeathmatchStats();
   else if(GameType == gt_coop)
      WI_initNetgameStats();
   else
      WI_initStats();
}

// haleyjd: DOOM intermission object

interfns_t DoomIntermission =
{
   WI_Ticker,
   WI_DrawBackground,
   WI_Drawer,
   WI_Start,
};

//----------------------------------------------------------------------------
//
// $Log: wi_stuff.c,v $
// Revision 1.11  1998/05/04  21:36:02  thldrmn
// commenting and reformatting
//
// Revision 1.10  1998/05/03  22:45:35  killough
// Provide minimal correct #include's at top; nothing else
//
// Revision 1.9  1998/04/27  02:11:44  killough
// Fix lnames being freed too early causing crashes
//
// Revision 1.8  1998/04/26  14:55:38  jim
// Fixed animated back bug
//
// Revision 1.7  1998/04/11  14:49:52  thldrmn
// Fixed par display logic
//
// Revision 1.6  1998/03/28  18:12:03  killough
// Make acceleratestage external so it can be used for teletype
//
// Revision 1.5  1998/03/28  05:33:12  jim
// Text enabling changes for DEH
//
// Revision 1.4  1998/03/18  23:14:14  jim
// Deh text additions
//
// Revision 1.3  1998/02/23  05:00:19  killough
// Fix Secret percentage, avoid par times on pwads
//
// Revision 1.2  1998/01/26  19:25:12  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:05  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
