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
#include "e_exdata.h"
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

inline static unsigned int EV_CompositeActionFlags(ev_action_t *action)
{
   return (action->type->flags | action->flags);
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
   unsigned int flags = EV_CompositeActionFlags(action);

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
   if(!activation->actor->player && !(flags & EV_PREALLOWMONSTERS))
      return false;

   // jff 2/27/98 disallow zero tag on some types
   // killough 11/98: compatibility option:
   if(!(activation->tag || comp[comp_zerotags] || 
        (action->flags & EV_PREALLOWZEROTAG)))
      return false;

   // check for first-side-only activation
   if(activation->side && (flags & EV_PREFIRSTSIDEONLY))
      return false;

   // line type is *only* for monsters?
   if(activation->actor->player && (flags & EV_PREMONSTERSONLY))
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
   unsigned int flags = EV_CompositeActionFlags(action);

   if(flags & EV_POSTCLEARSPECIAL)
   {
      bool clearSpecial = (result || P_ClearSwitchOnFail());

      if(clearSpecial || (flags & EV_POSTCLEARALWAYS))
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
   // case 125: (W1)
   // case 126: (WR)
   // TELEPORT MonsterONLY.
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
   // case 268: (W1 - BOOM Extended)
   // case 269: (WR - BOOM Extended)
   // jff 4/14/98 add monster-only silent
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
   // case 266: (W1 - BOOM Extended)
   // case 267: (WR - BOOM Extended)
   // jff 4/14/98 add monster-only silent line-line

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
   // case 263: (WR - BOOM Extended)
   // jff 4/14/98 add silent line-line reversed
   // case 264: (W1 - BOOM Extended)
   // case 265: (WR - BOOM Extended)
   // jff 4/14/98 add monster-only silent line-line reversed
   return !!EV_SilentLineTeleport(line, side, thing, true);
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

//
// EV_ActionStartLineScript
//
// SMMU script activation linedefs (now adapted to work for ACS)
//
static bool EV_ActionStartLineScript(ev_action_t *action,
                                     specialactivation_t *activation)
{
   P_StartLineScript(activation->line, activation->actor);
   return true;
}

//=============================================================================
//
// Action Types
//

// DOOM-Style Action Types

// WR-Type lines may be crossed multiple times
static ev_actiontype_t WRAction =
{
   SPAC_CROSS,           // line must be crossed
   EV_DOOMPreCrossLine,  // pre-activation callback
   EV_DOOMPostCrossLine, // post-activation callback
   0                     // no default flags
};

// W1-Type lines may be activated once. Semantics are implemented in the 
// post-cross callback to implement compatibility behaviors regarding the 
// premature clearing of specials crossed from the wrong side or without
// successful activation having occurred.
static ev_actiontype_t W1Action =
{
   SPAC_CROSS,           // line must be crossed
   EV_DOOMPreCrossLine,  // pre-activation callback
   EV_DOOMPostCrossLine, // post-activation callback
   EV_POSTCLEARSPECIAL   // special will be cleared after activation
};


//=============================================================================
//
// DOOM Line Actions
//

// Macro to declare a DOOM-style W1 line action
#define W1LINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &W1Action,                             \
      EV_Action ## action,                   \
      flags,                                 \
      version                                \
   }

// Macro to declare a DOOM-style WR line action
#define WRLINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &WRAction,                             \
      EV_Action ## action,                   \
      flags,                                 \
      version                                \
   }

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

// DOOM Line Type 8 - W1 Build Stairs Up 8
W1LINE(W1BuildStairsUp8, BuildStairsUp8, 0, 0);

// DOOM Line Type 10 - W1 Plat Down-Wait-Up-Stay
W1LINE(W1PlatDownWaitUpStay, PlatDownWaitUpStay, EV_PREALLOWMONSTERS, 0);

// DOOM Line Type 12 - W1 Light Turn On
W1LINE(W1LightTurnOn, LightTurnOn, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 13 - W1 Light Turn On 255
W1LINE(W1LightTurnOn255, LightTurnOn255, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 16 - W1 Close Door, Open in 30
W1LINE(W1CloseDoor30, CloseDoor30, 0, 0);

// DOOM Line Type 17 - W1 Start Light Strobing
W1LINE(W1StartLightStrobing, StartLightStrobing, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 19 - W1 Lower Floor
W1LINE(W1LowerFloor, LowerFloor, 0, 0);

// DOOM Line Type 22 - W1 Plat Raise to Nearest and Change
W1LINE(W1PlatRaiseNearestChange, PlatRaiseNearestChange, 0, 0);

// DOOM Line Type 25 - W1 Ceiling Crush and Raise
W1LINE(W1CeilingCrushAndRaise, CeilingCrushAndRaise, 0, 0);

// DOOM Line Type 30 - W1 Floor Raise To Texture
W1LINE(W1FloorRaiseToTexture, FloorRaiseToTexture, 0, 0);

// DOOM Line Type 35 - W1 Lights Very Dark
W1LINE(W1LightsVeryDark, LightsVeryDark, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 36 - W1 Lower Floor Turbo
W1LINE(W1LowerFloorTurbo, LowerFloorTurbo, 0, 0);

// DOOM Line Type 37 - W1 Floor Lower and Change
W1LINE(W1FloorLowerAndChange, FloorLowerAndChange, 0, 0);

// DOOM Line Type 38 - W1 Floor Lower to Lowest
W1LINE(W1FloorLowerToLowest, FloorLowerToLowest, 0, 0);

// DOOM Line Type 39 - W1 Teleport
W1LINE(W1Teleport, Teleport, EV_PREALLOWMONSTERS | EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 40 - W1 Raise Ceiling Lower Floor (only raises ceiling)
W1LINE(W1RaiseCeilingLowerFloor, RaiseCeilingLowerFloor, 0, 0);

// DOOM Line Type 44 - W1 Ceiling Crush
W1LINE(W1CeilingLowerAndCrush, CeilingLowerAndCrush, 0, 0);

// DOOM Line Type 52 - WR Exit Level
WRLINE(WRExitLevel, ExitLevel, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 53 - W1 Perpetual Platform Raise
W1LINE(W1PlatPerpetualRaise, PlatPerpetualRaise, 0, 0);

// DOOM Line Type 54 - W1 Platform Stop
W1LINE(W1PlatStop, PlatStop, 0, 0);

// DOOM Line Type 56 - W1 Floor Raise & Crush
W1LINE(W1FloorRaiseCrush, FloorRaiseCrush, 0, 0);

// DOOM Line Type 57 - W1 Ceiling Crush Stop
W1LINE(W1CeilingCrushStop, CeilingCrushStop, 0, 0);

// DOOM Line Type 58 - W1 Raise Floor 24
W1LINE(W1RaiseFloor24, RaiseFloor24, 0, 0);

// DOOM Line Type 59 - W1 Raise Floor 24 and Change
W1LINE(W1RaiseFloor24Change, RaiseFloor24Change, 0, 0);

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
WRLINE(WRPlatPerpetualRaise, PlatPerpetualRaise, 0, 0);

// DOOM Line Type 88 - WR Plat DWUS
WRLINE(WRPlatDownWaitUpStay, PlatDownWaitUpStay, EV_PREALLOWMONSTERS, 0);

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
WRLINE(WRPlatRaiseNearestChange, PlatRaiseNearestChange, 0, 0);

// DOOM Line Type 96 - WR Floor Raise to Texture
WRLINE(WRFloorRaiseToTexture, FloorRaiseToTexture, 0, 0);

// DOOM Line Type 97 - WR Teleport
WRLINE(WRTeleport, Teleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS, 0);

// DOOM Line Type 98 - WR Lower Floor Turbo
WRLINE(WRLowerFloorTurbo, LowerFloorTurbo, 0, 0);

// DOOM Line Type 100 - W1 Build Stairs Turbo 16
W1LINE(W1BuildStairsTurbo16, BuildStairsTurbo16, 0, 0);

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

// DOOM Line Type 119 - W1 Raise Floor to Nearest Floor
W1LINE(W1FloorRaiseToNearest, FloorRaiseToNearest, 0, 0);

// DOOM Line Type 120 - WR Plat Blaze DWUS
WRLINE(WRPlatBlazeDWUS, PlatBlazeDWUS, 0, 0);

// DOOM Line Type 121 - W1 Plat Blaze Down-Wait-Up-Stay
W1LINE(W1PlatBlazeDWUS, PlatBlazeDWUS, 0, 0);

// DOOM Line Type 124 - WR Secret Exit
WRLINE(WRSecretExit, SecretExit, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 125 - W1 Teleport Monsters Only
// jff 3/5/98 add ability of monsters etc. to use teleporters
W1LINE(W1TeleportMonsters, Teleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS | EV_PREMONSTERSONLY, 0);

// DOOM Line Type 126 - WR Teleport Monsters Only
WRLINE(WRTeleportMonsters, Teleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS | EV_PREMONSTERSONLY, 0);

// DOOM Line Type 128 - WR Raise Floor to Nearest Floor
WRLINE(WRFloorRaiseToNearest, FloorRaiseToNearest, 0, 0);

// DOOM Line Type 129 - WR Raise Floor Turbo
WRLINE(WRRaiseFloorTurbo, RaiseFloorTurbo, 0, 0);

// DOOM Line Type 130 - W1 Raise Floor Turbo
W1LINE(W1RaiseFloorTurbo, RaiseFloorTurbo, 0, 0);

// DOOM Line Type 141 - W1 Ceiling Silent Crush & Raise
W1LINE(W1SilentCrushAndRaise, SilentCrushAndRaise, 0, 0);

// BOOM Extended Line Type 142 - W1 Raise Floor 512
W1LINE(W1RaiseFloor512, RaiseFloor512, 0, 200);

// BOOM Extended Line Type 143 - W1 Plat Raise 24 and Change
W1LINE(W1PlatRaise24Change, PlatRaise24Change, 0, 200);

// BOOM Extended Line Type 144 - W1 Plat Raise 32 and Change
W1LINE(W1PlatRaise32Change, PlatRaise32Change, 0, 200);

// BOOM Extended Line Type 145 - W1 Ceiling Lower to Floor
W1LINE(W1CeilingLowerToFloor, CeilingLowerToFloor, 0, 200);

// BOOM Extended Line Type 146 - W1 Donut
W1LINE(W1DoDonut, DoDonut, 0, 200);

// BOOM Extended Line Type 147 - WR Raise Floor 512
WRLINE(WRRaiseFloor512, RaiseFloor512, 0, 200);

// BOOM Extended Line Type 148 - WR Raise Floor 24 and Change
WRLINE(WRPlatRaise24Change, PlatRaise24Change, 0, 200);

// BOOM Extended Line Type 149 - WR Raise Floor 32 and Change
WRLINE(WRPlatRaise32Change, PlatRaise32Change, 0, 200);

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

// BOOM Extended Line Type 199 - W1 Ceiling Lower to Lowest Surr. Ceiling
W1LINE(W1CeilingLowerToLowest, CeilingLowerToLowest, 0, 200);

// BOOM Extended Line Type 200 - W1 Ceiling Lower to Max Floor
W1LINE(W1CeilingLowerToMaxFloor, CeilingLowerToMaxFloor, 0, 200);

// BOOM Extended Line Type 201 - WR Lower Ceiling to Lowest Surr. Ceiling
WRLINE(WRCeilingLowerToLowest, CeilingLowerToLowest, 0, 200);

// BOOM Extended Line Type 202 - WR Lower Ceiling to Max Floor
WRLINE(WRCeilingLowerToMaxFloor, CeilingLowerToMaxFloor, 0, 200);

// BOOM Extended Line Type 207 - W1 Silent Teleport
W1LINE(W1SilentTeleport, SilentTeleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS, 200);

// BOOM Extended Line Type 208 - WR Silent Teleport
WRLINE(WRSilentTeleport, SilentTeleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS, 200);

// BOOM Extended Line Type 212 - WR Plat Toggle Floor Instant C/F
WRLINE(WRPlatToggleUpDown, PlatToggleUpDown, 0, 200);

// BOOM Extended Line Type 219 - W1 Floor Lower to Next Lower Neighbor
W1LINE(W1FloorLowerToNearest, FloorLowerToNearest, 0, 200);

// BOOM Extended Line Type 220 - WR Floor Lower to Next Lower Neighbor
WRLINE(WRFloorLowerToNearest, FloorLowerToNearest, 0, 200);

// BOOM Extended Line Type 227 - W1 Raise Elevator to Next Floor
W1LINE(W1ElevatorUp, ElevatorUp, 0, 200);

// BOOM Extended Line Type 228 - WR Raise Elevator to Next Floor
WRLINE(WRElevatorUp, ElevatorUp, 0, 200);

// BOOM Extended Line Type 231 - W1 Lower Elevator to Next Floor
W1LINE(W1ElevatorDown, ElevatorDown, 0, 200);

// BOOM Extended Line Type 232 - WR Lower Elevator to Next Floor
WRLINE(WRElevatorDown, ElevatorDown, 0, 200);

// BOOM Extended Line Type 235 - W1 Elevator to Current Floor
W1LINE(W1ElevatorCurrent, ElevatorCurrent, 0, 200);

// BOOM Extended Line Type 236 - WR Elevator to Current Floor
WRLINE(WRElevatorCurrent, ElevatorCurrent, 0, 200);

// BOOM Extended Line Type 239 - W1 Change Texture/Type Numeric
W1LINE(W1ChangeOnlyNumeric, ChangeOnlyNumeric, 0, 200);

// BOOM Extended Line Type 240 - WR Change Texture/Type Numeric
WRLINE(WRChangeOnlyNumeric, ChangeOnlyNumeric, 0, 200);

// BOOM Extended Line Type 243 - W1 Silent Teleport Line-to-Line
W1LINE(W1SilentLineTeleport, SilentLineTeleport, EV_PREALLOWMONSTERS, 200);

// BOOM Extended Line Type 244 - WR Silent Teleport Line-to-Line
WRLINE(WRSilentLineTleeport, SilentLineTeleport, EV_PREALLOWMONSTERS, 200);

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

// BOOM Extended Line Type 256 - WR Build Stairs Up 8
// jff 3/16/98 renumber 153->256
WRLINE(WRBuildStairsUp8, BuildStairsUp8, 0, 200);

// BOOM Extended Line Type 257 - WR Build Stairs Up Turbo 16
// jff 3/16/98 renumber 154->257
WRLINE(WRBuildStairsTurbo16, BuildStairsTurbo16, 0, 200);

// SMMU Extended Line Type 273 - WR Start Script One-Way
WRLINE(WRStartLineScript1S, StartLineScript, EV_PREFIRSTSIDEONLY | EV_PREALLOWZEROTAG, 300);

// SMMU Extended Line Type 274 - W1 Start Script
W1LINE(W1StartLineScript, StartLineScript, EV_PREALLOWZEROTAG, 300);

// SMMU Extended Line Type 275 - W1 Start Script One-Way
W1LINE(W1StartLineScript1S, StartLineScript, EV_PREFIRSTSIDEONLY | EV_PREALLOWZEROTAG, 300);

// SMMU Extended Line Type 280 - WR Start Script
WRLINE(WRStartLineScript, StartLineScript, EV_PREALLOWZEROTAG, 300);

//=============================================================================
//
// Special Bindings
//
// Each level type (DOOM, Heretic, Strife, Hexen) has its own set of line 
// specials. Heretic and Strife's sets are based on DOOM's with certain 
// additions (many of which conflict with BOOM extensions).
//


//
// EV_ActionForLineSpecial
//
// Given a special number, obtain the corresponding ev_action_t structure,
// within the currently defined set of bindings.
//
ev_action_t *EV_ActionForNumber(int special)
{
   return NULL; // TODO
}

//=============================================================================
//
// Activation
//

//
// EV_CheckSpac
//
// Checks against the activation characteristics of the action to see if this
// method of activating the line is allowed.
//
static bool EV_CheckSpac(ev_action_t *action, specialactivation_t *activation)
{
   if(action->type->activation >= 0) // specific type for this action?
   {
      return action->type->activation == activation->spac;
   }
   else // activation ability is determined by the linedef
   {
      Mobj   *thing = activation->actor;
      line_t *line  = activation->line;
      int     flags = 0;

      REQUIRE_ACTOR(thing);
      REQUIRE_LINE(line);

      // check player / monster / missile enable flags
      if(thing->player)                   // treat as player?
         flags |= EX_ML_PLAYER;
      if(thing->flags3 & MF3_SPACMISSILE) // treat as missile?
         flags |= EX_ML_MISSILE;
      if(thing->flags3 & MF3_SPACMONSTER) // treat as monster?
         flags |= EX_ML_MONSTER;

      if(!(line->extflags & flags))
         return false;

      // check 1S only flag -- if set, must be activated from first side
      if((line->extflags & EX_ML_1SONLY) && activation->side != 0)
         return false;

      // check activation flags -- can we activate this line this way?
      switch(activation->spac)
      {
      case SPAC_CROSS:
         flags = EX_ML_CROSS;
         break;
      case SPAC_USE:
         flags = EX_ML_USE;
         break;
      case SPAC_IMPACT:
         flags = EX_ML_IMPACT;
         break;
      case SPAC_PUSH:
         flags = EX_ML_PUSH;
         break;
      }

      return (line->extflags & flags) != 0;
   }
}

//
// EV_ActivateSpecialLine
//
// Shared logic for all types of line activation
//
bool EV_ActivateSpecialLine(ev_action_t *action, specialactivation_t *activation)
{
   // check for special activation
   if(!EV_CheckSpac(action, activation))
      return false;

   // execute pre-amble action
   if(!action->type->pre(action, activation))
      return false;

   bool result = action->action(action, activation);

   // execute the post-action routine
   return action->type->post(action, result, activation);
}

//
// EV_CrossSpecialLine
//
// A line has been activated by an Mobj physically crossing it.
//
bool EV_CrossSpecialLine(line_t *line, int side, Mobj *thing)
{
   ev_action_t *action;
   specialactivation_t activation;

   // get action
   if(!(action = EV_ActionForNumber(line->special)))
      return false;
   
   // setup activation
   activation.actor = thing;
   activation.args  = line->args;
   activation.line  = line;
   activation.side  = side;
   activation.spac  = SPAC_CROSS;
   activation.tag   = line->tag;

   return EV_ActivateSpecialLine(action, &activation);
}

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

   // Dispatch on the line special value to the line's action routine
   // If a once only function, and successful, clear the line special

}
*/

// EOF

