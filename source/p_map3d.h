// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
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
//      Map functions
//
//-----------------------------------------------------------------------------

#ifndef P_MAP3D_H__
#define P_MAP3D_H__

bool  P_TestMobjZ(Mobj *mo);
bool  P_CheckPosition3D(Mobj *thing, fixed_t x, fixed_t y);
bool  P_CheckPositionExt(Mobj *mo, fixed_t x, fixed_t y);
bool  P_ChangeSector3D(sector_t *sector, int crunch, int amt, int floorOrCeil);
Mobj *P_GetThingUnder(Mobj *mo);

#endif 

// EOF

