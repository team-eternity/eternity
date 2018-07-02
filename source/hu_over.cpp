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
#include "hu_frags.h"
#include "hu_over.h"
#include "hu_stuff.h"
#include "m_qstr.h"
#include "p_info.h"
#include "p_map.h"
#include "p_setup.h"
#include "r_draw.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

//=============================================================================
//
// Statics
//

// Overlay drawer callback prototype
typedef void (*OverlayDrawer)(int, int);

// overlay structure
struct overlay_t
{
   int x, y;
   OverlayDrawer drawer;
};

// overlay enumeration
enum
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

static void HU_drawStatus (int x, int y);
static void HU_drawHealth (int x, int y);
static void HU_drawArmor  (int x, int y);
static void HU_drawWeapons(int x, int y);
static void HU_drawAmmo   (int x, int y);
static void HU_drawKeys   (int x, int y);
static void HU_drawFrags  (int x, int y);

// all overlay modules
static overlay_t overlay[NUMOVERLAY] =
{
   { 0, 0, HU_drawStatus  }, // ol_status
   { 0, 0, HU_drawHealth  }, // ol_health
   { 0, 0, HU_drawArmor   }, // ol_armor
   { 0, 0, HU_drawWeapons }, // ol_weap
   { 0, 0, HU_drawAmmo    }, // ol_ammo
   { 0, 0, HU_drawKeys    }, // ol_key
   { 0, 0, HU_drawFrags   }, // ol_frag
};

// HUD styles
enum
{
   HUD_OFF,
   HUD_BOOM,
   HUD_FLAT,
   HUD_DISTRIB,
   HUD_GRAPHICAL,
   HUD_NUMHUDS
};

//=============================================================================
//
// Internal Defines
//

// The HUD always displays information on the displayplayer
#define hu_player (players[displayplayer])

// Get the player's ammo for the given weapon, or 0 if am_noammo
static int wc_pammo(weaponinfo_t *w)
{
   return E_GetItemOwnedAmount(&hu_player, w->ammo);
}

// Determine if the player has enough ammo for one shot with the given weapon
static bool wc_noammo(weaponinfo_t *w)
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
static int wc_mammo(weaponinfo_t *w)
{
   int amount = 0;
   itemeffect_t *ammo = w->ammo;

   if(ammo)
      amount = E_GetMaxAmountForArtifact(&hu_player, ammo);

   return amount;
}

// Determine the color to use for the given weapon's number and ammo bar/count
static char weapcolor(weaponinfo_t *w)
{
   int  maxammo = wc_mammo(w);
   bool noammo  = wc_noammo(w);
   int  pammo   = wc_pammo(w);

   return
      (!maxammo ? *FC_GRAY :
       noammo   ? *FC_CUSTOM1 :
       pammo < ((maxammo * ammo_red   ) / 100) ? *FC_RED  :
       pammo < ((maxammo * ammo_yellow) / 100) ? *FC_GOLD :
       *FC_GREEN);
}

// Get the amount of ammo the displayplayer has left in his/her readyweapon
#define playerammo    wc_pammo(hu_player.readyweapon)

// Get the maximum amount the player could have for his/her readyweapon
#define playermaxammo wc_mammo(hu_player.readyweapon)

//
// setol
//
// Set up an overlay_t
//
static inline void setol(int o, int x, int y)
{
   overlay[o].x = x;
   overlay[o].y = y;
}

// Color of neutral informational components
#define HUDCOLOR FC_GRAY

// Globals
int hud_overlaystyle = 1;
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
static bool hu_fontloaded = false;

//
// HU_LoadFont
//
// Loads the heads-up font. The naming scheme for the lumps is
// not very consistent, so this is relatively complicated.
//
void HU_LoadFont()
{
   if(!(hud_overfont = E_FontForName(hud_overfontname)))
      I_Error("HU_LoadFont: bad EDF hu_font name %s\n", hud_overfontname);

   hu_fontloaded = true;
}

//
// HU_WriteText
//
// sf: write a text line to x, y
// haleyjd 01/14/05: now uses vfont engine
//
static void HU_WriteText(const char *s, int x, int y)
{
   if(hu_fontloaded)
      V_FontWriteText(hud_overfont, s, x, y, &subscreen43);
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

#define BARSIZE 15

//
// HU_textBar
//
// Create a string containing the text 'bar' which graphically show percentage
// of ammo/health/armor/etc. left.
//
static void HU_textBar(qstring &s, int pct)
{
   if(pct > 100)
      pct = 100;
  
   // build the string, decide how many blocks
   while(pct)
   {
      int addchar = 0;
      
      if(pct >= BARSIZE)
      {
         addchar = 123;  // full pct: 4 blocks
         pct -= BARSIZE;
      }
      else
      {
         addchar = 127 - (pct * 5) / BARSIZE;
         pct = 0;
      }
      s << static_cast<char>(addchar);
   }
}

//=============================================================================
//
// Drawer
//
// The actual drawer is the heart of the overlay code. It is split into 
// individual functions, each of which draws a different part.
//

// The offset of percentage bars from the starting text
#define GAP 40

//
// HU_drawHealth
//
static void HU_drawHealth(int x, int y)
{
   qstring tempstr;
   int fontcolor;
   
   HU_WriteText(HUDCOLOR "Health", x, y);
   x += GAP; // leave a gap between name and bar
   
   // decide on the colour first
   fontcolor =
      hu_player.health < health_red    ? *FC_RED   :
      hu_player.health < health_yellow ? *FC_GOLD  :
      hu_player.health <= health_green ? *FC_GREEN :
      *FC_BLUE;
  
   //psnprintf(tempstr, sizeof(tempstr), "%c", fontcolor);
   tempstr << static_cast<char>(fontcolor);

   // now make the actual bar
   HU_textBar(tempstr, hu_player.health);
   
   // append the percentage itself
   tempstr << " " << hu_player.health;
   
   // write it
   HU_WriteText(tempstr.constPtr(), x, y);
}


//
// HU_drawArmor
//
// Very similar to drawhealth.
//
static void HU_drawArmor(int x, int y)
{
   qstring tempstr;
   int fontcolor;

   // title first
   HU_WriteText(HUDCOLOR "Armor", x, y);
   x += GAP; // leave a gap between name and bar

   // decide on colour
   if(hu_player.armorpoints < armor_red)
      fontcolor = *FC_RED;
   else if(hu_player.armorpoints < armor_yellow)
      fontcolor = *FC_GOLD;
   else if(armor_byclass)
   {
      fixed_t armorclass = 0;
      if(hu_player.armordivisor)
         armorclass = (hu_player.armorfactor * FRACUNIT) / hu_player.armordivisor;
      fontcolor = (armorclass > FRACUNIT/3 ? *FC_BLUE : *FC_GREEN);
   }
   else if(hu_player.armorpoints <= armor_green)
      fontcolor = *FC_GREEN;
   else
      fontcolor = *FC_BLUE;

   tempstr << static_cast<char>(fontcolor);

   // make the bar
   HU_textBar(tempstr, hu_player.armorpoints);

   // append the percentage itself
   tempstr << ' ' << hu_player.armorpoints;

   HU_WriteText(tempstr.constPtr(), x, y);
}

//
// HU_drawAmmo
//
// Drawing Ammo
//
static void HU_drawAmmo(int x, int y)
{
   qstring tempstr;
   int fontcolor;
   int lmaxammo;
   
   HU_WriteText(HUDCOLOR "Ammo", x, y);
   x += GAP;
   
   fontcolor = weapcolor(hu_player.readyweapon);
   
   tempstr << static_cast<char>(fontcolor);
   
   lmaxammo = playermaxammo;

   if(lmaxammo)
   {
      HU_textBar(tempstr, (100 * playerammo) / lmaxammo);
      tempstr << ' ' << playerammo << '/' << playermaxammo;
   }
   else // fist or chainsaw
      tempstr << "N/A";
   
   HU_WriteText(tempstr.constPtr(), x, y);
}

//
// HU_drawWeapons
// 
// Weapons List
//
static void HU_drawWeapons(int x, int y)
{
   qstring tempstr;
   int fontcolor;
   
   HU_WriteText(HUDCOLOR "Weapons", x, y);  // draw then leave a gap
   x += GAP;
  
   for(int i = 0; i < NUMWEAPONS; i++)
   {
      if(E_PlayerOwnsWeaponForDEHNum(&hu_player, i))
      {
         // got it
         fontcolor = weapcolor(E_WeaponForDEHNum(i));
         tempstr << static_cast<char>(fontcolor) << (i + 1) << ' ';
      }
   }
   
   HU_WriteText(tempstr.constPtr(), x, y);  // draw it
}

extern patch_t *keys[NUMCARDS+3];

//
// HU_drawKeys
//
// Draw the keys
//
static void HU_drawKeys(int x, int y)
{
   HU_WriteText(HUDCOLOR "Keys", x, y);    // draw then leave a gap
   x += GAP;
   
   // haleyjd 10/09/05: don't show double keys in Heretic
   for(int i = 0; i < GameModeInfo->numHUDKeys; i++)
   {
      if(E_GetItemOwnedAmountName(&hu_player, GameModeInfo->cardNames[i]) > 0)
      {
         // got that key
         V_DrawPatch(x, y, &subscreen43, keys[i]);
         x += 11;
      }
   }
}

//
// HU_drawFrags
//
// Draw the Frags
//
static void HU_drawFrags(int x, int y)
{
   qstring tempstr;

   HU_WriteText(HUDCOLOR "Frags", x, y); // draw then leave a gap
   x += GAP;

   tempstr << HUDCOLOR << hu_player.totalfrags;
   HU_WriteText(tempstr.constPtr(), x, y);
}

//
// HU_drawStatus
//
// Draw the status (number of kills etc)
//
static void HU_drawStatus(int x, int y)
{
   qstring tempstr;
   
   HU_WriteText(HUDCOLOR "Status", x, y); // draw, leave a gap
   x += GAP;
  
   // haleyjd 06/14/06: restored original colors to K/I/S
   tempstr 
      << FC_RED  "K " FC_GREEN << hu_player.killcount   << '/' << totalkills << ' ' 
      << FC_BLUE "I " FC_GREEN << hu_player.itemcount   << '/' << totalitems << ' ' 
      << FC_GOLD "S " FC_GREEN << hu_player.secretcount << '/' << totalsecret;
  
   HU_WriteText(tempstr.constPtr(), x, y);
}

//
// HU_overlaySetup
//
static void HU_overlaySetup()
{
   int i, x, y;

   // decide where to put all the widgets
   
   for(i = 0; i < NUMOVERLAY; i++)
      overlay[i].x = 1;       // turn em all on

   // turn off status if we aren't using it
   if(hud_hidestatus)
      overlay[ol_status].x = -1;

   // turn off frag counter or key display,
   // according to if we're in a deathmatch game or not
   if(GameType == gt_dm)
      overlay[ol_key].x = -1;
   else
      overlay[ol_frag].x = -1;

   // now build according to style
   
   switch(hud_overlaystyle)
   {      
   case HUD_OFF:       // 'off'
   case HUD_GRAPHICAL: // 'graphical' -- haleyjd 01/11/05: this is handled by status bar
      for(i = 0; i < NUMOVERLAY; i++)         
         setol(i, -1, -1); // turn it off
      break;
      
   case HUD_BOOM: // 'bottom left' / 'BOOM' style
      y = SCREENHEIGHT - 8;

      for(i = NUMOVERLAY - 1; i >= 0; --i)
      {
         if(overlay[i].x != -1)
         {
            setol(i, 0, y);
            y -= 8;
         }
      }
      break;
      
   case HUD_FLAT: // all at bottom of screen
      x = 160; 
      y = SCREENHEIGHT - 8;

      // haleyjd 06/14/06: rewrote to restore a sensible ordering
      for(i = NUMOVERLAY - 1; i >= 0; --i)
      {
         if(overlay[i].x != -1)
         {
            setol(i, x, y);
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
      setol(ol_health, SCREENWIDTH-138,   0);
      setol(ol_armor,  SCREENWIDTH-138,   8);
      setol(ol_weap,   SCREENWIDTH-138, 184);
      setol(ol_ammo,   SCREENWIDTH-138, 192);
      
      if(GameType == gt_dm)  // if dm, put frags in place of keys
         setol(ol_frag, 0, 192);
      else
         setol(ol_key, 0, 192);

      if(!hud_hidestatus)
         setol(ol_status, 0, 184);
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
   
   for(int i = 0; i < NUMOVERLAY; i++)
   {
      if(overlay[i].x != -1)
         overlay[i].drawer(overlay[i].x, overlay[i].y);
   }
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

VARIABLE_INT(hud_overlaystyle, NULL, HUD_OFF, HUD_GRAPHICAL, str_style);
CONSOLE_VARIABLE(hu_overlay, hud_overlaystyle, 0) {}

VARIABLE_BOOLEAN(hud_hidestatus, NULL, yesno);
CONSOLE_VARIABLE(hu_hidesecrets, hud_hidestatus, 0) {}

// EOF
