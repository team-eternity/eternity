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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Generalized line action system - Actions
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "a_small.h"
#include "acs_intr.h"
#include "d_gi.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "ev_macros.h"
#include "ev_specials.h"
#include "g_game.h"
#include "p_info.h"
#include "p_skin.h"
#include "p_spec.h"
#include "p_xenemy.h"
#include "polyobj.h"
#include "r_defs.h"
#include "s_sound.h"

#define INIT_STRUCT edefstructvar

//=============================================================================
// 
// Utilities
//

//
// EV_floorChangeForArg
//
// Gets sector property change type data for a parameterized floor type
// based on one of the instance arguments.
//
static void EV_floorChangeForArg(floordata_t &fd, int arg)
{
   static int fchgdata[7][2] =
   {
      //   model       change type
      { FTriggerModel, FNoChg   }, // no change
      { FTriggerModel, FChgZero }, // trigger, zero special
      { FNumericModel, FChgZero }, // numeric, zero special
      { FTriggerModel, FChgTxt  }, // trigger, change texture
      { FNumericModel, FChgTxt  }, // numeric, change texture
      { FTriggerModel, FChgTyp  }, // trigger, change type
      { FNumericModel, FChgTyp  }, // numeric, change type
   };

   if(arg < 0 || arg >= 7)
      arg = 0;

   fd.change_model = fchgdata[arg][0];
   fd.change_type  = fchgdata[arg][1];
}

//
// EV_ceilingChangeForArg
//
// Gets sector property change type data for a parameterized ceiling type
// based on one of the instance arguments.
//
static void EV_ceilingChangeForArg(ceilingdata_t &cd, int arg)
{
   static int cchgdata[7][2] =
   {
      //   model       change type
      { CTriggerModel, CNoChg },
      { CTriggerModel, CChgZero },
      { CNumericModel, CChgZero },
      { CTriggerModel, CChgTxt  },
      { CNumericModel, CChgTxt  },
      { CTriggerModel, CChgTyp  },
      { CNumericModel, CChgTyp  },
   };

   if(arg < 0 || arg >= 7)
      arg = 0;

   cd.change_model = cchgdata[arg][0];
   cd.change_type  = cchgdata[arg][1];
}

//=============================================================================
//
// Action Routines
//

#define DEFINE_ACTION(name) \
   int name (ev_action_t *action, ev_instance_t *instance)

//
// EV_ActionNull
//
// This is a non-action, used to override inherited bindings.
//
DEFINE_ACTION(EV_ActionNull)
{
   return false;
}

//
// DOOM and Shared Actions
//

//
// EV_ActionOpenDoor
//
DEFINE_ACTION(EV_ActionOpenDoor)
{
   // case   2: (W1)
   // case  46: (GR)
   // case  61: (SR)
   // case  86: (WR)
   // case 103: (S1)
   // Open Door
   return EV_DoDoor(instance->line, doorOpen);
}

//
// EV_ActionCloseDoor
//
DEFINE_ACTION(EV_ActionCloseDoor)
{
   // case 3:  (W1)
   // case 42: (SR)
   // case 50: (S1)
   // case 75: (WR)
   // Close Door
   return EV_DoDoor(instance->line, doorClose);
}

//
// EV_ActionRaiseDoor
//
DEFINE_ACTION(EV_ActionRaiseDoor)
{
   // case  4: (W1)
   // case 29: (S1)
   // case 63: (SR)
   // case 90: (WR)
   // Raise Door
   return EV_DoDoor(instance->line, doorNormal);
}

//
// EV_ActionRaiseFloor
//
DEFINE_ACTION(EV_ActionRaiseFloor)
{
   // case   5: (W1)
   // case  24: (G1)
   // case  64: (SR)
   // case  91: (WR)
   // case 101: (S1)
   // Raise Floor
   return EV_DoFloor(instance->line, raiseFloor);
}

//
// EV_ActionFastCeilCrushRaise
//
DEFINE_ACTION(EV_ActionFastCeilCrushRaise)
{
   // case   6: (W1)
   // case  77: (WR)
   // case 164: (S1 - BOOM Extended)
   // case 183: (SR - BOOM Extended)
   // Fast Ceiling Crush & Raise
   return EV_DoCeiling(instance->line, fastCrushAndRaise);
}

//
// EV_ActionBuildStairsUp8
//
DEFINE_ACTION(EV_ActionBuildStairsUp8)
{
   // case 7:   (S1)
   // case 8:   (W1)
   // case 256: (WR - BOOM Extended)
   // case 258: (SR - BOOM Extended)
   // Build Stairs
   return EV_BuildStairs(instance->line, build8);
}

//
// EV_ActionPlatDownWaitUpStay
//
DEFINE_ACTION(EV_ActionPlatDownWaitUpStay)
{
   // case 10: (W1)
   // case 21: (S1)
   // case 62: (SR)
   // case 88: (WR)
   // PlatDownWaitUp
   return EV_DoPlat(instance->line, downWaitUpStay, 0);
}

//
// EV_ActionLightTurnOn
//
DEFINE_ACTION(EV_ActionLightTurnOn)
{
   // case  12: (W1)
   // case  80: (WR)
   // case 169: (S1 - BOOM Extended)
   // case 192: (SR - BOOM Extended)
   // Light Turn On - brightest near
   return EV_LightTurnOn(instance->line, 0);
}

//
// EV_ActionLightTurnOn255
//
DEFINE_ACTION(EV_ActionLightTurnOn255)
{
   // case  13: (W1)
   // case  81: (WR)
   // case 138: (SR)
   // Light Turn On 255
   return EV_LightTurnOn(instance->line, 255);
}

//
// EV_ActionCloseDoor30
//
DEFINE_ACTION(EV_ActionCloseDoor30)
{
   // case  16: (W1)
   // case  76: (WR)
   // case 175: (S1 - BOOM Extended)
   // case 196: (SR - BOOM Extended)
   // Close Door 30
   return EV_DoDoor(instance->line, closeThenOpen);
}

//
// EV_ActionStartLightStrobing
//
DEFINE_ACTION(EV_ActionStartLightStrobing)
{
   // case  17: (W1)
   // case 156: (WR - BOOM Extended)
   // case 172: (S1 - BOOM Extended)
   // case 193: (SR - BOOM Extended)
   // Start Light Strobing
   return EV_StartLightStrobing(instance->line);
}

//
// EV_ActionLowerFloor
//
DEFINE_ACTION(EV_ActionLowerFloor)
{
   // case  19: (W1)
   // case  45: (SR)
   // case  83: (WR)
   // case 102: (S1)
   // Lower Floor
   return EV_DoFloor(instance->line, lowerFloor);
}

//
// EV_ActionPlatRaiseNearestChange
//
DEFINE_ACTION(EV_ActionPlatRaiseNearestChange)
{
   // case 20: (S1)
   // case 22: (W1)
   // case 47: (G1)
   // case 68: (SR)
   // case 95: (WR)
   // Raise floor to nearest height and change texture
   return EV_DoPlat(instance->line, raiseToNearestAndChange, 0);
}

//
// EV_ActionCeilingCrushAndRaise
//
DEFINE_ACTION(EV_ActionCeilingCrushAndRaise)
{
   // case  25: (W1)
   // case  49: (S1)
   // case  73: (WR)
   // case 184: (SR - BOOM Extended)
   // Ceiling Crush and Raise
   return EV_DoCeiling(instance->line, crushAndRaise);
}

//
// EV_ActionFloorRaiseToTexture
//
DEFINE_ACTION(EV_ActionFloorRaiseToTexture)
{
   // case  30: (W1)
   // case  96: (WR)
   // case 158: (S1 - BOOM Extended)
   // case 176: (SR - BOOM Extended)
   // Raise floor to shortest texture height on either side of lines.
   return EV_DoFloor(instance->line, raiseToTexture);
}

//
// EV_ActionLightsVeryDark
//
DEFINE_ACTION(EV_ActionLightsVeryDark)
{
   // case  35: (W1)
   // case  79: (WR)
   // case 139: (SR)
   // case 170: (S1 - BOOM Extended)
   // Lights Very Dark
   return EV_LightTurnOn(instance->line, 35);
}

//
// EV_ActionLowerFloorTurbo
//
DEFINE_ACTION(EV_ActionLowerFloorTurbo)
{
   // case 36: (W1)
   // case 70: (SR)
   // case 71: (S1)
   // case 98: (WR)
   // Lower Floor (TURBO)
   return EV_DoFloor(instance->line, turboLower);
}

//
// EV_ActionFloorLowerAndChange
//
DEFINE_ACTION(EV_ActionFloorLowerAndChange)
{
   // case  37: (W1)
   // case  84: (WR)
   // case 159: (S1 - BOOM Extended)
   // case 177: (SR - BOOM Extended)
   // LowerAndChange
   return EV_DoFloor(instance->line, lowerAndChange);
}

//
// EV_ActionFloorLowerToLowest
//
DEFINE_ACTION(EV_ActionFloorLowerToLowest)
{
   // case 23: (S1)
   // case 38: (W1)
   // case 60: (SR)
   // case 82: (WR)
   // Lower Floor To Lowest
   return EV_DoFloor(instance->line, lowerFloorToLowest);
}

//
// EV_ActionTeleport
//
DEFINE_ACTION(EV_ActionTeleport)
{
   // case  39: (W1)
   // case  97: (WR)
   // case 174: (S1 - BOOM Extended)
   // case 195: (SR - BOOM Extended)
   // TELEPORT!
   // case 125: (W1)
   // case 126: (WR)
   // TELEPORT MonsterONLY.
   // jff 02/09/98 fix using up with wrong side crossing
   return EV_Teleport(instance->line, instance->side, instance->actor);
}

//
// EV_ActionRaiseCeilingLowerFloor
//
// haleyjd: This action never historically worked (seems Romero forgot that
// only one action could be active on a sector at a time...), so it will 
// actually only ever raise the ceiling. In demo_compatibility mode we do
// try to lower the floor, but, it won't work. This is done for strict
// compatibility in case it somehow has an unpredictable playsim side effect.
//
DEFINE_ACTION(EV_ActionRaiseCeilingLowerFloor)
{
   line_t *line = instance->line;

   // case 40: (W1)
   // RaiseCeilingLowerFloor
   if(demo_compatibility)
   {
      EV_DoCeiling(line, raiseToHighest);
      EV_DoFloor(line, lowerFloorToLowest); // jff 02/12/98 doesn't work
      line->special = 0;
      return true;
   }
   else
      return EV_DoCeiling(line, raiseToHighest);
}

//
// EV_ActionBOOMRaiseCeilingLowerFloor
//
// This version on the other hand works unconditionally, and is a BOOM
// extension.
//
DEFINE_ACTION(EV_ActionBOOMRaiseCeilingLowerFloor)
{
   // case 151: (WR - BOOM Extended)
   // RaiseCeilingLowerFloor
   // 151 WR  EV_DoCeiling(raiseToHighest),
   //         EV_DoFloor(lowerFloortoLowest)
   EV_DoCeiling(instance->line, raiseToHighest);
   EV_DoFloor(instance->line, lowerFloorToLowest);

   return true;
}

//
// EV_ActionBOOMRaiseCeilingOrLowerFloor
//
// haleyjd: This is supposed to do the same as the above, but, thanks to a
// glaring hole in the logic, it will only perform one action or the other,
// depending on whether or not the ceiling is currently busy 9_9
//
DEFINE_ACTION(EV_ActionBOOMRaiseCeilingOrLowerFloor)
{
   // case 166: (S1 - BOOM Extended)
   // case 186: (SR - BOOM Extended)
   // Raise ceiling, Lower floor
   return (EV_DoCeiling(instance->line, raiseToHighest) ||
           EV_DoFloor(instance->line, lowerFloorToLowest));
}

//
// EV_ActionCeilingLowerAndCrush
//
DEFINE_ACTION(EV_ActionCeilingLowerAndCrush)
{
   // case  44: (W1)
   // case  72: (WR)
   // case 167: (S1 - BOOM Extended)
   // case 187: (SR - BOOM Extended)
   // Ceiling Crush
   return EV_DoCeiling(instance->line, lowerAndCrush);
}

//
// EV_ActionExitLevel
//
DEFINE_ACTION(EV_ActionExitLevel)
{
   Mobj *thing   = instance->actor;
   int   destmap = 0;

   if(LevelInfo.allowExitTags)
   {
      destmap = instance->tag;
      if(!destmap)
         destmap = gamemap + 1;
   }

   // case  52: (W1)
   // EXIT!
   // killough 10/98: prevent zombies from exiting levels
   if(!(thing->player && thing->player->health <= 0 && !comp[comp_zombie]))
      G_ExitLevel(destmap);

   return true;
}

//
// EV_ActionSwitchExitLevel
//
// This version of level exit logic is used for switch-type exits.
//
DEFINE_ACTION(EV_ActionSwitchExitLevel)
{
   Mobj *thing   = instance->actor;
   int   destmap = 0;

   if(LevelInfo.allowExitTags)
   {
      destmap = instance->tag;
      if(!destmap)
         destmap = gamemap + 1;
   }

   // case 11:
   // Exit level
   // killough 10/98: prevent zombies from exiting levels
   if(thing->player && thing->player->health <= 0 && !comp[comp_zombie])
   {
      S_StartSound(thing, GameModeInfo->playerSounds[sk_oof]);
      return false;
   }

   G_ExitLevel(destmap);
   return true;
}

//
// EV_ActionGunExitLevel
//
// This version of level exit logic is used for gun-type exits.
//
DEFINE_ACTION(EV_ActionGunExitLevel)
{
   Mobj *thing   = instance->actor;
   int   destmap = 0;

   if(LevelInfo.allowExitTags)
   {
      destmap = instance->tag;
      if(!destmap)
         destmap = gamemap + 1;
   }

   // case 197: (G1 - BOOM Extended)
   if(thing->player && thing->player->health <= 0 && !comp[comp_zombie])
      return false;

   G_ExitLevel(destmap);
   return true;
}

//
// EV_ActionPlatPerpetualRaise
//
DEFINE_ACTION(EV_ActionPlatPerpetualRaise)
{
   // case  53: (W1)
   // case  87: (WR)
   // case 162: (S1 - BOOM Extended)
   // case 181: (SR - BOOM Extended)
   // Perpetual Platform Raise
   return EV_DoPlat(instance->line, perpetualRaise, 0);
}

//
// EV_ActionPlatStop
//
DEFINE_ACTION(EV_ActionPlatStop)
{
   // case  54: (W1)
   // case  89: (WR)
   // case 163: (S1 - BOOM Extended)
   // case 182: (SR - BOOM Extended)
   // Platform Stop
   return EV_StopPlat(instance->line);
}

//
// EV_ActionFloorRaiseCrush
//
DEFINE_ACTION(EV_ActionFloorRaiseCrush)
{
   // case 55: (S1)
   // case 56: (W1)
   // case 65: (SR)
   // case 94: (WR)
   // Raise Floor Crush
   return EV_DoFloor(instance->line, raiseFloorCrush);
}

//
// EV_ActionCeilingCrushStop
//
DEFINE_ACTION(EV_ActionCeilingCrushStop)
{
   // case  57: (W1)
   // case  74: (WR)
   // case 168: (S1 - BOOM Extended)
   // case 188: (SR - BOOM Extended)
   // Ceiling Crush Stop
   return EV_CeilingCrushStop(instance->line);
}

//
// EV_ActionRaiseFloor24
//
DEFINE_ACTION(EV_ActionRaiseFloor24)
{
   // case  58: (W1)
   // case  92: (WR)
   // case 161: (S1 - BOOM Extended)
   // case 180: (SR - BOOM Extended)
   // Raise Floor 24
   return EV_DoFloor(instance->line, raiseFloor24);
}

//
// EV_ActionRaiseFloor24Change
//
DEFINE_ACTION(EV_ActionRaiseFloor24Change)
{
   // case  59: (W1)
   // case  93: (WR)
   // case 160: (S1 - BOOM Extended)
   // case 179: (SR - BOOM Extended)
   // Raise Floor 24 And Change
   return EV_DoFloor(instance->line, raiseFloor24AndChange);
}

//
// EV_ActionBuildStairsTurbo16
//
DEFINE_ACTION(EV_ActionBuildStairsTurbo16)
{
   // case 100: (W1)
   // case 127: (S1)
   // case 257: (WR - BOOM Extended)
   // case 259: (SR - BOOM Extended)
   // Build Stairs Turbo 16
   return EV_BuildStairs(instance->line, turbo16);
}

//
// EV_ActionTurnTagLightsOff
//
DEFINE_ACTION(EV_ActionTurnTagLightsOff)
{
   // case 104: (W1)
   // case 157: (WR - BOOM Extended)
   // case 173: (S1 - BOOM Extended)
   // case 194: (SR - BOOM Extended)
   // Turn lights off in sector(tag)
   return EV_TurnTagLightsOff(instance->line);
}

//
// EV_ActionDoorBlazeRaise
//
DEFINE_ACTION(EV_ActionDoorBlazeRaise)
{
   // case 105: (WR)
   // case 108: (W1)
   // case 111: (S1)
   // case 114: (SR)
   // Blazing Door Raise (faster than TURBO!)
   return EV_DoDoor(instance->line, blazeRaise);
}

//
// EV_ActionDoorBlazeOpen
//
DEFINE_ACTION(EV_ActionDoorBlazeOpen)
{
   // case 106: (WR)
   // case 109: (W1)
   // case 112: (S1)
   // case 115: (SR)
   // Blazing Door Open (faster than TURBO!)
   return EV_DoDoor(instance->line, blazeOpen);
}

//
// EV_ActionDoorBlazeClose
//
DEFINE_ACTION(EV_ActionDoorBlazeClose)
{
   // case 107: (WR)
   // case 110: (W1)
   // case 113: (S1)
   // case 116: (SR)
   // Blazing Door Close (faster than TURBO!)
   return EV_DoDoor(instance->line, blazeClose);
}

//
// EV_ActionFloorRaiseToNearest
//
DEFINE_ACTION(EV_ActionFloorRaiseToNearest)
{
   // case  18: (S1)
   // case  69: (SR)
   // case 119: (W1)
   // case 128: (WR)
   // Raise floor to nearest surr. floor
   return EV_DoFloor(instance->line, raiseFloorToNearest);
}

//
// EV_ActionPlatBlazeDWUS
//
DEFINE_ACTION(EV_ActionPlatBlazeDWUS)
{
   // case 120: (WR)
   // case 121: (W1)
   // case 122: (S1)
   // case 123: (SR)
   // Blazing PlatDownWaitUpStay
   return EV_DoPlat(instance->line, blazeDWUS, 0);
}

//
// EV_ActionSecretExit
//
DEFINE_ACTION(EV_ActionSecretExit)
{
   Mobj *thing   = instance->actor;
   int   destmap = 0;

   if(LevelInfo.allowSecretTags)
   {
      destmap = instance->tag;
      if(!destmap)
         destmap = gamemap + 1;
   }

   // case 124: (W1)
   // Secret EXIT
   // killough 10/98: prevent zombies from exiting levels
   if(!(thing->player && thing->player->health <= 0 && !comp[comp_zombie]))
      G_SecretExitLevel(destmap);

   return true;
}

//
// EV_ActionSwitchSecretExit
//
// This variant of secret exit action is used by switches.
//
DEFINE_ACTION(EV_ActionSwitchSecretExit)
{
   Mobj *thing   = instance->actor;
   int   destmap = 0;

   if(LevelInfo.allowSecretTags)
   {
      destmap = instance->tag;
      if(!destmap)
         destmap = gamemap + 1;
   }

   // case 51: (S1)
   // Secret EXIT
   // killough 10/98: prevent zombies from exiting levels
   if(thing->player && thing->player->health <= 0 && !comp[comp_zombie])
   {
      S_StartSound(thing, GameModeInfo->playerSounds[sk_oof]);
      return false;
   }

   G_SecretExitLevel(destmap);
   return true;
}

//
// EV_ActionGunSecretExit
//
// This variant of secret exit action is used by impact specials.
//
DEFINE_ACTION(EV_ActionGunSecretExit)
{
   Mobj *thing   = instance->actor;
   int   destmap = 0;

   if(LevelInfo.allowSecretTags)
   {
      destmap = instance->tag;
      if(!destmap)
         destmap = gamemap + 1;
   }

   // case 198: (G1 - BOOM Extended)
   if(thing->player && thing->player->health <= 0 && !comp[comp_zombie])
      return false;

   G_SecretExitLevel(destmap);
   return true;
}

//
// EV_ActionRaiseFloorTurbo
//
DEFINE_ACTION(EV_ActionRaiseFloorTurbo)
{
   // case 129: (WR)
   // case 130: (W1)
   // case 131: (S1)
   // case 132: (SR)
   // Raise Floor Turbo
   return EV_DoFloor(instance->line, raiseFloorTurbo);
}

//
// EV_ActionSilentCrushAndRaise
//
DEFINE_ACTION(EV_ActionSilentCrushAndRaise)
{
   // case 141: (W1)
   // case 150: (WR - BOOM Extended)
   // case 165: (S1 - BOOM Extended)
   // case 185: (SR - BOOM Extended)
   // Silent Ceiling Crush & Raise
   return EV_DoCeiling(instance->line, silentCrushAndRaise);
}

//
// EV_ActionRaiseFloor512
//
DEFINE_ACTION(EV_ActionRaiseFloor512)
{
   // case 140: (S1)
   // case 142: (W1 - BOOM Extended)
   // case 147: (WR - BOOM Extended)
   // case 178: (SR - BOOM Extended)
   // Raise Floor 512
   return EV_DoFloor(instance->line, raiseFloor512);
}

//
// EV_ActionPlatRaise24Change
//
DEFINE_ACTION(EV_ActionPlatRaise24Change)
{
   // case  15: (S1)
   // case  66: (SR)
   // case 143: (W1 - BOOM Extended)
   // case 148: (WR - BOOM Extended)
   // Raise Floor 24 and change
   return EV_DoPlat(instance->line, raiseAndChange, 24);
}

//
// EV_ActionPlatRaise32Change
//
DEFINE_ACTION(EV_ActionPlatRaise32Change)
{
   // case  14: (S1)
   // case  67: (SR)
   // case 144: (W1 - BOOM Extended)
   // case 149: (WR - BOOM Extended)
   // Raise Floor 32 and change
   return EV_DoPlat(instance->line, raiseAndChange, 32);
}

//
// EV_ActionCeilingLowerToFloor
//
DEFINE_ACTION(EV_ActionCeilingLowerToFloor)
{
   // case 41:  (S1)
   // case 43:  (SR)
   // case 145: (W1 - BOOM Extended)
   // case 152: (WR - BOOM Extended)
   // Lower Ceiling to Floor
   return EV_DoCeiling(instance->line, lowerToFloor);
}

//
// EV_ActionDoDonut
//
DEFINE_ACTION(EV_ActionDoDonut)
{
   // case 9:   (S1)
   // case 146: (W1 - BOOM Extended)
   // case 155: (WR - BOOM Extended)
   // case 191: (SR - BOOM Extended)
   // Lower Pillar, Raise Donut
   return EV_DoDonut(instance->line);
}

//
// EV_ActionCeilingLowerToLowest
//
DEFINE_ACTION(EV_ActionCeilingLowerToLowest)
{
   // case 199: (W1 - BOOM Extended)
   // case 201: (WR - BOOM Extended)
   // case 203: (S1 - BOOM Extended)
   // case 205: (SR - BOOM Extended)
   // Lower ceiling to lowest surrounding ceiling
   return EV_DoCeiling(instance->line, lowerToLowest);
}

//
// EV_ActionCeilingLowerToMaxFloor
//
DEFINE_ACTION(EV_ActionCeilingLowerToMaxFloor)
{
   // case 200: (W1 - BOOM Extended)
   // case 202: (WR - BOOM Extended)
   // case 204: (S1 - BOOM Extended)
   // case 206: (SR - BOOM Extended)
   // Lower ceiling to highest surrounding floor
   return EV_DoCeiling(instance->line, lowerToMaxFloor);
}

//
// EV_ActionSilentTeleport
//
DEFINE_ACTION(EV_ActionSilentTeleport)
{
   line_t *line  = instance->line;
   int     side  = instance->side;
   Mobj   *thing = instance->actor;

   // case 207: (W1 - BOOM Extended)
   // case 208: (WR - BOOM Extended)
   // case 209: (S1 - BOOM Extended)
   // case 210: (SR - BOOM Extended)
   // killough 2/16/98: W1 silent teleporter (normal kind)
   // case 268: (W1 - BOOM Extended)
   // case 269: (WR - BOOM Extended)
   // jff 4/14/98 add monster-only silent
   return EV_SilentTeleport(line, side, thing);
}

//
// EV_ActionChangeOnly
//
DEFINE_ACTION(EV_ActionChangeOnly)
{
   // jff 3/16/98 renumber 215->153
   // jff 3/15/98 create texture change no motion type
   // case 153: (W1 - BOOM Extended)
   // case 154: (WR - BOOM Extended)
   // case 189: (S1 - BOOM Extended)
   // case 190: (SR - BOOM Extended)
   // Texture/Type Change Only (Trig)
   return EV_DoChange(instance->line, trigChangeOnly);
}

//
// EV_ActionChangeOnlyNumeric
//
DEFINE_ACTION(EV_ActionChangeOnlyNumeric)
{
   //jff 3/15/98 create texture change no motion type
   // case  78: (SR - BOOM Extended)
   // case 239: (W1 - BOOM Extended)
   // case 240: (WR - BOOM Extended)
   // case 241: (S1 - BOOM Extended)
   // Texture/Type Change Only (Numeric)
   return EV_DoChange(instance->line, numChangeOnly);
}

//
// EV_ActionFloorLowerToNearest
//
DEFINE_ACTION(EV_ActionFloorLowerToNearest)
{
   // case 219: (W1 - BOOM Extended)
   // case 220: (WR - BOOM Extended)
   // case 221: (S1 - BOOM Extended)
   // case 222: (SR - BOOM Extended)
   // Lower floor to next lower neighbor
   return EV_DoFloor(instance->line, lowerFloorToNearest);
}

//
// EV_ActionElevatorUp
//
DEFINE_ACTION(EV_ActionElevatorUp)
{
   // case 227: (W1 - BOOM Extended)
   // case 228: (WR - BOOM Extended)
   // case 229: (S1 - BOOM Extended)
   // case 230: (SR - BOOM Extended)
   // Raise elevator next floor
   return EV_DoElevator(instance->line, elevateUp);
}

//
// EV_ActionElevatorDown
//
DEFINE_ACTION(EV_ActionElevatorDown)
{
   // case 231: (W1 - BOOM Extended)
   // case 232: (WR - BOOM Extended)
   // case 233: (S1 - BOOM Extended)
   // case 234: (SR - BOOM Extended)
   // Lower elevator next floor
   return EV_DoElevator(instance->line, elevateDown);

}

//
// EV_ActionElevatorCurrent
//
DEFINE_ACTION(EV_ActionElevatorCurrent)
{
   // case 235: (W1 - BOOM Extended)
   // case 236: (WR - BOOM Extended)
   // case 237: (S1 - BOOM Extended)
   // case 238: (SR - BOOM Extended)
   // Elevator to current floor
   return EV_DoElevator(instance->line, elevateCurrent);
}

//
// EV_ActionSilentLineTeleport
//
DEFINE_ACTION(EV_ActionSilentLineTeleport)
{
   line_t *line  = instance->line;
   int     side  = instance->side;
   Mobj   *thing = instance->actor;
  
   // case 243: (W1 - BOOM Extended)
   // case 244: (WR - BOOM Extended)
   // jff 3/6/98 make fit within DCK's 256 linedef types
   // killough 2/16/98: W1 silent teleporter (linedef-linedef kind)
   // case 266: (W1 - BOOM Extended)
   // case 267: (WR - BOOM Extended)
   // jff 4/14/98 add monster-only silent line-line

   return EV_SilentLineTeleport(line, side, thing, false);
}

//
// EV_ActionSilentLineTeleportReverse
//
DEFINE_ACTION(EV_ActionSilentLineTeleportReverse)
{
   line_t *line  = instance->line;
   int     side  = instance->side;
   Mobj   *thing = instance->actor;

   // case 262: (W1 - BOOM Extended)
   // case 263: (WR - BOOM Extended)
   // jff 4/14/98 add silent line-line reversed
   // case 264: (W1 - BOOM Extended)
   // case 265: (WR - BOOM Extended)
   // jff 4/14/98 add monster-only silent line-line reversed
   return EV_SilentLineTeleport(line, side, thing, true);
}

//
// EV_ActionPlatToggleUpDown
//
DEFINE_ACTION(EV_ActionPlatToggleUpDown)
{
   // jff 3/14/98 create instant toggle floor type
   // case 211: (SR - BOOM Extended)
   // case 212: (WR - BOOM Extended)
   return EV_DoPlat(instance->line, toggleUpDn, 0);
}

//
// EV_ActionStartLineScript
//
// SMMU script instance linedefs (now adapted to work for ACS)
//
DEFINE_ACTION(EV_ActionStartLineScript)
{
   P_StartLineScript(instance->line, instance->actor);
   return true;
}

// 
// EV_ActionVerticalDoor
//
DEFINE_ACTION(EV_ActionVerticalDoor)
{
   // Manual doors, push type with no tag
   // case 1:   -- Vertical Door
   // case 26:  -- Blue Door/Locked
   // case 27:  -- Yellow Door /Locked
   // case 28:  -- Red Door /Locked
   // case 31:  -- Manual door open
   // case 32:  -- Blue locked door open
   // case 33:  -- Red locked door open
   // case 34:  -- Yellow locked door open
   // case 117: -- Blazing door raise
   // case 118: -- Blazing door open
   
   // TODO: move special-specific logic out of EV_VerticalDoor to here,
   // or to preamble function. Or, break up function altogether.

   int lockID = EV_LockDefIDForSpecial(instance->special);
   return EV_VerticalDoor(instance->line, instance->actor, lockID);
}

//
// EV_ActionDoLockedDoor
//
DEFINE_ACTION(EV_ActionDoLockedDoor)
{
   // case  99: (SR) BlzOpenDoor BLUE
   // case 133: (S1) BlzOpenDoor BLUE
   // case 134: (SR) BlzOpenDoor RED
   // case 135: (S1) BlzOpenDoor RED
   // case 136: (SR) BlzOpenDoor YELLOW
   // case 137: (S1) BlzOpenDoor YELLOW

   int lockID = EV_LockDefIDForSpecial(instance->special);
   return EV_DoLockedDoor(instance->line, blazeOpen, lockID, instance->actor);
}

//
// Heretic Actions
//
// Heretic added a few new line types, all of which it should be noted conflict
// with BOOM extended line types.
//

//
// EV_ActionLowerFloorTurboA
//
// Heretic: 36 (W1)
// Heretic: 70 (SR)
// Heretic: 71 (S1)
// Heretic: 98 (WR)
//
DEFINE_ACTION(EV_ActionLowerFloorTurboA)
{
   return EV_DoFloor(instance->line, turboLowerA);
}

//
// EV_ActionHereticDoorRaise3x
//
// Heretic: 100
// Open-wait-close type door that moves at a unique 3x normal speed. This is 
// remapped to Door_Raise.
//
DEFINE_ACTION(EV_ActionHereticDoorRaise3x)
{
   INIT_STRUCT(doordata_t, dd);

   dd.kind         = OdCDoor;
   dd.spac         = SPAC_CROSS;
   dd.speed_value  = 3 * VDOORSPEED;
   dd.topcountdown = 0;
   dd.delay_value  = VDOORWAIT;
   dd.altlighttag  = 0;
   dd.flags        = DDF_HAVESPAC | DDF_REUSABLE;
   dd.thing        = instance->actor;

   return EV_DoParamDoor(instance->line, instance->tag, &dd);
}

//
// EV_ActionHereticStairsBuildUp8FS
//
// Heretic: 7 (S1)
// Heretic: 8 (W1)
// Heretic changed all stair types from DOOM to raise at FLOORSPEED;
// Thanks to Gez for having found this a long time ago for ZDoom and
// then sharing it with me.
//
DEFINE_ACTION(EV_ActionHereticStairsBuildUp8FS)
{
   INIT_STRUCT(stairdata_t, sd);

   sd.flags          = SDF_HAVESPAC;
   sd.spac           = instance->spac;
   sd.direction      = 1;
   sd.speed_type     = SpeedParam;
   sd.speed_value    = FLOORSPEED;
   sd.stepsize_type  = StepSizeParam;
   sd.stepsize_value = 8 * FRACUNIT;

   return EV_DoParamStairs(instance->line, instance->tag, &sd);
}

//
// EV_ActionHereticStairsBuildUp16FS
//
// Heretic: 106 (W1)
// Heretic: 107 (S1)
// Stair types which build stairs of size 16 up at FLOORSPEED.
// Remapped to Stairs_BuildUpDoom.
//
DEFINE_ACTION(EV_ActionHereticStairsBuildUp16FS)
{
   INIT_STRUCT(stairdata_t, sd);

   sd.flags          = SDF_HAVESPAC;
   sd.spac           = instance->spac; 
   sd.direction      = 1;              // up
   sd.speed_type     = SpeedParam;
   sd.speed_value    = FLOORSPEED;     // speed
   sd.stepsize_type  = StepSizeParam;
   sd.stepsize_value = 16 * FRACUNIT;  // height

   return EV_DoParamStairs(instance->line, instance->tag, &sd);
}

//
// BOOM Generalized Action
//

DEFINE_ACTION(EV_ActionBoomGen)
{
   switch(instance->gentype)
   {
   case GenTypeFloor:
      return EV_DoGenFloor(instance->line);
   case GenTypeCeiling:
      return EV_DoGenCeiling(instance->line);
   case GenTypeDoor:
      return EV_DoGenDoor(instance->line, instance->actor);
   case GenTypeLocked:
      return EV_DoGenLockedDoor(instance->line, instance->actor);
   case GenTypeLift:
      return EV_DoGenLift(instance->line);
   case GenTypeStairs:
      return EV_DoGenStairs(instance->line);
   case GenTypeCrusher:
      return EV_DoGenCrusher(instance->line);
   default:
      return false;
   }
}

//
// Parameterized Actions
//
// These are used directly by the Hexen map format and from within ACS scripts,
// and are also available to DOOM-format maps through use of ExtraData.
//

//
// EV_ActionParamDoorRaise
//
// Implements Door_Raise(tag, speed, delay, lighttag)
// * ExtraData: 300
// * Hexen:     12
//
DEFINE_ACTION(EV_ActionParamDoorRaise)
{
   INIT_STRUCT(doordata_t, dd);
   int extflags = instance->line ? instance->line->extflags : EX_ML_REPEAT;

   dd.kind         = OdCDoor;
   dd.spac         = instance->spac;
   dd.speed_value  = instance->args[1] * FRACUNIT / 8;
   dd.topcountdown = 0;
   dd.delay_value  = instance->args[2];
   dd.altlighttag  = instance->args[3];
   dd.thing        = instance->actor;
   
   dd.flags = DDF_HAVESPAC | DDF_USEALTLIGHTTAG;
   if(extflags & EX_ML_REPEAT)
      dd.flags |= DDF_REUSABLE;

   return EV_DoParamDoor(instance->line, instance->tag, &dd);
}

//
// EV_ActionParamDoorOpen
//
// Implements Door_Open(tag, speed, lighttag)
// * ExtraData: 301
// * Hexen:     11
//
DEFINE_ACTION(EV_ActionParamDoorOpen)
{
   INIT_STRUCT(doordata_t, dd);
   int extflags = instance->line ? instance->line->extflags : EX_ML_REPEAT;

   dd.kind         = ODoor;
   dd.spac         = instance->spac;
   dd.speed_value  = instance->args[1] * FRACUNIT / 8;
   dd.topcountdown = 0;
   dd.delay_value  = 0;
   dd.altlighttag  = instance->args[2];
   dd.thing        = instance->actor;

   dd.flags = DDF_HAVESPAC | DDF_USEALTLIGHTTAG;
   if(extflags & EX_ML_REPEAT)
      dd.flags |= DDF_REUSABLE;

   return EV_DoParamDoor(instance->line, instance->tag, &dd);
}

//
// EV_ActionParamDoorClose
//
// Implements Door_Close(tag, speed, lighttag)
// * ExtraData: 302
// * Hexen:     10
//
DEFINE_ACTION(EV_ActionParamDoorClose)
{
   INIT_STRUCT(doordata_t, dd);
   int extflags = instance->line ? instance->line->extflags : EX_ML_REPEAT;

   dd.kind         = CDoor;
   dd.spac         = instance->spac;
   dd.speed_value  = instance->args[1] * FRACUNIT / 8;
   dd.topcountdown = 0;
   dd.delay_value  = 0;
   dd.altlighttag  = instance->args[2];
   dd.thing        = instance->actor;

   dd.flags = DDF_HAVESPAC | DDF_USEALTLIGHTTAG;
   if(extflags & EX_ML_REPEAT)
      dd.flags |= DDF_REUSABLE;

   return EV_DoParamDoor(instance->line, instance->tag, &dd);
}

//
// EV_ActionParamDoorCloseWaitOpen
//
// Implements Door_CloseWaitOpen(tag, speed, delay, lighttag)
// * ExtraData: 303
// * Hexen (ZDoom Extension): 249
//
DEFINE_ACTION(EV_ActionParamDoorCloseWaitOpen)
{
   INIT_STRUCT(doordata_t, dd);
   int extflags = instance->line ? instance->line->extflags : EX_ML_REPEAT;

   dd.kind         = CdODoor;
   dd.spac         = instance->spac;
   dd.speed_value  = instance->args[1] * FRACUNIT / 8;
   dd.topcountdown = 0;
   dd.delay_value  = instance->args[2];
   dd.altlighttag  = instance->args[3];
   dd.thing        = instance->actor;

   dd.flags = DDF_HAVESPAC | DDF_USEALTLIGHTTAG;
   if(extflags & EX_ML_REPEAT)
      dd.flags |= DDF_REUSABLE;

   return EV_DoParamDoor(instance->line, instance->tag, &dd);
}

//
// EV_ActionParamDoorWaitRaise
//
// Implements Door_WaitRaise(tag, speed, delay, topcount, lighttag)
// * ExtraData: 304
//
DEFINE_ACTION(EV_ActionParamDoorWaitRaise)
{
   INIT_STRUCT(doordata_t, dd);
   int extflags = instance->line ? instance->line->extflags : EX_ML_REPEAT;

   dd.kind         = pDOdCDoor;
   dd.spac         = instance->spac;
   dd.speed_value  = instance->args[1] * FRACUNIT / 8;
   dd.delay_value  = instance->args[2];
   dd.topcountdown = instance->args[3];
   dd.altlighttag  = instance->args[4];
   dd.thing        = instance->actor;

   dd.flags = DDF_HAVESPAC | DDF_USEALTLIGHTTAG;
   if(extflags & EX_ML_REPEAT)
      dd.flags |= DDF_REUSABLE;

   return EV_DoParamDoor(instance->line, instance->tag, &dd);
}

//
// EV_ActionParamDoorWaitClose
//
// Implements Door_WaitClose(tag, speed, topcount, lighttag)
// * ExtraData: 305
//
DEFINE_ACTION(EV_ActionParamDoorWaitClose)
{
   INIT_STRUCT(doordata_t, dd);
   int extflags = instance->line ? instance->line->extflags : EX_ML_REPEAT;

   dd.kind         = pDCDoor;
   dd.spac         = instance->spac;
   dd.speed_value  = instance->args[1] * FRACUNIT / 8;
   dd.delay_value  = 0;
   dd.topcountdown = instance->args[2];
   dd.altlighttag  = instance->args[3];
   dd.thing        = instance->actor;

   dd.flags = DDF_HAVESPAC | DDF_USEALTLIGHTTAG;
   if(extflags & EX_ML_REPEAT)
      dd.flags |= DDF_REUSABLE;

   return EV_DoParamDoor(instance->line, instance->tag, &dd);
}

//
// EV_ActionParamFloorRaiseToHighest
//
// Implements Floor_RaiseToHighest(tag, speed, change, crush)
// * ExtraData: 306
// * Hexen:     24
//
DEFINE_ACTION(EV_ActionParamFloorRaiseToHighest)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction   = 1;              // up
   fd.target_type = FtoHnF;         // to highest neighboring floor
   fd.spac        = instance->spac; // activated Hexen-style
   fd.flags       = FDF_HAVESPAC;
   fd.speed_type  = SpeedParam;
   fd.speed_value = instance->args[1] * FRACUNIT / 8; // speed
   EV_floorChangeForArg(fd, instance->args[2]);       // change
   fd.crush       = instance->args[3];                // crush
   
   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamEEFloorLowerToHighest
//
// Implements Floor_LowerToHighestEE(tag, speed, change)
// * ExtraData: 307
//
// NB: Not ZDoom-compatible with special of same name
//
DEFINE_ACTION(EV_ActionParamEEFloorLowerToHighest)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction   = 0;              // down
   fd.target_type = FtoHnF;         // to highest neighboring floor
   fd.spac        = instance->spac; // activated Hexen-style
   fd.flags       = FDF_HAVESPAC;
   fd.speed_type  = SpeedParam;
   fd.speed_value = instance->args[1] * FRACUNIT / 8; // speed
   EV_floorChangeForArg(fd, instance->args[2]);       // change
   fd.crush       = -1;
   
   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorLowerToHighest
//
// Implements Floor_LowerToHighest(tag, speed, adjust, force_adjust)
// * ExtraData: 416
// * Hexen (ZDoom Extension): 242
//
DEFINE_ACTION(EV_ActionParamFloorLowerToHighest)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction = 0;
   fd.target_type = FtoHnF;
   fd.spac = instance->spac;
   fd.flags = FDF_HAVESPAC | FDF_HACKFORDESTHNF;
   fd.speed_type = SpeedParam;
   fd.speed_value = instance->args[1] * FRACUNIT / 8; // speed
   fd.crush = -1;
   fd.adjust = instance->args[2];
   fd.force_adjust = instance->args[3];

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorRaiseToLowest
//
// Implements Floor_RaiseToLowest(tag, change, crush)
// * ExtraData: 308
//
DEFINE_ACTION(EV_ActionParamFloorRaiseToLowest)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction   = 1;              // up
   fd.target_type = FtoLnF;         // to lowest neighboring floor
   fd.spac        = instance->spac; // activated Hexen-style
   fd.flags       = FDF_HAVESPAC;
   fd.speed_type  = SpeedNormal;    // target is lower, so motion is instant.
   EV_floorChangeForArg(fd, instance->args[1]); // change
   fd.crush       = instance->args[2];          // crush
   
   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorLowerToLowest
//
// Implements Floor_LowerToLowest(tag, speed, change)
// * ExtraData: 309
// * Hexen:     21
//
DEFINE_ACTION(EV_ActionParamFloorLowerToLowest)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction   = 0;              // down
   fd.target_type = FtoLnF;         // to lowest neighboring floor
   fd.spac        = instance->spac; // activated Hexen-style
   fd.flags       = FDF_HAVESPAC;
   fd.speed_type  = SpeedParam;    
   fd.speed_value = instance->args[1] * FRACUNIT / 8; // speed
   EV_floorChangeForArg(fd, instance->args[2]);       // change
   fd.crush       = -1;

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorRaiseToNearest
//
// Implements Floor_RaiseToNearest(tag, speed, change, crush)
// * ExtraData: 310
// * Hexen:     25
//
DEFINE_ACTION(EV_ActionParamFloorRaiseToNearest)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction   = 1;              // up
   fd.target_type = FtoNnF;         // to nearest neighboring floor
   fd.spac        = instance->spac; // activated Hexen-style
   fd.flags       = FDF_HAVESPAC;
   fd.speed_type  = SpeedParam;
   fd.speed_value = instance->args[1] * FRACUNIT / 8; // speed
   EV_floorChangeForArg(fd, instance->args[2]);       // change
   fd.crush       = instance->args[3];                // crush

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorLowerToNearest
//
// Implements Floor_LowerToNearest(tag, speed, change)
// * ExtraData: 311
// * Hexen:     22
//
DEFINE_ACTION(EV_ActionParamFloorLowerToNearest)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction   = 0;              // down
   fd.target_type = FtoNnF;         // to nearest neighboring floor
   fd.spac        = instance->spac; // activated Hexen-style
   fd.flags       = FDF_HAVESPAC;
   fd.speed_type  = SpeedParam;
   fd.speed_value = instance->args[1] * FRACUNIT / 8; // speed
   EV_floorChangeForArg(fd, instance->args[2]);       // change
   fd.crush       = -1;

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorRaiseToLowestCeiling
//
// Implements Floor_RaiseToLowestCeiling(tag, speed, change, crush)
// * ExtraData: 312
// * Hexen (ZDoom Extension): 238
//
DEFINE_ACTION(EV_ActionParamFloorRaiseToLowestCeiling)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction   = 1;              // up
   fd.target_type = FtoLnC;         // to lowest neighboring ceiling
   fd.spac        = instance->spac; // activated Hexen-style
   fd.flags       = FDF_HAVESPAC;
   fd.speed_type  = SpeedParam;
   fd.speed_value = instance->args[1] * FRACUNIT / 8; // speed
   EV_floorChangeForArg(fd, instance->args[2]);       // change
   fd.crush       = instance->args[3];                // crush

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorLowerToLowestCeiling
//
// Implements Floor_LowerToLowestCeiling(tag, speed, change)
// * ExtraData: 313
//
DEFINE_ACTION(EV_ActionParamFloorLowerToLowestCeiling)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction   = 0;              // down
   fd.target_type = FtoLnC;         // to lowest neighboring ceiling
   fd.spac        = instance->spac; // activated Hexen-style
   fd.flags       = FDF_HAVESPAC;
   fd.speed_type  = SpeedParam;
   fd.speed_value = instance->args[1] * FRACUNIT / 8; // speed
   EV_floorChangeForArg(fd, instance->args[2]);       // change
   fd.crush       = -1;

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorRaiseToCeiling
//
// Implements Floor_RaiseToCeiling(tag, speed, change, crush)
// * ExtraData: 314
//
DEFINE_ACTION(EV_ActionParamFloorRaiseToCeiling)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction   = 1;              // up
   fd.target_type = FtoC;           // to sector ceiling
   fd.spac        = instance->spac; // activated Hexen-style
   fd.flags       = FDF_HAVESPAC;
   fd.speed_type  = SpeedParam;
   fd.speed_value = instance->args[1] * FRACUNIT / 8; // speed
   EV_floorChangeForArg(fd, instance->args[2]);       // change
   fd.crush       = instance->args[3];                // crush

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorRaiseByTexture
//
// Implements Floor_RaiseByTexture(tag, speed, change, crush)
// * ExtraData: 315
// * Hexen (ZDoom Extension): 240
//
DEFINE_ACTION(EV_ActionParamFloorRaiseByTexture)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction   = 1;              // up
   fd.target_type = FbyST;          // by shortest lower texture
   fd.spac        = instance->spac; // activated Hexen-style
   fd.flags       = FDF_HAVESPAC;
   fd.speed_type  = SpeedParam;
   fd.speed_value = instance->args[1] * FRACUNIT / 8; // speed
   EV_floorChangeForArg(fd, instance->args[2]);       // change
   fd.crush       = instance->args[3];                // crush

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorLowerByTexture
//
// Implements Floor_LowerByTexture(tag, speed, change)
// * ExtraData: 316
//
DEFINE_ACTION(EV_ActionParamFloorLowerByTexture)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction   = 0;              // down
   fd.target_type = FbyST;          // by shortest lower texture
   fd.spac        = instance->spac; // activated Hexen-style
   fd.flags       = FDF_HAVESPAC;
   fd.speed_type  = SpeedParam;
   fd.speed_value = instance->args[1] * FRACUNIT / 8; // speed
   EV_floorChangeForArg(fd, instance->args[2]);       // change
   fd.crush       = -1;

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorRaiseByValue
//
// Implements Floor_RaiseByValue(tag, speed, height, change, crush)
// * ExtraData: 317
// * Hexen:     23
//
DEFINE_ACTION(EV_ActionParamFloorRaiseByValue)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction    = 1;              // up
   fd.target_type  = FbyParam;       // by value
   fd.spac         = instance->spac; // activated Hexen-style
   fd.flags        = FDF_HAVESPAC;
   fd.speed_type   = SpeedParam;
   fd.speed_value  = instance->args[1] * FRACUNIT / 8; // speed
   fd.height_value = instance->args[2] * FRACUNIT;     // height
   EV_floorChangeForArg(fd, instance->args[3]);        // change
   fd.crush        = instance->args[4];                // crush

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorRaiseByValueTimes8
//
// Implements Floor_RaiseByValueTimes8(tag, speed, height, change, crush)
// * ExtraData: TODO? (Not needed, really; args have full integer range)
// * Hexen:     35
//
DEFINE_ACTION(EV_ActionParamFloorRaiseByValueTimes8)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction    = 1;              // up
   fd.target_type  = FbyParam;       // by value
   fd.spac         = instance->spac; // activated Hexen-style
   fd.flags        = FDF_HAVESPAC;
   fd.speed_type   = SpeedParam;
   fd.speed_value  = instance->args[1] * FRACUNIT / 8; // speed
   fd.height_value = instance->args[2] * FRACUNIT * 8; // height
   EV_floorChangeForArg(fd, instance->args[3]);        // change
   fd.crush        = instance->args[4];                // crush

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorLowerByValue
//
// Implements Floor_LowerByValue(tag, speed, height, change)
// * ExtraData: 318
// * Hexen:     20
//
DEFINE_ACTION(EV_ActionParamFloorLowerByValue)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction    = 0;              // down
   fd.target_type  = FbyParam;       // by value
   fd.spac         = instance->spac; // activated Hexen-style
   fd.flags        = FDF_HAVESPAC;
   fd.speed_type   = SpeedParam;
   fd.speed_value  = instance->args[1] * FRACUNIT / 8; // speed
   fd.height_value = instance->args[2] * FRACUNIT;     // height
   EV_floorChangeForArg(fd, instance->args[3]);        // change
   fd.crush        = -1;

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorLowerByValueTimes8
//
// Implements Floor_LowerByValue(tag, speed, height, change)
// * ExtraData: TODO? (Not really needed)
// * Hexen:     36
//
DEFINE_ACTION(EV_ActionParamFloorLowerByValueTimes8)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction    = 0;              // down
   fd.target_type  = FbyParam;       // by value
   fd.spac         = instance->spac; // activated Hexen-style
   fd.flags        = FDF_HAVESPAC;
   fd.speed_type   = SpeedParam;
   fd.speed_value  = instance->args[1] * FRACUNIT / 8; // speed
   fd.height_value = instance->args[2] * FRACUNIT * 8; // height
   EV_floorChangeForArg(fd, instance->args[3]);        // change
   fd.crush        = -1;

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorMoveToValue
//
// Implements Floor_MoveToValue(tag, speed, height, neg, change)
// * ExtraData: 319
// * Hexen (ZDoom Extension): 37
//
DEFINE_ACTION(EV_ActionParamFloorMoveToValue)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction    = 1;              // not used; direction is relative to target
   fd.target_type  = FtoAbs;         // to absolute height
   fd.spac         = instance->spac; // activated Hexen-style
   fd.flags        = FDF_HAVESPAC;
   fd.speed_type   = SpeedParam;
   fd.speed_value  = instance->args[1] * FRACUNIT / 8; // speed
   fd.height_value = instance->args[2] * FRACUNIT;     // height
   if(instance->args[3])                               // neg
      fd.height_value = -fd.height_value;
   EV_floorChangeForArg(fd, instance->args[4]);        // change
   fd.crush        = -1;

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorMoveToValueTimes8
//
// Implements Floor_MoveToValueTimes8(tag, speed, height, neg, change)
// * ExtraData: TODO? (Not really needed)
// * Hexen:     68
//
DEFINE_ACTION(EV_ActionParamFloorMoveToValueTimes8)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction    = 1;              // not used; direction is relative to target
   fd.target_type  = FtoAbs;         // to absolute height
   fd.spac         = instance->spac; // activated Hexen-style
   fd.flags        = FDF_HAVESPAC;
   fd.speed_type   = SpeedParam;
   fd.speed_value  = instance->args[1] * FRACUNIT / 8; // speed
   fd.height_value = instance->args[2] * FRACUNIT * 8; // height
   if(instance->args[3])                               // neg
      fd.height_value = -fd.height_value;
   EV_floorChangeForArg(fd, instance->args[4]);        // change
   fd.crush        = -1;

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorRaiseInstant
//
// Implements Floor_RaiseInstant(tag, unused, height, change, crush)
// * ExtraData: 320
// * Hexen:     67
//
DEFINE_ACTION(EV_ActionParamFloorRaiseInstant)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction    = 1;              // up
   fd.target_type  = FInst;          // always move instantly
   fd.spac         = instance->spac; // activated Hexen-style
   fd.flags        = FDF_HAVESPAC;
   fd.speed_type   = SpeedNormal;    // unused, always instant.
   fd.height_value = instance->args[2] * FRACUNIT * 8; // height
   EV_floorChangeForArg(fd, instance->args[3]);        // change
   fd.crush        = instance->args[4];                // crush

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorLowerInstant
//
// Implements Floor_LowerInstant(tag, unused, height, change)
// * ExtraData: 321
// * Hexen:     66
//
DEFINE_ACTION(EV_ActionParamFloorLowerInstant)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction    = 0;              // down
   fd.target_type  = FInst;          // always move instantly
   fd.spac         = instance->spac; // activated Hexen-style
   fd.flags        = FDF_HAVESPAC;
   fd.speed_type   = SpeedNormal;    // unused, always instant.
   fd.height_value = instance->args[2] * FRACUNIT * 8; // height
   EV_floorChangeForArg(fd, instance->args[3]);        // change
   fd.crush        = -1;

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamFloorToCeilingInstant
//
// Implements Floor_ToCeilingInstant(tag, change, crush)
// * ExtraData: 322
//
DEFINE_ACTION(EV_ActionParamFloorToCeilingInstant)
{
   INIT_STRUCT(floordata_t, fd);

   fd.direction   = 0;              // down (to cause instant movement)
   fd.target_type = FtoC;           // to sector ceiling
   fd.spac        = instance->spac; // activated Hexen-style
   fd.flags       = FDF_HAVESPAC;
   fd.speed_type  = SpeedNormal;    // unused (always instant).
   EV_floorChangeForArg(fd, instance->args[1]); // change
   fd.crush       = instance->args[2];          // crush

   return EV_DoParamFloor(instance->line, instance->tag, &fd);
}

//
// EV_ActionParamCeilingRaiseToHighest
//
// Implements Ceiling_RaiseToHighest(tag, speed, change)
// * ExtraData: 323
//
DEFINE_ACTION(EV_ActionParamCeilingRaiseToHighest)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction   = 1;              // up
   cd.target_type = CtoHnC;         // to highest neighboring ceiling
   cd.spac        = instance->spac; // activated Hexen-style
   cd.flags       = CDF_HAVESPAC;
   cd.speed_type  = SpeedParam;
   cd.speed_value = instance->args[1] * FRACUNIT / 8; // speed
   EV_ceilingChangeForArg(cd, instance->args[2]);     // change
   cd.crush       = -1;

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingToHighestInstant
//
// Implements Ceiling_ToHighestInstant(tag, change, crush)
// * ExtraData: 324
//
DEFINE_ACTION(EV_ActionParamCeilingToHighestInstant)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction   = 0;              // down (to cause instant movement)
   cd.target_type = CtoHnC;         // to highest neighboring ceiling
   cd.spac        = instance->spac; // activated Hexen-style
   cd.flags       = CDF_HAVESPAC;
   cd.speed_type  = SpeedNormal;    // not used
   EV_ceilingChangeForArg(cd, instance->args[1]); // change
   cd.crush       = instance->args[2];            // crush

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingRaiseToNearest
//
// Implements Ceiling_RaiseToNearest(tag, speed, change)
// * ExtraData: 325
// * Hexen (ZDoom Extension): 252
//
DEFINE_ACTION(EV_ActionParamCeilingRaiseToNearest)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 1;              // up
   cd.target_type   = CtoNnC;         // to nearest neighboring ceiling
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   EV_ceilingChangeForArg(cd, instance->args[2]);       // change
   cd.crush         = -1;

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingLowerToNearest
//
// Implements Ceiling_LowerToNearest(tag, speed, change, crush)
// * ExtraData: 326
//
DEFINE_ACTION(EV_ActionParamCeilingLowerToNearest)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 0;              // down
   cd.target_type   = CtoNnC;         // to nearest neighboring ceiling
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   EV_ceilingChangeForArg(cd, instance->args[2]);       // change
   cd.crush         = instance->args[3];                // crush

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingRaiseToLowest
//
// Implements Ceiling_RaiseToLowest(tag, speed, change)
// * ExtraData: 327
//
DEFINE_ACTION(EV_ActionParamCeilingRaiseToLowest)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 1;              // up
   cd.target_type   = CtoLnC;         // to lowest neighboring ceiling
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   EV_ceilingChangeForArg(cd, instance->args[2]);       // change
   cd.crush         = -1;

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingLowerToLowest
//
// Implements Ceiling_LowerToLowest(tag, speed, change, crush)
// * ExtraData: 328
// * Hexen (ZDoom Extension): 253
//
DEFINE_ACTION(EV_ActionParamCeilingLowerToLowest)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 0;              // down
   cd.target_type   = CtoLnC;         // to lowest neighboring ceiling
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   EV_ceilingChangeForArg(cd, instance->args[2]);       // change
   cd.crush         = instance->args[3];                // crush

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingRaiseToHighestFloor
//
// Implements Ceiling_RaiseToHighestFloor(tag, speed, change)
// * ExtraData: 329
//
DEFINE_ACTION(EV_ActionParamCeilingRaiseToHighestFloor)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 1;              // up
   cd.target_type   = CtoHnF;         // to highest neighboring floor
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   EV_ceilingChangeForArg(cd, instance->args[2]);       // change
   cd.crush         = -1;

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingLowerToHighestFloor
//
// Implements Ceiling_LowerToHighestFloor(tag, speed, change crush)
// * ExtraData: 330
// * Hexen (ZDoom Extension): 192
//
DEFINE_ACTION(EV_ActionParamCeilingLowerToHighestFloor)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 0;              // down
   cd.target_type   = CtoHnF;         // to highest neighboring floor
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   EV_ceilingChangeForArg(cd, instance->args[2]);       // change
   cd.crush         = instance->args[3];                // crush

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingToFloorInstant
//
// Implements Ceiling_ToFloorInstant(tag, change, crush)
// * ExtraData: 331
//
DEFINE_ACTION(EV_ActionParamCeilingToFloorInstant)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 1;              // up (to cause instant movement)
   cd.target_type   = CtoF;           // to sector floor
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedNormal;    // not used
   EV_ceilingChangeForArg(cd, instance->args[1]); // change
   cd.crush         = instance->args[2];          // crush

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingLowerToFloor
//
// Implements Ceiling_LowerToFloor(tag, speed, change, crush)
// * ExtraData: 332
// * Hexen (ZDoom Extension): 254
//
DEFINE_ACTION(EV_ActionParamCeilingLowerToFloor)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 0;              // down
   cd.target_type   = CtoF;           // to sector floor
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   EV_ceilingChangeForArg(cd, instance->args[2]);       // change
   cd.crush         = instance->args[3];                // crush

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingRaiseByTexture
//
// Implements Ceiling_RaiseByTexture(tag, speed, change)
// * ExtraData: 333
//
DEFINE_ACTION(EV_ActionParamCeilingRaiseByTexture)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 1;              // up
   cd.target_type   = CbyST;          // by shortest upper texture
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   EV_ceilingChangeForArg(cd, instance->args[2]);       // change
   cd.crush         = -1;

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingLowerByTexture
//
// Implements Ceiling_LowerByTexture(tag, speed, change, crush)
// * ExtraData: 334
//
DEFINE_ACTION(EV_ActionParamCeilingLowerByTexture)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 0;              // down
   cd.target_type   = CbyST;          // by shortest upper texture
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   EV_ceilingChangeForArg(cd, instance->args[2]);       // change
   cd.crush         = instance->args[3];                // crush

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingRaiseByValue
//
// Implements Ceiling_RaiseByValue(tag, speed, height, change)
// * ExtraData: 335
// * Hexen:     41
//
DEFINE_ACTION(EV_ActionParamCeilingRaiseByValue)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 1;              // up
   cd.target_type   = CbyParam;       // by value
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   cd.height_value  = instance->args[2] * FRACUNIT;     // height
   EV_ceilingChangeForArg(cd, instance->args[3]);       // change
   cd.crush         = -1;

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingLowerByValue
//
// Implements Ceiling_LowerByValue(tag, speed, height, change, crush)
// * ExtraData: 336
// * Hexen:     40
//
DEFINE_ACTION(EV_ActionParamCeilingLowerByValue)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 0;              // down
   cd.target_type   = CbyParam;       // by value
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   cd.height_value  = instance->args[2] * FRACUNIT;     // height
   EV_ceilingChangeForArg(cd, instance->args[3]);       // change
   cd.crush         = instance->args[4];                // crush

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingRaiseByValueTimes8
//
// Implements Ceiling_RaiseByValueTimes8(tag, speed, height, change)
// * Hexen (ZDoom Extension): 198
//
DEFINE_ACTION(EV_ActionParamCeilingRaiseByValueTimes8)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 1;              // up
   cd.target_type   = CbyParam;       // by value
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   cd.height_value  = instance->args[2] * FRACUNIT * 8; // height
   EV_ceilingChangeForArg(cd, instance->args[3]);       // change
   cd.crush         = -1;

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingLowerByValueTimes8
//
// Implements Ceiling_LowerByValueTimes8(tag, speed, height, change, crush)
// * Hexen (ZDoom Extension): 199
//
DEFINE_ACTION(EV_ActionParamCeilingLowerByValueTimes8)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 0;              // down
   cd.target_type   = CbyParam;       // by value
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   cd.height_value  = instance->args[2] * FRACUNIT * 8; // height
   EV_ceilingChangeForArg(cd, instance->args[3]);       // change
   cd.crush         = instance->args[4];                // crush

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingMoveToValue
//
// Implements Ceiling_MoveToValue(tag, speed, height, neg, change)
// * ExtraData: 337
// * Hexen:     47
//
DEFINE_ACTION(EV_ActionParamCeilingMoveToValue)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 1;              // not used; motion is relative to target
   cd.target_type   = CtoAbs;         // move to absolute height
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   cd.height_value  = instance->args[2] * FRACUNIT;     // height
   if(instance->args[3])                                // neg
      cd.height_value = -cd.height_value;
   EV_ceilingChangeForArg(cd, instance->args[4]);       // change
   cd.crush = -1;

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingMoveToValueTimes8
//
// Implements Ceiling_MoveToValueTimes8(tag, speed, height, neg, change)
// * Hexen: 69
//
DEFINE_ACTION(EV_ActionParamCeilingMoveToValueTimes8)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 1;              // not used; motion is relative to target
   cd.target_type   = CtoAbs;         // move to absolute height
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedParam;
   cd.speed_value   = instance->args[1] * FRACUNIT / 8; // speed
   cd.height_value  = instance->args[2] * FRACUNIT * 8; // height
   if(instance->args[3])                                // neg
      cd.height_value = -cd.height_value;
   EV_ceilingChangeForArg(cd, instance->args[4]);       // change
   cd.crush = -1;

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingRaiseInstant
//
// Implements Ceiling_RaiseInstant(tag, unused, height, change)
// * ExtraData: 338
// * Hexen (ZDoom Extension): 194
//
DEFINE_ACTION(EV_ActionParamCeilingRaiseInstant)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 1;              // up
   cd.target_type   = CInst;          // instant motion to dest height
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedNormal;    // not used
   cd.height_value  = instance->args[2] * FRACUNIT * 8; // height
   EV_ceilingChangeForArg(cd, instance->args[3]);
   cd.crush         = -1;

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamCeilingLowerInstant
//
// Implements Ceiling_LowerInstant(tag, unused, height, change, crush)
// * ExtraData: 339
// * Hexen (ZDoom Extension): 193
//
DEFINE_ACTION(EV_ActionParamCeilingLowerInstant)
{
   INIT_STRUCT(ceilingdata_t, cd);

   cd.direction     = 0;              // down
   cd.target_type   = CInst;          // instant motion to dest height
   cd.spac          = instance->spac; // activated Hexen-style
   cd.flags         = CDF_HAVESPAC;
   cd.speed_type    = SpeedNormal;    // unused
   cd.height_value  = instance->args[2] * FRACUNIT * 8; // height
   EV_ceilingChangeForArg(cd, instance->args[3]);       // change
   cd.crush         = instance->args[4];

   return EV_DoParamCeiling(instance->line, instance->tag, &cd);
}

//
// EV_ActionParamStairsBuildUpDoom
//
// Implements Stairs_BuildUpDoom(tag, speed, height, delay, reset)
// * ExtraData: 340
// * Hexen (ZDoom Extension): 217
//
DEFINE_ACTION(EV_ActionParamStairsBuildUpDoom)
{
   INIT_STRUCT(stairdata_t, sd);

   sd.flags          = SDF_HAVESPAC;
   sd.spac           = instance->spac; // Hexen-style activation
   sd.direction      = 1;              // up
   sd.speed_type     = SpeedParam;
   sd.speed_value    = instance->args[1] * FRACUNIT / 8; // speed
   sd.stepsize_type  = StepSizeParam;
   sd.stepsize_value = instance->args[2] * FRACUNIT;     // height
   sd.delay_value    = instance->args[3];                // delay
   sd.reset_value    = instance->args[4];                // reset

   return EV_DoParamStairs(instance->line, instance->tag, &sd);
}

//
// EV_ActionParamStairsBuildDownDoom
//
// Implements Stairs_BuildDownDoom(tag, speed, height, delay, reset)
// * ExtraData: 341
//
DEFINE_ACTION(EV_ActionParamStairsBuildDownDoom)
{
   INIT_STRUCT(stairdata_t, sd);

   sd.flags          = SDF_HAVESPAC;
   sd.spac           = instance->spac; // Hexen-style activation
   sd.direction      = 0;              // down
   sd.speed_type     = SpeedParam;
   sd.speed_value    = instance->args[1] * FRACUNIT / 8; // speed
   sd.stepsize_type  = StepSizeParam;
   sd.stepsize_value = instance->args[2] * FRACUNIT;     // height
   sd.delay_value    = instance->args[3];                // delay
   sd.reset_value    = instance->args[4];

   return EV_DoParamStairs(instance->line, instance->tag, &sd);
}

//
// EV_ActionParamStairsBuildUpDoomSync
//
// Implements Stairs_BuildUpDoomSync(tag, speed, height, reset)
// * ExtraData: 342
//
DEFINE_ACTION(EV_ActionParamStairsBuildUpDoomSync)
{
   INIT_STRUCT(stairdata_t, sd);

   sd.flags          = SDF_HAVESPAC | SDF_SYNCHRONIZED;
   sd.spac           = instance->spac; 
   sd.direction      = 1;
   sd.speed_type     = SpeedParam;
   sd.speed_value    = instance->args[1] * FRACUNIT / 8; // speed
   sd.stepsize_type  = StepSizeParam;
   sd.stepsize_value = instance->args[2] * FRACUNIT;     // height
   sd.delay_value    = 0;
   sd.reset_value    = instance->args[3];                // reset

   return EV_DoParamStairs(instance->line, instance->tag, &sd);
}

//
// EV_ActionParamStairsBuildDownDoomSync
//
// Implements Stairs_BuildDownDoomSync(tag, speed, height, reset)
// * ExtraData: 343
//
DEFINE_ACTION(EV_ActionParamStairsBuildDownDoomSync)
{
   INIT_STRUCT(stairdata_t, sd);

   sd.flags          = SDF_HAVESPAC | SDF_SYNCHRONIZED;
   sd.spac           = instance->spac; 
   sd.direction      = 0;
   sd.speed_type     = SpeedParam;
   sd.speed_value    = instance->args[1] * FRACUNIT / 8; // speed
   sd.stepsize_type  = StepSizeParam;
   sd.stepsize_value = instance->args[2] * FRACUNIT;     // height
   sd.delay_value    = 0;
   sd.reset_value    = instance->args[3];                // reset

   return EV_DoParamStairs(instance->line, instance->tag, &sd);
}

//
// EV_ActionPolyobjDoorSlide
//
// Implements Polyobj_DoorSlide(id, speed, angle, distance, delay)
// * ExtraData: 350
// * Hexen:     8
//
DEFINE_ACTION(EV_ActionPolyobjDoorSlide)
{
   INIT_STRUCT(polydoordata_t, pdd);

   pdd.doorType   = POLY_DOOR_SLIDE;
   pdd.polyObjNum = instance->args[0];                // id
   pdd.speed      = instance->args[1] * FRACUNIT / 8; // speed
   pdd.angle      = instance->args[2];                // angle (byte angle)
   pdd.distance   = instance->args[3] * FRACUNIT;     // distance
   pdd.delay      = instance->args[4];                // delay in tics

   return EV_DoPolyDoor(&pdd);
}

//
// EV_ActionPolyobjDoorSwing
//
// Implements Polyobj_DoorSwing(id, speed, distance, delay)
// * ExtraData: 351
// * Hexen:     7
//
DEFINE_ACTION(EV_ActionPolyobjDoorSwing)
{
   INIT_STRUCT(polydoordata_t, pdd);

   pdd.doorType   = POLY_DOOR_SWING;
   pdd.polyObjNum = instance->args[0]; // id
   pdd.speed      = instance->args[1]; // angular speed (byte angle)
   pdd.distance   = instance->args[2]; // angular distance (byte angle)
   pdd.delay      = instance->args[3]; // delay in tics

   return EV_DoPolyDoor(&pdd);
}

//
// EV_ActionPolyobjMove
//
// Implements Polyobj_Move(id, speed, angle, distance)
// * ExtraData: 352
// * Hexen:     4
//
DEFINE_ACTION(EV_ActionPolyobjMove)
{
   INIT_STRUCT(polymovedata_t, pmd);

   pmd.polyObjNum = instance->args[0];                // id
   pmd.speed      = instance->args[1] * FRACUNIT / 8; // speed
   pmd.angle      = instance->args[2];                // angle (byte angle)
   pmd.distance   = instance->args[3] * FRACUNIT;     // distance
   pmd.overRide   = false;

   return EV_DoPolyObjMove(&pmd);
}

//
// EV_ActionPolyobjMoveTimes8
//
// Implements Polyobj_MoveTimes8(id, speed, angle, distance)
// * Hexen: 6
//
DEFINE_ACTION(EV_ActionPolyobjMoveTimes8)
{
   INIT_STRUCT(polymovedata_t, pmd);

   pmd.polyObjNum = instance->args[0];                // id
   pmd.speed      = instance->args[1] * FRACUNIT / 8; // speed
   pmd.angle      = instance->args[2];                // angle (byte angle)
   pmd.distance   = instance->args[3] * FRACUNIT * 8; // distance
   pmd.overRide   = false;

   return EV_DoPolyObjMove(&pmd);
}

//
// EV_ActionPolyobjORMove
//
// Implements Polyobj_OR_Move(id, speed, angle, distance)
// * ExtraData: 353
// * Hexen:     92
//
DEFINE_ACTION(EV_ActionPolyobjORMove)
{
   INIT_STRUCT(polymovedata_t, pmd);

   pmd.polyObjNum = instance->args[0];                // id
   pmd.speed      = instance->args[1] * FRACUNIT / 8; // speed
   pmd.angle      = instance->args[2];                // angle (byte angle)
   pmd.distance   = instance->args[3] * FRACUNIT;     // distance
   pmd.overRide   = true;

   return EV_DoPolyObjMove(&pmd);
}

//
// EV_ActionPolyobjORMoveTimes8
//
// Implements Polyobj_OR_MoveTimes8(id, speed, angle, distance)
// * Hexen: 93
//
DEFINE_ACTION(EV_ActionPolyobjORMoveTimes8)
{
   INIT_STRUCT(polymovedata_t, pmd);

   pmd.polyObjNum = instance->args[0];                // id
   pmd.speed      = instance->args[1] * FRACUNIT / 8; // speed
   pmd.angle      = instance->args[2];                // angle (byte angle)
   pmd.distance   = instance->args[3] * FRACUNIT * 8; // distance
   pmd.overRide   = true;

   return EV_DoPolyObjMove(&pmd);
}

//
// EV_ActionPolyobjRotateRight
//
// Implements Polyobj_RotateRight(id, speed, distance)
// * ExtraData: 354
// * Hexen:     3
//
DEFINE_ACTION(EV_ActionPolyobjRotateRight)
{
   INIT_STRUCT(polyrotdata_t, prd);

   prd.polyObjNum = instance->args[0]; // id
   prd.speed      = instance->args[1]; // angular speed (byte angle)
   prd.distance   = instance->args[2]; // angular distance (byte angle)
   prd.direction  = -1;
   prd.overRide   = false;

   return EV_DoPolyObjRotate(&prd);
}

//
// EV_ActionPolyobjORRotateRight
//
// Implements Polyobj_OR_RotateRight(id, speed, distance)
// * ExtraData: 355
// * Hexen:     91
//
DEFINE_ACTION(EV_ActionPolyobjORRotateRight)
{
   INIT_STRUCT(polyrotdata_t, prd);

   prd.polyObjNum = instance->args[0]; // id
   prd.speed      = instance->args[1]; // angular speed (byte angle)
   prd.distance   = instance->args[2]; // angular distance (byte angle)
   prd.direction  = -1;
   prd.overRide   = true;

   return EV_DoPolyObjRotate(&prd);
}

//
// EV_ActionPolyobjRotateLeft
//
// Implements Polyobj_RotateLeft(id, speed, distance)
// * ExtraData: 356
// * Hexen:     2
//
DEFINE_ACTION(EV_ActionPolyobjRotateLeft)
{
   INIT_STRUCT(polyrotdata_t, prd);

   prd.polyObjNum = instance->args[0]; // id
   prd.speed      = instance->args[1]; // angular speed (byte angle)
   prd.distance   = instance->args[2]; // angular distance (byte angle)
   prd.direction  = 1;
   prd.overRide   = false;

   return EV_DoPolyObjRotate(&prd);
}

//
// EV_ActionPolyobjORRotateLeft
//
// Implements Polyobj_OR_RotateLeft(id, speed, distance)
// * ExtraData: 357
// * Hexen:     90
//
DEFINE_ACTION(EV_ActionPolyobjORRotateLeft)
{
   INIT_STRUCT(polyrotdata_t, prd);

   prd.polyObjNum = instance->args[0]; // id
   prd.speed      = instance->args[1]; // angular speed (byte angle)
   prd.distance   = instance->args[2]; // angular distance (byte angle)
   prd.direction  = 1;
   prd.overRide   = true;

   return EV_DoPolyObjRotate(&prd);
}

//
// EV_ActionPillarBuild
//
// Implements Pillar_Build(tag, speed, height)
// * ExtraData: 362
// * Hexen:     29
//
DEFINE_ACTION(EV_ActionPillarBuild)
{
   INIT_STRUCT(pillardata_t, pd);

   pd.tag    = instance->tag;
   pd.speed  = instance->args[1] * FRACUNIT / 8;
   pd.height = instance->args[2] * FRACUNIT;
   pd.crush  = 0;

   return EV_PillarBuild(instance->line, &pd);
}

//
// EV_ActionPillarBuildAndCrush
//
// Implements Pillar_BuildAndCrush(tag, speed, height, crush)
// * ExtraData: 363
// * Hexen:     94
//
DEFINE_ACTION(EV_ActionPillarBuildAndCrush)
{
   INIT_STRUCT(pillardata_t, pd);

   pd.tag    = instance->tag;
   pd.speed  = instance->args[1] * FRACUNIT / 8;
   pd.height = instance->args[2] * FRACUNIT;
   pd.crush  = instance->args[3];
   // TODO: support ZDoom crush mode in args[4]

   return EV_PillarBuild(instance->line, &pd);
}

//
// EV_ActionPillarOpen
//
// Implements Pillar_Open(tag, speed, fdist, cdist)
// * ExtraData: 364
// * Hexen:     30
//
DEFINE_ACTION(EV_ActionPillarOpen)
{
   INIT_STRUCT(pillardata_t, pd);

   pd.tag   = instance->args[0];
   pd.speed = instance->args[1] * FRACUNIT / 8;
   pd.fdist = instance->args[2] * FRACUNIT;
   pd.cdist = instance->args[3] * FRACUNIT;
   pd.crush = 0;

   return EV_PillarOpen(instance->line, &pd);
}

//
// EV_ActionACSExecute
//
// Implements ACS_Execute(script, map, arg1, arg2, arg3)
// * ExtraData: 365
// * Hexen:     80
//
DEFINE_ACTION(EV_ActionACSExecute)
{
   Mobj   *thing = instance->actor;
   line_t *line  = instance->line;
   int     side  = instance->side;
   int     num   = instance->args[0];
   int     map   = instance->args[1];
   int     argc  = NUMLINEARGS - 2;
   int32_t argv[NUMLINEARGS - 2];

   for(int i = 0; i != argc; ++i)
      argv[i] = instance->args[i + 2];

   return ACS_ExecuteScriptNumber(num, map, 0, argv, argc, thing, line, side);
}


//
// EV_ActionACSSuspend
//
// Implements ACS_Suspend(script, map)
// * ExtraData: 366
// * Hexen:     81
//
DEFINE_ACTION(EV_ActionACSSuspend)
{
   return ACS_SuspendScriptNumber(instance->args[0], instance->args[1]);
}

//
// EV_ActionACSTerminate
//
// Implements ACS_Terminate(script, map)
// * ExtraData: 367
// * Hexen:     82
//
DEFINE_ACTION(EV_ActionACSTerminate)
{
   return ACS_TerminateScriptNumber(instance->args[0], instance->args[1]);
}

//
// EV_ActionACSExecuteWithResult
//
// Implements ACS_ExecuteWithResult(script, arg1, arg2, arg3, arg4)
// * ExtraData:               420
// * Hexen (ZDoom Extension): 84
//
DEFINE_ACTION(EV_ActionACSExecuteWithResult)
{
   Mobj   *thing = instance->actor;
   line_t *line  = instance->line;
   int     side  = instance->side;
   int     flags = ACS_EXECUTE_ALWAYS | ACS_EXECUTE_IMMEDIATE;
   int     num   = instance->args[0];
   int     argc  = NUMLINEARGS - 1;
   int32_t argv[NUMLINEARGS - 1];

   for(int i = 0; i != argc; ++i)
      argv[i] = instance->args[i + 1];

   ACSThinker *thread = NULL;
   ACS_ExecuteScriptNumber(num, 0, flags, argv, argc, thing, line, side, &thread);

   return thread ? thread->result : 0;
}

//
// EV_ActionParamLightRaiseByValue
//
// Implements Light_RaiseByValue(tag, value)
// * ExtraData: 368
// * Hexen:     110
//
DEFINE_ACTION(EV_ActionParamLightRaiseByValue)
{
   return EV_SetLight(instance->line, instance->tag, setlight_add, instance->args[1]);
}

//
// EV_ActionParamLightLowerByValue
//
// Implements Light_LowerByValue(tag, value)
// * ExtraData: 369
// * Hexen:     111
//
DEFINE_ACTION(EV_ActionParamLightLowerByValue)
{
   return EV_SetLight(instance->line, instance->tag, setlight_sub, instance->args[1]);
}

//
// EV_ActionParamLightChangeToValue
//
// Implements Light_ChangeToValue(tag, value)
// * ExtraData: 370
// * Hexen:     112
//
DEFINE_ACTION(EV_ActionParamLightChangeToValue)
{
   return EV_SetLight(instance->line, instance->tag, setlight_set, instance->args[1]);
}

//
// EV_ActionParamLightFade
//
// Implements Light_Fade(tag, dest, speed)
// * ExtraData: 371
// * Hexen:     113
//
DEFINE_ACTION(EV_ActionParamLightFade)
{
   return EV_FadeLight(instance->line, instance->tag,
                         instance->args[1], instance->args[2]);
}

//
// EV_ActionParamLightGlow
//
// Implements Light_Glow(tag, max, min, speed)
// * ExtraData: 372
// * Hexen:     114
//
DEFINE_ACTION(EV_ActionParamLightGlow)
{
   return EV_GlowLight(instance->line, instance->tag,
                         instance->args[1], instance->args[2], instance->args[3]);
}

//
// EV_ActionParamLightFlicker
//
// Implements Light_Flicker(tag, max, min)
// * ExtraData: 373
// * Hexen:     115
//
DEFINE_ACTION(EV_ActionParamLightFlicker)
{
   return EV_FlickerLight(instance->line, instance->tag,
                            instance->args[1], instance->args[2]);
}

//
// EV_ActionParamLightStrobe
//
// Implements Light_Strobe(tag, maxval, minval, maxtime, mintime)
// * ExtraData: 374
// * Hexen:     116
//
DEFINE_ACTION(EV_ActionParamLightStrobe)
{
   return EV_StrobeLight(instance->line, instance->tag, instance->args[1],
                           instance->args[2], instance->args[3], instance->args[4]);
}

//
// EV_ActionRadiusQuake
//
// Implements Radius_Quake(intensity, duration, dmgradius, radius, tid)
// * ExtraData: 375
// * Hexen:     120
//
DEFINE_ACTION(EV_ActionRadiusQuake)
{
   return P_StartQuake(instance->args);
}

//
// EV_ActionFloorWaggle
//
// Implements Floor_Waggle(tag, height, speed, offset, timer)
// * ExtraData: 397
// * Hexen:     138
//
DEFINE_ACTION(EV_ActionFloorWaggle)
{
   return EV_StartFloorWaggle(instance->line, instance->tag, instance->args[1],
                                instance->args[2], instance->args[3], instance->args[4]);
}

//
// EV_ActionThingSpawn
//
// Implements Thing_Spawn(tid, type, angle, newtid)
// * ExtraData: 398
// * Hexen:     135
//
DEFINE_ACTION(EV_ActionThingSpawn)
{
   return EV_ThingSpawn(instance->args, true);
}

//
// EV_ActionThingSpawnNoFog
//
// Implements Thing_SpawnNoFog(tid, type, angle, newtid)
// * ExtraData: 399
// * Hexen:     137
//
DEFINE_ACTION(EV_ActionThingSpawnNoFog)
{
   return EV_ThingSpawn(instance->args, false);
}

//
// EV_ActionTeleportEndGame
//
// Implements Teleport_EndGame()
// * ExtraData: 400
// * Hexen:     75
//
DEFINE_ACTION(EV_ActionTeleportEndGame)
{
   G_ForceFinale();
   return true;
}

//
// EV_ActionThingProjectile
//
// Implements Thing_Projectile(tid, type, angle, speed, vspeed)
// * ExtraData: 402
// * Hexen:     134
//
DEFINE_ACTION(EV_ActionThingProjectile)
{
   return EV_ThingProjectile(instance->args, false);
}

//
// EV_ActionThingProjectileGravity
//
// Implements Thing_ProjectileGravity(tid, type, angle, speed, vspeed)
// * ExtraData: 403
// * Hexen:     136
//
DEFINE_ACTION(EV_ActionThingProjectileGravity)
{
   return EV_ThingProjectile(instance->args, true);
}

//
// EV_ActionThingActivate
//
// Implements Thing_Activate(tid)
// * ExtraData: 404
// * Hexen:     130
//
DEFINE_ACTION(EV_ActionThingActivate)
{
   return EV_ThingActivate(instance->args[0]);
}

//
// EV_ActionThingDeactivate
//
// Implements Thing_Deactivate(tid)
// * ExtraData: 405
// * Hexen:     131
//
DEFINE_ACTION(EV_ActionThingDeactivate)
{
   return EV_ThingDeactivate(instance->args[0]);
}

//
// EV_ActionThrustThing
//
// Implements ThrustThing(angle, speed, reserved, tid)
// * ExtraData: 422
// * Hexen:     72
//
DEFINE_ACTION(EV_ActionThrustThing)
{
   return EV_ThrustThing(instance->actor, instance->args, instance->side);
}

//
// EV_ActionThrustThingZ
//
// Implements ThrustThingZ(tid, speed, updown, setadd)
// From ZDoom wiki documentation.
// * ExtraData: 423
// * Hexen:     128
//
DEFINE_ACTION(EV_ActionThrustThingZ)
{
   return EV_ThrustThingZ(instance->actor, instance->args);
}

//
// EV_ActionDamageThing
//
// Implements DamageThing(damage, mod)
// * ExtraData: 424
// * Hexen:     73
//
DEFINE_ACTION(EV_ActionDamageThing)
{
   return EV_DamageThing(instance->actor, instance->args);
}

//
// EV_ActionParamPlatPerpetualRaise
//
// Implements Plat_PerpetualRaise(tag, speed, delay)
// * ExtraData: 410
// * Hexen:     60
//
DEFINE_ACTION(EV_ActionParamPlatPerpetualRaise)
{
   return EV_DoParamPlat(instance->line, instance->args, paramPerpetualRaise);
}

//
// EV_ActionParamPlatStop
//
// Implements Plat_Stop(tag)
// * ExtraData: 411
// * Hexen:     61
//
DEFINE_ACTION(EV_ActionParamPlatStop)
{
   return EV_StopPlatByTag(instance->tag);
}

//
// EV_ActionParamPlatDWUS
//
// Implements Plat_DownWaitUpStay(tag, speed, delay)
// * ExtraData: 412
// * Hexen:     62
//
DEFINE_ACTION(EV_ActionParamPlatDWUS)
{
   return EV_DoParamPlat(instance->line, instance->args, paramDownWaitUpStay);
}

//
// EV_ActionParamPlatDownByValue
//
// Implements Plat_DownByValue(tag, speed, delay, height)
// * ExtraData: 413
// * Hexen:     63
//
DEFINE_ACTION(EV_ActionParamPlatDownByValue)
{
   return EV_DoParamPlat(instance->line, instance->args, paramDownByValueWaitUpStay);
}

//
// EV_ActionParamPlatUWDS
//
// Implements Plat_UpWaitDownStay(tag, speed, delay)
// * ExtraData: 414
// * Hexen:     64
//
DEFINE_ACTION(EV_ActionParamPlatUWDS)
{
   return EV_DoParamPlat(instance->line, instance->args, paramUpWaitDownStay);
}

//
// EV_ActionParamPlatUpByValue
//
// Implements Plat_UpByValue(tag, speed, delay, height)
// * ExtraData: 415
// * Hexen:     65
//
DEFINE_ACTION(EV_ActionParamPlatUpByValue)
{
   return EV_DoParamPlat(instance->line, instance->args, paramUpByValueWaitDownStay);
}

//
// EV_ActionThingChangeTID
//
// Implements Thing_ChangeTID(oldtid, newtid)
// * ExtraData: 421
// * Hexen:     176
//
DEFINE_ACTION(EV_ActionThingChangeTID)
{
   Mobj   *mo     = nullptr;
   Mobj   *next   = nullptr;
   int32_t oldtid = instance->args[0];
   int32_t newtid = instance->args[1];
   bool    found  = false;

   mo    = P_FindMobjFromTID(oldtid, nullptr, instance->actor);
   found = mo != nullptr;
   while(mo)
   {
      // Find next Mobj before changing TID.
      next = P_FindMobjFromTID(oldtid, mo, instance->actor);

      P_RemoveThingTID(mo);
      P_AddThingTID(mo, newtid);
      mo = next;
   }

   return found;
}

// EOF

