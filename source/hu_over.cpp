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
//--------------------------------------------------------------------------
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
#include "doomdef.h"
#include "doomstat.h"
#include "c_runcmd.h"
#include "d_deh.h"
#include "d_event.h"
#include "g_game.h"
#include "hu_frags.h"
#include "hu_over.h"
#include "hu_stuff.h"
#include "p_info.h"
#include "p_map.h"
#include "p_setup.h"
#include "r_draw.h"
#include "s_sound.h"
#include "v_video.h"
#include "w_wad.h"
#include "d_gi.h"
#include "v_font.h"
#include "e_fonts.h"


// internal for other defines:

#define _wc_pammo(w)    ( weaponinfo[w].ammo == am_noammo ? 0 : \
                          players[displayplayer].ammo[weaponinfo[w].ammo])
#define _wc_mammo(w)    ( weaponinfo[w].ammo == am_noammo ? 0 : \
                          players[displayplayer].maxammo[weaponinfo[w].ammo])


#define weapcolour(w)   ( !_wc_mammo(w) ? *FC_GRAY :    \
        _wc_pammo(w) < ( (_wc_mammo(w) * ammo_red) / 100) ? *FC_RED :    \
        _wc_pammo(w) < ( (_wc_mammo(w) * ammo_yellow) / 100) ? *FC_GOLD : \
                          *FC_GREEN );

// the amount of ammo the displayplayer has left 

#define playerammo _wc_pammo(players[displayplayer].readyweapon)

// the maximum amount the player could have for current weapon

#define playermaxammo _wc_mammo(players[displayplayer].readyweapon)

// set up an overlay_t

#define setol(o,a,b) \
   { \
      overlay[o].x = (a); \
      overlay[o].y = (b); \
   }

#define HUDCOLOUR FC_GRAY

int hud_overlaystyle = 1;
int hud_enabled = 1;
int hud_hidestatus = 0;

///////////////////////////////////////////////////////////////////////
//
// Heads Up Font
//

// note to programmers from other ports: hu_font is the heads up font
// *not* the general doom font as it is in the original sources and
// most ports

// haleyjd 02/25/09: hud font set by EDF:
const char *hud_overfontname;
vfont_t *hud_overfont;
static boolean hu_fontloaded = false;

//
// HU_LoadFont
//
// Loads the heads-up font. The naming scheme for the lumps is
// not very consistent, so this is relatively complicated.
//
void HU_LoadFont(void)
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
      V_FontWriteText(hud_overfont, s, x, y);
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

// create a string containing the text 'bar' which graphically
// show %age of ammo/health/armor etc left

static void HU_TextBar(char *s, size_t len, int pct)
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
         addchar = 127 - (pct*5)/BARSIZE;
         pct = 0;
      }
      psnprintf(s, len, "%s%c", s, addchar);
   }
}

/////////////////////////////////////////////////////////////////////////
//
// Drawer
//
// the actual drawer is the heart of the overlay
// code. It is split into individual functions,
// each of which draws a different part.

// the offset of percentage bars from the starting text
#define GAP 40

/////////////////////////
//
// draw health
//

void HU_DrawHealth(int x, int y)
{
   char tempstr[128];
   int fontcolour;
   
   memset(tempstr, 0, 128);

   HU_WriteText(HUDCOLOUR "Health", x, y);
   x += GAP;               // leave a gap between name and bar
   
   // decide on the colour first
   fontcolour =
      players[displayplayer].health < health_red ? *FC_RED :
      players[displayplayer].health < health_yellow ? *FC_GOLD :
      players[displayplayer].health <= health_green ? *FC_GREEN :
      *FC_BLUE;
  
   psnprintf(tempstr, sizeof(tempstr), "%c", fontcolour);

   // now make the actual bar
   HU_TextBar(tempstr, sizeof(tempstr), players[displayplayer].health);
   
   // append the percentage itself
   psnprintf(tempstr, sizeof(tempstr), 
             "%s %i", tempstr, players[displayplayer].health);
   
   // write it
   HU_WriteText(tempstr, x, y);
}

/////////////////////////////
//
// Draw Armour.
// very similar to drawhealth.
//

void HU_DrawArmor(int x, int y)
{
  char tempstr[128];
  int fontcolour;

  memset(tempstr, 0, 128);

  // title first
  HU_WriteText(HUDCOLOUR "Armor", x, y);
  x += GAP;              // leave a gap between name and bar
  
  // decide on colour
  fontcolour =
    players[displayplayer].armorpoints < armor_red ? *FC_RED :
    players[displayplayer].armorpoints < armor_yellow ? *FC_GOLD :
    players[displayplayer].armorpoints <= armor_green ? *FC_GREEN :
    *FC_BLUE;
  psnprintf(tempstr, sizeof(tempstr), "%c", fontcolour);
  
  // make the bar
  HU_TextBar(tempstr, sizeof(tempstr), 
             players[displayplayer].armorpoints);
  
  // append the percentage itself
  psnprintf(tempstr, sizeof(tempstr), "%s %i", tempstr,
	    players[displayplayer].armorpoints);
  
  HU_WriteText(tempstr, x, y);
}

////////////////////////////
//
// Drawing Ammo
//

void HU_DrawAmmo(int x, int y)
{
   char tempstr[128];
   int fontcolour;
   int maxammo;
   
   memset(tempstr, 0, sizeof(tempstr));
   
   HU_WriteText(HUDCOLOUR "Ammo", x, y);
   x += GAP;
   
   fontcolour = weapcolour(players[displayplayer].readyweapon);
   psnprintf(tempstr, sizeof(tempstr), "%c", fontcolour);
   
   maxammo = playermaxammo;

   if(maxammo)
   {
      HU_TextBar(tempstr, sizeof(tempstr), (100 * playerammo) / maxammo);
      psnprintf(tempstr, sizeof(tempstr), "%s %i/%i", 
                tempstr, playerammo, playermaxammo);
   }
   else // fist or chainsaw
      psnprintf(tempstr, sizeof(tempstr), "%sN/A", tempstr);
   
   HU_WriteText(tempstr, x, y);
}

//////////////////////////////
//
// Weapons List
//

void HU_DrawWeapons(int x, int y)
{
   char tempstr[128];
   int i;
   int fontcolour;
   
   memset(tempstr, 0, 128);
   
   HU_WriteText(HUDCOLOUR "Weapons", x, y);    // draw then leave a gap
   x += GAP;
  
   for(i=0; i<NUMWEAPONS; i++)
   {
      if(players[displayplayer].weaponowned[i])
      {
         // got it
         fontcolour = weapcolour(i);
         psnprintf(tempstr, sizeof(tempstr), "%s%c%i ", tempstr,
                   fontcolour, i+1);
      }
   }
   
   HU_WriteText(tempstr, x, y);    // draw it
}

////////////////////////////////
//
// Draw the keys
//

extern patch_t *keys[NUMCARDS+3];

void HU_DrawKeys(int x, int y)
{
   int i;
   
   HU_WriteText(HUDCOLOUR "Keys", x, y);    // draw then leave a gap
   x += GAP;
   
   // haleyjd 10/09/05: don't show double keys in Heretic
   for(i = 0; i < GameModeInfo->numHUDKeys; ++i)
   {
      if(players[displayplayer].cards[i])
      {
         // got that key
         V_DrawPatch(x, y, &vbscreen, keys[i]);
         x += 11;
      }
   }
}

///////////////////////////////
//
// Draw the Frags

void HU_DrawFrag(int x, int y)
{
  char tempstr[64];

  memset(tempstr, 0, 64);

  HU_WriteText(HUDCOLOUR "Frags", x, y);    // draw then leave a gap
  x += GAP;
  
  sprintf(tempstr, HUDCOLOUR "%i", players[displayplayer].totalfrags);
  HU_WriteText(tempstr, x, y);        
}

///////////////////////////////////////
//
// draw the status (number of kills etc)
//

void HU_DrawStatus(int x, int y)
{
  char tempstr[128];

  memset(tempstr, 0, 128);

  HU_WriteText(HUDCOLOUR "Status", x, y); // draw, leave a gap
  x += GAP;
  
  // haleyjd 06/14/06: restored original colors to K/I/S
  psnprintf(tempstr, sizeof(tempstr),
	    FC_RED  "K" FC_GREEN " %i/%i "
	    FC_BLUE "I" FC_GREEN " %i/%i "
	    FC_GOLD "S" FC_GREEN " %i/%i ",
	    players[displayplayer].killcount, totalkills,
	    players[displayplayer].itemcount, totalitems,
	    players[displayplayer].secretcount, totalsecret
	   );
  
  HU_WriteText(tempstr, x, y);
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
      {
         setol(i, -1, -1); // turn it off
      }
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
      {
         setol(ol_frag, 0, 192);
      }
      else
      {
         setol(ol_key, 0, 192);
      }
      if(!hud_hidestatus)
         setol(ol_status, 0, 184);
      break;
   }
}

////////////////////////////////////////////////////////////////////////
//
// heart of the overlay really.
// draw the overlay, deciding which bits to draw and where

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

char *str_style[] =
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
