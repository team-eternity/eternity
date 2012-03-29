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
//   Serverside routines for handling sectors and map specials.
//
//----------------------------------------------------------------------------

#include "r_defs.h"
#include "r_state.h"

#include "cs_spec.h"
#include "cs_netid.h"
#include "cs_main.h"
#include "sv_main.h"
#include "sv_spec.h"

void SV_LoadCurrentSectorPosition(uint32_t sector_number)
{
   CS_SetSectorPosition(sector_number, sv_world_index);
}

void SV_SaveSectorPositions(void)
{
   int i;

   if(!sv_world_index)
      return;

   for(i = 0; i < numsectors; i++)
   {
      CS_SaveSectorPosition(sv_world_index, i);
      if(!CS_SectorPositionsEqual(i, sv_world_index - 1, sv_world_index))
         SV_BroadcastSectorPosition(i);
   }
}

