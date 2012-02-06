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
//   Client/Server routines for handling commands.
//
//----------------------------------------------------------------------------

#ifndef CS_CMD_H__
#define CS_CMD_H__

#include "doomtype.h"
#include "d_ticcmd.h"
#include "m_queue.h"
#include "cs_position.h"

// [CG] Commands are sent over the wire and stored in demos, and therefore must
//      be packed and use exact-width integer types.

#pragma pack(push, 1)

typedef struct cs_cmd_s
{
   uint32_t index;
   uint32_t world_index;
   int8_t   forwardmove;
   int8_t   sidemove;
   int16_t  look;
   int16_t  angleturn;
   uint8_t  buttons;
   uint8_t  actions;
} cs_cmd_t;

#pragma pack(pop)

typedef struct cs_buffered_command_s
{
   mqueueitem_t mqitem;
   cs_cmd_t command;
} cs_buffered_command_t;

void CS_PrintCommand(cs_cmd_t *command);
void CS_PrintTiccmd(ticcmd_t *cmd);
void CS_CopyCommand(cs_cmd_t *dest, cs_cmd_t *src);
void CS_CopyTiccmdToCommand(cs_cmd_t *command, ticcmd_t *cmd, uint32_t index,
                            uint32_t world_index);
void CS_CopyCommandToTiccmd(ticcmd_t *cmd, cs_cmd_t *command);
bool CS_CommandsEqual(cs_cmd_t *command_one, cs_cmd_t *command_two);
bool CS_CommandIsBlank(cs_cmd_t *command);
void CS_SavePlayerCommand(cs_cmd_t *command, int playernum, uint32_t index,
                          uint32_t world_index);
void CS_SetPlayerCommand(unsigned int playernum, cs_cmd_t *command);
void CS_RunPlayerCommand(int playernum, ticcmd_t *cmd, bool think);

#endif

