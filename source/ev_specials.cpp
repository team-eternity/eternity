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
#include "e_hash.h"
#include "ev_specials.h"
#include "g_game.h"
#include "p_info.h"
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
         int useAgain   = !(flags & EV_POSTCLEARSPECIAL);
         int changeSide = (flags & EV_POSTCHANGESIDED) ? instance->side : 0;
         P_ChangeSwitchTexture(instance->line, useAgain, changeSide);
      }
   }

   // FIXME/TODO: ideally we return result; the vanilla code always returns true
   // if the line action was *attempted*, not if it succeeded. Fixing this will
   // require a new comp flag.
   
   //return result;
   return true; // temporary
}

//
// EV_DOOMPreShootLine
//
// Pre-activation semantics for DOOM gun-type actions
//
static bool EV_DOOMPreShootLine(ev_action_t *action, ev_instance_t *instance)
{
   unsigned int flags = EV_CompositeActionFlags(action);

   Mobj   *thing = instance->actor;
   line_t *line  = instance->line;

   // actor and line are required
   REQUIRE_ACTOR(thing);
   REQUIRE_LINE(line);

   if(!thing->player)
   {
      // Check if special allows monsters
      if(!(flags & EV_PREALLOWMONSTERS))
         return false;
   }

   // check for zero tag
   if(!(instance->tag || comp[comp_zerotags] || (flags & EV_PREALLOWZEROTAG)))
      return false;

   return true;
}

//
// EV_DOOMPostShootLine
//
// Post-activation semantics for DOOM-style use line actions.
//
static bool EV_DOOMPostShootLine(ev_action_t *action, bool result, 
                                 ev_instance_t *instance)
{
   unsigned int flags = EV_CompositeActionFlags(action);

   // check for switch texture changes
   if(flags & EV_POSTCHANGESWITCH)
   {
      // Non-BOOM gun line types may clear their special even if they fail
      if(result || (flags & EV_POSTCHANGEALWAYS) || 
         (action->minversion < 200 && P_ClearSwitchOnFail()))
      {
         int useAgain   = !(flags & EV_POSTCLEARSPECIAL);
         int changeSide = (flags & EV_POSTCHANGESIDED) ? instance->side : 0;
         P_ChangeSwitchTexture(instance->line, useAgain, changeSide);
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

   // FIXME/TODO: P_Cross and P_Shoot have always been void, and P_UseSpecialLine
   // just returns true if an action is attempted, not if it succeeds. Until a
   // comp var is added, we return true from here instead of forwarding on the
   // result.
   
   //return result;
   return true; // temporary
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
   // case   2: (W1)
   // case  46: (GR)
   // case  61: (SR)
   // case  86: (WR)
   // case 103: (S1)
   // Open Door
   return !!EV_DoDoor(instance->line, doorOpen);
}

//
// EV_ActionCloseDoor
//
static bool EV_ActionCloseDoor(ev_action_t *action, ev_instance_t *instance)
{
   // case 3:  (W1)
   // case 42: (SR)
   // case 50: (S1)
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
   // case 29: (S1)
   // case 63: (SR)
   // case 90: (WR)
   // Raise Door
   return !!EV_DoDoor(instance->line, doorNormal);
}

//
// EV_ActionRaiseFloor
//
static bool EV_ActionRaiseFloor(ev_action_t *action, ev_instance_t *instance)
{
   // case   5: (W1)
   // case  24: (G1)
   // case  64: (SR)
   // case  91: (WR)
   // case 101: (S1)
   // Raise Floor
   return !!EV_DoFloor(instance->line, raiseFloor);
}

//
// EV_ActionFastCeilCrushRaise
//
static bool EV_ActionFastCeilCrushRaise(ev_action_t *action, ev_instance_t *instance)
{
   // case   6: (W1)
   // case  77: (WR)
   // case 164: (S1 - BOOM Extended)
   // case 183: (SR - BOOM Extended)
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
   // case 258: (SR - BOOM Extended)
   // Build Stairs
   return !!EV_BuildStairs(instance->line, build8);
}

//
// EV_ActionPlatDownWaitUpStay
//
static bool EV_ActionPlatDownWaitUpStay(ev_action_t *action, ev_instance_t *instance)
{
   // case 10: (W1)
   // case 21: (S1)
   // case 62: (SR)
   // case 88: (WR)
   // PlatDownWaitUp
   return !!EV_DoPlat(instance->line, downWaitUpStay, 0);
}

//
// EV_ActionLightTurnOn
//
static bool EV_ActionLightTurnOn(ev_action_t *action, ev_instance_t *instance)
{
   // case  12: (W1)
   // case  80: (WR)
   // case 169: (S1 - BOOM Extended)
   // case 192: (SR - BOOM Extended)
   // Light Turn On - brightest near
   return !!EV_LightTurnOn(instance->line, 0);
}

//
// EV_ActionLightTurnOn255
//
static bool EV_ActionLightTurnOn255(ev_action_t *action, ev_instance_t *instance)
{
   // case  13: (W1)
   // case  81: (WR)
   // case 138: (SR)
   // Light Turn On 255
   return !!EV_LightTurnOn(instance->line, 255);
}

//
// EV_ActionCloseDoor30
//
static bool EV_ActionCloseDoor30(ev_action_t *action, ev_instance_t *instance)
{
   // case  16: (W1)
   // case  76: (WR)
   // case 175: (S1 - BOOM Extended)
   // case 196: (SR - BOOM Extended)
   // Close Door 30
   return !!EV_DoDoor(instance->line, closeThenOpen);
}

//
// EV_ActionStartLightStrobing
//
static bool EV_ActionStartLightStrobing(ev_action_t *action, ev_instance_t *instance)
{
   // case  17: (W1)
   // case 156: (WR - BOOM Extended)
   // case 172: (S1 - BOOM Extended)
   // case 193: (SR - BOOM Extended)
   // Start Light Strobing
   return !!EV_StartLightStrobing(instance->line);
}

//
// EV_ActionLowerFloor
//
static bool EV_ActionLowerFloor(ev_action_t *action, ev_instance_t *instance)
{
   // case  19: (W1)
   // case  45: (SR)
   // case  83: (WR)
   // case 102: (S1)
   // Lower Floor
   return !!EV_DoFloor(instance->line, lowerFloor);
}

//
// EV_ActionPlatRaiseNearestChange
//
static bool EV_ActionPlatRaiseNearestChange(ev_action_t *action, ev_instance_t *instance)
{
   // case 20: (S1)
   // case 22: (W1)
   // case 47: (G1)
   // case 68: (SR)
   // case 95: (WR)
   // Raise floor to nearest height and change texture
   return !!EV_DoPlat(instance->line, raiseToNearestAndChange, 0);
}

//
// EV_ActionCeilingCrushAndRaise
//
static bool EV_ActionCeilingCrushAndRaise(ev_action_t *action, ev_instance_t *instance)
{
   // case  25: (W1)
   // case  49: (S1)
   // case  73: (WR)
   // case 184: (SR - BOOM Extended)
   // Ceiling Crush and Raise
   return !!EV_DoCeiling(instance->line, crushAndRaise);
}

//
// EV_ActionFloorRaiseToTexture
//
static bool EV_ActionFloorRaiseToTexture(ev_action_t *action, ev_instance_t *instance)
{
   // case  30: (W1)
   // case  96: (WR)
   // case 158: (S1 - BOOM Extended)
   // case 176: (SR - BOOM Extended)
   // Raise floor to shortest texture height on either side of lines.
   return !!EV_DoFloor(instance->line, raiseToTexture);
}

//
// EV_ActionLightsVeryDark
//
static bool EV_ActionLightsVeryDark(ev_action_t *action, ev_instance_t *instance)
{
   // case  35: (W1)
   // case  79: (WR)
   // case 139: (SR)
   // case 170: (S1 - BOOM Extended)
   // Lights Very Dark
   return !!EV_LightTurnOn(instance->line, 35);
}

//
// EV_ActionLowerFloorTurbo
//
static bool EV_ActionLowerFloorTurbo(ev_action_t *action, ev_instance_t *instance)
{
   // case 36: (W1)
   // case 70: (SR)
   // case 71: (S1)
   // case 98: (WR)
   // Lower Floor (TURBO)
   return !!EV_DoFloor(instance->line, turboLower);
}

//
// EV_ActionFloorLowerAndChange
//
static bool EV_ActionFloorLowerAndChange(ev_action_t *action, ev_instance_t *instance)
{
   // case  37: (W1)
   // case  84: (WR)
   // case 159: (S1 - BOOM Extended)
   // case 177: (SR - BOOM Extended)
   // LowerAndChange
   return !!EV_DoFloor(instance->line, lowerAndChange);
}

//
// EV_ActionFloorLowerToLowest
//
static bool EV_ActionFloorLowerToLowest(ev_action_t *action, ev_instance_t *instance)
{
   // case 23: (S1)
   // case 38: (W1)
   // case 60: (SR)
   // case 82: (WR)
   // Lower Floor To Lowest
   return !!EV_DoFloor(instance->line, lowerFloorToLowest);
}

//
// EV_ActionTeleport
//
static bool EV_ActionTeleport(ev_action_t *action, ev_instance_t *instance)
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
// EV_ActionBOOMRaiseCeilingOrLowerFloor
//
// haleyjd: This is supposed to do the same as the above, but, thanks to a
// glaring hole in the logic, it will only perform one action or the other,
// depending on whether or not the ceiling is currently busy 9_9
//
static bool EV_ActionBOOMRaiseCeilingOrLowerFloor(ev_action_t *action, 
                                                  ev_instance_t *instance)
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
static bool EV_ActionCeilingLowerAndCrush(ev_action_t *action, ev_instance_t *instance)
{
   // case  44: (W1)
   // case  72: (WR)
   // case 167: (S1 - BOOM Extended)
   // case 187: (SR - BOOM Extended)
   // Ceiling Crush
   return !!EV_DoCeiling(instance->line, lowerAndCrush);
}

//
// EV_ActionExitLevel
//
static bool EV_ActionExitLevel(ev_action_t *action, ev_instance_t *instance)
{
   Mobj *thing = instance->actor;

   // case  52: (W1)
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
// EV_ActionGunExitLevel
//
// This version of level exit logic is used for gun-type exits.
//
static bool EV_ActionGunExitLevel(ev_action_t *action, ev_instance_t *instance)
{
   Mobj *thing = instance->actor;

   // case 197: (G1 - BOOM Extended)
   if(thing->player && thing->player->health <= 0 && !comp[comp_zombie])
      return false;

   G_ExitLevel();
   return true;
}

//
// EV_ActionPlatPerpetualRaise
//
static bool EV_ActionPlatPerpetualRaise(ev_action_t *action, ev_instance_t *instance)
{
   // case  53: (W1)
   // case  87: (WR)
   // case 162: (S1 - BOOM Extended)
   // case 181: (SR - BOOM Extended)
   // Perpetual Platform Raise
   return !!EV_DoPlat(instance->line, perpetualRaise, 0);
}

//
// EV_ActionPlatStop
//
static bool EV_ActionPlatStop(ev_action_t *action, ev_instance_t *instance)
{
   // case  54: (W1)
   // case  89: (WR)
   // case 163: (S1 - BOOM Extended)
   // case 182: (SR - BOOM Extended)
   // Platform Stop
   return !!EV_StopPlat(instance->line);
}

//
// EV_ActionFloorRaiseCrush
//
static bool EV_ActionFloorRaiseCrush(ev_action_t *action, ev_instance_t *instance)
{
   // case 55: (S1)
   // case 56: (W1)
   // case 65: (SR)
   // case 94: (WR)
   // Raise Floor Crush
   return !!EV_DoFloor(instance->line, raiseFloorCrush);
}

//
// EV_ActionCeilingCrushStop
//
static bool EV_ActionCeilingCrushStop(ev_action_t *action, ev_instance_t *instance)
{
   // case  57: (W1)
   // case  74: (WR)
   // case 168: (S1 - BOOM Extended)
   // case 188: (SR - BOOM Extended)
   // Ceiling Crush Stop
   return !!EV_CeilingCrushStop(instance->line);
}

//
// EV_ActionRaiseFloor24
//
static bool EV_ActionRaiseFloor24(ev_action_t *action, ev_instance_t *instance)
{
   // case  58: (W1)
   // case  92: (WR)
   // case 161: (S1 - BOOM Extended)
   // case 180: (SR - BOOM Extended)
   // Raise Floor 24
   return !!EV_DoFloor(instance->line, raiseFloor24);
}

//
// EV_ActionRaiseFloor24Change
//
static bool EV_ActionRaiseFloor24Change(ev_action_t *action, ev_instance_t *instance)
{
   // case  59: (W1)
   // case  93: (WR)
   // case 160: (S1 - BOOM Extended)
   // case 179: (SR - BOOM Extended)
   // Raise Floor 24 And Change
   return !!EV_DoFloor(instance->line, raiseFloor24AndChange);
}

//
// EV_ActionBuildStairsTurbo16
//
static bool EV_ActionBuildStairsTurbo16(ev_action_t *action, ev_instance_t *instance)
{
   // case 100: (W1)
   // case 127: (S1)
   // case 257: (WR - BOOM Extended)
   // case 259: (SR - BOOM Extended)
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
   // case 173: (S1 - BOOM Extended)
   // case 194: (SR - BOOM Extended)
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
   // case 111: (S1)
   // case 114: (SR)
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
   // case 112: (S1)
   // case 115: (SR)
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
   // case 113: (S1)
   // case 116: (SR)
   // Blazing Door Close (faster than TURBO!)
   return !!EV_DoDoor(instance->line, blazeClose);
}

//
// EV_ActionFloorRaiseToNearest
//
static bool EV_ActionFloorRaiseToNearest(ev_action_t *action, ev_instance_t *instance)
{
   // case  18: (S1)
   // case  69: (SR)
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
   // case 122: (S1)
   // case 123: (SR)
   // Blazing PlatDownWaitUpStay
   return !!EV_DoPlat(instance->line, blazeDWUS, 0);
}

//
// EV_ActionSecretExit
//
static bool EV_ActionSecretExit(ev_action_t *action, ev_instance_t *instance)
{
   Mobj *thing = instance->actor;

   // case 124: (W1)
   // Secret EXIT
   // killough 10/98: prevent zombies from exiting levels
   if(!(thing->player && thing->player->health <= 0 && !comp[comp_zombie]))
      G_SecretExitLevel();

   return true;
}

//
// EV_ActionSwitchSecretExit
//
// This variant of secret exit action is used by switches.
//
static bool EV_ActionSwitchSecretExit(ev_action_t *action, ev_instance_t *instance)
{
   Mobj *thing = instance->actor;

   // case 51: (S1)
   // Secret EXIT
   // killough 10/98: prevent zombies from exiting levels
   if(thing->player && thing->player->health <= 0 && !comp[comp_zombie])
   {
      S_StartSound(thing, GameModeInfo->playerSounds[sk_oof]);
      return false;
   }

   G_SecretExitLevel();
   return true;
}

//
// EV_ActionGunSecretExit
//
// This variant of secret exit action is used by impact specials.
//
static bool EV_ActionGunSecretExit(ev_action_t *action, ev_instance_t *instance)
{
   Mobj *thing = instance->actor;

   // case 198: (G1 - BOOM Extended)
   if(thing->player && thing->player->health <= 0 && !comp[comp_zombie])
      return false;

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
   // case 131: (S1)
   // case 132: (SR)
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
   // case 165: (S1 - BOOM Extended)
   // case 185: (SR - BOOM Extended)
   // Silent Ceiling Crush & Raise
   return !!EV_DoCeiling(instance->line, silentCrushAndRaise);
}

//
// EV_ActionRaiseFloor512
//
static bool EV_ActionRaiseFloor512(ev_action_t *action, ev_instance_t *instance)
{
   // case 140: (S1)
   // case 142: (W1 - BOOM Extended)
   // case 147: (WR - BOOM Extended)
   // case 178: (SR - BOOM Extended)
   // Raise Floor 512
   return !!EV_DoFloor(instance->line, raiseFloor512);
}

//
// EV_ActionPlatRaise24Change
//
static bool EV_ActionPlatRaise24Change(ev_action_t *action, ev_instance_t *instance)
{
   // case  15: (S1)
   // case  66: (SR)
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
   // case  14: (S1)
   // case  67: (SR)
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
   // case 41:  (S1)
   // case 43:  (SR)
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
   // case 191: (SR - BOOM Extended)
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
   // case 203: (S1 - BOOM Extended)
   // case 205: (SR - BOOM Extended)
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
   // case 204: (S1 - BOOM Extended)
   // case 206: (SR - BOOM Extended)
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
   // case 209: (S1 - BOOM Extended)
   // case 210: (SR - BOOM Extended)
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
   // case 189: (S1 - BOOM Extended)
   // case 190: (SR - BOOM Extended)
   // Texture/Type Change Only (Trig)
   return !!EV_DoChange(instance->line, trigChangeOnly);
}

//
// EV_ActionChangeOnlyNumeric
//
static bool EV_ActionChangeOnlyNumeric(ev_action_t *action, ev_instance_t *instance)
{
   //jff 3/15/98 create texture change no motion type
   // case  78: (SR - BOOM Extended)
   // case 239: (W1 - BOOM Extended)
   // case 240: (WR - BOOM Extended)
   // case 241: (S1 - BOOM Extended)
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
   // case 221: (S1 - BOOM Extended)
   // case 222: (SR - BOOM Extended)
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
   // case 229: (S1 - BOOM Extended)
   // case 230: (SR - BOOM Extended)
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
   // case 233: (S1 - BOOM Extended)
   // case 234: (SR - BOOM Extended)
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
   // case 237: (S1 - BOOM Extended)
   // case 238: (SR - BOOM Extended)
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
   // case 211: (SR - BOOM Extended)
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
// EV_ActionDoLockedDoor
//
static bool EV_ActionDoLockedDoor(ev_action_t *action, ev_instance_t *instance)
{
   // case  99: (SR) BlzOpenDoor BLUE
   // case 133: (S1) BlzOpenDoor BLUE
   // case 134: (SR) BlzOpenDoor RED
   // case 135: (S1) BlzOpenDoor RED
   // case 136: (SR) BlzOpenDoor YELLOW
   // case 137: (S1) BlzOpenDoor YELLOW

   // TODO: move special-specific logic out of EV_DoLockedDoor
   
   return !!EV_DoLockedDoor(instance->line, blazeOpen, instance->actor);
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

//
// Parameterized Actions
//
// These are used directly by the Hexen map format and from within ACS scripts,
// and are also available to DOOM-format maps through use of ExtraData.
//

//
// EV_ActionDoorRaise
//
// Implements Door_Raise(tag, speed, delay, lighttag)
//
static bool EV_ActionDoorRaise(ev_action_t *action, ev_instance_t *instance)
{
   doordata_t dd;
   int extflags = instance->line ? instance->line->extflags : EX_ML_REPEAT;

   dd.kind         = OdCDoor;
   dd.spac         = instance->spac;
   dd.speed_value  = instance->args[1] * FRACUNIT / 8;
   dd.topcountdown = 0;
   dd.delay_value  = instance->args[2];
   dd.altlighttag  = instance->args[3];
   
   dd.flags = DDF_HAVESPAC | DDF_USEALTLIGHTTAG;
   if(extflags & EX_ML_REPEAT)
      dd.flags |= DDF_REUSABLE;

   // FIXME/TODO: set genDoorThing in case of manual retrigger
   genDoorThing = instance->actor;

   return !!EV_DoParamDoor(instance->line, instance->args[0], &dd);
}

//
// EV_ActionDoorOpen
//
// Implements Door_Open(tag, speed, lighttag)
//
static bool EV_ActionDoorOpen(ev_action_t *action, ev_instance_t *instance)
{
   doordata_t dd;
   int extflags = instance->line ? instance->line->extflags : EX_ML_REPEAT;

   dd.kind         = ODoor;
   dd.spac         = instance->spac;
   dd.speed_value  = instance->args[1] * FRACUNIT / 8;
   dd.topcountdown = 0;
   dd.delay_value  = 0;
   dd.altlighttag  = instance->args[2];

   dd.flags = DDF_HAVESPAC | DDF_USEALTLIGHTTAG;
   if(extflags & EX_ML_REPEAT)
      dd.flags |= DDF_REUSABLE;

   // FIXME/TODO
   genDoorThing = instance->actor;

   return !!EV_DoParamDoor(instance->line, instance->args[0], &dd);
}

//
// EV_ActionDoorClose
//
// Implements Door_Close(tag, speed, lighttag)
//
static bool EV_ActionDoorClose(ev_action_t *action, ev_instance_t *instance)
{
   doordata_t dd;
   int extflags = instance->line ? instance->line->extflags : EX_ML_REPEAT;

   dd.kind         = CDoor;
   dd.spac         = instance->spac;
   dd.speed_value  = instance->args[1] * FRACUNIT / 8;
   dd.topcountdown = 0;
   dd.delay_value  = 0;
   dd.altlighttag  = instance->args[2];

   dd.flags = DDF_HAVESPAC | DDF_USEALTLIGHTTAG;
   if(extflags & EX_ML_REPEAT)
      dd.flags |= DDF_REUSABLE;

   // FIXME/TODO
   genDoorThing = instance->actor;

   return !!EV_DoParamDoor(instance->line, instance->args[0], &dd);
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

// GR-Type lines may be activated multiple times by hitscan attacks
// FIXME/TODO: In Raven and Rogue games, projectiles activate them as well.
static ev_actiontype_t GRActionType =
{
   SPAC_IMPACT,          // line must be hit
   EV_DOOMPreShootLine,  // pre-activation callback
   EV_DOOMPostShootLine, // post-activation callback (same as use)
   EV_POSTCHANGESWITCH   // change switch texture
};

// G1-Type lines may be activated once, by a hitscan attack.
static ev_actiontype_t G1ActionType = 
{
   SPAC_IMPACT,          // line must be hit
   EV_DOOMPreShootLine,  // pre-activation callback
   EV_DOOMPostShootLine, // post-activation callback
   EV_POSTCHANGESWITCH | EV_POSTCLEARSPECIAL // change switch and clear special
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

// Macro to declare a DOOM-style S1 line action
#define S1LINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &S1ActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version                                \
   }

// Macro to declare a DOOM-style SR line action
#define SRLINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &SRActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version                                \
   }

// Macro to declare a DOOM-style DR line action
#define DRLINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &DRActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version                                \
   }

// Macro to declare a DOOM-style G1 line action
#define G1LINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &G1ActionType,                         \
      EV_Action ## action,                   \
      flags,                                 \
      version                                \
   }

// Macro to declare a DOOM-style GR line action
#define GRLINE(name, action, flags, version) \
   static ev_action_t name =                 \
   {                                         \
      &GRActionType,                         \
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

// DOOM Line Type 14 - S1 Plat Raise 32 and Change
S1LINE(S1PlatRaise32Change, PlatRaise32Change, 0, 0);

// DOOM Line Type 15 - S1 Plat Raise 24 and Change
S1LINE(S1PlatRaise24Change, PlatRaise24Change, 0, 0);

// DOOM Line Type 16 - W1 Close Door, Open in 30
W1LINE(W1CloseDoor30, CloseDoor30, 0, 0);

// DOOM Line Type 17 - W1 Start Light Strobing
W1LINE(W1StartLightStrobing, StartLightStrobing, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 18 - S1 Raise Floor to Next Highest Floor
S1LINE(S1FloorRaiseToNearest, FloorRaiseToNearest, 0, 0);

// DOOM Line Type 19 - W1 Lower Floor
W1LINE(W1LowerFloor, LowerFloor, 0, 0);

// DOOM Line Type 20 - S1 Plat Raise to Nearest and Change
S1LINE(S1PlatRaiseNearestChange, PlatRaiseNearestChange, 0, 0);

// DOOM Line Type 21 - S1 Plat Down Wait Up Stay
S1LINE(S1PlatDownWaitUpStay, PlatDownWaitUpStay, 0, 0);

// DOOM Line Type 22 - W1 Plat Raise to Nearest and Change
W1LINE(W1PlatRaiseNearestChange, PlatRaiseNearestChange, 0, 0);

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
W1LINE(W1Teleport, Teleport, EV_PREALLOWMONSTERS | EV_PREALLOWZEROTAG, 0);

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
G1LINE(G1PlatRaiseNearestChange, PlatRaiseNearestChange, 0, 0);

// DOOM Line Type 49 - S1 Ceiling Crush And Raise
S1LINE(S1CeilingCrushAndRaise, CeilingCrushAndRaise, 0, 0);

// DOOM Line Type 50 - S1 Close Door
S1LINE(S1CloseDoor, CloseDoor, 0, 0);

// DOOM Line Type 51 - S1 Secret Exit
S1LINE(S1SecretExit, SwitchSecretExit, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 52 - WR Exit Level
WRLINE(WRExitLevel, ExitLevel, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 53 - W1 Perpetual Platform Raise
W1LINE(W1PlatPerpetualRaise, PlatPerpetualRaise, 0, 0);

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
SRLINE(SRPlatDownWaitUpStay, PlatDownWaitUpStay, 0, 0);

// DOOM Line Type 63 - SR Raise Door
SRLINE(SRRaiseDoor, RaiseDoor, 0, 0);

// DOOM Line Type 64 - SR Raise Floor to Ceiling
SRLINE(SRRaiseFloor, RaiseFloor, 0, 0);

// DOOM Line Type 65 - SR Raise Floor and Crush
SRLINE(SRFloorRaiseCrush, FloorRaiseCrush, 0, 0);

// DOOM Line Type 66 - SR Plat Raise 24 and Change
SRLINE(SRPlatRaise24Change, PlatRaise24Change, 0, 0);

// DOOM Line Type 67 - SR Plat Raise 32 and Change
SRLINE(SRPlatRaise32Change, PlatRaise32Change, 0, 0);

// DOOM Line Type 68 - SR Plat Raise to Nearest and Change
SRLINE(SRPlatRaiseNearestChange, PlatRaiseNearestChange, 0, 0);

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
WRLINE(WRPlatBlazeDWUS, PlatBlazeDWUS, 0, 0);

// DOOM Line Type 121 - W1 Plat Blaze Down-Wait-Up-Stay
W1LINE(W1PlatBlazeDWUS, PlatBlazeDWUS, 0, 0);

// DOOM Line Type 122 - S1 Plat Blaze Down-Wait-Up-Stay
S1LINE(S1PlatBlazeDWUS, PlatBlazeDWUS, 0, 0);

// DOOM Line Type 123 - SR Plat Blaze Down-Wait-Up-Stay
SRLINE(SRPlatBlazeDWUS, PlatBlazeDWUS, 0, 0);

// DOOM Line Type 124 - WR Secret Exit
WRLINE(WRSecretExit, SecretExit, EV_PREALLOWZEROTAG, 0);

// DOOM Line Type 125 - W1 Teleport Monsters Only
// jff 3/5/98 add ability of monsters etc. to use teleporters
W1LINE(W1TeleportMonsters, Teleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS | EV_PREMONSTERSONLY, 0);

// DOOM Line Type 126 - WR Teleport Monsters Only
WRLINE(WRTeleportMonsters, Teleport, EV_PREALLOWZEROTAG | EV_PREALLOWMONSTERS | EV_PREMONSTERSONLY, 0);

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

// DOOM Line Type 136 - SR Door BLaze Open Yellow Key
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

// BOOM Extended Line Type 158 - S1 Raise Floor to Shortest Lower Texture
S1LINE(S1FloorRaiseToTexture, FloorRaiseToTexture, 0, 200);

// BOOM Extended Line Type 159 - S1 Floor Lower and Change
S1LINE(S1FloorLowerAndChange, FloorLowerAndChange, 0, 200);

// BOOM Extended Line Type 160 - S1 Raise Floor 24 and Change
S1LINE(S1RaiseFloor24Change, RaiseFloor24Change, 0, 200);

// BOOM Extended Line Type 161 - S1 Raise Floor 24
S1LINE(S1RaiseFloor24, RaiseFloor24, 0, 200);

// BOOM Extended Line Type 162 - S1 Plat Perpetual Raise
S1LINE(S1PlatPerpetualRaise, PlatPerpetualRaise, 0, 200);

// BOOM Extended Line Type 163 - S1 Stop Plat
S1LINE(S1PlatStop, PlatStop, EV_POSTCHANGEALWAYS, 200);

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
SRLINE(SRPlatPerpetualRaise, PlatPerpetualRaise, 0, 200);

// BOOM Extended Line Type 182 - SR Plat Stop
SRLINE(SRPlatStop, PlatStop, 0, 200);

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
G1LINE(G1ExitLevel, GunExitLevel, EV_PREALLOWZEROTAG, 200);

// BOOM Extended Line Type 198 - G1 Secret Exit
G1LINE(G1SecretExit, GunSecretExit, EV_PREALLOWZEROTAG, 200);

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
SRLINE(SRPlatToggleUpDown, PlatToggleUpDown, 0, 200);

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
W1LINE(W1ElevatorUp, ElevatorUp, 0, 200);

// BOOM Extended Line Type 228 - WR Raise Elevator to Next Floor
WRLINE(WRElevatorUp, ElevatorUp, 0, 200);

// BOOM Extended Line Type 229 - S1 Raise Elevator to Next Floor
S1LINE(S1ElevatorUp, ElevatorUp, 0, 200);

// BOOM Extended Line Type 230 - SR Raise Elevator to Next Floor
SRLINE(SRElevatorUp, ElevatorUp, 0, 200);

// BOOM Extended Line Type 231 - W1 Lower Elevator to Next Floor
W1LINE(W1ElevatorDown, ElevatorDown, 0, 200);

// BOOM Extended Line Type 232 - WR Lower Elevator to Next Floor
WRLINE(WRElevatorDown, ElevatorDown, 0, 200);

// BOOM Extended Line Type 233 - S1 Lower Elevator to Next Floor
S1LINE(S1ElevatorDown, ElevatorDown, 0, 200);

// BOOM Extended Line Type 234 - SR Lower Elevator to Next Floor
SRLINE(SRElevatorDown, ElevatorDown, 0, 200);

// BOOM Extended Line Type 235 - W1 Elevator to Current Floor
W1LINE(W1ElevatorCurrent, ElevatorCurrent, 0, 200);

// BOOM Extended Line Type 236 - WR Elevator to Current Floor
WRLINE(WRElevatorCurrent, ElevatorCurrent, 0, 200);

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

#define LINESPEC(number, action) { number, &action },

// DOOM Bindings
static ev_binding_t DOOMBindings[] =
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
   // TODO: 48 - Scroll Texture (static init)
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
   // TODO: 85 (static init)
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
   // TODO: 213 - 218?
   LINESPEC(219, W1FloorLowerToNearest)
   LINESPEC(220, WRFloorLowerToNearest)
   LINESPEC(221, S1FloorLowerToNearest)
   LINESPEC(222, SRFloorLowerToNearest)
   // TODO: 223 - 226?
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
   // TODO: 242?
   LINESPEC(243, W1SilentLineTeleport)
   LINESPEC(244, WRSilentLineTeleport)
   // TODO: 245-255?
   LINESPEC(256, WRBuildStairsUp8)
   LINESPEC(257, WRBuildStairsTurbo16)
   LINESPEC(258, SRBuildStairsUp8)
   LINESPEC(259, SRBuildStairsTurbo16)
   // TODO: 260, 261?
   LINESPEC(262, W1SilentLineTeleportReverse)
   LINESPEC(263, WRSilentLineTeleportReverse)
   LINESPEC(264, W1SilentLineTRMonsters)
   LINESPEC(265, WRSilentLineTRMonsters)
   LINESPEC(266, W1SilentLineTeleMonsters)
   LINESPEC(267, WRSilentLineTeleMonsters)
   LINESPEC(268, W1SilentTeleportMonsters)
   LINESPEC(269, WRSilentTeleportMonsters)
   // TODO: 270-272?
   LINESPEC(273, WRStartLineScript1S)
   LINESPEC(274, W1StartLineScript)
   LINESPEC(275, W1StartLineScript1S)
   LINESPEC(276, SRStartLineScript)
   LINESPEC(277, S1StartLineScript)
   LINESPEC(278, GRStartLineScript)
   LINESPEC(279, G1StartLineScript)
   LINESPEC(280, WRStartLineScript)
   // TODO: EE Line Types 281+
};


//=============================================================================
//
// Special Resolution
//
// Lookup an action by number.
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

//=============================================================================
//
// Static Init Lines
//
// Rather than activatable triggers, these line types are used to setup some
// sort of persistent effect in the map at level load time.
//

#define STATICSPEC(line, function) { line, function },

// DOOM Static Init Bindings
static ev_static_t DOOMStaticBindings[] =
{
   STATICSPEC( 48, EV_STATIC_SCROLL_LINE_LEFT)
   STATICSPEC( 85, EV_STATIC_SCROLL_LINE_RIGHT)
   STATICSPEC(213, EV_STATIC_LIGHT_TRANSFER_FLOOR)
   STATICSPEC(214, EV_STATIC_SCROLL_ACCEL_CEILING)
   STATICSPEC(215, EV_STATIC_SCROLL_ACCEL_FLOOR)
   STATICSPEC(216, EV_STATIC_CARRY_ACCEL_FLOOR)
   STATICSPEC(217, EV_STATIC_SCROLL_CARRY_ACCEL_FLOOR)
   STATICSPEC(218, EV_STATIC_SCROLL_ACCEL_WALL)
   STATICSPEC(223, EV_STATIC_FRICTION_TRANSFER)
   STATICSPEC(224, EV_STATIC_WIND_CONTROL)
   STATICSPEC(225, EV_STATIC_CURRENT_CONTROL)
   STATICSPEC(226, EV_STATIC_PUSHPULL_CONTROL)
   STATICSPEC(242, EV_STATIC_TRANSFER_HEIGHTS)
   STATICSPEC(245, EV_STATIC_SCROLL_DISPLACE_CEILING)
   STATICSPEC(246, EV_STATIC_SCROLL_DISPLACE_FLOOR)
   STATICSPEC(247, EV_STATIC_CARRY_DISPLACE_FLOOR)
   STATICSPEC(248, EV_STATIC_SCROLL_CARRY_DISPLACE_FLOOR)
   STATICSPEC(249, EV_STATIC_SCROLL_DISPLACE_WALL)
   STATICSPEC(250, EV_STATIC_SCROLL_CEILING)
   STATICSPEC(251, EV_STATIC_SCROLL_FLOOR)
   STATICSPEC(252, EV_STATIC_CARRY_FLOOR)
   STATICSPEC(253, EV_STATIC_SCROLL_CARRY_FLOOR)
   STATICSPEC(254, EV_STATIC_SCROLL_WALL_WITH)
   STATICSPEC(255, EV_STATIC_SCROLL_BY_OFFSETS)
   STATICSPEC(260, EV_STATIC_TRANSLUCENT)
   STATICSPEC(261, EV_STATIC_LIGHT_TRANSFER_CEILING)
   STATICSPEC(270, EV_STATIC_EXTRADATA_LINEDEF)
   STATICSPEC(271, EV_STATIC_SKY_TRANSFER)
   STATICSPEC(272, EV_STATIC_SKY_TRANSFER_FLIPPED)
   STATICSPEC(281, EV_STATIC_3DMIDTEX_ATTACH_FLOOR)
   STATICSPEC(282, EV_STATIC_3DMIDTEX_ATTACH_CEILING)
   STATICSPEC(283, EV_STATIC_PORTAL_PLANE_CEILING)
   STATICSPEC(284, EV_STATIC_PORTAL_PLANE_FLOOR)
   STATICSPEC(285, EV_STATIC_PORTAL_PLANE_CEILING_FLOOR)
   STATICSPEC(286, EV_STATIC_PORTAL_HORIZON_CEILING)
   STATICSPEC(287, EV_STATIC_PORTAL_HORIZON_FLOOR)
   STATICSPEC(288, EV_STATIC_PORTAL_HORIZON_CEILING_FLOOR)
   STATICSPEC(289, EV_STATIC_PORTAL_LINE)
   STATICSPEC(290, EV_STATIC_PORTAL_SKYBOX_CEILING)
   STATICSPEC(291, EV_STATIC_PORTAL_SKYBOX_FLOOR)
   STATICSPEC(292, EV_STATIC_PORTAL_SKYBOX_CEILING_FLOOR)
   STATICSPEC(293, EV_STATIC_HERETIC_WIND)
   STATICSPEC(294, EV_STATIC_HERETIC_CURRENT)
   STATICSPEC(295, EV_STATIC_PORTAL_ANCHORED_CEILING)
   STATICSPEC(296, EV_STATIC_PORTAL_ANCHORED_FLOOR)
   STATICSPEC(297, EV_STATIC_PORTAL_ANCHORED_CEILING_FLOOR)
   STATICSPEC(298, EV_STATIC_PORTAL_ANCHOR)
   STATICSPEC(299, EV_STATIC_PORTAL_ANCHOR_FLOOR)
   STATICSPEC(344, EV_STATIC_PORTAL_TWOWAY_CEILING)
   STATICSPEC(345, EV_STATIC_PORTAL_TWOWAY_FLOOR)
   STATICSPEC(346, EV_STATIC_PORTAL_TWOWAY_ANCHOR)
   STATICSPEC(347, EV_STATIC_PORTAL_TWOWAY_ANCHOR_FLOOR)
   STATICSPEC(348, EV_STATIC_POLYOBJ_START_LINE)
   STATICSPEC(349, EV_STATIC_POLYOBJ_EXPLICIT_LINE)
   STATICSPEC(358, EV_STATIC_PORTAL_LINKED_CEILING)
   STATICSPEC(359, EV_STATIC_PORTAL_LINKED_FLOOR)
   STATICSPEC(360, EV_STATIC_PORTAL_LINKED_ANCHOR)
   STATICSPEC(361, EV_STATIC_PORTAL_LINKED_ANCHOR_FLOOR)
   STATICSPEC(376, EV_STATIC_PORTAL_LINKED_LINE2LINE)
   STATICSPEC(377, EV_STATIC_PORTAL_LINKED_L2L_ANCHOR)
   STATICSPEC(378, EV_STATIC_LINE_SET_IDENTIFICATION)
   STATICSPEC(379, EV_STATIC_ATTACH_SET_CEILING_CONTROL)
   STATICSPEC(380, EV_STATIC_ATTACH_SET_FLOOR_CONTROL)
   STATICSPEC(381, EV_STATIC_ATTACH_FLOOR_TO_CONTROL)
   STATICSPEC(382, EV_STATIC_ATTACH_CEILING_TO_CONTROL)
   STATICSPEC(383, EV_STATIC_ATTACH_MIRROR_FLOOR)
   STATICSPEC(384, EV_STATIC_ATTACH_MIRROR_CEILING)
   STATICSPEC(385, EV_STATIC_PORTAL_APPLY_FRONTSECTOR)
   STATICSPEC(386, EV_STATIC_SLOPE_FSEC_FLOOR)
   STATICSPEC(387, EV_STATIC_SLOPE_FSEC_CEILING)
   STATICSPEC(388, EV_STATIC_SLOPE_FSEC_FLOOR_CEILING)
   STATICSPEC(389, EV_STATIC_SLOPE_BSEC_FLOOR)
   STATICSPEC(390, EV_STATIC_SLOPE_BSEC_CEILING)
   STATICSPEC(391, EV_STATIC_SLOPE_BSEC_FLOOR_CEILING)
   STATICSPEC(392, EV_STATIC_SLOPE_BACKFLOOR_FRONTCEILING)
   STATICSPEC(393, EV_STATIC_SLOPE_FRONTFLOOR_BACKCEILING)
   STATICSPEC(394, EV_STATIC_SLOPE_FRONTFLOOR_TAG)
   STATICSPEC(395, EV_STATIC_SLOPE_FRONTCEILING_TAG)
   STATICSPEC(396, EV_STATIC_SLOPE_FRONTFLOORCEILING_TAG)
   STATICSPEC(401, EV_STATIC_EXTRADATA_SECTOR)
};

// Hexen Static Init Bindings
static ev_static_t HexenStaticBindings[] =
{
   STATICSPEC(  1, EV_STATIC_POLYOBJ_START_LINE)
   STATICSPEC(  5, EV_STATIC_POLYOBJ_EXPLICIT_LINE)
   STATICSPEC(121, EV_STATIC_LINE_SET_IDENTIFICATION)
};

//
// Hash Tables
//

typedef EHashTable<ev_static_t, EIntHashKey, 
                   &ev_static_t::staticFn, &ev_static_t::links> EV_StaticHash;

// DOOM hash
static EV_StaticHash DOOMStaticHash(earrlen(DOOMStaticBindings));


//
// EV_AddStaticSpecialsToHash
//
// Add an array of static specials to a hash table.
//
static void EV_AddStaticSpecialsToHash(EV_StaticHash &hash, 
                                       ev_static_t *bindings, size_t count)
{
   // Create a duplicate of the entire bindings array so that the items can
   // be linked into multiple hash tables at runtime.
   ev_static_t *newBindings = estructalloc(ev_static_t, count);

   memcpy(newBindings, bindings, count * sizeof(*bindings));

   for(size_t i = 0; i < count; i++)
      hash.addObject(newBindings[i]);
}

//
// EV_InitDOOMStaticHash
//
// First-time-use initialization for the DOOM static specials hash table.
//
static void EV_InitDOOMStaticHash()
{
   static bool firsttime = true;

   if(firsttime)
   {
      firsttime = false;
      
      // add every item in the DOOM static bindings array
      EV_AddStaticSpecialsToHash(DOOMStaticHash, DOOMStaticBindings, 
                                 earrlen(DOOMStaticBindings));
   }
}

//
// EV_DOOMSpecialForStaticInit
//
// Always looks up a special in the DOOM gamemode's static init list, regardless
// of the map format or gamemode in use. Returns 0 if no such special exists.
//
int EV_DOOMSpecialForStaticInit(int staticFn)
{
   ev_static_t *binding;

   // init the hash if it hasn't been done yet
   EV_InitDOOMStaticHash();

   return (binding = DOOMStaticHash.objectForKey(staticFn)) ? binding->actionNumber : 0;
}

//
// EV_HereticSpecialForStaticInit
//
// Always looks up a special in the Heretic gamemode's static init list, 
// regardless of the map format or gamemode in use. Returns 0 if no such 
// special exists.
//
int EV_HereticSpecialForStaticInit(int staticFn)
{
   // There is only one difference between Heretic and DOOM regarding static
   // init specials; line type 99 is equivalent to BOOM extended type 85, 
   // scroll line right.

   if(staticFn == EV_STATIC_SCROLL_LINE_RIGHT)
      return 99;

   return EV_DOOMSpecialForStaticInit(staticFn);
}

//
// EV_HexenSpecialForStaticInit
//
// Always looks up a special in the Hexen gamemode's static init list, 
// regardless of the map format or gamemode in use. Returns 0 if no such special
// exists.
//
int EV_HexenSpecialForStaticInit(int staticFn)
{
   // TODO
   return 0;
}

// 
// EV_StrifeSpecialForStaticInit
//
// Always looks up a special in the Strife gamemode's static init list,
// regardless of the map format or gamemode in use. Returns 0 if no such special
// exists.
//
int EV_StrifeSpecialForStaticInit(int staticFn)
{
   // TODO
   return 0;
}

typedef int (*EV_hashFunc)(int);

// Get a hash table lookup function for a particular map type
static EV_hashFunc hashFuncForMapType[LI_TYPE_NUMTYPES] =
{
   EV_DOOMSpecialForStaticInit,    // DOOM
   EV_HereticSpecialForStaticInit, // Heretic
   EV_HexenSpecialForStaticInit,   // Hexen
   EV_StrifeSpecialForStaticInit,  // Strife
};

//
// EV_SpecialForStaticInit
//
// Pass in the symbolic static function name you want the line special for; it
// will return the line special number currently bound to that function for the
// currently active map type.
//
int EV_SpecialForStaticInit(int staticFn)
{
   EV_hashFunc hashFn = hashFuncForMapType[LevelInfo.levelType];

   return hashFn(staticFn);
}

// EOF

