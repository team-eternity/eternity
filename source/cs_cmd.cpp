// Emacs style mode select -*- C++ -*- vim:sw=3 ts=3:
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

#include <stdio.h>

#include "doomstat.h"
#include "d_ticcmd.h"
#include "d_player.h"

#include "cs_position.h"
#include "cs_cmd.h"

void CS_PrintCommand(cs_cmd_t *command)
{
   printf(
      "%u: %d/%d %d/%d %d/%d.\n",
      command->world_index,
      command->ticcmd.forwardmove,
      command->ticcmd.sidemove,
      command->ticcmd.look,
      command->ticcmd.angleturn,
      command->ticcmd.buttons,
      command->ticcmd.actions
   );
}

void CS_PrintTiccmd(ticcmd_t *cmd)
{
   printf(
      "%d/%d %d/%d %d/%d.\n",
      cmd->forwardmove,
      cmd->sidemove,
      cmd->look,
      cmd->angleturn,
      cmd->buttons,
      cmd->actions
   );
}

void CS_CopyCommand(cs_cmd_t *dest, cs_cmd_t *src)
{
   dest->world_index        = src->world_index;
   dest->ticcmd.forwardmove = src->ticcmd.forwardmove;
   dest->ticcmd.sidemove    = src->ticcmd.sidemove;
   dest->ticcmd.look        = src->ticcmd.look;
   dest->ticcmd.angleturn   = src->ticcmd.angleturn;
   dest->ticcmd.buttons     = src->ticcmd.buttons;
   dest->ticcmd.actions     = src->ticcmd.actions;
}

void CS_CopyTiccmdToCommand(cs_cmd_t *command, ticcmd_t *cmd,
                            unsigned int index)
{
   command->world_index        = index;
   command->ticcmd.forwardmove = cmd->forwardmove;
   command->ticcmd.sidemove    = cmd->sidemove;
   command->ticcmd.look        = cmd->look;
   command->ticcmd.angleturn   = cmd->angleturn;
   command->ticcmd.buttons     = cmd->buttons;
   command->ticcmd.actions     = cmd->actions;
}

void CS_CopyCommandToTiccmd(ticcmd_t *cmd, cs_cmd_t *command)
{
   cmd->forwardmove = command->ticcmd.forwardmove;
   cmd->sidemove    = command->ticcmd.sidemove;
   cmd->look        = command->ticcmd.look;
   cmd->angleturn   = command->ticcmd.angleturn;
   cmd->buttons     = command->ticcmd.buttons;
   cmd->actions     = command->ticcmd.actions;
}

boolean CS_CommandsEqual(cs_cmd_t *command_one, cs_cmd_t *command_two)
{
   if(command_one->ticcmd.forwardmove == command_two->ticcmd.forwardmove &&
      command_one->ticcmd.sidemove    == command_two->ticcmd.sidemove    &&
      command_one->ticcmd.look        == command_two->ticcmd.look        &&
      command_one->ticcmd.angleturn   == command_two->ticcmd.angleturn   &&
      command_one->ticcmd.buttons     == command_two->ticcmd.buttons     &&
      command_one->ticcmd.actions     == command_two->ticcmd.actions)
   {
      return true;
   }
   return false;
}

boolean CS_CommandIsBlank(cs_cmd_t *command)
{
   // [CG] We don't care about consistancy or chatchar, but everything else has
   //      to be zero.
   if(command->ticcmd.forwardmove == 0 &&
      command->ticcmd.sidemove    == 0 &&
      command->ticcmd.look        == 0 &&
      command->ticcmd.angleturn   == 0 &&
      command->ticcmd.buttons     == 0 &&
      command->ticcmd.actions     == 0)
   {
      return true;
   }
   return false;
}

void CS_SavePlayerCommand(cs_cmd_t *command, unsigned int playernum,
                          unsigned int index)
{
   player_t *player = &players[playernum];

   command->world_index        = index;
   command->ticcmd.forwardmove = player->cmd.forwardmove;
   command->ticcmd.sidemove    = player->cmd.sidemove;
   command->ticcmd.look        = player->cmd.look;
   command->ticcmd.angleturn   = player->cmd.angleturn;
   command->ticcmd.buttons     = player->cmd.buttons;
   command->ticcmd.actions     = player->cmd.actions;
}

void CS_SetPlayerCommand(unsigned int playernum, cs_cmd_t *command)
{
   player_t *player = &players[playernum];

   player->cmd.forwardmove = command->ticcmd.forwardmove;
   player->cmd.sidemove    = command->ticcmd.sidemove;
   player->cmd.look        = command->ticcmd.look;
   player->cmd.angleturn   = command->ticcmd.angleturn;
   player->cmd.buttons     = command->ticcmd.buttons;
   player->cmd.actions     = command->ticcmd.actions;
}

