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

#ifndef CS_TEAM_H__
#define CS_TEAM_H__

#include "doomdef.h"
#include "doomdata.h"

#define CS_TEAMS_ENABLED (\
   (GameType == gt_tdm) || \
   (GameType == gt_ctf)\
)

// [CG] These are in order of max_teams, so max_teams = 2 means players can
//      join the game on the red and blue teams, max_teams = 4 means players
//      can join the game on the red, blue, green and white teams, and so on.
enum
{
   team_color_none,
   team_color_red,
   team_color_blue,
   team_color_max,
};

// 5080: Blue Start
// 5081: Red Start
// 5130: Blue Flag
// 5131: Red Flag

enum
{
   blue_team_start   = 5080,
   red_team_start    = 5081,
};

typedef struct team_start_s
{
   mapthing_t *mthing;
   int team_color;
} team_start_t;

extern const char *team_color_names[team_color_max];
extern unsigned int team_colormaps[team_color_max];
extern int team_scores[team_color_max];
extern int team_start_counts_by_team[team_color_max];
extern mapthing_t **team_starts_by_team;

void CS_InitTeams(void);
void CS_AddTeamStart(mapthing_t *mthing);
unsigned int CS_GetTeamPlayerCount(int team_color);

#endif

