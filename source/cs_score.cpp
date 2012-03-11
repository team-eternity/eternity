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
#include "v_block.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_video.h"

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
extern int show_scores;
extern int action_scoreboard;
extern int walkcam_active;

BaseScoreboard *cs_scoreboard = NULL;
static LowResCoopScoreboard *lo_res_coop_scoreboard = NULL;
static LowResDeathmatchScoreboard *lo_res_dm_scoreboard = NULL;
static LowResTeamDeathmatchScoreboard *lo_res_teamdm_scoreboard = NULL;
static HighResCoopScoreboard *hi_res_coop_scoreboard = NULL;
static HighResDeathmatchScoreboard *hi_res_dm_scoreboard = NULL;
static HighResTeamDeathmatchScoreboard *hi_res_teamdm_scoreboard = NULL;

BaseScoreboard::BaseScoreboard()
   : ZoneObject(), testing(false), m_x(0), m_y(0) {}

BaseScoreboard::~BaseScoreboard() {}

void BaseScoreboard::display() {}

void BaseScoreboard::setClientNeedsRepainted(int clientnum) {}

void BaseScoreboard::setX(unsigned int new_x) { m_x = new_x; }

void BaseScoreboard::setY(unsigned int new_y) { m_y = new_y; }

unsigned int BaseScoreboard::getX() const { return m_x; }

unsigned int BaseScoreboard::getY() const { return m_y; }

void BaseScoreboard::setTesting(bool new_testing) { testing = new_testing; }

void CS_InitScoreboard()
{
   if(cs_scoreboard)
      delete cs_scoreboard;

   if(CS_HEADLESS)
   {
      cs_scoreboard = new BaseScoreboard();
   }
   else if(GameType != gt_dm)
   {
      lo_res_coop_scoreboard = new LowResCoopScoreboard();
      cs_scoreboard = lo_res_coop_scoreboard;
   }
   else if(CS_TEAMS_ENABLED)
   {
      lo_res_teamdm_scoreboard = new LowResTeamDeathmatchScoreboard();
      cs_scoreboard = lo_res_teamdm_scoreboard;
   }
   else
   {
      lo_res_dm_scoreboard = new LowResDeathmatchScoreboard();
      cs_scoreboard = lo_res_dm_scoreboard;
   }
}

void CS_DrawScoreboard(unsigned int extra_top_margin)
{
   cs_scoreboard->setY(extra_top_margin);
   cs_scoreboard->display();
   fragsdrawn = true;
}

void CS_ShowScores()
{
   // [CG] Show scoreboard when dead.
   if(players[displayplayer].playerstate == PST_LIVE)
   {
      if(!show_scores || !action_scoreboard || automapactive || walkcam_active)
         return;
   }

   // [CG] Don't even try to draw the scoreboard in this case.
   if (viewwidth < 320 || viewwidth < 200)
      return;

   CS_DrawScoreboard(0);
}

