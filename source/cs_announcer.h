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

#include "doomstat.h"
#include "e_hash.h"
#include "e_sound.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "p_mobj.h"
#include "s_sound.h"

#include "cs_team.h"

#define INITIAL_ANNOUNCER_EVENT_COUNT 64

typedef enum
{
   ae_none,
   ae_flag_taken,
   ae_friendly_flag_taken,
   ae_enemy_flag_taken,
   ae_red_flag_taken,
   ae_blue_flag_taken,
   ae_flag_dropped,
   ae_friendly_flag_dropped,
   ae_enemy_flag_dropped,
   ae_red_flag_dropped,
   ae_blue_flag_dropped,
   ae_flag_returned,
   ae_friendly_flag_returned,
   ae_enemy_flag_returned,
   ae_red_flag_returned,
   ae_blue_flag_returned,
   ae_flag_captured,
   ae_friendly_flag_captured,
   ae_enemy_flag_captured,
   ae_red_flag_captured,
   ae_blue_flag_captured,
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
   char *name;
   char *sound_name;
   char *message;
   bool has_message;
   sfxinfo_t *sfx;

public:
   DLListItem<AnnouncerEvent> links;
   int type;

   AnnouncerEvent(int new_type, const char *new_name,
                  const char *new_sound_name)
      : ZoneObject()
   {
      if((new_type <= ae_none) || new_type >= ae_max)
         I_Error("AnnouncerEvent: got invalid type %d.\n", new_type);

      type = new_type;
      name = estrdup(new_name);
      sound_name = estrdup(new_sound_name);
      message = estrdup("");

      if(strlen(message))
         has_message = true;
      else
         has_message = false;

      sfx = E_SoundForName(sound_name);
   }

   AnnouncerEvent(int new_type, const char *new_name,
                  const char *new_sound_name, const char *new_message)
      : ZoneObject()
   {
      if((new_type <= ae_none) || new_type >= ae_max)
         I_Error("AnnouncerEvent: got invalid type %d.\n", new_type);

      type = new_type;
      name = estrdup(new_name);
      sound_name = estrdup(new_sound_name);
      message = estrdup(new_message);

      if(strlen(message))
         has_message = true;
      else
         has_message = false;

      sfx = E_SoundForName(sound_name);
   }

   ~AnnouncerEvent()
   {
      efree(name);
      efree(sound_name);
      efree(message);
   }

   const char* getName() { return name; }
   const char* getSoundName() { return sound_name; }
   const char* getMessage() { return message; }

   void activate(Mobj *source);

};

class Announcer : public ZoneObject
{
protected:
   bool enabled;
   EHashTable<AnnouncerEvent, EIntHashKey, &AnnouncerEvent::type,
              &AnnouncerEvent::links> events;
   int team;
public:
   DLListItem<Announcer> links;
   const char *name;

   Announcer(const char *new_name, int new_team, bool new_enabled)
      : ZoneObject(), enabled(new_enabled), team(new_team), name(new_name)
   {
      events.initialize(INITIAL_ANNOUNCER_EVENT_COUNT);
   }

   Announcer(const char *new_name)
      : ZoneObject(), enabled(true), team(team_color_none), name(new_name)
   {
      events.initialize(INITIAL_ANNOUNCER_EVENT_COUNT);
   }

   virtual AnnouncerEvent* getEvent(int type);
   
   void enable() { enabled = true; }
   void disable() { enabled = false; }
   bool isEnabled() { if(enabled) return true; return false; }
   bool isDisabled() { if(enabled) return false; return true; }

   void setTeam(int new_team);
   bool addEvent(AnnouncerEvent *ae);
   bool addEvent(int type, const char *new_name, const char *sound_name);
   bool addEvent(int type, const char *new_name, const char *sound_name,
                 const char *message);

};

class TeamSpecificAnnouncer : public Announcer
{
public:

   TeamSpecificAnnouncer(const char *new_name)
      : Announcer(new_name, team_color_none, true) {}

   AnnouncerEvent* getEvent(int type);

};

void            CS_EnableAnnouncer();
void            CS_DisableAnnouncer();
bool            CS_SetAnnouncer(const char *name);
bool            CS_AnnouncerEnabled(void);
void            CS_UpdateTeamSpecificAnnouncers(int color);
void            CS_InitAnnouncer(void);
AnnouncerEvent* CS_GetAnnouncerEvent(int event_type);
void            CS_Announce(int event_type, Mobj *source);

#endif

