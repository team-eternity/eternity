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
#include "e_args.h"
#include "e_things.h"
#include "m_random.h"
#include "p_mobj.h"
#include "p_inter.h"
#include "r_main.h"
#include "s_sound.h"
#include "tables.h"

#include "p_map.h"

#define WEAPONTOP    (FRACUNIT*32)

void A_StaffAttackPL1(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;
   int       damage = 5 + (P_Random(pr_staff) & 15);
   angle_t   angle  = player->mo->angle + (P_SubRandom(pr_staffangle) << 18);
   fixed_t   slope  =  P_AimLineAttack(player->mo, angle, MELEERANGE, 0);

   const int   tnum = E_SafeThingType(MT_STAFFPUFF);
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

void A_StaffAttackPL2(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;
   int       damage = 18 + (P_Random(pr_staff2) & 63);
   angle_t   angle = player->mo->angle + (P_SubRandom(pr_staffangle) << 18);
   fixed_t   slope = P_AimLineAttack(player->mo, angle, MELEERANGE, 0);

   const int   tnum = E_SafeThingType(MT_STAFFPUFF2);
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
   Mobj     *mo     = actionargs->actor;
   player_t *player = mo->player;
   angle_t   angle  = mo->angle;
   const int damage = 7 + (P_Random(pr_goldwand) & 7);;

   P_SubtractAmmo(player, -1);
   P_BulletSlope(mo);
   if(player->refire)
      angle += P_SubRandom(pr_goldwand) << 18;

   const int    tnum = E_SafeThingType(MT_GOLDWANDPUFF1);
   mobjinfo_t  *puff = mobjinfo[tnum];

   P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage, puff);
   S_StartSound(player->mo, sfx_gldhit);
}

void A_FireGoldWandPL2(actionargs_t *actionargs)
{
   Mobj     *mo     = actionargs->actor;
   player_t *player = mo->player;
   angle_t   angle;
   fixed_t momz, z;
   int damage, i;

   z  = mo->z + 32 * FRACUNIT;
   mo = player->mo;

   P_SubtractAmmo(player, -1);
   P_BulletSlope(mo);

   const int   tnum = E_SafeThingType(MT_GOLDWANDFX2);
   mobjinfo_t *fx   = mobjinfo[tnum];

   momz = FixedMul(fx->speed, bulletslope);
   P_SpawnMissileAngle(mo, tnum, mo->angle - (ANG45 / 8), momz, z);
   P_SpawnMissileAngle(mo, tnum, mo->angle + (ANG45 / 8), momz, z);
   angle = mo->angle - (ANG45 / 8);

   for(i = 0; i < 5; i++)
   {
      const int    pnum = E_SafeThingType(MT_GOLDWANDPUFF2);
      mobjinfo_t  *puff = mobjinfo[pnum];
      damage = 1 + (P_Random(pr_goldwand2) & 7);
      P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage, puff);
      angle += ((ANG45 / 8) * 2) / 4;
   }

   S_StartSound(player->mo, sfx_gldhit);
}

void A_FireCrossbowPL1(actionargs_t *actionargs)
{
   Mobj     *pmo    = actionargs->actor;
   player_t *player = pmo->player;
   const int tnum   = E_SafeThingType(MT_CRBOWFX3);

   P_SubtractAmmo(player, -1);
   P_SpawnPlayerMissile(pmo, E_SafeThingType(MT_CRBOWFX1));
   P_SpawnMissileAngleHeretic(pmo, tnum, pmo->angle - (ANG45 / 10));
   P_SpawnMissileAngleHeretic(pmo, tnum, pmo->angle + (ANG45 / 10));
}

void A_FireCrossbowPL2(actionargs_t *actionargs)
{
   Mobj     *pmo    = actionargs->actor;
   player_t *player = pmo->player;
   const int tnum2  = E_SafeThingType(MT_CRBOWFX2);
   const int tnum3  = E_SafeThingType(MT_CRBOWFX3);

   P_SubtractAmmo(player, -1);
   P_SpawnPlayerMissile(pmo, tnum2);
   P_SpawnMissileAngleHeretic(pmo, tnum2, pmo->angle - (ANG45 / 10));
   P_SpawnMissileAngleHeretic(pmo, tnum2, pmo->angle + (ANG45 / 10));
   P_SpawnMissileAngleHeretic(pmo, tnum3, pmo->angle - (ANG45 / 5));
   P_SpawnMissileAngleHeretic(pmo, tnum3, pmo->angle + (ANG45 / 5));
}

void A_BoltSpark(actionargs_t *actionargs)
{
   Mobj     *bolt = actionargs->actor;
   const int tnum = E_SafeThingType(MT_CRBOWFX4);

   if(P_Random(pr_boltspark) > 50)
   {
      Mobj *spark = P_SpawnMobj(bolt->x, bolt->y, bolt->z, tnum);
      spark->x += P_SubRandom(pr_boltspark) << 10;
      spark->y += P_SubRandom(pr_boltspark) << 10;
   }
}

void A_FireBlasterPL1(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;
   Mobj     *mo     = player->mo;
   angle_t   angle;
   int       damage;


   S_StartSound(mo, sfx_gldhit);
   P_SubtractAmmo(player, -1);
   P_BulletSlope(mo);
   damage = (1 + (P_Random(pr_blaster) & 7)) * 4;
   angle  = mo->angle;
   if(player->refire)
      angle += P_SubRandom(pr_blaster) << 18;

   const int    tnum = E_SafeThingType(MT_BLASTERPUFF1);
   mobjinfo_t  *puff = mobjinfo[tnum];

   P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage, puff);
   S_StartSound(player->mo, sfx_blssht);
}

void A_FireSkullRodPL1(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;

   if(!P_CheckAmmo(player))
      return;

   P_SubtractAmmo(player, -1);
   const int tnum = E_SafeThingType(MT_HORNRODFX1);
   Mobj     *mo   = P_SpawnPlayerMissile(player->mo, tnum);
   // Randomize the first frame
   if(mo && P_Random(pr_skullrod) > 128)
      P_SetMobjState(mo, mo->state->nextstate);
}

void A_FirePhoenixPL1(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;
   const int tnum   = E_SafeThingType(MT_PHOENIXFX1);
   angle_t   angle;

   P_SubtractAmmo(player, -1);
   P_SpawnPlayerMissile(player->mo, tnum);
   // TODO: This is from Choco, why is it commented out?
   //P_SpawnPlayerMissile(player->mo, MT_MNTRFX2);
   angle = player->mo->angle + ANG180;
   angle >>= ANGLETOFINESHIFT;
   player->mo->momx += FixedMul(4 * FRACUNIT, finecosine[angle]);
   player->mo->momy += FixedMul(4 * FRACUNIT, finesine[angle]);
}

void A_GauntletAttack(actionargs_t *actionargs)
{
   arglist_t  *args   = actionargs->args;
   player_t   *player = actionargs->actor->player;
   pspdef_t   *psp    = actionargs->pspr;
   fixed_t     dist;
   angle_t     angle;
   mobjinfo_t *puff;
   int damage, slope, randVal, tnum;

   if(!psp)
      return;

   int powered = E_ArgAsInt(args, 0, 0);

   psp->sx = ((P_Random(pr_gauntlets) & 3) - 2) * FRACUNIT;
   psp->sy = WEAPONTOP + (P_Random(pr_gauntlets) & 3) * FRACUNIT;
   angle = player->mo->angle;

   if(powered)
   {
      damage = (1 + (P_Random(pr_gauntlets) & 7))* 2;
      dist = 4 * MELEERANGE;
      angle += P_SubRandom(pr_gauntletsangle) << 17;
      tnum = E_SafeThingType(MT_GAUNTLETPUFF2);
   }
   else
   {
      damage = (1 + (P_Random(pr_gauntlets) & 7)) * 2;
      dist = MELEERANGE + 1;
      angle += P_SubRandom(pr_gauntletsangle) << 18;
      tnum = E_SafeThingType(MT_GAUNTLETPUFF1);
   }

   puff  = mobjinfo[tnum];
   slope = P_AimLineAttack(player->mo, angle, dist, 0);
   P_LineAttack(player->mo, angle, dist, slope, damage, puff);

   if(!clip.linetarget)
   {
      if(P_Random(pr_gauntlets) > 64) // TODO: Maybe don't use pr_gauntlets?
         player->extralight = !player->extralight;
      S_StartSound(player->mo, sfx_gntful);
      return;
   }

   randVal = P_Random(pr_gauntlets); // TODO: Maybe don't use pr_gauntlets?
   if(randVal < 64)
      player->extralight = 0;
   else if(randVal < 160)
      player->extralight = 1;
   else
      player->extralight = 2;

   if(powered)
   {
      // FIXME: This needs to do damage vamp
      //P_GiveBody(player, damage >> 1);
      S_StartSound(player->mo, sfx_gntpow);
   }
   else
      S_StartSound(player->mo, sfx_gnthit);

   // turn to face target
   angle = R_PointToAngle2(player->mo->x, player->mo->y,
                           clip.linetarget->x, clip.linetarget->y);

   if(angle - player->mo->angle > ANG180)
   {
      if(angle - player->mo->angle < -ANG90 / 20)
         player->mo->angle = angle + ANG90 / 21;
      else
         player->mo->angle -= ANG90 / 20;
   }
   else
   {
      if(angle - player->mo->angle > ANG90 / 20)
         player->mo->angle = angle - ANG90 / 21;
      else
         player->mo->angle += ANG90 / 20;
   }

   player->mo->flags |= MF_JUSTATTACKED;
}

// EOF

