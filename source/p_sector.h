//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   Sector effects.
//
//-----------------------------------------------------------------------------

#ifndef P_SECTOR_H__
#define P_SECTOR_H__

class Mobj;
struct sector_t;

enum ssurftype_e
{
   ssurf_floor,
   ssurf_ceiling,
};

void P_SaveSectorPositions();
void P_SaveSectorPosition(const sector_t &sec);
void P_SaveSectorPosition(const sector_t &sec, ssurftype_e surf);
void P_NewSectorActionFromMobj(Mobj *actor);
void P_SetSectorZoneFromMobj(Mobj *actor);

int EV_SectorSetRotation(const line_t *line, int tag, int floorangle,
                         int ceilingangle);
int EV_SectorSetCeilingPanning(const line_t *line, int tag, fixed_t xoffs,
                               fixed_t yoffs);
int EV_SectorSetFloorPanning(const line_t *line, int tag, fixed_t xoffs,
                             fixed_t yoffs);
int EV_SectorSoundChange(int tag, int sndSeqID);

#endif

// EOF

