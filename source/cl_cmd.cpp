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
//   C/S client commands.
//
//----------------------------------------------------------------------------

#include <list>
#include <string>

#include "z_zone.h"
#include "doomstat.h"
#include "doomtype.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "i_system.h"
#include "i_thread.h"
#include "m_misc.h"
#include "st_stuff.h"
#include "v_misc.h"

#include "cs_main.h"
#include "cs_hud.h"
#include "cs_config.h"
#include "cl_cmd.h"
#include "cl_main.h"
#include "cl_net.h"
#include "cl_buf.h"

bool cl_predict_shots = true;
bool default_cl_predict_shots = true;

bool cl_packet_buffer_enabled = false;
bool default_cl_packet_buffer_enabled = false;

unsigned int cl_packet_buffer_size = 0;
unsigned int default_cl_packet_buffer_size = 0;

unsigned int damage_screen_cap = NUMREDPALS;
unsigned int default_damage_screen_cap = NUMREDPALS;

// [CG] Shot result prediction.
VARIABLE_TOGGLE(cl_predict_shots, &default_cl_predict_shots, yesno);
CONSOLE_VARIABLE(predict_shots, cl_predict_shots, cf_netonly) {}

// [CG] Packet buffer.
VARIABLE_TOGGLE(cl_packet_buffer_enabled, &default_cl_packet_buffer_enabled,
                onoff);
CONSOLE_VARIABLE(packet_buffer, cl_packet_buffer_enabled, cf_netonly)
{
   if(cl_packet_buffer_enabled)
      cl_packet_buffer.enable();
   else
      cl_packet_buffer.disable();
}
VARIABLE_INT(
   cl_packet_buffer_size,
   &default_cl_packet_buffer_size,
   0, MAX_POSITIONS >> 1, NULL
);
CONSOLE_VARIABLE(packet_buffer_size, cl_packet_buffer_size, cf_netonly)
{
   cl_packet_buffer.setCapacity((uint32_t)cl_packet_buffer_size);
}

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

   team_buffer = Console.argv[0]->getBuffer();
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
VARIABLE_TOGGLE(show_netstats, &default_show_netstats, yesno);
CONSOLE_VARIABLE(show_netstats, show_netstats, 0) {}

// [CG] HUD timer.
VARIABLE_TOGGLE(show_timer, &default_show_timer, yesno);
CONSOLE_VARIABLE(show_timer, show_timer, 0) {}

// [CG] Team widget.
VARIABLE_TOGGLE(show_team_widget, &default_show_team_widget, yesno);
CONSOLE_VARIABLE(show_team, show_team_widget, 0) {}

// [CG] Damage screen ("red screen") cap.
VARIABLE_INT(
   damage_screen_cap,
   &default_damage_screen_cap,
   0, NUMREDPALS, NULL
);
CONSOLE_VARIABLE(damage_screen_cap, damage_screen_cap, 0) {}

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
   char *password;
   size_t password_size = strlen(Console.args.constPtr());

   if(Console.argc < 1)
   {
      C_Printf(FC_HI"Usage:" FC_NORMAL " password <password>\n");
      return;
   }

   password = ecalloc(char *, password_size, sizeof(char));
   // [CG] Use Console.args to support passwords that contain spaces.
   // [CG] Console.args apparently has a bug where there's a space appended to
   //      it, so strip it here.
   strncpy(password, Console.args.constPtr(), password_size - 1);

   CL_SendAuthMessage(password);

   efree(password);
}

CONSOLE_COMMAND(spectate, cf_netonly)
{
   // [CG] As a bonus, this will reset displayplayer to consoleplayer.
   CS_SetDisplayPlayer(consoleplayer);
   CL_Spectate();
}

void CL_AddCommands(void)
{
   C_AddCommand(team);
   C_AddCommand(show_netstats);
   C_AddCommand(show_timer);
   C_AddCommand(show_team);
   C_AddCommand(predict_shots);
   C_AddCommand(packet_buffer);
   C_AddCommand(packet_buffer_size);
   C_AddCommand(damage_screen_cap);
   C_AddCommand(disconnect);
   C_AddCommand(reconnect);
   C_AddCommand(password);
   C_AddCommand(spectate);
}

