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

static announcer_event_t *cs_announcer_events = NULL;

// [CG] TODO: It would be better if these could be defined somewhere instead of
//            being hard-coded.

announcer_event_t quake_announcer_events[] =
{
   { "Friendly Flag Taken", "FriendlyFlagTaken", "Flag Taken" },
   { "Enemy Flag Taken", "EnemyFlagTaken", "Enemy Flag Taken" },
   { "Friendly Flag Dropped", "FriendlyFlagDropped", "Flag Dropped" },
   { "Enemy Flag Dropped", "EnemyFlagDropped", "Enemy Flag Dropped" },
   { "Friendly Flag Returned", "FriendlyFlagReturned", "Flag Returned" },
   { "Enemy Flag Returned", "EnemyFlagReturned", "Enemy Flag Returned" },
   { "Friendly Flag Captured", "FriendlyFlagCaptured", "Flag Captured" },
   { "Enemy Flag Captured", "EnemyFlagCaptured", "Enemy Flag Captured" },
};

announcer_event_t red_unreal_tournament_announcer_events[] =
{
   { "Red Flag Taken", "RedFlagTaken", "Red Flag Taken" },
   { "Blue Flag Taken", "BlueFlagTaken", "Blue Flag Taken" },
   { "Red Flag Dropped", "RedFlagDropped", "Red Flag Dropped" },
   { "Blue Flag Dropped", "BlueFlagDropped", "Blue Flag Dropped" },
   { "Red Flag Returned", "RedFlagReturned", "Red Flag Returned" },
   { "Blue Flag Returned", "BlueFlagReturned", "Blue Flag Returned" },
   { "Red Flag Captured", "RedFlagCaptured", "Blue Team Scores" },
   { "Blue Flag Captured", "BlueFlagCaptured", "Red Team Scores" },
};

announcer_event_t blue_unreal_tournament_announcer_events[] =
{
   { "Blue Flag Taken", "BlueFlagTaken", "Blue Flag Taken" },
   { "Red Flag Taken", "RedFlagTaken", "Red Flag Taken" },
   { "Blue Flag Dropped", "BlueFlagDropped", "Blue Flag Dropped" },
   { "Red Flag Dropped", "RedFlagDropped", "Red Flag Dropped" },
   { "Blue Flag Returned", "BlueFlagReturned", "Blue Flag Returned" },
   { "Red Flag Returned", "RedFlagReturned", "Red Flag Returned" },
   { "Blue Flag Captured", "BlueFlagCaptured", "Red Team Scores" },
   { "Red Flag Captured", "RedFlagCaptured", "Blue Team Scores" },
};

void CS_SetAnnouncer(announcer_event_t *events)
{
   cs_announcer_events = events;
}

announcer_event_t* CS_GetAnnouncerEvent(uint32_t event_index)
{
   if(event_index >= max_announcer_event_types)
      return NULL;

   return &cs_announcer_events[event_index];
}

