// Emacs style mode select   -*- C -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2003 James Haley
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
//
// Heretic Status Bar
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "doomstat.h"
#include "m_random.h"
#include "m_swap.h"
#include "p_mobj.h"
#include "v_video.h"
#include "w_wad.h"
#include "st_stuff.h"

//
// Defines
//
#define ST_HBARHEIGHT 42
#define plyr (&players[displayplayer])

//
// Static variables
//

// cached patches
static patch_t *invnums[10];   // inventory numbers

// current state variables
static int chainhealth;        // current position of the gem
static int chainwiggle;        // small randomized addend for chain y coord.

//
// ST_HticInit
//
// Initializes the Heretic status bar:
// * Caches most patch graphics used throughout
//
static void ST_HticInit(void)
{
   int i;

   // load inventory numbers
   for(i = 0; i < 10; ++i)
   {
      char lumpname[9];

      memset(lumpname, 0, 9);
      sprintf(lumpname, "IN%d", i);

      invnums[i] = (patch_t *)W_CacheLumpName(lumpname, PU_STATIC);
   }

   // haleyjd 10/09/05: load key graphics for HUD
   for(i = 0; i < NUMCARDS+3; ++i)  //jff 2/23/98 show both keys too
   {
      extern patch_t *keys[];
      char namebuf[9];
      sprintf(namebuf, "STKEYS%d", i);
      keys[i] = (patch_t *)W_CacheLumpName(namebuf, PU_STATIC);
   }
}

//
// ST_HticStart
//
static void ST_HticStart(void)
{
}

//
// ST_HticTicker
//
// Processing code for Heretic status bar
//
static void ST_HticTicker(void)
{
   int playerHealth;

   // update the chain health value

   playerHealth = plyr->health;

   if(playerHealth != chainhealth)
   {
      int max, min, diff, sgn = 1;

      // set lower bound to zero
      if(playerHealth < 0)
         playerHealth = 0;

      // determine the max and min of the two values, and whether or
      // not to add or subtract from the chain position w/sgn
      if(playerHealth > chainhealth)
      {
         max = playerHealth; min = chainhealth;
      }
      else
      {
         sgn = -1; max = chainhealth; min = playerHealth;
      }

      // consider 1/4th of the difference
      diff = (max - min) >> 2;

      // don't move less than 1 or more than 8 units per tic
      if(diff < 1)
         diff = 1;
      else if(diff > 8)
         diff = 8;

      chainhealth += (sgn * diff);
   }

   // update chain wiggle value
   if(leveltime & 1)
      chainwiggle = M_Random() & 1;
}

static void ST_drawInvNum(int num, int x, int y)
{
   int numdigits = 3;
   boolean neg;

   neg = (num < 0);

   if(neg)
   {
      if(num < -9)
      {
         V_DrawPatch(x - 26, y + 1, &vbscreen, 
                     (patch_t *)W_CacheLumpName("LAME", PU_CACHE));
         return;
      }
         
      num = -num;
   }

   if(!num)
      V_DrawPatch(x - 9, y, &vbscreen, invnums[0]);

   while(num && numdigits--)
   {
      x -= 9;
      V_DrawPatch(x, y, &vbscreen, invnums[num%10]);
      num /= 10;
   }
   
   if(neg)
      V_DrawPatch(x - 18, y, &vbscreen, (patch_t *)W_CacheLumpName("NEGNUM", PU_CACHE));
}

//
// ST_drawBackground
//
// Draws the basic status bar background
//
static void ST_drawBackground(void)
{
   // draw the background
   V_DrawPatch(0, 158, &vbscreen, (patch_t *)W_CacheLumpName("BARBACK", PU_CACHE));
   
   // patch the face eyes with the GOD graphics if the player
   // is in god mode
   if(plyr->cheats & CF_GODMODE)
   {
      V_DrawPatch(16,  167, &vbscreen, (patch_t *)W_CacheLumpName("GOD1", PU_CACHE));
      V_DrawPatch(287, 167, &vbscreen, (patch_t *)W_CacheLumpName("GOD2", PU_CACHE));
   }
   
   // draw the tops of the faces
   V_DrawPatch(0,   148, &vbscreen, (patch_t *)W_CacheLumpName("LTFCTOP", PU_CACHE));
   V_DrawPatch(290, 148, &vbscreen, (patch_t *)W_CacheLumpName("RTFCTOP", PU_CACHE));
}

#define SHADOW_BOX_WIDTH  16
#define SHADOW_BOX_HEIGHT 10

static void ST_BlockDrawerS(int x, int y, int startmap, int mapdir)
{
   byte *dest, *row, *colormap;
   int w, h, i, realx, realy;
   int cx1, cy1, cx2, cy2;

   fixed_t mapnum, mapstep;
   
   cx1 = x;
   cy1 = y;
   cx2 = x + SHADOW_BOX_WIDTH  - 1;
   cy2 = y + SHADOW_BOX_HEIGHT - 1;
               
   realx = vbscreen.x1lookup[cx1];
   realy = vbscreen.y1lookup[cy1];
   w     = vbscreen.x2lookup[cx2] - realx + 1;
   h     = vbscreen.y2lookup[cy2] - realy + 1;

   dest = vbscreen.data + realy * vbscreen.pitch + realx;

   mapstep = mapdir * (16 << FRACBITS) / w;

#ifdef RANGECHECK
   // sanity check
   if(realx < 0 || realx + w > vbscreen.width ||
      realy < 0 || realy + h > vbscreen.height)
   {
      I_Error("ST_BlockDrawerS: block exceeds buffer boundaries.\n");
   }
#endif

   while(h--)
   {
      row = dest;
      i = w;
      mapnum = startmap << FRACBITS;
      
      while(i--)
      {
         colormap = colormaps[0] + (mapnum >> FRACBITS) * 256;
         *row = colormap[*row];
         ++row;
         mapnum += mapstep;
      }

      dest += vbscreen.pitch;
   }
}

//
// ST_chainShadow
//
// Draws two 16x10 shaded areas on the ends of the life chain, using
// the light-fading colormaps to darken what's already been drawn.
//
static void ST_chainShadow(void)
{
   ST_BlockDrawerS(277, 190,  9,  1);
   ST_BlockDrawerS( 19, 190, 23, -1);
}

//
// ST_drawLifeChain
//
// Draws the chain & gem health indicator at the bottom
//
static void ST_drawLifeChain(void)
{
   int y = 191;
   int chainpos = chainhealth;
   
   // bound chainpos between 0 and 100
   if(chainpos < 0)
      chainpos = 0;
   if(chainpos > 100)
      chainpos = 100;
   
   // the total length between the left- and right-most gem
   // positions is 256 pixels, so scale the chainpos by that
   // amount (gem can range from 17 to 273)
   chainpos = (chainpos << 8) / 100;
      
   // jiggle y coordinate when chain is moving
   if(plyr->health != chainhealth)
      y += chainwiggle;

   // draw the chain -- links repeat every 17 pixels, so we
   // wrap the chain back to the starting position every 17
   V_DrawPatch(2 + (chainpos%17), y, &vbscreen, 
               (patch_t *)W_CacheLumpName("CHAIN", PU_CACHE));
   
   // draw the gem (17 is the far left pos., 273 is max)   
   // TODO: fix life gem for multiplayer modes
   V_DrawPatch(17 + chainpos, y, &vbscreen, 
               (patch_t *)W_CacheLumpName("LIFEGEM2", PU_CACHE));
   
   // draw face patches to cover over spare ends of chain
   V_DrawPatch(0,   190, &vbscreen, (patch_t *)W_CacheLumpName("LTFACE", PU_CACHE));
   V_DrawPatch(276, 190, &vbscreen, (patch_t *)W_CacheLumpName("RTFACE", PU_CACHE));
   
   // use the colormap to shadow the ends of the chain
   ST_chainShadow();
}

//
// ST_drawStatBar
//
// Draws the main status bar, shown when the inventory is not
// active.
//
static void ST_drawStatBar(void)
{
   int temp;
   patch_t *statbar;

   // update the status bar patch for the appropriate game mode
   switch(GameType)
   {
   case gt_dm:
      statbar = (patch_t *)W_CacheLumpName("STATBAR", PU_CACHE);
      break;
   default:
      statbar = (patch_t *)W_CacheLumpName("LIFEBAR", PU_CACHE);
      break;
   }

   V_DrawPatch(34, 160, &vbscreen, statbar);

   // TODO: inventory stuff

   // draw frags or health
   if(GameType == gt_dm)
   {
      ST_drawInvNum(plyr->totalfrags, 88, 170);
   }
   else
   {
      temp = chainhealth;

      if(temp < 0)
         temp = 0;

      ST_drawInvNum(temp, 88, 170);
   }

   // draw armor
   ST_drawInvNum(plyr->armorpoints, 255, 170);

   // draw key icons
   if(plyr->cards[it_yellowcard])
      V_DrawPatch(153, 164, &vbscreen, (patch_t *)W_CacheLumpName("YKEYICON", PU_CACHE));
   if(plyr->cards[it_redcard])
      V_DrawPatch(153, 172, &vbscreen, (patch_t *)W_CacheLumpName("GKEYICON", PU_CACHE));
   if(plyr->cards[it_bluecard])
      V_DrawPatch(153, 180, &vbscreen, (patch_t *)W_CacheLumpName("BKEYICON", PU_CACHE));

   // TODO: ammo icon stuff
   // draw ammo amount
   temp = plyr->readyweapon;
   temp = weaponinfo[temp].ammo;
   if(temp < NUMAMMO)
   {
      temp = plyr->ammo[temp];
      ST_drawInvNum(temp, 136, 162);
   }
}

//
// ST_HticDrawer
//
// Draws the Heretic status bar
//
static void ST_HticDrawer(void)
{
   ST_drawBackground();
   ST_drawLifeChain();

   // TODO: choose whether to draw statbar or inventory bar here
   // based on whether the inventory is active
   ST_drawStatBar();
}

//
// ST_HticFSDrawer
//
// Draws the Heretic fullscreen hud/status information.
//
static void ST_HticFSDrawer(void)
{
}

//
// Status Bar Object for GameModeInfo
//
stbarfns_t HticStatusBar =
{
   ST_HBARHEIGHT,

   ST_HticTicker,
   ST_HticDrawer,
   ST_HticFSDrawer,
   ST_HticStart,
   ST_HticInit,
};

// EOF

