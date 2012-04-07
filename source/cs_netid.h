// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3: 
//-----------------------------------------------------------------------------
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   C/S Network ID assignment.
//
//-----------------------------------------------------------------------------

#ifndef CS_NETID_H__
#define CS_NETID_H__

#include "e_hash.h"
#include "i_system.h"
#include "m_queue.h"

#define NUM_NETID_CHAINS 251
#define MAX_NETID_LOAD_FACTOR 0.7

class Mobj;
class SectorThinker;

struct NetID
{
   mqueueitem_t item;
   uint32_t id;
};

template <typename T> struct NetIDToObject
{
   DLListItem<NetIDToObject> links;
   int id;
   T *object;
};

template <class T> class NetIDLookup
{
private:
   uint32_t netids_assigned;
   mqueue_t netid_queue;
   EHashTable<NetIDToObject<T>, EIntHashKey, &NetIDToObject<T>::id,
              &NetIDToObject<T>::links> netid_map;

public:
   NetIDLookup()
      : netids_assigned(0)
   {
      M_QueueInit(&netid_queue);
      netid_map.initialize(NUM_NETID_CHAINS);
   }

   void add(T *x)
   {
      NetIDToObject<T> *nito;

      // [CG] Get a new Net ID.  If the thing came in with a Net ID, check that
      //      it's not already in use (error if it is).  Otherwise check the
      //      queue for a recently-released Net ID.  Finally resort to creating
      //      a new one.
      if(x->net_id != 0)
      {
         if(lookup(x->net_id))
            I_Error("Net ID %u has multiple owners, exiting.\n", x->net_id);
      }
      else if(!M_QueueIsEmpty(&netid_queue))
      {
         NetID *ni = (NetID *)(M_QueuePop(&netid_queue));
         x->net_id = ni->id;
         efree(ni);
      }
      else
         x->net_id = netids_assigned + 1;

      nito = estructalloc(NetIDToObject<T>, 1);
      nito->id = x->net_id;
      nito->object = x;
      netid_map.addObject(nito);
      netids_assigned++;

      if(netid_map.getLoadFactor() > MAX_NETID_LOAD_FACTOR)
         netid_map.rebuild(netid_map.getNumChains() * 2);
   }

   void remove(T *x)
   {
      if(x->net_id != 0)
      {
         NetID *ni = estructalloc(NetID, 1);
         NetIDToObject<T> *nito = netid_map.objectForKey(x->net_id);

         ni->id = x->net_id;
         M_QueueInsert((mqueueitem_t *)(ni), &netid_queue);

         if(nito)
         {
            netid_map.removeObject(nito);
            efree(nito);
         }

         x->net_id = 0;
         netids_assigned--;
      }
   }

   T* lookup(uint32_t net_id)
   {
      NetIDToObject<T> *nito = netid_map.objectForKey(net_id);

      if(nito)
         return nito->object;

      return NULL;
   }

   void clear(void)
   {
      NetIDToObject<T> *nito = NULL;

      netids_assigned = 0;
      M_QueueFree(&netid_queue);

      while((nito = netid_map.tableIterator(NULL)))
      {
         netid_map.removeObject(nito);
         efree(nito);
      }
   }

   bool isEmpty() const
   {
      if(netids_assigned)
         return false;

      return true;
   }

   NetIDToObject<T>* iterate(NetIDToObject<T> *nito)
   {
      return netid_map.tableIterator(nito);
   }

   uint32_t getSize()
   {
      return netids_assigned;
   }

};

extern NetIDLookup<Mobj>          NetActors;
extern NetIDLookup<SectorThinker> NetSectorThinkers;
extern NetIDLookup<SectorThinker> CLNetSectorThinkers;

void CS_ClearNetIDs(void);

#endif

