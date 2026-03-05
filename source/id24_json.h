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

#ifndef ID24_JSON_H_
#define ID24_JSON_H_

#include "nlohmann/json.hpp"

class qstring;

namespace id24
{
enum jsonLumpResult_e
{
    JLR_OK,
    JLR_NO_LUMP,
    JLR_INVALID,
    JLR_UNSUPPORTED_VERSION
};
struct JSONLumpVersion
{
    bool operator>(const JSONLumpVersion &other) const;

    int major;
    int minor;
    int revision;
};
typedef void (*jsonWarning_t)(bool error, const char *msg);

typedef jsonLumpResult_e (*jsonLumpFunc_t)(const nlohmann::json &data, void *context, jsonWarning_t warningFunc);

jsonLumpResult_e ParseJSONLump(const void *rawdata, size_t rawsize, const char *lumptype,
                               const JSONLumpVersion &maxversion, jsonLumpFunc_t lumpFunc, void *context,
                               jsonWarning_t warningFunc);

bool CoerceInt(const nlohmann::json &object, const char *fieldName, int &out, jsonWarning_t warningFunc);

} // namespace id24

#endif
// EOF
