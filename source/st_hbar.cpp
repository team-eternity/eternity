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
//
// Heretic Status Bar
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "doomstat.h"
#include "d_dehtbl.h"
#include "e_inventory.h"
#include "m_random.h"
#include "m_swap.h"
#include "metaapi.h"
#include "p_mobj.h"
#include "v_patchfmt.h"
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
static patch_t *smallinvnums[10]; // small inventory numbers

// current state variables
static int chainhealth;        // current position of the gem
static int chainwiggle;        // small randomized addend for chain y coord.

static invbarstate_t hbarstate;

//
// ST_GetInventoryState
//
// Gets the inventory bar state.
//
static invbarstate_t &ST_GetInvBarState()
{
   return hbarstate;
}

//
// ST_drawSmallNumber
//
// Draws a small (at most) 5 digit number. It is RIGHT aligned for x and y.
// x is expected to be 8 more than its equivalent Heretic calls.
//
static void ST_drawSmallNumber(int val, int x, int y)
{   
   if(val > 1)
   {
      patch_t *patch;
      char buf[6];

      // If you want more than 99,999 of something then you probably
      //  know enough about coding to change this hard limit.
      if(val > 99999)
         val = 99999;
      sprintf(buf, "%d", val);
      x -= 4 * (strlen(buf));
      for(char *rover = buf; *rover; rover++)
      {
         int i = *rover - '0';
         patch = smallinvnums[i];
         V_DrawPatch(x, y, &subscreen43, patch);
         x += 4;
      }
   }
}

//
// Initializes the Heretic status bar:
// * Caches most patch graphics used throughout
//

//
// ST_HticInit
//
// Initializes the Heretic status bar:
// * Caches most patch graphics used throughout
//
static void ST_HticInit()
{
   int i;

   // load inventory numbers
   for(i = 0; i < 10; ++i)
   {
      char lumpname[9];

      memset(lumpname, 0, 9);
      sprintf(lumpname, "IN%d", i);

      invnums[i] = PatchLoader::CacheName(wGlobalDir, lumpname, PU_STATIC);
   }

   // load small inventory numbers
   for(i = 0; i < 10; ++i)
   {
      char lumpname[9];

      memset(lumpname, 0, 9);
      sprintf(lumpname, "SMALLIN%d", i);

      smallinvnums[i] = PatchLoader::CacheName(wGlobalDir, lumpname, PU_STATIC);
   }

   // haleyjd 10/09/05: load key graphics for HUD
   for(i = 0; i < NUMCARDS+3; ++i)  //jff 2/23/98 show both keys too
   {
      extern patch_t *keys[];
      char namebuf[9];
      sprintf(namebuf, "STKEYS%d", i);
      keys[i] = PatchLoader::CacheName(wGlobalDir, namebuf, PU_STATIC);
   }

   // Initialise the inventory bar state.
   hbarstate = { false, 0, 0, 0 };
}

//
// ST_HticStart
//
static void ST_HticStart()
{
}

//
// ST_HticTicker
//
// Processing code for Heretic status bar
//
static void ST_HticTicker()
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
   bool neg;

   neg = (num < 0);

   if(neg)
   {
      if(num < -9)
      {
         V_DrawPatch(x - 26, y + 1, &subscreen43, 
                     PatchLoader::CacheName(wGlobalDir, "LAME", PU_CACHE));
         return;
      }
         
      num = -num;
   }

   if(!num)
      V_DrawPatch(x - 9, y, &subscreen43, invnums[0]);

   while(num && numdigits--)
   {
      x -= 9;
      V_DrawPatch(x, y, &subscreen43, invnums[num%10]);
      num /= 10;
   }
   
   if(neg)
   {
      V_DrawPatch(x - 18, y, &subscreen43, 
                  PatchLoader::CacheName(wGlobalDir, "NEGNUM", PU_CACHE));
   }
}

//
// ST_drawBackground
//
// Draws the basic status bar background
//
static void ST_drawBackground()
{
   // draw the background
   V_DrawPatch(0, 158, &subscreen43, PatchLoader::CacheName(wGlobalDir, "BARBACK", PU_CACHE));
   
   // patch the face eyes with the GOD graphics if the player
   // is in god mode
   if(plyr->cheats & CF_GODMODE)
   {
      V_DrawPatch(16,  167, &subscreen43, PatchLoader::CacheName(wGlobalDir, "GOD1", PU_CACHE));
      V_DrawPatch(287, 167, &subscreen43, PatchLoader::CacheName(wGlobalDir, "GOD2", PU_CACHE));
   }
   
   // draw the tops of the faces
   V_DrawPatch(0,   148, &subscreen43, PatchLoader::CacheName(wGlobalDir, "LTFCTOP", PU_CACHE));
   V_DrawPatch(290, 148, &subscreen43, PatchLoader::CacheName(wGlobalDir, "RTFCTOP", PU_CACHE));
}

#define SHADOW_BOX_WIDTH  16
#define SHADOW_BOX_HEIGHT 10

static void ST_BlockDrawerS(int x, int y, int startcmap, int mapdir)
{
   byte *dest, *row, *colormap;
   int w, h, i, realx, realy;
   int cx1, cy1, cx2, cy2;

   fixed_t mapnum, mapstep;
   
   cx1 = x;
   cy1 = y;
   cx2 = x + SHADOW_BOX_WIDTH  - 1;
   cy2 = y + SHADOW_BOX_HEIGHT - 1;
               
   realx = subscreen43.x1lookup[cx1];
   realy = subscreen43.y1lookup[cy1];
   w     = subscreen43.x2lookup[cx2] - realx + 1;
   h     = subscreen43.y2lookup[cy2] - realy + 1;

   dest = subscreen43.data + realy * subscreen43.pitch + realx;

   mapstep = mapdir * (16 << FRACBITS) / w;

#ifdef RANGECHECK
   // sanity check
   if(realx < 0 || realx + w > subscreen43.width ||
      realy < 0 || realy + h > subscreen43.height)
   {
      I_Error("ST_BlockDrawerS: block exceeds buffer boundaries.\n");
   }
#endif

   while(h--)
   {
      row = dest;
      i = w;
      mapnum = startcmap << FRACBITS;
      
      while(i--)
      {
         colormap = colormaps[0] + (mapnum >> FRACBITS) * 256;
         *row = colormap[*row];
         ++row;
         mapnum += mapstep;
      }

      dest += subscreen43.pitch;
   }
}

//
// ST_chainShadow
//
// Draws two 16x10 shaded areas on the ends of the life chain, using
// the light-fading colormaps to darken what's already been drawn.
//
static void ST_chainShadow()
{
   ST_BlockDrawerS(277, 190,  9,  1);
   ST_BlockDrawerS( 19, 190, 23, -1);
}

//
// ST_drawLifeChain
//
// Draws the chain & gem health indicator at the bottom
//
static void ST_drawLifeChain()
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
   V_DrawPatch(2 + (chainpos%17), y, &subscreen43, 
               PatchLoader::CacheName(wGlobalDir, "CHAIN", PU_CACHE));
   
   // draw the gem (17 is the far left pos., 273 is max)   
   // TODO: fix life gem for multiplayer modes
   V_DrawPatch(17 + chainpos, y, &subscreen43, 
               PatchLoader::CacheName(wGlobalDir, "LIFEGEM2", PU_CACHE));
   
   // draw face patches to cover over spare ends of chain
   V_DrawPatch(0,   190, &subscreen43, PatchLoader::CacheName(wGlobalDir, "LTFACE", PU_CACHE));
   V_DrawPatch(276, 190, &subscreen43, PatchLoader::CacheName(wGlobalDir, "RTFACE", PU_CACHE));
   
   // use the colormap to shadow the ends of the chain
   ST_chainShadow();
}

//
// ST_drawStatBar
//
// Draws the main status bar, shown when the inventory is not
// active.
//
static void ST_drawStatBar()
{
   int temp;
   patch_t *statbar;
   const char *patch;
   itemeffect_t *artifact;

   // update the status bar patch for the appropriate game mode
   switch(GameType)
   {
   case gt_dm:
      statbar = PatchLoader::CacheName(wGlobalDir, "STATBAR", PU_CACHE);
      break;
   default:
      statbar = PatchLoader::CacheName(wGlobalDir, "LIFEBAR", PU_CACHE);
      break;
   }

   V_DrawPatch(34, 160, &subscreen43, statbar);

   // ArtifactFlash, it's a gas! Gas! Gas!
   if(hbarstate.ArtifactFlash)
   {
      V_DrawPatch(180, 161, &subscreen43,
         PatchLoader::CacheName(wGlobalDir, "BLACKSQ", PU_CACHE));

      temp = W_GetNumForName(DEH_String("useartia")) + hbarstate.ArtifactFlash - 1;

      V_DrawPatch(182, 161, &subscreen43,
         PatchLoader::CacheNum(wGlobalDir, temp, PU_CACHE));
      // TODO: Perhaps introduce a static int and slow down the effect?
      // MaxW: Choco Heretic does stuff that I'm not sure how to translate to EE.
      hbarstate.ArtifactFlash--;
   }
   // It's safety checks all the way down!
   else if(plyr->inventory[plyr->inv_ptr].amount)
   {
      if(artifact = E_EffectForInventoryItemID(plyr->inventory[plyr->inv_ptr].item))
      {
         if((patch = artifact->getString("icon", "")) != "")
         {
            V_DrawPatch(179, 160, &subscreen43,
               PatchLoader::CacheName(wGlobalDir, patch, PU_CACHE, lumpinfo_t::ns_sprites));
            ST_drawSmallNumber(plyr->inventory[plyr->inv_ptr].amount, 209, 182);
         }
      }
   }

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
   if(E_GetItemOwnedAmountName(plyr, ARTI_KEYYELLOW) > 0)
      V_DrawPatch(153, 164, &subscreen43, PatchLoader::CacheName(wGlobalDir, "YKEYICON", PU_CACHE));
   if(E_GetItemOwnedAmountName(plyr, ARTI_KEYGREEN) > 0)
      V_DrawPatch(153, 172, &subscreen43, PatchLoader::CacheName(wGlobalDir, "GKEYICON", PU_CACHE));
   if(E_GetItemOwnedAmountName(plyr, ARTI_KEYBLUE) > 0)
      V_DrawPatch(153, 180, &subscreen43, PatchLoader::CacheName(wGlobalDir, "BKEYICON", PU_CACHE));

   // TODO: ammo icon stuff
   // draw ammo amount
   itemeffect_t *ammo = P_GetReadyWeapon(plyr)->ammo;
   if(ammo)
      ST_drawInvNum(E_GetItemOwnedAmount(plyr, ammo), 136, 162);
}

static void ST_drawInvBar()
{
   itemeffect_t *artifact;
   const char *patch;
   int leftoffs = hbarstate.curpos >= 7 ? hbarstate.curpos - 6 : 0;

   V_DrawPatch(34, 160, &subscreen43, PatchLoader::CacheName(wGlobalDir, "INVBAR", PU_CACHE));

   int i = 0;
   do
   {
      if(plyr->inventory[i + leftoffs].amount > 0)
      {
         if(artifact = E_EffectForInventoryItemID(plyr->inventory[i + leftoffs].item))
         {
            if((patch = artifact->getString("icon", "")) != "")
            {
               V_DrawPatch(50 + i * 31, 160, &subscreen43,
                  PatchLoader::CacheName(wGlobalDir, patch, PU_CACHE, lumpinfo_t::ns_sprites));
               ST_drawSmallNumber(plyr->inventory[i + leftoffs].amount, 77 + i * 31, 182);
            }
         }
      }
   } while(E_MoveInventoryCursor(plyr, 1, i) && i < 7);

   V_DrawPatch(50 + (hbarstate.curpos - leftoffs)* 31, 189, &subscreen43,
     PatchLoader::CacheName(wGlobalDir, "SELECTBO", PU_CACHE));
}
//
// ST_HticDrawer
//
// Draws the Heretic status bar
//
static void ST_HticDrawer()
{
   ST_drawBackground();
   ST_drawLifeChain();

   // TODO: choose whether to draw statbar or inventory bar here
   // based on whether the inventory is active
   if(hbarstate.inventory)
      ST_drawInvBar();
   else
      ST_drawStatBar();
}

//
// ST_HticFSDrawer
//
// Draws the Heretic fullscreen hud/status information.
//
static void ST_HticFSDrawer()
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
   ST_GetInvBarState,
};

// EOF

