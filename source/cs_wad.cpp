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
// C/S resource (WAD, DEH, etc.) routines.
//
// By Charles Gunyon
//
//----------------------------------------------------------------------------

#ifdef _MSC_VER
#include "Win32/i_fnames.h"
#include "Win32/i_opndir.h"
#else
#include <dirent.h>
#endif

#include <list>

#include "z_zone.h"
#include "c_io.h"
#include "doomstat.h"
#include "doomtype.h"
#include "d_dehtbl.h"
#include "d_iwad.h"
#include "d_main.h"
#include "g_game.h"
#include "i_system.h"
#include "i_thread.h"
#include "m_file.h"
#include "r_data.h"
#include "r_main.h"
#include "s_sound.h"
#include "w_wad.h"

#include <curl/curl.h>

#include "cs_main.h"
#include "cs_demo.h"
#include "cs_config.h"
#include "cs_wad.h"
#include "cl_buf.h"

extern wfileadd_t *wadfiles;
extern int texturecount;

const char *cs_iwad = NULL;
char *cs_wad_repository = NULL;
cs_map_t *cs_maps = NULL;
unsigned int cs_map_count = 0;
cs_resource_t *cs_resources = NULL;
unsigned int cs_resource_count = 0;
unsigned int cs_current_map_index = 0;

void IdentifyVersion(void);

static cs_map_t* get_current_map(void)
{
   if(CS_DEMOPLAY)
      return &cs_maps[cs_demo->getCurrentDemoIndex()];

   return &cs_maps[cs_current_map_index];
}

static int console_progress(void *clientp, double dltotal, double dlnow,
                                           double ultotal, double ulnow)
{
   static int last_percent = 0;
   const char *wad_name = (const char *)clientp;
   double fraction_downloaded = (dlnow / dltotal) * 100.0;
   unsigned short percent_downloaded;
   int divisor;
   const char *si_unit;

   if(dlnow < 1.0 || dltotal < 1.0)
      fraction_downloaded = 0.0;

   percent_downloaded = (int)fraction_downloaded;

   if(dltotal <= 1048576)
   {
      divisor = 1024;
      si_unit = "K";
   }
   else
   {
      divisor = 1048576;
      si_unit = "M";
   }

   if(((percent_downloaded % 5) == 0) && percent_downloaded != last_percent)
   {
      C_Printf(
         "  %s: %.2f%s / %.2f%s\n",
         wad_name,
         dlnow / divisor,
         si_unit,
         dltotal / divisor,
         si_unit
      );
   }
   // C_Printf("Downloading %s: %f%%\n", wad_name, fraction_downloaded);
   I_StartTic();
   D_ProcessEvents();
   C_Update();
   C_Ticker();

   return 0;
}

static bool need_new_wad_dir(void)
{
   cs_map_t *map = get_current_map();
   cs_resource_t *resource = NULL;
   unsigned int wads_loaded = 0;
   unsigned int i, j;

   while(wadfiles[wads_loaded].filename != NULL)
      wads_loaded++;

   if((map->resource_count + 2) != wads_loaded)
      return true;

   for(i = 0, j = 2; i < map->resource_count; i++, j++)
   {
      resource = &cs_resources[map->resource_indices[i]];

      if(strcmp(M_Basename(wadfiles[j].filename), resource->name))
         return true;
   }

   return false;
}

void CS_ClearMaps(void)
{
   unsigned int i;

   for(i = 0; i < cs_map_count; i++)
   {
      efree(cs_maps[i].name);
      efree(cs_maps[i].resource_indices);
   }
   efree(cs_maps);
   cs_maps = NULL;
}

bool CS_CheckWADOverHTTP(const char *wad_name)
{
   size_t wad_name_size = strlen(wad_name);
   size_t wad_repository_size = strlen(cs_wad_repository);
   char *url =
      ecalloc(char *, wad_name_size = wad_repository_size + 2, sizeof(char));
   char *wad_path =
      ecalloc(char *, strlen(userpath) + wad_name_size + 7, sizeof(char));
   CURL *curl_handle;
   CURLcode res;
   long status_code;

   sprintf(url, "%s/%s", cs_wad_repository, wad_name);
   sprintf(wad_path, "%s/wads/%s", userpath, wad_name);

   curl_handle = curl_easy_init();
   if(!curl_handle)
   {
      C_Printf("Error initializing curl.\n");
      return false;
   }

   curl_easy_setopt(curl_handle, CURLOPT_URL, url);
   curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1);
   curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1);

   if((res = curl_easy_perform(curl_handle)) != 0)
   {
      C_Printf("Error checking for WAD: %s.\n", curl_easy_strerror(res));
      return false;
   }

   curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &status_code);

   while(status_code == 0)
   {
      I_Sleep(1);
      curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &status_code);
   }

   curl_easy_cleanup(curl_handle);

   if(status_code == 200)
   {
      C_Printf("Found %s at %s.\n", wad_name, cs_wad_repository);
      return true;
   }

   return false;
}

char* CS_DownloadWAD(const char *wad_name)
{
   size_t wad_name_size = strlen(wad_name);
   size_t wad_repository_size = strlen(cs_wad_repository);
   char *url =
      ecalloc(char *, wad_name_size + wad_repository_size + 2, sizeof(char));
   char *wad_path =
      ecalloc(char *, strlen(userpath) + wad_name_size + 7, sizeof(char));
   char *temp_path =
      ecalloc(char *, strlen(userpath) + wad_name_size + 13, sizeof(char));
   CURL *curl_handle;
   CURLcode res;
   FILE *fobj;

   sprintf(url, "%s/%s", cs_wad_repository, wad_name);
   sprintf(wad_path, "%s/wads/%s", userpath, wad_name);
   sprintf(temp_path, "%s/wads/%s.temp", userpath, wad_name);

   if(!M_CreateFile(temp_path))
   {
      printf("Error creating PWAD file on disk %s.\n", temp_path);
      printf("  Error: %s.\n", M_GetFileSystemErrorMessage());
      C_Printf("Error creating PWAD file on disk.\n");
      efree(url);
      efree(wad_path);
      efree(temp_path);
      return NULL;
   }

   if((fobj = fopen(temp_path, "wb")) == NULL)
   {
      printf("Error opening PWAD file for writing.\n");
      C_Printf("Error opening PWAD file for writing.\n");
      efree(url);
      efree(wad_path);
      efree(temp_path);
      return NULL;
   }

   curl_handle = curl_easy_init();
   if(!curl_handle)
   {
      fclose(fobj);
      printf("Error initializing curl.\n");
      C_Printf("Error initializing curl.\n");
      efree(url);
      efree(wad_path);
      efree(temp_path);
      return NULL;
   }

   curl_easy_setopt(curl_handle, CURLOPT_URL, url);
   curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fwrite);
   curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fobj);
   curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0);
   curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, (void *)wad_name);
   curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, console_progress);

   C_Printf("Downloading %s...\n", wad_name);
   C_Update();

   if((res = curl_easy_perform(curl_handle)) != 0)
   {
      fclose(fobj);
      printf("Error downloading WAD: %s.\n", curl_easy_strerror(res));
      C_Printf("Error downloading WAD: %s.\n", curl_easy_strerror(res));
      efree(url);
      efree(wad_path);
      efree(temp_path);
      return NULL;
   }

   fclose(fobj);
   curl_easy_cleanup(curl_handle);
   if(!M_RenamePath((const char *)temp_path, (const char *)wad_path))
   {
      printf("Error downloading WAD: %s.\n", M_GetFileSystemErrorMessage());
      C_Printf("Error downloading WAD: %s.\n", M_GetFileSystemErrorMessage());
      efree(url);
      efree(wad_path);
      efree(temp_path);
      return NULL;
   }
   efree(url);
   efree(temp_path);
   return wad_path;
}

void CS_ClearTempWADDownloads(void)
{
   DIR *folder;
   dirent *entry;
   size_t entry_length;
   char *wads_folder_path =
      ecalloc(char *, strlen(userpath) + 7, sizeof(char));

   sprintf(wads_folder_path, "%s/wads", userpath);

   if(!(folder = opendir(wads_folder_path)))
   {
      I_Error(
         "Unable to open WAD download folder %s, exiting.\n", cs_wad_repository
      );
   }

   while((entry = readdir(folder)))
   {
      if(!M_IsFileInFolder(wads_folder_path, entry->d_name))
         continue;

      entry_length = strlen(entry->d_name);

      if(!strcasecmp(entry->d_name + (entry_length - 5), ".temp"))
      {
         if(!M_DeleteFileInFolder(wads_folder_path, entry->d_name))
         {
            printf(
               "Error deleting partial WAD %s (%s), skipping.\n",
               entry->d_name,
               M_GetFileSystemErrorMessage()
            );
         }
      }
   }

   closedir(folder);
   efree(wads_folder_path);
}

bool CS_AddIWAD(const char *resource_name)
{
   char *resource_path = D_FindWADByName((char *)resource_name);

   if(resource_path == NULL)
      return false;

   cs_iwad = resource_path;

   cs_resources = erealloc(
      cs_resource_t *,
      cs_resources,
      sizeof(cs_resource_t) * ++cs_resource_count
   );
   cs_resources[cs_resource_count - 1].name = estrdup(resource_name);
   cs_resources[cs_resource_count - 1].type = rt_iwad;
   cs_resources[cs_resource_count - 1].path = estrdup(resource_path);
   strncpy(
      cs_resources[cs_resource_count - 1].sha1_hash,
      CS_GetSHA1HashFile((char *)cs_iwad),
      41
   );

   return true;
}

bool CS_AddWAD(const char *resource_name)
{
   bool do_strdup = true;
   char *resource_path = D_FindWADByName((char *)resource_name);

   if(resource_path == NULL)
   {
      if(CS_SERVER)
         return false;

      if((resource_path = CS_DownloadWAD(resource_name)) == NULL)
         return false;

      do_strdup = false;
   }

   cs_resources = erealloc(
      cs_resource_t *,
      cs_resources,
      sizeof(cs_resource_t) * ++cs_resource_count
   );
   cs_resources[cs_resource_count - 1].name = estrdup(resource_name);

   cs_resources[cs_resource_count - 1].type = rt_pwad;

   if(do_strdup)
      cs_resources[cs_resource_count - 1].path = estrdup(resource_path);
   else
      cs_resources[cs_resource_count - 1].path = resource_path;

   strncpy(
      cs_resources[cs_resource_count - 1].sha1_hash,
      CS_GetSHA1HashFile(resource_path),
      41
   );

   return true;
}

bool CS_AddDeHackEdFile(const char *resource_name)
{
   char *resource_path = D_FindWADByName((char *)resource_name);

   if(resource_path == NULL)
      return false;

   D_QueueDEH(resource_path, 0);
   cs_resources = erealloc(
      cs_resource_t *,
      cs_resources,
      sizeof(cs_resource_t) * ++cs_resource_count
   );
   cs_resources[cs_resource_count - 1].name = estrdup(resource_name);
   cs_resources[cs_resource_count - 1].path = estrdup(resource_path);
   cs_resources[cs_resource_count - 1].type = rt_deh;
   strncpy(
      cs_resources[cs_resource_count - 1].sha1_hash,
      CS_GetSHA1HashFile(resource_path),
      41
   );

   return true;
}

cs_resource_t* CS_GetResource(const char *resource_name)
{
   cs_resource_t *res = NULL;
   size_t name_length = strlen(resource_name);
   int resource_count = cs_resource_count;

   for(res = cs_resources; (res - cs_resources) < resource_count; res++)
   {
      if(strncmp(resource_name, res->name, name_length) == 0)
         return res;
   }
   return NULL;
}

bool CS_CheckResourceHash(const char *resource_name, const char *sha1_hash)
{
   cs_resource_t *res = CS_GetResource(resource_name);

   if(res == NULL)
      I_Error("Error: resource %s not found, exiting.\n", resource_name);

   if(strncmp(sha1_hash, res->sha1_hash, 41) == 0)
      return true;
   else
      return false;
}

void CS_AddMapAtIndex(const char *name, unsigned int resource_count,
                                        unsigned int *resource_indices,
                                        unsigned int index)
{
   if(index >= cs_map_count)
   {
      I_Error(
         "CS_AddDemoMap: Index exceeds map count (%d/%d), exiting.\n",
         index,
         cs_map_count
      );
   }
   cs_maps[index].name = estrdup(name);
   cs_maps[index].initialized = true;
   cs_maps[index].resource_count = resource_count;
   cs_maps[index].resource_indices = resource_indices;
}

void CS_AddMap(const char *name, unsigned int resource_count,
                                 unsigned int *resource_indices)
{
   cs_maps = erealloc(cs_map_t *, cs_maps, sizeof(cs_map_t) * ++cs_map_count);
   CS_AddMapAtIndex(name, resource_count, resource_indices, cs_map_count - 1);
}

bool CS_LoadMap(void)
{
   unsigned int i, resource_index;
   cs_map_t *map = get_current_map();
   // static unsigned int map_change_count = 0;

   // [CG] Check first that we actually need to reload everything, if not, we
   //      don't need to do any of this.
   if(!need_new_wad_dir())
      return false;

   // [CG] This may take a while, and the client might timeout as a result, so
   //      service the network in an independent thread (that buffers all
   //      received messages) until we're finished here.
   if(CS_CLIENT)
      cl_packet_buffer.startBufferingIndependently();

   // Z_LogPrintf("=== Start Logging (%u) ===\n", ++map_change_count);

   S_StopMusic();
   wGlobalDir.Clear();
   Z_FreeTags(PU_CACHE, PU_CACHE);
   D_ClearFiles();
   IdentifyVersion();

   if(map->resource_count > 0)
   {
      for(i = 0; i < map->resource_count; i++)
      {
         resource_index = map->resource_indices[i];
         D_AddFile(
            cs_resources[resource_index].path,
            lumpinfo_t::ns_global,
            NULL, 0, 0
         );
      }
   }
   wGlobalDir.InitMultipleFiles(wadfiles);
   D_ReInitWadfiles();

   if(CS_CLIENT)
      cl_packet_buffer.stopBufferingIndependently();

   return true;
}

void CS_NewGame(void)
{
   cs_map_t *map = get_current_map();

   CS_LoadMap();
   G_DeferedInitNew((skill_t)cs_settings->skill, map->name);
}

void CS_InitNew(void)
{
   skill_t skill = (skill_t)cs_settings->skill;

   CS_LoadMap();

   if(CS_DEMOPLAY)
      G_InitNew(skill, cs_maps[cs_demo->getCurrentDemoIndex()].name);
   else
      G_InitNew(skill, cs_maps[cs_current_map_index].name);
}

