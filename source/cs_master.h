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

#ifndef CS_MASTER_H__
#define CS_MASTER_H__

#include "doomtype.h"

#include <curl/curl.h>

enum
{
   CS_HTTP_METHOD_GET,
   CS_HTTP_METHOD_POST,
   CS_HTTP_METHOD_PUT,
   CS_HTTP_METHOD_DELETE
};

typedef struct cs_master_s
{
   const char *address;
   unsigned int port;
   const char *username;
   const char *password_hash;
   const char *group;
   const char *name;
   int last_update;
   bool updating;
   bool disabled;
} cs_master_t;

typedef struct cs_master_request_s
{
   int method;
   char *url;
   char *user_agent;
   CURL *curl_handle;
   struct curl_slist *headers;
   size_t data_transferred;
   size_t data_remaining;
   char *data_to_send;
   char *received_data;
   long status_code;
   CURLcode curl_errno;
   bool finished;
   bool is_http;
} cs_master_request_t;

extern cs_master_t *master_servers;
extern int sv_master_server_count;
extern Json::Value cs_server_config;

// [CG] Initialization and cleanup functions.
void CS_InitCurl(bool initialize_windows_networking);
void SV_MultiInit(void);
void SV_MasterCleanup(void);

// [CG] Request creation, augmentation and destruction.
void CS_SendMasterRequest(cs_master_request_t *request);
void CS_FreeMasterRequest(cs_master_request_t *request);
cs_master_request_t* CS_BuildMasterRequest(cs_master_request_t *request,
                                           int method);

// [CG] Server-specific request functions.
cs_master_request_t* SV_GetMasterRequest(cs_master_t *master, int method);
void SV_SetMasterRequestData(cs_master_request_t *request, const char *data);
void SV_FreeMasterRequest(cs_master_request_t *request);

// [CG] Client-specific request functions.
void CL_FreeMasterRequest(cs_master_request_t *request);

// [CG] Server-specific master server functions.
char* SV_GetStateJSON(void);
void SV_MasterAdvertise(void);
void SV_MasterDelist(void);
void SV_MasterUpdate(void);
void SV_AddUpdateRequest(cs_master_t *master);

// [CG] Client-specific master server functions.
void CL_RetrieveServerConfig(void);

#endif

