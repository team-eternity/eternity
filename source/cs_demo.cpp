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

// [CG] While not copied per se, a significant portion of this code comes from
//      libarchive's example documentation at
//      http://code.google.com/p/libarchive/wiki/Examples.  In the same vein,
//      the small amount of cURL code used to download demo files located at a
//      given URL is based on the "simple.c" example located at
//      http://curl.haxx.se/libcurl/c/simple.html

// [CG] There is a large amount of error handling here, and it's because demo
//      operations don't generally require that the engine exit on error.
//      However, it's very important that if something does go wrong, the user
//      knows exactly what it is and is notified exactly when it happens so
//      they can take appropriate action - usually start a new demo if possible
//      and investigate the situation later.

// [CG] It's probably worth making this into a class at some point, if only to
//      drop all the scoping prefixes.

#include "z_zone.h"

#include <list>
#include <fstream>
#include <iostream>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#endif

#include <json/json.h>
#include <curl/curl.h>
#include <archive.h>
#include <archive_entry.h>

#include "c_runcmd.h"
#include "doomstat.h"
#include "doomtype.h"
#include "doomdef.h"
#include "g_game.h"
#include "i_system.h"
#include "i_thread.h"
#include "m_file.h"
#include "m_misc.h"
#include "version.h"

#include "cs_main.h"
#include "cs_team.h"
#include "cs_ctf.h"
#include "cs_wad.h"
#include "cs_demo.h"
#include "cs_config.h"
#include "cl_buf.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "sv_main.h"

#define TIMESTAMP_SIZE 26
#define AR_BLOCK_SIZE 16384

extern char gamemapname[9];

void IdentifyVersion(void);

bool cs_demo_recording = false;
bool cs_demo_playback  = false;

char *cs_demo_folder_path = NULL;
char *cs_demo_path = NULL;
char *cs_demo_archive_path = NULL;
unsigned int cs_current_demo_map = 0;

static bool first_map_loaded = false;

static demo_error_t demo_error_code = DEMO_ERROR_NONE;
static const char *demo_error_message = NULL;

static char *cs_demo_info_path = NULL;
static char *current_demo_map_folder = NULL;
static char *current_demo_info_path = NULL;
static char *current_demo_data_path = NULL;
static FILE *current_demo_data_file = NULL;

static const char *demo_error_strings[MAX_DEMO_ERRORS] = {
   "No error.",
   "{DUMMY (errno)}",
   "{DUMMY (file errno)}",
   "Unknown error.",
   "No demo folder defined.",
   "Demo folder not found.",
   "Demo folder exists, but is not a folder.",
   "Demo already exists.",
   "Demo not found.",
   "Demo file exists, but is not a file.",
   "Creating demo information failed.",
   "Creating demo folder for new map failed."
   "Creating demo information for new map failed.",
   "Creating demo data for new map failed.",
   "Opening demo data file for new map failed.",
   "Writing demo header failed.",
   "Writing demo data failed.",
   "Reading demo data failed.",
   "Could not start recording demo.",
   "Creating new cURL handle to download demo file failed.",
   "Could not save demo data to disk.",
   "Invalid demo URL.",
   "Unknown demo packet type.",
   "Malformed demo file/folder structure.",
   "Invalid demo map number.",
   "First map is currently loaded.",
   "Last map is currently loaded..",
};

static void set_demo_error(demo_error_t error_type)
{
   switch(error_type)
   {
   case DEMO_ERROR_ERRNO:
      demo_error_code = DEMO_ERROR_ERRNO;
      demo_error_message = strerror(errno);
      break;
   case DEMO_ERROR_FILE_ERRNO:
      demo_error_code = DEMO_ERROR_FILE_ERRNO;
      demo_error_message = M_GetFileSystemErrorMessage();
      break;
   case DEMO_ERROR_NONE:
   case DEMO_ERROR_UNKNOWN:
   case DEMO_ERROR_DEMO_FOLDER_NOT_DEFINED:
   case DEMO_ERROR_DEMO_FOLDER_NOT_FOUND:
   case DEMO_ERROR_DEMO_FOLDER_NOT_FOLDER:
   case DEMO_ERROR_DEMO_ALREADY_EXISTS:
   case DEMO_ERROR_DEMO_NOT_FOUND:
   case DEMO_ERROR_DEMO_NOT_A_FILE:
   case DEMO_ERROR_CREATING_TOP_LEVEL_INFO_FAILED:
   case DEMO_ERROR_CREATING_NEW_MAP_FOLDER_FAILED:
   case DEMO_ERROR_CREATING_PER_MAP_INFO_FAILED:
   case DEMO_ERROR_CREATING_DEMO_DATA_FILE_FAILED:
   case DEMO_ERROR_OPENING_DEMO_DATA_FILE_FAILED:
   case DEMO_ERROR_WRITING_DEMO_HEADER_FAILED:
   case DEMO_ERROR_WRITING_DEMO_DATA_FAILED:
   case DEMO_ERROR_READING_DEMO_DATA_FAILED:
   case DEMO_ERROR_CREATING_CURL_HANDLE_FAILED:
   case DEMO_ERROR_SAVING_DEMO_DATA_FAILED:
   case DEMO_ERROR_INVALID_DEMO_URL:
   case DEMO_ERROR_UNKNOWN_DEMO_PACKET_TYPE:
   case DEMO_ERROR_MALFORMED_DEMO_STRUCTURE:
   case DEMO_ERROR_INVALID_MAP_NUMBER:
   case DEMO_ERROR_NO_PREVIOUS_MAP:
   case DEMO_ERROR_NO_NEXT_MAP:
   case MAX_DEMO_ERRORS:
   default:
      if(error_type < MAX_DEMO_ERRORS)
      {
         demo_error_code = error_type;
         demo_error_message = demo_error_strings[error_type];
      }
      else
      {
         demo_error_code = DEMO_ERROR_UNKNOWN;
         demo_error_message = demo_error_strings[DEMO_ERROR_UNKNOWN];
      }
      break;
   }
}

static int copy_data(struct archive *ar, struct archive *aw)
{
   int r;
   const void *buf;
   size_t size;
   off_t offset;

   while(1)
   {
      if((r = archive_read_data_block(ar, &buf, &size, &offset)) != ARCHIVE_OK)
      {
         if(r == ARCHIVE_EOF)
            return ARCHIVE_OK;

         demo_error_message = archive_error_string(ar);
         return r;
      }

      if((r = archive_write_data_block(aw, buf, size, offset)) != ARCHIVE_OK)
      {
         demo_error_message = archive_error_string(aw);
         return r;
      }
   }
}

static bool open_current_demo_data_file(const char *mode)
{
   if(current_demo_data_file != NULL)
   {
      I_Error(
         "Error: opened a new demo without closing the old one, exiting.\n"
      );
   }

   if((current_demo_data_file = fopen(current_demo_data_path, mode)) == NULL)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

   return true;
}

static bool close_current_demo_data_file(void)
{
   printf("close_current_demo_data_file: Closing.\n");

   if(current_demo_data_file == NULL)
      I_Error("Error: closed a demo when none are open, exiting.\n");

   if(current_demo_data_file != NULL)
   {
      if(fclose(current_demo_data_file) == -1)
      {
         set_demo_error(DEMO_ERROR_ERRNO);
         return false;
      }
      current_demo_data_file = NULL;
   }

   printf("close_current_demo_data_file: Done.\n");

   return true;
}

static bool close_current_map_demo(void)
{
   printf("close_current_map_demo: Closing.\n");

   if(cs_demo_recording)
   {
      if(!CS_UpdateDemoLength())
         return false;
   }

   if(!close_current_demo_data_file())
      return false;

   if(current_demo_map_folder)
   {
      efree(current_demo_map_folder);
      current_demo_map_folder = NULL;
   }
   if(current_demo_info_path)
   {
      efree(current_demo_info_path);
      current_demo_info_path = NULL;
   }
   if(current_demo_data_path)
   {
      efree(current_demo_data_path);
      current_demo_data_path = NULL;
   }

   printf("close_current_map_demo: Done.\n");

   return true;
}

static void set_current_map_demo_paths(void)
{
   if(current_demo_map_folder == NULL)
   {
      current_demo_map_folder =
         ecalloc(char *, strlen(cs_demo_path) + 6, sizeof(char));
   }
   else
   {
      memset(current_demo_map_folder, 0, strlen(cs_demo_path) + 6);
   }
   sprintf(
      current_demo_map_folder, "%s/%d", cs_demo_path, cs_current_demo_map
   );

   if(current_demo_info_path != NULL)
      efree(current_demo_info_path);

   current_demo_info_path =
      ecalloc(char *, strlen(current_demo_map_folder) + 10, sizeof(char));
   sprintf(current_demo_info_path, "%s/info.txt", current_demo_map_folder);

   if(current_demo_data_path != NULL)
      efree(current_demo_data_path);

   current_demo_data_path =
      ecalloc(char *, strlen(current_demo_map_folder) + 14, sizeof(char));
   sprintf(current_demo_data_path, "%s/demodata.bin", current_demo_map_folder);
}

static bool add_entry_to_archive(struct archive *a,
                                 struct archive_entry *entry, char *path)
{
   struct stat stat_result;
   char buf[AR_BLOCK_SIZE];
   int r, fd, len;

   if((stat(path, &stat_result)) == -1)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

   archive_entry_set_pathname(entry, path + strlen(cs_demo_folder_path) + 1);
   archive_entry_copy_stat(entry, &stat_result);
   archive_entry_set_size(entry, stat_result.st_size);
   archive_entry_set_filetype(entry, AE_IFREG);
   archive_entry_set_perm(entry, 0600);

   if((r = archive_write_header(a, entry)) != ARCHIVE_OK)
   {
      demo_error_message = archive_error_string(a);
      return false;
   }

#ifdef WIN32
   if((fd = _open(path, O_RDONLY)) < 0)
#else
   if((fd = open(path, O_RDONLY)) < 0)
#endif
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

#ifdef WIN32
   if((len = _read(fd, buf, AR_BLOCK_SIZE)) < 0)
#else
   if((len = read(fd, buf, AR_BLOCK_SIZE)) < 0)
#endif
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

   while(len > 0)
   {
      if((r = archive_write_data(a, buf, len)) < 0)
         return false;

#ifdef WIN32
      if((len = _read(fd, buf, AR_BLOCK_SIZE)) < 0)
#else
      if((len = read(fd, buf, AR_BLOCK_SIZE)) < 0)
#endif
         return false;
   }

#ifdef WIN32
   _close(fd);
#else
   close(fd);
#endif
   archive_entry_clear(entry);

   return true;
}

static bool close_current_demo(void)
{
   int r;
   unsigned int old_cs_current_demo_map;
   struct archive *a;
   struct archive_entry *entry;
   char *archive_file_path = NULL;

   printf("close_current_demo: Closing.\n");

   archive_file_path = ecalloc(char *, strlen(cs_demo_path) + 5, sizeof(char));
   sprintf(archive_file_path, "%s.ecd", cs_demo_path);

   if(!close_current_map_demo())
      return false;
   
   if((a = archive_write_new()) == NULL)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      printf("Couldn't create archive write instance.\n");
      return false;
   }

   r = archive_write_set_compression_bzip2(a);
   archive_write_set_format_ustar(a);
   // [CG] FIXME: Caused a symbol not found error linking in MSVC++?
   // archive_write_set_options(a, "compression=9");

   if((r = archive_write_open_filename(a, archive_file_path)) != ARCHIVE_OK)
   {
      demo_error_message = archive_error_string(a);
      printf("Couldn't open new archive %s.\n", archive_file_path);
      return false;
   }

   efree(archive_file_path);

   if((entry = archive_entry_new()) == NULL)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      printf("Couldn't create new archive entry.\n");
      return false;
   }

   if(!add_entry_to_archive(a, entry, cs_demo_info_path))
   {
      printf("Couldn't add entry to archive.\n");
      return false;
   }

   old_cs_current_demo_map = cs_current_demo_map;
   cs_current_demo_map = 0;
   while(cs_current_demo_map <= old_cs_current_demo_map)
   {
      printf("Adding map demos for map %d.\n", cs_current_demo_map);
      set_current_map_demo_paths();
      if(!add_entry_to_archive(a, entry, current_demo_info_path))
      {
         printf("Couldn't add entry %s to archive.\n", current_demo_info_path);
         return false;
      }
      if(!add_entry_to_archive(a, entry, current_demo_data_path))
      {
         printf("Couldn't add entry %s to archive.\n", current_demo_data_path);
         return false;
      }
      cs_current_demo_map++;
   }

   archive_entry_free(entry);
   archive_write_close(a);
   archive_write_finish(a);

   if(!M_DeleteFolderAndContents(cs_demo_path))
   {
      printf("Couldn't remove folder %s and its contents.", cs_demo_path);
      set_demo_error(DEMO_ERROR_FILE_ERRNO);
      return false;
   }

   printf("close_current_demo: Done.\n");

   return true;
}

static bool retrieve_demo(char *url)
{
   CURL *curl_handle;
   char *basename;
   CURLcode res;
   FILE *fobj;

   if(!CS_CheckURI(url))
   {
      set_demo_error(DEMO_ERROR_INVALID_DEMO_URL);
      return false;
   }

   if((basename = (char *)strrchr((const char *)url, '/')) == NULL)
   {
      set_demo_error(DEMO_ERROR_INVALID_DEMO_URL);
      return false;
   }

   if(strlen(basename) < 2)
   {
      set_demo_error(DEMO_ERROR_INVALID_DEMO_URL);
      return false;
   }

   basename++; // [CG] Skip preceding '/'.

   cs_demo_archive_path = ecalloc(
      char *,
      strlen(cs_demo_folder_path) + strlen(basename) + 2,
      sizeof(char)
   );

   sprintf(cs_demo_archive_path, "%s/%s", cs_demo_folder_path, basename);

   if(!M_CreateFile(cs_demo_archive_path))
   {
      set_demo_error(DEMO_ERROR_FILE_ERRNO);
      return false;
   }

   if((fobj = fopen(cs_demo_archive_path, "wb")) == NULL)
   {
      set_demo_error(DEMO_ERROR_SAVING_DEMO_DATA_FAILED);
      return false;
   }

   curl_handle = curl_easy_init();
   if(!curl_handle)
   {
      fclose(fobj);
      set_demo_error(DEMO_ERROR_CREATING_CURL_HANDLE_FAILED);
      return false;
   }

   curl_easy_setopt(curl_handle, CURLOPT_URL, url);
   curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fwrite);
   curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fobj);

   if((res = curl_easy_perform(curl_handle)) != 0)
   {
      fclose(fobj);
      demo_error_message = curl_easy_strerror(res);
      return false;
   }

   fclose(fobj);

   curl_easy_cleanup(curl_handle);

   return true;
}

static bool open_demo(char *path)
{
   struct archive *a;
   struct archive *ext;
   struct archive_entry *entry;
   int flags;
   int r;

   flags = ARCHIVE_EXTRACT_TIME            |
           ARCHIVE_EXTRACT_PERM            |
           ARCHIVE_EXTRACT_ACL             |
           ARCHIVE_EXTRACT_FFLAGS          |
           ARCHIVE_EXTRACT_NO_OVERWRITE    |
           ARCHIVE_EXTRACT_SECURE_SYMLINKS |
           ARCHIVE_EXTRACT_SECURE_NODOTDOT;

   if((a = archive_read_new()) == NULL)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

   archive_read_support_format_all(a);
   archive_read_support_compression_all(a);

   if((ext = archive_write_disk_new()) == NULL)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

   archive_write_disk_set_options(ext, flags);
   archive_write_disk_set_standard_lookup(ext);

   if((r = archive_read_open_file(a, path, AR_BLOCK_SIZE)))
   {
      demo_error_message = archive_error_string(a);
      return false;
   }

   while(1)
   {
      if((r = archive_read_next_header(a, &entry)) != ARCHIVE_OK)
      {
         if(r == ARCHIVE_EOF)
            break;

         demo_error_message = archive_error_string(a);
         return false;
      }
      if((r = archive_write_header(ext, entry)) != ARCHIVE_OK)
      {
         demo_error_message = archive_error_string(a);
         return false;
      }
      if(archive_entry_size(entry) > 0)
      {
         if((r = copy_data(a, ext)) != ARCHIVE_OK)
            return false;
      }
      if((r = archive_write_finish_entry(ext)) != ARCHIVE_OK)
      {
         demo_error_message = archive_error_string(a);
         return false;
      }

      // [CG] If cs_demo_path is not yet set, set it now.
      if(cs_demo_path == NULL)
      {
         char *entry_path_name = (char *)archive_entry_pathname(entry);
         size_t basename_size = strchr(entry_path_name, '/') - entry_path_name;
         size_t demo_path_size =
            strlen(cs_demo_folder_path) + basename_size + 2;

         cs_demo_path = ecalloc(char *, demo_path_size, sizeof(char));
         sprintf(cs_demo_path, "%s/", cs_demo_folder_path);
         strncat(cs_demo_path, entry_path_name, basename_size);
         printf("cs_demo_path: %s.\n", cs_demo_path);
      }
      archive_entry_clear(entry);
   }

   archive_read_close(a);
   archive_read_finish(a);
   archive_write_close(ext);
   archive_write_finish(ext);

   return true;
}

static bool write_to_demo(void *data, size_t data_size, size_t count)
{
   if(!fwrite((const void *)data, data_size, count, current_demo_data_file))
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }
   return true;
}

static bool read_from_demo(void *buffer, size_t size, size_t count)
{
   if(size == 0 || count == 0)
      return true;

   if(fread(buffer, size, count, current_demo_data_file) < count)
   {
      if(feof(current_demo_data_file))
         printf("Reached end-of-file for %s.\n", current_demo_data_path);
      else if(ferror(current_demo_data_file))
         printf("Error reading %s.\n", current_demo_data_path);
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }
   return true;
}

static bool check_demo_folder(void)
{
   if(cs_demo_folder_path != NULL)
   {
      if(!strlen(cs_demo_folder_path))
      {
         efree(cs_demo_folder_path);
         cs_demo_folder_path =
            ecalloc(char *, strlen(basepath) + 7, sizeof(char));
         sprintf(cs_demo_folder_path, "%s/demos", basepath);
      }
   }

   if(!cs_demo_folder_path)
   {
      cs_demo_folder_path =
         ecalloc(char *, strlen(basepath) + 7, sizeof(char));
      sprintf(cs_demo_folder_path, "%s/demos", basepath);
   }


   if(!M_IsFolder(cs_demo_folder_path))
   {
      if(!M_CreateFolder(cs_demo_folder_path))
      {
         set_demo_error(DEMO_ERROR_FILE_ERRNO);
         return false;
      }
   }

   return true;
}

static bool load_current_map_demo(void)
{
   demo_header_t demo_header;
   demo_marker_t header_marker;
   unsigned int resource_index = 0;
   cs_resource_t resource;
   cs_resource_t *stored_resource;
   size_t resource_name_size;
   unsigned int resource_count;
   unsigned int *resource_indices = NULL;

   set_current_map_demo_paths();

   if(!open_current_demo_data_file("rb"))
      return false;

   if(!read_from_demo(&demo_header, sizeof(demo_header_t), 1))
      return false;

   if(demo_header.demo_type == CLIENTSIDE_DEMO)
   {
      clientside = true;
      serverside = false;
   }
   else if(demo_header.demo_type == SERVERSIDE_DEMO)
   {
      clientside = false;
      serverside = true;
   }
   else
   {
      I_Error("Unknown demo type, demo likely corrupt.\n");
   }

   memcpy(cs_settings, &demo_header.settings, sizeof(clientserver_settings_t));
   memcpy(cs_original_settings, cs_settings, sizeof(clientserver_settings_t));

   player_bobbing      = demo_header.local_options.player_bobbing;
   doom_weapon_toggles = demo_header.local_options.doom_weapon_toggles;
   autoaim             = demo_header.local_options.autoaim;
   weapon_speed        = demo_header.local_options.weapon_speed;

   if(cs_maps[cs_current_demo_map].initialized == false)
   {
      resource.name = NULL;
      resource_count = 0;

      while(resource_index++ < demo_header.resource_count)
      {
         if(!read_from_demo(&resource_name_size, sizeof(size_t), 1))
            return false;

         resource.name = erealloc(char *, resource.name, resource_name_size);

         if(!read_from_demo(resource.name, sizeof(char), resource_name_size))
            return false;

         if(!read_from_demo(&resource.type, sizeof(resource_type_t), 1))
            return false;

         if(!read_from_demo(&resource.sha1_hash, sizeof(char), 41))
            return false;

         printf("load_current_map_demo: Loading resources.\n");
         switch(resource.type)
         {
         case rt_iwad:
            printf("load_current_map_demo: Found an IWAD resource.\n");
            if(!first_map_loaded)
            {
               if(!CS_AddIWAD(resource.name))
               {
                  I_Error(
                     "Error: Could not find IWAD %s, exiting.\n", resource.name
                  );
               }
               printf("load_current_map_demo: Loading IWAD.\n");
               // [CG] Loads the IWAD and attendant GameModeInfo.
               IdentifyVersion();
            }
            else if(strncmp(cs_resources[0].name,
                            resource.name,
                            strlen(resource.name)))
            {
               I_Error(
                  "Cannot change IWADs during a demo (%s => %s), exiting.\n",
                  cs_resources[0].name,
                  resource.name
               );
            }
            else
            {
               printf("load_current_map_demo: Skipping IWAD.\n");
            }
            if(!CS_CheckResourceHash(resource.name, resource.sha1_hash))
            {
               I_Error(
                  "SHA1 hash for %s did not match, exiting.\n", resource.name
               );
            }
            break;
         case rt_deh:
            if(!first_map_loaded)
            {
               if(!CS_AddDeHackEdFile(resource.name))
               {
                  I_Error(
                     "Could not find DeHackEd patch %s, exiting.\n",
                     resource.name
                  );
               }
            }
            else
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
         demo_header.map_name,
         resource_count,
         resource_indices,
         cs_current_demo_map
      );
   }
   else
   {
      printf("Map %d was already initialized.\n", cs_current_demo_map);
   }

   // G_SetGameMap();

   if(!read_from_demo(&header_marker, sizeof(demo_marker_t), 1))
      return false;

   if(header_marker != DEMO_PACKET_HEADER_END)
      I_Error("Malformed demo header, demo likely corrupt.\n");

   first_map_loaded = true;

   printf("load_current_map_demo: loaded demo for %d.\n", cs_current_demo_map);

   return true;
}

static bool handle_network_message_demo_packet(void)
{
   int playernum;
   size_t msg_size;
   char *message;

   if(!read_from_demo(&playernum, sizeof(int), 1))
      return false;

   if(!read_from_demo(&msg_size, sizeof(size_t), 1))
      return false;

   message = ecalloc(char *, msg_size, sizeof(char));

   if(!read_from_demo(message, sizeof(char), msg_size))
      return false;

   if(CS_CLIENTDEMO)
      cl_packet_buffer.add(message, (uint32_t)msg_size);
   else
      SV_HandleMessage(message, msg_size, playernum);

   return true;
}

static bool handle_player_command_demo_packet(void)
{
   cs_cmd_t command;
   size_t command_size;

   if(!read_from_demo(&command_size, sizeof(size_t), 1))
      return false;

   if(!read_from_demo(&command, command_size, 1))
      return false;

   CS_CopyCommandToTiccmd(&players[consoleplayer].cmd, &command);

   if(CS_CLIENTDEMO)
      CS_CopyCommand(CL_GetCurrentCommand(), &command);

   return true;
}

static bool handle_console_command_demo_packet(void)
{
   int command_type, command_source;
   size_t command_size, options_size;
   char *command_name, *options;
   command_t *command;

   if(!read_from_demo(&command_type, sizeof(int), 1))
      return false;

   if(!read_from_demo(&command_source, sizeof(int), 1))
      return false;

   if(!read_from_demo(&command_size, sizeof(size_t), 1))
      return false;

   command_name = ecalloc(char *, command_size, sizeof(char));

   if(!read_from_demo(command_name, sizeof(char), command_size))
      return false;

   if(!read_from_demo(&options_size, sizeof(size_t), 1))
      return false;

   options = ecalloc(char *, options_size, sizeof(char));

   if(!read_from_demo(options, sizeof(char), options_size))
      return false;

   if((command = C_GetCmdForName((const char *)command_name)) == NULL)
   {
      doom_printf("unknown command: '%s'\n", command_name);
      return false;
   }

   C_BufferCommand(
      command_type, command, (const char *)options, command_source
   );

   return true;
}

const char* CS_GetDemoErrorMessage(void)
{
   if(demo_error_message == NULL)
      return (const char *)demo_error_strings[DEMO_ERROR_NONE];

   return (const char *)demo_error_message;
}

bool CS_SetDemoFolderPath(char *demo_folder_path)
{
   if(!M_PathExists((const char *)demo_folder_path))
   {
      set_demo_error(DEMO_ERROR_DEMO_FOLDER_NOT_FOUND);
      return false;
   }

   if(!M_IsFolder((const char *)demo_folder_path))
   {
      set_demo_error(DEMO_ERROR_DEMO_FOLDER_NOT_FOLDER);
      return false;
   }

   if(cs_demo_folder_path)
      efree(cs_demo_folder_path);

   cs_demo_folder_path = estrdup(demo_folder_path);

   // [CG] Strip trailing '/' or '\'.
   if(*(demo_folder_path + (strlen(demo_folder_path) - 1)) == '/')
      demo_folder_path[strlen(demo_folder_path)] = '\0';

   return true;
}

bool CS_RecordDemo(void)
{
   char timestamp[TIMESTAMP_SIZE];
   Json::Value json;
   Json::StyledStreamWriter writer;
   std::ofstream demo_info_file;
   const time_t current_time = time(NULL);
   struct tm *local_current_time = localtime(&current_time);

   if(cs_demo_path)
      I_Error("Can't record a demo while already recording a demo (BUG).\n");

   if(!check_demo_folder())
      return false;

   // [CG] Build the timestamp.  This is used for the demo's info as well as
   //      a unique name for the demo; odds are extremely slim that a user will
   //      attempt to create more than one demo in a second.  This is in ISO
   //      8601 format, i.e. 2011-07-03T14:32:28-05:00.
   strftime(
      timestamp, TIMESTAMP_SIZE, "%Y-%m-%dT%H:%M:%S%z", local_current_time
   );

   // [CG] Build the demo's filename
   cs_demo_path = ecalloc(
      char *,
      strlen(cs_demo_folder_path) + TIMESTAMP_SIZE + 2,
      sizeof(char)
   );
   sprintf(cs_demo_path, "%s/%s", cs_demo_folder_path, timestamp);

   // [CG] Demos start out as folders, and are only archived when demo
   //      recording is stopped.  So a folder, rather than a file, is created
   //      here.
   if(!M_CreateFolder(cs_demo_path))
   {
      efree(cs_demo_path);
      cs_demo_path = NULL;
      set_demo_error(DEMO_ERROR_FILE_ERRNO);
      return false;
   }

   cs_demo_info_path =
      ecalloc(char *, strlen(cs_demo_path) + 10, sizeof(char));
   sprintf(cs_demo_info_path, "%s/info.txt", cs_demo_path);

   // [CG] Create the top-level info.txt file.
   if(!M_CreateFile(cs_demo_info_path))
   {
      set_demo_error(DEMO_ERROR_CREATING_TOP_LEVEL_INFO_FAILED);
      return false;
   }

   json["version"] = version;
   json["subversion"] = subversion;
   json["cs_protocol_version"] = cs_protocol_version;
   json["name"] = "";
   json["author"] = "";
   json["date_recorded"] = timestamp;
   demo_info_file.open(cs_demo_info_path);
   writer.write(demo_info_file, json);

   cs_demo_recording = true;

   return true;
}

bool CS_UpdateDemoLength(void)
{
   demo_header_t demo_header;
   long previous_demo_position = ftell(current_demo_data_file);
   Json::Value map_info;

   if(previous_demo_position == -1)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

   CS_ReadJSON(map_info, current_demo_info_path);
   map_info["length"] = cl_latest_world_index / TICRATE;
   CS_WriteJSON(current_demo_info_path, map_info, true);

   if(fseek(current_demo_data_file, 0, SEEK_SET) == -1)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

   if(!read_from_demo(&demo_header, sizeof(demo_header_t), 1))
      return false;

   demo_header.length = cl_latest_world_index / TICRATE;

   if(fseek(current_demo_data_file, 0, SEEK_SET) == -1)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

   if(!write_to_demo(&demo_header, sizeof(demo_header_t), 1))
      return false;

   if(fseek(current_demo_data_file, previous_demo_position, SEEK_SET) == -1)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

   return true;
}

bool CS_UpdateDemoSettings(void)
{
   demo_header_t demo_header;
   long previous_demo_position = ftell(current_demo_data_file);
   Json::Value map_info;

   CS_ReadJSON(map_info, current_demo_info_path);

   if(previous_demo_position == -1)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

   if(fseek(current_demo_data_file, 0, SEEK_SET) == -1)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

   if(!read_from_demo(&demo_header, sizeof(demo_header_t), 1))
      return false;

   memcpy(&demo_header.settings, cs_settings, sizeof(clientserver_settings_t));

   if(fseek(current_demo_data_file, 0, SEEK_SET) == -1)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

   if(!write_to_demo(&demo_header, sizeof(demo_header_t), 1))
      return false;

   if(fseek(current_demo_data_file, previous_demo_position, SEEK_SET) == -1)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }

   map_info["settings"]["skill"] = cs_settings->skill;
   map_info["settings"]["game_type"] = cs_settings->game_type;
   map_info["settings"]["ctf"] = cs_settings->ctf;
   map_info["settings"]["max_clients"] = cs_settings->max_clients;
   map_info["settings"]["max_players"] = cs_settings->max_players;
   map_info["settings"]["max_players_per_team"] =
      cs_settings->max_players_per_team;
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

   CS_WriteJSON(current_demo_info_path, map_info, false);

   return true;
}

bool CS_AddNewMapToDemo(void)
{
   unsigned int i, resource_index;
   cs_resource_t *res;
   demo_header_t demo_header;
   size_t resource_name_size;
   cs_map_t *map = &cs_maps[cs_current_map_number];
   demo_marker_t header_marker = DEMO_PACKET_HEADER_END;
   Json::Value map_info;

   if(current_demo_data_file != NULL)
   {
      if(!close_current_demo_data_file())
         return false;
      cs_current_demo_map++;
   }

   set_current_map_demo_paths();

   // [CG] Create a folder for the first map inside the new demo folder.
   if(!M_CreateFolder(current_demo_map_folder))
   {
      set_demo_error(DEMO_ERROR_CREATING_NEW_MAP_FOLDER_FAILED);
      return false;
   }

   // [CG] Create the per-map info.txt file.
   if(!M_CreateFile(current_demo_info_path))
   {
      set_demo_error(DEMO_ERROR_CREATING_PER_MAP_INFO_FAILED);
      return false;
   }

   // [CG] Create the demo data file.
   if(!M_CreateFile(current_demo_data_path))
   {
      set_demo_error(DEMO_ERROR_CREATING_DEMO_DATA_FILE_FAILED);
      return false;
   }

   if(!open_current_demo_data_file("w+b"))
      return false;

   CS_UpdateDemoSettings();

   CS_ReadJSON(map_info, current_demo_info_path);

   map_info["version"] = version;
   map_info["subversion"] = subversion;
   map_info["cs_protocol_version"] = cs_protocol_version;

   if(CS_CLIENT)
      map_info["demo_type"] = "client";
   else if(CS_SERVER)
      map_info["demo_type"] = "server";

   map_info["name"] = map->name;
   map_info["timestamp"] = (uint32_t)time(NULL);
   map_info["length"] = 0;
   map_info["iwad"] = cs_resources[0].name;
   map_info["local_options"]["player_bobbing"] = player_bobbing;
   map_info["local_options"]["doom_weapon_toggles"] = doom_weapon_toggles;
   map_info["local_options"]["autoaim"] = autoaim;
   map_info["local_options"]["weapon_speed"] = weapon_speed;

   for(i = 0; i < map->resource_count; i++)
   {
      resource_index = map->resource_indices[i];
      res = &cs_resources[resource_index];
      map_info["resources"][i]["name"] = res->name;
      map_info["resources"][i]["sha1_hash"] = res->sha1_hash;
   }

   CS_WriteJSON(current_demo_info_path, map_info, false);

   demo_header.version = version;
   demo_header.subversion = subversion;
   demo_header.cs_protocol_version = cs_protocol_version;
   if(CS_CLIENT)
      demo_header.demo_type = CLIENTSIDE_DEMO;
   else
      demo_header.demo_type = SERVERSIDE_DEMO;
   memcpy(&demo_header.settings, cs_settings, sizeof(clientserver_settings_t));
   demo_header.local_options.player_bobbing = player_bobbing;
   demo_header.local_options.doom_weapon_toggles = doom_weapon_toggles;
   demo_header.local_options.autoaim = autoaim;
   demo_header.local_options.weapon_speed = weapon_speed;
   time(&demo_header.timestamp);
   demo_header.length = 0;
   memset(demo_header.map_name, 0, 9);
   strncpy(demo_header.map_name, map->name, 8);
   demo_header.resource_count = map->resource_count + 1;

   if(!write_to_demo(&demo_header, sizeof(demo_header_t), 1))
      return false;

   resource_name_size = strlen(cs_resources[0].name) + 1;
   if(!write_to_demo(&resource_name_size, sizeof(size_t), 1))
      return false;
   if(!write_to_demo(cs_resources[0].name, sizeof(char), resource_name_size))
      return false;
   if(!write_to_demo(&cs_resources[0].type, sizeof(resource_type_t), 1))
      return false;
   if(!write_to_demo(&cs_resources[0].sha1_hash, sizeof(char), 41))
      return false;

   for(i = 0; i < map->resource_count; i++)
   {
      resource_index = map->resource_indices[i];
      res = &cs_resources[resource_index];
      resource_name_size = strlen(res->name) + 1;
      if(!write_to_demo(&resource_name_size, sizeof(size_t), 1))
         return false;
      if(!write_to_demo(res->name, sizeof(char), resource_name_size))
         return false;
      if(!write_to_demo(&res->type, sizeof(resource_type_t), 1))
         return false;
      if(!write_to_demo(res->sha1_hash, sizeof(char), 41))
         return false;
   }

   if(!write_to_demo(&header_marker, sizeof(demo_marker_t), 1))
      return false;

   fflush(current_demo_data_file);

   return true;
}

bool CS_PlayDemo(char *url)
{
   char *test_folder = NULL;
   const char *cwd = M_GetCurrentFolder();

   cs_demo_playback = true;

   if(cwd == NULL)
   {
      set_demo_error(DEMO_ERROR_ERRNO);
      return false;
   }
   
   if(!check_demo_folder())
      return false;

   cs_settings =
      emalloc(clientserver_settings_t *, sizeof(clientserver_settings_t));
   cs_original_settings =
      emalloc(clientserver_settings_t *, sizeof(clientserver_settings_t));

   if(strncmp(url, "file://", 7) == 0)
   {
      // [CG] If the demo's URI is a file: URI, then we don't need to download
      //      it, so just return success here (if the file exists).
      if(!M_IsFile(url + 7))
      {
         set_demo_error(DEMO_ERROR_DEMO_NOT_FOUND);
         return false;
      }

      if(M_IsAbsolutePath(url + 7))
      {
         cs_demo_archive_path = estrdup(url + 7);
      }
      else
      {
         cs_demo_archive_path = erealloc(
            char *, cs_demo_archive_path, strlen(cwd) + strlen(url + 7) + 2
         );
         sprintf(cs_demo_archive_path, "%s/%s", cwd, url + 7);
      }
   }
   else
   {
      if(strcmp(cs_demo_folder_path, cwd))
      {
         if(!M_SetCurrentFolder(cs_demo_folder_path))
         {
            set_demo_error(DEMO_ERROR_FILE_ERRNO);
            return false;
         }
      }

      if(!retrieve_demo(url))
      {
         M_SetCurrentFolder(cwd);
         return false;
      }

      if(strcmp(cs_demo_folder_path, cwd))
      {
         if(!M_SetCurrentFolder(cwd))
         {
            set_demo_error(DEMO_ERROR_FILE_ERRNO);
            return false;
         }
      }
   }

   if(strcmp(cs_demo_folder_path, cwd))
   {
      if(!M_SetCurrentFolder(cs_demo_folder_path))
      {
         set_demo_error(DEMO_ERROR_FILE_ERRNO);
         return false;
      }
   }

   if(!open_demo(cs_demo_archive_path))
   {
      M_SetCurrentFolder(cwd);
      return false;
   }

   if(cs_maps)
      CS_ClearMaps();
   
   cs_map_count = 0;
   test_folder = ecalloc(char *, strlen(cs_demo_path) + 6, sizeof(char));
   sprintf(test_folder, "%s/%d", cs_demo_path, cs_map_count);
   while(M_IsFolder(test_folder))
   {
      cs_map_count++;
      sprintf(test_folder, "%s/%d", cs_demo_path, cs_map_count);
   }
   efree(test_folder);
   cs_maps = ecalloc(cs_map_t *, cs_map_count, sizeof(cs_map_t));

   if(strcmp(cs_demo_folder_path, cwd))
   {
      if(!M_SetCurrentFolder(cwd))
      {
         set_demo_error(DEMO_ERROR_FILE_ERRNO);
         return false;
      }
   }

   cs_current_demo_map = 0;

   if(!load_current_map_demo())
      return false;

   return true;
}

bool CS_LoadDemoMap(unsigned int map_number)
{
   if(map_number > cs_map_count)
   {
      set_demo_error(DEMO_ERROR_INVALID_MAP_NUMBER);
      return false;
   }

   if(current_demo_data_file != NULL)
   {
      if(!close_current_map_demo())
         return false;
   }

   cs_current_demo_map = cs_current_map_number = map_number;

   if(!load_current_map_demo())
      return false;

   return true;
}

bool CS_LoadPreviousDemoMap(void)
{
   bool res = CS_LoadDemoMap(cs_current_demo_map - 1);

   if(res == false && demo_error_code == DEMO_ERROR_INVALID_MAP_NUMBER)
      set_demo_error(DEMO_ERROR_NO_PREVIOUS_MAP);

   return res;
}

bool CS_LoadNextDemoMap(void)
{
   bool res = CS_LoadDemoMap(cs_current_demo_map + 1);

   if(res == false && demo_error_code == DEMO_ERROR_INVALID_MAP_NUMBER)
      set_demo_error(DEMO_ERROR_NO_NEXT_MAP);

   return res;
}

bool CS_WriteNetworkMessageToDemo(void *network_message,
                                     size_t message_size,
                                     int playernum)
{
   demo_marker_t demo_marker = DEMO_PACKET_NETWORK_MESSAGE;
   int player_number = playernum;

   if(!write_to_demo(&demo_marker, sizeof(int32_t), 1))
      return false;

   if(CS_CLIENT)
      player_number = 0;

   if(!write_to_demo(&player_number, sizeof(int32_t), 1))
      return false;

   if(!write_to_demo(&message_size, sizeof(size_t), 1))
      return false;

   if(!write_to_demo(network_message, sizeof(char), message_size))
      return false;

   return true;
}

bool CS_WritePlayerCommandToDemo(cs_cmd_t *player_command)
{
   demo_marker_t demo_marker = DEMO_PACKET_PLAYER_COMMAND;
   size_t command_size = sizeof(cs_cmd_t);

   if(!write_to_demo(&demo_marker, sizeof(int32_t), 1))
      return false;

   if(!write_to_demo(&command_size, sizeof(size_t), 1))
      return false;

   if(!write_to_demo(player_command, sizeof(cs_cmd_t), 1))
      return false;

   return true;
}

bool CS_WriteConsoleCommandToDemo(int cmdtype, command_t *command,
                                     const char *options, int cmdsrc)
{
   demo_marker_t demo_marker = DEMO_PACKET_CONSOLE_COMMAND;
   size_t command_size = strlen(command->name) + 1;
   size_t options_size = strlen(options) + 1;

   if(!write_to_demo(&demo_marker, sizeof(int32_t), 1))
      return false;

   if(!write_to_demo(&cmdtype, sizeof(int32_t), 1))
      return false;

   if(!write_to_demo(&cmdsrc, sizeof(int32_t), 1))
      return false;

   if(!write_to_demo(&command_size, sizeof(size_t), 1))
      return false;

   if(!write_to_demo((void *)command->name, sizeof(char), command_size))
      return false;

   if(!write_to_demo(&options_size, sizeof(size_t), 1))
      return false;

   if(!write_to_demo((void *)options, sizeof(char), options_size))
      return false;

   return true;
}

bool CS_ReadDemoPacket(void)
{
   demo_marker_t type;

   if(!read_from_demo((void *)&type, sizeof(int32_t), 1))
      return false;

   switch(type)
   {
   case DEMO_PACKET_NETWORK_MESSAGE:
      return handle_network_message_demo_packet();
   case DEMO_PACKET_PLAYER_COMMAND:
      return handle_player_command_demo_packet();
   case DEMO_PACKET_CONSOLE_COMMAND:
      return handle_console_command_demo_packet();
   case DEMO_PACKET_HEADER_END:
   default:
      set_demo_error(DEMO_ERROR_UNKNOWN_DEMO_PACKET_TYPE);
      return false;
   }
}

bool CS_DemoFinished(void)
{
   if(!current_demo_data_file)
      return true;

   if(feof(current_demo_data_file) != 0)
      return true;

   return false;
}

bool CS_StopDemo(void)
{
   // [CG] Check if demo is already stopped.
   if(current_demo_data_file == NULL)
      return true;

   if(cs_demo_recording)
   {
      cs_demo_recording = false;

      if(!close_current_demo())
         return false;
   }
   else if(cs_demo_playback)
   {
      if(!close_current_demo_data_file())
         return false;

      if(!M_DeleteFolderAndContents(cs_demo_path))
      {
         set_demo_error(DEMO_ERROR_FILE_ERRNO);
         return false;
      }
   }
   else
   {
      return true;
   }

   if(cs_demo_path)
   {
      efree(cs_demo_path);
      cs_demo_path = NULL;
   }
   if(cs_demo_info_path)
   {
      efree(cs_demo_info_path);
      cs_demo_info_path = NULL;
   }
   cs_current_demo_map = 0;

   printf("CS_StopDemo: Demo stopped.\n");
   return true;
}

