// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3: 
//-----------------------------------------------------------------------------
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   Clientside packet buffer.
//
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <string.h>

#include "doomdef.h"
#include "g_game.h"

#include "cs_main.h"
#include "cl_buf.h"
#include "cl_cmd.h"
#include "cl_main.h"
#include "cl_net.h"
#include "cl_spec.h"

NetPacketBuffer cl_packet_buffer;

bool NetPacket::shouldProcessNow()
{
   if(!cl_received_sync)
      return true;

   if(cl_packet_buffer_size == 1)
      return true;

   if((getWorldIndex() <= cl_current_world_index) && cl_constant_prediction)
      return true;

   return false;
}

void NetPacket::process()
{
   switch(getType())
   {
   case nm_gamestate:
      CL_HandleGameStateMessage((nm_gamestate_t *)data);
      break;
   case nm_sync:
      CL_HandleSyncMessage((nm_sync_t *)data);
      break;
   case nm_mapstarted:
      CL_HandleMapStartedMessage((nm_mapstarted_t *)data);
      break;
   case nm_mapcompleted:
      CL_HandleMapCompletedMessage((nm_mapcompleted_t *)data);
      break;
   case nm_authresult:
      CL_HandleAuthResultMessage((nm_authresult_t *)data);
      break;
   case nm_clientinit:
      CL_HandleClientInitMessage((nm_clientinit_t *)data);
      break;
   case nm_clientstatus:
      CL_HandleClientStatusMessage((nm_clientstatus_t *)data);
      break;
   case nm_playerspawned:
      CL_HandlePlayerSpawnedMessage((nm_playerspawned_t *)data);
      break;
   case nm_playerinfoupdated:
      CS_HandleUpdatePlayerInfoMessage((nm_playerinfoupdated_t *)data);
      break;
   case nm_playerweaponstate:
      CL_HandlePlayerWeaponStateMessage((nm_playerweaponstate_t *)data);
      break;
   case nm_playerremoved:
      CL_HandlePlayerRemovedMessage((nm_playerremoved_t *)data);
      break;
   case nm_playertouchedspecial:
      CL_HandlePlayerTouchedSpecialMessage((nm_playertouchedspecial_t *)data);
      break;
   case nm_servermessage:
      CL_HandleServerMessage((nm_servermessage_t *)data);
      break;
   case nm_playermessage:
      CL_HandlePlayerMessage((nm_playermessage_t *)data);
      break;
   case nm_puffspawned:
      CL_HandlePuffSpawnedMessage((nm_puffspawned_t *)data);
      break;
   case nm_bloodspawned:
      CL_HandleBloodSpawnedMessage((nm_bloodspawned_t *)data);
      break;
   case nm_actorspawned:
      CL_HandleActorSpawnedMessage((nm_actorspawned_t *)data);
      break;
   case nm_actorposition:
      CL_HandleActorPositionMessage((nm_actorposition_t *)data);
      break;
   case nm_actortarget:
      CL_HandleActorTargetMessage((nm_actortarget_t *)data);
      break;
   case nm_actorstate:
      CL_HandleActorStateMessage((nm_actorstate_t *)data);
      break;
   case nm_actordamaged:
      CL_HandleActorDamagedMessage((nm_actordamaged_t *)data);
      break;
   case nm_actorkilled:
      CL_HandleActorKilledMessage((nm_actorkilled_t *)data);
      break;
   case nm_actorremoved:
      CL_HandleActorRemovedMessage((nm_actorremoved_t *)data);
      break;
   case nm_lineactivated:
      CL_HandleLineActivatedMessage((nm_lineactivated_t *)data);
      break;
   case nm_monsteractive:
      CL_HandleMonsterActiveMessage((nm_monsteractive_t *)data);
      break;
   case nm_monsterawakened:
      CL_HandleMonsterAwakenedMessage((nm_monsterawakened_t *)data);
      break;
   case nm_missilespawned:
      CL_HandleMissileSpawnedMessage((nm_missilespawned_t *)data);
      break;
   case nm_missileexploded:
      CL_HandleMissileExplodedMessage((nm_missileexploded_t *)data);
      break;
   case nm_cubespawned:
      CL_HandleCubeSpawnedMessage((nm_cubespawned_t *)data);
      break;
   case nm_specialspawned:
      CL_HandleMapSpecialSpawnedMessage((nm_specialspawned_t *)data);
      break;
   case nm_specialstatus:
      CL_HandleMapSpecialStatusMessage((nm_specialstatus_t *)data);
      break;
   case nm_specialremoved:
      CL_HandleMapSpecialRemovedMessage((nm_specialremoved_t *)data);
      break;
   case nm_sectorposition:
      CL_HandleSectorPositionMessage((nm_sectorposition_t *)data);
      break;
   case nm_announcerevent:
      CL_HandleAnnouncerEventMessage((nm_announcerevent_t *)data);
      break;
   case nm_ticfinished:
      CL_SetLatestFinishedIndices(((nm_ticfinished_t *)data)->world_index);
      break;
   case nm_playercommand:
   case nm_max_messages:
   default:
      doom_printf("Ignoring unknown server message %d.\n", getType());
      break;
   }
}

bool NetPacketBuffer::add(char *data, uint32_t size)
{
   NetPacket *packet = new NetPacket(data, size);

   if(packet->shouldProcessNow())
   {
      packet->process();
      delete packet;
      return true;
   }

   packet_buffer.push_back(packet);
   return false;
}

void NetPacketBuffer::processPacketsForIndex(uint32_t index)
{
   NetPacket *packet;

   if(cl_received_sync && cl_packet_buffer_size == 1)
      return;

   while(true)
   {
      packet = packet_buffer.front();

      if(packet->getWorldIndex() > index)
         break;

      packet->process();
      packet_buffer.pop_front();
      delete(packet);
   }
}

void NetPacketBuffer::processAllPackets()
{
   while(!packet_buffer.empty())
   {
      packet_buffer.front()->process();
      packet_buffer.pop_front();
   }
}

