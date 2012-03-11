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
//   Client/Server scoreboard drawing.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include <stdio.h>

#include "c_io.h"
#include "d_player.h"
#include "d_gi.h"
#include "doomdef.h"
#include "doomstat.h"
#include "st_stuff.h"

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

extern bool fragsdrawn;
extern int num_players;
extern int show_scores;
extern player_t *sortedplayers[MAXPLAYERS];
extern int action_scoreboard;
extern int walkcam_active;
extern int levelScoreLimit;

/*
 * [CG] The way these have to work is:
 *      - Surface for game type
 *      - Surface for time/frag/score limit
 *      - Surface for timer
 *      - For team games:
 *        - Surfaces for team names
 *        - Surfaces for team scores
 *      - Surfaces for each connected client
 *
 *      This probably means the days of player->frags[targetnum]++ are over,
 *      replaced by P_IncrementFrags(int fraggernum, int frageenum), etc. so
 *      they can also mark that client's surface as needing refreshed.
 *
 *      Additionally, it probably makes sense to build the capability to
 *      arrange client surfaces in various ways.
 *      - Team games:
 *        - Which team comes first
 *        - Split teams horizontally or vertically
 *      - Client stat columns
 *        - Name is not optional
 *          - Minimum width; I'm guessing enough for "[SUC]Potentpotables...",
 *            past which a name will be ellipsized if there isn't enough
 *            horizontal space.
 *        - Which columns to display
 *        - Allow/force/forbid linebreaks in column display
 *          - If linebreaks are forbidden, column names are printed as a "first
 *            row"; otherwise they're printed as labels, i.e. "ping: 38ms".
 *        - Every column has width & height; so running out of space can be
 *          caught and denied.
 */

