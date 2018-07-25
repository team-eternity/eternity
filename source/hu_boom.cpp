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
// Purpose: Boom HUD. Rewritten and put in a seperate module(seems sensible)
// Authors: Max Waine
//

#include "d_gi.h"
#include "doomstat.h"
#include "e_weapons.h"
#include "e_inventory.h"
#include "hu_boom.h"
#include "hu_over.h"
#include "metaqstring.h"
#include "v_font.h"
#include "v_misc.h"

// The HUD always displays information on the displayplayer
#define hu_player (players[displayplayer])

// Get the amount of ammo the displayplayer has left in his/her readyweapon
#define playerammo    HU_WC_PlayerAmmo(hu_player.readyweapon)

// Get the maximum amount the player could have for his/her readyweapon
#define playermaxammo HU_WC_MaxAmmo(hu_player.readyweapon)

// Color of neutral informational components
#define HUDCOLOR FC_GRAY

//=============================================================================
//
// Heads Up Font
//

// note to programmers from other ports: hu_font is the heads up font
// *not* the general doom font as it is in the original sources and
// most ports

//
// sf: write a text line to x, y
// haleyjd 01/14/05: now uses vfont engine
//
static void HU_WriteText(const char *s, int x, int y)
{
   if(hud_fontsloaded)
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
void BoomHUD::DrawHealth(int x, int y)
{
   qstring tempstr;

   HU_WriteText(HUDCOLOR "Health", x, y);
   x += GAP; // leave a gap between name and bar

   //psnprintf(tempstr, sizeof(tempstr), "%c", fontcolor);
   tempstr << HU_HealthColor();

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
void BoomHUD::DrawArmor(int x, int y)
{
   qstring tempstr;

   // title first
   HU_WriteText(HUDCOLOR "Armor", x, y);
   x += GAP; // leave a gap between name and bar

   tempstr << HU_ArmorColor();

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
void BoomHUD::DrawAmmo(int x, int y)
{
   qstring tempstr;
   int fontcolor;
   int lmaxammo;

   HU_WriteText(HUDCOLOR "Ammo", x, y);
   x += GAP;

   fontcolor = HU_WeapColor(hu_player.readyweapon);

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
void BoomHUD::DrawWeapons(int x, int y)
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
         fontcolor = HU_WeapColor(E_WeaponForDEHNum(i));
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
void BoomHUD::DrawKeys(int x, int y)
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
void BoomHUD::DrawFrags(int x, int y)
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
void BoomHUD::DrawStatus(int x, int y)
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

// The solitary instance of the BOOM HUD overlay
BoomHUD boom_overlay;

// EOF

