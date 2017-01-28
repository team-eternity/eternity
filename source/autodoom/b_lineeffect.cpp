// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2014 Ioan Chera
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
//      Line effects on the map, by their type numbers. Specified separately
//      from Eternity's internals because the latter is going too deep.
//
//-----------------------------------------------------------------------------

#include <set>
#include "../z_zone.h"

#include "b_analysis.h"
#include "b_botmap.h"
#include "b_lineeffect.h"
#include "../doomstat.h"
#include "../e_exdata.h"
#include "../e_inventory.h"
#include "../ev_actions.h"
#include "../ev_sectors.h"
#include "../ev_specials.h"
#include "../m_collection.h"
#include "../p_info.h"
#include "../p_spec.h"
#include "../r_data.h"
#include "../r_defs.h"
#include "../r_state.h"

// We need stacks of modified sector floorheights and ceilingheights.
// As we push linedef values to those stacks, we update the floor heights

//
// SectorHeightPair
//
// Just a pair of values, under a meaningful name
// FIXME: currently it's for vanilla only, with lots of duplicated functionality
// from the core EV_ and P_ code.
//
// On terminal kinds:
// - it is set if the effect is momentary: disallow and further pushes
// - the previous state will be in effect after the momentary one wears off.
//   Especially relevant for Doom lifts, which need both states for walking past
//   areas
// - What can i do with lifts connecting multiple levels? I really need to con-
//   sider them all, not just the previous and current state.
// - crushers are terminal and permanent (but can be paused into accessible and
//   inaccessible states -- what to do with latter, especially when once-only?)
// - perpetual lifts are also permanent
//
struct SectorStateEntry
{
   int      actionNumber;  // the action that did the trigger
   fixed_t  floorHeight;   // the resulting heights
   fixed_t  altFloorHeight; // alternate floor height (used for lifts)
   fixed_t  ceilingHeight;
   bool     floorTerminal; // true if momentary and nothing can be added on it
   bool     ceilingTerminal;
};

//
// SectorHeightStackEntry
//
// The sector stack entry. Contains the actual referenced sector and the stack
// of height pairs
//
static const player_t* g_keyPlayer;

static bool g_useRealHeights;

static fixed_t applyDoorCorrection(const sector_t& sector)
{
   if(g_useRealHeights)
      return sector.ceilingheight;
   
   
   size_t secnum = &sector - ::sectors;
   if(botMap && botMap->sectorFlags && botMap->sectorFlags[secnum].isDoor)
   {
      int lockID = botMap->sectorFlags[secnum].lockID;
      if(lockID && g_keyPlayer)
      {
         if(E_PlayerCanUnlock(g_keyPlayer, lockID, false, true))
            return P_FindLowestCeilingSurrounding(&sector) - 4 * FRACUNIT;
      }
      else if(!lockID)
         return P_FindLowestCeilingSurrounding(&sector) - 4 * FRACUNIT;
   }
   
    const VerticalDoorThinker* vdt = thinker_cast<const VerticalDoorThinker*>(sector.ceilingdata);
    if (vdt && vdt->direction == 1)
        return vdt->topheight;

    const CeilingThinker* ct = thinker_cast<const CeilingThinker*>(sector.ceilingdata);
    if (ct && !ct->inStasis && ct->speed > 0 && (ct->direction == 1 || ct->crush))
        return ct->topheight;
    
    const ElevatorThinker* et = thinker_cast<const ElevatorThinker*>(sector.ceilingdata);
    if (et && et->speed > 0)
        return et->ceilingdestheight;

    return sector.ceilingheight;
}

// Very special treatment needed for plats
static fixed_t applyLiftCorrection(const sector_t& sector)
{
   const PlatThinker* pt = thinker_cast<const PlatThinker*>(sector.floordata);
   if(pt && pt->speed > 0 && pt->status == PlatThinker::down)
   {
      return pt->high;  // will alternate with the pt->low on applyFloorCorr
   }
   
   return sector.floorheight;
}

static fixed_t applyFloorCorrection(const sector_t& sector)
{
    const PlatThinker* pt = thinker_cast<const PlatThinker*>(sector.floordata);
    if (pt && pt->speed > 0 && pt->status != PlatThinker::in_stasis)
    {
        switch (pt->status)
        {
        case PlatThinker::up:
        case PlatThinker::waiting:
            return pt->high;
        case PlatThinker::down:
            return pt->low;
        default:
            break;
        }
    }

    const ElevatorThinker* et = thinker_cast<const ElevatorThinker*>(sector.floordata);
    if (et && et->speed > 0)
        return et->floordestheight;

    const FloorMoveThinker* fmt = thinker_cast<const FloorMoveThinker*>(sector.floordata);
    if (fmt && fmt->speed > 0)
        return fmt->floordestheight;



    return sector.floorheight;
}

class SectorHeightStack
{
public:
   const sector_t* sector; // the affected sector
   PODCollection<SectorStateEntry> stack;   // stack of actions
   fixed_t getFloorHeight() const
   {
      return stack.getLength() ? stack.back().floorHeight
      : applyLiftCorrection(*sector);
   }
   fixed_t getAltFloorHeight() const
   {
       return stack.getLength() && isFloorTerminal()
      ? stack.back().altFloorHeight : applyFloorCorrection(*sector);
   }
   fixed_t getCeilingHeight() const
   {
      return stack.getLength() ? stack.back().ceilingHeight
      : applyDoorCorrection(*sector);

   }
   bool isFloorTerminal() const
   {
      return stack.getLength() ? stack.back().floorTerminal
      : P_SectorActive(floor_special, sector);
   }
   bool isCeilingTerminal() const
   {
      return stack.getLength() ? stack.back().ceilingTerminal
      : P_SectorActive(ceiling_special, sector);
   }
};

// A list of sectors, same size as sectors
static Collection<SectorHeightStack> g_affectedSectors;

// The actual action stack. Keeps track of g_affectedSectors indices
static Collection<PODCollection<int>> g_indexListStack;

//
// LevelStateStack::InitLevel
//
// Called from around P_SetupLevel, it should setup the collection from the
// sectors array
//
void LevelStateStack::InitLevel()
{
   static SectorHeightStack prototype;
   static PODCollection<int> prototype2;
   g_affectedSectors.setPrototype(&prototype);
   g_indexListStack.setPrototype(&prototype2);
   
   // Reset the new-sectors list
   g_affectedSectors.makeEmpty();
   for(int i = 0; i < numsectors; ++i)
   {
      g_affectedSectors.addNew().sector = sectors + i;
   }
   g_indexListStack.makeEmpty();
}

//
// LevelStateStack::Push
//
// Adds a new action to the stack, modifying the intended sectors
//
static void B_pushSectorHeights(int, const line_t& line, PODCollection<int>&, const player_t& player);

bool LevelStateStack::Push(const line_t& line, const player_t& player,
                           const sector_t *excludeSector)
{
   int secnum = -1;
   
   // Prepare the new list of indices to the global stack
   PODCollection<int>& coll = g_indexListStack.addNew();
   
   const ev_action_t* action = EV_ActionForSpecial(line.special);

   if (action && action->type == &DRActionType)
   {
       if (line.sidenum[1] >= 0)
           B_pushSectorHeights((int)(sides[line.sidenum[1]].sector - sectors), line, coll, player);
   }
   else
   {
      while((secnum = P_FindSectorFromTag(line.tag, secnum)) >= 0)
      {
         // For each successful state push, add an index to the collection

         if(::sectors + secnum == excludeSector
            && g_affectedSectors[secnum].stack.getLength())
         {
            B_Log("Exclude %d", secnum);
            continue;
         }
         B_pushSectorHeights(secnum, line, coll, player);
      }
   }
   
   if (coll.getLength() == 0)
   {
       // No valid state change, so cancel all this and return false
       g_indexListStack.pop();
       return false;
   }
   
   // okay
   return true;
}

void LevelStateStack::Pop()
{
    if (g_indexListStack.isEmpty())
        I_Error("%s: already empty!", __FUNCTION__);
    PODCollection<int>& coll = g_indexListStack[g_indexListStack.getLength() - 1];

    for (int n : coll)
    {
        g_affectedSectors[n].stack.pop();
    }
    g_indexListStack.pop();
}

//
// B_ClearLineEffectStacks
//
// Clears all the stacks
//
void LevelStateStack::Clear()
{
   for(auto& affectedSector : g_affectedSectors)
   {
      affectedSector.stack.makeEmpty();
   }
   g_indexListStack.makeEmpty();
}

//
// B_pushSectorHeights
//
// Tries to execute a state change on a sector. Returns false if it can't be
// done, for any reason (terminal/momentary state, non-sector action).
//
static bool fillInStairs(const sector_t* sector, SectorStateEntry& sae,
                         VanillaLineSpecial vls, PODCollection<int>& coll);
static bool fillInDonut(const sector_t& sector, SectorStateEntry& sae,
                        VanillaLineSpecial vls, PODCollection<int>& coll);
static fixed_t getMinTextureSize(const sector_t& sector);

static bool doorBusy(const sector_t& sector)
{
    auto secThinker = thinker_cast<SectorThinker *>(sector.ceilingdata);

    // exactly only one at most of these pointers is valid during demo_compatibility
    if (demo_compatibility)
    {
        if (!secThinker)
            secThinker = thinker_cast<SectorThinker *>(sector.floordata);
        if (!secThinker)
            secThinker = thinker_cast<SectorThinker *>(sector.lightingdata);
    }

    return secThinker != nullptr;
}

//
// True if special opens unlocked door behind linedef that won't close
//
static bool B_isManualDoorOpen(const line_t *line)
{
   int special = line->special;

   // Generalized special
   if(special >= GenDoorBase && special < GenCeilingBase)
   {
      int genspac = EV_GenActivationType(special);
      if(genspac == PushOnce || genspac == PushMany)
      {
         int kind = ((special - GenDoorBase) & DoorKind) >> DoorKindShift;
         return kind == ODoor;
      }
   }

   // Classic special
   const ev_action_t *action = EV_ActionForSpecial(special);
   if(!action)
      return false;

   if (action->action == EV_ActionVerticalDoor)
      return special == 31 || special == 118;

   // Parameterized special ("Door_Open")
   if(action->action == EV_ActionParamDoorOpen)
   {
      static const uint32_t flags = EX_ML_PLAYER | EX_ML_USE;
      return !line->args[0] && (line->extflags & flags) == flags;
   }
   return false;
}

//
// True if special opens a locked door behind linedef, that won't close
//
static bool B_isManualDoorOpenLocked(const line_t *line, int *lockid)
{
   int special = line->special;

   // Generalized
   if(special >= GenLockedBase && special < GenDoorBase)
   {
      int genspac = EV_GenActivationType(special);
      if(genspac == PushOnce || genspac == PushMany)
      {
         int kind = ((special - GenLockedBase) & LockedKind) >> LockedKindShift;
         if(kind == ODoor)
         {
            *lockid = EV_LockDefIDForSpecial(special);
            return true;
         }
         return false;
      }
   }

   // Classic
   const ev_action_t *action = EV_ActionForSpecial(special);
   if(!action)
      return false;

   if (action->action == EV_ActionVerticalDoor)
   {
      if (special >= 32 && special <= 34)
      {
         *lockid = EV_LockDefIDForSpecial(special);
         return true;
      }
      return false;
   }

   // no parameterized special

   return false;
}

//
// True if the special means a tagged open-only (no close) door
//
static bool B_isRemoteDoorOpen(const line_t *line)
{
   int special = line->special;

   // generalized
   if(special >= GenDoorBase && special < GenCeilingBase)
   {
      int genspac = EV_GenActivationType(special);
      if(genspac != PushOnce && genspac != PushMany)
      {
         int kind = ((special - GenDoorBase) & DoorKind) >> DoorKindShift;
         return kind == ODoor;
      }
   }

   // classic special
   const ev_action_t *action = EV_ActionForSpecial(special);
   if(!action)
      return false;

   if(action->action == EV_ActionOpenDoor ||
      action->action == EV_ActionDoorBlazeOpen)
   {
      return true;
   }

   // param special
   if(action->action == EV_ActionParamDoorOpen)
   {
      static const uint32_t flags = EX_ML_PLAYER | EX_ML_USE;
      return line->args[0] && (line->extflags & flags) == flags;
   }
   return false;
}

//
// Check if special is a remote locked door that doesn't close
//
static bool B_isRemoteDoorOpenLocked(const line_t *line, int *lockid)
{
   int special = line->special;

   // generalized
   if (special >= GenLockedBase && special < GenDoorBase)
   {
      int genspac = EV_GenActivationType(special);
      if (genspac != PushOnce && genspac != PushMany)
      {
         int kind = ((special - GenLockedBase) & LockedKind) >> LockedKindShift;
         if (kind == ODoor)
         {
            *lockid = EV_LockDefIDForSpecial(special);
            return true;
         }
         return false;
      }
   }

   // classic special
   const ev_action_t *action = EV_ActionForSpecial(special);
   if (!action)
      return false;

   if (action->action == EV_ActionDoLockedDoor)
      return true;

   // no param specials
   return false;
}

//
// Check that it's a manual standard lockless door
//
static bool B_isManualDoorRaise(const line_t *line)
{
   int special = line->special;

   // generalized
   if (special >= GenDoorBase && special < GenCeilingBase)
   {
      int genspac = EV_GenActivationType(special);
      if (genspac == PushOnce || genspac == PushMany)
      {
         int kind = ((special - GenDoorBase) & DoorKind) >> DoorKindShift;
         return kind == OdCDoor || pDOdCDoor;
      }
   }

   // classic special
   const ev_action_t *action = EV_ActionForSpecial(special);
   if (!action)
      return false;

   if (action->action == EV_ActionVerticalDoor)
      return special == 1 || special == 117;
   
   // param special
   if (action->action == EV_ActionParamDoorRaise ||
      action->action == EV_ActionParamDoorWaitRaise)
   {
      static const uint32_t flags = EX_ML_PLAYER | EX_ML_USE;
      return !line->args[0] && (line->extflags & flags) == flags;
   }

   return false;
}

//
// Check that it's a manual standard locked door
//
static bool B_isManualDoorRaiseLocked(const line_t *line, int *lockid)
{
   int special = line->special;

   // Generalized
   if (special >= GenLockedBase && special < GenDoorBase)
   {
      int genspac = EV_GenActivationType(special);
      if (genspac == PushOnce || genspac == PushMany)
      {
         int kind = ((special - GenLockedBase) & LockedKind) >> LockedKindShift;
         if (kind == ODoor)
         {
            *lockid = EV_LockDefIDForSpecial(special);
            return true;
         }
         return false;
      }
   }

   // classic special
   const ev_action_t *action = EV_ActionForSpecial(special);
   if (!action)
      return false;

   if (action->action == EV_ActionVerticalDoor)
   {
      if (special >= 26 && special <= 28)
      {
         *lockid = EV_LockDefIDForSpecial(special);
         return true;
      }
      return false;
   }

   // param special
   if (action->action == EV_ActionParamDoorLockedRaise)
   {
      static const uint32_t flags = EX_ML_PLAYER | EX_ML_USE;
      if (!line->args[0] && (line->extflags & flags) == flags)
      {
         *lockid = line->args[3];
         return true;
      }
      return false;
   }

   return false;
}

//
// Check that it's a remote standard lockless door
//
static bool B_isRemoteDoorRaise(const line_t *line)
{
   int special = line->special;

   // generalized
   if (special >= GenDoorBase && special < GenCeilingBase)
   {
      int genspac = EV_GenActivationType(special);
      if (genspac != PushOnce && genspac != PushMany)
      {
         int kind = ((special - GenDoorBase) & DoorKind) >> DoorKindShift;
         return kind == OdCDoor || pDOdCDoor;
      }
   }

   // classic special
   const ev_action_t *action = EV_ActionForSpecial(special);
   if (!action)
      return false;

   if (action->action == EV_ActionRaiseDoor || 
      action->action == EV_ActionDoorBlazeRaise)
   {
      return true;
   }

   // param special
   if (action->action == EV_ActionParamDoorRaise ||
      action->action == EV_ActionParamDoorWaitRaise)
   {
      static const uint32_t flags = EX_ML_PLAYER | EX_ML_USE;
      return line->args[0] && (line->extflags & flags) == flags;
   }

   return false;
}

static bool B_isCeilingToFloor(const line_t *line)
{
   int special = line->special;

   // generalized
   if (special >= GenCeilingBase && special < GenFloorBase)
   {
      int genspac = EV_GenActivationType(special);
      if (genspac != PushOnce && genspac != PushMany)
      {
         int kind = ((special - GenDoorBase) & DoorKind) >> DoorKindShift;
         return kind == OdCDoor || pDOdCDoor;
      }
   }
   return false;
}

// ce poate face actiunea
// poate schimba pozitii de sectoare. Lista de sectoare.

struct LineEffect
{

};

static void B_pushSectorHeights(int secnum, const line_t& line,
                                PODCollection<int>& indexList, const player_t& player)
{
   SectorHeightStack& affSector = g_affectedSectors[secnum];
   bool floorBlocked = affSector.isFloorTerminal();
   bool ceilingBlocked = affSector.isCeilingTerminal();
   if(floorBlocked && ceilingBlocked)  // all blocked: impossible
      return;
   
   const sector_t& sector = sectors[secnum];
   SectorStateEntry sae;
   sae.actionNumber = line.special;
   fixed_t lastFloorHeight = affSector.getFloorHeight();
   fixed_t lastCeilingHeight = affSector.getCeilingHeight();
   sae.floorHeight = lastFloorHeight;
   sae.ceilingHeight = lastCeilingHeight;
   sae.floorTerminal = floorBlocked;
   sae.ceilingTerminal = ceilingBlocked;
   sae.altFloorHeight = 0;  // this needs to be initialized in order to avoid Visual Studio debug errors.
   
   VanillaLineSpecial vls = static_cast<VanillaLineSpecial>(line.special);
   bool othersAffected = false;
   int amount;

   /*
   int lockID = EV_LockDefIDForSpecial(instance->special);
   return !!EV_VerticalDoor(instance->line, instance->actor, lockID);
   */
   int lockID;
   switch (vls)
   {
      case VLS_D1DoorBlazeOpen:
      case VLS_D1OpenDoor:
          if (ceilingBlocked || doorBusy(sector))
              return;
          sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4
              * FRACUNIT;
          break;

      case VLS_GROpenDoor:
      case VLS_S1DoorBlazeOpen:
      case VLS_S1OpenDoor:
      case VLS_SRDoorBlazeOpen:
      case VLS_SROpenDoor:
      case VLS_W1DoorBlazeOpen:
      case VLS_W1OpenDoor:
      case VLS_WRDoorBlazeOpen:
      case VLS_WROpenDoor:
         if(ceilingBlocked)
            return;
         sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4
         * FRACUNIT;
         break;
      case VLS_D1OpenDoorBlue:
      case VLS_D1OpenDoorRed:
      case VLS_D1OpenDoorYellow:
      {
          if (ceilingBlocked || doorBusy(sector))
              return;
          lockID = EV_LockDefIDForSpecial(line.special);
          if (!E_PlayerCanUnlock(&player, lockID, false, true))
              return;
          sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4
              * FRACUNIT;
          break;
      }

      case VLS_S1DoorBlazeOpenBlue:
      case VLS_S1DoorBlazeOpenRed:
      case VLS_S1DoorBlazeOpenYellow:
      case VLS_SRDoorBlazeOpenBlue:
      case VLS_SRDoorBlazeOpenRed:
      case VLS_SRDoorBlazeOpenYellow:
      {
          if (ceilingBlocked)
              return;
          lockID = EV_LockDefIDForSpecial(line.special);
          if (!E_PlayerCanUnlock(&player, lockID, false, true))
              return;
          sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4
              * FRACUNIT;
          break;
      }

      case VLS_DRDoorBlazeRaise:
      case VLS_DRRaiseDoor:
          if (ceilingBlocked || doorBusy(sector))
              return;
          sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4
              * FRACUNIT;
          sae.ceilingTerminal = true;
          break;

      case VLS_S1DoorBlazeRaise:
      case VLS_S1RaiseDoor:
      case VLS_SRDoorBlazeRaise:
      case VLS_SRRaiseDoor:
      case VLS_WRDoorBlazeRaise:
      case VLS_W1RaiseDoor:
      case VLS_WRRaiseDoor:
      case VLS_W1DoorBlazeRaise:
         if(ceilingBlocked)
            return;
         sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4
         * FRACUNIT;
         sae.ceilingTerminal = true;
         break;

      case VLS_DRRaiseDoorBlue:
      case VLS_DRRaiseDoorRed:
      case VLS_DRRaiseDoorYellow:
      {
          if (ceilingBlocked || doorBusy(sector))
              return;
          lockID = EV_LockDefIDForSpecial(line.special);
          if (!E_PlayerCanUnlock(&player, lockID, false, true))
              return;
          sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4
              * FRACUNIT;
          sae.ceilingTerminal = true;
          break;
      }
         
      case VLS_S1CeilingLowerToFloor:
      case VLS_SRCeilingLowerToFloor:
      case VLS_W1CeilingLowerToFloor:
      case VLS_WRCeilingLowerToFloor:
      case VLS_S1CloseDoor:
      case VLS_SRCloseDoor:
      case VLS_W1CloseDoor:
      case VLS_WRCloseDoor:
      case VLS_W1DoorBlazeClose:
      case VLS_WRDoorBlazeClose:
      case VLS_S1DoorBlazeClose:
      case VLS_SRDoorBlazeClose:
         if(ceilingBlocked)
            return;
         sae.ceilingHeight = sae.floorHeight;
         break;
         
      case VLS_W1CeilingLowerAndCrush:
      case VLS_WRCeilingLowerAndCrush:
      case VLS_S1CeilingLowerAndCrush:
      case VLS_SRCeilingLowerAndCrush:
         if(ceilingBlocked)
            return;
         sae.ceilingHeight = sae.floorHeight + 8 * FRACUNIT;
         break;
         
      case VLS_W1CloseDoor30:
      case VLS_WRCloseDoor30:
      case VLS_S1CloseDoor30:
      case VLS_SRCloseDoor30:
         if(ceilingBlocked)
            return;
         sae.ceilingHeight = sae.floorHeight;
         sae.ceilingTerminal = true;
         break;
         
      case VLS_W1CeilingLowerToLowest:
      case VLS_WRCeilingLowerToLowest:
      case VLS_S1CeilingLowerToLowest:
      case VLS_SRCeilingLowerToLowest:
          if (ceilingBlocked)
              return;
          sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true);
          break;

      case VLS_W1CeilingLowerToMaxFloor:
      case VLS_WRCeilingLowerToMaxFloor:
      case VLS_S1CeilingLowerToMaxFloor:
      case VLS_SRCeilingLowerToMaxFloor:
          if (ceilingBlocked)
              return;
          sae.ceilingHeight = P_FindHighestFloorSurrounding(&sector, true);
          break;

      case VLS_W1FloorLowerToNearest:
      case VLS_WRFloorLowerToNearest:
      case VLS_S1FloorLowerToNearest:
      case VLS_SRFloorLowerToNearest:
          if (floorBlocked)
              return;
          sae.floorHeight = P_FindNextLowestFloor(&sector, lastFloorHeight, true);
          break;

      case VLS_S1PlatDownWaitUpStay:
      case VLS_SRPlatDownWaitUpStay:
      case VLS_W1PlatDownWaitUpStay:
      case VLS_WRPlatDownWaitUpStay:
      case VLS_WRPlatBlazeDWUS:
      case VLS_W1PlatBlazeDWUS:
      case VLS_S1PlatBlazeDWUS:
      case VLS_SRPlatBlazeDWUS:
      {
         if(floorBlocked)
            return;
         fixed_t newheight = P_FindLowestFloorSurrounding(&sector, true);
         if(newheight < sae.floorHeight)
            sae.floorHeight = newheight;
         sae.floorTerminal = true;
         sae.altFloorHeight = lastFloorHeight;
         break;
      }
         
      case VLS_W1LowerFloor:
      case VLS_WRLowerFloor:
      case VLS_S1LowerFloor:
	   case VLS_SRLowerFloor:
         if(floorBlocked)
            return;
         sae.floorHeight = P_FindHighestFloorSurrounding(&sector, true);
         break;
         
      case VLS_S1LowerFloorTurbo:
      case VLS_SRLowerFloorTurbo:
      case VLS_W1LowerFloorTurbo:
      case VLS_WRLowerFloorTurbo:
         if(floorBlocked)
            return;
         sae.floorHeight = P_FindHighestFloorSurrounding(&sector, true);
         if(sae.floorHeight != lastFloorHeight)
            sae.floorHeight += 8 * FRACUNIT;
         break;
         
      case VLS_S1FloorLowerToLowest:
      case VLS_SRFloorLowerToLowest:
      case VLS_W1FloorLowerToLowest:
      case VLS_WRFloorLowerToLowest:
      case VLS_W1FloorLowerAndChange:
      case VLS_WRFloorLowerAndChange:
      case VLS_S1FloorLowerAndChange:
      case VLS_SRFloorLowerAndChange:
         if(floorBlocked)
            return;
         sae.floorHeight = P_FindLowestFloorSurrounding(&sector, true);
         break;
         
      case VLS_S1PlatRaise24Change:
      case VLS_SRPlatRaise24Change:
      case VLS_W1PlatRaise24Change:
      case VLS_WRPlatRaise24Change:
      case VLS_W1RaiseFloor24:
      case VLS_WRRaiseFloor24:
      case VLS_S1RaiseFloor24:
      case VLS_SRRaiseFloor24:
      case VLS_W1RaiseFloor24Change:
      case VLS_WRRaiseFloor24Change:
      case VLS_S1RaiseFloor24Change:
      case VLS_SRRaiseFloor24Change:
         amount = 24;
         goto platraise;
      case VLS_S1PlatRaise32Change:
      case VLS_SRPlatRaise32Change:
      case VLS_W1PlatRaise32Change:
      case VLS_WRPlatRaise32Change:
         amount = 32;
         goto platraise;
      case VLS_S1RaiseFloor512:
      case VLS_SRRaiseFloor512:
      case VLS_W1RaiseFloor512:
      case VLS_WRRaiseFloor512:
          amount = 512;
      platraise:
         if(floorBlocked)
            return;
         sae.floorHeight += amount * FRACUNIT;
         break;
         
      case VLS_W1FloorRaiseToTexture:
      case VLS_WRFloorRaiseToTexture:
      case VLS_S1FloorRaiseToTexture:
      case VLS_SRFloorRaiseToTexture:
      {
         if(floorBlocked)
            return;
         fixed_t minsize = getMinTextureSize(sector);
         if(comp[comp_model])
            sae.floorHeight += minsize;
         else
         {
            sae.floorHeight = (sae.floorHeight >> FRACBITS)
            + (minsize >> FRACBITS);
            if(sae.floorHeight > 32000)
               sae.floorHeight = 32000;
            sae.floorHeight <<= FRACBITS;
         }
         break;
      }

      case VLS_W1PlatPerpetualRaise:
      case VLS_WRPlatPerpetualRaise:
      case VLS_S1PlatPerpetualRaise:
      case VLS_SRPlatPerpetualRaise:
      case VLS_SRPlatToggleUpDown:
      case VLS_WRPlatToggleUpDown:
          if (floorBlocked)
              return;
          sae.floorHeight = P_FindLowestFloorSurrounding(&sector, true);
          if (sae.floorHeight > lastFloorHeight)
              sae.floorHeight = lastFloorHeight;

          sae.altFloorHeight = P_FindHighestFloorSurrounding(&sector, true);
          if (sae.altFloorHeight < lastFloorHeight)
              sae.altFloorHeight = lastFloorHeight;

          sae.floorTerminal = true;

          break;
         
      case VLS_G1ExitLevel:
      case VLS_G1SecretExit:
      case VLS_G1StartLineScript:
      case VLS_GRStartLineScript:
      case VLS_S1CeilingCrushStop:
      case VLS_S1ChangeOnly:
      case VLS_S1ChangeOnlyNumeric:
      case VLS_S1ExitLevel:
      case VLS_S1LightsVeryDark:
      case VLS_S1LightTurnOn:
      case VLS_S1LightTurnOn255:
      case VLS_S1PlatStop:
      case VLS_S1SecretExit:
      case VLS_S1StartLightStrobing:
      case VLS_S1StartLineScript:
      case VLS_S1SilentTeleport:
      case VLS_S1Teleport:
      case VLS_S1TurnTagLightsOff:
      case VLS_SRCeilingCrushStop:
      case VLS_SRChangeOnly:
      case VLS_SRChangeOnlyNumeric:
      case VLS_SRLightsVeryDark:
      case VLS_SRLightTurnOn:
      case VLS_SRLightTurnOn255:
      case VLS_SRPlatStop:
      case VLS_SRSilentTeleport:
      case VLS_SRStartLightStrobing:
      case VLS_SRStartLineScript:
      case VLS_SRTeleport:
      case VLS_SRTurnTagLightsOff:
      case VLS_W1CeilingCrushStop:
      case VLS_W1ChangeOnly:
      case VLS_W1ChangeOnlyNumeric:
      case VLS_W1LightsVeryDark:
      case VLS_W1LightTurnOn:
      case VLS_W1LightTurnOn255:
      case VLS_W1PlatStop:
      case VLS_W1SilentLineTeleMonsters:
      case VLS_W1SilentLineTeleport:
      case VLS_W1SilentLineTeleportReverse:
      case VLS_W1SilentLineTRMonsters:
      case VLS_W1SilentTeleport:
      case VLS_W1SilentTeleportMonsters:
      case VLS_W1StartLightStrobing:
      case VLS_W1StartLineScript:
      case VLS_W1StartLineScript1S:
      case VLS_W1Teleport:
      case VLS_W1TeleportMonsters:
      case VLS_W1TurnTagLightsOff:
      case VLS_WRCeilingCrushStop:
      case VLS_WRChangeOnly:
      case VLS_WRChangeOnlyNumeric:
      case VLS_WRExitLevel:
      case VLS_WRLightsVeryDark:
      case VLS_WRLightTurnOn:
      case VLS_WRLightTurnOn255:
      case VLS_WRPlatStop:
      case VLS_WRSecretExit:
      case VLS_WRSilentLineTeleMonsters:
      case VLS_WRSilentLineTeleport:
      case VLS_WRSilentLineTeleportReverse:
      case VLS_WRSilentLineTRMonsters:
      case VLS_WRSilentTeleport:
      case VLS_WRSilentTeleportMonsters:
      case VLS_WRStartLightStrobing:
      case VLS_WRStartLineScript:
      case VLS_WRStartLineScript1S:
      case VLS_WRTeleport:
      case VLS_WRTeleportMonsters:
      case VLS_WRTurnTagLightsOff:
         return;  // nope
         
         // CRUSHERS: just say "floor + 8" so the bot can avoid them
         // EDIT: bad idea, unless i also put altCeilheight = original height,
         // which is pointless. So just have no effect.
      case VLS_S1CeilingCrushAndRaise:
      case VLS_SRCeilingCrushAndRaise:
      case VLS_W1CeilingCrushAndRaise:
      case VLS_WRCeilingCrushAndRaise:
      case VLS_W1FastCeilCrushRaise:
      case VLS_WRFastCeilCrushRaise:
      case VLS_S1FastCeilCrushRaise:
      case VLS_SRFastCeilCrushRaise:
      case VLS_W1SilentCrushAndRaise:
      case VLS_WRSilentCrushAndRaise:
      case VLS_S1SilentCrushAndRaise:
      case VLS_SRSilentCrushAndRaise:
      {
          const CeilingThinker* ct = thinker_cast<const CeilingThinker*>(sector.ceilingdata);
          if (ct && ct->inStasis)
          {
              sae.ceilingHeight = ct->topheight;
              sae.ceilingTerminal = true;
          }
          else
          {
//              if (ceilingBlocked)
                  return;
//              sae.ceilingHeight = sae.floorHeight + 8 * FRACUNIT;
//              sae.ceilingTerminal = true;
          }
      }
         break;
         
      case VLS_G1PlatRaiseNearestChange:
      case VLS_S1PlatRaiseNearestChange:
      case VLS_SRPlatRaiseNearestChange:
      case VLS_W1PlatRaiseNearestChange:
      case VLS_WRPlatRaiseNearestChange:
      case VLS_S1FloorRaiseToNearest:
      case VLS_SRFloorRaiseToNearest:
      case VLS_W1FloorRaiseToNearest:
      case VLS_WRFloorRaiseToNearest:
      case VLS_WRRaiseFloorTurbo:
      case VLS_W1RaiseFloorTurbo:
      case VLS_S1RaiseFloorTurbo:
      case VLS_SRRaiseFloorTurbo:
         if(floorBlocked)
            return;
         sae.floorHeight = P_FindNextHighestFloor(&sector, sae.floorHeight,
                                                  true);
         break;
         
      case VLS_G1RaiseFloor:
      case VLS_W1RaiseFloor:
      case VLS_WRRaiseFloor:
      case VLS_S1RaiseFloor:
      case VLS_SRRaiseFloor:
         if(floorBlocked)
            return;
         sae.floorHeight = P_FindLowestCeilingSurrounding(&sector, true);
         if(sae.floorHeight > sae.ceilingHeight)
            sae.floorHeight = sae.ceilingHeight;
         break;

      case VLS_S1FloorRaiseCrush:
      case VLS_SRFloorRaiseCrush:
      case VLS_W1FloorRaiseCrush:
      case VLS_WRFloorRaiseCrush:
          if (floorBlocked)
              return;
          sae.floorHeight = P_FindLowestCeilingSurrounding(&sector, true);
          if (sae.floorHeight > sae.ceilingHeight)
              sae.floorHeight = sae.ceilingHeight;
          sae.floorHeight -= 8 * FRACUNIT;
          break;
         
      case VLS_W1RaiseCeilingLowerFloor:  // just ceiling
      case VLS_WRRaiseCeilingLowerFloor:
         if(ceilingBlocked)
            return;
         sae.ceilingHeight = P_FindHighestCeilingSurrounding(&sector, true);
         break;
         
      case VLS_S1BOOMRaiseCeilingOrLowerFloor:
      case VLS_SRBOOMRaiseCeilingOrLowerFloor:
         if(!ceilingBlocked)
            sae.ceilingHeight = P_FindHighestCeilingSurrounding(&sector, true);
         else if(!floorBlocked)
            sae.floorHeight = P_FindLowestFloorSurrounding(&sector, true);
         else
            return;
         break;
         
      case VLS_S1BuildStairsUp8:
      case VLS_SRBuildStairsUp8:
      case VLS_W1BuildStairsUp8:
      case VLS_WRBuildStairsUp8:
      case VLS_S1BuildStairsTurbo16:
      case VLS_WRBuildStairsTurbo16:
      case VLS_W1BuildStairsTurbo16:
      case VLS_SRBuildStairsTurbo16:
         if(floorBlocked)
            return;
         othersAffected = fillInStairs(&sector, sae, vls, indexList);
         break;
      case VLS_S1DoDonut:
      case VLS_SRDoDonut:
      case VLS_W1DoDonut:
      case VLS_WRDoDonut:
         if(floorBlocked)
            return;
         othersAffected = fillInDonut(sector, sae, vls, indexList);
         break;

      case VLS_W1ElevatorUp:
      case VLS_WRElevatorUp:
      case VLS_S1ElevatorUp:
      case VLS_SRElevatorUp:
          if (floorBlocked || ceilingBlocked)
              return;
          sae.floorHeight = P_FindNextHighestFloor(&sector, lastFloorHeight, true);
          sae.ceilingHeight = sae.floorHeight + lastCeilingHeight - lastFloorHeight;
          break;

      case VLS_W1ElevatorDown:
      case VLS_WRElevatorDown:
      case VLS_S1ElevatorDown:
      case VLS_SRElevatorDown:
          if (floorBlocked || ceilingBlocked)
              return;
          sae.floorHeight = P_FindNextLowestFloor(&sector, lastFloorHeight, true);
          sae.ceilingHeight = sae.floorHeight + lastCeilingHeight - lastFloorHeight;
          break;

      case VLS_W1ElevatorCurrent:
      case VLS_WRElevatorCurrent:
      case VLS_S1ElevatorCurrent:
      case VLS_SRElevatorCurrent:
      {
          if (floorBlocked || ceilingBlocked)
              return;
          const SectorHeightStack& front = g_affectedSectors[line.frontsector - sectors];

          sae.floorHeight = front.getFloorHeight();
          sae.ceilingHeight = sae.floorHeight + lastCeilingHeight - lastFloorHeight;
          break;
      }
      default:
         return;
         
   }
   
   if(!othersAffected && sae.floorHeight == lastFloorHeight
      && sae.ceilingHeight == lastCeilingHeight && (!sae.floorTerminal || sae.altFloorHeight == lastFloorHeight))
   {
      return;  // nothing changed; do not register.
   }
   
   // Okay.
   indexList.add(secnum);
   affSector.stack.add(sae);
}

static bool fillInStairs(const sector_t* sector, SectorStateEntry& sae,
                         VanillaLineSpecial vls, PODCollection<int>& coll)
{
   fixed_t stairIncrement;
   
   if(vls == VLS_S1BuildStairsTurbo16 || vls == VLS_SRBuildStairsTurbo16
      || vls == VLS_W1BuildStairsTurbo16 || vls == VLS_WRBuildStairsTurbo16)
   {
      stairIncrement = 16 * FRACUNIT;
   }
   else
      stairIncrement = 8 * FRACUNIT;
   
   fixed_t height = sae.floorHeight + stairIncrement;
   sae.floorHeight = height;
   bool ok;
   const sector_t* tsec;
   bool othersAffected = false;
   SectorHeightStack* otherAffSector;
   int i;
   do
   {
      // CODE LARGELY TAKEN FROM EV_BuildStairs
      ok = false;
      for(i = 0; i < sector->linecount; ++i)
      {
         tsec = sector->lines[i]->frontsector;
         if(tsec != sector || !(sector->lines[i]->flags & ML_TWOSIDED))
            continue;
         tsec = sector->lines[i]->backsector;
         if(!tsec || tsec->floorpic != sector->floorpic)
            continue;
         if(comp[comp_stairs] || demo_version == 203)
            height += stairIncrement;
         
         otherAffSector = &g_affectedSectors[tsec - sectors];
         if(otherAffSector->isFloorTerminal())
            continue;
         if(!comp[comp_stairs] && demo_version != 203)
            height += stairIncrement;
         
         // Follow up
         sector = tsec;

         // Now create the new entry
         if(!othersAffected && height != otherAffSector->getFloorHeight())
            othersAffected = true;
         
         SectorStateEntry tsae;
         tsae.actionNumber = vls;
         tsae.floorHeight = height;
         tsae.ceilingHeight = otherAffSector->getCeilingHeight();
         tsae.floorTerminal = false;
         tsae.ceilingTerminal = otherAffSector->isCeilingTerminal();
         otherAffSector->stack.add(tsae);
         coll.add(static_cast<int>(tsec - sectors));
         ok = true;
      }
   }while(ok);
   return othersAffected;
}

// CODE LARGELY TAKEN FROM EV_DoDonut
static bool fillInDonut(const sector_t& sector, SectorStateEntry& sae,
                        VanillaLineSpecial vls, PODCollection<int>& coll)
{
   const sector_t* s2 = getNextSector(sector.lines[0], &sector);
   if(!s2)
      return false;
   
   SectorHeightStack& otherAffSector = g_affectedSectors[s2 - sectors];
   if(!comp[comp_floors] && otherAffSector.isFloorTerminal())
      return false;
   
   const sector_t* s3;
   fixed_t s3_floorheight;
   int16_t s3_floorpic;
   const SectorHeightStack* thirdAffSector;
   bool othersAffected = false;
   for (int i = 0; i < s2->linecount; ++i)
   {
      if(comp[comp_model])
      {
         if(s2->lines[i]->backsector == &sector)
            continue;
      }
      else if(!s2->lines[i]->backsector || s2->lines[i]->backsector == &sector)
         continue;
      
      s3 = s2->lines[i]->backsector;
      
      if(!s3)
      {
         if(!DonutOverflow(&s3_floorheight, &s3_floorpic))
            return false;
      }
      else
      {
         thirdAffSector = &g_affectedSectors[s3 - sectors];
         s3_floorheight = thirdAffSector->getFloorHeight();
         s3_floorpic = thirdAffSector->getCeilingHeight();
      }
      
      if(s3_floorheight != otherAffSector.getFloorHeight())
         othersAffected = true;
      
      // ok
      // rising moat
      SectorStateEntry s2ae;
      s2ae.actionNumber = vls;
      s2ae.floorHeight = s3_floorheight;
      s2ae.ceilingHeight = otherAffSector.getCeilingHeight();
      // CAUTION: we're ignoring if it's already terminal!!
      s2ae.floorTerminal = otherAffSector.isFloorTerminal();
      s2ae.ceilingTerminal = otherAffSector.isCeilingTerminal();
      otherAffSector.stack.add(s2ae);
      coll.add(static_cast<int>(s2 - sectors));
      
      sae.floorHeight = s3_floorheight;
      
      return othersAffected;
   }
   
   return false;
}

// CODE LARGELY TAKEN FROM EV_DoFloor
static fixed_t getMinTextureSize(const sector_t& sector)
{
   fixed_t minsize = D_MAXINT;
   if(!comp[comp_model])
      minsize = 32000 << FRACBITS;
   int secnum;
   const side_t* side;
   for(int i = 0; i < sector.linecount; ++i)
   {
      secnum = static_cast<int>(&sector - sectors);
      if(twoSided(secnum, i))
      {
         side = getSide(secnum, i, 0);
         if(side->bottomtexture >= 0
            && (side->bottomtexture || comp[comp_model]))
         {
            if(textures[side->bottomtexture]->heightfrac < minsize)
               minsize = textures[side->bottomtexture]->heightfrac;
         }
         side = getSide(secnum, i, 1);
         if(side->bottomtexture >= 0
            && (side->bottomtexture || comp[comp_model]))
         {
            if(textures[side->bottomtexture]->heightfrac < minsize)
               minsize = textures[side->bottomtexture]->heightfrac;
         }
      }
   }
   return minsize;
}

//
// LevelStateStack::Ceiling
//
fixed_t LevelStateStack::Ceiling(const sector_t& sector)
{
   return g_affectedSectors[&sector - sectors].getCeilingHeight();
}

//
// LevelStateStack::Floor
//
fixed_t LevelStateStack::Floor(const sector_t& sector)
{
   return g_affectedSectors[&sector - sectors].getFloorHeight();
}

fixed_t LevelStateStack::AltFloor(const sector_t& sector)
{
    const SectorHeightStack& shs = g_affectedSectors[&sector - sectors];
    return shs.getAltFloorHeight();
}

bool LevelStateStack::IsClear()
{
    return g_indexListStack.isEmpty();
}

void LevelStateStack::SetKeyPlayer(const player_t* player)
{
   g_keyPlayer = player;
}

void LevelStateStack::UseRealHeights(bool value)
{
   g_useRealHeights = value;
}

//
// True if the linedef special triggers a backsector
//
bool B_LineTriggersBackSector(const line_t &line)
{
   if(!line.special)
      return false;
   // check for generalized
   int gentype = EV_GenTypeForSpecial(line.special);
   if(gentype >= GenTypeFloor)
   {
      int genspac = EV_GenActivationType(line.special);
      return genspac == PushOnce || genspac == PushMany;
   }

   const ev_action_t *action = EV_ActionForSpecial(line.special);
   if(!action)
      return false;

   EVActionFunc func = action->action;

   if(func == EV_ActionVerticalDoor)   // always
      return true;

   static std::unordered_set<EVActionFunc> hybridFuncs = {
      // Sector changers may matter, though
      EV_ActionChangeOnly,
      EV_ActionChangeOnlyNumeric,
      // Lights don't really matter for the bot
//      EV_ActionLightTurnOn,
//      EV_ActionTurnTagLightsOff
   };

   static std::unordered_set<EVActionFunc> paramFuncs = {
      EV_ActionFloorWaggle,
      EV_ActionHereticDoorRaise3x,
      EV_ActionHereticStairsBuildUp16FS,
      EV_ActionHereticStairsBuildUp8FS,
      EV_ActionParamCeilingCrushAndRaise,
      EV_ActionParamCeilingCrushAndRaiseA,
      EV_ActionParamCeilingCrushAndRaiseDist,
      EV_ActionParamCeilingCrushAndRaiseSilentA,
      EV_ActionParamCeilingCrushAndRaiseSilentDist,
      EV_ActionParamCeilingCrushRaiseAndStay,
      EV_ActionParamCeilingCrushRaiseAndStayA,
      EV_ActionParamCeilingCrushRaiseAndStaySilA,
      EV_ActionParamCeilingGeneric,
      EV_ActionParamCeilingLowerAndCrush,
      EV_ActionParamCeilingLowerAndCrushDist,
      EV_ActionParamCeilingLowerByTexture,
      EV_ActionParamCeilingLowerByValue,
      EV_ActionParamCeilingLowerByValueTimes8,
      EV_ActionParamCeilingLowerInstant,
      EV_ActionParamCeilingLowerToFloor,
      EV_ActionParamCeilingLowerToHighestFloor,
      EV_ActionParamCeilingLowerToNearest,
      EV_ActionParamCeilingMoveToValue,
      EV_ActionParamCeilingMoveToValueTimes8,
      EV_ActionParamCeilingRaiseByTexture,
      EV_ActionParamCeilingRaiseByValue,
      EV_ActionParamCeilingRaiseByValueTimes8,
      EV_ActionParamCeilingRaiseInstant,
      EV_ActionParamCeilingRaiseToHighest,
      EV_ActionParamCeilingRaiseToHighestFloor,
      EV_ActionParamCeilingRaiseToLowest,
      EV_ActionParamCeilingRaiseToNearest,
      EV_ActionParamCeilingToFloorInstant,
      EV_ActionParamCeilingToHighestInstant,
      EV_ActionParamDonut,
      EV_ActionParamDoorClose,
      EV_ActionParamDoorCloseWaitOpen,
      EV_ActionParamDoorLockedRaise,
      EV_ActionParamDoorOpen,
      EV_ActionParamDoorRaise,
      EV_ActionParamDoorWaitClose,
      EV_ActionParamDoorWaitRaise,
      EV_ActionParamEEFloorLowerToHighest,
      EV_ActionParamElevatorCurrent,
      EV_ActionParamElevatorDown,
      EV_ActionParamElevatorUp,
      EV_ActionParamFloorCeilingLowerByValue,
      EV_ActionParamFloorCeilingLowerRaise,
      EV_ActionParamFloorCeilingRaiseByValue,
      EV_ActionParamFloorGeneric,
      EV_ActionParamFloorLowerByTexture,
      EV_ActionParamFloorLowerByValue,
      EV_ActionParamFloorLowerByValueTimes8,
      EV_ActionParamFloorLowerInstant,
      EV_ActionParamFloorLowerToHighest,
      EV_ActionParamFloorLowerToLowest,
      EV_ActionParamFloorLowerToLowestCeiling,
      EV_ActionParamFloorLowerToNearest,
      EV_ActionParamFloorMoveToValue,
      EV_ActionParamFloorMoveToValueTimes8,
      EV_ActionParamFloorRaiseAndCrush,
      EV_ActionParamFloorRaiseByTexture,
      EV_ActionParamFloorRaiseByValue,
      EV_ActionParamFloorRaiseByValueTimes8,
      EV_ActionParamFloorRaiseInstant,
      EV_ActionParamFloorRaiseToCeiling,
      EV_ActionParamFloorRaiseToHighest,
      EV_ActionParamFloorRaiseToLowest,
      EV_ActionParamFloorRaiseToLowestCeiling,
      EV_ActionParamFloorRaiseToNearest,
      EV_ActionParamFloorToCeilingInstant,
      EV_ActionParamGenCrusher,
      EV_ActionParamPlatDWUS,
      EV_ActionParamPlatDWUSLip,
      EV_ActionParamPlatDownByValue,
      EV_ActionParamPlatPerpetualRaise,
      EV_ActionParamPlatPerpetualRaiseLip,
      EV_ActionParamPlatRaiseChange,
      EV_ActionParamPlatRaiseNearestChange,
      EV_ActionParamPlatToggleCeiling,
      EV_ActionParamPlatUWDS,
      EV_ActionParamPlatUpByValue,
      EV_ActionParamStairsBuildDownDoom,
      EV_ActionParamStairsBuildDownDoomSync,
      EV_ActionParamStairsBuildUpDoom,
      EV_ActionParamStairsBuildUpDoomCrush,
      EV_ActionParamStairsBuildUpDoomSync,
      EV_ActionPillarBuild,
      EV_ActionPillarBuildAndCrush,
      EV_ActionPillarOpen,
   };

   if(!line.args[0] && (paramFuncs.count(func) || (hybridFuncs.count(func) &&
      EV_CompositeActionFlags(action) & EV_PARAMLINESPEC)))
   {
      return true;
   }

   return false;
}

bool B_VlsTypeIsStair(VanillaLineSpecial vls)
{
   switch(vls)
   {
      case VLS_S1BuildStairsUp8:
      case VLS_SRBuildStairsUp8:
      case VLS_W1BuildStairsUp8:
      case VLS_WRBuildStairsUp8:
      case VLS_S1BuildStairsTurbo16:
      case VLS_WRBuildStairsTurbo16:
      case VLS_W1BuildStairsTurbo16:
      case VLS_SRBuildStairsTurbo16:
         return true;
      default:
         break;
   }
   return false;
}

bool B_SectorTypeIsHarmless(int16_t special)
{
   auto vss = static_cast<VanillaSectorSpecial>(special);
   switch (vss)
   {
      case VSS_DamageHellSlime:
      case VSS_DamageNukage:
      case VSS_DoorCloseIn30:
      case VSS_ExitSuperDamage:
      case VSS_DoorRaiseIn5Mins:
      case VSS_DamageSuperHellSlime:
         return false;
      default:
         break;
   }
   return true;
}
////////////////////////////////////////////////////////////////////////////////

static bool B_sectorIs30sClosingDoor(int special)
{
   const ev_sectorbinding_t *binding = EV_IsGenSectorSpecial(special) ?
   EV_GenBindingForSectorSpecial(special) :
   EV_BindingForSectorSpecial(special);

   return binding && binding->apply == EV_SectorDoorCloseIn30;
}

static bool B_sectorIs5minRisingDoor(int special)
{
   const ev_sectorbinding_t *binding = EV_IsGenSectorSpecial(special) ?
   EV_GenBindingForSectorSpecial(special) :
   EV_BindingForSectorSpecial(special);

   return binding && binding->apply == EV_SectorDoorRaiseIn5Mins;
}

//! Inserts stairs from source sector into set
static void B_insertStairs(const sector_t *sector, bool ignoreTextures, 
   std::unordered_set<const sector_t *> &set)
{
   set.insert(sector);
   int ok;
   int secnum = static_cast<int>(sector - sectors);

   std::unordered_set<const sector_t *> visited;
   visited.insert(sector); // equivalent of putting a thinker over it

   do
   {
      ok = 0;
      for (int i = 0; i < sector->linecount; ++i)
      {
         if (!(sector->lines[i])->backsector)
            continue;

         const sector_t *tsec = sector->lines[i]->frontsector;
         int newsecnum = static_cast<int>(tsec - sectors);

         if (secnum != newsecnum)
            continue;

         tsec = sector->lines[i]->backsector;
         newsecnum = static_cast<int>(tsec - sectors);

         if (!ignoreTextures && tsec->floorpic != sector->floorpic)
            continue;

         if (visited.count(tsec))
            continue;

         sector = tsec;
         secnum = newsecnum;

         set.insert(sector);
         visited.insert(sector);

         ok = 1;
         break;
      }
   } while (ok);
}

static void B_insertDonut(const sector_t *s1, std::unordered_set<const sector_t *> &set)
{
   const sector_t *s2 = getNextSector(s1->lines[0], s1);
   const sector_t *s3;
   if (!s2)
      return;

   fixed_t s3_floorheight;
   int16_t s3_floorpic;

   for (int i = 0; i < s2->linecount; ++i)
   {
      if (comp[comp_model])
      {
         if (s2->lines[i]->backsector == s1)
            continue;
      }
      else if (!s2->lines[i]->backsector || s2->lines[i]->backsector == s1)
         continue;

      s3 = s2->lines[i]->backsector;
      if (!s3)
      {
         if (!DonutOverflow(&s3_floorheight, &s3_floorpic))
            return;
      }

      set.insert(s1);
      set.insert(s2);
      break;
   }
 
}

//! Handles generalized types thereof
static void B_collectGeneralizedTargetSectors(const line_t *line, bool noshoot,
   std::unordered_set<const sector_t *> &set)
{
   bool isStair = line->special >= GenStairsBase && line->special < GenLiftBase;
   bool stairIgnoreTextures = false;
   if (isStair)   // stairs are more complex
      stairIgnoreTextures = !!((line->special - GenStairsBase & StairIgnore) >> StairIgnoreShift);

   int trigger_type = (line->special & TriggerType) >> TriggerTypeShift;
   if (noshoot && (trigger_type == GunOnce || trigger_type == GunMany))
      return;
   if (trigger_type == PushOnce || trigger_type == PushMany)
   {
      if (!line->backsector)
         return;  // dubious situation
      if (isStair)
         B_insertStairs(line->backsector, stairIgnoreTextures, set);
      else
         set.insert(line->backsector);
      return;
   }

   int secnum = -1;
   while ((secnum = P_FindSectorFromLineArg0(line, secnum)) >= 0)
   {
      if(isStair)
         B_insertStairs(&sectors[secnum], stairIgnoreTextures, set);
      else
         set.insert(&sectors[secnum]);
   }
}

//! True if it targets sectors and not other objects
static bool B_paramActionTargetsSectors(const ev_action_t *action)
{
   static std::unordered_set<EVActionFunc> set;
   if (set.empty())
   {
      set.insert(EV_ActionParamDoorRaise);
      set.insert(EV_ActionParamDoorOpen);
      set.insert(EV_ActionParamDoorClose);
      set.insert(EV_ActionParamDoorCloseWaitOpen);
      set.insert(EV_ActionParamDoorWaitRaise);
      set.insert(EV_ActionParamDoorWaitClose);
      set.insert(EV_ActionParamDoorLockedRaise);
      set.insert(EV_ActionParamFloorRaiseToHighest);
      set.insert(EV_ActionParamEEFloorLowerToHighest);
      set.insert(EV_ActionParamFloorLowerToHighest);
      set.insert(EV_ActionParamFloorRaiseToLowest);
      set.insert(EV_ActionParamFloorLowerToLowest);
      set.insert(EV_ActionParamFloorRaiseToNearest);
      set.insert(EV_ActionParamFloorLowerToNearest);
      set.insert(EV_ActionParamFloorRaiseToLowestCeiling);
      set.insert(EV_ActionParamFloorLowerToLowestCeiling);
      set.insert(EV_ActionParamFloorRaiseToCeiling);
      set.insert(EV_ActionParamFloorRaiseByTexture);
      set.insert(EV_ActionParamFloorLowerByTexture);
      set.insert(EV_ActionParamFloorRaiseByValue);
      set.insert(EV_ActionParamFloorRaiseByValueTimes8);
      set.insert(EV_ActionParamFloorLowerByValue);
      set.insert(EV_ActionParamFloorLowerByValueTimes8);
      set.insert(EV_ActionParamFloorMoveToValue);
      set.insert(EV_ActionParamFloorMoveToValueTimes8);
      set.insert(EV_ActionParamFloorRaiseInstant);
      set.insert(EV_ActionParamFloorLowerInstant);
      set.insert(EV_ActionParamFloorToCeilingInstant);
      set.insert(EV_ActionParamCeilingRaiseToHighest);
      set.insert(EV_ActionParamCeilingToHighestInstant);
      set.insert(EV_ActionParamCeilingRaiseToNearest);
      set.insert(EV_ActionParamCeilingLowerToNearest);
      set.insert(EV_ActionParamCeilingRaiseToLowest);
      set.insert(EV_ActionParamCeilingLowerToLowest);
      set.insert(EV_ActionParamCeilingRaiseToHighestFloor);
      set.insert(EV_ActionParamCeilingLowerToHighestFloor);
      set.insert(EV_ActionParamCeilingToFloorInstant);
      set.insert(EV_ActionParamCeilingLowerToFloor);
      set.insert(EV_ActionParamCeilingRaiseByTexture);
      set.insert(EV_ActionParamCeilingLowerByTexture);
      set.insert(EV_ActionParamCeilingRaiseByValue);
      set.insert(EV_ActionParamCeilingLowerByValue);
      set.insert(EV_ActionParamCeilingRaiseByValueTimes8);
      set.insert(EV_ActionParamCeilingLowerByValueTimes8);
      set.insert(EV_ActionParamCeilingMoveToValue);
      set.insert(EV_ActionParamCeilingMoveToValueTimes8);
      set.insert(EV_ActionParamCeilingRaiseInstant);
      set.insert(EV_ActionParamCeilingLowerInstant);
      set.insert(EV_ActionParamCeilingCrushAndRaise);
      set.insert(EV_ActionParamCeilingCrushAndRaiseA);
      set.insert(EV_ActionParamCeilingCrushAndRaiseSilentA);
      set.insert(EV_ActionParamCeilingCrushAndRaiseDist);
      set.insert(EV_ActionParamCeilingCrushAndRaiseSilentDist);
      set.insert(EV_ActionParamCeilingCrushStop);
      set.insert(EV_ActionParamCeilingCrushRaiseAndStay);
      set.insert(EV_ActionParamCeilingCrushRaiseAndStayA);
      set.insert(EV_ActionParamCeilingCrushRaiseAndStaySilA);
      set.insert(EV_ActionParamCeilingLowerAndCrush);
      set.insert(EV_ActionParamCeilingLowerAndCrushDist);
      set.insert(EV_ActionParamGenCrusher);
      set.insert(EV_ActionParamStairsBuildUpDoom);
      set.insert(EV_ActionParamStairsBuildDownDoom);
      set.insert(EV_ActionParamStairsBuildUpDoomSync);
      set.insert(EV_ActionParamStairsBuildDownDoomSync);
      set.insert(EV_ActionPillarBuild);
      set.insert(EV_ActionPillarBuildAndCrush);
      set.insert(EV_ActionPillarOpen);
      set.insert(EV_ActionFloorWaggle);
      set.insert(EV_ActionParamPlatPerpetualRaise);
      set.insert(EV_ActionParamPlatStop);
      set.insert(EV_ActionParamPlatDWUS);
      set.insert(EV_ActionParamPlatDownByValue);
      set.insert(EV_ActionParamPlatUWDS);
      set.insert(EV_ActionParamPlatUpByValue);
      set.insert(EV_ActionParamDonut);
      // lights are ignored because they have no gameplay effect
      // exits have no purpose here
   }

   return !!set.count(action->action);
}

static bool B_classicActionTargetsSectors(const ev_action_t *action)
{
   static std::unordered_set<EVActionFunc> set;
   if (set.empty())
   {
      set.insert(EV_ActionOpenDoor);
      set.insert(EV_ActionCloseDoor);
      set.insert(EV_ActionRaiseDoor);
      set.insert(EV_ActionRaiseFloor);
      set.insert(EV_ActionFastCeilCrushRaise);
      set.insert(EV_ActionBuildStairsUp8);
      set.insert(EV_ActionPlatDownWaitUpStay);
      set.insert(EV_ActionCloseDoor30);
      set.insert(EV_ActionLowerFloor);
      set.insert(EV_ActionPlatRaiseNearestChange);
      set.insert(EV_ActionCeilingCrushAndRaise);
      set.insert(EV_ActionFloorRaiseToTexture);
      set.insert(EV_ActionLowerFloorTurbo);
      set.insert(EV_ActionFloorLowerAndChange);
      set.insert(EV_ActionFloorLowerToLowest);
      set.insert(EV_ActionRaiseCeilingLowerFloor);
      set.insert(EV_ActionBOOMRaiseCeilingLowerFloor);
      set.insert(EV_ActionBOOMRaiseCeilingOrLowerFloor);
      set.insert(EV_ActionCeilingLowerAndCrush);
      set.insert(EV_ActionPlatPerpetualRaise);
      set.insert(EV_ActionFloorRaiseCrush);
      set.insert(EV_ActionRaiseFloor24);
      set.insert(EV_ActionRaiseFloor24Change);
      set.insert(EV_ActionBuildStairsTurbo16);
      set.insert(EV_ActionDoorBlazeRaise);
      set.insert(EV_ActionDoorBlazeOpen);
      set.insert(EV_ActionDoorBlazeClose);
      set.insert(EV_ActionFloorRaiseToNearest);
      set.insert(EV_ActionPlatBlazeDWUS);
      set.insert(EV_ActionRaiseFloorTurbo);
      set.insert(EV_ActionSilentCrushAndRaise);
      set.insert(EV_ActionRaiseFloor512);
      set.insert(EV_ActionPlatRaise24Change);
      set.insert(EV_ActionPlatRaise32Change);
      set.insert(EV_ActionCeilingLowerToFloor);
      set.insert(EV_ActionDoDonut);
      set.insert(EV_ActionCeilingLowerToLowest);
      set.insert(EV_ActionCeilingLowerToMaxFloor);
      set.insert(EV_ActionFloorLowerToNearest);
      set.insert(EV_ActionElevatorUp);
      set.insert(EV_ActionElevatorDown);
      set.insert(EV_ActionElevatorCurrent);
      set.insert(EV_ActionPlatToggleUpDown);
      set.insert(EV_ActionVerticalDoor);
      set.insert(EV_ActionDoLockedDoor);
      set.insert(EV_ActionLowerFloorTurboA);
      set.insert(EV_ActionHereticDoorRaise3x);
      set.insert(EV_ActionHereticStairsBuildUp8FS);
      set.insert(EV_ActionHereticStairsBuildUp16FS);
   }

   return !!set.count(action->action);
}

static void B_collectParameterizedTargetSectors(const line_t *line, const ev_action_t *action, 
   std::unordered_set<const sector_t *> &set)
{
   // TODO: also check more combos
   if (!(line->extflags & (EX_ML_PLAYER | EX_ML_MONSTER | EX_ML_MISSILE)) ||
      !(line->extflags & (EX_ML_CROSS | EX_ML_IMPACT | EX_ML_PUSH | EX_ML_USE)))
   {
      return;  // inaccessible
   }


   if (!B_paramActionTargetsSectors(action))
      return;

   static std::unordered_set<EVActionFunc> stairset;
   if (stairset.empty())
   {
      stairset.insert(EV_ActionParamStairsBuildUpDoom);
      stairset.insert(EV_ActionParamStairsBuildDownDoom);
      stairset.insert(EV_ActionParamStairsBuildUpDoomSync);
      stairset.insert(EV_ActionParamStairsBuildDownDoomSync);
   }

   auto solve = [line, action, &set](const sector_t *sector) {
      if (stairset.count(action->action))
         B_insertStairs(sector, false, set);
      else if (action->action == EV_ActionParamDonut)
         B_insertDonut(sector, set);
      else
         set.insert(sector);
   };
   
   if (!line->args[0])
   {
      if (!line->backsector)
         return;

      solve(line->backsector);
      return;
   }

   int secnum = -1;
   while ((secnum = P_FindSectorFromTag(line->args[0], secnum)) >= 0)
      solve(&sectors[secnum]);
}

static void B_collectClassicTargetSectors(const line_t *line, const ev_action_t *action,
   std::unordered_set<const sector_t *> &set)
{
   if (!B_classicActionTargetsSectors(action))
      return;

   if (action->type == &DRActionType)
   {
      // for the record, all DR action types are simple doors
      if (!line->backsector)
         return;
      set.insert(line->backsector);
      return;
   }

   static std::unordered_set<EVActionFunc> stairset;
   if (stairset.empty())
   {
      stairset.insert(EV_ActionBuildStairsUp8);
      stairset.insert(EV_ActionBuildStairsTurbo16);
      stairset.insert(EV_ActionHereticStairsBuildUp8FS);
      stairset.insert(EV_ActionHereticStairsBuildUp16FS);
   }

   int secnum = -1;
   while ((secnum = P_FindSectorFromLineArg0(line, secnum)) >= 0)
   {
      const sector_t *sector = sectors + secnum;
      if (stairset.count(action->action))
         B_insertStairs(sector, false, set);
      else if (action->action == EV_ActionDoDonut)
         B_insertDonut(sector, set);
      else
         set.insert(sector);
   }
}

//! Subroutine for linedefs targetting sectors
static void B_collectForLine(const line_t *line, bool fromLineEffect, 
   std::unordered_set<const sector_t *> &set)
{
   if (line->special >= GenCrusherBase)
   {
      B_collectGeneralizedTargetSectors(line, fromLineEffect, set);
      return;
   }

   const ev_action_t *action = EV_ActionForSpecial(line->special);
   if (!action)
      return;

   if (EV_CompositeActionFlags(action) & EV_PARAMLINESPEC)
   {
      if (fromLineEffect)   // ignore ALL param specials
         return;
      B_collectParameterizedTargetSectors(line, action, set);
      return;
   }

   if (fromLineEffect && action->type->activation == SPAC_IMPACT)
      return;
   B_collectClassicTargetSectors(line, action, set);
}

//! Looks in the map for moving sectors
static void B_collectActiveSectors(std::unordered_set<const sector_t *> &set)
{
   // 1. Find time-based doors (or boss-based)
   bool tag666used = false, tag667used = false;
   for(int i = 0; i < numsectors; ++i)
   {
      const sector_t *sector = sectors + i;
      if (B_sectorIs30sClosingDoor(sector->special) || B_sectorIs5minRisingDoor(sector->special))
      {
         set.insert(sector);
         continue;
      }
   }

   // 2. Find special linedefs
   for (int i = 0; i < numlines; ++i)
   {
      const line_t *line = lines + i;
      if (!line->special)
         continue;
      B_collectForLine(line, false, set);
   }

   // 3. Look for things with sector targets
   SectorAffectingStates sas;
   bool tag666added = false, tag667added = false;
   for (Thinker *th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      const Mobj *mo;
      if (!(mo = thinker_cast<const Mobj *>(th)))
         continue;

      sas.clear();
      B_GetMobjSectorTargetActions(*mo, sas);

      if (sas.bossDeath)
      {
         if (!tag666added &&
            (LevelInfo.bossSpecs & BSPEC_E1M8 && mo->flags2 & MF2_E1M8BOSS ||
               LevelInfo.bossSpecs & BSPEC_E4M6 && mo->flags2 & MF2_E4M6BOSS ||
               LevelInfo.bossSpecs & BSPEC_E4M8 && mo->flags2 & MF2_E4M8BOSS ||
               LevelInfo.bossSpecs & BSPEC_MAP07_1 && mo->flags2 & MF2_MAP07BOSS1))
         {
            tag666added = true;
            int secnum = -1;
            while ((secnum = P_FindSectorFromTag(666, secnum)) >= 0)
               set.insert(sectors + secnum);
         }
         if (!tag667added && LevelInfo.bossSpecs & BSPEC_MAP07_2 && mo->flags2 & MF2_MAP07BOSS2)
         {
            tag667added = true;
            int secnum = -1;
            while ((secnum = P_FindSectorFromTag(667, secnum)) >= 0)
               set.insert(sectors + secnum);
         }
      }
      if (!tag666added && (sas.keenDie || sas.hticBossDeath &&
         (LevelInfo.bossSpecs & BSPEC_E1M8 && mo->flags2 & MF2_E1M8BOSS ||
            LevelInfo.bossSpecs & BSPEC_E2M8 && mo->flags2 & MF2_E2M8BOSS ||
            LevelInfo.bossSpecs & BSPEC_E3M8 && mo->flags2 & MF2_E3M8BOSS ||
            LevelInfo.bossSpecs & BSPEC_E4M8 && mo->flags2 & MF2_E4M8BOSS ||
            LevelInfo.bossSpecs & BSPEC_E5M8 && mo->flags3 & MF3_E5M8BOSS)))
      {
         tag666added = true;
         int secnum = -1;
         while ((secnum = P_FindSectorFromTag(666, secnum)) >= 0)
            set.insert(sectors + secnum);
      }
      for (const state_t *state : sas.lineEffects)
      {
         // same devious semantics as A_LineEffect
         static line_t s;
         s = *lines;
         if ((s.special = state->misc1))
         {
            s.tag = state->misc2;
            B_collectForLine(&s, true, set);
         }
      }
   }
}

void B_LogActiveSectors()
{
   std::unordered_set<const sector_t *> set;
   B_collectActiveSectors(set);

   for (const sector_t *sector : set)
   {
      B_Log("Active: %d", static_cast<int>(sector - sectors));
   }
}