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
#include "m_qstr.h"
#include "w_wad.h"
#include "z_auto.h"

using json = nlohmann::json;

struct sky_t
{
    enum type_e
    {
        normal,
        fireType,
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

class SkyDefs
{
public:
    PODCollection<sky_t>     skies;
    PODCollection<flatmap_t> flatmapping;
};

namespace id24
{

static PODCollection<sky_t>     skies;
static PODCollection<flatmap_t> flatmapping;

static void populateTexture(sky_t::tex_t &tex, const json &jsky)
{
    tex.name    = estrdup(GetString(jsky, "name").value().constPtr());
    tex.mid     = GetDouble(jsky, "mid").value();
    tex.scrollx = GetDouble(jsky, "scrollx").value();
    tex.scrolly = GetDouble(jsky, "scrolly").value();
    tex.scalex  = GetDouble(jsky, "scalex").value();
    tex.scaley  = GetDouble(jsky, "scaley").value();
}

static jsonLumpResult_e parseFunc(const json &data, void *context, jsonWarning_t warningFunc)
{
    auto skydefs = static_cast<SkyDefs *>(context);
    try
    {
        const json *jskies = GetArray(data, "skies");
        if(jskies)
        {
            if(jskies->empty())
            {
                warningFunc(true, "Invalid empty skies array in SKYDEFS");
                return JLR_INVALID;
            }
            for(const json &jsky : *jskies)
            {
                sky_t sky = {};
                sky.type  = static_cast<sky_t::type_e>(GetInt(jsky, "type").value());
                switch(sky.type)
                {
                case sky_t::normal:
                case sky_t::fireType:
                case sky_t::withForeground: break;
                default:                    warningFunc(true, "Invalid type in SKYDEFS"); return JLR_INVALID;
                }
                populateTexture(sky.tex, jsky);
                if(sky.type == sky_t::fireType)
                {
                    const json *jfire = GetObject(jsky, "fire");
                    if(!jfire)
                    {
                        warningFunc(true, "Missing 'fire' for expected fire");
                        return JLR_INVALID;
                    }
                    const json *jpalette = GetArray(*jfire, "palette");
                    if(!jpalette || !jpalette->size())
                    {
                        warningFunc(true, "Missing 'palette' in fire");
                        return JLR_INVALID;
                    }
                    sky.fire.palette_size = jpalette->size();
                    sky.fire.palette      = emalloc(int *, sky.fire.palette_size * sizeof(int));
                    int i                 = 0;
                    for(const json &jpaletteEntry : *jpalette)
                        sky.fire.palette[i++] = jpaletteEntry.get<int>();
                    sky.fire.updatetime = GetDouble(*jfire, "updatetime").value();
                    if(sky.fire.updatetime <= 0)
                        warningFunc(true, "Invalid zero or negative fire updatetime");
                }
                else if(sky.type == sky_t::withForeground)
                {
                    const json *jforegroundtex = GetObject(jsky, "foregroundtex");
                    if(!jforegroundtex)
                    {
                        warningFunc(true, "Missing 'foregroundtex' for expected foreground");
                        continue;
                    }
                    populateTexture(sky.foregroundtex, *jforegroundtex);
                }
                skydefs->skies.add(sky);
            }
        }
        const json *jflatmapping = GetArray(data, "flatmapping");
        if(jflatmapping)
        {
            if(jflatmapping->empty())
            {
                warningFunc(true, "Invalid empty flatmapping array in SKYDEFS");
                return JLR_INVALID;
            }

            for(const json &jflatmap : *jflatmapping)
            {
                flatmap_t flatmap = {};
                flatmap.flat      = estrdup(GetString(jflatmap, "flat").value().constPtr());
                flatmap.sky       = estrdup(GetString(jflatmap, "sky").value().constPtr());
                skydefs->flatmapping.add(flatmap);
            }
        }
    }
    catch(const std::bad_optional_access &e)
    {
        warningFunc(true, (qstring("Error in SKYDEFS: ") + e.what()).constPtr());
        return JLR_INVALID;
    }

    return JLR_OK;
}

static void warningFunc(bool error, const char *msg)
{
    // TODO: check if critical and quit
    if(error)
        I_Error("SKYDEFS ERROR: %s\n", msg);
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

    SkyDefs skydefs;

    jsonLumpResult_e result =
        ParseJSONLump(buf.get(), buf.getSize(), "skydefs", { 1, 0, 0 }, parseFunc, &skydefs, warningFunc);

    if(result == JLR_OK)
    {
        // ok
    }
}
} // namespace id24

// EOF
