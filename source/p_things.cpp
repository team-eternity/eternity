// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3:
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
#include "e_states.h"
#include "e_things.h"
#include "s_sound.h"

//
// EV_ThingProjectile
//
// Implements Thing_Projectile(tid, type, angle, speed, vspeed)
//
int EV_ThingProjectile(int *args, bool gravity)
{
   int tid;
   angle_t angle;
   int fineAngle;
   fixed_t speed;
   fixed_t vspeed;
   mobjtype_t moType;
   Mobj *mobj = NULL, *newMobj;
   mobjinfo_t *mi;
   bool success = false;

   tid    = args[0];
   
   if(args[1] >= 0 && args[1] < ACS_NUM_THINGTYPES)
      moType = ACS_thingtypes[args[1]];
   else
      moType = UnknownThingType;

   mi = &mobjinfo[moType];
 
   // Don't spawn monsters if -nomonsters
   if(nomonsters && (mi->flags & MF_COUNTKILL || mi->flags3 & MF3_KILLABLE))
      return false;

   angle     = (angle_t)args[2] << 24;
   fineAngle = angle >> ANGLETOFINESHIFT;
   speed     = (fixed_t)args[3] << 13;
   vspeed    = (fixed_t)args[4] << 13;

   while((mobj = P_FindMobjFromTID(tid, mobj, NULL)))
   {
      newMobj = P_SpawnMobj(mobj->x, mobj->y, mobj->z, moType);
      if(newMobj->info->seesound)
         S_StartSound(newMobj, newMobj->info->seesound);
      P_SetTarget<Mobj>(&newMobj->target, mobj);
      newMobj->angle   = angle;
      newMobj->momx    = FixedMul(speed, finecosine[fineAngle]);
      newMobj->momy    = FixedMul(speed, finesine[fineAngle]);
      newMobj->momz    = vspeed;
      // HEXEN TODO: ???
      //newMobj->flags2 |= MF2_DROPPED; 
      if(gravity)
      {
         newMobj->flags &= ~MF_NOGRAVITY;
         newMobj->flags2 |= MF2_LOGRAV;
      }
      if(P_CheckMissileSpawn(newMobj))
         success = true;
   }

   return success;
}

//
// EV_ThingSpawn
//
// Implements Thing_Spawn(tid, type, angle, newtid)
//
int EV_ThingSpawn(int *args, bool fog)
{
   int tid;
   angle_t angle;
   Mobj *mobj = NULL, *newMobj, *fogMobj;
   mobjtype_t moType;
   mobjinfo_t *mi;
   bool success = false;
   fixed_t z;
   
   tid = args[0];
   
   if(args[1] >= 0 && args[1] < ACS_NUM_THINGTYPES)
      moType = ACS_thingtypes[args[1]];
   else
      moType = UnknownThingType;

   mi = &mobjinfo[moType];
   
   // Don't spawn monsters if -nomonsters 
   if(nomonsters && (mi->flags & MF_COUNTKILL || mi->flags3 & MF3_KILLABLE))
      return false;

   angle = (angle_t)args[2] << 24;
   
   while((mobj = P_FindMobjFromTID(tid, mobj, NULL)))
   {
      z = mobj->z;

      newMobj = P_SpawnMobj(mobj->x, mobj->y, z, moType);
      
      if(!P_CheckPositionExt(newMobj, newMobj->x, newMobj->y)) // Didn't fit?
         newMobj->removeThinker();
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

// 
// EV_ThingActivate
//
// Implements Thing_Activate(tid)
//
int EV_ThingActivate(int tid)
{
   Mobj *mobj = NULL;
   int success = 0;

   while((mobj = P_FindMobjFromTID(tid, mobj, NULL)))
   {
      if((mobj->flags & MF_COUNTKILL) || (mobj->flags3 & MF3_KILLABLE))
      {
         if(mobj->flags2 & MF2_DORMANT)
         {
            mobj->flags2 &= ~MF2_DORMANT;
            mobj->tics = 1;
            success = 1;
         }
      }

      // HEXEN TODO: crazy special-case behaviors for spikes and bells...

      if(mobj->info->activestate != NullStateNum)
         P_SetMobjState(mobj, mobj->info->activestate);
      S_StartSound(mobj, mobj->info->activatesound);

      if(mobj->info->activestate || mobj->info->activatesound)
         success = 1; // successful if anything actually happened
   }

   return success;
}

//
// EV_ThingDeactivate
//
// Implements Thing_Deactivate(tid)
//
int EV_ThingDeactivate(int tid)
{
   Mobj *mobj = NULL;
   int success = 0;

   while((mobj = P_FindMobjFromTID(tid, mobj, NULL)))
   {
      if((mobj->flags & MF_COUNTKILL) || (mobj->flags3 & MF3_KILLABLE))
      {
         if(!(mobj->flags2 & MF2_DORMANT))
         {
            mobj->flags2 |= MF2_DORMANT;
            mobj->tics = -1;
            success = 1;
         }
      }

      // HEXEN TODO: crazy special-case behavior for spikes...

      if(mobj->info->inactivestate != NullStateNum)
         P_SetMobjState(mobj, mobj->info->inactivestate);
      S_StartSound(mobj, mobj->info->deactivatesound);

      if(mobj->info->inactivestate || mobj->info->deactivatesound)
         success = 1; // successful if anything actually happened
   }

   return success;
}

// EOF

