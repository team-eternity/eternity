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
                   SCREENWIDTH - V_FontStringWidth(hud_fssmall, tempstr.constPtr()) - 3,
                   SCREENHEIGHT - 24, &vbscreen);
   tempstr.clear();
   tempstr << hu_player.itemcount << " / " << totalitems << "  " << FC_BLUE "ITEMS";
   V_FontWriteText(hud_fssmall, tempstr.constPtr(),
                   SCREENWIDTH - V_FontStringWidth(hud_fssmall, tempstr.constPtr()) - 3,
                   SCREENHEIGHT - 16, &vbscreen);
   tempstr.clear();
   tempstr << hu_player.secretcount << " / " << totalsecret << "  " << FC_GOLD "SCRTS";
   V_FontWriteText(hud_fssmall, tempstr.constPtr(),
                   SCREENWIDTH - V_FontStringWidth(hud_fssmall, tempstr.constPtr()) - 3,
                   SCREENHEIGHT - 8, &vbscreen);
}

#define RJUSTIFY(font, qstr, maxlen, x)  (x + \
                                          ((V_FontMaxWidth(font) * maxlen) - \
                                          (V_FontStringWidth(font, qstr.constPtr()))))

void ModernHUD::DrawHealth(int x, int y)
{
   static patch_t *nfs_health = PatchLoader::CacheName(wGlobalDir, "nhud_hlt", PU_STATIC);
   qstring         tempstr;

   V_DrawPatch(3, SCREENHEIGHT - 24, &vbscreen, nfs_health);
   tempstr << HU_HealthColor() << hu_player.health;
   V_FontWriteText(hud_fslarge, tempstr.constPtr(),
                   RJUSTIFY(hud_fslarge, tempstr, 3, 7 + nfs_health->width),
                   SCREENHEIGHT - 24, &vbscreen);
}

void ModernHUD::DrawArmor(int x, int y)
{
   static patch_t *nfs_armor = PatchLoader::CacheName(wGlobalDir, "nhud_amr", PU_STATIC);
   static patch_t *nfs_divider = PatchLoader::CacheName(wGlobalDir, "nhud_div", PU_STATIC);
   qstring         tempstr;

   V_DrawPatch(3, SCREENHEIGHT - 12, &vbscreen, nfs_armor);
   tempstr << HU_ArmorColor() << hu_player.armorpoints;
   V_FontWriteText(hud_fslarge, tempstr.constPtr(),
                   RJUSTIFY(hud_fslarge, tempstr, 3, 7 + nfs_armor->width),
                   SCREENHEIGHT - 12, &vbscreen);
   V_DrawPatch(40, SCREENHEIGHT - 22, &vbscreen, nfs_divider);
}

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
   V_FontWriteText(hud_fssmall, tempstr.constPtr(), 44, SCREENHEIGHT - 24, &vbscreen);
}

void ModernHUD::DrawAmmo(int x, int y)
{
   const int displayoffs = 44 + V_FontStringWidth(hud_fssmall, "AMMO") + 1;
   V_FontWriteText(hud_fssmall, "AMMO", 44, SCREENHEIGHT - 16, &vbscreen);
   if(hu_player.readyweapon->ammo != nullptr)
   {
      qstring    tempstr;
      const int  ammo      = playerammo;
      const int  maxammo   = playermaxammo;
      const char fontcolor = HU_WeapColor(hu_player.readyweapon);
      tempstr << fontcolor << ammo << FC_GRAY " / " << fontcolor << maxammo;
      V_FontWriteText(hud_fsmedium, tempstr.constPtr(), displayoffs, SCREENHEIGHT - 16, &vbscreen);
   }
}

extern patch_t *keys[NUMCARDS + 3];

void ModernHUD::DrawKeys(int x, int y)
{
   const int displayoffs = 44 + V_FontStringWidth(hud_fssmall, "KEYS") + 1;
   V_FontWriteText(hud_fssmall, "KEYS", 44, SCREENHEIGHT - 8, &vbscreen);
   for(int i = 0, x = displayoffs; i < GameModeInfo->numHUDKeys; i++)
   {
      if(E_GetItemOwnedAmountName(&hu_player, GameModeInfo->cardNames[i]) > 0)
      {
         // got that key
         V_DrawPatch(x, SCREENHEIGHT - 8, &vbscreen, keys[i]);
         x += 8;
      }
   }
}

void ModernHUD::DrawFrags(int x, int y)
{
}


ModernHUD modern_overlay;

// EOF

