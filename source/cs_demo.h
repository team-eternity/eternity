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
// C/S network demo routines.
//
// By Charles Gunyon
//
//----------------------------------------------------------------------------

#ifndef __CS_DEMO_H__
#define __CS_DEMO_H__

#include <time.h>

#include "doomtype.h"
#include "c_runcmd.h"

#include "cs_main.h"

typedef enum
{
   DEMO_ERROR_NONE,
   DEMO_ERROR_ERRNO,
   DEMO_ERROR_FILE_ERRNO,
   DEMO_ERROR_UNKNOWN,
   DEMO_ERROR_DEMO_FOLDER_NOT_DEFINED,
   DEMO_ERROR_DEMO_FOLDER_NOT_FOUND,
   DEMO_ERROR_DEMO_FOLDER_NOT_FOLDER,
   DEMO_ERROR_DEMO_ALREADY_EXISTS,
   DEMO_ERROR_DEMO_NOT_FOUND,
   DEMO_ERROR_DEMO_NOT_A_FILE,
   DEMO_ERROR_CREATING_TOP_LEVEL_INFO_FAILED,
   DEMO_ERROR_CREATING_NEW_MAP_FOLDER_FAILED,
   DEMO_ERROR_CREATING_PER_MAP_INFO_FAILED,
   DEMO_ERROR_CREATING_DEMO_DATA_FILE_FAILED,
   DEMO_ERROR_OPENING_DEMO_DATA_FILE_FAILED,
   DEMO_ERROR_WRITING_DEMO_HEADER_FAILED,
   DEMO_ERROR_WRITING_DEMO_DATA_FAILED,
   DEMO_ERROR_READING_DEMO_DATA_FAILED,
   DEMO_ERROR_CREATING_CURL_HANDLE_FAILED,
   DEMO_ERROR_SAVING_DEMO_DATA_FAILED,
   DEMO_ERROR_INVALID_DEMO_URL,
   DEMO_ERROR_UNKNOWN_DEMO_PACKET_TYPE,
   DEMO_ERROR_MALFORMED_DEMO_STRUCTURE,
   DEMO_ERROR_INVALID_MAP_NUMBER,
   DEMO_ERROR_NO_PREVIOUS_MAP,
   DEMO_ERROR_NO_NEXT_MAP,
   MAX_DEMO_ERRORS,
} demo_error_t;

typedef enum
{
   CLIENTSIDE_DEMO,
   SERVERSIDE_DEMO,
   DEMO_TYPE_MAX
} demo_type_t;

typedef enum
{
   DEMO_PACKET_HEADER_END,
   DEMO_PACKET_NETWORK_MESSAGE,
   DEMO_PACKET_PLAYER_COMMAND,
   DEMO_PACKET_CONSOLE_COMMAND,
} demo_marker_t;

// [CG] Demo headers are written to disk and therefore must be packed and use
//      exact-width integer types.

#pragma pack(push, 1)

typedef struct demo_header_s
{
   uint32_t version;
   uint32_t subversion;
   uint32_t cs_protocol_version;
   int32_t demo_type;
   clientserver_settings_t settings;
   client_options_t local_options;
   time_t timestamp;
   uint32_t length;
   char map_name[9];
   uint32_t resource_count;
} demo_header_t;

#pragma pack(pop)

extern char *cs_demo_folder_path;
extern char *cs_demo_path;
extern char *cs_demo_archive_path;
extern unsigned int cs_current_demo_map;

const char* CS_GetDemoErrorMessage(void);
bool CS_SetDemoFolderPath(char *demo_folder_path);
bool CS_AddNewMapToDemo(void);
bool CS_RecordDemo(void);
bool CS_UpdateDemoLength(void);
bool CS_UpdateDemoSettings(void);
bool CS_PlayDemo(char *url);
bool CS_StopDemo(void);
bool CS_WriteToDemo(void *data, size_t data_size);
bool CS_LoadDemoMap(unsigned int map_number);
bool CS_LoadPreviousDemoMap(void);
bool CS_LoadNextDemoMap(void);
bool CS_WriteNetworkMessageToDemo(void *network_message,
                                     size_t message_size, int playernum);
bool CS_WritePlayerCommandToDemo(cs_cmd_t *player_command);
bool CS_WriteConsoleCommandToDemo(int cmdtype, command_t *command,
                                     const char *options, int cmdsrc);
bool CS_ReadDemoPacket(void);
bool CS_DemoFinished(void);

/* [CG] Documentation for the various demo packets:

   DEMO_PACKET_NETWORK_MESSAGE
      - demo_marker_t demo_marker
      - int playernum (in client demos this is always 0)
      - size_t message_size
      - char *network_message

   DEMO_PACKET_PLAYER_COMMAND
      - demo_marker_t demo_marker
      - size_t command_size
      - cs_cmd_t player_command

   DEMO_PACKET_CONSOLE_COMMAND
      - demo_marker_t demo_marker
      - int command_type
      - int command_source
      - size_t command_size
      - char *command_name
      - size_t options_size
      - char *options

*/

#endif

