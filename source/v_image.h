// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Unified internal image storage
//
//-----------------------------------------------------------------------------

#ifndef V_IMAGE_H__
#define V_IMAGE_H__

#include "z_zone.h"
#include "m_dllist.h"
#include "w_wad.h"

//
// Palette class
//
class VPalette : public ZoneObject
{
public:
   // color struct
   struct rgba_t
   {
      uint8_t r, g, b, a;
   };

protected:
   uint8_t raw[1024];        // raw 1024-byte palette in RGBA order
   rgba_t  colors[256];      // structured palette
   bool    matchColors[256]; // colors allowed for best color matching
   bool    hasColorKey;      // true if colorkey is valid
   uint8_t colorKey;         // index which is a transparency color key, if any

public:

};

// input format hints
enum vimgformathint_e
{
   FORMAT_HINT_NONE,   // don't know or care
   FORMAT_HINT_LINEAR, // generic linear
   FORMAT_HINT_FLAT,   // linear flat
   FORMAT_HINT_FONT,   // linear font
   FORMAT_HINT_PATCH,  // patch_t
   FORMAT_HINT_PNG     // png graphic
};

// actual detected resource format
enum vimgformat_e
{
   FORMAT_INVALID, // missing or corrupt and must be replaced with a stand-in
   FORMAT_LINEAR,
   FORMAT_PATCH,
   FORMAT_PNG
};

struct vimageprops_t
{
   // Basic image properties
   int width;     // width in pixels
   int height;    // height in pixels
   int pitch;     // byte length of a row in image
   int pixelsize; // size of pixels
   int bpp;       // color depth
   int xoffset;   // x offset relative to drawing origin
   int yoffset;   // y offset relative to drawing origin

   // Format
   vimgformathint_e hintFormat;     // format the program expected for the resource
   vimgformat_e     format;         // format the resource was actually loaded from
   int              expectedWidth;  // width the program really wanted
   int              expectedHeight; // height the program really wanted
};

//
// Class for unified internal image storage 
//
class VImage : public ZoneObject
{
protected:
   // Data
   vimageprops_t props; // basic properties
   uint8_t *data;       // raw image data
   uint8_t *mask;       // image transparency mask
   VPalette palette;    // image palette
   
   friend class VImageManager;
   friend class VImageManagerPimpl;

   int lumpnum;              // lump number loaded from
   DLListItem<VImage> links; // hash links for lookup by lump number

   static VImage *FromPNG(const vimageprops_t &props, int lumpnum, VPalette *pal = nullptr);
   static VImage *FromPatch(const vimageprops_t &props, int lumpnum, VPalette *pal = nullptr);
   static VImage *FromLinear(const vimageprops_t &props, int lumpnum, VPalette *pal = nullptr);

public:
   // Test if the image is drawable
   bool isValid() const { return (props.width > 0 && props.height > 0 && data); }

   // Basic property accessors
   int getWidth()     const { return props.width;     }
   int getHeight()    const { return props.height;    }
   int getPitch()     const { return props.pitch;     }
   int getPixelSize() const { return props.pixelsize; }
   int getBPP()       const { return props.bpp;       }
   int getXOffset()   const { return props.xoffset;   }
   int getYOffset()   const { return props.yoffset;   }

   // Format information accessors, in case images need to be treated specially
   // based on expected and actual resource formats
   vimgformathint_e getFormatHint()     const { return props.hintFormat;     }
   vimgformat_e     getFormat()         const { return props.format;         }
   int              getExpectedWidth()  const { return props.expectedWidth;  }
   int              getExpectedHeight() const { return props.expectedHeight; }
   
   // Identification accessors
   int getLumpNum() const { return lumpnum; }
};

class VImageManagerPimpl;

//
// Class for image management
//
class VImageManager
{
private:
   VImageManagerPimpl *pImpl; // private implementation data

protected:
   int lookupResourceNum(WadDirectory &dir, const char *name,
      int li_namespace, bool allowglobal);

   VImage *findDefaultResource(int expectedWidth, int expectedHeight);
   
   VImage *generateDefaultResource(int expectedWidth, int expectedHeight);

   bool resourceIsPatch (void *data, size_t size);
   bool resourceIsPNG   (void *data, size_t size);
   bool resourceIsLinear(void *data, size_t size);

   void determineLinearDimensions(void *data, size_t size, int &w, int &h,
      int ew, int eh, vimgformathint_e expectedFormat);

   vimgformat_e detectResourceFormat(void *data, size_t size);

public:
   VImageManager();

   VImage *loadResource(WadDirectory &dir, int lumpnum,
      vimgformathint_e expectedFormat = FORMAT_HINT_NONE,
      int expectedWidth = 0, int expectedHeight = 0);

   VImage *loadResource(WadDirectory &dir, const char *name, 
      int li_namespace = lumpinfo_t::ns_graphics, bool allowglobal = true,
      vimgformathint_e expectedFormat = FORMAT_HINT_NONE,
      int expectedWidth = 0, int expectedHeight = 0);

   // Resource lookup
   bool hasResource(int lumpnum) const;
};

#endif

// EOF

