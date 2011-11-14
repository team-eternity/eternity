// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2007 James Haley
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
//
// Hexen enemy-related functions
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "a_small.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_sound.h"
#include "m_random.h"
#include "p_inter.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "p_xenemy.h"
#include "s_sound.h"

IMPLEMENT_THINKER_TYPE(QuakeThinker, PointThinker)

//
// T_QuakeThinker
//
// Earthquake effect thinker.
//
void QuakeThinker::Think()
{
   int i, tics;
   sfxinfo_t *quakesound;
   
   // quake is finished?
   if(this->duration == 0)
   {
      S_StopSound(this, CHAN_ALL);
      this->removeThinker();
      return;
   }
   
   quakesound = E_SoundForName("Earthquake");

   // loop quake sound
   if(quakesound && !S_CheckSoundPlaying(this, quakesound))
      S_StartSfxInfo(this, quakesound, 127, ATTN_NORMAL, true, CHAN_AUTO);

   tics = this->duration--;

   // do some rumbling
   for(i = 0; i < MAXPLAYERS; ++i)
   {
      if(playeringame[i])
      {
         player_t *p  = &players[i];
         Mobj   *mo = p->mo;
         fixed_t  dst = P_AproxDistance(this->x - mo->x, 
                                        this->y - mo->y);

         // test if player is in quake radius
         // haleyjd 04/16/07: only set p->quake when qt->intensity is greater;
         // this way, the strongest quake in effect always wins out
         if(dst < this->quakeRadius && p->quake < this->intensity)
            p->quake = this->intensity;

         // every 2 tics, the player may be damaged
         if(!(tics & 1))
         {
            angle_t  thrustangle;   
            
            // test if in damage radius and on floor
            if(dst < this->damageRadius && mo->z <= mo->floorz)
            {
               if(P_Random(pr_quake) < 50)
               {
                  P_DamageMobj(mo, NULL, NULL, P_Random(pr_quakedmg) % 8 + 1,
                               MOD_QUAKE);
               }
               thrustangle = (359 * P_Random(pr_quake) / 255) * ANGLE_1;
               P_ThrustMobj(mo, thrustangle, this->intensity * FRACUNIT / 2);
            }
         }
      }
   }
}

//
// QuakeThinker::serialize
//
// Saves/loads a QuakeThinker.
//
void QuakeThinker::serialize(SaveArchive &arc)
{
   PointThinker::serialize(arc);

   arc << intensity << duration << quakeRadius << damageRadius;
}

//
// P_StartQuake
//
// Starts an earthquake at each object with the tid in args[4]
//
bool P_StartQuake(int *args)
{
   Mobj *mo = NULL;
   bool ret = false;

   while((mo = P_FindMobjFromTID(args[4], mo, NULL)))
   {
      QuakeThinker *qt;
      ret = true;

      qt = new QuakeThinker;
      qt->addThinker();

      qt->intensity    = args[0];
      qt->duration     = args[1];
      qt->damageRadius = args[2] * (64 * FRACUNIT);
      qt->quakeRadius  = args[3] * (64 * FRACUNIT);

      qt->x       = mo->x;
      qt->y       = mo->y;
      qt->z       = mo->z;
      qt->groupid = mo->groupid;
   }

   return ret;
}

// EOF

