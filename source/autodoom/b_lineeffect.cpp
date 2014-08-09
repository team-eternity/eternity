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

#include "../z_zone.h"

#include "../doomstat.h"
#include "../m_collection.h"
#include "b_lineeffect.h"
#include "../p_spec.h"
#include "../r_data.h"
#include "../r_defs.h"
#include "../r_state.h"

// We need stacks of modified sector floorheights and ceilingheights.
// As we push linedef values to those stacks, we update the floor heights

//
// VanillaLineSpecial
//
// Just name all these into simple enums
//
enum VanillaLineSpecial
{
   VLS_DRRaiseDoor =   1,
   VLS_W1OpenDoor =   2,
   VLS_W1CloseDoor =   3,
   VLS_W1RaiseDoor =   4,
   VLS_W1RaiseFloor =   5,
   VLS_W1FastCeilCrushRaise =   6,
   VLS_S1BuildStairsUp8 =   7,
   VLS_W1BuildStairsUp8 =   8,
   VLS_S1DoDonut =   9,
   VLS_W1PlatDownWaitUpStay =  10,
   VLS_S1ExitLevel =  11,
   VLS_W1LightTurnOn =  12,
   VLS_W1LightTurnOn255 =  13,
   VLS_S1PlatRaise32Change =  14,
   VLS_S1PlatRaise24Change =  15,
   VLS_W1CloseDoor30 =  16,
   VLS_W1StartLightStrobing =  17,
   VLS_S1FloorRaiseToNearest =  18,
   VLS_W1LowerFloor =  19,
   VLS_S1PlatRaiseNearestChange =  20,
   VLS_S1PlatDownWaitUpStay =  21,
   VLS_W1PlatRaiseNearestChange =  22,
   VLS_S1FloorLowerToLowest =  23,
   VLS_G1RaiseFloor =  24,
   VLS_W1CeilingCrushAndRaise =  25,
   VLS_DRRaiseDoorBlue =  26,
   VLS_DRRaiseDoorYellow =  27,
   VLS_DRRaiseDoorRed =  28,
   VLS_S1RaiseDoor =  29,
   VLS_W1FloorRaiseToTexture =  30,
   VLS_D1OpenDoor =  31,
   VLS_D1OpenDoorBlue =  32,
   VLS_D1OpenDoorRed =  33,
   VLS_D1OpenDoorYellow =  34,
   VLS_W1LightsVeryDark =  35,
   VLS_W1LowerFloorTurbo =  36,
   VLS_W1FloorLowerAndChange =  37,
   VLS_W1FloorLowerToLowest =  38,
   VLS_W1Teleport =  39,
   VLS_W1RaiseCeilingLowerFloor =  40,
   VLS_S1CeilingLowerToFloor =  41,
   VLS_SRCloseDoor =  42,
   VLS_SRCeilingLowerToFloor =  43,
   VLS_W1CeilingLowerAndCrush =  44,
   VLS_SRLowerFloor =  45,
   VLS_GROpenDoor =  46,
   VLS_G1PlatRaiseNearestChange =  47,
   VLS_S1CeilingCrushAndRaise =  49,
   VLS_S1CloseDoor =  50,
   VLS_S1SecretExit =  51,
   VLS_WRExitLevel =  52,
   VLS_W1PlatPerpetualRaise =  53,
   VLS_W1PlatStop =  54,
   VLS_S1FloorRaiseCrush =  55,
   VLS_W1FloorRaiseCrush =  56,
   VLS_W1CeilingCrushStop =  57,
   VLS_W1RaiseFloor24 =  58,
   VLS_W1RaiseFloor24Change =  59,
   VLS_SRFloorLowerToLowest =  60,
   VLS_SROpenDoor =  61,
   VLS_SRPlatDownWaitUpStay =  62,
   VLS_SRRaiseDoor =  63,
   VLS_SRRaiseFloor =  64,
   VLS_SRFloorRaiseCrush =  65,
   VLS_SRPlatRaise24Change =  66,
   VLS_SRPlatRaise32Change =  67,
   VLS_SRPlatRaiseNearestChange =  68,
   VLS_SRFloorRaiseToNearest =  69,
   VLS_SRLowerFloorTurbo =  70,
   VLS_S1LowerFloorTurbo =  71,
   VLS_WRCeilingLowerAndCrush =  72,
   VLS_WRCeilingCrushAndRaise =  73,
   VLS_WRCeilingCrushStop =  74,
   VLS_WRCloseDoor =  75,
   VLS_WRCloseDoor30 =  76,
   VLS_WRFastCeilCrushRaise =  77,
   VLS_SRChangeOnlyNumeric =  78,
   VLS_WRLightsVeryDark =  79,
   VLS_WRLightTurnOn =  80,
   VLS_WRLightTurnOn255 =  81,
   VLS_WRFloorLowerToLowest =  82,
   VLS_WRLowerFloor =  83,
   VLS_WRFloorLowerAndChange =  84,
   VLS_WROpenDoor =  86,
   VLS_WRPlatPerpetualRaise =  87,
   VLS_WRPlatDownWaitUpStay =  88,
   VLS_WRPlatStop =  89,
   VLS_WRRaiseDoor =  90,
   VLS_WRRaiseFloor =  91,
   VLS_WRRaiseFloor24 =  92,
   VLS_WRRaiseFloor24Change =  93,
   VLS_WRFloorRaiseCrush =  94,
   VLS_WRPlatRaiseNearestChange =  95,
   VLS_WRFloorRaiseToTexture =  96,
   VLS_WRTeleport =  97,
   VLS_WRLowerFloorTurbo =  98,
   VLS_SRDoorBlazeOpenBlue =  99,
   VLS_W1BuildStairsTurbo16 = 100,
   VLS_S1RaiseFloor = 101,
   VLS_S1LowerFloor = 102,
   VLS_S1OpenDoor = 103,
   VLS_W1TurnTagLightsOff = 104,
   VLS_WRDoorBlazeRaise = 105,
   VLS_WRDoorBlazeOpen = 106,
   VLS_WRDoorBlazeClose = 107,
   VLS_W1DoorBlazeRaise = 108,
   VLS_W1DoorBlazeOpen = 109,
   VLS_W1DoorBlazeClose = 110,
   VLS_S1DoorBlazeRaise = 111,
   VLS_S1DoorBlazeOpen = 112,
   VLS_S1DoorBlazeClose = 113,
   VLS_SRDoorBlazeRaise = 114,
   VLS_SRDoorBlazeOpen = 115,
   VLS_SRDoorBlazeClose = 116,
   VLS_DRDoorBlazeRaise = 117,
   VLS_D1DoorBlazeOpen = 118,
   VLS_W1FloorRaiseToNearest = 119,
   VLS_WRPlatBlazeDWUS = 120,
   VLS_W1PlatBlazeDWUS = 121,
   VLS_S1PlatBlazeDWUS = 122,
   VLS_SRPlatBlazeDWUS = 123,
   VLS_WRSecretExit = 124,
   VLS_W1TeleportMonsters = 125,
   VLS_WRTeleportMonsters = 126,
   VLS_S1BuildStairsTurbo16 = 127,
   VLS_WRFloorRaiseToNearest = 128,
   VLS_WRRaiseFloorTurbo = 129,
   VLS_W1RaiseFloorTurbo = 130,
   VLS_S1RaiseFloorTurbo = 131,
   VLS_SRRaiseFloorTurbo = 132,
   VLS_S1DoorBlazeOpenBlue = 133,
   VLS_SRDoorBlazeOpenRed = 134,
   VLS_S1DoorBlazeOpenRed = 135,
   VLS_SRDoorBlazeOpenYellow = 136,
   VLS_S1DoorBlazeOpenYellow = 137,
   VLS_SRLightTurnOn255 = 138,
   VLS_SRLightsVeryDark = 139,
   VLS_S1RaiseFloor512 = 140,
   VLS_W1SilentCrushAndRaise = 141,
   VLS_W1RaiseFloor512 = 142,
   VLS_W1PlatRaise24Change = 143,
   VLS_W1PlatRaise32Change = 144,
   VLS_W1CeilingLowerToFloor = 145,
   VLS_W1DoDonut = 146,
   VLS_WRRaiseFloor512 = 147,
   VLS_WRPlatRaise24Change = 148,
   VLS_WRPlatRaise32Change = 149,
   VLS_WRSilentCrushAndRaise = 150,
   VLS_WRRaiseCeilingLowerFloor = 151,
   VLS_WRCeilingLowerToFloor = 152,
   VLS_W1ChangeOnly = 153,
   VLS_WRChangeOnly = 154,
   VLS_WRDoDonut = 155,
   VLS_WRStartLightStrobing = 156,
   VLS_WRTurnTagLightsOff = 157,
   VLS_S1FloorRaiseToTexture = 158,
   VLS_S1FloorLowerAndChange = 159,
   VLS_S1RaiseFloor24Change = 160,
   VLS_S1RaiseFloor24 = 161,
   VLS_S1PlatPerpetualRaise = 162,
   VLS_S1PlatStop = 163,
   VLS_S1FastCeilCrushRaise = 164,
   VLS_S1SilentCrushAndRaise = 165,
   VLS_S1BOOMRaiseCeilingOrLowerFloor = 166,
   VLS_S1CeilingLowerAndCrush = 167,
   VLS_S1CeilingCrushStop = 168,
   VLS_S1LightTurnOn = 169,
   VLS_S1LightsVeryDark = 170,
   VLS_S1LightTurnOn255 = 171,
   VLS_S1StartLightStrobing = 172,
   VLS_S1TurnTagLightsOff = 173,
   VLS_S1Teleport = 174,
   VLS_S1CloseDoor30 = 175,
   VLS_SRFloorRaiseToTexture = 176,
   VLS_SRFloorLowerAndChange = 177,
   VLS_SRRaiseFloor512 = 178,
   VLS_SRRaiseFloor24Change = 179,
   VLS_SRRaiseFloor24 = 180,
   VLS_SRPlatPerpetualRaise = 181,
   VLS_SRPlatStop = 182,
   VLS_SRFastCeilCrushRaise = 183,
   VLS_SRCeilingCrushAndRaise = 184,
   VLS_SRSilentCrushAndRaise = 185,
   VLS_SRBOOMRaiseCeilingOrLowerFloor = 186,
   VLS_SRCeilingLowerAndCrush = 187,
   VLS_SRCeilingCrushStop = 188,
   VLS_S1ChangeOnly = 189,
   VLS_SRChangeOnly = 190,
   VLS_SRDoDonut = 191,
   VLS_SRLightTurnOn = 192,
   VLS_SRStartLightStrobing = 193,
   VLS_SRTurnTagLightsOff = 194,
   VLS_SRTeleport = 195,
   VLS_SRCloseDoor30 = 196,
   VLS_G1ExitLevel = 197,
   VLS_G1SecretExit = 198,
   VLS_W1CeilingLowerToLowest = 199,
   VLS_W1CeilingLowerToMaxFloor = 200,
   VLS_WRCeilingLowerToLowest = 201,
   VLS_WRCeilingLowerToMaxFloor = 202,
   VLS_S1CeilingLowerToLowest = 203,
   VLS_S1CeilingLowerToMaxFloor = 204,
   VLS_SRCeilingLowerToLowest = 205,
   VLS_SRCeilingLowerToMaxFloor = 206,
   VLS_W1SilentTeleport = 207,
   VLS_WRSilentTeleport = 208,
   VLS_S1SilentTeleport = 209,
   VLS_SRSilentTeleport = 210,
   VLS_SRPlatToggleUpDown = 211,
   VLS_WRPlatToggleUpDown = 212,
   VLS_W1FloorLowerToNearest = 219,
   VLS_WRFloorLowerToNearest = 220,
   VLS_S1FloorLowerToNearest = 221,
   VLS_SRFloorLowerToNearest = 222,
   VLS_W1ElevatorUp = 227,
   VLS_WRElevatorUp = 228,
   VLS_S1ElevatorUp = 229,
   VLS_SRElevatorUp = 230,
   VLS_W1ElevatorDown = 231,
   VLS_WRElevatorDown = 232,
   VLS_S1ElevatorDown = 233,
   VLS_SRElevatorDown = 234,
   VLS_W1ElevatorCurrent = 235,
   VLS_WRElevatorCurrent = 236,
   VLS_S1ElevatorCurrent = 237,
   VLS_SRElevatorCurrent = 238,
   VLS_W1ChangeOnlyNumeric = 239,
   VLS_WRChangeOnlyNumeric = 240,
   VLS_S1ChangeOnlyNumeric = 241,
   VLS_W1SilentLineTeleport = 243,
   VLS_WRSilentLineTeleport = 244,
   VLS_WRBuildStairsUp8 = 256,
   VLS_WRBuildStairsTurbo16 = 257,
   VLS_SRBuildStairsUp8 = 258,
   VLS_SRBuildStairsTurbo16 = 259,
   VLS_W1SilentLineTeleportReverse = 262,
   VLS_WRSilentLineTeleportReverse = 263,
   VLS_W1SilentLineTRMonsters = 264,
   VLS_WRSilentLineTRMonsters = 265,
   VLS_W1SilentLineTeleMonsters = 266,
   VLS_WRSilentLineTeleMonsters = 267,
   VLS_W1SilentTeleportMonsters = 268,
   VLS_WRSilentTeleportMonsters = 269,
   VLS_WRStartLineScript1S = 273,
   VLS_W1StartLineScript = 274,
   VLS_W1StartLineScript1S = 275,
   VLS_SRStartLineScript = 276,
   VLS_S1StartLineScript = 277,
   VLS_GRStartLineScript = 278,
   VLS_G1StartLineScript = 279,
   VLS_WRStartLineScript = 280,
};


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
class SectorHeightStack
{
public:
   const sector_t* sector; // the affected sector
   PODCollection<SectorStateEntry> stack;   // stack of actions
   fixed_t getFloorHeight() const
   {
      return stack.getLength() ? stack.back().floorHeight : sector->floorheight;
   }
   fixed_t getAltFloorHeight() const
   {
       return stack.getLength() ? stack.back().altFloorHeight : sector->floorheight;
   }
   fixed_t getCeilingHeight() const
   {
      return stack.getLength() ? stack.back().ceilingHeight
      : sector->ceilingheight;
   }
   bool isFloorTerminal() const
   {
      return stack.getLength() ? stack.back().floorTerminal : false;
   }
   bool isCeilingTerminal() const
   {
      return stack.getLength() ? stack.back().ceilingTerminal : false;
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
static void B_pushSectorHeights(int, int, const line_t& line, PODCollection<int>&);
bool LevelStateStack::Push(const line_t& line, int action, int tag)
{
   int secnum = -1;
   
   // Prepare the new list of indices to the global stack
   PODCollection<int>& coll = g_indexListStack.addNew();
   
   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      // For each successful state push, add an index to the collection
      B_pushSectorHeights(action, secnum, line, coll);
   }
   
   if(coll.getLength() == 0)
   {
      // No valid state change, so cancel all this and return false
      g_indexListStack.pop();
      return false;
   }
   
   // okay
   return true;
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

static void B_pushSectorHeights(int action, int secnum, const line_t& line,
                                PODCollection<int>& indexList)
{
   SectorHeightStack& affSector = g_affectedSectors[secnum];
   bool floorBlocked = affSector.isFloorTerminal();
   bool ceilingBlocked = affSector.isCeilingTerminal();
   if(floorBlocked && ceilingBlocked)  // all blocked: impossible
      return;
   
   const sector_t& sector = sectors[secnum];
   SectorStateEntry sae;
   sae.actionNumber = action;
   fixed_t lastFloorHeight = affSector.getFloorHeight();
   fixed_t lastCeilingHeight = affSector.getCeilingHeight();
   sae.floorHeight = lastFloorHeight;
   sae.ceilingHeight = lastCeilingHeight;
   sae.floorTerminal = floorBlocked;
   sae.ceilingTerminal = ceilingBlocked;
   
   VanillaLineSpecial vls = static_cast<VanillaLineSpecial>(action);
   bool othersAffected = false;
   int amount;
   switch (vls)
   {
      case VLS_D1DoorBlazeOpen:
      case VLS_D1OpenDoor:
      case VLS_D1OpenDoorBlue:
      case VLS_D1OpenDoorRed:
      case VLS_D1OpenDoorYellow:
      case VLS_GROpenDoor:
      case VLS_S1DoorBlazeOpen:
      case VLS_S1DoorBlazeOpenBlue:
      case VLS_S1DoorBlazeOpenRed:
      case VLS_S1DoorBlazeOpenYellow:
      case VLS_S1OpenDoor:
      case VLS_SRDoorBlazeOpen:
      case VLS_SRDoorBlazeOpenBlue:
      case VLS_SRDoorBlazeOpenRed:
      case VLS_SRDoorBlazeOpenYellow:
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
      case VLS_DRDoorBlazeRaise:
      case VLS_DRRaiseDoor:
      case VLS_DRRaiseDoorBlue:
      case VLS_DRRaiseDoorRed:
      case VLS_DRRaiseDoorYellow:
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
         if(ceilingBlocked)
            return;
         sae.ceilingHeight = sae.floorHeight + 8 * FRACUNIT;
         sae.ceilingTerminal = true;
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
// B_getSectorHeightsFromAction
//
// Gets target floor and ceiling heights from hypothetical actions
//
#if 0
static bool B_getSectorHeightsFromAction(int action, const sector_t& sector)
{
   PODCollection<SectorStateEntry>& stack = g_affectedSectors[&sector - sectors]
   .stack;
   const SectorStateEntry* lastSae = nullptr;
   if(stack.getLength() >= 1)
   {
      lastSae = &stack[stack.getLength() - 1];
      if(lastSae->terminal)
         return;
   }
   
   SectorStateEntry sae;
   sae.actionNumber = action;
   sae.floorHeight = lastSae ? lastSae->floorHeight : sector.floorheight;
   sae.ceilingHeight = lastSae ? lastSae->ceilingHeight : sector.ceilingheight;
   sae.terminal = false;
   
   VanillaLineSpecial vls = static_cast<VanillaLineSpecial>(action);
   
   switch (vls)
   {
      case VLS_D1DoorBlazeOpen:
      case VLS_D1OpenDoor:
      case VLS_D1OpenDoorBlue:
      case VLS_D1OpenDoorRed:
      case VLS_D1OpenDoorYellow:
      case VLS_GROpenDoor:
         sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector) - 4 * FRACUNIT;
         break;
         
      case VLS_DRDoorBlazeRaise:
      case VLS_DRRaiseDoor:
      case VLS_DRRaiseDoorBlue:
      case VLS_DRRaiseDoorRed:
      case VLS_DRRaiseDoorYellow:
         sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector) - 4 * FRACUNIT;
         sae.terminal = true;
         break;
         
      case VLS_G1ExitLevel:
      case VLS_G1SecretExit:
         return;
      case VLS_G1StartLineScript:   // NOT YET ADDRESSED
      case VLS_GRStartLineScript:
         return;

      case VLS_G1PlatRaiseNearestChange:
         sae.floorHeight = P_FindNextHighestFloor(&sector, sector.floorheight);
         break;
         
      case VLS_G1RaiseFloor:
         sae.floorHeight = P_FindLowestCeilingSurrounding(&sector);
         if(sae.floorHeight > sae.ceilingHeight)
            sae.floorHeight = sae.ceilingHeight;
         break;
         
      case VLS_S1BOOMRaiseCeilingOrLowerFloor:
         if(!P_SectorActive(ceiling_special, &sector))
            sae.ceilingHeight = P_FindHighestCeilingSurrounding(&sector);
         else
            sae.floorHeight = P_FindLowestFloorSurrounding(&sector);
         break;
         
      case 2:// DOOM Line Type 2 - W1 Open Door
         sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector) - 4 * FRACUNIT;
         break;
         
      case 3:// DOOM Line Type 3 - W1 Close Door
         sae.ceilingHeight = sae.floorHeight;
         break;
         
      case 4:// DOOM Line Type 4 - W1 Raise Door
         sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector) - 4 * FRACUNIT;
         sae.terminal = true;
         break;
         
      case 5:// DOOM Line Type 5 - W1 Raise Floor
         sae.floorHeight = P_FindLowestCeilingSurrounding(&sector);
         break;
         
      case 6:// DOOM Line Type 6 - W1 Ceiling Fast Crush and Raise
      case VLS_S1CeilingCrushAndRaise:
         sae.terminal = true; // FIXME: maybe add permanent terminal?
         break;
         
      case VLS_S1BuildStairsUp8:
      case 8:// DOOM Line Type 8 - W1 Build Stairs Up 8
      case VLS_S1BuildStairsTurbo16:
      {
         fixed_t stairIncrement;
         if(vls == VLS_S1BuildStairsTurbo16)
            stairIncrement = 16 * FRACUNIT;
         else
            stairIncrement = 8 * FRACUNIT;
         fixed_t height = sae.floorHeight + stairIncrement;
         sae.floorHeight = height;
         bool ok;
         do
         {
            // CODE LARGELY TAKEN FROM EV_BuildStairs
            int i;
            ok = false;
            for(i = 0; i < sector.linecount; ++i)
            {
               const sector_t* tsec = sector.lines[i]->frontsector;
               if(!(sector.lines[i]->flags & ML_TWOSIDED))
                  continue;
               if(tsec != &sector)
                  continue;
               tsec = sector.lines[i]->backsector;
               if(!tsec)
                  continue;
               if(tsec->floorpic != sector.floorpic)
                  continue;
               if(comp[comp_stairs] || demo_version == 203)
                  height += stairIncrement;
               PODCollection<SectorStateEntry>& tstack = g_affectedSectors[tsec - sectors].stack;
               if(tstack[tstack.getLength() - 1].terminal)
                  continue;
               if(!comp[comp_stairs] && demo_version != 203)
                  height += stairIncrement;
               
               // Now create the new entry
               SectorStateEntry tsae;
               tsae.actionNumber = action;
               tsae.floorHeight = height;
               tsae.ceilingHeight = tsec->ceilingheight;
               tsae.terminal = false;
               tstack.add(tsae);
               ok = true;
            }
         }while(ok);
         break;
      }
      case VLS_S1CeilingCrushStop:
         return;  // what do?
      case VLS_S1CeilingLowerToFloor:
         sae.ceilingHeight = sae.floorHeight;
         break;
      case VLS_S1CeilingLowerAndCrush:
         sae.ceilingHeight = sae.floorHeight + 8 * FRACUNIT;
         break;
      case VLS_S1CeilingLowerToLowest:
         sae.ceilingHeight = P_FindLowestCeilingSurrounding(&sector);
         break;
      case 9:// DOOM Line Type 9 - S1 Donut
      {
         // EV_DoDonut
         const sector_t* sector2 = getNextSector(sector.lines[0], &sector);
         const sector_t* sector3;
         fixed_t s3_floorheight;
         int16_t s3_floorpic;
         if(!sector2)
            return;  // don't do anything!
         PODCollection<SectorStateEntry>& tstack = g_affectedSectors[sector2 - sectors].stack;
         if(!comp[comp_floors] && tstack[tstack.getLength() - 1].terminal)
            return;
         for(int i = 0; i < sector2->linecount; ++i)
         {
            if(comp[comp_model])
            {
               if(sector2->lines[i]->backsector == &sector)
                  continue;
            }
            else if(!sector2->lines[i]->backsector || sector2->lines[i]->backsector == &sector)
            {
               continue;
            }
            
            sector3 = sector2->lines[i]->backsector;
            if(!sector3)
            {
               if(!DonutOverflow(&s3_floorheight, &s3_floorpic))
                  return;  // DO NOTHING
            }
            else
            {
               s3_floorheight = sector3->floorheight;
               s3_floorpic = sector3->floorpic;
            }
            
            // DO IT NOW
            sae.floorHeight = s3_floorheight;
            
            if(!tstack[tstack.getLength() - 1].terminal)
            {
               SectorStateEntry tsae;
               tsae.actionNumber = sae.actionNumber;
               tsae.floorHeight = s3_floorheight;
               tsae.ceilingHeight = sector3->ceilingheight;
               tsae.terminal = false;
               tstack.add(tsae);
            }
         }
         break;
      }
      case 10:// DOOM Line Type 10 - W1 Plat Down-Wait-Up-Stay
      {
         // TODO: momentary floor lower: mind both possibilities!
      }
      default:
         break;
   }
   stack.add(sae);
}
#endif
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