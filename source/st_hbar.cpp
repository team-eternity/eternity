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
#include "doomstat.h"
#include "d_dehtbl.h"
#include "e_inventory.h"
#include "hu_inventory.h"
#include "metaapi.h"
#include "r_patch.h"
#include "v_patchfmt.h"
#include "v_video.h"
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
static patch_t *invnums[10];      // inventory numbers
static patch_t *smallinvnums[10]; // small inventory numbers
static patch_t *bigfsnums[10];    // large fullscreen numbers
static patch_t *PatchINVLFGEM1;
static patch_t *PatchINVLFGEM2;
static patch_t *PatchINVRTGEM1;
static patch_t *PatchINVRTGEM2;
static patch_t *PatchBLACKSQ;

// current state variables
static int chainhealth;        // current position of the gem
static int chainwiggle;        // small randomized addend for chain y coord.

//
// ST_drawSmallNumber
//
// Draws a small (at most) 5 digit number. It is RIGHT aligned for x and y.
// x is expected to be 8 more than its equivalent Heretic calls.
//
void ST_DrawSmallHereticNumber(int val, int x, int y, bool fullscreen)
{
   if(val > 1)
   {
      patch_t *patch;
      char buf[6];

      // If you want more than 99,999 of something then you probably
      // know enough about coding to change this hard limit.
      if(val > 99999)
         val = 99999;
      sprintf(buf, "%d", val);
      x -= static_cast<int>(4 * (strlen(buf)));
      for(char *rover = buf; *rover; rover++)
      {
         int i = *rover - '0';
         patch = smallinvnums[i];
         V_DrawPatch(x, y, fullscreen ? &vbscreenyscaled : &subscreen43, patch);
         x += 4;
      }
   }
}

//
// Big green numbers for fullscreen graphical HUD
//
static void ST_drawBigNumber(int val, int x, int y)
{
   int oldval = val;
   int xpos = x;
   if(val < 0)
      val = 0;
   patch_t *patch;
   if(val > 99)
   {
      patch = bigfsnums[val / 100 % 10];
      V_DrawPatchShadowed(xpos + 6 - patch->width / 2, y, &vbscreenyscaled, patch, nullptr, FRACUNIT);
   }
   val %= 100;
   xpos += 12;
   if(val > 9 || oldval > 99)
   {
      patch = bigfsnums[val / 10 % 10];
      V_DrawPatchShadowed(xpos + 6 - patch->width / 2, y, &vbscreenyscaled, patch, nullptr, FRACUNIT);
   }
   val %= 10;
   xpos += 12;
   patch = bigfsnums[val % 10];
   V_DrawPatchShadowed(xpos + 6 - patch->width / 2, y, &vbscreenyscaled, patch, nullptr, FRACUNIT);
}

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

      efree(invnums[i]);
      invnums[i] = PatchLoader::CacheName(wGlobalDir, lumpname, PU_STATIC);
   }

   // load small inventory numbers
   for(i = 0; i < 10; ++i)
   {
      char lumpname[9];

      memset(lumpname, 0, 9);
      sprintf(lumpname, "SMALLIN%d", i);

      smallinvnums[i] = PatchLoader::CacheName(wGlobalDir, lumpname, PU_STATIC);

      snprintf(lumpname, sizeof(lumpname), "FONTB%d", 16 + i); // FONTB16-FONTB25
      bigfsnums[i] = PatchLoader::CacheName(wGlobalDir, lumpname, PU_STATIC);
   }

   PatchINVLFGEM1 = PatchLoader::CacheName(wGlobalDir, DEH_String("INVGEML1"), PU_STATIC);
   PatchINVLFGEM2 = PatchLoader::CacheName(wGlobalDir, DEH_String("INVGEML2"), PU_STATIC);
   PatchINVRTGEM1 = PatchLoader::CacheName(wGlobalDir, DEH_String("INVGEMR1"), PU_STATIC);
   PatchINVRTGEM2 = PatchLoader::CacheName(wGlobalDir, DEH_String("INVGEMR2"), PU_STATIC);
   PatchBLACKSQ   = PatchLoader::CacheName(wGlobalDir, DEH_String("BLACKSQ"),  PU_STATIC);

   // haleyjd 10/09/05: load key graphics for HUD
   for(i = 0; i < NUMCARDS+3; ++i)  //jff 2/23/98 show both keys too
   {
      extern patch_t *keys[];
      char namebuf[9];
      sprintf(namebuf, "STKEYS%d", i);

      efree(keys[i]);
      keys[i] = PatchLoader::CacheName(wGlobalDir, namebuf, PU_STATIC);
   }
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
   static int prevleveltime = 0;
   invbarstate_t &invbarstate = players[displayplayer].invbarstate;

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
   if(invbarstate.ArtifactFlash)
   {
      V_DrawPatch(180, 161, &subscreen43, PatchBLACKSQ);

      temp = W_GetNumForName(DEH_String("useartia")) + invbarstate.ArtifactFlash - 1;

      V_DrawPatch(182, 161, &subscreen43, PatchLoader::CacheNum(wGlobalDir, temp, PU_CACHE));
      if(leveltime != prevleveltime)
      {
         invbarstate.ArtifactFlash--;
         prevleveltime = leveltime;
      }
   }
   // It's safety checks all the way down!
   else if(plyr->inventory[plyr->inv_ptr].amount)
   {
      if((artifact = E_EffectForInventoryIndex(plyr, plyr->inv_ptr)))
      {
         patch = artifact->getString("icon", nullptr);
         if(estrnonempty(patch) && artifact->getInt("invbar", 0))
         {
            V_DrawPatch(179, 160, &subscreen43,
                        PatchLoader::CacheName(wGlobalDir, patch,
                                               PU_CACHE, lumpinfo_t::ns_sprites));
            ST_DrawSmallHereticNumber(E_GetItemOwnedAmount(plyr, artifact), 209, 182, false);
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

   // draw ammo amount
   itemeffect_t *ammo = plyr->readyweapon->ammo;
   if(ammo)
   {
      V_DrawPatch(108, 161, &subscreen43, PatchBLACKSQ);
      patch = ammo->getString("icon", "");
      if(estrnonempty(patch))
      {
         V_DrawPatch(111, 172, &subscreen43,
                     PatchLoader::CacheName(wGlobalDir, patch, PU_CACHE));
      }
      ST_drawInvNum(E_GetItemOwnedAmount(plyr, ammo), 136, 162);
   }
}

static void ST_drawInvBar()
{
   constexpr int ST_INVBARBGX = 34; // You'd think it'd be (SCREENWIDTH - 248) / 2 (= 36)? Nope.
   constexpr int ST_INVBARBGY = SCREENHEIGHT - 40; // 31 high but 9 extra pixels
   constexpr int ST_INVSLOTSTARTX = ST_INVBARBGX + 16;

   const int inv_ptr  = players[displayplayer].inv_ptr;
   const int leftoffs = inv_ptr >= 7 ? inv_ptr - 6 : 0;

   V_DrawPatch(ST_INVBARBGX, ST_INVBARBGY, &subscreen43,
               PatchLoader::CacheName(wGlobalDir, "INVBAR", PU_CACHE));

   int i = -1;
   // E_MoveInventoryCursor returns false when it hits the boundary of the visible inventory,
   // so it's a useful iterator here.
   while(E_MoveInventoryCursor(plyr, 1, i) && i < 7)
   {
      // Safety check that the player has an inventory item, then that the effect exists
      // for the selected item, then that there is an associated patch for that effect.
      if(plyr->inventory[i + leftoffs].amount > 0)
      {
         itemeffect_t *artifact = E_EffectForInventoryIndex(plyr, i + leftoffs);
         if(artifact)
         {
            const char *patchname = artifact->getString("icon", nullptr);
            if(estrnonempty(patchname))
            {
               int ns = wGlobalDir.checkNumForName(patchname, lumpinfo_t::ns_global) >= 0 ?
                           lumpinfo_t::ns_global : lumpinfo_t::ns_sprites;
               patch_t *patch = PatchLoader::CacheName(wGlobalDir, patchname, PU_CACHE, ns);

               const int xoffs = artifact->getInt("icon.offset.x", 0);
               const int yoffs = artifact->getInt("icon.offset.y", 0);

               V_DrawPatch(ST_INVSLOTSTARTX + (i * 31) - xoffs, 160 - yoffs,
                           &subscreen43, patch);
               ST_DrawSmallHereticNumber(E_GetItemOwnedAmount(plyr, artifact), ST_INVSLOTSTARTX +
                                         27 + (i * 31), ST_INVBARBGY + 22, false);
            }
         }
      }
   }

   if(leftoffs)
      V_DrawPatch(38, 159, &subscreen43, !(leveltime & 4) ? PatchINVLFGEM1 : PatchINVLFGEM2);
   if(i == 7 && E_CanMoveInventoryCursor(plyr, 1, i + leftoffs - 1))
      V_DrawPatch(269, 159, &subscreen43, !(leveltime & 4) ? PatchINVRTGEM1 : PatchINVRTGEM2);

   V_DrawPatch(ST_INVSLOTSTARTX + ((inv_ptr - leftoffs) * 31), SCREENHEIGHT - 11,
               &subscreen43, PatchLoader::CacheName(wGlobalDir, "SELECTBO", PU_CACHE));
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

   if(players[displayplayer].invbarstate.inventory)
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
   ST_drawBigNumber(plyr->health, 5, 180);
   if(GameType == gt_dm)
      ST_drawInvNum(plyr->totalfrags, 45, 185);

   int posx, posy;
   HU_InventoryGetCurrentBoxHints(posx, posy);
   HU_InventoryDrawCurrentBox(posx, posy);
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

