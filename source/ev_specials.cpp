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

#include "d_gi.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "ev_specials.h"
#include "g_game.h"
#include "p_mobj.h"
#include "p_skin.h"
#include "p_spec.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_state.h"
#include "s_sound.h"

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
   case 48:  // Scrolling walls
   case 51:
   
   case 85:

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

//
// EV_Check3DMidTexSwitch
//
// haleyjd 12/02/12: split off 3DMidTex switch range testing into its own
// independent routine.
//
static bool EV_Check3DMidTexSwitch(line_t *line, Mobj *thing, int side)
{
   int     sidenum = line->sidenum[side];
   side_t *sidedef = NULL;

   if(demo_version < 331)
      return true; // 3DMidTex don't exist in old demos

   if(sidenum != -1)
      sidedef = &sides[sidenum];

   // SoM: only allow switch specials on 3d sides to be triggered if 
   // the mobj is within range of the side.
   // haleyjd 05/02/06: ONLY on two-sided lines.
   if((line->flags & ML_3DMIDTEX) && line->backsector && 
      sidedef && sidedef->midtexture)
   {
      fixed_t opentop, openbottom, textop, texbot;

      opentop = line->frontsector->ceilingheight < line->backsector->ceilingheight ?
                line->frontsector->ceilingheight : line->backsector->ceilingheight;
      
      openbottom = line->frontsector->floorheight > line->backsector->floorheight ?
                   line->frontsector->floorheight : line->backsector->floorheight;

      if(line->flags & ML_DONTPEGBOTTOM)
      {
         texbot = sidedef->rowoffset + openbottom;
         textop = texbot + textures[sidedef->midtexture]->heightfrac;
      }
      else
      {
         textop = opentop + sidedef->rowoffset;
         texbot = textop - textures[sidedef->midtexture]->heightfrac;
      }

      if(thing->z > textop || thing->z + thing->height < texbot)
         return false;
   }

   return true;
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
static bool EV_DOOMPreCrossLine(ev_action_t *action, ev_instance_t *instance)
{
   unsigned int flags = EV_CompositeActionFlags(action);

   // DOOM-style specials require an actor and a line
   REQUIRE_ACTOR(instance->actor);
   REQUIRE_LINE(instance->line);

   // Things that should never trigger lines
   if(!instance->actor->player)
   {
      // haleyjd: changed to check against MF2_NOCROSS flag instead of 
      // switching on type.
      if(instance->actor->flags2 & MF2_NOCROSS)
         return false;
   }

   // Check if action only allows player
   // jff 3/5/98 add ability of monsters etc. to use teleporters
   if(!instance->actor->player && !(flags & EV_PREALLOWMONSTERS))
      return false;

   // jff 2/27/98 disallow zero tag on some types
   // killough 11/98: compatibility option:
   if(!(instance->tag || comp[comp_zerotags] || (flags & EV_PREALLOWZEROTAG)))
      return false;

   // check for first-side-only instance
   if(instance->side && (flags & EV_PREFIRSTSIDEONLY))
      return false;

   // line type is *only* for monsters?
   if(instance->actor->player && (flags & EV_PREMONSTERSONLY))
      return false;

   return true;
}

//
// EV_DOOMPostCrossLine
//
// This function is used as the post-action for all DOOM cross-type line
// specials.
//
static bool EV_DOOMPostCrossLine(ev_action_t *action, bool result,
                                 ev_instance_t *instance)
{
   unsigned int flags = EV_CompositeActionFlags(action);

   if(flags & EV_POSTCLEARSPECIAL)
   {
      bool clearSpecial = (result || P_ClearSwitchOnFail());

      if(clearSpecial || (flags & EV_POSTCLEARALWAYS))
         instance->line->special = 0;
   }

   return result;
}

//
// EV_DOOMPreUseLine
//
// Preamble for DOOM-type use (switch or manual) line types.
//
static bool EV_DOOMPreUseLine(ev_action_t *action, ev_instance_t *instance)
{
   unsigned int flags = EV_CompositeActionFlags(action);

   Mobj   *thing = instance->actor;
   line_t *line  = instance->line;

   // actor and line are required
   REQUIRE_ACTOR(thing);
   REQUIRE_LINE(line);

   // All DOOM-style use specials only support activation from the first side
   if(instance->side) 
      return false;

   // Check for 3DMidTex range restrictions
   if(!EV_Check3DMidTexSwitch(line, thing, instance->side))
      return false;

   if(!thing->player)
   {
      // Monsters never activate use specials on secret lines
      if(line->flags & ML_SECRET)
         return false;

      // Otherwise, check the special flags
      if(!(flags & EV_PREALLOWMONSTERS))
         return false;
   }

   // check for zero tag
   if(!(instance->tag || comp[comp_zerotags] || (flags & EV_PREALLOWZEROTAG)))
      return false;

   return true;
}

//
// EV_DOOMPostUseLine
//
// Post-activation semantics for DOOM-style use line actions.
//
static bool EV_DOOMPostUseLine(ev_action_t *action, bool result, 
                               ev_instance_t *instance)
{
   unsigned int flags = EV_CompositeActionFlags(action);

   // check for switch texture changes
   if(flags & EV_POSTCHANGESWITCH)
   {
      if(result || (flags & EV_POSTCHANGEALWAYS))
      {
         int useAgain = !(flags & EV_POSTCLEARSPECIAL);
         P_ChangeSwitchTexture(instance->line, useAgain, instance->side);
      }
   }

   return result;
}

//=============================================================================
//
// BOOM Generalized Pre- and Post-Actions
//
// You'd think these could all be very simply combined into each other, but,
// thanks to a reckless implementation by the BOOM team, not quite.
//

//
// EV_BOOMGenPreActivate
//
// Pre-activation logic for BOOM generalized line types
//
static bool EV_BOOMGenPreActivate(ev_action_t *action, ev_instance_t *instance)
{
   Mobj   *thing = instance->actor;
   line_t *line  = instance->line;
   
   REQUIRE_ACTOR(thing);
   REQUIRE_LINE(line);

   // check against zero tags
   if(!line->tag)
   {
      switch(instance->genspac)
      {
      case WalkOnce:
      case WalkMany:
         // jff 2/27/98 all walk generalized types require tag
         // haleyjd 12/01/12: except not, Jim, because you forgot locked doors -_-
         if(instance->gentype != GenTypeLocked)
            return false;
         break;
      case SwitchOnce:
      case SwitchMany:
      case PushOnce:
      case PushMany:
         // jff 3/2/98 all non-manual generalized types require tag
         if((line->special & 6) != 6)
            return false;
         break;
      case GunOnce:
      case GunMany:
         // jff 2/27/98 all gun generalized types require tag
         // haleyjd 12/01/12: except this time you forgot lifts :/
         if(instance->gentype != GenTypeLift)
            return false;
         break;
      default:
         break;
      }
   }

   // check whether monsters are allowed or not
   if(!thing->player)
   {
      switch(instance->gentype)
      {
      case GenTypeFloor:
         // FloorModel is "Allow Monsters" if FloorChange is 0
         if((line->special & FloorChange) || !(line->special & FloorModel))
            return false;
         break;
      case GenTypeCeiling:
         // CeilingModel is "Allow Monsters" if CeilingChange is 0
         if((line->special & CeilingChange) || !(line->special & CeilingModel))
            return false; 
         break;
      case GenTypeDoor:
         if(!(line->special & DoorMonster))
            return false;            // monsters disallowed from this door
         if(line->flags & ML_SECRET) // they can't open secret doors either
            return false;
         break;
      case GenTypeLocked:
         return false; // monsters disallowed from unlocking doors
      case GenTypeLift:
         if(!(line->special & LiftMonster))
            return false; // monsters disallowed
         break;
      case GenTypeStairs:
         if(!(line->special & StairMonster))
            return false; // monsters disallowed
         break;
      case GenTypeCrusher:
         if(!(line->special & CrusherMonster))
            return false; // monsters disallowed
         break;
      default:
         break;
      }
   }

   // check each range of generalized linedefs (special checks)
   switch(instance->gentype)
   {
   case GenTypeLocked:
      if(thing->player && !P_CanUnlockGenDoor(line, thing->player))
         return false;
      break;
   case GenTypeCrusher:
      // haleyjd 06/09/09: This was completely forgotten in BOOM, disabling
      // all generalized walk-over crusher types!
      if((instance->genspac == WalkOnce || instance->genspac == WalkMany)
         && demo_version < 335)
         return false;
      break;
   default:
      break;
   }

   // check for line side?
   // NB: BOOM code checked specially for Push types, but, that check was
   // redundant to one at the top of P_UseSpecialLine that applies to 
   // everything.
   if(instance->side)
   {
      switch(instance->genspac)
      {
      case PushOnce:
      case PushMany:
      case SwitchOnce:
      case SwitchMany:
         return false; // activate from first side only.
      default:
         break;
      }
   }

   return true;
}

//
// EV_BOOMGenPostActivate
//
// Post-activation logic for BOOM generalized line types
//
static bool EV_BOOMGenPostActivate(ev_action_t *action, bool result,
                                   ev_instance_t *instance)
{
   if(result)
   {
      switch(instance->genspac)
      {
      case GunOnce:
      case SwitchOnce:
         P_ChangeSwitchTexture(instance->line, 0, 0);
         break;
      case GunMany:
      case SwitchMany:
         P_ChangeSwitchTexture(instance->line, 1, 0);
         break;
      case WalkOnce:
      case PushOnce:
         instance->line->special = 0;
         break;
      default:
         break;
      }
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
static bool EV_ActionOpenDoor(ev_action_t *action, ev_instance_t *instance)
{
   // case 2:  (W1)
   // case 86: (WR)
   // Open Door
   return !!EV_DoDoor(instance->line, doorOpen);
}

//
// EV_ActionCloseDoor
//
static bool EV_ActionCloseDoor(ev_action_t *action, ev_instance_t *instance)
{
   // case 3:  (W1)
   // case 75: (WR)
   // Close Door
   return !!EV_DoDoor(instance->line, doorClose);
}

//
// EV_ActionRaiseDoor
//
static bool EV_ActionRaiseDoor(ev_action_t *action, ev_instance_t *instance)
{
   // case  4: (W1)
   // case 90: (WR)
   // Raise Door
   return !!EV_DoDoor(instance->line, doorNormal);
}

//
// EV_ActionRaiseFloor
//
static bool EV_ActionRaiseFloor(ev_action_t *action, ev_instance_t *instance)
{
   // case  5: (W1)
   // case 91: (WR)
   // Raise Floor
   return !!EV_DoFloor(instance->line, raiseFloor);
}

//
// EV_ActionFastCeilCrushRaise
//
static bool EV_ActionFastCeilCrushRaise(ev_action_t *action, ev_instance_t *instance)
{
   // case 6:  (W1)
   // case 77: (WR)
   // Fast Ceiling Crush & Raise
   return !!EV_DoCeiling(instance->line, fastCrushAndRaise);
}

//
// EV_ActionBuildStairsUp8
//
static bool EV_ActionBuildStairsUp8(ev_action_t *action, ev_instance_t *instance)
{
   // case 7:   (S1)
   // case 8:   (W1)
   // case 256: (WR - BOOM Extended)
   // Build Stairs
   return !!EV_BuildStairs(instance->line, build8);
}

//
// EV_ActionPlatDownWaitUpStay
//
static bool EV_ActionPlatDownWaitUpStay(ev_action_t *action, ev_instance_t *instance)
{
   // case 10: (W1)
   // case 88: (WR)
   // PlatDownWaitUp
   return !!EV_DoPlat(instance->line, downWaitUpStay, 0);
}

//
// EV_ActionLightTurnOn
//
static bool EV_ActionLightTurnOn(ev_action_t *action, ev_instance_t *instance)
{
   // case 12: (W1)
   // case 80: (WR)
   // Light Turn On - brightest near
   return !!EV_LightTurnOn(instance->line, 0);
}

//
// EV_ActionLightTurnOn255
//
static bool EV_ActionLightTurnOn255(ev_action_t *action, ev_instance_t *instance)
{
   // case 13: (W1)
   // case 81: (WR)
   // Light Turn On 255
   return !!EV_LightTurnOn(instance->line, 255);
}

//
// EV_ActionCloseDoor30
//
static bool EV_ActionCloseDoor30(ev_action_t *action, ev_instance_t *instance)
{
   // case 16: (W1)
   // case 76: (WR)
   // Close Door 30
   return !!EV_DoDoor(instance->line, closeThenOpen);
}

//
// EV_ActionStartLightStrobing
//
static bool EV_ActionStartLightStrobing(ev_action_t *action, ev_instance_t *instance)
{
   // case 17:  (W1)
   // case 156: (WR - BOOM Extended)
   // Start Light Strobing
   return !!EV_StartLightStrobing(instance->line);
}

//
// EV_ActionLowerFloor
//
static bool EV_ActionLowerFloor(ev_action_t *action, ev_instance_t *instance)
{
   // case 19: (W1)
   // case 83: (WR)
   // Lower Floor
   return !!EV_DoFloor(instance->line, lowerFloor);
}

//
// EV_ActionPlatRaiseNearestChange
//
static bool EV_ActionPlatRaiseNearestChange(ev_action_t *action, ev_instance_t *instance)
{
   // case 22: (W1)
   // case 95: (WR)
   // Raise floor to nearest height and change texture
   return !!EV_DoPlat(instance->line, raiseToNearestAndChange, 0);
}

//
// EV_ActionCeilingCrushAndRaise
//
static bool EV_ActionCeilingCrushAndRaise(ev_action_t *action, ev_instance_t *instance)
{
   // case 25: (W1)
   // case 73: (WR)
   // Ceiling Crush and Raise
   return !!EV_DoCeiling(instance->line, crushAndRaise);
}

//
// EV_ActionFloorRaiseToTexture
//
static bool EV_ActionFloorRaiseToTexture(ev_action_t *action, ev_instance_t *instance)
{
   // case 30: (W1)
   // case 96: (WR)
   // Raise floor to shortest texture height on either side of lines.
   return !!EV_DoFloor(instance->line, raiseToTexture);
}

//
// EV_ActionLightsVeryDark
//
static bool EV_ActionLightsVeryDark(ev_action_t *action, ev_instance_t *instance)
{
   // case 35: (W1)
   // case 79: (WR)
   // Lights Very Dark
   return !!EV_LightTurnOn(instance->line, 35);
}

//
// EV_ActionLowerFloorTurbo
//
static bool EV_ActionLowerFloorTurbo(ev_action_t *action, ev_instance_t *instance)
{
   // case 36: (W1)
   // case 98: (WR)
   // Lower Floor (TURBO)
   return !!EV_DoFloor(instance->line, turboLower);
}

//
// EV_ActionFloorLowerAndChange
//
static bool EV_ActionFloorLowerAndChange(ev_action_t *action, ev_instance_t *instance)
{
   // case 37: (W1)
   // case 84: (WR)
   // LowerAndChange
   return !!EV_DoFloor(instance->line, lowerAndChange);
}

//
// EV_ActionFloorLowerToLowest
//
static bool EV_ActionFloorLowerToLowest(ev_action_t *action, ev_instance_t *instance)
{
   // case 38: (W1)
   // case 82: (WR)
   // Lower Floor To Lowest
   return !!EV_DoFloor(instance->line, lowerFloorToLowest);
}

//
// EV_ActionTeleport
//
static bool EV_ActionTeleport(ev_action_t *action, ev_instance_t *instance)
{
   // case 39: (W1)
   // case 97: (WR)
   // TELEPORT!
   // case 125: (W1)
   // case 126: (WR)
   // TELEPORT MonsterONLY.
   // jff 02/09/98 fix using up with wrong side crossing
   return !!EV_Teleport(instance->line, instance->side, instance->actor);
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
static bool EV_ActionRaiseCeilingLowerFloor(ev_action_t *action, ev_instance_t *instance)
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
      return !!EV_DoCeiling(line, raiseToHighest);
}

//
// EV_ActionBOOMRaiseCeilingLowerFloor
//
// This version on the other hand works unconditionally, and is a BOOM
// extension.
//
static bool EV_ActionBOOMRaiseCeilingLowerFloor(ev_action_t *action, 
                                                ev_instance_t *instance)
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
// EV_ActionCeilingLowerAndCrush
//
static bool EV_ActionCeilingLowerAndCrush(ev_action_t *action, ev_instance_t *instance)
{
   // case 44: (W1)
   // case 72: (WR)
   // Ceiling Crush
   return !!EV_DoCeiling(instance->line, lowerAndCrush);
}

//
// EV_ActionExitLevel
//
static bool EV_ActionExitLevel(ev_action_t *action, ev_instance_t *instance)
{
   Mobj *thing = instance->actor;

   // case 52:
   // EXIT!
   // killough 10/98: prevent zombies from exiting levels
   if(!(thing->player && thing->player->health <= 0 && !comp[comp_zombie]))
      G_ExitLevel();

   return true;
}

//
// EV_ActionSwitchExitLevel
//
// This version of level exit logic is used for switch-type exits.
//
static bool EV_ActionSwitchExitLevel(ev_action_t *action, ev_instance_t *instance)
{
   Mobj *thing = instance->actor;

   // case 11:
   // Exit level
   // killough 10/98: prevent zombies from exiting levels
   if(thing->player && thing->player->health <= 0 && !comp[comp_zombie])
   {
      S_StartSound(thing, GameModeInfo->playerSounds[sk_oof]);
      return false;
   }

   G_ExitLevel();
   return true;
}

//
// EV_ActionPlatPerpetualRaise
//
static bool EV_ActionPlatPerpetualRaise(ev_action_t *action, ev_instance_t *instance)
{
   // case 53: (W1)
   // case 87: (WR)
   // Perpetual Platform Raise
   return !!EV_DoPlat(instance->line, perpetualRaise, 0);
}

//
// EV_ActionPlatStop
//
static bool EV_ActionPlatStop(ev_action_t *action, ev_instance_t *instance)
{
   // case 54: (W1)
   // case 89: (WR)
   // Platform Stop
   return !!EV_StopPlat(instance->line);
}

//
// EV_ActionFloorRaiseCrush
//
static bool EV_ActionFloorRaiseCrush(ev_action_t *action, ev_instance_t *instance)
{
   // case 56: (W1)
   // case 94: (WR)
   // Raise Floor Crush
   return !!EV_DoFloor(instance->line, raiseFloorCrush);
}

//
// EV_ActionCeilingCrushStop
//
static bool EV_ActionCeilingCrushStop(ev_action_t *action, ev_instance_t *instance)
{
   // case 57: (W1)
   // case 74: (WR)
   // Ceiling Crush Stop
   return !!EV_CeilingCrushStop(instance->line);
}

//
// EV_ActionRaiseFloor24
//
static bool EV_ActionRaiseFloor24(ev_action_t *action, ev_instance_t *instance)
{
   // case 58: (W1)
   // case 92: (WR)
   // Raise Floor 24
   return !!EV_DoFloor(instance->line, raiseFloor24);
}

//
// EV_ActionRaiseFloor24Change
//
static bool EV_ActionRaiseFloor24Change(ev_action_t *action, ev_instance_t *instance)
{
   // case 59: (W1)
   // case 93: (WR)
   // Raise Floor 24 And Change
   return !!EV_DoFloor(instance->line, raiseFloor24AndChange);
}

//
// EV_ActionBuildStairsTurbo16
//
static bool EV_ActionBuildStairsTurbo16(ev_action_t *action, ev_instance_t *instance)
{
   // case 100: (W1)
   // case 257: (WR - BOOM Extended)
   // Build Stairs Turbo 16
   return !!EV_BuildStairs(instance->line, turbo16);
}

//
// EV_ActionTurnTagLightsOff
//
static bool EV_ActionTurnTagLightsOff(ev_action_t *action, ev_instance_t *instance)
{
   // case 104: (W1)
   // case 157: (WR - BOOM Extended)
   // Turn lights off in sector(tag)
   return !!EV_TurnTagLightsOff(instance->line);
}

//
// EV_ActionDoorBlazeRaise
//
static bool EV_ActionDoorBlazeRaise(ev_action_t *action, ev_instance_t *instance)
{
   // case 105: (WR)
   // case 108: (W1)
   // Blazing Door Raise (faster than TURBO!)
   return !!EV_DoDoor(instance->line, blazeRaise);
}

//
// EV_ActionDoorBlazeOpen
//
static bool EV_ActionDoorBlazeOpen(ev_action_t *action, ev_instance_t *instance)
{
   // case 106: (WR)
   // case 109: (W1)
   // Blazing Door Open (faster than TURBO!)
   return !!EV_DoDoor(instance->line, blazeOpen);
}

//
// EV_ActionDoorBlazeClose
//
static bool EV_ActionDoorBlazeClose(ev_action_t *action, ev_instance_t *instance)
{
   // case 107: (WR)
   // case 110: (W1)
   // Blazing Door Close (faster than TURBO!)
   return !!EV_DoDoor(instance->line, blazeClose);
}

//
// EV_ActionFloorRaiseToNearest
//
static bool EV_ActionFloorRaiseToNearest(ev_action_t *action, ev_instance_t *instance)
{
   // case 119: (W1)
   // case 128: (WR)
   // Raise floor to nearest surr. floor
   return !!EV_DoFloor(instance->line, raiseFloorToNearest);
}

//
// EV_ActionPlatBlazeDWUS
//
static bool EV_ActionPlatBlazeDWUS(ev_action_t *action, ev_instance_t *instance)
{
   // case 120: (WR)
   // case 121: (W1)
   // Blazing PlatDownWaitUpStay
   return !!EV_DoPlat(instance->line, blazeDWUS, 0);
}

//
// EV_ActionSecretExit
//
static bool EV_ActionSecretExit(ev_action_t *action, ev_instance_t *instance)
{
   Mobj *thing = instance->actor;

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
static bool EV_ActionRaiseFloorTurbo(ev_action_t *action, ev_instance_t *instance)
{
   // case 129: (WR)
   // case 130: (W1)
   // Raise Floor Turbo
   return !!EV_DoFloor(instance->line, raiseFloorTurbo);
}

//
// EV_ActionSilentCrushAndRaise
//
static bool EV_ActionSilentCrushAndRaise(ev_action_t *action, ev_instance_t *instance)
{
   // case 141: (W1)
   // case 150: (WR - BOOM Extended)
   // Silent Ceiling Crush & Raise
   return !!EV_DoCeiling(instance->line, silentCrushAndRaise);
}

//
// EV_ActionRaiseFloor512
//
static bool EV_ActionRaiseFloor512(ev_action_t *action, ev_instance_t *instance)
{
   // case 142: (W1 - BOOM Extended)
   // case 147: (WR - BOOM Extended)
   // Raise Floor 512
   return !!EV_DoFloor(instance->line, raiseFloor512);
}

//
// EV_ActionPlatRaise24Change
//
static bool EV_ActionPlatRaise24Change(ev_action_t *action, ev_instance_t *instance)
{
   // case 143: (W1 - BOOM Extended)
   // case 148: (WR - BOOM Extended)
   // Raise Floor 24 and change
   return !!EV_DoPlat(instance->line, raiseAndChange, 24);
}

//
// EV_ActionPlatRaise32Change
//
static bool EV_ActionPlatRaise32Change(ev_action_t *action, ev_instance_t *instance)
{
   // case 144: (W1 - BOOM Extended)
   // case 149: (WR - BOOM Extended)
   // Raise Floor 32 and change
   return !!EV_DoPlat(instance->line, raiseAndChange, 32);
}

//
// EV_ActionCeilingLowerToFloor
//
static bool EV_ActionCeilingLowerToFloor(ev_action_t *action, ev_instance_t *instance)
{
   // case 145: (W1 - BOOM Extended)
   // case 152: (WR - BOOM Extended)
   // Lower Ceiling to Floor
   return !!EV_DoCeiling(instance->line, lowerToFloor);
}

//
// EV_ActionDoDonut
//
static bool EV_ActionDoDonut(ev_action_t *action, ev_instance_t *instance)
{
   // case 9:   (S1)
   // case 146: (W1 - BOOM Extended)
   // case 155: (WR - BOOM Extended)
   // Lower Pillar, Raise Donut
   return !!EV_DoDonut(instance->line);
}

//
// EV_ActionCeilingLowerToLowest
//
static bool EV_ActionCeilingLowerToLowest(ev_action_t *action, ev_instance_t *instance)
{
   // case 199: (W1 - BOOM Extended)
   // case 201: (WR - BOOM Extended)
   // Lower ceiling to lowest surrounding ceiling
   return !!EV_DoCeiling(instance->line, lowerToLowest);
}

//
// EV_ActionCeilingLowerToMaxFloor
//
static bool EV_ActionCeilingLowerToMaxFloor(ev_action_t *action, ev_instance_t *instance)
{
   // case 200: (W1 - BOOM Extended)
   // case 202: (WR - BOOM Extended)
   // Lower ceiling to highest surrounding floor
   return !!EV_DoCeiling(instance->line, lowerToMaxFloor);
}

//
// EV_ActionSilentTeleport
//
static bool EV_ActionSilentTeleport(ev_action_t *action, ev_instance_t *instance)
{
   line_t *line  = instance->line;
   int     side  = instance->side;
   Mobj   *thing = instance->actor;

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
static bool EV_ActionChangeOnly(ev_action_t *action, ev_instance_t *instance)
{
   // jff 3/16/98 renumber 215->153
   // jff 3/15/98 create texture change no motion type
   // case 153: (W1 - BOOM Extended)
   // case 154: (WR - BOOM Extended)
   // Texture/Type Change Only (Trig)
   return !!EV_DoChange(instance->line, trigChangeOnly);
}

//
// EV_ActionChangeOnlyNumeric
//
static bool EV_ActionChangeOnlyNumeric(ev_action_t *action, ev_instance_t *instance)
{
   //jff 3/15/98 create texture change no motion type
   // case 239: (W1 - BOOM Extended)
   // case 240: (WR - BOOM Extended)
   // Texture/Type Change Only (Numeric)
   return !!EV_DoChange(instance->line, numChangeOnly);
}

//
// EV_ActionFloorLowerToNearest
//
static bool EV_ActionFloorLowerToNearest(ev_action_t *action, ev_instance_t *instance)
{
   // case 219: (W1 - BOOM Extended)
   // case 220: (WR - BOOM Extended)
   // Lower floor to next lower neighbor
   return !!EV_DoFloor(instance->line, lowerFloorToNearest);
}

//
// EV_ActionElevatorUp
//
static bool EV_ActionElevatorUp(ev_action_t *action, ev_instance_t *instance)
{
   // case 227: (W1 - BOOM Extended)
   // case 228: (WR - BOOM Extended)
   // Raise elevator next floor
   return !!EV_DoElevator(instance->line, elevateUp);
}

//
// EV_ActionElevatorDown
//
static bool EV_ActionElevatorDown(ev_action_t *action, ev_instance_t *instance)
{
   // case 231: (W1 - BOOM Extended)
   // case 232: (WR - BOOM Extended)
   // Lower elevator next floor
   return !!EV_DoElevator(instance->line, elevateDown);

}

//
// EV_ActionElevatorCurrent
//
static bool EV_ActionElevatorCurrent(ev_action_t *action, ev_instance_t *instance)
{
   // case 235: (W1 - BOOM Extended)
   // case 236: (WR - BOOM Extended)
   // Elevator to current floor
   return !!EV_DoElevator(instance->line, elevateCurrent);
}

//
// EV_ActionSilentLineTeleport
//
static bool EV_ActionSilentLineTeleport(ev_action_t *action, ev_instance_t *instance)
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

   return !!EV_SilentLineTeleport(line, side, thing, false);
}

//
// EV_ActionSilentLineTeleportReverse
//
static bool EV_ActionSilentLineTeleportReverse(ev_action_t *action, 
                                               ev_instance_t *instance)
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
   return !!EV_SilentLineTeleport(line, side, thing, true);
}

//
// EV_ActionPlatToggleUpDown
//
static bool EV_ActionPlatToggleUpDown(ev_action_t *action, ev_instance_t *instance)
{
   // jff 3/14/98 create instant toggle floor type
   // case 212: (WR - BOOM Extended)
   return !!EV_DoPlat(instance->line, toggleUpDn, 0);
}

//
// EV_ActionStartLineScript
//
// SMMU script instance linedefs (now adapted to work for ACS)
//
static bool EV_ActionStartLineScript(ev_action_t *action, ev_instance_t *instance)
{
   P_StartLineScript(instance->line, instance->actor);
   return true;
}

// 
// EV_ActionVerticalDoor
//
static bool EV_ActionVerticalDoor(ev_action_t *action, ev_instance_t *instance)
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

   return !!EV_VerticalDoor(instance->line, instance->actor);
}

//
// BOOM Generalized Action
//

static bool EV_ActionBoomGen(ev_action_t *action, ev_instance_t *instance)
{
   switch(instance->gentype)
   {
   case GenTypeFloor:
      return !!EV_DoGenFloor(instance->line);
   case GenTypeCeiling:
      return !!EV_DoGenCeiling(instance->line);
   case GenTypeDoor:
      genDoorThing = instance->actor; // FIXME!!!
      return !!EV_DoGenDoor(instance->line);
   case GenTypeLocked:
      genDoorThing = instance->actor; // FIXME!!!
      return !!EV_DoGenLockedDoor(instance->line);
   case GenTypeLift:
      return !!EV_DoGenLift(instance->line);
   case GenTypeStairs:
      return !!EV_DoGenStairs(instance->line);
   case GenTypeCrusher:
      return !!EV_DoGenCrusher(instance->line);
   default:
      return false;
   }
}

//=============================================================================
//
// Action Types
//

// DOOM-Style Action Types

// WR-Type lines may be crossed multiple times
static ev_actiontype_t WRActionType =
{
   SPAC_CROSS,           // line must be crossed
   EV_DOOMPreCrossLine,  // pre-activation callback
   EV_DOOMPostCrossLine, // post-activation callback
   0                     // no default flags
};

// W1-Type lines may be activated once. Semantics are implemented in the 
// post-cross callback to implement compatibility behaviors regarding the 
// premature clearing of specials crossed from the wrong side or without
// successful instance having occurred.
static ev_actiontype_t W1ActionType =
{
   SPAC_CROSS,           // line must be crossed
   EV_DOOMPreCrossLine,  // pre-activation callback
   EV_DOOMPostCrossLine, // post-activation callback
   EV_POSTCLEARSPECIAL   // special will be cleared after activation
};

// SR-Type lines may be activated multiple times by using them.
static ev_actiontype_t SRActionType =
{
   SPAC_USE,             // line must be used
   EV_DOOMPreUseLine,    // pre-activation callback
   EV_DOOMPostUseLine,   // post-activation callback
   EV_POSTCHANGESWITCH   // change switch texture
};

// S1-Type lines may be activated once, by using them.
static ev_actiontype_t S1ActionType =
{
   SPAC_USE,             // line must be used
   EV_DOOMPreUseLine,    // pre-activation callback
   EV_DOOMPostUseLine,   // post-activation callback
   EV_POSTCHANGESWITCH | EV_POSTCLEARSPECIAL // change switch and clear special
};

// DR-Type lines may be activated repeatedly by pushing; the main distinctions
// from switch-types are that switch textures are not changed and the sector
// target is always the line's backsector. Therefore the tag can be zero, and
// when not zero, is recycled by BOOM to allow a special lighting effect.
static ev_actiontype_t DRActionType =
{
   SPAC_USE,             // line must be used
   EV_DOOMPreUseLine,    // pre-activation callback
   EV_DOOMPostUseLine,   // post-activation callback
   EV_PREALLOWZEROTAG    // tags are never used for activation purposes
};


// BOOM Generalized Action Type (there is but one)

static ev_actiontype_t BoomGenActionType =
{
   -1,                     // SPAC is determined by the line special
   EV_BOOMGenPreActivate,  // pre-activation callback
   EV_BOOMGenPostActivate, // post-activation callback
   0                       // flags are not used by this type
};

//=============================================================================
//
// DOOM Line Actions
//

// Macro to declare a DOOM-style W1 line action
#define W1LINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &W1ActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version                                \
   }

// Macro to declare a DOOM-style WR line action
#define WRLINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &WRActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version                                \
   }

#define S1LINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &S1ActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version                                \
   }

#define DRLINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &DRActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version                                \
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
W1LINE(W1PlatDownWaitUpStay, PlatDownWaitUpStay, EV_PREALLOWMONSTERS, 0);

// DOOM Line Type 11 - S1 Exit Level
S1LINE(S1ExitLevel, SwitchExitLevel, EV_PREALLOWZEROTAG, 0);

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

// DOOM Line Type 26 - DR Raise Door Blue Key
DRLINE(DRRaiseDoorBlue, VerticalDoor, 0, 0);

// DOOM Line Type 27 - DR Raise Door Yellow Key
DRLINE(DRRaiseDoorYellow, VerticalDoor, 0, 0);

// DOOM Line Type 28 - DR Raise Door Red Key
DRLINE(DRRaiseDoorRed, VerticalDoor, 0, 0);

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

// DOOM Line Type 117 - DR Door Blaze Raise
DRLINE(DRDoorBlazeRaise, VerticalDoor, 0, 0);

// DOOM Line Type 118 - D1 Door Blaze Open
DRLINE(D1DoorBlazeOpen, VerticalDoor, 0, 0);

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

/*
bool P_UseSpecialLine(Mobj *thing, line_t *line, int side)
{
   // Switches that other things can activate.
   if(!thing->player)
   {
      switch(line->special)
      {
         //jff 3/5/98 add ability to use teleporters for monsters
      case 195:       // switch teleporters
      case 174:
      case 210:       // silent switch teleporters
      case 209:
         break;
         
      default:
         return false;
      }
   }

   // Dispatch to handler according to linedef type
   switch(line->special)
   {
     // Switches (non-retriggerable)
       
   case 14:
      // Raise Floor 32 and change texture
      if (EV_DoPlat(line,raiseAndChange,32))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 15:
      // Raise Floor 24 and change texture
      if (EV_DoPlat(line,raiseAndChange,24))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 18:
      // Raise Floor to next highest floor
      if (EV_DoFloor(line, raiseFloorToNearest))
         P_ChangeSwitchTexture(line,0,0);
      break;
        
   case 20:
      // Raise Plat next highest floor and change texture
      if (EV_DoPlat(line,raiseToNearestAndChange,0))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 21:
      // PlatDownWaitUpStay
      if (EV_DoPlat(line,downWaitUpStay,0))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 23:
      // Lower Floor to Lowest
      if (EV_DoFloor(line,lowerFloorToLowest))
         P_ChangeSwitchTexture(line,0,0);
      break;
        
   case 29:
      // Raise Door
      if (EV_DoDoor(line,doorNormal))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 41:
      // Lower Ceiling to Floor
      if (EV_DoCeiling(line,lowerToFloor))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 71:
      // Turbo Lower Floor
      if (EV_DoFloor(line,turboLower))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 49:
      // Ceiling Crush And Raise
      if (EV_DoCeiling(line,crushAndRaise))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 50:
      // Close Door
      if (EV_DoDoor(line,doorClose))
         P_ChangeSwitchTexture(line,0,0);
      break;
        
   case 51:
      // Secret EXIT
      // killough 10/98: prevent zombies from exiting levels
      if(thing->player && thing->player->health <= 0 &&
         !comp[comp_zombie])
      {
         S_StartSound(thing, GameModeInfo->playerSounds[sk_oof]);
         return false;
      }
      P_ChangeSwitchTexture(line,0,0);
      G_SecretExitLevel ();
      break;
        
   case 55:
      // Raise Floor Crush
      if (EV_DoFloor(line,raiseFloorCrush))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 101:
      // Raise Floor
      if (EV_DoFloor(line,raiseFloor))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 102:
      // Lower Floor to Surrounding floor height
      if (EV_DoFloor(line,lowerFloor))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 103:
      // Open Door
      if (EV_DoDoor(line,doorOpen))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 111:
      // Blazing Door Raise (faster than TURBO!)
      if (EV_DoDoor (line,blazeRaise))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 112:
      // Blazing Door Open (faster than TURBO!)
      if (EV_DoDoor (line,blazeOpen))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 113:
      // Blazing Door Close (faster than TURBO!)
      if (EV_DoDoor (line,blazeClose))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 122:
      // Blazing PlatDownWaitUpStay
      if (EV_DoPlat(line,blazeDWUS,0))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 127:
      // Build Stairs Turbo 16
      if (EV_BuildStairs(line,turbo16))
         P_ChangeSwitchTexture(line,0,0);
      break;
      
   case 131:
      // Raise Floor Turbo
      if (EV_DoFloor(line,raiseFloorTurbo))
         P_ChangeSwitchTexture(line,0,0);
      break;
        
   case 133:
      // BlzOpenDoor BLUE
   case 135:
      // BlzOpenDoor RED
   case 137:
      // BlzOpenDoor YELLOW
      if(EV_DoLockedDoor (line,blazeOpen,thing))
         P_ChangeSwitchTexture(line,0,0);
      break;
        
   case 140:
      // Raise Floor 512
      if (EV_DoFloor(line,raiseFloor512))
         P_ChangeSwitchTexture(line,0,0);
      break;

      // killough 1/31/98: factored out compatibility check;
      // added inner switch, relaxed check to demo_compatibility

   default:
      if(!demo_compatibility)
      {
         switch(line->special)
         {
            //jff 1/29/98 added linedef types to fill all functions out so that
            // all possess SR, S1, WR, W1 types

         case 158:
            // Raise Floor to shortest lower texture
            // 158 S1  EV_DoFloor(raiseToTexture), CSW(0)
            if (EV_DoFloor(line,raiseToTexture))
               P_ChangeSwitchTexture(line,0,0);
            break;

         case 159:
            // Raise Floor to shortest lower texture
            // 159 S1  EV_DoFloor(lowerAndChange)
            if (EV_DoFloor(line,lowerAndChange))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 160:
            // Raise Floor 24 and change
            // 160 S1  EV_DoFloor(raiseFloor24AndChange)
            if (EV_DoFloor(line,raiseFloor24AndChange))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 161:
            // Raise Floor 24
            // 161 S1  EV_DoFloor(raiseFloor24)
            if (EV_DoFloor(line,raiseFloor24))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 162:
            // Moving floor min n to max n
            // 162 S1  EV_DoPlat(perpetualRaise,0)
            if (EV_DoPlat(line,perpetualRaise,0))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 163:
            // Stop Moving floor
            // 163 S1  EV_DoPlat(perpetualRaise,0)
            EV_StopPlat(line);
            P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 164:
            // Start fast crusher
            // 164 S1  EV_DoCeiling(fastCrushAndRaise)
            if (EV_DoCeiling(line,fastCrushAndRaise))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 165:
            // Start slow silent crusher
            // 165 S1  EV_DoCeiling(silentCrushAndRaise)
            if (EV_DoCeiling(line,silentCrushAndRaise))
               P_ChangeSwitchTexture(line,0,0);
            break;

         case 166:
            // Raise ceiling, Lower floor
            // 166 S1 EV_DoCeiling(raiseToHighest), EV_DoFloor(lowerFloortoLowest)
            if(EV_DoCeiling(line, raiseToHighest) ||
               EV_DoFloor(line, lowerFloorToLowest))
               P_ChangeSwitchTexture(line,0,0);
            break;

         case 167:
            // Lower floor and Crush
            // 167 S1 EV_DoCeiling(lowerAndCrush)
            if (EV_DoCeiling(line, lowerAndCrush))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 168:
            // Stop crusher
            // 168 S1 EV_CeilingCrushStop()
            if (EV_CeilingCrushStop(line))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 169:
            // Lights to brightest neighbor sector
            // 169 S1  EV_LightTurnOn(0)
            EV_LightTurnOn(line,0);
            P_ChangeSwitchTexture(line,0,0);
            break;

         case 170:
            // Lights to near dark
            // 170 S1  EV_LightTurnOn(35)
            EV_LightTurnOn(line,35);
            P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 171:
            // Lights on full
            // 171 S1  EV_LightTurnOn(255)
            EV_LightTurnOn(line,255);
            P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 172:
            // Start Lights Strobing
            // 172 S1  EV_StartLightStrobing()
            EV_StartLightStrobing(line);
            P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 173:
            // Lights to Dimmest Near
            // 173 S1  EV_TurnTagLightsOff()
            EV_TurnTagLightsOff(line);
            P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 174:
            // Teleport
            // 174 S1  EV_Teleport(side,thing)
            if (EV_Teleport(line,side,thing))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 175:
            // Close Door, Open in 30 secs
            // 175 S1  EV_DoDoor(close30ThenOpen)
            if (EV_DoDoor(line, closeThenOpen))
               P_ChangeSwitchTexture(line,0,0);
            break;

         case 189: //jff 3/15/98 create texture change no motion type
            // Texture Change Only (Trigger)
            // 189 S1 Change Texture/Type Only
            if (EV_DoChange(line,trigChangeOnly))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 203:
            // Lower ceiling to lowest surrounding ceiling
            // 203 S1 EV_DoCeiling(lowerToLowest)
            if (EV_DoCeiling(line,lowerToLowest))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 204:
            // Lower ceiling to highest surrounding floor
            // 204 S1 EV_DoCeiling(lowerToMaxFloor)
            if (EV_DoCeiling(line,lowerToMaxFloor))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 209:
            // killough 1/31/98: silent teleporter
            //jff 209 S1 SilentTeleport 
            if (EV_SilentTeleport(line, side, thing))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 241: //jff 3/15/98 create texture change no motion type
            // Texture Change Only (Numeric)
            // 241 S1 Change Texture/Type Only
            if (EV_DoChange(line,numChangeOnly))
               P_ChangeSwitchTexture(line,0,0);
            break;

         case 221:
            // Lower floor to next lowest floor
            // 221 S1 Lower Floor To Nearest Floor
            if (EV_DoFloor(line,lowerFloorToNearest))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 229:
            // Raise elevator next floor
            // 229 S1 Raise Elevator next floor
            if (EV_DoElevator(line,elevateUp))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 233:
            // Lower elevator next floor
            // 233 S1 Lower Elevator next floor
            if (EV_DoElevator(line,elevateDown))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
         case 237:
            // Elevator to current floor
            // 237 S1 Elevator to current floor
            if (EV_DoElevator(line,elevateCurrent))
               P_ChangeSwitchTexture(line,0,0);
            break;
            
            
            // jff 1/29/98 end of added S1 linedef types
            
            //jff 1/29/98 added linedef types to fill all functions out so that
            // all possess SR, S1, WR, W1 types
            
         case 78: //jff 3/15/98 create texture change no motion type
            // Texture Change Only (Numeric)
            // 78 SR Change Texture/Type Only
            if (EV_DoChange(line,numChangeOnly))
               P_ChangeSwitchTexture(line,1,0);
            break;

         case 176:
            // Raise Floor to shortest lower texture
            // 176 SR  EV_DoFloor(raiseToTexture), CSW(1)
            if (EV_DoFloor(line,raiseToTexture))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 177:
            // Raise Floor to shortest lower texture
            // 177 SR  EV_DoFloor(lowerAndChange)
            if (EV_DoFloor(line,lowerAndChange))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 178:
            // Raise Floor 512
            // 178 SR  EV_DoFloor(raiseFloor512)
            if (EV_DoFloor(line,raiseFloor512))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 179:
            // Raise Floor 24 and change
            // 179 SR  EV_DoFloor(raiseFloor24AndChange)
            if (EV_DoFloor(line,raiseFloor24AndChange))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 180:
            // Raise Floor 24
            // 180 SR  EV_DoFloor(raiseFloor24)
            if (EV_DoFloor(line,raiseFloor24))
               P_ChangeSwitchTexture(line,1,0);
            break;

         case 181:
            // Moving floor min n to max n
            // 181 SR  EV_DoPlat(perpetualRaise,0)
            EV_DoPlat(line,perpetualRaise,0);
            P_ChangeSwitchTexture(line,1,0);
            break;

         case 182:
            // Stop Moving floor
            // 182 SR  EV_DoPlat(perpetualRaise,0)
            EV_StopPlat(line);
            P_ChangeSwitchTexture(line,1,0);
            break;

         case 183:
            // Start fast crusher
            // 183 SR  EV_DoCeiling(fastCrushAndRaise)
            if (EV_DoCeiling(line,fastCrushAndRaise))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 184:
            // Start slow crusher
            // 184 SR  EV_DoCeiling(crushAndRaise)
            if(EV_DoCeiling(line,crushAndRaise))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 185:
            // Start slow silent crusher
            // 185 SR  EV_DoCeiling(silentCrushAndRaise)
            if (EV_DoCeiling(line,silentCrushAndRaise))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 186:
            // Raise ceiling, Lower floor
            // 186 SR EV_DoCeiling(raiseToHighest), EV_DoFloor(lowerFloortoLowest)
            if(EV_DoCeiling(line, raiseToHighest) ||
               EV_DoFloor(line, lowerFloorToLowest))
               P_ChangeSwitchTexture(line,1,0);
            break;

         case 187:
            // Lower floor and Crush
            // 187 SR EV_DoCeiling(lowerAndCrush)
            if (EV_DoCeiling(line, lowerAndCrush))
               P_ChangeSwitchTexture(line,1,0);
            break;

         case 188:
            // Stop crusher
            // 188 SR EV_CeilingCrushStop()
            if (EV_CeilingCrushStop(line))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 190: //jff 3/15/98 create texture change no motion type
            // Texture Change Only (Trigger)
            // 190 SR Change Texture/Type Only
            if (EV_DoChange(line,trigChangeOnly))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 191:
            // Lower Pillar, Raise Donut
            // 191 SR  EV_DoDonut()
            if (EV_DoDonut(line))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 192:
            // Lights to brightest neighbor sector
            // 192 SR  EV_LightTurnOn(0)
            EV_LightTurnOn(line,0);
            P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 193:
            // Start Lights Strobing
            // 193 SR  EV_StartLightStrobing()
            EV_StartLightStrobing(line);
            P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 194:
            // Lights to Dimmest Near
            // 194 SR  EV_TurnTagLightsOff()
            EV_TurnTagLightsOff(line);
            P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 195:
            // Teleport
            // 195 SR  EV_Teleport(side,thing)
            if (EV_Teleport(line,side,thing))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 196:
            // Close Door, Open in 30 secs
            // 196 SR  EV_DoDoor(close30ThenOpen)
            if (EV_DoDoor(line, closeThenOpen))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 205:
            // Lower ceiling to lowest surrounding ceiling
            // 205 SR EV_DoCeiling(lowerToLowest)
            if (EV_DoCeiling(line,lowerToLowest))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 206:
            // Lower ceiling to highest surrounding floor
            // 206 SR EV_DoCeiling(lowerToMaxFloor)
            if (EV_DoCeiling(line,lowerToMaxFloor))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 210:
            // killough 1/31/98: silent teleporter
            //jff 210 SR SilentTeleport 
            if (EV_SilentTeleport(line, side, thing))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 211: //jff 3/14/98 create instant toggle floor type
            // Toggle Floor Between C and F Instantly
            // 211 SR Toggle Floor Instant
            if (EV_DoPlat(line,toggleUpDn,0))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 222:
            // Lower floor to next lowest floor
            // 222 SR Lower Floor To Nearest Floor
            if (EV_DoFloor(line,lowerFloorToNearest))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 230:
            // Raise elevator next floor
            // 230 SR Raise Elevator next floor
            if (EV_DoElevator(line,elevateUp))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 234:
            // Lower elevator next floor
            // 234 SR Lower Elevator next floor
            if (EV_DoElevator(line,elevateDown))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 238:
            // Elevator to current floor
            // 238 SR Elevator to current floor
            if (EV_DoElevator(line,elevateCurrent))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 258:
            // Build stairs, step 8
            // 258 SR EV_BuildStairs(build8)
            if (EV_BuildStairs(line,build8))
               P_ChangeSwitchTexture(line,1,0);
            break;
            
         case 259:
            // Build stairs, step 16
            // 259 SR EV_BuildStairs(turbo16)
            if (EV_BuildStairs(line,turbo16))
               P_ChangeSwitchTexture(line,1,0);
            break;
            // 1/29/98 jff end of added SR linedef types
            
            // sf: scripting
         case 277: // S1 start script
            line->special = 0;
         case 276: // SR start script
            P_ChangeSwitchTexture(line, (line->special ? 1 : 0), 0);
            P_StartLineScript(line, thing);
            break;            
         }
      }
      break;

      // Buttons (retriggerable switches)
   case 42:
      // Close Door
      if (EV_DoDoor(line,doorClose))
         P_ChangeSwitchTexture(line,1,0);
      break;
        
   case 43:
      // Lower Ceiling to Floor
      if (EV_DoCeiling(line,lowerToFloor))
         P_ChangeSwitchTexture(line,1,0);
      break;
        
   case 45:
      // Lower Floor to Surrounding floor height
      if (EV_DoFloor(line,lowerFloor))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 60:
      // Lower Floor to Lowest
      if (EV_DoFloor(line,lowerFloorToLowest))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 61:
      // Open Door
      if (EV_DoDoor(line,doorOpen))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 62:
      // PlatDownWaitUpStay
      if (EV_DoPlat(line,downWaitUpStay,1))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 63:
      // Raise Door
      if (EV_DoDoor(line,doorNormal))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 64:
      // Raise Floor to ceiling
      if (EV_DoFloor(line,raiseFloor))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 66:
      // Raise Floor 24 and change texture
      if (EV_DoPlat(line,raiseAndChange,24))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 67:
      // Raise Floor 32 and change texture
      if (EV_DoPlat(line,raiseAndChange,32))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 65:
      // Raise Floor Crush
      if (EV_DoFloor(line,raiseFloorCrush))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 68:
      // Raise Plat to next highest floor and change texture
      if (EV_DoPlat(line,raiseToNearestAndChange,0))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 69:
      // Raise Floor to next highest floor
      if (EV_DoFloor(line, raiseFloorToNearest))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 70:
      // Turbo Lower Floor
      if (EV_DoFloor(line,turboLower))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 114:
      // Blazing Door Raise (faster than TURBO!)
      if (EV_DoDoor (line,blazeRaise))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 115:
      // Blazing Door Open (faster than TURBO!)
      if (EV_DoDoor (line,blazeOpen))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 116:
      // Blazing Door Close (faster than TURBO!)
      if (EV_DoDoor (line,blazeClose))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 123:
      // Blazing PlatDownWaitUpStay
      if (EV_DoPlat(line,blazeDWUS,0))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 132:
      // Raise Floor Turbo
      if (EV_DoFloor(line,raiseFloorTurbo))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 99:
      // BlzOpenDoor BLUE
   case 134:
      // BlzOpenDoor RED
   case 136:
      // BlzOpenDoor YELLOW
      if (EV_DoLockedDoor (line,blazeOpen,thing))
         P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 138:
      // Light Turn On
      EV_LightTurnOn(line,255);
      P_ChangeSwitchTexture(line,1,0);
      break;
      
   case 139:
      // Light Turn Off
      EV_LightTurnOn(line,35);
      P_ChangeSwitchTexture(line,1,0);
      break;
   }
   return true;
}
*/

//=============================================================================
//
// BOOM Generalized Line Action
//

static ev_action_t BoomGenAction =
{                                         
   &BoomGenActionType,    // use generalized action type
   EV_ActionBoomGen,      // use a single action function
   0,                     // flags aren't used
   200                    // BOOM or up
};

//=============================================================================
//
// Special Bindings
//
// Each level type (DOOM, Heretic, Strife, Hexen) has its own set of line 
// specials. Heretic and Strife's sets are based on DOOM's with certain 
// additions (many of which conflict with BOOM extensions).
//

//
// EV_GenTypeForSpecial
//
// Get a GenType enumeration value given a line special
//
static int EV_GenTypeForSpecial(int16_t special)
{
   unsigned int uspec = static_cast<unsigned int>(special);

   // Floors
   if(uspec >= GenFloorBase)
      return GenTypeFloor;

   // Ceilings
   if(uspec >= GenCeilingBase)
      return GenTypeCeiling;

   // Doors
   if(uspec >= GenDoorBase)
      return GenTypeDoor;

   // Locked Doors
   if(uspec >= GenLockedBase)
      return GenTypeLocked;

   // Lifts
   if(uspec >= GenLiftBase)
      return GenTypeLift;

   // Stairs
   if(uspec >= GenStairsBase)
      return GenTypeStairs;

   // Crushers
   if(uspec >= GenCrusherBase)
      return GenTypeCrusher;

   // not a generalized line.
   return -1;
}

//
// EV_GenActivationType
//
// Extract the activation type from a generalized line special.
//
static int EV_GenActivationType(int16_t special)
{
   return (special & TriggerType) >> TriggerTypeShift;
}

//
// EV_ActionForInstance
//
// Given a special number, obtain the corresponding ev_action_t structure,
// within the currently defined set of bindings.
//
ev_action_t *EV_ActionForInstance(ev_instance_t &instance)
{
   // check if it is a generalized type 
   instance.gentype = EV_GenTypeForSpecial(instance.line->special);
   
   if(instance.gentype >= GenTypeFloor)
   {
      // This is a BOOM generalized special type

      // set trigger type
      instance.genspac = EV_GenActivationType(instance.line->special);

      return &BoomGenAction;
   }
   else
   {
      // TODO: normal
   }

   return NULL; 
}

//=============================================================================
//
// Activation
//

//
// EV_CheckSpac
//
// Checks against the instance characteristics of the action to see if this
// method of activating the line is allowed.
//
static bool EV_CheckSpac(ev_action_t *action, ev_instance_t *instance)
{
   if(action->type->activation >= 0) // specific type for this action?
   {
      return action->type->activation == instance->spac;
   }
   else if(instance->gentype >= GenTypeFloor) // generalized line?
   {
      switch(instance->genspac)
      {
      case WalkOnce:
      case WalkMany:
         return instance->spac == SPAC_CROSS;
      case GunOnce:
      case GunMany:
         return instance->spac == SPAC_IMPACT;
      case SwitchOnce:
      case SwitchMany:
      case PushOnce:
      case PushMany:
         return instance->spac == SPAC_USE;
      default:
         return false; // should be unreachable.
      }
   }
   else // activation ability is determined by the linedef's flags
   {
      Mobj   *thing = instance->actor;
      line_t *line  = instance->line;
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
      if((line->extflags & EX_ML_1SONLY) && instance->side != 0)
         return false;

      // check activation flags -- can we activate this line this way?
      switch(instance->spac)
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
// Shared logic for all types of line activationonly
//
bool EV_ActivateSpecialLine(ev_action_t *action, ev_instance_t *instance)
{
   // check for special instance
   if(!EV_CheckSpac(action, instance))
      return false;

   // execute pre-amble action
   if(!action->type->pre(action, instance))
      return false;

   bool result = action->action(action, instance);

   // execute the post-action routine
   return action->type->post(action, result, instance);
}

//
// EV_CrossSpecialLine
//
// A line has been activated by an Mobj physically crossing it.
//
bool EV_CrossSpecialLine(line_t *line, int side, Mobj *thing)
{
   ev_action_t   *action;
   ev_instance_t  instance;

   memset(&instance, 0, sizeof(instance));

   // setup instance
   instance.actor = thing;
   instance.args  = line->args;
   instance.line  = line;
   instance.side  = side;
   instance.spac  = SPAC_CROSS;
   instance.tag   = line->tag;

   // get action
   if(!(action = EV_ActionForInstance(instance)))
      return false;

   return EV_ActivateSpecialLine(action, &instance);
}

// EOF

