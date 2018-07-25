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
// External Patches
//
extern patch_t *nfs_health;
extern patch_t *nfs_divider;
extern patch_t *nfs_armor;

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

static inline void FontWriteTextRAlign(vfont_t *font, const char *s,
                                       int x, int y, VBuffer *screen)
{
   V_FontWriteText(font, s, x - V_FontStringWidth(font, s), y, &vbscreen);
}

//
// Right-justifies a given input for a given font and a provided maximum length for the string
//
static inline int RJustify(vfont_t *font, const qstring &qstr, const int maxstrlen, const int x)
{
   return x + ((V_FontMaxWidth(font) * maxstrlen) - V_FontStringWidth(font, qstr.constPtr()));
}

void ModernHUD::DrawStatus(int x, int y)
{
   qstring tempstr;

   if(hud_overlaylayout == HUD_BOOM || hud_overlaylayout == HUD_DISTRIB)
   {
      tempstr << FC_RED "KILLS" FC_GRAY "  " << hu_player.killcount << " / " << totalkills;
      V_FontWriteText(hud_fssmall, tempstr.constPtr(), x, y - 16, &vbscreen);
      tempstr.clear();
      tempstr << FC_BLUE "ITEMS" FC_GRAY "  " << hu_player.itemcount << " / " << totalitems;
      V_FontWriteText(hud_fssmall, tempstr.constPtr(), x, y - 8, &vbscreen);
      tempstr.clear();
      tempstr << FC_GOLD "SCRTS" FC_GRAY "  " << hu_player.secretcount << " / " << totalsecret;
      V_FontWriteText(hud_fssmall, tempstr.constPtr(), x, y, &vbscreen);
   }
   else
   {
      tempstr << hu_player.killcount << " / " << totalkills << "  " FC_RED "KILLS";
      FontWriteTextRAlign(hud_fssmall, tempstr.constPtr(), x, y, &vbscreen);
      tempstr.clear();
      tempstr << hu_player.itemcount << " / " << totalitems << "  " FC_BLUE "ITEMS";
      FontWriteTextRAlign(hud_fssmall, tempstr.constPtr(), x, y + 8, &vbscreen);
      tempstr.clear();
      tempstr << hu_player.secretcount << " / " << totalsecret << "  " FC_GOLD "SCRTS";
      FontWriteTextRAlign(hud_fssmall, tempstr.constPtr(), x, y + 16, &vbscreen);
   }
}

void ModernHUD::DrawHealth(int x, int y)
{
   qstring tempstr;

   if(hud_overlaylayout != HUD_DISTRIB)
   {
      V_DrawPatch(x, y, &vbscreen, nfs_health);
      tempstr << HU_HealthColor() << hu_player.health;
      V_FontWriteText(hud_fslarge, tempstr.constPtr(),
                      RJustify(hud_fslarge, tempstr, 3, x + 4 + nfs_health->width),
                      y, &vbscreen);
      if(hud_overlaylayout == HUD_FLAT)
         V_DrawPatch(x + 37, y + 1, &vbscreen, nfs_divider);
   }
   else
   {
      V_DrawPatch(x - nfs_health->width, y, &vbscreen, nfs_health);
      tempstr << HU_HealthColor() << hu_player.health;
      FontWriteTextRAlign(hud_fslarge, tempstr.constPtr(),
                          x - (4 + nfs_health->width), y, &vbscreen);
   }
}

//
// Very similar to DrawHealth
//
void ModernHUD::DrawArmor(int x, int y)
{
   qstring tempstr;

   if(hud_overlaylayout != HUD_DISTRIB)
   {
      V_DrawPatch(x, y, &vbscreen, nfs_armor);
      tempstr << HU_ArmorColor() << hu_player.armorpoints;
      V_FontWriteText(hud_fslarge, tempstr.constPtr(),
                      RJustify(hud_fslarge, tempstr, 3, x + 4 + nfs_armor->width),
                      y, &vbscreen);
   }
   else
   {
      V_DrawPatch(x - nfs_armor->width, y, &vbscreen, nfs_armor);
      tempstr << HU_ArmorColor() << hu_player.armorpoints;
      FontWriteTextRAlign(hud_fslarge, tempstr.constPtr(),
                          x - (4 + nfs_armor->width), y, &vbscreen);
   }
}

//
// Weapons List
//
void ModernHUD::DrawWeapons(int x, int y)
{
   qstring tempstr;

   if(hud_overlaylayout != HUD_DISTRIB)
      tempstr = "ARMS ";

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

      if(hud_overlaylayout != HUD_DISTRIB)
         tempstr << fontcolor << i + 1 << ' ';
      else
         tempstr << ' ' << fontcolor << i + 1;
   }

   if(hud_overlaylayout != HUD_DISTRIB)
      V_FontWriteText(hud_fssmall, tempstr.constPtr(), x, y, &vbscreen);
   else
   {
      tempstr << FC_GRAY " ARMS";
      FontWriteTextRAlign(hud_fssmall, tempstr.constPtr(), x, y, &vbscreen);
   }
}

//
// Drawing Ammo
//
void ModernHUD::DrawAmmo(int x, int y)
{
   const int displayoffs = hud_overlaylayout != HUD_DISTRIB ?
                              x + V_FontStringWidth(hud_fssmall, "AMMO") + 1 :
                              x - (V_FontStringWidth(hud_fssmall, "AMMO") + 1);
   if(hud_overlaylayout != HUD_DISTRIB)
      V_FontWriteText(hud_fssmall, "AMMO", x, y, &vbscreen);
   else
      FontWriteTextRAlign(hud_fssmall, "AMMO", x, y, &vbscreen);

   if(hu_player.readyweapon->ammo != nullptr)
   {
      qstring    tempstr;
      const int  ammo      = playerammo;
      const int  maxammo   = playermaxammo;
      const char fontcolor = HU_WeapColor(hu_player.readyweapon);
      tempstr << fontcolor << ammo << FC_GRAY " / " << fontcolor << maxammo;
      if(hud_overlaylayout != HUD_DISTRIB)
         V_FontWriteText(hud_fsmedium, tempstr.constPtr(), displayoffs, y, &vbscreen);
      else
         FontWriteTextRAlign(hud_fsmedium, tempstr.constPtr(), displayoffs, y, &vbscreen);
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
   qstring tempstr = qstring(FC_GRAY "FRAG ") << hu_player.totalfrags;
   V_FontWriteText(hud_fssmall, tempstr.constPtr(), x, y); // draw then leave a gap
}

//
// Based on old HU_overlaySetup, but with different values
//
void ModernHUD::Setup()
{
   int y;

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

      for(int i = NUMOVERLAY - 1; i >= ol_weap; --i)
      {
         if(GetOverlayEnabled(static_cast<overlay_e>(i)))
         {
            SetupOverlay(static_cast<overlay_e>(i), 3, y);
            y -= 8;
         }
      }
      SetupOverlay(ol_armor,  3, y - 4);
      SetupOverlay(ol_health, 3, y - 16);
      SetupOverlay(ol_status, 3, y - 24);
      break;

   case HUD_FLAT: // all at bottom of screen
      SetupOverlay(ol_status, SCREENWIDTH - 3, SCREENHEIGHT - 24);
      SetupOverlay(ol_health, 3,  SCREENHEIGHT - 24);
      SetupOverlay(ol_armor,  3,  SCREENHEIGHT - 12);
      SetupOverlay(ol_weap,   44, SCREENHEIGHT - 24);
      SetupOverlay(ol_ammo,   44, SCREENHEIGHT - 16);
      SetupOverlay(ol_key,    44, SCREENHEIGHT - 8);
      SetupOverlay(ol_frag,   44, SCREENHEIGHT - 8);
      break;

   case HUD_DISTRIB: // similar to boom 'distributed' style
      SetupOverlay(ol_health, SCREENWIDTH - 3, 3);
      SetupOverlay(ol_armor,  SCREENWIDTH - 3, 15);
      SetupOverlay(ol_weap,   SCREENWIDTH - 3, SCREENHEIGHT - 16);
      SetupOverlay(ol_ammo,   SCREENWIDTH - 3, SCREENHEIGHT - 8);

      if(GameType == gt_dm)  // if dm, put frags in place of keys
         SetupOverlay(ol_frag, 3, SCREENHEIGHT - 8);
      else
         SetupOverlay(ol_key, 3, SCREENHEIGHT - 8);

      if(!hud_hidestatus)
         SetupOverlay(ol_status, 3, SCREENHEIGHT - 16);
      break;

   default:
      break;
   }
}

// The solitary instance of the modern HUD overlay
ModernHUD modern_overlay;

// EOF

