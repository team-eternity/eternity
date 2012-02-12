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
//   Routines for handling client/server teams.
//
//----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomdef.h"
#include "doomdata.h"
#include "doomstat.h"
#include "i_system.h"

#include "cs_team.h"
#include "cs_main.h"

int number_of_team_starts = 0;
int team_scores[team_color_max];
int team_start_counts_by_team[team_color_max];
mapthing_t **team_starts_by_team = NULL;

const char *team_color_names[team_color_max] = { "none", "red", "blue" };

unsigned int team_colormaps[team_color_max] = { 0, 3, 6 };

void CS_InitTeams(void)
{
   int color;

   number_of_team_starts = 0;

   if(team_starts_by_team == NULL)
   {
      team_starts_by_team =
         ecalloc(mapthing_t **, team_color_max, sizeof(mapthing_t *));
   }

   for(color = team_color_none; color < team_color_max; color++)
   {
      if(team_starts_by_team[color])
      {
         efree(team_starts_by_team[color]);
         team_starts_by_team[color] = NULL;
      }
      team_start_counts_by_team[color] = 0;
   }
}

void CS_AddTeamStart(mapthing_t *team_start)
{
   int thing_index;
   int team_color;

   switch(team_start->type)
   {
   case red_team_start:
      team_color = team_color_red;
      break;
   case blue_team_start:
      team_color = team_color_blue;
      break;
   default:
      I_Error("Unknown team start type %d.\n", team_start->type);
      break;
    }

   if(team_start_counts_by_team[team_color] >= 100)
      I_Error("CS_AddTeamStart: Excessive team starts.\n");

   thing_index = team_start_counts_by_team[team_color];
   team_starts_by_team[team_color] = erealloc(
      mapthing_t*,
      team_starts_by_team[team_color],
      ++team_start_counts_by_team[team_color] * sizeof(mapthing_t)
   );
   memcpy(
      &team_starts_by_team[team_color][thing_index],
      team_start,
      sizeof(mapthing_t)
   );
   number_of_team_starts++;
}

unsigned int CS_GetTeamPlayerCount(int team_color)
{
   client_t *client;
   unsigned int i, playercount;

   for(playercount = 0, i = 0; i < MAXPLAYERS; i++)
   {
      client = &clients[i];
      if(playeringame[i] &&
         client->team == team_color &&
         client->queue_level == ql_playing)
      {
         playercount++;
      }
   }
   return playercount;
}

int CS_GetOtherTeam(int team_color)
{
   if(team_color == team_color_red)
      return team_color_blue;
   else if(team_color == team_color_blue)
      return team_color_red;

   return team_color_none;
}

