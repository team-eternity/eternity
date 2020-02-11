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
#include "d_player.h"
#include "e_inventory.h"
#include "hu_inventory.h"
#include "hu_over.h"
#include "hu_stuff.h"
#include "metaapi.h"
#include "r_draw.h"
#include "st_stuff.h"
#include "v_patchfmt.h"
#include "v_video.h"

//
// Heretic fullscreen inventory widget. Needs to appear independent on other HUD types.
//
class HUDInventoryWidget : public HUDWidget
{
public:
   void clear() override;
   void ticker() override;
   void drawer() override;

private:
   patch_t *leftarrows[2];
   patch_t *rightarrows[2];
};

static HUDInventoryWidget huInventory;

//
// startup
//
void HUDInventoryWidget::clear()
{
   // Currently, limit to Heretic. Later it should be game-agnostic and pick the appropriate GFX.
   disabled = GameModeInfo->type != Game_Heretic;

   if(disabled)
      return;  // don't continue

   leftarrows[0] = PatchLoader::CacheName(wGlobalDir, DEH_String("INVGEML1"), PU_STATIC);
   leftarrows[1] = PatchLoader::CacheName(wGlobalDir, DEH_String("INVGEML2"), PU_STATIC);
   rightarrows[0] = PatchLoader::CacheName(wGlobalDir, DEH_String("INVGEMR1"), PU_STATIC);
   rightarrows[1] = PatchLoader::CacheName(wGlobalDir, DEH_String("INVGEMR2"), PU_STATIC);
}

//
// Decide whether to draw or not
//
void HUDInventoryWidget::ticker()
{
}

//
// Drawing it
//
void HUDInventoryWidget::drawer()
{
   // Check if should draw. NOTE: this is Heretic specific. Adapt it to fit all new games, even DOOM
   if(!ST_IsHUDLike())  // do not draw inventory if status bar is active.
      return;

   const player_t &plyr = players[displayplayer];
   if(!plyr.invbarstate.inventory)
   {
      if(plyr.inventory[plyr.inv_ptr].amount)
      {
         const itemeffect_t *artifact = E_EffectForInventoryIndex(&plyr, plyr.inv_ptr);
         if(artifact)
         {
            const char *patch = artifact->getString("icon", nullptr);
            if(estrnonempty(patch) && artifact->getInt("invbar", 0))
            {
               // FIXME: this just assumes hardcoded constants in the overlay types
               int ybase = 170;
               if(hud_enabled)
               {
                  if(hud_overlaylayout == HUD_FLAT && (hud_overlayid != HUO_BOOM ||
                                                       vbscreenyscaled.unscaledw < 330) &&
                     (hud_overlayid != HUO_MODERN || !hud_hidestatus))
                  {
                     ybase -= 24;
                  }
                  else if(hud_overlaylayout == HUD_DISTRIB)
                     ybase -= 16;
               }
               patch_t *box = PatchLoader::CacheName(wGlobalDir, DEH_String("ARTIBOX"), PU_CACHE);
               int x = vbscreenyscaled.unscaledw - (320 - 286);
               V_DrawPatchTL(x, ybase, &vbscreenyscaled, box, nullptr, HTIC_GHOST_TRANS);
               patch_t *icon = PatchLoader::CacheName(wGlobalDir, patch, PU_CACHE,
                                                      lumpinfo_t::ns_sprites);
               V_DrawPatch(x, ybase, &vbscreenyscaled, icon);
               ST_DrawSmallHereticNumber(E_GetItemOwnedAmount(&plyr, artifact),
                                         vbscreenyscaled.unscaledw - (320 - 307) + 8, ybase + 22, true);
            }
         }
      }
   }
   else
   {
      int count = 7 + (vbscreenyscaled.unscaledw - SCREENWIDTH) / 31;
      int origin = 50 + (vbscreenyscaled.unscaledw - SCREENWIDTH) / 2 - (count - 7) * 31 / 2;
      int ybase = 168;
      if(hud_enabled)
      {
         switch(hud_overlaylayout)
         {
            case HUD_BOOM:
               ybase -= 48;
               break;
            case HUD_FLAT:
               ybase -= 24;
               break;
            case HUD_DISTRIB:
               ybase -= 16;
               break;
         }
      }

      patch_t *box = PatchLoader::CacheName(wGlobalDir, DEH_String("ARTIBOX"), PU_CACHE);
      for(int i = 0; i < count; ++i)
         V_DrawPatchTL(origin + i * 31, ybase, &vbscreenyscaled, box, nullptr, HTIC_GHOST_TRANS);
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
}

void HU_InitInventory()
{
   HUDWidget::AddWidgetToHash(&huInventory);
}

// EOF
