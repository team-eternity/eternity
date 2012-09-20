// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

// internal for other defines:

#define hu_player (players[displayplayer])

#define wc_pammo(w) \
   (weaponinfo[w].ammo == am_noammo ? 0 : hu_player.ammo[weaponinfo[w].ammo])

#define wc_noammo(w) \
   (weaponinfo[w].ammo == am_noammo ? 0 : \
    (hu_player.ammo[weaponinfo[w].ammo] < weaponinfo[w].ammopershot))

#define wc_mammo(w) \
   (weaponinfo[w].ammo == am_noammo ? 0 : hu_player.maxammo[weaponinfo[w].ammo])

#define weapcolor(w)                                                \
   (!wc_mammo(w)  ? *FC_GRAY    :                                   \
     wc_noammo(w) ? *FC_CUSTOM1 :                                   \
     wc_pammo(w) < ((wc_mammo(w) * ammo_red)    / 100) ? *FC_RED  : \
     wc_pammo(w) < ((wc_mammo(w) * ammo_yellow) / 100) ? *FC_GOLD : \
     *FC_GREEN )

// the amount of ammo the displayplayer has left 
#define playerammo    wc_pammo(hu_player.readyweapon)

// the maximum amount the player could have for current weapon
#define playermaxammo wc_mammo(hu_player.readyweapon)

// set up an overlay_t

#define setol(o, a, b)    \
   do {                   \
      overlay[o].x = (a); \
      overlay[o].y = (b); \
   } while(0)

#define HUDCOLOUR FC_GRAY

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
void HU_WriteText(const char *s, int x, int y)
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
// HU_TextBar
//
// Create a string containing the text 'bar' which graphically show percentage
// of ammo/health/armor/etc. left.
//
static void HU_TextBar(qstring &s, int pct)
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
// the actual drawer is the heart of the overlay
// code. It is split into individual functions,
// each of which draws a different part.

// the offset of percentage bars from the starting text
#define GAP 40

//
// HU_DrawHealth
//
void HU_DrawHealth(int x, int y)
{
   qstring tempstr;
   int fontcolor;
   
   HU_WriteText(HUDCOLOUR "Health", x, y);
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
   HU_TextBar(tempstr, hu_player.health);
   
   // append the percentage itself
   tempstr << " " << hu_player.health;
   
   // write it
   HU_WriteText(tempstr.constPtr(), x, y);
}


//
// HU_DrawArmor
//
// Very similar to drawhealth.
//
void HU_DrawArmor(int x, int y)
{
   qstring tempstr;
   int fontcolor;

   // title first
   HU_WriteText(HUDCOLOUR "Armor", x, y);
   x += GAP; // leave a gap between name and bar

   // decide on colour
   fontcolor =
      hu_player.armorpoints < armor_red    ? *FC_RED   :
      hu_player.armorpoints < armor_yellow ? *FC_GOLD  :
      hu_player.armorpoints <= armor_green ? *FC_GREEN :
      *FC_BLUE;

   tempstr << static_cast<char>(fontcolor);

   // make the bar
   HU_TextBar(tempstr, hu_player.armorpoints);

   // append the percentage itself
   tempstr << " " << hu_player.armorpoints;

   HU_WriteText(tempstr.constPtr(), x, y);
}

//
// HU_DrawAmmo
//
// Drawing Ammo
//
void HU_DrawAmmo(int x, int y)
{
   qstring tempstr;
   int fontcolor;
   int lmaxammo;
   
   HU_WriteText(HUDCOLOUR "Ammo", x, y);
   x += GAP;
   
   fontcolor = weapcolor(hu_player.readyweapon);
   
   tempstr << static_cast<char>(fontcolor);
   
   lmaxammo = playermaxammo;

   if(lmaxammo)
   {
      HU_TextBar(tempstr, (100 * playerammo) / lmaxammo);
      tempstr << " " << playerammo << "/" << playermaxammo;
   }
   else // fist or chainsaw
      tempstr << "N/A";
   
   HU_WriteText(tempstr.constPtr(), x, y);
}

//
// HU_DrawWeapons
// 
// Weapons List
//
void HU_DrawWeapons(int x, int y)
{
   qstring tempstr;
   int fontcolor;
   
   HU_WriteText(HUDCOLOUR "Weapons", x, y);  // draw then leave a gap
   x += GAP;
  
   for(int i = 0; i < NUMWEAPONS; i++)
   {
      if(hu_player.weaponowned[i])
      {
         // got it
         fontcolor = weapcolor(i);
         tempstr << static_cast<char>(fontcolor) << (i + 1) << ' ';
      }
   }
   
   HU_WriteText(tempstr.constPtr(), x, y);  // draw it
}

extern patch_t *keys[NUMCARDS+3];

//
// HU_DrawKeys
//
// Draw the keys
//
void HU_DrawKeys(int x, int y)
{
   HU_WriteText(HUDCOLOUR "Keys", x, y);    // draw then leave a gap
   x += GAP;
   
   // haleyjd 10/09/05: don't show double keys in Heretic
   for(int i = 0; i < GameModeInfo->numHUDKeys; i++)
   {
      if(hu_player.cards[i])
      {
         // got that key
         V_DrawPatch(x, y, &subscreen43, keys[i]);
         x += 11;
      }
   }
}

//
// HU_DrawFrag
//
// Draw the Frags
//
void HU_DrawFrag(int x, int y)
{
   qstring tempstr;

   HU_WriteText(HUDCOLOUR "Frags", x, y); // draw then leave a gap
   x += GAP;

   tempstr << HUDCOLOUR << hu_player.totalfrags;
   HU_WriteText(tempstr.constPtr(), x, y);
}

//
// HU_DrawStatus
//
// Draw the status (number of kills etc)
//
void HU_DrawStatus(int x, int y)
{
   qstring tempstr;
   
   HU_WriteText(HUDCOLOUR "Status", x, y); // draw, leave a gap
   x += GAP;
  
   // haleyjd 06/14/06: restored original colors to K/I/S
   tempstr 
      << FC_RED  "K " FC_GREEN << hu_player.killcount   << "/" << totalkills << " " 
      << FC_BLUE "I " FC_GREEN << hu_player.itemcount   << "/" << totalitems << " " 
      << FC_GOLD "S " FC_GREEN << hu_player.secretcount << "/" << totalsecret;
  
   HU_WriteText(tempstr.constPtr(), x, y);
}

// all overlay modules
overlay_t overlay[NUMOVERLAY];

void HU_ToggleHUD(void)
{
   hud_enabled = !hud_enabled;
}

void HU_DisableHUD(void)
{
   hud_enabled = false;
}

void HU_OverlaySetup(void)
{
   int i;
   
   // setup the drawers
   overlay[ol_health].drawer = HU_DrawHealth;
   overlay[ol_ammo].drawer   = HU_DrawAmmo;
   overlay[ol_weap].drawer   = HU_DrawWeapons;
   overlay[ol_armor].drawer  = HU_DrawArmor;
   overlay[ol_key].drawer    = HU_DrawKeys;
   overlay[ol_frag].drawer   = HU_DrawFrag;
   overlay[ol_status].drawer = HU_DrawStatus;

   // now decide where to put all the widgets
   
   for(i = 0; i < NUMOVERLAY; ++i)
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
   case 0: // 0: 'off'
   case 4: // 4: 'graphical' -- haleyjd 01/11/05: this is handled by status bar
      for(i = 0; i < NUMOVERLAY; ++i)         
         setol(i, -1, -1); // turn it off
      break;
      
   case 1: // 1:'bottom left' style
      {
         int y = SCREENHEIGHT - 8;
         
         for(i = NUMOVERLAY - 1; i >= 0; --i)
         {
            if(overlay[i].x != -1)
            {
               setol(i, 0, y);
               y -= 8;
            }
         }
      }
      break;
      
   case 2: // 2: all at bottom of screen
      {
         int x = 160, y = SCREENHEIGHT - 8;
         
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
      }
      break;

   case 3: // 3: similar to boom 'distributed' style
      setol(ol_health, SCREENWIDTH-138, 0);
      setol(ol_armor, SCREENWIDTH-138, 8);
      setol(ol_weap, SCREENWIDTH-138, 184);
      setol(ol_ammo, SCREENWIDTH-138, 192);
      
      if(GameType == gt_dm)  // if dm, put frags in place of keys
         setol(ol_frag, 0, 192);
      else
         setol(ol_key, 0, 192);

      if(!hud_hidestatus)
         setol(ol_status, 0, 184);
      break;
   }
}

//=============================================================================
//
// heart of the overlay really.
// draw the overlay, deciding which bits to draw and where

//
// HU_OverlayDraw
//
void HU_OverlayDraw(void)
{
   int i;
   
   // SoM 2-4-04: ANYRES
   if(viewheight != video.height || automapactive || !hud_enabled) 
      return;  // fullscreen only
  
   HU_OverlaySetup();
   
   for(i = 0; i < NUMOVERLAY; ++i)
   {
      if(overlay[i].x != -1)
         overlay[i].drawer(overlay[i].x, overlay[i].y);
   }
}

const char *str_style[] =
{
   "off",
   "boom style",
   "flat",
   "distributed",
   "graphical",   // haleyjd 01/11/05
};

VARIABLE_INT(hud_overlaystyle,  NULL,   0, 4,    str_style);
CONSOLE_VARIABLE(hu_overlay, hud_overlaystyle, 0) {}

VARIABLE_BOOLEAN(hud_hidestatus, NULL, yesno);
CONSOLE_VARIABLE(hu_hidesecrets, hud_hidestatus, 0) {}

void HU_OverAddCommands()
{
   C_AddCommand(hu_overlay);
   C_AddCommand(hu_hidesecrets);
}

// EOF
