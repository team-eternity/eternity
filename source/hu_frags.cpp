// Emacs style mode select -*- C -*- vi:sw=3 ts=3:
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
// Heads up frag counter
//
// Counts the frags by each player and sorts them so that the best
// player is at the top of the list
//
// By Simon Howard
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "hu_frags.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_player.h"
#include "doomdef.h"
#include "doomstat.h"
#include "g_game.h"
#include "r_draw.h"
#include "w_wad.h"
#include "v_video.h"
#include "g_bind.h"
#include "d_gi.h"
#include "e_fonts.h"

// [CG] Added.
#include "cs_main.h"
#include "cs_team.h"

#define FRAGSX 125
#define FRAGSY 10

#define NAMEX 165
#define NAMEY 65

#define FRAGNUMX 175

// extern boolean gamekeydown[NUMKEYS]; // g_game.c

extern int levelFragLimit;  // g_game.c
extern int levelTimeLimit;  // d_main.c
extern int levelScoreLimit; // g_game.c

player_t *sortedplayers[MAXPLAYERS];

int num_players;
int show_scores;                // enable scores

void HU_FragsInit(void)
{
}

extern vfont_t *hud_font;

boolean fragsdrawn;

void HU_FragsDrawer(void)
{
   int i, x, y;
   char tempstr[50];
   patch_t *fragtitle;

   if(!show_scores)
      return;

   if(((players[displayplayer].playerstate != PST_DEAD || walkcam_active)
      && !action_frags) || GameType != gt_dm || automapactive)
      return;

   // "frags"

   // haleyjd 04/08/10: draw more intelligently
   fragtitle = (patch_t *)(W_CacheLumpName("HU_FRAGS", PU_CACHE));
   x = (SCREENWIDTH - SwapShort(fragtitle->width)) / 2;
   y = (NAMEY - SwapShort(fragtitle->height)) / 2;

   V_DrawPatch(x, y, &vbscreen, fragtitle);

   y = NAMEY;
   
   for(i = 0; i < num_players; ++i)
   {
      // write their name
      psnprintf(tempstr, sizeof(tempstr), "%s%s", !demoplayback && 
         sortedplayers[i]==players+consoleplayer ? FC_HI : FC_NORMAL,
         sortedplayers[i]->name);
      
      V_FontWriteText(hud_font, tempstr, 
                      NAMEX - V_FontStringWidth(hud_font, tempstr), y);
      
      // box behind frag pic
      // haleyjd 01/12/04: changed translation handling

      V_DrawPatchTranslated(FRAGNUMX, y, &vbscreen,
                            (patch_t *)W_CacheLumpName("HU_FRGBX", PU_CACHE),
                            sortedplayers[i]->colormap ?
                            translationtables[(sortedplayers[i]->colormap - 1)] :
                            NULL, false);
      // draw the frags
      psnprintf(tempstr, sizeof(tempstr), "%i", sortedplayers[i]->totalfrags);
      V_FontWriteText(hud_font, tempstr, 
                      FRAGNUMX + 16 - V_FontStringWidth(hud_font, tempstr)/2, y);
      y += 10;
   }

   fragsdrawn = true;
}

void HU_FragsUpdate(void)
{
   int i, j;
   int change;
   player_t *temp;

   num_players = 0;

   for(i = 0; i < MAXPLAYERS; i++)
   {
      if(!playeringame[i])
         continue;

      // found a real player
      // add to list

      sortedplayers[num_players] = &players[i];
      num_players++;

      players[i].totalfrags = 0; // reset frag count

      for(j = 0; j < MAXPLAYERS; j++)  // add all frags for this player
      {
         if(!playeringame[j])
            continue;

         if(i == j)
            players[i].totalfrags -= players[i].frags[j];
         else
            players[i].totalfrags += players[i].frags[j];
      }
   }

   // use the bubble sort algorithm to sort the players

   do
   {
      change = false;
      for(i = 0; i < num_players - 1; i++)
      {
         if(sortedplayers[i]->totalfrags < sortedplayers[i + 1]->totalfrags)
         {
            temp = sortedplayers[i];
            sortedplayers[i] = sortedplayers[i + 1];
            sortedplayers[i + 1] = temp;
            change = true;
         }
      }
   } while(change);
}

void HU_FragsErase(void)
{
   // [CG] Added !clientserver check.
   if(!clientserver && GameType != gt_dm)
      return;

   if(fragsdrawn)
   {
      // FIXME: SUBOPTIMAL
      R_VideoErase(0, 0, SCREENWIDTH, SCREENHEIGHT);
      fragsdrawn = false;
   }
}

////////////////////////////////////
//
// Console Commands
//

CONSOLE_COMMAND(frags, 0)
{
   int i;
   
   for(i = 0; i < num_players; ++i)
   {
      C_Printf(FC_HI"%i"FC_NORMAL" %s\n",
               sortedplayers[i]->totalfrags,
               sortedplayers[i]->name);
   }
}

VARIABLE_BOOLEAN(show_scores,       NULL,           onoff);
CONSOLE_VARIABLE(show_scores,   show_scores,    0)      {}

void HU_FragsAddCommands(void)
{
   C_AddCommand(frags);
   C_AddCommand(show_scores);
}

// EOF
