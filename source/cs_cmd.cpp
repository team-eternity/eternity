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

#include "z_zone.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_player.h"
#include "d_ticcmd.h"
#include "p_mobj.h"
#include "p_user.h"

#include "cs_cmd.h"
#include "cs_ctf.h"
#include "cs_position.h"

void CS_PrintCommand(cs_cmd_t *command)
{
   printf(
      "%u: %d/%d %d/%d %d/%d.\n",
      command->world_index,
      command->forwardmove,
      command->sidemove,
      command->look,
      command->angleturn,
      command->buttons,
      command->actions
   );
}

void CS_PrintTiccmd(ticcmd_t *command)
{
   printf(
      "%d/%d %d/%d %d/%d.\n",
      command->forwardmove,
      command->sidemove,
      command->look,
      command->angleturn,
      command->buttons,
      command->actions
   );
}

void CS_CopyCommand(cs_cmd_t *dest, cs_cmd_t *src)
{
   dest->index       = src->index;
   dest->world_index = src->world_index;
   dest->forwardmove = src->forwardmove;
   dest->sidemove    = src->sidemove;
   dest->look        = src->look;
   dest->angleturn   = src->angleturn;
   dest->buttons     = src->buttons;
   dest->actions     = src->actions;
}

void CS_CopyTiccmdToCommand(cs_cmd_t *command, ticcmd_t *cmd, uint32_t index,
                            uint32_t world_index)
{
   command->index       = index;
   command->world_index = world_index;
   command->forwardmove = cmd->forwardmove;
   command->sidemove    = cmd->sidemove;
   command->look        = cmd->look;
   command->angleturn   = cmd->angleturn;
   command->buttons     = cmd->buttons;
   command->actions     = cmd->actions;
}

void CS_CopyCommandToTiccmd(ticcmd_t *cmd, cs_cmd_t *command)
{
   cmd->forwardmove = command->forwardmove;
   cmd->sidemove    = command->sidemove;
   cmd->look        = command->look;
   cmd->angleturn   = command->angleturn;
   cmd->buttons     = command->buttons;
   cmd->actions     = command->actions;
}

bool CS_CommandsEqual(cs_cmd_t *command_one, cs_cmd_t *command_two)
{
   if(command_one->forwardmove == command_two->forwardmove &&
      command_one->sidemove    == command_two->sidemove    &&
      command_one->look        == command_two->look        &&
      command_one->angleturn   == command_two->angleturn   &&
      command_one->buttons     == command_two->buttons     &&
      command_one->actions     == command_two->actions)
   {
      return true;
   }
   return false;
}

bool CS_CommandIsBlank(cs_cmd_t *command)
{
   // [CG] We don't care about consistancy or chatchar, but everything else has
   //      to be zero.
   if(command->forwardmove == 0 &&
      command->sidemove    == 0 &&
      command->look        == 0 &&
      command->angleturn   == 0 &&
      command->buttons     == 0 &&
      command->actions     == 0)
   {
      return true;
   }
   return false;
}

void CS_SavePlayerCommand(cs_cmd_t *command, int playernum, uint32_t index,
                          uint32_t world_index)
{
   player_t *player = &players[playernum];

   command->index       = index;
   command->world_index = world_index;
   command->forwardmove = player->cmd.forwardmove;
   command->sidemove    = player->cmd.sidemove;
   command->look        = player->cmd.look;
   command->angleturn   = player->cmd.angleturn;
   command->buttons     = player->cmd.buttons;
   command->actions     = player->cmd.actions;
}

void CS_SetPlayerCommand(int playernum, cs_cmd_t *command)
{
   player_t *player = &players[playernum];

   player->cmd.forwardmove = command->forwardmove;
   player->cmd.sidemove    = command->sidemove;
   player->cmd.look        = command->look;
   player->cmd.angleturn   = command->angleturn;
   player->cmd.buttons     = command->buttons;
   player->cmd.actions     = command->actions;
}

void CS_RunPlayerCommand(int playernum, ticcmd_t *cmd, bool think)
{
   player_t *player = &players[playernum];

   memcpy(&player->cmd, cmd, sizeof(ticcmd_t));

   if(player->playerstate == PST_DEAD)
   {
      player->cmd.forwardmove = 0;
      player->cmd.sidemove = 0;
      player->cmd.look = 0;
      player->cmd.angleturn = 0;
      if(player->cmd.buttons & BT_USE)
         player->cmd.buttons = BT_USE;
      else
         player->cmd.buttons = 0;
      player->cmd.actions = 0;
   }

   P_RunPlayerCommand(playernum);

   if(think && player->mo)
      player->mo->Think();
}

