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
//   Routines for sending/receiving data to/from the master.
//
//----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "doomtype.h"
#include "i_system.h"

#include <json/json.h>
#include <curl/curl.h>

#include "cs_main.h"
#include "cs_master.h"
#include "cs_config.h"
#include "cl_main.h"
#include "sv_main.h"

static bool advertised;

cs_master_t *master_servers;
int master_server_count;
Json::Value cs_server_config;

cs_master_request_t *cs_master_requests;
CURLM *cs_master_multi_handle;

extern char gamemapname[9];

static char* sys_strdup(const char *str)
{
   size_t length = strlen(str) + 1;
   char *out = (char *)(Z_SysMalloc(length * sizeof(char)));

   if(out != NULL)
      strncpy(out, str, length);

   return out;
}

void CS_InitCurl(void)
{
   CURLcode curl_errno;

   curl_errno = curl_global_init_mem(
      CURL_GLOBAL_NOTHING,
      Z_SysMalloc,
      Z_SysFree,
      Z_SysRealloc,
      sys_strdup,
      Z_SysCalloc
   );

   if(curl_errno)
      I_Error("CS_InitCurl: %s\n", curl_easy_strerror(curl_errno));

   if(CS_SERVER)
      advertised = false;
}

// [CG] For atexit().
void SV_MasterCleanup(void)
{
   int i;
   cs_master_request_t *request;

   if(!advertised)
      return;

   SV_MasterDelist();

   for(i = 0; i < master_server_count; i++)
   {
      request = &cs_master_requests[i];
      free(request->url);
      if(request->curl_handle != NULL)
      {
         curl_multi_remove_handle(
            cs_master_multi_handle, request->curl_handle
         );
         // curl_easy_cleanup(request->curl_handle);
      }
   }
   // curl_multi_cleanup(cs_master_multi_handle);
}

void SV_MultiInit(void)
{
   int i;
   cs_master_t *master;
   cs_master_request_t *request;
   char *url, *address, *group, *name;

   cs_master_requests = (cs_master_request_t *)(calloc(
      master_server_count, sizeof(cs_master_request_t)
   ));

   for(i = 0; i < master_server_count; i++)
   {
      master = &master_servers[i];
      request = &cs_master_requests[i];
      address = group = name = NULL;

      address = curl_easy_escape(NULL, master->address, 0);
      group = curl_easy_escape(NULL, master->group, 0);
      name = curl_easy_escape(NULL, master->name, 0);
      // [CG] "http://master.totaltrash.org/servers/totaltrash/Duel%201"
      //      "http://" +  "servers" +  "/" x 3 + '\0' is 18 characters.
      url = (char *)(calloc(
         strlen(address) + strlen(group) + strlen(name) + 18,
         sizeof(char)
      ));
      sprintf(url, "http://%s/servers/%s/%s", address, group, name);
      curl_free(address);
      curl_free(group);
      curl_free(name);

      request->url = url;
      request->user_agent = SV_GetUserAgent();
      request->curl_handle = NULL;
      request->headers = NULL;
      request->data_to_send = NULL;
      request->received_data = NULL;
   }

   if((cs_master_multi_handle = curl_multi_init()) == NULL)
      I_Error("Could not initialize CURL.\n");
}

static size_t CS_ReceiveHTTPData(void *p, size_t size, size_t nmemb, void *d)
{
   size_t max_size = size * nmemb;
   cs_master_request_t *request = (cs_master_request_t *)d;

   // [CG] For debugging.
   // printf("Receiving HTTP data: %d/%d.\n", size, nmemb);

   request->received_data = (char *)(realloc(
      request->received_data, request->data_transferred + max_size + 1
   ));
   memcpy(&(request->received_data[request->data_transferred]), p, max_size);
   request->data_transferred += max_size;
   request->received_data[request->data_transferred] = 0;

   return max_size;
}

static size_t CS_SendHTTPData(void *p, size_t size, size_t nmemb, void *s)
{
   int amount_to_send;
   size_t max_size = size * nmemb;
   cs_master_request_t *request = (cs_master_request_t *)s;


   if(max_size < request->data_remaining)
      amount_to_send = max_size;
   else
      amount_to_send = request->data_remaining;

   memcpy(
      p, request->data_to_send + request->data_transferred, amount_to_send
   );

   /* [CG] For debugging.
   printf("CS_SendHTTPData: Data to send: %s\n", request->data_to_send);
   printf("                 Sent size:    %d\n", amount_to_send);
   printf("                 Transferred:  %d\n", request->data_transferred);
   printf("                 Remaining:    %d\n", request->data_remaining);
   printf("                 Chunk size:   %d\n", size);
   printf("                 Chunk count   %d\n", nmemb);
   printf("                 Max size:     %d\n", max_size);
   */

   request->data_transferred += amount_to_send;
   request->data_remaining -= amount_to_send;

   return amount_to_send;
}

cs_master_request_t* CS_BuildMasterRequest(cs_master_request_t *request,
                                           int method)
{
   request->finished = false;

   // [CG] This will be realloc'd later.
   request->received_data = (char *)(calloc(1, 1));

   if(strncmp(request->url, "http", 4) == 0)
      request->is_http = true;
   else
      request->is_http = false;

   if((request->curl_handle = curl_easy_init()) == NULL)
      I_Error("Unknown error initializing CURL.\n");

   // [CG] For debugging.
   // curl_easy_setopt(request->curl_handle, CURLOPT_VERBOSE, 1L);

   curl_easy_setopt(request->curl_handle, CURLOPT_URL, request->url);
   curl_easy_setopt(
      request->curl_handle, CURLOPT_USERAGENT, request->user_agent
   );
   curl_easy_setopt(
      request->curl_handle, CURLOPT_WRITEFUNCTION, CS_ReceiveHTTPData
   );
   curl_easy_setopt(request->curl_handle, CURLOPT_WRITEDATA, (void *)request);
   curl_easy_setopt(
      request->curl_handle, CURLOPT_READFUNCTION, CS_SendHTTPData
   );
   curl_easy_setopt(request->curl_handle, CURLOPT_READDATA, (void *)request);
   if(method == CS_HTTP_METHOD_POST || method == CS_HTTP_METHOD_PUT)
   {
      if(method == CS_HTTP_METHOD_POST)
         curl_easy_setopt(request->curl_handle, CURLOPT_POST, 1L);
      else if(method == CS_HTTP_METHOD_PUT)
         curl_easy_setopt(request->curl_handle, CURLOPT_UPLOAD, 1L);
   }
   else if(method == CS_HTTP_METHOD_DELETE)
      curl_easy_setopt(request->curl_handle, CURLOPT_CUSTOMREQUEST, "DELETE");
   else if(method != CS_HTTP_METHOD_GET)
      I_Error("CS_BuildMasterRequest: Unsupported HTTP method\n");

   request->method = method;
   request->data_to_send = NULL;

   return request;
}

void CS_SendMasterRequest(cs_master_request_t *request)
{
   request->curl_errno = curl_easy_perform(request->curl_handle);
   if(request->curl_errno != CURLE_OK)
   {
      I_Error(
         "Error sending master request:\n%ld %s\n",
         (long int)request->curl_errno,
         curl_easy_strerror(request->curl_errno)
      );
   }
   if(request->is_http)
   {
      // [CG] CURL doesn't set the HTTP status_code until a response has been
      //      received (obviously), so just sleep here until we get it I guess.
      curl_easy_getinfo(request->curl_handle, CURLINFO_RESPONSE_CODE,
                        &request->status_code);
      while(request->status_code == 0)
      {
         I_Sleep(1);
         curl_easy_getinfo(request->curl_handle, CURLINFO_RESPONSE_CODE,
                           &request->status_code);
      }
   }
   // [CG] This goes here (and not CS_FreeMasterRequest) because it should
   //      always run at this point.
   curl_easy_cleanup(request->curl_handle);
}

void CS_FreeMasterRequest(cs_master_request_t *request)
{
   request->curl_handle = NULL;
   curl_slist_free_all(request->headers);
   request->headers = NULL;
   request->data_transferred = 0;
   request->data_remaining = 0;

   if(request->data_to_send != NULL)
   {
      free(request->data_to_send);
      request->data_to_send = NULL;
   }

   if(request->received_data != NULL)
   {
      free(request->received_data);
      request->received_data = NULL;
   }

   request->status_code = 0;
   request->curl_errno = (CURLcode)0;
   request->finished = true;
   request->is_http = false;
}

void CL_FreeMasterRequest(cs_master_request_t *request)
{
   CS_FreeMasterRequest(request);
   free(request->url);
   free(request->user_agent);
   free(request);
}

void SV_FreeMasterRequest(cs_master_request_t *request)
{
   // free(request->url);
   CS_FreeMasterRequest(request);
}

cs_master_request_t* SV_GetMasterRequest(cs_master_t *master, int method)
{
   char *auth;
   cs_master_request_t *request = &cs_master_requests[master - master_servers];

   // memset(request, 0, sizeof(cs_master_request_t));
   CS_FreeMasterRequest(request);

   CS_BuildMasterRequest(request, method);

   curl_easy_setopt(request->curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
   // [CG] ':' separator + '\0' is 2 characters.
   auth = (char *)(calloc(
      strlen(master->username) + strlen(master->password_hash) + 2,
      sizeof(char)
   ));
   sprintf(auth, "%s:%s", master->username, master->password_hash);
   curl_easy_setopt(request->curl_handle, CURLOPT_USERPWD, auth);
   free(auth);

   return request;
}

void SV_SetMasterRequestData(cs_master_request_t *request, const char *data)
{
   int i = strlen(data);

   request->data_to_send = strdup(data);

   // [CG] For debugging.
   // printf("SV_SetMasterRequestData: Data: %s.\n", request->data_to_send);

   request->data_remaining = i;
   request->headers = curl_slist_append(
      request->headers, "Content-Type: application/json"
   );

   if(request->method == CS_HTTP_METHOD_PUT)
   {
      curl_easy_setopt(
         request->curl_handle,
         CURLOPT_INFILESIZE_LARGE,
         (curl_off_t)request->data_remaining
      );
   }
   else
   {
      curl_easy_setopt(
         request->curl_handle,
         CURLOPT_POSTFIELDSIZE_LARGE,
         (curl_off_t)request->data_remaining
      );
   }
   curl_easy_setopt(request->curl_handle, CURLOPT_HTTPHEADER, request->headers);
   /* [CG] For debugging.
   printf("Data size: '%d, %d'.\n", i, request->data_remaining);
   if(request->data_remaining < 1000)
   {
      printf("Data:\n%s", request->data_to_send);
   }
   */
}

char* SV_GetStateJSON(void)
{
   unsigned int i, j, connected_clients;
   Json::Value server_json;
   Json::FastWriter writer;

   connected_clients = 0;
   for(i = 1; i < MAX_CLIENTS; i++)
   {
      if(playeringame[i])
         connected_clients++;
   }

   server_json["connected_clients"] = connected_clients;
   server_json["map"] = gamemapname;

   if(CS_TEAMS_ENABLED)
   {
      for(i = 0; i < cs_settings->number_of_teams; i++)
      {
         server_json["teams"][i]["color"] = team_color_names[i];
         server_json["teams"][i]["score"] = team_scores[i];
         for(j = 1; j < MAXPLAYERS; j++)
         {
            client_t *client = &clients[j];
            player_t *player = &players[j];

            if(!playeringame[j] && !client->team == i)
               continue;

            server_json["teams"][i]["players"][j - 1]["name"] =
               player->name;
            server_json["teams"][i]["players"][j - 1]["lag"] =
               client->transit_lag;
            server_json["teams"][i]["players"][j - 1]["packet_loss"] =
               client->packet_loss;
            server_json["teams"][i]["players"][j - 1]["frags"] =
               player->totalfrags;
            server_json["teams"][i]["players"][j - 1]["time"] =
               (gametic - client->join_tic) / TICRATE;
            server_json["teams"][i]["players"][j - 1]["playing"] =
               !client->spectating;
         }
      }
   }
   else
   {
      for(i = 1; i < MAXPLAYERS; i++)
      {
         client_t *client = &clients[i];
         player_t *player = &players[i];

         if(!playeringame[i])
            continue;

         server_json["players"][i - 1]["name"] = player->name;
         server_json["players"][i - 1]["lag"] = client->transit_lag;
         server_json["players"][i - 1]["packet_loss"] = client->packet_loss;
         server_json["players"][i - 1]["frags"] = player->totalfrags;
         server_json["players"][i - 1]["time"] =
            (gametic - client->join-tic) / TICRATE;
         server_json["players"][i - 1]["playing"] = !client->spectating;
      }
   }

   return strdup(writer.write(root).c_str());
}

void SV_MasterAdvertise(void)
{
   int i;
   cs_master_t *master;
   cs_master_request_t *request;
   std::string json_string;
   Json::Value json;
   Json::FastWriter writer;

   for(i = 0; i < master_server_count; i++)
   {
      master = master_servers + i;
      request = SV_GetMasterRequest(master, CS_HTTP_METHOD_PUT);
      json = cs_server_config;
      json["group"] = master->group;
      json["name"] = master->name;
      json_string = writer.write(json);
      SV_SetMasterRequestData(json_string.c_str());
      CS_SendMasterRequest(request);
      if(request->curl_errno != 0)
      {
         I_Error(
            "Curl error during during advertising:\n%ld %s",
            (long int)request->curl_errno,
            curl_easy_strerror(request->curl_errno)
         );
      }

      if(request->status_code == 201)
      {
         printf("Advertising on master [%s].\n", master->address);
         advertised = true;
      }
      else if(request->status_code == 301)
      {
         I_Error(
            "Server '%s' is already advertised on the master [%s].\n",
            master->name,
            master->address
         );
      }
      else if(request->status_code == 401)
      {
         I_Error(
            "Authenticating on master [%s] failed.\n", master->address
         );
      }
      else
      {
         I_Error(
            "Received unexpected HTTP status code '%ld' when advertising on "
            "master '%s'.\n",
            (long int)request->status_code,
            master->address
         );
      }
      SV_FreeMasterRequest(request);
   }
}

void SV_MasterDelist(void)
{
   int i;
   cs_master_t *master;
   cs_master_request_t *request;

   for(i = 0; i < master_server_count; i++)
   {
      master = master_servers + i;

      if(master->disabled)
         continue;

      request = SV_GetMasterRequest(master, CS_HTTP_METHOD_DELETE);
      CS_SendMasterRequest(request);
      if(request->curl_errno != 0)
      {
         I_Error(
            "Curl error during during delisting:\n%ld %s",
            (long int)request->curl_errno,
            curl_easy_strerror(request->curl_errno)
         );
      }
      if(request->status_code == 401)
      {
         I_Error(
            "Authenticating on master [%s] failed.\n", master->address
         );
      }
      else if(request->status_code != 204)
      {
         I_Error(
            "Received unexpected HTTP status code '%ld' when delisting from "
            "master [%s].\n",
            (long int)request->status_code,
            master->address
         );
      }
      printf("Delisted from master [%s].\n", master->address);
      SV_FreeMasterRequest(request);
   }
}

void SV_MasterUpdate(void)
{
   int maxfd = -1;
   int i, active_transfer_count, request_message_count;
   long curl_timeout;
   CURLMsg *request_message;
   CURLMcode multi_error;
   fd_set fdread;
   fd_set fdwrite;
   fd_set fdexcep;
   struct timeval timeout;
   cs_master_t *master;
   cs_master_request_t *request;
   unsigned int getinfo_attempts = 0;

   FD_ZERO(&fdread);
   FD_ZERO(&fdwrite);
   FD_ZERO(&fdexcep);

   timeout.tv_sec = 0;
   timeout.tv_usec = 5000;

   multi_error = curl_multi_fdset(
      cs_master_multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd
   );
   if(multi_error != CURLE_OK)
   {
      I_Error(
         "SV_MasterUpdate: Error running curl_multi_fdset:\n\t%d %s.\n",
         multi_error, curl_easy_strerror((CURLcode)multi_error)
      );
   }

   multi_error = curl_multi_timeout(cs_master_multi_handle, &curl_timeout);
   if(multi_error != CURLE_OK)
   {
      I_Error(
         "SV_MasterUpdate: Error running curl_multi_timeout:\n\t%d %s.\n",
         multi_error, curl_easy_strerror((CURLcode)multi_error)
      );
   }

   // [CG] CURLM_CALL_MULTI_PERFORM means "call curl_multi_perform() again, so
   //      loop here (shouldn't really be that long) while cURL wants us to.
   do
   {
      multi_error = curl_multi_perform(
         cs_master_multi_handle, &active_transfer_count
      );
   } while(multi_error == CURLM_CALL_MULTI_PERFORM);

   if(multi_error != CURLE_OK)
   {
      I_Error(
         "SV_MasterUpdate: Error running curl_multi_perform:\n\t%d %s.\n",
         multi_error, curl_easy_strerror((CURLcode)multi_error)
      );
   }

   while((request_message = curl_multi_info_read(
          cs_master_multi_handle, &request_message_count)))
   {
      if(request_message->msg == CURLMSG_DONE)
      {
         request = NULL;
         for(i = 0; i < master_server_count; i++)
         {
            if(request_message->easy_handle ==
               cs_master_requests[i].curl_handle)
            {
               request = &cs_master_requests[i];
               break;
            }
         }
         if(request == NULL)
         {
            I_Error(
               "SV_MasterUpdate: Couldn't match up request with curl "
               "response message, exiting.\n"
            );
         }
         master = &master_servers[request - cs_master_requests];
         master->updating = false;
         master->last_update = sv_world_index;
         request->finished = true;
         curl_easy_getinfo(
            request->curl_handle,
            CURLINFO_RESPONSE_CODE,
            &request->status_code
         );

         while(request->status_code == 0 && getinfo_attempts++ < 10)
         {
            I_Sleep(1);
            curl_easy_getinfo(
               request->curl_handle,
               CURLINFO_RESPONSE_CODE,
               &request->status_code
            );
         }

         if(getinfo_attempts >= 10)
         {
            printf(
               "Timed out waiting for HTTP status code from master [%s].\n",
               master->address
            );
         }
         else if(request->curl_errno != 0)
         {
            printf(
               "Curl error while updating master [%s]:\n%ld %s",
               master->address,
               (long int)request->curl_errno,
               curl_easy_strerror(request->curl_errno)
            );
         }
         else
         {
            switch(request->status_code)
            {
            case 200:
               break;
            case 401:
               printf(
                  "Authentication failed, removing master [%s] from "
                  "update list.\n",
                  master->address
               );
               master->disabled = true;
               break;
            case 408:
               // [CG] Maybe put a limit on master timeouts instead of
               //      just removing on the first one.
               printf(
                  "Master [%s] timed out, removing from update list.\n",
                  master->address
               );
               master->disabled = true;
               break;
            default:
               printf(
                  "Master [%s] responded with unexpected HTTP status "
                  "code '%ld', removing from update list.\n",
                  master->address,
                  (long int)request->status_code
               );
               master->disabled = true;
               break;
            }
         }
         curl_multi_remove_handle(
            cs_master_multi_handle, request->curl_handle
         );
         curl_easy_cleanup(request->curl_handle);
         SV_FreeMasterRequest(request);
      }
   }

   for(i = 0; i < master_server_count; i++)
   {
      master = &master_servers[i];
      if(!master->updating &&
         !master->disabled &&
         (sv_world_index - master->last_update) > ((TICRATE * 2) + i))
      {
         // [CG] Create new requests for a master if it's not currently
         //      updating and if its last update was at least 2 seconds ago.  I
         //      also added the index of the master, so we're not defaulting
         //      to processing all masters on the same TIC.
         SV_AddUpdateRequest(master);
      }
   }
}

void SV_AddUpdateRequest(cs_master_t *master)
{
   char *json_string = SV_GetStateJSON();
   cs_master_request_t *request = SV_GetMasterRequest(
      master, CS_HTTP_METHOD_POST
   );

   // [CG] For debugging.
   // printf("Adding update request for [%s].\n", master->address);

   SV_SetMasterRequestData(request, (const char *)json_string);
   free(json_string);
   curl_multi_add_handle(cs_master_multi_handle, request->curl_handle);
   master->updating = true;
}

void CL_RetrieveServerConfig(void)
{
   std::string json_data;
   Json::Value state_and_configuration;
   Json::Reader reader;
   cs_master_request_t *request = (cs_master_request_t *)(calloc(
      1, sizeof(cs_master_request_t)
   ));

   request->url = strdup(cs_server_url);
   request->user_agent = CL_GetUserAgent();

   CS_BuildMasterRequest(request, CS_HTTP_METHOD_GET);
   CS_SendMasterRequest(request);

   if(request->is_http && request->status_code != 200)
   {
      if(request->status_code == 404)
         I_Error("Server at [%s] was not found.\n", request->url);

      I_Error(
         "Received unexpected HTTP status code '%ld' when attempting to "
         "retrieve server data from [%s].\n",
         (long int)request->status_code,
         request->url
      );
   }

   printf("success!\n");
   json_data = request->received_data;
   if(!reader.parse(json_data, cs_json))
   {
      I_Error(
         "CS_LoadConfig: Parse error:\n\t%s.\n",
         reader.getFormattedErrorMessages()
      );
   }
   CL_FreeMasterRequest(request);
}

