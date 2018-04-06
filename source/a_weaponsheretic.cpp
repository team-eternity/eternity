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
//----------------------------------------------------------------------------
//
// Purpose: Heretic weapon action functions
// Almost all code is derived from Chocolate Doom, by Simon Howard, used under
// terms of the GPLv3.
//
// Authors: Max Waine
//

#include "z_zone.h"

#include "a_args.h"
#include "doomstat.h"
#include "d_player.h"
#include "e_args.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "e_weapons.h"
#include "m_random.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_spec.h"
#include "r_main.h"
#include "s_sound.h"
#include "tables.h"
#include "v_misc.h"

#include "p_map.h"

// FIXME: This will probably be defined elsewhere at some point
#undef PO2
#define PO2(x) (1 << x)

void A_StaffAttackPL1(actionargs_t *actionargs)
{
   Mobj    *mo     = actionargs->actor;
   int      damage = 5 + (P_Random(pr_staff) & 15);
   angle_t  angle  = mo->angle + (P_SubRandom(pr_staffangle) * PO2(18));
   fixed_t  slope  = P_DoAutoAim(mo, angle, MELEERANGE);

   const int   tnum = E_SafeThingType(MT_STAFFPUFF);
   edefstructvar(puffinfo_t, puff);
   puff.info = mobjinfo[tnum];
   puff.upspeed = FRACUNIT;

   P_LineAttack(mo, angle, MELEERANGE, slope, damage, &puff);
   if(clip.linetarget)
   {
      //S_StartSound(player->mo, sfx_stfhit);
      // turn to face target
      mo->angle = P_PointToAngle(mo->x, mo->y, clip.linetarget->x, clip.linetarget->y);
   }
}

void A_StaffAttackPL2(actionargs_t *actionargs)
{
   Mobj    *mo = actionargs->actor;
   int      damage = 18 + (P_Random(pr_staff2) & 63);
   angle_t  angle = mo->angle + (P_SubRandom(pr_staffangle) * PO2(18));
   fixed_t  slope = P_AimLineAttack(mo, angle, MELEERANGE, 0);

   const int   tnum = E_SafeThingType(MT_STAFFPUFF2);
   edefstructvar(puffinfo_t, puff);
   puff.info = mobjinfo[tnum];
   // no vertical speed here

   P_LineAttack(mo, angle, MELEERANGE, slope, damage, &puff);
   if(clip.linetarget)
   {
      //S_StartSound(player->mo, sfx_stfhit);
      // turn to face target
      mo->angle = R_PointToAngle2(mo->x, mo->y, clip.linetarget->x, clip.linetarget->y);
   }
}

void A_FireGoldWandPL1(actionargs_t *actionargs)
{
   Mobj     *mo     = actionargs->actor;
   player_t *player = mo->player;
   angle_t   angle  = mo->angle;
   const int damage = 7 + (P_Random(pr_goldwand) & 7);;

   if(!player)
      return;

   P_SubtractAmmo(player, -1);
   P_BulletSlope(mo);
   if(player->refire)
      angle += P_SubRandom(pr_goldwand) * PO2(18);

   const int    tnum = E_SafeThingType(MT_GOLDWANDPUFF1);
   edefstructvar(puffinfo_t, puff);
   puff.info = mobjinfo[tnum];

   P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage, &puff);
   P_WeaponSound(mo, sfx_gldhit);
}

void A_FireGoldWandPL2(actionargs_t *actionargs)
{
   Mobj     *mo     = actionargs->actor;
   player_t *player = mo->player;
   angle_t   angle;
   fixed_t   momz, z;
   int       damage, i;

   if(!player)
      return;

   z  = mo->z + 32 * FRACUNIT;

   P_SubtractAmmo(player, -1);
   P_BulletSlope(mo);

   const int   tnum = E_SafeThingType(MT_GOLDWANDFX2);
   mobjinfo_t *fx   = mobjinfo[tnum];

   momz = FixedMul(fx->speed, bulletslope);
   P_SpawnMissileAngle(mo, tnum, mo->angle - (ANG45 / 8), momz, z);
   P_SpawnMissileAngle(mo, tnum, mo->angle + (ANG45 / 8), momz, z);
   angle = mo->angle - (ANG45 / 8);

   const int    pnum = E_SafeThingType(MT_GOLDWANDPUFF2);
   edefstructvar(puffinfo_t, puff);
   puff.info = mobjinfo[pnum];

   for(i = 0; i < 5; i++)
   {
      damage = 1 + (P_Random(pr_goldwand2) & 7);
      P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage, &puff);
      angle += ((ANG45 / 8) * 2) / 4;
   }

   P_WeaponSound(mo, sfx_gldhit);
}

void A_FireMacePL1B(actionargs_t *actionargs)
{
   Mobj     *pmo    = actionargs->actor;
   player_t *player = pmo->player;
   Mobj     *ball;
   angle_t   angle;

   if(!player || !P_CheckAmmo(player))
      return;

   P_SubtractAmmo(player, -1);

   const int   tnum = E_SafeThingType(MT_MACEFX2);

   ball = P_SpawnMobj(pmo->x, pmo->y, pmo->z + (28 * FRACUNIT) - pmo->floorclip, tnum);

   mobjinfo_t *fx = mobjinfo[tnum];

   const fixed_t slope = P_PlayerPitchSlope(player);
   ball->momz = FixedMul(ball->info->speed, slope) + (2 * FRACUNIT);
   angle = pmo->angle;
   P_SetTarget(&ball->target, pmo);
   ball->angle = angle;
   ball->z += 2 * slope;
   angle >>= ANGLETOFINESHIFT;
   ball->momx = (pmo->momx  / PO2(1)) + FixedMul(ball->info->speed, finecosine[angle]);
   ball->momy = (pmo->momy / PO2(1)) + FixedMul(ball->info->speed, finesine[angle]);

   S_StartSound(ball, sfx_lobsht);
   P_CheckMissileSpawn(ball);
}

void A_FireMacePL1(actionargs_t *actionargs)
{
   Mobj     *mo     = actionargs->actor;
   player_t *player = mo->player;
   pspdef_t *psp    = actionargs->pspr;
   Mobj     *ball;

   if(!player || !psp)
      return;

   if(P_Random(pr_firemace) < 28)
   {
      A_FireMacePL1B(actionargs);
      return;
   }
   if(!P_CheckAmmo(player))
      return;
   
   const int tnum = E_SafeThingType(MT_MACEFX1);

   P_SubtractAmmo(player, -1);
   psp->sx = ((P_Random(pr_firemace) & 3) - 2) * FRACUNIT;
   psp->sy = WEAPONTOP + (P_Random(pr_firemace) & 3) * FRACUNIT;
   ball = P_SpawnMissileAngleHeretic(mo, tnum, mo->angle +
                                               ((P_Random(pr_firemace) & 7) - 4) * PO2(24));
   if(ball)
      ball->counters[0] = 16;    // tics till dropoff
}

void A_MacePL1Check(actionargs_t *actionargs)
{
   Mobj   *ball = actionargs->actor;
   angle_t angle;

   if(ball->counters[0] == 0)
      return;
   ball->counters[0] -= 4;
   if(ball->counters[0] > 0)
      return;
   ball->counters[0] = 0;
   ball->flags2 |= MF2_LOGRAV;
   angle = ball->angle >> ANGLETOFINESHIFT;
   ball->momx = FixedMul(7 * FRACUNIT, finecosine[angle]);
   ball->momy = FixedMul(7 * FRACUNIT, finesine[angle]);
   ball->momz -= ball->momz / PO2(1);
}

void A_MaceBallImpact(actionargs_t *actionargs)
{
   constexpr int MAGIC_JUNK = 1234;

   Mobj *ball = actionargs->actor;
   if((ball->z <= ball->floorz) && E_HitFloor(ball))
   {                           // Landed in some sort of liquid
      ball->remove();
      return;
   }
   if((ball->health != MAGIC_JUNK) && (ball->z <= ball->floorz)
      && ball->momz)
   {                           // Bounce
      ball->health = MAGIC_JUNK;
      ball->momz = (ball->momz * 192) / PO2(8);
      ball->flags4 &= ~MF4_HERETICBOUNCES;
      P_SetMobjState(ball, ball->info->spawnstate);
      S_StartSound(ball, sfx_bounce);
   }
   else
   {                           // Explode
      ball->flags |= MF_NOGRAVITY;
      ball->flags2 &= ~MF2_LOGRAV;
      S_StartSound(ball, sfx_lobhit);
   }
}

void A_MaceBallImpact2(actionargs_t *actionargs)
{
   Mobj     *ball = actionargs->actor;
   Mobj     *tiny;
   angle_t   angle;
   const int tnum = E_SafeThingType(MT_MACEFX3);

   if((ball->z <= ball->floorz) && E_HitFloor(ball))
   {                           // Landed in some sort of liquid
      ball->remove();
      return;
   }
   if((ball->z != ball->floorz) || (ball->momz < 2 * FRACUNIT))
   {                           // Explode
      ball->momx = ball->momy = ball->momz = 0;
      ball->flags |= MF_NOGRAVITY;
      ball->flags2 &= ~(MF2_LOGRAV | MF4_HERETICBOUNCES);
   }
   else
   {                           // Bounce
      ball->momz = ball->momz * 192 / PO2(8);
      P_SetMobjState(ball, ball->info->spawnstate);

      for(int horizontalmultiplier = -1; horizontalmultiplier <= 1; horizontalmultiplier += 2)
      {
         tiny = P_SpawnMobj(ball->x, ball->y, ball->z, tnum);
         angle = ball->angle + (horizontalmultiplier * ANG90);
         P_SetTarget(&tiny->target, ball->target);
         tiny->angle = angle;
         angle >>= ANGLETOFINESHIFT;
         tiny->momx = (ball->momx / PO2(1)) + FixedMul(ball->momz - FRACUNIT, finecosine[angle]);
         tiny->momy = (ball->momy / PO2(1)) + FixedMul(ball->momz - FRACUNIT, finesine[angle]);
         tiny->momz = ball->momz;
         P_CheckMissileSpawn(tiny);
      }
   }
}

void A_FireMacePL2(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;
   Mobj     *mo;
   const int tnum = E_SafeThingType(MT_MACEFX4);

   if(!player)
      return;

   //player->ammo[am_mace] -= deathmatch ? USE_MACE_AMMO_1 : USE_MACE_AMMO_2;
   // FIXME: This needs to do the above behaviour:
   // to wit, fire the wimpy version's amount of ammo if in deathmatch
   P_SubtractAmmo(player, -1);
   mo = P_SpawnPlayerMissile(player->mo, tnum);
   if(mo)
   {
      mobjinfo_t *fx = mobjinfo[tnum];
      const fixed_t slope = P_PlayerPitchSlope(player);

      mo->momx += player->mo->momx;
      mo->momy += player->mo->momy;
      mo->momz = FixedMul(fx->speed, slope) + (2 * FRACUNIT);
      if(clip.linetarget)
         P_SetTarget(&mo->tracer, clip.linetarget);
   }
   S_StartSound(player->mo, sfx_lobsht);
}

void A_DeathBallImpact(actionargs_t *actionargs)
{
   int      i;
   Mobj    *target, *ball = actionargs->actor;
   angle_t  angle;
   bool     newAngle;

   if((ball->z <= ball->floorz) && E_HitFloor(ball))
   {
      // Landed in some sort of liquid
      ball->remove();
      return;
   }
   if((ball->z <= ball->floorz) && ball->momz)
   {
      // Bounce
      newAngle = false;
      target = ball->tracer;
      if(target)
      {
         if(!(target->flags & MF_SHOOTABLE))
         {
            // Target died
            P_ClearTarget(ball->tracer);
         }
         else
         {
            // Seek
            angle = R_PointToAngle2(ball->x, ball->y, target->x, target->y);
            newAngle = true;
         }
      }
      else
      {
         // Find new target
         angle = 0;
         for(i = 0; i < 16; i++)
         {
            P_AimLineAttack(ball, angle, 10 * 64 * FRACUNIT, 0);
            if(clip.linetarget && ball->target != clip.linetarget)
            {
               P_SetTarget(&ball->tracer, clip.linetarget);
               angle = R_PointToAngle2(ball->x, ball->y,
                                       clip.linetarget->x, clip.linetarget->y);
               newAngle = true;
               break;
            }
            angle += ANG45 / 2;
         }
      }
      if(newAngle)
      {
         ball->angle = angle;
         angle >>= ANGLETOFINESHIFT;
         ball->momx = FixedMul(ball->info->speed, finecosine[angle]);
         ball->momy = FixedMul(ball->info->speed, finesine[angle]);
      }
      P_SetMobjState(ball, ball->info->spawnstate);
      S_StartSound(ball, sfx_hpstop);
   }
   else
   {
      // Explode
      ball->flags |= MF_NOGRAVITY;
      ball->flags2 &= ~MF2_LOGRAV;
      S_StartSound(ball, sfx_phohit);
   }
}

void A_FireCrossbowPL1(actionargs_t *actionargs)
{
   Mobj     *pmo    = actionargs->actor;
   player_t *player = pmo->player;
   const int tnum   = E_SafeThingType(MT_CRBOWFX3);

   if(!player)
      return;

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

   if(!player)
      return;

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
      spark->x += P_SubRandom(pr_boltspark) * PO2(10);
      spark->y += P_SubRandom(pr_boltspark) * PO2(10);
   }
}

void A_FireBlasterPL1(actionargs_t *actionargs)
{
   Mobj     *mo = actionargs->actor;
   player_t *player = mo->player;
   angle_t   angle;
   int       damage;

   if(!player)
      return;

   P_WeaponSound(mo, sfx_gldhit);
   P_SubtractAmmo(player, -1);
   P_BulletSlope(mo);
   damage = (1 + (P_Random(pr_blaster) & 7)) * 4;
   angle  = mo->angle;
   if(player->refire)
      angle += P_SubRandom(pr_blaster) * PO2(18);

   const int    tnum = E_SafeThingType(MT_BLASTERPUFF1);
   edefstructvar(puffinfo_t, puff);
   puff.info = mobjinfo[tnum];
   puff.hitinfo = mobjinfo[E_SafeThingType(MT_BLASTERPUFF2)];

   P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage, &puff);
   P_WeaponSound(mo, sfx_blssht);
}

void A_FireSkullRodPL1(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;

   if(!player || !P_CheckAmmo(player))
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
   Mobj     *mo     = actionargs->actor;
   player_t *player = mo->player;
   const int tnum   = E_SafeThingType(MT_PHOENIXFX1);
   angle_t   angle;

   if(!player)
      return;

   P_SubtractAmmo(player, -1);
   P_SpawnPlayerMissile(mo, tnum);
   // Commented out fire-trail functionality
   //P_SpawnPlayerMissile(player->mo, E_SafeThingType(MT_MNTRFX2));
   angle = mo->angle + ANG180;
   angle >>= ANGLETOFINESHIFT;
   mo->momx += FixedMul(4 * FRACUNIT, finecosine[angle]);
   mo->momy += FixedMul(4 * FRACUNIT, finesine[angle]);
}

void A_InitPhoenixPL2(actionargs_t *actionargs)
{
   constexpr int FLAME_THROWER_TICS = 10 * 35;
   Mobj          *mo = actionargs->actor;
   player_t      *player = mo->player;

   if(!player)
      return;
   
   player->weaponctrs->setCounter(player, 0, FLAME_THROWER_TICS);
}

void A_FirePhoenixPL2(actionargs_t *actionargs)
{
   Mobj     *mo;
   Mobj     *pmo = actionargs->actor;
   player_t *player = pmo->player;
   angle_t   angle;
   fixed_t   x, y, z;

   if(!player)
      return;

   int &flamecount = *E_GetIndexedWepCtrForPlayer(player, 0);
   const fixed_t slope = P_PlayerPitchSlope(player);

   if(--flamecount == 0)
   {
      // Out of flame
      //P_SetPsprite(player, ps_weapon, player->psprites[ps_weapon].state->nextstate);
      state_t *state = E_GetWpnJumpInfo(player->readyweapon, "Powerdown");
      if(state != nullptr)
      {
         P_SetPsprite(player, ps_weapon, state->index);
         player->refire = 0;
      }
      else
      {
         doom_printf(FC_ERROR "A_FirePhoenixPL2: Calling weapon '%s' has no DECORATE "
                     "'Powerdown' state\a\n", player->readyweapon->name);
      }
      return;
   }
   pmo = player->mo;
   angle = pmo->angle;
   x = pmo->x + (P_SubRandom(pr_phoenixrod2) * PO2(9));
   y = pmo->y + (P_SubRandom(pr_phoenixrod2) * PO2(9));
   // REASONING: In Choco, slope = ((player->lookdir) << FRACBITS) / 173 + (FRACUNIT / 10)
   // In EE, slope = P_PlayerPitchSlope, so you can sub slope in to remove the lookdir from:
   // z = pmo->z + 26 * FRACUNIT + ((player->lookdir) << FRACBITS) / 173;
   // Also, the -= flooclip can be put on the line
   z = pmo->z + 26 * FRACUNIT + slope - (FRACUNIT / 10) - pmo->floorclip;
   mo = P_SpawnMobj(x, y, z, E_SafeThingType(MT_PHOENIXFX2));
   mo->target = pmo;
   mo->angle = angle;
   mo->momx = pmo->momx + FixedMul(mo->info->speed, finecosine[angle >> ANGLETOFINESHIFT]);
   mo->momy = pmo->momy + FixedMul(mo->info->speed, finesine[angle >> ANGLETOFINESHIFT]);
   mo->momz = FixedMul(mo->info->speed, slope);
   if(!player->refire || !(leveltime % 38))
      S_StartSound(player->mo, sfx_phopow);
   P_CheckMissileSpawn(mo);
}

void A_ShutdownPhoenixPL2(actionargs_t *actionargs)
{
   player_t *player = actionargs->actor->player;
   if(!player)
      return;
   P_SubtractAmmo(player, -1);
}

void A_GauntletAttack(actionargs_t *actionargs)
{
   arglist_t  *args   = actionargs->args;
   Mobj       *mo     = actionargs->actor;
   player_t   *player = mo->player;
   pspdef_t   *psp    = actionargs->pspr;
   fixed_t     dist;
   angle_t     angle;
   int damage, slope, randVal, tnum;

   if(!player || !psp)
      return;

   int powered = E_ArgAsInt(args, 0, 0);

   psp->sx = ((P_Random(pr_gauntlets) & 3) - 2) * FRACUNIT;
   psp->sy = WEAPONTOP + (P_Random(pr_gauntlets) & 3) * FRACUNIT;
   angle = mo->angle;

   if(powered)
   {
      damage = (1 + (P_Random(pr_gauntlets) & 7)) * 2;
      dist = 4 * MELEERANGE;
      angle += P_SubRandom(pr_gauntletsangle) * PO2(17);
      tnum = E_SafeThingType(MT_GAUNTLETPUFF2);
   }
   else
   {
      damage = (1 + (P_Random(pr_gauntlets) & 7)) * 2;
      dist = MELEERANGE + 1;
      angle += P_SubRandom(pr_gauntletsangle) * PO2(18);
      tnum = E_SafeThingType(MT_GAUNTLETPUFF1);
   }

   slope = P_AimLineAttack(mo, angle, dist, 0);
   edefstructvar(puffinfo_t, puff);
   puff.info = mobjinfo[tnum];
   puff.upspeed = 4*FRACUNIT/5;

   P_LineAttack(mo, angle, dist, slope, damage, &puff);

   if(!clip.linetarget)
   {
      if(P_Random(pr_gauntlets) > 64) // TODO: Maybe don't use pr_gauntlets?
         player->extralight = !player->extralight;
      P_WeaponSound(mo, sfx_gntful);
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
      P_WeaponSound(mo, sfx_gntpow);
   }
   else
      P_WeaponSound(mo, sfx_gnthit);

   // turn to face target
   angle = R_PointToAngle2(mo->x, mo->y, clip.linetarget->x, clip.linetarget->y);

   if(angle - mo->angle > ANG180)
   {
      if(angle - mo->angle < -ANG90 / 20)
         mo->angle = angle + ANG90 / 21;
      else
         mo->angle -= ANG90 / 20;
   }
   else
   {
      if(angle - mo->angle > ANG90 / 20)
         mo->angle = angle - ANG90 / 21;
      else
         mo->angle += ANG90 / 20;
   }

   mo->flags |= MF_JUSTATTACKED;
}

//=============================================================================
//
// New Eternity Heretic artifact codepointers
//

//
// This is effectively an Eternity version of P_ArtiTele from Heretic,
// which is what runs when a Chaos Device is used.
//
void A_HticArtiTele(actionargs_t *actionargs)
{
   fixed_t destX, destY;
   angle_t destAngle;
   Mobj *mo = actionargs->actor;

   if(deathmatch)
   {
      const int i = P_Random(pr_hereticartiteleport) % (deathmatch_p - deathmatchstarts);
      destX = deathmatchstarts[i].x;
      destY = deathmatchstarts[i].y;
      destAngle = ANG45 * (deathmatchstarts[i].angle / 45);
   }
   else
   {
      destX = playerstarts[0].x;
      destY = playerstarts[0].y;
      destAngle = ANG45 * (playerstarts[0].angle / 45);
   }

   P_HereticTeleport(mo, destX, destY, destAngle);
   S_StartSound(nullptr, sfx_hwpnup);
}

//
// Use action for Timebomb of the Ancients
void A_HticSpawnFireBomb(actionargs_t *actionargs)
{
   Mobj *mo = actionargs->actor, *bomb;
   angle_t angle = mo->angle >> ANGLETOFINESHIFT;
   // In vanilla this is bugged:
   // The footclip check turns into:
   //   (mo->flags2 & 1)
   // due to C operator precedence and a lack of parens/brackets.
   const fixed_t z = comp[comp_terrain] || !((mo->flags2 & MF2_FOOTCLIP) && E_HitFloor(mo)) ?
                     mo->z : mo->z - (15 * FRACUNIT);
   bomb = P_SpawnMobj(mo->x + (24 * finecosine[angle]),
                      mo->y + (24 * finesine[angle]),
                      z, E_SafeThingType(MT_HFIREBOMB));
   P_SetTarget(&bomb->target, mo->target);
}

// EOF

