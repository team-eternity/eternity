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

#include "m_qstr.h"
#include "w_wad.h"
#include "z_auto.h"

#include "nlohmann/json.hpp"
#include <regex>

using json = nlohmann::json;

enum
{
    ERRLEN = 128,
};

static bool M_parseJSONVersion(const char *versionString, JSONLumpVersion &version)
{
    std::regex  pattern(R"(^(\d+)\.(\d+)\.(\d+)$)");
    std::cmatch match;
    if(!std::regex_match(versionString, match, pattern))
        return false;
    version          = {};
    version.major    = std::stoi(match[1].str(), nullptr, 10);
    version.minor    = std::stoi(match[2].str(), nullptr, 10);
    version.revision = std::stoi(match[3].str(), nullptr, 10);
    return true;
}

static bool M_validateType(const char *type)
{
    std::regex pattern(R"(^[a-z0-9_-]+$)");
    return std::regex_match(type, pattern);
}

bool JSONLumpVersion::operator>(const JSONLumpVersion &other) const
{
    if(major != other.major)
        return major > other.major;
    if(minor != other.minor)
        return minor > other.minor;
    return revision > other.revision;
}

jsonlumpresult_e M_ParseJSONLump(const WadDirectory &dir, const char *lumpname, const char *lumptype,
                                 const JSONLumpVersion &maxversion, qstring &error)
{
    int lumpnum = dir.checkNumForName(lumpname);
    if(lumpnum < 0)
        return JLR_NO_LUMP;

    ZAutoBuffer jsonData;
    dir.cacheLumpAuto(lumpnum, jsonData);

    json root;
    try
    {
        auto strData = static_cast<const char *>(jsonData.get());
        root         = json::parse(strData, strData + jsonData.getSize());
    }
    catch(const json::parse_error &e)
    {
        error.Printf(ERRLEN, "%s (%s) parse error at byte %zu: %s", lumpname, lumptype, e.byte, e.what());
        return JLR_INVALID;
    }

    if(!root.is_object())
        return JLR_INVALID;

    if(root.size() != 4 || !root.contains("type") || !root["type"].is_string() || !root.contains("version") ||
       !root["version"].is_string() || !root.contains("metadata") || !root["metadata"].is_object() ||
       !root.contains("data") || !root["data"].is_object())
    {
        error.Printf(ERRLEN, "%s (%s) is missing required root fields", lumpname, lumptype);
        return JLR_INVALID;
    }

    JSONLumpVersion version;
    std::string     versionStr = root["version"].get<std::string>();
    if(!M_parseJSONVersion(versionStr.c_str(), version))
    {
        error.Printf(ERRLEN, "%s (%s) has an invalid version string '%s'", lumpname, lumptype, versionStr.c_str());
        return JLR_INVALID;
    }

    if(version > maxversion)
    {
        error.Printf(ERRLEN, "%s (%s) version %s is unsupported (max %d.%d.%d)", lumpname, lumptype, versionStr.c_str(),
                     maxversion.major, maxversion.minor, maxversion.revision);
        return JLR_UNSUPPORTED_VERSION;
    }

    std::string typeStr = root["type"].get<std::string>();
    if(!M_validateType(typeStr.c_str()))
    {
        error.Printf(ERRLEN, "%s (%s) has an invalid type string '%s'. Capital letters not allowed.", lumpname,
                     lumptype, typeStr.c_str());
        return JLR_INVALID;
    }

    if(strcmp(typeStr.c_str(), lumptype))
    {
        error.Printf(ERRLEN, "%s (%s) has mismatched type '%s'", lumpname, lumptype, typeStr.c_str());
        return JLR_INVALID;
    }

    json metadata = root["metadata"];
    if(!metadata.contains("author") || !metadata["author"].is_string() || !metadata.contains("timestamp") ||
       !metadata["timestamp"].is_string() || !metadata.contains("application") || !metadata["application"].is_string())
    {
        error.Printf(ERRLEN, "%s (%s) is missing required metadata fields", lumpname, lumptype);
        return JLR_INVALID;
    }

    return JLR_OK;
}
