// Emacs style mode select   -*- C -*- 
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

boolean P_TestMobjZ(mobj_t *mo);
boolean P_CheckPosition3D(mobj_t *thing, fixed_t x, fixed_t y);
boolean P_CheckPositionExt(mobj_t *mo, fixed_t x, fixed_t y);
boolean P_ChangeSector3D(sector_t *sector, int crunch, int amt, int floorOrCeil);
mobj_t  *P_GetThingUnder(mobj_t *mo);

#endif 

// EOF

