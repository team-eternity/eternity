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
//--------------------------------------------------------------------------

#ifndef HU_OVER_H__
#define HU_OVER_H__

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
      default:
         break;
      }
   }

   inline void SetupOverlay(overlay_e o, int x, int y)
   {
      drawerdata[o].x = x;
      drawerdata[o].y = y;
   }

   void SetOverlayEnabled(overlay_e overlay, bool enabled)
   {
      drawerdata[overlay].enabled = enabled;
   }

   bool GetOverlayEnabled(overlay_e overlay) const
   {
      return drawerdata[overlay].enabled;
   }

   HUDOverlay()
   {
      for(auto &data : drawerdata)
         data = { 0, 0, false };
   }
};

// Overlays enumeration
enum hudoverlay_e : unsigned int
{
   HUO_MODERN,
   HUO_BOOM,
   HUO_MAXOVERLAYS
};

int HU_WC_PlayerAmmo(weaponinfo_t *w);
bool HU_WC_NoAmmo(weaponinfo_t *w);
int HU_WC_MaxAmmo(weaponinfo_t *w);
char HU_WeapColor(weaponinfo_t *w);

char HU_HealthColor();
char HU_ArmorColor();


// heads up font
void HU_LoadFont();
int  HU_StringWidth(const char *s);
int  HU_StringHeight(const char *s);

extern char    *hud_overfontname;
extern vfont_t *hud_overfont;

extern char    *hud_fssmallname;
extern char    *hud_fsmediumname;
extern char    *hud_fslargename;
extern vfont_t *hud_fssmall;
extern vfont_t *hud_fsmedium;
extern vfont_t *hud_fslarge;

// overlay interface
void HU_OverlayDraw();
void HU_ToggleHUD();
void HU_DisableHUD();

extern int hud_overlayid;
extern int hud_overlaylayout;
extern int hud_enabled;
extern int hud_hidestatus;

#endif

// EOF

