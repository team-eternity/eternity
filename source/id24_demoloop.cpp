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
// Purpose: ID24 DEMOLOOP format
// https://docs.google.com/document/d/1WzsUc1EpYd4HX7Nrs7gjy2ZO3cAc62vq8TKNqbWtF7M
//

#include "z_zone.h"
#include "id24_demoloop.h"

#include "d_gi.h"
#include "d_main.h"
#include "id24_json.h"
#include "i_system.h"
#include "m_collection.h"
#include "w_wad.h"
#include "z_auto.h"

using json = nlohmann::json;

namespace id24
{

static PODCollection<demostate_t> userDemoStates;

static jsonLumpResult_e parseFunc(const nlohmann::json &data, void *context, jsonWarning_t warningFunc)
{
    auto states = static_cast<PODCollection<demostate_t> *>(context);

    auto cleanStates = [&states]() {
        for(demostate_t &state : *states)
        {
            efree(const_cast<char *>(state.lumpname));
            efree(const_cast<char *>(state.musicname));
        }
    };

    if(!data.contains("entries") || !data["entries"].is_array())
    {
        warningFunc(true, "Missing or invalid 'entries' array");
        return JLR_INVALID;
    }

    const json &entries = data["entries"];
    for(const json &entry : entries)
    {
        demostate_t state = {};

        if(!entry.contains("type") || !entry["type"].is_number())
        {
            warningFunc(false, "Missing or invalid 'type' string in entry.");
            continue;
        }
        int type = entry["type"].get<int>();
        if(type != 0 && type != 1)
        {
            warningFunc(false, "Invalid 'type' value in entry, it can only be 0 or 1.");
            continue;
        }

        if(type == 1)
            state.flags |= DSF_DEMO;

        if(!entry.contains("primarylump") || !entry["primarylump"].is_string())
        {
            warningFunc(false, "Missing or invalid 'primarylump' string in entry.");
            if(type == 1) // art screens can have null screen in Eternity, which will show the EE credits
                continue;
        }
        state.lumpname = estrdup(entry["primarylump"].get<std::string>().c_str());

        if(type == 0) // art screen
        {
            if(entry.contains("secondarylump"))
            {
                const json &secondarylump = entry["secondarylump"];
                if(!secondarylump.is_string())
                    warningFunc(false, "Invalid 'secondarylump' string in art page entry.");
                else
                    state.musicname = estrdup(secondarylump.get<std::string>().c_str());
            }

            if(!entry.contains("duration") || !entry["duration"].is_number())
            {
                warningFunc(false, "Missing or invalid 'duration' number in demo entry.");
                state.tics = -1; // safe default
            }
            else
                state.tics = entry["duration"].get<double>() * (double)TICRATE;
        }

        if(entry.contains("outrowipe") && entry["outrowipe"].is_number())
        {
            int outrowipe = entry["outrowipe"].get<int>();
            if(outrowipe == 1)
                state.flags |= DSF_ENDWIPE;
            else if(outrowipe != 0)
            {
                warningFunc(false, "Invalid 'outrowipe' value in demo entry, it can only be 0 or 1.");
                state.flags |= DSF_ENDWIPE; // default to wipe
            }
        }
        else
        {
            warningFunc(false, "Missing or invalid 'outrowipe' number in demo entry, assuming wipe.");
            state.flags |= DSF_ENDWIPE;
        }

        states->add(state);
    }

    if(states->isEmpty())
    {
        warningFunc(true, "Empty demo loop.");
		cleanStates();
        return JLR_INVALID;
    }

    return JLR_OK;
}

static void warningFunc(bool error, const char *msg)
{
    if(error)
        usermsg("DEMOLOOP ERROR: %s", msg); // do not I_Error for demo loops, as it's not critical
    else
        usermsg("DEMOLOOP: %s", msg);
}

void LoadDemoLoop()
{
    int num = wGlobalDir.checkNumForName("DEMOLOOP");
    if(num < 0)
        return;

    ZAutoBuffer buf;
    wGlobalDir.cacheLumpAuto("DEMOLOOP", buf);
    if(!buf.get())
        return;

    usermsg("Loading DEMOLOOP...");
    PODCollection<demostate_t> states;

    jsonLumpResult_e result =
        ParseJSONLump(buf.get(), buf.getSize(), "demoloop", { 1, 0, 0 }, parseFunc, &states, warningFunc);

    if(result == JLR_OK)
    {
        demostate_t endstate = {};
        endstate.flags       = DSF_END;
        states.add(endstate); // terminator

        userDemoStates = std::move(states);
        usermsg("Loaded DEMOLOOP with %zu entries.", userDemoStates.getLength() - 1);

        // Override default demo states
        GameModeInfo->demoStates = &userDemoStates[0];
    }
    else
    {
        usermsg("Rejected DEMOLOOP lump.");
    }
}
} // namespace id24

// EOF
