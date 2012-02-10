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
//   Client/Server HUD widgets.
//
//----------------------------------------------------------------------------

#ifndef CS_HUD_H__
#define CS_HUD_H__

#include "d_event.h"
#include "hu_stuff.h"

#include "cl_main.h"

#define QUEUE_MESSAGE_DISPLAYED \
   (CS_CLIENT && (clients[consoleplayer].queue_level == ql_waiting || \
                  clients[consoleplayer].queue_level == ql_none || \
                  clients[consoleplayer].spectating))

extern bool show_timer;
extern bool default_show_timer;
extern bool show_netstats;
extern bool default_show_netstats;
extern bool show_team_widget;
extern bool default_show_team_widget;

extern bool cs_chat_active;

bool CS_ChatResponder(event_t *ev);
void CS_UpdateQueueMessage(void);
void CS_DrawChatWidget(void);
void CS_InitNetWidget(void);
void CS_InitTimerWidget(void);
void CS_InitChatWidget(void);
void CS_InitQueueWidget(void);
void CS_InitNetWidget(void);
void CS_InitTeamWidget(void);
void CS_InitVoteWidget(void);

#endif

