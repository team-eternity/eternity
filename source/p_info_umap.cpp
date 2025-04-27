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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: UMAPINFO lump processing.
// Authors: Ioan Chera
//

#include <assert.h>
#include "z_zone.h"

#include "c_io.h"
#include "d_gi.h"
#include "doomstat.h"
#include "e_things.h"
#include "ev_actions.h"
#include "ev_specials.h"
#include "f_finale.h"
#include "i_system.h"
#include "m_qstr.h"
#include "m_utils.h"
#include "metaapi.h"
#include "p_info.h"
#include "p_spec.h"
#include "v_misc.h"
#include "xl_umapinfo.h"

constexpr const char DEH_ACTOR_PREFIX[] = "deh_actor_";

enum
{
    MIN_UMAPINFO_DEH_ACTOR = 145,
    MAX_UMAPINFO_DEH_ACTOR = 249,
};

//
// UMAPINFO has Deh_Actor_* names for DSDHacked compatibility.
//
static mobjtype_t P_getUMapInfoThingType(const char *name)
{
    if(!strncasecmp(name, DEH_ACTOR_PREFIX, sizeof(DEH_ACTOR_PREFIX) - 1))
    {
        char *endptr = nullptr;
        int   index  = (int)strtol(name + sizeof(DEH_ACTOR_PREFIX) - 1, &endptr, 10);
        if(index >= MIN_UMAPINFO_DEH_ACTOR && index <= MAX_UMAPINFO_DEH_ACTOR && !*endptr)
            return E_ThingNumForDEHNum(index + 1); // yeah, dehackednum is from 146 to 250.
    }
    return E_ThingNumForCompatName(name);
}

//
// Process the bossaction part of UMAPINFO. Moved here due to complexity.
//
static bool P_processUMapInfoBossActions(MetaTable *info, qstring *error)
{

    // The presence of a bossaction = clear line must clear any built-in (vanilla) boss actions
    int val;
    val = info->getInt("bossaction", XL_UMAPINFO_SPECVAL_NOT_SET);
    if(val == XL_UMAPINFO_SPECVAL_CLEAR)
        LevelInfo.bossSpecs = 0;

    struct bossaction_t
    {
        const char *mobjclass;
        int         special;
        int         tag;
    };

    PODCollection<bossaction_t> actions;
    MetaObject                 *bossentry = nullptr;
    while((bossentry = info->getNextObject(bossentry, "bossaction")))
    {
        auto bosstable = runtime_cast<MetaTable *>(bossentry);
        if(!bosstable) // hit a "clear" entry
            break;
        edefstructvar(bossaction_t, bossinfo);
        bossinfo.mobjclass = bosstable->getString("thingtype", nullptr);
        assert(bossinfo.mobjclass);
        bossinfo.special = bosstable->getInt("linespecial", XL_UMAPINFO_SPECVAL_NOT_SET);
        assert(bossinfo.special != XL_UMAPINFO_SPECVAL_NOT_SET);
        bossinfo.tag = bosstable->getInt("tag", XL_UMAPINFO_SPECVAL_NOT_SET);
        assert(bossinfo.tag != XL_UMAPINFO_SPECVAL_NOT_SET);
        actions.add(bossinfo);
    }

    // Visit the list in reverse
    for(int i = (int)actions.getLength() - 1; i >= 0; --i)
    {
        const bossaction_t &action = actions[i];

        mobjtype_t type = P_getUMapInfoThingType(action.mobjclass);
        if(type == -1)
        {
            if(error)
                *error = qstring("UMAPINFO: invalid bossaction thingtype '") + action.mobjclass + "'";
            return false;
        }

        // Check the binding here

        // Ban parameterized special
        if(EV_IsParamLineSpec(action.special))
        {
            C_Printf(FC_ERROR "UMAPINFO: skipping illegal parameterized special %d from %s on tag "
                              "%d\n",
                     action.special, action.mobjclass, action.tag);
            continue;
        }

        // Block shootable specials
        const char        *skipname = nullptr;
        const ev_action_t *evaction = EV_DOOMActionForSpecial(action.special);
        if(EV_CheckGenSpecialSpac(action.special, SPAC_IMPACT))
            skipname = "generalized";
        else
        {
            // UMAPINFO uses the DOOM number space.
            if(evaction && EV_CheckActionIntrinsicSpac(*evaction, SPAC_IMPACT))
            {
                skipname = evaction->name;
                assert(skipname); // by how the code is written, we must be having the name always.
            }
        }
        if(skipname)
        {
            C_Printf(FC_ERROR "UMAPINFO: skipping bossaction %s from %s to tag %d because gunshot "
                              "types are not allowed\n",
                     skipname, action.mobjclass, action.tag);
            continue;
        }

        // Block teleport lines
        if(evaction)
        {
            EVActionFunc func = evaction->action;
            if(func == EV_ActionSilentLineTeleport || func == EV_ActionSilentLineTeleportReverse ||
               func == EV_ActionSilentTeleport || func == EV_ActionTeleport)
            {
                C_Printf(FC_ERROR "UMAPINFO: skipping bossaction %s from %s to tag %d because "
                                  "teleport types are not allowed\n",
                         evaction->name, action.mobjclass, action.tag);
                continue;
            }
        }

        // Block locked types
        // NOTE: the Dx generalized specials are also skipped because we have no line
        if(EV_GenTypeForSpecial(action.special) == GenTypeLocked ||
           (evaction && (evaction->action == EV_ActionVerticalDoor || evaction->action == EV_ActionDoLockedDoor)))
        {
            C_Printf(FC_ERROR "UMAPINFO: skipping bossaction %s from %s to tag %d because locked "
                              "types or manual doors are not allowed\n",
                     evaction ? evaction->name : "generalized", action.mobjclass, action.tag);
            continue;
        }

        // UMAPINFO specs state that the vanilla preset specs shall be cleared.
        LevelInfo.bossSpecs = 0;

        // Now we may have it
        auto bossaction       = estructalloctag(levelaction_t, 1, PU_LEVEL);
        bossaction->special   = action.special;
        bossaction->mobjtype  = type;
        bossaction->args[0]   = action.tag;
        bossaction->next      = LevelInfo.actions;
        bossaction->flags    |= levelaction_t::BOSS_ONLY | levelaction_t::CLASSIC_SPECIAL;
        LevelInfo.actions     = bossaction;
    }

    return true;
}

//
// Process UMAPINFO obtained data.
//
// Source: https://github.com/coelckers/prboom-plus/blob/master/prboom2/doc/umapinfo.txt
//
bool P_ProcessUMapInfo(MetaTable *info, const char *mapname, qstring *error)
{
    I_Assert(info, "No info provided!\n");
    I_Assert(mapname, "Null mapname provided!\n");

    // Mark if classic demos to avoid changing certain playsim settings
    bool classicDemo = demo_version <= 203;

    int         val;
    const char *strval;

    if(!classicDemo)
    {
        // Setup the next levels here. According to UMAPINFO, by default _both_ properties mean the
        // next map. Only do it for standard IWAD lump names
        int ep, lev;
        if(M_IsExMy(info->getKey(), &ep, &lev))
        {
            qstring nextname;
            nextname.Printf(8, "E%dM%d", ep, lev + 1);
            LevelInfo.nextLevel = LevelInfo.nextSecret = nextname.duplicate(PU_LEVEL);
        }
        else if(M_IsMAPxy(info->getKey(), &lev))
        {
            qstring nextname;
            nextname.Printf(8, "MAP%02d", lev + 1);
            LevelInfo.nextLevel = LevelInfo.nextSecret = nextname.duplicate(PU_LEVEL);
        }
    }

    strval = info->getString("levelname", nullptr);
    if(strval)
    {
        // Must always get the last object
        MetaObject *label = info->getObject("label");
        qstring     fullname;

        if(label)
        {
            auto labelString = runtime_cast<MetaString *>(label);
            if(labelString)
            {
                fullname  = labelString->getValue();
                fullname += ": ";
                if(GameModeInfo->type == Game_Heretic)
                    fullname += " "; // Heretic has two spaces after label name
                fullname += strval;
            }
            else // int: clear
                fullname = strval;
        }
        else // unmentioned
        {
            fullname  = mapname;
            fullname += ": ";
            if(GameModeInfo->type == Game_Heretic)
                fullname += " "; // Heretic has two spaces after label name
            fullname += strval;
        }

        LevelInfo.levelName = LevelInfo.interLevelName = fullname.duplicate(PU_LEVEL);
        LevelInfo.interLevelName                       = strval; // just the inter level name
    }
    strval = info->getString("levelpic", nullptr);
    if(strval)
        LevelInfo.levelPic = strval; // allocation is permanent in the UMAPINFO table
    if(!classicDemo)
    {
        strval = info->getString("next", nullptr);
        if(strval)
            LevelInfo.nextLevel = strval;
        strval = info->getString("nextsecret", nullptr);
        if(strval)
            LevelInfo.nextSecret = strval;
    }
    strval = info->getString("skytexture", nullptr);
    if(strval)
        LevelInfo.skyName = strval; // UMAPINFO is in PrBoom+UM, so don't disable the sky compat
    strval = info->getString("music", nullptr);
    if(strval)
        LevelInfo.musicName = strval;
    strval = info->getString("exitpic", nullptr);
    if(strval)
        LevelInfo.interPic = strval; // NOTE: enterpic not used here
    if(!classicDemo)
    {
        val = info->getInt("partime", XL_UMAPINFO_SPECVAL_NOT_SET);
        if(val != XL_UMAPINFO_SPECVAL_NOT_SET)
            LevelInfo.partime = val;

        // If we have the finale-secret-only flag, transfer whatever's in basic inter-text to
        // inter-text-secret, so that we replace the correct field afterwards
        if(LevelInfo.finaleSecretOnly && !LevelInfo.interTextSecret && LevelInfo.interText)
        {
            LevelInfo.interTextSecret = LevelInfo.interText;
            LevelInfo.interText       = nullptr;
        }
        else if(!LevelInfo.finaleNormalOnly && LevelInfo.interText)
            LevelInfo.interTextSecret = LevelInfo.interText; // also transfer if distributed
        else if(LevelInfo.finaleNormalOnly)                  // cleanup the other states
            LevelInfo.interTextSecret = nullptr;

        //
        // Needed to copy the pointer to the secret-exit story to the normal-exit one
        //
        auto reuseSecretStoryForMainExit = []() {
            if(LevelInfo.finaleSecretOnly && LevelInfo.interTextSecret && !LevelInfo.interText)
                LevelInfo.interText = LevelInfo.interTextSecret;
            LevelInfo.finaleSecretOnly = false;
        };

        val = info->getInt("endgame", XL_UMAPINFO_SPECVAL_NOT_SET);
        if(val == XL_UMAPINFO_SPECVAL_FALSE)
        {
            // Override a default map exit
            // Default map exits are defined in P_InfoDefaultFinale according to game mode info
            LevelInfo.finaleType  = FINALE_TEXT;
            LevelInfo.endOfGame   = false;
            LevelInfo.finaleEarly = false;
        }
        else if(val == XL_UMAPINFO_SPECVAL_TRUE)
        {
            const finalerule_t *rule = P_DetermineEpisodeFinaleRule(false);

            // Do not also change the finale-early flag, so that it's like PrBoom+um
            // HACK to maximize PrBoom+um compatibility, avoid
            P_SetFinaleFromRule(rule, false, GameModeInfo->id != commercial);

            // Also generalize this (and same below) for both exits. The presence of the respective
            // intertext will dictate whether it will really happen.
            reuseSecretStoryForMainExit();
            P_EnsureDefaultStoryText(false);
        }
        strval = info->getString("endpic", nullptr);
        if(strval)
        {
            // Now also update the ending type
            LevelInfo.endOfGame  = false;
            LevelInfo.finaleType = FINALE_END_PIC;
            LevelInfo.endPic     = strval;

            reuseSecretStoryForMainExit();
            P_EnsureDefaultStoryText(false);
        }
        val = info->getInt("endbunny", XL_UMAPINFO_SPECVAL_NOT_SET);
        if(val == XL_UMAPINFO_SPECVAL_TRUE)
        {
            LevelInfo.endOfGame  = false;
            LevelInfo.finaleType = FINALE_DOOM_BUNNY;
            reuseSecretStoryForMainExit();
            P_EnsureDefaultStoryText(false);
        }
        val = info->getInt("endcast", XL_UMAPINFO_SPECVAL_NOT_SET);
        if(val == XL_UMAPINFO_SPECVAL_TRUE)
        {
            LevelInfo.endOfGame  = true;
            LevelInfo.finaleType = FINALE_TEXT;
            reuseSecretStoryForMainExit();
            P_EnsureDefaultStoryText(false);
        }

        // NOTE: according to specs and PrBoom+um, nointermission only affects finale-early
        val = info->getInt("nointermission", XL_UMAPINFO_SPECVAL_NOT_SET);
        if(val == XL_UMAPINFO_SPECVAL_TRUE)
            LevelInfo.finaleEarly = true;
        else if(val == XL_UMAPINFO_SPECVAL_FALSE)
            LevelInfo.finaleEarly = false;

        const char *intertextkeys[]                  = { "intertext", "intertextsecret" };
        const char *LevelInfo_t::*intertexttargets[] = { &LevelInfo_t::interText, &LevelInfo_t::interTextSecret };

        for(int i = 0; i < 2; ++i)
        {
            val = info->getInt(intertextkeys[i], XL_UMAPINFO_SPECVAL_NOT_SET);
            if(val == XL_UMAPINFO_SPECVAL_CLEAR)
                LevelInfo.*intertexttargets[i] = nullptr;
            else
            {
                // Populate "intertext"
                const MetaString *metastr = nullptr;
                qstring           intertext;
                while((metastr = info->getNextKeyAndTypeEx(metastr, intertextkeys[i])))
                {
                    // MetaTables are stored in reverse order
                    if(!intertext.empty())
                        intertext = qstring(metastr->getValue()) + "\n" + intertext;
                    else
                        intertext = metastr->getValue();
                }
                if(!intertext.empty())
                    LevelInfo.*intertexttargets[i] = intertext.duplicate(PU_LEVEL);
            }
        }

        // Must clear what's not there.
        if(LevelInfo.interText && !LevelInfo.interTextSecret)
            LevelInfo.finaleNormalOnly = true;
        else if(!LevelInfo.interText && LevelInfo.interTextSecret)
            LevelInfo.finaleSecretOnly = true;
    }

    strval = info->getString("interbackdrop", nullptr);
    if(strval)
        LevelInfo.backDrop = strval;

    strval = info->getString("intermusic", nullptr);
    if(strval)
        LevelInfo.interMusic = strval;

    // boss specials (don't process them in classic demos)
    return classicDemo ? true : P_processUMapInfoBossActions(info, error);
}

// EOF
