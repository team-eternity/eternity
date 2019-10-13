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
// Bot target type
//
enum TargetType
{
   TargetMonster,
   TargetLine,
   TargetMissile
};

//
// Bot
//
// The main bot class. Handles environment and outputs tic commands
//
class Bot : public ZoneObject
{
//   friend void AM_drawNodeLines();
   
   player_t *pl = nullptr; // the controlled player. He'll likely receive the tic
                     // commands
   ticcmd_t *cmd = nullptr;    // my commands to output
   const BSubsec *ss = nullptr;  // subsector reference

   const BSubsec *m_lostPathSS;

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
   int                      m_straferunstate = 0;
   int m_combatStrafeState = -1; // -1 or 1
   PathFinder               m_finder;

   friend void AM_drawBotPath();    // let the automap have access to the path here
   BotPath                  m_path;
   bool                     m_runfast = false;
   bool                     m_hasPath = false;
   v2fixed_t                m_realVelocity; // real momentum, i.e. difference from last tic
   v2fixed_t                m_lastPosition;

   DeepSearch               m_deepSearchMode = DeepNormal;
   std::unordered_set<const line_t*>  m_deepTriedLines;
   std::unordered_set<const BSubsec*> m_deepAvailSsectors;
   const BSubsec*           m_deepRepeat = nullptr;

   // Holds info of potential visits behind a door or lift, to focus on them
   struct DeepPromise
   {
      enum: unsigned
      {
         BENEFICIAL = 1,
         URGENT = 2,
      };

      std::unordered_set<const BSubsec *> sss;
      unsigned flags = 0;

      bool isUrgent() const
      {
         return (flags & (BENEFICIAL | URGENT)) == (BENEFICIAL | URGENT);
      }

      void clear()
      {
         sss.clear();
         flags = 0;
      }
   } m_deepPromise;

   bool                     m_justGotLost = false;
   bool                     m_intoSwitch = false;
   const Mobj*              m_currentTargetMobj = nullptr;
   int                     m_exitDelay = 0;

   // Chat time keepers
   int                      m_lastHelpCry = 0;
   int                     m_lastExitMessage = 0;
   int                     m_lastDunnoMessage = 0;
   
   // internal states
   unsigned m_searchstage = 0;

   //
   // Bot combat target
   //
   class Target : public ZoneObject
   {
   public:
      Target() : type(TargetMonster), mobj(), coord(), dist(), dangle()
      {
      }

      Target(const Mobj &mobj, const Mobj &source, bool ismissile);
      Target(const line_t &line, const Mobj &source);
      Target(const Target &other) = delete;
      Target &operator = (const Target &other) = delete;
      Target(Target &&other)
      {
         *this = std::move(other);
      }
      Target &operator = (Target &&other);
      ~Target()
      {
         if(type != TargetLine)
            P_ClearTarget(mobj);
      }

      TargetType type;

      union
      {
         const Mobj*    mobj; // monster, missile
         const line_t*  gline;   // line
      };
      v2fixed_t coord;
      fixed_t dist;
      angle_t dangle;

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

   void enemyVisible(Collection<Target>& targets);

   
   bool goalAchieved();
   
   void pickRandomWeapon(const Target& target);
   void pickBestWeapon(const Target& target);
   const Target *pickBestTarget(const Collection<Target>& targets, CombatInfo &cinfo);
   void doCombatAI(const Collection<Target>& targets);

   bool shouldWaitSector(const BNeigh &neigh) const;
   void doNonCombatAI();

   // Movement control
   void toggleStrafeState()
   {
      m_combatStrafeState = random.range(0, 1) * 2 - 1;
   }
   bool stepLedges(bool avoid, v2fixed_t npos);
   void cruiseControl(v2fixed_t npos, bool moveslow);

   void capCommands();

   bool checkDeadEndTrap(const BSubsec& targss);
   enum SpecialChoice   // to what degree to use a special from the path
   {
      SpecialChoice_no,          // not useful
      SpecialChoice_worth,       // worth trying but not directly beneficial to user
      SpecialChoice_favourable   // also helps user (e.g. exits the level, heals etc.)
   };
   SpecialChoice shouldUseSpecial(const line_t& line, const BSubsec& liness);
   bool checkItemType(const Mobj *special) const;
   bool otherBotsHaveGoal(const char *key, v2fixed_t coord) const;
   static bool objOfInterest(const BSubsec& ss, BotPathEnd& coord, void* v);
   bool handleLineGoal(const BSubsec& ss, BotPathEnd& coord, const line_t& line);
   static PathResult reachableItem(const BSubsec& ss, void* v);

   bool shouldChat(int intervalSec = 0, int timekeeper = 0) const;
   
public:
   
   bool active = true;      // whether active (Can be turned off)
   int justPunched = 0;
   
   //
   // Constructor
   //
    Bot() : ZoneObject(),
   m_finder(nullptr)
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

