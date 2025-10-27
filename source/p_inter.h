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
// Purpose: Constants and functions for object interaction.
//  External variables editable by DeHackEd patch.
//
// Authors: James Haley, Ioan Chera, Max Waine
//

#ifndef P_INTER_H__
#define P_INTER_H__

struct emodmorph_t;
struct player_t;
class Mobj;
class MetaTable;

using itemeffect_t = MetaTable;

// follow a player exlusively for 3 seconds
static constexpr int16_t BASETHRESHOLD = 100;

enum
{
    GOD_BREACH_DAMAGE        = 10000, // common damage that bypasses invulnerability
    LESSER_GOD_BREACH_DAMAGE = 1000,  // this one bypasses less cases but is there (P_DamageMobj)
};

bool P_GiveInventory(player_t *player, itemeffect_t *item, const int itemamount = 1, const int power = NUMPOWERS);
bool P_TakeInventory(player_t *player, itemeffect_t *item, const int itemamount = 1, const int power = NUMPOWERS);
int  P_CheckInventory(player_t *player, itemeffect_t *item, const int power = NUMPOWERS);
bool P_ClearInventory(player_t *player);
bool P_UseInventory(player_t *player, itemeffect_t *item);
int  P_GetMaxInventory(player_t *player, itemeffect_t *item, const int power = NUMPOWERS);

bool P_GiveWeaponByGiver(player_t &player, itemeffect_t *giver, bool ignoreskill, int itemamount = 1,
                         bool givemax = false);
bool P_TakeWeaponByGiver(player_t &player, itemeffect_t *giver, bool ignoreskill, int itemamount = 1);

bool P_GiveAmmoPickup(player_t &, const itemeffect_t *, bool, int, int itemamount = 1, bool givemax = false);
bool P_TakeAmmoPickup(player_t &, const itemeffect_t *, int itemamount = 1);

bool P_GiveBody(player_t &, const itemeffect_t *, int itemamount = 1, bool givemax = false);
bool P_TakeBody(player_t &, const itemeffect_t *, int itemamount = 1);

bool P_GiveArmor(player_t &, const itemeffect_t *, int itemamount = 1, bool givemax = false);
bool P_TakeArmor(player_t &, const itemeffect_t *, int itemamount = 1);

// MaxW 2016/07/23: P_GivePower is no longer required for external use;
// previously it was used in m_cheats, but the CheatX powereffects mean
// that P_GivePowerForItem can be used.
bool P_GivePowerForItem(player_t &, const itemeffect_t *, int itemamount = 1, bool givemax = false);
bool P_TakePowerForItem(player_t &, const itemeffect_t *, int itemamount = 1);

bool P_GivePower(player_t &player, int power, int duration, bool permanent, bool additiveTime);
bool P_TakePower(player_t &player, int power, int itemamount = 1);

e_pickupfx_t *P_GetPickUpEffect(const Mobj *special);
void          P_TouchSpecialThing(Mobj *special, Mobj *toucher);
void          P_DamageMobj(Mobj *target, Mobj *inflictor, Mobj *source, int damage, int mod);
void          P_DropItems(Mobj *actor, bool tossitems);
bool          P_MorphPlayer(const emodmorph_t &minfo, player_t &player);

void P_Whistle(Mobj *actor, int mobjtype);

// ioanch 20160221
// Archvile interaction check. Used by the A_VileChase and Thing_Raise
bool P_ThingIsCorpse(const Mobj *mobj);
bool P_CheckCorpseRaiseSpace(Mobj *corpse);
void P_RaiseCorpse(Mobj *corpse, const Mobj *raiser, const int sound);

// MaxW: 2016/07/14:
// Used by HealThing line actions
bool EV_DoHealThing(Mobj *actor, int amount, int max);

// killough 5/2/98: moved from d_deh.c, g_game.c, m_misc.c, others:

extern int god_health_override; // Ty 03/09/98 - deh support, see also p_inter.c
// Ty 03/13/98 - externalized initial settings for respawned player
extern int bfgcells;

// haleyjd 08/01/04: special inflictor types
enum inflictor_type_e
{
    INFLICTOR_NONE,
    INFLICTOR_MINOTAUR,     // minotaur charge
    INFLICTOR_WHIRLWIND,    // whirlwinds
    INFLICTOR_MACEBALL,     // powered mace ball
    INFLICTOR_PHOENIXFIRE,  // powered Phoenix fire
    INFLICTOR_BOSSTELEPORT, // determine bosses like D'Sparil to teleport
    INFLICTOR_NUMTYPES
};

#endif

//----------------------------------------------------------------------------
//
// $Log: p_inter.h,v $
// Revision 1.3  1998/05/03  23:08:57  killough
// beautification, add of the DEH parameter declarations
//
// Revision 1.2  1998/01/26  19:27:19  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
