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
//   Client/Server configuration loading and parsing.
//
//----------------------------------------------------------------------------

#include <string>
#include <vector>

#include <stdio.h>
#include <string.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_iwad.h"
#include "d_main.h"
#include "d_player.h"
#include "d_ticcmd.h"
#include "doomdef.h"
#include "doomstat.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "p_inter.h"
#include "v_misc.h"
#include "v_video.h"
#include "version.h"
#include "w_wad.h"

#include "cs_main.h"
#include "cs_config.h"
#include "cs_master.h"
#include "cs_wad.h"
#include "cl_main.h"
#include "sv_main.h"

#include <json/json.h>
#include <curl/curl.h>

void D_CheckGamePath(char *game);

// [CG] Zarg huge defines.

#define set_int(options, option_name, default_value)\
   if(options[#option_name].empty())\
      cs_original_settings->option_name = default_value;\
   else\
      cs_original_settings->option_name = options[#option_name].asInt();

#define set_float(options, option_name, default_value)\
   if(options[#option_name].empty())\
      cs_original_settings->option_name = default_value;\
   else\
      cs_original_settings->option_name = options[#option_name].asFloat();

#define set_bool(options, option_name, default_value)\
   if(options[#option_name].empty())\
      cs_original_settings->option_name = default_value;\
   else\
      cs_original_settings->option_name = options[#option_name].asBool();

#define set_flags(options, option_name, flags, default_value)\
   if(options[#option_name].empty())\
   {\
      if((default_value))\
      {\
         flags |= dmf_##option_name;\
         cs_original_settings->flags |= dmf_##option_name;\
      }\
      else\
      {\
         flags &= ~dmf_##option_name;\
         cs_original_settings->flags &= ~dmf_##option_name;\
      }\
   }\
   else if(options[#option_name].asBool())\
   {\
      flags |= dmf_##option_name;\
      cs_original_settings->flags |= dmf_##option_name;\
   }\
   else\
   {\
      flags &= ~dmf_##option_name;\
      cs_original_settings->flags &= ~dmf_##option_name;\
   }

#define set_comp(options, config_name, option_name, default_value)\
   if(options[#config_name].empty())\
   {\
      if((default_value))\
      {\
         default_comp[comp_##option_name] = true;\
         cs_original_settings->compatflags |= cmf_comp_##option_name;\
      }\
      else\
      {\
         default_comp[comp_##option_name] = false;\
         cs_original_settings->compatflags &= ~cmf_comp_##option_name;\
      }\
   }\
   else if(options[#config_name].asBool())\
   {\
      default_comp[comp_##option_name] = true;\
      cs_original_settings->compatflags |= cmf_comp_##option_name;\
   }\
   else\
   {\
      default_comp[comp_##option_name] = false;\
      cs_original_settings->compatflags &= ~cmf_comp_##option_name;\
   }

#define set_comp2(options, config_name, opt, default_value)\
   if(options[#config_name].empty())\
   {\
      if((default_value))\
      {\
         opt = true;\
         cs_original_settings->compatflags2 |= cmf_##opt;\
      }\
      else\
      {\
         opt = false;\
         cs_original_settings->compatflags2 &= ~cmf_##opt;\
      }\
   }\
   else if(options[#config_name].asBool())\
   {\
      opt = true;\
      cs_original_settings->compatflags2 |= cmf_##opt;\
   }\
   else\
   {\
      opt = false;\
      cs_original_settings->compatflags2 &= ~cmf_##opt;\
   }

#define override_int(options, option_name)\
   if(!options[#option_name].empty())\
      cs_settings->option_name = options[#option_name].asInt();

#define override_float(options, option_name)\
   if(!options[#option_name].empty())\
      cs_settings->option_name = options[#option_name].asFloat();

#define override_flags(options, option_name, flags)\
   if(!options[#option_name].empty())\
   {\
      if(options[#option_name].asBool())\
      {\
         flags |= dmf_##option_name;\
         cs_settings->flags |= dmf_##option_name;\
      }\
      else\
      {\
         flags &= ~dmf_##option_name;\
         cs_settings->flags &= ~dmf_##option_name;\
      }\
   }

#define override_comp(options, config_name, option_name)\
   if(!options[#config_name].empty())\
   {\
      if(options[#option_name].asBool())\
      {\
         default_comp[comp_##option_name] = true;\
         cs_settings->compatflags |= cmf_comp_##option_name;\
      }\
      else\
      {\
         default_comp[comp_##option_name] = false;\
         cs_settings->compatflags &= ~cmf_comp_##option_name;\
      }\
   }

#define override_comp2(options, config_name, opt)\
   if(!options[#config_name].empty())\
   {\
      if(options[#config_name].asBool())\
      {\
         opt = true;\
         cs_settings->compatflags2 |= cmf_##opt;\
      }\
      else\
      {\
         opt = false;\
         cs_settings->compatflags2 &= ~cmf_##opt;\
      }\
   }

#define check_int_option_range(options, option_name, min, max)\
   if(!options[#option_name].empty())\
   {\
      if(options[#option_name].asInt() < (min))\
         I_Error("CS_LoadConfig: '" #option_name "' must be >= %d.\n", (min));\
      else if(options[#option_name].asInt() > (max))\
         I_Error("CS_LoadConfig: '" #option_name "' must be <= %d.\n", (max));\
   }

#define string_option_is(option, s) ((option).asString().compare((s)) == 0)

extern ENetAddress *server_address;
extern int levelFragLimit;
extern int levelTimeLimit;
extern int levelScoreLimit;
extern bool gamepathset;

extern void AddIWADDir(char *dir);

Json::Value cs_json;
clientserver_settings_t *cs_original_settings = NULL;
clientserver_settings_t *cs_settings = NULL;

static void CS_checkSectionHasOption(Json::Value& section,
                                     const char *section_name,
                                     const char *option_name)
{
   if(section[option_name].empty())
   {
      I_Error(
         "CS_LoadConfig: %s missing required option '%s'.\n",
         section_name,
         option_name
      );
   }
}

void CS_ValidateOptions(Json::Value &options)
{
   if(MAX_CLIENTS < MAXPLAYERS)
   {
      check_int_option_range(options, max_players, 1, MAX_CLIENTS);
   }
   else
   {
      check_int_option_range(options, max_players, 1, MAXPLAYERS);
   }
   check_int_option_range(options, max_players_per_team, 1, 16);
   check_int_option_range(options, dogs, 0, 3);
   check_int_option_range(options, skill, 1, 5);
   check_int_option_range(options, frag_limit, 0, 100000);
   check_int_option_range(options, time_limit, 0, 100000);
   check_int_option_range(options, score_limit, 0, 100000);
   check_int_option_range(options, death_time_limit, 0, 100000);
   check_int_option_range(options, respawn_protection_time, 0, 100000);
   check_int_option_range(options, friend_distance, 1, 1024);
   check_int_option_range(options, friendly_damage_percentage, 0, 100);

   if(options["death_time_expired_action"].empty())
      cs_original_settings->death_time_expired_action = DEATH_LIMIT_SPECTATE;
   else if(string_option_is(options["death_time_expired_action"], "spectate"))
      cs_original_settings->death_time_expired_action = DEATH_LIMIT_SPECTATE;
   else if(string_option_is(options["death_time_expired_action"], "respawn"))
      cs_original_settings->death_time_expired_action = DEATH_LIMIT_RESPAWN;
   else
   {
      I_Error(
         "CS_LoadConfig: 'death_time_expired_action' must be either "
         "'spectate' or 'respawn'.\n"
      );
   }

   if(options["bfg_type"].empty())
      cs_original_settings->bfg_type = bfg_normal;
   else if(string_option_is(options["bfg_type"], "9000"))
      cs_original_settings->bfg_type = bfg_normal;
   else if(string_option_is(options["bfg_type"], "2704"))
      cs_original_settings->bfg_type = bfg_classic;
   else if(string_option_is(options["bfg_type"], "11000"))
      cs_original_settings->bfg_type = bfg_11k;
   else if(string_option_is(options["bfg_type"], "bouncing"))
      cs_original_settings->bfg_type = bfg_bouncing;
   else if(string_option_is(options["bfg_type"], "plasma burst"))
      cs_original_settings->bfg_type = bfg_burst;
   else
   {
      I_Error(
         "CS_LoadConfig: 'bfg_type' must be either '9000', '2704', '11000', "
         "'bouncing' or 'plasma burst'.\n"
      );
   }
}

void CL_LoadConfig(void)
{
   printf("CS_Init: Retrieving server configuration data... ");
   CL_RetrieveServerConfig();
   CS_LoadConfig();

   CS_checkSectionHasOption(cs_json, "main", "server");

   cs_original_settings->requires_spectator_password = false;
   cs_original_settings->requires_player_password = false;

   if(!cs_json["server"]["requires_spectator_password"].empty())
      cs_original_settings->requires_spectator_password = true;

   if(!cs_json["server"]["requires_player_password"].empty())
      cs_original_settings->requires_player_password = true;

   CS_ApplyConfigSettings();
}

void SV_HandleMastersSection(Json::Value &masters)
{
   cs_master_t *master;
   unsigned int i;
   size_t found;
   std::string member_name;
   std::vector<std::string> member_names;
   std::vector<std::string>::iterator it;
   qstring port_str;

   master_servers =
      ecalloc(cs_master_t *, masters.size(), sizeof(cs_master_t));
   member_names = masters.getMemberNames();

   for(i = 0, it = member_names.begin(); it != member_names.end(); i++, it++)
   {
      master = &master_servers[i];
      CS_checkSectionHasOption(masters[*it], "master entry", "username");
      CS_checkSectionHasOption(masters[*it], "master entry", "password");
      CS_checkSectionHasOption(masters[*it], "master entry", "group");
      CS_checkSectionHasOption(masters[*it], "master entry", "name");

      member_name = *it;
      if((found = member_name.find(":")) == std::string::npos ||
         (found + 1) == member_name.length())
      {
         master->address = estrdup(member_name.c_str());
         master->port = 80; // [CG] Default to port 80.
      }
      else
      {
         port_str += member_name.substr(found+1, member_name.length()).c_str();
         if(!port_str.isNum())
         {
            I_Error(
               "CS_LoadConfig: Invalid master entry '%s'.\n",
               port_str.getBuffer()
            );
         }

         master->address = estrdup(member_name.substr(0, found).c_str());
         master->port = port_str.toInt();
      }

      master->username = estrdup(masters[*it]["username"].asCString());
      master->password_hash = CS_GetSHA1Hash(
         masters[*it]["password"].asCString(),
         strlen(masters[*it]["password"].asCString())
      );
      master->group = estrdup(masters[*it]["group"].asCString());
      master->name = estrdup(masters[*it]["name"].asCString());
      master_servers[i].disabled = false;
      master_servers[i].updating = false;
   }

   sv_master_server_count = masters.size();
}

void SV_LoadConfig(void)
{
   char *config_path = NULL;
   int position_of_config, i;
   struct stat sbuf;
   bool requires_spectator_password, requires_player_password,
        requires_moderator_password, requires_administrator_password,
        should_free;

   requires_spectator_password = false;
   requires_player_password = false;
   requires_moderator_password = false;
   requires_administrator_password = false;
   should_free = false;

   position_of_config = M_CheckParm("-server-config");
   if(position_of_config)
   {
      config_path = myargv[position_of_config + 1];
   }
   else
   {
      config_path = ecalloc(
         char *,
         strlen(basepath) + strlen(DEFAULT_CONFIG_FILENAME) + 2,
         sizeof(char)
      );
      sprintf(config_path, "%s/%s", basepath, DEFAULT_CONFIG_FILENAME);
      M_NormalizeSlashes(config_path);
      should_free = true;
   }

   if(stat(config_path, &sbuf))
      I_Error("CS_LoadConfig: Config file %s does not exist.\n", config_path);

   if(!S_ISREG(sbuf.st_mode))
   {
      I_Error(
         "CS_LoadConfig: Config file %s exists but is not a file.\n",
         config_path
      );
   }

   CS_ReadJSON(cs_json, (const char *)config_path);

   if(should_free)
      efree(config_path);

   // [CG] Loads server and options sections.
   CS_LoadConfig();

   // [CG] Load master advertising information.
   if(!cs_json["masters"].empty())
      SV_HandleMastersSection(cs_json["masters"]);

   sv_spectator_password = NULL;
   sv_player_password = NULL;
   sv_moderator_password = NULL;
   sv_administrator_password = NULL;

   // [CG] Load password information (serverside-only).
   if(!cs_json["server"]["spectator_password"].empty() &&
       cs_json["server"]["spectator_password"].asString().length())
   {
      sv_spectator_password =
         estrdup(cs_json["server"]["spectator_password"].asCString());
      requires_spectator_password = true;
   }

   if(!cs_json["server"]["player_password"].empty() &&
       cs_json["server"]["player_password"].asString().length())
   {
      sv_player_password =
         estrdup(cs_json["server"]["player_password"].asCString());
      requires_player_password = true;
   }

   if(!cs_json["server"]["moderator_password"].empty() &&
       cs_json["server"]["moderator_password"].asString().length())
   {
      sv_moderator_password =
         estrdup(cs_json["server"]["moderator_password"].asCString());
      requires_moderator_password = true;
   }
   else
   {
      I_Error(
         "SV_LoadConfig: Option 'moderator_password' blank or not found in "
         "'server' section.\n"
      );
   }

   if(!cs_json["server"]["administrator_password"].empty() &&
       cs_json["server"]["administrator_password"].asString().length())
   {
      sv_administrator_password =
         estrdup(cs_json["server"]["administrator_password"].asCString());
      requires_administrator_password = true;
   }
   else
   {
      I_Error(
         "SV_LoadConfig: Option 'administrator_password' blank or not found "
         "in 'server' section.\n"
      );
   }

   if(!cs_json["server"]["wad_folders"].empty())
   {
      if(!cs_json["server"]["wad_folders"].isArray())
         I_Error("CS_LoadConfig: 'wad_folders' is not an array.\n");

      for(i = 0; i < cs_json["server"]["wad_folders"].size(); i++)
         AddIWADDir((char *)cs_json["server"]["wad_folders"][i].asCString());
   }

   if(!cs_json["server"]["write_config_to"].empty())
      if(!cs_json["server"]["write_config_to"].isString())
         I_Error("CS_LoadConfig: 'write_config_to' is not a string.\n");

   // [CG] Copy the relevant sections to the server's configuration.
   cs_server_config["resources"] = cs_json["resources"];
   cs_server_config["server"] = cs_json["server"];
   cs_server_config["options"] = cs_json["options"];
   cs_server_config["maps"] = cs_json["maps"];

   // [CG] Remove sensitive or irrelevant information from the server's
   //      configuration, because this data is sent to the master.
   if(cs_server_config["server"].isMember("spectator_password"))
      cs_server_config["server"].removeMember("spectator_password");

   if(cs_server_config["server"].isMember("player_password"))
      cs_server_config["server"].removeMember("player_password");

   if(cs_server_config["server"].isMember("moderator_password"))
      cs_server_config["server"].removeMember("moderator_password");

   if(cs_server_config["server"].isMember("administrator_password"))
      cs_server_config["server"].removeMember("administrator_password");

   if(cs_server_config["server"].isMember("write_config_to"))
      cs_server_config["server"].removeMember("write_config_to");

   if(cs_server_config["server"].isMember("wad_folders"))
      cs_server_config["server"].removeMember("wad_folders");

   // [CG] Add password requirement information.
   cs_server_config["server"]["requires_spectator_password"] =
      requires_spectator_password;
   cs_server_config["server"]["requires_player_password"] =
      requires_player_password;

   // [CG] Write the configuration to disk if so directed.
   if(!cs_json["server"]["write_config_to"].empty())
   {
      Json::Value out;

      out["configuration"] = cs_server_config;
      CS_WriteJSON(
         cs_json["server"]["write_config_to"].asCString(), out, true
      );
   }
}

void CS_HandleResourcesSection(Json::Value &resources)
{
   int i, j;
   const char *resource_name;

   // [CG] Check for the IWAD first.
   for(i = 0; i < resources.size(); i++)
   {
      if(cs_json["resources"][i].isObject())
      {
         if(cs_json["resources"][i]["name"].empty())
         {
            printf("CS_LoadConfig: Skipping resource entry with no name.\n");
            continue;
         }

         if(cs_json["resources"][i]["type"].empty())
         {
            printf("CS_LoadConfig: Skipping resource entry with no type.\n");
            continue;
         }

         if(string_option_is(cs_json["resources"][i]["type"], "iwad"))
         {
            if(cs_iwad != NULL)
               I_Error("CS_LoadConfig: Cannot specify multiple IWAD files.\n");

            resource_name = cs_json["resources"][i]["name"].asCString();
            if(!CS_AddIWAD(resource_name) &&
               !cs_json["resources"][i]["alternates"].empty())
            {
               for(j = 0; j < cs_json["resources"][i]["alternates"].size(); j++)
               {
                  resource_name =
                     cs_json["resources"][i]["alternates"][j].asCString();

                  if(CS_AddIWAD(resource_name))
                     break;
               }
            }
         }
      }
   }

   if(cs_iwad == NULL)
      I_Error("CS_LoadConfig: No IWAD specified.\n");

   for(i = 0; i < resources.size(); i++)
   {
      if(cs_json["resources"][i].isString())
      {
         resource_name = cs_json["resources"][i].asCString();
         if(!CS_AddWAD(resource_name))
            I_Error("Could not find PWAD '%s'.\n", resource_name);

         modifiedgame = true;
      }
      else if(cs_json["resources"][i].isObject())
      {
         if(cs_json["resources"][i]["name"].empty())
         {
            printf("CS_LoadConfig: Skipping resource entry with no name.\n");
            continue;
         }

         if(!cs_json["resources"][i]["type"].empty() &&
            !string_option_is(cs_json["resources"][i]["type"], "iwad") &&
            !string_option_is(cs_json["resources"][i]["type"], "wad") &&
            !string_option_is(cs_json["resources"][i]["type"], "dehacked"))
         {
            printf(
               "CS_LoadConfig: Skipping resource entry with invalid type "
               "%s.\n",
               cs_json["resources"][i]["type"].asCString()
            );
            continue;
         }

         if(string_option_is(cs_json["resources"][i]["type"], "dehacked"))
         {
            if(!cs_json["resources"][i]["alternates"].empty())
            {
               I_Error(
                  "CS_LoadConfig: Cannot specify alternate DeHackEd patches.\n"
               );
            }

            if(!CS_AddDeHackEdFile(cs_json["resources"][i]["name"].asCString()))
            {
               I_Error(
                  "Could not find DeHackEd file '%s'.\n",
                  cs_json["resources"][i]["name"].asCString()
               );
            }

            modifiedgame = true;
         }
         else if(string_option_is(cs_json["resources"][i]["type"], "wad"))
         {
            // [CG] If the specified PWAD wasn't found, search for specified
            //      alternates.
            if(!CS_AddWAD(cs_json["resources"][i]["name"].asCString()))
            {
               if(cs_json["resources"][i]["alternates"].empty() ||
                  !cs_json["resources"][i]["alternates"].isArray())
               {
                  I_Error(
                     "Could not find PWAD '%s'.\n", 
                     cs_json["resources"][i]["name"].asCString()
                  );
               }

               for(j = 0; j < cs_json["resources"][i]["alternates"].size(); j++)
               {
                  Json::Value alt = cs_json["resources"][i]["alternates"][j];
                  if(CS_AddWAD(alt.asCString()))
                     break;
               }

               // [CG] No alternates were found, so error out.
               if(j == cs_json["resources"][i]["alternates"].size())
               {
                  I_Error(
                     "Could not find PWAD '%s' or any alternates.\n",
                     cs_json["resources"][i]["name"].asCString()
                  );
               }
            }
         }
      }
      else
         I_Error("CS_LoadConfig: Unknown entry type in 'resources'.\n");
   }
}

void CS_HandleServerSection(Json::Value &server)
{
   unsigned long public_address = 0;
   char *public_address_string;
#ifdef WIN32
   unsigned int i = 0;
   char hostname[256];
   struct hostent *host;
   struct in_addr interface_address;
   bool found_address = false;
#else
   struct ifaddrs *interface_addresses = NULL;
   struct ifaddrs *interface_address = NULL;
   struct sockaddr_in *socket = NULL;
#endif

   if(server["address"].empty())
      server_address->host = ENET_HOST_ANY;
   else if(string_option_is(server["address"], "public"))
      server_address->host = ENET_HOST_ANY;
   else
      enet_address_set_host(server_address, server["address"].asCString());

   if(CS_SERVER)
   {
      if(server_address->host == ENET_HOST_ANY)
      {
#ifdef WIN32
         if(gethostname(hostname, 256) == SOCKET_ERROR)
            I_Error("Error getting hostname: %s.\n", WSAGetLastError());
         if((host = gethostbyname(hostname)) == 0)
            I_Error("Error looking up local hostname, exiting.\n");
         if(host->h_addrtype != AF_INET)
            I_Error("Local host is not an IPv4 host, exiting.\n");

         for(i = 0; (host->h_addr_list[i] != 0) && !found_address; i++)
         {
            public_address = *(u_long *)host->h_addr_list[i];
            found_address = true;
         }

         if(!found_address)
            I_Error("No suitable IP address found, exiting.\n");
#else
         // [CG] Figure out the first public address (if any) to report to the
         //      master.
         if(getifaddrs(&interface_addresses) == -1)
         {
            I_Error(
               "CS_HandleServerSection: Error retrieving interface "
               "addresses: %s, exiting.\n",
               strerror(errno)
            );
         }

         for(interface_address = interface_addresses;
             interface_address != NULL;
             interface_address = interface_address->ifa_next)
         {
            // [CG] Don't list interfaces that aren't actually up.
            if((interface_address->ifa_flags & IFF_UP) == 0)
               continue;

            // [CG] getifaddrs will set ifa_addr of PPP links to NULL, but
            //      later on in the interface list it will have a second entry
            //      for that PPP link with a proper address.  To avoid
            //      segfaults we skip the interfaces without addresses here.
            if(interface_address->ifa_addr == NULL)
               continue;

            if(interface_address->ifa_addr->sa_family == AF_INET)
            {
               socket = (struct sockaddr_in *)interface_address->ifa_addr;
               public_address = socket->sin_addr.s_addr;
            }
         }
         freeifaddrs(interface_addresses);
#endif
         if(public_address == 0)
            I_Error("Could not find an IP address to use, exiting.\n");
      }
      else
         public_address = server_address->host;

      public_address_string = CS_IPToString(public_address);
      server["address"] = public_address_string;
      efree(public_address_string);
   }

   if(!server["game"].empty())
   {
      D_CheckGamePath((char *)server["game"].asCString());
      gamepathset = true;
   }

   server_address->port = DEFAULT_PORT;
   if(!server["port"].empty())
   {
      if(server["port"].asInt() < 1 || server["port"].asInt() > 65535)
         I_Error("CS_LoadConfig: Invalid port.\n");
      server_address->port = server["port"].asInt();
   }

   // [CG] 2 for the server's spectator and a given client.
   cs_original_settings->max_player_clients = (MAXPLAYERS - 2);
   if(!server["max_player_clients"].empty())
   {
      check_int_option_range(server, max_player_clients, 2, (MAXPLAYERS - 2));
      cs_original_settings->max_player_clients =
         server["max_player_clients"].asInt();
   }

   cs_original_settings->max_admin_clients = 2;
   if(!server["max_admin_clients"].empty())
   {
      check_int_option_range(server, max_admin_clients, 2, (MAXPLAYERS >> 1));
      cs_original_settings->max_admin_clients =
         server["max_admin_clients"].asInt();
   }

   if(cs_original_settings->max_player_clients +
      cs_original_settings->max_admin_clients > MAXPLAYERS)
   {
      I_Error(
         "CS_LoadConfig: The total of 'max_player_clients' and "
         "'max_admin_clients' cannot exceed %d.\n",
         MAXPLAYERS
      );
   }

   cs_original_settings->game_type = gt_coop;
   if(!server["game_type"].empty())
   {
      cs_original_settings->ctf = false;
      if(string_option_is(server["game_type"], "ctf") ||
         string_option_is(server["game_type"], "capture-the-flag") ||
         string_option_is(server["game_type"], "capture the flag"))
      {
         cs_original_settings->game_type = gt_ctf;
      }
      else if(string_option_is(server["game_type"], "tdm") ||
              string_option_is(server["game_type"], "teamdm") ||
              string_option_is(server["game_type"], "team dm") ||
              string_option_is(server["game_type"], "team deathmatch"))
      {
         cs_original_settings->game_type = gt_tdm;
      }
      else if(string_option_is(server["game_type"], "coop") &&
              string_option_is(server["game_type"], "cooperative"))
      {
         cs_original_settings->game_type = gt_coop;
      }
      else
      {
         cs_original_settings->game_type = gt_dm;
         if(string_option_is(server["game_type"], "duel")||
            string_option_is(server["game_type"], "1v1") ||
            string_option_is(server["game_type"], "1on1") ||
            string_option_is(server["game_type"], "1 on 1"))
         {
            cs_original_settings->max_players = 2;
         }
      }
   }
   DefaultGameType = GameType = (gametype_t)cs_original_settings->game_type;

   sv_randomize_maps = false;
   if(!server["randomize_maps"].empty() &&
       server["randomize_maps"].asBool())
   {
      sv_randomize_maps = true;
   }

   cs_wad_repository = NULL;
   if(!server["wad_repository"].empty() &&
      server["wad_repository"].asString().length() &&
      CS_CheckURI((char *)server["wad_repository"].asCString()))
   {
      cs_wad_repository = estrdup(server["wad_repository"].asCString());
   }
}

void CS_HandleMapsSection(Json::Value &maps)
{
   cs_resource_t *resource;
   unsigned int i, j, wads_count, overrides_count;
   unsigned int *resource_indices;
   const char *name, *wad_name;

   for(i = 0; i < maps.size(); i++)
   {
      wads_count = overrides_count = 0;

      if(maps[i].isString())
      {
         name = maps[i].asCString();
         printf("CS_LoadConfig: Adding map '%s'.\n", name);
      }
      else if(maps[i].isObject())
      {
         if(maps[i]["name"].empty())
            I_Error("CS_LoadConfig: Map %d is missing option 'name'.\n", i);

         name = maps[i]["name"].asCString();

         if(!maps[i]["wads"].empty())
            wads_count = maps[i]["wads"].size();

         if(!maps[i]["overrides"].empty())
         {
            CS_ValidateOptions(maps[i]["overrides"]);
            overrides_count = maps[i]["overrides"].size();
         }

         if(wads_count == 0 && overrides_count == 0)
         {
            printf(
               "CS_LoadConfig: Adding map '%s' using no PWADs and no "
               "overrides.\n",
               name
            );
            continue;
         }

         printf("CS_LoadConfig: Adding map '%s'", name);

         if(wads_count < 1)
            printf(" using no PWADs");
         else if(wads_count == 1)
            printf(" using %s", maps[i]["wads"][0].asCString());

         if(wads_count <= 1)
            printf(" and");
         else
            printf(" with");

         if(overrides_count == 0)
            printf(" no overrides");
         else if(overrides_count == 1)
            printf(" %u override", overrides_count);
         else
            printf(" %u overrides", overrides_count);

         if(wads_count > 1)
         {
            printf(", WADs:");
            for(j = 0; j < wads_count; j++)
               printf("\n\t%s", maps[i]["wads"][j].asCString());
         }
         printf(".\n");
      }
      else
      {
         fprintf(stderr, "Skipping invalid map entry, invalid type.\n");
         continue;
      }

      if(strlen(name) > 8)
         I_Error("Map name '%s' exceeds max length of 8 characters.\n", name);

      resource_indices =
         ecalloc(unsigned int *, wads_count, sizeof(unsigned int));

      for(j = 0; j < wads_count; j++)
      {
         wad_name = maps[i]["wads"][j].asCString();
         resource = CS_GetResource(wad_name);

         if(resource == NULL)
            I_Error("PWAD '%s' not found in resources.\n", wad_name);

         resource_indices[j] = resource - cs_resources;
      }
      CS_AddMap(name, wads_count, resource_indices);
   }

   cs_current_map_index = cs_current_map_number = 0;
}

void CS_LoadConfig(void)
{
   server_address =
      erealloc(ENetAddress *, server_address, sizeof(ENetAddress));
   memset(server_address, 0, sizeof(ENetAddress));

   cs_settings = erealloc(
      clientserver_settings_t *, cs_settings, sizeof(clientserver_settings_t)
   );
   memset(cs_settings, 0, sizeof(clientserver_settings_t));

   cs_original_settings = erealloc(
      clientserver_settings_t *,
      cs_original_settings,
      sizeof(clientserver_settings_t)
   );
   memset(cs_original_settings, 0, sizeof(clientserver_settings_t));

   if(cs_json["resources"].empty())
      I_Error("Required config section 'resources' missing.\n");

   if(cs_json["server"].empty())
      I_Error("Required config section 'server' missing.\n");

   if(cs_json["options"].empty())
      I_Error("Required config section 'options' missing.\n");

   if(cs_json["maps"].empty())
      I_Error("Required config section 'maps' missing.\n");

   CS_HandleServerSection(cs_json["server"]);
   CS_HandleOptionsSection(cs_json["options"]);
}

void CS_LoadWADs(void)
{
   printf("CS_LoadWADs: Loading WADs.\n");
   
   CS_HandleResourcesSection(cs_json["resources"]);
   CS_HandleMapsSection(cs_json["maps"]);
}

void CS_ReloadDefaults(void)
{
   // [CG] Some stuff from G_ReloadDefaults, because we won't run that in c/s.
   demoplayback = false;
   singledemo = false;
   compatibility = false;
   demo_version = version;
   demo_subversion = subversion;
   demo_insurance = default_demo_insurance == 1;
   G_ScrambleRand();

   memcpy(cs_settings, cs_original_settings, sizeof(clientserver_settings_t));
   memcpy(comp, default_comp, sizeof(comp));
}

void CS_ApplyConfigSettings(void)
{
   // [CG] Apply the settings.
   GameType = DefaultGameType = (gametype_t)cs_settings->game_type;
   startskill = gameskill     = (skill_t)cs_settings->skill;
   dogs = default_dogs        = cs_settings->dogs;
   distfriend                 = cs_settings->friend_distance;
   respawn_protection_time    = cs_settings->respawn_protection_time;
   death_time_limit           = cs_settings->death_time_limit;
   death_time_expired_action  = cs_settings->death_time_expired_action;
   friendly_damage_percentage = cs_settings->friendly_damage_percentage;
   bfgtype                    = (bfg_t)cs_settings->bfg_type;
   levelTimeLimit             = cs_settings->time_limit;
   levelFragLimit             = cs_settings->frag_limit;
   levelScoreLimit            = cs_settings->score_limit;
   r_blockmap                 = cs_settings->build_blockmap;
   dmflags                    = cs_settings->dmflags;
   dmflags2                   = cs_settings->dmflags2;

   if((dmflags2 & dmf_allow_movebob_change) == 0)
      player_bobbing = default_player_bobbing;

   if((dmflags2 & dmf_enable_variable_friction) == 0)
      variable_friction = false;
   else
      variable_friction = true;

   if((dmflags2 & dmf_enable_boom_push_effects) == 0)
      allow_pushers = false;
   else
      allow_pushers = true;

   if((dmflags2 & dmf_allow_freelook))
      allowmlook = true;
   else
      allowmlook = false;

   if((dmflags & dmf_respawn_monsters))
      respawnparm = clrespawnparm = true;
   else
      respawnparm = clrespawnparm = false;

   if((dmflags & dmf_fast_monsters))
      fastparm = clfastparm = true;
   else
      fastparm = clfastparm = false;

   if((dmflags & dmf_spawn_monsters))
      nomonsters = clnomonsters = false;
   else
      nomonsters = clnomonsters = true;

   if(startskill == sk_nightmare)
   {
      respawnparm = clrespawnparm = true;
      fastparm = clfastparm = true;
   }

   if(cs_settings->compatflags & cmf_comp_god)
      default_comp[comp_god] = true;
   else
      default_comp[comp_god] = false;

   if(cs_settings->compatflags & cmf_comp_infcheat)
      default_comp[comp_infcheat] = true;
   else
      default_comp[comp_infcheat] = false;

   if(cs_settings->compatflags & cmf_comp_skymap)
      default_comp[comp_skymap] = true;
   else
      default_comp[comp_skymap] = false;

   if(cs_settings->compatflags & cmf_comp_zombie)
      default_comp[comp_zombie] = true;
   else
      default_comp[comp_zombie] = false;

   if(cs_settings->compatflags & cmf_comp_vile)
      default_comp[comp_vile] = true;
   else
      default_comp[comp_vile] = false;

   if(cs_settings->compatflags & cmf_comp_pain)
      default_comp[comp_pain] = true;
   else
      default_comp[comp_pain] = false;

   if(cs_settings->compatflags & cmf_comp_skull)
      default_comp[comp_skull] = true;
   else
      default_comp[comp_skull] = false;

   if(cs_settings->compatflags & cmf_comp_soul)
      default_comp[comp_soul] = true;
   else
      default_comp[comp_soul] = false;

   if(cs_settings->compatflags & cmf_comp_staylift)
      default_comp[comp_staylift] = true;
   else
      default_comp[comp_staylift] = false;

   if(cs_settings->compatflags & cmf_comp_doorstuck)
      default_comp[comp_doorstuck] = true;
   else
      default_comp[comp_doorstuck] = false;

   if(cs_settings->compatflags & cmf_comp_pursuit)
      default_comp[comp_pursuit] = true;
   else
      default_comp[comp_pursuit] = false;

   if(cs_settings->compatflags & cmf_comp_dropoff)
      default_comp[comp_dropoff] = true;
   else
      default_comp[comp_dropoff] = false;

   if(cs_settings->compatflags & cmf_comp_falloff)
      default_comp[comp_falloff] = true;
   else
      default_comp[comp_falloff] = false;

   if(cs_settings->compatflags & cmf_comp_telefrag)
      default_comp[comp_telefrag] = true;
   else
      default_comp[comp_telefrag] = false;

   if(cs_settings->compatflags & cmf_comp_respawnfix)
      default_comp[comp_respawnfix] = true;
   else
      default_comp[comp_respawnfix] = false;

   if(cs_settings->compatflags & cmf_comp_terrain)
      default_comp[comp_terrain] = true;
   else
      default_comp[comp_terrain] = false;

   if(cs_settings->compatflags & cmf_comp_theights)
      default_comp[comp_theights] = true;
   else
      default_comp[comp_theights] = false;

   if(cs_settings->compatflags & cmf_comp_planeshoot)
      default_comp[comp_planeshoot] = true;
   else
      default_comp[comp_planeshoot] = false;

   if(cs_settings->compatflags & cmf_comp_blazing)
      default_comp[comp_blazing] = true;
   else
      default_comp[comp_blazing] = false;

   if(cs_settings->compatflags & cmf_comp_doorlight)
      default_comp[comp_doorlight] = true;
   else
      default_comp[comp_doorlight] = false;

   if(cs_settings->compatflags & cmf_comp_stairs)
      default_comp[comp_stairs] = true;
   else
      default_comp[comp_stairs] = false;

   if(cs_settings->compatflags & cmf_comp_floors)
      default_comp[comp_floors] = true;
   else
      default_comp[comp_floors] = false;

   if(cs_settings->compatflags & cmf_comp_model)
      default_comp[comp_model] = true;
   else
      default_comp[comp_model] = false;

   if(cs_settings->compatflags & cmf_comp_zerotags)
      default_comp[comp_zerotags] = true;
   else
      default_comp[comp_zerotags] = false;

   if(cs_settings->compatflags & cmf_comp_special)
      default_comp[comp_special] = true;
   else
      default_comp[comp_special] = false;

   if(cs_settings->compatflags & cmf_comp_fallingdmg)
      default_comp[comp_fallingdmg] = true;
   else
      default_comp[comp_fallingdmg] = false;

   if(cs_settings->compatflags & cmf_comp_overunder)
      default_comp[comp_overunder] = true;
   else
      default_comp[comp_overunder] = false;

   if(cs_settings->compatflags & cmf_comp_ninja)
      default_comp[comp_ninja] = true;
   else
      default_comp[comp_ninja] = false;

   if(cs_settings->compatflags2 & cmf_monsters_remember)
      monsters_remember = true;
   else
      monsters_remember = false;

   if(cs_settings->compatflags2 & cmf_monster_infighting)
      monster_infighting = true;
   else
      monster_infighting = false;

   if(cs_settings->compatflags2 & cmf_monster_backing)
      monster_backing = true;
   else
      monster_backing = false;

   if(cs_settings->compatflags2 & cmf_monster_avoid_hazards)
      monster_avoid_hazards = true;
   else
      monster_avoid_hazards = false;

   if(cs_settings->compatflags2 & cmf_monster_friction)
      monster_friction = true;
   else
      monster_friction = false;

   if(cs_settings->compatflags2 & cmf_monkeys)
      monkeys = true;
   else
      monkeys = false;

   if(cs_settings->compatflags2 & cmf_help_friends)
      help_friends = true;
   else
      help_friends = false;

   if(cs_settings->compatflags2 & cmf_dog_jumping)
      dog_jumping = true;
   else
      dog_jumping = false;
}

void CS_HandleOptionsSection(Json::Value &options)
{
   // [CG] Validate the integer/string options here.
   CS_ValidateOptions(options);

   // [CG] Non-boolean options
   set_int(options, max_players, 16);
   set_int(options, max_players_per_team, 8);
   set_int(options, dogs, 0);
   set_int(options, skill, 5); // [CG] Nightmare!!!!
   set_int(options, frag_limit, 0);
   set_int(options, time_limit, 0);
   set_int(options, score_limit, 0);
   set_int(options, death_time_limit, 0);
   set_int(options, respawn_protection_time, 0);
   set_int(options, friend_distance, 128);

   // [CG] Float options
   cs_original_settings->radial_attack_damage =
      options.get("radial_attack_damage", 1.0).asFloat();
   cs_original_settings->radial_attack_self_damage =
      options.get("radial_attack_self_damage", 1.0).asFloat();
   cs_original_settings->radial_attack_lift =
      options.get("radial_attack_lift", 0.5).asFloat();
   cs_original_settings->radial_attack_self_lift =
      options.get("radial_attack_self_lift", 0.8).asFloat();

   // [CG] Boolean options
   set_bool(options, build_blockmap, false);

   // [CG] Internal skill is 0-4, configured skill is 1-5, translate here.
   cs_original_settings->skill--;

   // [CG] General features
   set_flags(options, respawn_items, dmflags, false);
   set_flags(options, leave_weapons, dmflags, true);
   set_flags(options, respawn_barrels, dmflags, false);
   set_flags(options, players_drop_everything, dmflags, false);
   set_flags(options, respawn_super_items, dmflags, false);
   set_flags(options, instagib, dmflags, false);
   set_flags(options, spawn_armor, dmflags, true);
   set_flags(options, spawn_super_items, dmflags, true);
   set_flags(options, respawn_health, dmflags, false);
   set_flags(options, respawn_armor, dmflags, false);
   set_flags(options, spawn_monsters, dmflags, false);
   set_flags(options, fast_monsters, dmflags, false);
   set_flags(options, strong_monsters, dmflags, false);
   set_flags(options, powerful_monsters, dmflags, false);
   set_flags(options, respawn_monsters, dmflags, false);
   set_flags(options, allow_exit, dmflags, true);
   set_flags(options, kill_on_exit, dmflags, true);
   set_flags(options, exit_to_same_level, dmflags, false);
   set_flags(options, spawn_in_same_spot, dmflags, false);
   set_flags(options, leave_keys, dmflags, true);
   set_flags(options, players_drop_items, dmflags, false);
   set_flags(options, players_drop_weapons, dmflags, false);
   set_flags(options, keep_items_on_exit, dmflags, false);
   set_flags(options, keep_keys_on_exit, dmflags, false);

   // [CG] Newschool features
   set_flags(options, allow_jump, dmflags2, false);
   set_flags(options, allow_freelook, dmflags2, true);
   set_flags(options, allow_crosshair, dmflags2, true);
   set_flags(options, allow_target_names, dmflags2, true);
   set_flags(options, allow_movebob_change, dmflags2, true);
   set_flags(options, use_oldschool_sound_cutoff, dmflags2, true);
   set_flags(options, allow_two_way_wallrun, dmflags2, true);
   set_flags(options, allow_no_weapon_switch_on_pickup, dmflags2, true);
   set_flags(options, allow_preferred_weapon_order, dmflags2, true);
   set_flags(options, silent_weapon_pickup, dmflags2, true);
   set_flags(options, allow_weapon_speed_change, dmflags2, false);
   set_flags(options, teleport_missiles, dmflags2, false);
   set_flags(options, allow_chasecam, dmflags2, false);
   set_flags(options, follow_fragger_on_death, dmflags2, false);
   set_flags(options, spawn_farthest, dmflags2, false);
   set_flags(options, infinite_ammo, dmflags2, false);
   set_flags(options, enable_variable_friction, dmflags2, false);
   set_flags(options, enable_boom_push_effects, dmflags2, false);
   set_flags(options, enable_nukage, dmflags2, true);
   set_flags(options, allow_damage_screen_change, dmflags2, false);

   // [CG] Compatibility options
   set_comp(options, imperfect_god_mode, god, false);
   set_comp(options, time_limit_powerups, infcheat, false);
   set_comp(options, normal_sky_when_invulnerable, skymap, false);
   set_comp(options, zombie_players_can_exit, zombie, false);
   set_comp(options, arch_viles_can_create_ghosts, vile, false);
   set_comp(options, limit_lost_souls, pain, false);
   set_comp(options, lost_souls_get_stuck_in_walls, skull, false);
   set_comp(options, lost_souls_never_bounce_on_floors, soul, false);
   set_comp(options, monsters_randomly_walk_off_lifts, staylift, false);
   set_comp(options, monsters_get_stuck_on_door_tracks, doorstuck, false);
   set_comp(options, monsters_give_up_pursuit, pursuit, false);
   set_comp(options, actors_get_stuck_over_dropoffs, dropoff, false);
   set_comp(options, actors_never_fall_off_ledges, falloff, false);
   set_comp(options, monsters_can_telefrag_on_map30, telefrag, false);
   set_comp(options, monsters_can_respawn_outside_map, respawnfix, false);
   set_comp(options, disable_terrain_types, terrain, false);
   set_comp(options, disable_falling_damage, fallingdmg, true);
   set_comp(options, actors_have_infinite_height, overunder, false);
   set_comp(options, doom_actor_heights_are_inaccurate, theights, false);
   set_comp(options, bullets_never_hit_floors_and_ceilings, planeshoot, false);
   set_comp(options, respawns_are_sometimes_silent_in_dm, ninja, false);
   set_comp(options, short_vertical_mouselook_range, mouselook, false);
   set_comp(options, radius_attacks_only_thrust_in_2d, 2dradatk, false);
   set_comp(options, turbo_doors_make_two_closing_sounds, blazing, false);
   set_comp(options, disable_tagged_door_light_fading, doorlight, false);
   set_comp(options, use_doom_stairbuilding_method, stairs, false);
   set_comp(options, use_doom_floor_motion_behavior, floors, false);
   set_comp(options, use_doom_linedef_trigger_model, model, false);
   set_comp(options, line_effects_work_on_sector_tag_zero, zerotags, false);
   set_comp(options, one_time_line_effects_can_break, special, false);

   set_comp2(options, monsters_remember_target, monsters_remember, false);
   set_comp2(options, monster_infighting, monster_infighting, false);
   set_comp2(options, monsters_back_out, monster_backing, false);
   set_comp2(options, monsters_avoid_hazards, monster_avoid_hazards, false);
   set_comp2(options, monsters_affected_by_friction, monster_friction, false);
   set_comp2(options, monsters_climb_tall_stairs, monkeys, false);
   set_comp2(options, rescue_dying_friends, help_friends, false);
   set_comp2(options, dogs_can_jump_down, dog_jumping, false);
}

void CS_LoadMapOverrides(unsigned int index)
{
   Json::Value overrides;
   // [CG] To ensure that map overrides are only valid for the given map,
   //      restore the original values first.
   CS_HandleOptionsSection(cs_json["options"]);

   CS_ReloadDefaults();

   if(index > cs_json["maps"].size())
      I_Error("CS_LoadMapOverrides: Invalid map index %d, exiting.\n", index);

   // [CG] Non-object maplist entries can't have any overrides, so if this map
   //      is not an object entry then there's nothing to do.  This is also the
   //      case if no overrides are specified.
   if(!cs_json["maps"][index].isObject() ||
       cs_json["maps"][index]["overrides"].empty())
   {
      CS_ApplyConfigSettings();
      return;
   }

   overrides = cs_json["maps"][index]["overrides"];

   cs_settings->build_blockmap =
      overrides.get("build_blockmap", false).asBool();

   // [CG] DMFLAGS
   override_flags(overrides, respawn_items, dmflags)
   override_flags(overrides, leave_weapons, dmflags)
   override_flags(overrides, respawn_barrels, dmflags)
   override_flags(overrides, players_drop_everything, dmflags)
   override_flags(overrides, respawn_super_items, dmflags)
   override_flags(overrides, instagib, dmflags)
   override_flags(overrides, spawn_armor, dmflags)
   override_flags(overrides, spawn_super_items, dmflags)
   override_flags(overrides, respawn_health, dmflags)
   override_flags(overrides, respawn_armor, dmflags)
   override_flags(overrides, spawn_monsters, dmflags)
   override_flags(overrides, fast_monsters, dmflags)
   override_flags(overrides, strong_monsters, dmflags)
   override_flags(overrides, powerful_monsters, dmflags)
   override_flags(overrides, respawn_monsters, dmflags)
   override_flags(overrides, allow_exit, dmflags)
   override_flags(overrides, kill_on_exit, dmflags)
   override_flags(overrides, exit_to_same_level, dmflags)
   override_flags(overrides, spawn_in_same_spot, dmflags)
   override_flags(overrides, leave_keys, dmflags)
   override_flags(overrides, players_drop_items, dmflags)
   override_flags(overrides, players_drop_weapons, dmflags)
   override_flags(overrides, keep_items_on_exit, dmflags)
   override_flags(overrides, keep_keys_on_exit, dmflags)

   // [CG] DMFLAGS2
   override_flags(overrides, allow_jump, dmflags2)
   override_flags(overrides, allow_freelook, dmflags2)
   override_flags(overrides, allow_crosshair, dmflags2)
   override_flags(overrides, allow_target_names, dmflags2)
   override_flags(overrides, allow_movebob_change, dmflags2)
   override_flags(overrides, use_oldschool_sound_cutoff, dmflags2)
   override_flags(overrides, allow_two_way_wallrun, dmflags2)
   override_flags(overrides, allow_no_weapon_switch_on_pickup, dmflags2)
   override_flags(overrides, allow_preferred_weapon_order, dmflags2)
   override_flags(overrides, silent_weapon_pickup, dmflags2)
   override_flags(overrides, allow_weapon_speed_change, dmflags2)
   override_flags(overrides, teleport_missiles, dmflags2)
   override_flags(overrides, allow_chasecam, dmflags2)
   override_flags(overrides, follow_fragger_on_death, dmflags2);
   override_flags(overrides, spawn_farthest, dmflags2)
   override_flags(overrides, infinite_ammo, dmflags2)
   override_flags(overrides, enable_variable_friction, dmflags2)
   override_flags(overrides, enable_boom_push_effects, dmflags2)
   override_flags(overrides, enable_nukage, dmflags2)
   override_flags(overrides, allow_damage_screen_change, dmflags2)

   // [CG] Compatibility options
   override_comp(overrides, imperfect_god_mode, god);
   override_comp(overrides, time_limit_powerups, infcheat);
   override_comp(overrides, normal_sky_when_invulnerable, skymap);
   override_comp(overrides, zombie_players_can_exit, zombie);
   override_comp(overrides, arch_viles_can_create_ghosts, vile);
   override_comp(overrides, limit_lost_souls, pain);
   override_comp(overrides, lost_souls_get_stuck_in_walls, skull);
   override_comp(overrides, lost_souls_never_bounce_on_floors, soul);
   override_comp(overrides, monsters_randomly_walk_off_lifts, staylift);
   override_comp(overrides, monsters_get_stuck_on_door_tracks, doorstuck);
   override_comp(overrides, monsters_give_up_pursuit, pursuit);
   override_comp(overrides, actors_get_stuck_over_dropoffs, dropoff);
   override_comp(overrides, actors_never_fall_off_ledges, falloff);
   override_comp(overrides, monsters_can_telefrag_on_map30, telefrag);
   override_comp(overrides, monsters_can_respawn_outside_map, respawnfix);
   override_comp(overrides, disable_terrain_types, terrain);
   override_comp(overrides, disable_falling_damage, fallingdmg);
   override_comp(overrides, actors_have_infinite_height, overunder);
   override_comp(overrides, doom_actor_heights_are_inaccurate, theights);
   override_comp(overrides, bullets_never_hit_floors_and_ceilings, planeshoot);
   override_comp(overrides, respawns_are_sometimes_silent_in_dm, ninja);
   override_comp(overrides, short_vertical_mouselook_range, mouselook);
   override_comp(overrides, radius_attacks_only_thrust_in_2d, 2dradatk);
   override_comp(overrides, turbo_doors_make_two_closing_sounds, blazing);
   override_comp(overrides, disable_tagged_door_light_fading, doorlight);
   override_comp(overrides, use_doom_stairbuilding_method, stairs);
   override_comp(overrides, use_doom_floor_motion_behavior, floors);
   override_comp(overrides, use_doom_linedef_trigger_model, model);
   override_comp(overrides, line_effects_work_on_sector_tag_zero, zerotags);
   override_comp(overrides, one_time_line_effects_can_break, special);

   // [CG] Compatibility options that aren't in the compatibility vector.
   override_comp2(overrides, monsters_remember_target, monsters_remember);
   override_comp2(overrides, monster_infighting, monster_infighting);
   override_comp2(overrides, monsters_back_out, monster_backing);
   override_comp2(overrides, monsters_avoid_hazards, monster_avoid_hazards);
   override_comp2(overrides, monsters_affected_by_friction, monster_friction);
   override_comp2(overrides, monsters_climb_tall_stairs, monkeys);
   override_comp2(overrides, rescue_dying_friends, help_friends);
   override_comp2(overrides, dogs_can_jump_down, dog_jumping);

   // [CG] Non-boolean options
   override_int(overrides, max_players);
   override_int(overrides, dogs);
   override_int(overrides, friend_distance);
   override_int(overrides, death_time_limit);
   override_int(overrides, respawn_protection_time);
   override_int(overrides, time_limit);
   override_int(overrides, frag_limit);
   override_int(overrides, score_limit);
   override_int(overrides, skill);

   override_float(overrides, radial_attack_damage);
   override_float(overrides, radial_attack_self_damage);
   override_float(overrides, radial_attack_lift);
   override_float(overrides, radial_attack_self_lift);

   CS_ApplyConfigSettings();
}

