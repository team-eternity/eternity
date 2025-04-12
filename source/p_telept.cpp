//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Teleportation.
// Authors: James Haley, Stephen McGranahan, Ioan Chera
//

#include "z_zone.h"

#include "a_small.h"
#include "d_gi.h"
#include "doomstat.h"
#include "e_things.h"
#include "m_collection.h"
#include "m_random.h"
#include "p_chase.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_portalcross.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_user.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"

// ioanch 20160423: helper
static void P_dropToFloor(Mobj *thing, player_t *player)
{
   // haleyjd 12/15/02: cph found out this was removed
   // in Final DOOM, so don't do it for Final DOOM demos.
   if(!(demo_compatibility &&
      (GameModeInfo->missionInfo->flags & MI_NOTELEPORTZ)))
   {
      // SoM: so yeah... Need this for linked portals.
      if(demo_version >= 333)
         thing->z = thing->zref.secfloor;
      else
         thing->z = thing->zref.floor;
   }

   if(player)
   {
      player->viewz = thing->z + player->viewheight;
      player->prevviewz = player->viewz;
   }
}

//
// P_Teleport from Heretic, but made to be more generic and Eternity-ready
// FIXME: This should be made more generic, so that other stuff can benefit from it
//
bool P_HereticTeleport(Mobj *thing, fixed_t x, fixed_t y, angle_t angle, bool alwaysfrag)
{
   fixed_t oldx, oldy, oldz, aboveFloor, fogDelta;
   player_t *player;
   unsigned an;

   oldx = thing->x;
   oldy = thing->y;
   oldz = thing->z;
   aboveFloor = thing->z - thing->zref.floor;
   if(!P_TeleportMove(thing, x, y, alwaysfrag ? TELEMOVE_FRAG : 0))
      return false;

   if((player = thing->player))
   {
      if(player->powers[pw_flight].isActive() && aboveFloor)
      {
         thing->z = thing->zref.floor + aboveFloor;
         if(thing->z + thing->height > thing->zref.ceiling)
            thing->z = thing->zref.ceiling - thing->height;
         player->prevviewz = player->viewz = thing->z + player->viewheight;
      }
      else
      {
         P_dropToFloor(thing, player);
         player->prevpitch = player->pitch = 0;
      }
   }
   else if(thing->flags & MF_MISSILE)
   {
      thing->z = thing->zref.floor + aboveFloor;
      if(thing->z + thing->height > thing->zref.ceiling)
         thing->z = thing->zref.ceiling - thing->height;
   }
   else
      P_dropToFloor(thing, nullptr);

   if(thing->player == players + displayplayer)
      P_ResetChasecam();

   // Spawn teleport fog at source and destination
   const int fogNum = E_SafeThingName(GameModeInfo->teleFogType);
   fogDelta = thing->flags & MF_MISSILE ? 0 : GameModeInfo->teleFogHeight;
   S_StartSound(P_SpawnMobj(oldx, oldy, oldz + fogDelta, fogNum),
                GameModeInfo->teleSound);
   an = angle >> ANGLETOFINESHIFT;
   v2fixed_t pos = P_LinePortalCrossing(x, y, 20 * finecosine[an], 20 * finesine[an]);
   S_StartSound(P_SpawnMobj(pos.x, pos.y, thing->z + fogDelta, fogNum), GameModeInfo->teleSound);

   // Freeze player for ~.5s, but only if they don't have tome active
   if(thing->player && !thing->player->powers[pw_weaponlevel2].isActive())
      thing->reactiontime = 18;
   thing->angle = angle;
   if(thing->flags & MF_MISSILE)
   {
      angle >>= ANGLETOFINESHIFT;
      thing->momx = FixedMul(thing->info->speed, finecosine[angle]);
      thing->momy = FixedMul(thing->info->speed, finesine[angle]);
   }
   else
      thing->momx = thing->momy = thing->momz = 0;

   // killough 10/98: kill all bobbing momentum too
   if(player)
      player->momx = player->momy = 0;

   thing->backupPosition();
   P_AdjustFloorClip(thing);

   return true;
}

//
// Gets the telefog coordinates for an arrival telefog, which must be in front of the thing
//
v3fixed_t P_GetArrivalTelefogLocation(v3fixed_t landing, angle_t angle)
{
   v2fixed_t pos = P_LinePortalCrossing(landing.x, landing.y, 
                                        20 * finecosine[angle >> ANGLETOFINESHIFT], 
                                        20 * finesine[angle >> ANGLETOFINESHIFT]);
   return { pos.x, pos.y, landing.z + GameModeInfo->teleFogHeight };
}

//
// ioanch 20160330
// General teleportation routine. Needed because it's reused by Hexen's teleport
//
static int P_Teleport(Mobj *thing, const Mobj *landing, bool alwaysfrag)
{
   if(GameModeInfo->type == Game_Heretic)
      return P_HereticTeleport(thing, landing->x, landing->y, landing->angle, alwaysfrag);

   fixed_t oldx = thing->x, oldy = thing->y, oldz = thing->z;
   player_t *player = thing->player;
            
   // killough 5/12/98: exclude voodoo dolls:
   if(player && player->mo != thing)
      player = nullptr;

   if(!P_TeleportMove(thing, landing->x, landing->y, alwaysfrag ? TELEMOVE_FRAG : 0)) // killough 8/9/98
      return 0;

   P_dropToFloor(thing, player);

   thing->angle = landing->angle;

   // sf: reset the chasecam at its new position.
   //     this needs to be done before startsound so we hear
   //     the teleport sound when using the chasecam
   if(thing->player == players + displayplayer)
      P_ResetChasecam();

   // spawn teleport fog and emit sound at source
   S_StartSound(P_SpawnMobj(oldx, oldy, 
                              oldz + GameModeInfo->teleFogHeight, 
                              E_SafeThingName(GameModeInfo->teleFogType)), 
                  GameModeInfo->teleSound);

   // spawn teleport fog and emit sound at destination
   // ioanch 20160229: portal aware
   v3fixed_t landingFogPos = P_GetArrivalTelefogLocation({ landing->x, landing->y, thing->z },
                                                         landing->angle);
   S_StartSound(P_SpawnMobj(landingFogPos.x, landingFogPos.y, landingFogPos.z, 
                            E_SafeThingName(GameModeInfo->teleFogType)), 
                GameModeInfo->teleSound);
            
   thing->backupPosition();
   P_AdjustFloorClip(thing);

   // don't move for a bit // killough 10/98
   if(thing->player)
      thing->reactiontime = 18;

   // kill all momentum
   thing->momx = thing->momy = thing->momz = 0;
            
   // killough 10/98: kill all bobbing momentum too
   if(player)
      player->momx = player->momy = 0;
 
   return 1;
}

//
// ioanch 20160423: silent teleportation routine
// Use Hexen restriction parameter in case it's required
//
static int P_SilentTeleport(Mobj *thing, const line_t *line,
                            const Mobj *m, struct teleparms_t parms)
{
   // Height of thing above ground, in case of mid-air teleports:
   fixed_t z = thing->z - thing->zref.floor;

   // Get the angle between the exit thing and source linedef.
   // Rotate 90 degrees, so that walking perpendicularly across
   // teleporter linedef causes thing to exit in the direction
   // indicated by the exit thing.

   // ioanch: in case of Hexen, don't change angle
   angle_t angle;
   switch(parms.teleangle)
   {
      default:
      case teleangle_keep:
         angle = 0;
         break;
      case teleangle_absolute:
         angle = m->angle - thing->angle;
         break;
      case teleangle_relative_boom:
         // Fall back to absolute if line is missing
         angle = line ? P_PointToAngle(0, 0, line->dx, line->dy) - m->angle
         + ANG90 : m->angle - thing->angle;
         break;
      case teleangle_relative_correct:
         angle = line ? m->angle - (P_PointToAngle(0, 0, line->dx, line->dy)
         + ANG90) : m->angle - thing->angle;
         break;
   }

   // Sine, cosine of angle adjustment
   fixed_t s = finesine[angle>>ANGLETOFINESHIFT];
   fixed_t c = finecosine[angle>>ANGLETOFINESHIFT];

   // Momentum of thing crossing teleporter linedef
   fixed_t momx = thing->momx;
   fixed_t momy = thing->momy;

   // Whether this is a player, and if so, a pointer to its player_t
   player_t *player = thing->player;

   // Attempt to teleport, aborting if blocked
   if(!P_TeleportMove(thing, m->x, m->y, parms.alwaysfrag ? TELEMOVE_FRAG : 0)) // killough 8/9/98
      return 0;

   // Rotate thing according to difference in angles
   thing->angle += angle;

   if(!parms.keepheight)
      P_dropToFloor(thing, player);
   else
   {
      // Adjust z position to be same height above ground as before
      thing->z = z + thing->zref.floor;
   }

   // Hexen (teleangle_keep) behavior doesn't even touch the velocity
   if(parms.teleangle != teleangle_keep)
   {
      // Rotate thing's momentum to come out of exit just like it entered
      thing->momx = FixedMul(momx, c) - FixedMul(momy, s);
      thing->momy = FixedMul(momy, c) + FixedMul(momx, s);
   }

   // Adjust player's view, in case there has been a height change
   // Voodoo dolls are excluded by making sure player->mo == thing.
   if(player && player->mo == thing)
   {
      // Save the current deltaviewheight, used in stepping
      fixed_t deltaviewheight = player->deltaviewheight;

      // Clear deltaviewheight, since we don't want any changes
      player->deltaviewheight = 0;

      // Set player's view according to the newly set parameters
      P_CalcHeight(*player);

      player->prevviewz = player->viewz;

      // Reset the delta to have the same dynamics as before
      player->deltaviewheight = deltaviewheight;

      if(player == players+displayplayer)
         P_ResetChasecam();
   }

   thing->backupPosition();
   P_AdjustFloorClip(thing);

   return 1;
}

//
// TELEPORTATION
//
// killough 5/3/98: reformatted, cleaned up
// ioanch 20160330: modified not to use line parameter
//
int EV_Teleport(int tag, int side, Mobj *thing, bool alwaysfrag)
{
   Thinker *thinker;
   Mobj    *m;
   int       i;

   // don't teleport missiles
   // Don't teleport if hit back of line,
   //  so you can get out of teleporter.
   if(!thing || side || (thing->flags & MF_MISSILE && !(thing->flags3 & MF3_TELESTOMP)))
      return 0;

   // killough 1/31/98: improve performance by using
   // P_FindSectorFromLineArg0 instead of simple linear search.

   for(i = -1; (i = P_FindSectorFromTag(tag, i)) >= 0;)
   {
      for(thinker = thinkercap.next; thinker != &thinkercap; thinker = thinker->next)
      {
         if(!(m = thinker_cast<Mobj *>(thinker)))
            continue;

         if(m->type == E_ThingNumForDEHNum(MT_TELEPORTMAN) &&
            m->subsector->sector-sectors == i)
         {
            return P_Teleport(thing, m, alwaysfrag);
         }
      }
   }
   return 0;
}

//
// ioanch 20160423: for parameterized teleport that has both tid and tag
//
static const Mobj *P_pickRandomLanding(int tid, int tag)
{
   Mobj *mobj = nullptr;
   PODCollection<Mobj *> destColl;
   while((mobj = P_FindMobjFromTID(tid, mobj, nullptr)))
   {
      if(!tag || mobj->subsector->sector->tag == tag)
         destColl.add(mobj);
   }
   if(destColl.isEmpty())
      return nullptr;

   mobj = destColl.getRandom(pr_hexenteleport);

   return mobj;
}

//
// ioanch 20160329: param teleport (Teleport special)
//
int EV_ParamTeleport(int tid, int tag, int side, Mobj *thing, bool alwaysfrag)
{
   // don't teleport missiles
   // Don't teleport if hit back of line,
   //  so you can get out of teleporter.
   if(!thing || side || thing->flags & MF_MISSILE)
      return 0;

   if(tid)
   {
      const Mobj *mobj = P_pickRandomLanding(tid, tag);
      if(mobj)
         return P_Teleport(thing, mobj, alwaysfrag);
      return 0;
   }

   // no TID: act like classic Doom
   return EV_Teleport(tag, side, thing, alwaysfrag);
}

//
// Silent TELEPORTATION, by Lee Killough
// Primarily for rooms-over-rooms etc.
//

int EV_SilentTeleport(const line_t *line, int tag, int side, Mobj *thing,
                      teleparms_t parms)
{
   int       i;
   Mobj    *m;
   Thinker *th;
   
   // don't teleport missiles
   // Don't teleport if hit back of line,
   // so you can get out of teleporter.
   
   if(!thing || side || thing->flags & MF_MISSILE)
      return 0;

   for(i = -1; (i = P_FindSectorFromTag(tag, i)) >= 0;)
   {
      for(th = thinkercap.next; th != &thinkercap; th = th->next)
      {
         if(!(m = thinker_cast<Mobj *>(th)))
            continue;
         
         if(m->type == E_ThingNumForDEHNum(MT_TELEPORTMAN) &&
            m->subsector->sector-sectors == i)
         {
            return P_SilentTeleport(thing, line, m, parms);
         }
      }
   }
   return 0;
}

//
// ioanch 20160423: param silent teleport (Teleport_NoFog special)
//
int EV_ParamSilentTeleport(int tid, const line_t *line, int tag, int side,
                           Mobj *thing, teleparms_t parms)
{
   if(!thing || side || thing->flags & MF_MISSILE)
      return 0;

   if(tid)
   {
      const Mobj *mobj = P_pickRandomLanding(tid, tag);
      if(mobj)
         return P_SilentTeleport(thing, line, mobj, parms);
      return 0;
   }

   return EV_SilentTeleport(line, tag, side, thing, parms);
}

//
// Silent linedef-based TELEPORTATION, by Lee Killough
// Primarily for rooms-over-rooms etc.
// This is the complete player-preserving kind of teleporter.
// It has advantages over the teleporter with thing exits.
//

// maximum fixed_t units to move object to avoid hiccups
#define FUDGEFACTOR 10
// ioanch 20160424: added lineid as a separate arg
int EV_SilentLineTeleport(const line_t *line, int lineid, int side, Mobj *thing,
                          bool reverse, bool alwaysfrag)
{
   int i;
   line_t *l;

   // ioanch 20160424: protect against null line or thing pointer
   if(side || !thing || thing->flags & MF_MISSILE || !line)
      return 0;

   for (i = -1; (i = P_FindLineFromTag(lineid, i)) >= 0;)
   {
      if ((l=lines+i) != line && l->backsector)
      {
         // Get the thing's position along the source linedef
         fixed_t pos = D_abs(line->dx) > D_abs(line->dy) ?
            FixedDiv(thing->x - line->v1->x, line->dx) :
            FixedDiv(thing->y - line->v1->y, line->dy) ;

         // Get the angle between the two linedefs, for rotating
         // orientation and momentum. Rotate 180 degrees, and flip
         // the position across the exit linedef, if reversed.
         angle_t angle = (reverse ? pos = FRACUNIT-pos, 0 : ANG180) +
            P_PointToAngle(0, 0, l->dx, l->dy) -
            P_PointToAngle(0, 0, line->dx, line->dy);

         // Interpolate position across the exit linedef
         fixed_t x = l->v2->x - FixedMul(pos, l->dx);
         fixed_t y = l->v2->y - FixedMul(pos, l->dy);

         // Sine, cosine of angle adjustment
         fixed_t s = finesine[angle>>ANGLETOFINESHIFT];
         fixed_t c = finecosine[angle>>ANGLETOFINESHIFT];

         // Maximum distance thing can be moved away from interpolated
         // exit, to ensure that it is on the correct side of exit linedef
         int fudge = FUDGEFACTOR;

         // Whether this is a player, and if so, a pointer to its player_t.
         // Voodoo dolls are excluded by making sure thing->player->mo==thing.
         player_t *player = thing->player && thing->player->mo == thing ?
            thing->player : nullptr;

         // Whether walking towards first side of exit linedef steps down
         int stepdown =
            l->frontsector->srf.floor.getZAt(x, y) < l->backsector->srf.floor.getZAt(x, y);

         // Height of thing above ground
         fixed_t z = thing->z - thing->zref.floor;

         // Side to exit the linedef on positionally.
         //
         // Notes:
         //
         // This flag concerns exit position, not momentum. Due to
         // roundoff error, the thing can land on either the left or
         // the right side of the exit linedef, and steps must be
         // taken to make sure it does not end up on the wrong side.
         //
         // Exit momentum is always towards side 1 in a reversed
         // teleporter, and always towards side 0 otherwise.
         //
         // Exiting positionally on side 1 is always safe, as far
         // as avoiding oscillations and stuck-in-wall problems,
         // but may not be optimum for non-reversed teleporters.
         //
         // Exiting on side 0 can cause oscillations if momentum
         // is towards side 1, as it is with reversed teleporters.
         //
         // Exiting on side 1 slightly improves player viewing
         // when going down a step on a non-reversed teleporter.

         int lside = reverse || (player && stepdown);
         
         // Make sure we are on correct side of exit linedef.
         while(P_PointOnLineSide(x, y, l) != lside && --fudge>=0)
         {
            if (D_abs(l->dx) > D_abs(l->dy))
               y -= (l->dx < 0) != lside ? -1 : 1;
            else
               x += (l->dy < 0) != lside ? -1 : 1;
         }

         // Attempt to teleport, aborting if blocked
         if(!P_TeleportMove(thing, x, y, alwaysfrag ? TELEMOVE_FRAG : 0)) // killough 8/9/98
            return 0;

         // Adjust z position to be same height above ground as before.
         // Ground level at the exit is measured as the higher of the
         // two floor heights at the exit linedef.
         thing->z = z + sides[l->sidenum[stepdown]].sector->srf.floor.getZAt(x, y);

         // Rotate thing's orientation according to difference in linedef angles
         thing->angle += angle;

         // Momentum of thing crossing teleporter linedef
         x = thing->momx;
         y = thing->momy;

         // Rotate thing's momentum to come out of exit just like it entered
         thing->momx = FixedMul(x, c) - FixedMul(y, s);
         thing->momy = FixedMul(y, c) + FixedMul(x, s);

         // Adjust a player's view, in case there has been a height change
         if(player)
         {
            // Save the current deltaviewheight, used in stepping
            fixed_t deltaviewheight = player->deltaviewheight;

            // Clear deltaviewheight, since we don't want any changes now
            player->deltaviewheight = 0;

            // Set player's view according to the newly set parameters
            P_CalcHeight(*player);

            player->prevviewz = player->viewz;

            // Reset the delta to have the same dynamics as before
            player->deltaviewheight = deltaviewheight;

            if(player == players + displayplayer)
                P_ResetChasecam();
         }

         thing->backupPosition();
         P_AdjustFloorClip(thing);
         
         return 1;
      }
   }
   return 0;
}

//----------------------------------------------------------------------------
//
// $Log: p_telept.c,v $
// Revision 1.13  1998/05/12  06:10:43  killough
// Fix silent teleporter bugs
//
// Revision 1.12  1998/05/10  23:41:37  killough
// Fix silent teleporters, add lots of comments
//
// Revision 1.11  1998/05/07  00:55:08  killough
// Fix exit position of reversed teleporters
//
// Revision 1.10  1998/05/03  22:36:39  killough
// beautification, #includes
//
// Revision 1.9  1998/04/17  10:27:56  killough
// Use P_FindLineFromLineTag() to improve speed, add FUDGEFACTOR macro
//
// Revision 1.8  1998/04/16  06:31:51  killough
// Fix double-teleportation problems
//
// Revision 1.7  1998/04/14  22:03:18  killough
// add parens
//
// Revision 1.6  1998/04/14  18:49:56  jim
// Added monster only and reverse teleports
//
// Revision 1.5  1998/03/20  00:30:31  phares
// Changed friction to linedef control
//
// Revision 1.4  1998/02/17  06:18:19  killough
// Add silent teleporter w/ exit thing, rename other
//
// Revision 1.3  1998/02/02  13:16:59  killough
// Add silent teleporter
//
// Revision 1.2  1998/01/26  19:24:30  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:01  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

