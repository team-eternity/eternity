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
//   Clientside network packet handlers.
//
//-----------------------------------------------------------------------------

#ifndef CL_NET__
#define CL_NET__

#include "cs_main.h"

void  CL_SendPlayerStringInfo(client_info_e info_type);
void  CL_SendPlayerArrayInfo(client_info_e info_type, int array_index);
void  CL_SendPlayerScalarInfo(client_info_e info_type);
void  CL_SendCommand(void);
void  CL_SendTeamRequest(teamcolor_t team);
void  CL_SendSyncRequest(void);
void  CL_SendSyncReceived(void);

void  CL_SendAuthMessage(const char *password);
void  CL_SendRCONMessage(const char *command);
void  CL_SendServerMessage(const char *message);
void  CL_SendPlayerMessage(int playernum, const char *message);
void  CL_SendTeamMessage(const char *message);
void  CL_BroadcastMessage(const char *message);
void  CL_Spectate(void);

void CL_HandleInitialStateMessage(nm_initialstate_t *message);
void CL_HandleCurrentStateMessage(nm_currentstate_t *message);
void CL_HandleSyncMessage(nm_sync_t *message);
void CL_HandleMapStartedMessage(nm_mapstarted_t *message);
void CL_HandleMapCompletedMessage(nm_mapcompleted_t *message);
void CL_HandleAuthResultMessage(nm_authresult_t *message);
void CL_HandleClientInitMessage(nm_clientinit_t *message);
void CL_HandleClientStatusMessage(nm_clientstatus_t *message);
void CL_HandlePlayerSpawnedMessage(nm_playerspawned_t *message);
void CL_HandlePlayerWeaponStateMessage(nm_playerweaponstate_t *message);
void CL_HandlePlayerRemovedMessage(nm_playerremoved_t *message);
void CL_HandlePlayerTouchedSpecialMessage(nm_playertouchedspecial_t *message);
void CL_HandleServerMessage(nm_servermessage_t *message);
void CL_HandlePlayerMessage(nm_playermessage_t *message);
void CL_HandlePuffSpawnedMessage(nm_puffspawned_t *message);
void CL_HandleBloodSpawnedMessage(nm_bloodspawned_t *message);
void CL_HandleActorSpawnedMessage(nm_actorspawned_t *message);
void CL_HandleActorPositionMessage(nm_actorposition_t *message);
void CL_HandleActorTargetMessage(nm_actortarget_t *message);
void CL_HandleActorStateMessage(nm_actorstate_t *message);
void CL_HandleActorDamagedMessage(nm_actordamaged_t *message);
void CL_HandleActorKilledMessage(nm_actorkilled_t *message);
void CL_HandleActorRemovedMessage(nm_actorremoved_t *message);
void CL_HandleLineActivatedMessage(nm_lineactivated_t *message);
void CL_HandleMonsterActiveMessage(nm_monsteractive_t *message);
void CL_HandleMonsterAwakenedMessage(nm_monsterawakened_t *message);
void CL_HandleMissileSpawnedMessage(nm_missilespawned_t *message);
void CL_HandleMissileExplodedMessage(nm_missileexploded_t *message);
void CL_HandleCubeSpawnedMessage(nm_cubespawned_t *message);
#if 0
void CL_HandleMapSpecialSpawnedMessage(nm_specialspawned_t *message);
void CL_HandleMapSpecialStatusMessage(nm_specialstatus_t *message);
void CL_HandleMapSpecialRemovedMessage(nm_specialremoved_t *message);
#endif
void CL_HandleSectorPositionMessage(nm_sectorposition_t *message);
void CL_HandleAnnouncerEventMessage(nm_announcerevent_t *message);

#endif

