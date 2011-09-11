// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright (C) 2006-2010 by The Odamex Team.
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
//   C/S CTF routines.
//
//----------------------------------------------------------------------------

// [CG] Notably, CTF was largely written by Toke for Odamex, and although that
//      implementation varies significantly from this one, it's still the same
//      fundamental design.  Thanks :) .

#include "c_io.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_player.h"
#include "e_things.h"
#include "e_sound.h"
#include "g_game.h"
#include "m_fixed.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "s_sound.h"

#include "cs_team.h"
#include "cs_ctf.h"
#include "cs_main.h"
#include "cs_netid.h"
#include "cl_main.h"
#include "sv_main.h"

flag_stand_t cs_flag_stands[team_color_max];
flag_t cs_flags[team_color_max];

static void respawn_flag(flag_t *flag, fixed_t x, fixed_t y, fixed_t z,
                         const char *type_name)
{
   mobj_t *flag_actor;

   CS_RemoveFlagActor(flag);

   if(serverside)
   {
      flag_actor = P_SpawnMobj(x, y, z, E_SafeThingName(type_name));
      if(CS_SERVER)
         SV_BroadcastActorSpawned(flag_actor);
      flag->net_id = flag_actor->net_id;
   }
}

static void start_flag_sound(mobj_t *actor, const char *sound)
{
   sfxinfo_t *sfx = E_SoundForName(sound);

   S_StartSfxInfo(actor, sfx, 127, ATTN_NONE, false, CHAN_AUTO);
   if(CS_SERVER)
      SV_BroadcastSoundPlayed(actor, sound, 127, ATTN_NONE, false, CHAN_AUTO);
}

void CS_AnnounceFlagTaken(teamcolor_t flag_color)
{
   mobj_t *flag_actor = CS_GetActorFromNetID(cs_flags[flag_color].net_id);

   if(!flag_actor)
      return;

   if(s_use_announcer && flag_color == team_color_red)
      start_flag_sound(flag_actor, "RedFlagTaken");
   else if(s_use_announcer && flag_color == team_color_blue)
      start_flag_sound(flag_actor, "BlueFlagTaken");
   else if(flag_color == clients[consoleplayer].team)
      start_flag_sound(flag_actor, "FriendlyFlagTaken");
   else
      start_flag_sound(flag_actor, "EnemyFlagTaken");
}

void CS_AnnounceFlagDropped(teamcolor_t flag_color)
{
   mobj_t *flag_actor = CS_GetActorFromNetID(cs_flags[flag_color].net_id);

   if(!flag_actor)
      return;

   if(s_use_announcer && flag_color == team_color_red)
      start_flag_sound(flag_actor, "RedFlagDropped");
   else if(s_use_announcer && flag_color == team_color_blue)
      start_flag_sound(flag_actor, "BlueFlagDropped");
   else if(flag_color == clients[consoleplayer].team)
      start_flag_sound(flag_actor, "FriendlyFlagDropped");
   else
      start_flag_sound(flag_actor, "EnemyFlagDropped");
}

void CS_AnnounceFlagReturned(teamcolor_t flag_color)
{
   mobj_t *flag_actor = CS_GetActorFromNetID(cs_flags[flag_color].net_id);

   if(!flag_actor)
      return;

   if(s_use_announcer && flag_color == team_color_red)
      start_flag_sound(flag_actor, "RedFlagReturned");
   else if(s_use_announcer && flag_color == team_color_blue)
      start_flag_sound(flag_actor, "BlueFlagReturned");
   else if(flag_color == clients[consoleplayer].team)
      start_flag_sound(flag_actor, "FriendlyFlagReturned");
   else
      start_flag_sound(flag_actor, "EnemyFlagReturned");
}

void CS_AnnounceFlagCaptured(teamcolor_t flag_color)
{
   mobj_t *flag_actor = CS_GetActorFromNetID(cs_flags[flag_color].net_id);

   if(!flag_actor)
      return;

   if(s_use_announcer && flag_color == team_color_red)
      start_flag_sound(flag_actor, "BlueTeamScores");
   else if(s_use_announcer && flag_color == team_color_blue)
      start_flag_sound(flag_actor, "RedTeamScores");
   else if(flag_color == clients[consoleplayer].team)
      start_flag_sound(flag_actor, "FriendlyFlagCaptured");
   else
      start_flag_sound(flag_actor, "EnemyFlagCaptured");
}

void CS_RemoveFlagActor(flag_t *flag)
{
   mobj_t *flag_actor;

   if(flag->net_id)
   {
      if((flag_actor = CS_GetActorFromNetID(flag->net_id)) != NULL)
      {
         if(serverside)
         {
            if(CS_SERVER)
               SV_BroadcastActorRemoved(flag_actor);
            P_RemoveMobj(flag_actor);
         }
      }
      flag->net_id = 0;
   }
}

void CS_ReturnFlag(flag_t *flag)
{
   teamcolor_t color = flag - cs_flags;
   flag_stand_t *flag_stand = &cs_flag_stands[color];

   if(color == team_color_red)
   {
      respawn_flag(
         flag, flag_stand->x, flag_stand->y, flag_stand->z, "RedFlag"
      );
   }
   else if(color == team_color_blue)
   {
      respawn_flag(
         flag, flag_stand->x, flag_stand->y, flag_stand->z, "BlueFlag"
      );
   }

   flag->carrier = 0;
   flag->pickup_time = 0;
   flag->timeout = 0;
   flag->state = flag_home;
}

void CS_GiveFlag(int playernum, flag_t *flag)
{
   teamcolor_t color = flag - cs_flags;
   player_t *player = &players[playernum];
   position_t position;

   CS_SaveActorPosition(&position, player->mo, 0);

   if(color == team_color_red)
   {
      respawn_flag(
         flag, position.x, position.y, position.z, "CarriedRedFlag"
      );
   }
   else if(color == team_color_blue)
   {
      respawn_flag(
         flag, position.x, position.y, position.z, "CarriedBlueFlag"
      );
   }

   flag->carrier = playernum;

   if(CS_CLIENT)
      flag->pickup_time = cl_current_world_index;
   else if(CS_SERVER)
      flag->pickup_time = sv_world_index;
   else
      flag->pickup_time = gametic;

   printf("Gave %s flag to player %d.\n", team_color_names[color], playernum);

   flag->timeout = 0;
   flag->state = flag_carried;
}

void CS_HandleFlagTouch(player_t *player, teamcolor_t color)
{
   int playernum = player - players;
   client_t *client = &clients[playernum];
   flag_t *flag = &cs_flags[color];
   teamcolor_t other_color;

   if(color == team_color_red)
      other_color = team_color_blue;
   else
      other_color = team_color_red;

   if(flag->state == flag_dropped)
   {
      if(client->team == color)
      {
         CS_ReturnFlag(flag);
         doom_printf(
            "%s returned the %s flag", player->name, team_color_names[color]
         );
         if(serverside)
            CS_AnnounceFlagReturned(color);
      }
      else
      {
         CS_GiveFlag(playernum, flag);
         doom_printf(
            "%s picked up the %s flag", player->name, team_color_names[color]
         );
         if(serverside)
            CS_AnnounceFlagTaken(color);
      }
   }
   else
   {
      if(client->team == color)
      {
         if(cs_flags[other_color].carrier == playernum)
         {
            CS_ReturnFlag(&cs_flags[other_color]);
            team_scores[color]++;
            doom_printf(
               "%s captured the %s flag",
               player->name,
               team_color_names[other_color]
            );
            if(serverside)
               CS_AnnounceFlagCaptured(other_color);
         }
      }
      else
      {
         CS_GiveFlag(playernum, flag);
         doom_printf(
            "%s has taken the %s flag", player->name, team_color_names[color]
         );
         if(serverside)
            CS_AnnounceFlagTaken(color);
      }
   }
}

flag_t* CS_GetFlagCarriedByPlayer(int playernum)
{
   flag_t *flag;
   teamcolor_t other_color;
   client_t *client = &clients[playernum];

   if(client->team == team_color_red)
      other_color = team_color_blue;
   else
      other_color = team_color_red;

   flag = &cs_flags[other_color];

   if(flag->carrier == playernum)
      return flag;

   return NULL;
}

void CS_DropFlag(int playernum)
{
   flag_t *flag = CS_GetFlagCarriedByPlayer(playernum);
   teamcolor_t color = flag - cs_flags;
   mobj_t *corpse = players[playernum].mo;

   if(!flag || flag->carrier != playernum)
      return;

   doom_printf(
      "%s dropped the %s flag",
      players[playernum].name,
      team_color_names[color]
   );

   if(color == team_color_red)
      respawn_flag(flag, corpse->x, corpse->y, corpse->z, "RedFlag");
   else if(color == team_color_blue)
      respawn_flag(flag, corpse->x, corpse->y, corpse->z, "BlueFlag");

   if(serverside)
      CS_AnnounceFlagDropped(color);

   flag->state = flag_dropped;
   flag->carrier = 0;
   flag->pickup_time = 0;
   flag->timeout = 0;
}

void CS_CTFTicker(void)
{
   teamcolor_t color;
   flag_t *flag;
   position_t position;
   mobj_t *flag_actor;

   for(color = team_color_none; color < team_color_max; color++)
   {
      flag = &cs_flags[color];

      if(flag->state == flag_dropped)
      {
         if(++flag->timeout > (10 * TICRATE))
         {
            CS_ReturnFlag(flag);
            doom_printf("%s flag returned", team_color_names[color]);
            if(serverside)
               CS_AnnounceFlagReturned(color);
         }
      }
      else if(flag->state == flag_carried)
      {
         if((flag_actor = CS_GetActorFromNetID(flag->net_id)) != NULL)
         {
            CS_SaveActorPosition(&position, players[flag->carrier].mo, 0);
            P_UnsetThingPosition(flag_actor);
            flag_actor->x = position.x;
            flag_actor->y = position.y;
            flag_actor->z = position.z;
            P_SetThingPosition(flag_actor);
         }
      }
   }
}

