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
//   Clientside routines for handling sectors and map specials.
//
//----------------------------------------------------------------------------

#ifndef __CL_SPEC_H__
#define __CL_SPEC_H__

#include "doomtype.h"
#include "m_dllist.h"
#include "e_hash.h"

#include "cs_main.h"
#include "cs_spec.h"

#define SPEC_MAX_LOAD_FACTOR 0.7

// [CG] Just guessing at the proper amount, will probably need tuning.
#define SPEC_INIT_CHAINS 8192

#define SPEC_MAX_BUFFER_SIZE MAX_POSITIONS

#define CL_FOR_ALL_SPEC_NODES(t, h, n) \
   while(((n) = (t *)E_HashTableIterator((h), (n))) != NULL)

#define CL_FOR_SPEC_NODES_AT(t, h, n, i) \
   while(((n) = (t *)E_HashObjectIterator((h), (n), &(i))) != NULL)

#define FOR_ALL_SECTOR_POSITION_NODES CL_FOR_ALL_SPEC_NODES(\
   sector_position_buffer_node_t, cl_sector_position_hash, node\
)

#define FOR_SECTOR_POSITION_NODES_AT(i) CL_FOR_SPEC_NODES_AT(\
   sector_position_buffer_node_t, cl_sector_position_hash, node, (i)\
)

#define FOR_ALL_SPECIAL_STATUS_NODES CL_FOR_ALL_SPEC_NODES(\
   ms_status_buffer_node_t, cl_ms_status_hash, node\
)

#define FOR_SPECIAL_STATUS_NODES_AT(i) CL_FOR_SPEC_NODES_AT(\
   ms_status_buffer_node_t, cl_ms_status_hash, node, (i)\
)

typedef struct sector_position_buffer_node_s
{
   mdllistitem_t link;
   uint32_t world_index;
   size_t sector_number;
   sector_position_t position;
} sector_position_buffer_node_t;

typedef struct ms_status_buffer_node_s
{
   mdllistitem_t link;
   map_special_t type;
   uint32_t world_index;
   size_t sector_number;
   void *status;
} ms_status_buffer_node_t;

typedef struct ms_removal_buffer_node_s
{
   mdllistitem_t link;
   map_special_t type;
   uint32_t world_index;
   uint32_t net_id;
} ms_removal_buffer_node_t;

extern boolean cl_predicting_sectors;
extern boolean cl_setting_sector_positions;

extern ehash_t *cl_sector_position_hash;
extern ehash_t *cl_ms_status_hash;

void CL_SpecInit(void);

void CL_BufferSectorPosition(nm_sectorposition_t *message);
void CL_BufferMapSpecialStatus(nm_specialstatus_t *message, void *special);

void CL_LoadSectorState(unsigned int index);
void CL_LoadSectorPositions(unsigned int index);
void CL_LoadCurrentSectorPositions(void);
void CL_LoadCurrentSectorState(void);

void CL_HandleMapSpecialSpawnedMessage(nm_specialspawned_t *message);
void CL_HandleMapSpecialRemovedMessage(nm_specialremoved_t *message);
void CL_ProcessSectorPositions(unsigned int index);

void CL_ClearSectorPositions(void);
void CL_ClearMapSpecialStatuses(void);
void CL_PrintSpecialStatuses(void);

#endif

