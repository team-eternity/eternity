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

#ifndef CS_DEMO_H__
#define CS_DEMO_H__

#include "z_zone.h"

class ZipFile;

#include "cs_config.h"
#include "cs_main.h"
#include "cs_wad.h"

struct command_t;

#define MAX_CS_DEMO_CHECKPOINT_INDEX_SIZE 11

enum
{
   cs_demo_speed_slowest,
   cs_demo_speed_eighth = cs_demo_speed_slowest,
   cs_demo_speed_quarter,
   cs_demo_speed_half,
   cs_demo_speed_normal,
   cs_demo_speed_double,
   cs_demo_speed_triple,
   cs_demo_speed_quadruple,
   cs_demo_speed_fastest = cs_demo_speed_quadruple
};

extern unsigned int cs_demo_speed_rates[cs_demo_speed_fastest + 1];
extern const char *cs_demo_speed_names[cs_demo_speed_fastest + 1];

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
   uint64_t timestamp;
   uint32_t length;
   char map_name[9];
   uint32_t resource_count;
   uint32_t consoleplayer;
} demo_header_t;

#pragma pack(pop)

extern char         *default_cs_demo_folder_path;
extern char         *cs_demo_folder_path;
extern int          default_cs_demo_compression_level;
extern int          cs_demo_compression_level;
extern unsigned int cs_demo_speed;
extern bool         cs_demo_seeking;
extern unsigned int cs_demo_destination_index;

class SingleCSDemo : public ZoneObject
{
   // [CG] A single c/s demo must consist of:
   //      - an info.json file
   //      - a demodata.bin file
   //
   //      It can optionally include:
   //      - toc.json:  maps savegames to game indices
   //      - log.txt:   a log of the game
   //      - saveN.sav: savegames (for loading demo checkpoints)
   //      - saveN.png: screenshots (also for demo checkpoints)

private:
   int number;
   cs_map_t *map;
   int demo_type;
   char *base_path;
   char *data_path;
   char *info_path;
   char *toc_path;
   char *log_path;
   char *save_path_format;
   char *screenshot_path_format;
   char *demo_timestamp;
   char *iso_timestamp;
   FILE *demo_data_handle;
   int mode;
   int internal_error;
   demo_header_t header;
   uint32_t packet_buffer_size;
   char *packet_buffer;

   static const char *base_data_file_name;
   static const char *base_info_file_name;
   static const char *base_toc_file_name;
   static const char *base_log_file_name;
   static const char *base_save_format;
   static const char *base_screenshot_format;

   const static int base_data_file_name_length    = 12;
   const static int base_info_file_name_length    = 9;
   const static int base_toc_file_name_length     = 8;
   const static int base_log_file_name_length     = 7;
   const static int base_save_format_length       = 20;
   const static int base_screenshot_format_length = 20;

   const static uint8_t end_of_header_packet   = 1;
   const static uint8_t network_message_packet = 2;
   const static uint8_t player_command_packet  = 3;
   const static uint8_t console_command_packet = 4;

   const static int mode_none      = 0;
   const static int mode_recording = 1;
   const static int mode_playback  = 2;

   const static int client_demo = 1;
   const static int server_demo = 2;

   const static int no_error = 0;
   const static int fs_error = 1;
   const static int not_open_for_playback = 2;
   const static int not_open_for_recording = 3;
   const static int not_open = 4;
   const static int already_open = 5;
   const static int already_exists = 6;
   const static int demo_data_is_not_file = 7;
   const static int unknown_demo_packet_type = 8;
   const static int invalid_network_message_size = 9;
   const static int toc_is_not_file = 10;
   const static int toc_does_not_exist = 11;
   const static int checkpoint_save_does_not_exist = 12;
   const static int checkpoint_save_is_not_file = 13;
   const static int no_checkpoints = 14;
   const static int checkpoint_index_not_found = 15;
   const static int checkpoint_toc_corrupt = 16;
   const static int no_previous_checkpoint = 17;
   const static int no_subsequent_checkpoint = 18;

   void setError(int error_code);
   bool writeToDemo(void *data, size_t size, size_t count);
   bool readFromDemo(void *buffer, size_t size, size_t count);
   bool writeHeader();
   bool writeInfo();
   bool updateInfo();
   bool readHeader();
   bool handleNetworkMessageDemoPacket();
   bool handlePlayerCommandDemoPacket();
   bool handleConsoleCommandDemoPacket();

public:
   const static uint8_t client_demo_type = 1;
   const static uint8_t server_demo_type = 2;

   SingleCSDemo(const char *new_base_path, int new_number, cs_map_t *map,
                int new_type);
   ~SingleCSDemo();

   int         getNumber() const;
   bool        openForPlayback();
   bool        openForRecording();
   bool        write(void *message, uint32_t size, int32_t playernum);
   bool        write(cs_cmd_t *command);
   bool        write(command_t *cmd, int type, const char *opts, int src);
   bool        saveCheckpoint();
   bool        loadCheckpoint(int checkpoint_index, uint32_t byte_index);
   bool        loadCheckpoint(int checkpoint_index);
   bool        loadCheckpointBefore(uint32_t index);
   bool        loadPreviousCheckpoint();
   bool        loadNextCheckpoint();
   bool        readPacket();
   bool        isFinished();
   bool        close();
   bool        hasError();
   void        clearError();
   const char* getError();
};

class CSDemo : public ZoneObject
{
private:
   const static int no_error = 0;
   const static int fs_error = 1;
   const static int zip_error = 2;
   const static int demo_error = 3;
   const static int curl_error = 4;
   const static int folder_does_not_exist = 5;
   const static int folder_is_not_folder = 6;
   const static int not_open_for_playback = 7;
   const static int not_open_for_recording = 8;
   const static int already_playing = 9;
   const static int already_recording = 10;
   const static int already_exists = 11;
   const static int not_open = 12;
   const static int invalid_demo_structure = 13;
   const static int demo_archive_not_found = 14;
   const static int invalid_demo_index = 15;
   const static int first_demo = 16;
   const static int last_demo = 17;
   const static int invalid_url = 18;

   const static int mode_none      = 0;
   const static int mode_recording = 1;
   const static int mode_playback  = 2;

   int mode;
   int internal_error;
   int internal_curl_error;
   int current_demo_index;
   int demo_count;

   static const char *demo_extension;
   static const char *base_info_file_name;

   char *folder_path;
   char *current_demo_archive_path;
   char *current_temp_demo_archive_path;
   char *current_demo_folder_path;
   char *current_demo_info_path;

   char *iso_timestamp;
   char *demo_timestamp;

   ZipFile *current_zip_file;
   SingleCSDemo *current_demo;

   bool loadZipFile();
   bool loadTempZipFile();
   bool retrieveDemo(const char *url);
   void setError(int error_code);
   void setCURLError(long error_code);

public:

   CSDemo();
   ~CSDemo();

   int         getCurrentDemoIndex() const;
   bool        setFolderPath(const char *new_folder_path);
   bool        record();
   bool        addNewMap();
   bool        play(const char *url);
   bool        setCurrentDemo(int new_demo_index);
   bool        playNext();
   bool        playPrevious();
   bool        stop();
   bool        close();
   bool        isRecording();
   bool        isPlaying();
   bool        write(void *message, uint32_t size, int32_t playernum);
   bool        write(cs_cmd_t *command);
   bool        write(command_t *cmd, int type, const char *opts, int src);
   bool        saveCheckpoint();
   bool        loadCheckpoint(int checkpoint_index, uint32_t byte_index);
   bool        loadCheckpoint(int checkpoint_index);
   bool        loadCheckpointBefore(uint32_t index);
   bool        loadPreviousCheckpoint();
   bool        loadNextCheckpoint();
   bool        rewind(uint32_t tic_count);
   bool        fastForward(uint32_t tic_count);
   bool        readPacket();
   bool        isFinished();
   const char* getBasename();
   const char* getError();
};

extern CSDemo *cs_demo;

void CS_NewDemo();
void CS_StopDemo();

#endif

