//
// Copyright (C) 2023 James Haley, Max Waine et al.
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
//----------------------------------------------------------------------------
//
// Purpose: Strife action functions
// Almost all code is derived from Strife: Veteran Edition, by Simon Howard,
// Samuel Villarreal, and James Haley, used under terms of the GPLv3.
//
// Authors: Max Waine
//

#include "z_zone.h"

#include "a_args.h"
#include "e_things.h"
#include "info.h"
#include "m_random.h"
#include "p_mobj.h"

// STRIFE_TODO: Implement for real
static bool classicmode = false;
static bool d_maxgore = false;

//
// villsa [STRIFE] new codepointer
// 09/06/10: Spawns gibs when organic actors get splattered, or junk
// when robots explode.
//
void A_BodyParts(actionargs_t *actionargs)
{
   Mobj *actor = actionargs->actor;
   mobjtype_t type;
   Mobj *mo;
   angle_t an;
   int numchunks = (classicmode || !d_maxgore) ? 1 : 4;

   // STRIFE_TODO: MetaGibSpec?
   if(actor->flags & MF_NOBLOOD) // Robots are flagged NOBLOOD
      type = MT_JUNK;
   else
      type = MT_MEAT;

   // haleyjd 20140907: [SVE] max gore - ludicrous gibs
   do
   {
      mo = P_SpawnMobj(actor->x, actor->y, actor->z + (24 * FRACUNIT), E_SafeThingType(type));
      P_SetMobjState(mo, mo->info->spawnstate + (P_Random(pr_bodyparts) % 19));

      an = (P_Random(pr_bodyparts) << 13) / 255;
      mo->angle = an << ANGLETOFINESHIFT;

      mo->momx = FixedMul(finecosine[an], (P_Random(pr_bodyparts) & 0x0f) << FRACBITS);
      mo->momy = FixedMul(finesine[an], (P_Random(pr_bodyparts) & 0x0f) << FRACBITS);
      mo->momz = (P_Random(pr_bodyparts) & 0x0f) << FRACBITS;

      // SVE_TODO
      // [SVE] svillarreal - even more ludicrous gore
      //if(d_maxgore && !(actor->flags & MF_NOBLOOD))
      //{
      //   Mobj *gore = P_SpawnMobj(actor->x, actor->y, actor->z + (32 * FRACUNIT), MT_BLOOD_GORE);
      //
      //   gore->angle = mo->angle;
      //
      //   gore->momx = mo->momx;
      //   gore->momy = mo->momy;
      //   gore->momz = mo->momz;
      //}
   } while(--numchunks > 0);
}

// EOF

