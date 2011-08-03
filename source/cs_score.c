// Emacs style mode select -*- C -*- vim:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2011 Charles Gunyon
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
// DESCRIPTION:
//   Client/Server scoreboard drawing.
//
//----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#include "c_io.h"
#include "d_player.h"
#include "d_gi.h"
#include "doomdef.h"
#include "doomstat.h"
#include "v_block.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_video.h"

#include "cs_team.h"
#include "cs_main.h"
#include "cs_config.h"
#include "cs_score.h"
#include "cl_main.h"

#define WINDOW_MARGIN 2
#define SCORES_MARGIN 5
#define WINDOW_WIDTH  (SCREENWIDTH - (WINDOW_MARGIN * 2))
#define WINDOW_HEIGHT ((ST_Y - WINDOW_MARGIN) - 18)
#define SCREEN_MIDDLE ((SCREENWIDTH >> 1))

extern vfont_t *hud_font;
extern vfont_t *hud_overfont;
extern boolean fragsdrawn;
extern int num_players;
extern int show_scores;
extern player_t *sortedplayers[MAXPLAYERS];

// [CG] These are named backwards because EE names them backwards.
static crange_idx_e team_colors_to_font_colors[team_color_max] = {
   CR_GREEN,
   CR_RED,
   CR_BLUE /*,
   CR_GREEN, CR_GRAY, CR_BROWN, CR_BRICK, CR_GOLD */
};

static int team_colors_to_color_ranges[team_color_max] = {
   TEXT_COLOR_GREEN,
   TEXT_COLOR_RED,
   TEXT_COLOR_BLUE /*,
   TEXT_COLOR_GREEN,
   TEXT_COLOR_GRAY,
   TEXT_COLOR_BROWN,
   TEXT_COLOR_BRICK,
   TEXT_COLOR_GOLD */
};

static void display_deathmatch_scoreboard(unsigned int extra_top_margin)
{
   int playernum, i, x, y, window_width;
   int window_x, window_y, scores_x, window_height;
   int frags_column_end, frags_stat_position;
   int deaths_column_end, deaths_stat_position;
   int lag_column_end, lag_stat_position;
   byte white = (byte)GameModeInfo->whiteIndex;
   char s[91], level_time_s[11], level_timelimit_s[11];
   crange_idx_e font_color;
   player_t *player;
   client_t *client;
   int left_border;
   int right_border;
   int size_of_space = (V_FontStringWidth(hud_overfont, " ") * 2);

   // [CG] Format the time properly.
   CS_FormatTicsAsTime(level_time_s, leveltime);
   if(levelTimeLimit)
   {
      // [CG] levelTimeLimit is in minutes... I guess.
      CS_FormatTime(level_timelimit_s, levelTimeLimit * 60);
   }

   if(extra_top_margin > 0)
   {
      // [CG] If we've moved the top down at all, then we can't just stop at
      //      where the status bar would normally be.  Use as much of the
      //      screen as possible in this case.
      window_height = SCREENHEIGHT - (WINDOW_MARGIN + extra_top_margin);
   }
   else
   {
      window_height = WINDOW_HEIGHT;
   }

   psnprintf(s, sizeof(s), "[TKV]RussianDumbass  -9999  9999  9999ms");
   window_width = V_FontStringWidth(hud_overfont, s) + (SCORES_MARGIN * 2);
   left_border  = SCREEN_MIDDLE - (window_width >> 1);
   right_border = SCREEN_MIDDLE + (window_width >> 1);

   // window_x = window_y = WINDOW_MARGIN;
   window_x = left_border;
   window_y = 0 + extra_top_margin; // WINDOW_MARGIN + 2;
   scores_x = window_x + SCORES_MARGIN;
   y = window_y + 4 + extra_top_margin;

   // [CG] TODO: Add option to set the scoreboard's background opacity.
   V_ColorBlockTLScaled(
      &vbscreen,
      (byte)CR_GRAY,
      window_x,
      window_y,
      window_width,
      window_height,
      FRACUNIT >> 1
   );

   // [CG] Draw gamemode header
   if(cs_settings->max_players == 2)
   {
      psnprintf(s, sizeof(s), "%s", "DUEL");
   }
   else
   {
      psnprintf(s, sizeof(s), "%s", "DEATHMATCH");
   }
   x = SCREEN_MIDDLE - (V_FontStringWidth(hud_font, s) >> 1);
   V_FontWriteTextColored(hud_font, s, CR_GOLD, x, y);
   y += (V_FontStringHeight(hud_font, s) + 2);

   // [CG] Draw game status (frag limit & time/time limit).
   if(levelTimeLimit && levelFragLimit)
   {
      psnprintf(s, sizeof(s), "%sFrag Limit: %d  |", FC_HI, levelFragLimit);
      x = SCREEN_MIDDLE - V_FontStringWidth(hud_overfont, s);
      V_FontWriteText(hud_overfont, s, x, y);
      psnprintf(s, sizeof(s), "%s| Time: %s/%s", FC_HI,
         level_time_s,
         level_timelimit_s
      );
      x = SCREENWIDTH >> 1;
      V_FontWriteText(hud_overfont, s, x, y);
      psnprintf(s, sizeof(s), "%sFrag Limit: %d || Time: %s/%s", FC_HI,
         levelFragLimit,
         level_time_s,
         level_timelimit_s
      );
   }
   else if(levelFragLimit)
   {
      psnprintf(s, sizeof(s), "%sFrag Limit: %d || Time: %s", FC_HI,
         levelFragLimit,
         level_time_s
      );
      x = (SCREENWIDTH - V_FontStringWidth(hud_overfont, s)) >> 1;
      V_FontWriteText(hud_overfont, s, x, y);
   }
   else
   {
      if(levelTimeLimit)
      {
         psnprintf(s, sizeof(s), "%sTime: %s/%s", FC_HI,
            level_time_s,
            level_timelimit_s
         );
      }
      else
      {
         psnprintf(s, sizeof(s), "%sTime: %s", FC_HI, level_time_s);
      }
      x = (SCREENWIDTH - V_FontStringWidth(hud_overfont, s)) >> 1;
      V_FontWriteText(hud_overfont, s, x, y);
   }

   y += 1;
   // y += (V_FontStringHeight(hud_overfont, s) + 3);

   // [CG] Calculate lag stat position.
   lag_column_end = right_border - SCORES_MARGIN;
   psnprintf(s, sizeof(s), "%sms", "0000");
   lag_stat_position = lag_column_end - V_FontStringWidth(hud_overfont, s);

   // [CG] Calculate deaths stat position.
   deaths_column_end = lag_stat_position - size_of_space;
   psnprintf(s, sizeof(s), "%s", "0000");
   deaths_stat_position =
      deaths_column_end - V_FontStringWidth(hud_overfont, s);

   // [CG] Calculate frag stat position.
   frags_column_end = deaths_stat_position - size_of_space;
   psnprintf(s, sizeof(s), "%s", "-0000");
   frags_stat_position = frags_column_end - V_FontStringWidth(hud_overfont, s);

   // [CG] Draw separation line between headers and player stat rows.
   y += (V_FontStringHeight(hud_overfont, s));
   V_ColorBlockScaled(
      &vbscreen, white, scores_x, y, window_width - (SCORES_MARGIN * 2), 1
   );
   y += 2;

   // [CG] Draw stat rows (names, etc.).

   // [CG] This fills up the scoreboard for testing.
   /*
   for(i = num_players; i < 17; i++)
   {
      // [CG] Draw the player's name.
      V_FontWriteTextColored(
         hud_overfont, "[TKV]RussianDumbass", CR_GREEN,
         scores_x, y
      );
      // [CG] Draw the player's frags.
      psnprintf(s, sizeof(s), "%d", -9999);
      V_FontWriteTextColored(
         hud_overfont, s, CR_GREEN,
         frags_column_end - V_FontStringWidth(hud_overfont, s), y
      );
      // [CG] Draw the player's deaths.
      psnprintf(s, sizeof(s), "%u", 9999);
      V_FontWriteTextColored(
         hud_overfont, s, CR_GREEN,
         deaths_column_end - V_FontStringWidth(hud_overfont, s), y
      );
      // [CG] Draw the player's lag.
      psnprintf(s, sizeof(s), "%3ums", 9999);
      V_FontWriteTextColored(
         hud_overfont, s, CR_GREEN,
         lag_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      // [CG] Next line.
      y += V_FontStringHeight(hud_overfont, s);
   }
   */

   for(i = 0; i < num_players; i++)
   {
      player = sortedplayers[i];
      playernum = player - players;

      if(!playeringame[playernum])
      {
         continue;
      }

      client = &clients[playernum];

      // [CG] Don't list the server's spectator player, and don't list
      //      spectators yet (they're listed below playing players).
      if(playernum == 0 || client->spectating)
      {
         continue;
      }

      if(playernum == displayplayer)
      {
         font_color  = CR_GRAY;
      }
      else
      {
         font_color = CR_GREEN;
      }

      // [CG] Draw the player's name.
      V_FontWriteTextColored(
         hud_overfont, player->name, font_color,
         scores_x, y
      );

      // [CG] Draw the player's frags.
      psnprintf(s, sizeof(s), "%d", player->totalfrags);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         frags_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      // [CG] Draw the player's deaths.
      psnprintf(s, sizeof(s), "%u", client->death_count);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         deaths_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      // [CG] Draw the player's lag.
      psnprintf(s, sizeof(s), "%3ums", client->transit_lag);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         lag_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      // [CG] Next line.
      y += V_FontStringHeight(hud_overfont, s);
   }

   y += 1;

   for(i = 0; i < num_players; i++)
   {
      player = sortedplayers[i];
      playernum = player - players;

      if(!playeringame[playernum])
      {
         continue;
      }

      client = &clients[playernum];

      if(playernum == displayplayer)
      {
         font_color = CR_GRAY;
      }
      else
      {
         font_color = CR_GREEN;
      }

      if(playernum == 0 || !client->spectating)
      {
         continue;
      }

      // [CG] Draw the player's name.
      psnprintf(s, sizeof(s), "%s%s", FC_TRANS, player->name);
      V_FontWriteTextColored(hud_overfont, s, font_color, scores_x, y);

      // [CG] Draw the player's frags.
      psnprintf(s, sizeof(s), "%s%d", FC_TRANS, player->totalfrags);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         frags_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      // [CG] Draw the player's deaths.
      psnprintf(s, sizeof(s), "%s%u", FC_TRANS, client->death_count);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         deaths_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      // [CG] Draw the player's lag.
      psnprintf(s, sizeof(s), "%s%3ums", FC_TRANS, client->transit_lag);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         lag_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      // [CG] Next line.
      y += V_FontStringHeight(hud_overfont, s);
   }
}

static void display_team_scoreboard(unsigned int extra_top_margin)
{
   int i, playernum, x, y, name_x, frags_x, lag_x, red_y, blue_y, stat_y;
   int window_height, window_x, window_y, scores_x, size_of_space;
   int red_score_position, blue_score_position;
   int red_score_column_end, blue_score_column_end;
   int red_frags_column_end, blue_frags_column_end;
   byte white = (byte)GameModeInfo->whiteIndex;
   char s[91], level_time_s[11], level_timelimit_s[11];
   int color_range;
   crange_idx_e font_color;
   player_t *player;
   client_t *client;

   // [CG] Setup the initial dimensions and the y position.
   size_of_space = (V_FontStringWidth(hud_overfont, " ") * 2);
   window_x = WINDOW_MARGIN + extra_top_margin;
   window_y = 0;
   scores_x = window_x + SCORES_MARGIN;
   y = window_y + 4 + extra_top_margin;

   // [CG] TODO: Add option to set the scoreboard's background opacity.
   if(extra_top_margin > 0)
   {
      // [CG] If we've moved the top down at all, then we can't just stop at
      //      where the status bar would normally be.  Use as much of the
      //      screen as possible in this case.
      window_height = SCREENHEIGHT - (WINDOW_MARGIN + extra_top_margin);
   }
   else if(CS_TEAMS_ENABLED)
   {
      window_height = ST_Y - WINDOW_MARGIN;
   }

   // [CG] TODO: Add option to set the scoreboard's background opacity.
   V_ColorBlockTLScaled(
      &vbscreen,
      (byte)CR_GRAY,
      window_x,
      window_y,
      WINDOW_WIDTH,
      window_height,
      FRACUNIT >> 1
   );

   // [CG] Format the time properly.
   CS_FormatTicsAsTime(level_time_s, leveltime);
   if(levelTimeLimit)
   {
      // [CG] levelTimeLimit is in minutes... I guess.
      CS_FormatTime(level_timelimit_s, levelTimeLimit * 60);
   }

   // [CG] Draw gamemode header
   if(CS_TEAMDM)
   {
      psnprintf(s, sizeof(s), "%s", "TEAM DEATHMATCH");
   }
   else if(CS_CTF)
   {
      psnprintf(s, sizeof(s), "%s", "CAPTURE THE FLAG");
   }
   else
   {
      psnprintf(s, sizeof(s), "%s", "TEAM GAME");
   }
   x = SCREEN_MIDDLE - (V_FontStringWidth(hud_font, s) >> 1);
   V_FontWriteTextColored(hud_font, s, CR_GOLD, x, y);
   y += (V_FontStringHeight(hud_font, s) + 2);

   // [CG] Draw game status (frag limit & time/time limit).
   if(levelTimeLimit && levelScoreLimit)
   {
      psnprintf(s, sizeof(s), "%sScore Limit: %d  |", FC_HI, levelScoreLimit);
      x = SCREEN_MIDDLE - V_FontStringWidth(hud_overfont, s);
      V_FontWriteText(hud_overfont, s, x, y);

      psnprintf(s, sizeof(s), "%s| Time: %s/%s", FC_HI,
         level_time_s,
         level_timelimit_s
      );
      x = SCREEN_MIDDLE;
      V_FontWriteText(hud_overfont, s, x, y);
      /*
      psnprintf(s, sizeof(s), "%sScore Limit: %d || Time: %s/%s", FC_HI,
         levelFragLimit,
         level_time_s,
         level_timelimit_s
      );
      */
   }
   else if(levelScoreLimit)
   {
      psnprintf(s, sizeof(s), "%sScore Limit: %d || Time: %s", FC_HI,
         levelScoreLimit,
         level_time_s
      );
      x = (SCREENWIDTH - V_FontStringWidth(hud_overfont, s)) >> 1;
      V_FontWriteText(hud_overfont, s, x, y);
   }
   else
   {
      if(levelTimeLimit)
      {
         psnprintf(s, sizeof(s), "%sTime: %s/%s", FC_HI,
            level_time_s,
            level_timelimit_s
         );
      }
      else
      {
         psnprintf(s, sizeof(s), "%sTime: %s", FC_HI, level_time_s);
      }
      x = (SCREENWIDTH - V_FontStringWidth(hud_overfont, s)) >> 1;
      V_FontWriteText(hud_overfont, s, x, y);
   }

   y += (V_FontStringHeight(hud_overfont, s));

   // [CG] Draw team color names and scores.
   font_color = team_colors_to_font_colors[team_color_red];
   psnprintf(s, sizeof(s), "%s", team_color_names[team_color_red]);
   V_FontWriteTextColored(hud_overfont, s, font_color, scores_x, y);
   red_score_column_end = SCREEN_MIDDLE - SCORES_MARGIN;
   psnprintf(s, sizeof(s), "%d", team_scores[team_color_red]);
   red_score_position =
      red_score_column_end - V_FontStringWidth(hud_overfont, s);
   V_FontWriteTextColored(hud_overfont, s, font_color, red_score_position, y);

   font_color = team_colors_to_font_colors[team_color_blue];
   psnprintf(s, sizeof(s), "%s", team_color_names[team_color_blue]);
   V_FontWriteTextColored(
      hud_overfont, s, font_color, SCREEN_MIDDLE + SCORES_MARGIN, y
   );
   blue_score_column_end = SCREENWIDTH - (SCORES_MARGIN * 2);
   psnprintf(s, sizeof(s), "%d", team_scores[team_color_blue]);
   blue_score_position =
      blue_score_column_end - V_FontStringWidth(hud_overfont, s);
   V_FontWriteTextColored(hud_overfont, s, font_color, blue_score_position, y);

   // [CG] Calculate how wide the lag stat can possibly be in order to
   //      determine where to put frag stats.
   psnprintf(s, sizeof(s), "%3u", 999);
   /*
   psnprintf(s, sizeof(s), "--/%2u/%2u/%2u/%2u",
      MAX_POSITIONS >> 1,                     // [CG] Client lag.
      MAX_POSITIONS >> 1,                     // [CG] Server lag.
      (MAX_POSITIONS >> 1) * (1.0 / TICRATE), // [CG] Transit lag.
      100                                     // [CG] Packet loss.
   );
   */
   red_frags_column_end =
      red_score_column_end - V_FontStringWidth(hud_overfont, s);
   blue_frags_column_end =
      blue_score_column_end - V_FontStringWidth(hud_overfont, s);

   // [CG] Draw separation line between headers and player stat rows.
   y += (V_FontStringHeight(hud_overfont, s));

   V_ColorBlockScaled(
      &vbscreen, white, scores_x, y, (WINDOW_WIDTH >> 1) - 10, 1
   );
   V_ColorBlockScaled(
      &vbscreen, white, SCREEN_MIDDLE + 2, y, (WINDOW_WIDTH >> 1) - 10, 1
   );
   y += 2;

   red_y = blue_y = y;

   // [CG] This fills up the scoreboard for testing.
   /*
   for(i = num_players; i < 17; i++)
   {
      font_color = team_colors_to_font_colors[team_color_red];
      color_range = team_colors_to_color_ranges[team_color_red];
      name_x = scores_x;
      frags_x = red_frags_column_end;
      lag_x  = red_score_column_end;
      stat_y = red_y;
      red_y += V_FontStringHeight(hud_overfont, s);

      // [CG] Draw the player's name.
      V_FontWriteTextColored(
         hud_overfont, "[FLS]PotentPotables", font_color, name_x, stat_y
      );

      // [CG] Draw the player's frags.
      psnprintf(s, sizeof(s), "%d", -999);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         frags_x - (V_FontStringWidth(hud_overfont, s) + size_of_space), stat_y
      );

      // [CG] Draw the player's lag.
      psnprintf(s, sizeof(s), "%3u", 999);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         lag_x - V_FontStringWidth(hud_overfont, s), stat_y
      );

      // [CG] Next line.
      y += V_FontStringHeight(hud_overfont, s);
   }
   */

   // [CG] Draw stat rows (names, etc.).
   for(i = 0; i < num_players; i++)
   {
      player = sortedplayers[i];
      playernum = player - players;

      if(!playeringame[playernum])
      {
         continue;
      }

      client = &clients[playernum];

      // [CG] Don't list the server's spectator player, and don't list
      //      spectators yet (they're listed below playing players).
      if(playernum == 0 || client->spectating)
      {
         continue;
      }

      if(playernum == displayplayer)
      {
         font_color = CR_GRAY;
         color_range = TEXT_COLOR_GRAY;
      }
      else
      {
         font_color = team_colors_to_font_colors[client->team];
         color_range = team_colors_to_color_ranges[client->team];
      }

      if(client->team == team_color_red)
      {
         name_x = scores_x;
         frags_x = red_frags_column_end;
         lag_x  = red_score_column_end;
         stat_y = red_y;
         red_y += V_FontStringHeight(hud_overfont, s);
      }
      else
      {
         name_x = SCREEN_MIDDLE + SCORES_MARGIN;
         frags_x = blue_frags_column_end;
         lag_x = blue_score_column_end;
         stat_y = blue_y;
         blue_y += V_FontStringHeight(hud_overfont, s);
      }

      // [CG] Draw the player's name.
      V_FontWriteTextColored(
         hud_overfont, player->name, font_color, name_x, stat_y
      );

      // [CG] Draw the player's frags.
      psnprintf(s, sizeof(s), "%d", player->totalfrags);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         frags_x - (V_FontStringWidth(hud_overfont, s) + size_of_space), stat_y
      );

      // [CG] Draw the player's lag.
      psnprintf(s, sizeof(s), "%3u", client->transit_lag);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         lag_x - V_FontStringWidth(hud_overfont, s), stat_y
      );
   }

   y += 3;

   for(i = 0; i < num_players; i++)
   {
      player = sortedplayers[i];
      playernum = player - players;

      if(!playeringame[playernum])
      {
         continue;
      }

      client = &clients[playernum];

      if(playernum == 0 || !client->spectating)
      {
         continue;
      }

      if(playernum == displayplayer)
      {
         font_color = CR_GRAY;
         color_range = TEXT_COLOR_GRAY;
      }
      else
      {
         font_color = team_colors_to_font_colors[client->team];
         color_range = team_colors_to_color_ranges[client->team];
      }

      if(client->team == team_color_red)
      {
         name_x = scores_x;
         frags_x = red_frags_column_end;
         lag_x  = red_score_column_end;
         stat_y = red_y;
         red_y += V_FontStringHeight(hud_overfont, s);
      }
      else
      {
         name_x = SCREEN_MIDDLE + SCORES_MARGIN;
         frags_x = blue_frags_column_end;
         lag_x = blue_score_column_end;
         stat_y = blue_y;
         blue_y += V_FontStringHeight(hud_overfont, s);
      }

      // [CG] Draw the player's name.
      psnprintf(s, sizeof(s), "%s%s", FC_TRANS, player->name);
      V_FontWriteTextColored(hud_overfont, s, font_color, name_x, stat_y);

      // [CG] Draw the player's frags.
      psnprintf(s, sizeof(s), "%s%d", FC_TRANS, player->totalfrags);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         frags_x - (V_FontStringWidth(hud_overfont, s) + size_of_space), stat_y
      );

      // [CG] Draw the player's lag.
      psnprintf(s, sizeof(s), "%s%3u", FC_TRANS, client->transit_lag);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         lag_x - V_FontStringWidth(hud_overfont, s), stat_y
      );
   }
}

static void display_coop_scoreboard(unsigned int extra_top_margin)
{
   int i, x, y;
   int window_width = SCREENWIDTH - (SCORES_MARGIN * 2);
   int window_x, window_y, scores_x, window_height;
   int ammo_column_end, ammo_stat_position;
   int hp_column_end, hp_stat_position;
   int kills_column_end, kills_stat_position;
   int lives_column_end, lives_stat_position;
   int ping_column_end, ping_stat_position;
   byte white = (byte)GameModeInfo->whiteIndex;
   char s[91];
   crange_idx_e font_color;
   player_t *player;
   client_t *client;
   int left_border;
   int right_border;
   int size_of_space = (V_FontStringWidth(hud_overfont, " ") * 2);

   left_border  = SCREEN_MIDDLE - (window_width >> 1);
   right_border = SCREEN_MIDDLE + (window_width >> 1);
   window_x = WINDOW_MARGIN + extra_top_margin;
   window_y = 0;
   scores_x = window_x + SCORES_MARGIN;
   y = window_y + 4 + extra_top_margin;

   // [CG] TODO: Add option to set the scoreboard's background opacity.
   if(extra_top_margin > 0)
   {
      // [CG] If we've moved the top down at all, then we can't just stop at
      //      where the status bar would normally be.  Use as much of the
      //      screen as possible in this case.
      window_height = SCREENHEIGHT - (WINDOW_MARGIN + extra_top_margin);
   }
   else
   {
      window_height = ST_Y - WINDOW_MARGIN;
   }

   // [CG] TODO: Add option to set the scoreboard's background opacity.
   V_ColorBlockTLScaled(
      &vbscreen,
      (byte)CR_GRAY,
      window_x,
      window_y,
      WINDOW_WIDTH,
      window_height,
      FRACUNIT >> 1
   );

   // [CG] Draw header
   psnprintf(s, sizeof(s), "%s", "COOPERATIVE");

   x = SCREEN_MIDDLE - (V_FontStringWidth(hud_font, s) >> 1);
   V_FontWriteTextColored(hud_font, s, CR_GOLD, x, y);
   y += (V_FontStringHeight(hud_font, s) + 2);

   /*
   CS_FormatTicsAsTime(level_time_s, leveltime);
   if(levelTimeLimit)
   {
      CS_FormatTime(level_timelimit_s, levelTimeLimit * 60);
      psnprintf(s, sizeof(s), "%sTime: %s/%s", FC_HI,
         level_time_s,
         level_timelimit_s
      );
   }
   else
   {
      psnprintf(s, sizeof(s), "%sTime: %s", FC_HI, level_time_s);
   }
   x = (SCREENWIDTH - V_FontStringWidth(hud_overfont, s)) >> 1;
   V_FontWriteText(hud_overfont, s, x, y);
   y += (V_FontStringHeight(hud_overfont, s));

   */

   // [CG] Draw column names.
   //      Basically the way this is done is we measure the width of the
   //      largest value we expect, and then we write the column name where the
   //      end of that value would be.

   ping_column_end = right_border - SCORES_MARGIN;
   psnprintf(s, sizeof(s), "%ums", 999);
   ping_stat_position = ping_column_end - V_FontStringWidth(hud_overfont, s);

   lives_column_end = ping_stat_position - size_of_space;
   psnprintf(s, sizeof(s), "%u", 9999);
   lives_stat_position = lives_column_end - V_FontStringWidth(hud_overfont, s);

   kills_column_end = lives_stat_position - size_of_space;
   psnprintf(s, sizeof(s), "%u", 999999);
   kills_stat_position = kills_column_end - V_FontStringWidth(hud_overfont, s);

   hp_column_end = kills_stat_position - size_of_space;
   // [CG] "HP/Armor" is wider than 999/999, so use it instead.
   // psnprintf(s, sizeof(s), "%d/%d", 999, 999);
   psnprintf(s, sizeof(s), "%sHP/Armor", FC_HI);
   hp_stat_position = hp_column_end - V_FontStringWidth(hud_overfont, s);

   ammo_column_end = hp_stat_position - size_of_space;
   psnprintf(s, sizeof(s), "%d/%d/%d/%d", 999, 999, 999, 999);
   ammo_stat_position = ammo_column_end - V_FontStringWidth(hud_overfont, s);

   psnprintf(s, sizeof(s), "%sPlayer", FC_HI);
   V_FontWriteText(hud_overfont, s, scores_x, y);

   psnprintf(s, sizeof(s), "%sAmmo", FC_HI);
   V_FontWriteText(
      hud_overfont, s, ammo_column_end - V_FontStringWidth(hud_overfont, s), y
   );

   psnprintf(s, sizeof(s), "%sHP/Armor", FC_HI);
   V_FontWriteText(
      hud_overfont, s, hp_column_end - V_FontStringWidth(hud_overfont, s), y
   );

   psnprintf(s, sizeof(s), "%sKills", FC_HI);
   V_FontWriteText(
      hud_overfont, s, kills_column_end - V_FontStringWidth(hud_overfont, s), y
   );

   psnprintf(s, sizeof(s), "%sLives", FC_HI);
   V_FontWriteText(
      hud_overfont, s, lives_column_end - V_FontStringWidth(hud_overfont, s), y
   );

   psnprintf(s, sizeof(s), "%sPing", FC_HI);
   V_FontWriteText(
      hud_overfont, s, ping_column_end - V_FontStringWidth(hud_overfont, s), y
   );

   // [CG] Draw row separator.
   y += (V_FontStringHeight(hud_overfont, s));

   V_ColorBlockScaled(
      &vbscreen, white, scores_x, y, WINDOW_WIDTH - (SCORES_MARGIN * 2), 1
   );

   y += 3;

   // [CG] Draw stat rows (names, etc.).

   for(i = 1; i < MAXPLAYERS; i++)
   {
      player = &players[i];
      client = &clients[i];

      if(!playeringame[i] || client->spectating)
      {
         // [CG] List spectators after playing players.
         continue;
      }

      if(i == displayplayer)
      {
         font_color  = CR_GRAY;
      }
      else
      {
         font_color = CR_GREEN;
      }

      // [CG] Draw the player's name.
      V_FontWriteTextColored(
         hud_overfont, player->name, font_color,
         scores_x, y
      );

      // [CG] Columns:
      //
      //   - Name
      //   - Ammo (bullets, shells, rockets, cells)
      //   - Health/Armor
      //   - Kills
      //   - Lives remaining
      //   - Ping
      //

      psnprintf(s, sizeof(s), "%d/%d/%d/%d",
         player->ammo[am_clip],
         player->ammo[am_shell],
         player->ammo[am_misl],
         player->ammo[am_cell]
      );
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         ammo_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      psnprintf(s, sizeof(s), "%d/%d", player->health, player->armorpoints);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         hp_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      psnprintf(s, sizeof(s), "%u", player->killcount);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         kills_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      psnprintf(s, sizeof(s), "%u", 0);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         lives_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      psnprintf(s, sizeof(s), "%3ums", client->transit_lag);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         ping_column_end - V_FontStringWidth(hud_overfont, s), y
      );
      y += V_FontStringHeight(hud_overfont, s);
   }

   y += 1;

   for(i = 1; i < MAXPLAYERS; i++)
   {
      player = &players[i];
      client = &clients[i];

      if(!playeringame[i] || !client->spectating)
      {
         // [CG] List spectators after playing players.
         continue;
      }

      if(i == displayplayer)
      {
         font_color  = CR_GRAY;
      }
      else
      {
         font_color = CR_GREEN;
      }

      // [CG] Draw the player's name.
      psnprintf(s, sizeof(s), "%s%s", FC_TRANS, player->name);
      V_FontWriteTextColored(hud_overfont, s, font_color,
         scores_x, y
      );

      // [CG] Columns:
      //
      //   - Name
      //   - Ammo (bullets, shells, rockets, cells)
      //   - Health/Armor
      //   - Kills
      //   - Lives remaining
      //   - Ping
      //

      psnprintf(s, sizeof(s), "%s%d/%d/%d/%d", FC_TRANS,
         player->ammo[am_clip],
         player->ammo[am_shell],
         player->ammo[am_misl],
         player->ammo[am_cell]
      );
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         ammo_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      psnprintf(s, sizeof(s), "%s%d/%d", FC_TRANS,
         player->health,
         player->armorpoints
      );
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         hp_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      psnprintf(s, sizeof(s), "%s%u", FC_TRANS, player->killcount);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         kills_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      psnprintf(s, sizeof(s), "%s%u", FC_TRANS, 0);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         lives_column_end - V_FontStringWidth(hud_overfont, s), y
      );

      psnprintf(s, sizeof(s), "%s%3ums", FC_TRANS, client->transit_lag);
      V_FontWriteTextColored(
         hud_overfont, s, font_color,
         ping_column_end - V_FontStringWidth(hud_overfont, s), y
      );
      y += V_FontStringHeight(hud_overfont, s);
   }
}

void CS_DrawScoreboard(unsigned int extra_top_margin)
{

   if(GameType == gt_dm)
   {
      if(CS_TEAMS_ENABLED)
      {
         display_team_scoreboard(extra_top_margin);
      }
      else
      {
         display_deathmatch_scoreboard(extra_top_margin);
      }
   }
   else
   {
      display_coop_scoreboard(extra_top_margin);
   }

   fragsdrawn = true;
}

void CS_ShowScores(void)
{
   if(!show_scores || !action_frags || automapactive || walkcam_active)
   {
      return;
   }

   if (viewwidth < 320 || viewwidth < 200)
   {
      // [CG] Don't even try to draw the scoreboard in this case.
      return;
   }
   CS_DrawScoreboard(0);
}

