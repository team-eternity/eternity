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

#include "p_mobj.h"

#include "cs_announcer.h"
#include "cs_ctf.h"
#include "cs_team.h"
#include "sv_main.h"

// [CG] TODO: It would be better if these could be defined somewhere instead of
//            being hard-coded.

announcer_event_t null_announcer_events[1] = {NULL};

announcer_event_t quake_announcer_events[] =
{
   { "Friendly Flag Taken", "FriendlyFlagTaken", "Flag Taken" },
   { "Enemy Flag Taken", "EnemyFlagTaken", "Enemy Flag Taken" },
   { "Friendly Flag Dropped", "FriendlyFlagDropped", "Flag Dropped" },
   { "Enemy Flag Dropped", "EnemyFlagDropped", "Enemy Flag Dropped" },
   { "Friendly Flag Returned", "FriendlyFlagReturned", "Flag Returned" },
   { "Enemy Flag Returned", "EnemyFlagReturned", "Enemy Flag Returned" },
   { "Enemy Flag Captured", "EnemyFlagCaptured", "Enemy Flag Captured" },
   { "Friendly Flag Captured", "FriendlyFlagCaptured", "Flag Captured" },
};

announcer_event_t red_unreal_tournament_announcer_events[] =
{
   { "Red Flag Taken", "RedFlagTaken", "Red Flag Taken" },
   { "Blue Flag Taken", "BlueFlagTaken", "Blue Flag Taken" },
   { "Red Flag Dropped", "RedFlagDropped", "Red Flag Dropped" },
   { "Blue Flag Dropped", "BlueFlagDropped", "Blue Flag Dropped" },
   { "Red Flag Returned", "RedFlagReturned", "Red Flag Returned" },
   { "Blue Flag Returned", "BlueFlagReturned", "Blue Flag Returned" },
   { "Blue Flag Captured", "RedTeamScores", "Red Team Scores" },
   { "Red Flag Captured", "BlueTeamScores", "Blue Team Scores" },
};

announcer_event_t blue_unreal_tournament_announcer_events[] =
{
   { "Blue Flag Taken", "BlueFlagTaken", "Blue Flag Taken" },
   { "Red Flag Taken", "RedFlagTaken", "Red Flag Taken" },
   { "Blue Flag Dropped", "BlueFlagDropped", "Blue Flag Dropped" },
   { "Red Flag Dropped", "RedFlagDropped", "Red Flag Dropped" },
   { "Blue Flag Returned", "BlueFlagReturned", "Blue Flag Returned" },
   { "Red Flag Returned", "RedFlagReturned", "Red Flag Returned" },
   { "Red Flag Captured", "BlueTeamScores", "Blue Team Scores" },
   { "Blue Flag Captured", "RedTeamScores", "Red Team Scores" },
};

static announcer_event_t *cs_announcer_events = null_announcer_events;

bool CS_AnnouncerEnabled(void)
{
   if((s_announcer_type != S_ANNOUNCER_NONE) &&
      (cs_announcer_events != null_announcer_events))
      return true;
   return false;
}

void CS_SetAnnouncer(announcer_event_t *events)
{
   if(events)
      cs_announcer_events = events;
   else
      cs_announcer_events = null_announcer_events;
}

void CS_InitAnnouncer(void)
{
   if(s_announcer_type == S_ANNOUNCER_NONE)
      CS_SetAnnouncer(NULL);
   else if(s_announcer_type == S_ANNOUNCER_QUAKE)
      CS_SetAnnouncer(quake_announcer_events);
   else if(clients[consoleplayer].team == team_color_blue)
      CS_SetAnnouncer(blue_unreal_tournament_announcer_events);
   else
      CS_SetAnnouncer(red_unreal_tournament_announcer_events);
}

announcer_event_t* CS_GetAnnouncerEvent(uint32_t event_index)
{
   if(event_index >= max_announcer_event_types)
      return NULL;

   return &cs_announcer_events[event_index];
}

