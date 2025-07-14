// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2016 David Hill, James Haley, et al.
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
//----------------------------------------------------------------------------
//
// Original 100% GPL ACS Interpreter
//
// By James Haley
//
// Improved by David Hill
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "acs_intr.h"
#include "c_runcmd.h"
#include "doomstat.h"
#include "e_hash.h"
#include "ev_specials.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "m_buffer.h"
#include "m_collection.h"
#include "m_qstr.h"
#include "m_swap.h"
#include "m_utils.h"
#include "p_info.h"
#include "p_maputl.h"
#include "polyobj.h"
#include "p_saveg.h"
#include "p_spec.h"
#include "r_state.h"
#include "v_misc.h"
#include "w_wad.h"

// need wad iterators
#include "w_iterator.h"

#include "ACSVM/Action.hpp"
#include "ACSVM/BinaryIO.hpp"
#include "ACSVM/Code.hpp"
#include "ACSVM/CodeData.hpp"
#include "ACSVM/Error.hpp"
#include "ACSVM/Module.hpp"
#include "ACSVM/Scope.hpp"
#include "ACSVM/Script.hpp"
#include "ACSVM/Serial.hpp"


//
// Global Variables
//

ACSEnvironment ACSenv;

// ACS_thingtypes:
// This array translates from ACS spawn numbers to internal thingtype indices.
// ACS spawn numbers are specified via EDF and are gamemode-dependent. EDF takes
// responsibility for populating this list.

int ACS_thingtypes[ACS_NUM_THINGTYPES];

int ACSThread::saveLoadVersion;

//
// Global Functions
//


//
// ACSEnvironment constructor
//
ACSEnvironment::ACSEnvironment() :
   dir   {nullptr},
   global{getGlobalScope(0)},
   hub   {nullptr},
   map   {nullptr}
{
   global->active = true;

   // Add code translations.

   // 0-56: ACSVM internal codes.
   addCodeDataACS0( 57, {"",        2, addCallFunc(ACS_CF_Random)});
   addCodeDataACS0( 58, {"WW",      0, addCallFunc(ACS_CF_Random)});
   addCodeDataACS0( 59, {"",        2, addCallFunc(ACS_CF_ThingCount)});
   addCodeDataACS0( 60, {"WW",      0, addCallFunc(ACS_CF_ThingCount)});
   addCodeDataACS0( 61, {"",        1, addCallFunc(ACS_CF_TagWait)});
   addCodeDataACS0( 62, {"W",       0, addCallFunc(ACS_CF_TagWait)});
   addCodeDataACS0( 63, {"",        1, addCallFunc(ACS_CF_PolyWait)});
   addCodeDataACS0( 64, {"W",       0, addCallFunc(ACS_CF_PolyWait)});
   addCodeDataACS0( 65, {"",        2, addCallFunc(ACS_CF_ChangeFloor)});
   addCodeDataACS0( 66, {"WWS",     0, addCallFunc(ACS_CF_ChangeFloor)});
   addCodeDataACS0( 67, {"",        2, addCallFunc(ACS_CF_ChangeCeiling)});
   addCodeDataACS0( 68, {"WWS",     0, addCallFunc(ACS_CF_ChangeCeiling)});
   // 69-79: ACSVM internal codes.
   addCodeDataACS0( 80, {"",        0, addCallFunc(ACS_CF_LineSide)});
   // 81-82: ACSVM internal codes.
   addCodeDataACS0( 83, {"",        0, addCallFunc(ACS_CF_ClearLineSpecial)});
   // 84-85: ACSVM internal codes.
   addCodeDataACS0( 86, {"",        0, addCallFunc(ACS_CF_EndPrint)});
   // 87-89: ACSVM internal codes.
   addCodeDataACS0( 90, {"",        0, addCallFunc(ACS_CF_PlayerCount)});
   addCodeDataACS0( 91, {"",        0, addCallFunc(ACS_CF_GameType)});
   addCodeDataACS0( 92, {"",        0, addCallFunc(ACS_CF_GameSkill)});
   addCodeDataACS0( 93, {"",        0, addCallFunc(ACS_CF_Timer)});
   addCodeDataACS0( 94, {"",        2, addCallFunc(ACS_CF_SectorSound)});
   addCodeDataACS0( 95, {"",        2, addCallFunc(ACS_CF_AmbientSound)});
   addCodeDataACS0( 96, {"",        1, addCallFunc(ACS_CF_SoundSequence)});
   addCodeDataACS0( 97, {"",        4, addCallFunc(ACS_CF_SetLineTexture)});
   addCodeDataACS0( 98, {"",        2, addCallFunc(ACS_CF_SetLineBlocking)});
   addCodeDataACS0( 99, {"",        7, addCallFunc(ACS_CF_SetLineSpecial)});
   addCodeDataACS0(100, {"",        3, addCallFunc(ACS_CF_ThingSound)});
   addCodeDataACS0(101, {"",        0, addCallFunc(ACS_CF_EndPrintBold)});
   addCodeDataACS0(102, {"",        2, addCallFunc(ACS_CF_ActivatorSound)});
   addCodeDataACS0(103, {"",        2, addCallFunc(ACS_CF_LocalAmbientSound)});
   addCodeDataACS0(104, {"",        2, addCallFunc(ACS_CF_SetLineMonsterBlocking)});
   // 105-118: Unused codes.
 //addCodeDATAACS0(119, {"",        0, addCallFunc(ACS_CF_ActivatorTream)});
   addCodeDataACS0(120, {"",        0, addCallFunc(ACS_CF_PlayerHealth)});
   addCodeDataACS0(121, {"",        0, addCallFunc(ACS_CF_PlayerArmorPoints)});
   addCodeDataACS0(122, {"",        0, addCallFunc(ACS_CF_PlayerFrags)});
   // 123-123: Unused codes.
 //addCodeDataACS0(124, {"",        0, addCallFunc(ACS_CF_BlueTeamCount)});
 //addCodeDataACS0(125, {"",        0, addCallFunc(ACS_CF_RedTeamCount)});
 //addCodeDataACS0(126, {"",        0, addCallFunc(ACS_CF_BlueTeamScore)});
 //addCodeDataACS0(127, {"",        0, addCallFunc(ACS_CF_RedTeamScore)});
 //addCodeDataACS0(128, {"",        0, addCallFunc(ACS_CF_OneFlagCTF)});
 //addCodeDataACS0(129, {"",        0, addCallFunc(ACS_CF_GetInvasionWave)});
 //addCodeDataACS0(130, {"",        0, addCallFunc(ACS_CF_GetInvasionState)});
   addCodeDataACS0(131, {"",        0, addCallFunc(ACS_CF_PrintName)});
   addCodeDataACS0(132, {"",        2, addCallFunc(ACS_CF_SetMusic)});
 //addCodeDataACS0(133, {"WSWW",    0, addCallFunc(ACS_CF_ConsoleCommand)});
 //addCodeDataACS0(134, {"",        3, addCallFunc(ACS_CF_ConsoleCommand)});
   addCodeDataACS0(135, {"",        0, addCallFunc(ACS_CF_SinglePlayer)});
   // 136-137: ACSVM internal codes.
   addCodeDataACS0(138, {"",        1, addCallFunc(ACS_CF_SetGravity)});
   addCodeDataACS0(139, {"W",       0, addCallFunc(ACS_CF_SetGravity)});
   addCodeDataACS0(140, {"",        1, addCallFunc(ACS_CF_SetAirControl)});
   addCodeDataACS0(141, {"W",       0, addCallFunc(ACS_CF_SetAirControl)});
 //addCodeDataACS0(142, {"",        0, addCallFunc(ACS_CF_ClrInventory)});
 //addCodeDataACS0(143, {"",        2, addCallFunc(ACS_CF_AddInventory)});
 //addCodeDataACS0(144, {"WSW",     0, addCallFunc(ACS_CF_AddInventory)});
   addCodeDataACS0(145, {"",        2, addCallFunc(ACS_CF_TakeInventory)});
   addCodeDataACS0(146, {"WSW",     0, addCallFunc(ACS_CF_TakeInventory)});
   addCodeDataACS0(147, {"",        1, addCallFunc(ACS_CF_CheckInventory)});
   addCodeDataACS0(148, {"WS",      0, addCallFunc(ACS_CF_CheckInventory)});
   addCodeDataACS0(149, {"",        6, addCallFunc(ACS_CF_Spawn)});
   addCodeDataACS0(150, {"WSWWWWW", 0, addCallFunc(ACS_CF_Spawn)});
   addCodeDataACS0(151, {"",        4, addCallFunc(ACS_CF_SpawnSpot)});
   addCodeDataACS0(152, {"WSWWW",   0, addCallFunc(ACS_CF_SpawnSpot)});
   addCodeDataACS0(153, {"",        3, addCallFunc(ACS_CF_SetMusic)});
   addCodeDataACS0(154, {"WSWW",    0, addCallFunc(ACS_CF_SetMusic)});
   addCodeDataACS0(155, {"",        3, addCallFunc(ACS_CF_LocalSetMusic)});
   addCodeDataACS0(156, {"WSWW",    0, addCallFunc(ACS_CF_LocalSetMusic)});
   // 157-157: ACSVM internal codes.
 //addCodeDataACS0(158, {"",        1, addCallFunc(ACS_CF_PrintLocale)});
 //addCodeDataACS0(159, {"",        0, addCallFunc(ACS_CF_PrintHudMore)});
 //addCodeDataACS0(160, {"",        0, addCallFunc(ACS_CF_PrintHudOpt)});
 //addCodeDataACS0(161, {"",        0, addCallFunc(ACS_CF_PrintHudEnd)});
 //addCodeDataACS0(162, {"",        0, addCallFunc(ACS_CF_PrintHudEndB)});
   // 163-164: Unused codes.
 //addCodeDataACS0(165, {"",        1, addCallFunc(ACS_CF_SetFont)});
 //addCodeDataACS0(166, {"WS",      0, addCallFunc(ACS_CF_SetFont)});
   // 167-173: ACSVM internal codes.
   addCodeDataACS0(174, {"BB",      0, addCallFunc(ACS_CF_Random)});
   // 175-179: ACSVM internal codes.
   addCodeDataACS0(180, {"",        7, addCallFunc(ACS_CF_SetThingSpecial)});
   // 181-189: ACSVM internal codes.
 //addCodeDataACS0(190, {"",        5, addCallFunc(ACS_CF_FadeTo)});
 //addCodeDataACS0(191, {"",        9, addCallFunc(ACS_CF_FadeRange)});
 //addCodeDataACS0(192, {"",        0, addCallFunc(ACS_CF_FadeCancel)});
 //addCodeDataACS0(193, {"",        1, addCallFunc(ACS_CF_PlayMovie)});
 //addCodeDataACS0(194, {"",        8, addCallFunc(ACS_CF_SetFloorTrig)});
 //addCodeDataACS0(195, {"",        8, addCallFunc(ACS_CF_SetCeilTrig)});
   addCodeDataACS0(196, {"",        1, addCallFunc(ACS_CF_GetActorX)});
   addCodeDataACS0(197, {"",        1, addCallFunc(ACS_CF_GetActorY)});
   addCodeDataACS0(198, {"",        1, addCallFunc(ACS_CF_GetActorZ)});
 //addCodeDataACS0(199, {"",        1, addCallFunc(ACS_CF_transStart)});
 //addCodeDataACS0(200, {"",        4, addCallFunc(ACS_CF_TransPalette)});
 //addCodeDataACS0(201, {"",        8, addCallFunc(ACS_CF_TransRGB)});
 //addCodeDataACS0(202, {"",        0, addCallFunc(ACS_CF_TransEnd)});
   // 203-217: ACSVM internal codes.
   // 218-219: Unused codes.
   addCodeDataACS0(220, {"",        1, addCallFunc(ACS_CF_Sin)});
   addCodeDataACS0(221, {"",        1, addCallFunc(ACS_CF_Cos)});
   addCodeDataACS0(222, {"",        2, addCallFunc(ACS_CF_VectorAngle)});
   addCodeDataACS0(223, {"",        1, addCallFunc(ACS_CF_CheckWeapon)});
   addCodeDataACS0(224, {"",        1, addCallFunc(ACS_CF_SetWeapon)});
   // 225-243: ACSVM internal codes.
 //addCodeDataACS0(244, {"",        2, addCallFunc(ACS_CF_SetMarineWeapon)});
   addCodeDataACS0(245, {"",        3, addCallFunc(ACS_CF_SetActorProperty)});
   addCodeDataACS0(246, {"",        2, addCallFunc(ACS_CF_GetActorProperty)});
   addCodeDataACS0(247, {"",        0, addCallFunc(ACS_CF_PlayerNumber)});
   addCodeDataACS0(248, {"",        0, addCallFunc(ACS_CF_ActivatorTID)});
 //addCodeDataACS0(249, {"",        2, addCallFunc(ACS_CF_SetMarineSprite)});
   addCodeDataACS0(250, {"",        0, addCallFunc(ACS_CF_GetScreenW)});
   addCodeDataACS0(251, {"",        0, addCallFunc(ACS_CF_GetScreenH)});
   addCodeDataACS0(252, {"",        7, addCallFunc(ACS_CF_Thing_Projectile2)});
   // 253-253: ACSVM internal codes.
 //addCodeDataACS0(254, {"",        3, addCallFunc(ACS_CF_SetHudSize)});
   addCodeDataACS0(255, {"",        1, addCallFunc(ACS_CF_GetCVar)});
   // 256-257: ACSVM internal codes.
   addCodeDataACS0(258, {"",        0, addCallFunc(ACS_CF_GetLineRowOffset)});
   addCodeDataACS0(259, {"",        1, addCallFunc(ACS_CF_GetActorFloorZ)});
   addCodeDataACS0(260, {"",        1, addCallFunc(ACS_CF_GetActorAngle)});
   addCodeDataACS0(261, {"",        3, addCallFunc(ACS_CF_GetSectorFloorZ)});
   addCodeDataACS0(262, {"",        3, addCallFunc(ACS_CF_GetSectorCeilingZ)});
   // 263-263: ACSVM internal codes.
   addCodeDataACS0(264, {"",        0, addCallFunc(ACS_CF_GetSigilPieces)});
   addCodeDataACS0(265, {"",        1, addCallFunc(ACS_CF_GetLevelInfo)});
 //addCodeDataACS0(266, {"",        2, addCallFunc(ACS_CF_ChangeSky)});
 //addCodeDataACS0(267, {"",        1, addCallFunc(ACS_CF_PlayerInGame)});
 //addCodeDataACS0(268, {"",        1, addCallFunc(ACS_CF_PlayerIsBot)});
 //addCodeDataACS0(269, {"",        0, addCallFunc(ACS_CF_SetCameraTex)});
   addCodeDataACS0(270, {"",        0, addCallFunc(ACS_CF_EndLog)});
 //addCodeDataACS0(271, {"",        1, addCallFunc(ACS_CF_GetAmmoCap)});
 //addCodeDataACS0(272, {"",        2, addCallFunc(ACS_CF_SetAmmoCap)});
   // 273-275: ACSVM internal codes.
   addCodeDataACS0(276, {"",        2, addCallFunc(ACS_CF_SetActorAngle)});
   // 277-279: Unused codes.
   addCodeDataACS0(280, {"",        7, addCallFunc(ACS_CF_SpawnProjectile)});
   addCodeDataACS0(281, {"",        1, addCallFunc(ACS_CF_GetSectorLightLevel)});
   addCodeDataACS0(282, {"",        1, addCallFunc(ACS_CF_GetActorCeilingZ)});
   addCodeDataACS0(283, {"",        5, addCallFunc(ACS_CF_SetActorPosition)});
 //addCodeDataACS0(284, {"",        1, addCallFunc(ACS_CF_ClrThingInv)});
 //addCodeDataACS0(285, {"",        3, addCallFunc(ACS_CF_AddThingInv)});
 //addCodeDataACS0(286, {"",        3, addCallFunc(ACS_CF_SubThingInv)});
 //addCodeDataACS0(287, {"",        2, addCallFunc(ACS_CF_GetThingInv)});
   addCodeDataACS0(288, {"",        2, addCallFunc(ACS_CF_ThingCountName)});
   addCodeDataACS0(289, {"",        3, addCallFunc(ACS_CF_SpawnSpotFacing)});
 //addCodeDataACS0(290, {"",        1, addCallFunc(ACS_CF_PlayerClass)});
   // 291-325: ACSVM internal codes.
 //addCodeDataACS0(326, {"",        2, addCallFunc(ACS_CF_GetPlayerProp)});
 //addCodeDataACS0(327, {"",        4, addCallFunc(ACS_CF_ChangeLevel)});
   addCodeDataACS0(328, {"",        5, addCallFunc(ACS_CF_SectorDamage)});
   addCodeDataACS0(329, {"",        3, addCallFunc(ACS_CF_ReplaceTextures)});
   // 330-330: ACSVM internal codes.
   addCodeDataACS0(331, {"",        1, addCallFunc(ACS_CF_GetActorPitch)});
   addCodeDataACS0(332, {"",        2, addCallFunc(ACS_CF_SetActorPitch)});
 //addCodeDataACS0(333, {"",        1, addCallFunc(ACS_CF_PrintBind)});
   addCodeDataACS0(334, {"",        3, addCallFunc(ACS_CF_SetActorState)});
   addCodeDataACS0(335, {"",        3, addCallFunc(ACS_CF_Thing_Damage2)});
 //addCodeDataACS0(336, {"",        1, addCallFunc(ACS_CF_UseInventory)});
 //addCodeDataACS0(337, {"",        2, addCallFunc(ACS_CF_UseThingInv)});
   addCodeDataACS0(338, {"",        2, addCallFunc(ACS_CF_CheckActorCeilingTexture)});
   addCodeDataACS0(339, {"",        2, addCallFunc(ACS_CF_CheckActorFloorTexture)});
   addCodeDataACS0(340, {"",        1, addCallFunc(ACS_CF_GetActorLightLevel)});
 //addCodeDataACS0(341, {"",        1, addCallFunc(ACS_CF_SetMugState)});
   addCodeDataACS0(342, {"",        3, addCallFunc(ACS_CF_ThingCountSector)});
   addCodeDataACS0(343, {"",        3, addCallFunc(ACS_CF_ThingCountNameSector)});
 //addCodeDataACS0(344, {"",        1, addCallFunc(ACS_CF_GetPlayerCam)});
 //addCodeDataACS0(345, {"",        7, addCallFunc(ACS_CF_MorphThing)});
 //addCodeDataACS0(346, {"",        2, addCallFunc(ACS_CF_UnmorphThing)});
   addCodeDataACS0(347, {"",        2, addCallFunc(ACS_CF_GetPlayerInput)});
   addCodeDataACS0(348, {"",        1, addCallFunc(ACS_CF_ClassifyActor)});
   // 349-361: ACSVM internal codes.
 //addCodeDataACS0(362, {"",        8, addCallFunc(ACS_CF_TransDesat)});
   // 363-380: ACSVM internal codes.

   // Add func translations.

   // 0-0: ACSVM interal funcs.
 //addFuncDataACS0(  1, addCallFunc(ACS_CF_GetLineUDMFInt));
 //addFuncDataACS0(  2, addCallFunc(ACS_CF_GetLineUDMFFixed));
 //addFuncDataACS0(  3, addCallFunc(ACS_CF_GetThingUDMFInt));
 //addFuncDataACS0(  4, addCallFunc(ACS_CF_GetThingUDMFFixed));
 //addFuncDataACS0(  5, addCallFunc(ACS_CF_GetSectorUDMFInt));
 //addFuncDataACS0(  6, addCallFunc(ACS_CF_GetSectorUDMFFixed));
 //addFuncDataACS0(  7, addCallFunc(ACS_CF_GetSideUDMFInt));
 //addFuncDataACS0(  8, addCallFunc(ACS_CF_GetSideUDMFFixed));
   addFuncDataACS0(  9, addCallFunc(ACS_CF_GetActorVelX));
   addFuncDataACS0( 10, addCallFunc(ACS_CF_GetActorVelY));
   addFuncDataACS0( 11, addCallFunc(ACS_CF_GetActorVelZ));
   addFuncDataACS0( 12, addCallFunc(ACS_CF_SetActivator));
   addFuncDataACS0( 13, addCallFunc(ACS_CF_SetActivatorToTarget));
 //addFuncDataACS0( 14, addCallFunc(ACS_CF_GetThingViewHeight));
   // 15-15: ACSVM internal funcs.
 //addFuncDataACS0( 16, addCallFunc(ACS_CF_GetPlayerAir));
 //addFuncDataACS0( 17, addCallFunc(ACS_CF_SetPlayerAir));
   addFuncDataACS0( 18, addCallFunc(ACS_CF_SetSkyScrollSpeed));
 //addFuncDataACS0( 19, addCallFunc(ACS_CF_GetPlayerArmor));
   addFuncDataACS0( 20, addCallFunc(ACS_CF_SpawnSpotForced));
   addFuncDataACS0( 21, addCallFunc(ACS_CF_SpawnSpotFacingForced));
   addFuncDataACS0( 22, addCallFunc(ACS_CF_CheckActorProperty));
   addFuncDataACS0( 23, addCallFunc(ACS_CF_SetActorVelocity));
 //addFuncDataACS0( 24, addCallFunc(ACS_CF_SetThingUserVar));
 //addFuncDataACS0( 25, addCallFunc(ACS_CF_GetThingUserVar));
   addFuncDataACS0( 26, addCallFunc(ACS_CF_Radius_Quake2));
   addFuncDataACS0( 27, addCallFunc(ACS_CF_CheckActorClass));
 //addFuncDataACS0( 28, addCallFunc(ACS_CF_SetThingUserArr));
 //addFuncDataACS0( 29, addCallFunc(ACS_CF_GetThingUserArr));
   addFuncDataACS0( 30, addCallFunc(ACS_CF_SoundSequenceOnActor));
 //addFuncDataACS0( 31, addCallFunc(ACS_CF_SectorSoundSeq));
 //addFuncDataACS0( 32, addCallFunc(ACS_CF_PolyojbSoundSeq));
   addFuncDataACS0( 33, addCallFunc(ACS_CF_GetPolyobjX));
   addFuncDataACS0( 34, addCallFunc(ACS_CF_GetPolyobjY));
   addFuncDataACS0( 35, addCallFunc(ACS_CF_CheckSight));
   addFuncDataACS0( 36, addCallFunc(ACS_CF_SpawnForced));
 //addFuncDataACS0( 37, addCallFunc(ACS_CF_AnnouncerSound));
 //addFuncDataACS0( 38, addCallFunc(ACS_CF_SetPointer));
   // 39-45: ACSVM internal funcs.
   addFuncDataACS0( 46, addCallFunc(ACS_CF_UniqueTID));
   addFuncDataACS0( 47, addCallFunc(ACS_CF_IsTIDUsed));
   addFuncDataACS0( 48, addCallFunc(ACS_CF_Sqrt));
   addFuncDataACS0( 49, addCallFunc(ACS_CF_FixedSqrt));
   addFuncDataACS0( 50, addCallFunc(ACS_CF_VectorLength));
 //addFuncDataACS0( 51, addCallFunc(ACS_CF_SetHudClipRect));
 //addFuncDataACS0( 52, addCallFunc(ACS_CF_SetHudWrapWidth));
 //addFuncDataACS0( 53, addCallFunc(ACS_CF_SetCVar));
 //addFuncDataACS0( 54, addCallFunc(ACS_CF_GetUserCVar));
 //addFuncDataACS0( 55, addCallFunc(ACS_CF_SetUserCVar));
   addFuncDataACS0( 56, addCallFunc(ACS_CF_GetCVarString));
 //addFuncDataACS0( 57, addCallFunc(ACS_CF_SetCVarString));
 //addFuncDataACS0( 58, addCallFunc(ACS_CF_GetUserCVarString));
 //addFuncDataACS0( 59, addCallFunc(ACS_CF_SetUserCVarString));
 //addFuncDataACS0( 60, addCallFunc(ACS_CF_LineAttack));
   addFuncDataACS0( 61, addCallFunc(ACS_CF_PlaySound));
   addFuncDataACS0( 62, addCallFunc(ACS_CF_StopSound));
   // 63-67: ACSVM internal funcs.
 //addFuncDataACS0( 68, addCallFunc(ACS_CF_GetThingType));
   addFuncDataACS0( 69, addCallFunc(ACS_CF_GetWeapon));
 //addFuncDataACS0( 70, addCallFunc(ACS_CF_SoundVolume));
   addFuncDataACS0( 71, addCallFunc(ACS_CF_PlayActorSound));
 //addFuncDataACS0( 72, addCallFunc(ACS_CF_SpawnDecal));
 //addFuncDataACS0( 73, addCallFunc(ACS_CF_CheckFont));
 //addFuncDataACS0( 74, addCallFunc(ACS_CF_DropItem));
   addFuncDataACS0( 75, addCallFunc(ACS_CF_CheckFlag));
 //addFuncDataACS0( 76, addCallFunc(ACS_CF_SetLineActivation));
 //addFuncDataACS0( 77, addCallFunc(ACS_CF_GetLineActivation));
 //addFuncDataACS0( 78, addCallFunc(ACS_CF_GetThingPowerupTics));
   addFuncDataACS0( 79, addCallFunc(ACS_CF_ChangeActorAngle));
   addFuncDataACS0( 80, addCallFunc(ACS_CF_ChangeActorPitch));
 //addFuncDataACS0( 81, addCallFunc(ACS_CF_GetArmorInfo));
 //addFuncDataACS0( 82, addCallFunc(ACS_CF_DropInventory));
 //addFuncDataACS0( 83, addCallFunc(ACS_CF_PickThing));
 //addFuncDataACS0( 84, addCallFunc(ACS_CF_IsPointerEqual));
 //addFuncDataACS0( 85, addCallFunc(ACS_CF_CanRaiseThing));
 //addFuncDataACS0( 86, addCallFunc(ACS_CF_SetThingTeleFog));
 //addFuncDataACS0( 87, addCallFunc(ACS_CF_SwapThingTeleFog));
 //addFuncDataACS0( 88, addCallFunc(ACS_CF_SetThingRoll));
 //addFuncDataACS0( 89, addCallFunc(ACS_CF_SetThingRoll));
 //addFuncDataACS0( 90, addCallFunc(ACS_CF_GetThingRoll));
 //addFuncDataACS0( 91, addCallFunc(ACS_CF_QuakeEx));
 //addFuncDataACS0( 92, addCallFunc(ACS_CF_Warp));
 //addFuncDataACS0( 93, addCallFunc(ACS_CF_GetMaxInventory));
   addFuncDataACS0( 94, addCallFunc(ACS_CF_SetSectorDamage));
 //addFuncDataACS0( 95, addCallFunc(ACS_CF_SetSectorTerrain));
 //addFuncDataACS0( 96, addCallFunc(ACS_CF_SpawnParticle));
 //addFuncDataACS0( 97, addCallFunc(ACS_CF_SetMusicVolume));
   addFuncDataACS0( 98, addCallFunc(ACS_CF_CheckProximity));
 //addFuncDataACS0( 99, addCallFunc(ACS_CF_CheckActorState));

   addFuncDataACS0(300, addCallFunc(ACS_CF_GetLineX));
   addFuncDataACS0(301, addCallFunc(ACS_CF_GetLineY));
   addFuncDataACS0(302, addCallFunc(ACS_CF_SetAirFriction));
   addFuncDataACS0(303, addCallFunc(ACS_CF_SetPolyobjXY));
}

//
// ACSEnvironment::allocThread
//
ACSVM::Thread *ACSEnvironment::allocThread()
{
   return new ACSThread(this);
}

//
// ACSEnvironment::callSpecImpl
//
ACSVM::Word ACSEnvironment::callSpecImpl(ACSVM::Thread *thread, ACSVM::Word spec,
                                         const ACSVM::Word *argV, ACSVM::Word argC)
{
   auto info = &static_cast<ACSThread *>(thread)->info;
   int args[NUMLINEARGS] = {};

   for(ACSVM::Word i = argC < NUMLINEARGS ? argC : NUMLINEARGS; i--;)
      args[i] = argV[i];

   return EV_ActivateACSSpecial(info->line, spec, args, info->side, info->mo, info->po);
}

//
// ACSEnvironment::checkTag
//
bool ACSEnvironment::checkTag(ACSVM::Word type, ACSVM::Word tag)
{
   switch(type)
   {
   case ACS_TAGTYPE_SECTOR:
      for(int secnum = -1; (secnum = P_FindSectorFromTag(tag, secnum)) >= 0;)
      {
         sector_t *sec = &sectors[secnum];
         if(sec->srf.floor.data || sec->srf.ceiling.data)
            return false;
      }
      return true;
   case ACS_TAGTYPE_POLYOBJ:
      const polyobj_t *po = Polyobj_GetForNum(tag);
      return !po || po->thinker == nullptr;
   }

   return false;
}

//
// ACSEnvironment::getModuleName
//
ACSVM::ModuleName ACSEnvironment::getModuleName(char const *str, size_t len)
{
   ACSVM::String *name = getString(str, len);

   if(!dir)
      return {name, dir, SIZE_MAX};

   int lump =  dir->checkNumForName(str, lumpinfo_t::ns_acs);

   return {name, dir, static_cast<size_t>(lump)};
}

//
// ACSEnvironment::getScriptTypeACS0
//
std::pair<ACSVM::Word /*type*/, ACSVM::Word /*name*/>
ACSEnvironment::getScriptTypeACS0(ACSVM::Word name)
{
   if(name >= 1000)
      return {ACS_STYPE_Open, name - 1000};
   else
      return {ACS_STYPE_Closed, name};
}

//
// ACSEnvironment::getSCriptTypeACSE
//
ACSVM::Word ACSEnvironment::getScriptTypeACSE(ACSVM::Word type)
{
   switch(type)
   {
   default:
   case  0: return ACS_STYPE_Closed;
   case  1: return ACS_STYPE_Open;
   case  4: return ACS_STYPE_Enter;
   }
}

//
// ACSEnvironment::loadModule
//
void ACSEnvironment::loadModule(ACSVM::Module *module)
{
   byte  *data;
   size_t size;

   if(module->name.i == SIZE_MAX)
      throw ACSVM::ReadError("ACSEnvironment::loadModule: bad lump");

   WadDirectory *moduleDir = static_cast<WadDirectory *>(module->name.p);

   // Fetch lump data. Use PU_LEVEL so the lump data does not get unexpectedly
   // purged as a result of further allocations.
   data = (byte *)moduleDir->cacheLumpNum(static_cast<int>(module->name.i), PU_LEVEL);
   size = moduleDir->lumpLength(static_cast<int>(module->name.i));

   try
   {
      module->readBytecode(data, size);
   }
   catch(const ACSVM::ReadError &e)
   {
      ++errors;
      doom_printf(FC_ERROR "failed to load ACS module: '%s': %s\a", module->name.s->str, e.what());
      throw ACSVM::ReadError("failed import");
   }
}

//
// ACSEnvironment::loadState
//
void ACSEnvironment::loadState(ACSVM::Serial &in)
{
   ACSVM::Environment::loadState(in);

   global = getGlobalScope(0);
   hub    = global->getHubScope(0);
   map    = hub->getMapScope(gamemap);
}

//
// ACSEnvironment::readModuleName
//
ACSVM::ModuleName ACSEnvironment::readModuleName(ACSVM::Serial &in) const
{
   auto name = ACSVM::Environment::readModuleName(in);
   name.p = dir;
   return name;
}

//
// ACSEnvironment::refStrings
//
void ACSEnvironment::refStrings()
{
   ACSVM::Environment::refStrings();
}

//
// ACSThread::loadState
//
void ACSThread::loadState(ACSVM::Serial &in)
{
   ACSVM::Thread::loadState(in);

   info.mo = static_cast<Mobj *>(P_ThinkerForNum(static_cast<unsigned>(ACSVM::ReadVLN<size_t>(in))));

   size_t linenum = ACSVM::ReadVLN<size_t>(in);
   info.line = linenum ? &lines[linenum - 1] : nullptr;

   info.side = static_cast<int>(ACSVM::ReadVLN<size_t>(in));

   if(saveLoadVersion >= 9)
   {
      size_t ponum = ACSVM::ReadVLN<size_t>(in);
      info.po = ponum ? &PolyObjects[ponum - 1] : nullptr;
   }
}

//
// ACSThread::saveState
//
void ACSThread::saveState(ACSVM::Serial &out) const
{
   ACSVM::Thread::saveState(out);

   ACSVM::WriteVLN<size_t>(out, P_NumForThinker(info.mo));
   ACSVM::WriteVLN<size_t>(out, info.line ? info.line - lines + 1 : 0);
   ACSVM::WriteVLN<size_t>(out, info.side);
   if(saveLoadVersion >= 9)
      ACSVM::WriteVLN<size_t>(out, info.po ? info.po - PolyObjects + 1 : 0);
}

//
// ACSThread::start
//
void ACSThread::start(ACSVM::Script *script, ACSVM::MapScope *map,
   const ACSVM::ThreadInfo *infoPtr, const ACSVM::Word *argV, ACSVM::Word argC)
{
   ACSVM::Thread::start(script, map, infoPtr, argV, argC);

   result = 1;

   if(infoPtr)
      info = *static_cast<const ACSThreadInfo *>(infoPtr);
   else
      info = {};
}

//
// ACSThread::stop
//
void ACSThread::stop()
{
   ACSVM::Thread::stop();

   info = {};
}

//
// ACS_Init
//
// Called at startup.
//
void ACS_Init(void)
{
}

//
// ACS_NewGame
//
// Called when a new game is started.
//
void ACS_NewGame(void)
{
   ACSenv.global->reset();
   ACSenv.global->active = true;
   ACSenv.hub = ACSenv.global->getHubScope(0);
   ACSenv.hub->active = true;
   ACSenv.map = nullptr;
}

//
// ACS_InitLevel
//
// Called at level start from P_SetupLevel
//
void ACS_InitLevel(void)
{
   if(ACSenv.map)
      ACSenv.map->reset();
}

//
// ACS_loadModule
//
static void ACS_loadModule(PODCollection<ACSVM::Module *> &modules, ACSVM::ModuleName &&name)
{
   ACSVM::Module *module;

   try
   {
      module = ACSenv.getModule(std::move(name));
   }
   catch(const ACSVM::ReadError &)
   {
      return;
   }

   modules.add(module);
}

//
// ACS_loadModules
//
// Reads lump names out of the lump and loads them as modules.
//
static void ACS_loadModules(PODCollection<ACSVM::Module *> &modules, WadDirectory *dir, int lump)
{
   const char *lumpItr, *lumpEnd;
   char lumpname[9], *nameItr, *const nameEnd = lumpname+8;

   lumpItr = (const char *)dir->cacheLumpNum(lump, PU_STATIC);
   lumpEnd = lumpItr + dir->lumpLength(lump);

   for(;;)
   {
      // Discard any whitespace.
      while(lumpItr != lumpEnd && ectype::isSpace(*lumpItr)) ++lumpItr;

      if(lumpItr == lumpEnd) break;

      // Read a name.
      nameItr = lumpname;
      while(lumpItr != lumpEnd && nameItr != nameEnd && !ectype::isSpace(*lumpItr))
         *nameItr++ = *lumpItr++;
      *nameItr = '\0';

      // Discard excess letters.
      while(lumpItr != lumpEnd && !ectype::isSpace(*lumpItr)) ++lumpItr;

      ACS_loadModule(modules, ACSenv.getModuleName(lumpname));
   }
}

//
// ACS_LoadLevelScript
//
// Loads the level scripts and initializes the levelscript virtual machine.
// Called from P_SetupLevel.
//
void ACS_LoadLevelScript(WadDirectory *dir, int lump)
{
   PODCollection<ACSVM::Module *> modules;

   // Set environment's WadDirectory.
   ACSenv.dir = dir;

   // Reset HubScope if entering a new hub, or if in no hub.
   // TODO: Hubs.
   ACSenv.hub->reset();
   ACSenv.hub->active = true;

   // Fetch MapScope for current map.
   ACSenv.map = ACSenv.hub->getMapScope(gamemap);
   ACSenv.map->active = true;

   // Load modules for map.
   // TODO: Only do this if not revisiting the map.

   ACSenv.errors = 0;

   // Load the level script, if any.
   if(lump != -1)
   {
      ACSVM::String *lumpName = ACSenv.getString(dir->getLumpName(lump));
      ACS_loadModule(modules, {lumpName, dir, (size_t)lump});
   }

   // Load LOADACS modules.
   WadChainIterator wci(*dir, "LOADACS");

   for(wci.begin(); wci.current(); wci.next())
   {
      if(wci.testLump(lumpinfo_t::ns_global))
         ACS_loadModules(modules, dir, (*wci)->selfindex);
   }

   // If any errors, give up loading level script.
   if(ACSenv.errors)
      return;

   // Finish adding modules to map scope.
   if(!modules.isEmpty())
      ACSenv.map->addModules(&modules[0], modules.getLength());

   // Start open and enter scripts.
   {
      ACSVM::MapScope::ScriptStartInfo scriptInfo;
      ACSThreadInfo                    threadInfo;

      // Set initial thread delay.
      scriptInfo.func = [](ACSVM::Thread *thread)
      {
         if(thread->script->type == ACS_STYPE_Open && LevelInfo.acsOpenDelay)
            thread->delay = TICRATE;
         else
            thread->delay = 1;
      };

      // Open scripts.
      ACSenv.map->scriptStartType(ACS_STYPE_Open, scriptInfo);

      // Enter scripts.
      for(int pnum = 0; pnum != MAXPLAYERS; ++pnum)
      {
         if(playeringame[pnum])
         {
            P_SetTarget(&threadInfo.mo, players[pnum].mo);
            scriptInfo.info = &threadInfo;
            ACSenv.map->scriptStartTypeForced(ACS_STYPE_Enter, scriptInfo);
         }
      }
   }
}

//
// ACS_Exec
//
void ACS_Exec()
{
   ACSenv.exec();
}

//
// ACS_ExecuteScriptI
//
bool ACS_ExecuteScriptI(uint32_t name, uint32_t mapnum, const uint32_t *argv,
                        uint32_t argc, Mobj *mo, line_t *line, int side, polyobj_t *po)
{
   ACSVM::ScopeID scope{ACSenv.global->id, ACSenv.hub->id, mapnum ? mapnum : gamemap};
   ACSThreadInfo  info{mo, line, side, po};
   return ACSenv.map->scriptStart(name, scope, {argv, argc, &info});
}

//
// ACS_ExecuteScriptIAlways
//
bool ACS_ExecuteScriptIAlways(uint32_t name, uint32_t mapnum, const uint32_t *argv,
                              uint32_t argc, Mobj *mo, line_t *line, int side, polyobj_t *po)
{
   ACSVM::ScopeID scope{ACSenv.global->id, ACSenv.hub->id, mapnum ? mapnum : gamemap};
   ACSThreadInfo  info{mo, line, side, po};
   return ACSenv.map->scriptStartForced(name, scope, {argv, argc, &info});
}

//
// ACS_ExecuteScriptIResult
//
uint32_t ACS_ExecuteScriptIResult(uint32_t name, const uint32_t *argv,
                                  uint32_t argc, Mobj *mo, line_t *line, int side, polyobj_t *po)
{
   ACSThreadInfo info{mo, line, side, po};
   return ACSenv.map->scriptStartResult(name, {argv, argc, &info});
}

//
// ACS_ExecuteScriptS
//
bool ACS_ExecuteScriptS(const char *str, uint32_t mapnum, const uint32_t *argv,
                        uint32_t argc, Mobj *mo, line_t *line, int side, polyobj_t *po)
{
   ACSVM::String *name = ACSenv.getString(str, strlen(str));
   ACSVM::ScopeID scope{ACSenv.global->id, ACSenv.hub->id, mapnum ? mapnum : gamemap};
   ACSThreadInfo  info{mo, line, side, po};
   return ACSenv.map->scriptStart(name, scope, {argv, argc, &info});
}

//
// ACS_ExecuteScriptSAlways
//
bool ACS_ExecuteScriptSAlways(const char *str, uint32_t mapnum, const uint32_t *argv,
                              uint32_t argc, Mobj *mo, line_t *line, int side, polyobj_t *po)
{
   ACSVM::String *name = ACSenv.getString(str, strlen(str));
   ACSVM::ScopeID scope{ACSenv.global->id, ACSenv.hub->id, mapnum ? mapnum : gamemap};
   ACSThreadInfo  info{mo, line, side, po};
   return ACSenv.map->scriptStartForced(name, scope, {argv, argc, &info});
}

//
// ACS_ExecuteScriptSResult
//
uint32_t ACS_ExecuteScriptSResult(const char *str, const uint32_t *argv,
                                 uint32_t argc, Mobj *mo, line_t *line, int side, polyobj_t *po)
{
   ACSVM::String *name = ACSenv.getString(str, strlen(str));
   ACSThreadInfo  info{mo, line, side, po};
   return ACSenv.map->scriptStartResult(name, {argv, argc, &info});
}

//
// ACS_SuspendScriptI
//
bool ACS_SuspendScriptI(uint32_t name, uint32_t mapnum)
{
   ACSVM::ScopeID scope{ACSenv.global->id, ACSenv.hub->id, mapnum ? mapnum : gamemap};
   return ACSenv.map->scriptPause(name, scope);
}

//
// ACS_SuspendScriptS
//
bool ACS_SuspendScriptS(const char *str, uint32_t mapnum)
{
   ACSVM::String *name = ACSenv.getString(str, strlen(str));
   ACSVM::ScopeID scope{ACSenv.global->id, ACSenv.hub->id, mapnum ? mapnum : gamemap};
   return ACSenv.map->scriptPause(name, scope);
}

//
// ACS_TerminateScriptI
//
bool ACS_TerminateScriptI(uint32_t name, uint32_t mapnum)
{
   ACSVM::ScopeID scope{ACSenv.global->id, ACSenv.hub->id, mapnum ? mapnum : gamemap};
   return ACSenv.map->scriptStop(name, scope);
}

//
// ACS_TerminateScriptS
//
bool ACS_TerminateScriptS(const char *str, uint32_t mapnum)
{
   ACSVM::String *name = ACSenv.getString(str, strlen(str));
   ACSVM::ScopeID scope{ACSenv.global->id, ACSenv.hub->id, mapnum ? mapnum : gamemap};
   return ACSenv.map->scriptStop(name, scope);
}

//=============================================================================
//
// Save/Load Code
//

//
// ACSBuffer
//
// Wraps an InBuffer or OutBuffer in a std::streambuf for ACSVM serialization.
//
class ACSBuffer : public std::streambuf
{
public:
   explicit ACSBuffer(InBuffer *in_) : in{in_}, out{nullptr} {}
   explicit ACSBuffer(OutBuffer *out_) : in{nullptr}, out{out_} {}

   InBuffer  *in;
   OutBuffer *out;

protected:
   virtual int overflow(int c)
   {
      // Write single byte to destination.
      if(!out || !out->writeUint8(c)) return EOF;
      return c;
   }

   virtual int underflow()
   {
      // Read single byte from source.
      if(!in || in->read(buf, 1) != 1) return EOF;
      setg(buf, buf, buf + 1);
      return static_cast<unsigned char>(buf[0]);
   }

   char buf[1];
};

//
// ACS_Archive
//
void ACS_Archive(SaveArchive &arc)
{
   // Setup the context here so we know how to sav it
   ACSThread::saveLoadVersion = arc.saveVersion();

   if(arc.isLoading())
   {
      ACSBuffer     buf{arc.getLoadFile()};
      std::istream  str{&buf};
      ACSVM::Serial in {str};

      try
      {
         in.loadHead();
         ACSenv.loadState(in);
         in.loadTail();
      }
      catch(ACSVM::SerialError const &e)
      {
         I_Error("ACS_Archive: %s\n", e.what());
      }
   }
   else if(arc.isSaving())
   {
      ACSBuffer     buf{arc.getSaveFile()};
      std::ostream  str{&buf};
      ACSVM::Serial out{str};

      // Enable debug signatures.
      out.signs = true;

      out.saveHead();
      ACSenv.saveState(out);
      out.saveTail();
   }
}

// EOF

