// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
//
// Heads up 'overlay' for fullscreen
//
// Rewritten and put in a seperate module(seems sensible)
//
// By Simon Howard
//
//----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "c_runcmd.h"
#include "d_deh.h"
#include "d_event.h"
#include "d_gi.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_fonts.h"
#include "e_inventory.h"
#include "e_weapons.h"
#include "g_game.h"
#include "hu_boom.h"
#include "hu_frags.h"
#include "hu_over.h"
#include "hu_stuff.h"
#include "m_qstr.h"
#include "p_info.h"
#include "p_map.h"
#include "p_setup.h"
#include "r_draw.h"
#include "r_patch.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"

//=============================================================================
//
// HUD Overlay Object Pointer
//

HUDOverlay *hu_overlay = nullptr;

//=============================================================================
//
// HUD Overlay Objects
//

class ModernHUD : public HUDOverlay
{
protected:
   virtual void DrawStatus (int x, int y);
   virtual void DrawHealth (int x, int y);
   virtual void DrawArmor  (int x, int y);
   virtual void DrawWeapons(int x, int y);
   virtual void DrawAmmo   (int x, int y);
   virtual void DrawKeys   (int x, int y);
   virtual void DrawFrags  (int x, int y);
} static modern_overlay;

//=============================================================================
//
// HUD Overlay Table
//

// Config variable to select HUD overlay
int hud_overlayid = -1;

struct hudoverlayitem_t
{
   int id;              // unique ID #
   const char *name;    // descriptive name of this HUD
   HUDOverlay *overlay; // overlay object
};

// Driver table
static hudoverlayitem_t hudOverlayItemTable[HUO_MAXOVERLAYS] =
{
   // Modern HUD Overlay
   {
      HUO_MODERN,
      "Modern Overlay",
      &modern_overlay
   },

   // BOOM HUD Overlay
   {
      HUO_BOOM,
      "BOOM Overlay",
      &boom_overlay
   }
};

//
// Find the currently selected video driver by ID
//
static hudoverlayitem_t *I_findHUDOverlayByID(int id)
{
   for(unsigned int i = 0; i < HUO_MAXOVERLAYS; i++)
   {
      if(hudOverlayItemTable[i].id == id && hudOverlayItemTable[i].overlay)
         return &hudOverlayItemTable[i];
   }

   return nullptr;
}

//
// Chooses the default video driver based on user specifications, or on preset
// platform-specific defaults when there is no user specification.
//
static hudoverlayitem_t *I_defaultHUDOverlay()
{
   hudoverlayitem_t *item;

   if(!(item = I_findHUDOverlayByID(hud_overlayid)))
   {
      // Default or plain invalid setting.
      // Find the lowest-numbered valid driver and use it.
      for(unsigned int i = 0; i < HUO_MAXOVERLAYS; i++)
      {
         if(hudOverlayItemTable[i].overlay)
         {
            item = &hudOverlayItemTable[i];
            break;
         }
      }
   }

   return item;
}

//=============================================================================
//
// Internal Defines
//

// The HUD always displays information on the displayplayer
#define hu_player (players[displayplayer])

// Get the player's ammo for the given weapon, or 0 if am_noammo
int HU_WC_PlayerAmmo(weaponinfo_t *w)
{
   return E_GetItemOwnedAmount(&hu_player, w->ammo);
}

// Determine if the player has enough ammo for one shot with the given weapon
bool HU_WC_NoAmmo(weaponinfo_t *w)
{
   bool outofammo = false;
   itemeffect_t *ammo = w->ammo;

   // no-ammo weapons are always considered to have ammo
   if(ammo)
   {
      int amount = E_GetItemOwnedAmount(&hu_player, ammo);
      if(amount)
         outofammo = (amount < w->ammopershot);
   }

   return outofammo;
}

// Get the player's maxammo for the given weapon, or 0 if am_noammo
int HU_WC_MaxAmmo(weaponinfo_t *w)
{
   int amount = 0;
   itemeffect_t *ammo = w->ammo;

   if(ammo)
      amount = E_GetMaxAmountForArtifact(&hu_player, ammo);

   return amount;
}

// Determine the color to use for the given weapon's number and ammo bar/count
char HU_WeapColor(weaponinfo_t *w)
{
   int  maxammo = HU_WC_MaxAmmo(w);
   bool noammo  = HU_WC_NoAmmo(w);
   int  pammo   = HU_WC_PlayerAmmo(w);

   return
      (!maxammo                                ? *FC_GRAY    :
       noammo                                  ? *FC_CUSTOM1 :
       pammo == maxammo                        ? *FC_BLUE    :
       pammo < ((maxammo * ammo_red   ) / 100) ? *FC_RED     :
       pammo < ((maxammo * ammo_yellow) / 100) ? *FC_GOLD    :
                                                 *FC_GREEN);
}

// Determine the color to use for a given player's health
char HU_HealthColor()
{
   return hu_player.health  < health_red    ? *FC_RED   :
          hu_player.health  < health_yellow ? *FC_GOLD  :
          hu_player.health <= health_green  ? *FC_GREEN :
                                              *FC_BLUE;
}

// Determine the color to use for a given player's armor
char HU_ArmorColor()
{
   if(hu_player.armorpoints < armor_red)
      return *FC_RED;
   else if(hu_player.armorpoints < armor_yellow)
      return *FC_GOLD;
   else if(armor_byclass)
   {
      fixed_t armorclass = 0;
      if(hu_player.armordivisor)
         armorclass = (hu_player.armorfactor * FRACUNIT) / hu_player.armordivisor;
      return (armorclass > FRACUNIT / 3 ? *FC_BLUE : *FC_GREEN);
   }
   else if(hu_player.armorpoints <= armor_green)
      return *FC_GREEN;
   else
      return *FC_BLUE;
}

// Globals
int hud_overlaylayout = 1;
int hud_enabled      = 1;
int hud_hidestatus   = 0;

//=============================================================================
//
// Heads Up Font
//

// note to programmers from other ports: hu_font is the heads up font
// *not* the general doom font as it is in the original sources and
// most ports

// haleyjd 02/25/09: hud font set by EDF:
char    *hud_overfontname;
vfont_t *hud_overfont;
bool     hud_fontsloaded = false;

//
// HU_LoadFonts
//
// Loads the heads-up font. The naming scheme for the lumps is
// not very consistent, so this is relatively complicated.
//
void HU_LoadFonts()
{
   if(!(hud_overfont = E_FontForName(hud_overfontname)))
      I_Error("HU_LoadFonts: bad EDF hu_font name %s\n", hud_overfontname);

   hud_fontsloaded = true;
}

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
      const int  ammo      = HU_WC_PlayerAmmo(hu_player.readyweapon);
      const int  maxammo   = HU_WC_MaxAmmo(hu_player.readyweapon);
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

//
// HU_overlaySetup
//
static void HU_overlaySetup()
{
   int x, y;

   if(!hu_overlay)
   {
      hudoverlayitem_t *item = I_defaultHUDOverlay();
      hud_overlayid = item->id;
      hu_overlay = item->overlay;
   }

   // decide where to put all the widgets

   for(unsigned int i = 0; i < NUMOVERLAY; i++)
      hu_overlay->SetOverlayEnabled(static_cast<overlay_e>(i), true); // turn em all on

   // turn off status if we aren't using it
   if(hud_hidestatus)
      hu_overlay->SetOverlayEnabled(ol_status, false);

   // turn off frag counter or key display,
   // according to if we're in a deathmatch game or not
   if(GameType == gt_dm)
      hu_overlay->SetOverlayEnabled(ol_key, false);
   else
      hu_overlay->SetOverlayEnabled(ol_frag, false);

   // now build according to style

   switch(hud_overlaylayout)
   {
   case HUD_OFF:       // 'off'
   case HUD_GRAPHICAL: // 'graphical' -- haleyjd 01/11/05: this is handled by status bar
      for(unsigned int i = 0; i < NUMOVERLAY; i++)
         hu_overlay->SetOverlayEnabled(static_cast<overlay_e>(i), false);
      break;

   case HUD_BOOM: // 'bottom left' / 'BOOM' style
      y = SCREENHEIGHT - 8;

      for(int i = NUMOVERLAY - 1; i >= 0; --i)
      {
         if(hu_overlay->GetOverlayEnabled(static_cast<overlay_e>(i)))
         {
            hu_overlay->SetupOverlay(static_cast<overlay_e>(i), 0, y);
            y -= 8;
         }
      }
      break;

   case HUD_FLAT: // all at bottom of screen
      x = 160;
      y = SCREENHEIGHT - 8;

      // haleyjd 06/14/06: rewrote to restore a sensible ordering
      for(int i = NUMOVERLAY - 1; i >= 0; --i)
      {
         if(hu_overlay->GetOverlayEnabled(static_cast<overlay_e>(i)))
         {
            hu_overlay->SetupOverlay(static_cast<overlay_e>(i), x, y);
            y -= 8;
         }
         if(i == ol_weap)
         {
            x = 0;
            y = SCREENHEIGHT - 8;
         }
      }
      break;

   case HUD_DISTRIB: // similar to boom 'distributed' style
      hu_overlay->SetupOverlay(ol_health, SCREENWIDTH-138,   0);
      hu_overlay->SetupOverlay(ol_armor,  SCREENWIDTH-138,   8);
      hu_overlay->SetupOverlay(ol_weap,   SCREENWIDTH-138, 184);
      hu_overlay->SetupOverlay(ol_ammo,   SCREENWIDTH-138, 192);

      if(GameType == gt_dm)  // if dm, put frags in place of keys
         hu_overlay->SetupOverlay(ol_frag, 0, 192);
      else
         hu_overlay->SetupOverlay(ol_key, 0, 192);

      if(!hud_hidestatus)
         hu_overlay->SetupOverlay(ol_status, 0, 184);
      break;

   default:
      break;
   }
}

//=============================================================================
//
// Interface
//

//
// HU_ToggleHUD
//
// Called from CONSOLE_COMMAND(screensize)
//
void HU_ToggleHUD()
{
   hud_enabled = !hud_enabled;
}

//
// HU_DisableHUD
//
// haleyjd: Called from CONSOLE_COMMAND(screensize). Added since SMMU; is 
// required to properly support changing between fullscreen/status bar/HUD.
//
void HU_DisableHUD()
{
   hud_enabled = false;
}


//=============================================================================
//
// Heart of the overlay really.
// Draw the overlay, deciding which bits to draw and where.
//

//
// HU_OverlayDraw
//
void HU_OverlayDraw()
{
   // SoM 2-4-04: ANYRES
   if(viewwindow.height != video.height || automapactive || !hud_enabled)
      return;  // fullscreen only

   HU_overlaySetup();

   for(unsigned int i = 0; i < NUMOVERLAY; i++)
      hu_overlay->DrawOverlay(static_cast<overlay_e>(i));
}

static const char *hud_overlaynames[] =
{
   "default",
   "Modern HUD",
   "Boom HUD",
};

VARIABLE_INT(hud_overlayid, NULL, -1, HUO_MAXOVERLAYS - 1, hud_overlaynames);
CONSOLE_VARIABLE(hu_overlayid, hud_overlayid, 0)
{
   hudoverlayitem_t *item = I_defaultHUDOverlay();
   hud_overlayid = item->id;
   hu_overlay = item->overlay;
}


// HUD type names
const char *str_style[HUD_NUMHUDS] =
{
   "off",
   "boom style",
   "flat",
   "distributed",
   "graphical",   // haleyjd 01/11/05
};

VARIABLE_INT(hud_overlaylayout, NULL, HUD_OFF, HUD_GRAPHICAL, str_style);
CONSOLE_VARIABLE(hu_overlaystyle, hud_overlaylayout, 0) {}

VARIABLE_BOOLEAN(hud_hidestatus, NULL, yesno);
CONSOLE_VARIABLE(hu_hidesecrets, hud_hidestatus, 0) {}

// EOF
