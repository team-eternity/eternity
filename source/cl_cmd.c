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
//   C/S client commands.
//
//----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include "doomstat.h"
#include "doomtype.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "i_system.h"
#include "m_misc.h"

#include "cs_main.h"
#include "cs_hud.h"
#include "cs_config.h"
#include "cl_cmd.h"
#include "cl_main.h"

boolean cl_flush_packet_buffer = false;

boolean cl_predict_shots = true;
boolean default_cl_predict_shots = true;

boolean cl_enable_prediction = true;
boolean default_cl_enable_prediction = true;

unsigned int cl_packet_buffer_size;
unsigned int default_cl_packet_buffer_size = 0;

boolean cl_buffer_packets_while_spectating;
boolean default_cl_buffer_packets_while_spectating = true;

boolean cl_flush_packet_buffer_on_respawn;
boolean default_cl_flush_packet_buffer_on_respawn = true;

boolean cl_constant_prediction;
boolean default_cl_constant_prediction = true;

// [CG] Clientside prediction.
VARIABLE_TOGGLE(cl_enable_prediction, &default_cl_enable_prediction, onoff);
CONSOLE_VARIABLE(prediction, cl_enable_prediction, cf_netonly) {}

// [CG] Shot result prediction.
VARIABLE_TOGGLE(cl_predict_shots, &default_cl_predict_shots, yesno);
CONSOLE_VARIABLE(predict_shots, cl_predict_shots, cf_netonly) {}

// [CG] Constant prediction.
VARIABLE_TOGGLE(
   cl_constant_prediction,
   &default_cl_constant_prediction,
   onoff
);
CONSOLE_VARIABLE(constant_prediction, cl_constant_prediction, cf_netonly) {}

// [CG] Team.
CONSOLE_COMMAND(team, cf_netonly)
{
   char *team_buffer;

   if(!CS_TEAMS_ENABLED)
   {
      doom_printf("Cannot switch teams in non-team games.");
      return;
   }

   if(!Console.argc)
   {
      doom_printf("Team: %s", team_color_names[clients[consoleplayer].team]);
      return;
   }

   team_buffer = QStrBuffer(&Console.argv[0]);
   if(strncasecmp(team_buffer, team_color_names[team_color_red], 3) == 0)
      CL_SendTeamRequest(team_color_red);
   else if(strncasecmp(team_buffer, team_color_names[team_color_blue], 4) == 0)
      CL_SendTeamRequest(team_color_blue);
   else if(strncasecmp(team_buffer, team_color_names[team_color_none], 4) == 0)
      doom_printf("Cannot switch off all teams in team games.\n");
   else
      doom_printf("Invalid team '%s'.\n", team_buffer);
}

// [CG] Netstats.
VARIABLE_TOGGLE(show_netstats, &default_show_netstats, onoff);
CONSOLE_VARIABLE(show_netstats, show_netstats, 0) {}

// [CG] HUD timer.
VARIABLE_TOGGLE(show_timer, &default_show_timer, onoff);
CONSOLE_VARIABLE(show_timer, show_timer, 0) {}

// [CG] Packet buffer flushing.
CONSOLE_COMMAND(flush_packet_buffer, cf_netonly)
{
   cl_flush_packet_buffer = true;
}

// [CG] Packet buffer size.
VARIABLE_INT(
   cl_packet_buffer_size,
   &default_cl_packet_buffer_size,
   0, MAX_POSITIONS, NULL
);
CONSOLE_VARIABLE(packet_buffer_size, cl_packet_buffer_size, cf_netonly) {}

// [CG] Packet buffer flushing when the player's spectating.
VARIABLE_TOGGLE(
   cl_buffer_packets_while_spectating,
   &default_cl_buffer_packets_while_spectating,
   onoff
);
CONSOLE_VARIABLE(
   buffer_packets_while_spectating,
   cl_buffer_packets_while_spectating,
   cf_netonly
) {}

// [CG] Packet buffer flushing when the player respawns.
VARIABLE_TOGGLE(
   cl_flush_packet_buffer_on_respawn,
   &default_cl_flush_packet_buffer_on_respawn,
   onoff
);
CONSOLE_VARIABLE(
   flush_packet_buffer_on_respawn,
   cl_flush_packet_buffer_on_respawn,
   cf_netonly
) {}

CONSOLE_COMMAND(disconnect, cf_netonly)
{
   CL_Disconnect();
}

CONSOLE_COMMAND(reconnect, cf_netonly)
{
   if(net_peer != NULL)
   {
      doom_printf("Disconnecting....");
      CL_Disconnect();
      // [CG] Sleep for whatever MAX_LATENCY is set to (in milliseconds).
      I_Sleep(MAX_LATENCY * 1000);
   }

   doom_printf("Reconnecting....");

   CL_Connect();
}

CONSOLE_COMMAND(password, cf_netonly)
{
   char *pw_buffer, *pw;
   size_t length;

   if(Console.argc < 1)
   {
      C_Printf(FC_HI"Usage:" FC_NORMAL " password <password>\n");
      return;
   }

   // [CG] Use Console.args to support passwords that contain spaces.
   // [CG] ZARG.  EE has a bug where Console.args has a space at the end of it.
   //      So we use calloc to zero the string buffer, and we give memcpy a
   //      shorter-than-normal-by-one size to copy.
   pw_buffer = QStrBuffer(&Console.args);
   length = strlen(pw_buffer);
   pw = calloc(length + 1, sizeof(char));
   memcpy(pw, pw_buffer, length - 1);
   CL_AuthMessage(pw);
   free(pw);
}

CONSOLE_COMMAND(spectate, cf_netonly)
{
   // [CG] As a bonus, this will reset displayplayer to consoleplayer.
   CS_SetDisplayPlayer(consoleplayer);
   CL_Spectate();
}

CONSOLE_COMMAND(spectate_prev, cf_netonly)
{
   int i = displayplayer;
   client_t *client = &clients[consoleplayer];

   if(!client->spectating)
   {
      doom_printf("Cannot spectate other players while not a spectator.\n");
      return;
   }

   do
   {
      i--;

      if(i == -1)
         i = (MAXPLAYERS - 1);

      if(i == displayplayer)
         return;

   } while(!playeringame[i] || clients[i].spectating ||
           (CS_TEAMS_ENABLED && clients[i].team != client->team));

   CS_SetDisplayPlayer(i);
}

CONSOLE_COMMAND(spectate_next, cf_netonly)
{
   int i = displayplayer;
   client_t *client = &clients[consoleplayer];

   if(!client->spectating)
   {
      doom_printf("Cannot spectate other players while not a spectator.\n");
      return;
   }

   do
   {
      i++;

      if(i == MAXPLAYERS - 1)
         i = 0;

      if(i == displayplayer)
         return;

   } while(!playeringame[i] || clients[i].spectating ||
           (CS_TEAMS_ENABLED && clients[i].team != client->team));

   CS_SetDisplayPlayer(i);
}

void CL_AddCommands(void)
{
   C_AddCommand(prediction);
   C_AddCommand(predict_shots);
   C_AddCommand(constant_prediction);
   C_AddCommand(team);
   C_AddCommand(show_netstats);
   C_AddCommand(show_timer);
   C_AddCommand(flush_packet_buffer);
   C_AddCommand(packet_buffer_size);
   C_AddCommand(buffer_packets_while_spectating);
   C_AddCommand(flush_packet_buffer_on_respawn);
   C_AddCommand(disconnect);
   C_AddCommand(reconnect);
   C_AddCommand(password);
   C_AddCommand(spectate);
   C_AddCommand(spectate_prev);
   C_AddCommand(spectate_next);
}

