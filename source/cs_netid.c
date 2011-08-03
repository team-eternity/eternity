// Emacs style mode select -*- C -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   C/S Network ID assignment.
//
//-----------------------------------------------------------------------------

#include "c_io.h"
#include "p_mobj.h"
#include "i_system.h"

#include "cs_netid.h"

// [CG] Oh templates, donde estan ustedes?!?!

#define DEFINE_NETID_LOOKUP(type, name, funcname) \
   ehash_t *name##_by_netid = NULL;\
   static const void* EHashKeyFunc##funcname##NetID(void *object) {\
      return &(((name##_netid_t *)object)->name->net_id);\
   }\
   void CS_Obtain##funcname##NetID(type *name) {\
      name##_netid_t *netid;\
      unsigned int id = 0;\
      /* [CG] Valid NetIDs start at 1, so pre-increment is not a mistake */ \
      while(++id < name##_by_netid->numchains) {\
         if((name##_netid_t *)E_HashObjectForKey(name##_by_netid, &id) == NULL)\
            break;\
         if(id >= NETID_MAX)\
            I_Error("Exceeded maximum number of network IDs!\n");\
      }\
      if(id == name##_by_netid->numchains ||\
         name##_by_netid->loadfactor > NETID_MAX_LOAD_FACTOR)\
         E_HashRebuild(name##_by_netid, name##_by_netid->numchains * 2);\
      name->net_id = id;\
      netid = Z_Calloc(1, sizeof(name##_netid_t), PU_LEVEL, NULL);\
      netid->name = name;\
      E_HashAddObject(name##_by_netid, netid);\
   }\
   void CS_Release##funcname##NetID(type *name) {\
      name##_netid_t *netid;\
      if(name->net_id == 0)\
         return;\
      netid = E_HashObjectForKey(name##_by_netid, &name->net_id);\
      if(netid == NULL)\
         return;\
      E_HashRemoveObject(name##_by_netid, netid);\
      free(netid);\
      name->net_id = 0;\
   }\
   void CS_Register##funcname##NetID(type *name) {\
      name##_netid_t *netid = E_HashObjectForKey(\
         name##_by_netid, &name->net_id\
      );\
      if(netid != NULL) {\
         if(netid->name != name) {\
            I_Error(\
               "CS_Register" #funcname "NetID: Network ID %d has "\
               "multiple owners, exiting.\n",\
               name->net_id\
            );\
         }\
      }\
      else {\
         netid = Z_Calloc(1, sizeof(name##_netid_t), PU_LEVEL, NULL);\
         netid->name = name;\
         E_HashAddObject(name##_by_netid, netid);\
      }\
   }\
   type* CS_Get##funcname##FromNetID(unsigned int net_id) {\
      name##_netid_t *netid = E_HashObjectForKey(name##_by_netid, &net_id);\
      if(netid == NULL)\
         return NULL;\
      return netid->name;\
   }\
   void CS_Reset##funcname##NetIDs(void) {\
      void *current_netid = NULL;\
      void *next_netid = NULL;\
      current_netid = E_HashTableIterator(name##_by_netid, current_netid);\
      while(current_netid != NULL) {\
         next_netid = E_HashTableIterator(name##_by_netid, current_netid);\
         E_HashRemoveObject(name##_by_netid, current_netid);\
         ((name##_netid_t *)current_netid)->name->net_id = 0;\
         free(current_netid);\
         current_netid = next_netid;\
      }\
      /* E_HashDestroy(name##_by_netid); free(name##_by_netid); */ \
   }\
   name##_netid_t* CS_Iterate##funcname##s(name##_netid_t *node) {\
      return (name##_netid_t *)E_HashTableIterator(name##_by_netid, node);\
   }


#define INIT_NETID_LOOKUP(name, funcname) \
   name##_by_netid = calloc(1, sizeof(ehash_t));\
   E_UintHashInit(\
     name##_by_netid, NETID_INIT_CHAINS, EHashKeyFunc##funcname##NetID, NULL\
   )

#define RESET_NETID_LOOKUP(funcname) CS_Reset##funcname##NetIDs()

DEFINE_NETID_LOOKUP(mobj_t, actor, Actor);
DEFINE_NETID_LOOKUP(ceiling_t, ceiling, Ceiling);
DEFINE_NETID_LOOKUP(vldoor_t, door, Door);
DEFINE_NETID_LOOKUP(floormove_t, floor, Floor);
DEFINE_NETID_LOOKUP(elevator_t, elevator, Elevator);
DEFINE_NETID_LOOKUP(pillar_t, pillar, Pillar);
DEFINE_NETID_LOOKUP(floorwaggle_t, floorwaggle, FloorWaggle);
DEFINE_NETID_LOOKUP(plat_t, platform, Platform);

void CS_InitNetIDs(void)
{
   INIT_NETID_LOOKUP(actor, Actor);
   INIT_NETID_LOOKUP(ceiling, Ceiling);
   INIT_NETID_LOOKUP(door, Door);
   INIT_NETID_LOOKUP(floor, Floor);
   INIT_NETID_LOOKUP(elevator, Elevator);
   INIT_NETID_LOOKUP(pillar, Pillar);
   INIT_NETID_LOOKUP(floorwaggle, FloorWaggle);
   INIT_NETID_LOOKUP(platform, Platform);
}

void CS_ResetNetIDs(void)
{
   RESET_NETID_LOOKUP(Actor);
   RESET_NETID_LOOKUP(Ceiling);
   RESET_NETID_LOOKUP(Door);
   RESET_NETID_LOOKUP(Floor);
   RESET_NETID_LOOKUP(Elevator);
   RESET_NETID_LOOKUP(Pillar);
   RESET_NETID_LOOKUP(FloorWaggle);
   RESET_NETID_LOOKUP(Platform);

   // CS_InitNetIDs();
}

