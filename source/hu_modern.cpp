//
// Copyright (C) 2018 James Haley, Max Waine et al.
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
// Purpose: ptoing's modern HUD.
// Authors: Max Waine
//

#include "d_gi.h"
#include "doomstat.h"
#include "e_inventory.h"
#include "e_weapons.h"
#include "hu_modern.h"
#include "m_qstr.h"
#include "r_patch.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"

//=============================================================================
//
// Internal Defines
//

// The HUD always displays information on the displayplayer
#define hu_player (players[displayplayer])

// Get the amount of ammo the displayplayer has left in his/her readyweapon
#define playerammo    HU_WC_PlayerAmmo(hu_player.readyweapon)

// Get the maximum amount the player could have for his/her readyweapon
#define playermaxammo HU_WC_MaxAmmo(hu_player.readyweapon)

//=============================================================================
//
// Heart of the overlay really.
// Draw the overlay, deciding which bits to draw and where.
//

void ModernHUD::DrawStatus(int x, int y)
{
   qstring tempstr;

   tempstr << hu_player.killcount << " / " << totalkills << "  " << FC_RED "KILLS";
   V_FontWriteText(hud_fssmall, tempstr.constPtr(),
                   x - V_FontStringWidth(hud_fssmall, tempstr.constPtr()),
                   y, &vbscreen);
   tempstr.clear();
   tempstr << hu_player.itemcount << " / " << totalitems << "  " << FC_BLUE "ITEMS";
   V_FontWriteText(hud_fssmall, tempstr.constPtr(),
                   x - V_FontStringWidth(hud_fssmall, tempstr.constPtr()),
                   y + 8, &vbscreen);
   tempstr.clear();
   tempstr << hu_player.secretcount << " / " << totalsecret << "  " << FC_GOLD "SCRTS";
   V_FontWriteText(hud_fssmall, tempstr.constPtr(),
                   x - V_FontStringWidth(hud_fssmall, tempstr.constPtr()),
                   y + 16, &vbscreen);
}

#define RJUSTIFY(font, qstr, maxlen, x)  (x + \
                                          ((V_FontMaxWidth(font) * maxlen) - \
                                          (V_FontStringWidth(font, qstr.constPtr()))))

void ModernHUD::DrawHealth(int x, int y)
{
   static patch_t *nfs_health  = PatchLoader::CacheName(wGlobalDir, "nhud_hlt", PU_STATIC);
   static patch_t *nfs_divider = PatchLoader::CacheName(wGlobalDir, "nhud_div", PU_STATIC);
   qstring         tempstr;

   V_DrawPatch(x, y, &vbscreen, nfs_health);
   tempstr << HU_HealthColor() << hu_player.health;
   V_FontWriteText(hud_fslarge, tempstr.constPtr(),
                   RJUSTIFY(hud_fslarge, tempstr, 3, x + 4 + nfs_health->width),
                   y, &vbscreen);
   V_DrawPatch(x + 37, y + 2, &vbscreen, nfs_divider);
}

//
// Very similar to DrawHealth
//
void ModernHUD::DrawArmor(int x, int y)
{
   static patch_t *nfs_armor = PatchLoader::CacheName(wGlobalDir, "nhud_amr", PU_STATIC);
   qstring         tempstr;

   V_DrawPatch(x, y, &vbscreen, nfs_armor);
   tempstr << HU_ArmorColor() << hu_player.armorpoints;
   V_FontWriteText(hud_fslarge, tempstr.constPtr(),
                   RJUSTIFY(hud_fslarge, tempstr, 3, x + 4 + nfs_armor->width),
                   y, &vbscreen);
}

//
// Weapons List
//
void ModernHUD::DrawWeapons(int x, int y)
{
   qstring tempstr("ARMS ");
   for(int i = 0; i < NUMWEAPONS; i++)
   {
      char fontcolor;
      if(E_PlayerOwnsWeaponForDEHNum(&hu_player, i))
      {
         const weaponinfo_t *weapon = E_WeaponForDEHNum(i);
         fontcolor = weapon->ammo ? HU_WeapColor(E_WeaponForDEHNum(i)) : *FC_GRAY;
      }
      else
         fontcolor = *FC_CUSTOM2;

      tempstr << fontcolor << i + 1 << ' ';
   }
   V_FontWriteText(hud_fssmall, tempstr.constPtr(), x, y, &vbscreen);
}

//
// Drawing Ammo
//
void ModernHUD::DrawAmmo(int x, int y)
{
   const int displayoffs = x + V_FontStringWidth(hud_fssmall, "AMMO") + 1;
   V_FontWriteText(hud_fssmall, "AMMO", x, y, &vbscreen);
   if(hu_player.readyweapon->ammo != nullptr)
   {
      qstring    tempstr;
      const int  ammo      = playerammo;
      const int  maxammo   = playermaxammo;
      const char fontcolor = HU_WeapColor(hu_player.readyweapon);
      tempstr << fontcolor << ammo << FC_GRAY " / " << fontcolor << maxammo;
      V_FontWriteText(hud_fsmedium, tempstr.constPtr(), displayoffs, y, &vbscreen);
   }
}

extern patch_t *keys[NUMCARDS + 3];

//
// Draw the keys
//
void ModernHUD::DrawKeys(int x, int y)
{
   const int displayoffs = x + V_FontStringWidth(hud_fssmall, "KEYS") + 1;
   V_FontWriteText(hud_fssmall, "KEYS", x, y, &vbscreen);
   for(int i = 0, xoffs = displayoffs; i < GameModeInfo->numHUDKeys; i++)
   {
      if(E_GetItemOwnedAmountName(&hu_player, GameModeInfo->cardNames[i]) > 0)
      {
         // got that key
         V_DrawPatch(xoffs, y, &vbscreen, keys[i]);
         xoffs += 8;
      }
   }
}

//
// Draw the Frags
//
void ModernHUD::DrawFrags(int x, int y)
{
}

//
// Based on old HU_overlaySetup, but with different values
//
void ModernHUD::Setup()
{
   int x, y;

   // decide where to put all the widgets

   for(unsigned int i = 0; i < NUMOVERLAY; i++)
      SetOverlayEnabled(static_cast<overlay_e>(i), true); // turn em all on

  // turn off status if we aren't using it
   if(hud_hidestatus)
      SetOverlayEnabled(ol_status, false);

   // turn off frag counter or key display,
   // according to if we're in a deathmatch game or not
   if(GameType == gt_dm)
      SetOverlayEnabled(ol_key, false);
   else
      SetOverlayEnabled(ol_frag, false);

   // now build according to style

   switch(hud_overlaylayout)
   {
   case HUD_OFF:       // 'off'
   case HUD_GRAPHICAL: // 'graphical' -- haleyjd 01/11/05: this is handled by status bar
      for(unsigned int i = 0; i < NUMOVERLAY; i++)
         SetOverlayEnabled(static_cast<overlay_e>(i), false);
      break;

   case HUD_BOOM: // 'bottom left' / 'BOOM' style
      y = SCREENHEIGHT - 8;

      for(int i = NUMOVERLAY - 1; i >= 0; --i)
      {
         if(GetOverlayEnabled(static_cast<overlay_e>(i)))
         {
            SetupOverlay(static_cast<overlay_e>(i), 0, y);
            y -= 8;
         }
      }
      break;

   case HUD_FLAT: // all at bottom of screen

      SetupOverlay(ol_status, SCREENWIDTH - 3, SCREENHEIGHT - 24);
      SetupOverlay(ol_health, 3,  SCREENHEIGHT - 24);
      SetupOverlay(ol_armor,  3,  SCREENHEIGHT - 12);
      SetupOverlay(ol_weap,   44, SCREENHEIGHT - 24);
      SetupOverlay(ol_ammo,   44, SCREENHEIGHT - 16);
      SetupOverlay(ol_key,    44, SCREENHEIGHT - 8);
      break;

   case HUD_DISTRIB: // similar to boom 'distributed' style
      SetupOverlay(ol_health, SCREENWIDTH - 138, 0);
      SetupOverlay(ol_armor,  SCREENWIDTH - 138, 8);
      SetupOverlay(ol_weap,   SCREENWIDTH - 138, 184);
      SetupOverlay(ol_ammo,   SCREENWIDTH - 138, 192);

      if(GameType == gt_dm)  // if dm, put frags in place of keys
         SetupOverlay(ol_frag, 0, 192);
      else
         SetupOverlay(ol_key, 0, 192);

      if(!hud_hidestatus)
         SetupOverlay(ol_status, 0, 184);
      break;

   default:
      break;
   }
}



ModernHUD modern_overlay;

// EOF

