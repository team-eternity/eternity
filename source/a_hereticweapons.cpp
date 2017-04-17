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
#include "doomstat.h"
//#include "e_sound.h"
#include "e_things.h"
#include "m_random.h"
#include "p_mobj.h"
#include "r_main.h"
#include "s_sound.h"
#include "tables.h"

#include "p_map.h"


// TODO: Add a define for E_ThingNumForDEHNum(foo) for when it's only needed once
// per-function? Shouldn't matter due to constant folding in release config anyways.

void A_StaffAttackPL1(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;
   int damage = 5 + (P_Random(pr_staff) & 15);
   angle_t angle = player->mo->angle + (P_SubRandom(pr_staffangle) << 18);
   fixed_t slope = P_AimLineAttack(player->mo, angle, MELEERANGE, 0);

   const int tnum = E_ThingNumForDEHNum(MT_STAFFPUFF);
   mobjinfo_t *puff = mobjinfo[tnum];

   P_LineAttack(player->mo, angle, MELEERANGE, slope, damage, puff);
   if(clip.linetarget)
   {
      //S_StartSound(player->mo, sfx_stfhit);
      // turn to face target
      player->mo->angle = R_PointToAngle2(player->mo->x,
                                          player->mo->y, clip.linetarget->x,
                                          clip.linetarget->y);
   }
}

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

   const int tnum = E_ThingNumForDEHNum(MT_GOLDWANDPUFF1);
   mobjinfo_t  *puff = mobjinfo[tnum];
   P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage, puff);
   S_StartSound(player->mo, sfx_gldhit);
}

void A_FireCrossbowPL1(actionargs_t *actionargs)
{
   Mobj *pmo = actionargs->actor;
   player_t *player = pmo->player;
   const int tnum = E_ThingNumForDEHNum(MT_CRBOWFX3);

   P_SubtractAmmo(player, 1);
   P_SpawnPlayerMissile(pmo, E_ThingNumForDEHNum(MT_CRBOWFX1));
   P_SpawnMissileAngleHeretic(pmo, tnum, pmo->angle - (ANG45 / 10));
   P_SpawnMissileAngleHeretic(pmo, tnum, pmo->angle + (ANG45 / 10));
}

void A_FireSkullRodPL1(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;
   Mobj *mo;

   if(!P_CheckAmmo(player))
      return;

   P_SubtractAmmo(player, 1);
   const int tnum = E_ThingNumForDEHNum(MT_HORNRODFX1);
   mo = P_SpawnPlayerMissile(player->mo, tnum);
   // Randomize the first frame
   if(mo && P_Random(pr_skullrod) > 128)
      P_SetMobjState(mo, mo->state->nextstate);
}

void A_FirePhoenixPL1(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;
   angle_t angle;

   P_SubtractAmmo(player, 1);
   const int tnum = E_ThingNumForDEHNum(MT_PHOENIXFX1);
   P_SpawnPlayerMissile(player->mo, tnum);
   // TODO: This is from Choco, why is it commented out?
   //P_SpawnPlayerMissile(player->mo, MT_MNTRFX2);
   angle = player->mo->angle + ANG180;
   angle >>= ANGLETOFINESHIFT;
   player->mo->momx += FixedMul(4 * FRACUNIT, finecosine[angle]);
   player->mo->momy += FixedMul(4 * FRACUNIT, finesine[angle]);
}

void A_FireBlasterPL1(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;
   Mobj *mo = player->mo;
   angle_t angle;
   int damage;


   S_StartSound(mo, sfx_gldhit);
   P_SubtractAmmo(player, 1);
   P_BulletSlope(mo);
   damage = (1 + (P_Random(pr_blaster) & 7)) * 4;
   angle = mo->angle;
   if(player->refire)
   {
      angle += P_SubRandom(pr_blaster) << 18;
   }
   const int tnum = E_ThingNumForDEHNum(MT_BLASTERPUFF1);
   mobjinfo_t  *puff = mobjinfo[tnum];
   P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage, puff);
   S_StartSound(player->mo, sfx_blssht);
}

// EOF

