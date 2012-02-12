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

#include <list>

#include "z_zone.h"
#include "doomdef.h"
#include "g_game.h"
#include "i_system.h"
#include "i_thread.h"

#include "cs_main.h"
#include "cl_buf.h"
#include "cl_cmd.h"
#include "cl_main.h"
#include "cl_net.h"
#include "cl_spec.h"

NetPacketBuffer cl_packet_buffer;

int CL_serviceNetwork(void *)
{
   while(cl_packet_buffer.m_buffering_independently)
      CS_ReadFromNetwork(1000 / TICRATE);
   return 0;
}

bool NetPacket::shouldProcessNow()
{
   if(!cl_packet_buffer.synchronized())
      return true;

   if(!cl_packet_buffer.enabled())
      return true;

   if(getWorldIndex() <= cl_current_world_index)
      return true;

   return false;
}

void NetPacket::process()
{
   if(LOG_ALL_NETWORK_MESSAGES)
      printf("Received [%s message].\n", getName());

   switch(getType())
   {
   case nm_initialstate:
      CL_HandleInitialStateMessage((nm_initialstate_t *)data);
      break;
   case nm_currentstate:
      CL_HandleCurrentStateMessage((nm_currentstate_t *)data);
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
   case nm_playerposition:
      CL_HandlePlayerPositionMessage((nm_playerposition_t *)data);
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
   case nm_actormiscstate:
      CL_HandleActorMiscStateMessage((nm_actormiscstate_t *)data);
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
#if 0
   case nm_specialspawned:
      CL_HandleMapSpecialSpawnedMessage((nm_specialspawned_t *)data);
      break;
   case nm_specialstatus:
      CL_HandleMapSpecialStatusMessage((nm_specialstatus_t *)data);
      break;
   case nm_specialremoved:
      CL_HandleMapSpecialRemovedMessage((nm_specialremoved_t *)data);
      break;
#endif
   case nm_sectorposition:
      CL_HandleSectorPositionMessage((nm_sectorposition_t *)data);
      break;
   case nm_announcerevent:
      CL_HandleAnnouncerEventMessage((nm_announcerevent_t *)data);
      break;
   case nm_vote:
      CL_HandleVoteMessage((nm_vote_t *)data);
      break;
   case nm_voteresult:
      CL_HandleVoteResultMessage((nm_voteresult_t *)data);
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

NetPacketBuffer::NetPacketBuffer(void)
{
   m_buffering_independently = false;
   m_needs_flushing = false;
   m_needs_filling = false;
   m_synchronized = false;
   m_enabled = false;
   m_net_service_thread = NULL;
   m_size = 0;
   m_capacity = 0;
}

void NetPacketBuffer::startBufferingIndependently()
{
   if(m_buffering_independently)
      return;

   printf("Start independent buffering.\n");

   m_buffering_independently = true;
   m_net_service_thread = I_CreateThread(CL_serviceNetwork, NULL);
   if(m_net_service_thread == NULL)
   {
      I_Error(
         "Unable to create thread to service network independently, "
         "exiting.\n"
      );
   }
}

void NetPacketBuffer::stopBufferingIndependently()
{
   m_buffering_independently = false;
   I_WaitThread(m_net_service_thread, NULL);

   printf("Independent buffering complete.\n");
}

void NetPacketBuffer::setSynchronized(bool b)
{
   m_synchronized = b;

   if(synchronized())
   {
      setNeedsFlushing(true);
      setNeedsFilling(true);
   }
   else
      disable();
}

void NetPacketBuffer::enable(void)
{
   m_enabled = true;
   CL_SendPlayerScalarInfo(ci_buffering);
}

void NetPacketBuffer::disable(void)
{
   m_enabled = false;
   CL_SendPlayerScalarInfo(ci_buffering);
}

bool NetPacketBuffer::overflowed(void)
{
   return m_size > capacity();
}

uint32_t NetPacketBuffer::capacity(void)
{
   uint32_t c = m_capacity;

   if(adaptive())
   {
      if(clients[consoleplayer].transit_lag <= TICRATE)
         return 2;
      else
         return (clients[consoleplayer].transit_lag / TICRATE) * 2;
   }

   return c;
}

void NetPacketBuffer::setCapacity(uint32_t new_capacity)
{
   if(new_capacity >= CL_MAX_BUFFER_SIZE || new_capacity == m_capacity)
      return;

   setNeedsFlushing(true);
   setNeedsFilling(true);

   m_capacity = new_capacity;
}

void NetPacketBuffer::add(char *data, uint32_t data_size)
{
   NetPacket *packet = new NetPacket(data, data_size);

   if(packet->getType() == nm_ticfinished)
      m_size++;

   if(m_needs_filling && m_size > capacity())
      m_needs_filling = false;

   if((!m_buffering_independently) &&
      (!m_needs_filling) &&
      (packet->shouldProcessNow()))
   {
      packet->process();

      if(packet->getType() == nm_ticfinished)
         m_size--;

      delete packet;
   }
   else
   {
      packet_buffer.push_back(packet);
   }
}

void NetPacketBuffer::processPacketsForIndex(uint32_t index)
{
   NetPacket *packet;

   if(synchronized() && !enabled())
      return;

   if(m_needs_filling)
      return;

   while(true)
   {
      if(packet_buffer.empty())
         return;

      packet = packet_buffer.front();

      if(!packet || packet->getWorldIndex() > index)
         break;

      packet->process();

      if(packet_buffer.front()->getType() == nm_ticfinished)
         m_size--;

      packet_buffer.pop_front();
      delete(packet);
   }
}

void NetPacketBuffer::processAllPackets()
{
   NetPacket *packet;

   if(m_needs_filling)
      return;

   while(!packet_buffer.empty())
   {
      packet = packet_buffer.front();

      packet_buffer.front()->process();

      if(packet_buffer.front()->getType() == nm_ticfinished)
         m_size--;

      packet_buffer.pop_front();
      delete(packet);
   }
}

