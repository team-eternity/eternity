// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 Charles Gunyon
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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Extensible game types.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_player.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_lib.h"
#include "g_type.h"
#include "g_coop.h"
#include "g_ctf.h"
#include "g_dmatch.h"
#include "i_system.h"
#include "info.h"
#include "m_fixed.h"
#include "m_misc.h"

#include "cs_announcer.h"
#include "cs_config.h"
#include "cs_team.h"

/*
 * [CG] It's time to specify exactly what "game type" means.
 *      A game type specifies the current rules of the game.  For example,
 *      in deathmatch, killing other players adds to your score and killing
 *      yourself subtracts from it.  In team deathmatch, killing players on the
 *      other team adds to your score, and killing yourself or other players on
 *      your team subtracts from it.
 *
 *      Doom conflates "game type" with "what stuff do I spawn" and "how does
 *      the HUD look", and while game types should be able to specify that kind
 *      of thing, Doom's singleplayer/cooperative/deathmatch concepts are too
 *      limiting once teams are added to the mix.  It's conceivable, for
 *      example to have a team cooperative mode.  Do deathmatch items spawn in
 *      that mode?
 *
 *      Therefore, Doom's singleplayer/cooperative/deathmatch concepts are
 *      enshrined in the gametype_t enum as gt_single, gt_coop and gt_dm
 *      respectively.  These options control only what spawns and what the
 *      classic status bar looks like.  Everything else is handled by the
 *      chosen game type instance.  In this way, users can specify what stuff
 *      they want spawned and what they want the status bar to look like
 *      entirely separately from game type, which is as it should be.
 */

/*
 * [CG] How game types work:
 *
 *      Game types are expected to handle the following events:
 *        - An actor is spawned
 *          - P_SpawnMobj
 *        - An actor is damaged
 *          - P_DamageMobj
 *        - An actor dies
 *          - P_KillMobj
 *        - An actor touches an item
 *          - P_TouchSpecialThing
 *        - An actor activates a line
 *          - P_UseSpecialLine
 *          - P_CrossSpecialLine
 *          - P_ShootSpecialLine
 *        - An actor enters a sector
 *          - P_SetThingPosition
 *        - An actor's state changes
 *          - P_SetMobjState
 *        - A sector moves
 *          - ???
 *
 *      The default is, of course, to do nothing, but game types can override
 *      their handlers with their own behavior.  They are also expected to
 *      check the configuration in order to prevent strange situations, like
 *      enabling CTF with max_teams set to < 2.
 *
 *      Finally, game types shouldn't assume singleplayer, p2p or c/s; they
 *      should work equally well in all 3 modes.
 */

BaseGameType *current_game_type = NULL;

BaseGameType::BaseGameType(const char *new_name)
{
   E_ReplaceString(name, estrdup(new_name));
   E_ReplaceString(all_caps_name, estrdup(new_name));
   M_Strupr(all_caps_name);
}

BaseGameType::BaseGameType(const char *new_name, uint8_t new_id)
{
   id = new_id;
   E_ReplaceString(name, estrdup(new_name));
   E_ReplaceString(all_caps_name, estrdup(new_name));
   M_Strupr(all_caps_name);
}

BaseGameType::~BaseGameType()
{
   if(name)
      efree(name);

   if(all_caps_name)
      efree(all_caps_name);
}

uint8_t BaseGameType::getID() const
{
   return id;
}

const char* BaseGameType::getName()
{
   return name;
}

const char* BaseGameType::getCapitalizedName()
{
   return all_caps_name;
}

int BaseGameType::getStrengthOfVictory(float low_score, float high_score)
{
   return sov_normal;
}

bool BaseGameType::shouldExitLevel() { return false; }

void BaseGameType::handleNewLevel(skill_t skill, char *name)
{
}

void BaseGameType::handleLoadLevel()
{
}

void BaseGameType::handleExitLevel()
{
}

bool BaseGameType::usesFlagsAsScore() { return false; }

void BaseGameType::handleActorSpawned(fixed_t x, fixed_t y, fixed_t z,
                                      mobjtype_t type, Mobj *actor)
{
}

void BaseGameType::handleActorDamaged(Mobj *target, Mobj *inflictor,
                                      Mobj *source, int damage, int mod)
{
}

void BaseGameType::handleActorKilled(Mobj *source, Mobj *target, emod_t *mod)
{
}

void BaseGameType::handleActorRemoved(Mobj *actor)
{
}

bool BaseGameType::handleActorTouchedSpecial(Mobj *special, Mobj *toucher)
{
   return false;
}

void BaseGameType::handleActorUsedSpecialLine(Mobj *thing, line_t *line,
                                              int side)
{
}

void BaseGameType::handleActorShotSpecialLine(Mobj *thing, line_t *line,
                                              int side)
{
}

void BaseGameType::handleActorCrossedSpecialLine(Mobj *thing, line_t *line,
                                                 int side)
{
}

void BaseGameType::handleActorPositionSet(Mobj *thing)
{
}

void BaseGameType::handleActorPositionUnset(Mobj *thing)
{
}

void BaseGameType::handleActorStateChanged(Mobj *mobj, statenum_t state,
                                           bool still_exists)
{
}

void BaseGameType::handlePlayerInSpecialSector(player_t *player)
{
}

void BaseGameType::handleClientSpectated(int clientnum)
{
}

void BaseGameType::handleClientChangedTeam(int clientnum)
{
}

void BaseGameType::handleClientDisconnected(int clientnum)
{
}

void BaseGameType::tick()
{
}

void G_SetGameType(uint8_t game_type_id)
{
   if(current_game_type)
      efree(current_game_type);

   // [CG] This, of course, isn't extensible, but until I know how scripting
   //      will work I don't know exactly what modders will want to do when
   //      adding new custom game modes, so this is the way it is for now.
   switch(game_type_id)
   {
      case xgt_none:
         current_game_type = new BaseGameType("Eternity Engine");
         break;
      case xgt_cooperative:
         if(CS_TEAMS_ENABLED)
            current_game_type = new CoopGameType("Team Cooperative");
         else
            current_game_type = new CoopGameType("Cooperative");
         break;
      case xgt_duel:
         current_game_type = new DeathmatchGameType("Duel");
         break;
      case xgt_deathmatch:
         if(CS_TEAMS_ENABLED)
            current_game_type = new DeathmatchGameType("Team Deathmatch");
         else
            current_game_type = new DeathmatchGameType("Deathmatch");
         break;
      case xgt_capture_the_flag:
         current_game_type = new CTFGameType("Capture The Flag");
         break;
      default:
         I_Error("Invalid game type ID %u.\n", game_type_id);
         break;
   }
}

void G_NewGameType()
{
   G_SetGameType(xgt_none);
}

