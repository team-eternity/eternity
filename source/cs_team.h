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

#ifndef __CS_TEAM_H__
#define __CS_TEAM_H__

#include "doomdef.h"
#include "doomdata.h"

#define CS_TEAMS_ENABLED ((clientserver) && (cs_settings->number_of_teams > 0))
#define CS_TEAMDM \
   (CS_TEAMS_ENABLED && GameType == gt_dm && cs_settings->ctf == false)
#define CS_CTF \
   (CS_TEAMS_ENABLED && GameType == gt_dm && cs_settings->ctf == true)
#define MAX_TEAM_STARTS 32 // [CG] 32 starts per team should be plenty.

// [CG] These are in order of max_teams, so max_teams = 2 means players can
//      join the game on the red and blue teams, max_teams = 4 means players
//      can join the game on the red, blue, green and white teams, and so on.
typedef enum
{
   team_color_none,
   team_color_red,
   team_color_blue,
   /*
   team_color_green,
   team_color_white,
   team_color_black,
   team_color_purple,
   team_color_yellow,
   */
   team_color_max,
} teamcolor_t;

/*
 * 5080: Blue Start
 * 5081: Red Start
 * 5130: Blue Flag
 * 5131: Red Flag
 */

enum
{
   // red_team_start    = 9027, // [CG] Red Particle Fountain
   // blue_team_start   = 9029, // [CG] Blue Particle Fountain
   blue_team_start   = 5080,
   red_team_start    = 5081,
   /*
   green_team_start  = 9028, // [CG] Green Particle Fountain
   yellow_team_start = 9030, // [CG] Yellow Particle Fountain
   purple_team_start = 9031, // [CG] Purple Particle Fountain
   black_team_start  = 9032, // [CG] Black Particle Fountain
   white_team_start  = 9033, // [CG] White Particle Fountain
   */
};

typedef struct team_start_s
{
   mapthing_t *mthing;
   teamcolor_t team_color;
} team_start_t;

extern char *team_color_names[team_color_max];
extern unsigned int team_colormaps[team_color_max];
extern int team_scores[team_color_max];
extern int team_start_counts_by_team[team_color_max];
extern mapthing_t **team_starts_by_team;

void CS_InitTeams(void);
void CS_AddTeamStart(mapthing_t *mthing);
unsigned int CS_GetTeamPlayerCount(teamcolor_t team_color);

#endif

