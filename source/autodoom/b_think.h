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

#include "b_path.h"
#include "../metaapi.h"

// goal metatable keys
#define BOT_PICKUP "pickupEvent"
#define BOT_WALKTRIG "walkTriggerEvent"
#define BOT_FLOORSECTOR "floorSectorEvent"

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
       DeepCheckLosses,	// used by one-time items to prevent death traps. TODO: use it for ALL destinations!
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
   const Mobj*              m_currentTargetMobj;
   int                     m_exitDelay;

   // Chat time keepers
   int                      m_lastHelpCry;
   int                     m_lastExitMessage;
   int                     m_lastDunnoMessage;
   
   // internal states
   unsigned prevCtr;
   unsigned m_searchstage;

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


   //! This holds information obtained from the list of targets
   struct CombatInfo
   {
      bool hasShooters;
      bool hasExploders;
      double totalThreat;
   };

   void enemyVisible(PODCollection<Target>& targets);

   
   bool goalAchieved();
   
   void pickRandomWeapon(const Target& target);
   void pickBestWeapon(const Target& target);
   const Target *pickBestTarget(const PODCollection<Target>& targets, CombatInfo &cinfo);
   void doCombatAI(const PODCollection<Target>& targets);

   void doNonCombatAI();

   // Movement control
   void toggleStrafeState()
   {
      m_combatStrafeState = random.range(0, 1) * 2 - 1;
   }
   bool stepLedges(bool avoid, fixed_t nx, fixed_t ny);
   void cruiseControl(fixed_t nx, fixed_t ny, bool moveslow);

   void capCommands();

   bool checkDeadEndTrap(const BSubsec& targss);
   bool shouldUseSpecial(const line_t& line, const BSubsec& liness);
   bool checkItemType(const Mobj *special) const;
   static bool objOfInterest(const BSubsec& ss, BotPathEnd& coord, void* v);
   bool handleLineGoal(const BSubsec& ss, BotPathEnd& coord, const line_t& line);
   static PathResult reachableItem(const BSubsec& ss, void* v);

   void simulateBaseTiccmd();

   bool shouldChat(int intervalSec = 0, int timekeeper = 0) const;
   
public:
   
   bool active;      // whether active (Can be turned off)
   int justPunched;
   
   //
   // Constructor
   //
    Bot() : ZoneObject(),
   pl(nullptr),
   cmd(nullptr),
   ss(nullptr),
   m_lastPathSS(nullptr),
   m_straferunstate(0),
   m_combatStrafeState(-1),
   m_finder(nullptr),
   m_runfast(false),
   m_hasPath(false),
   m_deepSearchMode(DeepNormal),
   m_deepRepeat(nullptr),
   m_justGotLost(false),
   m_intoSwitch(false),
   m_goalTimer(0),
   m_currentTargetMobj(nullptr),
   m_exitDelay(0),
   m_lastHelpCry(0),
   m_lastExitMessage(0),
   m_lastDunnoMessage(0),
   prevCtr(0),
   m_searchstage(0),
   active(true),
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
};

extern Bot bots[];

#endif /* defined(__EternityEngine__b_think__) */

// EOF

