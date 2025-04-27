//
// Copyright (C) 2013 Samuel Villarreal
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
//------------------------------------------------------------------------------
//
// Purpose: PNG resource reading and conversion.
// Authors: Samuel Villarreal, James Haley
//

#ifndef V_PNG_H__
#define V_PNG_H__

#include "z_zone.h"

struct patch_t;
class WadDirectory;

// Forward declare private implementation class
class VPNGImagePimpl;

class VPNGImage : public ZoneObject
{
private:
    VPNGImagePimpl *pImpl; // Private implementation object to hide libpng

public:
    VPNGImage();
    ~VPNGImage();

    // Methods
    bool readImage(const void *data);
    bool readFromLump(WadDirectory &dir, int lumpnum);
    bool readFromLump(WadDirectory &dir, const char *lumpname);

    // clang-format off

   // accessors
    uint32_t getWidth()      const;
    uint32_t getHeight()     const;
    uint32_t getPitch()      const;
    int      getBitDepth()   const;
    int      getChannels()   const;
    byte    *getRawSurface() const;
    int      getNumColors()  const;
    byte    *getPalette()    const;
    byte    *expandPalette() const;
    int32_t  getXOffset()    const;
    int32_t  getYOffset()    const;

    // clang-format on

    // conversions
    byte    *getAs8Bit(const byte *outpal) const;
    byte    *getAs24Bit() const;
    patch_t *getAsPatch(int tag, void **user = nullptr, size_t *size = nullptr) const;

    // Static routines
    static bool     CheckPNGFormat(const void *data);
    static patch_t *LoadAsPatch(int lumpnum, int tag, void **user = nullptr, size_t *size = nullptr);
    static patch_t *LoadAsPatch(const char *lumpname, int tag, void **user = nullptr, size_t *size = nullptr);
};

bool V_WritePNG(byte *linear, int width, int height, const char *filename);

#endif

// EOF

