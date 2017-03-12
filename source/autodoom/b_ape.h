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

#ifndef b_ape_hpp
#define b_ape_hpp

#include "b_botmap.h"
#include "../doomdef.h"
#include "../m_vector.h"

class Bot;
struct player_t;

struct JumpObservation
{
   const BSubsec *destss;
   v3fixed_t startpos;
   v3fixed_t vel;
   bool success;
};

class PlayerObserver
{
public:
   static void initObservers();

   PlayerObserver() : bot(), pl()
   {
      mapInit();
   }
   void mapInit();

   void makeObservations();

private:
   void observeJumping();

   const Bot *bot;
   const player_t *pl;

   // General stuff
   v3fixed_t prevpos;

   // Jumping stuff
   bool mInAir;
   JumpObservation mJump;
   std::unordered_map<const BSubsec *, PODCollection<JumpObservation>> ssJumps;
};

extern PlayerObserver gPlayerObservers[MAXPLAYERS];

#endif /* b_ape_hpp */
