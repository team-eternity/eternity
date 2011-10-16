// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3: 
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

#ifndef __CS_NETID_H__
#define __CS_NETID_H__

#include <map>
#include <queue>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#include "i_system.h"

class Mobj;
#if 0
class CeilingThinker;
class VerticalDoorThinker;
class FloorMoveThinker;
class ElevatorThinker;
class PillarThinker;
class FloorWaggleThinker;
class PlatThinker;
#endif

template <class T> class NetIDLookup
{
private:
   uint32_t netids_assigned;
   std::queue<uint32_t> netid_queue;
   std::map<uint32_t, T*> netid_map;

public:
   NetIDLookup()
   {
      netids_assigned = 0;
   }

   void add(T *x)
   {
      uint32_t net_id;

      if(x->net_id != 0)
      {
         if(netid_map.count(x->net_id))
            I_Error("Net ID %u has multiple owners, exiting.\n", x->net_id);
         net_id = x->net_id;
      }
      else if(!netid_queue.empty())
      {
         net_id = netid_queue.front();
         netid_queue.pop();
      }
      else
      {
         net_id = netids_assigned + 1;
      }

      netid_map[net_id] = x;
      x->net_id = net_id;
      netids_assigned++;
   }

   void remove(T *x)
   {
      if(x->net_id != 0)
      {
         netid_queue.push(x->net_id);
         netid_map.erase(x->net_id);
         x->net_id = 0;
         netids_assigned--;
      }
   }

   T* lookup(uint32_t net_id)
   {
      typename std::map<uint32_t, T*>::iterator it = netid_map.find(net_id);

      if(it == netid_map.end())
         return NULL;

      return it->second;
   }

   void clear(void)
   {
      netids_assigned = 0;
      while(netid_queue.size())
         netid_queue.pop();
      netid_map.clear();
   }

   typename std::map<uint32_t, T*>::iterator begin(void)
   {
      return netid_map.begin();
   }

   typename std::map<uint32_t, T*>::iterator end(void)
   {
      return netid_map.end();
   }

};

extern NetIDLookup<Mobj>                NetActors;
#if 0
extern NetIDLookup<CeilingThinker>      NetCeilings;
extern NetIDLookup<VerticalDoorThinker> NetDoors;
extern NetIDLookup<FloorMoveThinker>    NetFloors;
extern NetIDLookup<ElevatorThinker>     NetElevators;
extern NetIDLookup<PillarThinker>       NetPillars;
extern NetIDLookup<FloorWaggleThinker>  NetFloorWaggles;
extern NetIDLookup<PlatThinker>         NetPlatforms;
#endif

void CS_ClearNetIDs(void);

#endif

