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
// Purpose: Inventory.
// Authors: James Haley, Max Waine, Ioan Chera
//

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"
#include "i_system.h"

#include "Confuse/confuse.h"
#include "e_edf.h"
#include "e_hash.h"
#include "e_inventory.h"
#include "e_lib.h"
#include "e_player.h"
#include "e_sprite.h"
#include "e_string.h"

#include "autopalette.h"
#include "a_args.h"
#include "am_map.h"
#include "c_runcmd.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "doomstat.h"
#include "e_args.h"
#include "e_weapons.h"
#include "g_game.h"
#include "info.h"
#include "m_collection.h"
#include "metaapi.h"
#include "p_inter.h"
#include "p_mobj.h"
#include "p_skin.h"
#include "s_sound.h"
#include "v_video.h"

//=============================================================================
//
// Effect Classes
//

// Item Effect Type names
static const char *e_ItemEffectTypeNames[NUMITEMFX] = { "None",  "Health",      "Armor",   "Ammo",
                                                        "Power", "WeaponGiver", "Artifact" };

//
// E_EffectTypeForName
//
// Look up an effect type index based on its name. ITEMFX_NONE is returned
// if an invalid name is provided.
//
itemeffecttype_t E_EffectTypeForName(const char *name)
{
    itemeffecttype_t fx;

    if((fx = E_StrToNumLinear(e_ItemEffectTypeNames, NUMITEMFX, name)) == NUMITEMFX)
        fx = ITEMFX_NONE;

    return fx;
}

//=============================================================================
//
// Effects
//
// An effect applies when an item is collected, used, or dropped.
//

// The effects table contains as properties metatables for each effect.
static MetaTable e_effectsTable;

//
// E_addItemEffect
//
// Add an item effect from a cfg_t definition.
//
static void          E_convertKeywordEnumsToStrings(MetaTable &table);
static itemeffect_t *E_addItemEffect(cfg_t *cfg, const char *const cfgSecName, const int secNum)
{
    itemeffect_t *table;
    const char   *name = cfg_title(cfg);

    if(!(table = E_ItemEffectForName(name)))
    {
        table = new itemeffect_t(name);
        e_effectsTable.addObject(table);
    }
    else if(table->getInt(keyClass, ITEMFX_NONE) != secNum)
    {
        E_EDFLoggedWarning(2,
                           "E_addItemEffect: Multiple item effects with same name "
                           "'%s' but different types '%s' and '%s' defined\n",
                           name, cfgSecName, table->getConstString(keyClassName, ""));
    }

    E_MetaTableFromCfg(cfg, table);
    E_convertKeywordEnumsToStrings(*table);

    return table;
}

//
// E_ItemEffectForName
//
// Find an item effect by name.
//
itemeffect_t *E_ItemEffectForName(const char *name)
{
    return runtime_cast<itemeffect_t *>(e_effectsTable.getObject(name));
}

//
// E_GetItemEffects
//
// Get the whole effects table, for the few places it is needed externally
// (mainly for console debugging features).
//
MetaTable *E_GetItemEffects()
{
    return &e_effectsTable;
}

//=============================================================================
//
// Effect Processing
//

// metakey vocabulary
constexpr const char KEY_ADDITIVE[]                = "additive";
constexpr const char KEY_ADDITIVETIME[]            = "additivetime";
constexpr const char KEY_ALWAYSPICKUP[]            = "alwayspickup";
constexpr const char KEY_AMMO[]                    = "ammo";
constexpr const char KEY_AMMOGIVEN[]               = "ammogiven";
constexpr const char KEY_AMOUNT[]                  = "amount";
constexpr const char KEY_ARGS[]                    = "args";
constexpr const char KEY_ARTIFACTTYPE[]            = "artifacttype";
constexpr const char KEY_AUTOUSE_HEALTH_MODE[]     = "autouse.health.mode";
constexpr const char KEY_AUTOUSE_HEALTH_RESTRICT[] = "autouse.health.restrict";
constexpr const char KEY_BACKPACKAMOUNT[]          = "ammo.backpackamount";
constexpr const char KEY_BACKPACKMAXAMT[]          = "ammo.backpackmaxamount";
constexpr const char KEY_AMMOCOOPSTAY[]            = "ammo.coopstay";
constexpr const char KEY_AMMODMSTAY[]              = "ammo.dmstay";
constexpr const char KEY_AMMODROPPED[]             = "ammo.dropped";
constexpr const char KEY_AMMOGIVE[]                = "ammo.give";
constexpr const char KEY_CLASS[]                   = "class";
constexpr const char KEY_CLASSNAME[]               = "classname";
constexpr const char KEY_CLRAMMOGIVEN[]            = "clearammogiven";
constexpr const char KEY_DROPAMOUNT[]              = "dropamount";
constexpr const char KEY_DURATION[]                = "duration";
constexpr const char KEY_FULLAMOUNTONLY[]          = "fullamountonly";
constexpr const char KEY_ICON[]                    = "icon";
constexpr const char KEY_ICON_XOFFS[]              = "icon.offset.x";
constexpr const char KEY_ICON_YOFFS[]              = "icon.offset.y";
constexpr const char KEY_IGNORESKILL[]             = "ignoreskill";
constexpr const char KEY_INTERHUBAMOUNT[]          = "interhubamount";
constexpr const char KEY_INVBAR[]                  = "invbar";
constexpr const char KEY_ITEMID[]                  = "itemid";
constexpr const char KEY_KEEPDEPLETED[]            = "keepdepleted";
constexpr const char KEY_LOWMESSAGE[]              = "lowmessage";
constexpr const char KEY_MAXAMOUNT[]               = "maxamount";
constexpr const char KEY_MAXSAVEAMOUNT[]           = "maxsaveamount";
constexpr const char KEY_NOSHAREWARE[]             = "noshareware";
constexpr const char KEY_OVERRIDESSELF[]           = "overridesself";
constexpr const char KEY_PERMANENT[]               = "permanent";
constexpr const char KEY_SAVEAMOUNT[]              = "saveamount";
constexpr const char KEY_SAVEDIVISOR[]             = "savedivisor";
constexpr const char KEY_SAVEFACTOR[]              = "savefactor";
constexpr const char KEY_SETHEALTH[]               = "sethealth";
constexpr const char KEY_SETABSORPTION[]           = "setabsorption";
constexpr const char KEY_SORTORDER[]               = "sortorder";
constexpr const char KEY_TYPE[]                    = "type";
constexpr const char KEY_UNDROPPABLE[]             = "undroppable";
constexpr const char KEY_USEACTION[]               = "useaction";
constexpr const char KEY_USEEFFECT[]               = "useeffect";
constexpr const char KEY_USESOUND[]                = "usesound";
constexpr const char KEY_WEAPON[]                  = "weapon";

constexpr const char KEY_DELTA_NAME[] = "name";

// clang-format off

// Interned metatable keys
MetaKeyIndex keyAmount               (KEY_AMOUNT                 );
MetaKeyIndex keyBackpackAmount       (KEY_BACKPACKAMOUNT         );
MetaKeyIndex keyClass                (KEY_CLASS                  );
MetaKeyIndex keyClassName            (KEY_CLASSNAME              );
MetaKeyIndex keyItemID               (KEY_ITEMID                 );
MetaKeyIndex keyMaxAmount            (KEY_MAXAMOUNT              );
MetaKeyIndex keyBackpackMaxAmt       (KEY_BACKPACKMAXAMT         );
MetaKeyIndex keyInvBar               (KEY_INVBAR                 );
MetaKeyIndex keyAmmoGiven            (KEY_AMMOGIVEN              );
MetaKeyIndex keyAutouseHealthMode    (KEY_AUTOUSE_HEALTH_MODE    );
MetaKeyIndex keyAutouseHealthRestrict(KEY_AUTOUSE_HEALTH_RESTRICT);

// Static interened metatable keys
static MetaKeyIndex keyArtifactType  (KEY_ARTIFACTTYPE  );
static MetaKeyIndex keyFullAmountOnly(KEY_FULLAMOUNTONLY);
static MetaKeyIndex keyInterHubAmount(KEY_INTERHUBAMOUNT);
static MetaKeyIndex keyKeepDepleted  (KEY_KEEPDEPLETED  );
static MetaKeyIndex keySortOrder     (KEY_SORTORDER     );
static MetaKeyIndex keyUseEffect     (KEY_USEEFFECT     );
static MetaKeyIndex keyUseAction     (KEY_USEACTION     );
static MetaKeyIndex keyUseSound      (KEY_USESOUND      );
static MetaKeyIndex keyArgs          (KEY_ARGS          );
static MetaKeyIndex keyUndroppable   (KEY_UNDROPPABLE   );

// Keys for specially treated artifact types
static MetaKeyIndex keyBackpackItem  (ARTI_BACKPACKITEM );

// clang-format on

//
// Temporary special values, starting from exceedingly remote negatives. They will be transformed
// back to special strings, to prevent mistakenly using them accidentally.
//
enum
{
    E_SPECIAL_KEYWORD_MAXHEALTH,
    E_SPECIAL_KEYWORD_SUPERHEALTH,
    E_NUM_SPECIAL_KEYWORDS // used when iterating
};

//
// These must be in sync with the above enum's meaningful values
//
static const char *eSpecialKeyword[] = {
    "@maxhealth",
    "@superhealth",
};

//
// Health special keyword support. The returned values will then be postprocessed and replaced with
// actual metastrings
//
static int E_keywordCB(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result)
{
    for(int i = 0; i < E_NUM_SPECIAL_KEYWORDS; ++i)
    {
        if(!strcasecmp(value, eSpecialKeyword[i]))
        {
            *static_cast<int *>(result) = INT_MIN + 1 + i;
            return 0;
        }
    }
    char *endptr = nullptr;
    long  val    = strtol(value, &endptr, 0);
    if(*endptr)
    {
        if(cfg)
            cfg_error(cfg, "invalid health value '%s' for option '%s'\n", value, opt->name);
        return -1;
    }
    *static_cast<int *>(result) = static_cast<int>(val);
    return 0;
}

// Health fields
#define HEALTHFX_FIELDS \
    CFG_INT_CB(KEY_AMOUNT,    0, CFGF_NONE, E_keywordCB), /* amount to recover          */   \
    CFG_INT_CB(KEY_MAXAMOUNT, 0, CFGF_NONE, E_keywordCB), /* max that can be recovered  */   \
    CFG_STR(KEY_LOWMESSAGE,  "", CFGF_NONE), /* message if health < amount */   \
                                                                                \
    CFG_FLAG(KEY_ALWAYSPICKUP, 0, CFGF_SIGNPREFIX), /* if +, always pick up */  \
    CFG_FLAG(KEY_SETHEALTH,    0, CFGF_SIGNPREFIX), /* if +, sets health    */  \
                                                                                \
    CFG_END()

cfg_opt_t edf_healthfx_opts[] = {
    HEALTHFX_FIELDS,
};

cfg_opt_t edf_healthfx_delta_opts[] = {
    CFG_STR(KEY_DELTA_NAME, nullptr, CFGF_NONE),
    HEALTHFX_FIELDS,
};

// Armor fields
#define ARMORFX_FIELDS \
    CFG_INT(KEY_SAVEAMOUNT,    -1,  CFGF_NONE), /* amount of armor given          */                 \
    CFG_INT(KEY_SAVEFACTOR,     1,  CFGF_NONE), /* numerator of save percentage   */                 \
    CFG_INT(KEY_SAVEDIVISOR,    3,  CFGF_NONE), /* denominator of save percentage */                 \
    CFG_INT(KEY_MAXSAVEAMOUNT,  0,  CFGF_NONE), /* max save amount, for bonuses   */                 \
                                                                                                     \
    CFG_FLAG(KEY_ALWAYSPICKUP,  0, CFGF_SIGNPREFIX), /* if +, always pick up                      */ \
    CFG_FLAG(KEY_ADDITIVE,      0, CFGF_SIGNPREFIX), /* if +, adds to the current amount of armor */ \
    CFG_FLAG(KEY_SETABSORPTION, 0, CFGF_SIGNPREFIX), /* if +, sets absorption values              */ \
                                                                                                     \
    CFG_END()

cfg_opt_t edf_armorfx_opts[] = { ARMORFX_FIELDS };

cfg_opt_t edf_armorfx_delta_opts[] = {
    CFG_STR(KEY_DELTA_NAME, nullptr, CFGF_NONE),
    ARMORFX_FIELDS,
};

// Ammo giver fields
#define AMMOFX_FIELDS \
    CFG_STR(KEY_AMMO,       "", CFGF_NONE), /* name of ammo type artifact to give        */               \
    CFG_INT(KEY_AMOUNT,     0,  CFGF_NONE), /* amount of ammo given                      */               \
    CFG_INT(KEY_DROPAMOUNT, 0,  CFGF_NONE), /* amount of ammo given when item is dropped */               \
                                                                                                          \
    CFG_FLAG(KEY_IGNORESKILL, 0, CFGF_SIGNPREFIX), /* if +, does not double on skills that double ammo */ \
                                                                                                          \
    CFG_END()

cfg_opt_t edf_ammofx_opts[] = {
    AMMOFX_FIELDS,
};

cfg_opt_t edf_ammofx_delta_opts[] = {
    CFG_STR(KEY_DELTA_NAME, nullptr, CFGF_NONE),
    AMMOFX_FIELDS,
};

// Powerup effect fields
#define POWERFX_FIELDS \
    CFG_INT(KEY_DURATION,  -1, CFGF_NONE), /* length of time to last         */                       \
    CFG_STR(KEY_TYPE,      "", CFGF_NONE), /* name of powerup effect to give */                       \
                                                                                                      \
    CFG_FLAG(KEY_ADDITIVETIME,  0, CFGF_SIGNPREFIX), /* if +, adds to current duration */             \
                                                                                                      \
    CFG_FLAG(KEY_PERMANENT,     0, CFGF_SIGNPREFIX), /* if +, lasts forever                        */ \
    CFG_FLAG(KEY_OVERRIDESSELF, 0, CFGF_SIGNPREFIX), /* if +, getting the power again while still  */ \
                                                     /* under its influence is allowed (a la DOOM) */ \
    /* TODO: support HUBPOWER and PERSISTENTPOWER properties, etc. */                                 \
                                                                                                      \
    CFG_END()

cfg_opt_t edf_powerfx_opts[] = {
    POWERFX_FIELDS,
};

cfg_opt_t edf_powerfx_delta_opts[] = {
    CFG_STR(KEY_DELTA_NAME, nullptr, CFGF_NONE),
    POWERFX_FIELDS,
};

// clang-format off

// NOTE TO SELF: Ratio in DOOMs is 2N, N, 5N, N
static cfg_opt_t ammogiven_opts[] =
{
    CFG_STR(KEY_TYPE,         "", CFGF_NONE), // type of ammo given
    CFG_INT(KEY_AMMOGIVE,     -1, CFGF_NONE), // amount of ammo given normally
    CFG_INT(KEY_AMMODROPPED,  -1, CFGF_NONE), // amount of ammo given when dropped
    CFG_INT(KEY_AMMODMSTAY,   -1, CFGF_NONE), // amount of ammo given in DM w/weapons stay
    CFG_INT(KEY_AMMOCOOPSTAY, -1, CFGF_NONE), // amount of ammo given in coop w/weapon stay
    CFG_END()
};

// clang-format on

// Weapon Giver effect fields
#define WEAPONFX_FIELDS \
    CFG_STR(KEY_WEAPON,       "",             CFGF_NONE             ), /* name of weapon to give             */ \
    CFG_MVPROP(KEY_AMMOGIVEN, ammogiven_opts, CFGF_MULTI|CFGF_NOCASE), /* type and quantities of ammos given */ \
    CFG_END()

cfg_opt_t edf_weapgfx_opts[] = {
    WEAPONFX_FIELDS,
};

// clang-format off

cfg_opt_t edf_weapgfx_delta_opts[] =
{
    CFG_STR(KEY_DELTA_NAME,    nullptr, CFGF_NONE),
    CFG_FLAG(KEY_CLRAMMOGIVEN, 0,       CFGF_NONE),
    WEAPONFX_FIELDS
};

// clang-format on

// Artifact subtype names
static const char *artiTypeNames[NUMARTITYPES] = {
    "NORMAL", // an ordinary artifact
    "AMMO",   // ammo type
    "KEY",    // key
    "PUZZLE", // puzzle item
    "POWER",  // powerup token
    "WEAPON", // weapon token
    "QUEST"   // quest token
};

//
// E_artiTypeCB
//
// Value parsing callback for artifact type
//
static int E_artiTypeCB(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result)
{
    int res;

    if((res = E_StrToNumLinear(artiTypeNames, NUMARTITYPES, value)) == NUMARTITYPES)
        res = ARTI_NORMAL;

    *(int *)result = res;

    return 0;
}

// Autouse options
static const char *autouseHealthModeNames[(int)AutoUseHealthMode::MAX] = {
    "none", "heretic",

    // TODO: add this when we support strife
    //   "strife"
};

//
// Parsing callback for autouse
//
static int E_autouseHealthModeCB(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result)
{
    int res;

    if((res = E_StrToNumLinear(autouseHealthModeNames, (int)earrlen(autouseHealthModeNames), value)) ==
       (int)earrlen(autouseHealthModeNames))
    {
        res = (int)AutoUseHealthMode::none;
    }
    *(int *)result = res;

    return 0;
}

static dehflags_t autousehealthrestrict_flaglist[] = {
    { "baby",       AHR_BABY       },
    { "deathmatch", AHR_DEATHMATCH },
    { nullptr,      0              },
};

static dehflagset_t autousehealthrestrict_flagset = {
    autousehealthrestrict_flaglist,
    0,
};

//
// Restrict by skill or game mode
//
static int E_autouseHealthRestrictCB(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result)
{
    *(int *)result = (int)E_ParseFlags(value, &autousehealthrestrict_flagset);
    return 0;
}

//
// Callback function for the function-valued string option used to
// specify state action functions. This is called during parsing, not
// processing, and thus we do not look up/resolve anything at this point.
// We are only interested in populating the cfg's args values with the
// strings passed to this callback as parameters. The value of the option has
// already been set to the name of the codepointer by the libConfuse framework.
//
static int E_actionFuncCB(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
    if(argc > EMAXARGS)
    {
        E_EDFLoggedWarning(2, "E_actionFuncCB: More than 16 args specified for artifact '%s'\n", cfg->title);
    }
    else if(argc > 0)
        cfg_setlistptr(cfg, KEY_ARGS, argc, static_cast<const void *>(argv));

    return 0; // everything is good
}

// Artifact fields
#define ARTIFACT_FIELDS \
    CFG_INT(KEY_AMOUNT,          1,  CFGF_NONE), /* amount gained with one pickup               */    \
    CFG_INT(KEY_MAXAMOUNT,       1,  CFGF_NONE), /* max amount that can be carried in inventory */    \
    CFG_INT(KEY_INTERHUBAMOUNT,  0,  CFGF_NONE), /* amount carryable between hubs (or levels)   */    \
    CFG_INT(KEY_SORTORDER,       0,  CFGF_NONE), /* relative ordering within inventory          */    \
    CFG_STR(KEY_ICON,            "", CFGF_NONE), /* icon used on inventory bars                 */    \
    CFG_INT(KEY_ICON_XOFFS,      0,  CFGF_NONE), /* x offset of icon (+ is left)                */    \
    CFG_INT(KEY_ICON_YOFFS,      0,  CFGF_NONE), /* y offset of icon (+ is right)               */    \
    CFG_STR(KEY_USESOUND,        "", CFGF_NONE), /* sound to play when used                     */    \
    CFG_STR(KEY_USEEFFECT,       "", CFGF_NONE), /* effect to activate when used                */    \
                                                                                                      \
    CFG_FLAG(KEY_UNDROPPABLE,    0, CFGF_SIGNPREFIX), /* if +, cannot be dropped                   */ \
    CFG_FLAG(KEY_INVBAR,         0, CFGF_SIGNPREFIX), /* if +, appears in inventory bar            */ \
    CFG_FLAG(KEY_KEEPDEPLETED,   0, CFGF_SIGNPREFIX), /* if +, remains in inventory if amount is 0 */ \
    CFG_FLAG(KEY_FULLAMOUNTONLY, 0, CFGF_SIGNPREFIX), /* if +, pick up for full amount only        */ \
    CFG_FLAG(KEY_NOSHAREWARE,    0, CFGF_SIGNPREFIX), /* if +, non-shareware                       */ \
                                                                                                      \
    CFG_INT_CB(KEY_AUTOUSE_HEALTH_MODE, (int)AutoUseHealthMode::none, CFGF_NONE, E_autouseHealthModeCB), \
    CFG_INT_CB(KEY_AUTOUSE_HEALTH_RESTRICT, 0, CFGF_NONE, E_autouseHealthRestrictCB),                 \
                                                                                                      \
    CFG_INT_CB(KEY_ARTIFACTTYPE, ARTI_NORMAL, CFGF_NONE, E_artiTypeCB), /* artifact sub-type */       \
                                                                                                      \
    CFG_STRFUNC(KEY_USEACTION,  "NULL",                  E_actionFuncCB), /* action function */       \
    CFG_STR(KEY_ARGS,           nullptr,      CFGF_LIST),                                             \
                                                                                                      \
                                                                                                      \
    /* Sub-Type Specific Fields */                                                                    \
    /* These only have meaning if the value of artifacttype is the expected value. */                 \
    /* You can set the keys on other artifacts, but they'll have no effect. */                        \
                                                                                                      \
    /* Ammo sub-type */                                                                               \
    CFG_INT(KEY_BACKPACKAMOUNT, 0, CFGF_NONE),                                                        \
    CFG_INT(KEY_BACKPACKMAXAMT, 0, CFGF_NONE),                                                        \
                                                                                                      \
    CFG_END()

cfg_opt_t edf_artifact_opts[] = {
    ARTIFACT_FIELDS,
};

cfg_opt_t edf_artifact_delta_opts[] = {
    CFG_STR(KEY_DELTA_NAME, nullptr, CFGF_NONE),
    ARTIFACT_FIELDS,
};

static const char *e_ItemSectionNames[NUMITEMFX] = {
    "", EDF_SEC_HEALTHFX, EDF_SEC_ARMORFX, EDF_SEC_AMMOFX, EDF_SEC_POWERFX, EDF_SEC_WEAPGFX, EDF_SEC_ARTIFACT
};

static const char *e_ItemDeltaSectionNames[NUMITEMFX] = { "",
                                                          EDF_SEC_HEALTHFXDELTA,
                                                          EDF_SEC_ARMORFXDELTA,
                                                          EDF_SEC_AMMOFXDELTA,
                                                          EDF_SEC_POWERFXDELTA,
                                                          EDF_SEC_WEAPGFXDELTA,
                                                          EDF_SEC_ARTIFACTDELTA };

//
// Some delta properties need special handling, so that's done here
//
static void E_handleSpecialItemDeltaProperties(const int i, cfg_t *sec, MetaTable *table, const char *const name)
{
    if(i == ITEMFX_WEAPONGIVER && cfg_size(sec, KEY_CLRAMMOGIVEN) > 0)
    {
        do
        {
            table->removeMetaTableNR(keyAmmoGiven);
        }
        while(metaerrno == META_ERR_NOERR);
    }
    else if(i == ITEMFX_ARTIFACT && (cfg_size(sec, KEY_USEACTION) > 0 || cfg_size(sec, KEY_ARGS) > 0))
    {
        do
        {
            table->removeString(keyArgs);
        }
        while(metaerrno == META_ERR_NOERR);
    }
}

//
// Converts any ultra-negative special values back to metastrings, for safety in case they leak
// elsewhere.
//
static void E_convertKeywordEnumsToStrings(MetaTable &table)
{
    // basic recursion defense
    if(table.getInt(__func__, 0))
        return;
    table.setInt(__func__, 1);

    MetaInteger *mint = nullptr;
    while((mint = table.getNextTypeEx(mint)))
    {
        // Remove any prior const string for any scanned metaint
        table.removeConstString(mint->getKeyIdx());
        for(int i = 0; i < E_NUM_SPECIAL_KEYWORDS; ++i)
        {
            if(mint->getValue() != INT_MIN + 1 + i)
                continue;
            table.setConstString(mint->getKeyIdx(), eSpecialKeyword[i]);
            table.removeObject(mint);
            delete mint;
            mint = nullptr; // restart search when deleting stuff
            break;
        }
    }

    MetaTable *subtable = nullptr;
    while((subtable = table.getNextTypeEx(subtable))) // also go recursively
        E_convertKeywordEnumsToStrings(*subtable);

    table.removeInt(__func__);
}

//
// E_processItemEffects
//
static void E_processItemEffects(cfg_t *cfg)
{
    // process each type of effect section
    for(int i = ITEMFX_HEALTH; i < NUMITEMFX; i++)
    {
        const char  *cfgSecName  = e_ItemSectionNames[i];
        const char  *className   = e_ItemEffectTypeNames[i];
        unsigned int numSections = cfg_size(cfg, cfgSecName);

        E_EDFLogPrintf("\t* Processing %s item effects (%u defined)\n", className, numSections);

        // process each section of the current type
        for(unsigned int secNum = 0; secNum < numSections; secNum++)
        {
            auto newEffect = E_addItemEffect(cfg_getnsec(cfg, cfgSecName, secNum), cfgSecName, secNum);

            // add the item effect type and name as properties
            newEffect->setInt(keyClass, i);
            newEffect->setConstString(keyClassName, className);

            E_EDFLogPrintf("\t\t* Processed item '%s'\n", newEffect->getKey());
        }

        cfgSecName  = e_ItemDeltaSectionNames[i];
        numSections = cfg_size(cfg, cfgSecName);

        E_EDFLogPrintf("\t* Processing %s item effect deltas (%u defined)\n", className, numSections);

        // process each section of the current delta type
        for(unsigned int secNum = 0; secNum < numSections; secNum++)
        {
            cfg_t      *sec  = cfg_getnsec(cfg, cfgSecName, secNum);
            const char *name = cfg_getstr(sec, KEY_DELTA_NAME);
            if(estrempty(name)) // invalid name?
                continue;
            auto table = e_effectsTable.getObjectKeyAndTypeEx<MetaTable>(name);
            if(!table) // nothing to delta
                continue;

            E_handleSpecialItemDeltaProperties(i, sec, table, name);

            MetaTable base(*table); // store the base entries in a copy

            // Update table
            E_MetaTableFromCfg(sec, table, &base);
            E_convertKeywordEnumsToStrings(*table);

            E_EDFLogPrintf("\t\t* Processed item delta %u to '%s'\n", secNum, name);
        }
    }
}

static void E_generateWeaponTrackers()
{
    itemeffect_t *trackerTemplate = E_ItemEffectForName("_WeaponTrackerTemplate");
    if(trackerTemplate == nullptr)
    {
        // For some weird, weird, weird, weird reason, the template wasn't found.
        E_EDFLoggedErr(2, "E_generateWeaponTrackers: Weapon tracker template "
                          "artifact (_WeaponTrackerTemplate) not found.\n");
    }

    for(int i = 0; i < NUMWEAPONTYPES; i++)
    {
        weaponinfo_t *currWeapon  = E_WeaponForID(i);
        itemeffect_t *currTracker = E_ItemEffectForName(currWeapon->name);
        if(currTracker != nullptr)
        {
            if(currTracker->getInt(keyClass, ITEMFX_NONE) != ITEMFX_ARTIFACT)
            {
                E_EDFLoggedErr(
                    2, "E_generateWeaponTrackers: Non-artifact item effect sharing name of weaponinfo '%s' found\n",
                    currWeapon->name);
            }
            currWeapon->tracker = currTracker;
        }
        else
        {
            currTracker = new itemeffect_t(currWeapon->name);
            e_effectsTable.addObject(currTracker);
            trackerTemplate->copyTableTo(currTracker);
            currWeapon->tracker = currTracker;
        }
    }
}

//=============================================================================
//
// Ammo Types
//
// Solely for efficiency, a collection of all ammo type artifacts is kept.
// This allows iterating over all ammo types without having hard-coded sets
// of metakeys or scanning the entire artifacts table.
//

// The ammo types lookup provides fast lookup of every artifact type that is of
// subtype ARTI_AMMO. This is for benefit of effects like the backpack, and
// cheats that give all ammo.
static PODCollection<itemeffect_t *> e_ammoTypesLookup;

//
// E_GetNumAmmoTypes
//
// Returns the total number of ammo types defined.
//
size_t E_GetNumAmmoTypes()
{
    return e_ammoTypesLookup.getLength();
}

//
// E_AmmoTypeForIndex
//
// Get an ammo type for its index in the ammotypes lookup table.
// There is no extra bounds check here, so an illegal request will exit the
// game engine. Use E_GetNumAmmoTypes to get the upper array bound.
//
itemeffect_t *E_AmmoTypeForIndex(size_t idx)
{
    return e_ammoTypesLookup[idx];
}

//
// E_collectAmmoTypes
//
// Scan the effects table for all effects that are of type ARTI_AMMO and add
// them to the ammo types lookup.
//
static void E_collectAmmoTypes()
{
    e_ammoTypesLookup.makeEmpty();

    itemeffect_t *itr = nullptr;

    while((itr = runtime_cast<itemeffect_t *>(e_effectsTable.tableIterator(itr))))
    {
        if(itr->getInt(keyArtifactType, ARTI_NORMAL) == ARTI_AMMO)
            e_ammoTypesLookup.add(itr);
    }
}

//
// E_GiveAllAmmo
//
// Function to give the player a certain amount of all ammo types; the amount
// given can be controlled using enumeration values in e_inventory.h
//
void E_GiveAllAmmo(player_t &player, giveallammo_e op, int amount)
{
    size_t numAmmo = E_GetNumAmmoTypes();

    for(size_t i = 0; i < numAmmo; i++)
    {
        int  giveamount = 0;
        auto ammoType   = E_AmmoTypeForIndex(i);

        switch(op)
        {
            // ioanch 20151225: removed GAA_BACKPACKAMOUNT because backpack really
            // does more than populate the inventory.
        case GAA_MAXAMOUNT: //
            giveamount = E_GetMaxAmountForArtifact(player, ammoType);
            break;
        case GAA_CUSTOM: //
            giveamount = amount;
            break;
        default: //
            break;
        }

        E_GiveInventoryItem(player, ammoType, giveamount);
    }
}

//=============================================================================
//
// Keys
//
// For similar reasons as for ammo, we keep a direct lookup table for artifacts
// of "key" type, so we can look them all up quickly when checking for locks,
// etc.
//

static PODCollection<itemeffect_t *> e_keysLookup;

//
// E_GetNumKeyItems
//
// Returns the total number of keys defined.
//
size_t E_GetNumKeyItems()
{
    return e_keysLookup.getLength();
}

//
// E_KeyItemForIndex
//
// Get a key type for its index in the ammotypes lookup table.
// There is no extra bounds check here, so an illegal request will exit the
// game engine. Use E_GetNumKeyItems to get the upper array bound.
//
itemeffect_t *E_KeyItemForIndex(size_t idx)
{
    return e_keysLookup[idx];
}

//
// E_collectKeyItems
//
// Scan the effects table for all effects that are of type ARTI_KEY and add
// them to the keys lookup.
//
static void E_collectKeyItems()
{
    e_keysLookup.makeEmpty();

    itemeffect_t *itr = nullptr;

    while((itr = runtime_cast<itemeffect_t *>(e_effectsTable.tableIterator(itr))))
    {
        if(itr->getInt(keyArtifactType, ARTI_NORMAL) == ARTI_KEY)
            e_keysLookup.add(itr);
    }
}

//=============================================================================
//
// Weapons
//
// We also need to look up weapons, due to polymorphing stripping player of
// current arsenal
//

static PODCollection<itemeffect_t *> e_weaponsLookup;

static size_t E_getNumWeaponItems()
{
    return e_weaponsLookup.getLength();
}

static itemeffect_t *E_weaponItemForIndex(size_t idx)
{
    return e_weaponsLookup[idx];
}

static void E_collectWeaponTypes()
{
    e_weaponsLookup.makeEmpty();
    itemeffect_t *itr = nullptr;
    while((itr = runtime_cast<itemeffect_t *>(e_effectsTable.tableIterator(itr))))
    {
        if(itr->getInt(keyArtifactType, ARTI_NORMAL) == ARTI_WEAPON)
            e_weaponsLookup.add(itr);
    }
}

//=============================================================================
//
// Autouse health items
//

static PODCollection<e_autouseid_t> e_autouseLookup; // cached collection of autouse items

//
// Comparator for sorting ascending by health provided
//
static int autouseCompare(const void *p1, const void *p2)
{
    auto a1 = static_cast<const e_autouseid_t *>(p1);
    auto a2 = static_cast<const e_autouseid_t *>(p2);

    return a1->amount - a2->amount;
}

//
// Collect inventory items for using in Heretic for saving player's life under certain skill levels
//
static void E_collectAutouseHealthItems()
{
    e_autouseLookup.makeEmpty();
    itemeffect_t *itr = nullptr;
    while((itr = runtime_cast<itemeffect_t *>(e_effectsTable.tableIterator(itr))))
    {
        if(itr->getInt(keyArtifactType, ARTI_NORMAL) != ARTI_NORMAL ||
           itr->getInt(keyAutouseHealthMode, (int)AutoUseHealthMode::none) != (int)AutoUseHealthMode::heretic)
        {
            continue;
        }

        const char         *useeffectstr = itr->getString(keyUseEffect, "");
        const itemeffect_t *effect       = E_ItemEffectForName(useeffectstr);
        if(!effect || effect->getInt("sethealth", 0)) // don't support health-set items, just health-add
            continue;

        int amount = effect->getInt(keyAmount, 0);
        if(!amount)
            continue;

        int restriction = itr->getInt(keyAutouseHealthRestrict, 0);

        e_autouseid_t &useid = e_autouseLookup.addNew();
        useid.amount         = amount;
        useid.artifact       = itr;
        useid.restriction    = restriction;
    }

    if(e_autouseLookup.isEmpty())
        return;

    // Now sort
    qsort(&e_autouseLookup[0], e_autouseLookup.getLength(), sizeof(e_autouseid_t), autouseCompare);
}

const PODCollection<e_autouseid_t> &E_GetAutouseList()
{
    return e_autouseLookup;
}

//=============================================================================
//
// Lockdefs
//

// lockdef color types
enum lockdefcolor_e
{
    LOCKDEF_COLOR_CONSTANT, // use a constant value
    LOCKDEF_COLOR_VARIABLE  // use a console variable
};

struct anykey_t
{
    unsigned int   numKeys;
    itemeffect_t **keys;
};

struct lockdef_t
{
    DLListItem<lockdef_t> links; // list links
    int                   id;    // lock ID, for ACS and internal use

    // Keys which are required to open the lock. If this list is empty, and
    // there are no "any" keys, then possession of any artifact of the "Key"
    // subtype is sufficient to open the lock.
    unsigned int   numRequiredKeys;
    itemeffect_t **requiredKeys;

    // Groups of keys in which at least one key is needed in each group to
    // open the lock - this forms a two-dimensional grid of lists.
    unsigned int numAnyLists; // number of anykey_t structures
    unsigned int numAnyKeys;  // total number of keys in the structures
    anykey_t    *anyKeys;

    char *message;       // message to give if attempt to open fails
    char *remoteMessage; // message to give if remote attempt to open fails
    char *lockedSound;   // name of sound to play on failure

    // Lock color data
    lockdefcolor_e colorType; // either constant or variable
    int            color;     // constant color, if colorType == LOCKDEF_COLOR_CONSTANT
    int           *colorVar;  // cvar color, if colorType == LOCKDEF_COLOR_VARIABLE
};

// Lockdefs hash, by ID number
static EHashTable<lockdef_t, EIntHashKey, &lockdef_t::id, &lockdef_t::links> e_lockDefs;

constexpr const char ITEM_LOCKDEF_REQUIRE[]  = "require";
constexpr const char ITEM_LOCKDEF_ANY[]      = "any";
constexpr const char ITEM_LOCKDEF_MESSAGE[]  = "message";
constexpr const char ITEM_LOCKDEF_REMOTE[]   = "remotemessage";
constexpr const char ITEM_LOCKDEF_ANY_KEYS[] = "keys";
constexpr const char ITEM_LOCKDEF_LOCKSND[]  = "lockedsound";
constexpr const char ITEM_LOCKDEF_MAPCOLOR[] = "mapcolor";

// "any" section options
static cfg_opt_t any_opts[] = {
    CFG_STR(ITEM_LOCKDEF_ANY_KEYS, "", CFGF_LIST),
    CFG_END(),
};

// clang-format off

// Lockdef section options
cfg_opt_t edf_lockdef_opts[] =
{
    CFG_STR(ITEM_LOCKDEF_REQUIRE,  "",       CFGF_MULTI),
    CFG_SEC(ITEM_LOCKDEF_ANY,      any_opts, CFGF_MULTI),
    CFG_STR(ITEM_LOCKDEF_MESSAGE,  nullptr,  CFGF_NONE ),
    CFG_STR(ITEM_LOCKDEF_REMOTE,   nullptr,  CFGF_NONE ),
    CFG_STR(ITEM_LOCKDEF_LOCKSND,  nullptr,  CFGF_NONE ),
    CFG_STR(ITEM_LOCKDEF_MAPCOLOR, nullptr,  CFGF_NONE ),
    CFG_END(),
};

// clang-format on

//
// E_LockDefForID
//
// Look up a lockdef by its id number.
//
static lockdef_t *E_LockDefForID(int id)
{
    return e_lockDefs.objectForKey(id);
}

//
// E_freeLockDefData
//
// Free the data in a lockdef and return it to its default state.
//
static void E_freeLockDefData(lockdef_t *lockdef)
{
    if(lockdef->requiredKeys)
    {
        efree(lockdef->requiredKeys);
        lockdef->requiredKeys = nullptr;
    }
    lockdef->numRequiredKeys = 0;

    if(lockdef->anyKeys)
    {
        for(unsigned int i = 0; i < lockdef->numAnyLists; i++)
        {
            anykey_t *any = &lockdef->anyKeys[i];

            if(any->keys)
                efree(any->keys);
        }
        efree(lockdef->anyKeys);
        lockdef->anyKeys = nullptr;
    }
    lockdef->numAnyLists = 0;
    lockdef->numAnyKeys  = 0;

    if(lockdef->message)
    {
        efree(lockdef->message);
        lockdef->message = nullptr;
    }
    if(lockdef->remoteMessage)
    {
        efree(lockdef->remoteMessage);
        lockdef->remoteMessage = nullptr;
    }
    if(lockdef->lockedSound)
    {
        efree(lockdef->lockedSound);
        lockdef->lockedSound = nullptr;
    }
}

//
// E_processKeyList
//
// Resolve a list of key names into a set of itemeffect pointers.
//
static void E_processKeyList(itemeffect_t **effects, unsigned int numKeys, cfg_t *sec, const char *fieldName)
{
    for(unsigned int i = 0; i < numKeys; i++)
    {
        const char   *name = cfg_getnstr(sec, fieldName, i);
        itemeffect_t *fx   = E_ItemEffectForName(name);

        if(!fx || fx->getInt(keyClass, ITEMFX_NONE) != ITEMFX_ARTIFACT)
            E_EDFLoggedWarning(2, "Warning: lockdef key '%s' is not an artifact\n", name);

        effects[i] = fx;
    }
}

//
// E_processLockDefColor
//
// Process a lockdef color field.
//
static void E_processLockDefColor(lockdef_t *lock, const char *value)
{
    // Default behavior - act like a closed door.
    lock->colorType = LOCKDEF_COLOR_VARIABLE;
    lock->colorVar  = &mapcolor_clsd;

    if(!value || !*value)
        return;

    AutoPalette pal(wGlobalDir);
    long        lresult = 0;
    command_t  *cmd     = nullptr;

    switch(*value)
    {
    case '$':
        // cvar value
        if((cmd = C_GetCmdForName(value + 1)) && cmd->type == ct_variable && cmd->variable->type == vt_int &&
           cmd->variable->min == 0 && cmd->variable->max == 255)
        {
            lock->colorType = LOCKDEF_COLOR_VARIABLE;
            lock->colorVar  = (int *)(cmd->variable->variable);
        }
        break;
    case '#':
        // hex constant
        lresult         = strtol(value + 1, nullptr, 16);
        lock->colorType = LOCKDEF_COLOR_CONSTANT;
        lock->color     = V_FindBestColor(pal.get(), (int)((lresult >> 16) & 0xff), (int)((lresult >> 8) & 0xff),
                                          (int)(lresult & 0xff));
        break;
    default:
        // decimal palette index
        lresult = strtol(value, nullptr, 10);
        if(lresult >= 0 && lresult <= 255)
        {
            lock->colorType = LOCKDEF_COLOR_CONSTANT;
            lock->color     = (int)lresult;
        }
        break;
    }

    // Only consider single-type locks for assigning color to key artifacts
    if(lock->numRequiredKeys + lock->numAnyLists == 1)
    {
        I_Assert(lock->numRequiredKeys >= 0 && lock->numAnyLists >= 0, "Negative key count");
        if(lock->numRequiredKeys && !lock->requiredKeys[0]->hasKey("_lockDefID"))
            lock->requiredKeys[0]->setInt("_lockDefID", lock->id);
        else if(lock->numAnyLists)
            for(int i = 0; i < lock->anyKeys[0].numKeys; ++i)
                if(!lock->anyKeys[0].keys[i]->hasKey("_lockDefID"))
                    lock->anyKeys[0].keys[i]->setInt("_lockDefID", lock->id);
    }
}

//
// E_processLockDef
//
// Process a single lock definition.
//
static void E_processLockDef(cfg_t *lock)
{
    lockdef_t *lockdef = nullptr;

    // the ID of the lockdef is the title of the section; it will be interpreted
    // as an integer
    int id = atoi(cfg_title(lock));

    // ID must be greater than 0
    if(id <= 0)
    {
        E_EDFLoggedWarning(2, "Warning: lockdef with invalid ID %d has been ignored\n", id);
        return;
    }

    // do we have this lock already?
    if((lockdef = E_LockDefForID(id)))
    {
        // free the existing data for this lock
        E_freeLockDefData(lockdef);
    }
    else
    {
        // create a new lockdef and hash it by key
        lockdef     = estructalloc(lockdef_t, 1);
        lockdef->id = id;
        e_lockDefs.addObject(lockdef);
    }

    // process required key definitions
    if((lockdef->numRequiredKeys = cfg_size(lock, ITEM_LOCKDEF_REQUIRE)))
    {
        lockdef->requiredKeys = estructalloc(itemeffect_t *, lockdef->numRequiredKeys);
        E_processKeyList(lockdef->requiredKeys, lockdef->numRequiredKeys, lock, ITEM_LOCKDEF_REQUIRE);
    }

    // process "any" key lists
    if((lockdef->numAnyLists = cfg_size(lock, ITEM_LOCKDEF_ANY)))
    {
        lockdef->anyKeys = estructalloc(anykey_t, lockdef->numAnyLists);
        for(unsigned int i = 0; i < lockdef->numAnyLists; i++)
        {
            cfg_t    *anySec    = cfg_getnsec(lock, ITEM_LOCKDEF_ANY, i);
            anykey_t *curAnyKey = &lockdef->anyKeys[i];

            if((curAnyKey->numKeys = cfg_size(anySec, ITEM_LOCKDEF_ANY_KEYS)))
            {
                curAnyKey->keys = estructalloc(itemeffect_t *, curAnyKey->numKeys);
                E_processKeyList(curAnyKey->keys, curAnyKey->numKeys, anySec, ITEM_LOCKDEF_ANY_KEYS);
                lockdef->numAnyKeys += curAnyKey->numKeys;
            }
        }
    }

    // process messages and sounds
    const char *tempstr;
    if((tempstr = cfg_getstr(lock, ITEM_LOCKDEF_MESSAGE))) // message
        lockdef->message = estrdup(tempstr);
    if((tempstr = cfg_getstr(lock, ITEM_LOCKDEF_REMOTE))) // remote message
        lockdef->remoteMessage = estrdup(tempstr);
    if((tempstr = cfg_getstr(lock, ITEM_LOCKDEF_LOCKSND))) // locked sound
        lockdef->lockedSound = estrdup(tempstr);

    // process map color
    E_processLockDefColor(lockdef, cfg_getstr(lock, ITEM_LOCKDEF_MAPCOLOR));

    E_EDFLogPrintf("\t\tDefined lockdef %d\n", lockdef->id);
}

//
// E_processLockDefs
//
// Process all lock definitions.
//
static void E_processLockDefs(cfg_t *cfg)
{
    unsigned int numLockDefs = cfg_size(cfg, EDF_SEC_LOCKDEF);

    E_EDFLogPrintf("\t* Processing lockdefs (%u defined)\n", numLockDefs);

    for(unsigned int i = 0; i < numLockDefs; i++)
        E_processLockDef(cfg_getnsec(cfg, EDF_SEC_LOCKDEF, i));
}

//
// E_failPlayerUnlock
//
// Routine to call when unlocking a lock has failed.
//
static void E_failPlayerUnlock(const player_t &player, const lockdef_t *lock, bool remote)
{
    const char *msg = nullptr;

    if(remote && lock->remoteMessage)
    {
        // if remote and have a remote message, give remote message
        msg = lock->remoteMessage;
        if(msg[0] == '$')
            msg = E_StringOrDehForName(msg + 1);
    }
    else if(lock->message)
    {
        // otherwise, give normal message
        msg = lock->message;
        if(msg[0] == '$')
            msg = E_StringOrDehForName(msg + 1);
    }
    if(msg)
        player_printf(&player, "%s", msg);

    // play sound if specified; if not, use skin default
    if(lock->lockedSound)
        S_StartSoundName(player.mo, lock->lockedSound);
    else
        S_StartSound(player.mo, GameModeInfo->playerSounds[sk_oof]);
}

//
// E_PlayerCanUnlock
//
// Check if a player has the keys necessary to unlock an object that is
// protected by a lock with the given ID.
//
bool E_PlayerCanUnlock(const player_t &player, int lockID, bool remote)
{
    lockdef_t *lock;

    if(!(lock = E_LockDefForID(lockID)))
        return true; // there's no such lock, so you can open it.

    // does this lock have required keys?
    if(lock->numRequiredKeys > 0)
    {
        unsigned int numRequiredHave = 0;

        for(unsigned int i = 0; i < lock->numRequiredKeys; i++)
        {
            itemeffect_t *key = lock->requiredKeys[i];
            if(E_GetItemOwnedAmount(player, key) > 0)
                ++numRequiredHave;
        }

        // check that the full number of required keys is present
        if(numRequiredHave < lock->numRequiredKeys)
        {
            E_failPlayerUnlock(player, lock, remote);
            return false;
        }
    }

    // does this lock have "any" key sets?
    if(lock->numAnyKeys > 0)
    {
        unsigned int numAnyHave = 0;

        for(unsigned int i = 0; i < lock->numAnyLists; i++)
        {
            anykey_t *any = &lock->anyKeys[i];

            if(!any->numKeys)
                continue;

            // we need at least one key in this set
            for(unsigned int keynum = 0; keynum < any->numKeys; keynum++)
            {
                itemeffect_t *key = any->keys[keynum];
                if(E_GetItemOwnedAmount(player, key) > 0)
                {
                    numAnyHave += any->numKeys; // credit for full set
                    break;                      // can break out of inner loop, player has a key in this set
                }
            }
        }

        // missing one or more categories of "any" list keys?
        if(numAnyHave < lock->numAnyKeys)
        {
            E_failPlayerUnlock(player, lock, remote);
            return false;
        }
    }

    // if a lockdef has neither required nor "any" keys, then it opens if the
    // player possesses at least one item of class "Key"
    if(!lock->numRequiredKeys && !lock->numAnyKeys)
    {
        int numKeys = 0;

        for(size_t i = 0; i < E_GetNumKeyItems(); i++)
        {
            itemeffect_t *key = E_KeyItemForIndex(i);
            if(E_GetItemOwnedAmount(player, key) > 0)
                ++numKeys;
        }

        // if no keys are possessed, fail the lock
        if(!numKeys)
        {
            E_failPlayerUnlock(player, lock, remote);
            return false;
        }
    }

    // you can unlock it!
    return true;
}

//
// E_GetLockDefColor
//
// Get the automap color for a lockdef.
//
int E_GetLockDefColor(int lockID, bool keyCheatHack)
{
    int              color = 0;
    const lockdef_t *lock;

    if((lock = E_LockDefForID(lockID)))
    {
        switch(lock->colorType)
        {
        case LOCKDEF_COLOR_CONSTANT: //
            color = lock->color;
            break;
        case LOCKDEF_COLOR_VARIABLE: //
            color = *lock->colorVar;
            if(keyCheatHack)
            {
                if(color == mapcolor_bdor)
                    color = mapcolor_bkey;
                else if(color == mapcolor_ydor)
                    color = mapcolor_ykey;
                else if(color == mapcolor_rdor)
                    color = mapcolor_rkey;
            }
            break;
        default: //
            break;
        }
    }

    return color;
}

//
// E_GiveAllKeys
//
// Give a player every artifact type that is considered a key and is not
// already owned. Returns the number of keys given.
//
int E_GiveAllKeys(player_t &player)
{
    size_t numKeys   = E_GetNumKeyItems();
    int    keysGiven = 0;

    for(size_t i = 0; i < numKeys; i++)
    {
        itemeffect_t *key = E_KeyItemForIndex(i);
        if(!E_GetItemOwnedAmount(player, key))
        {
            if(E_GiveInventoryItem(player, key))
                ++keysGiven;
        }
    }

    return keysGiven;
}

//
// E_TakeAllKeys
//
// Take away every artifact a player has that is of "key" type.
// Returns the number of keys taken away.
//
int E_TakeAllKeys(player_t &player)
{
    size_t numKeys   = E_GetNumKeyItems();
    int    keysTaken = 0;

    for(size_t i = 0; i < numKeys; i++)
    {
        if(E_RemoveInventoryItem(player, E_KeyItemForIndex(i), -1) != INV_NOTREMOVED)
            ++keysTaken;
    }

    return keysTaken;
}

//=============================================================================
//
// Effect Hash Table
//

static EHashTable<e_pickupfx_t, ENCStringHashKey, &e_pickupfx_t::name, &e_pickupfx_t::namelinks> e_PickupNameHash;

static EHashTable<e_pickupfx_t, ENCStringHashKey, &e_pickupfx_t::compatname, &e_pickupfx_t::cnamelinks>
    e_PickupCNameHash;

static EHashTable<e_pickupfx_t, EIntHashKey, &e_pickupfx_t::sprnum, &e_pickupfx_t::sprnumlinks> e_PickupSprNumHash;

//=============================================================================
//
// Effect Flags
//

static dehflags_t e_PickupFlags[] = {
    { "ALWAYSPICKUP",      PFXF_ALWAYSPICKUP      },
    { "LEAVEINMULTI",      PFXF_LEAVEINMULTI      },
    { "NOSCREENFLASH",     PFXF_NOSCREENFLASH     },
    { "SILENTNOBENEFIT",   PFXF_SILENTNOBENEFIT   },
    { "COMMERCIALONLY",    PFXF_COMMERCIALONLY    },
    { "GIVESBACKPACKAMMO", PFXF_GIVESBACKPACKAMMO },
    { nullptr,             0                      }
};

static dehflagset_t e_PickupFlagSet = {
    e_PickupFlags, // flags
    0              // mode
};

//=============================================================================
//
// Effect Bindings
//
// Effects can be bound to sprites.
//

// Pick-up effects
constexpr const char ITEM_PICKUP_CNAME[]     = "pfxname";
constexpr const char ITEM_PICKUP_SPRITE[]    = "sprite";
constexpr const char ITEM_PICKUP_FX[]        = "effect";
constexpr const char ITEM_PICKUP_EFFECTS[]   = "effects";
constexpr const char ITEM_PICKUP_CHANGEWPN[] = "changeweapon";
constexpr const char ITEM_PICKUP_MSG[]       = "message";
constexpr const char ITEM_PICKUP_SOUND[]     = "sound";
constexpr const char ITEM_PICKUP_FLAGS[]     = "flags";

// clang-format off

// sprite-based pickup items
cfg_opt_t edf_sprpkup_opts[] =
{
    CFG_STR(ITEM_PICKUP_FX,    "PFX_NONE", CFGF_NONE),
    CFG_STR(ITEM_PICKUP_MSG,   nullptr,    CFGF_NONE),
    CFG_STR(ITEM_PICKUP_SOUND, nullptr,    CFGF_NONE),

    CFG_END()
};

// standalone pickup effects
cfg_opt_t edf_pkupfx_opts[] =
{
    CFG_STR(ITEM_PICKUP_CNAME,     nullptr, CFGF_NONE),
    CFG_STR(ITEM_PICKUP_SPRITE,    nullptr, CFGF_NONE),
    CFG_STR(ITEM_PICKUP_EFFECTS,   nullptr, CFGF_LIST),
    CFG_STR(ITEM_PICKUP_CHANGEWPN, "",      CFGF_NONE),
    CFG_STR(ITEM_PICKUP_MSG,       nullptr, CFGF_NONE),
    CFG_STR(ITEM_PICKUP_SOUND,     nullptr, CFGF_NONE),
    CFG_STR(ITEM_PICKUP_FLAGS,     nullptr, CFGF_NONE),

    CFG_END()
};

// clang-format on

// pickup variables

//
// Obtain a e_pickupfx_t structure by name.
//
e_pickupfx_t *E_PickupFXForName(const char *name)
{
    return e_PickupNameHash.objectForKey(name);
}

//
// Obtain a e_pickupfx_t structure by compat name.
//
static e_pickupfx_t *e_PickupFXForCName(const char *cname)
{
    return e_PickupCNameHash.objectForKey(cname);
}

//
// Obtain a e_pickupfx_t structure by sprite num.
//
e_pickupfx_t *E_PickupFXForSprNum(spritenum_t sprnum)
{
    return e_PickupSprNumHash.objectForKey(sprnum);
}

//
// E_processPickupItems
//
// Allocates the pickupfx array used in P_TouchSpecialThing, and loads all
// pickupitem definitions, using the sprite hash table to resolve what sprite
// owns the specified effect.
//
static void E_processPickupItems(cfg_t *cfg)
{
    int i, numpickups;

    E_EDFLogPuts("\t* Processing pickup items\n");

    // load pickupfx
    numpickups = cfg_size(cfg, EDF_SEC_SPRPKUP);
    E_EDFLogPrintf("\t\t%d pickup item(s) defined\n", numpickups);
    for(i = 0; i < numpickups; ++i)
    {
        int         sprnum;
        cfg_t      *sec   = cfg_getnsec(cfg, EDF_SEC_SPRPKUP, i);
        const char *title = cfg_title(sec);
        const char *str   = cfg_getstr(sec, ITEM_PICKUP_FX);

        // validate the sprite name given in the section title and
        // resolve to a sprite number (hashed)
        sprnum = E_SpriteNumForName(title);

        if(sprnum == -1)
        {
            // haleyjd 05/31/06: downgraded to warning, substitute blanksprite
            E_EDFLoggedWarning(2, "Warning: invalid sprite mnemonic for pickup item: '%s'\n", title);
            continue;
        }

        e_pickupfx_t *pfx = e_PickupFXForCName(str);
        if(pfx == nullptr)
        {
            E_EDFLoggedWarning(2, "Warning: invalid effect '%s' for pickup item : '%s'\n", str, title);
            continue;
        }

        // EDF_FEATURES_FIXME: This?
        e_PickupSprNumHash.removeObject(pfx);
        pfx->sprnum = sprnum;
        e_PickupSprNumHash.addObject(pfx);

        // free any strings that might have been previously set
        if(pfx->message)
        {
            efree(pfx->message);
            pfx->message = nullptr;
        }
        if(pfx->sound)
        {
            efree(pfx->sound);
            pfx->sound = nullptr;
        }

        // process effect properties

        // message
        if((str = cfg_getstr(sec, ITEM_PICKUP_MSG)))
            pfx->message = estrdup(str);

        // sound
        if((str = cfg_getstr(sec, ITEM_PICKUP_SOUND)))
            pfx->sound = estrdup(str);
    }
}

//
// Parse the given string's pickup flags
//
pickupflags_e E_PickupFlagsForStr(const char *flagstr)
{
    if(*flagstr == '\0')
        return PXFX_NONE;
    else
    {
        unsigned int results = E_ParseFlags(flagstr, &e_PickupFlagSet);
        return static_cast<pickupflags_e>(results);
    }
}

//
// Process a single modern pickup effect.
//
static void E_processPickupEffect(cfg_t *sec)
{
    const char   *title = cfg_title(sec);
    const char   *str;
    e_pickupfx_t *pfx = E_PickupFXForName(title);
    if(pfx == nullptr)
        pfx = estructalloc(e_pickupfx_t, 1);

    if(!pfx->name)
    {
        pfx->name = estrdup(title);
        e_PickupNameHash.addObject(pfx);
    }

    if(pfx->effects)
    {
        efree(pfx->effects);
        pfx->effects = nullptr;
    }
    if(pfx->changeweapon)
        pfx->changeweapon = nullptr;
    if(pfx->compatname)
    {
        e_PickupCNameHash.removeObject(pfx);
        efree(pfx->compatname);
        pfx->compatname = nullptr;
    }
    if(pfx->sprnum != -1)
    {
        pfx->sprnum = -1;
        e_PickupSprNumHash.removeObject(pfx);
    }
    if(pfx->message)
    {
        efree(pfx->message);
        pfx->message = nullptr;
    }
    if(pfx->sound)
    {
        efree(pfx->sound);
        pfx->sound = nullptr;
    }

    if((pfx->numEffects = cfg_size(sec, ITEM_PICKUP_EFFECTS)))
    {
        pfx->effects = ecalloc(itemeffect_t **, pfx->numEffects, sizeof(itemeffect_t **));
        for(unsigned int i = 0; i < pfx->numEffects; i++)
        {
            str = cfg_getnstr(sec, ITEM_PICKUP_EFFECTS, i);
            if(!(pfx->effects[i] = E_ItemEffectForName(str)))
            {
                E_EDFLoggedWarning(2, "Warning: invalid pickup effect: '%s'\n", str);
                return;
            }
            else if(pfx->effects[i]->getInt(keyArtifactType, NUMARTITYPES) == ARTI_WEAPON)
            {
                // This should never happen, but whatever.
                E_EDFLoggedWarning(2,
                                   "Warning: pickup effect '%s' refers to "
                                   "weapon tracker: '%s'\n",
                                   title, str);
            }
        }
    }

    if((str = cfg_getstr(sec, ITEM_PICKUP_CHANGEWPN)))
    {
        if(estrnonempty(str) && !(pfx->changeweapon = E_WeaponForName(str)))
        {
            E_EDFLoggedWarning(2, "Warning: invalid changeweapon '%s' for pickup effect '%s'\n", str, title);
        }
    }

    // compatname
    if((str = cfg_getstr(sec, ITEM_PICKUP_CNAME)))
    {
        pfx->compatname = estrdup(str);
        e_PickupCNameHash.addObject(pfx);
    }

    if((str = cfg_getstr(sec, ITEM_PICKUP_SPRITE)))
    {
        pfx->sprnum = E_SpriteNumForName(str);
        if(pfx->sprnum == -1)
        {
            E_EDFLoggedWarning(2, "Warning: invalid sprite '%s' for pickup effect '%s'\n", str, title);
        }
        else
            e_PickupSprNumHash.addObject(pfx);
    }
    else
        pfx->sprnum = -1;

    // message
    if((str = cfg_getstr(sec, ITEM_PICKUP_MSG)))
        pfx->message = estrdup(str);

    // sound
    if((str = cfg_getstr(sec, ITEM_PICKUP_SOUND)))
        pfx->sound = estrdup(str);

    if((str = cfg_getstr(sec, ITEM_PICKUP_FLAGS)))
        pfx->flags = E_PickupFlagsForStr(str);
}

//
// Process all the modern pickup effects, done after most other processing.
//
static void E_processPickupEffects(cfg_t *cfg)
{
    int i, numpickups;

    E_EDFLogPuts("\t* Processing pickup items\n");

    // sanity check
    // if(!pickupfx)
    //   E_EDFLoggedErr(2, "E_ProcessItems: no sprites defined!?\n");

    // load pickupfx
    numpickups = cfg_size(cfg, EDF_SEC_PICKUPFX);
    E_EDFLogPrintf("\t\t%d pickup item(s) defined\n", numpickups);
    for(i = 0; i < numpickups; ++i)
    {
        cfg_t      *sec   = cfg_getnsec(cfg, EDF_SEC_PICKUPFX, i);
        const char *title = cfg_title(sec);

        E_EDFLogPrintf("\tCreated pickup effect %s\n", title);

        // process effect properties
        E_processPickupEffect(sec);
    }
}

//=============================================================================
//
// Inventory Items
//
// Inventory items represent a holdable item that can take up a slot in an
// inventory.
//

// Lookup table of inventory item types by assigned ID number
static PODCollection<itemeffect_t *> e_InventoryItemsByID;
static inventoryitemid_t             e_maxitemid;

inventoryindex_t e_maxvisiblesortorder = INT_MIN;

//
// E_MoveInventoryCursor
//
// Tries to move the inventory cursor 'amount' right.
// Returns true if cursor wasn't adjusted (outside of amount being added).
//
bool E_MoveInventoryCursor(const player_t &player, const int amount, int &cursor)
{
    if(cursor + amount < 0)
    {
        cursor = 0;
        return false;
    }
    if(amount <= 0)
    {
        cursor += amount;
        return true; // We know that the cursor will succeed in moving left
    }

    itemeffect_t *effect = E_EffectForInventoryIndex(player, cursor + amount);
    if(!effect)
        return false;
    if(effect->getInt(keySortOrder, INT_MAX) > e_maxvisiblesortorder)
        return false;

    cursor += amount;
    return true;
}

//
// Checks if cursor can be moved 'amount' right without mutating cursor
//
bool E_CanMoveInventoryCursor(const player_t *player, const int amount, const int cursor)
{
    if(cursor + amount < 0)
        return false;
    if(amount <= 0)
        return true; // We know that the cursor will succeed in moving left

    itemeffect_t *effect = E_EffectForInventoryIndex(*player, cursor + amount);
    if(!effect)
        return false;
    if(effect->getInt(keySortOrder, INT_MAX) > e_maxvisiblesortorder)
        return false;

    return true;
}

//
// Says if a player possesses at least one item w/ +invbar
//
bool E_PlayerHasVisibleInvItem(const player_t &player)
{
    int i = -1;
    return E_MoveInventoryCursor(player, 1, i);
}

//
// E_getItemEffectType
//
// Gets the effect type of an item.
//
static itemeffecttype_t E_getItemEffectType(itemeffect_t *fx)
{
    return static_cast<itemeffecttype_t>(fx->getInt(keyClass, 0));
}

//
// An extended actionargs_t, that allows for hashing
//
struct useaction_t : actionargs_t
{
    const char *artifactname; // The key that's used in hashing

    DLListItem<useaction_t> links; // Hash by name
};

//=============================================================================
//
// Artifact Action Hash Table
//

static EHashTable<useaction_t, ENCStringHashKey, &useaction_t::artifactname, &useaction_t::links> e_UseActionHash;

//
// Obtain a useaction_t structure by name.
//
static useaction_t *E_useActionForArtifactName(const char *name)
{
    return e_UseActionHash.objectForKey(name);
}

//
// Create and add a new useaction_t, then return a pointer it
//
static useaction_t *E_addUseAction(itemeffect_t *artifact)
{
    useaction_t *toadd = estructalloc(useaction_t, 1);
    arglist_t   *args  = estructalloc(arglist_t, 1);

    MetaString *ms = nullptr;
    while((ms = artifact->getNextKeyAndTypeEx(ms, keyArgs)))
    {
        if(!E_AddArgToList(args, ms->getValue()))
            return nullptr;
    }

    toadd->artifactname = artifact->getKey(); // FIXME: This is safe, right?
    toadd->actiontype   = actionargs_t::ARTIFACT;
    toadd->args         = args;
    e_UseActionHash.addObject(toadd);
    return toadd;
}

//
// E_TryUseItem
//
// Tries to use the currently selected item
// Returns true if item was successfully used, otherwise false
//
bool E_TryUseItem(player_t &player, inventoryitemid_t ID)
{
    invbarstate_t &invbarstate = player.invbarstate;
    itemeffect_t  *artifact    = E_EffectForInventoryItemID(ID);

    if(!artifact)
        return false;

    if(E_getItemEffectType(artifact) == ITEMFX_ARTIFACT)
    {
        if(artifact->getInt(keyArtifactType, -1) == ARTI_NORMAL)
        {
            bool success = false;

            const char   *useeffectstr = artifact->getString(keyUseEffect, "");
            itemeffect_t *effect       = E_ItemEffectForName(useeffectstr);
            if(effect)
            {
                switch(E_getItemEffectType(effect))
                {
                case ITEMFX_HEALTH: //
                    success = P_GiveBody(player, effect);
                    break;
                case ITEMFX_ARMOR: //
                    success = P_GiveArmor(player, effect);
                    break;
                case ITEMFX_AMMO: //
                    success = P_GiveAmmoPickup(player, effect, false, 0);
                    break;
                case ITEMFX_POWER: //
                    success = P_GivePowerForItem(player, effect);
                    break;
                default: //
                    return false;
                }
            }

            const char *useactionstr = artifact->getString(keyUseAction, "");
            deh_bexptr *ptr          = D_GetBexPtr(useactionstr);
            if(ptr)
            {
                // Try and get the cached useaction, and if we fail, make one
                useaction_t *useaction = E_useActionForArtifactName(artifact->getKey());
                if(useaction == nullptr)
                {
                    if((useaction = E_addUseAction(artifact)) == nullptr)
                    {
                        doom_printf("Too many args specified in useaction for artifact '%s'\a\n", artifact->getKey());
                    }
                }
                if(useaction != nullptr)
                {
                    actionargs_t action = *useaction;

                    // Temporarily note down that the called codepointer shouldn't subtract ammo
                    player.attackdown = static_cast<attacktype_e>(player.attackdown | AT_ITEM);
                    // We ALWAYS update the actor and psprite
                    action.actor = player.mo;
                    action.pspr  = player.psprites;
                    ptr->cptr(&action);
                    success           = true;
                    player.attackdown = static_cast<attacktype_e>(player.attackdown & ~AT_ITEM);
                }
            }
            else if(estrnonempty(useactionstr))
            {
                doom_printf("Codepointer '%s' not found in useaction for artifact '%s'\a\n", useactionstr,
                            artifact->getKey());
            }

            if(success)
            {
                const char *sound;
                E_RemoveInventoryItem(player, artifact, 1);

                sound = artifact->getString(keyUseSound, "");
                if(estrnonempty(sound))
                    S_StartSoundName(player.mo, sound);

                invbarstate.ArtifactFlash = 5;
            }
            else
            {
                // Heretic shifts inventory one left if you fail to use your selected item.
                // FIXME: Make this behaviour optional, or remove
                E_MoveInventoryCursor(player, -1, player.inv_ptr);
            }

            return success;
        }
    }

    return false;
}

//
// E_allocateInventoryItemIDs
//
// First, if the item ID table has already been built, we need to wipe it out.
// Then, regardless, build the artifact ID table by scanning the effects table
// for items of classes which enter the inventory, and assign each one a unique
// artifact ID. The ID will be added to the object.
//
static void E_allocateInventoryItemIDs()
{
    itemeffect_t *item = nullptr;

    // empty the table if it was already created before
    e_InventoryItemsByID.clear();
    e_maxitemid = 0;

    // scan the effects table and add artifacts to the table
    while((item = runtime_cast<itemeffect_t *>(e_effectsTable.tableIterator(item))))
    {
        itemeffecttype_t fxtype = item->getInt(keyClass, ITEMFX_NONE);

        // only interested in effects that are recorded in the inventory
        if(fxtype == ITEMFX_ARTIFACT)
        {
            // If the current item's sort order is the largest thus far and is visible
            if(item->getInt(keySortOrder, 0) > e_maxvisiblesortorder && item->getInt(keyInvBar, 0))
                e_maxvisiblesortorder = item->getInt(keySortOrder, 0);

            // add it to the table
            e_InventoryItemsByID.add(item);

            // add the ID to the artifact definition
            item->setInt(keyItemID, e_maxitemid++);
        }
    }
}

//
// Allocates sort orders to any artifact that needs one
//
static void E_allocateSortOrders()
{
    // Scan the effects table and add artifacts to the table
    itemeffect_t *item = nullptr;

    // All +invbar items w/o explicit sort order share the same sort order
    // They are then ordered at runtime alphabetically
    e_maxvisiblesortorder++;

    while((item = runtime_cast<itemeffect_t *>(e_effectsTable.tableIterator(item))))
    {
        itemeffecttype_t fxtype = item->getInt(keyClass, ITEMFX_NONE);

        // Only interested in effects that are recorded in the inventory
        if(fxtype == ITEMFX_ARTIFACT)
        {
            // If the current isn't visible
            if(!item->getInt(keyInvBar, 0))
                item->setInt(keySortOrder, e_maxvisiblesortorder + 1);
            else if(item->getObject(keySortOrder) == nullptr)
                item->setInt(keySortOrder, e_maxvisiblesortorder);
        }
    }
}

//
// E_allocatePlayerInventories
//
// Allocate inventory arrays for the player_t structures.
//
static void E_allocatePlayerInventories()
{
    for(int i = 0; i < MAXPLAYERS; i++)
    {
        if(players[i].inventory)
            efree(players[i].inventory);

        players[i].inventory = estructalloc(inventoryslot_t, e_maxitemid);

        for(inventoryindex_t idx = 0; idx < e_maxitemid; idx++)
            players[i].inventory[idx].item = -1;

        if(players[i].unmorphInventory)
            efree(players[i].unmorphInventory);
        players[i].unmorphInventory = estructalloc(inventoryslot_t, e_maxitemid);
    }
}

//
// E_EffectForInventoryItemID
//
// Return the effect definition associated with an inventory item ID.
//
itemeffect_t *E_EffectForInventoryItemID(inventoryitemid_t id)
{
    return (id >= 0 && id < e_maxitemid) ? e_InventoryItemsByID[id] : nullptr;
}

//
// E_EffectForInventoryIndex
//
// Get the item effect for a particular index in a given player's inventory.
//
itemeffect_t *E_EffectForInventoryIndex(const player_t &player, inventoryindex_t idx)
{
    return (idx >= 0 && idx < e_maxitemid) ? E_EffectForInventoryItemID(player.inventory[idx].item) : nullptr;
}

//
// E_InventorySlotForItemID
//
// Find the slot being used by an item in the player's inventory, if one exists.
// nullptr is returned if the item is not in the player's inventory.
//
inventoryslot_t *E_InventorySlotForItemID(const player_t &player, inventoryitemid_t id)
{
    inventory_t inventory = player.inventory;

    for(inventoryindex_t idx = 0; idx < e_maxitemid; idx++)
    {
        if(inventory[idx].item == id)
            return &inventory[idx];
    }

    return nullptr;
}

//
// E_InventorySlotForItem
//
// Find the slot being used by an item in the player's inventory, by pointer,
// if one exists. nullptr is returned if the item is not in the player's
// inventory.
//
inventoryslot_t *E_InventorySlotForItem(const player_t &player, const itemeffect_t *effect)
{
    inventoryitemid_t id;

    if(effect && (id = effect->getInt(keyItemID, -1)) >= 0)
        return E_InventorySlotForItemID(player, id);
    else
        return nullptr;
}

//
// E_InventorySlotForItemName
//
// Find the slot being used by an item in the player's inventory, by name,
// if one exists. nullptr is returned if the item is not in the player's
// inventory.
//
inventoryslot_t *E_InventorySlotForItemName(const player_t &player, const char *name)
{
    return E_InventorySlotForItem(player, E_ItemEffectForName(name));
}

//
// Returns the item ID for a given item effect name. Returns -1 if not found.
//
int E_ItemIDForName(const char *name)
{
    const itemeffect_t *effect = E_ItemEffectForName(name);
    if(!effect)
        return -1;
    return effect->getInt(keyItemID, -1);
}

//
// E_findInventorySlot
//
// Finds the first unused inventory slot index.
//
static inventoryindex_t E_findInventorySlot(inventory_t inventory)
{
    // find the first unused slot and return it
    for(inventoryindex_t idx = 0; idx < e_maxitemid; idx++)
    {
        if(inventory[idx].item == -1)
            return idx;
    }

    // should be unreachable
    return -1;
}

//
// E_sortInventory
//
// After a new slot is added to the inventory, it needs to be placed into its
// proper sorted position based on the item effects' sortorder fields.
//
static void E_sortInventory(const player_t &player, inventoryindex_t newIndex, int sortorder, const char *name)
{
    inventory_t     inventory = player.inventory;
    inventoryslot_t tempSlot  = inventory[newIndex];

    for(inventoryindex_t idx = 0; idx < newIndex; idx++)
    {
        const itemeffect_t *effect;

        if((effect = E_EffectForInventoryIndex(player, idx)))
        {
            const int thatorder = effect->getInt(keySortOrder, 0);
            // If sort order is shared then sort them alphabetically (case-insensitive)
            if(thatorder < sortorder || (thatorder == sortorder && strcasecmp(name, effect->getKey()) > 0))
                continue;
            else
            {
                // shift everything up
                for(inventoryindex_t up = newIndex; up > idx; up--)
                    inventory[up] = inventory[up - 1];

                // put the saved slot into its proper place
                inventory[idx] = tempSlot;
                return;
            }
        }
    }
}

//
// E_PlayerHasBackpack
//
// Special lookup function to test if the player has a backpack.
//
bool E_PlayerHasBackpack(const player_t &player)
{
    auto backpackItem = runtime_cast<itemeffect_t *>(e_effectsTable.getObject(keyBackpackItem));
    return (E_GetItemOwnedAmount(player, backpackItem) > 0);
}

//
// E_GiveBackpack
//
// Special function to give a backpack.
//
bool E_GiveBackpack(player_t &player)
{
    auto backpackItem = runtime_cast<itemeffect_t *>(e_effectsTable.getObject(keyBackpackItem));

    return E_GiveInventoryItem(player, backpackItem);
}

//
// E_RemoveBackpack
//
// Special function to remove a backpack.
//
bool E_RemoveBackpack(player_t &player)
{
    auto          backpackItem = runtime_cast<itemeffect_t *>(e_effectsTable.getObject(keyBackpackItem));
    bool          removed      = false;
    itemremoved_e code;

    if((code = E_RemoveInventoryItem(player, backpackItem, -1)) != INV_NOTREMOVED)
    {
        removed = true;

        // cut all ammo types to their normal max amount
        size_t numAmmo = E_GetNumAmmoTypes();

        for(size_t i = 0; i < numAmmo; i++)
        {
            auto ammo      = E_AmmoTypeForIndex(i);
            int  maxamount = ammo->getInt(keyMaxAmount, 0);
            auto slot      = E_InventorySlotForItem(player, ammo);

            if(slot && slot->amount > maxamount)
                slot->amount = maxamount;
        }
    }

    return removed;
}

//
// E_GetMaxAmountForArtifact
//
// Get the max amount of an artifact that can be carried. There are some
// special cases for different token subtypes of artifact.
//
int E_GetMaxAmountForArtifact(const player_t &player, const itemeffect_t *artifact)
{
    if(!artifact)
        return 0;

    int subType = artifact->getInt(keyArtifactType, ARTI_NORMAL);

    switch(subType)
    {
    case ARTI_AMMO:
        // ammo may increase the max amount if the player is carrying a backpack
        if(E_PlayerHasBackpack(player))
            return artifact->getInt(keyBackpackMaxAmt, 0);
        break;
    default: //
        break;
    }

    // The default case is to return the ordinary max amount.
    return artifact->getInt(keyMaxAmount, 1);
}

//
// E_GetItemOwnedAmount
//
// If you do not need the inventory slot for any other purpose, you can lookup
// the amount of an item owned in one step by using this function.
//
int E_GetItemOwnedAmount(const player_t &player, const itemeffect_t *artifact)
{
    auto slot = E_InventorySlotForItem(player, artifact);

    return (slot ? slot->amount : 0);
}

//
// E_GetItemOwnedAmountName
//
// As above, but also doing a lookup on name.
//
int E_GetItemOwnedAmountName(const player_t &player, const char *name)
{
    auto slot = E_InventorySlotForItemName(player, name);

    return (slot ? slot->amount : 0);
}

//
// Check if player has power by name
//
bool E_PlayerHasPowerName(const player_t &player, const char *name)
{
    if(estrempty(name))
        return false;
    int num = E_StrToNumLinear(powerStrings, NUMPOWERS, name);
    if(num == NUMPOWERS)
        return false;
    return !!player.powers[num].isActive();
}

//
// E_GiveInventoryItem
//
// Place an artifact effect into the player's inventory, if it will fit.
//
bool E_GiveInventoryItem(player_t &player, const itemeffect_t *artifact, int amount, bool givemax)
{
    if(!artifact)
        return false;

    itemeffecttype_t  fxtype = artifact->getInt(keyClass, ITEMFX_NONE);
    inventoryitemid_t itemid = artifact->getInt(keyItemID, -1);

    // Not an artifact??
    if(fxtype != ITEMFX_ARTIFACT || itemid < 0)
        return false;

    inventoryindex_t newSlot      = -1;
    int              amountToGive = artifact->getInt(keyAmount, 1);
    int              maxAmount    = E_GetMaxAmountForArtifact(player, artifact);

    if(givemax)
        amountToGive = maxAmount;
    else if(amount > 0) // may override amount to give via parameter "amount", if > 0
        amountToGive = amount;

    // Does the player already have this item?
    inventoryslot_t       *slot     = E_InventorySlotForItemID(player, itemid);
    const inventoryslot_t *initslot = slot;

    // If not, make a slot for it
    if(!slot)
    {
        if((newSlot = E_findInventorySlot(player.inventory)) < 0)
            return false; // internal error, actually... shouldn't happen
        slot = &player.inventory[newSlot];
    }
    else if(slot->amount == maxAmount)
        return false;

    // If must collect full amount, but it won't fit, return now.
    if(artifact->getInt(keyFullAmountOnly, 0) && slot->amount + amountToGive > maxAmount)
        return false;

    // Make sure the player's inv_ptr is updated if need be
    if(!initslot && E_PlayerHasVisibleInvItem(player))
    {
        const itemeffect_t *other      = E_EffectForInventoryIndex(player, player.inv_ptr);
        const int           artiorder  = artifact->getInt(keySortOrder, 0);
        const int           otherorder = other->getInt(keySortOrder, 0);
        if(artiorder < otherorder || (artiorder == otherorder && strcasecmp(artifact->getKey(), other->getKey()) < 0))
            player.inv_ptr++;
    }

    // set the item type in case the slot is new, and increment the amount owned
    // by the amount this item gives, capping to the maximum allowed amount
    slot->item    = itemid;
    slot->amount += amountToGive;
    if(slot->amount > maxAmount)
        slot->amount = maxAmount;

    // sort if needed
    if(newSlot > 0)
        E_sortInventory(player, newSlot, artifact->getInt(keySortOrder, 0), artifact->getKey());

    return true;
}

//
// E_removeInventorySlot
//
// Remove a slot from the player's inventory.
//
static void E_removeInventorySlot(const player_t *player, inventoryslot_t *slot)
{
    inventory_t inventory = player->inventory;

    for(inventoryindex_t idx = 0; idx < e_maxitemid; idx++)
    {
        if(slot == &inventory[idx])
        {
            // shift everything down
            for(inventoryindex_t down = idx; down < e_maxitemid - 1; down++)
                inventory[down] = inventory[down + 1];

            // clear the top slot
            inventory[e_maxitemid - 1].item   = -1;
            inventory[e_maxitemid - 1].amount = 0;

            E_MoveInventoryCursor(*player, -1, const_cast<int &>(player->inv_ptr));

            return;
        }
    }
}

//
// E_RemoveInventoryItem
//
// Remove some amount of a specific item from the player's inventory, if
// possible. If amount is less than zero, then all of the item will be removed.
//
// If removemore is true, then if the amount is greater than what is actually
// in the inventory, everything will be removed. For compatibility reasons,
// this parameter is false by default.
//
itemremoved_e E_RemoveInventoryItem(player_t &player, const itemeffect_t *artifact, int amount, bool removemore)
{
    inventoryslot_t *slot = E_InventorySlotForItem(player, artifact);

    // don't have that item at all?
    if(!slot)
        return INV_NOTREMOVED;

    // a negative amount means to remove the full possessed amount
    if(amount < 0)
        amount = slot->amount;

    if(slot->amount < amount)
    {
        if(removemore)
            amount = slot->amount;
        else
            // don't own that many?
            return INV_NOTREMOVED;
    }

    itemremoved_e ret = INV_REMOVED;

    // subtract the amount requested
    slot->amount -= amount;

    // are they used up?
    if(slot->amount == 0)
    {
        // check for "keep depleted" flag to see if item stays even when we have
        // a zero amount of it.
        if(!artifact->getInt(keyKeepDepleted, 0))
        {
            // otherwise, we need to remove that item and collapse the player's
            // inventory
            E_removeInventorySlot(&player, slot);
            ret = INV_REMOVEDSLOT;
        }
    }

    // Select an empty weapon if player has no weapons left (without giving dummy weapon)
    E_PlayerHasAnyWeapons(player, false, true);

    return ret;
}

//
// E_InventoryEndHub
//
// At the end of a hub (or a level that is not part of a hub), call this
// function to strip all inventory items that are not meant to remain across
// levels to their max hub amount.
//
void E_InventoryEndHub(player_t *player)
{
    for(inventoryindex_t i = 0; i < e_maxitemid; i++)
    {
        int           amount = player->inventory[i].amount;
        itemeffect_t *item   = E_EffectForInventoryIndex(*player, i);

        if(item)
        {
            int interHubAmount = item->getInt(keyInterHubAmount, 0);

            // an interhubamount less than zero means no stripping occurs
            if(interHubAmount >= 0 && amount > interHubAmount)
            {
                auto code = E_RemoveInventoryItem(*player, item, amount - interHubAmount);
                if(code == INV_REMOVEDSLOT)
                    --i; // back up one slot, because the current one was removed
            }
        }
    }
}

//
// E_ClearInventory
//
// Completely clear a player's inventory.
// If undroppable is true, undroppable items will be kept.
// If setemptyweapon is true, the player will be given an empty weapon when done.
//
void E_ClearInventory(player_t *player, bool undroppable, bool setemptyweapon)
{
    invbarstate_t &invbarstate = player->invbarstate;

    for(inventoryindex_t i = 0; i < e_maxitemid; i++)
    {
        if(!undroppable)
        {
            itemeffect_t *item = E_EffectForInventoryIndex(*player, i);
            if(!(item &&
                 !(item->getInt(keyUndroppable, 0) && item->getInt(keyArtifactType, ARTI_NORMAL) != ARTI_WEAPON)))
                continue;
        }

        player->inventory[i].amount = 0;
        player->inventory[i].item   = -1;
        // Also unmorph inventory
        player->unmorphInventory[i].amount = 0;
        player->unmorphInventory[i].item   = -1;
    }

    player->inv_ptr = 0;
    invbarstate     = { false, 0 };

    // Select an empty weapon if player has no weapons left (without giving dummy weapon)
    if(setemptyweapon)
        E_PlayerHasAnyWeapons(*player, false, true);
}

//
// E_GetInventoryAllocSize
//
// Return the allocated size of a player's inventory in number of slots.
// This has nothing to do with the number of items owned.
//
int E_GetInventoryAllocSize()
{
    return e_maxitemid;
}

//
// Gets the actual health if special
//
int E_GetPClassHealth(const itemeffect_t &effect, size_t keyIndex, const playerclass_t &pclass, int def)
{
    const char *keyword = effect.getConstString(keyIndex, nullptr);
    if(keyword == eSpecialKeyword[E_SPECIAL_KEYWORD_MAXHEALTH])
        return pclass.maxhealth;
    if(keyword == eSpecialKeyword[E_SPECIAL_KEYWORD_SUPERHEALTH])
        return pclass.superhealth;
    return effect.getInt(keyIndex, def);
}
int E_GetPClassHealth(const itemeffect_t &effect, const char *key, const playerclass_t &pclass, int def)
{
    return E_GetPClassHealth(effect, MetaKeyIndex(key), pclass, def);
}

//
// Upon a polymorph, stash the current weapons to a separate inventory, since they can't be used by
// morphing class.
//
void E_StashOriginalMorphWeapons(player_t &player)
{
    size_t numWeapons = E_getNumWeaponItems();

    // WARNING: this clears existing unmorph inventory
    memset(player.unmorphInventory, 0, e_maxitemid * sizeof(inventoryslot_t));
    int pos = 0;

    for(size_t i = 0; i < numWeapons; ++i)
    {
        const itemeffect_t *effect = E_weaponItemForIndex(i);
        inventoryslot_t    *slot   = E_InventorySlotForItem(player, effect);

        if(!slot || slot->amount <= 0)
            continue;

        // Move the slot to the unmorph inventory
        player.unmorphInventory[pos++] = *slot;
        E_RemoveInventoryItem(player, effect, -1);
    }
}

//
// Unstash weapons
//
void E_UnstashWeaponsForUnmorphing(player_t &player)
{
    size_t numWeapons = E_getNumWeaponItems();

    // First remove all owned weapons (specific to morph class)
    for(size_t i = 0; i < numWeapons; ++i)
    {
        const itemeffect_t *effect = E_weaponItemForIndex(i);
        inventoryslot_t    *slot   = E_InventorySlotForItem(player, effect);

        if(!slot)
            continue;

        E_RemoveInventoryItem(player, effect, -1);
    }

    for(inventoryitemid_t i = 0; i < e_maxitemid; ++i)
    {
        const inventoryslot_t &slot = player.unmorphInventory[i];
        if(!slot.amount)
            break; // reached end

        E_GiveInventoryItem(player, E_EffectForInventoryItemID(slot.item));
    }
}

//
// Remove an item effect from the table.
//
void E_SafeDeleteItemEffect(itemeffect_t *effect)
{
    // Delete references from other collections
    // NOTE: this would also need to handle e_ammoTypesLookup and e_keysLookup
    for(size_t i = 0; i < e_weaponsLookup.getLength(); ++i)
        if(e_weaponsLookup[i] == effect)
        {
            e_weaponsLookup[i] = e_weaponsLookup.back();
            e_weaponsLookup.pop();
        }

    e_effectsTable.removeObject(effect);
    delete effect;
}

//=============================================================================
//
// Global Processing
//

//
// Process pickups that aren't embedded in things
//
void E_ProcessPickups(cfg_t *cfg)
{
    // process pickup effect bindings
    E_processPickupEffects(cfg);

    // process sprite pickup bindings
    E_processPickupItems(cfg);
}

//
// E_ProcessInventory
//
// Main global function for performing all inventory-related EDF processing.
//
void E_ProcessInventory(cfg_t *cfg)
{
    // process item effects
    E_processItemEffects(cfg);

    // generate weapon trackers (item effects)
    E_generateWeaponTrackers();

    // allocate inventory item IDs
    E_allocateInventoryItemIDs();

    // allocate sort orders to -invbar items
    E_allocateSortOrders();

    // allocate player inventories
    E_allocatePlayerInventories();

    // collect special artifact type definitions
    E_collectAmmoTypes();
    E_collectKeyItems();
    E_collectWeaponTypes();
    E_collectAutouseHealthItems();

    // process lockdefs
    E_processLockDefs(cfg);

    // TODO: MOAR?
}

// EOF

