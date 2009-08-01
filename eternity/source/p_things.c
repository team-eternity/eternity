// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2009 Raven Software, James Haley
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
// Description:
//
// Mobj-related line specials. Mostly from Hexen.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomstat.h"
#include "d_gi.h"
#include "p_mobj.h"
#include "p_map3d.h"
#include "a_small.h"
#include "acs_intr.h"
#include "e_things.h"
#include "s_sound.h"

//
// EV_ThingSpawn
//
// Implements Thing_Spawn(tid, type, angle, newtid)
//
int EV_ThingSpawn(int *args, boolean fog)
{
   int tid;
   angle_t angle;
   mobj_t *mobj = NULL, *newMobj, *fogMobj;
   mobjtype_t moType;
   mobjinfo_t *mi;
   boolean success = false;
   fixed_t z;
   
   tid = args[0];
   
   if(args[1] >= 0 && args[1] <= ACS_NUM_THINGTYPES)
      moType = ACS_thingtypes[args[1]];
   else
      moType = UnknownThingType;

   mi = &mobjinfo[moType];
   
   if(nomonsters && 
      (mi->flags & MF_COUNTKILL ||
       mi->flags3 & MF3_KILLABLE))
   { 
      // Don't spawn monsters if -nomonsters
      return false;
   }

   angle = (angle_t)args[2] << 24;
   
   while((mobj = P_FindMobjFromTID(tid, mobj, NULL)))
   {
      z = mobj->z;

      newMobj = P_SpawnMobj(mobj->x, mobj->y, z, moType);
      
      if(!P_CheckPositionExt(newMobj, newMobj->x, newMobj->y)) // Didn't fit?
         P_RemoveMobj(newMobj);
      else
      {
         newMobj->angle = angle;

         if(fog)
         {
            fogMobj = 
               P_SpawnMobj(mobj->x, mobj->y,
                           mobj->z + GameModeInfo->teleFogHeight, 
                           GameModeInfo->teleFogType);
            S_StartSound(fogMobj, GameModeInfo->teleSound);
         }
         
         // don't item-respawn
         newMobj->flags3 |= MF3_NOITEMRESP;

         success = true;
      }
   }

   return success;
}


// EOF

