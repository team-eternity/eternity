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
//   Client/Server configuration loading and parsing.
//
//----------------------------------------------------------------------------

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
#include "m_qstr.h"
#include "p_inter.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

#include "cs_main.h"
#include "cs_config.h"
#include "cs_master.h"
#include "cs_wad.h"
#include "cl_main.h"
#include "sv_main.h"

#include <json/json.h>
#include <curl/curl.h>

// [CG] Zarg huge defines.

#define __set_int(options, option_name, default_value)\
   option = json_object_object_get(options, #option_name);\
   if(option == NULL)\
      cs_original_settings->option_name = default_value;\
   else\
      cs_original_settings->option_name = json_object_get_int(option);

#define __set_bool(options, option_name, default_value)\
   option = json_object_object_get(options, #option_name);\
   if(option == NULL)\
      cs_original_settings->option_name = default_value;\
   else if(json_object_get_boolean(option))\
      cs_original_settings->option_name = true;\
   else\
      cs_original_settings->option_name = false;

#define __set_flags(options, option_name, flags, default_value)\
   option = json_object_object_get(options, #option_name);\
   if(option == NULL)\
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
   else if(json_object_get_boolean(option))\
   {\
      flags |= dmf_##option_name;\
      cs_original_settings->flags |= dmf_##option_name;\
   }\
   else\
   {\
      flags &= ~dmf_##option_name;\
      cs_original_settings->flags &= ~dmf_##option_name;\
   }

#define __set_compat(options, config_name, option_name, default_value)\
   option = json_object_object_get(options, #config_name);\
   if(option == NULL)\
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
   else if(json_object_get_boolean(option))\
   {\
      default_comp[comp_##option_name] = true;\
      cs_original_settings->compatflags |= cmf_comp_##option_name;\
   }\
   else\
   {\
      default_comp[comp_##option_name] = false;\
      cs_original_settings->compatflags &= ~cmf_comp_##option_name;\
   }

#define __set_compat2(options, config_name, opt, default_value)\
   option = json_object_object_get(options, #config_name);\
   if(option == NULL)\
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
   else if(json_object_get_boolean(option))\
   {\
      opt = true;\
      cs_original_settings->compatflags2 |= cmf_##opt;\
   }\
   else\
   {\
      opt = false;\
      cs_original_settings->compatflags2 &= ~cmf_##opt;\
   }\

#define __override_int(options, option_name)\
   option = json_object_object_get(options, #option_name);\
   if(option != NULL)\
      cs_settings->option_name = json_object_get_int(option);

#define __override_flags(options, option_name, flags)\
   option = json_object_object_get(options, #option_name);\
   if(option != NULL)\
   {\
      if(json_object_get_boolean(option))\
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

#define __override_compat(options, config_name, option_name)\
   option = json_object_object_get(options, #config_name);\
   if(option != NULL)\
   {\
      if(json_object_get_boolean(option))\
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

#define __override_compat2(options, config_name, opt)\
   option = json_object_object_get(options, #config_name);\
   if(option != NULL)\
   {\
      if(json_object_get_boolean(option))\
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

extern ENetAddress *server_address;
extern json_object *cs_json;
extern int levelFragLimit;
extern int levelTimeLimit;
extern int levelScoreLimit;
extern boolean netdemo;

extern void AddIWADDir(char *dir);

json_object *cs_json = NULL;
clientserver_settings_t *cs_original_settings = NULL;
clientserver_settings_t *cs_settings = NULL;

void CS_ValidateOptions(json_object *options)
{
   json_object *option;
   int int_value;
   const char *str_value;

   option = json_object_object_get(options, "max_players");
   if(option != NULL)
   {
      int_value = json_object_get_int(option);
      if(int_value < 1)
      {
         I_Error("CS_LoadConfig: 'max_players' must be greater than zero.\n");
      }
      if(int_value > 16)
      {
         I_Error("CS_LoadConfig: 'max_players' cannot exceed %d.\n", 16);
      }
      if(int_value > MAX_CLIENTS)
      {
         I_Error(
            "CS_LoadConfig: 'max_players' cannot exceed the maximum number of "
            "clients ('max_player_clients' + 'max_admin_clients')\n"
         );
      }
   }

   option = json_object_object_get(options, "max_players_per_team");
   if(option != NULL)
   {
      int_value = json_object_get_int(option);
      if(int_value < 1)
      {
         I_Error(
            "CS_LoadConfig: 'max_players_per_team' must be greater than "
            "zero.\n"
         );
      }
      if(int_value > 16)
      {
         I_Error(
            "CS_LoadConfig: 'max_players_per_team cannot exceed %d.\n", 8
         );
      }
   }

   option = json_object_object_get(options, "dogs");
   if(option != NULL)
   {
      int_value = json_object_get_int(option);
      if(int_value < 0)
      {
         I_Error("CS_LoadConfig: 'dogs' must be at least zero.\n");
      }
      if(int_value > 3)
      {
         I_Error("CS_LoadConfig: 'dogs' cannot exceed 3.\n");
      }
   }

   option = json_object_object_get(options, "skill");
   if(option != NULL)
   {
      int_value = json_object_get_int(option);
      if(int_value < 1)
      {
         I_Error("CS_LoadConfig: 'skill' must be greater than zero.\n");
      }
      if(int_value > 5)
      {
         I_Error("CS_LoadConfig: 'skill' cannot exceed 5.\n");
      }
   }

   option = json_object_object_get(options, "frag_limit");
   if(option != NULL)
   {
      int_value = json_object_get_int(option);
      if(int_value < 0)
      {
         I_Error("CS_LoadConfig: 'frag_limit' must be at least zero.\n");
      }
      if(int_value > 100000)
      {
         I_Error("CS_LoadConfig: 'frag_limit' is unreasonable.\n");
      }
   }

   option = json_object_object_get(options, "time_limit");
   if(option != NULL)
   {
      int_value = json_object_get_int(option);
      if(int_value < 0)
      {
         I_Error("CS_LoadConfig: 'time_limit' must be at least zero.\n");
      }
      if(int_value > 100000)
      {
         I_Error("CS_LoadConfig: 'time_limit' is unreasonable.\n");
      }
   }

   option = json_object_object_get(options, "score_limit");
   if(option != NULL)
   {
      int_value = json_object_get_int(option);
      if(int_value < 0)
      {
         I_Error("CS_LoadConfig: 'score_limit' must be at least zero.\n");
      }
      if(int_value > 100000)
      {
         I_Error("CS_LoadConfig: 'score_limit' is unreasonable.\n");
      }
   }

   option = json_object_object_get(options, "death_time_limit");
   if(option != NULL)
   {
      int_value = json_object_get_int(option);
      if(int_value < 0)
      {
         I_Error(
            "CS_LoadConfig: 'death_time_limit' must be at least "
            "zero.\n"
         );
      }
      if(int_value > 100000)
      {
         I_Error(
            "CS_LoadConfig: 'death_time_limit' is unreasonable.\n"
         );
      }
   }

   option = json_object_object_get(options, "death_time_expired_action");
   if(option != NULL)
   {
      str_value = json_object_get_string(option);
      if(option == NULL)
      {
         cs_original_settings->death_time_expired_action = DEATH_LIMIT_SPECTATE;
      }
      else if(strncmp(str_value, "spectate", 8) == 0)
      {
         cs_original_settings->death_time_expired_action = DEATH_LIMIT_SPECTATE;
      }
      else if(strncmp(str_value, "respawn", 7) == 0)
      {
         cs_original_settings->death_time_expired_action = DEATH_LIMIT_RESPAWN;
      }
      else
      {
         I_Error(
            "CS_LoadConfig: 'death_time_expired_action' must be either "
            "'spectate' or 'respawn'.\n"
         );
      }

   }

   option = json_object_object_get(options, "respawn_protection_time");
   if(option != NULL)
   {
      int_value = json_object_get_int(option);
      if(int_value < 0)
      {
         I_Error(
            "CS_LoadConfig: 'respawn_protection_time' must be at least zero.\n"
         );
      }
      if(int_value > 100000)
      {
         I_Error(
            "CS_LoadConfig: 'respawn_protection_time' is unreasonable.\n"
         );
      }
   }

   option = json_object_object_get(options, "friend_distance");
   if(option != NULL)
   {
      int_value = json_object_get_int(option);
      if(int_value < 1)
      {
         I_Error(
            "CS_LoadConfig: 'friend_distance' must be greater than zero.\n"
         );
      }
      if(int_value > 1024)
      {
         I_Error(
            "CS_LoadConfig: 'friend_distance' cannot exceed 1024.\n"
         );
      }
   }

   option = json_object_object_get(options, "bfg_type");
   if(option == NULL)
   {
      cs_original_settings->bfg_type = bfg_normal;
   }
   else
   {
      str_value = json_object_get_string(option);
      if(strncmp(str_value, "9000", 4) == 0)
      {
         cs_original_settings->bfg_type = bfg_normal;
      }
      else if(strncmp(str_value, "2704", 4) == 0)
      {
         cs_original_settings->bfg_type = bfg_classic;
      }
      else if(strncmp(str_value, "11000", 5) == 0)
      {
         cs_original_settings->bfg_type = bfg_11k;
      }
      else if(strncmp(str_value, "bouncing", 8) == 0)
      {
         cs_original_settings->bfg_type = bfg_bouncing;
      }
      else if(strncmp(str_value, "plasma burst", 12) == 0)
      {
         cs_original_settings->bfg_type = bfg_burst;
      }
      else
      {
         I_Error(
            "CS_LoadConfig: 'bfg_type' must be either '9000', '2704', "
            "'11000', 'bouncing' or 'plasma burst'.\n"
         );
      }
   }

   option = json_object_object_get(options, "friendly_damage_percentage");
   if(option != NULL)
   {
      int_value = json_object_get_int(option);
      if(int_value < 0)
      {
         I_Error(
            "CS_LoadConfig: 'friendly_damage_percentage' must be at least "
            "zero.\n"
         );
      }
      if(int_value > 100)
      {
         I_Error(
            "CS_LoadConfig: 'friendly_damage_percentage' cannot exceed 100.\n"
         );
      }
      cs_original_settings->friendly_damage_percentage = int_value;
   }
}

void CL_LoadConfig(void)
{
   json_object *section;

   printf("CS_Init: Retrieving server configuration data... ");
   CL_RetrieveServerConfig();
   CS_LoadConfig();

   section = json_object_object_get(cs_json, "server");
   if(section == NULL)
   {
      I_Error(
         "CL_LoadConfig: No 'server' section present in server "
         "configuration data.\n"
      );
   }

   cs_original_settings->requires_spectator_password = false;
   cs_original_settings->requires_player_password = false;
   if(json_object_get_boolean(json_object_object_get(
      section, "requires_spectator_password")))
   {
      cs_original_settings->requires_spectator_password = true;
   }
   if(json_object_get_boolean(json_object_object_get(
      section, "requires_player_password")))
   {
      cs_original_settings->requires_player_password = true;
   }
   CS_ApplyConfigSettings();
}

void SV_HandleMastersSection(json_object *masters)
{
   cs_master_t *master;
   json_object *master_section, *master_option;
   unsigned int i;
   unsigned int master_server_count;

   master_server_count = json_object_array_length(masters);
   master_servers = calloc(master_server_count, sizeof(cs_master_t));

   for(i = 0; i < master_server_count; i++)
   {
      master = &master_servers[i];
      master_section = json_object_array_get_idx(masters, i);

      master_option = json_object_object_get(master_section, "address");
      if(master_option == NULL)
      {
         I_Error(
            "CS_LoadConfig: master entry missing required option "
            "'address'.\n"
         );
      }
      master->address = strdup(json_object_get_string(master_option));

      master_option = json_object_object_get(master_section, "username");
      if(master_option == NULL)
      {
         I_Error(
            "CS_LoadConfig: master entry missing required option "
            "'username'.\n"
         );
      }
      master->username = strdup(json_object_get_string(master_option));

      master_option = json_object_object_get(master_section, "password");
      if(master_option == NULL)
      {
         I_Error(
            "CS_LoadConfig: master entry missing required option "
            "'password'.\n"
         );
      }
      master->password_hash = CS_GetSHA1Hash(
         json_object_get_string(master_option),
         strlen(json_object_get_string(master_option))
      );

      master_option = json_object_object_get(master_section, "group");
      if(master_option == NULL)
      {
         I_Error(
            "CS_LoadConfig: master entry missing required option "
            "'group'.\n"
         );
      }
      master->group = strdup(json_object_get_string(master_option));

      master_option = json_object_object_get(master_section, "name");
      if(master_option == NULL)
      {
         I_Error(
            "CS_LoadConfig: master entry missing required option "
            "'name'.\n"
         );
      }
      master->name = strdup(json_object_get_string(master_option));

      master_servers[i].disabled = false;
      master_servers[i].updating = false;
   }
}

void SV_LoadConfig(void)
{
   json_object *section, *password, *option, *json_write_out,
               *minimum_buffer_size;
   char *config_path = NULL;
   const char *wad_folder;
   char *write_config_to = NULL;
   int position_of_config, i;
   struct stat sbuf;
   boolean requires_spectator_password, requires_player_password,
           requires_moderator_password, requires_administrator_password,
           should_free;

   requires_spectator_password = false;
   requires_player_password = false;
   requires_moderator_password = false;
   requires_administrator_password = false;
   should_free = false;

   cs_server_config = json_object_new_object();

   position_of_config = M_CheckParm("-server-config");
   if(position_of_config)
   {
      config_path = myargv[position_of_config + 1];
   }
   else
   {
      config_path = calloc(
         strlen(basepath) + strlen(DEFAULT_CONFIG_FILENAME) + 2, sizeof(char)
      );
      sprintf(config_path, "%s/%s", basepath, DEFAULT_CONFIG_FILENAME);
      M_NormalizeSlashes(config_path);
      should_free = true;
   }

   if(!stat(config_path, &sbuf))
   {
      if(!S_ISREG(sbuf.st_mode))
      {
         I_Error(
            "CS_LoadConfig: Config file %s exists but is not a file.\n",
            config_path
         );
      }
   }
   else
   {
      I_Error(
         "CS_LoadConfig: Config file %s does not exist.\n", config_path
      );
   }

   cs_json = json_object_from_file(config_path);

   if(should_free)
   {
      free(config_path);
   }

   if(is_error(cs_json))
   {
      I_Error(
         "CS_LoadConfig: Parse error:\n\t%s.\n",
         json_tokener_errors[(unsigned long)cs_json]
      );
   }

   // [CG] Loads server and options sections.
   CS_LoadConfig();

   // [CG] Load master advertising information.
   /* [CG] FIXME: It would be better to have a structure like:
    *             "masters": {
    *                 "master.totaltrash.org": {
    *                     "username": "ladna",
    *                     "password": "0wn$u",
    *                     "group": "totaltrash",
    *                     "name": "duel1"
    *                 }
    *             }
    *             Instead of:
    *             "masters": [
    *                 {
    *                     "address": "master.totaltrash.org",
    *                     "username": "ladna",
    *                     "password": "0wn$u",
    *                     "group": "totaltrash",
    *                     "name": "duel1"
    *                 }
    *             ]
    */
   section = json_object_object_get(cs_json, "masters");

   if(section != NULL)
      SV_HandleMastersSection(section);

   sv_minimum_buffer_size = 2;
   sv_spectator_password = NULL;
   sv_player_password = NULL;
   sv_moderator_password = NULL;
   sv_administrator_password = NULL;

   section = json_object_object_get(cs_json, "server");

   // [CG] Set the minimum size of the serverside command buffer.
   minimum_buffer_size = json_object_object_get(
      section, "minimum_buffer_size"
   );
   if(minimum_buffer_size != NULL)
      sv_minimum_buffer_size = json_object_get_int(minimum_buffer_size);

   // [CG] Load password information (serverside-only).
   password = json_object_object_get(section, "spectator_password");
   if(password != NULL)
   {
      if(strlen(json_object_get_string(password)))
      {
         sv_spectator_password = strdup(json_object_get_string(password));
         requires_spectator_password = true;
      }
   }

   password = json_object_object_get(section, "player_password");
   if(password != NULL)
   {
      if(strlen(json_object_get_string(password)))
      {
         sv_player_password = strdup(json_object_get_string(password));
         requires_player_password = true;
      }
   }

   password = json_object_object_get(section, "moderator_password");
   if(password != NULL)
   {
      if(strlen(json_object_get_string(password)))
      {
         sv_moderator_password = strdup(json_object_get_string(password));
         requires_moderator_password = true;
      }
   }

   password = json_object_object_get(section, "administrator_password");
   if(password != NULL)
   {
      if(strlen(json_object_get_string(password)))
      {
         sv_administrator_password = strdup(json_object_get_string(password));
         requires_administrator_password = true;
      }
   }

   if(!requires_moderator_password)
   {
      I_Error(
         "SV_LoadConfig: Option 'moderator_password' blank or not found in "
         "'server' section.\n"
      );
   }

   if(!requires_administrator_password)
   {
      I_Error(
         "SV_LoadConfig: Option 'administrator_password' blank or not found "
         "in 'server' section.\n"
      );
   }

   option = json_object_object_get(section, "wad_folders");
   if(option != NULL)
   {
      if(json_object_get_type(option) != json_type_array)
         I_Error("CS_LoadConfig: 'wad_folders' is not an array.\n");

      for(i = 0; i < json_object_array_length(option); i++)
      {
         wad_folder = json_object_get_string(json_object_array_get_idx(
            option, i
         ));
         AddIWADDir((char *)wad_folder);
      }
   }

   option = json_object_object_get(section, "write_config_to");
   if(option != NULL)
   {
      if(json_object_get_type(option) != json_type_string)
         I_Error("CS_LoadConfig: 'write_config_to' is not a string.\n");
      write_config_to = strdup(json_object_get_string(option));
   }

   // [CG] Copy the relevant sections to the server's configuration.
   section = json_object_object_get(cs_json, "resources");
   json_object_object_add(cs_server_config, "resources", section);

   section = json_object_object_get(cs_json, "server");
   json_object_object_add(cs_server_config, "server", section);

   section = json_object_object_get(cs_json, "options");
   json_object_object_add(cs_server_config, "options", section);

   section = json_object_object_get(cs_json, "maps");
   json_object_object_add(cs_server_config, "maps", section);

   // [CG] Remove sensitive or irrelevant information from the server's
   //      configuration, because this data is sent to the master.
   section = json_object_object_get(cs_server_config, "server");
   if(requires_spectator_password)
   {
      json_object_object_del(section, "spectator_password");
      cs_original_settings->requires_spectator_password = true;
   }
   if(requires_player_password)
   {
      json_object_object_del(section, "player_password");
      cs_original_settings->requires_player_password = true;
   }
   json_object_object_del(section, "moderator_password");
   json_object_object_del(section, "administrator_password");
   json_object_object_del(section, "write_config_to");

   if(json_object_object_get(section, "wad_folders") != NULL)
   {
      json_object_object_del(section, "wad_folders");
   }

   // [CG] Add password requirement information.
   json_object_object_add(
      section,
      "requires_spectator_password",
      json_object_new_boolean(requires_spectator_password)
   );
   json_object_object_add(
      section,
      "requires_player_password",
      json_object_new_boolean(requires_player_password)
   );

   // [CG] Write the configuration to disk if so directed.
   if(write_config_to != NULL)
   {
      json_write_out = json_object_new_object();
      json_object_object_add(
         json_write_out, "configuration", json_object_get(cs_server_config)
      );
      CS_PrintJSONToFile(
         write_config_to, json_object_get_string(json_write_out)
      );
      json_object_put(json_write_out);
      free(write_config_to);
   }
}

void CS_HandleResourcesSection(json_object *resources)
{
   int alternates_count, i, j;
   json_object *resource, *name, *type, *json_alternates;
   const char *resource_name, *resource_type, *alternate_resource_name;

   // [CG] Check for the IWAD first, but don't add it.  That happens later.
   for(i = 0; i < json_object_array_length(resources); i++)
   {
      resource = json_object_array_get_idx(resources, i);
      switch(json_object_get_type(resource))
      {
      case json_type_object:
         name = json_object_object_get(resource, "name");
         if(name == NULL)
         {
            printf("Skipping invalid entry, no name.\n");
            break;
         }
         resource_name = json_object_get_string(name);
         json_alternates = json_object_object_get(resource, "alternates");

         if(json_alternates == NULL)
            alternates_count = 0;
         else
            alternates_count = json_object_array_length(json_alternates);

         type = json_object_object_get(resource, "type");
         if(type && strncmp(json_object_get_string(type), "iwad", 4) == 0)
         {
            if(cs_iwad != NULL)
               I_Error("CS_LoadConfig: Cannot specify multiple IWAD files.\n");

            if(!CS_AddIWAD(resource_name))
            {
               for(j = 0; j < alternates_count; j++)
               {
                  alternate_resource_name = json_object_get_string(
                     json_object_array_get_idx(json_alternates, j)
                  );

                  if(CS_AddIWAD(alternate_resource_name))
                     break;
               }
            }
         }
         break;
      case json_type_boolean:
      case json_type_double:
      case json_type_int:
      case json_type_array:
      case json_type_string:
      case json_type_null:
      default:
         break;
      }
   }

   if(cs_iwad == NULL)
      I_Error("CS_LoadConfig: No IWAD specified.\n");

   for(i = 0; i < json_object_array_length(resources); i++)
   {
      resource = json_object_array_get_idx(resources, i);

      switch(json_object_get_type(resource))
      {
      case json_type_string:
         resource_name = json_object_get_string(resource);

         if(!CS_AddWAD(json_object_get_string(resource)))
            I_Error("Could not find PWAD '%s'.\n", resource_name);

         modifiedgame = true;
         break;
      case json_type_object:
         name = json_object_object_get(resource, "name");
         if(name == NULL)
         {
            printf("Skipping invalid entry, no name.\n");
            break;
         }
         resource_name = json_object_get_string(name);
         type = json_object_object_get(resource, "type");

         if(type)
            resource_type = json_object_get_string(type);
         else
            resource_type = "wad";

         json_alternates = json_object_object_get(resource, "alternates");

         if(json_alternates == NULL)
            alternates_count = 0;
         else
            alternates_count = json_object_array_length(json_alternates);

         if(strncmp(resource_type, "wad", 3) == 0)
         {
            if(!CS_AddWAD(resource_name))
            {
               for(j = 0; j < alternates_count; j++)
               {
                  alternate_resource_name = json_object_get_string(
                     json_object_array_get_idx(json_alternates, j)
                  );

                  if(CS_AddWAD(alternate_resource_name))
                     break;
               }
               if(j == alternates_count)
               {
                  I_Error(
                     "Could not find PWAD '%s' or any alternates.\n",
                     resource_name
                  );
               }
            }
         }
         else if(strncmp(resource_type, "dehacked", 8) == 0)
         {
            if(alternates_count)
            {
               I_Error(
                  "CS_LoadConfig: Cannot specify alternate DeHackEd patches.\n"
               );
            }
            if(!CS_AddDeHackEdFile(resource_name))
            {
               I_Error(
                  "Could not find DeHackEd file '%s'.\n", resource_name
               );
            }
            modifiedgame = true;
         }
         else if(strncmp(resource_type, "iwad", 4) != 0)
         {
            I_Error(
               "CS_LoadConfig: Unknown resource type '%s'.\n", resource_type
            );
         }
         break;
      case json_type_boolean:
      case json_type_double:
      case json_type_int:
      case json_type_array:
      case json_type_null:
         printf("Skipping invalid entry.");
         break;
      default:
         I_Error(
            "CS_LoadConfig: Unknown object type in 'resources' section.\n"
         );
         break;
      }
   }
}

void CS_HandleServerSection(json_object *server)
{
   json_object *setting;
   const char *str_value;
   int int_value;
   unsigned long public_address = 0;
   char *public_address_string;
   unsigned int i = 0;
#ifdef WIN32
   char hostname[256];
   struct hostent *host;
   struct in_addr interface_address;
   boolean found_address = false;
#else
   struct ifaddrs *interface_addresses = NULL;
   struct ifaddrs *interface_address = NULL;
   struct sockaddr_in *socket = NULL;
#endif

   setting = json_object_object_get(server, "address");
   if(setting == NULL)
      server_address->host = ENET_HOST_ANY;
   else
   {
      str_value = json_object_get_string(setting);
      if(strncmp(str_value, "public", 6) == 0)
         server_address->host = ENET_HOST_ANY;
      else
         enet_address_set_host(server_address, str_value);
   }

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
      json_object_object_del(server, "address");
      json_object_object_add(
         server, "address", json_object_new_string(public_address_string)
      );
      free(public_address_string);

      if((setting = json_object_object_get(server, "game")) != NULL)
      {
         D_CheckGamePath((char *)json_object_get_string(setting));
         gamepathset = true;
      }
   }

   server_address->port = DEFAULT_PORT;
   setting = json_object_object_get(server, "port");
   if(setting != NULL)
   {
      int_value = json_object_get_int(setting);

      if(int_value < 1 || int_value > 65535)
         I_Error("CS_LoadConfig: Invalid port.\n");

      server_address->port = int_value;
   }

   setting = json_object_object_get(server, "max_player_clients");
   cs_original_settings->max_player_clients = (MAXPLAYERS - 2);
   if(setting != NULL)
   {
      int_value = json_object_get_int(setting);
      // [CG] 2 for the server's spectator and a given client.
      if(int_value < 2)
         I_Error("CS_LoadConfig: 'max_player_clients' must be at least 2.");

      if(int_value > (MAXPLAYERS - 2))
      {
         I_Error(
            "CS_LoadConfig: 'max_player_clients' cannot exceed %d.\n",
            MAXPLAYERS - 2
         );
      }
      cs_original_settings->max_player_clients = int_value;
   }

   cs_original_settings->max_admin_clients = 2;
   setting = json_object_object_get(server, "max_admin_clients");
   if(setting != NULL)
   {
      int_value = json_object_get_int(setting);
      if(int_value < 2)
      {
         I_Error("CS_LoadConfig: 'max_admin_clients' must be at least 2.");
      }
      if(int_value > (MAXPLAYERS - 16))
      {
         I_Error(
            "CS_LoadConfig: 'max_admin_clients' cannot exceed %d.\n",
            MAXPLAYERS - 16
         );
      }
      cs_original_settings->max_admin_clients = int_value;
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

   cs_original_settings->number_of_teams = 0;
   setting = json_object_object_get(server, "number_of_teams");
   if(setting != NULL)
   {
      int_value = json_object_get_int(setting);
      if(int_value < 0)
         I_Error("CS_LoadConfig: Invalid value for 'number_of_teams'.\n");

      if(int_value > 2)
      {
         // [CG] FIXME: Teams are currently limited to 2 due to scoreboard
         //             space constraints.
         I_Error("CS_LoadConfig: 'number_of_teams' cannot exceed '2'.\n");
      }

      if(int_value >= team_color_max)
      {
         I_Error(
            "CS_LoadConfig: 'number_of_teams' cannot exceed '%d'.\n",
            team_color_max - 1
         );
      }

      cs_original_settings->number_of_teams = int_value;
   }
   printf(
      "CS_LoadConfig: Number of teams: %u.\n",
      cs_original_settings->number_of_teams
   );

   setting = json_object_object_get(server, "game_type");
   if(setting == NULL)
   {
      if(cs_original_settings->number_of_teams > 0)
      {
         I_Error("CS_LoadConfig: Cannot enable teams in coop.\n");
      }
      cs_original_settings->game_type = DefaultGameType = GameType = gt_coop;
   }
   else
   {
      cs_original_settings->ctf = false;
      str_value = json_object_get_string(setting);
      if(strncmp(str_value, "coop", 4) == 0)
      {
         if(cs_original_settings->number_of_teams > 0)
         {
            I_Error("CS_LoadConfig: Cannot enable teams in coop.\n");
         }
         cs_original_settings->game_type = DefaultGameType = GameType = gt_coop;
      }
      else
      {
         cs_original_settings->game_type = DefaultGameType = GameType = gt_dm;
         if((strncmp(str_value, "duel", 4) == 0))
         {
            if(cs_original_settings->number_of_teams > 0)
            {
               I_Error("CS_LoadConfig: Cannot enable teams in a duel.\n");
            }
            // [CG] TODO: Put this override into CS_LoadOverrides as well.
            cs_original_settings->max_players = 2;
         }
         else if((strncmp(str_value, "ctf", 3) == 0))
         {
            if(cs_original_settings->number_of_teams != 2)
            {
               I_Error(
                  "CS_LoadConfig: Must set 'number_of_teams' to 2 in CTF.\n"
               );
            }
            cs_original_settings->ctf = true;
         }
      }
   }

   setting = json_object_object_get(server, "wad_folders");
   if(setting != NULL)
   {
      for(i = 0; i < json_object_array_length(setting); i++)
      {
         AddIWADDir((char *)json_object_get_string(json_object_array_get_idx(
            setting, i
         )));
      }
   }

   setting = json_object_object_get(server, "wad_repository");
   if(setting == NULL ||
      strlen(json_object_get_string(setting)) == 0 ||
      !CS_CheckURI((char *)json_object_get_string(setting)))
   {
      cs_wad_repository = NULL;
   }
   else
   {
      cs_wad_repository = strdup(json_object_get_string(setting));
   }
}

void CS_HandleOptionsSection(json_object *options)
{
   json_object *option;

   // [CG] Validate the integer/string options here.
   CS_ValidateOptions(options);

   // [CG] Non-boolean options
   __set_int(options, max_players, 16);
   __set_int(options, max_players_per_team, 8);
   __set_int(options, dogs, 0);
   __set_int(options, skill, 5); // [CG] Nightmare!!!!
   __set_int(options, frag_limit, 0);
   __set_int(options, time_limit, 0);
   __set_int(options, score_limit, 0);
   __set_int(options, death_time_limit, 0);
   __set_int(options, respawn_protection_time, 0);
   __set_int(options, friend_distance, 128);

   // [CG] Boolean options
   __set_bool(options, build_blockmap, false);

   // [CG] Internal skill is 0-4, configured skill is 1-5, translate here.
   cs_original_settings->skill--;

   // [CG] General features
   __set_flags(options, spawn_armor, dmflags, true);
   __set_flags(options, spawn_super_items, dmflags, true);
   __set_flags(options, respawn_health, dmflags, false);
   __set_flags(options, respawn_items, dmflags, false);
   __set_flags(options, respawn_armor, dmflags, false);
   __set_flags(options, respawn_super_items, dmflags, false);
   __set_flags(options, respawn_barrels, dmflags, false);
   __set_flags(options, spawn_monsters, dmflags, false);
   __set_flags(options, fast_monsters, dmflags, false);
   __set_flags(options, strong_monsters, dmflags, false);
   __set_flags(options, powerful_monsters, dmflags, false);
   __set_flags(options, respawn_monsters, dmflags, false);
   __set_flags(options, allow_exit, dmflags, true);
   __set_flags(options, kill_on_exit, dmflags, true);
   __set_flags(options, exit_to_same_level, dmflags, false);
   __set_flags(options, spawn_in_same_spot, dmflags, false);
   __set_flags(options, leave_keys, dmflags, true);
   __set_flags(options, leave_weapons, dmflags, true);
   __set_flags(options, drop_weapons, dmflags, false);
   __set_flags(options, drop_items, dmflags, false);
   __set_flags(options, keep_items_on_exit, dmflags, false);
   __set_flags(options, keep_keys_on_exit, dmflags, false);

   // [CG] Newschool features
   __set_flags(options, allow_jump, dmflags2, false);
   __set_flags(options, allow_freelook, dmflags2, true);
   __set_flags(options, allow_crosshair, dmflags2, true);
   __set_flags(options, allow_target_names, dmflags2, true);
   __set_flags(options, allow_movebob_change, dmflags2, true);
   __set_flags(options, use_oldschool_sound_cutoff, dmflags2, true);
   __set_flags(options, allow_two_way_wallrun, dmflags2, true);
   __set_flags(options, allow_no_weapon_switch_on_pickup, dmflags2, true);
   __set_flags(options, allow_preferred_weapon_order, dmflags2, true);
   __set_flags(options, silent_weapon_pickup, dmflags2, true);
   __set_flags(options, allow_weapon_speed_change, dmflags2, false);
   __set_flags(options, teleport_missiles, dmflags2, false);
   __set_flags(options, instagib, dmflags2, false);
   __set_flags(options, allow_chasecam, dmflags2, false);
   __set_flags(options, follow_fragger_on_death, dmflags2, false);
   __set_flags(options, spawn_farthest, dmflags2, false);
   __set_flags(options, infinite_ammo, dmflags2, false);
   __set_flags(options, enable_variable_friction, dmflags2, false);
   __set_flags(options, enable_boom_push_effects, dmflags2, false);
   __set_flags(options, enable_nukage, dmflags2, true);
   __set_flags(options, allow_damage_screen_change, dmflags2, false);

   // [CG] Compatibility options
   __set_compat(options, imperfect_god_mode, god, false);
   __set_compat(options, time_limit_powerups, infcheat, false);
   __set_compat(options, normal_sky_when_invulnerable, skymap, false);
   __set_compat(options, zombie_players_can_exit, zombie, false);
   __set_compat(options, arch_viles_can_create_ghosts, vile, false);
   __set_compat(options, limit_lost_souls, pain, false);
   __set_compat(options, lost_souls_get_stuck_in_walls, skull, false);
   __set_compat(options, lost_souls_never_bounce_on_floors, soul, false);
   __set_compat(options, monsters_randomly_walk_off_lifts, staylift, false);
   __set_compat(options, monsters_get_stuck_on_door_tracks, doorstuck, false);
   __set_compat(options, monsters_give_up_pursuit, pursuit, false);
   __set_compat(options, actors_get_stuck_over_dropoffs, dropoff, false);
   __set_compat(options, actors_never_fall_off_ledges, falloff, false);
   __set_compat(options, monsters_can_telefrag_on_map30, telefrag, false);
   __set_compat(options, monsters_can_respawn_outside_map, respawnfix, false);
   __set_compat(options, disable_terrain_types, terrain, false);
   __set_compat(options, disable_falling_damage, fallingdmg, true);
   __set_compat(options, actors_have_infinite_height, overunder, false);
   __set_compat(options, doom_actor_heights_are_inaccurate, theights, false);
   __set_compat(options, bullets_never_hit_floors_and_ceilings, planeshoot,
             false);
   __set_compat(options, respawns_are_sometimes_silent_in_dm, ninja, false);
   __set_compat(options, short_vertical_mouselook_range, mouselook, false);
   __set_compat(options, radius_attacks_only_thrust_in_2d, 2dradatk, false);
   __set_compat(options, turbo_doors_make_two_closing_sounds, blazing, false);
   __set_compat(options, disable_tagged_door_light_fading, doorlight, false);
   __set_compat(options, use_doom_stairbuilding_method, stairs, false);
   __set_compat(options, use_doom_floor_motion_behavior, floors, false);
   __set_compat(options, use_doom_linedef_trigger_model, model, false);
   __set_compat(options, line_effects_work_on_sector_tag_zero, zerotags,
             false);
   __set_compat(options, one_time_line_effects_can_break, special, false);

   __set_compat2(options, monsters_remember_target, monsters_remember, false);
   __set_compat2(options, monster_infighting, monster_infighting, false);
   __set_compat2(options, monsters_back_out, monster_backing, false);
   __set_compat2(options, monsters_avoid_hazards, monster_avoid_hazards,
              false);
   __set_compat2(options, monsters_affected_by_friction, monster_friction,
              false);
   __set_compat2(options, monsters_climb_tall_stairs, monkeys, false);
   __set_compat2(options, rescue_dying_friends, help_friends, false);
   __set_compat2(options, dogs_can_jump_down, dog_jumping, false);
}

void CS_HandleMapsSection(json_object *maps)
{
   cs_resource_t *resource;
   json_object *map, *name, *wads, *overrides;
   unsigned int wads_count, overrides_count, i, j;
   unsigned int *resource_indices;
   const char *name_str, *wad_str;

   /*
   cs_map_count = json_object_array_length(maps);

   if(cs_map_count < 1)
   {
      I_Error("CS_LoadConfig: no maps defined.\n");
   }
   */

   wads = NULL;

   for(i = 0; i < json_object_array_length(maps); i++)
   {
      map = json_object_array_get_idx(maps, i);
      wads_count = overrides_count = 0;

      switch (json_object_get_type(map))
      {
         case json_type_boolean:
         case json_type_double:
         case json_type_int:
         case json_type_null:
         case json_type_array:
            fprintf(stderr, "Skipping invalid map entry, invalid type.\n");
            name_str = NULL;
            continue;
         case json_type_string:
            name_str = json_object_get_string(map);
            printf(
               "CS_LoadConfig: Adding map '%s' using no PWADs and no "
               "overrides.\n",
               name_str
            );
            break;
         case json_type_object:
            name = json_object_object_get(map, "name");
            wads = json_object_object_get(map, "wads");
            overrides = json_object_object_get(map, "overrides");
            if(name == NULL)
            {
               fprintf(stderr, "Skipping invalid map entry, no name.\n");
               continue;
            }
            name_str = json_object_get_string(name);
            if(wads != NULL)
            {
               wads_count = json_object_array_length(wads);
            }
            if(overrides != NULL)
            {
               CS_ValidateOptions(overrides);
               overrides_count = json_object_array_length(overrides);
            }
            if(wads_count == 0 && overrides_count == 0)
            {
               printf(
                  "CS_LoadConfig: Adding map '%s' using no PWADs and no "
                  "overrides.\n",
                  name_str
               );
               break;
            }
            printf("CS_LoadConfig: Adding map '%s'", name_str);
            if(wads_count < 1)
            {
               printf(" using no PWADs");
            }
            else if(wads_count == 1)
            {
               printf(" using %s", json_object_get_string(
                  json_object_array_get_idx(wads, 0)
               ));
            }
            if(wads_count <= 1)
            {
               printf(" and");
            }
            else
            {
               printf(" with");
            }
            if(overrides_count == 0)
            {
               printf(" no overrides");
            }
            else if(overrides_count == 1)
            {
               printf(" %d override", overrides_count);
            }
            else
            {
               printf(" %d overrides", overrides_count);
            }
            if(wads_count > 1)
            {
               printf(", WADs:");
               for(j = 0; j < wads_count; j++)
               {
                  printf("\n\t%s", json_object_get_string(
                     json_object_array_get_idx(wads, j)
                  ));
               }
            }
            printf(".\n");
            break;
         default:
            I_Error("Unknown object type in config file.\n");
            break;
      }
      if(strlen(name_str) > 8)
      {
         I_Error(
            "Map name '%s' exceeds the max length of 8 characters.\n",
            name_str
         );
      }
      resource_indices = calloc(wads_count, sizeof(unsigned int));
      for(j = 0; j < wads_count; j++)
      {
         wad_str = json_object_get_string(json_object_array_get_idx(wads, j));
         resource = CS_GetResource(wad_str);
         if(resource == NULL)
         {
            I_Error(
               "Error: PWAD '%s' not specified in resources, exiting.\n",
               wad_str
            );
         }
         resource_indices[j] = resource - cs_resources;
      }
      CS_AddMap(name_str, wads_count, resource_indices);
   }
   cs_current_map_index = cs_current_map_number = 0;
}

void CS_LoadConfig(void)
{
   json_object *section;

   if(server_address == NULL)
   {
      server_address = calloc(1, sizeof(ENetAddress));
   }
   else
   {
      memset(server_address, 0, sizeof(ENetAddress));
   }
   if(cs_settings == NULL)
   {
      cs_settings = calloc(1, sizeof(clientserver_settings_t));
   }
   else
   {
      memset(cs_settings, 0, sizeof(clientserver_settings_t));
   }
   if(cs_original_settings == NULL)
   {
      cs_original_settings = calloc(1, sizeof(clientserver_settings_t));
   }
   else
   {
      memset(cs_original_settings, 0, sizeof(clientserver_settings_t));
   }

   section = json_object_object_get(cs_json, "resources");
   if(section == NULL)
   {
      I_Error("Required config section 'resources' missing.\n");
   }

   // CS_HandleResourcesSection(section);

   section = json_object_object_get(cs_json, "server");

   if(section == NULL)
   {
      I_Error("Required config section 'server' missing.\n");
   }

   CS_HandleServerSection(section);

   section = json_object_object_get(cs_json, "options");

   if(section == NULL)
   {
      I_Error("Required config section 'options' missing.\n");
   }

   CS_HandleOptionsSection(section);

   section = json_object_object_get(cs_json, "maps");

   if(section == NULL)
   {
      I_Error("Required config section 'maps' missing.\n");
   }

   // CS_HandleMapsSection(section);
}

void CS_LoadWADs(void)
{
   json_object *section;
   
   section = json_object_object_get(cs_json, "resources");
   if(section == NULL)
   {
      I_Error("Required config section 'resources' missing.\n");
   }
   CS_HandleResourcesSection(section);

   section = json_object_object_get(cs_json, "maps");
   if(section == NULL)
   {
      I_Error("Required config section 'maps' missing.\n");
   }
   CS_HandleMapsSection(section);
}

void CS_ReloadDefaults(void)
{
   // [CG] Some stuff from G_ReloadDefaults, because we won't run that if
   //      in c/s mode.
   demoplayback = false;
   singledemo = false;
   netdemo = false;
   // memset(playeringame + 1, 0, sizeof(*playeringame) * (MAXPLAYERS - 1));
   // consoleplayer = displayplayer = 0;
   compatibility = false;
   demo_version = version;
   demo_subversion = SUBVERSION;
   demo_insurance = default_demo_insurance == 1;
   G_ScrambleRand();

   memcpy(cs_settings, cs_original_settings, sizeof(clientserver_settings_t));
   // [CG] Copy the default compatibility options over.
   memcpy(comp, default_comp, sizeof(comp));
}

void CS_ApplyConfigSettings(void)
{
   // [CG] Apply the settings.
   GameType = DefaultGameType = cs_settings->game_type;
   startskill = gameskill     = cs_settings->skill;
   dogs = default_dogs        = cs_settings->dogs;
   distfriend                 = cs_settings->friend_distance;
   respawn_protection_time    = cs_settings->respawn_protection_time;
   death_time_limit           = cs_settings->death_time_limit;
   death_time_expired_action  = cs_settings->death_time_expired_action;
   friendly_damage_percentage = cs_settings->friendly_damage_percentage;
   bfgtype                    = cs_settings->bfg_type;
   levelTimeLimit             = cs_settings->time_limit;
   levelFragLimit             = cs_settings->frag_limit;
   levelScoreLimit            = cs_settings->score_limit;
   r_blockmap                 = cs_settings->build_blockmap;
   dmflags                    = cs_settings->dmflags;
   dmflags2                   = cs_settings->dmflags2;

   if((dmflags2 & dmf_allow_movebob_change) == 0)
   {
      player_bobbing = default_player_bobbing;
   }
   if((dmflags2 & dmf_enable_variable_friction) == 0)
   {
      variable_friction = false;
   }
   else
   {
      variable_friction = true;
   }
   if((dmflags2 & dmf_enable_boom_push_effects) == 0)
   {
      allow_pushers = false;
   }
   else
   {
      allow_pushers = true;
   }
   if((dmflags2 & dmf_allow_freelook))
   {
      allowmlook = true;
   }
   else
   {
      allowmlook = false;
   }
   if((dmflags & dmf_respawn_monsters))
   {
      respawnparm = clrespawnparm = true;
   }
   else
   {
      respawnparm = clrespawnparm = false;
   }
   if((dmflags & dmf_fast_monsters))
   {
      fastparm = clfastparm = true;
   }
   else
   {
      fastparm = clfastparm = false;
   }
   if((dmflags & dmf_spawn_monsters))
   {
      nomonsters = clnomonsters = false;
   }
   else
   {
      nomonsters = clnomonsters = true;
   }

   if(startskill == sk_nightmare)
   {
      respawnparm = clrespawnparm = true;
      fastparm = clfastparm = true;
   }

   if(cs_settings->compatflags & cmf_comp_god)
   {
      default_comp[comp_god] = true;
   }
   else
   {
      default_comp[comp_god] = false;
   }
   if(cs_settings->compatflags & cmf_comp_infcheat)
   {
      default_comp[comp_infcheat] = true;
   }
   else
   {
      default_comp[comp_infcheat] = false;
   }
   if(cs_settings->compatflags & cmf_comp_skymap)
   {
      default_comp[comp_skymap] = true;
   }
   else
   {
      default_comp[comp_skymap] = false;
   }
   if(cs_settings->compatflags & cmf_comp_zombie)
   {
      default_comp[comp_zombie] = true;
   }
   else
   {
      default_comp[comp_zombie] = false;
   }
   if(cs_settings->compatflags & cmf_comp_vile)
   {
      default_comp[comp_vile] = true;
   }
   else
   {
      default_comp[comp_vile] = false;
   }
   if(cs_settings->compatflags & cmf_comp_pain)
   {
      default_comp[comp_pain] = true;
   }
   else
   {
      default_comp[comp_pain] = false;
   }
   if(cs_settings->compatflags & cmf_comp_skull)
   {
      default_comp[comp_skull] = true;
   }
   else
   {
      default_comp[comp_skull] = false;
   }
   if(cs_settings->compatflags & cmf_comp_soul)
   {
      default_comp[comp_soul] = true;
   }
   else
   {
      default_comp[comp_soul] = false;
   }
   if(cs_settings->compatflags & cmf_comp_staylift)
   {
      default_comp[comp_staylift] = true;
   }
   else
   {
      default_comp[comp_staylift] = false;
   }
   if(cs_settings->compatflags & cmf_comp_doorstuck)
   {
      default_comp[comp_doorstuck] = true;
   }
   else
   {
      default_comp[comp_doorstuck] = false;
   }
   if(cs_settings->compatflags & cmf_comp_pursuit)
   {
      default_comp[comp_pursuit] = true;
   }
   else
   {
      default_comp[comp_pursuit] = false;
   }
   if(cs_settings->compatflags & cmf_comp_dropoff)
   {
      default_comp[comp_dropoff] = true;
   }
   else
   {
      default_comp[comp_dropoff] = false;
   }
   if(cs_settings->compatflags & cmf_comp_falloff)
   {
      default_comp[comp_falloff] = true;
   }
   else
   {
      default_comp[comp_falloff] = false;
   }
   if(cs_settings->compatflags & cmf_comp_telefrag)
   {
      default_comp[comp_telefrag] = true;
   }
   else
   {
      default_comp[comp_telefrag] = false;
   }
   if(cs_settings->compatflags & cmf_comp_respawnfix)
   {
      default_comp[comp_respawnfix] = true;
   }
   else
   {
      default_comp[comp_respawnfix] = false;
   }
   if(cs_settings->compatflags & cmf_comp_terrain)
   {
      default_comp[comp_terrain] = true;
   }
   else
   {
      default_comp[comp_terrain] = false;
   }
   if(cs_settings->compatflags & cmf_comp_theights)
   {
      default_comp[comp_theights] = true;
   }
   else
   {
      default_comp[comp_theights] = false;
   }
   if(cs_settings->compatflags & cmf_comp_planeshoot)
   {
      default_comp[comp_planeshoot] = true;
   }
   else
   {
      default_comp[comp_planeshoot] = false;
   }
   if(cs_settings->compatflags & cmf_comp_blazing)
   {
      default_comp[comp_blazing] = true;
   }
   else
   {
      default_comp[comp_blazing] = false;
   }
   if(cs_settings->compatflags & cmf_comp_doorlight)
   {
      default_comp[comp_doorlight] = true;
   }
   else
   {
      default_comp[comp_doorlight] = false;
   }
   if(cs_settings->compatflags & cmf_comp_stairs)
   {
      default_comp[comp_stairs] = true;
   }
   else
   {
      default_comp[comp_stairs] = false;
   }
   if(cs_settings->compatflags & cmf_comp_floors)
   {
      default_comp[comp_floors] = true;
   }
   else
   {
      default_comp[comp_floors] = false;
   }
   if(cs_settings->compatflags & cmf_comp_model)
   {
      default_comp[comp_model] = true;
   }
   else
   {
      default_comp[comp_model] = false;
   }
   if(cs_settings->compatflags & cmf_comp_zerotags)
   {
      default_comp[comp_zerotags] = true;
   }
   else
   {
      default_comp[comp_zerotags] = false;
   }
   if(cs_settings->compatflags & cmf_comp_special)
   {
      default_comp[comp_special] = true;
   }
   else
   {
      default_comp[comp_special] = false;
   }
   if(cs_settings->compatflags & cmf_comp_fallingdmg)
   {
      default_comp[comp_fallingdmg] = true;
   }
   else
   {
      default_comp[comp_fallingdmg] = false;
   }
   if(cs_settings->compatflags & cmf_comp_overunder)
   {
      default_comp[comp_overunder] = true;
   }
   else
   {
      default_comp[comp_overunder] = false;
   }
   if(cs_settings->compatflags & cmf_comp_ninja)
   {
      default_comp[comp_ninja] = true;
   }
   else
   {
      default_comp[comp_ninja] = false;
   }
   if(cs_settings->compatflags2 & cmf_monsters_remember)
   {
      monsters_remember = true;
   }
   else
   {
      monsters_remember = false;
   }
   if(cs_settings->compatflags2 & cmf_monster_infighting)
   {
      monster_infighting = true;
   }
   else
   {
      monster_infighting = false;
   }
   if(cs_settings->compatflags2 & cmf_monster_backing)
   {
      monster_backing = true;
   }
   else
   {
      monster_backing = false;
   }
   if(cs_settings->compatflags2 & cmf_monster_avoid_hazards)
   {
      monster_avoid_hazards = true;
   }
   else
   {
      monster_avoid_hazards = false;
   }
   if(cs_settings->compatflags2 & cmf_monster_friction)
   {
      monster_friction = true;
   }
   else
   {
      monster_friction = false;
   }
   if(cs_settings->compatflags2 & cmf_monkeys)
   {
      monkeys = true;
   }
   else
   {
      monkeys = false;
   }
   if(cs_settings->compatflags2 & cmf_help_friends)
   {
      help_friends = true;
   }
   else
   {
      help_friends = false;
   }
   if(cs_settings->compatflags2 & cmf_dog_jumping)
   {
      dog_jumping = true;
   }
   else
   {
      dog_jumping = false;
   }
}

void CS_LoadMapOverrides(unsigned int map_index)
{
   json_object *map, *maps, *overrides, *option;

   overrides = NULL;

   // [CG] To ensure that map overrides are only valid for the given map,
   //      restore the original values first.
   CS_HandleOptionsSection(json_object_object_get(cs_json, "options"));

   CS_ReloadDefaults();

   if((maps = json_object_object_get(cs_json, "maps")) == NULL)
   {
      I_Error("Required config section 'maps' missing.\n");
   }

   if(map_index > json_object_array_length(maps))
   {
      I_Error(
         "CS_LoadMapOverrides: Invalid map index %d, exiting.\n", map_index
      );
   }

   map = json_object_array_get_idx(maps, map_index);

   if(json_object_get_type(map) != json_type_object)
   {
      // [CG] Non-object maplist entries can't have any overrides, so there's
      //      nothing to do.
      CS_ApplyConfigSettings();
      return;
   }

   if((overrides = json_object_object_get(map, "overrides")) == NULL)
   {
      // [CG] Didn't find any overrides, nothing to do.
      CS_ApplyConfigSettings();
      return;
   }

   option = json_object_object_get(overrides, "build_blockmap");
   if(option != NULL)
   {
      if(json_object_get_boolean(option))
         cs_settings->build_blockmap = true;
      else
         cs_settings->build_blockmap = false;
   }

   // [CG] DMFLAGS
   __override_flags(overrides, spawn_armor, dmflags)
   __override_flags(overrides, spawn_super_items, dmflags)
   __override_flags(overrides, respawn_health, dmflags)
   __override_flags(overrides, respawn_items, dmflags)
   __override_flags(overrides, respawn_armor, dmflags)
   __override_flags(overrides, respawn_super_items, dmflags)
   __override_flags(overrides, respawn_barrels, dmflags)
   __override_flags(overrides, spawn_monsters, dmflags)
   __override_flags(overrides, fast_monsters, dmflags)
   __override_flags(overrides, strong_monsters, dmflags)
   __override_flags(overrides, powerful_monsters, dmflags)
   __override_flags(overrides, respawn_monsters, dmflags)
   __override_flags(overrides, allow_exit, dmflags)
   __override_flags(overrides, kill_on_exit, dmflags)
   __override_flags(overrides, exit_to_same_level, dmflags)
   __override_flags(overrides, spawn_in_same_spot, dmflags)
   __override_flags(overrides, leave_keys, dmflags)
   __override_flags(overrides, leave_weapons, dmflags)
   __override_flags(overrides, drop_weapons, dmflags)
   __override_flags(overrides, drop_items, dmflags)
   __override_flags(overrides, keep_items_on_exit, dmflags)
   __override_flags(overrides, keep_keys_on_exit, dmflags)

   // [CG] DMFLAGS2
   __override_flags(overrides, allow_jump, dmflags2)
   __override_flags(overrides, allow_freelook, dmflags2)
   __override_flags(overrides, allow_crosshair, dmflags2)
   __override_flags(overrides, allow_target_names, dmflags2)
   __override_flags(overrides, allow_movebob_change, dmflags2)
   __override_flags(overrides, use_oldschool_sound_cutoff, dmflags2)
   __override_flags(overrides, allow_two_way_wallrun, dmflags2)
   __override_flags(overrides, allow_no_weapon_switch_on_pickup, dmflags2)
   __override_flags(overrides, allow_preferred_weapon_order, dmflags2)
   __override_flags(overrides, silent_weapon_pickup, dmflags2)
   __override_flags(overrides, allow_weapon_speed_change, dmflags2)
   __override_flags(overrides, teleport_missiles, dmflags2)
   __override_flags(overrides, instagib, dmflags2)
   __override_flags(overrides, allow_chasecam, dmflags2)
   __override_flags(overrides, follow_fragger_on_death, dmflags2);
   __override_flags(overrides, spawn_farthest, dmflags2)
   __override_flags(overrides, infinite_ammo, dmflags2)
   __override_flags(overrides, enable_variable_friction, dmflags2)
   __override_flags(overrides, enable_boom_push_effects, dmflags2)
   __override_flags(overrides, enable_nukage, dmflags2)
   __override_flags(overrides, allow_damage_screen_change, dmflags2)

   // [CG] Compatibility options
   __override_compat(overrides, imperfect_god_mode, god);
   __override_compat(overrides, time_limit_powerups, infcheat);
   __override_compat(overrides, normal_sky_when_invulnerable, skymap);
   __override_compat(overrides, zombie_players_can_exit, zombie);
   __override_compat(overrides, arch_viles_can_create_ghosts, vile);
   __override_compat(overrides, limit_lost_souls, pain);
   __override_compat(overrides, lost_souls_get_stuck_in_walls, skull);
   __override_compat(overrides, lost_souls_never_bounce_on_floors, soul);
   __override_compat(overrides, monsters_randomly_walk_off_lifts, staylift);
   __override_compat(overrides, monsters_get_stuck_on_door_tracks, doorstuck);
   __override_compat(overrides, monsters_give_up_pursuit, pursuit);
   __override_compat(overrides, actors_get_stuck_over_dropoffs, dropoff);
   __override_compat(overrides, actors_never_fall_off_ledges, falloff);
   __override_compat(overrides, monsters_can_telefrag_on_map30, telefrag);
   __override_compat(overrides, monsters_can_respawn_outside_map, respawnfix);
   __override_compat(overrides, disable_terrain_types, terrain);
   __override_compat(overrides, disable_falling_damage, fallingdmg);
   __override_compat(overrides, actors_have_infinite_height, overunder);
   __override_compat(overrides, doom_actor_heights_are_inaccurate, theights);
   __override_compat(
      overrides, bullets_never_hit_floors_and_ceilings, planeshoot
   );
   __override_compat(overrides, respawns_are_sometimes_silent_in_dm, ninja);
   __override_compat(overrides, short_vertical_mouselook_range, mouselook);
   __override_compat(overrides, radius_attacks_only_thrust_in_2d, 2dradatk);
   __override_compat(overrides, turbo_doors_make_two_closing_sounds, blazing);
   __override_compat(overrides, disable_tagged_door_light_fading, doorlight);
   __override_compat(overrides, use_doom_stairbuilding_method, stairs);
   __override_compat(overrides, use_doom_floor_motion_behavior, floors);
   __override_compat(overrides, use_doom_linedef_trigger_model, model);
   __override_compat(
      overrides, line_effects_work_on_sector_tag_zero, zerotags
   );
   __override_compat(overrides, one_time_line_effects_can_break, special);

   // [CG] Compatibility options that aren't in the compatibility vector.
   __override_compat2(overrides, monsters_remember_target, monsters_remember);
   __override_compat2(overrides, monster_infighting, monster_infighting);
   __override_compat2(overrides, monsters_back_out, monster_backing);
   __override_compat2(
      overrides, monsters_avoid_hazards, monster_avoid_hazards
   );
   __override_compat2(
      overrides, monsters_affected_by_friction, monster_friction
   );
   __override_compat2(overrides, monsters_climb_tall_stairs, monkeys);
   __override_compat2(overrides, rescue_dying_friends, help_friends);
   __override_compat2(overrides, dogs_can_jump_down, dog_jumping);

   // [CG] Non-boolean options
   __override_int(overrides, max_players);
   __override_int(overrides, dogs);
   __override_int(overrides, friend_distance);
   __override_int(overrides, death_time_limit);
   __override_int(overrides, respawn_protection_time);
   __override_int(overrides, time_limit);
   __override_int(overrides, frag_limit);
   __override_int(overrides, score_limit);
   __override_int(overrides, skill);

   CS_ApplyConfigSettings();
}

