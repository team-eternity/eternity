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
//   Client/Server server routines.
//
//----------------------------------------------------------------------------

#ifndef __CS_SERVER_H__
#define __CS_SERVER_H__

#include "d_player.h"
#include "doomdata.h"
#include "doomdef.h"
#include "p_spec.h"
#include "s_sound.h"

class Mobj;

#include "cs_announcer.h"
#include "cs_main.h"

#define CONSOLE_INPUT_LENGTH 1024

extern unsigned int sv_world_index;
extern unsigned long sv_public_address;
extern bool sv_should_send_new_map;
extern server_client_t server_clients[MAXPLAYERS];
extern int sv_randomize_maps;
extern bool sv_buffer_commands;
extern const char *sv_spectator_password;
extern const char *sv_player_password;
extern const char *sv_moderator_password;
extern const char *sv_administrator_password;
extern const char *sv_access_list_filename;
extern const char *sv_default_access_list_filename;

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
void SV_LoadPlayerMiscStateAt(int playernum, unsigned int index);
void SV_LoadCurrentPlayerPosition(int playernum);
void SV_LoadCurrentPlayerMiscState(int playernum);

// [CG] Some lookup functions.
ENetPeer* SV_GetPlayerPeer(int playernum);
unsigned int SV_GetPlayerNumberFromPeer(ENetPeer *peer);
unsigned int SV_GetClientNumberFromAddress(ENetAddress *address);

// [CG] Functions to find a valid spawn point for a player.
mapthing_t* SV_GetCoopSpawnPoint(int playernum);
mapthing_t* SV_GetDeathMatchSpawnPoint(int playernum);
mapthing_t* SV_GetTeamSpawnPoint(int playernum);
void SV_SpawnPlayer(int playernum, bool as_spectator);

// [CG] Authorization functions.
bool SV_AuthorizeClient(int playernum, const char *password);
void SV_SendAuthorizationResult(int playernum,
                                bool authorization_successful,
                                cs_auth_level_e authorization_level);

// [CG] Game loop functions.
void SV_ProcessPlayerCommand(int playernum);
void SV_BroadcastUpdatedActorPositionsAndMiscState(void);

// [CG] Misc. functions.
void SV_AddClient(int playernum);
void SV_AddNewClients(void);
void SV_SetPlayerTeam(int playernum, teamcolor_t team);
void SV_DisconnectPlayer(int playernum, disconnection_reason_e reason);
void SV_SetWeaponPreference(int playernum, int slot, weapontype_t weapon);
void SV_AdvanceMapList(void);
void SV_LoadClientOptions(int playernum);
void SV_RestoreServerOptions(void);
unsigned int SV_ClientCommandBufferSize(int playernum);
void SV_RunPlayerCommand(int playernum, cs_buffered_command_t *bufcmd);
void SV_RunPlayerCommands(int playernum);
void SV_UpdateClientStatus(void);

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
void SV_BroadcastMapCompleted(bool enter_intermission);
void SV_BroadcastSettings(void);
void SV_BroadcastCurrentMap(void);
void SV_BroadcastPlayerSpawned(mapthing_t *spawn_point, int playernum);
void SV_BroadcastClientStatuses(void);
void SV_BroadcastPlayerPositions(void);
void SV_BroadcastPlayerStringInfo(int playernum, client_info_e info_type);
void SV_BroadcastPlayerArrayInfo(int playernum, client_info_e info_type,
                                 int array_index);
void SV_BroadcastPlayerScalarInfo(int playernum, client_info_e info_type);
void SV_BroadcastPlayerTouchedSpecial(int playernum, int thing_net_id);
void SV_BroadcastPlayerWeaponState(int playernum, int position,
                                   statenum_t stnum);
void SV_BroadcastPlayerRemoved(int playernum, disconnection_reason_e reason);
void SV_BroadcastPuffSpawned(Mobj *puff, Mobj *shooter, int updown,
                             bool ptcl);
void SV_BroadcastBloodSpawned(Mobj *blood, Mobj *shooter, int damage,
                              Mobj *target);
void SV_BroadcastActorSpawned(Mobj *actor);
void SV_BroadcastActorPosition(Mobj *actor, int tic);
void SV_BroadcastActorMiscState(Mobj *actor, int tic);
void SV_BroadcastActorTarget(Mobj *actor, actor_target_e target_type);
void SV_BroadcastActorState(Mobj *actor, statenum_t state_number);
void SV_BroadcastActorDamaged(Mobj *target, Mobj *inflictor,
                              Mobj *source, int health_damage,
                              int armor_damage, int mod,
                              bool damage_was_fatal, bool just_hit);
void SV_BroadcastActorKilled(Mobj *target, Mobj *inflictor, Mobj *source,
                             int damage, int mod);
void SV_BroadcastActorRemoved(Mobj *mo);
void SV_BroadcastLineUsed(Mobj *actor, line_t *line, int side);
void SV_BroadcastLineCrossed(Mobj *actor, line_t *line, int side);
void SV_BroadcastLineShot(Mobj *actor, line_t *line, int side);
void SV_BroadcastMonsterActive(Mobj *monster);
void SV_BroadcastMonsterAwakened(Mobj *monster);
void SV_BroadcastMissileSpawned(Mobj *source, Mobj *missile);
void SV_BroadcastMissileExploded(Mobj *missile);
void SV_BroadcastCubeSpawned(Mobj *cube);
void SV_BroadcastSectorPosition(size_t sector_number);
void SV_BroadcastMapSpecialSpawned(void *special, void *data, line_t *line,
                                   sector_t *sector,
                                   map_special_e special_type);
void SV_BroadcastMapSpecialStatus(void *special, map_special_e special_type);
void SV_BroadcastMapSpecialRemoved(unsigned int net_id,
                                   map_special_e special_type);
void SV_BroadcastAnnouncerEvent(announcer_event_type_e event, Mobj *source);
void SV_BroadcastTICFinished(void);

// [CG] Handler functions.
int SV_HandleClientConnection(ENetPeer *peer);
bool SV_HandleJoinRequest(int playernum);
void SV_HandlePlayerMessage(char *data, size_t data_length, int playernum);
void SV_HandleUpdatePlayerInfoMessage(char *data, size_t data_length,
                                      int playernum);
void SV_HandlePlayerCommandMessage(char *data, size_t data_length,
                                   int playernum);
void SV_HandleUpdatePlayerInfoMessage(char *data, size_t data_length,
                                      int playernum);
void SV_HandleMessage(char *data, size_t data_length, int playernum);
void SV_HandleSyncRequestMessage(char *data, size_t data_length,
                                 int playernum);
void SV_HandleSyncReceivedMessage(char *data, size_t data_length,
                                  int playernum);

#endif

