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
//   C/S server commands.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include <json/json.h>

#include "c_io.h"
#include "c_runcmd.h"
#include "d_net.h"
#include "doomstat.h"
#include "m_random.h"

#include "cs_main.h"
#include "cs_wad.h"
#include "sv_bans.h"
#include "sv_main.h"

CONSOLE_COMMAND(kick, cf_server)
{
   int playernum;
   char *name = NULL;
   const char *reason = NULL;

   if(!Console.argc)
   {
      C_Printf("usage: kick <playernum> <reason>\n"
               " use playerinfo to find playernum\n"
               " reason is optional\n");
      return;
   }

   playernum = Console.argv[0]->toInt();

   if(!playeringame[playernum])
      return;

   name = players[playernum].name;

   if(Console.argc > 1)
      reason = Console.argv[1]->constPtr();

   if(clientserver)
   {
      if(reason)
      {
         SV_BroadcastMessage("%s was kicked from the game: %s", name, reason);
         doom_printf("%s was kicked from the game: %s", name, reason);
      }
      else
      {
         SV_BroadcastMessage("%s was kicked from the game", name);
         doom_printf("%s was kicked from the game", name);
      }
      SV_DisconnectPlayer(playernum, dr_kicked);
   }
   else
      D_KickPlayer(playernum);
}

CONSOLE_COMMAND(ban, cf_server)
{
   int i, playernum;
   uint32_t ip_address;
   char *address = NULL;
   const char *name = NULL;
   const char *reason = NULL;
   int minutes = 0;

   if(Console.argc < 2 || Console.argc > 3)
   {
      C_Printf("usage: ban <playernum> <reason> <minutes>\n"
               " use playerinfo to find playernum\n"
               " minutes is optional\n");
      return;
   }

   playernum = Console.argv[0]->toInt();

   if(!playeringame[playernum])
      return;

   name = (const char *)players[playernum].name;
   reason = Console.argv[1]->constPtr();
   ip_address = server_clients[playernum].address.host;
   address = CS_IPToString(ip_address);
   if(Console.argc == 3)
      minutes = Console.argv[2]->toInt();

   if(sv_access_list->addBanListEntry(address, name, reason, minutes))
   {
      for(i = 1; i < MAX_CLIENTS; i++)
      {
         if(playeringame[i] &&
            server_clients[playernum].address.host == ip_address)
         {
            if(minutes > 0)
            {
               SV_BroadcastMessage(
                  "%s was banned for %d minutes: %s", name, minutes, reason
               );
               doom_printf(
                  "%s was banned for %d minutes: %s", name, minutes, reason
               );
            }
            else
            {
               SV_BroadcastMessage("%s was banned: %s", name, reason);
               doom_printf("%s was banned: %s", name, reason);
            }
            SV_DisconnectPlayer(i, dr_banned);
         }
      }
   }
   else
      C_Printf("Error: %s\n", sv_access_list->getError());

   efree(address);
}

CONSOLE_COMMAND(list_bans, cf_server)
{
   sv_access_list->printBansToConsole();
}

CONSOLE_COMMAND(unban, cf_server)
{
   if(!Console.argc)
   {
      C_Printf("usage: unban <address>\n");
      return;
   }

   if(!sv_access_list->removeBanListEntry(Console.argv[1]->constPtr()))
      C_Printf("Error: %s\n", sv_access_list->getError());
}

CONSOLE_COMMAND(whitelist, cf_server)
{
   const char *name = NULL;
   const char *address = NULL;

   if(Console.argc < 2)
   {
      C_Printf("usage: whitelist <address> <name>\n");
      return;
   }

   name = Console.argv[0]->constPtr();
   address = Console.argv[1]->constPtr();

   if(!sv_access_list->addWhiteListEntry(address, name))
      C_Printf("Error: %s\n", sv_access_list->getError());
}

CONSOLE_COMMAND(list_whitelists, cf_server)
{
   sv_access_list->printWhiteListsToConsole();
}

CONSOLE_COMMAND(unwhitelist, cf_server)
{
   if(!Console.argc)
   {
      C_Printf("usage: unwhitelist <address>\n");
      return;
   }

   if(!sv_access_list->removeWhiteListEntry(Console.argv[0]->constPtr()))
      C_Printf("Error: %s\n", sv_access_list->getError());
}

CONSOLE_COMMAND(coinflip, cf_server)
{
   if(M_Random() / 2)
      SV_BroadcastHUDMessage("Coin flip result: Heads");
   else
      SV_BroadcastHUDMessage("Coin flip result: Tails");
}

CONSOLE_COMMAND(random_map_number, cf_server)
{
   SV_BroadcastHUDMessage(
      "Random map number: %d.\n", M_Random() % cs_map_count
   );
}

void SV_AddCommands(void)
{
   C_AddCommand(kick);
   C_AddCommand(ban);
   C_AddCommand(list_bans);
   C_AddCommand(unban);
   C_AddCommand(whitelist);
   C_AddCommand(list_whitelists);
   C_AddCommand(unwhitelist);
   C_AddCommand(coinflip);
   C_AddCommand(random_map_number);
}

