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
#include "m_qstr.h"
#include "v_font.h"
#include "v_misc.h"

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

// Color of neutral informational components
#define HUDCOLOR FC_GRAY

//=============================================================================
//
// Heads Up Font
//

//
// sf: write a text line to x, y
// haleyjd 01/14/05: now uses vfont engine
//
static void HU_WriteText(const char *s, int x, int y, VBuffer *screen)
{
   if(hud_fontsloaded)
      V_FontWriteText(hud_overfont, s, x, y, screen);
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
// Draw the status (number of kills etc)
//
void BoomHUD::DrawStatus(int x, int y)
{
   qstring tempstr;

   HU_WriteText(HUDCOLOR "Status", x, y, m_screen); // draw, leave a gap
   x += GAP;

   // haleyjd 06/14/06: restored original colors to K/I/S
   tempstr
      << FC_RED  "K " FC_GREEN << hu_player.displaykillcount << '/' << totalkills << ' '
      << FC_BLUE "I " FC_GREEN << hu_player.itemcount << '/' << totalitems << ' ';
   
   if (!hud_hidesecrets)
   {
      tempstr << FC_GOLD "S " FC_GREEN << hu_player.secretcount << '/' << totalsecret;
   }

   HU_WriteText(tempstr.constPtr(), x, y, m_screen);
}

void BoomHUD::DrawHealth(int x, int y)
{
   qstring tempstr;

   HU_WriteText(HUDCOLOR "Health", x, y, m_screen);
   x += GAP; // leave a gap between name and bar

   //psnprintf(tempstr, sizeof(tempstr), "%c", fontcolor);
   tempstr << HU_HealthColor();

   // now make the actual bar
   HU_textBar(tempstr, hu_player.health);

   // append the percentage itself
   tempstr << " " << hu_player.health;

   // write it
   HU_WriteText(tempstr.constPtr(), x, y, m_screen);
}


//
// Very similar to DrawHealth
//
void BoomHUD::DrawArmor(int x, int y)
{
   qstring tempstr;

   // title first
   HU_WriteText(HUDCOLOR "Armor", x, y, m_screen);
   x += GAP; // leave a gap between name and bar

   tempstr << HU_ArmorColor();

   // make the bar
   HU_textBar(tempstr, hu_player.armorpoints);

   // append the percentage itself
   tempstr << ' ' << hu_player.armorpoints;

   HU_WriteText(tempstr.constPtr(), x, y, m_screen);
}

//
// Drawing Ammo
//
void BoomHUD::DrawAmmo(int x, int y)
{
   qstring tempstr;
   int fontcolor;
   int lmaxammo;

   HU_WriteText(HUDCOLOR "Ammo", x, y, m_screen);
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

   HU_WriteText(tempstr.constPtr(), x, y, m_screen);
}

//
// Weapons List
//
void BoomHUD::DrawWeapons(int x, int y)
{
   qstring tempstr;

   HU_WriteText(HUDCOLOR "Weapons", x, y, m_screen);  // draw then leave a gap
   x += GAP;

   for(int i = 0; i < NUMWEAPONS; i++)
   {
      bool had;
      char fontcolor = HU_WeaponColourGeneralized(hu_player, i, &had);
      if(had)  // got it
         tempstr << fontcolor << (i + 1) << ' ';
   }

   HU_WriteText(tempstr.constPtr(), x, y, m_screen);  // draw it
}

extern patch_t *keys[NUMCARDS+3];

//
// Draw the keys
//
void BoomHUD::DrawKeys(int x, int y)
{
   HU_WriteText(HUDCOLOR "Keys", x, y, m_screen);    // draw then leave a gap
   x += GAP;

   // haleyjd 10/09/05: don't show double keys in Heretic
   for(int i = 0; i < GameModeInfo->numHUDKeys; i++)
   {
      if(E_GetItemOwnedAmountName(hu_player, GameModeInfo->cardNames[i]) > 0)
      {
         // got that key
         V_DrawPatch(x, y, m_screen, keys[i]);
         x += 11;
      }
   }
}

//
// Draw the Frags
//
void BoomHUD::DrawFrags(int x, int y)
{
   qstring tempstr;

   HU_WriteText(HUDCOLOR "Frags", x, y, m_screen); // draw then leave a gap
   x += GAP;

   tempstr << HUDCOLOR << hu_player.totalfrags;
   HU_WriteText(tempstr.constPtr(), x, y, m_screen);
}

//
// This is the same as the old HU_overlaySetup.
//
void BoomHUD::Setup()
{
   int x, y;

   m_screen = &vbscreenyscaled;

   // decide where to put all the widgets

   for(auto &data : drawerdata)
      data.enabled = true; // turn em all on

   // turn off status if we aren't using it
   if(hud_hidestats)
      drawerdata[ol_status].enabled = false;

   // turn off frag counter or key display,
   // according to if we're in a deathmatch game or not
   if(GameType == gt_dm)
      drawerdata[ol_key].enabled  = false;
   else
      drawerdata[ol_frag].enabled = false;

   // now build according to style

   int boxx, boxy;
   HU_InventoryGetCurrentBoxHints(boxx, boxy);

   rightoffset = 0;  // initially be 0

   switch(hud_overlaylayout)
   {
   case HUD_OFF:       // 'off'
   case HUD_GRAPHICAL: // 'graphical' -- haleyjd 01/11/05: this is handled by status bar
      for(auto &data : drawerdata)
         data.enabled = false;
      break;

   case HUD_BOOM: // 'bottom left' / 'BOOM' style
      y = m_screen->unscaledh - 8;

      for(int i = NUMOVERLAY - 2; i >= 0; --i)
      {
         if(drawerdata[i].enabled)
         {
            SetupOverlay(static_cast<overlay_e>(i), 0, y);
            y -= 8;
         }
      }
      SetupOverlay(ol_invcurr, boxx, boxy);
      break;

   case HUD_FLAT: // all at bottom of screen
      x = m_screen->unscaledw - 138;
      y = m_screen->unscaledh - 8;

      // haleyjd 06/14/06: rewrote to restore a sensible ordering
      for(int i = NUMOVERLAY - 2; i >= 0; --i)
      {
         if(drawerdata[i].enabled)
         {
            SetupOverlay(static_cast<overlay_e>(i), x, y);
            y -= 8;
         }
         if(i == ol_weap)
         {
            x = 0;
            y = m_screen->unscaledh - 8;
         }
      }
      SetupOverlay(ol_invcurr, boxx, boxy);
      break;

   case HUD_DISTRIB: // similar to boom 'distributed' style
      SetupOverlay(ol_health, m_screen->unscaledw - 138, 0);
      SetupOverlay(ol_armor,  m_screen->unscaledw - 138, 8);
      rightoffset = 16;
      SetupOverlay(ol_weap,   m_screen->unscaledw - 138, m_screen->unscaledh - 16);
      SetupOverlay(ol_ammo,   m_screen->unscaledw - 138, m_screen->unscaledh - 8);

      if(GameType == gt_dm)  // if dm, put frags in place of keys
         SetupOverlay(ol_frag, 0, m_screen->unscaledh - 8);
      else
         SetupOverlay(ol_key,  0, m_screen->unscaledh - 8);

      if(!hud_hidestats)
         SetupOverlay(ol_status, 0, m_screen->unscaledh - 16);
      boxy -= 16;
      SetupOverlay(ol_invcurr, boxx, boxy);
      break;

   default:
      break;
   }
}

// The solitary instance of the BOOM HUD overlay
BoomHUD boom_overlay;

// EOF

