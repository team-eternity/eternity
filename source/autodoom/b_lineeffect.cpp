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
   if(botMap && botMap->sectorFlags && botMap->sectorFlags[secnum].door.valid)
   {
      int lockID = botMap->sectorFlags[secnum].door.lock;
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
   
   if(B_LineTriggersBackSector(line))
   {
      if(line.backsector)
      {
         B_pushSectorHeights((int)(line.backsector - sectors), line, coll,
                             player);
      }
   }
   else
   {
      while((secnum = P_FindSectorFromLineArg0(&line, secnum)) >= 0)
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
static bool B_fillInStairs(const sector_t* sector, SectorStateEntry& sae,
                           EVActionFunc func, int special,
                           PODCollection<int>& coll);
static bool B_fillInDonut(const sector_t& sector, SectorStateEntry& sae,
                          int special, PODCollection<int>& coll);
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
   
   bool othersAffected = false;

   const ev_action_t *action = EV_ActionForSpecial(line.special);
   if(!action)
      return;
   EVActionFunc func = action->action;
   int lockID;

   if(func == EV_ActionDoorBlazeOpen || func == EV_ActionOpenDoor)
   {
      if(ceilingBlocked || B_LineTriggersBackSector(line) && doorBusy(sector))
         return;
      sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4
      * FRACUNIT;
   }
   else if(func == EV_ActionVerticalDoor)
   {
      if(ceilingBlocked || B_LineTriggersBackSector(line) && doorBusy(sector))
         return;
      lockID = EV_LockDefIDForSpecial(line.special);
      if(lockID && !E_PlayerCanUnlock(&player, lockID, false, true))
         return;
      sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4
      * FRACUNIT;
      switch(line.special)
      {
         case 1:
         case 26:
         case 27:
         case 28:
         case 117:
            sae.ceilingTerminal = true;
            break;
      }
   }
   else if(func == EV_ActionDoLockedDoor)
   {
      if(ceilingBlocked)
         return;
      lockID = EV_LockDefIDForSpecial(line.special);
      if(lockID && !E_PlayerCanUnlock(&player, lockID, true, true))
         return;
      sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4
      * FRACUNIT;
   }
   else if(func == EV_ActionDoorBlazeRaise || func == EV_ActionRaiseDoor)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4
      * FRACUNIT;
      sae.ceilingTerminal = true;
   }
   else if(func == EV_ActionCloseDoor ||
           func == EV_ActionCeilingLowerToFloor ||
           func == EV_ActionDoorBlazeClose)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight = sae.floorHeight;
   }
   else if(func == EV_ActionCeilingLowerAndCrush)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight = sae.floorHeight + 8 * FRACUNIT;
   }
   else if(func == EV_ActionCloseDoor30)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight = sae.floorHeight;
      sae.ceilingTerminal = true;
   }
   else if(func == EV_ActionCeilingLowerToLowest)
   {
      if (ceilingBlocked)
         return;
      sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true);
   }
   else if(func == EV_ActionCeilingLowerToMaxFloor)
   {
      if (ceilingBlocked)
         return;
      sae.ceilingHeight = P_FindHighestFloorSurrounding(&sector, true);
   }
   else if(func == EV_ActionFloorLowerToNearest)
   {
      if (floorBlocked)
         return;
      sae.floorHeight = P_FindNextLowestFloor(&sector, lastFloorHeight, true);
   }
   else if(func == EV_ActionRaiseDoor)
   {
      if(ceilingBlocked || B_LineTriggersBackSector(line) && doorBusy(sector))
         return;
      sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4
      * FRACUNIT;
      sae.ceilingTerminal = true;
   }
   else if(func == EV_ActionLowerFloor)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindHighestFloorSurrounding(&sector, true);
   }
   else if(func == EV_ActionLowerFloorTurbo)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindHighestFloorSurrounding(&sector, true);
      if(sae.floorHeight != lastFloorHeight)
         sae.floorHeight += 8 * FRACUNIT;
   }
   else if(func == EV_ActionFloorLowerAndChange ||
           func == EV_ActionFloorLowerToLowest)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindLowestFloorSurrounding(&sector, true);
   }
   else if(func == EV_ActionRaiseFloor24 ||
           func == EV_ActionPlatRaise24Change ||
           func == EV_ActionRaiseFloor24Change)
   {
      if(floorBlocked)
         return;
      sae.floorHeight += 24 * FRACUNIT;
   }
   else if(func == EV_ActionPlatRaise32Change)
   {
      if(floorBlocked)
         return;
      sae.floorHeight += 32 * FRACUNIT;
   }
   else if(func == EV_ActionRaiseFloor512)
   {
      if(floorBlocked)
         return;
      sae.floorHeight += 512 * FRACUNIT;
   }
   else if(func == EV_ActionPlatPerpetualRaise)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindLowestFloorSurrounding(&sector, true);
      if (sae.floorHeight > lastFloorHeight)
         sae.floorHeight = lastFloorHeight;

      sae.altFloorHeight = P_FindHighestFloorSurrounding(&sector, true);
      if (sae.altFloorHeight < lastFloorHeight)
         sae.altFloorHeight = lastFloorHeight;

      sae.floorTerminal = true;
   }
   else if(func == EV_ActionPlatToggleUpDown)
   {
      if(floorBlocked)
      {
         const PlatThinker *pt = thinker_cast<PlatThinker *>(sector.floordata);
         if(pt && pt->type == toggleUpDn &&
            pt->status == PlatThinker::in_stasis)
         {
            sae.floorHeight = pt->high;   // consider this
            sae.floorTerminal = true;
         }
         else
            return;
      }
      sae.floorHeight = sector.ceilingheight;
      sae.floorTerminal = true;
   }
   else if(func == EV_ActionPlatRaiseNearestChange ||
           func == EV_ActionFloorRaiseToNearest ||
           func == EV_ActionRaiseFloorTurbo)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindNextHighestFloor(&sector, sae.floorHeight,
                                               true);
   }
   else if(func == EV_ActionRaiseFloor)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindLowestCeilingSurrounding(&sector, true);
      if(sae.floorHeight > sae.ceilingHeight)
         sae.floorHeight = sae.ceilingHeight;
   }
   else if(func == EV_ActionFloorRaiseCrush)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindLowestCeilingSurrounding(&sector, true);
      if (sae.floorHeight > sae.ceilingHeight)
         sae.floorHeight = sae.ceilingHeight;
      sae.floorHeight -= 8 * FRACUNIT;
   }
   else if(func == EV_ActionRaiseCeilingLowerFloor)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight = P_FindHighestCeilingSurrounding(&sector, true);
   }
   else if(func == EV_ActionBOOMRaiseCeilingOrLowerFloor)
   {
      if(!ceilingBlocked)
         sae.ceilingHeight = P_FindHighestCeilingSurrounding(&sector, true);
      else if(!floorBlocked)
         sae.floorHeight = P_FindLowestFloorSurrounding(&sector, true);
      else
         return;
   }
   else if(func == EV_ActionBOOMRaiseCeilingLowerFloor)
   {
      if(ceilingBlocked && floorBlocked)
         return;
      if(!ceilingBlocked)
         sae.ceilingHeight = P_FindHighestCeilingSurrounding(&sector, true);
      if(!floorBlocked)
         sae.floorHeight = P_FindLowestFloorSurrounding(&sector, true);
   }
   else if(func == EV_ActionFloorRaiseToTexture)
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
   }
   else if(func == EV_ActionCeilingCrushAndRaise ||
           func == EV_ActionFastCeilCrushRaise ||
           func == EV_ActionSilentCrushAndRaise)
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
   else if(func == EV_ActionBuildStairsUp8 ||
           func == EV_ActionBuildStairsTurbo16)
   {
      if(floorBlocked)
         return;
      othersAffected = B_fillInStairs(&sector, sae, func, line.special,
                                      indexList);
   }
   else if(func == EV_ActionDoDonut)
   {
      if(floorBlocked)
         return;
      othersAffected = B_fillInDonut(sector, sae, line.special, indexList);
   }
   else if(func == EV_ActionElevatorUp)
   {
      if (floorBlocked || ceilingBlocked)
         return;
      sae.floorHeight = P_FindNextHighestFloor(&sector, lastFloorHeight, true);
      sae.ceilingHeight = sae.floorHeight + lastCeilingHeight - lastFloorHeight;
   }
   else if(func == EV_ActionElevatorDown)
   {
      if (floorBlocked || ceilingBlocked)
         return;
      sae.floorHeight = P_FindNextLowestFloor(&sector, lastFloorHeight, true);
      sae.ceilingHeight = sae.floorHeight + lastCeilingHeight - lastFloorHeight;
   }
   else if(func == EV_ActionElevatorCurrent)
   {
      if (floorBlocked || ceilingBlocked)
         return;
      const SectorHeightStack& front = g_affectedSectors[line.frontsector - sectors];

      sae.floorHeight = front.getFloorHeight();
      sae.ceilingHeight = sae.floorHeight + lastCeilingHeight - lastFloorHeight;
   }
   else if(func == EV_ActionPlatDownWaitUpStay ||
           func == EV_ActionPlatBlazeDWUS)
   {
      if(floorBlocked)
         return;
      fixed_t newheight = P_FindLowestFloorSurrounding(&sector, true);
      if(newheight < sae.floorHeight)
         sae.floorHeight = newheight;
      sae.floorTerminal = true;
      sae.altFloorHeight = lastFloorHeight;
   }
   else
      return;  // unknown or useless/irrelevant actions (e.g. lights) are ignored
   
   if(!othersAffected && sae.floorHeight == lastFloorHeight
      && sae.ceilingHeight == lastCeilingHeight && (!sae.floorTerminal || sae.altFloorHeight == lastFloorHeight))
   {
      return;  // nothing changed; do not register.
   }
   
   // Okay.
   indexList.add(secnum);
   affSector.stack.add(sae);
}

static bool B_fillInStairs(const sector_t* sector, SectorStateEntry& sae,
                           EVActionFunc func, int special,
                           PODCollection<int>& coll)
{
   fixed_t stairIncrement;

   if(func == EV_ActionBuildStairsTurbo16)
      stairIncrement = 16 * FRACUNIT;
   else
      stairIncrement = 8 * FRACUNIT;
   // TODO: other specials

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
         tsae.actionNumber = special;
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
static bool B_fillInDonut(const sector_t& sector, SectorStateEntry& sae,
                          int special, PODCollection<int>& coll)
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
      s2ae.actionNumber = special;
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
   if(gentype != -1)
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

   static const std::unordered_set<EVActionFunc> hybridFuncs = {
      // Sector changers may matter, though
      EV_ActionChangeOnly,
      EV_ActionChangeOnlyNumeric,
      // Lights don't really matter for the bot
//      EV_ActionLightTurnOn,
//      EV_ActionTurnTagLightsOff
   };

   static const std::unordered_set<EVActionFunc> paramFuncs = {
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

//
// True if the linedef special affects both tagged sectors and the surrounding
// ones, using the donut model
//
bool B_LineTriggersDonut(const line_t &line)
{
   if(!line.special)
      return false;

   // generalized specials not available here

   const ev_action_t *action = EV_ActionForSpecial(line.special);
   if(!action)
      return false;

   EVActionFunc func = action->action;
   return func == EV_ActionDoDonut || func == EV_ActionParamDonut;
}

//
// True if line triggers Doom-style stairs
//
bool B_LineTriggersStairs(const line_t &line)
{
   if(!line.special)
      return false;

   // check for generalized
   if(EV_GenTypeForSpecial(line.special) == GenTypeStairs)
      return true;

   const ev_action_t *action = EV_ActionForSpecial(line.special);
   if(!action)
      return false;

   static const std::unordered_set<EVActionFunc> funcs = {
      EV_ActionBuildStairsTurbo16,
      EV_ActionBuildStairsUp8,
      EV_ActionHereticStairsBuildUp16FS,
      EV_ActionHereticStairsBuildUp8FS,
      EV_ActionParamStairsBuildDownDoom,
      EV_ActionParamStairsBuildDownDoomSync,
      EV_ActionParamStairsBuildUpDoom,
      EV_ActionParamStairsBuildUpDoomCrush,
      EV_ActionParamStairsBuildUpDoomSync,
   };

   return !!funcs.count(action->action);
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
   for(int i = 0; i < numsectors; ++i)
   {
      const sector_t *sector = sectors + i;
      EVSectorSpecialFunc func = EV_GetSectorSpecial(*sector);
      if (func == EV_SectorDoorCloseIn30 || func == EV_SectorDoorRaiseIn5Mins)
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
