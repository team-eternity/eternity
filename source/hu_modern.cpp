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
#include "d_player.h"
#include "doomstat.h"
#include "e_inventory.h"
#include "e_player.h"
#include "e_weapons.h"
#include "hu_modern.h"
#include "m_qstr.h"
#include "r_patch.h"
#include "v_font.h"
#include "v_misc.h"

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

//
// Writes right-aligned text
//
static inline void FontWriteTextRAlign(vfont_t *font, const char *s,
                                       const int x, const int y, VBuffer *screen)
{
   V_FontWriteText(font, s, x - V_FontStringWidth(font, s), y, &vbscreen);
}

//
// Right-justifies a given string for a given font and maximum length for the string
//
static inline int RJustify(vfont_t *font, const qstring &qstr, const int maxstrlen, const int x)
{
   return x + ((V_FontMaxWidth(font) * maxstrlen) - V_FontStringWidth(font, qstr.constPtr()));
}

//
// Writes right-justified text within a left-aligned line
// Used to properly align health and armour digits regardless of their values
//
static inline void FontWriteTextRJustify(vfont_t *font, const qstring &qstr,
                                         const int maxstrlen, const int x, const int y,
                                         VBuffer *screen)
{
   V_FontWriteText(font, qstr.constPtr(),
                   RJustify(font, qstr, maxstrlen, x), y,
                   &vbscreen);
}

void ModernHUD::DrawStatus(int x, int y)
{
   qstring tempstr;

   if(hud_overlaylayout == HUD_BOOM)
   {
      tempstr << FC_RED   "K" FC_GRAY "  " << hu_player.killcount   << " / " << totalkills <<
                 FC_BLUE " I" FC_GRAY "  " << hu_player.itemcount   << " / " << totalitems <<
                 FC_GOLD " S" FC_GRAY "  " << hu_player.secretcount << " / " << totalsecret;
      V_FontWriteText(hud_fssmall, tempstr.constPtr(), x, y, &vbscreen);
   }
   else if(hud_overlaylayout == HUD_DISTRIB)
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

   if(hud_overlaylayout == HUD_BOOM)
   {
      V_FontWriteText(hud_fssmall, "HLTH ", x, y, &vbscreen);
      x += V_FontStringWidth(hud_fssmall, "HLTH ");
      tempstr << HU_HealthColor() << hu_player.health;
      FontWriteTextRJustify(hud_fsmedium, tempstr, 3, x, y, &vbscreen);
   }
   else if(hud_overlaylayout == HUD_DISTRIB)
   {
      V_DrawPatch(x - nfs_health->width, y, &vbscreen, nfs_health);
      tempstr << HU_HealthColor() << hu_player.health;
      FontWriteTextRAlign(hud_fslarge, tempstr.constPtr(), x - (4 + nfs_health->width), y,
                          &vbscreen);
   }
   else
   {
      V_DrawPatch(x, y, &vbscreen, nfs_health);
      tempstr << HU_HealthColor() << hu_player.health;
      FontWriteTextRJustify(hud_fslarge, tempstr, 3, x + 4 + nfs_health->width, y, &vbscreen);
      if(hud_overlaylayout == HUD_FLAT)
         V_DrawPatch(x + 37, y + 1, &vbscreen, nfs_divider);
   }
}

//
// Very similar to DrawHealth
//
void ModernHUD::DrawArmor(int x, int y)
{
   qstring tempstr;

   if(hud_overlaylayout == HUD_BOOM)
   {
      V_FontWriteText(hud_fssmall, "ARMR ", x, y, &vbscreen);
      x += V_FontStringWidth(hud_fssmall, "ARMR ");
      tempstr << HU_ArmorColor() << hu_player.armorpoints;
      FontWriteTextRJustify(hud_fsmedium, tempstr, 3, x, y, &vbscreen);
   }
   else if(hud_overlaylayout == HUD_DISTRIB)
   {
      V_DrawPatch(x - nfs_armor->width, y, &vbscreen, nfs_armor);
      tempstr << HU_ArmorColor() << hu_player.armorpoints;
      FontWriteTextRAlign(hud_fslarge, tempstr.constPtr(), x - (4 + nfs_armor->width), y,
                          &vbscreen);
   }
   else
   {
      V_DrawPatch(x, y, &vbscreen, nfs_armor);
      tempstr << HU_ArmorColor() << hu_player.armorpoints;
      FontWriteTextRJustify(hud_fslarge, tempstr, 3, x + 4 + nfs_armor->width, y, &vbscreen);
   }
}

// HU_WeapColor tuned to work for a weapon slot
static char HU_weapSlotColor(const int slot)
{
   if(!hu_player.pclass->weaponslots[slot])
      return 0;

   const weaponinfo_t *weapon = nullptr;

   BDListItem<weaponslot_t> *weaponslot = E_FirstInSlot(hu_player.pclass->weaponslots[slot]);
   do
   {
      if(E_PlayerOwnsWeapon(&hu_player, weaponslot->bdObject->weapon))
      {
         if(weapon == nullptr)
            weapon = weaponslot->bdObject->weapon;
         else if(weapon->ammo != weaponslot->bdObject->weapon->ammo)
            return *FC_GRAY; // TODO: Change "more than one ammo type in slot" color
      }
      weaponslot = weaponslot->bdNext;
   } while(!weaponslot->isDummy());

   return weapon->ammo ? HU_WeapColor(weapon) : *FC_GRAY;
}

//
// Weapons List
//
void ModernHUD::DrawWeapons(int x, int y)
{
   qstring tempstr;
   const bool laligned = hud_overlaylayout != HUD_DISTRIB;

   if(laligned)
      tempstr = "ARMS ";

   for(int i = 0; i < NUMWEAPONS; i++)
   {
      char fontcolor;
      if(E_NumWeaponsInSlotPlayerOwns(&hu_player, i) > 1)
         fontcolor = HU_weapSlotColor(i);
      else if(E_PlayerOwnsWeaponForDEHNum(&hu_player, i))
      {
         const weaponinfo_t *weapon = E_WeaponForDEHNum(i);
         fontcolor = weapon->ammo ? HU_WeapColor(E_WeaponForDEHNum(i)) : *FC_GRAY;
      }
      else if(E_PlayerOwnsWeaponInSlot(&hu_player, i))
      {
         const weaponinfo_t *weapon = P_GetPlayerWeapon(&hu_player, i);
         fontcolor = weapon->ammo ? HU_WeapColor(E_WeaponForDEHNum(i)) : *FC_GRAY;
      }
      else
         fontcolor = *FC_CUSTOM2;

      if(laligned)
         tempstr << fontcolor << i + 1 << ' ';
      else
         tempstr << ' ' << fontcolor << i + 1;
   }

   if(laligned)
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
   const bool laligned = hud_overlaylayout != HUD_DISTRIB;
   const int displayoffs = laligned ? x +  V_FontStringWidth(hud_fssmall, "AMMO") + 1 :
                                      x - (V_FontStringWidth(hud_fssmall, "AMMO") + 1);
   if(laligned)
      V_FontWriteText(hud_fssmall, "AMMO", x, y, &vbscreen);
   else
      FontWriteTextRAlign(hud_fssmall, "AMMO", x, y, &vbscreen);

   if(hu_player.readyweapon->ammo != nullptr)
   {
      qstring    tempstr;
      const char fontcolor = HU_WeapColor(hu_player.readyweapon);
      tempstr << fontcolor << playerammo << FC_GRAY " / " << fontcolor << playermaxammo;
      if(laligned)
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

   for(auto &data : drawerdata)
      data.enabled = true; // turn em all on

  // turn off status if we aren't using it
   if(hud_hidestatus)
      drawerdata[ol_status].enabled = false;

   // turn off frag counter or key display,
   // according to if we're in a deathmatch game or not
   if(GameType == gt_dm)
      drawerdata[ol_key].enabled = false;
   else
      drawerdata[ol_frag].enabled = false;

   // now build according to style

   switch(hud_overlaylayout)
   {
   case HUD_OFF:       // 'off'
   case HUD_GRAPHICAL: // 'graphical' -- haleyjd 01/11/05: this is handled by status bar
      for(auto &data : drawerdata)
         data.enabled = false;
      break;

   case HUD_BOOM: // 'bottom left' / 'BOOM' style
      y = SCREENHEIGHT - 8;

      for(int i = NUMOVERLAY - 1; i >= 0; --i)
      {
         if(drawerdata[i].enabled)
         {
            SetupOverlay(static_cast<overlay_e>(i), 3, y);
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
         SetupOverlay(ol_key,  3, SCREENHEIGHT - 8);

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

