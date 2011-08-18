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
//   Client/Server server routines.
//
//----------------------------------------------------------------------------

#ifndef __CS_SERVER_H__
#define __CS_SERVER_H__

#include "d_player.h"
#include "doomdata.h"
#include "doomdef.h"
#include "p_spec.h"

#include "cs_main.h"

#define CONSOLE_INPUT_LENGTH 1024

extern unsigned int sv_world_index;
extern unsigned long sv_public_address;
extern unsigned int sv_minimum_buffer_size;
extern boolean sv_should_send_new_map;
extern server_client_t server_clients[MAXPLAYERS];
extern const char *sv_spectator_password;
extern const char *sv_player_password;
extern const char *sv_moderator_password;
extern const char *sv_administrator_password;

// [CG] General stuff.
void SV_Init(void);
void SV_CleanUp(void);
char* SV_GetUserAgent(void);
void SV_ConsoleTicker(void);
void SV_RunDemoTics(void);
void SV_TryRunTics(void);

// [CG] Unlagged functions.
#if _UNLAG_DEBUG
void SV_SpawnGhost(int playernum);
#endif
void SV_StartUnlag(int playernum);
void SV_EndUnlag(int playernum);
void SV_LoadPlayerPositionAt(int playernum, unsigned int index);
void SV_LoadCurrentPlayerPosition(int playernum);

// [CG] Some lookup functions.
ENetPeer* SV_GetPlayerPeer(int playernum);
unsigned int SV_GetPlayerNumberFromPeer(ENetPeer *peer);
unsigned int SV_GetClientNumberFromAddress(ENetAddress *address);

// [CG] Functions to find a valid spawn point for a player.
mapthing_t* SV_GetCoopSpawnPoint(int playernum);
mapthing_t* SV_GetDeathMatchSpawnPoint(int playernum);
mapthing_t* SV_GetTeamSpawnPoint(int playernum);

// [CG] Authorization functions.
boolean SV_AuthorizeClient(int playernum, const char *password);
void SV_SendAuthorizationResult(int playernum,
                                boolean authorization_successful,
                                cs_auth_level_t authorization_level);

// [CG] Game loop functions.
void SV_SaveActorPositions(void);
void SV_ProcessPlayerCommand(int playernum);
boolean SV_RunPlayerCommands(int playernum);

// [CG] Misc. functions.
void SV_AddClient(int playernum);
void SV_AddNewClients(void);
void SV_SetPlayerTeam(int playernum, teamcolor_t team);
void SV_DisconnectPlayer(int playernum, disconnection_reason_t reason);
void SV_SetWeaponPreference(int playernum, int slot, weapontype_t weapon);
void SV_AdvanceMapList(void);
void SV_LoadClientOptions(int playernum);
void SV_RestoreServerOptions(void);

// [CG] Sending functions.
void SV_SendNewClient(int playernum, int clientnum);
void SV_SendGameState(int playernum);
void SV_SendClientInfo(int playernum, int clientnum);
void SV_SendSync(int playernum);

// [CG] Messaging functions.
void SV_SendMessage(int playernum, const char *fmt, ...);
void SV_SendHUDMessage(int playernum, const char *fmt, ...);
void SV_Say(const char *fmt, ...);
void SV_SayToPlayer(int playernum, const char *fmt, ...);

// [CG] Broadcasting functions.
void SV_BroadcastNewClient(int clientnum);
void SV_BroadcastMessage(const char *fmt, ...);
void SV_BroadcastHUDMessage(const char *fmt, ...);
// void SV_BroadcastGameState(void);
void SV_BroadcastMapStarted(void);
void SV_BroadcastMapCompleted(boolean enter_intermission);
void SV_BroadcastSettings(void);
void SV_BroadcastCurrentMap(void);
void SV_BroadcastPlayerSpawned(mapthing_t *spawn_point, int playernum);
void SV_BroadcastClientStatus(int playernum);
void SV_BroadcastPlayerStringInfo(int playernum, client_info_t info_type);
void SV_BroadcastPlayerArrayInfo(int playernum, client_info_t info_type,
                                 int array_index);
void SV_BroadcastPlayerScalarInfo(int playernum, client_info_t info_type);
void SV_BroadcastPlayerTouchedSpecial(int playernum, int thing_net_id);
void SV_BroadcastPlayerWeaponState(int playernum, int position,
                                   statenum_t stnum);
void SV_BroadcastPlayerRemoved(int playernum, disconnection_reason_t reason);
void SV_BroadcastPuffSpawned(mobj_t *puff, int updown, boolean ptcl);
void SV_BroadcastBloodSpawned(mobj_t *blood, int damage, mobj_t *target);
void SV_BroadcastActorSpawned(mobj_t *actor);
void SV_BroadcastActorPosition(mobj_t *actor, int tic);
void SV_BroadcastActorTarget(mobj_t *actor);
void SV_BroadcastActorTracer(mobj_t *actor);
void SV_BroadcastActorState(mobj_t *actor, statenum_t state_number);
void SV_BroadcastActorDamaged(mobj_t *target, mobj_t *inflictor,
                              mobj_t *source, int health_damage,
                              int armor_damage, int mod,
                              boolean damage_was_fatal, boolean just_hit);
void SV_BroadcastActorAttribute(mobj_t *actor, actor_attribute_type_t type);
void SV_BroadcastActorKilled(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                             int damage, int mod);
void SV_BroadcastActorRemoved(mobj_t *mo);
void SV_BroadcastLineUsed(mobj_t *actor, line_t *line, int side);
void SV_BroadcastLineCrossed(mobj_t *actor, line_t *line, int side);
void SV_BroadcastLineShot(mobj_t *actor, line_t *line, int side);
void SV_BroadcastMonsterActive(mobj_t *monster);
void SV_BroadcastMonsterAwakened(mobj_t *monster);
void SV_BroadcastMissileSpawned(mobj_t *source, mobj_t *missile);
void SV_BroadcastMissileExploded(mobj_t *missile);
void SV_BroadcastCubeSpawned(mobj_t *cube);
void SV_BroadcastSectorPosition(size_t sector_number);
void SV_BroadcastMapSpecialSpawned(void *special, void *special_data,
                                   line_t *line, map_special_t special_type);
void SV_BroadcastMapSpecialStatus(void *special, map_special_t special_type);
void SV_BroadcastMapSpecialRemoved(unsigned int net_id,
                                   map_special_t special_type);
void SV_BroadcastTICFinished(void);

// [CG] Handler functions.
int SV_HandleClientConnection(ENetPeer *peer);
boolean SV_HandleJoinRequest(int playernum);
void SV_HandlePlayerMessage(char *data, size_t data_length, int playernum);
void SV_HandleUpdatePlayerInfoMessage(char *data, size_t data_length,
                                      int playernum);
void SV_HandlePlayerCommandMessage(char *data, size_t data_length,
                                   int playernum);
void SV_HandleUpdatePlayerInfoMessage(char *data, size_t data_length,
                                      int playernum);
void SV_HandleMessage(char *data, size_t data_length, int playernum);

#endif

