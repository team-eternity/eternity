// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//
// Hexen enemy-related functions
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

// Need gamepad HAL
#include "hal/i_gamepads.h"

#include "a_small.h"
#include "d_mod.h"
#include "doomstat.h"
#include "e_sound.h"
#include "m_random.h"
#include "p_inter.h"
#include "p_maputl.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "p_xenemy.h"
#include "s_sound.h"

IMPLEMENT_THINKER_TYPE(QuakeThinker)

//
// T_QuakeThinker
//
// Earthquake effect thinker.
//
void QuakeThinker::Think()
{
   int i, tics;
   soundparams_t params;
   
   // quake is finished?
   if(this->duration == 0)
   {
      this->remove();
      return;
   }

   params.sfx = E_SoundForName(soundName.constPtr());

   // loop quake sound
   if(params.sfx && !S_CheckSoundPlaying(this, params.sfx))
   {
      params.setNormalDefaults(this);
      params.loop = true;
      S_StartSfxInfo(params);
   }

   tics = this->duration--;

   // do some rumbling
   const linkoffset_t *link;
   for(i = 0; i < MAXPLAYERS; i++)
   {
      if(playeringame[i])
      {
         player_t *p  = &players[i];
         Mobj     *mo = p->mo;

         link = P_GetLinkOffset(this->groupid, mo->groupid);
         fixed_t dst = P_AproxDistance(this->x - mo->x + link->x,
                                       this->y - mo->y + link->y);

         // test if player is in quake radius
         // haleyjd 04/16/07: only set p->quake when qt->intensity is greater;
         // this way, the strongest quake in effect always wins out
         if(dst < this->quakeRadius && p->quake < this->intensity)
         {
            p->quake = this->intensity;
            if(p == &players[consoleplayer])
               I_StartHaptic(HALHapticInterface::EFFECT_RUMBLE, this->intensity, 1000/TICRATE);
         }

         // every 2 tics, the player may be damaged
         if(!(tics & 1))
         {
            angle_t  thrustangle;   
            
            // test if in damage radius and on floor
            if(dst < this->damageRadius && mo->z <= mo->zref.floor)
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
   Super::serialize(arc);

   arc << intensity << duration << quakeRadius << damageRadius;
   soundName.archive(arc);
}

//
// P_StartQuake
//
// Starts an earthquake at each object with the tid in args[4]
//
bool P_StartQuake(int *args, Mobj *activator)
{
   Mobj *mo = NULL;
   bool ret = false;

   while((mo = P_FindMobjFromTID(args[4], mo, activator)))
   {
      QuakeThinker *qt;
      ret = true;

      qt = new QuakeThinker();
      qt->addThinker();

      qt->intensity    = args[0];
      qt->duration     = args[1];
      qt->damageRadius = args[2] * (64 * FRACUNIT);
      qt->quakeRadius  = args[3] * (64 * FRACUNIT);
      qt->soundName    = "Earthquake";

      qt->x       = mo->x;
      qt->y       = mo->y;
      qt->z       = mo->z;
      qt->groupid = mo->groupid;
   }

   return ret;
}

// EOF

