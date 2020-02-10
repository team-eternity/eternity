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
#include "doomstat.h"
#include "d_player.h"
#include "e_inventory.h"
#include "hu_inventory.h"
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
   disabled = scaledwindow.height != SCREENHEIGHT;
}

//
// Drawing it
//
void HUDInventoryWidget::drawer()
{
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
               patch_t *box = PatchLoader::CacheName(wGlobalDir, DEH_String("ARTIBOX"), PU_CACHE);
               int x = vbscreenyscaled.unscaledw - (320 - 286);
               V_DrawPatchTL(x, 170, &vbscreenyscaled, box, nullptr, HTIC_GHOST_TRANS);
               patch_t *icon = PatchLoader::CacheName(wGlobalDir, patch, PU_CACHE,
                                                      lumpinfo_t::ns_sprites);
               V_DrawPatch(x, 170, &vbscreenyscaled, icon);
               ST_DrawSmallHereticNumber(E_GetItemOwnedAmount(&plyr, artifact),
                                         vbscreenyscaled.unscaledw - (320 - 307) + 8, 192, true);
            }
         }
      }
   }
   else
   {
      int count = 7 + (vbscreenyscaled.unscaledw - SCREENWIDTH) / 31;
      int origin = 50 + (vbscreenyscaled.unscaledw - SCREENWIDTH) / 2 - (count - 7) * 31 / 2;

      patch_t *box = PatchLoader::CacheName(wGlobalDir, DEH_String("ARTIBOX"), PU_CACHE);
      for(int i = 0; i < count; ++i)
         V_DrawPatchTL(origin + i * 31, 168, &vbscreenyscaled, box, nullptr, HTIC_GHOST_TRANS);
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

         V_DrawPatch(origin + i * 31 - xoffs, 168 - yoffs, &vbscreenyscaled, patch);
         ST_DrawSmallHereticNumber(E_GetItemOwnedAmount(&plyr, artifact), origin + 19 + i * 31 + 8,
                                   190, true);
      }
      patch_t *selectbox = PatchLoader::CacheName(wGlobalDir, "SELECTBO", PU_CACHE);
      V_DrawPatch(origin + (inv_ptr - leftoffs) * 31, 197, &vbscreenyscaled, selectbox);
      if(leftoffs)
      {
         V_DrawPatch(origin - 12, 167, &vbscreenyscaled,
                     !(leveltime & 4) ? leftarrows[0] : leftarrows[1]);
      }
      if(i == count && E_CanMoveInventoryCursor(&plyr, 1, i + leftoffs - 1))
      {
         V_DrawPatch(origin + count * 31 + 2, 167, &vbscreenyscaled,
                     !(leveltime & 4) ? rightarrows[0] : rightarrows[1]);
      }
   }
}

void HU_InitInventory()
{
   HUDWidget::AddWidgetToHash(&huInventory);
}

// EOF
