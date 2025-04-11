//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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

#ifndef HU_OVER_H__
#define HU_OVER_H__

#include "hu_inventory.h"

struct player_t;
struct vfont_t;
struct weaponinfo_t;

// HUD styles
enum hudstyle_e : unsigned int
{
   HUD_OFF,
   HUD_BOOM,
   HUD_FLAT,
   HUD_DISTRIB,
   HUD_GRAPHICAL,
   HUD_NUMHUDS
};

// overlay enumeration
enum overlay_e : unsigned int
{
   ol_status,
   ol_health,
   ol_armor,
   ol_weap,
   ol_ammo,
   ol_key,
   ol_frag,
   ol_invcurr,
   NUMOVERLAY
};

//
// HUD Overlay Base Class
//
// All HUD overlays should inherit this interface.
//
class HUDOverlay
{
protected:
   struct
   {
      int x, y;
      bool enabled;
   } drawerdata[NUMOVERLAY];

   virtual void DrawStatus (int x, int y) = 0;
   virtual void DrawHealth (int x, int y) = 0;
   virtual void DrawArmor  (int x, int y) = 0;
   virtual void DrawWeapons(int x, int y) = 0;
   virtual void DrawAmmo   (int x, int y) = 0;
   virtual void DrawKeys   (int x, int y) = 0;
   virtual void DrawFrags  (int x, int y) = 0;

public:
   virtual void Setup() = 0;

   //
   // Draws the overlay
   //
   inline void DrawOverlay(overlay_e overlay)
   {
      if(!drawerdata[overlay].enabled)
         return;

      // TODO: Structured binding when we move over to C++17 fully
      const int x = drawerdata[overlay].x;
      const int y = drawerdata[overlay].y;

      switch(overlay)
      {
      case ol_status:
         DrawStatus(x, y);
         break;
      case ol_health:
         DrawHealth(x, y);
         break;
      case ol_armor:
         DrawArmor(x, y);
         break;
      case ol_weap:
         DrawWeapons(x, y);
         break;
      case ol_ammo:
         DrawAmmo(x, y);
         break;
      case ol_key:
         DrawKeys(x, y);
         break;
      case ol_frag:
         DrawFrags(x, y);
         break;
      case ol_invcurr:
         HU_InventoryDrawCurrentBox(x, y);
         break;
      default:
         break;
      }
   }

   //
   // Sets up the offsets of a given overlay
   //
   inline void SetupOverlay(overlay_e o, int x, int y)
   {
      drawerdata[o].x = x;
      drawerdata[o].y = y;
   }

   //
   // Default constructor
   //
   HUDOverlay() : drawerdata{}, leftoffset{}, rightoffset{}
   {
   }

   int leftoffset, rightoffset;
};

// Overlays enumeration
enum hudoverlay_e : unsigned int
{
   HUO_MODERN,
   HUO_BOOM,
   HUO_MAXOVERLAYS
};

int  HU_WC_PlayerAmmo(const weaponinfo_t *w);
bool HU_WC_NoAmmo(const weaponinfo_t *w);
int  HU_WC_MaxAmmo(const weaponinfo_t *w);
char HU_WeapColor(const weaponinfo_t *w);
char HU_WeaponColourGeneralized(const player_t &player, int index, bool *had);

char HU_HealthColor();
char HU_ArmorColor();


// heads up font
void HU_LoadFonts();
int  HU_StringWidth(const char *s);
int  HU_StringHeight(const char *s);

extern char    *hud_overfontname;
extern vfont_t *hud_overfont;
extern bool     hud_fontsloaded;

extern char    *hud_fssmallname;
extern char    *hud_fsmediumname;
extern char    *hud_fslargename;
extern vfont_t *hud_fssmall;
extern vfont_t *hud_fsmedium;
extern vfont_t *hud_fslarge;

// overlay interface
void HU_OverlayDraw(int &leftoffset, int &rightoffset);
void HU_ToggleHUD();
void HU_DisableHUD();

extern int hud_overlayid;
extern int hud_overlaylayout;
extern int hud_enabled;
extern int hud_hidestats;
extern int hud_hidesecrets;

inline bool hud_restrictoverlaywidth = true;

#endif

// EOF

