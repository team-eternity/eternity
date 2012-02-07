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

#include "z_zone.h"
#include "cs_netid.h"

NetIDLookup<Mobj>                NetActors;
#if 0
NetIDLookup<CeilingThinker>      NetCeilings;
NetIDLookup<VerticalDoorThinker> NetDoors;
NetIDLookup<FloorMoveThinker>    NetFloors;
NetIDLookup<ElevatorThinker>     NetElevators;
NetIDLookup<PillarThinker>       NetPillars;
NetIDLookup<FloorWaggleThinker>  NetFloorWaggles;
NetIDLookup<PlatThinker>         NetPlatforms;
#endif

void CS_ClearNetIDs(void)
{
   NetActors.clear();
#if 0
   NetCeilings.clear();
   NetDoors.clear();
   NetFloors.clear();
   NetElevators.clear();
   NetPillars.clear();
   NetFloorWaggles.clear();
   NetPlatforms.clear();
#endif
}

