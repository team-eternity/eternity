// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 Charles Gunyon
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

#include "z_zone.h"

#include "p_mobj.h"

#include "cs_announcer.h"
#include "cs_ctf.h"
#include "cs_team.h"
#include "sv_main.h"

/******************************************************************************\
* TODO:                                                                        *
*   - Test with spectators                                                     *
*   - Test with multiple players                                               *
*                                                                              *
\******************************************************************************/

#define INITIAL_ANNOUNCER_COUNT 8

Announcer *current_announcer = NULL;
static bool announcer_enabled = false;
static EHashTable<Announcer, ENCStringHashKey, &Announcer::name, &Announcer::links> announcers(INITIAL_ANNOUNCER_COUNT);

void AnnouncerEvent::activate(Mobj *source)
{
   if(sfx)
      S_StartSfxInfo(source, sfx, 127, ATTN_NONE, false, CHAN_AUTO);

   if(has_message)
      HU_CenterMessage(message);
}

AnnouncerEvent* Announcer::getEvent(int type)
{
   if(team == team_color_red)
   {
      switch(type)
      {
      case ae_red_flag_taken:
         return events.objectForKey(ae_friendly_flag_taken);
         break;
      case ae_blue_flag_taken:
         return events.objectForKey(ae_enemy_flag_taken);
         break;
      case ae_red_flag_dropped:
         return events.objectForKey(ae_friendly_flag_dropped);
         break;
      case ae_blue_flag_dropped:
         return events.objectForKey(ae_enemy_flag_dropped);
         break;
      case ae_red_flag_returned:
         return events.objectForKey(ae_friendly_flag_returned);
         break;
      case ae_blue_flag_returned:
         return events.objectForKey(ae_enemy_flag_returned);
         break;
      case ae_red_flag_captured:
         return events.objectForKey(ae_friendly_flag_captured);
         break;
      case ae_blue_flag_captured:
         return events.objectForKey(ae_enemy_flag_captured);
         break;
      default:
         return events.objectForKey(type);
         break;
      }
   }
   else if(team == team_color_blue)
   {
      switch(type)
      {
      case ae_red_flag_taken:
         return events.objectForKey(ae_enemy_flag_taken);
         break;
      case ae_blue_flag_taken:
         return events.objectForKey(ae_friendly_flag_taken);
         break;
      case ae_red_flag_dropped:
         return events.objectForKey(ae_enemy_flag_dropped);
         break;
      case ae_blue_flag_dropped:
         return events.objectForKey(ae_friendly_flag_dropped);
         break;
      case ae_red_flag_returned:
         return events.objectForKey(ae_enemy_flag_returned);
         break;
      case ae_blue_flag_returned:
         return events.objectForKey(ae_friendly_flag_returned);
         break;
      case ae_red_flag_captured:
         return events.objectForKey(ae_enemy_flag_captured);
         break;
      case ae_blue_flag_captured:
         return events.objectForKey(ae_friendly_flag_captured);
         break;
      default:
         return events.objectForKey(type);
         break;
      }
   }
   else
      return events.objectForKey(type);
}

void Announcer::setTeam(int new_team)
{
   if((new_team != team) && new_team != team_color_none)
      team = new_team;
}

bool Announcer::addEvent(AnnouncerEvent *ae)
{
   AnnouncerEvent *existing_ae = NULL;

   if((existing_ae = events.objectForKey(ae->type)))
      I_Error("Event %s (%d) already exists.\n", ae->getName(), ae->type);

   events.addObject(ae);
   return true;
}

bool Announcer::addEvent(int type, const char *new_name,
                         const char *sound_name)
{
   AnnouncerEvent *ae;
   
   ae = new AnnouncerEvent(type, new_name, sound_name);

   if(!addEvent(ae))
   {
      delete ae;
      return false;
   }

   return true;
}

bool Announcer::addEvent(int type, const char *new_name,
                         const char *sound_name, const char *message)
{
   AnnouncerEvent *ae;
   
   ae = new AnnouncerEvent(type, new_name, sound_name, message);

   if(!addEvent(ae))
   {
      delete ae;
      return false;
   }

   return true;
}

AnnouncerEvent* TeamSpecificAnnouncer::getEvent(int type)
{
   return events.objectForKey(type);
}

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

void CS_UpdateTeamSpecificAnnouncers(int color)
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
   TeamSpecificAnnouncer *unreal_announcer =
      new TeamSpecificAnnouncer("unreal");

   // [CG] TODO: It would be better if these could be defined somewhere instead
   //            of being hard-coded.

   unreal_announcer->setTeam(team_color_none);
   unreal_announcer->addEvent(ae_red_flag_taken,
      "Red Flag Taken", "RedFlagTaken", "Red Flag Taken"
   );
   unreal_announcer->addEvent(ae_blue_flag_taken,
      "Blue Flag Taken", "BlueFlagTaken", "Blue Flag Taken"
   );
   unreal_announcer->addEvent(ae_red_flag_dropped,
      "Red Flag Dropped", "RedFlagDropped", "Red Flag Dropped"
   );
   unreal_announcer->addEvent(ae_blue_flag_dropped,
      "Blue Flag Dropped", "BlueFlagDropped", "Blue Flag Dropped"
   );
   unreal_announcer->addEvent(ae_red_flag_returned,
      "Red Flag Returned", "RedFlagReturned", "Red Flag Returned"
   );
   unreal_announcer->addEvent(ae_blue_flag_returned,
      "Blue Flag Returned", "BlueFlagReturned", "Blue Flag Returned"
   );
   unreal_announcer->addEvent(ae_red_flag_captured,
      "Red Flag Captured", "BlueTeamScores", "Blue Team Scores"
   );
   unreal_announcer->addEvent(ae_blue_flag_captured,
      "Blue Flag Captured", "RedTeamScores", "Red Team Scores"
   );
   announcers.addObject(unreal_announcer);

   unreal_announcer->setTeam(team_color_none);
   quake_announcer->addEvent(ae_friendly_flag_taken,
      "Friendly Flag Taken", "FriendlyFlagTaken", "Flag Taken"
   );
   quake_announcer->addEvent(ae_enemy_flag_taken,
      "Enemy Flag Taken", "EnemyFlagTaken", "Enemy Flag Taken"
   );
   quake_announcer->addEvent(ae_friendly_flag_dropped,
      "Friendly Flag Dropped", "FriendlyFlagDropped", "Flag Dropped"
   );
   quake_announcer->addEvent(ae_enemy_flag_dropped,
      "Enemy Flag Dropped", "EnemyFlagDropped", "Enemy Flag Dropped"
   );
   quake_announcer->addEvent(ae_friendly_flag_returned,
      "Friendly Flag Returned", "FriendlyFlagReturned", "Flag Returned"
   );
   quake_announcer->addEvent(ae_enemy_flag_returned,
      "Enemy Flag Returned", "EnemyFlagReturned", "Enemy Flag Returned"
   );
   quake_announcer->addEvent(ae_friendly_flag_captured,
      "Enemy Flag Captured", "EnemyFlagCaptured", "Enemy Flag Captured"
   );
   quake_announcer->addEvent(ae_enemy_flag_captured,
      "Friendly Flag Captured", "FriendlyFlagCaptured", "Flag Captured"
   );
   announcers.addObject(quake_announcer);

   while((ann = announcers.tableIterator(ann)))
   {
      ann->addEvent(ae_round_starting, "Round Starting", "RoundStarting");
      ann->addEvent(ae_round_started, "Round Start", "RoundStart");
      ann->addEvent(ae_three_frags_left, "Three Frags Left", "ThreeFragsLeft");
      ann->addEvent(ae_two_frags_left, "Two Frags Left", "TwoFragsLeft");
      ann->addEvent(ae_one_frag_left, "One Frag Left", "OneFragLeft");
      ann->addEvent(ae_team_kill, "Team Kill One", "TeamKillOne");
      ann->addEvent(ae_suicide_death, "Suicide Death One", "SuicideDeathOne");
      ann->addEvent(ae_blowout_win, "Blowout Win", "BlowoutWin");
      ann->addEvent(ae_humiliating_win, "Humiliating Win", "HumiliatingWin");
      ann->addEvent(ae_impressive_win, "Impressive Win", "ImpressiveWin");
      ann->addEvent(ae_lead_gained, "Lead Gained", "LeadGained");
      ann->addEvent(ae_lead_lost, "Lead Lost", "LeadLost");
      ann->addEvent(ae_lead_tied, "Lead Tied", "LeadTied");
      ann->addEvent(ae_round_lost, "Round Lost", "RoundLost");
      ann->addEvent(ae_round_tied, "Round Tied", "RoundTied");
      ann->addEvent(ae_round_won, "Round Won", "RoundWon");
      ann->addEvent(ae_one_minute_warning,
         "One Minute Warning", "OneMinuteWarning"
      );
      ann->addEvent(ae_five_minute_warning,
         "Five Minute Warning", "FiveMinuteWarning"
      );
      ann->addEvent(ae_double_kill, "Double Kill", "DoubleKill");
      ann->addEvent(ae_multi_kill, "Multi Kill", "MultiKill");
      ann->addEvent(ae_ultra_kill, "Ultra Kill", "UltraKill");
      ann->addEvent(ae_monster_kill, "Monster Kill", "MonsterKill");
      ann->addEvent(ae_killing_spree, "Killing Spree", "KillingSpree");
      ann->addEvent(ae_rampage_spree, "Rampage Spree", "RampageSpree");
      ann->addEvent(ae_dominating_spree,
         "Dominating Spree", "DominatingSpree"
      );
      ann->addEvent(ae_unstoppable_spree,
         "Unstoppable Spree", "UnstoppableSpree"
      );
      ann->addEvent(ae_god_like_spree, "God-Like Spree", "GodLikeSpree");
      ann->addEvent(ae_wicked_sick_spree,
         "Wicked Sick Spree", "WickedSickSpree"
      );
   }

   current_announcer = quake_announcer;
}

AnnouncerEvent* CS_GetAnnouncerEvent(int event_type)
{
   if((event_type <= ae_none) || (event_type >= ae_max))
      return NULL;

   if(!current_announcer)
      return NULL;

   return current_announcer->getEvent(event_type);
}

void CS_Announce(int event_type, Mobj *source)
{
   AnnouncerEvent *ev;

   if(!announcer_enabled)
      return;

   if(!current_announcer)
      return;
   
   if((ev = CS_GetAnnouncerEvent(event_type)))
      ev->activate(source);
}

