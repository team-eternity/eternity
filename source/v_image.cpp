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

#include "z_zone.h"

#include "e_hash.h"
#include "m_qstrkeys.h"
#include "m_binary.h"
#include "v_image.h"
#include "z_auto.h"

// Need libpng
#include "png.h"

//=============================================================================
//
// VPalette
//



//=============================================================================
//
// VImage
//

//
// VImage::FromPNG
//
// Transform a PNG resource into a VImage instance.
//
VImage *VImage::FromPNG(const vimageprops_t &props, int lumpnum, VPalette *pal)
{
   // TODO
   return NULL;
}

//
// VImage::FromPatch
//
// Transform a DOOM patch resource into a VImage instance.
//
VImage *VImage::FromPatch(const vimageprops_t &props, int lumpnum, VPalette *pal)
{
   // TODO
   return NULL;
}

//
// VImage::FromLinear
//
// Transform a linear resource into a VImage instance.
//
VImage *VImage::FromLinear(const vimageprops_t &props, int lumpnum, VPalette *pal)
{
   // TODO
   return NULL;
}

//=============================================================================
//
// VImageManager
//

//
// Private implementation data for VImageManager
//
class VImageManagerPimpl : public ZoneObject
{
public:
   VImageManagerPimpl() : defaultResources(), hash(10247)
   {      
   }

   // Default resources
   DLList<VImage, &VImage::links> defaultResources;

   // Resource hash table
   EHashTable<VImage, EIntHashKey, &VImage::lumpnum, &VImage::links> hash;
};

//
// Constructor
//
VImageManager::VImageManager()
{
   pImpl = new VImageManagerPimpl;
}

//
// VImageManager::lookupResourceNum
//
// Resolve a WadDirectory lump name to lump number.
//
int VImageManager::lookupResourceNum(WadDirectory &dir, const char *name, 
                                     int li_namespace, bool allowglobal)
{
   bool useNSG = false;
   bool lfn = false;

   if(allowglobal && li_namespace != lumpinfo_t::ns_global)
      useNSG = true;

   if(strlen(name) > 8 || strchr(name, '/') != NULL)
      lfn = true;

   // NSG lookup prefers a resource in the given namespace if it is available,
   // but will fall back to ns_global if it's not.
   if(useNSG)
   {
      if(lfn)
         return dir.checkNumForLFNNSG(name, li_namespace);
      else
         return dir.checkNumForNameNSG(name, li_namespace);
   }
   else
   {
      if(lfn)
         return dir.checkNumForLFN(name, li_namespace);
      else
         return dir.checkNumForName(name, li_namespace);
   }
}

//
// VImageManager::findDefaultResource
//
// Protected method.
// See if a default resource with the same properties was already generated.
//
VImage *VImageManager::findDefaultResource(int expectedWidth, int expectedHeight)
{
   VImage *ret = NULL;
   auto img = pImpl->defaultResources.head;

   while(img)
   {
      if((*img)->props.width  == expectedWidth && 
         (*img)->props.height == expectedHeight)
      {
         ret = *img;
         break;
      }

      img = img->dllNext;
   }

   return ret;
}

//
// VImageManager::generateDefaultResource
//
// Protected method.
// When a resource is missing, a stand-in can be generated. The more information
// that was passed on expected image format, the more useful the stand-in will be.
//
VImage *VImageManager::generateDefaultResource(int expectedWidth, int expectedHeight)
{
   VImage *ret = NULL;

   // check if we already have a default resource with these properties
   if((ret = findDefaultResource(expectedWidth, expectedHeight)))
      return ret;

   // generate a new one.
   // TODO

   return ret;
}

//
// VImageManager::resourceIsPatch
//
// Protected method.
// Format detection for DOOM's patch_t image format.
//
bool VImageManager::resourceIsPatch(void *data, size_t size)
{
   // Must be at least as large as the header.
   if(size < 8)
      return false; // invalid header

   auto    base   = static_cast<uint8_t *>(data);
   auto    header = base;
   int16_t width  = GetBinaryWord(header);
   int16_t height = GetBinaryWord(header);
   int16_t left   = GetBinaryWord(header);
   int16_t top    = GetBinaryWord(header);

   // Check for sane header values
   if(width < 0     || width > 4096 || height < 0     || height > 4096 ||
      left  < -2000 || left  > 2000 || top    < -2000 || top    > 2000)
      return false; // invalid or very unlikely graphic size

   // Number of bytes needed for columnofs
   size_t numBytesNeeded = width * 4;
   if(size - 8 < numBytesNeeded)
      return false; // invalid columnofs table size
   
   // Verify all columns
   for(int i = 0; i < width; i++)
   {
      size_t offset = static_cast<size_t>(GetBinaryUDWord(header));

      if(offset < 12 || offset >= size)
         return false; // offset lies outside the data

      // Verify the series of posts at that offset
      byte *rover = base + offset;
      while(*rover != 0xff)
      {
         byte *nextPost = rover + *(rover + 1) + 4;

         if(nextPost >= base + size)
            return false; // Unterminated series of posts, or too long

         rover = nextPost;
      }
   }

   // Patch is valid!
   return true;
}

//
// VImageManager::resourceIsPNG
//
// Format detection for standard PNG images.
//
bool VImageManager::resourceIsPNG(void *data, size_t size)
{
   // check minimum size (need at least a bit more than the header)
   if(size <= 8)
      return false;

   // libpng can sort out the rest
   return !png_sig_cmp((png_const_bytep)data, 0, 8);
}

//
// VImageManager::resourceIsLinear
//
// This will accept any non-empty data source, so it is the lowest priority.
//
bool VImageManager::resourceIsLinear(void *data, size_t size)
{
   return (size != 0);
}

//
// V_linearOptimalSize
//
// Try to find the most rectangular size for a linear raw graphic, preferring
// a wider width than height when the graphic is not square.
//
// Rational log2(N) courtesy of Sean Eron Anderson's famous bit hacks:
// http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogObvious
//
static void V_linearOptimalSize(size_t lumpsize, int &w, int &h)
{
   size_t v = lumpsize;
   size_t r = 0;
   
   while(v >>= 1)
      ++r;

   h = static_cast<int>(1 << (r / 2));
   w = static_cast<int>(lumpsize / h);
}

//
// VImageManager::determineLinearDimensions
//
// Linear graphics do not have size information in their header so in some
// situations the dimensions to use must be determined manually. For historical
// reasons there are multiple ways to do this depending on the resource format
// hint.
//
void VImageManager::determineLinearDimensions(void *data, size_t size, 
                                              int &w, int &h, int ew, int eh, 
                                              vimgformathint_e expectedFormat)
{
   // if exact match for expectations, use them.
   if(size == (size_t)ew * (size_t)eh)
   {
      w = ew;
      h = ew;
      return;
   }

   if(expectedFormat == FORMAT_HINT_FLAT) // image is a flat
   {
      switch(size)
      {
      case 4160: // Heretic 64x65 scrolling flats
      case 8192: // Hexen 64x128 scrolling flats
         w = h = 64;
         break;
      default: 
         V_linearOptimalSize(size, w, h);
         break;
      }
   }
   else if(expectedFormat == FORMAT_HINT_FONT) // image is a font
   {
      for(int i = 5; i <= 32; i++)
      {
         if(static_cast<size_t>(i * i * 128) == size)
         {
            w = i * 16;
            h = i * 8;
            return;
         }
      }
      // can calculate optimal size, but, font code will reject it.
      V_linearOptimalSize(size, w, h);
   }
   else // screen graphics
   {
      switch(size)
      {
      case 120:  // special case - "gnum" format
         w = 10;
         h = 12;
         break;
      case 2048: // special case (Strife startup sprite)
         w = 32;
         h = 64;
         break;
      case 2304: // special case (Strife startup sprite)
         w = h = 48;
         break;
      case 4160: // stupid Heretic flats
         w = h = 64;
         break;
      case 8192: // special case
         w = 64;
         h = 128;
         break;
      default:
         if(!(size % 320)) // covers fullscreen gfx and Heretic AUTOPAGE
         {
            w = 320;
            h = static_cast<int>(size / 320);
         }
         else
            V_linearOptimalSize(size, w, h);
         break;
      }
   }
}

//
// VImageManager::detectResourceFormat
//
// Protected method.
// Given the raw image data, detect what actual format it is in.
//
vimgformat_e VImageManager::detectResourceFormat(void *data, size_t size)
{
   typedef bool (VImageManager::*fmtmethod_t)(void *, size_t);

   static fmtmethod_t methods[3] =
   {
      &VImageManager::resourceIsPNG,
      &VImageManager::resourceIsPatch,
      &VImageManager::resourceIsLinear
   };
   static vimgformat_e fmts[3] =
   {
      FORMAT_PNG,
      FORMAT_PATCH,
      FORMAT_LINEAR
   };

   vimgformat_e fmt = FORMAT_INVALID;

   for(size_t i = 0; i < earrlen(methods); i++)
   {
      if((this->*(methods[i]))(data, size))
      {
         fmt = fmts[i];
         break;
      }
   }
   
   return fmt;
}

//
// VImageManager::loadResource
//
// Loads resource from a wad directory by lump number. Optional parameters
// allow storage of information in the image about what was expected by the
// program versus whatever may be actually found.
//
VImage *VImageManager::loadResource(WadDirectory &dir, int lumpnum,
                                    vimgformathint_e expectedFormat,
                                    int expectedWidth, int expectedHeight)
{
   VImage *ret = NULL;

   // missing resource, generate a default with characteristics that best
   // match any expected ones passed in.
   if(lumpnum < 0)
      return generateDefaultResource(expectedWidth, expectedHeight);
  
   // cached already?
   if(hasResource(lumpnum))
      return pImpl->hash.objectForKey(lumpnum);

   // check size
   size_t size;
   if(!(size = static_cast<size_t>(dir.lumpLength(lumpnum))))
      return generateDefaultResource(expectedWidth, expectedHeight);

   // load wad lump
   void *rawdata = dir.cacheLumpNum(lumpnum, PU_STATIC);

   // detect format
   vimgformat_e fmt;
   // if invalid, generate default resource
   if((fmt = detectResourceFormat(rawdata, size)) == FORMAT_INVALID)
      return generateDefaultResource(expectedWidth, expectedHeight);
   
   // TODO

   // change wad lump to PU_CACHE
   Z_ChangeTag(rawdata, PU_CACHE);

   return ret;
}

//
// VImageManager::loadResource
//
// Loads resource from a wad directory by name. If lfn is true, the resource
// name is a "long file name," such as for a resource in a zip. Optional 
// parameters allow storage of information in the image about what was expected
// by the program versus whatever may be actually found.
//
VImage *VImageManager::loadResource(WadDirectory &dir, const char *name,
                                    int li_namespace, bool allowglobal,
                                    vimgformathint_e expectedFormat,
                                    int expectedWidth, int expectedHeight)
{
   int lumpnum = lookupResourceNum(dir, name, li_namespace, allowglobal);
   return loadResource(dir, lumpnum, expectedFormat, expectedWidth, expectedHeight);
}


//
// VImageManager::hasResource
//
// Returns true if the manager has a resource by the given name
//
bool VImageManager::hasResource(int lumpnum) const
{
   return pImpl->hash.objectForKey(lumpnum) != NULL;
}

// EOF

