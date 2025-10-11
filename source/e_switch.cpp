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
// Purpose: EDF switch definitions.
// Authors: Ioan Chera
//

#include "z_zone.h"

#include "e_edf.h"
#include "e_hash.h"
#include "e_switch.h"
#include "m_qstrkeys.h"

constexpr const char ITEM_SWITCH_ONPIC[]     = "on";
constexpr const char ITEM_SWITCH_ONSOUND[]   = "sound";
constexpr const char ITEM_SWITCH_OFFSOUND[]  = "offsound";
constexpr const char ITEM_SWITCH_GAMEINDEX[] = "gameindex";

constexpr int NUMSWITCHCHAINS = 67;

// clang-format off

//
// The EDF switch property
//
cfg_opt_t edf_switch_opts[] = {
    CFG_STR(ITEM_SWITCH_ONPIC,     "", CFGF_NONE),
    CFG_STR(ITEM_SWITCH_ONSOUND,   "", CFGF_NONE),
    CFG_STR(ITEM_SWITCH_OFFSOUND,  "", CFGF_NONE),
    CFG_INT(ITEM_SWITCH_GAMEINDEX, 1,  CFGF_NONE),
    CFG_END()
};

// clang-format on

// the switches defined (order important)
PODCollection<ESwitchDef *> eswitches;

// Put them in a hash. They'll be stored in a list later.
static EHashTable<ESwitchDef, ENCQStrHashKey, &ESwitchDef::offpic, &ESwitchDef::link>
    e_switch_namehash(NUMSWITCHCHAINS);

//
// Resets a switch definition. Doesn't touch lists.
//
void ESwitchDef::reset()
{
    offpic.clear();
    onpic.clear();
    onsound.clear();
    offsound.clear();
    episode = 0;
}

//
// Process an individual switch
//
static void E_processSwitch(cfg_t *cfg)
{
    const char *title    = cfg_title(cfg);
    ESwitchDef *def      = e_switch_namehash.objectForKey(title);
    const bool  modified = !!def;
    if(!def)
    {
        def         = new ESwitchDef;
        def->offpic = title;
        e_switch_namehash.addObject(def);
        eswitches.add(def);
        E_EDFLogPrintf("\t\tDefined switch %s\n", title);
    }
    else
        E_EDFLogPrintf("\t\tModified switch %s\n", title);

    auto isset = [modified, cfg](const char *field) { return !modified || cfg_size(cfg, field) > 0; };

    if(isset(ITEM_SWITCH_ONPIC))
        def->onpic = cfg_getstr(cfg, ITEM_SWITCH_ONPIC);
    if(isset(ITEM_SWITCH_ONSOUND))
        def->onsound = cfg_getstr(cfg, ITEM_SWITCH_ONSOUND);
    if(isset(ITEM_SWITCH_OFFSOUND))
        def->offsound = cfg_getstr(cfg, ITEM_SWITCH_OFFSOUND);
    if(isset(ITEM_SWITCH_GAMEINDEX))
        def->episode = cfg_getint(cfg, ITEM_SWITCH_GAMEINDEX);
}

//
// Processes "switch" blocks
//
void E_ProcessSwitches(cfg_t *cfg)
{
    unsigned numswitches = cfg_size(cfg, EDF_SEC_SWITCH);

    E_EDFLogPrintf("\t* Processing switches\n"
                   "\t\t%u switch(es) defined\n",
                   numswitches);

    for(unsigned i = 0; i < numswitches; ++i)
        E_processSwitch(cfg_getnsec(cfg, EDF_SEC_SWITCH, i));
}

//
// Adds a switch defined externally
//
void E_AddSwitchDef(const ESwitchDef &extdef)
{
    if(extdef.offpic.empty())
        return;
    const char *title = extdef.offpic.constPtr();
    ESwitchDef *def   = e_switch_namehash.objectForKey(title);

    // NOTE: by external means, switches can't be modified. EDF takes priority.
    if(def)
        return;

    def = new ESwitchDef(extdef);
    e_switch_namehash.addObject(def);
    eswitches.add(def);
    E_EDFLogPrintf("\t\tDefined switch %s from ANIMDEFS\n", title);
}

//
// Returns the switch based on name
//
const ESwitchDef *E_SwitchForName(const char *name)
{
    return e_switch_namehash.objectForKey(name);
}

// EOF

