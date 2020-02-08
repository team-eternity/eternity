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
#include "doomstat.h"
#include "e_fonts.h"
#include "e_inventory.h"
#include "g_game.h"
#include "hu_boom.h"
#include "hu_modern.h"
#include "hu_over.h"
#include "p_setup.h"
#include "r_draw.h"
#include "st_stuff.h"
#include "v_font.h"
#include "v_misc.h"

//=============================================================================
//
// HUD Overlay Object Pointer
//

HUDOverlay *hu_overlay = nullptr;

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
int HU_WC_PlayerAmmo(const weaponinfo_t *w)
{
   return E_GetItemOwnedAmount(&hu_player, w->ammo);
}

// Determine if the player has enough ammo for one shot with the given weapon
bool HU_WC_NoAmmo(const weaponinfo_t *w)
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
int HU_WC_MaxAmmo(const weaponinfo_t *w)
{
   int amount = 0;
   itemeffect_t *ammo = w->ammo;

   if(ammo)
      amount = E_GetMaxAmountForArtifact(&hu_player, ammo);

   return amount;
}

// Determine the color to use for the given weapon's number and ammo bar/count
char HU_WeapColor(const weaponinfo_t *w)
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
int hud_overlaylayout = HUD_BOOM;
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
char    *hud_fssmallname;
char    *hud_fsmediumname;
char    *hud_fslargename;
vfont_t *hud_overfont;
vfont_t *hud_fssmall;
vfont_t *hud_fsmedium;
vfont_t *hud_fslarge;
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
   if(!(hud_fssmall = E_FontForName(hud_fssmallname)))
      I_Error("HU_LoadFonts: bad EDF hu_font name %s\n", hud_fssmallname);
   if(!(hud_fsmedium = E_FontForName(hud_fsmediumname)))
      I_Error("HU_LoadFonts: bad EDF hu_font name %s\n", hud_fsmediumname);
   if(!(hud_fslarge = E_FontForName(hud_fslargename)))
      I_Error("HU_LoadFonts: bad EDF hu_font name %s\n", hud_fslargename);

   hud_fontsloaded = true;
}

//
// HU_StringWidth
//
// Calculates the width in pixels of a string in heads-up font
// haleyjd 01/14/05: now uses vfont engine
//
int HU_StringWidth(const char *s)
{
   return V_FontStringWidth(hud_overfont, s);
}

//
// HU_StringHeight
//
// Calculates the height in pixels of a string in heads-up font
// haleyjd 01/14/05: now uses vfont engine
//
int HU_StringHeight(const char *s)
{
   return V_FontStringHeight(hud_overfont, s);
}

//
// HU_overlaySetup
//
static inline void HU_overlaySetup()
{
   if(!hu_overlay)
   {
      hudoverlayitem_t *item = I_defaultHUDOverlay();
      hud_overlayid = item->id;
      hu_overlay = item->overlay;
   }

   hu_overlay->Setup();
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
   if(hud_overlayid < -1 || hud_overlayid > HUO_MAXOVERLAYS - 1)
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
