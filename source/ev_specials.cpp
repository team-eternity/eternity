// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Generalized line action system
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomstat.h"
#include "ev_specials.h"
#include "g_game.h"
#include "p_mobj.h"
#include "p_spec.h"
#include "r_defs.h"

// Call to require a valid actor
#define REQUIRE_ACTOR(actor) \
   if(!(actor))              \
      return false

// Call to require a valid line
#define REQUIRE_LINE(line) \
   if(!(line))             \
      return false

/*
int P_CheckTag(line_t *line)
{
   // killough 11/98: compatibility option:
   
   if(comp[comp_zerotags] || line->tag)
      return 1;

   switch (line->special)
   {
   case 1:   // Manual door specials
   
   case 11:  // Exits
   
   case 26:
   case 27:
   case 28:
  
   case 31:
   case 32:
   case 33:
   case 34:
   
   case 48:  // Scrolling walls
   case 51:
   
   case 85:

   case 117:
   case 118:
   
   case 138:
   case 139:  // Lighting specials
   
   case 169:
   case 170:
   case 171:
   case 172:
   case 173:
   case 174:
   
   case 192:
   case 193:  
   case 194:
   case 195:  // Thing teleporters
   case 197:
   case 198:

   case 209:
   case 210:
   
   case 273:
   case 274:   // W1
   case 275:
   case 276:   // SR
   case 277:   // S1
   case 278:   // GR
   case 279:   // G1
   case 280:   // WR -- haleyjd
      return 1;
   }

  return 0;
}
*/

//=============================================================================
//
// Utilities
//

//
// P_ClearSwitchOnFail
//
// haleyjd 08/29/09: Replaces demo_compatibility checks for clearing 
// W1/S1/G1 line actions on action failure, because it makes some maps
// unplayable if it is disabled unconditionally outside of demos.
//
inline static bool P_ClearSwitchOnFail(void)
{
   return demo_compatibility || (demo_version >= 335 && comp[comp_special]);
}

//=============================================================================
//
// DOOM Activation Helpers - Preambles and Post-Actions
//

//
// EV_DOOMPreCrossLine
//
// This function is used as the preamble to all DOOM cross-type line specials.
//
bool EV_DOOMPreCrossLine(ev_action_t *action, specialactivation_t *activation)
{
   // DOOM-style specials require an actor and a line
   REQUIRE_ACTOR(activation->actor);
   REQUIRE_LINE(activation->line);

   // Things that should never trigger lines
   if(!activation->actor->player)
   {
      // haleyjd: changed to check against MF2_NOCROSS flag instead of 
      // switching on type.
      if(activation->actor->flags2 & MF2_NOCROSS)
         return false;
   }

   // Check if action only allows player
   // jff 3/5/98 add ability of monsters etc. to use teleporters
   if(!activation->actor->player && !(action->flags & EV_PREALLOWMONSTERS))
      return false;

   // jff 2/27/98 disallow zero tag on some types
   // killough 11/98: compatibility option:
   if(!(activation->tag || comp[comp_zerotags] || 
        (action->flags & EV_PREALLOWZEROTAG)))
      return false;

   // check for first-side-only activation
   if(activation->side && (action->flags & EV_PREFIRSTSIDEONLY))
      return false;

   return true;
}

//
// EV_DOOMPostCrossLine
//
// This function is used as the post-action for all DOOM cross-type line
// specials.
//
bool EV_DOOMPostCrossLine(ev_action_t *action, bool result,
                          specialactivation_t *activation)
{
   if(action->flags & EV_POSTCLEARSPECIAL)
   {
      bool clearSpecial = (result || P_ClearSwitchOnFail());

      if(clearSpecial || (action->flags & EV_POSTCLEARALWAYS))
         activation->line->special = 0;
   }

   return result;
}

//=============================================================================
//
// Action Routines
//

//
// EV_ActionOpenDoor
//
static bool EV_ActionOpenDoor(ev_action_t *action, 
                              specialactivation_t *activation)
{
   // case 2:  (W1)
   // case 86: (WR)
   // Open Door
   return !!EV_DoDoor(activation->line, doorOpen);
}

//
// EV_ActionCloseDoor
//
static bool EV_ActionCloseDoor(ev_action_t *action,
                               specialactivation_t *activation)
{
   // case 3:  (W1)
   // case 75: (WR)
   // Close Door
   return !!EV_DoDoor(activation->line, doorClose);
}

//
// EV_ActionRaiseDoor
//
static bool EV_ActionRaiseDoor(ev_action_t *action,
                               specialactivation_t *activation)
{
   // case  4: (W1)
   // case 90: (WR)
   // Raise Door
   return !!EV_DoDoor(activation->line, doorNormal);
}

//
// EV_ActionRaiseFloor
//
static bool EV_ActionRaiseFloor(ev_action_t *action,
                                specialactivation_t *activation)
{
   // case  5: (W1)
   // case 91: (WR)
   // Raise Floor
   return !!EV_DoFloor(activation->line, raiseFloor);
}

//
// EV_ActionFastCeilCrushRaise
//
static bool EV_ActionFastCeilCrushRaise(ev_action_t *action,
                                        specialactivation_t *activation)
{
   // case 6:  (W1)
   // case 77: (WR)
   // Fast Ceiling Crush & Raise
   return !!EV_DoCeiling(activation->line, fastCrushAndRaise);
}

//
// EV_ActionBuildStairsUp8
//
static bool EV_ActionBuildStairsUp8(ev_action_t *action, 
                                    specialactivation_t *activation)
{
   // case 8:   (W1)
   // case 256: (WR - BOOM Extended)
   // Build Stairs
   return !!EV_BuildStairs(activation->line, build8);
}

//
// EV_ActionPlatDownWaitUpStay
//
static bool EV_ActionPlatDownWaitUpStay(ev_action_t *action,
                                        specialactivation_t *activation)
{
   // case 10: (W1)
   // case 88: (WR)
   // PlatDownWaitUp
   return !!EV_DoPlat(activation->line, downWaitUpStay, 0);
}

//
// EV_ActionLightTurnOn
//
static bool EV_ActionLightTurnOn(ev_action_t *action,
                                 specialactivation_t *activation)
{
   // case 12: (W1)
   // case 80: (WR)
   // Light Turn On - brightest near
   return !!EV_LightTurnOn(activation->line, 0);
}

//
// EV_ActionLightTurnOn255
//
static bool EV_ActionLightTurnOn255(ev_action_t *action,
                                    specialactivation_t *activation)
{
   // case 13: (W1)
   // case 81: (WR)
   // Light Turn On 255
   return !!EV_LightTurnOn(activation->line, 255);
}

//
// EV_ActionCloseDoor30
//
static bool EV_ActionCloseDoor30(ev_action_t *action,
                                 specialactivation_t *activation)
{
   // case 16: (W1)
   // case 76: (WR)
   // Close Door 30
   return !!EV_DoDoor(activation->line, closeThenOpen);
}

//
// EV_ActionStartLightStrobing
//
static bool EV_ActionStartLightStrobing(ev_action_t *action,
                                        specialactivation_t *activation)
{
   // case 17:  (W1)
   // case 156: (WR - BOOM Extended)
   // Start Light Strobing
   return !!EV_StartLightStrobing(activation->line);
}

//
// EV_ActionLowerFloor
//
static bool EV_ActionLowerFloor(ev_action_t *action,
                                specialactivation_t *activation)
{
   // case 19: (W1)
   // case 83: (WR)
   // Lower Floor
   return !!EV_DoFloor(activation->line, lowerFloor);
}

//
// EV_ActionPlatRaiseNearestChange
//
static bool EV_ActionPlatRaiseNearestChange(ev_action_t *action,
                                            specialactivation_t *activation)
{
   // case 22: (W1)
   // case 95: (WR)
   // Raise floor to nearest height and change texture
   return !!EV_DoPlat(activation->line, raiseToNearestAndChange, 0);
}

//
// EV_ActionCeilingCrushAndRaise
//
static bool EV_ActionCeilingCrushAndRaise(ev_action_t *action,
                                          specialactivation_t *activation)
{
   // case 25: (W1)
   // case 73: (WR)
   // Ceiling Crush and Raise
   return !!EV_DoCeiling(activation->line, crushAndRaise);
}

//
// EV_ActionFloorRaiseToTexture
//
static bool EV_ActionFloorRaiseToTexture(ev_action_t *action,
                                         specialactivation_t *activation)
{
   // case 30: (W1)
   // case 96: (WR)
   // Raise floor to shortest texture height on either side of lines.
   return !!EV_DoFloor(activation->line, raiseToTexture);
}

//
// EV_ActionLightsVeryDark
//
static bool EV_ActionLightsVeryDark(ev_action_t *action,
                                    specialactivation_t *activation)
{
   // case 35: (W1)
   // case 79: (WR)
   // Lights Very Dark
   return !!EV_LightTurnOn(activation->line, 35);
}

//
// EV_ActionLowerFloorTurbo
//
static bool EV_ActionLowerFloorTurbo(ev_action_t *action,
                                     specialactivation_t *activation)
{
   // case 36: (W1)
   // case 98: (WR)
   // Lower Floor (TURBO)
   return !!EV_DoFloor(activation->line, turboLower);
}

//
// EV_ActionFloorLowerAndChange
//
static bool EV_ActionFloorLowerAndChange(ev_action_t *action,
                                         specialactivation_t *activation)
{
   // case 37: (W1)
   // case 84: (WR)
   // LowerAndChange
   return !!EV_DoFloor(activation->line, lowerAndChange);
}

//
// EV_ActionFloorLowerToLowest
//
static bool EV_ActionFloorLowerToLowest(ev_action_t *action,
                                        specialactivation_t *activation)
{
   // case 38: (W1)
   // case 82: (WR)
   // Lower Floor To Lowest
   return !!EV_DoFloor(activation->line, lowerFloorToLowest);
}

//
// EV_ActionTeleport
//
static bool EV_ActionTeleport(ev_action_t *action, 
                              specialactivation_t *activation)
{
   // case 39: (W1)
   // case 97: (WR)
   // TELEPORT!
   // jff 02/09/98 fix using up with wrong side crossing
   return !!EV_Teleport(activation->line, activation->side, activation->actor);
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
static bool EV_ActionRaiseCeilingLowerFloor(ev_action_t *action,
                                            specialactivation_t *activation)
{
   line_t *line = activation->line;

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
      return !!EV_DoCeiling(line, raiseToHighest);
}

//
// EV_ActionBOOMRaiseCeilingLowerFloor
//
// This version on the other hand works unconditionally, and is a BOOM
// extension.
//
static bool EV_ActionBOOMRaiseCeilingLowerFloor(ev_action_t *action,
                                                specialactivation_t *activation)
{
   // case 151: (WR - BOOM Extended)
   // RaiseCeilingLowerFloor
   // 151 WR  EV_DoCeiling(raiseToHighest),
   //         EV_DoFloor(lowerFloortoLowest)
   EV_DoCeiling(activation->line, raiseToHighest);
   EV_DoFloor(activation->line, lowerFloorToLowest);

   return true;
}


//
// EV_ActionCeilingLowerAndCrush
//
static bool EV_ActionCeilingLowerAndCrush(ev_action_t *action,
                                          specialactivation_t *activation)
{
   // case 44: (W1)
   // case 72: (WR)
   // Ceiling Crush
   return !!EV_DoCeiling(activation->line, lowerAndCrush);
}

//
// EV_ActionExitLevel
//
static bool EV_ActionExitLevel(ev_action_t *action,
                               specialactivation_t *activation)
{
   Mobj *thing = activation->actor;

   // case 52:
   // EXIT!
   // killough 10/98: prevent zombies from exiting levels
   if(!(thing->player && thing->player->health <= 0 && !comp[comp_zombie]))
      G_ExitLevel();

   return true;
}

//
// EV_ActionPlatPerpetualRaise
//
static bool EV_ActionPlatPerpetualRaise(ev_action_t *action,
                                        specialactivation_t *activation)
{
   // case 53: (W1)
   // case 87: (WR)
   // Perpetual Platform Raise
   return !!EV_DoPlat(activation->line, perpetualRaise, 0);
}

//
// EV_ActionPlatStop
//
static bool EV_ActionPlatStop(ev_action_t *action,
                              specialactivation_t *activation)
{
   // case 54: (W1)
   // case 89: (WR)
   // Platform Stop
   return !!EV_StopPlat(activation->line);
}

//
// EV_ActionFloorRaiseCrush
//
static bool EV_ActionFloorRaiseCrush(ev_action_t *action,
                                     specialactivation_t *activation)
{
   // case 56: (W1)
   // case 94: (WR)
   // Raise Floor Crush
   return !!EV_DoFloor(activation->line, raiseFloorCrush);
}

//
// EV_ActionCeilingCrushStop
//
static bool EV_ActionCeilingCrushStop(ev_action_t *action,
                                      specialactivation_t *activation)
{
   // case 57: (W1)
   // case 74: (WR)
   // Ceiling Crush Stop
   return !!EV_CeilingCrushStop(activation->line);
}

//
// EV_ActionRaiseFloor24
//
static bool EV_ActionRaiseFloor24(ev_action_t *action,
                                  specialactivation_t *activation)
{
   // case 58: (W1)
   // case 92: (WR)
   // Raise Floor 24
   return !!EV_DoFloor(activation->line, raiseFloor24);
}

//
// EV_ActionRaiseFloor24Change
//
static bool EV_ActionRaiseFloor24Change(ev_action_t *action,
                                        specialactivation_t *activation)
{
   // case 59: (W1)
   // case 93: (WR)
   // Raise Floor 24 And Change
   return !!EV_DoFloor(activation->line, raiseFloor24AndChange);
}

//
// EV_ActionBuildStairsTurbo16
//
static bool EV_ActionBuildStairsTurbo16(ev_action_t *action,
                                        specialactivation_t *activation)
{
   // case 100: (W1)
   // case 257: (WR - BOOM Extended)
   // Build Stairs Turbo 16
   return !!EV_BuildStairs(activation->line, turbo16);
}

//
// EV_ActionTurnTagLightsOff
//
static bool EV_ActionTurnTagLightsOff(ev_action_t *action,
                                      specialactivation_t *activation)
{
   // case 104: (W1)
   // case 157: (WR - BOOM Extended)
   // Turn lights off in sector(tag)
   return !!EV_TurnTagLightsOff(activation->line);
}

//
// EV_ActionDoorBlazeRaise
//
static bool EV_ActionDoorBlazeRaise(ev_action_t *action,
                                    specialactivation_t *activation)
{
   // case 105: (WR)
   // case 108: (W1)
   // Blazing Door Raise (faster than TURBO!)
   return !!EV_DoDoor(activation->line, blazeRaise);
}

//
// EV_ActionDoorBlazeOpen
//
static bool EV_ActionDoorBlazeOpen(ev_action_t *action,
                                   specialactivation_t *activation)
{
   // case 106: (WR)
   // case 109: (W1)
   // Blazing Door Open (faster than TURBO!)
   return !!EV_DoDoor(activation->line, blazeOpen);
}

//
// EV_ActionDoorBlazeClose
//
static bool EV_ActionDoorBlazeClose(ev_action_t *action,
                                    specialactivation_t *activation)
{
   // case 107: (WR)
   // case 110: (W1)
   // Blazing Door Close (faster than TURBO!)
   return !!EV_DoDoor(activation->line, blazeClose);
}

//
// EV_ActionFloorRaiseToNearest
//
static bool EV_ActionFloorRaiseToNearest(ev_action_t *action,
                                         specialactivation_t *activation)
{
   // case 119: (W1)
   // case 128: (WR)
   // Raise floor to nearest surr. floor
   return !!EV_DoFloor(activation->line, raiseFloorToNearest);
}

//
// EV_ActionPlatBlazeDWUS
//
static bool EV_ActionPlatBlazeDWUS(ev_action_t *action,
                                   specialactivation_t *activation)
{
   // case 120: (WR)
   // case 121: (W1)
   // Blazing PlatDownWaitUpStay
   return !!EV_DoPlat(activation->line, blazeDWUS, 0);
}

//
// EV_ActionSecretExit
//
static bool EV_ActionSecretExit(ev_action_t *action,
                                specialactivation_t *activation)
{
   Mobj *thing = activation->actor;

   // case 124:
   // Secret EXIT
   // killough 10/98: prevent zombies from exiting levels
   if(!(thing->player && thing->player->health <= 0 && !comp[comp_zombie]))
      G_SecretExitLevel();

   return true;
}

//
// EV_ActionTeleportMonsterOnly
//
static bool EV_ActionTeleportMonsterOnly(ev_action_t *action,
                                         specialactivation_t *activation)
{
   line_t *line  = activation->line;
   int     side  = activation->side;
   Mobj   *thing = activation->actor;

   // case 125: (W1)
   // case 126: (WR)
   // TELEPORT MonsterONLY.
   return (!thing->player && EV_Teleport(line, side, thing));
}

//
// EV_ActionRaiseFloorTurbo
//
static bool EV_ActionRaiseFloorTurbo(ev_action_t *action,
                                     specialactivation_t *activation)
{
   // case 129: (WR)
   // case 130: (W1)
   // Raise Floor Turbo
   return !!EV_DoFloor(activation->line, raiseFloorTurbo);
}

//
// EV_ActionSilentCrushAndRaise
//
static bool EV_ActionSilentCrushAndRaise(ev_action_t *action,
                                         specialactivation_t *activation)
{
   // case 141: (W1)
   // case 150: (WR - BOOM Extended)
   // Silent Ceiling Crush & Raise
   return !!EV_DoCeiling(activation->line, silentCrushAndRaise);
}

//
// EV_ActionRaiseFloor512
//
static bool EV_ActionRaiseFloor512(ev_action_t *action,
                                   specialactivation_t *activation)
{
   // case 142: (W1 - BOOM Extended)
   // case 147: (WR - BOOM Extended)
   // Raise Floor 512
   return !!EV_DoFloor(activation->line, raiseFloor512);
}

//
// EV_ActionPlatRaise24Change
//
static bool EV_ActionPlatRaise24Change(ev_action_t *action,
                                       specialactivation_t *activation)
{
   // case 143: (W1 - BOOM Extended)
   // case 148: (WR - BOOM Extended)
   // Raise Floor 24 and change
   return !!EV_DoPlat(activation->line, raiseAndChange, 24);
}

//
// EV_ActionPlatRaise32Change
//
static bool EV_ActionPlatRaise32Change(ev_action_t *action,
                                       specialactivation_t *activation)
{
   // case 144: (W1 - BOOM Extended)
   // case 149: (WR - BOOM Extended)
   // Raise Floor 32 and change
   return !!EV_DoPlat(activation->line, raiseAndChange, 32);
}

//
// EV_ActionCeilingLowerToFloor
//
static bool EV_ActionCeilingLowerToFloor(ev_action_t *action,
                                         specialactivation_t *activation)
{
   // case 145: (W1 - BOOM Extended)
   // case 152: (WR - BOOM Extended)
   // Lower Ceiling to Floor
   return !!EV_DoCeiling(activation->line, lowerToFloor);
}

//
// EV_ActionDoDonut
//
static bool EV_ActionDoDonut(ev_action_t *action, 
                             specialactivation_t *activation)
{
   // case 146: (W1 - BOOM Extended)
   // case 155: (WR - BOOM Extended)
   // Lower Pillar, Raise Donut
   return !!EV_DoDonut(activation->line);
}

//
// EV_ActionCeilingLowerToLowest
//
static bool EV_ActionCeilingLowerToLowest(ev_action_t *action,
                                          specialactivation_t *activation)
{
   // case 199: (W1 - BOOM Extended)
   // case 201: (WR - BOOM Extended)
   // Lower ceiling to lowest surrounding ceiling
   return !!EV_DoCeiling(activation->line, lowerToLowest);
}

//
// EV_ActionCeilingLowerToMaxFloor
//
static bool EV_ActionCeilingLowerToMaxFloor(ev_action_t *action,
                                            specialactivation_t *activation)
{
   // case 200: (W1 - BOOM Extended)
   // case 202: (WR - BOOM Extended)
   // Lower ceiling to highest surrounding floor
   return !!EV_DoCeiling(activation->line, lowerToMaxFloor);
}

//
// EV_ActionSilentTeleport
//
static bool EV_ActionSilentTeleport(ev_action_t *action,
                                    specialactivation_t *activation)
{
   line_t *line  = activation->line;
   int     side  = activation->side;
   Mobj   *thing = activation->actor;

   // case 207: (W1 - BOOM Extended)
   // case 208: (WR - BOOM Extended)
   // killough 2/16/98: W1 silent teleporter (normal kind)
   return !!EV_SilentTeleport(line, side, thing);
}

//
// EV_ActionChangeOnly
//
static bool EV_ActionChangeOnly(ev_action_t *action,
                                specialactivation_t *activation)
{
   // jff 3/16/98 renumber 215->153
   // jff 3/15/98 create texture change no motion type
   // case 153: (W1 - BOOM Extended)
   // case 154: (WR - BOOM Extended)
   // Texture/Type Change Only (Trig)
   return !!EV_DoChange(activation->line, trigChangeOnly);
}

//
// EV_ActionChangeOnlyNumeric
//
static bool EV_ActionChangeOnlyNumeric(ev_action_t *action,
                                       specialactivation_t *activation)
{
   //jff 3/15/98 create texture change no motion type
   // case 239: (W1 - BOOM Extended)
   // case 240: (WR - BOOM Extended)
   // Texture/Type Change Only (Numeric)
   return !!EV_DoChange(activation->line, numChangeOnly);
}

//
// EV_ActionFloorLowerToNearest
//
static bool EV_ActionFloorLowerToNearest(ev_action_t *action,
                                         specialactivation_t *activation)
{
   // case 219: (W1 - BOOM Extended)
   // case 220: (WR - BOOM Extended)
   // Lower floor to next lower neighbor
   return !!EV_DoFloor(activation->line, lowerFloorToNearest);
}

//
// EV_ActionElevatorUp
//
static bool EV_ActionElevatorUp(ev_action_t *action,
                                specialactivation_t *activation)
{
   // case 227: (W1 - BOOM Extended)
   // case 228: (WR - BOOM Extended)
   // Raise elevator next floor
   return !!EV_DoElevator(activation->line, elevateUp);
}

//
// EV_ActionElevatorDown
//
static bool EV_ActionElevatorDown(ev_action_t *action,
                                  specialactivation_t *activation)
{
   // case 231: (W1 - BOOM Extended)
   // case 232: (WR - BOOM Extended)
   // Lower elevator next floor
   return !!EV_DoElevator(activation->line, elevateDown);

}

//
// EV_ActionElevatorCurrent
//
static bool EV_ActionElevatorCurrent(ev_action_t *action,
                                     specialactivation_t *activation)
{
   // case 235: (W1 - BOOM Extended)
   // case 236: (WR - BOOM Extended)
   // Elevator to current floor
   return !!EV_DoElevator(activation->line, elevateCurrent);
}

//
// EV_ActionSilentLineTeleport
//
static bool EV_ActionSilentLineTeleport(ev_action_t *action,
                                        specialactivation_t *activation)
{
   line_t *line  = activation->line;
   int     side  = activation->side;
   Mobj   *thing = activation->actor;
  
   // case 243: (W1 - BOOM Extended)
   // case 244: (WR - BOOM Extended)
   // jff 3/6/98 make fit within DCK's 256 linedef types
   // killough 2/16/98: W1 silent teleporter (linedef-linedef kind)
   return !!EV_SilentLineTeleport(line, side, thing, false);
}

//
// EV_ActionSilentLineTeleportReverse
//
static bool EV_ActionSilentLineTeleportReverse(ev_action_t *action,
                                               specialactivation_t *activation)
{
   line_t *line  = activation->line;
   int     side  = activation->side;
   Mobj   *thing = activation->actor;

   // case 262: (W1 - BOOM Extended)
   // jff 4/14/98 add silent line-line reversed
   return !!EV_SilentLineTeleport(line, side, thing, true);
}

//
// EV_ActionSilentLineTRMonster
//
static bool EV_ActionSilentLineTRMonster(ev_action_t *action,
                                         specialactivation_t *activation)
{
   line_t *line  = activation->line;
   int     side  = activation->side;
   Mobj   *thing = activation->actor;
   
   // case 264: (W1 - BOOM Extended)
   // jff 4/14/98 add monster-only silent line-line reversed
   return (!thing->player && EV_SilentLineTeleport(line, side, thing, true));
}

//
// EV_ActionSilentLineTeleMonster
//
static bool EV_ActionSilentLineTeleMonster(ev_action_t *action,
                                           specialactivation_t *activation)
{
   line_t *line  = activation->line;
   int     side  = activation->side;
   Mobj   *thing = activation->actor;
   
   // case 266: (W1 - BOOM Extended)
   // jff 4/14/98 add monster-only silent line-line
   return (!thing->player && EV_SilentLineTeleport(line, side, thing, false));
}

//
// EV_ActionSilentTeleportMonster
//
static bool EV_ActionSilentTeleportMonster(ev_action_t *action,
                                           specialactivation_t *activation)
{
   line_t *line  = activation->line;
   int     side  = activation->side;
   Mobj   *thing = activation->actor;

   // case 268: (W1 - BOOM Extended)
   // jff 4/14/98 add monster-only silent
   return (!thing->player && EV_SilentTeleport(line, side, thing));
}

//
// EV_ActionPlatToggleUpDown
//
static bool EV_ActionPlatToggleUpDown(ev_action_t *action,
                                      specialactivation_t *activation)
{
   // jff 3/14/98 create instant toggle floor type
   // case 212: (WR - BOOM Extended)
   return !!EV_DoPlat(activation->line, toggleUpDn, 0);
}

//=============================================================================
//
// DOOM Line Actions
//

// Macro to declare a DOOM-style W1 line action
#define DOOM_W1CROSSLINE(name, flags)    \
   static ev_action_t actionW1 ## name = \
   {                                     \
      SPAC_CROSS,                        \
      EV_DOOMPreCrossLine,               \
      EV_Action ## name,                 \
      EV_DOOMPostCrossLine,              \
      (flags) | EV_POSTCLEARSPECIAL,     \
      0                                  \
   }

// Macro to declare a DOOM-style WR line action
#define DOOM_WRCROSSLINE(name, flags)    \
   static ev_action_t actionWR ## name = \
   {                                     \
      SPAC_CROSS,                        \
      EV_DOOMPreCrossLine,               \
      EV_Action ## name,                 \
      EV_DOOMPostCrossLine,              \
      flags,                             \
      0                                  \
   }

// jff 1/29/98 added new linedef types to fill all functions out so that
// all have varieties SR, S1, WR, W1

// killough 1/31/98: "factor out" compatibility test, by adding inner switch 
// qualified by compatibility flag. relax test to demo_compatibility

// killough 2/16/98: Fix problems with W1 types being cleared too early

// Macro to declare a BOOM extended W1 line action
#define BOOM_W1CROSSLINE(name, flags)    \
   static ev_action_t actionW1 ## name = \
   {                                     \
      SPAC_CROSS,                        \
      EV_DOOMPreCrossLine,               \
      EV_Action ## name,                 \
      EV_DOOMPostCrossLine,              \
      (flags) | EV_POSTCLEARSPECIAL,     \
      200                                \
   }

// Macro to declare a BOOM extended WR line action
#define BOOM_WRCROSSLINE(name, flags)    \
   static ev_action_t actionWR ## name = \
   {                                     \
      SPAC_CROSS,                        \
      EV_DOOMPreCrossLine,               \
      EV_Action ## name,                 \
      EV_DOOMPostCrossLine,              \
      flags,                             \
      200                                \
   }

// DOOM Line Type 2 - W1 Open Door
DOOM_W1CROSSLINE(OpenDoor, 0);

// DOOM Line Type 3 - W1 Close Door
DOOM_W1CROSSLINE(CloseDoor, 0);

// DOOM Line Type 4 - W1 Raise Door
DOOM_W1CROSSLINE(RaiseDoor, EV_PREALLOWMONSTERS);

// DOOM Line Type 5 - W1 Raise Floor
DOOM_W1CROSSLINE(RaiseFloor, 0);

// DOOM Line Type 6 - W1 Ceiling Fast Crush and Raise
DOOM_W1CROSSLINE(FastCeilCrushRaise, 0);

// DOOM Line Type 8 - W1 Build Stairs Up 8
DOOM_W1CROSSLINE(BuildStairsUp8, 0);

// DOOM Line Type 10 - W1 Plat Down-Wait-Up-Stay
DOOM_W1CROSSLINE(PlatDownWaitUpStay, EV_PREALLOWMONSTERS);

// DOOM Line Type 12 - W1 Light Turn On
DOOM_W1CROSSLINE(LightTurnOn, EV_PREALLOWZEROTAG);

// DOOM Line Type 13 - W1 Light Turn On 255
DOOM_W1CROSSLINE(LightTurnOn255, EV_PREALLOWZEROTAG);

// DOOM Line Type 16 - W1 Close Door, Open in 30
DOOM_W1CROSSLINE(CloseDoor30, 0);

// DOOM Line Type 17 - W1 Start Light Strobing
DOOM_W1CROSSLINE(StartLightStrobing, EV_PREALLOWZEROTAG);

// DOOM Line Type 19 - W1 Lower Floor
DOOM_W1CROSSLINE(LowerFloor, 0);

// DOOM Line Type 22 - W1 Plat Raise to Nearest and Change
DOOM_W1CROSSLINE(PlatRaiseNearestChange, 0);

// DOOM Line Type 25 - W1 Ceiling Crush and Raise
DOOM_W1CROSSLINE(CeilingCrushAndRaise, 0);

// DOOM Line Type 30 - W1 Floor Raise To Texture
DOOM_W1CROSSLINE(FloorRaiseToTexture, 0);

// DOOM Line Type 35 - W1 Lights Very Dark
DOOM_W1CROSSLINE(LightsVeryDark, EV_PREALLOWZEROTAG);

// DOOM Line Type 36 - W1 Lower Floor Turbo
DOOM_W1CROSSLINE(LowerFloorTurbo, 0);

// DOOM Line Type 37 - W1 Floor Lower and Change
DOOM_W1CROSSLINE(FloorLowerAndChange, 0);

// DOOM Line Type 38 - W1 Floor Lower to Lowest
DOOM_W1CROSSLINE(FloorLowerToLowest, 0);

// DOOM Line Type 39 - W1 Teleport
DOOM_W1CROSSLINE(Teleport, EV_PREALLOWMONSTERS | EV_PREALLOWZEROTAG);

// DOOM Line Type 40 - W1 Raise Ceiling Lower Floor (only raises ceiling)
DOOM_W1CROSSLINE(RaiseCeilingLowerFloor, 0);

// DOOM Line Type 44 - W1 Ceiling Crush
DOOM_W1CROSSLINE(CeilingLowerAndCrush, 0);

// DOOM Line Type 52 - WR Exit Level
DOOM_WRCROSSLINE(ExitLevel, EV_PREALLOWZEROTAG);

// DOOM Line Type 53 - W1 Perpetual Platform Raise
DOOM_W1CROSSLINE(PlatPerpetualRaise, 0);

// DOOM Line Type 54 - W1 Platform Stop
DOOM_W1CROSSLINE(PlatStop, 0);

// DOOM Line Type 56 - W1 Floor Raise & Crush
DOOM_W1CROSSLINE(FloorRaiseCrush, 0);

// DOOM Line Type 57 - W1 Ceiling Crush Stop
DOOM_W1CROSSLINE(CeilingCrushStop, 0);

// DOOM Line Type 58 - W1 Raise Floor 24
DOOM_W1CROSSLINE(RaiseFloor24, 0);

// DOOM Line Type 59 - W1 Raise Floor 24 and Change
DOOM_W1CROSSLINE(RaiseFloor24Change, 0);

// DOOM Line Type 72 - WR Ceiling Crush
DOOM_WRCROSSLINE(CeilingLowerAndCrush, 0);

// DOOM Line Type 73 - WR Ceiling Crush and Raise
DOOM_WRCROSSLINE(CeilingCrushAndRaise, 0);

// DOOM Line Type 74 - WR Ceiling Crush Stop
DOOM_WRCROSSLINE(CeilingCrushStop, 0);

// DOOM Line Type 75 - WR Door Close
DOOM_WRCROSSLINE(CloseDoor, 0);

// DOOM Line Type 76 - WR Close Door 30
DOOM_WRCROSSLINE(CloseDoor30, 0);

// DOOM Line Type 77 - WR Fast Ceiling Crush & Raise
DOOM_WRCROSSLINE(FastCeilCrushRaise, 0);

// DOOM Line Type 79 - WR Lights Very Dark
DOOM_WRCROSSLINE(LightsVeryDark, EV_PREALLOWZEROTAG);

// DOOM Line Type 80 - WR Light Turn On - Brightest Near
DOOM_WRCROSSLINE(LightTurnOn, EV_PREALLOWZEROTAG);

// DOOM Line Type 81 - WR Light Turn On 255
DOOM_WRCROSSLINE(LightTurnOn255, EV_PREALLOWZEROTAG);

// DOOM Line Type 82 - WR Lower Floor To Lowest
DOOM_WRCROSSLINE(FloorLowerToLowest, 0);

// DOOM Line Type 83 - WR Lower Floor
DOOM_WRCROSSLINE(LowerFloor, 0);

// DOOM Line Type 84 - WR Floor Lower and Change
DOOM_WRCROSSLINE(FloorLowerAndChange, 0);

// DOOM Line Type 86 - WR Open Door
DOOM_WRCROSSLINE(OpenDoor, 0);

// DOOM Line Type 87 - WR Plat Perpetual Raise
DOOM_WRCROSSLINE(PlatPerpetualRaise, 0);

// DOOM Line Type 88 - WR Plat DWUS
DOOM_WRCROSSLINE(PlatDownWaitUpStay, EV_PREALLOWMONSTERS);

// DOOM Line Type 89 - WR Platform Stop
DOOM_WRCROSSLINE(PlatStop, 0);

// DOOM Line Type 90 - WR Raise Door
DOOM_WRCROSSLINE(RaiseDoor, 0);

// DOOM Line Type 91 - WR Raise Floor
DOOM_WRCROSSLINE(RaiseFloor, 0);

// DOOM Line Type 92 - WR Raise Floor 24
DOOM_WRCROSSLINE(RaiseFloor24, 0);

// DOOM Line Type 93 - WR Raise Floor 24 and Change
DOOM_WRCROSSLINE(RaiseFloor24Change, 0);

// DOOM Line Type 94 - WR Raise Floor and Crush
DOOM_WRCROSSLINE(FloorRaiseCrush, 0);

// DOOM Line Type 95 - WR Plat Raise to Nearest & Change
DOOM_WRCROSSLINE(PlatRaiseNearestChange, 0);

// DOOM Line Type 96 - WR Floor Raise to Texture
DOOM_WRCROSSLINE(FloorRaiseToTexture, 0);

// DOOM Line Type 97 - WR Teleport
DOOM_WRCROSSLINE(Teleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS);

// DOOM Line Type 98 - WR Lower Floor Turbo
DOOM_WRCROSSLINE(LowerFloorTurbo, 0);

// DOOM Line Type 100 - W1 Build Stairs Turbo 16
DOOM_W1CROSSLINE(BuildStairsTurbo16, 0);

// DOOM Line Type 104 - W1 Turn Lights Off in Sector
DOOM_W1CROSSLINE(TurnTagLightsOff, EV_PREALLOWZEROTAG);

// DOOM Line Type 105 - WR Door Blaze Raise
DOOM_WRCROSSLINE(DoorBlazeRaise, 0);

// DOOM Line Type 106 - WR Door Blaze Open
DOOM_WRCROSSLINE(DoorBlazeOpen, 0);

// DOOM Line Type 107 - WR Door Blaze Close
DOOM_WRCROSSLINE(DoorBlazeClose, 0);

// DOOM Line Type 108 - W1 Blazing Door Raise
DOOM_W1CROSSLINE(DoorBlazeRaise, 0);

// DOOM Line Type 109 - W1 Blazing Door Open
DOOM_W1CROSSLINE(DoorBlazeOpen, 0);

// DOOM Line Type 110 - W1 Blazing Door Close
DOOM_W1CROSSLINE(DoorBlazeClose, 0);

// DOOM Line Type 119 - W1 Raise Floor to Nearest Floor
DOOM_W1CROSSLINE(FloorRaiseToNearest, 0);

// DOOM Line Type 120 - WR Plat Blaze DWUS
DOOM_WRCROSSLINE(PlatBlazeDWUS, 0);

// DOOM Line Type 121 - W1 Plat Blaze Down-Wait-Up-Stay
DOOM_W1CROSSLINE(PlatBlazeDWUS, 0);

// DOOM Line Type 124 - WR Secret Exit
DOOM_WRCROSSLINE(SecretExit, EV_PREALLOWZEROTAG);

// DOOM Line Type 125 - W1 Teleport Monsters Only
// jff 3/5/98 add ability of monsters etc. to use teleporters
DOOM_W1CROSSLINE(TeleportMonsterOnly, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS);

// DOOM Line Type 126 - WR Teleport Monsters Only
DOOM_WRCROSSLINE(TeleportMonsterOnly, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS);

// DOOM Line Type 128 - WR Raise Floor to Nearest Floor
DOOM_WRCROSSLINE(FloorRaiseToNearest, 0);

// DOOM Line Type 129 - WR Raise Floor Turbo
DOOM_WRCROSSLINE(RaiseFloorTurbo, 0);

// DOOM Line Type 130 - W1 Raise Floor Turbo
DOOM_W1CROSSLINE(RaiseFloorTurbo, 0);

// DOOM Line Type 141 - W1 Ceiling Silent Crush & Raise
DOOM_W1CROSSLINE(SilentCrushAndRaise, 0);

// BOOM Extended Line Type 142 - W1 Raise Floor 512
BOOM_W1CROSSLINE(RaiseFloor512, 0);

// BOOM Extended Line Type 143 - W1 Plat Raise 24 and Change
BOOM_W1CROSSLINE(PlatRaise24Change, 0);

// BOOM Extended Line Type 144 - W1 Plat Raise 32 and Change
BOOM_W1CROSSLINE(PlatRaise32Change, 0);

// BOOM Extended Line Type 145 - W1 Ceiling Lower to Floor
BOOM_W1CROSSLINE(CeilingLowerToFloor, 0);

// BOOM Extended Line Type 146 - W1 Donut
BOOM_W1CROSSLINE(DoDonut, 0);

// BOOM Extended Line Type 147 - WR Raise Floor 512
BOOM_WRCROSSLINE(RaiseFloor512, 0);

// BOOM Extended Line Type 148 - WR Raise Floor 24 and Change
BOOM_WRCROSSLINE(PlatRaise24Change, 0);

// BOOM Extended Line Type 149 - WR Raise Floor 32 and Change
BOOM_WRCROSSLINE(PlatRaise32Change, 0);

// BOOM Extended Line Type 150 - WR Start Slow Silent Crusher
BOOM_WRCROSSLINE(SilentCrushAndRaise, 0);

// BOOM Extended Line Type 151 - WR Raise Ceiling, Lower Floor
BOOM_WRCROSSLINE(BOOMRaiseCeilingLowerFloor, 0);

// BOOM Extended Line Type 152 - WR Lower Ceiling to Floor
BOOM_WRCROSSLINE(CeilingLowerToFloor, 0);

// BOOM Extended Line Type 153 - W1 Change Texture/Type
BOOM_W1CROSSLINE(ChangeOnly, 0);

// BOOM Extended Line Type 154 - WR Change Texture/Type
// jff 3/16/98 renumber 216->154
BOOM_WRCROSSLINE(ChangeOnly, 0);

// BOOM Extended Line Type 155 - WR Lower Pillar, Raise Donut
BOOM_WRCROSSLINE(DoDonut, 0);

// BOOM Extended Line Type 156 - WR Start Lights Strobing
BOOM_WRCROSSLINE(StartLightStrobing, EV_PREALLOWZEROTAG);

// BOOM Extended Line Type 157 - WR Lights to Dimmest Near
BOOM_WRCROSSLINE(TurnTagLightsOff, EV_PREALLOWZEROTAG);

// BOOM Extended Line Type 199 - W1 Ceiling Lower to Lowest Surr. Ceiling
BOOM_W1CROSSLINE(CeilingLowerToLowest, 0);

// BOOM Extended Line Type 200 - W1 Ceiling Lower to Max Floor
BOOM_W1CROSSLINE(CeilingLowerToMaxFloor, 0);

// BOOM Extended Line Type 201 - WR Lower Ceiling to Lowest Surr. Ceiling
BOOM_WRCROSSLINE(CeilingLowerToLowest, 0);

// BOOM Extended Line Type 202 - WR Lower Ceiling to Max Floor
BOOM_WRCROSSLINE(CeilingLowerToMaxFloor, 0);

// BOOM Extended Line Type 207 - W1 Silent Teleport
BOOM_W1CROSSLINE(SilentTeleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS);

// BOOM Extended Line Type 208 - WR Silent Teleport
BOOM_WRCROSSLINE(SilentTeleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS);

// BOOM Extended Line Type 212 - WR Plat Toggle Floor Instant C/F
BOOM_WRCROSSLINE(PlatToggleUpDown, 0);

// BOOM Extended Line Type 219 - W1 Floor Lower to Next Lower Neighbor
BOOM_W1CROSSLINE(FloorLowerToNearest, 0);

// BOOM Extended Line Type 220 - WR Floor Lower to Next Lower Neighbor
BOOM_WRCROSSLINE(FloorLowerToNearest, 0);

// BOOM Extended Line Type 227 - W1 Raise Elevator to Next Floor
BOOM_W1CROSSLINE(ElevatorUp, 0);

// BOOM Extended Line Type 228 - WR Raise Elevator to Next Floor
BOOM_WRCROSSLINE(ElevatorUp, 0);

// BOOM Extended Line Type 231 - W1 Lower Elevator to Next Floor
BOOM_W1CROSSLINE(ElevatorDown, 0);

// BOOM Extended Line Type 232 - WR Lower Elevator to Next Floor
BOOM_WRCROSSLINE(ElevatorDown, 0);

// BOOM Extended Line Type 235 - W1 Elevator to Current Floor
BOOM_W1CROSSLINE(ElevatorCurrent, 0);

// BOOM Extended Line Type 236 - WR Elevator to Current Floor
BOOM_WRCROSSLINE(ElevatorCurrent, 0);

// BOOM Extended Line Type 239 - W1 Change Texture/Type Numeric
BOOM_W1CROSSLINE(ChangeOnlyNumeric, 0);

// BOOM Extended Line Type 240 - WR Change Texture/Type Numeric
BOOM_WRCROSSLINE(ChangeOnlyNumeric, 0);

// BOOM Extended Line Type 243 - W1 Silent Teleport Line-to-Line
BOOM_W1CROSSLINE(SilentLineTeleport, EV_PREALLOWMONSTERS);

// BOOM Extended Line Type 244 - WR Silent Teleport Line-to-Line
BOOM_WRCROSSLINE(SilentLineTeleport, EV_PREALLOWMONSTERS);

// BOOM Extended Line Type 262 - W1 Silent Teleport Line-to-Line Reversed
BOOM_W1CROSSLINE(SilentLineTeleportReverse, EV_PREALLOWMONSTERS);

// BOOM Extended Line Type 264 - W1 Silent Line Teleport Monsters Only Reversed
BOOM_W1CROSSLINE(SilentLineTRMonster, EV_PREALLOWMONSTERS);

// BOOM Extended Line Type 266 - W1 Silent Line Teleport Monsters Only
BOOM_W1CROSSLINE(SilentLineTeleMonster, EV_PREALLOWMONSTERS);

// BOOM Extended Line Type 268 - W1 Silent Teleport Monster Only
BOOM_W1CROSSLINE(SilentTeleportMonster, EV_PREALLOWMONSTERS);

// BOOM Extended Line Type 256 - WR Build Stairs Up 8
// jff 3/16/98 renumber 153->256
BOOM_WRCROSSLINE(BuildStairsUp8, 0);

// BOOM Extended Line Type 257 - WR Build Stairs Up Turbo 16
// jff 3/16/98 renumber 154->257
BOOM_WRCROSSLINE(BuildStairsTurbo16, 0);

/*
         case 244: //jff 3/6/98 make fit within DCK's 256 linedef types
            // killough 2/16/98: WR silent teleporter (linedef-linedef kind)
            EV_SilentLineTeleport(line, side, thing, false);
            break;
*/
/*
void P_CrossSpecialLine(line_t *line, int side, Mobj *thing)
{
   //jff 02/04/98 add check here for generalized lindef types
   if(!demo_compatibility) // generalized types not recognized if old demo
   {
      // pointer to line function is NULL by default, set non-null if
      // line special is walkover generalized linedef type
      int (*linefunc)(line_t *)=NULL;

      // check each range of generalized linedefs
      if((unsigned int)line->special >= GenFloorBase)
      {
         if(!thing->player)
         {
            if((line->special & FloorChange) || 
               !(line->special & FloorModel))
               return;     // FloorModel is "Allow Monsters" if FloorChange is 0
         }
         if(!line->tag) //jff 2/27/98 all walk generalized types require tag
            return;
         linefunc = EV_DoGenFloor;
      }
      else if((unsigned int)line->special >= GenCeilingBase)
      {
         if(!thing->player)
         {
            if((line->special & CeilingChange) || !(line->special & CeilingModel))
               return;     // CeilingModel is "Allow Monsters" if CeilingChange is 0
         }
         if(!line->tag) //jff 2/27/98 all walk generalized types require tag
            return;
         linefunc = EV_DoGenCeiling;
      }
      else if((unsigned int)line->special >= GenDoorBase)
      {
         if (!thing->player)
         {
            if(!(line->special & DoorMonster))
               return;                    // monsters disallowed from this door
            if(line->flags & ML_SECRET) // they can't open secret doors either
               return;
         }
         if(!line->tag) //3/2/98 move outside the monster check
            return;
         genDoorThing = thing;
         linefunc = EV_DoGenDoor;
      }
      else if((unsigned int)line->special >= GenLockedBase)
      {
         if(!thing->player)
            return;                     // monsters disallowed from unlocking doors
         if(((line->special&TriggerType)==WalkOnce) || 
            ((line->special&TriggerType)==WalkMany))
         { //jff 4/1/98 check for being a walk type before reporting door type
            if(!P_CanUnlockGenDoor(line,thing->player))
               return;
         }
         else
            return;
         genDoorThing = thing;
         linefunc = EV_DoGenLockedDoor;
      }
      else if((unsigned int)line->special >= GenLiftBase)
      {
         if(!thing->player)
         {
            if(!(line->special & LiftMonster))
               return; // monsters disallowed
         }
         if(!line->tag) //jff 2/27/98 all walk generalized types require tag
            return;
         linefunc = EV_DoGenLift;
      }
      else if((unsigned int)line->special >= GenStairsBase)
      {
         if(!thing->player)
         {
            if(!(line->special & StairMonster))
               return; // monsters disallowed
         }
         if(!line->tag) //jff 2/27/98 all walk generalized types require tag
            return;
         linefunc = EV_DoGenStairs;
      }
      else if(demo_version >= 335 && (unsigned int)line->special >= GenCrusherBase)
      {
         // haleyjd 06/09/09: This was completely forgotten in BOOM, disabling
         // all generalized walk-over crusher types!

         if(!thing->player)
         {
            if(!(line->special & CrusherMonster))
               return; // monsters disallowed
         }
         if(!line->tag) //jff 2/27/98 all walk generalized types require tag
            return;
         linefunc = EV_DoGenCrusher;
      }

      if(linefunc) // if it was a valid generalized type
      {
         switch((line->special & TriggerType) >> TriggerTypeShift)
         {
         case WalkOnce:
            if(linefunc(line))
               line->special = 0;  // clear special if a walk once type
            return;
         case WalkMany:
            linefunc(line);
            return;
         default:              // if not a walk type, do nothing here
            return;
         }
      }
   }

   if(!thing->player)
   {
      ok = 0;
      switch(line->special)
      {
      case 244:     //jff 3/6/98 make fit within DCK's 256 linedef types
      case 263:     //jff 4/14/98 silent thing,line,line rev types
      case 265:     //            reversed types
      case 267:
      case 269:
         ok = 1;
         break;
      }
      if(!ok)
         return;
   }

   // Dispatch on the line special value to the line's action routine
   // If a once only function, and successful, clear the line special

   switch(line->special)
   {
   default:
      if(!demo_compatibility)
      {
         switch (line->special)
         {
            // Extended walk once triggers
           
            //jff 1/29/98 end of added W1 linedef types
            
            // Extended walk many retriggerable
            
            //jff 1/29/98 added new linedef types to fill all functions
            //out so that all have varieties SR, S1, WR, W1

         case 263: //jff 4/14/98 add silent line-line reversed
            EV_SilentLineTeleport(line, side, thing, true);
            break;
            
         case 265: //jff 4/14/98 add monster-only silent line-line reversed
            if(!thing->player)
               EV_SilentLineTeleport(line, side, thing, true);
            break;
            
         case 267: //jff 4/14/98 add monster-only silent line-line
            if(!thing->player)
               EV_SilentLineTeleport(line, side, thing, false);
            break;
            
         case 269: //jff 4/14/98 add monster-only silent
            if(!thing->player)
               EV_SilentTeleport(line, side, thing);
            break;
            //jff 1/29/98 end of added WR linedef types
            
            // scripting ld types
            
            // repeatable            
         case 273:  // WR start script 1-way
            if(side)
               break;            
         case 280:  // WR start script
            P_StartLineScript(line, thing);
            break;
                        
            // once-only triggers            
         case 275:  // W1 start script 1-way
            if(side)
               break;            
         case 274:  // W1 start script
            P_StartLineScript(line, thing);
            line->special = 0;        // clear trigger
            break;
         }
      }
      break;
   }
}
*/

// EOF

