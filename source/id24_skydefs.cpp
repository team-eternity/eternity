//
// The Eternity Engine
// Copyright (C) 2026 Ioan Chera
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
// Purpose: ID24 SKYDEFS format
// https://github.com/doom-cross-port-collab/id24/blob/main/version_0_99_2_md/SKYDEFS.md
//

#include "z_zone.h"
#include "id24_skydefs.h"

#include "d_main.h"
#include "i_system.h"
#include "id24_json.h"
#include "m_collection.h"
#include "w_wad.h"
#include "z_auto.h"

using json = nlohmann::json;

struct sky_t
{
    enum type_e
    {
        normal,
        fire,
        withForeground
    };

    struct fire_t
    {
        int   *palette;
        size_t palette_size;

        double updatetime;
    };

    struct tex_t
    {
        char  *name;
        double mid;
        double scrollx;
        double scrolly;
        double scalex;
        double scaley;
    };

    type_e type;
    tex_t  tex;
    fire_t fire;
    tex_t  foregroundtex;
};

struct flatmap_t
{
    char *flat;
    char *sky;
};

namespace id24
{

static PODCollection<sky_t>     skies;
static PODCollection<flatmap_t> flatmapping;

static jsonLumpResult_e parseFunc(const nlohmann::json &data, void *context, jsonWarning_t warningFunc)
{
    if(data.contains("skies") && data["skies"].is_array())
    {
        const json &jskies = data["skies"];
        for(const json &jsky : jskies)
        {
            sky_t sky = {};
            if(!jsky.contains("type") || !jsky["type"].is_number_integer())
            {
                warningFunc(true, "Missing or invalid 'type' integer in sky");
                continue;
            }
            int type = jsky["type"].get<int>();
            if(type != 0 && type != 1 && type != 2)
            {
                warningFunc(true, "Invalid type value in sky");
                continue;
            }
            sky.type = static_cast<sky_t::type_e>(type);
            if(!jsky.contains("name") || !jsky["name"].is_string())
            {
                warningFunc(true, "Missing or invalid 'name' string in sky");
                continue;
            }
            sky.tex.name = estrdup(jsky["name"].get<std::string>().c_str());

            if(!jsky.contains("mid") || !jsky["mid"].is_number())
            {
                warningFunc(false, "Missing or invalid 'mid' string in sky, assuming 100");
                // stays zero
            }
            else
            {
                sky.tex.mid = jsky["mid"].get<double>();
            }
        }
    }
    return JLR_OK;
}

static void warningFunc(bool error, const char *msg)
{
    // TODO: check if critical and quit
    if(error)
        usermsg("SKYDEFS ERROR: %s\n", msg);
    else
        usermsg("SKYDEFS: %s", msg);
}

void LoadSkyDefs()
{
    int num = wGlobalDir.checkNumForName("SKYDEFS");
    if(num < 0)
        return;

    ZAutoBuffer buf;
    wGlobalDir.cacheLumpAuto("SKYDEFS", buf);
    if(!buf.get())
        return;

    usermsg("Loading SKYDEFS...");

    jsonLumpResult_e result =
        ParseJSONLump(buf.get(), buf.getSize(), "skydefs", { 1, 0, 0 }, parseFunc, nullptr, warningFunc);

    // TODO: populate
}
} // namespace id24

// EOF