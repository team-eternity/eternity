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

#ifndef __EternityEngine__b_lineeffect__
#define __EternityEngine__b_lineeffect__

#include "../m_fixed.h"

struct line_t;
struct sector_t;
struct player_t;

namespace LevelStateStack
{
   void     InitLevel();
   bool     Push(const line_t& line, const player_t& player,
                 const sector_t *excludeSector = nullptr);
   void     Pop();
   void     Clear();
   fixed_t  Floor(const sector_t& sector);
   fixed_t  AltFloor(const sector_t& sector);
   fixed_t  Ceiling(const sector_t& sector);
   bool     IsClear();
   
   void SetKeyPlayer(const player_t* player);
   void UseRealHeights(bool value);
}

//
// VanillaLineSpecial
//
// Just name all these into simple enums
//
enum VanillaLineSpecial
{
    VLS_DRRaiseDoor = 1,
    VLS_W1OpenDoor = 2,
    VLS_W1CloseDoor = 3,
    VLS_W1RaiseDoor = 4,
    VLS_W1RaiseFloor = 5,
    VLS_W1FastCeilCrushRaise = 6,
    VLS_S1BuildStairsUp8 = 7,
    VLS_W1BuildStairsUp8 = 8,
    VLS_S1DoDonut = 9,
    VLS_W1PlatDownWaitUpStay = 10,
    VLS_S1ExitLevel = 11,
    VLS_W1LightTurnOn = 12,
    VLS_W1LightTurnOn255 = 13,
    VLS_S1PlatRaise32Change = 14,
    VLS_S1PlatRaise24Change = 15,
    VLS_W1CloseDoor30 = 16,
    VLS_W1StartLightStrobing = 17,
    VLS_S1FloorRaiseToNearest = 18,
    VLS_W1LowerFloor = 19,
    VLS_S1PlatRaiseNearestChange = 20,
    VLS_S1PlatDownWaitUpStay = 21,
    VLS_W1PlatRaiseNearestChange = 22,
    VLS_S1FloorLowerToLowest = 23,
    VLS_G1RaiseFloor = 24,
    VLS_W1CeilingCrushAndRaise = 25,
    VLS_DRRaiseDoorBlue = 26,
    VLS_DRRaiseDoorYellow = 27,
    VLS_DRRaiseDoorRed = 28,
    VLS_S1RaiseDoor = 29,
    VLS_W1FloorRaiseToTexture = 30,
    VLS_D1OpenDoor = 31,
    VLS_D1OpenDoorBlue = 32,
    VLS_D1OpenDoorRed = 33,
    VLS_D1OpenDoorYellow = 34,
    VLS_W1LightsVeryDark = 35,
    VLS_W1LowerFloorTurbo = 36,
    VLS_W1FloorLowerAndChange = 37,
    VLS_W1FloorLowerToLowest = 38,
    VLS_W1Teleport = 39,
    VLS_W1RaiseCeilingLowerFloor = 40,
    VLS_S1CeilingLowerToFloor = 41,
    VLS_SRCloseDoor = 42,
    VLS_SRCeilingLowerToFloor = 43,
    VLS_W1CeilingLowerAndCrush = 44,
    VLS_SRLowerFloor = 45,
    VLS_GROpenDoor = 46,
    VLS_G1PlatRaiseNearestChange = 47,
    VLS_S1CeilingCrushAndRaise = 49,
    VLS_S1CloseDoor = 50,
    VLS_S1SecretExit = 51,
    VLS_WRExitLevel = 52,
    VLS_W1PlatPerpetualRaise = 53,
    VLS_W1PlatStop = 54,
    VLS_S1FloorRaiseCrush = 55,
    VLS_W1FloorRaiseCrush = 56,
    VLS_W1CeilingCrushStop = 57,
    VLS_W1RaiseFloor24 = 58,
    VLS_W1RaiseFloor24Change = 59,
    VLS_SRFloorLowerToLowest = 60,
    VLS_SROpenDoor = 61,
    VLS_SRPlatDownWaitUpStay = 62,
    VLS_SRRaiseDoor = 63,
    VLS_SRRaiseFloor = 64,
    VLS_SRFloorRaiseCrush = 65,
    VLS_SRPlatRaise24Change = 66,
    VLS_SRPlatRaise32Change = 67,
    VLS_SRPlatRaiseNearestChange = 68,
    VLS_SRFloorRaiseToNearest = 69,
    VLS_SRLowerFloorTurbo = 70,
    VLS_S1LowerFloorTurbo = 71,
    VLS_WRCeilingLowerAndCrush = 72,
    VLS_WRCeilingCrushAndRaise = 73,
    VLS_WRCeilingCrushStop = 74,
    VLS_WRCloseDoor = 75,
    VLS_WRCloseDoor30 = 76,
    VLS_WRFastCeilCrushRaise = 77,
    VLS_SRChangeOnlyNumeric = 78,
    VLS_WRLightsVeryDark = 79,
    VLS_WRLightTurnOn = 80,
    VLS_WRLightTurnOn255 = 81,
    VLS_WRFloorLowerToLowest = 82,
    VLS_WRLowerFloor = 83,
    VLS_WRFloorLowerAndChange = 84,
    VLS_WROpenDoor = 86,
    VLS_WRPlatPerpetualRaise = 87,
    VLS_WRPlatDownWaitUpStay = 88,
    VLS_WRPlatStop = 89,
    VLS_WRRaiseDoor = 90,
    VLS_WRRaiseFloor = 91,
    VLS_WRRaiseFloor24 = 92,
    VLS_WRRaiseFloor24Change = 93,
    VLS_WRFloorRaiseCrush = 94,
    VLS_WRPlatRaiseNearestChange = 95,
    VLS_WRFloorRaiseToTexture = 96,
    VLS_WRTeleport = 97,
    VLS_WRLowerFloorTurbo = 98,
    VLS_SRDoorBlazeOpenBlue = 99,
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
// VanillaSectorSpecial
//
enum VanillaSectorSpecial
{
   VSS_LightRandomOff = 1,
   VSS_LightStrobeFast = 2,
   VSS_LightStrobeSlow = 3,
   VSS_LightStrobeHurt = 4,
   VSS_DamageHellSlime = 5,
   VSS_DamageNukage = 7,
   VSS_LightGlow = 8,
   VSS_Secret = 9,
   VSS_DoorCloseIn30 = 10,
   VSS_ExitSuperDamage = 11,
   VSS_LightStrobeSlowSync = 12,
   VSS_LightStrobeFastSync = 13,
   VSS_DoorRaiseIn5Mins = 14,
   VSS_DamageSuperHellSlime = 16,
   VSS_LightFireFlicker = 17,
};

bool B_LineTriggersBackSector(const line_t &line);
inline static bool B_VlsTypeIsDonut(VanillaLineSpecial vls)
{
   return vls == VLS_S1DoDonut || vls == VLS_SRDoDonut || vls == VLS_W1DoDonut || vls == VLS_WRDoDonut;
}
bool B_VlsTypeIsStair(VanillaLineSpecial vls);

bool B_SectorTypeIsHarmless(int16_t special);

void B_LogActiveSectors();

#endif /* defined(__EternityEngine__b_lineeffect__) */
