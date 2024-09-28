//
// Copyright (C) 2020 James Haley, Ioan Chera et al.
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//----------------------------------------------------------------------------
//
// Purpose: inventory HUD widget
// Authors: Ioan Chera
//

#include "z_zone.h"

#include "d_dehtbl.h"
#include "d_gi.h"
#include "doomstat.h"
#include "e_inventory.h"
#include "hu_over.h"
#include "metaapi.h"
#include "st_stuff.h"
#include "v_patchfmt.h"
#include "v_video.h"

//
// Draws the current-item inventory display
//
void HU_InventoryDrawCurrentBox(int x, int y)
{
   if(GameModeInfo->type != Game_Heretic)
      return;  // currently only enabled for Heretic. Other games will need it too
   const player_t &plyr = players[displayplayer];
   if(plyr.invbarstate.inventory || !plyr.inventory[plyr.inv_ptr].amount)
      return;  // do not display it if selector is active or in case of no item
   const itemeffect_t *artifact = E_EffectForInventoryIndex(&plyr, plyr.inv_ptr);
   if(!artifact)
      return;
   const char *patch = artifact->getString("icon", nullptr);
   if(estrempty(patch) || !artifact->getInt("invbar", 0))
      return;

   patch_t *box = PatchLoader::CacheName(wGlobalDir, DEH_String("ARTIBOX"), PU_CACHE);
   V_DrawPatchTL(x, y, &vbscreenyscaled, box, nullptr, HTIC_GHOST_TRANS);
   patch_t *icon = PatchLoader::CacheName(wGlobalDir, patch, PU_CACHE, lumpinfo_t::ns_sprites);
   V_DrawPatch(x, y, &vbscreenyscaled, icon);
   ST_DrawSmallHereticNumber(E_GetItemOwnedAmount(&plyr, artifact), x + 29, y + 22, true);
}

//
// Gets suggested coordinates, free for customization later
//
void HU_InventoryGetCurrentBoxHints(int &x, int &y)
{
   x = vbscreenyscaled.unscaledw - (320 - 286);
   y = 170;
}

//
// Draws the selector box
//
void HU_InventoryDrawSelector()
{
   if(GameModeInfo->type != Game_Heretic)
      return;  // currently only enabled for Heretic. Other games will need it too

   // HERETIC CODE BELOW. Branch off to other games when it comes to it.

   if(!ST_IsHUDLike())  // do not draw inventory if status bar is active.
      return;
   const player_t &plyr = players[displayplayer];
   if(!plyr.invbarstate.inventory)  // must be in inventory mode, otherwise don't draw
      return;

   int count = 7 /*+ (vbscreenyscaled.unscaledw - SCREENWIDTH) / 31*/;
   int origin = 50 + (vbscreenyscaled.unscaledw - SCREENWIDTH) / 2 - (count - 7) * 31 / 2;
   int ybase = 168;

   patch_t *box = PatchLoader::CacheName(wGlobalDir, DEH_String("ARTIBOX"), PU_CACHE);
   for(int i = 0; i < count; ++i)
   {
      if(hud_enabled && hud_overlaylayout != HUD_OFF && hud_overlaylayout != HUD_GRAPHICAL)
         V_DrawPatch(origin + i * 31, ybase, &vbscreenyscaled, box);
      else
         V_DrawPatchTL(origin + i * 31, ybase, &vbscreenyscaled, box, nullptr, HTIC_GHOST_TRANS);
   }
   const int inv_ptr = plyr.inv_ptr;
   const int leftoffs = inv_ptr >= count ? inv_ptr - count + 1 : 0;
   int i = -1;
   while(E_MoveInventoryCursor(&plyr, 1, i) && i < count)
   {
      if(plyr.inventory[i + leftoffs].amount <= 0)
         continue;
      const itemeffect_t *artifact = E_EffectForInventoryIndex(&plyr, i + leftoffs);
      if(!artifact)
         continue;
      const char *patchname = artifact->getString("icon", nullptr);
      if(estrempty(patchname))
         continue;
      int ns = wGlobalDir.checkNumForName(patchname, lumpinfo_t::ns_global) >= 0 ?
         lumpinfo_t::ns_global : lumpinfo_t::ns_sprites;
      patch_t *patch = PatchLoader::CacheName(wGlobalDir, patchname, PU_CACHE, ns);
      const int xoffs = artifact->getInt("icon.offset.x", 0);
      const int yoffs = artifact->getInt("icon.offset.y", 0);

      V_DrawPatch(origin + i * 31 - xoffs, ybase - yoffs, &vbscreenyscaled, patch);
      ST_DrawSmallHereticNumber(E_GetItemOwnedAmount(&plyr, artifact), origin + 19 + i * 31 + 8,
                                ybase + 22, true);
   }
   patch_t *selectbox = PatchLoader::CacheName(wGlobalDir, "SELECTBO", PU_CACHE);
   V_DrawPatch(origin + (inv_ptr - leftoffs) * 31, ybase + 29, &vbscreenyscaled, selectbox);

   patch_t *leftarrows[2], *rightarrows[2];
   leftarrows[0] = PatchLoader::CacheName(wGlobalDir, DEH_String("INVGEML1"), PU_CACHE);
   leftarrows[1] = PatchLoader::CacheName(wGlobalDir, DEH_String("INVGEML2"), PU_CACHE);
   rightarrows[0] = PatchLoader::CacheName(wGlobalDir, DEH_String("INVGEMR1"), PU_CACHE);
   rightarrows[1] = PatchLoader::CacheName(wGlobalDir, DEH_String("INVGEMR2"), PU_CACHE);

   if(leftoffs)
   {
      V_DrawPatch(origin - 12, ybase - 1, &vbscreenyscaled,
                  !(leveltime & 4) ? leftarrows[0] : leftarrows[1]);
   }
   if(i == count && E_CanMoveInventoryCursor(&plyr, 1, i + leftoffs - 1))
   {
      V_DrawPatch(origin + count * 31 + 2, ybase - 1, &vbscreenyscaled,
                  !(leveltime & 4) ? rightarrows[0] : rightarrows[1]);
   }
}

//
// Draws the buff effects
//
void HU_BuffsDraw(int leftoffset, int rightoffset)
{
   static bool hitCenterFrame;
   static const char *const spinbooklumps[] =
   {
      "SPINBK0", "SPINBK1", "SPINBK2", "SPINBK3", "SPINBK4", "SPINBK5", "SPINBK6", "SPINBK7",
      "SPINBK8", "SPINBK9", "SPINBK10", "SPINBK11", "SPINBK12", "SPINBK13", "SPINBK14", "SPINBK15",
   };
   static const char *const spinflylumps[] =
   {
      "SPFLY0", "SPFLY1", "SPFLY2", "SPFLY3", "SPFLY4", "SPFLY5", "SPFLY6", "SPFLY7", "SPFLY8",
      "SPFLY9", "SPFLY10", "SPFLY11", "SPFLY12", "SPFLY13", "SPFLY14", "SPFLY15",
   };

   if(GameModeInfo->type != Game_Heretic)
      return;  // currently only enabled for Heretic. Other games will need it too
   const player_t &plyr = players[displayplayer];
   if(plyr.powers[pw_flight].isActive())
   {
      // Blink the wings when the player is almost out
      if(plyr.powers->infinite || plyr.powers[pw_flight].tics > (4 * 32) || !(plyr.powers[pw_flight].tics & 16))
      {
         int frame = (leveltime / 3) & 15;
         if(plyr.mo->flags4 & MF4_FLY)
         {
            if(hitCenterFrame && (frame != 15 && frame != 0))
            {
               V_DrawPatch(20, 17 + leftoffset, &vbscreenyscaled,
                           PatchLoader::CacheName(wGlobalDir, spinflylumps[15], PU_CACHE));
            }
            else
            {
               V_DrawPatch(20, 17 + leftoffset, &vbscreenyscaled,
                           PatchLoader::CacheName(wGlobalDir, spinflylumps[frame], PU_CACHE));
               hitCenterFrame = false;
            }
         }
         else
         {
            // FIXME: Why would this code trigger?
            if(!hitCenterFrame && (frame != 15 && frame != 0))
            {
               V_DrawPatch(20, 17 + leftoffset, &vbscreenyscaled,
                           PatchLoader::CacheName(wGlobalDir, spinflylumps[frame], PU_CACHE));
               hitCenterFrame = false;
            }
            else
            {
               V_DrawPatch(20, 17 + leftoffset, &vbscreenyscaled,
                           PatchLoader::CacheName(wGlobalDir, spinflylumps[15], PU_CACHE));
               hitCenterFrame = true;
            }
         }
      }
   }

   if(plyr.powers[pw_weaponlevel2].isActive() && !plyr.morphTics)
   {
      if(plyr.powers[pw_weaponlevel2].infinite ||
         plyr.powers[pw_weaponlevel2].tics > (4 * 32) || !(plyr.powers[pw_weaponlevel2].tics & 16))
      {
         const int frame = (leveltime / 3) & 15;
         V_DrawPatch(vbscreenyscaled.unscaledw - 20, 17 + rightoffset, &vbscreenyscaled,
                     PatchLoader::CacheName(wGlobalDir, spinbooklumps[frame], PU_CACHE));
      }
   }
}

// EOF
