// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 Charles Gunyon
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
// C/S network demo routines.
//
// By Charles Gunyon
//
//----------------------------------------------------------------------------

// [CG] The small amount of cURL code used to download a demo file is based on
//      the "simple.c" example at http://curl.haxx.se/libcurl/c/simple.html

// [CG] TODO: Recursively remove all folders found in cs_demo_folder_path.

#include "z_zone.h"

#include <json/json.h>
#include <curl/curl.h>

#include "c_runcmd.h"
#include "doomstat.h"
#include "doomtype.h"
#include "doomdef.h"
#include "e_lib.h"
#include "g_game.h"
#include "i_system.h"
#include "i_thread.h"
#include "m_file.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "m_shots.h"
#include "m_zip.h"
#include "p_saveg.h"
#include "version.h"

#include "cs_main.h"
#include "cs_team.h"
#include "cs_wad.h"
#include "cs_demo.h"
#include "cs_config.h"
#include "cl_buf.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "sv_main.h"

extern int disk_icon;

void IdentifyVersion(void);

#define TIMESTAMP_SIZE 30
#define MAX_DEMO_PATH_LENGTH 512

const char* SingleCSDemo::base_data_file_name    = "demodata.bin";
const char* SingleCSDemo::base_info_file_name    = "info.json";
const char* SingleCSDemo::base_toc_file_name     = "toc.json";
const char* SingleCSDemo::base_log_file_name     = "log.txt";
const char* SingleCSDemo::base_save_format       = "save%d.sav";
const char* SingleCSDemo::base_screenshot_format = "save%d.png";

const char* CSDemo::demo_extension      = ".ecd";
const char* CSDemo::base_info_file_name = "info.json";

static bool iwad_loaded = false;

unsigned int cs_demo_speed_rates[cs_demo_speed_fastest + 1] = {
   3, 2, 1, 0, 1, 2, 3
};

const char *cs_demo_speed_names[cs_demo_speed_fastest + 1] = {
   "0.125x", "0.25x", "0.50x", "1x", "2x", "3x", "4x"
};

CSDemo       *cs_demo = NULL;
char         *default_cs_demo_folder_path = NULL;
char         *cs_demo_folder_path = NULL;
int           default_cs_demo_compression_level = 4;
int           cs_demo_compression_level = 4;
unsigned int  cs_demo_speed = cs_demo_speed_normal;

static uint64_t getSecondsSinceEpoch()
{
   time_t t;

   if(sizeof(time_t) > sizeof(uint64_t))
      I_Error("getSecondsSinceEpoch: Timestamp larger than 64-bits.\n");

   t = time(NULL);
   if(t == -1)
      I_Error("getSecondsSinceEpoch: Error getting current time.\n");

   return (uint64_t)t;
}

static void buildDemoTimestamps(char **demo_timestamp, char **iso_timestamp)
{
   *demo_timestamp = ecalloc(char *, sizeof(char), TIMESTAMP_SIZE);
   *iso_timestamp  = ecalloc(char *, sizeof(char), TIMESTAMP_SIZE);
#ifdef _WIN32
   SYSTEMTIME stUTC, stLocal;
   FILETIME ftUTC, ftLocal;
   ULARGE_INTEGER utcInt, localInt;
   LONGLONG delta = 0;
   LONGLONG hour_offset = 0;
   LONGLONG minute_offset = 0;
   char *s;
   bool negative = false;

   GetSystemTime(&stUTC);
   SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

   SystemTimeToFileTime(&stUTC, &ftUTC);
   SystemTimeToFileTime(&stLocal, &ftLocal);

   utcInt.LowPart = ftUTC.dwLowDateTime;
   utcInt.HighPart = ftUTC.dwHighDateTime;
   localInt.LowPart = ftLocal.dwLowDateTime;
   localInt.HighPart = ftLocal.dwHighDateTime;

   // [CG] utcInt and localInt are in 100s of nanoseconds, and there are 10
   //      million of those per second, and there are 60 seconds in a minute,
   //      so convert here.  Additionally, avoid any unsigned rollover problems
   //      inside the 64-bit arithmetic.
   if(localInt.QuadPart != utcInt.QuadPart)
   {
      if(localInt.QuadPart > utcInt.QuadPart)
      {
         delta = (localInt.QuadPart - utcInt.QuadPart) / 600000000;
         negative = false;
      }
      else if(localInt.QuadPart < utcInt.QuadPart)
      {
         delta = (utcInt.QuadPart - localInt.QuadPart) / 600000000;
         negative = true;
      }

      hour_offset = delta / 60;
      minute_offset = delta % 60;
   }

   s = *demo_timestamp;
   s += psnprintf(s, 15, "%04d%02d%02d_%02d%02d%02d",
      stLocal.wYear,
      stLocal.wMonth,
      stLocal.wDay,
      stLocal.wHour,
      stLocal.wMinute,
      stLocal.wSecond
   );

   if(negative)
      psnprintf(s, 5, "-%02d%02d", hour_offset, minute_offset);
   else
      psnprintf(s, 5, "+%02d%02d", hour_offset, minute_offset);

   s = *iso_timestamp;
   s += psnprintf(s, 19, "%04d-%02d-%02dT%02d:%02d:%02d",
      stLocal.wYear,
      stLocal.wMonth,
      stLocal.wDay,
      stLocal.wHour,
      stLocal.wMinute,
      stLocal.wSecond
   );

   if(negative)
      psnprintf(s, 5, "-%02d%02d", hour_offset, minute_offset);
   else
      psnprintf(s, 5, "+%02d%02d", hour_offset, minute_offset);
#else
   const time_t current_time = time(NULL);
   struct tm *current_tm = localtime(&current_time);

   strftime(*demo_timestamp, TIMESTAMP_SIZE, "%Y%m%d_%H%M%S%z", current_tm);
   strftime(*iso_timestamp, TIMESTAMP_SIZE, "%Y-%m-%dT%H:%M:%S%z", current_tm);
#endif
}

static bool CS_deleteIfFolder(const char *path)
{
   if((M_IsFolder(path)) && (!M_DeleteFolderAndContents(path)))
      return false;
   return true;
}

SingleCSDemo::SingleCSDemo(const char *new_base_path, int new_number,
                           cs_map_t *new_map, int new_type)
   : ZoneObject()
{
   qstring base_buf;
   qstring sub_buf;

   if(new_type == client_demo_type)
      demo_type = client_demo_type;
   else if(new_type == server_demo_type)
      demo_type = server_demo_type;
   else
      I_Error("SingleCSDemo: Invalid demo type %d.\n", new_type);

   // [CG] TODO: Switch between client & server in these cases.  Switching
   //            between client & server at all is a broader TODO, but this is
   //            an area users are highly likely to wander into.
   if(CS_CLIENT && demo_type == server_demo_type)
      I_Error("Cannot load server demos in c/s client mode.\n");

   if(CS_SERVER && demo_type == client_demo_type)
      I_Error("Cannot load client demos in c/s server mode.\n");

   number = new_number;
   map = new_map;

   base_buf.Printf(0, "%s/%d", new_base_path, number);
   base_buf.normalizeSlashes();
   E_ReplaceString(base_path, base_buf.duplicate(PU_STATIC));

   sub_buf.Printf(0, "%s/%s", base_buf.constPtr(), base_data_file_name);
   E_ReplaceString(data_path, sub_buf.duplicate(PU_STATIC));

   sub_buf.Printf(0, "%s/%s", base_buf.constPtr(), base_info_file_name);
   E_ReplaceString(info_path, sub_buf.duplicate(PU_STATIC));

   sub_buf.Printf(0, "%s/%s", base_buf.constPtr(), base_toc_file_name);
   E_ReplaceString(toc_path, sub_buf.duplicate(PU_STATIC));

   sub_buf.Printf(0, "%s/%s", base_buf.constPtr(), base_log_file_name);
   E_ReplaceString(log_path, sub_buf.duplicate(PU_STATIC));

   sub_buf.Printf(0, "%s/%s", base_buf.constPtr(), base_save_format);
   E_ReplaceString(save_path_format, sub_buf.duplicate(PU_STATIC));

   sub_buf.Printf(0, "%s/%s", base_buf.constPtr(), base_screenshot_format);
   E_ReplaceString(screenshot_path_format, sub_buf.duplicate(PU_STATIC));

   buildDemoTimestamps(&demo_timestamp, &iso_timestamp);

   demo_data_handle = NULL;
   mode = mode_none;
   internal_error = no_error;

   packet_buffer_size = 0;
   packet_buffer = 0;
}

SingleCSDemo::~SingleCSDemo()
{
   if(base_path)
      efree(base_path);

   if(toc_path)
      efree(toc_path);

   if(log_path)
      efree(log_path);

   if(data_path)
      efree(data_path);

   if(save_path_format)
      efree(save_path_format);

   if(screenshot_path_format)
      efree(screenshot_path_format);

   if(demo_timestamp)
      efree(demo_timestamp);

   if(iso_timestamp)
      efree(iso_timestamp);
}

void SingleCSDemo::setError(int error_code)
{
   internal_error = error_code;
}

bool SingleCSDemo::writeToDemo(void *data, size_t size, size_t count)
{
   int saved_disk_icon;

   if(mode != mode_recording)
   {
      setError(not_open_for_recording);
      return false;
   }

   if(!demo_data_handle)
   {
      setError(not_open);
      return false;
   }

   // [CG] The Disk icon causes severe renderer lag, so disable it.
   disk_icon = 0;
   saved_disk_icon = disk_icon;

   if(M_WriteToFile(data, size, count, demo_data_handle) != count)
   {
      setError(fs_error);
      disk_icon = saved_disk_icon;
      return false;
   }

   disk_icon = saved_disk_icon;
   return true;
}

bool SingleCSDemo::readFromDemo(void *buffer, size_t size, size_t count)
{
   int saved_disk_icon;

   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!demo_data_handle)
   {
      setError(not_open);
      return false;
   }

   // [CG] The Disk icon causes severe renderer lag, so disable it.
   saved_disk_icon = disk_icon;
   disk_icon = 0;

   if(M_ReadFromFile(buffer, size, count, demo_data_handle) != count)
   {
      setError(fs_error);
      disk_icon = saved_disk_icon;
      return false;
   }

   disk_icon = saved_disk_icon;
   return true;
}

bool SingleCSDemo::skipForward(long int bytes)
{
   int saved_disk_icon;

   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!demo_data_handle)
   {
      setError(not_open);
      return false;
   }

   // [CG] The Disk icon causes severe renderer lag, so disable it.
   saved_disk_icon = disk_icon;
   disk_icon = 0;

   if(!M_SeekFile(demo_data_handle, bytes, SEEK_CUR))
   {
      setError(fs_error);
      disk_icon = saved_disk_icon;
      return false;
   }

   disk_icon = saved_disk_icon;
   return true;
}

bool SingleCSDemo::writeHeader()
{
   unsigned int i;
   uint32_t resource_name_size;
   clientserver_settings_t *settings = cs_settings;
   uint8_t eoh = end_of_header_packet;

   header.version = version;
   header.subversion = subversion;
   header.cs_protocol_version = cs_protocol_version;
   header.demo_type = demo_type;
   memcpy(&header.settings, settings, sizeof(clientserver_settings_t));
   header.local_options.player_bobbing = player_bobbing;
   header.local_options.bobbing_intensity = bobbing_intensity;
   header.local_options.doom_weapon_toggles = doom_weapon_toggles;
   header.local_options.autoaim = autoaim;
   header.local_options.weapon_speed = weapon_speed;
   header.timestamp = getSecondsSinceEpoch();
   header.length = 0;
   strncpy(header.map_name, map->name, 9);
   header.resource_count = map->resource_count + 1;
   header.consoleplayer = consoleplayer;

   if(!writeToDemo(&header, sizeof(demo_header_t), 1))
      return false;

   resource_name_size = (uint32_t)(strlen(cs_resources[0].name) + 1);
   if(!writeToDemo(&resource_name_size, sizeof(uint32_t), 1))
      return false;
   if(!writeToDemo(cs_resources[0].name, sizeof(char), resource_name_size))
      return false;
   if(!writeToDemo(&cs_resources[0].type, sizeof(int8_t), 1))
      return false;
   if(!writeToDemo(&cs_resources[0].sha1_hash, sizeof(char), 41))
      return false;

   for(i = 0; i < map->resource_count; i++)
   {
      cs_resource_t *res = &cs_resources[map->resource_indices[i]];
      resource_name_size = (uint32_t)(strlen(res->name) + 1);

      if(!writeToDemo(&resource_name_size, sizeof(uint32_t), 1))
         return false;
      if(!writeToDemo(res->name, sizeof(char), resource_name_size))
         return false;
      if(!writeToDemo(&res->type, sizeof(int8_t), 1))
         return false;
      if(!writeToDemo(res->sha1_hash, sizeof(char), 41))
         return false;
   }

   if(!writeToDemo(&eoh, sizeof(uint8_t), 1))
      return false;

   if(!M_FlushFile(demo_data_handle))
   {
      setError(fs_error);
      return false;
   }

   return true;
}

bool SingleCSDemo::writeInfo()
{
   unsigned int i;
   Json::Value map_info;

   if(M_PathExists(info_path))
      CS_ReadJSON(map_info, info_path);

   map_info["name"] = map->name;
   map_info["timestamp"] = iso_timestamp;
   map_info["length"] = header.length;
   map_info["iwad"] = cs_resources[0].name;
   map_info["local_options"]["player_bobbing"] = player_bobbing;
   map_info["local_options"]["bobbing_intensity"] = bobbing_intensity;
   map_info["local_options"]["doom_weapon_toggles"] = doom_weapon_toggles;
   map_info["local_options"]["autoaim"] = autoaim;
   map_info["local_options"]["weapon_speed"] = weapon_speed;
   map_info["settings"]["skill"] = cs_settings->skill;
   map_info["settings"]["game_type"] = cs_settings->game_type;
   map_info["settings"]["deathmatch"] = cs_settings->deathmatch;
   map_info["settings"]["teams"] = cs_settings->teams;
   map_info["settings"]["max_clients"] = cs_settings->max_clients;
   map_info["settings"]["max_players"] = cs_settings->max_players;
   map_info["settings"]["max_players_per_team"] =
      cs_settings->max_players_per_team;
   map_info["settings"]["max_lives"] = cs_settings->max_lives;
   map_info["settings"]["frag_limit"] = cs_settings->frag_limit;
   map_info["settings"]["time_limit"] = cs_settings->time_limit;
   map_info["settings"]["score_limit"] = cs_settings->score_limit;
   map_info["settings"]["dogs"] = cs_settings->dogs;
   map_info["settings"]["friend_distance"] = cs_settings->friend_distance;
   map_info["settings"]["bfg_type"] = cs_settings->bfg_type;
   map_info["settings"]["friendly_damage_percentage"] =
      cs_settings->friendly_damage_percentage;
   map_info["settings"]["spectator_time_limit"] =
      cs_settings->spectator_time_limit;
   map_info["settings"]["death_time_limit"] = cs_settings->death_time_limit;

   if(cs_settings->death_time_expired_action == DEATH_LIMIT_SPECTATE)
      map_info["settings"]["death_time_expired_action"] = "spectate";
   else
      map_info["settings"]["death_time_expired_action"] = "respawn";

   map_info["settings"]["respawn_protection_time"] =
      cs_settings->respawn_protection_time;
   map_info["settings"]["dmflags"] = cs_settings->dmflags;
   map_info["settings"]["dmflags2"] = cs_settings->dmflags2;
   map_info["settings"]["compatflags"] = cs_settings->compatflags;
   map_info["settings"]["compatflags2"] = cs_settings->compatflags2;

   for(i = 0; i < map->resource_count; i++)
   {
      cs_resource_t *res = &cs_resources[map->resource_indices[i]];

      map_info["resources"][i]["name"] = res->name;
      map_info["resources"][i]["sha1_hash"] = res->sha1_hash;
   }

   CS_WriteJSON(info_path, map_info, true);

   return true;
}

bool SingleCSDemo::updateInfo()
{
   Json::Value map_info;
   long current_demo_handle_position = M_GetFilePosition(demo_data_handle);

   if(current_demo_handle_position == -1)
   {
      setError(fs_error);
      return false;
   }

   if(!M_SeekFile(demo_data_handle, 0, SEEK_SET))
   {
      setError(fs_error);
      return false;
   }

   if(!writeToDemo(&header, sizeof(demo_header_t), 1))
      return false;

   if(!M_SeekFile(demo_data_handle, current_demo_handle_position, SEEK_SET))
   {
      setError(fs_error);
      return false;
   }

   CS_ReadJSON(map_info, info_path);

   map_info["length"] = header.length;
   map_info["settings"]["skill"] = cs_settings->skill;
   map_info["settings"]["game_type"] = cs_settings->game_type;
   map_info["settings"]["deathmatch"] = cs_settings->deathmatch;
   map_info["settings"]["teams"] = cs_settings->teams;
   map_info["settings"]["max_clients"] = cs_settings->max_clients;
   map_info["settings"]["max_players"] = cs_settings->max_players;
   map_info["settings"]["max_players_per_team"] =
      cs_settings->max_players_per_team;
   map_info["settings"]["max_lives"] = cs_settings->max_lives;
   map_info["settings"]["frag_limit"] = cs_settings->frag_limit;
   map_info["settings"]["time_limit"] = cs_settings->time_limit;
   map_info["settings"]["score_limit"] = cs_settings->score_limit;
   map_info["settings"]["dogs"] = cs_settings->dogs;
   map_info["settings"]["friend_distance"] = cs_settings->friend_distance;
   map_info["settings"]["bfg_type"] = cs_settings->bfg_type;
   map_info["settings"]["friendly_damage_percentage"] =
      cs_settings->friendly_damage_percentage;
   map_info["settings"]["spectator_time_limit"] =
      cs_settings->spectator_time_limit;
   map_info["settings"]["death_time_limit"] = cs_settings->death_time_limit;

   if(cs_settings->death_time_expired_action == DEATH_LIMIT_SPECTATE)
      map_info["settings"]["death_time_expired_action"] = "spectate";
   else
      map_info["settings"]["death_time_expired_action"] = "respawn";

   map_info["settings"]["respawn_protection_time"] =
      cs_settings->respawn_protection_time;
   map_info["settings"]["dmflags"] = cs_settings->dmflags;
   map_info["settings"]["dmflags2"] = cs_settings->dmflags2;
   map_info["settings"]["compatflags"] = cs_settings->compatflags;
   map_info["settings"]["compatflags2"] = cs_settings->compatflags2;

   CS_WriteJSON(info_path, map_info, true);

   return true;
}

bool SingleCSDemo::readHeader()
{
   unsigned int resource_index = 0;
   cs_resource_t resource;
   cs_resource_t *stored_resource;
   uint32_t resource_name_size;
   unsigned int resource_count = 0;
   unsigned int *resource_indices = NULL;
   uint8_t header_marker;

   if(!readFromDemo(&header, sizeof(demo_header_t), 1))
      return false;

   if(header.demo_type == client_demo)
   {
      demo_type = client_demo;
      clientside = true;
      serverside = false;
   }
   else if(header.demo_type == server_demo)
   {
      demo_type = server_demo;
      clientside = false;
      serverside = true;
   }
   else
      I_Error("Unknown demo type, demo likely corrupt.\n");

   if(!reloadSettings())
      return false;

   if(map->initialized)
   {
      while(resource_index++ < header.resource_count)
      {
         readFromDemo(&resource_name_size, sizeof(uint32_t), 1);
         skipForward(sizeof(char) * resource_name_size); // [CG] name
         skipForward(sizeof(int8_t));                    // [CG] type
         skipForward(sizeof(char) * 41);                 // [CG] sha1_hash
      }
   }
   else
   {
      resource.name = NULL;

      while(resource_index++ < header.resource_count)
      {
         if(!readFromDemo(&resource_name_size, sizeof(uint32_t), 1))
            return false;

         resource.name = erealloc(char *, resource.name, resource_name_size);

         if(!readFromDemo(resource.name, sizeof(char), resource_name_size))
            return false;

         if(!readFromDemo(&resource.type, sizeof(int8_t), 1))
            return false;

         if(!readFromDemo(&resource.sha1_hash, sizeof(char), 41))
            return false;

         switch(resource.type)
         {
         case rt_iwad:
            if(iwad_loaded)
            {
               if(strncmp(cs_resources[0].name, resource.name,
                                                strlen(resource.name)))
               {
                  I_Error(
                     "Cannot change IWADs during a demo (%s => %s), exiting.\n",
                     cs_resources[0].name,
                     resource.name
                  );
               }
            }
            else if(CS_AddIWAD(resource.name))
            {
               IdentifyVersion();
               iwad_loaded = true;
            }
            else
            {
               I_Error(
                  "Error: Could not find IWAD %s, exiting.\n", resource.name
               );
            }

            if(!CS_CheckResourceHash(resource.name, resource.sha1_hash))
            {
               I_Error(
                  "SHA1 hash for %s did not match, exiting.\n", resource.name
               );
            }
            break;
         case rt_deh:
            if(iwad_loaded)
            {
               stored_resource = CS_GetResource(resource.name);
               if(stored_resource == NULL)
               {
                  I_Error(
                     "Cannot add new DeHackEd patches during a demo (%s), "
                     "exiting.\n",
                     resource.name
                  );
               }
            }
            else if(!CS_AddDeHackEdFile(resource.name))
            {
               I_Error(
                  "Could not find DeHackEd patch %s, exiting.\n",
                  resource.name
               );
            }

            if(!CS_CheckResourceHash(resource.name, resource.sha1_hash))
            {
               I_Error(
                  "SHA1 hash for %s did not match, exiting.\n", resource.name
               );
            }
            break;
         case rt_pwad:
            resource_count++;
            if((stored_resource = CS_GetResource(resource.name)) == NULL)
            {
               if(!CS_AddWAD(resource.name))
                  I_Error("Could not find PWAD %s, exiting.\n", resource.name);

               stored_resource = CS_GetResource(resource.name);
            }
            resource_indices = erealloc(
               unsigned int *,
               resource_indices,
               resource_count * sizeof(unsigned int)
            );
            resource_indices[resource_count - 1] =
               stored_resource - cs_resources;
            if(!CS_CheckResourceHash(resource.name, resource.sha1_hash))
            {
               I_Error(
                  "SHA1 hash for %s did not match, exiting.\n", resource.name
               );
            }
            break;
         default:
            I_Error(
               "Demo contains invalid resource type and is likely corrupt.\n"
            );
            break;
         }
      }
      CS_AddMapAtIndex(
         header.map_name, resource_count, resource_indices, map - cs_maps
      );
   }

   if(!readFromDemo(&header_marker, sizeof(uint8_t), 1))
      return false;

   // [CG] FIXME: Doesn't deserve an I_Error here, should doom_printf and set
   //             console.
   if(header_marker != end_of_header_packet)
      I_Error("Malformed demo header, demo likely corrupt.\n");

   return true;
}

bool SingleCSDemo::handleNetworkMessageDemoPacket()
{
   int32_t player_number;
   uint32_t message_size;

   if(!readFromDemo(&player_number, sizeof(int32_t), 1))
      return false;
   if(!readFromDemo(&message_size, sizeof(uint32_t), 1))
      return false;

   if((!message_size) || (message_size > 1000000))
   {
      setError(invalid_network_message_size);
      return false;
   }

   if(packet_buffer_size < message_size)
   {
      packet_buffer_size = message_size;
      packet_buffer = erealloc(char *, packet_buffer, packet_buffer_size);
   }

   if(!readFromDemo(packet_buffer, sizeof(char), message_size))
      return false;

   if(demo_type == client_demo_type)
      cl_packet_buffer.add(packet_buffer, message_size);
   else if(demo_type == server_demo_type)
      SV_HandleMessage(packet_buffer, message_size, player_number);

   return true;
}

bool SingleCSDemo::handlePlayerCommandDemoPacket()
{
   cs_cmd_t command;

   if(!readFromDemo(&command, sizeof(cs_cmd_t), 1))
      return false;

   // [CG] This is always a command for consoleplayer.  Anything else is
   //      handled by handleNetworkMessageDemoPacket (clients don't receive
   //      commands from other players anyway).

   CS_CopyCommandToTiccmd(&players[consoleplayer].cmd, &command);

   if(demo_type == client_demo_type)
   {
      cs_cmd_t *dest_command = CL_GetCommandAtIndex(cl_commands_sent);

      memcpy(dest_command, &command, sizeof(cs_cmd_t));
      cl_commands_sent = command.index + 1;
   }

   if(demo_type == server_demo_type)
      CS_CopyCommandToTiccmd(&players[consoleplayer].cmd, &command);

   return true;
}

bool SingleCSDemo::handleConsoleCommandDemoPacket()
{
   int32_t type, src;
   uint32_t command_size, options_size;
   command_t *command;

   static char *options = NULL;
   static size_t current_options_size = 0;

   if(!readFromDemo(&type, sizeof(int32_t), 1))
      return false;

   if(!readFromDemo(&src, sizeof(int32_t), 1))
      return false;

   if(!readFromDemo(&command_size, sizeof(uint32_t), 1))
      return false;

   if(packet_buffer_size < command_size)
      packet_buffer = erealloc(char *, packet_buffer, command_size);

   if(!readFromDemo(packet_buffer, sizeof(char), command_size))
      return false;

   if(!(command = C_GetCmdForName((const char *)packet_buffer)))
   {
      doom_printf("unknown command: '%s'", packet_buffer);
      return false;
   }

   if(!readFromDemo(&options_size, sizeof(uint32_t), 1))
      return false;

   if(options_size)
   {
      if(current_options_size < options_size)
         options = erealloc(char *, options, options_size * sizeof(char));

      if(!readFromDemo(options, sizeof(char), options_size))
         return false;

      if(strncmp(packet_buffer, "mn_", 3))
         C_BufferCommand(type, command, options, src);
   }
   else if(strncmp(packet_buffer, "mn_", 3))
      C_BufferCommand(type, command, "", src);

   return true;
}


int SingleCSDemo::getNumber() const
{
   return number;
}

bool SingleCSDemo::openForPlayback()
{
   if(mode != mode_none)
   {
      setError(already_open);
      return false;
   }

   if(demo_data_handle)
   {
      setError(already_open);
      return false;
   }

   if(!(demo_data_handle = M_OpenFile(data_path, "rb")))
   {
      setError(fs_error);
      return false;
   }

   mode = mode_playback;

   if(!readHeader())
   {
      mode = mode_none;
      return false;
   }

   return true;
}

bool SingleCSDemo::openForRecording()
{
   size_t buffer_size;
   nm_sync_t sync_packet;
   byte *buffer = NULL;

   if(mode != mode_none)
   {
      setError(already_open);
      return false;
   }

   if(demo_data_handle)
   {
      setError(already_open);
      return false;
   }

   if(M_PathExists(data_path))
   {
      setError(already_exists);
      return false;
   }

   if(!M_PathExists(base_path))
   {
      if(!M_CreateFolder(base_path))
      {
         setError(fs_error);
         return false;
      }
   }

   if(!(demo_data_handle = M_OpenFile(data_path, "wb")))
   {
      setError(fs_error);
      return false;
   }

   mode = mode_recording;

   if(!writeHeader())
      return false;

   if(!writeInfo())
      return false;

   if(!updateInfo())
      return false;

   buffer_size = CS_BuildGameState(consoleplayer, &buffer);

   if(!write(buffer, buffer_size, 0))
      return false;

   efree(buffer);

   sync_packet.message_type = nm_sync;

   if(demo_type == client_demo)
      sync_packet.world_index = cl_current_world_index;
   else if(demo_type == server_demo)
      sync_packet.world_index = sv_world_index;
   else
   {
      I_Error(
         "SingleCSDemo::openForRecording: Invalid demo type %d.\n", demo_type
      );
   }

   sync_packet.gametic = gametic;
   sync_packet.levelstarttic = levelstarttic;
   sync_packet.basetic = basetic;
   sync_packet.leveltime = leveltime;

   if(!write(&sync_packet, sizeof(nm_sync_t), 0))
      return false;

   return true;
}

bool SingleCSDemo::reloadSettings()
{
   memcpy(cs_settings, &header.settings, sizeof(clientserver_settings_t));
   memcpy(cs_original_settings, cs_settings, sizeof(clientserver_settings_t));

   player_bobbing      = header.local_options.player_bobbing;
   bobbing_intensity   = header.local_options.bobbing_intensity;
   doom_weapon_toggles = header.local_options.doom_weapon_toggles;
   autoaim             = header.local_options.autoaim;
   weapon_speed        = header.local_options.weapon_speed;

   consoleplayer = displayplayer = header.consoleplayer;

   CS_ApplyConfigSettings();

   return true;
}

bool SingleCSDemo::write(void *message, uint32_t size, int32_t playernum)
{
   uint8_t demo_marker = network_message_packet;
   int32_t player_number;

   if(CS_CLIENT)
      player_number = 0;
   else
      player_number = playernum;

   if(!writeToDemo(&demo_marker, sizeof(uint8_t), 1))
      return false;
   if(!writeToDemo(&player_number, sizeof(int32_t), 1))
      return false;
   if(!writeToDemo(&size, sizeof(uint32_t), 1))
      return false;
   if(!writeToDemo(message, sizeof(char), size))
      return false;

   return true;
}

bool SingleCSDemo::write(cs_cmd_t *command)
{
   uint8_t demo_marker = player_command_packet;

   if(!writeToDemo(&demo_marker, sizeof(uint8_t), 1))
      return false;
   if(!writeToDemo(command, sizeof(cs_cmd_t), 1))
      return false;

   return true;
}

bool SingleCSDemo::write(command_t *cmd, int type, const char *opts, int src)
{
   uint8_t demo_marker = console_command_packet;
   uint32_t command_size = (uint32_t)(strlen(cmd->name) + 1);
   uint32_t options_size = (uint32_t)(strlen(opts) + 1);

   if(!writeToDemo(&demo_marker, sizeof(uint8_t), 1))
      return false;
   if(!writeToDemo(&type, sizeof(int32_t), 1))
      return false;
   if(!writeToDemo(&src, sizeof(int32_t), 1))
      return false;
   if(!writeToDemo(&command_size, sizeof(uint32_t), 1))
      return false;
   if(!writeToDemo((void *)cmd->name, sizeof(char), command_size))
      return false;
   if(!writeToDemo(&options_size, sizeof(uint32_t), 1))
      return false;
   if(!writeToDemo((void *)opts, sizeof(char), options_size))
      return false;

   return true;
}

bool SingleCSDemo::saveCheckpoint()
{
   Json::Value toc;
   Json::Value checkpoint = Json::Value(Json::objectValue);
   qstring buf, checkpoint_name;
   uint32_t index;
   long byte_index;

   if(CS_CLIENT)
      index = cl_current_world_index;
   else
      index = sv_world_index;

   buf.Printf(0, screenshot_path_format, index);
   if(!M_SaveScreenShotAs(buf.constPtr()))
   {
      setError(fs_error);
      return false;
   }

   buf.Printf(0, save_path_format, index);
   checkpoint_name.Printf(0, "Checkpoint %d", index);
   P_SaveCurrentLevel(buf.getBuffer(), checkpoint_name.getBuffer());

   if(mode == mode_recording)
   {
      if(!M_FlushFile(demo_data_handle))
      {
         setError(fs_error);
         return false;
      }
   }

   if((byte_index = M_GetFilePosition(demo_data_handle)) == -1)
   {
      setError(fs_error);
      return false;
   }

   if(M_PathExists(toc_path))
   {
      if(!M_IsFile(toc_path))
      {
         setError(toc_is_not_file);
         return false;
      }
      CS_ReadJSON(toc, toc_path);
   }
   else
      toc = Json::Value(Json::objectValue);

   checkpoint["byte_index"] = (uint32_t)(byte_index);
   checkpoint["index"] = index;
   buf.Printf(0, base_save_format, index);
   checkpoint["data_file"] = buf.constPtr();
   buf.Printf(0, base_screenshot_format, index);
   checkpoint["screenshot_file"] = buf.constPtr();
   toc["checkpoints"].append(checkpoint);
   CS_WriteJSON(toc_path, toc, true);

   return true;
}

bool SingleCSDemo::loadCheckpoint(int checkpoint_index, uint32_t byte_index)
{
   qstring buf;

   buf.Printf(0, save_path_format, checkpoint_index);

   if(!M_PathExists(buf.constPtr()))
   {
      setError(checkpoint_save_does_not_exist);
      return false;
   }

   if(!M_IsFile(buf.constPtr()))
   {
      setError(checkpoint_save_is_not_file);
      return false;
   }

   if(!M_SeekFile(demo_data_handle, byte_index, SEEK_SET))
   {
      setError(fs_error);
      return false;
   }

   CS_DoWorldDone();

   if(CS_CLIENT)
      CL_LoadGame(buf.constPtr());
   else
      P_LoadGame(buf.constPtr());

   return true;
}

bool SingleCSDemo::loadCheckpoint(int checkpoint_index)
{
   Json::Value toc;
   uint32_t byte_index;

   if(!M_PathExists(toc_path))
   {
      setError(toc_does_not_exist);
      return false;
   }

   if(!M_IsFile(toc_path))
   {
      setError(toc_is_not_file);
      return false;
   }

   CS_ReadJSON(toc, toc_path);

   if(toc["checkpoints"].empty())
   {
      setError(no_checkpoints);
      return false;
   }

   if(toc["checkpoints"][checkpoint_index].empty())
   {
      setError(checkpoint_index_not_found);
      return false;
   }

   byte_index = toc["checkpoints"][checkpoint_index]["byte_index"].asUInt();

   return loadCheckpoint(checkpoint_index, byte_index);
}

bool SingleCSDemo::loadCheckpointBefore(uint32_t index)
{
   Json::Value toc;
   int i;
   uint32_t checkpoint_index, best_index, best_byte_index;

   if(!M_PathExists(toc_path))
   {
      setError(toc_does_not_exist);
      return false;
   }

   if(!M_IsFile(toc_path))
   {
      setError(toc_is_not_file);
      return false;
   }

   CS_ReadJSON(toc, toc_path);

   if(toc["checkpoints"].empty())
   {
      setError(no_checkpoints);
      return false;
   }

   best_index = best_byte_index = 0;
   for(i = 0; toc["checkpoints"].isValidIndex(i); i++)
   {
      if(toc["checkpoints"][i].empty()                    ||
         toc["checkpoints"][i]["byte_index"].empty()      ||
         (!toc["checkpoints"][i]["byte_index"].isInt())   ||
         toc["checkpoints"][i]["index"].empty()           ||
         (!toc["checkpoints"][i]["index"].isInt())        ||
         toc["checkpoints"][i]["data_file"].empty()       ||
         (!toc["checkpoints"][i]["data_file"].isString()) ||
         toc["checkpoints"][i]["screenshot_file"].empty() ||
         (!toc["checkpoints"][i]["screenshot_file"].isString()))
      {
         setError(checkpoint_toc_corrupt);
         return false;
      }
      checkpoint_index = toc["checkpoints"][i]["index"].asUInt();

      if((checkpoint_index < index) && (checkpoint_index > best_index))
      {
         best_index = checkpoint_index;
         best_byte_index = toc["checkpoints"][i]["byte_index"].asUInt();
      }
   }

   return loadCheckpoint(best_index, best_byte_index);
}

bool SingleCSDemo::loadPreviousCheckpoint()
{
   uint32_t current_index;

   if(CS_CLIENT)
      current_index = cl_current_world_index;
   else
      current_index = sv_world_index;

   return loadCheckpointBefore(current_index);
}

bool SingleCSDemo::loadNextCheckpoint()
{
   Json::Value toc;
   int i;
   uint32_t index, current_index, best_index;
   uint32_t best_byte_index = 0;

   if(!M_PathExists(toc_path))
   {
      setError(toc_does_not_exist);
      return false;
   }

   if(!M_IsFile(toc_path))
   {
      setError(toc_is_not_file);
      return false;
   }

   CS_ReadJSON(toc, toc_path);

   if(toc["checkpoints"].empty())
   {
      setError(no_checkpoints);
      return false;
   }

   if(CS_CLIENT)
      current_index = cl_current_world_index;
   else
      current_index = sv_world_index;

   for(i = 0, best_index = 0; toc["checkpoints"].isValidIndex(i); i++)
   {
      if(toc["checkpoints"][i].empty()                    ||
         toc["checkpoints"][i]["byte_index"].empty()      ||
         (!toc["checkpoints"][i]["byte_index"].isInt())   ||
         toc["checkpoints"][i]["index"].empty()           ||
         (!toc["checkpoints"][i]["index"].isInt())        ||
         toc["checkpoints"][i]["data_file"].empty()       ||
         (!toc["checkpoints"][i]["data_file"].isString()) ||
         toc["checkpoints"][i]["screenshot_file"].empty() ||
         (!toc["checkpoints"][i]["screenshot_file"].isString()))
      {
         setError(checkpoint_toc_corrupt);
         return false;
      }
      index = toc["checkpoints"][i]["index"].asUInt();

      if(index > current_index)
      {
         if((best_index == 0) || (index < best_index))
         {
            best_index = index;
            best_byte_index = toc["checkpoints"][i]["byte_index"].asUInt();
         }
      }
   }

   if((best_index == 0) || (best_byte_index == 0))
   {
      setError(no_next_checkpoint);
      return false;
   }

   return loadCheckpoint(best_index, best_byte_index);
}

bool SingleCSDemo::readPacket()
{
   uint8_t packet_type;

   if(!readFromDemo(&packet_type, sizeof(uint8_t), 1))
      return false;

   if(packet_type == network_message_packet)
      return handleNetworkMessageDemoPacket();
   if(packet_type == player_command_packet)
      return handlePlayerCommandDemoPacket();
   if(packet_type == console_command_packet)
      return handleConsoleCommandDemoPacket();

   setError(unknown_demo_packet_type);
   return false;
}

bool SingleCSDemo::isFinished()
{
   if(!demo_data_handle)
      return true;

   if(feof(demo_data_handle))
      return true;

   return false;
}

bool SingleCSDemo::close()
{
   if(mode == mode_recording)
   {
      header.length = cl_current_world_index;
      updateInfo();
   }

   if(!M_CloseFile(demo_data_handle))
   {
      setError(fs_error);
      return false;
   }

   mode = mode_none;

   return true;
}

bool SingleCSDemo::hasError()
{
   if(internal_error != no_error)
      return true;

   return false;
}

void SingleCSDemo::clearError()
{
   internal_error = no_error;
}

const char* SingleCSDemo::getError()
{
   if(internal_error == fs_error)
      return M_GetFileSystemErrorMessage();

   if(internal_error == not_open_for_playback)
      return "Demo is not open for playback";

   if(internal_error == not_open_for_recording)
      return "Demo is not open for recording";

   if(internal_error == not_open)
      return "Demo is not open";

   if(internal_error == already_open)
      return "Demo is already open";

   if(internal_error == already_exists)
      return "Demo already exists";

   if(internal_error == demo_data_is_not_file)
      return "Demo data file is not a file";

   if(internal_error == unknown_demo_packet_type)
      return "unknown demo packet type";

   if(internal_error == invalid_network_message_size)
      return "invalid network message size";

   if(internal_error == toc_is_not_file)
      return "table of contents file is not a file";

   if(internal_error == toc_does_not_exist)
      return "table of contents file not found";

   if(internal_error == checkpoint_save_does_not_exist)
      return "checkpoint data file not found";

   if(internal_error == checkpoint_save_is_not_file)
      return "checkpoint data file is not a file";

   if(internal_error == no_checkpoints)
      return "no checkpoints saved";

   if(internal_error == checkpoint_index_not_found)
      return "checkpoint index not found";

   if(internal_error == checkpoint_toc_corrupt)
      return "checkpoint table-of-contents file corrupt";

   if(internal_error == no_previous_checkpoint)
      return "no previous checkpoint";

   if(internal_error == no_next_checkpoint)
      return "no next checkpoint";

   return "no_error";
}

CSDemo::CSDemo()
   : ZoneObject(), mode(0), internal_error(no_error), internal_curl_error(0),
                   current_demo_index(0), demo_count(0), folder_path(NULL)
{
   if((!default_cs_demo_folder_path) || (!strlen(default_cs_demo_folder_path)))
      default_cs_demo_folder_path = M_PathJoin(userpath, "demos");
   setFolderPath(default_cs_demo_folder_path);
}

CSDemo::~CSDemo()
{
   if(folder_path)
      efree(folder_path);
}

bool CSDemo::loadZipFile()
{
   // [CG] Eat errors here.
   if(current_zip_file)
   {
      if(!current_zip_file->close())
      {
         setError(zip_error);
         return false;
      }

      delete current_zip_file;
   }

   current_zip_file = new ZipFile(current_demo_archive_path);

   return true;
}

bool CSDemo::loadTempZipFile()
{
   // [CG] Eat errors here.
   if(current_zip_file)
   {
      if(!current_zip_file->close())
      {
         setError(zip_error);
         return false;
      }

      delete current_zip_file;
   }

   current_zip_file = new ZipFile(current_temp_demo_archive_path);

   return true;
}

bool CSDemo::retrieveDemo(const char *url)
{
   CURL *curl_handle;
   const char *basename;
   CURLcode res;
   FILE *fobj;
   qstring buf;
   char *real_url = NULL;

   if(CS_CheckURI(url))
      real_url = estrdup(url);
   else if(strncmp(url, "eternity://", 11) == 0)
   {
      real_url = ecalloc(char *, strlen(url) + 1, sizeof(char));
      sprintf(real_url, "http://%s", url + 11);
   }
   else
   {
      setError(invalid_url);
      return false;
   }

   if(!(basename = M_Basename(real_url)))
   {
      setError(fs_error);
      efree(real_url);
      return false;
   }

   if(strlen(basename) < 2)
   {
      setError(invalid_url);
      efree(real_url);
      return false;
   }

   buf.Printf(0, "%s/%s", folder_path, basename);

   // [CG] If the file's already been downloaded, skip all the hard work below.
   if(M_IsFile(buf.constPtr()))
   {
      efree(real_url);
      return true;
   }

   if(!M_CreateFile(buf.constPtr()))
   {
      setError(fs_error);
      efree(real_url);
      return false;
   }

   if(!(fobj = M_OpenFile(buf.constPtr(), "wb")))
   {
      setError(fs_error);
      efree(real_url);
      return false;
   }

   if(!(curl_handle = curl_easy_init()))
   {
      M_CloseFile(fobj);
      setCURLError((long)curl_handle);
      efree(real_url);
      return false;
   }

   curl_easy_setopt(curl_handle, CURLOPT_URL, real_url);
   curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fwrite);
   curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fobj);

   if((res = curl_easy_perform(curl_handle)) != 0)
   {
      M_CloseFile(fobj);
      setCURLError(res);
      efree(real_url);
      return false;
   }

   M_CloseFile(fobj);

   curl_easy_cleanup(curl_handle);

   E_ReplaceString(current_demo_archive_path, buf.duplicate(PU_STATIC));

   buf.Printf(0, "%s/%s.temp", folder_path, basename);
   E_ReplaceString(current_temp_demo_archive_path, buf.duplicate(PU_STATIC));

   efree(real_url);

   return true;
}

void CSDemo::setError(int error_code)
{
   internal_error = error_code;
}

void CSDemo::setCURLError(long error_code)
{
   internal_error = curl_error;
   internal_curl_error = error_code;
}

int CSDemo::getDemoCount() const
{
   return demo_count;
}

int CSDemo::getCurrentDemoIndex() const
{
   return current_demo_index;
}

bool CSDemo::setFolderPath(const char *new_folder_path)
{
   if(!M_PathExists(new_folder_path))
   {
      setError(folder_does_not_exist);
      return false;
   }

   if(!M_IsFolder(new_folder_path))
   {
      setError(folder_is_not_folder);
      return false;
   }

   E_ReplaceString(folder_path, estrdup(new_folder_path));
   return true;
}

bool CSDemo::record()
{
   qstring buf;
   int demo_type;
   Json::Value info;

   if(mode == mode_playback)
   {
      setError(already_playing);
      return false;
   }

   if(mode == mode_recording)
   {
      setError(already_recording);
      return false;
   }

   if(!(strncmp(folder_path, cs_demo_folder_path, MAX_DEMO_PATH_LENGTH)))
      setFolderPath(cs_demo_folder_path);

   // [CG] Build demo timestamps.  The internal format is ISO 8601 format, but
   //      because Windows won't allow colons in filenames (they are for drive-
   //      specification only) we use a custom format for the folder/filename.

   if(demo_timestamp)
      efree(demo_timestamp);

   if(iso_timestamp)
      efree(iso_timestamp);

   buildDemoTimestamps(&demo_timestamp, &iso_timestamp);

   buf.Printf(0, "%s/%s", folder_path, demo_timestamp);
   buf.normalizeSlashes();

   info["version"] = version;
   info["subversion"] = subversion;
   info["cs_protocol_version"] = cs_protocol_version;
   info["name"] = "";
   info["author"] = "";
   info["date_recorded"] = iso_timestamp;

   if(M_PathExists(buf.constPtr()))
   {
      setError(already_exists);
      return false;
   }

   if(!M_CreateFolder(buf.constPtr()))
   {
      setError(fs_error);
      return false;
   }

   if(CS_CLIENT)
      demo_type = SingleCSDemo::client_demo_type;
   else
      demo_type = SingleCSDemo::server_demo_type;

   current_demo = new SingleCSDemo(
      buf.constPtr(),
      current_demo_index,
      &cs_maps[cs_current_map_index],
      demo_type
   );

   if(current_demo->hasError())
   {
      setError(demo_error);
      M_DeleteFolder(buf.constPtr());
      return false;
   }

   if(!current_demo->openForRecording())
   {
      setError(demo_error);
      M_DeleteFolderAndContents(buf.constPtr());
      return false;
   }

   E_ReplaceString(current_demo_folder_path, buf.duplicate(PU_STATIC));
   buf.Printf(0, "%s/%s", current_demo_folder_path, base_info_file_name);
   E_ReplaceString(current_demo_info_path, buf.duplicate(PU_STATIC));
   buf.Printf(0, "%s%s", current_demo_folder_path, demo_extension);
   E_ReplaceString(current_demo_archive_path, buf.duplicate(PU_STATIC));
   buf.Printf(0, "%s%s.temp", current_demo_folder_path, demo_extension);
   E_ReplaceString(current_temp_demo_archive_path, buf.duplicate(PU_STATIC));

   CS_WriteJSON(current_demo_info_path, info, true);

   mode = mode_recording;

   return true;
}

bool CSDemo::addNewMap()
{
   int demo_type;
   qstring buf;

   if(mode != mode_recording)
   {
      setError(not_open_for_recording);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->close())
   {
      setError(demo_error);
      return false;
   }

   delete current_demo;

   if(CS_CLIENT)
      demo_type = SingleCSDemo::client_demo_type;
   else
      demo_type = SingleCSDemo::server_demo_type;

   current_demo = new SingleCSDemo(
      current_demo_folder_path,
      current_demo_index + 1,
      &cs_maps[cs_current_map_index],
      demo_type
   );

   buf.Printf(0, "%s/%d", current_demo_folder_path, current_demo_index + 1);

   if(current_demo->hasError())
   {
      setError(demo_error);
      M_DeleteFolder(buf.constPtr());
      return false;
   }

   if(!current_demo->openForRecording())
   {
      setError(demo_error);
      M_DeleteFolderAndContents(buf.constPtr());
      return false;
   }

   current_demo_index++;

   return true;
}

bool CSDemo::load(const char *url)
{
   int demo_type;
   qstring qbuf, saved_buf, test_folder_buf;
   Json::Value info;
   size_t slash_index;
   char *buf = NULL;

   if(mode == mode_playback)
   {
      setError(already_playing);
      return false;
   }

   if(mode == mode_recording)
   {
      setError(already_recording);
      return false;
   }

   if(!(strncmp(folder_path, cs_demo_folder_path, MAX_DEMO_PATH_LENGTH)))
      setFolderPath(cs_demo_folder_path);

   // [CG] If the demo's URI is a file: URI, then we don't need to download it.
   //      Otherwise it must be downloaded.

   if(strncmp(url, "file://", 7) == 0)
   {
      const char *cwd;
      qstring path_buf;

      if(!M_IsAbsolutePath(url + 7))
      {
         if(!(cwd = M_GetCurrentFolder()))
         {
            setError(fs_error);
            return false;
         }
         path_buf.Printf(0, "%s/%s", cwd, url + 7);
         path_buf.normalizeSlashes();
         efree((void *)cwd);
      }
      else
         path_buf = (url + 7);

      E_ReplaceString(current_demo_archive_path, path_buf.duplicate(PU_STATIC));

      if(!M_IsFile(current_demo_archive_path))
      {
         setError(demo_archive_not_found);
         return false;
      }

      path_buf.Printf(0, "%s/%s.temp", cs_demo_folder_path, M_Basename(url));
      E_ReplaceString(
         current_temp_demo_archive_path, path_buf.duplicate(PU_STATIC)
      );
   }
   else
   {
      qstring path_buf;

      path_buf.Printf(0, "%s/%s", cs_demo_folder_path, M_Basename(url));
      path_buf.normalizeSlashes();

      if(M_PathExists(path_buf.constPtr()))
      {
         // [CG] It would be way better to download the ZIP file into memory
         //      instead of disk.  Then all this "what if it already exists?"
         //      stuff can be removed.
         if(!M_IsFile(path_buf.constPtr()))
         {
            setError(demo_file_is_not_file);
            return false;
         }
      }
      else if(!retrieveDemo(url))
         return false;

      E_ReplaceString(current_demo_archive_path, path_buf.duplicate(PU_STATIC));

      path_buf.Printf(0, "%s/%s.temp", cs_demo_folder_path, M_Basename(url));
      E_ReplaceString(
         current_temp_demo_archive_path, path_buf.duplicate(PU_STATIC)
      );
   }

   loadZipFile();

   if(!current_zip_file->openForReading())
   {
      setError(zip_error);
      return false;
   }

   // [CG] Iterate through all the filenames in the ZIP file.  They should all
   //      have a common basename (like "20120313_194207-0400"); if they don't
   //      then the demo's structure is invalid and we must abort.
   while(current_zip_file->iterateFilenames(&buf))
   {
      qbuf = buf;

      qbuf.normalizeSlashes();
      slash_index = qbuf.findFirstOf('/');

      if(slash_index != qstring::npos)
      {
         qbuf.truncate(slash_index);

         if(!qbuf.length())
         {
            setError(invalid_demo_structure);
            current_zip_file->resetFilenameIterator();
            return false;
         }
      }

      if(!saved_buf.getSize())
      {
         saved_buf = qbuf;
         continue;
      }

      if(saved_buf != qbuf)
      {
         setError(invalid_demo_structure);
         current_zip_file->resetFilenameIterator();
         return false;
      }
   }

   if(current_zip_file->hasError())
   {
      setError(zip_error);
      return false;
   }

   qbuf.Printf(0, "%s/%s", folder_path, saved_buf.constPtr());
   qbuf.normalizeSlashes();
   E_ReplaceString(current_demo_folder_path, qbuf.duplicate(PU_STATIC));

   if(!current_zip_file->extractAllTo(folder_path))
   {
      setError(zip_error);
      return false;
   }

   if(cs_maps)
      CS_ClearMaps();

   demo_count = cs_map_count = 0;

   test_folder_buf.Printf(0, "%s/%d", qbuf.constPtr(), cs_map_count);
   while(M_IsFolder(test_folder_buf.constPtr()))
   {
      demo_count++;
      cs_map_count++;
      test_folder_buf.Printf(0, "%s/%d", qbuf.constPtr(), cs_map_count);
   }
   cs_maps = ecalloc(cs_map_t *, cs_map_count, sizeof(cs_map_t));

   if(CS_CLIENT)
      demo_type = SingleCSDemo::client_demo_type;
   else
      demo_type = SingleCSDemo::server_demo_type;

   current_demo = new SingleCSDemo(
      current_demo_folder_path,
      current_demo_index,
      &cs_maps[current_demo_index],
      demo_type
   );

   qbuf.Printf(0, "%s/%d", current_demo_folder_path, current_demo_index);

   if(current_demo->hasError())
   {
      setError(demo_error);
      M_DeleteFolderAndContents(current_demo_folder_path);
      return false;
   }

   cs_settings = estructalloc(clientserver_settings_t, 1);
   cs_original_settings = estructalloc(clientserver_settings_t, 1);

   if(!current_demo->openForPlayback())
   {
      setError(demo_error);
      M_DeleteFolderAndContents(current_demo_folder_path);
      return false;
   }

   qbuf.Printf(0, "%s/%s", current_demo_folder_path, base_info_file_name);

   E_ReplaceString(current_demo_info_path, qbuf.duplicate(PU_STATIC));

   mode = mode_playback;

   G_SetGameType(cs_original_settings->game_type);

   return true;
}

bool CSDemo::play()
{
   uint32_t current_index;

   if((!current_demo) || (mode != mode_playback))
   {
      setError(not_open_for_playback);
      return false;
   }

   CS_InitNew();

   if(CS_CLIENT)
      current_index = cl_current_world_index;
   else
      current_index = sv_world_index;

   if(!current_demo->checkpointExistsAt(current_index))
   {
      if(current_demo->hasError())
         return false;

      saveCheckpoint();
   }

   return true;
}

bool CSDemo::pause()
{
   if(this->paused)
   {
      setError(already_paused);
      return false;
   }

   this->paused = true;

   return true;
}

bool CSDemo::resume()
{
   if(!this->paused)
   {
      setError(not_paused);
      return false;
   }

   this->paused = false;

   return true;
}

bool CSDemo::setCurrentDemo(int new_demo_index)
{
   int demo_type;

   if((new_demo_index < 0) || (new_demo_index > (demo_count - 1)))
   {
      setError(invalid_demo_index);
      return false;
   }

   if(new_demo_index == current_demo_index)
   {
      setError(already_playing);
      return false;
   }

   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo->close())
   {
      setError(demo_error);
      return false;
   }

   delete current_demo;

   if(CS_CLIENT)
      demo_type = SingleCSDemo::client_demo_type;
   else
      demo_type = SingleCSDemo::server_demo_type;

   current_demo = new SingleCSDemo(
      current_demo_folder_path,
      new_demo_index,
      &cs_maps[new_demo_index],
      demo_type
   );

   if(current_demo->hasError())
   {
      setError(demo_error);
      return false;
   }

   if(CS_CLIENT)
   {
      cl_commands_sent = 0;
      cl_latest_world_index = cl_current_world_index = 0;
      cl_packet_buffer.setSynchronized(false);
   }

   if(!current_demo->openForPlayback())
   {
      setError(demo_error);
      return false;
   }

   current_demo_index = new_demo_index;

   CS_DoWorldDone();

   return true;
}

bool CSDemo::hasPrevious()
{
   if(current_demo_index > 0)
      return true;

   return false;
}

bool CSDemo::hasNext()
{
   if(current_demo_index < (demo_count - 1))
      return true;

   return false;
}

bool CSDemo::playNext()
{
   if(!hasNext())
   {
      setError(last_demo);
      return false;
   }

   G_DoCompleted(false);
   return setCurrentDemo(current_demo_index + 1);
}

bool CSDemo::playPrevious()
{
   if(!hasPrevious())
   {
      setError(first_demo);
      return false;
   }

   G_DoCompleted(false);
   return setCurrentDemo(current_demo_index - 1);
}

bool CSDemo::stop()
{
   if((mode == mode_none) || (!current_demo))
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->close())
   {
      setError(demo_error);
      return false;
   }

   delete current_demo;

   mode = mode_none;

   current_demo = NULL;

   return true;
}

bool CSDemo::close()
{
   if((mode != mode_none) && (current_demo))
   {
      if(!stop())
         return false;
   }

   if(current_demo_info_path)
      efree(current_demo_info_path);

   current_demo_index = 0;

   if(current_demo_folder_path && M_IsFolder(current_demo_folder_path))
   {
      // [CG] Don't re-ZIP the folder if there was a ZIP error, this could
      //      potentially corrupt demos.  Just remove the demo folder instead.
      if(internal_error != zip_error)
      {
         // [CG] Zip up folder to temp ZIP file, if successful overwrite the
         //      old ZIP file with the temporary one.

         loadTempZipFile();

         if(!current_zip_file->createForWriting())
         {
            setError(zip_error);
            return false;
         }

         if(!current_zip_file->addFolderRecursive(current_demo_folder_path,
                                                  cs_demo_compression_level))
         {
            setError(zip_error);
            return false;
         }

         if(!current_zip_file->close())
         {
            setError(zip_error);
            return false;
         }

         if(M_PathExists(current_demo_archive_path))
         {
            if(!M_DeleteFile(current_demo_archive_path))
            {
               setError(fs_error);
               return false;
            }
         }

         if(!M_RenamePath(current_temp_demo_archive_path,
                          current_demo_archive_path))
         {
            setError(fs_error);
            return false;
         }
      }

      if(!M_DeleteFolderAndContents(current_demo_folder_path))
      {
         setError(fs_error);
         return false;
      }
   }

   delete current_zip_file;
   current_zip_file = NULL;

   if(current_temp_demo_archive_path)
      efree(current_temp_demo_archive_path);

   if(current_demo_archive_path)
      efree(current_demo_archive_path);

   if(current_demo_folder_path)
      efree(current_demo_folder_path);

   return true;
}

bool CSDemo::reloadSettings()
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   return current_demo->reloadSettings();
}

bool CSDemo::isRecording()
{
   if(mode == mode_recording)
      return true;

   return false;
}

bool CSDemo::isPlaying()
{
   if(mode == mode_playback)
      return true;

   return false;
}

bool CSDemo::write(void *message, uint32_t size, int32_t playernum)
{
   if(mode != mode_recording)
   {
      setError(not_open_for_recording);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->write(message, size, playernum))
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::write(cs_cmd_t *command)
{
   if(mode != mode_recording)
   {
      setError(not_open_for_recording);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->write(command))
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::write(command_t *cmd, int type, const char *opts, int src)
{
   if(mode != mode_recording)
   {
      setError(not_open_for_recording);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->write(cmd, type, opts, src))
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::saveCheckpoint()
{
   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->saveCheckpoint())
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::loadCheckpoint(int checkpoint_index)
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->loadCheckpoint(checkpoint_index))
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::loadCheckpointBefore(uint32_t index)
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->loadCheckpointBefore(index))
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::loadPreviousCheckpoint()
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->loadPreviousCheckpoint())
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::loadNextCheckpoint()
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->loadNextCheckpoint())
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool SingleCSDemo::checkpointExistsAt(uint32_t index)
{
   int i;
   Json::Value toc;

   if(!M_PathExists(toc_path))
   {
      setError(toc_does_not_exist);
      return false;
   }

   if(!M_IsFile(toc_path))
   {
      setError(toc_is_not_file);
      return false;
   }

   CS_ReadJSON(toc, toc_path);

   if(toc["checkpoints"].empty())
   {
      setError(no_checkpoints);
      return false;
   }

   for(i = 0; toc["checkpoints"].isValidIndex(i); i++)
   {
      if(toc["checkpoints"][i].empty()                    ||
         toc["checkpoints"][i]["byte_index"].empty()      ||
         (!toc["checkpoints"][i]["byte_index"].isInt())   ||
         toc["checkpoints"][i]["index"].empty()           ||
         (!toc["checkpoints"][i]["index"].isInt())        ||
         toc["checkpoints"][i]["data_file"].empty()       ||
         (!toc["checkpoints"][i]["data_file"].isString()) ||
         toc["checkpoints"][i]["screenshot_file"].empty() ||
         (!toc["checkpoints"][i]["screenshot_file"].isString()))
      {
         setError(checkpoint_toc_corrupt);
         return false;
      }

      if(toc["checkpoints"][i]["index"].asUInt() == index)
         return true;
   }

   return false;
}

bool CSDemo::rewind(uint32_t tic_count)
{
   uint32_t current_index;
   unsigned int new_destination_index;

   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(CS_CLIENT)
      current_index = cl_current_world_index;
   else
      current_index = sv_world_index;

   if(tic_count > current_index)
      new_destination_index = 0;
   else
      new_destination_index = current_index - tic_count;

   if(!loadCheckpointBefore(new_destination_index))
      return false;

   while(true)
   {
      if(!readPacket())
      {
         if(isFinished())
            return true;
         else
            return false;
      }

      if(CS_CLIENT)
      {
         if(cl_current_world_index > new_destination_index)
            break;
      }
      else if(sv_world_index > new_destination_index)
         break;
   }
   return true;
}

bool CSDemo::fastForward(uint32_t tic_count)
{
   unsigned int new_destination_index;

   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(CS_CLIENT)
      new_destination_index = cl_current_world_index + tic_count;
   else
      new_destination_index = sv_world_index + tic_count;

   while(true)
   {
      if(!readPacket())
      {
         if(isFinished())
            return true;
         else
            return false;
      }

      if(CS_CLIENT)
      {
         if(cl_current_world_index > new_destination_index)
            break;
      }
      else if(sv_world_index > new_destination_index)
         break;
   }
   return true;
}

bool CSDemo::readPacket()
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->readPacket())
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::isPaused()
{
   if(this->paused)
      return true;

   return false;
}

bool CSDemo::isFinished()
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   return current_demo->isFinished();
}

bool CSDemo::hasError()
{
   if(internal_error != no_error)
      return true;

   return false;
}

const char* CSDemo::getBasename()
{
   return M_Basename(current_demo_archive_path);
}

const char* CSDemo::getError()
{
   if(internal_error == fs_error)
      return M_GetFileSystemErrorMessage();

   if(internal_error == zip_error)
   {
      if(current_zip_file && current_zip_file->hasError())
         return current_zip_file->getError();
   }

   if(internal_error == demo_error)
   {
      if(current_demo && current_demo->hasError())
         return current_demo->getError();
   }

   if(internal_error == curl_error)
      return curl_easy_strerror((CURLcode)internal_curl_error);

   if(internal_error == folder_does_not_exist)
      return "demo folder does not exist";

   if(internal_error == folder_is_not_folder)
      return "demo folder is not a folder";

   if(internal_error == not_open_for_playback)
      return "demo is not open for playback";

   if(internal_error == not_open_for_recording)
      return "demo is not open for recording";

   if(internal_error == already_playing)
      return "demo is already playing";

   if(internal_error == already_recording)
      return "demo is already recording";

   if(internal_error == already_exists)
      return "demo already exists";

   if(internal_error == not_open)
      return "demo is not open";

   if(internal_error == invalid_demo_structure)
      return "invalid demo structure";

   if(internal_error == demo_archive_not_found)
      return "demo not found";

   if(internal_error == invalid_demo_index)
      return "invalid demo index";

   if(internal_error == first_demo)
      return "already at the first demo";

   if(internal_error == last_demo)
      return "already at the last demo";

   if(internal_error == invalid_url)
      return "invalid url";

   if(internal_error == already_paused)
      return "already paused";

   if(internal_error == not_paused)
      return "not paused";

   if(internal_error == demo_file_is_not_file)
      return "demo is not a file";

   return "no_error";
}

void CS_NewDemo()
{
   if(cs_demo)
   {
      if(!cs_demo->close())
         doom_printf("Error closing demo: %s.", cs_demo->getError());

      delete cs_demo;
   }

   cs_demo = new CSDemo();
}

// [CG] For atexit.
void CS_StopDemo()
{
   if(cs_demo)
   {
      if(!cs_demo->close())
         printf("Error closing demo: %s.\n", cs_demo->getError());
   }
}

bool CS_PlayDemo(const char *url)
{
   if(CS_CLIENT && net_peer)
      CL_Disconnect();

   CS_NewDemo();

   if(!cs_demo->load(Console.argv[0]->constPtr()))
   {
      doom_printf("Error loading demo: %s.", cs_demo->getError());
      return false;
   }

   if(!cs_demo->play())
   {
      doom_printf("Error playing demo: %s.", cs_demo->getError());
      return false;
   }

   doom_printf("Playing demo %s.", Console.argv[0]->constPtr());
   return true;
}

bool CS_RecordDemo()
{
   CS_NewDemo();

   if(!cs_demo->record())
   {
      doom_printf("Error recording demo: %s.", cs_demo->getError());
      if(!cs_demo->stop())
         doom_printf("Error stopping demo: %s.", cs_demo->getError());
      return false;
   }

   doom_printf("Recording %s.", cs_demo->getBasename());
   return true;
}

void CS_ClearOldDemos()
{
   M_IterateFiles(cs_demo_folder_path, CS_deleteIfFolder);
}

