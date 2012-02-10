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

#include "z_zone.h"
#include "e_hash.h"
#include "i_system.h"

#include "cs_team.h"

#define INITIAL_ANNOUNCER_EVENT_COUNT 64

typedef enum
{
   ae_none,
   ae_flag_taken,
   ae_enemy_flag_taken,
   ae_flag_dropped,
   ae_enemy_flag_dropped,
   ae_flag_returned,
   ae_enemy_flag_returned,
   ae_flag_captured,
   ae_enemy_flag_captured,
   ae_round_starting,
   ae_round_started,
   ae_three_frags_left,
   ae_two_frags_left,
   ae_one_frag_left,
   ae_team_kill,
   ae_suicide_death,
   ae_blowout_win,
   ae_humiliating_win,
   ae_impressive_win,
   ae_lead_gained,
   ae_lead_lost,
   ae_lead_tied,
   ae_round_lost,
   ae_round_tied,
   ae_round_won,
   ae_one_minute_warning,
   ae_five_minute_warning,
   ae_double_kill,
   ae_multi_kill,
   ae_ultra_kill,
   ae_monster_kill,
   ae_killing_spree,
   ae_rampage_spree,
   ae_dominating_spree,
   ae_unstoppable_spree,
   ae_god_like_spree,
   ae_wicked_sick_spree,
   ae_max
} announcer_event_type_e;

class AnnouncerEvent : public ZoneObject
{
private:
   const char *name;
   const char *sound_name;
   const char *message;

public:
   DLListItem<AnnouncerEvent> links;
   int type;

   AnnouncerEvent(int type, const char *new_sound_name)
      : ZoneObject(), name(""), sound_name(new_sound_name), message("")
   {
      if((type <= ae_none) || type >= ae_max)
         I_Error("AnnouncerEvent: got invalid type %d.\n", type);
   }

   AnnouncerEvent(int type, const char *new_name, const char *new_sound_name,
                  const char *new_message)
      : ZoneObject(), name(new_name), sound_name(new_sound_name),
        message(new_message)
   {
      if((type <= ae_none) || type >= ae_max)
         I_Error("AnnouncerEvent: got invalid type %d.\n", type);
   }

   const char* getSoundName() { return sound_name; }
   const char* getMessage() { return message; }

};

class Announcer : public ZoneObject
{
protected:
   bool enabled;
   EHashTable<AnnouncerEvent, EIntHashKey, &AnnouncerEvent::type,
              &AnnouncerEvent::links> events;
   teamcolor_t team;
public:
   DLListItem<Announcer> links;
   const char *name;

   Announcer(const char *new_name, teamcolor_t new_team, bool new_enabled)
      : ZoneObject(), enabled(new_enabled), team(new_team), name(new_name)
   {
      events.initialize(INITIAL_ANNOUNCER_EVENT_COUNT);
   }

   Announcer(const char *new_name)
      : ZoneObject(), enabled(true), team(team_color_none), name(new_name)
   {
      events.initialize(INITIAL_ANNOUNCER_EVENT_COUNT);
   }

   bool addEvent(AnnouncerEvent *ae)
   {
      AnnouncerEvent *existing_ae = NULL;

      if((existing_ae = events.objectForKey(ae->type)))
         return false;

      events.addObject(ae);
      return true;
   }

   bool addEvent(int type, const char *new_sound_name)
   {
      AnnouncerEvent *ae;
      
      ae = new AnnouncerEvent(type, new_sound_name);

      if(!addEvent(ae))
      {
         delete ae;
         return false;
      }

      return true;
   }

   bool addEvent(int type, const char *new_name, const char *new_sound_name,
                 const char *new_message)
   {
      AnnouncerEvent *ae;
      
      ae = new AnnouncerEvent(type, new_name, new_sound_name, new_message);

      if(!addEvent(ae))
      {
         delete ae;
         return false;
      }

      return true;
   }

   virtual void setTeam(teamcolor_t new_team) {}

   void enable() { enabled = true; }
   void disable() { enabled = false; }
   bool isEnabled() { if(enabled) return true; return false; }
   bool isDisabled() { if(enabled) return false; return true; }
   AnnouncerEvent* getEvent(int type) { return events.objectForKey(type); }

};

class TeamSpecificAnnouncer : public Announcer
{
private:
   void swapEventTypes(int t1, int t2, const char *n1, const char *n2)
   {
      AnnouncerEvent *friend_event, *enemy_event;

      friend_event = events.objectForKey(t1);
      enemy_event = events.objectForKey(t2);
      if(!friend_event)
         I_Error("TeamSpecificAnnouncer: missing %s event.\n", n1);
      if(!enemy_event)
         I_Error("TeamSpecificAnnouncer: missing %s event.\n", n2);
      events.removeObject(friend_event);
      events.removeObject(enemy_event);
      friend_event->type = t2;
      enemy_event->type = t1;
      events.addObject(friend_event);
      events.addObject(enemy_event);
   }
public:

   TeamSpecificAnnouncer(const char *new_name)
      : Announcer(new_name, team_color_red, true) {}

   void setTeam(teamcolor_t new_team)
   {
      if((new_team == team) || new_team == team_color_none)
         return;

      swapEventTypes(
         ae_flag_taken, ae_enemy_flag_taken, "flag taken", "enemy_flag_taken"
      );
      swapEventTypes(
         ae_flag_dropped,
         ae_enemy_flag_dropped,
         "flag dropped",
         "enemy_flag_dropped"
      );
      swapEventTypes(
         ae_flag_returned,
         ae_enemy_flag_returned,
         "flag returned",
         "enemy_flag_returned"
      );
      swapEventTypes(
         ae_flag_captured,
         ae_enemy_flag_captured,
         "flag captured",
         "enemy_flag_captured"
      );

      team = new_team;
   }
};

void            CS_EnableAnnouncer();
void            CS_DisableAnnouncer();
bool            CS_SetAnnouncer(const char *name);
bool            CS_AnnouncerEnabled(void);
void            CS_UpdateTeamSpecificAnnouncers(teamcolor_t color);
void            CS_InitAnnouncer(void);
AnnouncerEvent* CS_GetAnnouncerEvent(int event_type);

#endif

