// Emacs style mode select -*- C++ -*- vim:sw=3 ts=3:
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

#ifndef __CS_HUD_H__
#define __CS_HUD_H__

#include "d_event.h"
#include "hu_stuff.h"

#include "cl_main.h"

#define QUEUE_MESSAGE_DISPLAYED \
   (CS_CLIENT && (clients[consoleplayer].queue_level == ql_waiting || \
                  clients[consoleplayer].queue_level == ql_none || \
                  clients[consoleplayer].spectating))

typedef struct hu_customwidget_s
{
   hu_widget_t widget;
   unsigned int x;
   unsigned int y;
   unsigned int width;
   unsigned int height;
   vfont_t *font;
   int bg_color;
   int bg_opacity;
} hu_customwidget_t;

extern boolean show_timer;
extern boolean default_show_timer;
extern boolean show_netstats;
extern boolean default_show_netstats;
extern boolean show_team_widget;
extern boolean default_show_team_widget;

extern boolean cs_chat_active;

boolean CS_ChatResponder(event_t *ev);
void CS_UpdateQueueMessage(void);
void CS_DrawChatWidget(void);
void CS_InitNetWidget(void);
void CS_InitTimerWidget(void);
void CS_InitChatWidget(void);
void CS_InitQueueWidget(void);
void CS_InitNetWidget(void);
void CS_InitTeamWidget(void);

#endif

