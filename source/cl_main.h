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
//   Client/Server client routines.
//
//----------------------------------------------------------------------------

#ifndef CL_MAIN_H__
#define CL_MAIN_H__

#include "e_mod.h"
#include "p_spec.h"

#include "cs_main.h"

#define CL_MAX_BUFFER_SIZE MAX_POSITIONS

#define PASSWORD_FILENAME "passwords.json"

typedef struct
{
   // [CG] tic is necessary because the server needs to know when clients are
   //      seeing other clients for unlagged; everything related to unlagged
   //      is based on the server's gametic, so the server sends statuses at a
   //      given TIC, and clients acknowledge receipt of a status for the same
   //      TIC.
   int tic;
   bool has_command;
   // [CG] Clients only care about a position's index member if it's their own
   //      position; they use it to check their prediction.  The value is
   //      the index of the last command run that resulted in this position.
   cs_player_position_t position;
   cs_cmd_t command;
} client_status_t;

typedef struct
{
   Mobj *local_ghost;
   Mobj *remote_ghost;
} cl_ghost_t;

extern char *cs_server_url;
extern char *cs_server_password;
extern char *cs_client_password_file;
extern Json::Value cs_client_password_json;

extern unsigned int cl_current_world_index;
extern unsigned int cl_latest_world_index;
extern unsigned int cl_commands_sent;

extern bool cl_initial_spawn;
extern bool cl_received_first_sync;
extern bool cl_spawning_actor_from_message;
extern bool cl_removing_actor_from_message;
extern bool cl_setting_player_weapon_sprites;
extern bool cl_handling_damaged_actor;
extern bool cl_handling_damaged_actor_and_justhit;
extern bool cl_setting_actor_state;

extern cl_ghost_t cl_unlagged_ghosts[MAXPLAYERS];

void  CL_Init(char *url);
void  CL_InitAnnouncer();
void  CL_InitPlayDemoMode(void);
void  CL_InitNetworkMessageHash(void);
void  CL_Reset(void);
char* CL_GetUserAgent(void);
void  CL_Connect(void);
void  CL_Reconnect(void);
void  CL_Disconnect(void);

void  CL_SpawnLocalGhost(Mobj *actor);
void  CL_SpawnRemoteGhost(unsigned int net_id, fixed_t x, fixed_t y, fixed_t z,
                          angle_t angle, unsigned int world_index);
void  CL_SaveServerPassword(void);
void  CL_MessageTeam(event_t *ev);
void  CL_MessageServer(event_t *ev);
void  CL_RCONMessage(event_t *ev);
void  CL_LoadGame(const char *path);
Mobj* CL_SpawnMobj(uint32_t net_id, fixed_t x, fixed_t y, fixed_t z,
                   mobjtype_t type);
void  CL_SpawnPlayer(int playernum, uint32_t net_id, fixed_t x, fixed_t y,
                     fixed_t z, angle_t angle, bool as_spectator);
void  CL_RemovePlayer(int playernum);
Mobj* CL_SpawnPuff(uint32_t net_id, fixed_t x, fixed_t y, fixed_t z,
                   angle_t angle, int updown, bool ptcl);
Mobj* CL_SpawnBlood(uint32_t net_id, fixed_t x, fixed_t y, fixed_t z,
                    angle_t angle, int damage, Mobj *target);
void  CL_RemoveMobj(Mobj *actor);
void  CL_LoadWADs(void);
bool  CL_SetMobjState(Mobj *mobj, statenum_t state);
void  CL_PlayerThink(int playernum);
Mobj* CL_SpawnMissile(Mobj *source, Mobj *target,
                      fixed_t x, fixed_t y, fixed_t z,
                      fixed_t momx, fixed_t momy, fixed_t momz,
                      angle_t angle, mobjtype_t type);
char* CL_ExtractServerMessage(nm_servermessage_t *message);
char* CL_ExtractPlayerMessage(nm_playermessage_t *message);
void  CL_SetActorNetID(Mobj *actor, unsigned int net_id);
void  CL_HandleMessage(char *data, size_t data_length);
void  CL_AdvancePlayerPositions(void);
void  CL_ProcessNetworkMessages(void);
void  CL_MoveCommands(unsigned int last_index, unsigned int position_index);
void  CL_SetPsprite(player_t *player, int position, statenum_t stnum);
void  CL_HandleDamagedMobj(Mobj *target, Mobj *source, int damage, int mod,
                           emod_t *emod, bool justhit);
void  CL_SetLatestFinishedIndices(unsigned int index);
void  CL_RunDemoTics(void);
void  CL_TryRunTics(void);

#endif

