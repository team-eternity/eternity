// Emacs style mode select -*- C -*- vi:sw=3 ts=3:
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
//   Client/Server client routines.
//
//----------------------------------------------------------------------------

#ifndef __CS_CLIENT_H__
#define __CS_CLIENT_H__

#include "m_dllist.h"
#include "p_spec.h"

#include "cs_main.h"

#include <json/json.h>

#define CL_MAX_BUFFER_SIZE MAX_POSITIONS

#define PASSWORD_FILENAME "passwords.json"

/*
#define CLIENT_STATUS(i) (\
   &client_statuses[(i)][(client_status_indices[(i)]\
) % MAX_POSITIONS])

#define NEXT_CLIENT_STATUS(i) (&client_statuses[(i)][(\
   client_status_indices[(i)] + client_statuses_received[(i)] + 1\
) % MAX_POSITIONS])

#define CLIENT_HAS_COMMAND(i) (\
   (client_statuses_received > 0) && (CLIENT_STATUS((i))->has_command)\
)
*/

typedef struct client_status_s
{
   // [CG] tic is necessary because the server needs to know when clients are
   //      seeing other clients for unlagged; everything related to unlagged
   //      is based on the server's gametic, so the server sends statuses at a
   //      given TIC, and clients acknowledge receipt of a status for the same
   //      TIC.
   int tic;
   boolean has_command;
   // [CG] Clients only care about a position's index member if it's their own
   //      position; they use it to check their prediction.  The value is
   //      the index of the last command run that resulted in this position.
   position_t position;
   cs_cmd_t command;
} client_status_t;

typedef struct nm_buffer_node_s
{
   mdllistitem_t link;
   unsigned int world_index;
   void *message;
} nm_buffer_node_t;

extern char *cs_server_url;
extern char *cs_client_password_file;
extern boolean need_to_sync;
extern json_object *cs_client_password_json;
extern cs_queue_level_t client_queue_level;
extern unsigned int client_queue_position;

extern unsigned int cl_current_world_index;
extern unsigned int cl_latest_world_index;

extern boolean cl_initial_spawn;
extern boolean cl_received_sync;

void CL_Init(char *url);
void CL_InitPlayDemoMode(void);
void CL_InitNetworkMessageHash(void);
void CL_Reset(void);
char* CL_GetUserAgent(void);
void CL_Connect(void);
void CL_Reconnect(void);
void CL_Disconnect(void);
void CL_Spectate(void);
void CL_SendPlayerStringInfo(client_info_t info_type);
void CL_SendPlayerArrayInfo(client_info_t info_type, int array_index);
void CL_SendPlayerScalarInfo(client_info_t info_type);
void CL_SendCommand(void);
void CL_SendTeamRequest(teamcolor_t team);
void CL_ServerMessage(const char *message);
void CL_PlayerMessage(int playernum, const char *message);
void CL_TeamMessage(const char *message);
void CL_BroadcastMessage(const char *message);
void CL_AuthMessage(const char *password);
void CL_RCONMessage(const char *command);
void CL_SaveServerPassword(void);
void CL_LoadWADs(void);
boolean CL_SetMobjState(mobj_t *mobj, statenum_t state);
mobj_t* CL_SpawnMissile(mobj_t *source, mobj_t *target,
                        fixed_t x, fixed_t y, fixed_t z,
                        fixed_t momx, fixed_t momy, fixed_t momz,
                        angle_t angle, mobjtype_t type);
char* CL_ExtractServerMessage(nm_servermessage_t *message);
char* CL_ExtractPlayerMessage(nm_playermessage_t *message);
void CL_SetActorNetID(mobj_t *actor, unsigned int net_id);
void CL_HandleMessage(char *data, size_t data_length);
void CL_PrintClientStatus(char *prefix, int playernum, client_status_t *status);
void CL_AdvancePlayerPositions(void);
void CL_ProcessNetworkMessages(void);
void CL_MoveCommands(unsigned int last_index, unsigned int position_index);
void CL_SetPsprite(player_t *player, int position, statenum_t stnum);
void CL_HandleDamagedMobj(mobj_t *target, mobj_t *source, int damage, int mod,
                          emod_t *emod, boolean justhit);
void CL_SetLatestFinishedIndices(unsigned int index);
void CL_RunDemoTics(void);
void CL_TryRunTics(void);

// [CG] Really, this is more documentation of the client message handlers
//      than anything.

void CL_HandleGameStateMessage(nm_gamestate_t *message);
void CL_HandleSyncMessage(nm_sync_t *message);
void CL_HandleMapCompletedMessage(nm_mapcompleted_t *message);
void CL_HandleMapStartedMessage(nm_mapstarted_t *message);
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
void CL_HandleBloodSpawnedMessage(nm_bloodspawned_t *blood_message);
void CL_HandleActorSpawnedMessage(nm_actorspawned_t *spawn_message);
void CL_HandleActorPositionMessage(nm_actorposition_t *message);
void CL_HandleActorTargetMessage(nm_actortarget_t *message);
void CL_HandleActorTracerMessage(nm_actortracer_t *message);
void CL_HandleActorStateMessage(nm_actorstate_t *message);
void CL_HandleActorAttributeMessage(nm_actorattribute_t *message);
void CL_HandleActorDamagedMessage(nm_actordamaged_t *message);
void CL_HandleActorKilledMessage(nm_actorkilled_t *message);
void CL_HandleActorRemovedMessage(nm_actorremoved_t *message);
void CL_HandleLineActivatedMessage(nm_lineactivated_t *message);
void CL_HandleMonsterActiveMessage(nm_monsteractive_t *message);
void CL_HandleMonsterAwakenedMessage(nm_monsterawakened_t *message);
void CL_HandleMissileSpawnedMessage(nm_missilespawned_t *message);
void CL_HandleMissileExplodedMessage(nm_missileexploded_t *message);
void CL_HandleCubeSpawnedMessage(nm_cubespawned_t *message);

#endif

