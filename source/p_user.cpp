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
//      Player related stuff.
//      Bobbing POV/weapon, movement.
//      Pending weapon.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "a_small.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_gi.h"
#include "e_inventory.h"
#include "e_player.h"
#include "e_states.h"
#include "e_weapons.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "m_random.h"
#include "p_chase.h"
#include "p_info.h"
#include "p_map.h"
#include "p_map3d.h"
#include "p_maputl.h"
#include "p_portalcross.h"
#include "p_skin.h"
#include "p_spec.h"
#include "p_user.h"
#include "r_defs.h"
#include "r_main.h"
#include "s_sound.h"
#include "sounds.h"
#include "st_stuff.h"

//
// Movement.
//

// 16 pixels of bob

#define MAXBOB  0x100000

bool onground; // whether player is on ground or in air

bool pitchedflight = true;
bool default_pitchedflight = true;

//
// P_SetDisplayPlayer
//
// Sets the current display player.
//
void P_SetDisplayPlayer(int new_displayplayer)
{
   displayplayer = new_displayplayer;

   ST_Start();
   HU_Start();
   S_UpdateSounds(players[displayplayer].mo);
   P_ResetChasecam();
}

//
// P_Thrust
// Moves the given origin along a given angle.
//
// davidph 06/06/12: Added pitch.
//
void P_Thrust(player_t *player, angle_t angle, angle_t pitch, fixed_t move)
{
   if(pitch)
   {
      pitch >>= ANGLETOFINESHIFT;
      player->mo->momz -= FixedMul(move, finesine[pitch]);
      move = FixedMul(move, finecosine[pitch]);
   }

   player->mo->momx += FixedMul(move, finecosine[angle >>= ANGLETOFINESHIFT]);
   player->mo->momy += FixedMul(move, finesine[angle]);
}

//
// P_Bob
// Same as P_Thrust, but only affects bobbing.
//
// killough 10/98: We apply thrust separately between the real physical player
// and the part which affects bobbing. This way, bobbing only comes from player
// motion, nothing external, avoiding many problems, e.g. bobbing should not
// occur on conveyors, unless the player walks on one, and bobbing should be
// reduced at a regular rate, even on ice (where the player coasts).
//
// davidph 06/06/12: Added pitch. (Though only used to determine the true move.)
//
void P_Bob(player_t *player, angle_t angle, angle_t pitch, fixed_t move)
{
   // e6y
   if(demo_version < 203)
      return;

   if(pitch)
   {
      pitch >>= ANGLETOFINESHIFT;
    //player->momz -= FixedMul(move, finesine[pitch]);
      move = FixedMul(move, finecosine[pitch]);
   }

   player->momx += FixedMul(move,finecosine[angle >>= ANGLETOFINESHIFT]);
   player->momy += FixedMul(move,finesine[angle]);
}

//
// P_CalcHeight
//
// Calculate the walking / running height adjustment
//
void P_CalcHeight(player_t *player)
{
   int     angle;
   fixed_t bob;

   // Regular movement bobbing
   // (needs to be calculated for gun swing
   // even if not on ground)
   // OPTIMIZE: tablify angle
   // Note: a LUT allows for effects
   //  like a ramp with low health.
   
   // killough 10/98: Make bobbing depend only on player-applied motion.
   //
   // Note: don't reduce bobbing here if on ice: if you reduce bobbing here,
   // it causes bobbing jerkiness when the player moves from ice to non-ice,
   // and vice-versa.
   //
   // haleyjd: cph found out this can affect demo sync due to
   // differences it introduces in firing height etc. so it needs to be
   // optioned.
   // 04/11/10: refactored

   player->bob = 0;
   if(demo_version >= 203)
   {
      if(player_bobbing)
      {
         player->bob = (FixedMul(player->momx, player->momx) +
                        FixedMul(player->momy, player->momy)) >> 2;
      }
   }
   else
   {
      if(demo_compatibility || player_bobbing)
      {
         player->bob = (FixedMul(player->mo->momx, player->mo->momx) +
                        FixedMul(player->mo->momy, player->mo->momy)) >> 2;
      }
   }

   // haleyjd 04/11/10:
   // e6y
   if(demo_version == 202 && player->mo->friction > ORIG_FRICTION) // ice?
   {
      if(player->bob > (MAXBOB>>2))
         player->bob = MAXBOB>>2;
   }
   else
   {
      if(player->bob > MAXBOB)
         player->bob = MAXBOB;
   }

   // haleyjd 06/05/12: flying players
   if(player->mo->flags4 & MF4_FLY && !P_OnGroundOrThing(*player->mo))
      player->bob = FRACUNIT / 2;

   if(!onground || player->cheats & CF_NOMOMENTUM)
   {
      player->viewz = player->mo->z + player->pclass->viewheight;
      
      if(player->viewz > player->mo->zref.ceiling - 4 * FRACUNIT)
         player->viewz = player->mo->zref.ceiling - 4 * FRACUNIT;

      // phares 2/25/98:
      // The following line was in the Id source and appears
      // to be a bug. player->viewz is checked in a similar
      // manner at a different exit below.
      
      // player->viewz = player->mo->z + player->viewheight;
      
      return;
   }

   angle = (FINEANGLES / 20 * leveltime) & FINEMASK;
   bob   = FixedMul(player->bob / 2, finesine[angle]);

   // move viewheight
   
   if(player->playerstate == PST_LIVE)
   {
      player->viewheight += player->deltaviewheight;
      
      if(player->viewheight > player->pclass->viewheight)
      {
         player->viewheight = player->pclass->viewheight;
         player->deltaviewheight = 0;
      }

      if(player->viewheight < player->pclass->viewheight / 2)
      {
         player->viewheight = player->pclass->viewheight / 2;
         if(player->deltaviewheight <= 0)
            player->deltaviewheight = 1;
      }

      if(player->deltaviewheight)
      {
         player->deltaviewheight += FRACUNIT / 4;
         if(!player->deltaviewheight)
            player->deltaviewheight = 1;
      }
   }

   player->viewz = player->mo->z + player->viewheight + bob;

   // haleyjd 08/07/04: new floorclip system
   if(player->mo->floorclip && player->playerstate != PST_DEAD && 
      player->mo->z <= player->mo->zref.floor)
   {
      player->viewz -= player->mo->floorclip;
   }
   
   if(player->viewz > player->mo->zref.ceiling - 4 * FRACUNIT)
      player->viewz = player->mo->zref.ceiling - 4 * FRACUNIT;
}

//
// P_PlayerFlight
//
// haleyjd 06/05/12: flying logic for players
//
static void P_PlayerFlight(player_t *player, const ticcmd_t *cmd)
{
   int fly = cmd->fly;

   if(fly && player->powers[pw_flight])
   {
      if(fly != FLIGHT_CENTER)
      {
         player->flyheight = fly * 2;

         if(!(player->mo->flags4 & MF4_FLY))
            P_PlayerStartFlight(player, false);
      }
      else
         P_PlayerStopFlight(player);
   }
   else if(fly > 0 && estrnonempty(GameModeInfo->autoFlightArtifact) &&
           E_GetItemOwnedAmountName(player, GameModeInfo->autoFlightArtifact) >= 1)
   {
      E_TryUseItem(player, E_ItemIDForName(GameModeInfo->autoFlightArtifact));
   }

   if(player->mo->flags4 & MF4_FLY)
   {
      if(player->mo->intflags & MIF_CLEARMOMZ)
      {
         player->mo->momz = 0;
         player->mo->intflags &= ~MIF_CLEARMOMZ;
      }

      if(player->flyheight)
      {
         player->mo->momz = player->flyheight * FRACUNIT;
         player->flyheight /= 2;
         // When flyheight turns to 0 from this location, mark it to become 0 instead of letting it
         // be subject to P_ZMovement flying friction.
         if(!(GameModeInfo->flags & GIF_FLIGHTINERTIA) && !player->flyheight)
            player->mo->intflags |= MIF_CLEARMOMZ;
      }
   }
}

//
// P_MovePlayer
//
// Adds momentum if the player is not in the air
//
// killough 10/98: simplified
//
void P_MovePlayer(player_t* player)
{
   const ticcmd_t *cmd = &player->cmd;
   Mobj *mo = player->mo;
   
   mo->angle += cmd->angleturn << 16;
   
   // haleyjd: OVER_UNDER
   // 06/05/12: flying players
   onground = P_OnGroundOrThing(*mo) || (mo->flags4 & MF4_FLY);
   
   // killough 10/98:
   //
   // We must apply thrust to the player and bobbing separately, to avoid
   // anomalies. The thrust applied to bobbing is always the same strength on
   // ice, because the player still "works just as hard" to move, while the
   // thrust applied to the movement varies with 'movefactor'.
   
   if((!demo_compatibility && demo_version < 203) || 
      (cmd->forwardmove | cmd->sidemove)) // killough 10/98
   {
      if(onground || mo->flags & MF_BOUNCES) // killough 8/9/98
      {
         int friction, movefactor = P_GetMoveFactor(mo, &friction);

         // killough 11/98:
         // On sludge, make bobbing depend on efficiency.
         // On ice, make it depend on effort.
         
         int bobfactor =
            friction < ORIG_FRICTION ? movefactor : ORIG_FRICTION_FACTOR;

         // davidph 06/06/12: pitch-to-fly
         fixed_t pitch = player->pitch;

         if(!(mo->flags4 & MF4_FLY) || !pitchedflight)
            pitch = 0;

         if(cmd->forwardmove)
         {
            P_Bob(player, mo->angle, pitch, cmd->forwardmove*bobfactor);
            P_Thrust(player, mo->angle, pitch, cmd->forwardmove*movefactor);
         }
         
         if(cmd->sidemove)
         {
            P_Bob(player, mo->angle-ANG90, 0, cmd->sidemove*bobfactor);
            P_Thrust(player, mo->angle-ANG90, 0, cmd->sidemove*movefactor);
         }
      }
      else if(LevelInfo.airControl > 0 || LevelInfo.airControl < -1)
      {
         // Do not move player unless aircontrol
         // -1 has a special meaning that totally disables air control, even
         // if the compatibility flag is unset.

         // This is a new EMAPINFO property and doesn't emulate Hexen and
         // Strife. For those, look below.

         int friction, movefactor = P_GetMoveFactor(mo, &friction);

         movefactor = FixedMul(movefactor, LevelInfo.airControl);

         if(cmd->forwardmove)
            P_Thrust(player, mo->angle, 0, cmd->forwardmove*movefactor);

         // TODO: disable this part in Strife
         if(cmd->sidemove)
            P_Thrust(player, mo->angle - ANG90, 0, cmd->sidemove*movefactor);
      }
      else if(LevelInfo.airControl == 0 && E_CanJump(*player->pclass))
      {
         // Apply legacy Hexen/Strife primitive air control if air control is 0
         // (default) and the compatibility setting is "NO".

         if(cmd->forwardmove)
            P_Thrust(player, mo->angle, 0, FRACUNIT >> 8);

         // TODO: disable this in Strife
         if(cmd->sidemove)
            P_Thrust(player, mo->angle, 0, FRACUNIT >> 8);
      }

      if(mo->state == states[mo->info->spawnstate])
         P_SetMobjState(mo, mo->info->seestate);
   }

   // haleyjd 06/05/12: flight
   P_PlayerFlight(player, cmd);
}

#define ANG5 (ANG90/18)

//
// P_DeathThink
//
// Fall on your face when dying.
// Decrease POV height to floor height.
//
void P_DeathThink(player_t *player)
{
   angle_t angle;
   angle_t delta;
   
   P_MovePsprites(player);
   
   // fall to the ground
   
   if(player->viewheight > 6 * FRACUNIT)
      player->viewheight -= FRACUNIT;
   
   if(player->viewheight < 6 * FRACUNIT)
      player->viewheight = 6 * FRACUNIT;
   
   player->deltaviewheight = 0;

   // haleyjd: never bob player view when dead, and always treat player like
   //          he is on the ground
   if(demo_version >= 333)
   {
      onground = true;
      player->momx = player->momy = 0;
   }
   else
      onground = P_OnGroundOrThing(*player->mo);

   P_CalcHeight(player);
   
   if(player->attacker && player->attacker != player->mo)
   {
      // ioanch 20151226: use portal-translated coordinates if needed
      angle = P_PointToAngle(player->mo->x,
                             player->mo->y,
                             getThingX(player->mo, player->attacker),
                             getThingY(player->mo, player->attacker));

      delta = angle - player->mo->angle;
      
      if(delta < ANG5 || delta > (unsigned int)-ANG5)
      {
         // Looking at killer,
         //  so fade damage flash down.
         
         player->mo->angle = angle;
         
         if(player->damagecount)
            player->damagecount--;
      }
      else 
         if(delta < ANG180)
            player->mo->angle += ANG5;
         else
            player->mo->angle -= ANG5;
   }
   else if(player->damagecount)
      player->damagecount--;

   // haleyjd 10/05/08:
   // handle looking slightly up when the player is attached to a non-player
   // object and is dead. This was done for the decapitation deaths in Heretic
   // and Hexen.
   if(!E_IsPlayerClassThingType(player->mo->type))
   {
      player->prevpitch = player->pitch;
      if(player->mo->z <= player->mo->zref.floor && player->pitch > -ANGLE_1 * 15)
         player->pitch -= 2*ANGLE_1/3;
   }

   if(player->cmd.buttons & BT_USE)
      player->playerstate = PST_REBORN;
}

//
// P_HereticCurrent
//
// Applies Heretic current effects to the player.
//
// haleyjd 09/09/07: Rewritten to use msecnodes and eliminate the redundant 
// Mobj::floorsec field.
//
static void P_HereticCurrent(player_t *player)
{
   msecnode_t *m;
   Mobj     *thing = player->mo;

   // don't affect the player if noclipping is on (pushes you through walls)
   if(thing->flags & MF_NOCLIP)
      return;
   
   // determine what touched sector the player is standing on
   for(m = thing->touching_sectorlist; m; m = m->m_tnext)
   {
      if(thing->z == m->m_sector->srf.floor.height)
         break;
   }

   if(m)
   {
      sector_t *sec = m->m_sector;

      if(sec->hticPushType == SECTOR_HTIC_CURRENT)
         P_Thrust(player, sec->hticPushAngle, 0, sec->hticPushForce);
   }
}

//
// P_doTorchFlicker
//
// haleyjd 08/31/13: apply flickering effect when player has a torch
//
static void P_doTorchFlicker(player_t *player)
{
   // if infinite duration, or just starting, set fixedcolormap
   // if it's out of range of the torch effect
   if(player->powers[pw_torch] < 0 ||
      player->powers[pw_torch] >= INFRATICS - 1)
   {
      if(!player->fixedcolormap || player->fixedcolormap > 7)
         player->fixedcolormap = 1;
   }

   if(leveltime & 16)
      return;

   if(player->newtorch)
   {
      if(player->fixedcolormap + player->torchdelta > 7 ||
         player->fixedcolormap + player->torchdelta < 1 ||
         player->newtorch == player->fixedcolormap)
      {
         player->newtorch = 0;
      }
      else
         player->fixedcolormap += player->torchdelta;
   }
   else
   {
      player->newtorch = (M_Random() & 7) + 1;
      if(player->newtorch == player->fixedcolormap)
         player->torchdelta = 0;
      else if(player->newtorch > player->fixedcolormap)
         player->torchdelta = 1;
      else
         player->torchdelta = -1;
   }
}

enum
{
   PLS_NONE,
   PLS_LIGHTAMP,
   PLS_TORCH
};

//
// P_PlayerLightSourceType
//
// Return the type of light source the player is using.
//
static int P_PlayerLightSourceType(player_t *player)
{
   // infrared is higher priority than torch
   if(player->powers[pw_infrared])
      return PLS_LIGHTAMP;
   else if(player->powers[pw_torch])
      return PLS_TORCH;
   else
      return PLS_NONE;
}

//
// P_PlayerLightTics
//
// haleyjd 08/31/13: get the player's light power tics
//
static int P_PlayerLightTics(player_t *player)
{
   switch(P_PlayerLightSourceType(player))
   {
   case PLS_LIGHTAMP:
      return player->powers[pw_infrared];
   case PLS_TORCH:
      return player->powers[pw_torch];
   default:
      return 0;
   }
}

//
// P_SectorIsSpecial
//
// haleyjd 12/28/08: Determines whether or not a sector is special.
//
inline static bool P_SectorIsSpecial(const sector_t *sector)
{
   return (sector->special || sector->flags || sector->damage);
}

//
// P_PlayerThink
//
void P_PlayerThink(player_t *player)
{
   ticcmd_t*    cmd;

   // haleyjd 01/04/14: backup viewz and mobj location for interpolation
   player->prevviewz = player->viewz;
   player->mo->backupPosition();

   // killough 2/8/98, 3/21/98:
   // (this code is necessary despite questions raised elsewhere in a comment)

   if(player->cheats & CF_NOCLIP)
      player->mo->flags |= MF_NOCLIP;
   else
      player->mo->flags &= ~MF_NOCLIP;

   // chain saw run forward

   cmd = &player->cmd;

   if(cmd->itemID && (demo_version >= 401 || vanilla_heretic))
      E_TryUseItem(player, cmd->itemID - 1); // ticcmd ID is off by one

   if(player->mo->flags & MF_JUSTATTACKED)
   {
      cmd->angleturn = 0;
      cmd->forwardmove = 0xc800/512;
      cmd->sidemove = 0;
      player->mo->flags &= ~MF_JUSTATTACKED;
   }

   if(player->playerstate == PST_DEAD)
   {
      P_DeathThink(player);
      return;
   }

   // haleyjd 04/03/05: new yshear code
   if(!allowmlook)
      player->prevpitch = player->pitch = 0;
   else
   {
      player->prevpitch = player->pitch;
      int look = cmd->look;

      if(look && (!player->mo->reactiontime || demo_version < 342))
      {
         // test for special centerview value
         if(look == -32768)
            player->pitch = 0;
         else
         {
            player->pitch -= look << 16;
            if(player->pitch < -ANGLE_1*MAXPITCHUP)
               player->pitch = -ANGLE_1*MAXPITCHUP;
            else if(player->pitch > ANGLE_1*MAXPITCHDOWN)
               player->pitch = ANGLE_1*MAXPITCHDOWN;
         }
      }
   }

   // haleyjd: count down jump timer
   if(player->jumptime)
      player->jumptime--;

   // Move around.
   // Reactiontime is used to prevent movement
   //  for a bit after a teleport.
   
   if(player->mo->reactiontime)
      player->mo->reactiontime--;
   else
   {
      P_MovePlayer(player);

      // Handle actions   -- joek 12/22/07
      // ioanch: not on demo_version lower than some amount. Was happening
      // accidentally in -vanilla.
      if(cmd->actions & AC_JUMP)
      {
         if(E_CanJump(*player->pclass))
         {
            if((player->mo->z == player->mo->zref.floor ||
                (player->mo->intflags & MIF_ONMOBJ)) && !player->jumptime)
            {
               if(strcasecmp(player->skin->sounds[sk_jump], "none"))
                  S_StartSound(player->mo, GameModeInfo->playerSounds[sk_jump]);
               player->mo->momz += player->pclass->jumpspeed;
               player->mo->intflags &= ~MIF_ONMOBJ;
               player->jumptime = 18;
            }
         }
         else
         {
            static int printtic = -3 * TICRATE;
            if(gametic >= printtic + 3 * TICRATE && player == &players[consoleplayer])
            {
               printtic = gametic;
               if(E_MayJumpIfOverriden(*player->pclass))
                  doom_printf("Jumping needs to be allowed in the settings.");
               else
                  doom_printf("Jumping not possible.");
            }
         }
      }
   }
  
   P_CalcHeight(player); // Determines view height and bobbing
   
   // haleyjd: are we falling? might need to scream :->
   if(!getComp(comp_fallingdmg) && demo_version >= 329)
   {  
      if(player->mo->momz >= 0)
         player->mo->intflags &= ~MIF_SCREAMED;

      if(player->mo->momz <= -35*FRACUNIT && 
         player->mo->momz >= -40*FRACUNIT &&
         !(player->mo->intflags & MIF_SCREAMED))
      {
         player->mo->intflags |= MIF_SCREAMED;
         S_StartSound(player->mo, GameModeInfo->playerSounds[sk_plfall]);
      }
   }

   // Determine if there's anything about the sector you're in that's
   // going to affect you, like painful floors.

   // ioanch 20160116: portal aware
   sector_t *sector = P_ExtremeSectorAtPoint(player->mo, surf_floor);
   if(P_SectorIsSpecial(sector))
      P_PlayerInSpecialSector(player, sector);

   // haleyjd 08/23/05: terrain-based effects
   P_PlayerOnSpecialFlat(player);

   // haleyjd: Heretic current specials
   P_HereticCurrent(player);
   
   // Check for weapon change.
   
   // A special event has no other buttons.

   if(cmd->buttons & BT_SPECIAL)
      cmd->buttons = 0;

   if(demo_version >= 401 || vanilla_heretic)
   {
      if(cmd->weaponID)
      {
         weaponinfo_t *wp = E_WeaponForID(cmd->weaponID - 1); // weaponID is off by one
         weaponinfo_t *sister = wp->sisterWeapon;
         if(player->powers[pw_weaponlevel2] && E_IsPoweredVariant(sister))
            player->pendingweapon = sister;
         else
            player->pendingweapon = wp;

         player->pendingweaponslot = E_FindEntryForWeaponInSlotIndex(player, wp, cmd->slotIndex);
      }
   }
   else if(cmd->buttons & BT_CHANGE)
   {
      // The actual changing of the weapon is done
      //  when the weapon psprite can do it
      //  (read: not in the middle of an attack).
      
      weapontype_t newweapon = (cmd->buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT;
      
      // killough 3/22/98: For demo compatibility we must perform the fist
      // and SSG weapons switches here, rather than in G_BuildTiccmd(). For
      // other games which rely on user preferences, we must use the latter.

      // WEAPON_FIXME: bunch of crap (compat weapon changing)

      if(demo_compatibility)
      { 
         // compatibility mode -- required for old demos -- killough
         // ioanch 20160426: imported the following line from PrBoom+
         //e6y
         newweapon = (cmd->buttons & BT_WEAPONMASK_OLD)>>BT_WEAPONSHIFT;

         if(newweapon == wp_fist && E_PlayerOwnsWeaponForDEHNum(player, wp_chainsaw) &&
            (!E_WeaponIsCurrentDEHNum(player, wp_chainsaw) ||
             !player->powers[pw_strength]))
            newweapon = wp_chainsaw;
         if(enable_ssg &&
            newweapon == wp_shotgun &&
            E_PlayerOwnsWeaponForDEHNum(player, wp_supershotgun) &&
            !E_WeaponIsCurrentDEHNum(player, wp_supershotgun))
            newweapon = wp_supershotgun;
      }

      // killough 2/8/98, 3/22/98 -- end of weapon selection changes

      // WEAPON_FIXME: setting pendingweapon

      weaponinfo_t *pendingweapon = E_WeaponForDEHNum(newweapon);
      if(E_PlayerOwnsWeapon(player, pendingweapon) &&
         pendingweapon->id != player->readyweapon->id)
      {
         // Do not go to plasma or BFG in shareware, even if cheated.
         // haleyjd 06/28/13: generalized for EDF weapon system
         weaponinfo_t *pendingweapon = P_GetPlayerWeapon(player, newweapon);
         
         if(pendingweapon && 
            !(GameModeInfo->flags & GIF_SHAREWARE && 
              pendingweapon->flags & WPF_NOTSHAREWARE))
         {
            player->pendingweapon = pendingweapon;
         }
      }
   }

   // check for use
   
   if(cmd->buttons & BT_USE)
   {
      if(!player->usedown)
      {
         P_UseLines(player);
         player->usedown = true;
      }
   }
   else
      player->usedown = false;

   // cycle psprites

   P_MovePsprites (player);

   // Counters, time dependent power ups.

   // Strength counts up to diminish fade.

   if(player->powers[pw_strength])
      player->powers[pw_strength]++;

   // killough 1/98: Make idbeholdx toggle:

   if(player->powers[pw_invulnerability] > 0) // killough
      player->powers[pw_invulnerability]--;

   if(player->powers[pw_invisibility] > 0)
   {
      if(!--player->powers[pw_invisibility] )
         player->mo->flags &= ~MF_SHADOW;
   }

   if(player->powers[pw_infrared] > 0)        // killough
      player->powers[pw_infrared]--;

   // haleyjd: torch
   if(player->powers[pw_torch] > 0)
      player->powers[pw_torch]--;

   if(player->powers[pw_ironfeet] > 0)        // killough
      player->powers[pw_ironfeet]--;

   if(player->powers[pw_ghost] > 0)        // haleyjd
   {
      if(!--player->powers[pw_ghost])
         player->mo->flags3 &= ~MF3_GHOST;
   }

   if(player->powers[pw_totalinvis] > 0) // haleyjd
   {
      if(!--player->powers[pw_totalinvis])
      {
         player->mo->flags2 &= ~MF2_DONTDRAW;
         player->mo->flags4 &= ~MF4_TOTALINVISIBLE;
      }
   }

   if(player->powers[pw_flight] > 0) // haleyjd 06/05/12
   {
      if(!--player->powers[pw_flight])
         P_PlayerStopFlight(player);
   }

   if(player->powers[pw_weaponlevel2] > 0) // MaxW: 2018/01/02
   {
      if(!--player->powers[pw_weaponlevel2])
      {
         // switch back to normal weapon if need be
         if(E_IsPoweredVariant(player->readyweapon))
         {
            // Note: sisterWeapon is guaranteed to != nullptr elsewhere
            weaponinfo_t *unpowered = player->readyweapon->sisterWeapon;
            if(player->readyweapon->flags & WPF_PHOENIXRESET &&
               player->psprites[ps_weapon].state->index != player->readyweapon->readystate &&
               player->psprites[ps_weapon].state->index != player->readyweapon->upstate)
            {
               P_SetPsprite(player, ps_weapon, unpowered->readystate);
               P_SubtractAmmo(player, -1);
               player->refire = 0;
            }
            else if(unpowered->flags & WPF_FORCETOREADY || player->attackdown == AT_NONE)
            {
               // TODO: Figure out if should be || (player->attackdown == AT_NONE && current-state-isireadystate)
               P_SetPsprite(player, ps_weapon, unpowered->readystate);
               player->refire = 0;
            }
            else if(player->readyweapon->flags & WPF_DEPOWERSWITCH)
               player->pendingweapon = unpowered;

            player->readyweapon = unpowered;
         }
      }
   }

   if(player->damagecount)
      player->damagecount--;

   if(player->bonuscount)
      player->bonuscount--;

   // get length of time left on any active lighting powerup
   int lighttics   = P_PlayerLightTics(player);
   int lightsource = P_PlayerLightSourceType(player);

   // Handling colormaps.
   if(player->powers[pw_invulnerability])
   {
      if(player->powers[pw_invulnerability] > 4*32 || 
         player->powers[pw_invulnerability] & 8)
         player->fixedcolormap = INVERSECOLORMAP;
      else
         player->fixedcolormap = 0;
   }
   else if(lightsource != PLS_NONE)
   {
      if(lighttics > 0 && lighttics <= 4*32) // fading out?
         player->fixedcolormap = ((lighttics & 8) != 0);
      else
      {
         // haleyjd: if player has a torch, do flickering
         if(lightsource == PLS_TORCH)
            P_doTorchFlicker(player);
         else
            player->fixedcolormap = 1; // almost full bright
      }
   }
   else
      player->fixedcolormap = 0;

   // haleyjd 01/21/07: clear earthquake flag before running quake thinkers later
   player->quake = 0;
}

//
// P_SetPlayerAttacker
//
// haleyjd 09/30/2011: Needed function to fix a BOOM problem wherein 
// player_t::attacker is not properly reference-counted against the
// Mobj to which it points.
//
// Usually the worst consequence of this problem was having your view
// spin around to a pseudo-random angle when watching a Lost Soul that
// had killed you be removed from the game world.
//
void P_SetPlayerAttacker(player_t *player, Mobj *attacker)
{
   if(full_demo_version >= make_full_version(340, 17))
      P_SetTarget<Mobj>(&player->attacker, attacker);
   else
      player->attacker = attacker;
}

//
// P_PlayerStartFlight
//
// Call this to start the player flying.
//
void P_PlayerStartFlight(player_t *player, bool thrustup)
{
   if(full_demo_version < make_full_version(340, 23) && !vanilla_heretic)
      return;

   player->mo->flags4 |= MF4_FLY;
   player->mo->flags  |= MF_NOGRAVITY;

   if(thrustup && player->mo->z <= player->mo->zref.floor)
      player->flyheight = 2 * FLIGHT_IMPULSE_AMT;

   // TODO: stop screaming if falling
}

//
// P_PlayerStopFlight
//
// Call this to make the player stop flying.
//
void P_PlayerStopFlight(player_t *player)
{
   if(full_demo_version < make_full_version(340, 23) && !vanilla_heretic)
      return;

   player->mo->flags4 &= ~MF4_FLY;
   player->mo->flags  &= ~MF_NOGRAVITY;
}

#if 0
// Small native functions for player stuff

static cell AMX_NATIVE_CALL sm_getplayername(AMX *amx, cell *params)
{
   int err, pnum, packed;
   cell *cstr;

   pnum   = (int)params[1] - 1;
   packed = (int)params[3];

   if((err = amx_GetAddr(amx, params[2], &cstr)) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return 0;
   }

   if(pnum < 0 || pnum >= MAXPLAYERS)
   {
      amx_RaiseError(amx, AMX_ERR_BOUNDS);
      return 0;
   }

   if(!playeringame[pnum])
      amx_SetString(cstr, "null", packed, 0);
   else
      amx_SetString(cstr, players[pnum].name, packed, 0);

   return 0;
}

AMX_NATIVE_INFO user_Natives[] =
{
   { "_GetPlayerName", sm_getplayername },
   { nullptr, nullptr }
};
#endif

//----------------------------------------------------------------------------
//
// $Log: p_user.c,v $
// Revision 1.14  1998/05/12  12:47:25  phares
// Removed OVER UNDER code
//
// Revision 1.13  1998/05/10  23:38:04  killough
// Add #include p_user.h to ensure consistent prototypes
//
// Revision 1.12  1998/05/05  15:35:20  phares
// Documentation and Reformatting changes
//
// Revision 1.11  1998/05/03  23:21:04  killough
// Fix #includes and remove unnecessary decls at the top, nothing else
//
// Revision 1.10  1998/03/23  15:24:50  phares
// Changed pushers to linedef control
//
// Revision 1.9  1998/03/23  03:35:24  killough
// Move weapons changes to G_BuildTiccmd, fix idclip
//
// Revision 1.8  1998/03/12  14:28:50  phares
// friction and IDCLIP changes
//
// Revision 1.7  1998/03/09  18:26:55  phares
// Fixed bug in neighboring variable friction sectors
//
// Revision 1.6  1998/02/27  08:10:08  phares
// Added optional player bobbing
//
// Revision 1.5  1998/02/24  08:46:42  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.4  1998/02/15  02:47:57  phares
// User-defined keys
//
// Revision 1.3  1998/02/09  03:13:20  killough
// Improve weapon control and add preferences
//
// Revision 1.2  1998/01/26  19:24:34  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:01  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

