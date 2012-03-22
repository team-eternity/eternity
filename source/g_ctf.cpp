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
//   Capture the flag game type.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "d_player.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "e_edf.h"
#include "e_things.h"
#include "info.h"
#include "m_fixed.h"
#include "g_dmatch.h"
#include "g_game.h"
#include "g_type.h"
#include "g_ctf.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "v_patchfmt.h"

#include "cs_announcer.h"
#include "cs_team.h"
#include "cs_main.h"
#include "cs_netid.h"
#include "cl_main.h"
#include "sv_main.h"

extern int levelScoreLimit;

/*
 * [CG] TODO:
 *      - Individual scores (CTF points system).  I don't think this will be
 *        used by very many players, but nonetheless we need *some* value for
 *        that column on the scoreboard.  Right now it's always 0.  Probably
 *        this will be configurable.  I guess game types need a configuration
 *        hook?
 *      - Stats.  Currently none are kept.
 */

flag_stand_t cs_flag_stands[team_color_max];
flag_t cs_flags[team_color_max];

static void CS_removeFlagActor(flag_t *flag)
{
   Mobj *flag_actor;

   if(flag->net_id && ((flag_actor = NetActors.lookup(flag->net_id)) != NULL))
   {
      if(CS_SERVER)
         SV_BroadcastActorRemoved(flag_actor);
      flag_actor->removeThinker();
   }

   flag->net_id = 0;
}

static void CS_respawnFlag(flag_t *flag, fixed_t x, fixed_t y, fixed_t z,
                           const char *type_name)
{
   Mobj *flag_actor;

   if(!serverside)
      return;

   CS_removeFlagActor(flag);
   flag_actor = P_SpawnMobj(x, y, z, E_SafeThingName(type_name));
   if(CS_SERVER)
      SV_BroadcastActorSpawned(flag_actor);
   flag->net_id = flag_actor->net_id;
}

static int CS_getOtherColor(int color)
{
   if(color == team_color_blue)
      return team_color_red;
   else if(color == team_color_red)
      return team_color_blue;
   return team_color_none;
}

static const char* CS_getFlagName(int color)
{
   if(color == team_color_red)
      return "RedFlag";
   else if(color == team_color_blue)
      return "BlueFlag";
   return "Flag";
}

static const char* CS_getCarriedFlagName(int color)
{
   if(color == team_color_red)
      return "CarriedRedFlag";
   else if(color == team_color_blue)
      return "CarriedBlueFlag";
   return "CarriedFlag";
}

static void CS_setFlagPosition(Mobj* actor, fixed_t x, fixed_t y, fixed_t z)
{
   P_UnsetThingPosition(actor);
   actor->x = x;
   actor->y = y;
   actor->z = z;
   P_SetThingPosition(actor);
}

static patch_t* CS_getRedFlagPatch()
{
   patch_t *patch = NULL;
   int color = team_color_red;

   if(cs_flags[color].state == flag_home)
      patch = PatchLoader::CacheName(wGlobalDir, "RFLGH", PU_CACHE);
   else if(cs_flags[color].state == flag_dropped)
      patch = PatchLoader::CacheName(wGlobalDir, "FLGHDRP", PU_CACHE);
   else
   {
      int holding_team = clients[cs_flags[color].carrier].team;

      if(cs_flags[color].carrier == displayplayer)
      {
         if(holding_team == team_color_red)
            patch = PatchLoader::CacheName(wGlobalDir, "RFLGHCRP", PU_CACHE);
         else if(holding_team == team_color_blue)
            patch = PatchLoader::CacheName(wGlobalDir, "RFLGHCBP", PU_CACHE);
         else
            patch = PatchLoader::CacheName(wGlobalDir, "RFLGHCGP", PU_CACHE);
      }
      else if(holding_team == color)
         patch = PatchLoader::CacheName(wGlobalDir, "RFLGHCR", PU_CACHE);
      else if(holding_team == team_color_blue)
         patch = PatchLoader::CacheName(wGlobalDir, "RFLGHCB", PU_CACHE);
      else
         patch = PatchLoader::CacheName(wGlobalDir, "RFLGHCW", PU_CACHE);
   }

   return patch;
}

static patch_t* CS_getBlueFlagPatch()
{
   patch_t *patch = NULL;
   int color = team_color_blue;

   if(cs_flags[color].state == flag_home)
      patch = PatchLoader::CacheName(wGlobalDir, "BFLGH", PU_CACHE);
   else if(cs_flags[color].state == flag_dropped)
      patch = PatchLoader::CacheName(wGlobalDir, "FLGHDRP", PU_CACHE);
   else
   {
      int holding_team = clients[cs_flags[color].carrier].team;

      if(cs_flags[color].carrier == displayplayer)
      {
         if(holding_team == team_color_red)
            patch = PatchLoader::CacheName(wGlobalDir, "BFLGHCRP", PU_CACHE);
         else if(holding_team == team_color_blue)
            patch = PatchLoader::CacheName(wGlobalDir, "BFLGHCBP", PU_CACHE);
         else
            patch = PatchLoader::CacheName(wGlobalDir, "BFLGHCGP", PU_CACHE);
      }
      else if(holding_team == color)
         patch = PatchLoader::CacheName(wGlobalDir, "BFLGHCR", PU_CACHE);
      else if(holding_team == team_color_blue)
         patch = PatchLoader::CacheName(wGlobalDir, "BFLGHCB", PU_CACHE);
      else
         patch = PatchLoader::CacheName(wGlobalDir, "BFLGHCW", PU_CACHE);
   }

   return patch;
}

static patch_t* CS_getGreenFlagPatch()
{
   patch_t *patch = NULL;
   int color = team_color_none;

   if(cs_flags[color].state == flag_home)
      patch = PatchLoader::CacheName(wGlobalDir, "GFLGH", PU_CACHE);
   else if(cs_flags[color].state == flag_dropped)
      patch = PatchLoader::CacheName(wGlobalDir, "FLGHDRP", PU_CACHE);
   else
   {
      int holding_team = clients[cs_flags[color].carrier].team;

      if(cs_flags[color].carrier == displayplayer)
      {
         if(holding_team == team_color_red)
            patch = PatchLoader::CacheName(wGlobalDir, "GFLGHCRP", PU_CACHE);
         else if(holding_team == team_color_blue)
            patch = PatchLoader::CacheName(wGlobalDir, "GFLGHCBP", PU_CACHE);
         else
            patch = PatchLoader::CacheName(wGlobalDir, "GFLGHCGP", PU_CACHE);
      }
      else if(holding_team == color)
         patch = PatchLoader::CacheName(wGlobalDir, "GFLGHCR", PU_CACHE);
      else if(holding_team == team_color_blue)
         patch = PatchLoader::CacheName(wGlobalDir, "GFLGHCB", PU_CACHE);
      else
         patch = PatchLoader::CacheName(wGlobalDir, "GFLGHCW", PU_CACHE);
   }

   return patch;
}

static void CS_returnFlag(flag_t *flag)
{
   int color = flag - cs_flags;
   flag_stand_t *flag_stand = &cs_flag_stands[color];

   CS_respawnFlag(
      flag, flag_stand->x, flag_stand->y, flag_stand->z, CS_getFlagName(color)
   );

   flag->carrier = 0;
   flag->pickup_time = 0;
   flag->timeout = 0;
   flag->state = flag_home;
}

static void CS_giveFlag(int playernum, flag_t *flag)
{
   int color = flag - cs_flags;
   player_t *player = &players[playernum];
   cs_actor_position_t position;

   CS_SaveActorPosition(&position, player->mo, 0);

   CS_respawnFlag(
      flag, position.x, position.y, position.z, CS_getCarriedFlagName(color)
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

static void CS_handleFlagTouch(player_t *player, int color)
{
   int playernum = player - players;
   client_t *client = &clients[playernum];
   flag_t *flag = &cs_flags[color];
   int other_color = CS_getOtherColor(color);

   if(flag->state == flag_dropped)
   {
      if(client->team == color)
      {
         if(CS_SERVER)
         {
            if(color == team_color_red)
               SV_BroadcastAnnouncerEvent(ae_red_flag_returned, NULL);
            else if(color == team_color_blue)
               SV_BroadcastAnnouncerEvent(ae_blue_flag_returned, NULL);
            else
               SV_BroadcastAnnouncerEvent(ae_flag_returned, NULL);

            SV_BroadcastMessage(
               "%s returned the %s flag", player->name, team_color_names[color]
            );
         }
         CS_returnFlag(flag);
      }
      else
      {
         if(CS_SERVER)
         {
            if(color == team_color_red)
               SV_BroadcastAnnouncerEvent(ae_red_flag_taken, NULL);
            else if(color == team_color_blue)
               SV_BroadcastAnnouncerEvent(ae_blue_flag_taken, NULL);
            else
               SV_BroadcastAnnouncerEvent(ae_flag_taken, NULL);

            SV_BroadcastMessage(
               "%s picked up the %s flag",
               player->name,
               team_color_names[color]
            );
         }
         client->stats.flag_picks++;
         CS_giveFlag(playernum, flag);
      }
   }
   else if(client->team != color)
   {
      if(CS_SERVER)
      {
         if(color == team_color_red)
            SV_BroadcastAnnouncerEvent(ae_red_flag_taken, NULL);
         else if(color == team_color_blue)
            SV_BroadcastAnnouncerEvent(ae_blue_flag_taken, NULL);
         else
            SV_BroadcastAnnouncerEvent(ae_flag_taken, NULL);

         SV_BroadcastMessage(
            "%s has taken the %s flag",
            player->name,
            team_color_names[color]
         );
      }
      client->stats.flag_touches++;
      CS_giveFlag(playernum, flag);
   }
   else if(cs_flags[other_color].carrier == playernum)
   {
      if(CS_SERVER)
      {
         if(other_color == team_color_red)
            SV_BroadcastAnnouncerEvent(ae_red_flag_captured, NULL);
         else if(other_color == team_color_blue)
            SV_BroadcastAnnouncerEvent(ae_blue_flag_captured, NULL);
         else
            SV_BroadcastAnnouncerEvent(ae_flag_captured, NULL);

         SV_BroadcastMessage(
            "%s captured the %s flag",
            player->name,
            team_color_names[other_color]
         );
      }
      CS_returnFlag(&cs_flags[other_color]);
      team_scores[color]++;
      client->stats.flag_captures++;
   }
}

static flag_t* CS_getFlagCarriedByPlayer(int playernum)
{
   client_t *client = &clients[playernum];
   int other_color = CS_getOtherColor(client->team);
   flag_t *flag = &cs_flags[other_color];

   if(flag->carrier == playernum)
      return flag;

   return NULL;
}

static void CS_dropFlag(int playernum)
{
   flag_t *flag = CS_getFlagCarriedByPlayer(playernum);
   int color = flag - cs_flags;
   Mobj *corpse = players[playernum].mo;
   Mobj *flag_actor;

   if(!playernum)
      return;

   if(!flag || flag->carrier != playernum)
      return;

   CS_respawnFlag(flag, corpse->x, corpse->y, corpse->z, CS_getFlagName(color));

   flag_actor = NetActors.lookup(flag->net_id);

   if(CS_SERVER)
   {
      if(color == team_color_red)
         SV_BroadcastAnnouncerEvent(ae_red_flag_dropped, NULL);
      else if(color == team_color_blue)
         SV_BroadcastAnnouncerEvent(ae_blue_flag_dropped, NULL);
      else
         SV_BroadcastAnnouncerEvent(ae_flag_dropped, NULL);

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

CTFGameType::CTFGameType(const char *new_name)
   : DeathmatchGameType(new_name, xgt_capture_the_flag) {}

int CTFGameType::getStrengthOfVictory(float low_score, float high_score)
{
   float score_ratio = 0.0;

   if(low_score && high_score)
      score_ratio = low_score / high_score;

   if((score_ratio <= 20.0) && ((high_score - low_score) >= 4))
      return sov_blowout;

   if(score_ratio <= 40.0)
      return sov_humiliation;

   if(score_ratio <= 60.0)
      return sov_impressive;

   return sov_normal;
}

bool CTFGameType::usesFlagsAsScore()
{
   return true;
}

bool CTFGameType::shouldExitLevel()
{
   int i;

   for(i = 1; i <= cs_settings->teams; i++)
   {
      if(team_scores[i] >= levelScoreLimit)
         return true;
   }

   return false;
}

void CTFGameType::handleActorKilled(Mobj *source, Mobj *target, emod_t *mod)
{
   DeathmatchGameType::handleActorKilled(source, target, mod);

   int targetnum;

   if(!target || !target->player)
      return;

   targetnum = target->player - players;

   if(source && source->player && (source != target))
   {
      int source_team = clients[source->player - players].team;
      int target_team = clients[target->player - players].team;

      if((!CS_TEAMS_ENABLED) || (source_team != target_team))
      {
         if(CS_getFlagCarriedByPlayer(targetnum))
            clients[source->player - players].stats.flag_carriers_fragged++;
      }
   }

   CS_dropFlag(targetnum);
}

void CTFGameType::handleActorRemoved(Mobj *actor)
{
   if(actor && actor->player)
      CS_dropFlag(actor->player - players);
}

void CTFGameType::handleLoadLevel()
{
   int i;

   if(clientserver)
   {
      for(i = team_color_none; i < team_color_max; i++)
         CS_removeFlagActor(&cs_flags[i]);
   }
}

bool CTFGameType::handleActorTouchedSpecial(Mobj *special, Mobj *toucher)
{
   player_t *player;

   // haleyjd: don't crash if a monster gets here.
   if(!(player = toucher->player))
      return false;
   
   // Dead thing touching.
   // Can happen with a sliding player corpse.
   if(toucher->health <= 0)
      return false;

   // haleyjd 05/11/03: EDF pickups modifications
   if(special->sprite < 0 || special->sprite >= NUMSPRITES)
      return false;

   switch(pickupfx[special->sprite])
   {
      case PFX_BLUEFLAG:
      case PFX_DROPPEDBLUEFLAG:
         CS_handleFlagTouch(player, team_color_blue);
         return true;

      case PFX_REDFLAG:
      case PFX_DROPPEDREDFLAG:
         CS_handleFlagTouch(player, team_color_red);
         return true;
   }

   return false;
}

void CTFGameType::handleClientSpectated(int clientnum)
{
   CS_dropFlag(clientnum);
}

void CTFGameType::handleClientChangedTeam(int clientnum)
{
   CS_dropFlag(clientnum);
}

void CTFGameType::handleClientDisconnected(int clientnum)
{
   CS_dropFlag(clientnum);
}

void CTFGameType::tick()
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

      CS_returnFlag(flag);

      if(CS_SERVER)
      {
         if((flag_actor = NetActors.lookup(flag->net_id)) == NULL)
            continue;

         if(color == team_color_red)
            SV_BroadcastAnnouncerEvent(ae_red_flag_returned, NULL);
         else if(color == team_color_blue)
            SV_BroadcastAnnouncerEvent(ae_blue_flag_returned, NULL);
         else
            SV_BroadcastAnnouncerEvent(ae_flag_returned, NULL);

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

   if((flag = CS_getFlagCarriedByPlayer(playernum)) != NULL)
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

patch_t* CS_GetFlagPatch(int flag_color)
{
   if(flag_color == team_color_red)
      return CS_getRedFlagPatch();

   if(flag_color == team_color_blue)
      return CS_getBlueFlagPatch();

   return CS_getGreenFlagPatch();
}

void SV_SpawnFlag(flag_t *flag)
{
   int color = flag - cs_flags;
   flag_stand_t *flag_stand = &cs_flag_stands[color];
   const char *flag_name = CS_getFlagName(color);

   // [CG] This function is only called from G_DoLoadLevel in g_game.cpp.  It
   //      cannot broadcast flag spawns because it will cause Net ID errors in
   //      any connected clients.  This isn't a problem, because clients are
   //      sent the flags when they request state each map anyway.

   CS_removeFlagActor(flag);
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

