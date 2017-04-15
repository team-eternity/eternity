//
// The Eternity Engine
// Copyright(C) 2017 James Haley, Max Waine, et al.
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//----------------------------------------------------------------------------
//
// Purpose: Heretic weapon action functions
// Some code is derived from Chocolate Doom, by Simon Howard, used under
// terms of the GPLv3.
//
// Authors: Max Waine
//

#include "z_zone.h"

#include "a_args.h"
#include "d_player.h"
//#include "e_sound.h"
#include "e_things.h"
#include "m_random.h"
#include "p_mobj.h"
#include "tables.h"
#include "s_sound.h"

#include "p_map.h"

void A_FireGoldWandPL1(actionargs_t *actionargs)
{
   Mobj     *mo = actionargs->actor;
   player_t *player = mo->player;
   angle_t   angle = mo->angle;
   const int damage = 7 + (P_Random(pr_goldwand) & 7);;

   P_SubtractAmmo(player, 1);
   P_BulletSlope(mo);
   if(player->refire)
      angle += P_SubRandom(pr_goldwand) << 18;

   const int tnum = E_ThingNumForDEHNum(MT_HGWANDPUFF1);
   mobjinfo_t  *puff = mobjinfo[tnum];
   P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage, puff);
   S_StartSound(player->mo, sfx_gldhit);
}

// EOF

