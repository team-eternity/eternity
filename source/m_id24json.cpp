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

#include "nlohmann/json.hpp"

using json = nlohmann::json;

enum
{
    ERRLEN = 128,
};

static bool M_parseJSONVersion(const char *versionString, JSONLumpVersion &version)
{
    const char *p = versionString;
    char       *endptr;

    version = {};

    // Parse major version
    version.major = std::strtol(p, &endptr, 10);
    if(endptr == p || *endptr != '.' || version.major < 0)
        return false;
    p = endptr + 1;

    // Parse minor version
    version.minor = std::strtol(p, &endptr, 10);
    if(endptr == p || *endptr != '.' || version.minor < 0)
        return false;
    p = endptr + 1;

    // Parse revision version
    version.revision = std::strtol(p, &endptr, 10);
    if(endptr == p || *endptr != '\0' || version.revision < 0)
        return false;

    return true;
}

static bool M_validateType(const char *type)
{
    for(const char *p = type; *p; p++)
    {
        char c = *p;
        if((c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '_' && c != '-')
            return false;
    }

    return true;
}

static void M_warnf(bool error, jsonWarning_t warningFunc, const char *fmt, ...)
{
    if(!warningFunc)
        return;

    char    buffer[ERRLEN];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    warningFunc(error, buffer);
}

bool JSONLumpVersion::operator>(const JSONLumpVersion &other) const
{
    if(major != other.major)
        return major > other.major;
    if(minor != other.minor)
        return minor > other.minor;
    return revision > other.revision;
}

jsonLumpResult_e M_ParseJSONLump(const void *rawdata, size_t rawsize, const char *lumptype,
                                 const JSONLumpVersion &maxversion, jsonLumpFunc_t lumpFunc, void *context,
                                 jsonWarning_t warningFunc)
{
    json root;
    try
    {
        auto strData = static_cast<const char *>(rawdata);
        root         = json::parse(strData, strData + rawsize);
    }
    catch(const json::parse_error &e)
    {
        M_warnf(true, warningFunc, "%s parse error at byte %zu: %s", lumptype, e.byte, e.what());
        return JLR_INVALID;
    }

    if(!root.is_object())
        return JLR_INVALID;

    if(root.size() != 4 || !root.contains("type") || !root["type"].is_string() || !root.contains("version") ||
       !root["version"].is_string() || !root.contains("metadata") || !root["metadata"].is_object() ||
       !root.contains("data") || !root["data"].is_object())
    {
        M_warnf(true, warningFunc, "%s is missing required root fields", lumptype);
        return JLR_INVALID;
    }

    JSONLumpVersion version;
    std::string     versionStr = root["version"].get<std::string>();
    if(!M_parseJSONVersion(versionStr.c_str(), version))
    {
        M_warnf(true, warningFunc, "%s has an invalid version string '%s'", lumptype, versionStr.c_str());
        return JLR_INVALID;
    }

    if(version > maxversion)
    {
        M_warnf(true, warningFunc, "%s version %s is unsupported (max %d.%d.%d)", lumptype, versionStr.c_str(),
                maxversion.major, maxversion.minor, maxversion.revision);
        return JLR_UNSUPPORTED_VERSION;
    }

    std::string typeStr = root["type"].get<std::string>();
    if(!M_validateType(typeStr.c_str()))
    {
        M_warnf(false, warningFunc, "%s has an invalid type string '%s'. Capital letters not officially allowed.",
                lumptype, typeStr.c_str());
    }

    if(strcasecmp(typeStr.c_str(), lumptype))
    {
        M_warnf(true, warningFunc, "%s has mismatched type '%s'", lumptype, typeStr.c_str());
        return JLR_INVALID;
    }

    json metadata = root["metadata"];
    if(!metadata.contains("author") || !metadata["author"].is_string() || !metadata.contains("timestamp") ||
       !metadata["timestamp"].is_string() || !metadata.contains("application") || !metadata["application"].is_string())
    {
        M_warnf(false, warningFunc, "%s metadata doesn't follow specs", lumptype);
    }

    return lumpFunc(root["data"], context, warningFunc);
}
