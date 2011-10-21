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
//   C/S Announcer support.
//
//----------------------------------------------------------------------------

#ifndef CS_ANNOUNCER_H__
#define CS_ANNOUNCER_H__

#include "e_sound.h"
#include "s_sound.h"

#include "cs_ctf.h"
#include "cs_main.h"
#include "cs_team.h"

typedef struct
{
   const char* name;
   const char* sound_name;
   const char* message;
} announcer_event_t;

typedef enum
{
   ae_flag_taken,
   ae_enemy_flag_taken,
   ae_flag_dropped,
   ae_enemy_flag_dropped,
   ae_flag_returned,
   ae_enemy_flag_returned,
   ae_flag_captured,
   ae_enemy_flag_captured,
   max_announcer_event_types
} announcer_event_type_e;

extern announcer_event_t null_announcer_events[];
extern announcer_event_t quake_announcer_events[];
extern announcer_event_t red_unreal_tournament_announcer_events[];
extern announcer_event_t blue_unreal_tournament_announcer_events[];

bool               CS_AnnouncerEnabled(void);
void               CS_SetAnnouncer(announcer_event_t *events);
void               CS_InitAnnouncer(void);
announcer_event_t* CS_GetAnnouncerEvent(uint32_t event_index);

#endif

