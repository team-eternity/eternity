// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2017 Ioan Chera
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Bot learning by imitating player
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"

#include "b_ape.h"
#include "b_botmap.h"
#include "b_think.h"
#include "../doomstat.h"
#include "../p_maputl.h"

PlayerObserver gPlayerObservers[MAXPLAYERS];

void PlayerObserver::initObservers()
{
   for(int i = 0; i < MAXPLAYERS; ++i)
   {
      gPlayerObservers[i].bot = &bots[i];
      gPlayerObservers[i].pl = &players[i];
   }
}

void PlayerObserver::mapInit()
{
   prevpos.x = D_MAXINT;   // start with an invalid value. FIXME: use spawn point
   mInAir = false;
   mJump = { 0 };
   ssJumps.clear();
}

void PlayerObserver::makeObservations()
{
   if(bot->active || !botMap)
      return;  // don't run on active bots; only on human players
   // also don't run if there's no bot map.

   if(prevpos.x != D_MAXINT)
      observeJumping();

   // Save the current position for next tic
   prevpos.x = pl->mo->x;
   prevpos.y = pl->mo->y;
   prevpos.z = pl->mo->z;
}

void PlayerObserver::observeJumping()
{
   bool onground = P_MobjOnGround(*pl->mo);
   if(!mInAir && !onground)
   {
      mInAir = true;
      mJump = {0};
      mJump.vel.x = pl->mo->momx;
      mJump.vel.y = pl->mo->momy;
      mJump.vel.z = pl->mo->momz;
      mJump.startpos = prevpos;
   }
   if(mInAir)
   {
      botMap->pathTraverse(prevpos.x, prevpos.y, pl->mo->x, pl->mo->y, [this](const BotMap::Line &line, const divline_t &dl, fixed_t frac) -> bool {

         if(!frac)
            return true;
         divline_t linedl;
         linedl.x = line.v[0]->x;
         linedl.y = line.v[0]->y;
         linedl.dx = line.v[1]->x - linedl.x;
         linedl.dy = line.v[1]->y - linedl.y;
         int side = P_PointOnDivlineSide(dl.x, dl.y, &linedl);
         const MetaSector *ms1 = line.msec[side];
         const MetaSector *ms2 = line.msec[side ^ 1];
         if(!ms1 || !ms2)
            return false;  // 1-sided line stops
         if(!botMap->canPass(ms1, ms2, pl->mo->height))
            mJump.success = true;
         return true;
      });
      if(onground)
      {
         if(mJump.success)
         {
            mJump.destss = &botMap->pointInSubsector(pl->mo->x, pl->mo->y);
            // We now have a jump point at startpos, facing vel, reaching ss
            // now when bot does pathfinding, use this as option
            const BSubsec &startss = botMap->pointInSubsector(mJump.startpos.x,
                                                              mJump.startpos.y);
//            ssJumps[&startss].add(mJump);
//            B_Log("Registered jump from (%g %g %g %d) by velocity (%g %g %g) to ss %d",
//                  mJump.startpos.x / 65536., mJump.startpos.y / 65536.,
//                  mJump.startpos.z / 65536.,
//                  int(&startss - &botMap->ssectors[0]), mJump.vel.x / 65536.,
//                  mJump.vel.y / 65536., mJump.vel.z / 65536.,
//                  int(mJump.destss - &botMap->ssectors[0]));
         }
         mInAir = false;
      }
   }
}
