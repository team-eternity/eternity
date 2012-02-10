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

#define INITIAL_ANNOUNCER_COUNT 8

Announcer *current_announcer = NULL;
static bool announcer_enabled = false;
static EHashTable<Announcer, ENCStringHashKey, &Announcer::name, &Announcer::links> announcers(INITIAL_ANNOUNCER_COUNT);

void CS_EnableAnnouncer()
{
   Announcer *ann = NULL;

   while((ann = announcers.tableIterator(ann)))
      ann->enable();

   announcer_enabled = true;
}

void CS_DisableAnnouncer()
{
   Announcer *ann = NULL;

   while((ann = announcers.tableIterator(ann)))
      ann->disable();

   announcer_enabled = false;
}

bool CS_SetAnnouncer(const char *name)
{
   Announcer *ann;
   
   if(!strncmp(name, "none", 4))
   {
      CS_DisableAnnouncer();
      return false;
   }

   if(!(ann = announcers.objectForKey(name)))
      return false;

   if(!announcer_enabled)
      CS_EnableAnnouncer();

   if(ann != current_announcer)
      current_announcer = ann;

   return true;
}

bool CS_AnnouncerEnabled(void)
{
   if(announcer_enabled)
      return true;

   return false;
}

void CS_UpdateTeamSpecificAnnouncers(teamcolor_t color)
{
   Announcer *ann = NULL;

   while((ann = announcers.tableIterator(ann)))
      ann->setTeam(color);
   /*
   {
      TeamSpecificAnnouncer *tsann;

      if((tsann = dynamic_cast<TeamSpecificAnnouncer *>(ann)))
         tsann->setTeam(color);
   }
   */
}

void CS_InitAnnouncer(void)
{
   Announcer *ann = NULL;
   Announcer *quake_announcer = new Announcer("quake");
   Announcer *unreal_announcer = new TeamSpecificAnnouncer("unreal");

   // [CG] TODO: It would be better if these could be defined somewhere instead
   //            of being hard-coded.

   quake_announcer->setTeam(team_color_none);
   quake_announcer->addEvent(ae_flag_taken,
      "Friendly Flag Taken", "FriendlyFlagTaken", "Flag Taken"
   );
   quake_announcer->addEvent(ae_enemy_flag_taken,
      "Enemy Flag Taken", "EnemyFlagTaken", "Enemy Flag Taken"
   );
   quake_announcer->addEvent(ae_flag_dropped,
      "Friendly Flag Dropped", "FriendlyFlagDropped", "Flag Dropped"
   );
   quake_announcer->addEvent(ae_enemy_flag_dropped,
      "Enemy Flag Dropped", "EnemyFlagDropped", "Enemy Flag Dropped"
   );
   quake_announcer->addEvent(ae_flag_returned,
      "Friendly Flag Returned", "FriendlyFlagReturned", "Flag Returned"
   );
   quake_announcer->addEvent(ae_enemy_flag_returned,
      "Enemy Flag Returned", "EnemyFlagReturned", "Enemy Flag Returned"
   );
   quake_announcer->addEvent(ae_flag_captured,
      "Enemy Flag Captured", "EnemyFlagCaptured", "Enemy Flag Captured"
   );
   quake_announcer->addEvent(ae_enemy_flag_captured,
      "Friendly Flag Captured", "FriendlyFlagCaptured", "Flag Captured"
   );
   announcers.addObject(quake_announcer);

   unreal_announcer->setTeam(team_color_red);
   unreal_announcer->addEvent(ae_flag_taken,
      "Red Flag Taken", "RedFlagTaken", "Red Flag Taken"
   );
   unreal_announcer->addEvent(ae_enemy_flag_taken,
      "Blue Flag Taken", "BlueFlagTaken", "Blue Flag Taken"
   );
   unreal_announcer->addEvent(ae_flag_dropped,
      "Red Flag Dropped", "RedFlagDropped", "Red Flag Dropped"
   );
   unreal_announcer->addEvent(ae_enemy_flag_dropped,
      "Blue Flag Dropped", "BlueFlagDropped", "Blue Flag Dropped"
   );
   unreal_announcer->addEvent(ae_flag_returned,
      "Red Flag Returned", "RedFlagReturned", "Red Flag Returned"
   );
   unreal_announcer->addEvent(ae_enemy_flag_returned,
      "Blue Flag Returned", "BlueFlagReturned", "Blue Flag Returned"
   );
   unreal_announcer->addEvent(ae_flag_captured,
      "Blue Flag Captured", "RedTeamScores", "Red Team Scores"
   );
   unreal_announcer->addEvent(ae_enemy_flag_captured,
      "Red Flag Captured", "BlueTeamScores", "Blue Team Scores"
   );
   announcers.addObject(unreal_announcer);

   while((ann = announcers.tableIterator(ann)))
   {
      ann->addEvent(ae_round_starting, "RoundStarting");
      ann->addEvent(ae_round_started, "RoundStarted");
      ann->addEvent(ae_three_frags_left, "ThreeFragsLeft");
      ann->addEvent(ae_two_frags_left, "TwoFragsLeft");
      ann->addEvent(ae_one_frag_left, "OneFragLeft");
      ann->addEvent(ae_team_kill, "TeamKillOne");
      ann->addEvent(ae_suicide_death, "SuicideDeathOne");
      ann->addEvent(ae_blowout_win, "BlowoutWin");
      ann->addEvent(ae_humiliating_win, "HumiliatingWin");
      ann->addEvent(ae_impressive_win, "ImpressiveWin");
      ann->addEvent(ae_lead_gained, "LeadGained");
      ann->addEvent(ae_lead_lost, "LeadLost");
      ann->addEvent(ae_lead_tied, "LeadTied");
      ann->addEvent(ae_round_lost, "RoundLost");
      ann->addEvent(ae_round_tied, "RoundTied");
      ann->addEvent(ae_round_won, "RoundWon");
      ann->addEvent(ae_one_minute_warning, "OneMinuteWarning");
      ann->addEvent(ae_five_minute_warning, "FiveMinuteWarning");
      ann->addEvent(ae_double_kill, "DoubleKill");
      ann->addEvent(ae_multi_kill, "MultiKill");
      ann->addEvent(ae_ultra_kill, "UltraKill");
      ann->addEvent(ae_monster_kill, "MonsterKill");
      ann->addEvent(ae_killing_spree, "KillingSpree");
      ann->addEvent(ae_rampage_spree, "RampageSpree");
      ann->addEvent(ae_dominating_spree, "DominatingSpree");
      ann->addEvent(ae_unstoppable_spree, "UnstoppableSpree");
      ann->addEvent(ae_god_like_spree, "GodLikeSpree");
      ann->addEvent(ae_wicked_sick_spree, "WickedSickSpree");
   }

   current_announcer = quake_announcer;
}

AnnouncerEvent* CS_GetAnnouncerEvent(int event_type)
{
   if((event_type <= ae_none) || (event_type >= ae_max))
      return NULL;

   return current_announcer->getEvent(event_type);
}

