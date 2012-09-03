// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 Samuel Villarreal
// Copyright(C) 2011 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   PNG Resource Reading and Conversion
//
//-----------------------------------------------------------------------------

#ifndef V_PNG_H__
#define V_PNG_H__

#include "z_zone.h"

struct patch_t;

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
 
   // accessors
   uint32_t  getWidth()      const;
   uint32_t  getHeight()     const;
   uint32_t  getPitch()      const;
   int       getBitDepth()   const;
   int       getChannels()   const;
   byte     *getRawSurface() const;
   int       getNumColors()  const;
   byte     *getPalette()    const;
   byte     *expandPalette() const;

   // conversions
   byte     *getAs8Bit(const byte *outpal) const;
   byte     *getAs24Bit() const;
   patch_t  *getAsPatch(int tag, void **user = NULL, size_t *size = NULL) const;

   // Static routines
   static bool     CheckPNGFormat(const void *data);
   static patch_t *LoadAsPatch(int lumpnum, int tag, void **user = NULL,
                               size_t *size = NULL);
   static patch_t *LoadAsPatch(const char *lumpname, int tag, 
                               void **user = NULL, size_t *size = NULL);
};

#endif

// EOF

