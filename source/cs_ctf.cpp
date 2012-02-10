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

#include "z_zone.h"
#include "c_io.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_player.h"
#include "e_things.h"
#include "g_game.h"
#include "m_fixed.h"
#include "p_maputl.h"
#include "p_mobj.h"

#include "cs_announcer.h"
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
   Mobj *flag_actor;

   if(!serverside)
      return;

   CS_RemoveFlagActor(flag);
   flag_actor = P_SpawnMobj(x, y, z, E_SafeThingName(type_name));
   if(CS_SERVER)
      SV_BroadcastActorSpawned(flag_actor);
   flag->net_id = flag_actor->net_id;
}

static teamcolor_t get_other_color(teamcolor_t color)
{
   if(color == team_color_blue)
      return team_color_red;

   return team_color_blue;
}

static const char* get_flag_name(teamcolor_t color)
{
   if(color == team_color_red)
      return "RedFlag";

   return "BlueFlag";
}

static const char* get_carried_flag_name(teamcolor_t color)
{
   if(color == team_color_red)
      return "CarriedRedFlag";

   return "CarriedBlueFlag";
}

static void CS_setFlagPosition(Mobj* actor, fixed_t x, fixed_t y, fixed_t z)
{
   P_UnsetThingPosition(actor);
   actor->x = x;
   actor->y = y;
   actor->z = z;
   P_SetThingPosition(actor);
}

void CS_RemoveFlagActor(flag_t *flag)
{
   Mobj *flag_actor;

   if(flag->net_id && ((flag_actor = NetActors.lookup(flag->net_id)) != NULL))
   {
      SV_BroadcastActorRemoved(flag_actor);
      flag_actor->removeThinker();
   }

   flag->net_id = 0;
}

void CL_SpawnFlag(flag_t *flag, uint32_t net_id)
{
   teamcolor_t color = (teamcolor_t)(flag - cs_flags);
   flag_stand_t *flag_stand = &cs_flag_stands[color];
   const char *flag_name = get_flag_name(color);

   CL_SpawnMobj(
      net_id,
      flag_stand->x,
      flag_stand->y,
      flag_stand->z,
      E_SafeThingName(flag_name)
   );

   flag->net_id = net_id;
   flag->carrier = 0;
   flag->pickup_time = 0;
   flag->timeout = 0;
   flag->state = flag_home;
}

void SV_SpawnFlag(flag_t *flag)
{
   teamcolor_t color = (teamcolor_t)(flag - cs_flags);
   flag_stand_t *flag_stand = &cs_flag_stands[color];
   const char *flag_name = get_flag_name(color);

   // [CG] This function is only called from G_DoLoadLevel in g_game.cpp.  It
   //      cannot broadcast flag spawns because it will cause Net ID errors in
   //      any connected clients.  This isn't a problem, because clients are
   //      sent the flags when they request state each map anyway.

   CS_RemoveFlagActor(flag);
   flag->net_id = P_SpawnMobj(
      flag_stand->x,
      flag_stand->y,
      flag_stand->z,
      E_SafeThingName(flag_name)
   )->net_id;

   flag->carrier = 0;
   flag->pickup_time = 0;
   flag->timeout = 0;
   flag->state = flag_home;
}

void CS_ReturnFlag(flag_t *flag)
{
   teamcolor_t color = (teamcolor_t)(flag - cs_flags);
   flag_stand_t *flag_stand = &cs_flag_stands[color];

   respawn_flag(
      flag, flag_stand->x, flag_stand->y, flag_stand->z, get_flag_name(color)
   );

   flag->carrier = 0;
   flag->pickup_time = 0;
   flag->timeout = 0;
   flag->state = flag_home;
}

void CS_GiveFlag(int playernum, flag_t *flag)
{
   teamcolor_t color = (teamcolor_t)(flag - cs_flags);
   player_t *player = &players[playernum];
   cs_actor_position_t position;

   CS_SaveActorPosition(&position, player->mo, 0);

   respawn_flag(
      flag, position.x, position.y, position.z, get_carried_flag_name(color)
   );

   flag->carrier = playernum;

   if(CS_CLIENT)
      flag->pickup_time = cl_current_world_index;
   else if(CS_SERVER)
      flag->pickup_time = sv_world_index;
   else
      flag->pickup_time = gametic;

   flag->timeout = 0;
   flag->state = flag_carried;
}

void CS_HandleFlagTouch(player_t *player, teamcolor_t color)
{
   int playernum = player - players;
   client_t *client = &clients[playernum];
   flag_t *flag = &cs_flags[color];
   teamcolor_t other_color = get_other_color(color);

   if(flag->state == flag_dropped)
   {
      if(client->team == color)
      {
         if(CS_SERVER)
         {
            SV_BroadcastAnnouncerEvent(
               ae_flag_returned, CS_GetFlagStandActorForFlag(flag)
            );
            SV_BroadcastMessage(
               "%s returned the %s flag", player->name, team_color_names[color]
            );
         }
         CS_ReturnFlag(flag);
      }
      else
      {
         if(CS_SERVER)
         {
            SV_BroadcastAnnouncerEvent(
               ae_flag_taken, CS_GetFlagStandActorForFlag(flag)
            );
            SV_BroadcastMessage(
               "%s picked up the %s flag",
               player->name,
               team_color_names[color]
            );
         }
         CS_GiveFlag(playernum, flag);
      }
   }
   else
   {
      if(client->team != color)
      {
         if(CS_SERVER)
         {
            SV_BroadcastAnnouncerEvent(
               ae_flag_taken, CS_GetFlagStandActorForFlag(flag)
            );
            SV_BroadcastMessage(
               "%s has taken the %s flag",
               player->name,
               team_color_names[color]
            );
         }
         CS_GiveFlag(playernum, flag);
      }
      else if(cs_flags[other_color].carrier == playernum)
      {
         if(CS_SERVER)
         {
            SV_BroadcastAnnouncerEvent(
               ae_flag_captured, CS_GetFlagStandActorForFlag(flag)
            );
            SV_BroadcastMessage(
               "%s captured the %s flag",
               player->name,
               team_color_names[other_color]
            );
         }
         CS_ReturnFlag(&cs_flags[other_color]);
         team_scores[color]++;
      }
   }
}

flag_t* CS_GetFlagCarriedByPlayer(int playernum)
{
   client_t *client = &clients[playernum];
   teamcolor_t other_color = get_other_color((teamcolor_t)client->team);
   flag_t *flag = &cs_flags[other_color];

   if(flag->carrier == playernum)
      return flag;

   return NULL;
}

void CS_DropFlag(int playernum)
{
   flag_t *flag = CS_GetFlagCarriedByPlayer(playernum);
   teamcolor_t color = (teamcolor_t)(flag - cs_flags);
   Mobj *corpse = players[playernum].mo;
   Mobj *flag_actor;

   if(!flag || flag->carrier != playernum)
      return;

   respawn_flag(flag, corpse->x, corpse->y, corpse->z, get_flag_name(color));

   flag_actor = NetActors.lookup(flag->net_id);

   if(CS_SERVER)
   {
      SV_BroadcastAnnouncerEvent(
         ae_flag_dropped, CS_GetFlagStandActorForFlag(flag)
      );
      SV_BroadcastMessage(
         "%s dropped the %s flag",
         players[playernum].name,
         team_color_names[color]
      );
   }

   flag->state = flag_dropped;
   flag->carrier = 0;
   flag->pickup_time = 0;
   flag->timeout = 0;
}

void CS_CTFTicker(void)
{
   int color;
   flag_t *flag;
   Mobj *flag_actor;

   for(color = team_color_none; color < team_color_max; color++)
   {
      flag = &cs_flags[color];

      if(flag->state != flag_dropped)
         continue;

      if(++flag->timeout <= (10 * TICRATE))
         continue;

      CS_ReturnFlag(flag);

      if(CS_SERVER)
      {
         if((flag_actor = NetActors.lookup(flag->net_id)) == NULL)
            continue;
         SV_BroadcastAnnouncerEvent(
            ae_flag_returned, CS_GetFlagStandActorForFlag(flag)
         );
         SV_BroadcastMessage("%s flag returned", team_color_names[color]);
      }
   }
}

void CS_SnapCarriedFlagToCarrier(Mobj *flag_actor)
{
   player_t *player;
   flag_t *flag = &cs_flags[team_color_none];

   for(; flag != &cs_flags[team_color_max]; flag++)
   {
      if(flag->state != flag_carried)
         continue;

      if(flag->net_id == flag_actor->net_id)
         break;
   }

   if(flag == &cs_flags[team_color_max])
      return;

   player = &players[flag->carrier];

   CS_setFlagPosition(
      flag_actor, player->mo->x, player->mo->y, player->mo->z
   );
}

void CS_CheckCarriedFlagPosition(int playernum)
{
   flag_t *flag = NULL;
   Mobj *flag_actor = NULL;
   player_t *player = &players[playernum];

   if((flag = CS_GetFlagCarriedByPlayer(playernum)) != NULL)
   {
      if((flag_actor = NetActors.lookup(flag->net_id)) != NULL)
      {
         CS_setFlagPosition(
            flag_actor,
            player->mo->x,
            player->mo->y,
            player->mo->z
         );
      }
   }
}

bool CS_ActorIsCarriedFlag(Mobj *actor)
{
   int color;

   for(color = team_color_none; color < team_color_max; color++)
   {
      if(cs_flags[color].state != flag_carried)
         continue;

      if(cs_flags[color].net_id == actor->net_id)
         return true;
   }

   return false;
}

Mobj* CS_GetFlagStandActorForFlag(flag_t *flag)
{
   return NetActors.lookup(cs_flag_stands[flag - cs_flags].net_id);
}

