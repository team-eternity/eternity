//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Max Waine, et al.
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
// Purpose: Dynamic weapons system.
// Authors: James Haley, Max Waine
//

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"
#include "i_system.h"

#include "Confuse/confuse.h"
#include "d_mod.h"
#include "d_player.h"
#include "e_dstate.h"
#include "e_edf.h"
#include "e_hash.h"
#include "e_inventory.h"
#include "e_lib.h"
#include "e_metastate.h"
#include "e_mod.h"
#include "e_player.h"
#include "e_sound.h"
#include "e_states.h"
#include "e_things.h" // TODO: Move E_SplitTypeAndState and remove this include?
#include "e_weapons.h"
#include "metaapi.h"

#include "d_dehtbl.h"
#include "d_items.h"

static weaponinfo_t **weaponinfo     = nullptr;
int                   NUMWEAPONTYPES = 0;

// track generations
static int edf_weapon_generation = 1;

// The "Unknown" weapon info, which is required, has its type
// number resolved in E_CollectWeaponinfo
weapontype_t UnknownWeaponInfo;

// clang-format off

// Weapon Keywords
// TODO: Reorder
constexpr const char ITEM_WPN_DEHNUM[]          = "dehackednum";

constexpr const char ITEM_WPN_AMMO[]            = "ammotype";
constexpr const char ITEM_WPN_UPSTATE[]         = "upstate";
constexpr const char ITEM_WPN_DOWNSTATE[]       = "downstate";
constexpr const char ITEM_WPN_READYSTATE[]      = "readystate";
constexpr const char ITEM_WPN_ATKSTATE[]        = "attackstate";
constexpr const char ITEM_WPN_FLASHSTATE[]      = "flashstate";
constexpr const char ITEM_WPN_HOLDSTATE[]       = "holdstate";
constexpr const char ITEM_WPN_AMMOPERSHOT[]     = "ammouse";

constexpr const char ITEM_WPN_AMMO_ALT[]        = "ammotype2";
constexpr const char ITEM_WPN_ATKSTATE_ALT[]    = "attackstate2";
constexpr const char ITEM_WPN_FLASHSTATE_ALT[]  = "flashstate2";
constexpr const char ITEM_WPN_HOLDSTATE_ALT[]   = "holdstate2";
constexpr const char ITEM_WPN_AMMOPERSHOT_ALT[] = "ammouse2";

constexpr const char ITEM_WPN_SELECTORDER[]     = "selectionorder";
constexpr const char ITEM_WPN_SISTERWEAPON[]    = "sisterweapon";

constexpr const char ITEM_WPN_SLOTNUM[]         = "slotnumber";
constexpr const char ITEM_WPN_SLOTRANK[]        = "slotselectionorder";

constexpr const char ITEM_WPN_FLAGS[]           = "flags";
constexpr const char ITEM_WPN_ADDFLAGS[]        = "addflags";
constexpr const char ITEM_WPN_REMFLAGS[]        = "remflags";
constexpr const char ITEM_WPN_MOD[]             = "mod";
constexpr const char ITEM_WPN_RECOIL[]          = "recoil";
constexpr const char ITEM_WPN_HAPTICRECOIL[]    = "hapticrecoil";
constexpr const char ITEM_WPN_HAPTICTIME[]      = "haptictime";
constexpr const char ITEM_WPN_UPSOUND[]         = "upsound";
constexpr const char ITEM_WPN_READYSOUND[]      = "readysound";

constexpr const char ITEM_WPN_FSOFFSET[]        = "fullscreenoffset";

constexpr const char ITEM_WPN_FIRSTDECSTATE[]   = "firstdecoratestate";

// DECORATE state block
constexpr const char ITEM_WPN_STATES[]          = "states";

constexpr const char ITEM_WPN_INHERITS[]        = "inherits";

// WeaponInfo Delta Keywords
constexpr const char ITEM_DELTA_NAME[] = "name";

// Title properties
constexpr const char ITEM_WPN_TITLE_SUPER[]  = "superclass";
constexpr const char ITEM_WPN_TITLE_DEHNUM[] = "dehackednum";

cfg_opt_t wpninfo_tprops[] = {
    CFG_STR(ITEM_WPN_TITLE_SUPER,      nullptr, CFGF_NONE),
    CFG_INT(ITEM_WPN_TITLE_DEHNUM,    -1,       CFGF_NONE),
    CFG_END()
};

#define WEAPONINFO_FIELDS \
    CFG_INT(ITEM_WPN_DEHNUM,          -1,       CFGF_NONE), \
    CFG_STR(ITEM_WPN_AMMO,            "",       CFGF_NONE), \
    CFG_STR(ITEM_WPN_UPSTATE,         "S_NULL", CFGF_NONE), \
    CFG_STR(ITEM_WPN_DOWNSTATE,       "S_NULL", CFGF_NONE), \
    CFG_STR(ITEM_WPN_READYSTATE,      "S_NULL", CFGF_NONE), \
    CFG_STR(ITEM_WPN_ATKSTATE,        "S_NULL", CFGF_NONE), \
    CFG_STR(ITEM_WPN_FLASHSTATE,      "S_NULL", CFGF_NONE), \
    CFG_STR(ITEM_WPN_HOLDSTATE,       "S_NULL", CFGF_NONE), \
    CFG_INT(ITEM_WPN_AMMOPERSHOT,     0,        CFGF_NONE), \
    CFG_STR(ITEM_WPN_AMMO_ALT,        "",       CFGF_NONE), \
    CFG_STR(ITEM_WPN_ATKSTATE_ALT,    "S_NULL", CFGF_NONE), \
    CFG_STR(ITEM_WPN_FLASHSTATE_ALT,  "S_NULL", CFGF_NONE), \
    CFG_STR(ITEM_WPN_HOLDSTATE_ALT,   "S_NULL", CFGF_NONE), \
    CFG_INT(ITEM_WPN_AMMOPERSHOT_ALT,  0,       CFGF_NONE), \
    CFG_FLOAT(ITEM_WPN_SELECTORDER,   -1,       CFGF_NONE), \
    CFG_INT(ITEM_WPN_SLOTNUM,         -1,       CFGF_NONE), \
    CFG_FLOAT(ITEM_WPN_SLOTRANK,      -1.0,     CFGF_NONE), \
    CFG_STR(ITEM_WPN_SISTERWEAPON,    "",       CFGF_NONE), \
    CFG_STR(ITEM_WPN_FLAGS,           "",       CFGF_NONE), \
    CFG_STR(ITEM_WPN_ADDFLAGS,        "",       CFGF_NONE), \
    CFG_STR(ITEM_WPN_REMFLAGS,        "",       CFGF_NONE), \
    CFG_STR(ITEM_WPN_MOD,             "",       CFGF_NONE), \
    CFG_FLOAT(ITEM_WPN_RECOIL,        0.0,      CFGF_NONE), \
    CFG_INT(ITEM_WPN_HAPTICRECOIL,    1,        CFGF_NONE), \
    CFG_INT(ITEM_WPN_HAPTICTIME,      1,        CFGF_NONE), \
    CFG_STR(ITEM_WPN_UPSOUND,         "none",   CFGF_NONE), \
    CFG_STR(ITEM_WPN_READYSOUND,      "none",   CFGF_NONE), \
    CFG_FLOAT(ITEM_WPN_FSOFFSET,      0.0,      CFGF_NONE), \
    CFG_STR(ITEM_WPN_FIRSTDECSTATE,   nullptr,  CFGF_NONE), \
    CFG_STR(ITEM_WPN_STATES,          nullptr,  CFGF_NONE), \
    CFG_END()

cfg_opt_t edf_wpninfo_opts[] = {
    CFG_TPROPS(wpninfo_tprops,           CFGF_NOCASE),
    CFG_STR(ITEM_WPN_INHERITS,  nullptr, CFGF_NONE  ),
    WEAPONINFO_FIELDS
};

cfg_opt_t edf_wdelta_opts[] = {
    CFG_STR(ITEM_DELTA_NAME, nullptr, CFGF_NONE),
    WEAPONINFO_FIELDS
};

// clang-format on

//=============================================================================
//
// Globals
//

//
// Weapon Slots
//
// There are up to 16 possible slots for weapons to stack in, but any number
// of weapons can stack in each slot. The correlation to the weapon action
// binding used to cycle through weapons in that slot is direct. The order of
// weapons in the slot is determined by their relative priorities.
//
WeaponSlotTree *weaponslots[NUMWEAPONSLOTS];

//
// The global select order tree, used to prioritise weapons and
// is the way of objectively figuring out which weapon is "best".
//
static SelectOrderTree *selectordertree = nullptr;

//=============================================================================
//
// Weapon Flags
//

static dehflags_t e_weaponFlags[] = {
    { "HAPTICRECOIL",   0                  },
    { "NOTHRUST",       WPF_NOTHRUST       },
    { "NOHITGHOSTS",    WPF_NOHITGHOSTS    },
    { "NOTSHAREWARE",   WPF_NOTSHAREWARE   },
    { "DISABLEAPS",     WPF_DISABLEAPS     },
    { "SILENCEABLE",    WPF_SILENCEABLE    },
    { "SILENT",         WPF_SILENT         },
    { "NOAUTOFIRE",     WPF_NOAUTOFIRE     },
    { "FLEEMELEE",      WPF_FLEEMELEE      },
    { "ALWAYSRECOIL",   WPF_ALWAYSRECOIL   },
    { "NOHAPTICRECOIL", WPF_NOHAPTICRECOIL },
    { "READYSNDHALF",   WPF_READYSNDHALF   },
    { "AUTOSWITCHFROM", WPF_AUTOSWITCHFROM },
    { "POWEREDUP",      WPF_POWEREDUP      },
    { "FORCETOREADY",   WPF_FORCETOREADY   },
    { "PHOENIXRESET",   WPF_PHOENIXRESET   },
    { "DEPOWERSWITCH",  WPF_DEPOWERSWITCH  },
    { "NOAUTOSWITCHTO", WPF_NOAUTOSWITCHTO },
    { nullptr,          0                  }
};

static dehflagset_t e_weaponFlagSet = {
    e_weaponFlags, // flags
    0              // mode
};

//=============================================================================
//
// Weapon Hash Tables
//

static EHashTable<weaponinfo_t, EIntHashKey, &weaponinfo_t::id, &weaponinfo_t::idlinks> e_WeaponIDHash;

static EHashTable<weaponinfo_t, ENCStringHashKey, &weaponinfo_t::name, &weaponinfo_t::namelinks> e_WeaponNameHash;

static EHashTable<weaponinfo_t, EIntHashKey, &weaponinfo_t::dehnum, &weaponinfo_t::dehlinks> e_WeaponDehHash;

//
// Obtain a weaponinfo_t structure for its ID number.
//
weaponinfo_t *E_WeaponForID(const int id)
{
    return e_WeaponIDHash.objectForKey(id);
}

//
// Obtain a weaponinfo_t structure by name.
//
weaponinfo_t *E_WeaponForName(const char *name)
{
    return e_WeaponNameHash.objectForKey(name);
}

//
// Obtain a weaponinfo_t structure by name.
//
weaponinfo_t *E_WeaponForDEHNum(const int dehnum)
{
    return e_WeaponDehHash.objectForKey(dehnum);
}

//
// Returns a weapon type index given its name. Returns -1
// if a weapon type is not found.
//
weapontype_t E_WeaponNumForName(const char *name)
{
    weaponinfo_t *info = nullptr;
    weapontype_t  ret  = -1;

    if((info = e_WeaponNameHash.objectForKey(name)))
        ret = info->id;

    return ret;
}

//
// As above, but causes a fatal error if the weapon type isn't found.
//
static weapontype_t E_getWeaponNumForName(const char *name)
{
    weapontype_t weaponnum = E_WeaponNumForName(name);

    if(weaponnum == -1)
        I_Error("E_GetWeaponNumForName: bad weapon type %s\n", name);

    return weaponnum;
}

//
// Check if the weaponsec in the slotnum is currently equipped
// DON'T CALL THIS IN NEW CODE, IT EXISTS SOLELY FOR COMPAT.
//
bool E_WeaponIsCurrentDEHNum(const player_t *player, const int dehnum)
{
    const weaponinfo_t *weapon = E_WeaponForDEHNum(dehnum);
    return weapon ? player->readyweapon->id == weapon->id : false;
}

//
// Convenience function to check if a player owns a weapon
//
bool E_PlayerOwnsWeapon(const player_t &player, const weaponinfo_t *weapon)
{
    return weapon ? E_GetItemOwnedAmount(player, weapon->tracker) : false;
}

//
// Convenience function to check if a player owns the weapon specific by dehnum
//
bool E_PlayerOwnsWeaponForDEHNum(const player_t &player, const int dehnum)
{
    return E_PlayerOwnsWeapon(player, E_WeaponForDEHNum(dehnum));
}

//
// Checks if a player owns a weapon in the provided slot, useful for things like the
// Doom weapon-number widget
//
bool E_PlayerOwnsWeaponInSlot(const player_t &player, const int slot)
{
    if(!player.pclass->weaponslots[slot])
        return false;

    BDListItem<weaponslot_t> *weaponslot = E_FirstInSlot(player.pclass->weaponslots[slot]);
    do
    {
        if(E_PlayerOwnsWeapon(player, weaponslot->bdObject->weapon))
            return true;
        weaponslot = weaponslot->bdNext;
    }
    while(!weaponslot->isDummy());
    return false;
}

//
// As above, but returning the number owned in the slot
//
int E_NumWeaponsInSlotPlayerOwns(const player_t &player, const int slot)
{
    if(!player.pclass->weaponslots[slot])
        return 0;

    int                       ret        = 0;
    BDListItem<weaponslot_t> *weaponslot = E_FirstInSlot(player.pclass->weaponslots[slot]);
    do
    {
        if(E_PlayerOwnsWeapon(player, weaponslot->bdObject->weapon))
            ret++;
        weaponslot = weaponslot->bdNext;
    }
    while(!weaponslot->isDummy());

    return ret;
}

//
// If it doesn't have an alt atkstate, it can't have an alt fire
//
bool E_WeaponHasAltFire(const weaponinfo_t *wp)
{
    return wp->atkstate_alt != E_SafeState(S_NULL);
}

//
// Give a player a weapon if they don't own it
//
void E_GiveWeapon(player_t &player, const weaponinfo_t *weapon)
{
    if(!E_PlayerOwnsWeapon(player, weapon))
        E_GiveInventoryItem(player, weapon->tracker);
}

//
// Give the player all class weapons
//
void E_GiveAllClassWeapons(player_t *player)
{
    for(weaponslot_t *&slot : player->pclass->weaponslots)
    {
        if(!slot)
            continue;

        BDListItem<weaponslot_t> *weaponslot = E_FirstInSlot(slot);
        do
        {
            E_GiveWeapon(*player, weaponslot->bdObject->weapon);
            weaponslot = weaponslot->bdNext;
        }
        while(!weaponslot->isDummy());
    }
}

//
// Returns whether or not a weapon is the powered (tomed) version of another weapon
//
bool E_IsPoweredVariant(const weaponinfo_t *wp)
{
    // wp->sisterWeapon is guaranteed to be != nullptr elsewhere
    return wp && wp->flags & WPF_POWEREDUP;
}

//
// Returns whether or not a weapon is the powered (tomed) version of a specific weapon
//
bool E_IsPoweredVariantOf(const weaponinfo_t *untomed, const weaponinfo_t *tomed)
{
    return untomed && untomed->sisterWeapon && untomed->sisterWeapon == tomed && tomed->flags & WPF_POWEREDUP;
}

//
// Gets the first weapon in the given slot
//
BDListItem<weaponslot_t> *E_FirstInSlot(const weaponslot_t *dummyslot)
{
    // This should NEVER happen
    if(dummyslot->links.bdNext->isDummy())
        I_Error("E_FirstInSlot: No weapon is first in slot (report to Altazimuth)\n");

    return dummyslot->links.bdNext;
}

//
// Gets the last weapon in the slot
//
BDListItem<weaponslot_t> *E_LastInSlot(const weaponslot_t *dummyslot)
{
    // This should NEVER happen
    if(dummyslot->links.bdPrev->isDummy())
        I_Error("E_LastInSlot: No weapon is first in slot (report to Altazimuth)\n");

    return dummyslot->links.bdPrev;
}

//
// Looks for the given weapon  in an indexed slot
//
static weaponslot_t *E_findEntryForWeaponInSlot(const weaponinfo_t *wp, const weaponslot_t *slot)
{
    if(slot == nullptr)
        return nullptr;

    auto baseslot = E_FirstInSlot(slot);

    // Try finding the player's currently-equipped weapon.
    while(!baseslot->isDummy())
    {
        if(baseslot->bdObject->weapon->id == wp->id)
        {
            return baseslot->bdObject;
        }
        else
            baseslot = baseslot->bdNext;
    }
    return nullptr;
}

weaponslot_t *E_FindEntryForWeaponInSlotIndex(const player_t &player, const weaponinfo_t *wp, const int slotindex)
{
    return E_findEntryForWeaponInSlot(wp, player.pclass->weaponslots[slotindex]);
}
//
// Finds the first instance of a weapon in a player's weaponslots
//
weaponslot_t *E_FindFirstWeaponSlot(const player_t &player, const weaponinfo_t *wp)
{
    for(const weaponslot_t *slot : player.pclass->weaponslots)
    {
        weaponslot_t *ret;
        if((ret = E_findEntryForWeaponInSlot(wp, slot)) != nullptr)
            return ret;
    }
    return nullptr;
}

//=============================================================================
//
// Weapon Selection Order Functions
//

//
// Perform an augmented in-order traversal of the select order tree
// to try and find the best weapon the player can fire.
//
static weaponinfo_t *E_findBestWeapon(const player_t &player, const SelectOrderNode *node)
{
    weaponinfo_t *ret = nullptr;
    if(node == nullptr)
        return nullptr; // This *really* shouldn't happen

    if(node->left && (ret = E_findBestWeapon(player, node->left)))
        return ret;
    if(E_PlayerOwnsWeapon(player, node->object) && P_WeaponHasAmmo(player, node->object))
        return node->object;
    if(node->next && (ret = E_findBestWeapon(player, node->next)))
        return ret;
    if(node->right && (ret = E_findBestWeapon(player, node->right)))
        return ret;

    // The player doesn't have a weapon that they've ammo for in this sub-tree
    return nullptr;
}

//
// Initial function to call the function that recursively finds the
// best weapon the player owns that they have the ammo to fire
//
weaponinfo_t *E_FindBestWeapon(const player_t &player)
{
    return E_findBestWeapon(player, selectordertree->root);
}

//
// Perform an augmented in-order traversal of the select order tree
// to try and find the best weapon the player can fire.
//
static weaponinfo_t *E_findBestWeaponUsingAmmo(const player_t &player, const itemeffect_t *ammo,
                                               const SelectOrderNode *node)
{
    bool          correctammo;
    weaponinfo_t *ret, *temp;
    if(node == nullptr)
        return nullptr; // This *really* shouldn't happen
    temp = node->object;

    weaponinfo_t *powerChecked;
    powerChecked = &E_TryPowered(player, *temp);

    // Analyze powered weapon's ammo type
    if(powerChecked->ammo && ammo)
        correctammo = !strcasecmp(powerChecked->ammo->getKey(), ammo->getKey());
    else
        correctammo = powerChecked->ammo == nullptr && ammo == nullptr;

    if(node->left && (ret = E_findBestWeaponUsingAmmo(player, ammo, node->left)))
        return ret;
    // Player owns normal weapon always, but check if the powered one has the flag
    if(E_PlayerOwnsWeapon(player, temp) && !(powerChecked->flags & WPF_NOAUTOSWITCHTO) && correctammo &&
       P_WeaponHasAmmo(player, powerChecked))
        return temp;
    if(node->next && (ret = E_findBestWeaponUsingAmmo(player, ammo, node->next)))
        return ret;
    if(node->right && (ret = E_findBestWeaponUsingAmmo(player, ammo, node->right)))
        return ret;

    // The player doesn't have a weapon that they've ammo for in this sub-tree
    return nullptr;
}

//
// Initial function to call the function that recursively finds the
// best weapon the player owns that has the provided primary ammo.
// If the best weapon found isn't better than the player's readyweapon
// then nullptr is returned.
//
weaponinfo_t *E_FindBestBetterWeaponUsingAmmo(const player_t &player, const itemeffect_t *ammo)
{
    weaponinfo_t *wp = E_findBestWeaponUsingAmmo(player, ammo, selectordertree->root);
    if(wp != nullptr && wp->sortorder < player.readyweapon->sortorder)
        return wp;
    else
        return nullptr;
}

//
// Gets a pointer to the counter at a given index for a given player's readyweapon
//
int *E_GetIndexedWepCtrForPlayer(const player_t *player, int index)
{
    return WeaponCounterTree::getIndexedCounterForPlayer(player, index);
}

//=============================================================================
//
// Weapon Processing
//

// TODO: I dunno, something about the below comment
// Warning: likely to get lost as there's no module for metastates currently!
// IMPLEMENT_RTTI_TYPE(MetaState)

//
// Adds a state to the mobjinfo metatable.
//
static void E_AddMetaState(const weaponinfo_t *wi, state_t *state, const char *name)
{
    wi->meta->addObject(new MetaState(name, state));
}

static const char *nativeWepStateLabels[] = {
    "Select",   // upstate
    "Deselect", // downstate
    "Ready",    // readystate
    "Fire",     // atkstate
    "Flash",    // flashstate
    "Hold",     // holdstate
    "AltFire",  // atkstate_alt
    "AltFlash", // flashstate_alt
    "AltHold",  // holdstate_alt
    "Reload",   // reloadstate
    "Zoom",     // zoomstate
    "User1",    // userstate_1
    "User2",    // userstate_2
    "User3",    // userstate_3
    "User4",    // userstate_4
};

enum wepstatetypes_e
{
    WSTATE_SELECT,
    WSTATE_DESELECT,
    WSTATE_READY,
    WSTATE_FIRE,
    WSTATE_FLASH,
    WSTATE_HOLD,
    WSTATE_FIRE_ALT,
    WSTATE_FLASH_ALT,
    WSTATE_HOLD_ALT,
    WSTATE_RELOAD,
    WSTATE_ZOOM,
    WSTATE_USER1,
    WSTATE_USER2,
    WSTATE_USER3,
    WSTATE_USER4,
};

constexpr size_t NUMNATIVEWSTATES = earrlen(nativeWepStateLabels);

//
// Gets a state that is stored inside an weaponinfo metatable.
// Returns null if no such object exists.
//
static MetaState *E_GetMetaState(const weaponinfo_t *wi, const char *name)
{
    return wi->meta->getObjectKeyAndTypeEx<MetaState>(name);
}

//
// Returns a pointer to an weaponinfo's native state field if the given name
// is a match for that field's corresponding DECORATE label. Returns null
// if the name is not a match for a native state field.
//
static int *E_getNativeWepStateLoc(weaponinfo_t *wi, const char *label)
{
    int  nativenum = E_StrToNumLinear(nativeWepStateLabels, NUMNATIVEWSTATES, label);
    int *ret       = nullptr;

    // clang-format off

    switch(nativenum)
    {
    case WSTATE_SELECT:    ret = &wi->upstate;        break;
    case WSTATE_DESELECT:  ret = &wi->downstate;      break;
    case WSTATE_READY:     ret = &wi->readystate;     break;
    case WSTATE_FIRE:      ret = &wi->atkstate;       break;
    case WSTATE_FLASH:     ret = &wi->flashstate;     break;
    case WSTATE_HOLD:      ret = &wi->holdstate;      break;
    case WSTATE_FIRE_ALT:  ret = &wi->atkstate_alt;   break;
    case WSTATE_FLASH_ALT: ret = &wi->flashstate_alt; break;
    case WSTATE_HOLD_ALT:  ret = &wi->holdstate_alt;  break;
    case WSTATE_RELOAD:    ret = &wi->reloadstate;    break;
    case WSTATE_ZOOM:      ret = &wi->zoomstate;      break;
    case WSTATE_USER1:     ret = &wi->userstate_1;    break;
    case WSTATE_USER2:     ret = &wi->userstate_2;    break;
    case WSTATE_USER3:     ret = &wi->userstate_3;    break;
    case WSTATE_USER4:     ret = &wi->userstate_4;    break;
    default:
        break;
    }

    // clang-format on

    return ret;
}
inline static const int *E_getNativeWepStateLoc(const weaponinfo_t *wi, const char *label)
{
    return E_getNativeWepStateLoc(const_cast<weaponinfo_t *>(wi), label);
}

//
// Retrieves any state for an weaponinfo, either native or metastate.
// Returns null if no such state can be found. Note that the null state is
// not considered a valid state.
//
state_t *E_GetStateForWeaponInfo(const weaponinfo_t *wi, const char *label)
{
    const MetaState *ms;
    state_t         *ret         = nullptr;
    const int       *nativefield = nullptr;

    // check metastates
    if((ms = E_GetMetaState(wi, label)))
        ret = ms->state;
    else if((nativefield = E_getNativeWepStateLoc(wi, label)))
    {
        // only if not S_NULL
        if(*nativefield != NullStateNum)
            ret = states[*nativefield];
    }

    return ret;
}

//
// Returns an weaponinfo_t * if the given weaponinfo inherits from the given type
// by name. Returns null otherwise. Self-identity is *not* considered
// inheritance.
//
static weaponinfo_t *E_isWeaponInfoDescendantOf(const weaponinfo_t *wi, const char *type)
{
    weaponinfo_t *curwi      = wi->parent;
    weapontype_t  targettype = E_WeaponNumForName(type);

    while(curwi)
    {
        if(curwi->id == targettype)
            break; // found it

        // walk up the inheritance tree
        curwi = curwi->parent;
    }

    return curwi;
}

//
// Deal with unresolved goto entries in the DECORATE state object.
//
static void E_processDecorateWepGotos(weaponinfo_t *wi, const edecstateout_t *dso)
{
    int i;

    for(i = 0; i < dso->numgotos; ++i)
    {
        weaponinfo_t *type = nullptr;
        state_t      *state;
        statenum_t    statenum;
        char         *statename = nullptr;

        // see if the label contains a colon, and if so, it may be an
        // access to an inherited state
        char *colon = strchr(dso->gotos[i].label, ':');

        if(colon)
        {
            char *typestr = nullptr;

            E_SplitTypeAndState(dso->gotos[i].label, &typestr, &statename);

            if(!(typestr && statename))
            {
                // error, set to null state
                *(dso->gotos[i].nextstate) = NullStateNum;
                continue;
            }

            // if "super" this means find the state in the parent type;
            // otherwise, check if the indicated type inherits from this one
            if(!strcasecmp(typestr, "super") && wi->parent)
                type = wi->parent;
            else
                type = E_isWeaponInfoDescendantOf(wi, typestr);
        }
        else
        {
            // otherwise this is a name to resolve within the scope of the local
            // weaponinfo - it may be a state acquired through inheritance, for
            // example, and is thus not defined within the weaponinfo's state block.
            type      = wi;
            statename = dso->gotos[i].label;
        }

        if(!type)
        {
            // error; set to null state and continue
            *(dso->gotos[i].nextstate) = NullStateNum;
            continue;
        }

        // find the state in the proper type
        if(!(state = E_GetStateForWeaponInfo(type, statename)))
        {
            // error; set to null state and continue
            *(dso->gotos[i].nextstate) = NullStateNum;
            continue;
        }

        statenum = state->index + dso->gotos[i].offset;

        if(statenum < 0 || statenum >= NUMSTATES)
        {
            // error; set to null state and continue
            *(dso->gotos[i].nextstate) = NullStateNum;
            continue;
        }

        // resolve the goto
        *(dso->gotos[i].nextstate) = statenum;
    }
}

//
// Add all labeled states from a DECORATE state block to the given weaponinfo.
//
static void E_processDecorateWepStates(weaponinfo_t *wi, const edecstateout_t *dso)
{
    int i;

    for(i = 0; i < dso->numstates; ++i)
    {
        int *nativefield;

        // first see if this is a native state
        if((nativefield = E_getNativeWepStateLoc(wi, dso->states[i].label)))
            *nativefield = dso->states[i].state->index;
        else
        {
            MetaState *msnode;

            // there is not a matching native field, so add the state as a
            // metastate
            if((msnode = E_GetMetaState(wi, dso->states[i].label)))
                msnode->state = dso->states[i].state;
            else
                E_AddMetaState(wi, dso->states[i].state, dso->states[i].label);
        }
    }
}

//
// Processes the DECORATE state list in a weapon
//
static void E_processDecorateWepStateList(weaponinfo_t *wi, const char *str, const char *firststate, bool recursive)
{
    edecstateout_t *dso;

    qstring owner;
    owner.Printf(strlen(wi->name) + 4, "w{%s}", wi->name);
    if(!(dso = E_ParseDecorateStates(owner.constPtr(), str, firststate)))
    {
        E_EDFLoggedWarning(2, "Warning: couldn't attach DECORATE states to weapon '%s'.\n", wi->name);
        return;
    }

    // first deal with any gotos that still need resolution
    if(dso->numgotos)
        E_processDecorateWepGotos(wi, dso);

    // add all labeled states from the block to the weaponinfo
    if(dso->numstates && !recursive)
        E_processDecorateWepStates(wi, dso);

    // free the DSO object
    E_FreeDSO(dso);
}

//
// Process DECORATE-format states
//
static void E_processDecorateWepStatesRecursive(cfg_t *weaponsec, int wnum, bool recursive)
{
    cfg_t *displaced;

    // 01/02/12: Process displaced sections recursively first.
    if((displaced = cfg_displaced(weaponsec)))
        E_processDecorateWepStatesRecursive(displaced, wnum, true);

    // haleyjd 06/22/10: Process DECORATE state block
    if(cfg_size(weaponsec, ITEM_WPN_STATES) > 0)
    {
        // 01/01/12: allow use of pre-existing reserved states; they must be
        // defined consecutively in EDF and should be flagged +decorate in order
        // for values inside them to be overridden by the DECORATE state block.
        // If this isn't being done, firststate will be null.
        const char *firststate = cfg_getstr(weaponsec, ITEM_WPN_FIRSTDECSTATE);
        const char *tempstr    = cfg_getstr(weaponsec, ITEM_WPN_STATES);

        // recursion should process states only if firststate is valid
        if(!recursive || firststate)
            E_processDecorateWepStateList(weaponinfo[wnum], tempstr, firststate, recursive);
    }
}

//
// Function to reallocate the weaponinfo array safely.
//
static void E_reallocWeapons(unsigned int numnewweapons)
{
    static int numweaponsalloc = 0;

    // only realloc when needed
    if(!numweaponsalloc || (static_cast<unsigned>(NUMWEAPONTYPES) < numweaponsalloc + numnewweapons))
    {
        int i;

        // First time, just allocate the requested number of weaponinfo.
        // Afterward:
        // * If the number of weaponinfo requested is small, add 2 times as many
        //   requested, plus a small constant amount.
        // * If the number is large, just add that number.

        if(!numweaponsalloc)
            numweaponsalloc = numnewweapons;
        else if(numnewweapons <= 50)
            numweaponsalloc += numnewweapons * 2 + 32;
        else
            numweaponsalloc += numnewweapons;

        // reallocate weaponinfo[]
        weaponinfo = erealloc(weaponinfo_t **, weaponinfo, numweaponsalloc * sizeof(weaponinfo_t *));

        for(i = NUMWEAPONTYPES; i < numweaponsalloc; i++)
            weaponinfo[i] = nullptr;
    }

    NUMWEAPONTYPES += numnewweapons;
}

//
// Pre-creates and hashes by name the weaponinfo, for purpose
// of mutual and forward references.
//
void E_CollectWeapons(cfg_t *cfg)
{
    weapontype_t  i;
    unsigned int  numweapons;       // number of weaponinfo defined by the cfg
    unsigned int  curnewweapon = 0; // index of current new weaponinfo being used
    weaponinfo_t *newWeapon    = nullptr;
    static bool   firsttime    = true;

    // get number of weaponinfo defined by the cfg
    numweapons = cfg_size(cfg, EDF_SEC_WEAPONINFO);

    // echo counts
    E_EDFLogPrintf("\t\t%u weaponinfo defined\n", numweapons);

    if(numweapons)
    {
        unsigned int firstnewweapon = 0; // index of first new weaponinfo
        // allocate weaponinfo_t structures for the new weapons
        newWeapon = estructalloc(weaponinfo_t, numweapons);

        // add space to the weaponinfo array
        curnewweapon = firstnewweapon = NUMWEAPONTYPES;

        E_reallocWeapons(numweapons);

        // set pointers in weaponinfo[] to the proper structures;
        // also set self-referential index member, and allocate a
        // metatable.
        for(i = firstnewweapon; i < weapontype_t(NUMWEAPONTYPES); i++)
        {
            weaponinfo[i]       = &newWeapon[i - firstnewweapon];
            weaponinfo[i]->id   = i;
            weaponinfo[i]->meta = new MetaTable("weaponinfo");
        }
    }

    // build hash tables
    E_EDFLogPuts("\t\tBuilding weapon hash tables\n");

    // cycle through the weaponinfo defined in the cfg
    for(i = 0; static_cast<unsigned>(i) < numweapons; i++)
    {
        cfg_t      *weaponcfg = cfg_getnsec(cfg, EDF_SEC_WEAPONINFO, i);
        const char *name      = cfg_title(weaponcfg);
        int         dehnum    = -1;

        // This is a new weaponinfo, whether or not one already exists by this name
        // in the hash table. For subsequent addition of EDF weaponinfo at runtime,
        // the hash table semantics of "find newest first" take care of overriding,
        // while not breaking objects that depend on the original definition of
        // the weaponinfo for inheritance purposes.
        weaponinfo_t *wi = weaponinfo[curnewweapon++];

        // initialize name
        wi->name = estrdup(name);

        // add to name & ID hash
        e_WeaponIDHash.addObject(wi);
        e_WeaponNameHash.addObject(wi);

        // check for titleprops definition first
        if(cfg_size(weaponcfg, "#title") > 0)
        {
            cfg_t *titleprops = cfg_gettitleprops(weaponcfg);
            if(titleprops)
                dehnum = cfg_getint(titleprops, ITEM_WPN_TITLE_DEHNUM);
        }

        // If undefined, check the legacy value inside the section
        if(dehnum == -1)
            dehnum = cfg_getint(weaponcfg, ITEM_WPN_DEHNUM);

        // process dehackednum and add thing to dehacked hash table,
        // if appropriate
        if((wi->dehnum = dehnum) >= 0)
            e_WeaponDehHash.addObject(wi);

        // set generation
        wi->generation = edf_weapon_generation;
    }

    // first-time-only events
    if(firsttime)
    {
        // check that at least one weaponinfo was defined
        if(!NUMWEAPONTYPES)
            E_EDFLoggedErr(2, "E_CollectWeapons: no weaponinfo defined.\n");

        // Verify existence of the Unknown weapon type
        UnknownWeaponInfo = E_WeaponNumForName("Unknown");
        if(UnknownWeaponInfo < 0)
            E_EDFLoggedErr(2, "E_CollectWeapons: 'Unknown' weaponinfo must be defined.\n");

        firsttime = false;
    }
}

// weapon_hitlist: keeps track of what weaponinfo are initialized
static byte *weapon_hitlist = nullptr;

// weapon_pstack: used by recursive E_ProcessWeapon to track inheritance
static int *weapon_pstack = nullptr;
static int  weapon_pindex = 0;

//
// Makes sure the weapon type being inherited from has not already
// been inherited during the current inheritance chain. Returns
// false if the check fails, and true if it succeeds.
//
static bool E_checkWeaponInherit(int pnum)
{
    for(int i = 0; i < NUMWEAPONTYPES; i++)
    {
        // circular inheritance
        if(weapon_pstack[i] == pnum)
            return false;

        // found end of list
        if(weapon_pstack[i] == -1)
            break;
    }

    return true;
}

//
// Adds a type number to the inheritance stack.
//
static void E_addWeaponToPStack(weapontype_t num)
{
    // Overflow shouldn't happen since it would require cyclic
    // inheritance as well, but I'll guard against it anyways.

    if(weapon_pindex >= NUMWEAPONTYPES)
        E_EDFLoggedErr(2, "E_AddWeaponToPStack: max inheritance depth exceeded\n");

    weapon_pstack[weapon_pindex++] = num;
}

//
// Resets the weaponinfo inheritance stack, setting all the pstack
// values to -1, and setting pindex back to zero.
//
static void E_resetWeaponPStack()
{
    for(int i = 0; i < NUMWEAPONTYPES; i++)
        weapon_pstack[i] = -1;

    weapon_pindex = 0;
}

//
// Copies one weaponinfo into another.
//
static void E_copyWeapon(weapontype_t num, weapontype_t pnum)
{
    weaponinfo_t            *this_wi;
    DLListItem<weaponinfo_t> idlinks, namelinks, dehlinks;
    const char              *name;
    int                      dehnum;
    MetaTable               *meta;
    weapontype_t             id;
    int                      generation;
    itemeffect_t            *tracker;

    this_wi = weaponinfo[num];

    // must save the following fields in the destination weapon:
    idlinks    = this_wi->idlinks;
    namelinks  = this_wi->namelinks;
    dehlinks   = this_wi->dehlinks;
    name       = this_wi->name;
    dehnum     = this_wi->dehnum;
    meta       = this_wi->meta;
    id         = this_wi->id;
    generation = this_wi->generation;
    tracker    = this_wi->tracker;

    // copy from source to destination
    memcpy(this_wi, weaponinfo[pnum], sizeof(weaponinfo_t));

    // normalize special fields?
    // FIXME: IDK

    // copy metatable
    meta->copyTableFrom(weaponinfo[pnum]->meta);

    // restore metatable pointer
    this_wi->meta = meta;

    // must restore name and dehacked num data
    this_wi->dehlinks   = dehlinks;
    this_wi->namelinks  = namelinks;
    this_wi->idlinks    = idlinks;
    this_wi->name       = name;
    this_wi->dehnum     = dehnum;
    this_wi->id         = id;
    this_wi->generation = generation;

    this_wi->upsound    = estrdup(weaponinfo[pnum]->upsound);
    this_wi->readysound = estrdup(weaponinfo[pnum]->readysound);

    // tracker inheritance is weird
    // if(!(this_wi->flags & WPF_[TomedVersionOfWeapon]))
    this_wi->tracker = tracker;

    // MaxW: Inheriting weapons have no use for this flag, and I don't want
    // users to have to unset it when it shouldn't be set in the first place.
    this_wi->flags &= ~WPF_DISABLEAPS;
}

struct weapontitleprops_t
{
    const char *superclass;
    int         dehackednum;
};

static void E_getWeaponTitleProps(cfg_t *weaponsec, weapontitleprops_t &props, bool def)
{
    cfg_t *titleprops;

    if(def && cfg_size(weaponsec, "#title") > 0 && (titleprops = cfg_gettitleprops(weaponsec)))
    {
        props.superclass  = cfg_getstr(titleprops, ITEM_WPN_TITLE_SUPER);
        props.dehackednum = cfg_getint(titleprops, ITEM_WPN_TITLE_DEHNUM);
    }
    else
    {
        props.superclass  = nullptr;
        props.dehackednum = -1;
    }
}

//
// Get the weaponinfo index for the weapon's superclass weaponinfo.
//
static weapontype_t E_resolveParentWeapon(cfg_t *weaponsec, const weapontitleprops_t &props)
{
    weapontype_t pnum = -1;

    // check title props first
    if(props.superclass)
        pnum = E_getWeaponNumForName(props.superclass);
    else // resolve parent weaponinfo through legacy "inherits" field
        pnum = E_getWeaponNumForName(cfg_getstr(weaponsec, ITEM_WPN_INHERITS));

    return pnum;
}

static void E_insertSelectOrderNode(fixed_t sortorder, weaponinfo_t *wp, bool modify)
{
    if(modify && (wp->intflags & WIF_HASSORTORDER))
        selectordertree->deleteNode(wp->sortorder, wp);

    if(selectordertree == nullptr)
        selectordertree = new SelectOrderTree(sortorder, wp);
    else
        selectordertree->insert(sortorder, wp);

    wp->intflags |= WIF_HASSORTORDER;
}

static void E_insertWeaponSlotNode(int slotindex, fixed_t slotrank, weaponinfo_t *wp, bool modify)
{
    if(modify && (wp->intflags & WIF_INGLOBALSLOT))
        weaponslots[wp->defaultslotindex]->deleteNode(wp->defaultslotrank, wp);

    if(weaponslots[slotindex] == nullptr)
        weaponslots[slotindex] = new WeaponSlotTree(slotrank, wp);
    else
        weaponslots[slotindex]->insert(slotrank, wp);

    wp->intflags |= WIF_INGLOBALSLOT;
}

static void E_updateVanillaStatesByDehNum(weaponinfo_t &wp)
{
    // If a given vanilla weapon state was "replaced" by a frame that uses
    // the same dehackednum but not the same name then update the weapon
    // state to be that of the final state created with that dehackednum
    auto TryUpdateStateByDehNum = [](int &state) -> void {
        if(state >= 0)
        {
            if(const state_t *const currState = states[state]; currState->dehnum >= 0)
            {
                if(const int statenum = E_StateNumForDEHNum(currState->dehnum); statenum >= 0)
                    state = statenum;
            }
        }
    };

    TryUpdateStateByDehNum(wp.readystate);
    TryUpdateStateByDehNum(wp.upstate);
    TryUpdateStateByDehNum(wp.downstate);
    TryUpdateStateByDehNum(wp.readystate);
    TryUpdateStateByDehNum(wp.atkstate);
    TryUpdateStateByDehNum(wp.flashstate);
}

//
// Process a single weaponinfo, or weapondelta
//
static void E_processWeapon(weapontype_t i, cfg_t *const weaponsec, cfg_t *pcfg, const bool def)
{
    int                tempint;
    const char        *tempstr;
    double             tempfloat;
    bool               inherits = false;
    weapontitleprops_t titleprops;
    weaponinfo_t      &wp = *weaponinfo[i];

    const auto IS_SET = [weaponsec, def, &inherits](const char *const name) -> bool {
        return (def && !inherits) || cfg_size(weaponsec, (name)) > 0;
    };

    // if weaponsec is null, we are in the situation of inheriting from a weapon
    // that was processed in a previous EDF generation, so no processing is
    // required; return immediately.
    if(!weaponsec)
        return;

    // Retrieve title properties
    E_getWeaponTitleProps(weaponsec, titleprops, def);

    // inheritance -- not in deltas
    if(def)
    {
        int pnum = -1;

        // if this weapon is already processed via recursion due to
        // inheritance, don't process it again
        if(weapon_hitlist[i])
            return;

        if(titleprops.superclass || cfg_size(weaponsec, ITEM_WPN_INHERITS) > 0)
            pnum = E_resolveParentWeapon(weaponsec, titleprops);

        if(pnum >= 0)
        {
            cfg_t       *parent_tngsec;
            weapontype_t pnum = E_resolveParentWeapon(weaponsec, titleprops); // FIXME: Is this required?

            // check against cyclic inheritance
            if(!E_checkWeaponInherit(pnum))
            {
                E_EDFLoggedErr(2,
                               "E_processWeapon: cyclic inheritance "
                               "detected in weaponinfo '%s'\n",
                               wp.name);
            }

            // add to inheritance stack
            E_addWeaponToPStack(pnum);

            // process parent recursively
            // must use cfg_gettsec; note can return null
            parent_tngsec = cfg_gettsec(pcfg, EDF_SEC_WEAPONINFO, weaponinfo[pnum]->name);
            E_processWeapon(pnum, parent_tngsec, pcfg, true);

            // copy parent to this weapon
            E_copyWeapon(i, pnum);

            // keep track of parent explicitly
            wp.parent = weaponinfo[pnum];

            // we inherit, so treat defaults as no value
            inherits = true;
        }
    }

    if(IS_SET(ITEM_WPN_SELECTORDER))
    {
        if((tempfloat = cfg_getfloat(weaponsec, ITEM_WPN_SELECTORDER)) >= 0)
        {
            const fixed_t tempfixed = M_DoubleToFixed(tempfloat);
            E_insertSelectOrderNode(tempfixed, &wp, !def);
            wp.sortorder = tempfixed;
        }
    }
    if(cfg_size(weaponsec, ITEM_WPN_SISTERWEAPON) > 0)
    {
        tempstr = cfg_getstr(weaponsec, ITEM_WPN_SISTERWEAPON);
        if(!(wp.sisterWeapon = E_WeaponForName(tempstr)))
        {
            E_EDFLoggedErr(2,
                           "E_processWeapon: invalid sisterweapon '%s' defined "
                           "in weaponinfo '%s'\n",
                           tempstr, wp.name);
        }
    }

    if(cfg_size(weaponsec, ITEM_WPN_SLOTNUM) > 0)
    {
        tempint = cfg_getint(weaponsec, ITEM_WPN_SLOTNUM) - 1;
        if(tempint < 0 || tempint > NUMWEAPONSLOTS)
        {
            E_EDFLoggedWarning(2, "E_processWeapon: invalid slotnumber in weaponinfo '%s'\n", wp.name);
        }
        else if(cfg_size(weaponsec, ITEM_WPN_SLOTRANK) > 0)
        {
            tempfloat = cfg_getfloat(weaponsec, ITEM_WPN_SLOTRANK);
            // Make sure tempfloat is within the bounds of a fixed_t
            if(tempfloat > M_FixedToDouble(0x7FFFFFFF) || tempfloat < 0.0)
            {
                E_EDFLoggedWarning(2,
                                   "E_processWeapon: invalid slotselectionorder %f in "
                                   "weaponinfo '%s'\n",
                                   tempfloat, wp.name);
            }
            else
            {
                fixed_t tempfixed = M_DoubleToFixed(tempfloat);
                E_insertWeaponSlotNode(tempint, tempfixed, &wp, !def);
                wp.defaultslotindex = tempint;
                wp.defaultslotrank  = tempfixed;
            }
        }
        else
        {
            E_EDFLoggedWarning(2,
                               "E_processWeapon: no slotselectionorder defined for weapon with "
                               "slotnumber in weaponinfo '%s'\n",
                               wp.name);
        }
    }
    else if(cfg_size(weaponsec, ITEM_WPN_SLOTRANK) > 0)
    {
        E_EDFLoggedWarning(2,
                           "E_processWeapon: slotselectionorder defined for weapon with no "
                           "slotnumber in weaponinfo '%s'\n",
                           wp.name);
    }

    // Attack properties
    if(IS_SET(ITEM_WPN_AMMO))
    {
        tempstr = cfg_getstr(weaponsec, ITEM_WPN_AMMO);
        if((wp.ammo = E_ItemEffectForName(tempstr)) == nullptr && estrnonempty(tempstr))
        {
            E_EDFLoggedErr(2, "E_processWeapon: bad ammotype '%s' in weaponinfo '%s'\n", tempstr, wp.name);
        }
    }

    if(IS_SET(ITEM_WPN_UPSTATE))
        wp.upstate = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_UPSTATE));
    if(IS_SET(ITEM_WPN_DOWNSTATE))
        wp.downstate = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_DOWNSTATE));
    if(IS_SET(ITEM_WPN_READYSTATE))
        wp.readystate = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_READYSTATE));
    if(IS_SET(ITEM_WPN_ATKSTATE))
        wp.atkstate = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_ATKSTATE));
    if(IS_SET(ITEM_WPN_FLASHSTATE))
        wp.flashstate = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_FLASHSTATE));
    if(IS_SET(ITEM_WPN_HOLDSTATE))
        wp.holdstate = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_HOLDSTATE));

    if(IS_SET(ITEM_WPN_AMMOPERSHOT))
    {
        if(!def)
            wp.flags &= ~WPF_DISABLEAPS;
        wp.ammopershot = cfg_getint(weaponsec, ITEM_WPN_AMMOPERSHOT);
    }

    // Alt attack properties
    if(IS_SET(ITEM_WPN_AMMO_ALT))
    {
        tempstr = cfg_getstr(weaponsec, ITEM_WPN_AMMO_ALT);
        if((wp.ammo_alt = E_ItemEffectForName(tempstr)) == nullptr && estrnonempty(tempstr))
        {
            E_EDFLoggedErr(2, "E_processWeapon: bad ammotype2 '%s' in weaponinfo '%s'\n", tempstr, wp.name);
        }
    }

    if(IS_SET(ITEM_WPN_ATKSTATE_ALT))
        wp.atkstate_alt = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_ATKSTATE_ALT));
    if(IS_SET(ITEM_WPN_FLASHSTATE_ALT))
        wp.flashstate_alt = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_FLASHSTATE_ALT));
    if(IS_SET(ITEM_WPN_HOLDSTATE_ALT))
        wp.holdstate_alt = E_GetStateNumForName(cfg_getstr(weaponsec, ITEM_WPN_HOLDSTATE_ALT));

    if(IS_SET(ITEM_WPN_AMMOPERSHOT_ALT))
    {
        if(!def)
            wp.flags &= ~WPF_DISABLEAPS;
        wp.ammopershot_alt = cfg_getint(weaponsec, ITEM_WPN_AMMOPERSHOT_ALT);
    }

    if(IS_SET(ITEM_WPN_MOD))
    {
        tempstr = cfg_getstr(weaponsec, ITEM_WPN_MOD);
        wp.mod  = E_DamageTypeNumForName(tempstr);
    }
    else if(def && !inherits)
        wp.mod = MOD_UNKNOWN;

    // process combined flags first
    if(IS_SET(ITEM_WPN_FLAGS))
    {
        tempstr = cfg_getstr(weaponsec, ITEM_WPN_FLAGS);
        if(estrempty(tempstr))
            wp.flags = 0;
        else
        {
            unsigned int results = E_ParseFlags(tempstr, &e_weaponFlagSet);
            wp.flags             = results;
        }
    }

    // process addflags and remflags modifiers

    if(cfg_size(weaponsec, ITEM_WPN_ADDFLAGS) > 0)
    {
        tempstr = cfg_getstr(weaponsec, ITEM_WPN_ADDFLAGS);

        unsigned int results  = E_ParseFlags(tempstr, &e_weaponFlagSet);
        wp.flags             |= results;
    }

    if(cfg_size(weaponsec, ITEM_WPN_REMFLAGS) > 0)
    {
        tempstr = cfg_getstr(weaponsec, ITEM_WPN_REMFLAGS);

        unsigned int results  = E_ParseFlags(tempstr, &e_weaponFlagSet);
        wp.flags             &= ~results;
    }

    if(wp.flags & WPF_POWEREDUP)
    {
        if(wp.sisterWeapon == nullptr)
        {
            E_EDFLoggedErr(2,
                           "E_processWeapon: weaponinfo '%s' has flag 'POWERED_UP', "
                           "but lacks a required sisterweapon\n",
                           wp.name);
        }
        else if(wp.sisterWeapon->flags & WPF_POWEREDUP)
        {
            E_EDFLoggedErr(2,
                           "E_processWeapon: weaponinfo '%s' has flag 'POWERED_UP', "
                           "when its sisterweapon also has this flag\n",
                           wp.name);
        }

        if(IS_SET(ITEM_WPN_SELECTORDER))
        {
            E_EDFLoggedErr(2,
                           "E_processWeapon: weaponinfo '%s' has flag 'POWERED_UP' as well "
                           "as an explicit selectionorder.\nPowered weapons use the same "
                           "selectionorder as their unpowered sisterweapon\n",
                           wp.name);
        }
    }

    if(IS_SET(ITEM_WPN_RECOIL))
        wp.recoil = M_DoubleToFixed(cfg_getfloat(weaponsec, ITEM_WPN_RECOIL));
    if(IS_SET(ITEM_WPN_HAPTICRECOIL))
        wp.hapticrecoil = cfg_getint(weaponsec, ITEM_WPN_HAPTICRECOIL);
    if(IS_SET(ITEM_WPN_HAPTICTIME))
        wp.haptictime = cfg_getint(weaponsec, ITEM_WPN_HAPTICTIME);

    if(IS_SET(ITEM_WPN_UPSOUND))
    {
        tempstr            = cfg_getstr(weaponsec, ITEM_WPN_UPSOUND);
        sfxinfo_t *tempsfx = E_EDFSoundForName(tempstr);
        if(wp.upsound)
        {
            efree(const_cast<char *>(wp.upsound));
            wp.upsound = nullptr;
        }
        if(tempsfx)
            wp.upsound = estrdup(tempstr);
    }

    if(IS_SET(ITEM_WPN_READYSOUND))
    {
        tempstr            = cfg_getstr(weaponsec, ITEM_WPN_READYSOUND);
        sfxinfo_t *tempsfx = E_EDFSoundForName(tempstr);
        if(wp.readysound)
        {
            efree(const_cast<char *>(wp.readysound));
            wp.readysound = nullptr;
        }
        if(tempsfx)
            wp.readysound = estrdup(tempstr);
    }

    if(IS_SET(ITEM_WPN_FSOFFSET))
        wp.fullscreenoffset = M_DoubleToFixed(cfg_getfloat(weaponsec, ITEM_WPN_FSOFFSET));

    // Process DECORATE state block
    E_processDecorateWepStatesRecursive(weaponsec, i, false);

    E_updateVanillaStatesByDehNum(wp);
}

//
// Process all weapons!
//
void E_ProcessWeaponInfo(cfg_t *cfg)
{
    E_EDFLogPuts("\t* Processing weaponinfo data\n");

    const unsigned int numWeapons = cfg_size(cfg, EDF_SEC_WEAPONINFO);

    // allocate inheritance stack and hitlist
    weapon_hitlist = ecalloc(byte *, NUMWEAPONTYPES, sizeof(byte));
    weapon_pstack  = ecalloc(int *, NUMWEAPONTYPES, sizeof(int));

    for(weapontype_t i = 0; i < NUMWEAPONTYPES; i++)
    {
        if(weaponinfo[i]->generation != edf_weapon_generation)
            weapon_hitlist[i] = 1;
    }

    for(unsigned int i = 0; i < numWeapons; i++)
    {
        cfg_t       *weaponsec = cfg_getnsec(cfg, EDF_SEC_WEAPONINFO, i);
        const char  *name      = cfg_title(weaponsec);
        weapontype_t weaponnum = E_WeaponNumForName(name);

        // reset the inheritance stack
        E_resetWeaponPStack();

        // add this weapon to the stack
        E_addWeaponToPStack(weaponnum);

        E_processWeapon(weaponnum, weaponsec, cfg, true);

        E_EDFLogPrintf("\t\tFinished weaponinfo %s (#%d)\n", weaponinfo[weaponnum]->name, weaponnum);
    }

    // free tables
    efree(weapon_hitlist);
    efree(weapon_pstack);

    // increment generation count
    ++edf_weapon_generation;
}

//
// Powered weapons should just use the inventory-item trackers of their basic counterparts. The
// single powered-basic pairing check was already done. This must be done after the entire weapon
// processing is complete, including deltas, otherwise Eternity crashes.
//
static void E_clearPoweredWeaponTrackers()
{
    for(int i = 0; i < NUMWEAPONTYPES; ++i)
    {
        weaponinfo_t *currWeapon = E_WeaponForID(i);
        if(currWeapon->flags & WPF_POWEREDUP && currWeapon->sisterWeapon)
        {
            E_SafeDeleteItemEffect(currWeapon->tracker);
            currWeapon->tracker = currWeapon->sisterWeapon->tracker;
        }
    }
}

void E_ProcessWeaponDeltas(cfg_t *cfg)
{
    E_EDFLogPuts("\t* Processing weapondelta data\n");

    const unsigned int numDeltas = cfg_size(cfg, EDF_SEC_WPNDELTA);

    for(unsigned int i = 0; i < numDeltas; i++)
    {
        const char  *name;
        weapontype_t weaponNum;
        cfg_t       *deltasec = cfg_getnsec(cfg, EDF_SEC_WPNDELTA, i);
        // get weaponinfo to edit
        if(!cfg_size(deltasec, ITEM_DELTA_NAME))
            E_EDFLoggedErr(2, "E_ProcessWeaponDeltas: weapondelta requires name field\n");

        name      = cfg_getstr(deltasec, ITEM_DELTA_NAME);
        weaponNum = E_getWeaponNumForName(name);

        E_processWeapon(weaponNum, deltasec, cfg, false);

        E_EDFLogPrintf("\t\tApplied weapondelta #%d to %s(#%d)\n", i, weaponinfo[weaponNum]->name, weaponNum);
    }

    E_clearPoweredWeaponTrackers();
}

//
// Use powered weapon if needed
//
weaponinfo_t &E_TryPowered(const player_t &player, weaponinfo_t &weapon)
{
    weaponinfo_t *sister = weapon.sisterWeapon;
    if(player.powers[pw_weaponlevel2].isActive() && E_IsPoweredVariant(sister))
        return *sister;
    return weapon;
}

const weaponinfo_t &E_TryPowered(const player_t &player, const weaponinfo_t &weapon)
{
    return E_TryPowered(player, const_cast<weaponinfo_t &>(weapon));
}

// EOF

