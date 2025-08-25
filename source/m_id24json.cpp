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
// Purpose: ID24 JSON format
// https://docs.google.com/document/d/1SGWMFggsARYKWRsSHm_7BPMrGCEcm6t6DCpWiQyfjTY/
//

#include "z_zone.h"
#include "m_id24json.h"

#include "w_wad.h"
#include "z_auto.h"

#include "nlohmann/json.hpp"

using json = nlohmann::json;

static bool M_parseJSONVersion(const char *versionString, JSONLumpVersion &version)
{
    size_t len = strlen(versionString);
    if(len < 5)
        return false;
    version = {};
    char *endptr;
    long  value = strtol(versionString, &endptr, 10);
    if(endptr == versionString || *endptr != '.' || value < 0)
        return false;
    version.major = static_cast<int>(value);
    versionString = endptr + 1;
    value         = strtol(versionString, &endptr, 10);
    if(endptr == versionString || *endptr != '.' || value < 0)
        return false;
    version.minor = static_cast<int>(value);
    versionString = endptr + 1;
    value         = strtol(versionString, &endptr, 10);
    if(endptr == versionString || *endptr != '\0' || value < 0)
        return false;
    version.revision = static_cast<int>(value);
    return true;
}

static bool M_validateType(const char *type)
{
    for(const char *pc = type; *pc; pc++)
    {
        char c = *pc;
        if(c >= 'a' && c <= 'z')
            continue;
        if(c >= '0' && c <= '9')
            continue;
        if(c == '_')
            continue;
        return false;
    }
    return true;
}

jsonlumpresult_e M_ParseJSONLump(const WadDirectory &dir, const char *lumpname, const char *lumptype,
                                 const JSONLumpVersion &maxversion)
{
    int lumpnum = dir.checkNumForName(lumpname);
    if(lumpnum < 0)
        return JLR_NO_LUMP;

    ZAutoBuffer jsonData;
    dir.cacheLumpAuto(lumpnum, jsonData);

    json root;
    try
    {
        root = json::parse(jsonData.get());
    }
    catch(const json::parse_error &)
    {
        return JLR_INVALID;
    }

    if(!root.is_object())
        return JLR_INVALID;

    if(root.size() != 4 || !root.contains("type") || !root["type"].is_string() || !root.contains("version") ||
       !root["version"].is_string() || !root.contains("metadata") || !root["metadata"].is_object() ||
       !root.contains("data") || !root["data"].is_object())
    {
        return JLR_INVALID;
    }

    JSONLumpVersion version;
    if(!M_parseJSONVersion(root["version"].get<std::string>().c_str(), version))
        return JLR_INVALID;

    if(version.major > maxversion.major || (version.major == maxversion.major && version.minor > maxversion.minor) ||
       (version.major == maxversion.major && version.minor == maxversion.minor &&
        version.revision > maxversion.revision))
    {
        return JLR_UNSUPPORTED_VERSION;
    }

    std::string type = root["type"].get<std::string>();
    if(!M_validateType(type.c_str()))
        return JLR_INVALID;

    if(strcmp(root["type"].get<std::string>().c_str(), lumptype))
        return JLR_INVALID;

    json metadata = root["metadata"];
    if(!metadata.contains("author") || !metadata["author"].is_string() ||
       !metadata.contains("timestamp") || !metadata["timestamp"].is_string() ||
       !metadata.contains("application") || !metadata["application"].is_string())
    {
        return JLR_INVALID;
    }

    return JLR_OK;
}
