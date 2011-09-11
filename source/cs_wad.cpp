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

#include <stdlib.h>
#include <string.h>

#include "c_io.h"
#include "doomtype.h"
#include "d_dehtbl.h"
#include "d_iwad.h"
#include "d_main.h"
#include "g_game.h"
#include "m_file.h"
#include "r_data.h"
#include "s_sound.h"
#include "w_wad.h"

#include <curl/curl.h>

#include "cs_main.h"
#include "cs_demo.h"
#include "cs_config.h"
#include "cs_wad.h"

extern wfileadd_t *wadfiles;
extern int texturecount;

const char *cs_iwad = NULL;
char *cs_wad_repository = NULL;
cs_map_t *cs_maps = NULL;
unsigned int cs_map_count = 0;
cs_resource_t *cs_resources = NULL;
unsigned int cs_resource_count = 0;
unsigned int cs_current_map_index = 0;
unsigned int cs_current_map_number = 0;

void IdentifyVersion(void);
void G_DeferedInitNewFromDir(skill_t skill, char *levelname, waddir_t *dir);

static cs_map_t* get_current_map(void)
{
   if(cs_demo_playback)
      return &cs_maps[cs_current_demo_map];

   return &cs_maps[cs_current_map_number];
}

static int console_progress(void *clientp, double dltotal, double dlnow,
                                           double ultotal, double ulnow)
{
   const char *wad_name = (const char *)clientp;
   double fraction_downloaded = (dlnow / dltotal) * 100.0;

   if(dlnow < 1.0 || dltotal < 1.0)
      fraction_downloaded = 0.0;

   C_Printf("Downloading %s: %f%%\n", wad_name, fraction_downloaded);
   C_Update();
   C_Ticker();
   return 0;
}

static boolean need_new_wad_dir(void)
{
   cs_map_t *map = get_current_map();
   cs_resource_t *resource = NULL;
   boolean found_mismatch = false;
   unsigned int wads_loaded = 0;
   unsigned int i, j;

   while(wadfiles[wads_loaded].filename != NULL)
      wads_loaded++;

   if(map->resource_count != (wads_loaded - 2))
      return true;

   for(i = 0, j = 2; i < map->resource_count; i++, j++)
   {
      resource = &cs_resources[map->resource_indices[i]];
      if(strcmp(M_Basename(wadfiles[j].filename), resource->name))
      {
         found_mismatch = true;
         break;
      }
   }

   return found_mismatch;
}

void CS_ClearMaps(void)
{
   unsigned int i;

   for(i = 0; i < cs_map_count; i++)
   {
      free(cs_maps[i].name);
      free(cs_maps[i].resource_indices);
   }
   free(cs_maps);
   cs_maps = NULL;
}

char* CS_DownloadWAD(const char *wad_name)
{
   size_t wad_name_size = strlen(wad_name);
   size_t wad_repository_size = strlen(cs_wad_repository);
   char *url = calloc(wad_name_size + wad_repository_size + 2, sizeof(char));
   char *wad_path = calloc(strlen(basepath) + wad_name_size + 7, sizeof(char));
   CURL *curl_handle;
   CURLcode res;
   FILE *fobj;

   sprintf(url, "%s/%s", cs_wad_repository, wad_name);
   sprintf(wad_path, "%s/wads/%s", basepath, wad_name);

   if(!M_CreateFile(wad_path))
   {
      C_Printf("Error creating PWAD file on disk.\n");
      return NULL;
   }

   if((fobj = fopen(wad_path, "wb")) == NULL)
   {
      C_Printf("Error opening PWAD file for writing.\n");
      return NULL;
   }

   curl_handle = curl_easy_init();
   if(!curl_handle)
   {
      fclose(fobj);
      C_Printf("Error initializing curl.\n");
      return NULL;
   }

   curl_easy_setopt(curl_handle, CURLOPT_URL, url);
   curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fwrite);
   curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fobj);
   curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0);
   curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, (void *)wad_name);
   curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, console_progress);

   C_Printf("Downloading %s: 0%%\n", wad_name);
   C_Update();

   if((res = curl_easy_perform(curl_handle)) != 0)
   {
      fclose(fobj);
      C_Printf("Error downloading WAD: %s.\n", curl_easy_strerror(res));
      return NULL;
   }

   fclose(fobj);
   curl_easy_cleanup(curl_handle);
   return wad_path;
}

boolean CS_AddIWAD(const char *resource_name)
{
   char *resource_path = D_FindWADByName((char *)resource_name);
   
   if(resource_path == NULL)
      return false;

   cs_iwad = resource_path;

   cs_resources = realloc(
      cs_resources, sizeof(cs_resource_t) * ++cs_resource_count
   );
   cs_resources[cs_resource_count - 1].name = strdup(resource_name);
   cs_resources[cs_resource_count - 1].type = rt_iwad;
   cs_resources[cs_resource_count - 1].path = strdup(resource_path);
   strncpy(
      cs_resources[cs_resource_count - 1].sha1_hash,
      CS_GetSHA1HashFile((char *)cs_iwad),
      41
   );

   return true;
}

boolean CS_AddWAD(const char *resource_name)
{
   boolean do_strdup = true;
   char *resource_path = D_FindWADByName((char *)resource_name);

   if(resource_path == NULL)
   {
      resource_path = CS_DownloadWAD(resource_name);
      if(resource_path == NULL)
         return false;
      do_strdup = false;
   }

   cs_resources = realloc(
      cs_resources, sizeof(cs_resource_t) * ++cs_resource_count
   );
   cs_resources[cs_resource_count - 1].name = strdup(resource_name);

   cs_resources[cs_resource_count - 1].type = rt_pwad;

   if(do_strdup)
      cs_resources[cs_resource_count - 1].path = strdup(resource_path);
   else
      cs_resources[cs_resource_count - 1].path = resource_path;

   strncpy(
      cs_resources[cs_resource_count - 1].sha1_hash,
      CS_GetSHA1HashFile(resource_path),
      41
   );

   return true;
}

boolean CS_AddDeHackEdFile(const char *resource_name)
{
   char *resource_path = D_FindWADByName((char *)resource_name);

   if(resource_path == NULL)
      return false;

   D_QueueDEH(resource_path, 0);
   cs_resources = realloc(
      cs_resources, sizeof(cs_resource_t) * ++cs_resource_count
   );
   cs_resources[cs_resource_count - 1].name = strdup(resource_name);
   cs_resources[cs_resource_count - 1].path = strdup(resource_path);
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

   for(res = cs_resources; (res - cs_resources) < cs_resource_count; res++)
   {
      if(strncmp(resource_name, res->name, name_length) == 0)
         return res;
   }
   return NULL;
}

boolean CS_CheckResourceHash(const char *resource_name, const char *sha1_hash)
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
   cs_maps[index].name = strdup(name);
   cs_maps[index].initialized = true;
   cs_maps[index].resource_count = resource_count;
   cs_maps[index].resource_indices = resource_indices;
}

void CS_AddMap(const char *name, unsigned int resource_count,
                                 unsigned int *resource_indices)
{
   cs_maps = realloc(cs_maps, sizeof(cs_map_t) * ++cs_map_count);
   CS_AddMapAtIndex(name, resource_count, resource_indices, cs_map_count - 1);
}

boolean CS_LoadMap(void)
{
   unsigned int i, resource_index;
   cs_map_t *map = get_current_map();
   static unsigned int map_change_count = 0;

   // [CG] Check first that we actually need to reload everything, if not, we
   //      don't need to do any of this.
   if(!need_new_wad_dir())
      return false;

   Z_LogPrintf("=== Start Logging (%u) ===\n", ++map_change_count);

   S_StopMusic();
   W_ClearWadDir(&w_GlobalDir);
   Z_FreeTags(PU_CACHE, PU_CACHE);
   D_ClearFiles();
   IdentifyVersion();

   if(map->resource_count > 0)
   {
      for(i = 0; i < map->resource_count; i++)
      {
         resource_index = map->resource_indices[i];
         D_AddFile(cs_resources[resource_index].path, ns_global, NULL, 0, 0);
      }
   }
   W_InitMultipleFiles(&w_GlobalDir, wadfiles);
   D_ReInitWadfiles();
   return true;
}

void CS_NewGame(void)
{
   cs_map_t *map = get_current_map();

   printf("CS_NewGame: Loading map %d.\n", cs_current_map_number);

   CS_LoadMap();
   G_DeferedInitNew(cs_settings->skill, map->name);
}

void CS_InitNew(void)
{
   printf("CS_InitNew: Loading map %d.\n", cs_current_map_number);

   CS_LoadMap();

   if(cs_demo_playback)
      G_InitNew(cs_settings->skill, cs_maps[cs_current_demo_map].name);
   else
      G_InitNew(cs_settings->skill, cs_maps[cs_current_map_number].name);
}

