// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------

#ifndef C_NET_H__
#define C_NET_H__

#include "d_keywds.h"

struct command_t;

// net command numbers

// NETCODE_FIXME -- CONSOLE_FIXME: Netcmd system must be replaced.

enum
{
   netcmd_null,       // 0 is empty
   netcmd_colour,
   netcmd_deathmatch,
   netcmd_exitlevel,
   netcmd_fast,
   netcmd_kill,
   netcmd_name,
   netcmd_nomonsters,
   netcmd_nuke,
   netcmd_respawn,
   netcmd_skill,
   netcmd_skin,
   netcmd_allowmlook,
   netcmd_bobbing, // haleyjd
   netcmd_autoaim,
   netcmd_bfgtype,
   netcmd_recoil,
   netcmd_pushers,
   netcmd_varfriction,
   netcmd_chat,
   netcmd_monremember,
   netcmd_moninfight,
   netcmd_monbacking,
   netcmd_monavoid,
   netcmd_monfriction,
   netcmd_monclimb,
   netcmd_monhelpfriends,
   netcmd_mondistfriend,
   netcmd_map,
   netcmd_restartmap,
   netcmd_nukage,
   netcmd_timelimit,
   netcmd_fraglimit,
   netcmd_dmflags,       // haleyjd 04/14/03
   netcmd_dogjumping,    // haleyjd 11/06/05
   netcmd_pitchedflight, // haleyjd 06/07/12
   NETCMD_COMP_0,        // keep this as 0-base index
   netcmd_comp_telefrag = NETCMD_COMP_0,
   netcmd_comp_dropoff,
   netcmd_comp_vile,
   netcmd_comp_pain,
   netcmd_comp_skull,
   netcmd_comp_blazing,
   netcmd_comp_doorlight,
   netcmd_comp_model,
   netcmd_comp_god,
   netcmd_comp_falloff,
   netcmd_comp_floors,
   netcmd_comp_skymap,
   netcmd_comp_pursuit,
   netcmd_comp_doorstuck,
   netcmd_comp_staylift,
   netcmd_comp_zombie,
   netcmd_comp_stairs,
   netcmd_comp_infcheat,
   netcmd_comp_zerotags,
   netcmd_comp_terrain,     // haleyjd: TerrainTypes
   netcmd_comp_respawnfix,  //          respawn fix
   netcmd_comp_fallingdmg,  //          falling damage
   netcmd_comp_soul,        //          lost soul bouncing
   netcmd_comp_theights,    //          thing heights fix
   netcmd_comp_overunder,   //          extended z clipping
   netcmd_comp_planeshoot,  //          plane shooting
   netcmd_comp_special,     //          special failure
   netcmd_comp_ninja,       //          ninja spawn
   netcmd_comp_jump,        // ioanch:  air control for jumping
   netcmd_comp_aircontrol = netcmd_comp_jump,
   NUMNETCMDS
};

void C_InitPlayerName(void); // haleyjd
void C_SendCmd(int dest, int cmdnum, E_FORMAT_STRING(const char *s), ...) E_PRINTF(3, 4);
void C_queueChatChar(unsigned char c);
unsigned char C_dequeueChatChar(void);
void C_NetTicker(void);
void C_NetInit(void);
void C_SendNetData(void);
void C_UpdateVar(command_t *command);

struct command_t;
extern command_t *c_netcmds[NUMNETCMDS];
extern char* default_name;
extern int default_colour;

#define CN_BROADCAST 128

#endif

// EOF
