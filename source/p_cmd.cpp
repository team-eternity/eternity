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
//------------------------------------------------------------------------------
//
// Purpose: Game console variables.
// Authors: Simon Howard, James Haley, Joe Kennedy, David Hill, Ioan Chera
//

/* includes ************************/

#include "z_zone.h"

#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"

#include "acs_intr.h"
#include "am_map.h"
#include "d_main.h"
#include "doomstat.h"
#include "f_wipe.h"
#include "g_game.h"
#include "m_random.h"
#include "mn_engin.h"
#include "p_anim.h"
#include "p_info.h"
#include "p_map.h"
#include "p_mobj.h"
#include "p_inter.h"
#include "p_spec.h"
#include "p_user.h"
#include "r_draw.h"
#include "v_misc.h"

/***************************************************************************
                'defines': string values for variables
***************************************************************************/

const char *yesno[] = { "no", "yes" };
const char *onoff[] = { "off", "on" };

// clang-format off

const char *colournames[] = 
{ 
    "green",  "indigo", "brown",  "red",
    "tomato", "dirt",   "blue",   "gold",
    "sea",    "black",  "purple", "orange",
    "pink",   "cream",  "white"
};

const char *textcolours[] =
{
    FC_BRICK  "brick"  FC_NORMAL,
    FC_TAN    "tan"    FC_NORMAL,
    FC_GRAY   "gray"   FC_NORMAL,
    FC_GREEN  "green"  FC_NORMAL,
    FC_BROWN  "brown"  FC_NORMAL,
    FC_GOLD   "gold"   FC_NORMAL,
    FC_RED    "red"    FC_NORMAL,
    FC_BLUE   "blue"   FC_NORMAL,
    FC_ORANGE "orange" FC_NORMAL,
    FC_YELLOW "yellow" FC_NORMAL
};

const char *skills[]=
{
    "im too young to die",
    "hey not too rough",
    "hurt me plenty",
    "ultra violence",
    "nightmare"
};

// clang-format on

const char *bfgtypestr[5] = { "bfg9000", "press release", "bfg11k", "bouncing", "plasma burst" };
const char *dmstr[]       = { "single", "coop", "deathmatch" };

/*************************************************************************
        Constants
 *************************************************************************/

// haleyjd: had to change this into a command
CONSOLE_COMMAND(creator, 0)
{
    C_Printf("creator is '%s'\n", LevelInfo.creator);
}

/*************************************************************************
                Game variables
 *************************************************************************/

//
// NETCODE_FIXME: NETWORK VARIABLES
//

// player colour

//
// NETCODE_FIXME: See notes in d_net.c about the handling of player
// colors. It needs serious refinement.
//

VARIABLE_INT(default_colour, nullptr, 0, TRANSLATIONCOLOURS, colournames);
CONSOLE_NETVAR(colour, default_colour, cf_handlerset, netcmd_colour)
{
    int playernum, colour;

    if(!Console.argc)
        return;

    playernum = Console.cmdsrc;

    colour = Console.argv[0]->toInt();

    if(colour < 0)
        colour = 0;
    if(colour > TRANSLATIONCOLOURS)
        colour = TRANSLATIONCOLOURS;

    players[playernum].colormap = colour;
    if(gamestate == GS_LEVEL)
        players[playernum].mo->colour = colour;

    if(playernum == consoleplayer)
        default_colour = colour; // typed
}

// deathmatch

// VARIABLE_INT(deathmatch, nullptr,               0, 3, dmstr);
// CONSOLE_NETVAR(deathmatch, deathmatch, cf_server, netcmd_deathmatch) {}

//
// NETCODE_FIXME: Changing gametype at run time could present problems
// unless handled very carefully.
//

VARIABLE_INT(GameType, &DefaultGameType, gt_single, gt_dm, dmstr);
CONSOLE_NETVAR(gametype, GameType, cf_server, netcmd_deathmatch)
{
    if(netgame && GameType == gt_single) // not allowed
        GameType = DefaultGameType = gt_coop;
}

// skill level

VARIABLE_INT(gameskill, &defaultskill, 0, 4, skills);
CONSOLE_NETVAR(skill, gameskill, cf_server, netcmd_skill)
{
    if(!Console.argc)
        return;

    startskill = gameskill = (skill_t)(Console.argv[0]->toInt());
    if(Console.cmdsrc == consoleplayer)
        defaultskill = static_cast<skill_t>(gameskill + 1);
}

// allow mlook

VARIABLE_BOOLEAN(allowmlook, &default_allowmlook, onoff);
CONSOLE_NETVAR(allowmlook, allowmlook, cf_server, netcmd_allowmlook) {}

// bfg type

//
// NETCODE_FIXME: Each player should be able to have their own BFG
// type. Changing this will also require propagated changes to the
// weapon system.
//

VARIABLE_INT(bfgtype, &default_bfgtype, 0, 4, bfgtypestr);
CONSOLE_NETVAR(bfgtype, bfgtype, cf_server, netcmd_bfgtype) {}

// autoaiming

//
// NETCODE_FIXME: Players should be able to have their own autoaim
// settings. Changing this will also require propagated changes to
// the weapon system.
//

VARIABLE_BOOLEAN(autoaim, &default_autoaim, onoff);
CONSOLE_NETVAR(autoaim, autoaim, cf_server, netcmd_autoaim) {}

// weapons recoil

VARIABLE_BOOLEAN(weapon_recoil, &default_weapon_recoil, onoff);
CONSOLE_NETVAR(recoil, weapon_recoil, cf_server, netcmd_recoil) {}

// allow pushers

VARIABLE_BOOLEAN(allow_pushers, &default_allow_pushers, onoff);
CONSOLE_NETVAR(pushers, allow_pushers, cf_server, netcmd_pushers) {}

// varying friction

VARIABLE_BOOLEAN(variable_friction, &default_variable_friction, onoff);
CONSOLE_NETVAR(varfriction, variable_friction, cf_server, netcmd_varfriction) {}

// enable nukage

extern int enable_nuke; // p_spec.c
VARIABLE_BOOLEAN(enable_nuke, nullptr, onoff);
CONSOLE_NETVAR(nukage, enable_nuke, cf_server, netcmd_nukage) {}

// allow mlook with bfg

// davidph 06/06/12 -- haleyjd 06/07/12: promoted to netvar since sync-critical
VARIABLE_TOGGLE(pitchedflight, &default_pitchedflight, onoff)
CONSOLE_NETVAR(p_pitchedflight, pitchedflight, cf_server, netcmd_pitchedflight) {}

// 'auto exit' variables

VARIABLE_INT(levelTimeLimit, nullptr, 0, 100, nullptr);
CONSOLE_NETVAR(timelimit, levelTimeLimit, cf_server, netcmd_timelimit) {}

VARIABLE_INT(levelFragLimit, nullptr, 0, 100, nullptr);
CONSOLE_NETVAR(fraglimit, levelFragLimit, cf_server, netcmd_fraglimit) {}

/************** monster variables ***********/

// fast monsters

VARIABLE_TOGGLE(fastparm, &clfastparm, onoff);
CONSOLE_NETVAR(fast, fastparm, cf_server, netcmd_fast)
{
    G_SetFastParms(fastparm); // killough 4/10/98: set -fast parameter correctly
}

// no monsters

VARIABLE_TOGGLE(nomonsters, &clnomonsters, onoff);
CONSOLE_NETVAR(nomonsters, nomonsters, cf_server, netcmd_nomonsters)
{
    if(gamestate == GS_LEVEL)
        C_Printf("note: nomonsters will not change until next level\n");
    if(menuactive)
        MN_ErrorMsg("does not take effect until next level");
}

// respawning monsters

VARIABLE_TOGGLE(respawnparm, &clrespawnparm, onoff);
CONSOLE_NETVAR(respawn, respawnparm, cf_server, netcmd_respawn)
{
    if(gamestate == GS_LEVEL)
        C_Printf("note: respawn will change on new game\n");
    if(menuactive)
        MN_ErrorMsg("will take effect on new game");
}

// monsters remember

VARIABLE_BOOLEAN(monsters_remember, &default_monsters_remember, onoff);
CONSOLE_NETVAR(mon_remember, monsters_remember, cf_server, netcmd_monremember) {}

// infighting among monsters

VARIABLE_BOOLEAN(monster_infighting, &default_monster_infighting, onoff);
CONSOLE_NETVAR(mon_infight, monster_infighting, cf_server, netcmd_moninfight) {}

// monsters backing out

VARIABLE_BOOLEAN(monster_backing, &default_monster_backing, onoff);
CONSOLE_NETVAR(mon_backing, monster_backing, cf_server, netcmd_monbacking) {}

// monsters avoid hazards

VARIABLE_BOOLEAN(monster_avoid_hazards, &default_monster_avoid_hazards, onoff);
CONSOLE_NETVAR(mon_avoid, monster_avoid_hazards, cf_server, netcmd_monavoid) {}

// monsters affected by friction

VARIABLE_BOOLEAN(monster_friction, &default_monster_friction, onoff);
CONSOLE_NETVAR(mon_friction, monster_friction, cf_server, netcmd_monfriction) {}

// monsters climb tall steps

VARIABLE_BOOLEAN(monkeys, &default_monkeys, onoff);
CONSOLE_NETVAR(mon_climb, monkeys, cf_server, netcmd_monclimb) {}

// help dying friends

VARIABLE_BOOLEAN(help_friends, &default_help_friends, onoff);
CONSOLE_NETVAR(mon_helpfriends, help_friends, cf_server, netcmd_monhelpfriends) {}

// distance friends keep from player

VARIABLE_INT(distfriend, &default_distfriend, 0, 1024, nullptr);
CONSOLE_NETVAR(mon_distfriend, distfriend, cf_server, netcmd_mondistfriend) {}

static const char *spechit_strs[] = { "off", "chocodoom", "prboomplus" };

// haleyjd 09/20/06: spechits overflow emulation
VARIABLE_INT(spechits_emulation, nullptr, 0, 2, spechit_strs);
CONSOLE_VARIABLE(spechits_emulation, spechits_emulation, 0) {}

VARIABLE_TOGGLE(donut_emulation, nullptr, onoff);
CONSOLE_VARIABLE(donut_emulation, donut_emulation, 0) {}

// haleyjd 01/24/07: spawn Unknowns for missing things on maps?
VARIABLE_BOOLEAN(p_markunknowns, nullptr, yesno);
CONSOLE_VARIABLE(p_markunknowns, p_markunknowns, 0) {}

// haleyjd 10/09/07
extern int wipewait;

static const char *wipewait_strs[] = { "never", "always", "demos" };
static const char *wipetype_strs[] = { "none", "melt", "fade" };

VARIABLE_INT(wipewait, nullptr, 0, 2, wipewait_strs);
CONSOLE_VARIABLE(wipewait, wipewait, 0) {}

VARIABLE_INT(wipetype, nullptr, 0, 2, wipetype_strs);
CONSOLE_VARIABLE(wipetype, wipetype, 0) {}

void P_Chase_AddCommands(void);
void P_Skin_AddCommands(void);

CONSOLE_COMMAND(spacejump, cf_hidden | cf_notnet)
{
    if(gamestate == GS_LEVEL)
        players[0].mo->momz = 10 * FRACUNIT;
}

CONSOLE_COMMAND(puke, cf_notnet)
{
    if(Console.argc < 1)
        return;

    uint32_t args[5] = { 0, 0, 0, 0, 0 };

    for(int i = 1; i < Console.argc && i <= 5; i++)
        args[i - 1] = Console.argv[i]->toInt();

    ACS_ExecuteScriptIAlways(Console.argv[0]->toInt(), gamemap, args, 5, nullptr, nullptr, 0, nullptr);
}

CONSOLE_COMMAND(enable_lightning, 0)
{
    LevelInfo.hasLightning = true;
    P_InitLightning();
}

CONSOLE_COMMAND(thunder, 0)
{
    P_ForceLightning();
}

// EOF

