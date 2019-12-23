// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//   Generalized line action system - Actions
//
//-----------------------------------------------------------------------------

#ifndef EV_ACTIONS_H__
#define EV_ACTIONS_H__

struct ev_action_t;
struct ev_instance_t;

#define DECLARE_ACTION(name) int name (ev_action_t *, ev_instance_t *)

// Null Action
DECLARE_ACTION(EV_ActionNull);

// DOOM Actions
DECLARE_ACTION(EV_ActionOpenDoor);
DECLARE_ACTION(EV_ActionCloseDoor);
DECLARE_ACTION(EV_ActionRaiseDoor);
DECLARE_ACTION(EV_ActionRaiseFloor);
DECLARE_ACTION(EV_ActionFastCeilCrushRaise);
DECLARE_ACTION(EV_ActionBuildStairsUp8);
DECLARE_ACTION(EV_ActionPlatDownWaitUpStay);
DECLARE_ACTION(EV_ActionLightTurnOn);
DECLARE_ACTION(EV_ActionLightTurnOn255);
DECLARE_ACTION(EV_ActionCloseDoor30);
DECLARE_ACTION(EV_ActionStartLightStrobing);
DECLARE_ACTION(EV_ActionLowerFloor);
DECLARE_ACTION(EV_ActionPlatRaiseNearestChange);
DECLARE_ACTION(EV_ActionCeilingCrushAndRaise);
DECLARE_ACTION(EV_ActionFloorRaiseToTexture);
DECLARE_ACTION(EV_ActionLightsVeryDark);
DECLARE_ACTION(EV_ActionLowerFloorTurbo);
DECLARE_ACTION(EV_ActionFloorLowerAndChange);
DECLARE_ACTION(EV_ActionFloorLowerToLowest);
DECLARE_ACTION(EV_ActionTeleport);
DECLARE_ACTION(EV_ActionRaiseCeilingLowerFloor);
DECLARE_ACTION(EV_ActionBOOMRaiseCeilingLowerFloor);
DECLARE_ACTION(EV_ActionBOOMRaiseCeilingOrLowerFloor);
DECLARE_ACTION(EV_ActionCeilingLowerAndCrush);
DECLARE_ACTION(EV_ActionExitLevel);
DECLARE_ACTION(EV_ActionSwitchExitLevel);
DECLARE_ACTION(EV_ActionGunExitLevel);
DECLARE_ACTION(EV_ActionPlatPerpetualRaise);
DECLARE_ACTION(EV_ActionPlatStop);
DECLARE_ACTION(EV_ActionFloorRaiseCrush);
DECLARE_ACTION(EV_ActionCeilingCrushStop);
DECLARE_ACTION(EV_ActionRaiseFloor24);
DECLARE_ACTION(EV_ActionRaiseFloor24Change);
DECLARE_ACTION(EV_ActionBuildStairsTurbo16);
DECLARE_ACTION(EV_ActionTurnTagLightsOff);
DECLARE_ACTION(EV_ActionDoorBlazeRaise);
DECLARE_ACTION(EV_ActionDoorBlazeOpen);
DECLARE_ACTION(EV_ActionDoorBlazeClose);
DECLARE_ACTION(EV_ActionFloorRaiseToNearest );
DECLARE_ACTION(EV_ActionPlatBlazeDWUS);
DECLARE_ACTION(EV_ActionSecretExit);
DECLARE_ACTION(EV_ActionSwitchSecretExit);
DECLARE_ACTION(EV_ActionGunSecretExit);
DECLARE_ACTION(EV_ActionRaiseFloorTurbo);
DECLARE_ACTION(EV_ActionSilentCrushAndRaise );
DECLARE_ACTION(EV_ActionRaiseFloor512);
DECLARE_ACTION(EV_ActionPlatRaise24Change);
DECLARE_ACTION(EV_ActionPlatRaise32Change);
DECLARE_ACTION(EV_ActionCeilingLowerToFloor );
DECLARE_ACTION(EV_ActionDoDonut);
DECLARE_ACTION(EV_ActionCeilingLowerToLowest);
DECLARE_ACTION(EV_ActionCeilingLowerToMaxFloor);
DECLARE_ACTION(EV_ActionSilentTeleport);
DECLARE_ACTION(EV_ActionChangeOnly);
DECLARE_ACTION(EV_ActionChangeOnlyNumeric  );
DECLARE_ACTION(EV_ActionFloorLowerToNearest);
DECLARE_ACTION(EV_ActionElevatorUp);
DECLARE_ACTION(EV_ActionElevatorDown);
DECLARE_ACTION(EV_ActionElevatorCurrent);
DECLARE_ACTION(EV_ActionSilentLineTeleport);
DECLARE_ACTION(EV_ActionSilentLineTeleportReverse);
DECLARE_ACTION(EV_ActionPlatToggleUpDown);
DECLARE_ACTION(EV_ActionStartLineScript);
DECLARE_ACTION(EV_ActionVerticalDoor);
DECLARE_ACTION(EV_ActionDoLockedDoor);

// Heretic Actions
DECLARE_ACTION(EV_ActionLowerFloorTurboA);
DECLARE_ACTION(EV_ActionHereticDoorRaise3x);
DECLARE_ACTION(EV_ActionHereticStairsBuildUp8FS);
DECLARE_ACTION(EV_ActionHereticStairsBuildUp16FS);

// BOOM Generalized Action
DECLARE_ACTION(EV_ActionBoomGen);

// Parameterized Actions
DECLARE_ACTION(EV_ActionParamDoorRaise);
DECLARE_ACTION(EV_ActionParamDoorOpen);
DECLARE_ACTION(EV_ActionParamDoorClose);
DECLARE_ACTION(EV_ActionParamDoorCloseWaitOpen);
DECLARE_ACTION(EV_ActionParamDoorWaitRaise);
DECLARE_ACTION(EV_ActionParamDoorWaitClose);
DECLARE_ACTION(EV_ActionParamDoorLockedRaise);
DECLARE_ACTION(EV_ActionParamFloorRaiseToHighest);
DECLARE_ACTION(EV_ActionParamEEFloorLowerToHighest);
DECLARE_ACTION(EV_ActionParamFloorLowerToHighest);
DECLARE_ACTION(EV_ActionParamFloorRaiseToLowest);
DECLARE_ACTION(EV_ActionParamFloorLowerToLowest);
DECLARE_ACTION(EV_ActionParamFloorRaiseToNearest);
DECLARE_ACTION(EV_ActionParamFloorLowerToNearest);
DECLARE_ACTION(EV_ActionParamFloorRaiseToLowestCeiling);
DECLARE_ACTION(EV_ActionParamFloorLowerToLowestCeiling);
DECLARE_ACTION(EV_ActionParamFloorRaiseToCeiling);
DECLARE_ACTION(EV_ActionParamFloorRaiseByTexture);
DECLARE_ACTION(EV_ActionParamFloorLowerByTexture);
DECLARE_ACTION(EV_ActionParamFloorRaiseByValue);
DECLARE_ACTION(EV_ActionParamFloorRaiseByValueTimes8);
DECLARE_ACTION(EV_ActionParamFloorLowerByValue);
DECLARE_ACTION(EV_ActionParamFloorLowerByValueTimes8);
DECLARE_ACTION(EV_ActionParamFloorMoveToValue);
DECLARE_ACTION(EV_ActionParamFloorMoveToValueTimes8 );
DECLARE_ACTION(EV_ActionParamFloorRaiseInstant);
DECLARE_ACTION(EV_ActionParamFloorLowerInstant);
DECLARE_ACTION(EV_ActionParamFloorToCeilingInstant);
DECLARE_ACTION(EV_ActionParamFloorRaiseAndCrush);
DECLARE_ACTION(EV_ActionParamFloorCrushStop);
DECLARE_ACTION(EV_ActionParamFloorCeilingLowerByValue);
DECLARE_ACTION(EV_ActionParamFloorCeilingRaiseByValue);
DECLARE_ACTION(EV_ActionParamFloorGeneric);
DECLARE_ACTION(EV_ActionParamFloorCeilingLowerRaise);
DECLARE_ACTION(EV_ActionParamCeilingRaiseToHighest);
DECLARE_ACTION(EV_ActionParamCeilingToHighestInstant);
DECLARE_ACTION(EV_ActionParamCeilingRaiseToNearest);
DECLARE_ACTION(EV_ActionParamCeilingLowerToNearest);
DECLARE_ACTION(EV_ActionParamCeilingRaiseToLowest);
DECLARE_ACTION(EV_ActionParamCeilingLowerToLowest);
DECLARE_ACTION(EV_ActionParamCeilingRaiseToHighestFloor);
DECLARE_ACTION(EV_ActionParamCeilingLowerToHighestFloor);
DECLARE_ACTION(EV_ActionParamCeilingToFloorInstant);
DECLARE_ACTION(EV_ActionParamCeilingLowerToFloor);
DECLARE_ACTION(EV_ActionParamCeilingRaiseByTexture);
DECLARE_ACTION(EV_ActionParamCeilingLowerByTexture);
DECLARE_ACTION(EV_ActionParamCeilingRaiseByValue);
DECLARE_ACTION(EV_ActionParamCeilingLowerByValue);
DECLARE_ACTION(EV_ActionParamCeilingRaiseByValueTimes8);
DECLARE_ACTION(EV_ActionParamCeilingLowerByValueTimes8);
DECLARE_ACTION(EV_ActionParamCeilingMoveToValue);
DECLARE_ACTION(EV_ActionParamCeilingMoveToValueTimes8);
DECLARE_ACTION(EV_ActionParamCeilingRaiseInstant);
DECLARE_ACTION(EV_ActionParamCeilingLowerInstant);
DECLARE_ACTION(EV_ActionParamCeilingCrushAndRaise);
DECLARE_ACTION(EV_ActionParamCeilingCrushAndRaiseA);
DECLARE_ACTION(EV_ActionParamCeilingCrushAndRaiseSilentA);
DECLARE_ACTION(EV_ActionParamCeilingCrushAndRaiseDist);
DECLARE_ACTION(EV_ActionParamCeilingCrushAndRaiseSilentDist);
DECLARE_ACTION(EV_ActionParamCeilingCrushStop);
DECLARE_ACTION(EV_ActionParamCeilingCrushRaiseAndStay);
DECLARE_ACTION(EV_ActionParamCeilingCrushRaiseAndStayA);
DECLARE_ACTION(EV_ActionParamCeilingCrushRaiseAndStaySilA);
DECLARE_ACTION(EV_ActionParamCeilingLowerAndCrush);
DECLARE_ACTION(EV_ActionParamCeilingLowerAndCrushDist);
DECLARE_ACTION(EV_ActionParamCeilingGeneric);
DECLARE_ACTION(EV_ActionParamGenCrusher);
DECLARE_ACTION(EV_ActionParamStairsBuildUpDoom);
DECLARE_ACTION(EV_ActionParamStairsBuildUpDoomCrush);
DECLARE_ACTION(EV_ActionParamStairsBuildDownDoom);
DECLARE_ACTION(EV_ActionParamStairsBuildUpDoomSync);
DECLARE_ACTION(EV_ActionParamStairsBuildDownDoomSync);
DECLARE_ACTION(EV_ActionParamGenStairs);
DECLARE_ACTION(EV_ActionPolyobjDoorSlide);
DECLARE_ACTION(EV_ActionPolyobjDoorSwing);
DECLARE_ACTION(EV_ActionPolyobjMove);
DECLARE_ACTION(EV_ActionPolyobjMoveTimes8);
DECLARE_ACTION(EV_ActionPolyobjMoveTo);
DECLARE_ACTION(EV_ActionPolyobjMoveToSpot);
DECLARE_ACTION(EV_ActionPolyobjORMove);
DECLARE_ACTION(EV_ActionPolyobjORMoveTimes8);
DECLARE_ACTION(EV_ActionPolyobjORMoveTo);
DECLARE_ACTION(EV_ActionPolyobjORMoveToSpot);
DECLARE_ACTION(EV_ActionPolyobjRotateRight);
DECLARE_ACTION(EV_ActionPolyobjORRotateRight);
DECLARE_ACTION(EV_ActionPolyobjRotateLeft);
DECLARE_ACTION(EV_ActionPolyobjORRotateLeft);
DECLARE_ACTION(EV_ActionPolyobjStop);
DECLARE_ACTION(EV_ActionPillarBuild);
DECLARE_ACTION(EV_ActionPillarBuildAndCrush );
DECLARE_ACTION(EV_ActionPillarOpen);
DECLARE_ACTION(EV_ActionACSExecute);
DECLARE_ACTION(EV_ActionACSExecuteAlways);
DECLARE_ACTION(EV_ActionACSSuspend);
DECLARE_ACTION(EV_ActionACSTerminate);
DECLARE_ACTION(EV_ActionACSExecuteWithResult);
DECLARE_ACTION(EV_ActionACSLockedExecute);
DECLARE_ACTION(EV_ActionACSLockedExecuteDoor);
DECLARE_ACTION(EV_ActionParamLightRaiseByValue);
DECLARE_ACTION(EV_ActionParamLightLowerByValue);
DECLARE_ACTION(EV_ActionParamLightChangeToValue);
DECLARE_ACTION(EV_ActionParamLightFade);
DECLARE_ACTION(EV_ActionParamLightGlow);
DECLARE_ACTION(EV_ActionParamLightFlicker);
DECLARE_ACTION(EV_ActionParamLightStrobe );
DECLARE_ACTION(EV_ActionParamLightStrobeDoom);
DECLARE_ACTION(EV_ActionRadiusQuake);
DECLARE_ACTION(EV_ActionCeilingWaggle);
DECLARE_ACTION(EV_ActionFloorWaggle);
DECLARE_ACTION(EV_ActionThingSpawn);
DECLARE_ACTION(EV_ActionThingSpawnNoFog);
DECLARE_ACTION(EV_ActionTeleportNewMap);
DECLARE_ACTION(EV_ActionTeleportEndGame);
DECLARE_ACTION(EV_ActionThingChangeTID);
DECLARE_ACTION(EV_ActionThingProjectile);
DECLARE_ACTION(EV_ActionThingProjectileGravity);
DECLARE_ACTION(EV_ActionThingActivate);
DECLARE_ACTION(EV_ActionThingDeactivate);
DECLARE_ACTION(EV_ActionThingRaise);
DECLARE_ACTION(EV_ActionThingStop);
DECLARE_ACTION(EV_ActionThrustThing);
DECLARE_ACTION(EV_ActionThrustThingZ);
DECLARE_ACTION(EV_ActionDamageThing);
DECLARE_ACTION(EV_ActionDamageThingEx);   // Thing_Damage
DECLARE_ACTION(EV_ActionThingDestroy);
DECLARE_ACTION(EV_ActionThingRemove);
DECLARE_ACTION(EV_ActionParamPlatPerpetualRaise);
DECLARE_ACTION(EV_ActionParamPlatPerpetualRaiseLip);
DECLARE_ACTION(EV_ActionParamPlatStop);
DECLARE_ACTION(EV_ActionParamPlatDWUS);
DECLARE_ACTION(EV_ActionParamPlatDWUSLip);
DECLARE_ACTION(EV_ActionParamPlatDownByValue);
DECLARE_ACTION(EV_ActionParamPlatUWDS);
DECLARE_ACTION(EV_ActionParamPlatUpByValue);
DECLARE_ACTION(EV_ActionParamPlatRaiseNearestChange);
DECLARE_ACTION(EV_ActionParamPlatRaiseChange);
DECLARE_ACTION(EV_ActionParamPlatToggleCeiling);
DECLARE_ACTION(EV_ActionParamPlatGeneric);
DECLARE_ACTION(EV_ActionParamDonut);
DECLARE_ACTION(EV_ActionParamTeleport);
DECLARE_ACTION(EV_ActionParamTeleportNoFog);
DECLARE_ACTION(EV_ActionParamTeleportLine);
DECLARE_ACTION(EV_ActionParamExitNormal);
DECLARE_ACTION(EV_ActionParamExitSecret);
DECLARE_ACTION(EV_ActionParamElevatorUp);
DECLARE_ACTION(EV_ActionParamElevatorDown);
DECLARE_ACTION(EV_ActionParamElevatorCurrent);
DECLARE_ACTION(EV_ActionChangeSkill);
DECLARE_ACTION(EV_ActionHealThing);
DECLARE_ACTION(EV_ActionParamSectorSetRotation);
DECLARE_ACTION(EV_ActionParamSectorSetCeilingPanning);
DECLARE_ACTION(EV_ActionParamSectorSetFloorPanning);
DECLARE_ACTION(EV_ActionParamSectorChangeSound);

DECLARE_ACTION(EV_ActionACSScrollFloor);
DECLARE_ACTION(EV_ActionACSScrollCeiling);
#endif

// EOF

