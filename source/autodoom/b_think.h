// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Ioan Chera
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
//      Main bot command
//
//-----------------------------------------------------------------------------

#ifndef __EternityEngine__b_think__
#define __EternityEngine__b_think__

struct player_t;
struct ticcmd_t;

#include "b_botmap.h"
#include "b_itemlearn.h"
#include "b_path.h"
#include "../e_inventory.h"
#include "../metaapi.h"

// goal metatable keys
#define BOT_PICKUP "pickupEvent"
#define BOT_WALKTRIG "walkTriggerEvent"

//
// Bot
//
// The main bot class. Handles environment and outputs tic commands
//
class Bot : public ZoneObject
{
//   friend void AM_drawNodeLines();
   
   player_t *pl; // the controlled player. He'll likely receive the tic
                     // commands
   bool active;      // whether active (Can be turned off)
   ticcmd_t *cmd;    // my commands to output
   BSubsec *ss;  // subsector reference

   MetaTable goalTable;  // objectives to get (typically one object)
   MetaTable goalEvents;   // any potential goal events, which may be the same
   // as the one in goalTable

   enum DeepSearch
   {
       DeepNormal,  // normal pathfinding search
       DeepAvail,   // find already available items
       DeepBeyond,  // look deeply for whatever is available beyond
   };
   
   RandomGenerator          random;       // random generator for bot
   PathFinder               m_finder;
   BotPath                  m_path;
   bool                     m_hasPath;
   DeepSearch               m_deepSearchMode;
   std::set<const line_t*>  m_deepTriedLines;
   std::unordered_set<const BSubsec*> m_deepAvailSsectors;
   
   // internal states
   int prevPathIdx[2];
   unsigned prevCtr;
   unsigned m_searchstage;
public:
   PathArray path;   // pathfinding
private:

   // Item knowledge builder
   // nopickStats: the minimum stats by which an item won't be picked
   // effectStats: the maximum benefit from picking up an item.
   std::unordered_map<spritenum_t, PlayerStats> nopickStats, effectStats;
   
   bool objectOfInterest(const BSeg *targsg, const BSubsec *targss,
                         v2fixed_t &destcoord);
   
   bool routePath();
   
   bool goalAchieved();
   void doNonCombatAI();
   
   void capCommands();
   
   // Want item conditions
   bool wantArmour(itemeffect_t *ie) const;

   bool shouldUseSpecial(const line_t& line);
   static bool objOfInterest(const BSubsec& ss, v2fixed_t& coord, void* v);
   static PathResult reachableItem(const BSubsec& ss, void* v);
   
public:
   
   //
   // Constructor
   //
    Bot() : ZoneObject(), pl(nullptr), active(true), cmd(nullptr), ss(nullptr),
        prevCtr(0), m_searchstage(0), m_finder(nullptr), m_hasPath(false), m_deepSearchMode(DeepNormal)
   {
      memset(prevPathIdx, 0xff, sizeof(prevPathIdx));
      random.initialize((unsigned)time(nullptr));
   }
   
   void doCommand();
   
   //
   // addXYEvent
   //
   void addXYEvent(const char *key, v2fixed_t value)
   {
      goalEvents.addV2Fixed(key, value);
   }
   
   void mapInit();
   
   static void InitBots();

   PlayerStats &getNopickStats(spritenum_t spnum);
   PlayerStats *findNopickStats(spritenum_t spnum)
   {
      auto fnd = nopickStats.find(spnum);
      if(fnd != nopickStats.cend())
         return &fnd->second;
      return nullptr;
   }

   PlayerStats &getEffectStats(spritenum_t spnum);
};

extern Bot bots[];
extern std::unordered_map<player_t *, Bot *> botDict;

#endif /* defined(__EternityEngine__b_think__) */

// EOF

