//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Generalized line action system - Bindings.
// Authors: James Haley, David Hill, Ioan Chera, Max Waine
//

#include "z_zone.h"

#include "ev_actions.h"
#include "ev_specials.h"

#include "ev_bindings.h"

//=============================================================================
//
// DOOM Line Actions
//

//
// The Null line action doesn't do anything, and is used to override inherited
// bindings. For example in Heretic, type 99 is a static init line. Since
// Heretic defers to DOOM's bindings for types it does not implement, it needs
// to hide BOOM type 99 with a null action binding.
//
ev_action_t NullAction =
{
   &NullActionType,
   EV_ActionNull,
   0,
   0,
   "None",
   0
};

// Macro to declare a DOOM-style W1 line action
#define W1LINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &W1ActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version,                               \
      #name,                                 \
      0                                      \
   }

// Macro to declare a DOOM-style WR line action
#define WRLINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &WRActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version,                               \
      #name,                                 \
      0                                      \
   }

// Macro to declare a DOOM-style S1 line action
#define S1LINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &S1ActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version,                               \
      #name,                                 \
      0                                      \
   }

// Macro to declare a DOOM-style SR line action
#define SRLINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &SRActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version,                               \
      #name,                                 \
      0                                      \
   }

// Macro to declare a DOOM-style DR line action
#define DRLINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &DRActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version,                               \
      #name,                                 \
      0                                      \
   }

// Macro to declare a DOOM-style G1 line action
#define G1LINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &G1ActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version,                               \
      #name,                                 \
      0                                      \
   }

// Macro to declare a DOOM-style GR line action
#define GRLINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &GRActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version,                               \
      #name,                                 \
      0                                      \
   }

// DOOM Line Type 1 - DR Raise Door
DRLINE(DRRaiseDoor, VerticalDoor, EV_PREALLOWMONSTERS, 0);

// DOOM Line Type 2 - W1 Open Door
W1LINE(W1OpenDoor, OpenDoor, 0, 0);

// DOOM Line Type 3 - W1 Close Door
W1LINE(W1CloseDoor, CloseDoor, 0, 0);

// DOOM Line Type 4 - W1 Raise Door
W1LINE(W1RaiseDoor, RaiseDoor, EV_PREALLOWMONSTERS, 0);

// DOOM Line Type 5 - W1 Raise Floor
W1LINE(W1RaiseFloor, RaiseFloor, 0, 0);

// DOOM Line Type 6 - W1 Ceiling Fast Crush and Raise
W1LINE(W1FastCeilCrushRaise, FastCeilCrushRaise, 0, 0);

// DOOM Line Type 7 - S1 Build Stairs Up 8
S1LINE(S1BuildStairsUp8, BuildStairsUp8, 0, 0);

// DOOM Line Type 8 - W1 Build Stairs Up 8
W1LINE(W1BuildStairsUp8, BuildStairsUp8, 0, 0);

// DOOM Line Type 9 - S1 Donut
S1LINE(S1DoDonut, DoDonut, 0, 0);

// DOOM Line Type 10 - W1 Plat Down-Wait-Up-Stay
W1LINE(W1PlatDownWaitUpStay, PlatDownWaitUpStay, EV_PREALLOWMONSTERS | EV_ISMBFLIFT, 0);

// DOOM Line Type 11 - S1 Exit Level
S1LINE(S1ExitLevel, SwitchExitLevel, EV_PREALLOWZEROTAG | EV_ISMAPPEDEXIT, 0);

// DOOM Line Type 12 - W1 Light Turn On
W1LINE(W1LightTurnOn, LightTurnOn, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 13 - W1 Light Turn On 255
W1LINE(W1LightTurnOn255, LightTurnOn255, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 14 - S1 Plat Raise 32 and Change
S1LINE(S1PlatRaise32Change, PlatRaise32Change, EV_ISMBFLIFT, 0);

// DOOM Line Type 15 - S1 Plat Raise 24 and Change
S1LINE(S1PlatRaise24Change, PlatRaise24Change, EV_ISMBFLIFT, 0);

// DOOM Line Type 16 - W1 Close Door, Open in 30
W1LINE(W1CloseDoor30, CloseDoor30, 0, 0);

// DOOM Line Type 17 - W1 Start Light Strobing
W1LINE(W1StartLightStrobing, StartLightStrobing, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 18 - S1 Raise Floor to Next Highest Floor
S1LINE(S1FloorRaiseToNearest, FloorRaiseToNearest, 0, 0);

// DOOM Line Type 19 - W1 Lower Floor
W1LINE(W1LowerFloor, LowerFloor, 0, 0);

// DOOM Line Type 20 - S1 Plat Raise to Nearest and Change
S1LINE(S1PlatRaiseNearestChange, PlatRaiseNearestChange, EV_ISMBFLIFT, 0);

// DOOM Line Type 21 - S1 Plat Down Wait Up Stay
S1LINE(S1PlatDownWaitUpStay, PlatDownWaitUpStay, EV_ISMBFLIFT, 0);

// DOOM Line Type 22 - W1 Plat Raise to Nearest and Change
W1LINE(W1PlatRaiseNearestChange, PlatRaiseNearestChange, EV_ISMBFLIFT, 0);

// DOOM Line Type 23 - S1 Floor Lower to Lowest
S1LINE(S1FloorLowerToLowest, FloorLowerToLowest, 0, 0);

// DOOM Line Type 24 - G1 Raise Floor to Highest Adjacent
G1LINE(G1RaiseFloor, RaiseFloor, 0, 0);

// DOOM Line Type 25 - W1 Ceiling Crush and Raise
W1LINE(W1CeilingCrushAndRaise, CeilingCrushAndRaise, 0, 0);

// DOOM Line Type 26 - DR Raise Door Blue Key
DRLINE(DRRaiseDoorBlue, VerticalDoor, 0, 0);

// DOOM Line Type 27 - DR Raise Door Yellow Key
DRLINE(DRRaiseDoorYellow, VerticalDoor, 0, 0);

// DOOM Line Type 28 - DR Raise Door Red Key
DRLINE(DRRaiseDoorRed, VerticalDoor, 0, 0);

// DOOM Line Type 29 - S1 Raise Door
S1LINE(S1RaiseDoor, RaiseDoor, 0, 0);

// DOOM Line Type 30 - W1 Floor Raise To Texture
W1LINE(W1FloorRaiseToTexture, FloorRaiseToTexture, 0, 0);

// DOOM Line Type 31 - D1 Open Door
DRLINE(D1OpenDoor, VerticalDoor, 0, 0);

// DOOM Line Type 32 - D1 Open Door Blue Key
// FIXME / TODO: Due to a combination of problems between this
// special and EV_VerticalDoor, monsters think they can open types
// 32 through 34 and therefore stick to them. Needs a comp var fix.
DRLINE(D1OpenDoorBlue, VerticalDoor, EV_PREALLOWMONSTERS, 0);

// DOOM Line Type 33 - D1 Open Door Red Key
// FIXME / TODO: See above.
DRLINE(D1OpenDoorRed, VerticalDoor, EV_PREALLOWMONSTERS, 0);

// DOOM Line Type 34 - D1 Open Door Yellow Key
DRLINE(D1OpenDoorYellow, VerticalDoor, EV_PREALLOWMONSTERS, 0);

// DOOM Line Type 35 - W1 Lights Very Dark
W1LINE(W1LightsVeryDark, LightsVeryDark, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 36 - W1 Lower Floor Turbo
W1LINE(W1LowerFloorTurbo, LowerFloorTurbo, 0, 0);

// DOOM Line Type 37 - W1 Floor Lower and Change
W1LINE(W1FloorLowerAndChange, FloorLowerAndChange, 0, 0);

// DOOM Line Type 38 - W1 Floor Lower to Lowest
W1LINE(W1FloorLowerToLowest, FloorLowerToLowest, 0, 0);

// DOOM Line Type 39 - W1 Teleport
W1LINE(W1Teleport, Teleport, EV_PREALLOWMONSTERS | EV_PREALLOWZEROTAG | EV_ISTELEPORTER, 0);

// DOOM Line Type 40 - W1 Raise Ceiling Lower Floor (only raises ceiling)
W1LINE(W1RaiseCeilingLowerFloor, RaiseCeilingLowerFloor, 0, 0);

// DOOM Line Type 41 - S1 Ceiling Lower to Floor
S1LINE(S1CeilingLowerToFloor, CeilingLowerToFloor, 0, 0);

// DOOM Line Type 42 - SR Close Door
SRLINE(SRCloseDoor, CloseDoor, 0, 0);

// DOOM Line Type 43 - SR Lower Ceiling to Floor
SRLINE(SRCeilingLowerToFloor, CeilingLowerToFloor, 0, 0);

// DOOM Line Type 44 - W1 Ceiling Crush
W1LINE(W1CeilingLowerAndCrush, CeilingLowerAndCrush, 0, 0);

// DOOM Line Type 45 - SR Floor Lower to Surr. Floor 
SRLINE(SRLowerFloor, LowerFloor, 0, 0);

// DOOM Line Type 46 - GR Open Door
GRLINE(GROpenDoor, OpenDoor, EV_PREALLOWMONSTERS | EV_POSTCHANGEALWAYS, 0);

// DOOM Line Type 47 - G1 Plat Raise to Nearest and Change
G1LINE(G1PlatRaiseNearestChange, PlatRaiseNearestChange, EV_ISMBFLIFT, 0);

// DOOM Line Type 49 - S1 Ceiling Crush And Raise
S1LINE(S1CeilingCrushAndRaise, CeilingCrushAndRaise, 0, 0);

// DOOM Line Type 50 - S1 Close Door
S1LINE(S1CloseDoor, CloseDoor, 0, 0);

// DOOM Line Type 51 - S1 Secret Exit
S1LINE(S1SecretExit, SwitchSecretExit, EV_PREALLOWZEROTAG | EV_ISMAPPEDEXIT, 0);

// DOOM Line Type 52 - WR Exit Level
WRLINE(WRExitLevel, ExitLevel, EV_PREALLOWZEROTAG | EV_ISMAPPEDEXIT, 0);

// DOOM Line Type 53 - W1 Perpetual Platform Raise
W1LINE(W1PlatPerpetualRaise, PlatPerpetualRaise, EV_ISMBFLIFT, 0);

// DOOM Line Type 54 - W1 Platform Stop
W1LINE(W1PlatStop, PlatStop, 0, 0);

// DOOM Line Type 55 - S1 Floor Raise & Crush
S1LINE(S1FloorRaiseCrush, FloorRaiseCrush, 0, 0);

// DOOM Line Type 56 - W1 Floor Raise & Crush
W1LINE(W1FloorRaiseCrush, FloorRaiseCrush, 0, 0);

// DOOM Line Type 57 - W1 Ceiling Crush Stop
W1LINE(W1CeilingCrushStop, CeilingCrushStop, 0, 0);

// DOOM Line Type 58 - W1 Raise Floor 24
W1LINE(W1RaiseFloor24, RaiseFloor24, 0, 0);

// DOOM Line Type 59 - W1 Raise Floor 24 and Change
W1LINE(W1RaiseFloor24Change, RaiseFloor24Change, 0, 0);

// DOOM Line Type 60 - SR Lower Floor to Lowest
SRLINE(SRFloorLowerToLowest, FloorLowerToLowest, 0, 0);

// DOOM Line Type 61 - SR Open Door
SRLINE(SROpenDoor, OpenDoor, 0, 0);

// DOOM Line Type 62 - SR Plat Down Wait Up Stay
SRLINE(SRPlatDownWaitUpStay, PlatDownWaitUpStay, EV_ISMBFLIFT, 0);

// DOOM Line Type 63 - SR Raise Door
SRLINE(SRRaiseDoor, RaiseDoor, 0, 0);

// DOOM Line Type 64 - SR Raise Floor to Ceiling
SRLINE(SRRaiseFloor, RaiseFloor, 0, 0);

// DOOM Line Type 65 - SR Raise Floor and Crush
SRLINE(SRFloorRaiseCrush, FloorRaiseCrush, 0, 0);

// DOOM Line Type 66 - SR Plat Raise 24 and Change
SRLINE(SRPlatRaise24Change, PlatRaise24Change, EV_ISMBFLIFT, 0);

// DOOM Line Type 67 - SR Plat Raise 32 and Change
SRLINE(SRPlatRaise32Change, PlatRaise32Change, EV_ISMBFLIFT, 0);

// DOOM Line Type 68 - SR Plat Raise to Nearest and Change
SRLINE(SRPlatRaiseNearestChange, PlatRaiseNearestChange, EV_ISMBFLIFT, 0);

// DOOM Line Type 69 - SR Floor Raise to Next Highest Floor
SRLINE(SRFloorRaiseToNearest, FloorRaiseToNearest, 0, 0);

// DOOM Line Type 70 - SR Lower Floor Turbo
SRLINE(SRLowerFloorTurbo, LowerFloorTurbo, 0, 0);

// DOOM Line Type 71 - S1 Lower Floor Turbo
S1LINE(S1LowerFloorTurbo, LowerFloorTurbo, 0, 0);

// DOOM Line Type 72 - WR Ceiling Crush
WRLINE(WRCeilingLowerAndCrush, CeilingLowerAndCrush, 0, 0);

// DOOM Line Type 73 - WR Ceiling Crush and Raise
WRLINE(WRCeilingCrushAndRaise, CeilingCrushAndRaise, 0, 0);

// DOOM Line Type 74 - WR Ceiling Crush Stop
WRLINE(WRCeilingCrushStop, CeilingCrushStop, 0, 0);

// DOOM Line Type 75 - WR Door Close
WRLINE(WRCloseDoor, CloseDoor, 0, 0);

// DOOM Line Type 76 - WR Close Door 30
WRLINE(WRCloseDoor30, CloseDoor30, 0, 0);

// DOOM Line Type 77 - WR Fast Ceiling Crush & Raise
WRLINE(WRFastCeilCrushRaise, FastCeilCrushRaise, 0, 0);

// BOOM Extended Line Type 78 - SR Change Texture/Type Only (Numeric)
SRLINE(SRChangeOnlyNumeric, ChangeOnlyNumeric, 0, 200);

// DOOM Line Type 79 - WR Lights Very Dark
WRLINE(WRLightsVeryDark, LightsVeryDark, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 80 - WR Light Turn On - Brightest Near
WRLINE(WRLightTurnOn, LightTurnOn, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 81 - WR Light Turn On 255
WRLINE(WRLightTurnOn255, LightTurnOn255, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 82 - WR Lower Floor To Lowest
WRLINE(WRFloorLowerToLowest, FloorLowerToLowest, 0, 0);

// DOOM Line Type 83 - WR Lower Floor
WRLINE(WRLowerFloor, LowerFloor, 0, 0);

// DOOM Line Type 84 - WR Floor Lower and Change
WRLINE(WRFloorLowerAndChange, FloorLowerAndChange, 0, 0);

// DOOM Line Type 86 - WR Open Door
WRLINE(WROpenDoor, OpenDoor, 0, 0);

// DOOM Line Type 87 - WR Plat Perpetual Raise
WRLINE(WRPlatPerpetualRaise, PlatPerpetualRaise, EV_ISMBFLIFT, 0);

// DOOM Line Type 88 - WR Plat DWUS
WRLINE(WRPlatDownWaitUpStay, PlatDownWaitUpStay, EV_PREALLOWMONSTERS | EV_ISMBFLIFT, 0);

// DOOM Line Type 89 - WR Platform Stop
WRLINE(WRPlatStop, PlatStop, 0, 0);

// DOOM Line Type 90 - WR Raise Door
WRLINE(WRRaiseDoor, RaiseDoor, 0, 0);

// DOOM Line Type 91 - WR Raise Floor
WRLINE(WRRaiseFloor, RaiseFloor, 0, 0);

// DOOM Line Type 92 - WR Raise Floor 24
WRLINE(WRRaiseFloor24, RaiseFloor24, 0, 0);

// DOOM Line Type 93 - WR Raise Floor 24 and Change
WRLINE(WRRaiseFloor24Change, RaiseFloor24Change, 0, 0);

// DOOM Line Type 94 - WR Raise Floor and Crush
WRLINE(WRFloorRaiseCrush, FloorRaiseCrush, 0, 0);

// DOOM Line Type 95 - WR Plat Raise to Nearest & Change
WRLINE(WRPlatRaiseNearestChange, PlatRaiseNearestChange, EV_ISMBFLIFT, 0);

// DOOM Line Type 96 - WR Floor Raise to Texture
WRLINE(WRFloorRaiseToTexture, FloorRaiseToTexture, 0, 0);

// DOOM Line Type 97 - WR Teleport
WRLINE(WRTeleport, Teleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS | EV_ISTELEPORTER, 0);

// DOOM Line Type 98 - WR Lower Floor Turbo
WRLINE(WRLowerFloorTurbo, LowerFloorTurbo, 0, 0);

// DOOM Line Type 99 - SR Blaze Open Door Blue Key
SRLINE(SRDoorBlazeOpenBlue, DoLockedDoor, 0, 0);

// DOOM Line Type 100 - W1 Build Stairs Turbo 16
W1LINE(W1BuildStairsTurbo16, BuildStairsTurbo16, 0, 0);

// DOOM Line Type 101 - S1 Raise Floor
S1LINE(S1RaiseFloor, RaiseFloor, 0, 0);

// DOOM Line Type 102 - S1 Lower Floor to Surrounding Floor
S1LINE(S1LowerFloor, LowerFloor, 0, 0);

// DOOM Line Type 103 - S1 Open Door
S1LINE(S1OpenDoor, OpenDoor, 0, 0);

// DOOM Line Type 104 - W1 Turn Lights Off in Sector
W1LINE(W1TurnTagLightsOff, TurnTagLightsOff, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 105 - WR Door Blaze Raise
WRLINE(WRDoorBlazeRaise, DoorBlazeRaise, 0, 0);

// DOOM Line Type 106 - WR Door Blaze Open
WRLINE(WRDoorBlazeOpen, DoorBlazeOpen, 0, 0);

// DOOM Line Type 107 - WR Door Blaze Close
WRLINE(WRDoorBlazeClose, DoorBlazeClose, 0, 0);

// DOOM Line Type 108 - W1 Blazing Door Raise
W1LINE(W1DoorBlazeRaise, DoorBlazeRaise, 0, 0);

// DOOM Line Type 109 - W1 Blazing Door Open
W1LINE(W1DoorBlazeOpen, DoorBlazeOpen, 0, 0);

// DOOM Line Type 110 - W1 Blazing Door Close
W1LINE(W1DoorBlazeClose, DoorBlazeClose, 0, 0);

// DOOM Line Type 111 - S1 Blazing Door Raise
S1LINE(S1DoorBlazeRaise, DoorBlazeRaise, 0, 0);

// DOOM Line Type 112 - S1 Blazing Door Open
S1LINE(S1DoorBlazeOpen, DoorBlazeOpen, 0, 0);

// DOOM Line Type 113 - S1 Blazing Door Close
S1LINE(S1DoorBlazeClose, DoorBlazeClose, 0, 0);

// DOOM Line Type 114 - SR Blazing Door Raise
SRLINE(SRDoorBlazeRaise, DoorBlazeRaise, 0, 0);

// DOOM Line Type 115 - SR Blazing Door Open
SRLINE(SRDoorBlazeOpen, DoorBlazeOpen, 0, 0);

// DOOM Line Type 116 - SR Blazing Door Close
SRLINE(SRDoorBlazeClose, DoorBlazeClose, 0, 0);

// DOOM Line Type 117 - DR Door Blaze Raise
DRLINE(DRDoorBlazeRaise, VerticalDoor, 0, 0);

// DOOM Line Type 118 - D1 Door Blaze Open
DRLINE(D1DoorBlazeOpen, VerticalDoor, 0, 0);

// DOOM Line Type 119 - W1 Raise Floor to Nearest Floor
W1LINE(W1FloorRaiseToNearest, FloorRaiseToNearest, 0, 0);

// DOOM Line Type 120 - WR Plat Blaze DWUS
WRLINE(WRPlatBlazeDWUS, PlatBlazeDWUS, EV_ISMBFLIFT, 0);

// DOOM Line Type 121 - W1 Plat Blaze Down-Wait-Up-Stay
W1LINE(W1PlatBlazeDWUS, PlatBlazeDWUS, EV_ISMBFLIFT, 0);

// DOOM Line Type 122 - S1 Plat Blaze Down-Wait-Up-Stay
S1LINE(S1PlatBlazeDWUS, PlatBlazeDWUS, EV_ISMBFLIFT, 0);

// DOOM Line Type 123 - SR Plat Blaze Down-Wait-Up-Stay
SRLINE(SRPlatBlazeDWUS, PlatBlazeDWUS, EV_ISMBFLIFT, 0);

// DOOM Line Type 124 - WR Secret Exit
WRLINE(WRSecretExit, SecretExit, EV_PREALLOWZEROTAG | EV_ISMAPPEDEXIT, 0);

// DOOM Line Type 125 - W1 Teleport Monsters Only
// jff 3/5/98 add ability of monsters etc. to use teleporters
W1LINE(W1TeleportMonsters, Teleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS | EV_PREMONSTERSONLY | EV_ISTELEPORTER, 0);

// DOOM Line Type 126 - WR Teleport Monsters Only
WRLINE(WRTeleportMonsters, Teleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS | EV_PREMONSTERSONLY | EV_ISTELEPORTER, 0);

// DOOM Line Type 127 - S1 Build Stairs Up Turbo 16
S1LINE(S1BuildStairsTurbo16, BuildStairsTurbo16, 0, 0);

// DOOM Line Type 128 - WR Raise Floor to Nearest Floor
WRLINE(WRFloorRaiseToNearest, FloorRaiseToNearest, 0, 0);

// DOOM Line Type 129 - WR Raise Floor Turbo
WRLINE(WRRaiseFloorTurbo, RaiseFloorTurbo, 0, 0);

// DOOM Line Type 130 - W1 Raise Floor Turbo
W1LINE(W1RaiseFloorTurbo, RaiseFloorTurbo, 0, 0);

// DOOM Line Type 131 - S1 Raise Floor Turbo
S1LINE(S1RaiseFloorTurbo, RaiseFloorTurbo, 0, 0);

// DOOM Line Type 132 - SR Raise Floor Turbo
SRLINE(SRRaiseFloorTurbo, RaiseFloorTurbo, 0, 0);

// DOOM Line Type 133 - S1 Door Blaze Open Blue Key
S1LINE(S1DoorBlazeOpenBlue, DoLockedDoor, 0, 0);

// DOOM Line Type 134 - SR Door Blaze Open Red Key
SRLINE(SRDoorBlazeOpenRed, DoLockedDoor, 0, 0);

// DOOM Line Type 135 - S1 Door Blaze Open Red Key
S1LINE(S1DoorBlazeOpenRed, DoLockedDoor, 0, 0);

// DOOM Line Type 136 - SR Door Blaze Open Yellow Key
SRLINE(SRDoorBlazeOpenYellow, DoLockedDoor, 0, 0);

// DOOM Line Type 137 - S1 Door Blaze Open Yellow Key
S1LINE(S1DoorBlazeOpenYellow, DoLockedDoor, 0, 0);

// DOOM Line Type 138 - SR Light Turn On 255
SRLINE(SRLightTurnOn255, LightTurnOn255, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 0);

// DOOM Line Type 139 - SR Light Turn Off (35)
SRLINE(SRLightsVeryDark, LightsVeryDark, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 0);

// DOOM Line Type 140 - S1 Raise Floor 512
S1LINE(S1RaiseFloor512, RaiseFloor512, 0, 0);

// DOOM Line Type 141 - W1 Ceiling Silent Crush & Raise
W1LINE(W1SilentCrushAndRaise, SilentCrushAndRaise, 0, 0);

// BOOM Extended Line Type 142 - W1 Raise Floor 512
W1LINE(W1RaiseFloor512, RaiseFloor512, 0, 200);

// BOOM Extended Line Type 143 - W1 Plat Raise 24 and Change
W1LINE(W1PlatRaise24Change, PlatRaise24Change, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 144 - W1 Plat Raise 32 and Change
W1LINE(W1PlatRaise32Change, PlatRaise32Change, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 145 - W1 Ceiling Lower to Floor
W1LINE(W1CeilingLowerToFloor, CeilingLowerToFloor, 0, 200);

// BOOM Extended Line Type 146 - W1 Donut
W1LINE(W1DoDonut, DoDonut, 0, 200);

// BOOM Extended Line Type 147 - WR Raise Floor 512
WRLINE(WRRaiseFloor512, RaiseFloor512, 0, 200);

// BOOM Extended Line Type 148 - WR Raise Floor 24 and Change
WRLINE(WRPlatRaise24Change, PlatRaise24Change, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 149 - WR Raise Floor 32 and Change
WRLINE(WRPlatRaise32Change, PlatRaise32Change, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 150 - WR Start Slow Silent Crusher
WRLINE(WRSilentCrushAndRaise, SilentCrushAndRaise, 0, 200);

// BOOM Extended Line Type 151 - WR Raise Ceiling, Lower Floor
WRLINE(WRRaiseCeilingLowerFloor, BOOMRaiseCeilingLowerFloor, 0, 200);

// BOOM Extended Line Type 152 - WR Lower Ceiling to Floor
WRLINE(WRCeilingLowerToFloor, CeilingLowerToFloor, 0, 200);

// BOOM Extended Line Type 153 - W1 Change Texture/Type
W1LINE(W1ChangeOnly, ChangeOnly, 0, 200);

// BOOM Extended Line Type 154 - WR Change Texture/Type
// jff 3/16/98 renumber 216->154
WRLINE(WRChangeOnly, ChangeOnly, 0, 200);

// BOOM Extended Line Type 155 - WR Lower Pillar, Raise Donut
WRLINE(WRDoDonut, DoDonut, 0, 200);

// BOOM Extended Line Type 156 - WR Start Lights Strobing
WRLINE(WRStartLightStrobing, StartLightStrobing, EV_PREALLOWZEROTAG, 200);

// BOOM Extended Line Type 157 - WR Lights to Dimmest Near
WRLINE(WRTurnTagLightsOff, TurnTagLightsOff, EV_PREALLOWZEROTAG, 200);

// BOOM Extended Line Type 158 - S1 Raise Floor to Shortest Lower Texture
S1LINE(S1FloorRaiseToTexture, FloorRaiseToTexture, 0, 200);

// BOOM Extended Line Type 159 - S1 Floor Lower and Change
S1LINE(S1FloorLowerAndChange, FloorLowerAndChange, 0, 200);

// BOOM Extended Line Type 160 - S1 Raise Floor 24 and Change
S1LINE(S1RaiseFloor24Change, RaiseFloor24Change, 0, 200);

// BOOM Extended Line Type 161 - S1 Raise Floor 24
S1LINE(S1RaiseFloor24, RaiseFloor24, 0, 200);

// BOOM Extended Line Type 162 - S1 Plat Perpetual Raise
S1LINE(S1PlatPerpetualRaise, PlatPerpetualRaise, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 163 - S1 Stop Plat
S1LINE(S1PlatStop, PlatStop, EV_POSTCHANGEALWAYS | EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 164 - S1 Ceiling Fast Crush and Raise
S1LINE(S1FastCeilCrushRaise, FastCeilCrushRaise, 0, 200);

// BOOM Extended Line Type 165 - S1 Ceiling Silent Crush and Raise
S1LINE(S1SilentCrushAndRaise, SilentCrushAndRaise, 0, 200);

// BOOM Extended Line Type 166 - S1 Raise Ceiling -or- Lower Floor
// haleyjd: It's supposed to do both. Due to short-circuiting, it will
// only do one or the other. This is a BOOM glitch.
S1LINE(S1BOOMRaiseCeilingOrLowerFloor, BOOMRaiseCeilingOrLowerFloor, 0, 200);

// BOOM Extended Line Type 167 - S1 Ceiling Lower and Crush
S1LINE(S1CeilingLowerAndCrush, CeilingLowerAndCrush, 0, 200);

// BOOM Extended Line Type 168 - S1 Ceiling Crush Stop
S1LINE(S1CeilingCrushStop, CeilingCrushStop, 0, 200);

// BOOM Extended Line Type 169 - S1 Lights to Brighest Neighbor
S1LINE(S1LightTurnOn, LightTurnOn, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 200);

// BOOM Extended Line Type 170 - S1 Lights Very Dark
S1LINE(S1LightsVeryDark, LightsVeryDark, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 200);

// BOOM Extended Line Type 171 - S1 Lights On 255
S1LINE(S1LightTurnOn255, LightTurnOn255, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 200);

// BOOM Extended Line Type 172 - S1 Start Lights Strobing
S1LINE(S1StartLightStrobing, StartLightStrobing, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 200);

// BOOM Extended Line Type 173 - S1 Lights to Lowest Neighbor
S1LINE(S1TurnTagLightsOff, TurnTagLightsOff, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 200);

// BOOM Extended Line Type 174 - S1 Teleport
S1LINE(S1Teleport, Teleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS, 200);

// BOOM Extended Line Type 175 - S1 Door Close 30, then Open
S1LINE(S1CloseDoor30, CloseDoor30, 0, 200);

// BOOM Extended Line Type 176 - SR Floor Raise to Texture
SRLINE(SRFloorRaiseToTexture, FloorRaiseToTexture, 0, 200);

// BOOM Extended Line Type 177 - SR Floor Lower and Change
SRLINE(SRFloorLowerAndChange, FloorLowerAndChange, 0, 200);

// BOOM Extended Line Type 178 - SR Raise Floor 512
SRLINE(SRRaiseFloor512, RaiseFloor512, 0, 200);

// BOOM Extended Line Type 179 - SR Raise Floor 24 and Change
SRLINE(SRRaiseFloor24Change, RaiseFloor24Change, 0, 200);

// BOOM Extended Line Type 180 - SR Raise Floor 24
SRLINE(SRRaiseFloor24, RaiseFloor24, 0, 200);

// BOOM Extended Line Type 181 - SR Plat Perpetual Raise
SRLINE(SRPlatPerpetualRaise, PlatPerpetualRaise, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 182 - SR Plat Stop
SRLINE(SRPlatStop, PlatStop, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 183 - SR Fast Ceiling Crush and Raise
SRLINE(SRFastCeilCrushRaise, FastCeilCrushRaise, 0, 200);

// BOOM Extended Line Type 184 - SR Ceiling Crush and Raise
SRLINE(SRCeilingCrushAndRaise, CeilingCrushAndRaise, 0, 200);

// BOOM Extended Line Type 185 - SR Silent Crush and Raise
SRLINE(SRSilentCrushAndRaise, SilentCrushAndRaise, 0, 200);

// BOOM Extended Line Type 186 - SR Raise Ceiling -OR- Lower Floor
// NB: This line is also bugged, like its 166 S1 counterpart.
SRLINE(SRBOOMRaiseCeilingOrLowerFloor, BOOMRaiseCeilingOrLowerFloor, 0, 200);

// BOOM Extended Line Type 187 - SR Ceiling Lower and Crush
SRLINE(SRCeilingLowerAndCrush, CeilingLowerAndCrush, 0, 200);

// BOOM Extended Line Type 188 - SR Ceiling Crush Stop
SRLINE(SRCeilingCrushStop, CeilingCrushStop, 0, 200);

// BOOM Extended Line Type 189 - S1 Change Texture Only
S1LINE(S1ChangeOnly, ChangeOnly, 0, 200);

// BOOM Extended Line Type 190 - SR Change Texture/Type Only
SRLINE(SRChangeOnly, ChangeOnly, 0, 200);

// BOOM Extended Line Type 191 - SR Lower Pillar, Raise Donut
SRLINE(SRDoDonut, DoDonut, 0, 200);

// BOOM Extended Line Type 192 - SR Lights to Brightest Neighbor
SRLINE(SRLightTurnOn, LightTurnOn, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 200);

// BOOM Extended Line Type 193 - SR Start Lights Strobing
SRLINE(SRStartLightStrobing, StartLightStrobing, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 200);

// BOOM Extended Line Type 194 - SR Lights to Dimmest Near
SRLINE(SRTurnTagLightsOff, TurnTagLightsOff, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 200);

// BOOM Extended Line Type 195 - SR Teleport
SRLINE(SRTeleport, Teleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS, 200);

// BOOM Extended Line Type 196 - SR Close Door, Open in 30
SRLINE(SRCloseDoor30, CloseDoor30, 0, 200);

// BOOM Extended Line Type 197 - G1 Exit Level
G1LINE(G1ExitLevel, GunExitLevel, EV_PREALLOWZEROTAG | EV_ISMAPPEDEXIT, 200);

// BOOM Extended Line Type 198 - G1 Secret Exit
G1LINE(G1SecretExit, GunSecretExit, EV_PREALLOWZEROTAG | EV_ISMAPPEDEXIT, 200);

// BOOM Extended Line Type 199 - W1 Ceiling Lower to Lowest Surr. Ceiling
W1LINE(W1CeilingLowerToLowest, CeilingLowerToLowest, 0, 200);

// BOOM Extended Line Type 200 - W1 Ceiling Lower to Max Floor
W1LINE(W1CeilingLowerToMaxFloor, CeilingLowerToMaxFloor, 0, 200);

// BOOM Extended Line Type 201 - WR Lower Ceiling to Lowest Surr. Ceiling
WRLINE(WRCeilingLowerToLowest, CeilingLowerToLowest, 0, 200);

// BOOM Extended Line Type 202 - WR Lower Ceiling to Max Floor
WRLINE(WRCeilingLowerToMaxFloor, CeilingLowerToMaxFloor, 0, 200);

// BOOM Extended Line Type 203 - S1 Ceiling Lower to Lowest Surr. Ceiling
S1LINE(S1CeilingLowerToLowest, CeilingLowerToLowest, 0, 200);

// BOOM Extended Line Type 204 - S1 Ceiling Lower to Highest Surr. Floor
S1LINE(S1CeilingLowerToMaxFloor, CeilingLowerToMaxFloor, 0, 200);

// BOOM Extended Line Type 205 - SR Ceiling Lower to Lowest Surr. Ceiling
SRLINE(SRCeilingLowerToLowest, CeilingLowerToLowest, 0, 200);

// BOOM Extended Line Type 206 - SR CeilingLower to Highest Surr. Floor
SRLINE(SRCeilingLowerToMaxFloor, CeilingLowerToMaxFloor, 0, 200);

// BOOM Extended Line Type 207 - W1 Silent Teleport
W1LINE(W1SilentTeleport, SilentTeleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS, 200);

// BOOM Extended Line Type 208 - WR Silent Teleport
WRLINE(WRSilentTeleport, SilentTeleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS, 200);

// BOOM Extended Line Type 209 - S1 Silent Teleport
S1LINE(S1SilentTeleport, SilentTeleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS, 200);

// BOOM Extended Line Type 210 - SR Silent Teleport
SRLINE(SRSilentTeleport, SilentTeleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS, 200);

// BOOM Extended Line Type 211 - SSR Plat Toggle Floor Instant C/F
// jff 3/14/98 create instant toggle floor type
SRLINE(SRPlatToggleUpDown, PlatToggleUpDown, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 212 - WR Plat Toggle Floor Instant C/F
WRLINE(WRPlatToggleUpDown, PlatToggleUpDown, 0, 200);

// BOOM Extended Line Type 219 - W1 Floor Lower to Next Lower Neighbor
W1LINE(W1FloorLowerToNearest, FloorLowerToNearest, 0, 200);

// BOOM Extended Line Type 220 - WR Floor Lower to Next Lower Neighbor
WRLINE(WRFloorLowerToNearest, FloorLowerToNearest, 0, 200);

// BOOM Extended Line Type 221 - S1 Floor Lower to Next Lower Neighbor
S1LINE(S1FloorLowerToNearest, FloorLowerToNearest, 0, 200);

// BOOM Extended Line Type 222 - SR FLoor Lower to Next Lower Neighbor
SRLINE(SRFloorLowerToNearest, FloorLowerToNearest, 0, 200);

// BOOM Extended Line Type 227 - W1 Raise Elevator to Next Floor
W1LINE(W1ElevatorUp, ElevatorUp, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 228 - WR Raise Elevator to Next Floor
WRLINE(WRElevatorUp, ElevatorUp, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 229 - S1 Raise Elevator to Next Floor
S1LINE(S1ElevatorUp, ElevatorUp, 0, 200);

// BOOM Extended Line Type 230 - SR Raise Elevator to Next Floor
SRLINE(SRElevatorUp, ElevatorUp, 0, 200);

// BOOM Extended Line Type 231 - W1 Lower Elevator to Next Floor
W1LINE(W1ElevatorDown, ElevatorDown, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 232 - WR Lower Elevator to Next Floor
WRLINE(WRElevatorDown, ElevatorDown, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 233 - S1 Lower Elevator to Next Floor
S1LINE(S1ElevatorDown, ElevatorDown, 0, 200);

// BOOM Extended Line Type 234 - SR Lower Elevator to Next Floor
SRLINE(SRElevatorDown, ElevatorDown, 0, 200);

// BOOM Extended Line Type 235 - W1 Elevator to Current Floor
W1LINE(W1ElevatorCurrent, ElevatorCurrent, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 236 - WR Elevator to Current Floor
WRLINE(WRElevatorCurrent, ElevatorCurrent, EV_ISMBFLIFT, 200);

// BOOM Extended Line Type 237 - S1 Elevator to Current Floor
S1LINE(S1ElevatorCurrent, ElevatorCurrent, 0, 200);

// BOOM Extended Line Type 238 - SR Elevator to Current Floor
SRLINE(SRElevatorCurrent, ElevatorCurrent, 0, 200);

// BOOM Extended Line Type 239 - W1 Change Texture/Type Numeric
W1LINE(W1ChangeOnlyNumeric, ChangeOnlyNumeric, 0, 200);

// BOOM Extended Line Type 240 - WR Change Texture/Type Numeric
WRLINE(WRChangeOnlyNumeric, ChangeOnlyNumeric, 0, 200);

// BOOM Extended Line Type 241 - S1 Change Only, Numeric Model
S1LINE(S1ChangeOnlyNumeric, ChangeOnlyNumeric, 0, 200);

// BOOM Extended Line Type 243 - W1 Silent Teleport Line-to-Line
W1LINE(W1SilentLineTeleport, SilentLineTeleport, EV_PREALLOWMONSTERS, 200);

// BOOM Extended Line Type 244 - WR Silent Teleport Line-to-Line
WRLINE(WRSilentLineTeleport, SilentLineTeleport, EV_PREALLOWMONSTERS, 200);

// BOOM Extended Line Type 256 - WR Build Stairs Up 8
// jff 3/16/98 renumber 153->256
WRLINE(WRBuildStairsUp8, BuildStairsUp8, 0, 200);

// BOOM Extended Line Type 257 - WR Build Stairs Up Turbo 16
// jff 3/16/98 renumber 154->257
WRLINE(WRBuildStairsTurbo16, BuildStairsTurbo16, 0, 200);

// BOOM Extended Line Type 258 - SR Build Stairs Up 8
SRLINE(SRBuildStairsUp8, BuildStairsUp8, 0, 200);

// BOOM Extended Line Type 259 - SR Build Stairs Up Turbo 16
SRLINE(SRBuildStairsTurbo16, BuildStairsTurbo16, 0, 200);

// BOOM Extended Line Type 262 - W1 Silent Teleport Line-to-Line Reversed
W1LINE(W1SilentLineTeleportReverse, SilentLineTeleportReverse, EV_PREALLOWMONSTERS, 200);

// BOOM Extended Line Type 263 - WR Silent Teleport Line-to-Line Reversed
WRLINE(WRSilentLineTeleportReverse, SilentLineTeleportReverse, EV_PREALLOWMONSTERS, 200);

// BOOM Extended Line Type 264 - W1 Silent Line Teleport Monsters Only Reversed
W1LINE(W1SilentLineTRMonsters, SilentLineTeleportReverse, EV_PREALLOWMONSTERS | EV_PREMONSTERSONLY, 200);

// BOOM Extended Line Type 265 - WR Silent Line Teleport Monsters Only Reversed
WRLINE(WRSilentLineTRMonsters, SilentLineTeleportReverse, EV_PREALLOWMONSTERS | EV_PREMONSTERSONLY, 200);

// BOOM Extended Line Type 266 - W1 Silent Line Teleport Monsters Only
W1LINE(W1SilentLineTeleMonsters, SilentLineTeleport, EV_PREALLOWMONSTERS | EV_PREMONSTERSONLY, 200);

// BOOM Extended Line Type 267 - WR Silent Line Teleport Monsters Only
WRLINE(WRSilentLineTeleMonsters, SilentLineTeleport, EV_PREALLOWMONSTERS | EV_PREMONSTERSONLY, 200);

// BOOM Extended Line Type 268 - W1 Silent Teleport Monster Only
W1LINE(W1SilentTeleportMonsters, SilentTeleport, EV_PREALLOWMONSTERS | EV_PREMONSTERSONLY, 200);

// BOOM Extended Line Type 269 - WR Silent Teleport Monster Only
WRLINE(WRSilentTeleportMonsters, SilentTeleport, EV_PREALLOWMONSTERS | EV_PREMONSTERSONLY, 200);

// SMMU Extended Line Type 273 - WR Start Script One-Way
WRLINE(WRStartLineScript1S, StartLineScript, EV_PREFIRSTSIDEONLY | EV_PREALLOWZEROTAG, 300);

// SMMU Extended Line Type 274 - W1 Start Script
W1LINE(W1StartLineScript, StartLineScript, EV_PREALLOWZEROTAG, 300);

// SMMU Extended Line Type 275 - W1 Start Script One-Way
W1LINE(W1StartLineScript1S, StartLineScript, EV_PREFIRSTSIDEONLY | EV_PREALLOWZEROTAG, 300);

// SMMU Extended Line Type 276 - SR Start Script
SRLINE(SRStartLineScript, StartLineScript, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 300);

// SMMU Extended Line Type 277 - S1 Start Script
S1LINE(S1StartLineScript, StartLineScript, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 300);

// SMMU Extended Line Type 278 - GR Start Script
GRLINE(GRStartLineScript, StartLineScript, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 300);

// SMMU Extended Line Type 279 - G1 Start Script
G1LINE(G1StartLineScript, StartLineScript, EV_PREALLOWZEROTAG | EV_POSTCHANGEALWAYS, 300);

// SMMU Extended Line Type 280 - WR Start Script
WRLINE(WRStartLineScript, StartLineScript, EV_PREALLOWZEROTAG, 300);

//
// Heretic Actions
//

// Heretic Line Type 7 - S1 Stairs Build Up 8 FLOORSPEED
S1LINE(S1HereticStairsBuildUp8FS, HereticStairsBuildUp8FS, 0, 0);

// Heretic Line Type 8 - W1 Stairs Build Up 8 FLOORSPEED
W1LINE(W1HereticStairsBuildUp8FS, HereticStairsBuildUp8FS, 0, 0);

// Heretic Line Type 10 - W1 Plat Down-Wait-Up-Stay, No Monsters
W1LINE(W1HticPlatDownWaitUpStay, PlatDownWaitUpStay, 0, 0);

// Heretic Line Type 36 - W1 Lower Floor Turbo, Floorheight + 8
W1LINE(W1LowerFloorTurboA, LowerFloorTurboA, 0, 0);

// Heretic Line Type 70 - SR Lower Floor Turbo, Floorheight + 8
SRLINE(SRLowerFloorTurboA, LowerFloorTurboA, 0, 0);

// Heretic Line Type 71 - S1 Lower Floor Turbo, Floorheight + 8
S1LINE(S1LowerFloorTurboA, LowerFloorTurboA, 0, 0);

// Heretic Line Type 88 - WR Plat DWUS, No Monsters
WRLINE(WRHticPlatDownWaitUpStay, PlatDownWaitUpStay, 0, 0);

// Heretic Line Type 98 - WR Lower Floor Turbo, Floorheight + 8
WRLINE(WRLowerFloorTurboA, LowerFloorTurboA, 0, 0);

// Heretic Line Type 100 - WR Raise Door 3x
WRLINE(WRHereticDoorRaise3x, HereticDoorRaise3x, 0, 0);

// Heretic Line Type 106 - W1 Stairs Build Up 16 FLOORSPEED
W1LINE(W1HereticStairsBuildUp16FS, HereticStairsBuildUp16FS, 0, 0);

// Heretic Line Type 107 - S1 Stairs Build Up 16 FLOORSPEED
S1LINE(S1HereticStairsBuildUp16FS, HereticStairsBuildUp16FS, 0, 0);

//=============================================================================
//
// BOOM Generalized Line Action
//

ev_action_t BoomGenAction =
{                                         
   &BoomGenActionType,    // use generalized action type
   EV_ActionBoomGen,      // use a single action function
   0,                     // flags aren't used
   200,                   // BOOM or up
   "BoomGenAction",       // display name
   0                      // lock ID
};

//=============================================================================
//
// Parameterized Line Actions
//

// Macro to declare a parameterized line action

#define PARAMLINE(name)      \
   static ev_action_t name = \
   {                         \
      &ParamActionType,      \
      EV_Action ## name,     \
      0,                     \
      333,                   \
      #name,                 \
      0                      \
   }

#define PARAMLINELOCKED(name, lockid) \
   static ev_action_t name =          \
   {                                  \
      &ParamActionType,               \
      EV_Action ## name,              \
      EV_PARAMLOCKID,                 \
      333,                            \
      #name,                          \
      lockid                          \
   }

#define PARAMLINEFLAGGED(name, flags) \
   static ev_action_t name =          \
   {                                  \
      &ParamActionType,               \
      EV_Action ## name,              \
      flags,                          \
      333,                            \
      #name,                          \
      0                               \
   }

PARAMLINE(ParamDoorRaise);
PARAMLINE(ParamDoorOpen);
PARAMLINE(ParamDoorClose);
PARAMLINE(ParamDoorCloseWaitOpen);
PARAMLINE(ParamDoorWaitRaise);
PARAMLINE(ParamDoorWaitClose);
PARAMLINELOCKED(ParamDoorLockedRaise, 3);
PARAMLINE(ParamFloorRaiseToHighest);
PARAMLINE(ParamEEFloorLowerToHighest); 
PARAMLINE(ParamFloorLowerToHighest);
PARAMLINE(ParamFloorRaiseToLowest);
PARAMLINE(ParamFloorLowerToLowest);
PARAMLINE(ParamFloorRaiseToNearest);
PARAMLINE(ParamFloorLowerToNearest);
PARAMLINE(ParamFloorRaiseToLowestCeiling);
PARAMLINE(ParamFloorLowerToLowestCeiling);
PARAMLINE(ParamFloorRaiseToCeiling);
PARAMLINE(ParamFloorRaiseByTexture);
PARAMLINE(ParamFloorLowerByTexture);
PARAMLINE(ParamFloorRaiseByValue);
PARAMLINE(ParamFloorRaiseByValueTimes8);
PARAMLINE(ParamFloorLowerByValue);
PARAMLINE(ParamFloorLowerByValueTimes8);
PARAMLINE(ParamFloorMoveToValue);
PARAMLINE(ParamFloorMoveToValueTimes8);
PARAMLINE(ParamFloorRaiseInstant);
PARAMLINE(ParamFloorLowerInstant);
PARAMLINE(ParamFloorToCeilingInstant);
PARAMLINE(ParamFloorRaiseAndCrush);
PARAMLINE(ParamFloorCrushStop);
PARAMLINE(ParamFloorCeilingLowerByValue);
PARAMLINE(ParamFloorCeilingRaiseByValue);
PARAMLINE(ParamFloorGeneric);
PARAMLINE(ParamFloorCeilingLowerRaise);
PARAMLINE(ParamCeilingRaiseToHighest);
PARAMLINE(ParamCeilingToHighestInstant);
PARAMLINE(ParamCeilingRaiseToNearest);
PARAMLINE(ParamCeilingLowerToNearest);
PARAMLINE(ParamCeilingRaiseToLowest);
PARAMLINE(ParamCeilingLowerToLowest);
PARAMLINE(ParamCeilingRaiseToHighestFloor);
PARAMLINE(ParamCeilingLowerToHighestFloor);
PARAMLINE(ParamCeilingToFloorInstant);
PARAMLINE(ParamCeilingLowerToFloor);
PARAMLINE(ParamCeilingRaiseByTexture);
PARAMLINE(ParamCeilingLowerByTexture);
PARAMLINE(ParamCeilingRaiseByValue);
PARAMLINE(ParamCeilingLowerByValue);
PARAMLINE(ParamCeilingRaiseByValueTimes8);
PARAMLINE(ParamCeilingLowerByValueTimes8);
PARAMLINE(ParamCeilingMoveToValue);
PARAMLINE(ParamCeilingMoveToValueTimes8);
PARAMLINE(ParamCeilingRaiseInstant);
PARAMLINE(ParamCeilingLowerInstant);
PARAMLINE(ParamCeilingCrushAndRaise);
PARAMLINE(ParamCeilingCrushAndRaiseA);
PARAMLINE(ParamCeilingCrushAndRaiseSilentA);
PARAMLINE(ParamCeilingCrushAndRaiseDist);
PARAMLINE(ParamCeilingCrushAndRaiseSilentDist);
PARAMLINE(ParamCeilingCrushStop);
PARAMLINE(ParamCeilingCrushRaiseAndStay);
PARAMLINE(ParamCeilingCrushRaiseAndStayA);
PARAMLINE(ParamCeilingCrushRaiseAndStaySilA);
PARAMLINE(ParamCeilingLowerAndCrush);
PARAMLINE(ParamCeilingLowerAndCrushDist);
PARAMLINE(ParamCeilingGeneric);
PARAMLINE(ParamGenCrusher);
PARAMLINE(ParamStairsBuildUpDoom);
PARAMLINE(ParamStairsBuildUpDoomCrush);
PARAMLINE(ParamStairsBuildDownDoom);
PARAMLINE(ParamStairsBuildUpDoomSync);
PARAMLINE(ParamStairsBuildDownDoomSync);
PARAMLINE(ParamGenStairs);
PARAMLINE(PolyobjDoorSlide);
PARAMLINE(PolyobjDoorSwing);
PARAMLINE(PolyobjMove);
PARAMLINE(PolyobjMoveTimes8);
PARAMLINE(PolyobjMoveTo);
PARAMLINE(PolyobjMoveToSpot);
PARAMLINE(PolyobjORMove);
PARAMLINE(PolyobjORMoveTimes8);
PARAMLINE(PolyobjORMoveTo);
PARAMLINE(PolyobjORMoveToSpot);
PARAMLINE(PolyobjRotateRight);
PARAMLINE(PolyobjORRotateRight);
PARAMLINE(PolyobjRotateLeft);
PARAMLINE(PolyobjORRotateLeft);
PARAMLINE(PolyobjStop);
PARAMLINE(PillarBuild);
PARAMLINE(PillarBuildAndCrush);
PARAMLINE(PillarOpen);
PARAMLINE(ACSExecute);
PARAMLINE(ACSExecuteAlways);
PARAMLINE(ACSSuspend);
PARAMLINE(ACSTerminate);
PARAMLINE(ACSExecuteWithResult);
PARAMLINELOCKED(ACSLockedExecute, 4);
PARAMLINELOCKED(ACSLockedExecuteDoor, 4);
PARAMLINE(ParamLightRaiseByValue);
PARAMLINE(ParamLightLowerByValue);
PARAMLINE(ParamLightChangeToValue);
PARAMLINE(ParamLightFade);
PARAMLINE(ParamLightGlow);
PARAMLINE(ParamLightFlicker);
PARAMLINE(ParamLightStrobe);
PARAMLINE(ParamLightStrobeDoom);
PARAMLINE(RadiusQuake);
PARAMLINE(CeilingWaggle);
PARAMLINE(FloorWaggle);
PARAMLINE(ThingSpawn);
PARAMLINE(ThingSpawnNoFog);
PARAMLINEFLAGGED(TeleportNewMap, EV_ISMAPPEDEXIT);
PARAMLINEFLAGGED(TeleportEndGame, EV_ISMAPPEDEXIT);
PARAMLINE(ThingChangeTID);
PARAMLINE(ThingProjectile);
PARAMLINE(ThingProjectileGravity);
PARAMLINE(ThingActivate);
PARAMLINE(ThingDeactivate);
PARAMLINE(ThingRaise);
PARAMLINE(ThingStop);
PARAMLINE(ThrustThing);
PARAMLINE(ThrustThingZ);
PARAMLINE(DamageThing);
PARAMLINE(DamageThingEx);  // Thing_Damage essentially
PARAMLINE(ThingDestroy);
PARAMLINE(ThingRemove);
PARAMLINE(ParamPlatPerpetualRaise);
PARAMLINE(ParamPlatPerpetualRaiseLip);
PARAMLINE(ParamPlatStop);
PARAMLINE(ParamPlatDWUS);
PARAMLINE(ParamPlatDWUSLip);
PARAMLINE(ParamPlatDownByValue);
PARAMLINE(ParamPlatUWDS);
PARAMLINE(ParamPlatUpByValue);
PARAMLINE(ParamPlatRaiseNearestChange);
PARAMLINE(ParamPlatRaiseChange);
PARAMLINE(ParamPlatToggleCeiling);
PARAMLINE(ParamPlatGeneric);
PARAMLINE(ParamDonut);
PARAMLINEFLAGGED(ParamTeleport, EV_ISTELEPORTER);
PARAMLINE(ParamTeleportNoFog);
PARAMLINE(ParamTeleportLine);
PARAMLINEFLAGGED(ParamExitNormal, EV_ISMAPPEDEXIT);
PARAMLINEFLAGGED(ParamExitSecret, EV_ISMAPPEDEXIT);
PARAMLINE(ParamElevatorUp);
PARAMLINE(ParamElevatorDown);
PARAMLINE(ParamElevatorCurrent);
PARAMLINE(LightTurnOn);
PARAMLINE(ChangeSkill);
PARAMLINE(ChangeOnly);
PARAMLINE(ChangeOnlyNumeric);
PARAMLINE(HealThing);
PARAMLINE(ParamSectorSetRotation);
PARAMLINE(ParamSectorSetFloorPanning);
PARAMLINE(ParamSectorSetCeilingPanning);
PARAMLINE(ParamSectorChangeSound);
PARAMLINE(TurnTagLightsOff);

PARAMLINE(ACSSetFriction);
PARAMLINE(ACSScrollFloor);
PARAMLINE(ACSScrollCeiling);

//=============================================================================
//
// Special Bindings
//
// Each level type (DOOM, Heretic, Strife, Hexen) has its own set of line 
// specials. Heretic and Strife's sets are based on DOOM's with certain 
// additions (many of which conflict with BOOM extensions).
//

#define LINESPEC(number, action)            { number, &action, nullptr },
#define LINESPECNAMED(number, action, name) { number, &action, name    },

// DOOM Bindings
ev_binding_t DOOMBindings[] =
{
   LINESPEC(  1, DRRaiseDoor)
   LINESPEC(  2, W1OpenDoor)
   LINESPEC(  3, W1CloseDoor)
   LINESPEC(  4, W1RaiseDoor)
   LINESPEC(  5, W1RaiseFloor)
   LINESPEC(  6, W1FastCeilCrushRaise)
   LINESPEC(  7, S1BuildStairsUp8)
   LINESPEC(  8, W1BuildStairsUp8)
   LINESPEC(  9, S1DoDonut)
   LINESPEC( 10, W1PlatDownWaitUpStay)
   LINESPEC( 11, S1ExitLevel)
   LINESPEC( 12, W1LightTurnOn)
   LINESPEC( 13, W1LightTurnOn255)
   LINESPEC( 14, S1PlatRaise32Change)
   LINESPEC( 15, S1PlatRaise24Change)
   LINESPEC( 16, W1CloseDoor30)
   LINESPEC( 17, W1StartLightStrobing)
   LINESPEC( 18, S1FloorRaiseToNearest)
   LINESPEC( 19, W1LowerFloor)
   LINESPEC( 20, S1PlatRaiseNearestChange)
   LINESPEC( 21, S1PlatDownWaitUpStay)
   LINESPEC( 22, W1PlatRaiseNearestChange)
   LINESPEC( 23, S1FloorLowerToLowest)
   LINESPEC( 24, G1RaiseFloor)
   LINESPEC( 25, W1CeilingCrushAndRaise)
   LINESPEC( 26, DRRaiseDoorBlue)
   LINESPEC( 27, DRRaiseDoorYellow)
   LINESPEC( 28, DRRaiseDoorRed)
   LINESPEC( 29, S1RaiseDoor)
   LINESPEC( 30, W1FloorRaiseToTexture)
   LINESPEC( 31, D1OpenDoor)
   LINESPEC( 32, D1OpenDoorBlue)
   LINESPEC( 33, D1OpenDoorRed)
   LINESPEC( 34, D1OpenDoorYellow)
   LINESPEC( 35, W1LightsVeryDark)
   LINESPEC( 36, W1LowerFloorTurbo)
   LINESPEC( 37, W1FloorLowerAndChange)
   LINESPEC( 38, W1FloorLowerToLowest)
   LINESPEC( 39, W1Teleport)
   LINESPEC( 40, W1RaiseCeilingLowerFloor)
   LINESPEC( 41, S1CeilingLowerToFloor)
   LINESPEC( 42, SRCloseDoor)
   LINESPEC( 43, SRCeilingLowerToFloor)
   LINESPEC( 44, W1CeilingLowerAndCrush)
   LINESPEC( 45, SRLowerFloor)
   LINESPEC( 46, GROpenDoor)
   LINESPEC( 47, G1PlatRaiseNearestChange)
   LINESPEC( 49, S1CeilingCrushAndRaise)
   LINESPEC( 50, S1CloseDoor)
   LINESPEC( 51, S1SecretExit)
   LINESPEC( 52, WRExitLevel)
   LINESPEC( 53, W1PlatPerpetualRaise)
   LINESPEC( 54, W1PlatStop)
   LINESPEC( 55, S1FloorRaiseCrush)
   LINESPEC( 56, W1FloorRaiseCrush)
   LINESPEC( 57, W1CeilingCrushStop)
   LINESPEC( 58, W1RaiseFloor24)
   LINESPEC( 59, W1RaiseFloor24Change)
   LINESPEC( 60, SRFloorLowerToLowest)
   LINESPEC( 61, SROpenDoor)
   LINESPEC( 62, SRPlatDownWaitUpStay)
   LINESPEC( 63, SRRaiseDoor)
   LINESPEC( 64, SRRaiseFloor)
   LINESPEC( 65, SRFloorRaiseCrush)
   LINESPEC( 66, SRPlatRaise24Change)
   LINESPEC( 67, SRPlatRaise32Change)
   LINESPEC( 68, SRPlatRaiseNearestChange)
   LINESPEC( 69, SRFloorRaiseToNearest)
   LINESPEC( 70, SRLowerFloorTurbo)
   LINESPEC( 71, S1LowerFloorTurbo)
   LINESPEC( 72, WRCeilingLowerAndCrush)
   LINESPEC( 73, WRCeilingCrushAndRaise)
   LINESPEC( 74, WRCeilingCrushStop)
   LINESPEC( 75, WRCloseDoor)
   LINESPEC( 76, WRCloseDoor30)
   LINESPEC( 77, WRFastCeilCrushRaise)
   LINESPEC( 78, SRChangeOnlyNumeric)
   LINESPEC( 79, WRLightsVeryDark)
   LINESPEC( 80, WRLightTurnOn)
   LINESPEC( 81, WRLightTurnOn255)
   LINESPEC( 82, WRFloorLowerToLowest)
   LINESPEC( 83, WRLowerFloor)
   LINESPEC( 84, WRFloorLowerAndChange)
   LINESPEC( 86, WROpenDoor)
   LINESPEC( 87, WRPlatPerpetualRaise)
   LINESPEC( 88, WRPlatDownWaitUpStay)
   LINESPEC( 89, WRPlatStop)
   LINESPEC( 90, WRRaiseDoor)
   LINESPEC( 91, WRRaiseFloor)
   LINESPEC( 92, WRRaiseFloor24)
   LINESPEC( 93, WRRaiseFloor24Change)
   LINESPEC( 94, WRFloorRaiseCrush)
   LINESPEC( 95, WRPlatRaiseNearestChange)
   LINESPEC( 96, WRFloorRaiseToTexture)
   LINESPEC( 97, WRTeleport)
   LINESPEC( 98, WRLowerFloorTurbo)
   LINESPEC( 99, SRDoorBlazeOpenBlue)
   LINESPEC(100, W1BuildStairsTurbo16)
   LINESPEC(101, S1RaiseFloor)
   LINESPEC(102, S1LowerFloor)
   LINESPEC(103, S1OpenDoor)
   LINESPEC(104, W1TurnTagLightsOff)
   LINESPEC(105, WRDoorBlazeRaise)
   LINESPEC(106, WRDoorBlazeOpen)
   LINESPEC(107, WRDoorBlazeClose)
   LINESPEC(108, W1DoorBlazeRaise)
   LINESPEC(109, W1DoorBlazeOpen)
   LINESPEC(110, W1DoorBlazeClose)
   LINESPEC(111, S1DoorBlazeRaise)
   LINESPEC(112, S1DoorBlazeOpen)
   LINESPEC(113, S1DoorBlazeClose)
   LINESPEC(114, SRDoorBlazeRaise)
   LINESPEC(115, SRDoorBlazeOpen)
   LINESPEC(116, SRDoorBlazeClose)
   LINESPEC(117, DRDoorBlazeRaise)
   LINESPEC(118, D1DoorBlazeOpen)
   LINESPEC(119, W1FloorRaiseToNearest)
   LINESPEC(120, WRPlatBlazeDWUS)
   LINESPEC(121, W1PlatBlazeDWUS)
   LINESPEC(122, S1PlatBlazeDWUS)
   LINESPEC(123, SRPlatBlazeDWUS)
   LINESPEC(124, WRSecretExit)
   LINESPEC(125, W1TeleportMonsters)
   LINESPEC(126, WRTeleportMonsters)
   LINESPEC(127, S1BuildStairsTurbo16)
   LINESPEC(128, WRFloorRaiseToNearest)
   LINESPEC(129, WRRaiseFloorTurbo)
   LINESPEC(130, W1RaiseFloorTurbo)
   LINESPEC(131, S1RaiseFloorTurbo)
   LINESPEC(132, SRRaiseFloorTurbo)
   LINESPEC(133, S1DoorBlazeOpenBlue)
   LINESPEC(134, SRDoorBlazeOpenRed)
   LINESPEC(135, S1DoorBlazeOpenRed)
   LINESPEC(136, SRDoorBlazeOpenYellow)
   LINESPEC(137, S1DoorBlazeOpenYellow)
   LINESPEC(138, SRLightTurnOn255)
   LINESPEC(139, SRLightsVeryDark)
   LINESPEC(140, S1RaiseFloor512)
   LINESPEC(141, W1SilentCrushAndRaise)
   LINESPEC(142, W1RaiseFloor512)
   LINESPEC(143, W1PlatRaise24Change)
   LINESPEC(144, W1PlatRaise32Change)
   LINESPEC(145, W1CeilingLowerToFloor)
   LINESPEC(146, W1DoDonut)
   LINESPEC(147, WRRaiseFloor512)
   LINESPEC(148, WRPlatRaise24Change)
   LINESPEC(149, WRPlatRaise32Change)
   LINESPEC(150, WRSilentCrushAndRaise)
   LINESPEC(151, WRRaiseCeilingLowerFloor)
   LINESPEC(152, WRCeilingLowerToFloor)
   LINESPEC(153, W1ChangeOnly)
   LINESPEC(154, WRChangeOnly)
   LINESPEC(155, WRDoDonut)
   LINESPEC(156, WRStartLightStrobing)
   LINESPEC(157, WRTurnTagLightsOff)
   LINESPEC(158, S1FloorRaiseToTexture)
   LINESPEC(159, S1FloorLowerAndChange)
   LINESPEC(160, S1RaiseFloor24Change)
   LINESPEC(161, S1RaiseFloor24)
   LINESPEC(162, S1PlatPerpetualRaise)
   LINESPEC(163, S1PlatStop)
   LINESPEC(164, S1FastCeilCrushRaise)
   LINESPEC(165, S1SilentCrushAndRaise)
   LINESPEC(166, S1BOOMRaiseCeilingOrLowerFloor)
   LINESPEC(167, S1CeilingLowerAndCrush)
   LINESPEC(168, S1CeilingCrushStop)
   LINESPEC(169, S1LightTurnOn)
   LINESPEC(170, S1LightsVeryDark)
   LINESPEC(171, S1LightTurnOn255)
   LINESPEC(172, S1StartLightStrobing)
   LINESPEC(173, S1TurnTagLightsOff)
   LINESPEC(174, S1Teleport)
   LINESPEC(175, S1CloseDoor30)
   LINESPEC(176, SRFloorRaiseToTexture)
   LINESPEC(177, SRFloorLowerAndChange)
   LINESPEC(178, SRRaiseFloor512)
   LINESPEC(179, SRRaiseFloor24Change)
   LINESPEC(180, SRRaiseFloor24)
   LINESPEC(181, SRPlatPerpetualRaise)
   LINESPEC(182, SRPlatStop)
   LINESPEC(183, SRFastCeilCrushRaise)
   LINESPEC(184, SRCeilingCrushAndRaise)
   LINESPEC(185, SRSilentCrushAndRaise)
   LINESPEC(186, SRBOOMRaiseCeilingOrLowerFloor)
   LINESPEC(187, SRCeilingLowerAndCrush)
   LINESPEC(188, SRCeilingCrushStop)
   LINESPEC(189, S1ChangeOnly)
   LINESPEC(190, SRChangeOnly)
   LINESPEC(191, SRDoDonut)
   LINESPEC(192, SRLightTurnOn)
   LINESPEC(193, SRStartLightStrobing)
   LINESPEC(194, SRTurnTagLightsOff)
   LINESPEC(195, SRTeleport)
   LINESPEC(196, SRCloseDoor30)
   LINESPEC(197, G1ExitLevel)
   LINESPEC(198, G1SecretExit)
   LINESPEC(199, W1CeilingLowerToLowest)
   LINESPEC(200, W1CeilingLowerToMaxFloor)
   LINESPEC(201, WRCeilingLowerToLowest)
   LINESPEC(202, WRCeilingLowerToMaxFloor)
   LINESPEC(203, S1CeilingLowerToLowest)
   LINESPEC(204, S1CeilingLowerToMaxFloor)
   LINESPEC(205, SRCeilingLowerToLowest)
   LINESPEC(206, SRCeilingLowerToMaxFloor)
   LINESPEC(207, W1SilentTeleport)
   LINESPEC(208, WRSilentTeleport)
   LINESPEC(209, S1SilentTeleport)
   LINESPEC(210, SRSilentTeleport)
   LINESPEC(211, SRPlatToggleUpDown)
   LINESPEC(212, WRPlatToggleUpDown)
   LINESPEC(219, W1FloorLowerToNearest)
   LINESPEC(220, WRFloorLowerToNearest)
   LINESPEC(221, S1FloorLowerToNearest)
   LINESPEC(222, SRFloorLowerToNearest)
   LINESPEC(227, W1ElevatorUp)
   LINESPEC(228, WRElevatorUp)
   LINESPEC(229, S1ElevatorUp)
   LINESPEC(230, SRElevatorUp)
   LINESPEC(231, W1ElevatorDown)
   LINESPEC(232, WRElevatorDown)
   LINESPEC(233, S1ElevatorDown)
   LINESPEC(234, SRElevatorDown)
   LINESPEC(235, W1ElevatorCurrent)
   LINESPEC(236, WRElevatorCurrent)
   LINESPEC(237, S1ElevatorCurrent)
   LINESPEC(238, SRElevatorCurrent)
   LINESPEC(239, W1ChangeOnlyNumeric)
   LINESPEC(240, WRChangeOnlyNumeric)
   LINESPEC(241, S1ChangeOnlyNumeric)
   LINESPEC(243, W1SilentLineTeleport)
   LINESPEC(244, WRSilentLineTeleport)
   LINESPEC(256, WRBuildStairsUp8)
   LINESPEC(257, WRBuildStairsTurbo16)
   LINESPEC(258, SRBuildStairsUp8)
   LINESPEC(259, SRBuildStairsTurbo16)
   LINESPEC(262, W1SilentLineTeleportReverse)
   LINESPEC(263, WRSilentLineTeleportReverse)
   LINESPEC(264, W1SilentLineTRMonsters)
   LINESPEC(265, WRSilentLineTRMonsters)
   LINESPEC(266, W1SilentLineTeleMonsters)
   LINESPEC(267, WRSilentLineTeleMonsters)
   LINESPEC(268, W1SilentTeleportMonsters)
   LINESPEC(269, WRSilentTeleportMonsters)
   LINESPEC(273, WRStartLineScript1S)
   LINESPEC(274, W1StartLineScript)
   LINESPEC(275, W1StartLineScript1S)
   LINESPEC(276, SRStartLineScript)
   LINESPEC(277, S1StartLineScript)
   LINESPEC(278, GRStartLineScript)
   LINESPEC(279, G1StartLineScript)
   LINESPEC(280, WRStartLineScript)

   LINESPECNAMED(300, ParamDoorRaise,                      "Door_Raise")
   LINESPECNAMED(301, ParamDoorOpen,                       "Door_Open")
   LINESPECNAMED(302, ParamDoorClose,                      "Door_Close")
   LINESPECNAMED(303, ParamDoorCloseWaitOpen,              "Door_CloseWaitOpen")
   LINESPECNAMED(304, ParamDoorWaitRaise,                  "Door_WaitRaise")
   LINESPECNAMED(305, ParamDoorWaitClose,                  "Door_WaitClose")
   LINESPECNAMED(306, ParamFloorRaiseToHighest,            "Floor_RaiseToHighest")
   LINESPECNAMED(307, ParamEEFloorLowerToHighest,          "Floor_LowerToHighestEE")
   LINESPECNAMED(308, ParamFloorRaiseToLowest,             "Floor_RaiseToLowest")
   LINESPECNAMED(309, ParamFloorLowerToLowest,             "Floor_LowerToLowest")
   LINESPECNAMED(310, ParamFloorRaiseToNearest,            "Floor_RaiseToNearest")
   LINESPECNAMED(311, ParamFloorLowerToNearest,            "Floor_LowerToNearest")
   LINESPECNAMED(312, ParamFloorRaiseToLowestCeiling,      "Floor_RaiseToLowestCeiling")
   LINESPECNAMED(313, ParamFloorLowerToLowestCeiling,      "Floor_LowerToLowestCeiling")
   LINESPECNAMED(314, ParamFloorRaiseToCeiling,            "Floor_RaiseToCeiling")
   LINESPECNAMED(315, ParamFloorRaiseByTexture,            "Floor_RaiseByTexture")
   LINESPECNAMED(316, ParamFloorLowerByTexture,            "Floor_LowerByTexture")
   LINESPECNAMED(317, ParamFloorRaiseByValue,              "Floor_RaiseByValue")
   LINESPECNAMED(318, ParamFloorLowerByValue,              "Floor_LowerByValue")
   LINESPECNAMED(319, ParamFloorMoveToValue,               "Floor_MoveToValue")
   LINESPECNAMED(320, ParamFloorRaiseInstant,              "Floor_RaiseInstant")
   LINESPECNAMED(321, ParamFloorLowerInstant,              "Floor_LowerInstant")
   LINESPECNAMED(322, ParamFloorToCeilingInstant,          "Floor_ToCeilingInstant")
   LINESPECNAMED(323, ParamCeilingRaiseToHighest,          "Ceiling_RaiseToHighest")
   LINESPECNAMED(324, ParamCeilingToHighestInstant,        "Ceiling_ToHighestInstant")
   LINESPECNAMED(325, ParamCeilingRaiseToNearest,          "Ceiling_RaiseToNearest")
   LINESPECNAMED(326, ParamCeilingLowerToNearest,          "Ceiling_LowerToNearest")
   LINESPECNAMED(327, ParamCeilingRaiseToLowest,           "Ceiling_RaiseToLowest")
   LINESPECNAMED(328, ParamCeilingLowerToLowest,           "Ceiling_LowerToLowest")
   LINESPECNAMED(329, ParamCeilingRaiseToHighestFloor,     "Ceiling_RaiseToHighestFloor")
   LINESPECNAMED(330, ParamCeilingLowerToHighestFloor,     "Ceiling_LowerToHighestFloor")
   LINESPECNAMED(331, ParamCeilingToFloorInstant,          "Ceiling_ToFloorInstant")
   LINESPECNAMED(332, ParamCeilingLowerToFloor,            "Ceiling_LowerToFloor")
   LINESPECNAMED(333, ParamCeilingRaiseByTexture,          "Ceiling_RaiseByTexture")
   LINESPECNAMED(334, ParamCeilingLowerByTexture,          "Ceiling_LowerByTexture")
   LINESPECNAMED(335, ParamCeilingRaiseByValue,            "Ceiling_RaiseByValue")
   LINESPECNAMED(336, ParamCeilingLowerByValue,            "Ceiling_LowerByValue")
   LINESPECNAMED(337, ParamCeilingMoveToValue,             "Ceiling_MoveToValue")
   LINESPECNAMED(338, ParamCeilingRaiseInstant,            "Ceiling_RaiseInstant")
   LINESPECNAMED(339, ParamCeilingLowerInstant,            "Ceiling_LowerInstant")
   LINESPECNAMED(340, ParamStairsBuildUpDoom,              "Stairs_BuildUpDoom")
   LINESPECNAMED(341, ParamStairsBuildDownDoom,            "Stairs_BuildDownDoom")
   LINESPECNAMED(342, ParamStairsBuildUpDoomSync,          "Stairs_BuildUpDoomSync")
   LINESPECNAMED(343, ParamStairsBuildDownDoomSync,        "Stairs_BuildDownDoomSync")
   LINESPECNAMED(350, PolyobjDoorSlide,                    "Polyobj_DoorSlide")
   LINESPECNAMED(351, PolyobjDoorSwing,                    "Polyobj_DoorSwing")
   LINESPECNAMED(352, PolyobjMove,                         "Polyobj_Move")
   LINESPECNAMED(353, PolyobjORMove,                       "Polyobj_OR_Move")
   LINESPECNAMED(354, PolyobjRotateRight,                  "Polyobj_RotateRight")
   LINESPECNAMED(355, PolyobjORRotateRight,                "Polyobj_OR_RotateRight")
   LINESPECNAMED(356, PolyobjRotateLeft,                   "Polyobj_RotateLeft")
   LINESPECNAMED(357, PolyobjORRotateLeft,                 "Polyobj_OR_RotateLeft")
   LINESPECNAMED(362, PillarBuild,                         "Pillar_Build")
   LINESPECNAMED(363, PillarBuildAndCrush,                 "Pillar_BuildAndCrush")
   LINESPECNAMED(364, PillarOpen,                          "Pillar_Open")
   LINESPECNAMED(365, ACSExecute,                          "ACS_Execute")
   LINESPECNAMED(366, ACSSuspend,                          "ACS_Suspend")
   LINESPECNAMED(367, ACSTerminate,                        "ACS_Terminate")
   LINESPECNAMED(368, ParamLightRaiseByValue,              "Light_RaiseByValue")
   LINESPECNAMED(369, ParamLightLowerByValue,              "Light_LowerByValue")
   LINESPECNAMED(370, ParamLightChangeToValue,             "Light_ChangeToValue")
   LINESPECNAMED(371, ParamLightFade,                      "Light_Fade")
   LINESPECNAMED(372, ParamLightGlow,                      "Light_Glow")
   LINESPECNAMED(373, ParamLightFlicker,                   "Light_Flicker")
   LINESPECNAMED(374, ParamLightStrobe,                    "Light_Strobe")
   LINESPECNAMED(375, RadiusQuake,                         "Radius_Quake")
   LINESPECNAMED(397, FloorWaggle,                         "Floor_Waggle")
   LINESPECNAMED(398, ThingSpawn,                          "Thing_Spawn")
   LINESPECNAMED(399, ThingSpawnNoFog,                     "Thing_SpawnNoFog")
   LINESPECNAMED(400, TeleportEndGame,                     "Teleport_EndGame")
   LINESPECNAMED(402, ThingProjectile,                     "Thing_Projectile")
   LINESPECNAMED(403, ThingProjectileGravity,              "Thing_ProjectileGravity")
   LINESPECNAMED(404, ThingActivate,                       "Thing_Activate")
   LINESPECNAMED(405, ThingDeactivate,                     "Thing_Deactivate")
   LINESPECNAMED(410, ParamPlatPerpetualRaise,             "Plat_PerpetualRaise")
   LINESPECNAMED(411, ParamPlatStop,                       "Plat_Stop")
   LINESPECNAMED(412, ParamPlatDWUS,                       "Plat_DownWaitUpStay")
   LINESPECNAMED(413, ParamPlatDownByValue,                "Plat_DownByValue")
   LINESPECNAMED(414, ParamPlatUWDS,                       "Plat_UpWaitDownStay")
   LINESPECNAMED(415, ParamPlatUpByValue,                  "Plat_UpByValue")
   LINESPECNAMED(416, ParamFloorLowerToHighest,            "Floor_LowerToHighest")
   LINESPECNAMED(420, ACSExecuteWithResult,                "ACS_ExecuteWithResult")
   LINESPECNAMED(421, ThingChangeTID,                      "Thing_ChangeTID")
   LINESPECNAMED(422, ThingRaise,                          "Thing_Raise")
   LINESPECNAMED(423, ThingStop,                           "Thing_Stop")
   LINESPECNAMED(424, ThrustThing,                         "ThrustThing")
   LINESPECNAMED(425, ThrustThingZ,                        "ThrustThingZ")
   LINESPECNAMED(426, DamageThing,                         "DamageThing")
   LINESPECNAMED(427, DamageThingEx,                       "Thing_Damage")
   LINESPECNAMED(428, ThingDestroy,                        "Thing_Destroy")
   LINESPECNAMED(429, ParamDoorLockedRaise,                "Door_LockedRaise")
   LINESPECNAMED(430, ACSLockedExecute,                    "ACS_LockedExecute")
   LINESPECNAMED(431, ParamDonut,                          "Floor_Donut")
   LINESPECNAMED(432, ParamCeilingCrushAndRaise,           "Ceiling_CrushAndRaise")
   LINESPECNAMED(433, ParamCeilingCrushStop,               "Ceiling_CrushStop")
   LINESPECNAMED(434, ParamCeilingCrushRaiseAndStay,       "Ceiling_CrushRaiseAndStay")
   LINESPECNAMED(435, ParamCeilingLowerAndCrush,           "Ceiling_LowerAndCrush")
   LINESPECNAMED(436, ParamCeilingLowerAndCrushDist,       "Ceiling_LowerAndCrushDist")
   LINESPECNAMED(437, ParamCeilingCrushAndRaiseDist,       "Ceiling_CrushAndRaiseDist")
   LINESPECNAMED(438, ParamCeilingCrushRaiseAndStayA,      "Ceiling_CrushRaiseAndStayA")
   LINESPECNAMED(439, ParamCeilingCrushAndRaiseA,          "Ceiling_CrushAndRaiseA")
   LINESPECNAMED(440, ParamCeilingCrushAndRaiseSilentA,    "Ceiling_CrushAndRaiseSilentA")
   LINESPECNAMED(441, ParamCeilingCrushAndRaiseSilentDist, "Ceiling_CrushAndRaiseSilentDist")
   LINESPECNAMED(442, ParamCeilingCrushRaiseAndStaySilA,   "Ceiling_CrushRaiseAndStaySilA")
   LINESPECNAMED(443, ParamGenCrusher,                     "Generic_Crusher")
   LINESPECNAMED(444, ParamTeleport,                       "Teleport")
   LINESPECNAMED(445, ParamTeleportNoFog,                  "Teleport_NoFog")
   LINESPECNAMED(446, ParamTeleportLine,                   "Teleport_Line")
   LINESPECNAMED(447, ParamExitNormal,                     "Exit_Normal")
   LINESPECNAMED(448, ParamExitSecret,                     "Exit_Secret")
   LINESPECNAMED(449, TeleportNewMap,                      "Teleport_NewMap")
   LINESPECNAMED(451, ParamFloorRaiseAndCrush,             "Floor_RaiseAndCrush")
   LINESPECNAMED(452, ParamFloorCrushStop,                 "Floor_CrushStop")
   LINESPECNAMED(453, ParamFloorCeilingLowerByValue,       "FloorAndCeiling_LowerByValue")
   LINESPECNAMED(454, ParamFloorCeilingRaiseByValue,       "FloorAndCeiling_RaiseByValue")
   LINESPECNAMED(458, ParamElevatorUp,                     "Elevator_RaiseToNearest")
   LINESPECNAMED(459, ParamElevatorDown,                   "Elevator_LowerToNearest")
   LINESPECNAMED(460, ParamElevatorCurrent,                "Elevator_MoveToFloor")
   LINESPECNAMED(461, LightTurnOn,                         "Light_MaxNeighbor")
   LINESPECNAMED(462, ChangeSkill,                         "ChangeSkill")
   LINESPECNAMED(463, ParamLightStrobeDoom,                "Light_StrobeDoom")
   LINESPECNAMED(464, ParamFloorGeneric,                   "Generic_Floor")
   LINESPECNAMED(465, ParamCeilingGeneric,                 "Generic_Ceiling")
   LINESPECNAMED(466, ChangeOnly,                          "Floor_TransferTrigger")
   LINESPECNAMED(467, ChangeOnlyNumeric,                   "Floor_TransferNumeric")
   LINESPECNAMED(468, ParamFloorCeilingLowerRaise,         "FloorAndCeiling_LowerRaise")
   LINESPECNAMED(469, HealThing,                           "HealThing")
   LINESPECNAMED(470, ParamSectorSetRotation,              "Sector_SetRotation")
   LINESPECNAMED(471, ParamSectorSetFloorPanning,          "Sector_SetFloorPanning")
   LINESPECNAMED(472, ParamSectorSetCeilingPanning,        "Sector_SetCeilingPanning")
   LINESPECNAMED(473, TurnTagLightsOff,                    "Light_MinNeighbor")
   LINESPECNAMED(474, PolyobjStop,                         "Polyobj_Stop")
   LINESPECNAMED(475, ParamPlatRaiseNearestChange,         "Plat_RaiseAndStayTx0")
   LINESPECNAMED(476, ParamPlatRaiseChange,                "Plat_UpByValueStayTx")
   LINESPECNAMED(477, ACSExecuteAlways,                    "ACS_ExecuteAlways")
   LINESPECNAMED(478, ThingRemove,                         "Thing_Remove")
   LINESPECNAMED(487, ParamPlatToggleCeiling,              "Plat_ToggleCeiling")
   LINESPECNAMED(488, ParamPlatDWUSLip,                    "Plat_DownWaitUpStayLip")
   LINESPECNAMED(489, ParamPlatPerpetualRaiseLip,          "Plat_PerpetualRaiseLip")
   LINESPECNAMED(490, ACSLockedExecuteDoor,                "ACS_LockedExecuteDoor")
   LINESPECNAMED(494, ParamStairsBuildUpDoomCrush,         "Stairs_BuildUpDoomCrush")
   LINESPECNAMED(495, ParamSectorChangeSound,              "Sector_ChangeSound")
   LINESPECNAMED(496, PolyobjMoveToSpot,                   "Polyobj_MoveToSpot")
   LINESPECNAMED(497, PolyobjMoveTo,                       "Polyobj_MoveTo")
   LINESPECNAMED(498, PolyobjORMoveTo,                     "Polyobj_OR_MoveTo")
   LINESPECNAMED(499, PolyobjORMoveToSpot,                 "Polyobj_OR_MoveToSpot")
   LINESPECNAMED(500, CeilingWaggle,                       "Ceiling_Waggle")
   LINESPECNAMED(501, ParamPlatGeneric,                    "Generic_Lift")
   LINESPECNAMED(502, ParamGenStairs,                      "Generic_Stairs")
};

const size_t DOOMBindingsLen = earrlen(DOOMBindings);

// DOOM Lockdef ID Lookup (Doubles for Heretic)
ev_lockdef_t DOOMLockDefs[] =
{
   {  26, EV_LOCKDEF_BLUE     },
   {  27, EV_LOCKDEF_YELLOW   },
   {  28, EV_LOCKDEF_REDGREEN },
   {  32, EV_LOCKDEF_BLUE     },
   {  33, EV_LOCKDEF_REDGREEN },
   {  34, EV_LOCKDEF_YELLOW   },
   {  99, EV_LOCKDEF_BLUE     },
   { 133, EV_LOCKDEF_BLUE     }, 
   { 134, EV_LOCKDEF_REDGREEN },
   { 135, EV_LOCKDEF_REDGREEN },
   { 136, EV_LOCKDEF_YELLOW   },
   { 137, EV_LOCKDEF_YELLOW   },
};

const size_t DOOMLockDefsLen = earrlen(DOOMLockDefs);

// Heretic Lockdef ID Lookup
// All this does is remap line type 99 to lockdef 0, as linetype
// 99 is not a locked door in Heretic. All other bindings will defer
// to the DOOM lookup.
ev_lockdef_t HereticLockDefs[] =
{
   { 99, EV_LOCKDEF_NULL },
};

const size_t HereticLockDefsLen = earrlen(HereticLockDefs);

// Heretic Bindings
// Heretic's bindings are additive over DOOM's.
// Special thanks to Gez for saving me a ton of trouble by reporting missing
// differences in behavior for types 7, 8, 10, 36, 49, 70, 71, 88, and 98
ev_binding_t HereticBindings[] =
{
   LINESPEC(  7, S1HereticStairsBuildUp8FS)
   LINESPEC(  8, W1HereticStairsBuildUp8FS)
   LINESPEC( 10, W1HticPlatDownWaitUpStay)
   LINESPEC( 36, W1LowerFloorTurboA)
   LINESPEC( 49, S1CeilingLowerAndCrush)
   LINESPEC( 70, SRLowerFloorTurboA)
   LINESPEC( 71, S1LowerFloorTurboA)
   LINESPEC( 88, WRHticPlatDownWaitUpStay)
   LINESPEC( 98, WRLowerFloorTurboA)
   LINESPEC( 99, NullAction)                 // Hide BOOM type 99
   LINESPEC(100, WRHereticDoorRaise3x)
   LINESPEC(105, WRSecretExit)
   LINESPEC(106, W1HereticStairsBuildUp16FS)
   LINESPEC(107, S1HereticStairsBuildUp16FS)
};

const size_t HereticBindingsLen = earrlen(HereticBindings);

// Hexen Bindings
ev_binding_t HexenBindings[] =
{
   LINESPECNAMED(2,   PolyobjRotateLeft,                   "Polyobj_RotateLeft")
   LINESPECNAMED(3,   PolyobjRotateRight,                  "Polyobj_RotateRight")
   LINESPECNAMED(4,   PolyobjMove,                         "Polyobj_Move")
   LINESPECNAMED(6,   PolyobjMoveTimes8,                   "Polyobj_MoveTimes8")
   LINESPECNAMED(7,   PolyobjDoorSwing,                    "Polyobj_DoorSwing")
   LINESPECNAMED(8,   PolyobjDoorSlide,                    "Polyobj_DoorSlide")
   LINESPECNAMED(10,  ParamDoorClose,                      "Door_Close")
   LINESPECNAMED(11,  ParamDoorOpen,                       "Door_Open")
   LINESPECNAMED(12,  ParamDoorRaise,                      "Door_Raise")
   LINESPECNAMED(13,  ParamDoorLockedRaise,                "Door_LockedRaise")
   LINESPECNAMED(17,  ThingRaise,                          "Thing_Raise")
   LINESPECNAMED(19,  ThingStop,                           "Thing_Stop")
   LINESPECNAMED(20,  ParamFloorLowerByValue,              "Floor_LowerByValue")
   LINESPECNAMED(21,  ParamFloorLowerToLowest,             "Floor_LowerToLowest")
   LINESPECNAMED(22,  ParamFloorLowerToNearest,            "Floor_LowerToNearest")
   LINESPECNAMED(23,  ParamFloorRaiseByValue,              "Floor_RaiseByValue")
   LINESPECNAMED(24,  ParamFloorRaiseToHighest,            "Floor_RaiseToHighest")
   LINESPECNAMED(25,  ParamFloorRaiseToNearest,            "Floor_RaiseToNearest")
   LINESPECNAMED(28,  ParamFloorRaiseAndCrush,             "Floor_RaiseAndCrush")
   LINESPECNAMED(29,  PillarBuild,                         "Pillar_Build")
   LINESPECNAMED(30,  PillarOpen,                          "Pillar_Open")
   LINESPECNAMED(35,  ParamFloorRaiseByValueTimes8,        "Floor_RaiseByValueTimes8")
   LINESPECNAMED(36,  ParamFloorLowerByValueTimes8,        "Floor_LowerByValueTimes8")
   LINESPECNAMED(37,  ParamFloorMoveToValue,               "Floor_MoveToValue")
   LINESPECNAMED(38,  CeilingWaggle,                       "Ceiling_Waggle")
   LINESPECNAMED(40,  ParamCeilingLowerByValue,            "Ceiling_LowerByValue")
   LINESPECNAMED(41,  ParamCeilingRaiseByValue,            "Ceiling_RaiseByValue")
   LINESPECNAMED(42,  ParamCeilingCrushAndRaise,           "Ceiling_CrushAndRaise")
   LINESPECNAMED(43,  ParamCeilingLowerAndCrush,           "Ceiling_LowerAndCrush")
   LINESPECNAMED(44,  ParamCeilingCrushStop,               "Ceiling_CrushStop")
   LINESPECNAMED(45,  ParamCeilingCrushRaiseAndStay,       "Ceiling_CrushRaiseAndStay")
   LINESPECNAMED(46,  ParamFloorCrushStop,                 "Floor_CrushStop")
   LINESPECNAMED(47,  ParamCeilingMoveToValue,             "Ceiling_MoveToValue")
   LINESPECNAMED(59,  PolyobjORMoveToSpot,                 "Polyobj_OR_MoveToSpot")
   LINESPECNAMED(60,  ParamPlatPerpetualRaise,             "Plat_PerpetualRaise")
   LINESPECNAMED(61,  ParamPlatStop,                       "Plat_Stop")
   LINESPECNAMED(62,  ParamPlatDWUS,                       "Plat_DownWaitUpStay")
   LINESPECNAMED(63,  ParamPlatDownByValue,                "Plat_DownByValue")
   LINESPECNAMED(64,  ParamPlatUWDS,                       "Plat_UpWaitDownStay")
   LINESPECNAMED(65,  ParamPlatUpByValue,                  "Plat_UpByValue")
   LINESPECNAMED(66,  ParamFloorLowerInstant,              "Floor_LowerInstant")
   LINESPECNAMED(67,  ParamFloorRaiseInstant,              "Floor_RaiseInstant")
   LINESPECNAMED(68,  ParamFloorMoveToValueTimes8,         "Floor_MoveToValueTimes8")
   LINESPECNAMED(69,  ParamCeilingMoveToValueTimes8,       "Ceiling_MoveToValueTimes8")
   LINESPECNAMED(70,  ParamTeleport,                       "Teleport")
   LINESPECNAMED(71,  ParamTeleportNoFog,                  "Teleport_NoFog")
   LINESPECNAMED(72,  ThrustThing,                         "ThrustThing")
   LINESPECNAMED(73,  DamageThing,                         "DamageThing")
   LINESPECNAMED(74,  TeleportNewMap,                      "Teleport_NewMap")
   LINESPECNAMED(75,  TeleportEndGame,                     "Teleport_EndGame")
   LINESPECNAMED(80,  ACSExecute,                          "ACS_Execute")
   LINESPECNAMED(81,  ACSSuspend,                          "ACS_Suspend")
   LINESPECNAMED(82,  ACSTerminate,                        "ACS_Terminate")
   LINESPECNAMED(83,  ACSLockedExecute,                    "ACS_LockedExecute")
   LINESPECNAMED(84,  ACSExecuteWithResult,                "ACS_ExecuteWithResult")
   LINESPECNAMED(85,  ACSLockedExecuteDoor,                "ACS_LockedExecuteDoor")
   LINESPECNAMED(86,  PolyobjMoveToSpot,                   "Polyobj_MoveToSpot")
   LINESPECNAMED(87,  PolyobjStop,                         "Polyobj_Stop")
   LINESPECNAMED(88,  PolyobjMoveTo,                       "Polyobj_MoveTo")
   LINESPECNAMED(89,  PolyobjORMoveTo,                     "Polyobj_OR_MoveTo")
   LINESPECNAMED(90,  PolyobjORRotateLeft,                 "Polyobj_OR_RotateLeft")
   LINESPECNAMED(91,  PolyobjORRotateRight,                "Polyobj_OR_RotateRight")
   LINESPECNAMED(92,  PolyobjORMove,                       "Polyobj_OR_Move")
   LINESPECNAMED(93,  PolyobjORMoveTimes8,                 "Polyobj_OR_MoveTimes8")
   LINESPECNAMED(94,  PillarBuildAndCrush,                 "Pillar_BuildAndCrush")
   LINESPECNAMED(95,  ParamFloorCeilingLowerByValue,       "FloorAndCeiling_LowerByValue")
   LINESPECNAMED(96,  ParamFloorCeilingRaiseByValue,       "FloorAndCeiling_RaiseByValue")
   LINESPECNAMED(97,  ParamCeilingLowerAndCrushDist,       "Ceiling_LowerAndCrushDist")
   LINESPECNAMED(104, ParamCeilingCrushAndRaiseSilentDist, "Ceiling_CrushAndRaiseSilentDist")
   LINESPECNAMED(105, ParamDoorWaitRaise,                  "Door_WaitRaise")
   LINESPECNAMED(106, ParamDoorWaitClose,                  "Door_WaitClose")
   LINESPECNAMED(110, ParamLightRaiseByValue,              "Light_RaiseByValue")
   LINESPECNAMED(111, ParamLightLowerByValue,              "Light_LowerByValue")
   LINESPECNAMED(112, ParamLightChangeToValue,             "Light_ChangeToValue")
   LINESPECNAMED(113, ParamLightFade,                      "Light_Fade")
   LINESPECNAMED(114, ParamLightGlow,                      "Light_Glow")
   LINESPECNAMED(115, ParamLightFlicker,                   "Light_Flicker")
   LINESPECNAMED(116, ParamLightStrobe,                    "Light_Strobe")
   LINESPECNAMED(119, DamageThingEx,                       "Thing_Damage")
   LINESPECNAMED(120, RadiusQuake,                         "Radius_Quake")
   LINESPECNAMED(128, ThrustThingZ,                        "ThrustThingZ")
   LINESPECNAMED(130, ThingActivate,                       "Thing_Activate")
   LINESPECNAMED(131, ThingDeactivate,                     "Thing_Deactivate")
   LINESPECNAMED(132, ThingRemove,                         "Thing_Remove")
   LINESPECNAMED(133, ThingDestroy,                        "Thing_Destroy")
   LINESPECNAMED(134, ThingProjectile,                     "Thing_Projectile")
   LINESPECNAMED(135, ThingSpawn,                          "Thing_Spawn")
   LINESPECNAMED(136, ThingProjectileGravity,              "Thing_ProjectileGravity")
   LINESPECNAMED(137, ThingSpawnNoFog,                     "Thing_SpawnNoFog")
   LINESPECNAMED(138, FloorWaggle,                         "Floor_Waggle")
   LINESPECNAMED(140, ParamSectorChangeSound,              "Sector_ChangeSound")
   LINESPECNAMED(168, ParamCeilingCrushAndRaiseDist,       "Ceiling_CrushAndRaiseDist")
   LINESPECNAMED(176, ThingChangeTID,                      "Thing_ChangeTID")
   LINESPECNAMED(179, ChangeSkill,                         "ChangeSkill")
   LINESPECNAMED(185, ParamSectorSetRotation,              "Sector_SetRotation")
   LINESPECNAMED(186, ParamSectorSetCeilingPanning,        "Sector_SetCeilingPanning")
   LINESPECNAMED(187, ParamSectorSetFloorPanning,          "Sector_SetFloorPanning")
   LINESPECNAMED(192, ParamCeilingLowerToHighestFloor,     "Ceiling_LowerToHighestFloor")
   LINESPECNAMED(193, ParamCeilingLowerInstant,            "Ceiling_LowerInstant")
   LINESPECNAMED(194, ParamCeilingRaiseInstant,            "Ceiling_RaiseInstant")
   LINESPECNAMED(195, ParamCeilingCrushRaiseAndStayA,      "Ceiling_CrushRaiseAndStayA")
   LINESPECNAMED(196, ParamCeilingCrushAndRaiseA,          "Ceiling_CrushAndRaiseA")
   LINESPECNAMED(197, ParamCeilingCrushAndRaiseSilentA,    "Ceiling_CrushAndRaiseSilentA")
   LINESPECNAMED(198, ParamCeilingRaiseByValueTimes8,      "Ceiling_RaiseByValueTimes8")
   LINESPECNAMED(199, ParamCeilingLowerByValueTimes8,      "Ceiling_LowerByValueTimes8")
   LINESPECNAMED(200, ParamFloorGeneric,                   "Generic_Floor")
   LINESPECNAMED(201, ParamCeilingGeneric,                 "Generic_Ceiling")
   LINESPECNAMED(203, ParamPlatGeneric,                    "Generic_Lift")
   LINESPECNAMED(204, ParamGenStairs,                      "Generic_Stairs")
   LINESPECNAMED(205, ParamGenCrusher,                     "Generic_Crusher")
   LINESPECNAMED(206, ParamPlatDWUSLip,                    "Plat_DownWaitUpStayLip")
   LINESPECNAMED(207, ParamPlatPerpetualRaiseLip,          "Plat_PerpetualRaiseLip")
   LINESPECNAMED(215, ParamTeleportLine,                   "Teleport_Line")
   LINESPECNAMED(217, ParamStairsBuildUpDoom,              "Stairs_BuildUpDoom")
   LINESPECNAMED(226, ACSExecuteAlways,                    "ACS_ExecuteAlways")
   LINESPECNAMED(228, ParamPlatRaiseNearestChange,         "Plat_RaiseAndStayTx0")
   LINESPECNAMED(230, ParamPlatRaiseChange,                "Plat_UpByValueStayTx")
   LINESPECNAMED(231, ParamPlatToggleCeiling,              "Plat_ToggleCeiling")
   LINESPECNAMED(232, ParamLightStrobeDoom,                "Light_StrobeDoom")
   LINESPECNAMED(233, TurnTagLightsOff,                    "Light_MinNeighbor")
   LINESPECNAMED(234, LightTurnOn,                         "Light_MaxNeighbor")
   LINESPECNAMED(235, ChangeOnly,                          "Floor_TransferTrigger")
   LINESPECNAMED(236, ChangeOnlyNumeric,                   "Floor_TransferNumeric")
   LINESPECNAMED(238, ParamFloorRaiseToLowestCeiling,      "Floor_RaiseToLowestCeiling")
   LINESPECNAMED(240, ParamFloorRaiseByTexture,            "Floor_RaiseByTexture")
   LINESPECNAMED(242, ParamFloorLowerToHighest,            "Floor_LowerToHighest")
   LINESPECNAMED(243, ParamExitNormal,                     "Exit_Normal")
   LINESPECNAMED(244, ParamExitSecret,                     "Exit_Secret")
   LINESPECNAMED(245, ParamElevatorUp,                     "Elevator_RaiseToNearest")
   LINESPECNAMED(246, ParamElevatorCurrent,                "Elevator_MoveToFloor")
   LINESPECNAMED(247, ParamElevatorDown,                   "Elevator_LowerToNearest")
   LINESPECNAMED(248, HealThing,                           "HealThing")
   LINESPECNAMED(249, ParamDoorCloseWaitOpen,              "Door_CloseWaitOpen")
   LINESPECNAMED(250, ParamDonut,                          "Floor_Donut")
   LINESPECNAMED(251, ParamFloorCeilingLowerRaise,         "FloorAndCeiling_LowerRaise")
   LINESPECNAMED(252, ParamCeilingRaiseToNearest,          "Ceiling_RaiseToNearest")
   LINESPECNAMED(253, ParamCeilingLowerToLowest,           "Ceiling_LowerToLowest")
   LINESPECNAMED(254, ParamCeilingLowerToFloor,            "Ceiling_LowerToFloor")
   LINESPECNAMED(255, ParamCeilingCrushRaiseAndStaySilA,   "Ceiling_CrushRaiseAndStaySilA")
};

const size_t HexenBindingsLen = earrlen(HexenBindings);

// PSX Mission Bindings
// * TODO: type 142 ("start Club DOOM music")
ev_binding_t PSXBindings[] =
{
   LINESPEC(142, NullAction) // TODO
};

const size_t PSXBindingsLen = earrlen(PSXBindings);

// UDMF "Eternity" Namespace Bindings
ev_binding_t UDMFEternityBindings[] =
{
   // No bindings for ExtraData as it isn't required any more \o/
   LINESPECNAMED(256, ParamEEFloorLowerToHighest,      "Floor_LowerToHighestEE")
   LINESPECNAMED(257, ParamFloorRaiseToLowest,         "Floor_RaiseToLowest")
   LINESPECNAMED(258, ParamFloorLowerToLowestCeiling,  "Floor_LowerToLowestCeiling")
   LINESPECNAMED(259, ParamFloorRaiseToCeiling,        "Floor_RaiseToCeiling")
   LINESPECNAMED(260, ParamFloorToCeilingInstant,      "Floor_ToCeilingInstant")
   LINESPECNAMED(261, ParamFloorLowerByTexture,        "Floor_LowerByTexture")
   LINESPECNAMED(262, ParamCeilingRaiseToHighest,      "Ceiling_RaiseToHighest")
   LINESPECNAMED(263, ParamCeilingToHighestInstant,    "Ceiling_ToHighestInstant")
   LINESPECNAMED(264, ParamCeilingLowerToNearest,      "Ceiling_LowerToNearest")
   LINESPECNAMED(265, ParamCeilingRaiseToLowest,       "Ceiling_RaiseToLowest")
   LINESPECNAMED(266, ParamCeilingRaiseToHighestFloor, "Ceiling_RaiseToHighestFloor")
   LINESPECNAMED(267, ParamCeilingToFloorInstant,      "Ceiling_ToFloorInstant")
   LINESPECNAMED(268, ParamCeilingRaiseByTexture,      "Ceiling_RaiseByTexture")
   LINESPECNAMED(269, ParamCeilingLowerByTexture,      "Ceiling_LowerByTexture")
   LINESPECNAMED(270, ParamStairsBuildDownDoom,        "Stairs_BuildDownDoom")
   LINESPECNAMED(271, ParamStairsBuildUpDoomSync,      "Stairs_BuildUpDoomSync")
   LINESPECNAMED(272, ParamStairsBuildDownDoomSync,    "Stairs_BuildDownDoomSync")
   LINESPECNAMED(273, ParamStairsBuildUpDoomCrush,     "Stairs_BuildUpDoomCrush")
};

const size_t UDMFEternityBindingsLen = earrlen(UDMFEternityBindings);

// ACS-specific bindings, for functions that are static but need to be
// accessible by ACS, or that have multiple definitions, one for lines, one for ACS
ev_binding_t ACSBindings[] =
{
   LINESPEC(219, ACSSetFriction)
   LINESPEC(223, ACSScrollFloor)
   LINESPEC(224, ACSScrollCeiling)
};

const size_t ACSBindingsLen = earrlen(ACSBindings);

//=============================================================================
//
// UDMF-to-ExtraData Reverse Special Lookups
//
// For ACS we sometimes need to walk backwards from a UDMF-compatible special
// number to a special number in the DOOM/ExtraData binding set.
//
//=============================================================================

//
// Initialize the UDMF-to-ExtraData binding pointers
//
void EV_InitUDMFToExtraDataLookup()
{
   static bool isInit = false;

   if(isInit)
      return;
   isInit = true;

   // do the Hexen binding table first
   for(size_t i = 0; i < HexenBindingsLen; i++)
   {
      ev_binding_t &hexenBind = HexenBindings[i];

      // try to find the same action in the DOOM bindings table
      for(size_t j = 0; j < DOOMBindingsLen; j++)
      {
         ev_binding_t &doomBind = DOOMBindings[j];

         if(doomBind.action == hexenBind.action)
         {
            hexenBind.pEDBinding = &doomBind;
            break;
         }
      }
   }

   // also do the UDMF table
   for(size_t i = 0; i < UDMFEternityBindingsLen; i++)
   {
      ev_binding_t &udmfBind = UDMFEternityBindings[i];

      for(size_t j = 0; j < DOOMBindingsLen; j++)
      {
         ev_binding_t &doomBind = DOOMBindings[j];

         if(doomBind.action == udmfBind.action)
         {
            udmfBind.pEDBinding = &doomBind;
            break;
         }
      }
   }
}



// EOF

