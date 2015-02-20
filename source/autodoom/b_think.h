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
   const BSubsec *ss;  // subsector reference
   const BSubsec* m_lastPathSS;  // last subsector when on path
   std::unordered_set<const BSubsec*> m_dropSS;

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
   int                      m_straferunstate;
   int m_combatStrafeState; // -1 or 1
   PathFinder               m_finder;

   friend void AM_drawBotPath();    // let the automap have access to the path here
   BotPath                  m_path;
   bool                     m_runfast;
   bool                     m_hasPath;
   v2fixed_t                m_realVelocity; // real momentum, i.e. difference from last tic
   v2fixed_t                m_lastPosition;

   DeepSearch               m_deepSearchMode;
   std::unordered_set<const line_t*>  m_deepTriedLines;
   std::unordered_set<const BSubsec*> m_deepAvailSsectors;
   const BSubsec*           m_deepRepeat;
   bool                     m_justGotLost;
   bool                     m_intoSwitch;
   int                      m_goalTimer;
   
   // internal states
   unsigned prevCtr;
   unsigned m_searchstage;

   // Item knowledge builder
   // nopickStats: the minimum stats by which an item won't be picked
   // effectStats: the maximum benefit from picking up an item.
   std::unordered_map<spritenum_t, PlayerStats> nopickStats, effectStats;
   
   struct Target
   {
      union
      {
         const Mobj*    mobj;
         const line_t*  gline;
         const void*    exists;
      };
      v2fixed_t coord;
      fixed_t dist;
      angle_t dangle;
      bool isLine;

      // for heaping
      bool operator < (const Target& o) const
      {
          return dist > o.dist; // min heap
      }
   };

   void enemyVisible(PODCollection<Target>& targets);

   
   bool goalAchieved();
   
   void pickRandomWeapon(const Target& target);
   void doCombatAI(const PODCollection<Target>& targets);
   void doNonCombatAI();
   
   void cruiseControl(fixed_t nx, fixed_t ny, bool moveslow);
   void capCommands();
   
   bool shouldUseSpecial(const line_t& line, const BSubsec& liness);
   static bool objOfInterest(const BSubsec& ss, BotPathEnd& coord, void* v);
   static PathResult reachableItem(const BSubsec& ss, void* v);
   
public:
   
   int justPunched;
   
   //
   // Constructor
   //
    Bot() : ZoneObject(),
   pl(nullptr),
   active(true),
   cmd(nullptr),
   ss(nullptr),
   m_lastPathSS(nullptr),
   prevCtr(0),
   m_searchstage(0),
   m_finder(nullptr),
   m_runfast(false),
   m_hasPath(false),
   m_deepSearchMode(DeepNormal),
   m_deepRepeat(nullptr),
   m_straferunstate(0),
   m_combatStrafeState(-1),
   m_justGotLost(false),
   m_intoSwitch(false),
   m_goalTimer(0),
   justPunched(0)
   {
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

#endif /* defined(__EternityEngine__b_think__) */

// EOF

