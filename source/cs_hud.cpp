// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
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
//   Queue HUD widget.
//
//----------------------------------------------------------------------------

#include <list>

#include "c_runcmd.h"
#include "d_event.h"
#include "d_gi.h"
#include "g_bind.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "i_thread.h"
#include "psnprntf.h"
#include "r_draw.h"
#include "st_stuff.h"
#include "v_block.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"

#include "cs_hud.h"
#include "cs_main.h"
#include "cs_vote.h"
#include "cl_buf.h"
#include "cl_main.h"
#include "cl_vote.h"

bool show_timer = false;
bool default_show_timer = false;
bool show_netstats = false;
bool default_show_netstats = false;
bool show_team_widget = false;
bool default_show_team_widget = false;
bool cs_chat_active = false;

extern vfont_t *hud_font;

#define TIMER_SIZE 10
#define CHAT_BUFFER_SIZE (MAX_STRING_SIZE + 15)

static char cs_chat_input[MAX_STRING_SIZE];
static message_recipient_e current_recipient;
static hu_widget_t *queue_widget = NULL;
static hu_widget_t *chat_widget = NULL;
static hu_widget_t *timer_widget = NULL;
static hu_widget_t *net_widget = NULL;
static hu_widget_t *team_widget = NULL;
static hu_widget_t *vote_widget = NULL;

bool CS_ChatResponder(event_t *ev)
{
   char ch = 0;
   char cmd_buffer[CHAT_BUFFER_SIZE];
   static bool shiftdown;

   // haleyjd 06/11/08: get HUD actions
   G_KeyResponder(ev, kac_hud);

   if(ev->type != ev_keydown)
      return false;

   if(ev->data1 == KEYD_RSHIFT)
      shiftdown = true;

   // [CG] TODO: Figure out player-to-player messaging, as in, how to choose
   //            the receiving player.

   if(!cs_chat_active)
   {
      // [CG] You can't bind a key to start chatting with a specific player
      //      number, so there is no key_playerchat.
      if(action_message_all    ||
         action_message_team   ||
         action_message_server ||
         action_rcon)
      {
         cs_chat_active = true;
         cs_chat_input[0] = 0;

         if(action_rcon)
         {
            current_recipient = mr_rcon;
            action_rcon = false;
         }
         else if(action_message_server)
         {
            current_recipient = mr_server;
            action_message_server = false;
         }
         else if(action_message_team)
         {
            current_recipient = mr_team;
            action_message_team = false;
         }
         else
         {
            current_recipient = mr_all;
            action_message_all = false;
         }
         return true;
      }
      return false;
   }

   if(ev->data1 == KEYD_ESCAPE)    // kill chat
   {
      cs_chat_active = false;
      return true;
   }

   if(ev->data1 == KEYD_BACKSPACE && cs_chat_input[0])
   {
      cs_chat_input[strlen(cs_chat_input)-1] = 0; // remove last char
      return true;
   }

   if(ev->data1 == KEYD_ENTER)
   {
      if(current_recipient == mr_rcon)
      {
         psnprintf(cmd_buffer, sizeof(cmd_buffer), "%s \"%s\"",
            "rcon", cs_chat_input
         );
      }
      else if(current_recipient == mr_server)
      {
         psnprintf(cmd_buffer, sizeof(cmd_buffer), "%s \"%s\"",
            "to_server", cs_chat_input
         );
      }
      else if(current_recipient == mr_team)
      {
         psnprintf(cmd_buffer, sizeof(cmd_buffer), "%s \"%s\"",
            "to_team", cs_chat_input
         );
      }
      else
      {
         psnprintf(cmd_buffer, sizeof(cmd_buffer), "%s \"%s\"",
            "say", cs_chat_input
         );
      }
      C_RunTextCmd(cmd_buffer);
      cs_chat_active = false;
      return true;
   }

   if(ev->character)
      ch = ev->character;
   else if(ev->data1 > 31 && ev->data1 < 127)
      ch = shiftdown ? shiftxform[ev->data1] : ev->data1; // shifted?

   if(ch > 31 && ch < 127)
   {
      if(strlen(cs_chat_input) < MAX_STRING_SIZE)
      {
         psnprintf(
            cs_chat_input, sizeof(cs_chat_input), "%s%c", cs_chat_input, ch
         );
      }
      return true;
   }
   return false;
}

static void CS_TimerWidgetTick(hu_widget_t *widget)
{
   hu_textwidget_t *tw = (hu_textwidget_t *)widget;

   if(show_timer)
      CS_FormatTicsAsTime(tw->message, leveltime);
   else
      tw->message[0] = 0;
}

static void CS_ChatTick(hu_widget_t *widget)
{
   hu_textwidget_t *cw = (hu_textwidget_t *)widget;

   if(cs_chat_active)
   {
      memset(cw->message, 0, CHAT_BUFFER_SIZE);
      if(current_recipient == mr_rcon)
      {
         psnprintf(
            cw->message, MAX_STRING_SIZE + 8, "%s> %s_", "RCON", cs_chat_input
         );
      }
      else if(current_recipient == mr_server)
      {
         psnprintf(
            cw->message,
            MAX_STRING_SIZE + 10,
            "%s> %s_",
            "SERVER",
            cs_chat_input
         );
      }
      else if(current_recipient == mr_team)
      {
         psnprintf(
            cw->message, MAX_STRING_SIZE + 8, "%s> %s_", "TEAM", cs_chat_input
         );
      }
      else
      {
         psnprintf(
            cw->message, MAX_STRING_SIZE + 7, "%s> %s_", "SAY", cs_chat_input
         );
      }
   }
   else
      cw->message[0] = 0;
}

void CS_DrawChatWidget(void)
{
   hu_widget_t *w;
   
   w = HU_WidgetForName("_HU_CSChatWidget");
   if(!w->disabled)
   {
      w->ticker(w);
      w->drawer(w);
   }

   w = HU_WidgetForName("_HU_MsgWidget");
   if(!w->disabled)
   {
      w->ticker(w);
      w->drawer(w);
   }
}

void CS_InitTimerWidget(void)
{
   hu_textwidget_t *tw;

   if(timer_widget != NULL)
   {
      efree(((hu_textwidget_t *)timer_widget)->message);
      efree(timer_widget->name);
      efree(timer_widget);
      timer_widget = NULL;
   }

   HU_DynamicTextWidget(
      "_HU_CSTimerWidget",
      SCREENWIDTH - (V_FontStringWidth(hud_font, "999999:99") + 5),
      ST_Y - 16,
      hud_font->num,
      "999999:99",
      0,
      TW_NOCLEAR
   );

   timer_widget = HU_WidgetForName("_HU_CSTimerWidget");
   timer_widget->prevdisabled = timer_widget->disabled;
   timer_widget->disabled = false;
   timer_widget->ticker = CS_TimerWidgetTick;

   tw = (hu_textwidget_t *)timer_widget;
   tw->color = CR_GRAY + 1;
}

void CS_InitChatWidget(void)
{
   hu_textwidget_t *cw;

   if(chat_widget != NULL)
   {
      efree(((hu_textwidget_t *)chat_widget)->message);
      efree(chat_widget->name);
      efree(chat_widget);
      chat_widget = NULL;
   }

   HU_DynamicTextWidget(
      "_HU_CSChatWidget",
      0,
      ST_Y - 32,
      hud_font->num,
      "",
      0,
      TW_NOCLEAR
   );

   chat_widget = HU_WidgetForName("_HU_CSChatWidget");
   chat_widget->prevdisabled = chat_widget->disabled;
   chat_widget->disabled = false;
   chat_widget->ticker = CS_ChatTick;

   cw = (hu_textwidget_t *)chat_widget;
   cw->color = CR_GREEN + 1;
   efree(cw->message);
   cw->message = ecalloc(char *, CHAT_BUFFER_SIZE, sizeof(char));
}

void CS_InitQueueWidget(void)
{
   if(queue_widget != NULL)
   {
      efree(((hu_textwidget_t *)queue_widget)->message);
      efree(queue_widget->name);
      efree(queue_widget);
      queue_widget = NULL;
   }

   HU_DynamicTextWidget(
      "_HU_CSQueueWidget",
      0, 0,
      hud_font->num,
      FC_ABSCENTER "Press [USE] to join",
      0,
      TW_NOCLEAR
   );

   queue_widget = HU_WidgetForName("_HU_CSQueueWidget");
   queue_widget->prevdisabled = queue_widget->disabled;
   if(!clientserver)
   {
      // [CG] So the queue message doesn't display during single player.
      queue_widget->disabled = true;
   }
   else
      queue_widget->disabled = false;
}

void CS_UpdateQueueMessage(void)
{
   client_t *client = &clients[consoleplayer];
   hu_widget_t *tw = HU_WidgetForName("_HU_CSQueueWidget");
   hu_textwidget_t *cw = (hu_textwidget_t *)tw;

   tw->prevdisabled = tw->disabled;

   if(QUEUE_MESSAGE_DISPLAYED)
   {
      if(cw->alloc)
      {
         efree(cw->alloc);
         cw->alloc = NULL;
      }

      tw->disabled = false;
      if(client->queue_level == ql_waiting)
      {
         cw->message = cw->alloc = ecalloc(char *, 21, sizeof(char));
         psnprintf(
            cw->alloc,
            21,
            FC_ABSCENTER "You are player %d",
            client->queue_position
         );
      }
      else if(client->queue_level == ql_none || client->spectating)
         cw->message = cw->alloc = estrdup(FC_ABSCENTER "Press [USE] to join");
      else
         cw->message = cw->alloc = estrdup(FC_ABSCENTER);
   }
   else
      tw->disabled = true;
}

static void CS_NetWidgetTick(hu_widget_t *widget)
{
   hu_textwidget_t *tw = (hu_textwidget_t *)widget;

   if(show_netstats)
   {
      psnprintf(tw->message, 21, "%5u/%3u/%3u/%3u%%",
         clients[consoleplayer].client_lag,
         cl_packet_buffer.size(),
         clients[consoleplayer].server_lag,
         100 - clients[consoleplayer].packet_loss
      );
   }
   else
      tw->message[0] = 0;
}

void CS_InitNetWidget(void)
{
   hu_textwidget_t *tw;

   if(net_widget != NULL)
   {
      efree(((hu_textwidget_t *)net_widget)->message);
      efree(net_widget->name);
      efree(net_widget);
      net_widget = NULL;
   }

   HU_DynamicTextWidget(
      "_HU_CSNetWidget",
      SCREENWIDTH - (V_FontStringWidth(hud_font, "00000/000/000/000%") + 5),
      ST_Y - (16 + V_FontStringHeight(hud_font, "00000/000/000/000%")),
      hud_font->num,
      "00000/000/000/000%"
      "0000000000000000000000000",
      0,
      TW_NOCLEAR
   );

   net_widget = HU_WidgetForName("_HU_CSNetWidget");
   net_widget->prevdisabled = net_widget->disabled;
   net_widget->disabled = false;
   net_widget->ticker = CS_NetWidgetTick;

   tw = (hu_textwidget_t *)net_widget;
   tw->color = CR_GREEN + 1;
}

static void CS_TeamWidgetTick(hu_widget_t *widget) {}

static void CS_TeamWidgetDraw(hu_widget_t *widget)
{
   int x;
   hu_customwidget_t *cw;
   patch_t *p;
   static char score_buffer[5];
   client_t *client;
   byte red_font_color, blue_font_color;

   if(widget->disabled || !show_team_widget)
      return;

   cw = (hu_customwidget_t *)widget;
   client = &clients[displayplayer];

   if(cw->bg_opacity < FRACUNIT)
   {
      V_ColorBlockTLScaled(
         &vbscreen,
         cw->bg_color,
         cw->x,
         cw->y,
         cw->width,
         cw->height,
         cw->bg_opacity
      );
   }

   if(client->team == team_color_red)
   {
      V_ColorBlockScaled(
         &vbscreen,
         GameModeInfo->whiteIndex,
         cw->x,
         cw->y + 2,
         2,
         (cw->height >> 1) - 4
      );
   }
   else if(client->team == team_color_blue)
   {
      V_ColorBlockScaled(
         &vbscreen,
         GameModeInfo->whiteIndex,
         cw->x,
         cw->y + 2 + (cw->height >> 1),
         2,
         (cw->height >> 1) - 4
      );
   }

   x = cw->x + 2;

   if(GameType == gt_ctf)
   {
      if((p = CS_GetFlagPatch(team_color_red)))
         V_DrawPatch(cw->x + 4, cw->y + 2, &vbscreen, p);

      if((p = CS_GetFlagPatch(team_color_blue)))
         V_DrawPatch(cw->x + 4, cw->y + 3 + (cw->height >> 1), &vbscreen, p);

      x += 16;
   }
   else
      x += 2;

   if(GameType == gt_ctf)
      red_font_color = blue_font_color = CR_GRAY;
   else
   {
      red_font_color = CR_RED;
      blue_font_color = CR_BLUE;
   }

   sprintf(score_buffer, "%4d", team_scores[team_color_red] % 1000);
   V_FontWriteTextColored(
      hud_font,
      score_buffer,
      red_font_color,
      x,
      cw->y + 4
   );

   sprintf(score_buffer, "%4d", team_scores[team_color_blue] % 1000);
   V_FontWriteTextColored(
      hud_font,
      score_buffer,
      blue_font_color,
      x,
      cw->y + 5 + (cw->height >> 1)
   );
}

void CS_InitTeamWidget(void)
{
   if(team_widget != NULL)
   {
      efree(team_widget->name);
      efree(team_widget);
      team_widget = NULL;
   }

   if(GameType == gt_ctf)
   {
      HU_DynamicCustomWidget(
         "_HU_CSTeamWidget",
         SCREENWIDTH - 48, ST_Y - 68,
         48, 34,
         GameModeInfo->blackIndex,
         FRACUNIT >> 1
      );
   }
   else
   {
      HU_DynamicCustomWidget(
         "_HU_CSTeamWidget",
         SCREENWIDTH - 36, ST_Y - 68,
         36, 34,
         GameModeInfo->blackIndex,
         FRACUNIT >> 1
      );
   }

   team_widget = HU_WidgetForName("_HU_CSTeamWidget");
   team_widget->drawer = CS_TeamWidgetDraw;
   team_widget->ticker = CS_TeamWidgetTick;
   team_widget->prevdisabled = false;

   if(CS_TEAMS_ENABLED)
      team_widget->disabled = false;
   else
      team_widget->disabled = true;
}

static void CS_VoteWidgetTick(hu_widget_t *widget)
{
   char *index;
   hu_textwidget_t *tw = (hu_textwidget_t *)widget;
   ClientVote *vote = CL_GetCurrentVote();
   unsigned int max_votes, blank_votes, blank_votes_left, blank_votes_right;
   unsigned int saved_yeas, yea_votes, saved_nays, nay_votes;
   bool changed_color = false;

   static int max_width = 0;
   static int min_width = 72;

   if(!CS_CLIENT)
      return;

   if(gamestate != GS_LEVEL)
      return;

   if(!vote)
   {
      if(!widget->disabled)
      {
         widget->prevdisabled = widget->disabled;
         widget->disabled = true;
         tw->message[0] = 0;
      }
      max_width = 0;
      return;
   }
   else if(widget->disabled)
   {
      if(!max_width)
      {
         qstring buf;
         int max_stars = vote->getMaxVotes();
         int star_width, command_width;

         if(max_stars)
         {
            while(max_stars--)
               buf += "*";
            star_width = V_FontStringWidth(hud_font, buf.constPtr());
         }
         else
            star_width = 0;

         buf = vote->getCommand();
         command_width = V_FontStringWidth(hud_font, buf.constPtr());

         if(star_width > command_width)
            max_width = star_width;
         else
            max_width = command_width;

         if(max_width < min_width)
            max_width = min_width;

         tw->x = SCREENWIDTH - (max_width + 10);
      }
      widget->prevdisabled = widget->disabled;
      widget->disabled = false;
   }

   max_votes = vote->getMaxVotes();
   saved_yeas = yea_votes = vote->getYeaVotes();
   saved_nays = nay_votes = vote->getNayVotes();
   blank_votes = max_votes - (yea_votes + nay_votes);
   blank_votes_left = blank_votes >> 1;
   blank_votes_right = blank_votes - blank_votes_left;

   index = tw->message;
   index += sprintf(index, "%c", TEXT_COLOR_GRAY);
   index += sprintf(index, "Vote (%u):\n", vote->timeRemaining());
   index += sprintf(index, "%s\n", vote->getCommand());

   if(blank_votes_left)
   {
      while(blank_votes_left--)
         index += sprintf(index, "*");
   }

   if(yea_votes)
   {
      index += sprintf(index, "%c", TEXT_COLOR_GREEN);
      changed_color = true;
      while(yea_votes--)
         index += sprintf(index, "*");
   }

   if(nay_votes)
   {
      index += sprintf(index, "%c", TEXT_COLOR_RED);
      changed_color = true;
      while(nay_votes--)
         index += sprintf(index, "*");
   }

   if(changed_color)
      index += sprintf(index, "%c", TEXT_COLOR_GRAY);

   if(blank_votes_right)
   {
      while(blank_votes_right--)
         index += sprintf(index, "*");
   }

   index += sprintf(index, "\nY: %u | N: %u", saved_yeas, saved_nays);
   *index = 0;
}

void CS_InitVoteWidget(void)
{
   hu_textwidget_t *tw;
   const char *xs = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n";

   if(vote_widget != NULL)
   {
      efree(((hu_textwidget_t *)vote_widget)->message);
      efree(vote_widget->name);
      efree(vote_widget);
      vote_widget = NULL;
   }

   HU_DynamicTextWidget(
      "_HU_CSVoteWidget",
      SCREENWIDTH - V_FontStringWidth(hud_font, xs),
      (ST_Y >> 1) - 30,
      hud_font->num,
      "Vote (999s):\n"
      "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
      "********************************\n"
      "Y: 32 | N: 32"
      "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
      0,
      TW_NOCLEAR
   );

   vote_widget = HU_WidgetForName("_HU_CSVoteWidget");
   vote_widget->prevdisabled = true;
   vote_widget->disabled = true;
   vote_widget->ticker = CS_VoteWidgetTick;

   tw = (hu_textwidget_t *)vote_widget;
   tw->color = 0;
   tw->flags |= TW_TRANS;
   tw->bg_opacity = FRACUNIT >> 1;
}

