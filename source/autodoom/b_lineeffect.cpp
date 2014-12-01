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

#include "../doomstat.h"
#include "../e_inventory.h"
#include "../ev_specials.h"
#include "../m_collection.h"
#include "b_botmap.h"
#include "b_lineeffect.h"
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

bool LevelStateStack::Push(const line_t& line, const player_t& player)
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
   else while((secnum = P_FindSectorFromTag(line.tag, secnum)) >= 0)
   {
      // For each successful state push, add an index to the collection
      B_pushSectorHeights(secnum, line, coll, player);
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