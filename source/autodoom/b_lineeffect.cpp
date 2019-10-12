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
         B_pushSectorHeights((int)(line.backsector - sectors), line, coll, player);
   }
   else
   {
      while((secnum = P_FindSectorFromLineArg0(&line, secnum)) >= 0)
      {
         // For each successful state push, add an index to the collection

         if(::sectors + secnum == excludeSector && g_affectedSectors[secnum].stack.getLength())
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
static bool B_fillInGenStairs(const sector_t *sec, SectorStateEntry &sae, int special,
                              fixed_t lastFloorHeight, fixed_t stairsize, int dir, bool ignoretex,
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

// ce poate face actiunea
// poate schimba pozitii de sectoare. Lista de sectoare.

struct LineEffect
{

};

//
// Does the highly specific texture effect
//
static void B_applyShortestTexture(SectorStateEntry &sae, const sector_t &sector)
{
   fixed_t minsize = getMinTextureSize(sector);

   if(comp[comp_model])
      sae.floorHeight += minsize;
   else
   {
      sae.floorHeight = (sae.floorHeight >> FRACBITS) + (minsize >> FRACBITS);
      if(sae.floorHeight > 32000)
         sae.floorHeight = 32000;
      sae.floorHeight <<= FRACBITS;
   }
}

//
// Handles a crusher. Only push state when resuming a crusher, because it can be used to regain
// access. Otherwise let other parts of the AI treat the crusher as a hazard.
//
static bool B_handleCrusher(const sector_t &sector, SectorStateEntry &sae)
{
   const CeilingThinker* ct = thinker_cast<const CeilingThinker*>(sector.ceilingdata);
   if (ct && ct->inStasis)
   {
      sae.ceilingHeight = ct->topheight;
      sae.ceilingTerminal = true;
      return true;
   }
   //              if (ceilingBlocked)
   return false;
   //              sae.ceilingHeight = sae.floorHeight + 8 * FRACUNIT;
   //              sae.ceilingTerminal = true;
}

//
// Common routine for generalized and parameterized
//
static void B_handleParamShortestTexture(SectorStateEntry & sae, fixed_t lastFloorHeight, int dir,
                                         const sector_t &sector)
{
   sae.floorHeight = (lastFloorHeight >> FRACBITS) +
         dir * (P_FindShortestTextureAround(eindex(&sector - sectors)) >> FRACBITS);

   if(sae.floorHeight > 32000)  //jff 3/13/98 prevent overflow
      sae.floorHeight = 32000;    // wraparound in floor height
   if(sae.floorHeight < -32000)
      sae.floorHeight = -32000;
   sae.floorHeight<<=FRACBITS;
}
static void B_handleParamShortestUpper(SectorStateEntry & sae, fixed_t lastCeilingHeight, int dir,
                                       const sector_t &sector)
{
   sae.ceilingHeight = (lastCeilingHeight >> FRACBITS) +
         dir * (P_FindShortestUpperAround(eindex(&sector - sectors)) >> FRACBITS);
   if(sae.ceilingHeight > 32000)  // jff 3/13/98 prevent overflow
      sae.ceilingHeight = 32000;  // wraparound in ceiling height
   if(sae.ceilingHeight < -32000)
      sae.ceilingHeight = -32000;
   sae.ceilingHeight <<= FRACBITS;
}

//
// generalized special routine. returns false if blocked
//
static bool B_pushGeneralized(int special, SectorStateEntry &sae, const sector_t &sector,
                              const line_t &line, bool &othersAffected, bool floorBlocked,
                              bool ceilingBlocked, fixed_t lastFloorHeight,
                              fixed_t lastCeilingHeight, const player_t &player,
                              PODCollection<int> &coll)
{
   int gentype = EV_GenTypeForSpecial(special);
   int value;
   switch(gentype)
   {
      case GenTypeFloor:
      {
         if(floorBlocked)
            return false;
         value = special - GenFloorBase;
         int target = (value & FloorTarget   ) >> FloorTargetShift;
         int dir = ((value & FloorDirection) >> FloorDirectionShift) ? plat_up : plat_down;
         switch (target)
         {
            case FtoHnF:
               sae.floorHeight = P_FindHighestFloorSurrounding(&sector, true);
               break;
            case FtoLnF:
               sae.floorHeight = P_FindLowestFloorSurrounding(&sector, true);
               break;
            case FtoNnF:
               sae.floorHeight = dir ? P_FindNextHighestFloor(&sector, lastFloorHeight, true) :
               P_FindNextLowestFloor(&sector, lastFloorHeight, true);
               break;
            case FtoLnC:
               sae.floorHeight = P_FindLowestCeilingSurrounding(&sector, true);
               break;
            case FtoC:
               sae.floorHeight = lastCeilingHeight;
               break;
            case FbyST:
               B_handleParamShortestTexture(sae, lastFloorHeight, dir, sector);
               break;
            case Fby24:
               sae.floorHeight = lastFloorHeight + dir * 24 * FRACUNIT;
               break;
            case Fby32:
               sae.floorHeight = lastFloorHeight + dir * 32 * FRACUNIT;
               break;
         }
         break;
      }
      case GenTypeCeiling:
      {
         if(ceilingBlocked)
            return false;
         value = special - GenCeilingBase;
         int target = (value & CeilingTarget   ) >> CeilingTargetShift;
         int dir = (value & CeilingDirection) >> CeilingDirectionShift ? plat_up : plat_down;
         switch (target)
         {
            case CtoHnC:
               sae.ceilingHeight = P_FindHighestCeilingSurrounding(&sector, true);
               break;
            case CtoLnC:
               sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true);
               break;
            case CtoNnC:
               sae.ceilingHeight = dir ? P_FindNextHighestCeiling(&sector, lastCeilingHeight, true)
               : P_FindNextLowestCeiling(&sector, lastCeilingHeight, true);
               break;
            case CtoHnF:
               sae.ceilingHeight = P_FindHighestFloorSurrounding(&sector, true);
               break;
            case CtoF:
               sae.ceilingHeight = lastFloorHeight;
               break;
            case CbyST:
               B_handleParamShortestUpper(sae, lastCeilingHeight, dir, sector);
               break;
            case Cby24:
               sae.ceilingHeight = lastCeilingHeight + dir * 24 * FRACUNIT;
               break;
            case Cby32:
               sae.ceilingHeight = lastCeilingHeight + dir * 32 * FRACUNIT;
               break;
         }
         break;
      }
      case GenTypeDoor:
      {
         if(ceilingBlocked || B_LineTriggersBackSector(line) && doorBusy(sector))
            return false;
         value = special - GenDoorBase;
         int kind = (value & DoorKind) >> DoorKindShift;
         switch (kind)
         {
            case OdCDoor:
               sae.ceilingTerminal = true;
            case ODoor:
               sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4 * FRACUNIT;
               break;
            case CdODoor:
               sae.ceilingTerminal = true;
            case CDoor:
               sae.ceilingHeight = lastFloorHeight;
               break;
         }
         break;
      }
      case GenTypeLocked:
      {
         if(ceilingBlocked || B_LineTriggersBackSector(line) && doorBusy(sector))
            return false;
         value = special - GenLockedBase;
         int lockID = EV_LockDefIDForSpecial(line.special);
         if(lockID && !E_PlayerCanUnlock(&player, lockID, false, true))
            return false;
         sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4 * FRACUNIT;
         int kind = (value & LockedKind) >> LockedKindShift;
         if(kind == OdCDoor)
            sae.ceilingTerminal = true;
         break;
      }
      case GenTypeLift:
      {
         if(floorBlocked)
            return false;
         value = line.special - GenLiftBase;
         int target = (value & LiftTarget ) >> LiftTargetShift;
         sae.floorTerminal = true;
         sae.altFloorHeight = lastFloorHeight;
         switch (target)
         {
            case F2LnF:
               sae.floorHeight = P_FindLowestFloorSurrounding(&sector, true);
               if(sae.floorHeight > lastFloorHeight)
                  sae.floorHeight = lastFloorHeight;
               break;
            case F2NnF:
               sae.floorHeight = P_FindNextLowestFloor(&sector, lastFloorHeight, true);
               break;
            case F2LnC:
               sae.floorHeight = P_FindLowestCeilingSurrounding(&sector, true);
               if(sae.floorHeight > lastFloorHeight)
                  sae.floorHeight = lastFloorHeight;
               break;
            case LnF2HnF:
               sae.floorHeight = P_FindLowestFloorSurrounding(&sector, true);
               if(sae.floorHeight > lastFloorHeight)
                  sae.floorHeight = lastFloorHeight;
               sae.altFloorHeight = P_FindHighestFloorSurrounding(&sector, true);
               if(sae.altFloorHeight < lastFloorHeight)
                  sae.altFloorHeight = lastFloorHeight;
               break;
            default:
               break;
         }
         break;
      }
      case GenTypeStairs:
      {
         if(floorBlocked)
            return false;
         value = line.special - GenStairsBase;
         int dir = (value & StairDirection) >> StairDirectionShift;
         int step = (value & StairStep) >> StairStepShift;
         bool ignoretex = !!((value & StairIgnore) >> StairIgnoreShift);
         fixed_t ssize;
         switch (step)
         {
            default:
            case StepSize4:
               ssize = 4 * FRACUNIT;
               break;
            case StepSize8:
               ssize = 8 * FRACUNIT;
               break;
            case StepSize16:
               ssize = 16 * FRACUNIT;
               break;
            case StepSize24:
               ssize = 24 * FRACUNIT;
               break;
         }
         othersAffected = B_fillInGenStairs(&sector, sae, line.special, lastFloorHeight, ssize,
                                            dir ? plat_up : plat_down, ignoretex, coll);
         break;
      }
      case GenTypeCrusher:
      {
         if(!B_handleCrusher(sector, sae))
            return false;
         break;
      }
      default:
         return false;  // shouldn't really get here
   }
   return true;
}

//
// Updare heights from state
//
static void B_pushSectorHeights(int secnum, const line_t& line,
                                PODCollection<int>& indexList, const player_t& player)
{
   SectorHeightStack& affSector = g_affectedSectors[secnum];
   const bool floorBlocked = affSector.isFloorTerminal();
   const bool ceilingBlocked = affSector.isCeilingTerminal();
   if(floorBlocked && ceilingBlocked)  // all blocked: impossible
      return;
   
   const sector_t& sector = sectors[secnum];
   SectorStateEntry sae;
   sae.actionNumber = line.special;
   const fixed_t lastFloorHeight = affSector.getFloorHeight();
   const fixed_t lastCeilingHeight = affSector.getCeilingHeight();
   sae.floorHeight = lastFloorHeight;
   sae.ceilingHeight = lastCeilingHeight;
   sae.floorTerminal = floorBlocked;
   sae.ceilingTerminal = ceilingBlocked;
   sae.altFloorHeight = 0;  // this needs to be initialized in order to avoid Visual Studio debug errors.
   
   bool othersAffected = false;

   // FIXME: figure out how to support polyobj specials.

   const ev_action_t *action = EV_ActionForSpecial(line.special);
   if(!action)
      return;
   EVActionFunc func = action->action;
   int lockID;
   bool hexen = P_LevelIsVanillaHexen();

   if(func == EV_ActionBoomGen)
   {
      if(!B_pushGeneralized(line.special, sae, sector, line, othersAffected, floorBlocked,
                            ceilingBlocked, lastFloorHeight, lastCeilingHeight, player, indexList))
      {
         return;
      }
   }
   else if(func == EV_ActionDoorBlazeOpen || func == EV_ActionOpenDoor ||
           func == EV_ActionParamDoorOpen)
   {
      if(ceilingBlocked || B_LineTriggersBackSector(line) && doorBusy(sector))
         return;
      sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4 * FRACUNIT;
   }
   else if(func == EV_ActionVerticalDoor)
   {
      if(ceilingBlocked || B_LineTriggersBackSector(line) && doorBusy(sector))
         return;
      lockID = EV_LockDefIDForSpecial(line.special);
      if(lockID && !E_PlayerCanUnlock(&player, lockID, false, true))
         return;
      sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4 * FRACUNIT;
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
      sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4 * FRACUNIT;
   }
   else if(func == EV_ActionDoorBlazeRaise || func == EV_ActionRaiseDoor ||
           func == EV_ActionHereticDoorRaise3x || func == EV_ActionParamDoorRaise ||
           func == EV_ActionParamDoorWaitRaise)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4 * FRACUNIT;
      sae.ceilingTerminal = true;
   }
   else if(func == EV_ActionCloseDoor || func == EV_ActionCeilingLowerToFloor ||
           func == EV_ActionDoorBlazeClose || func == EV_ActionParamDoorClose ||
           func == EV_ActionParamDoorWaitClose)
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
   else if(func == EV_ActionCloseDoor30 || func == EV_ActionParamDoorCloseWaitOpen)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight = sae.floorHeight;
      sae.ceilingTerminal = true;
   }
   else if(func == EV_ActionCeilingLowerToLowest || func == EV_ActionParamCeilingRaiseToLowest)
   {
      if (ceilingBlocked)
         return;
      sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true);
   }
   else if(func == EV_ActionCeilingLowerToMaxFloor || func == EV_ActionParamCeilingRaiseToHighestFloor)
   {
      if (ceilingBlocked)
         return;
      sae.ceilingHeight = P_FindHighestFloorSurrounding(&sector, true);
   }
   else if(func == EV_ActionParamCeilingLowerToHighestFloor)
   {
      if (ceilingBlocked)
         return;
      sae.ceilingHeight = P_FindHighestFloorSurrounding(&sector, true);
      fixed_t gap = line.args[4] * FRACUNIT;
      if(gap)
      {
         if(sae.ceilingHeight < lastFloorHeight)
            sae.ceilingHeight = lastFloorHeight;
         sae.ceilingHeight += gap;
      }
   }
   else if(func == EV_ActionParamCeilingToFloorInstant)
   {
      if (ceilingBlocked)
         return;
      fixed_t gap = line.args[3] * FRACUNIT;
      sae.ceilingHeight = lastFloorHeight + gap;
   }
   else if(func == EV_ActionParamCeilingLowerToFloor)
   {
      if (ceilingBlocked)
         return;
      fixed_t gap = line.args[4] * FRACUNIT;
      sae.ceilingHeight = lastFloorHeight + gap;
   }
   else if(func == EV_ActionParamCeilingRaiseByTexture)
   {
      if(ceilingBlocked)
         return;
      B_handleParamShortestUpper(sae, lastCeilingHeight, 1, sector);
   }
   else if(func == EV_ActionParamCeilingLowerByTexture)
   {
      if(ceilingBlocked)
         return;
      B_handleParamShortestUpper(sae, lastCeilingHeight, -1, sector);
   }
   else if(func == EV_ActionParamCeilingRaiseByValue || func == EV_ActionParamCeilingRaiseInstant)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight += line.args[2] * FRACUNIT;
   }
   else if(func == EV_ActionParamCeilingRaiseByValueTimes8)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight += line.args[2] * FRACUNIT * 8;
   }
   else if(func == EV_ActionParamCeilingLowerByValue || func == EV_ActionParamCeilingLowerInstant)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight -= line.args[2] * FRACUNIT;
   }
   else if(func == EV_ActionParamCeilingLowerByValueTimes8)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight -= line.args[2] * FRACUNIT * 8;
   }
   else if(func == EV_ActionParamCeilingMoveToValue)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight = line.args[2] * FRACUNIT;
      if(line.args[3])
         sae.ceilingHeight = -sae.ceilingHeight;
   }
   else if(func == EV_ActionParamCeilingMoveToValueTimes8)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight = line.args[2] * FRACUNIT * 8;
      if(line.args[3])
         sae.ceilingHeight = -sae.ceilingHeight;
   }
   else if(func == EV_ActionFloorLowerToNearest || (func == EV_ActionParamFloorLowerToNearest &&
                                                    !hexen))
   {
      if (floorBlocked)
         return;
      sae.floorHeight = P_FindNextLowestFloor(&sector, lastFloorHeight, true);
   }
   else if(func == EV_ActionRaiseDoor)
   {
      if(ceilingBlocked || B_LineTriggersBackSector(line) && doorBusy(sector))
         return;
      sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector, true) - 4 * FRACUNIT;
      sae.ceilingTerminal = true;
   }
   else if(func == EV_ActionLowerFloor || (func == EV_ActionParamFloorRaiseToHighest && !hexen) ||
           func == EV_ActionParamEEFloorLowerToHighest ||
           (func == EV_ActionParamFloorLowerToNearest && hexen))
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindHighestFloorSurrounding(&sector, true);
   }
   else if(func == EV_ActionParamFloorLowerToHighest)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindHighestFloorSurrounding(&sector, true);
      int adjust = line.args[2];
      int forceadjust = line.args[3];
      fixed_t amt = (adjust - 128) * FRACUNIT;
      if(forceadjust == 1 || sae.floorHeight != lastFloorHeight)
         sae.floorHeight += amt;
   }
   else if(func == EV_ActionLowerFloorTurbo)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindHighestFloorSurrounding(&sector, true);
      if(sae.floorHeight != lastFloorHeight)
         sae.floorHeight += 8 * FRACUNIT;
   }
   else if(func == EV_ActionLowerFloorTurboA)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindHighestFloorSurrounding(&sector, true) + 8 * FRACUNIT;
   }
   else if(func == EV_ActionFloorLowerAndChange || func == EV_ActionFloorLowerToLowest ||
           func == EV_ActionParamFloorRaiseToLowest || func == EV_ActionParamFloorLowerToLowest)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindLowestFloorSurrounding(&sector, true);
   }
   else if(func == EV_ActionRaiseFloor24 || func == EV_ActionPlatRaise24Change ||
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
   else if(func == EV_ActionPlatRaiseNearestChange || func == EV_ActionFloorRaiseToNearest ||
           func == EV_ActionRaiseFloorTurbo || func == EV_ActionParamFloorRaiseToNearest)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindNextHighestFloor(&sector, sae.floorHeight, true);
   }
   else if(func == EV_ActionRaiseFloor || (func == EV_ActionParamFloorRaiseToHighest && hexen))
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindLowestCeilingSurrounding(&sector, true);
      if(sae.floorHeight > sae.ceilingHeight)
         sae.floorHeight = sae.ceilingHeight;
   }
   else if(func == EV_ActionParamFloorRaiseToLowestCeiling)
   {
      if(floorBlocked)
         return;
      fixed_t gap = -line.args[4] * FRACUNIT;
      sae.floorHeight = P_FindLowestCeilingSurrounding(&sector, true) + gap;
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
   else if(func == EV_ActionParamFloorLowerToLowestCeiling)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = P_FindLowestCeilingSurrounding(&sector, true);
   }
   else if(func == EV_ActionParamFloorRaiseToCeiling)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = lastCeilingHeight - line.args[4] * FRACUNIT;
   }
   else if(func == EV_ActionParamFloorToCeilingInstant)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = lastCeilingHeight - line.args[3] * FRACUNIT;
   }
   else if(func == EV_ActionRaiseCeilingLowerFloor || func == EV_ActionParamCeilingRaiseToHighest ||
           func == EV_ActionParamCeilingToHighestInstant)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight = P_FindHighestCeilingSurrounding(&sector, true);
   }
   else if(func == EV_ActionParamCeilingRaiseToNearest)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight = P_FindNextHighestCeiling(&sector, lastCeilingHeight, true);
   }
   else if(func == EV_ActionParamCeilingLowerToNearest)
   {
      if(ceilingBlocked)
         return;
      sae.ceilingHeight = P_FindNextLowestCeiling(&sector, lastCeilingHeight, true);
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
      B_applyShortestTexture(sae, sector);
   }
   else if(func == EV_ActionParamFloorRaiseByTexture)
   {
      if(floorBlocked)
         return;
      B_handleParamShortestTexture(sae, lastFloorHeight, 1, sector);
   }
   else if(func == EV_ActionParamFloorLowerByTexture)
   {
      if(floorBlocked)
         return;
      B_handleParamShortestTexture(sae, lastFloorHeight, -1, sector);
   }
   else if(func == EV_ActionParamFloorRaiseByValue || func == EV_ActionParamFloorRaiseInstant)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = lastFloorHeight + line.args[2] * FRACUNIT;
   }
   else if(func == EV_ActionParamFloorRaiseByValueTimes8)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = lastFloorHeight + line.args[2] * FRACUNIT * 8;
   }
   else if(func == EV_ActionParamFloorLowerByValue || func == EV_ActionParamFloorLowerInstant)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = lastFloorHeight - line.args[2] * FRACUNIT;
   }
   else if(func == EV_ActionParamFloorLowerByValueTimes8)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = lastFloorHeight - line.args[2] * FRACUNIT * 8;
   }
   else if(func == EV_ActionParamFloorMoveToValue)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = line.args[2] * FRACUNIT;
      if(line.args[3])
         sae.floorHeight = -sae.floorHeight;
   }
   else if(func == EV_ActionParamFloorMoveToValueTimes8)
   {
      if(floorBlocked)
         return;
      sae.floorHeight = line.args[2] * FRACUNIT * 8;
      if(line.args[3])
         sae.floorHeight = -sae.floorHeight;
   }
   else if(func == EV_ActionCeilingCrushAndRaise || func == EV_ActionFastCeilCrushRaise ||
           func == EV_ActionSilentCrushAndRaise)
   {
      if(!B_handleCrusher(sector, sae))
         return;
   }
   else if(func == EV_ActionBuildStairsUp8 || func == EV_ActionBuildStairsTurbo16 ||
           func == EV_ActionHereticStairsBuildUp8FS || func == EV_ActionHereticStairsBuildUp16FS)
   {
      if(floorBlocked)
         return;
      othersAffected = B_fillInStairs(&sector, sae, func, line.special, indexList);
   }
   else if(func == EV_ActionParamStairsBuildUpDoom || func == EV_ActionParamStairsBuildUpDoomSync)
   {
      if(floorBlocked)
         return;
      othersAffected = B_fillInGenStairs(&sector, sae, line.special, lastFloorHeight,
                                         line.args[2] * FRACUNIT, plat_up, false, indexList);
   }
   else if(func == EV_ActionParamStairsBuildDownDoom ||
           func == EV_ActionParamStairsBuildDownDoomSync)
   {
      if(floorBlocked)
         return;
      othersAffected = B_fillInGenStairs(&sector, sae, line.special, lastFloorHeight,
                                         line.args[2] * FRACUNIT, plat_down, false, indexList);
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
   else if(func == EV_ActionPlatDownWaitUpStay || func == EV_ActionPlatBlazeDWUS)
   {
      if(floorBlocked)
         return;
      fixed_t newheight = P_FindLowestFloorSurrounding(&sector, true);
      if(newheight < sae.floorHeight)
         sae.floorHeight = newheight;
      sae.floorTerminal = true;
      sae.altFloorHeight = lastFloorHeight;
   }
   else if(func == EV_ActionPillarBuild || func == EV_ActionPillarBuildAndCrush)
   {
      if(floorBlocked || ceilingBlocked || lastCeilingHeight <= lastFloorHeight)
         return;
      fixed_t height = line.args[2] * FRACUNIT;
      fixed_t destheight;
      if(!height)
         destheight = lastFloorHeight + (lastCeilingHeight - lastFloorHeight) / 2;
      else
         destheight = lastFloorHeight + height;

      sae.floorHeight = sae.ceilingHeight = destheight;
   }
   else if(func == EV_ActionPillarOpen)
   {
      fixed_t fdist = line.args[2] * FRACUNIT;
      fixed_t cdist = line.args[3] * FRACUNIT;
      if(floorBlocked || ceilingBlocked || lastFloorHeight != lastCeilingHeight)
         return;
      if(!fdist)
         sae.floorHeight = P_FindLowestFloorSurrounding(&sector, true);
      else
         sae.floorHeight = lastFloorHeight - fdist;
      if(!cdist)
         sae.ceilingHeight = P_FindHighestCeilingSurrounding(&sector, true);
      else
         sae.ceilingHeight = lastCeilingHeight + cdist;
   }
   // TODO: waggle
   else
      return;  // unknown or useless/irrelevant actions (e.g. lights) are ignored
   
   if(!othersAffected && sae.floorHeight == lastFloorHeight &&
      sae.ceilingHeight == lastCeilingHeight && (!sae.floorTerminal ||
                                                 sae.altFloorHeight == lastFloorHeight))
   {
      return;  // nothing changed; do not register.
   }
   
   // Okay.
   indexList.add(secnum);
   affSector.stack.add(sae);
}

//
// for classic doom specials
//
static bool B_fillInStairs(const sector_t* sector, SectorStateEntry& sae,
                           EVActionFunc func, int special,
                           PODCollection<int>& coll)
{
   fixed_t stairIncrement;

   if(func == EV_ActionBuildStairsTurbo16 || func == EV_ActionHereticStairsBuildUp16FS)
      stairIncrement = 16 * FRACUNIT;
   else
      stairIncrement = 8 * FRACUNIT;
   // TODO: other specials

   fixed_t height = sae.floorHeight + stairIncrement;
   sae.floorHeight = height;
   bool ok;

   bool othersAffected = false;
   do
   {
      // CODE LARGELY TAKEN FROM EV_BuildStairs
      ok = false;
      for(int i = 0; i < sector->linecount; ++i)
      {
         const sector_t* tsec;
         tsec = sector->lines[i]->frontsector;
         if(tsec != sector || !(sector->lines[i]->flags & ML_TWOSIDED))
            continue;
         tsec = sector->lines[i]->backsector;
         if(!tsec || tsec->floorpic != sector->floorpic)
            continue;
         if(comp[comp_stairs] || demo_version == 203)
            height += stairIncrement;
         
         SectorHeightStack* otherAffSector = &g_affectedSectors[tsec - sectors];
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

//
// the more complex boom stairs
//
static bool B_fillInGenStairs(const sector_t *sec, SectorStateEntry &sae, int special,
                              fixed_t lastFloorHeight, fixed_t stairsize, int dir, bool ignoretex,
                              PODCollection<int>& coll)
{
   fixed_t height = lastFloorHeight + dir * stairsize;
   int texture = sec->floorpic;
   bool ok;
   bool othersAffected = false;
   do
   {
      ok = false;
      for(int i = 0; i < sec->linecount; ++i)
      {
         if(!sec->lines[i]->backsector)
            continue;
         const sector_t *tsec = sec->lines[i]->frontsector;
         if(tsec != sec)
            continue;
         tsec = sec->lines[i]->backsector;
         if(!ignoretex && tsec->floorpic != texture)
            continue;

         if(demo_version < 202)
            height += dir * stairsize;

         SectorHeightStack &otherAffSector = g_affectedSectors[tsec - sectors];
         if(otherAffSector.isFloorTerminal())
            continue;
         if(demo_version >= 202)
            height += dir * stairsize;

         // FIXME: is there anything about stairlock I may be concerned about?

         sec = tsec;
         // Now create the new entry
         if(!othersAffected && height != otherAffSector.getFloorHeight())
            othersAffected = true;

         SectorStateEntry tsae;
         tsae.actionNumber = special;
         tsae.floorHeight = height;
         tsae.ceilingHeight = otherAffSector.getCeilingHeight();
         tsae.floorTerminal = false;
         tsae.ceilingTerminal = otherAffSector.isCeilingTerminal();
         otherAffSector.stack.add(tsae);
         coll.add(eindex(tsec - sectors));
         ok = true;
      }
   } while(ok);
   return othersAffected;
}

// TODO: add generalized equivalent

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

   for(int i = 0; i < sector.linecount; ++i)
   {
      int secnum = eindex(&sector - sectors);
      if(twoSided(secnum, i))
      {
         const side_t* side;
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
