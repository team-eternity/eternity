// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3:
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
//   Some code derived from Doom 64 EX, by Kaiser. Used under the GPL.
//   Some code derived from SDL_image, by Sam Lantinga. Used under the GPL
//    (via license upgrade option of the LGPL v2.1 license)
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "doomtype.h"
#include "v_misc.h"
#include "v_png.h"
#include "v_video.h"

// Need libpng
#include "png.h"

//=============================================================================
//
// VPNGImagePimpl
//
// A private implementation object idiom will help us keep libpng from being
// globally visible.
//

class VPNGImagePimpl
{
public:
   // Basic Image Properties
   png_uint_32 width;
   png_uint_32 height;
   png_uint_32 pitch;
   int         color_type;
   int         bit_depth;
   int         color_key;
   png_byte    channels;
   byte       *surface;

   struct palette_t
   {
      byte *colors;
      int   numColors;
   };

   palette_t palette;

   // PNG reading structures (valid during read only)
   png_structp png_ptr;
   png_infop   info_ptr;
   png_bytepp  row_pointers;

   // Methods
   bool  readImage(const void *data);
   void  freeImage();
   byte *getAs8Bit(const byte *outpal) const;
   byte *getAs24Bit() const;
   byte *buildTranslation(const byte *outpal) const;
};

//=============================================================================
//
// Static callbacks for libpng
//

struct vpngiostruct_t
{
   const byte *data;
};

//
// V_pngError
//
// Error callback for libpng.
//
static void V_pngError(png_structp png_ptr, png_const_charp error_msg)
{
   C_Printf(FC_ERROR "libpng error: %s\a", error_msg);

   throw 0;
}

//
// V_pngWarning
//
// Warning callback for libpng.
//
static void V_pngWarning(png_structp png_ptr, png_const_charp error_msg)
{
   C_Printf(FC_ERROR "libpng warning: %s", error_msg);
}

//
// V_pngReadFunc
//
// Read some data from the buffer.
//
static void V_pngReadFunc(png_structp png_ptr, png_bytep area, png_size_t size)
{
   vpngiostruct_t *io = static_cast<vpngiostruct_t *>(png_get_io_ptr(png_ptr));
   
   memcpy(area, io->data, size);
   io->data += size;
}


//=============================================================================
//
// VPNGImagePimpl Methods
//

//
// VPNGImagePimpl::readImage
//
// Does the full process of using libpng to read a PNG file from memory.
// Most of the unusual PNG formats are handled here by having libpng normalize
// them into what is effectively either 8- or 32-bit output.
//
bool VPNGImagePimpl::readImage(const void *data)
{
   bool readSuccess = true;
   vpngiostruct_t ioStruct;
   png_color_16 *transv;

   ioStruct.data = static_cast<const byte *>(data);

   if(!VPNGImage::CheckPNGFormat(data))
      return false;

   png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, 
                                    V_pngError, V_pngWarning);
   if(!png_ptr)
      return false;

   info_ptr = png_create_info_struct(png_ptr);
   if(!info_ptr)
   {
      png_destroy_read_struct(&png_ptr, NULL, NULL);
      return false;
   }

   // TODO: Warning - the concept of throwing an exception through libpng is
   // still untested. One of the libpng authors claims it should work, in lieu
   // of using setjmp/longjmp. Need to feed it a bad PNG to see what happens.
   try
   {
      // set read function, since we are reading from a data source in memory
      png_set_read_fn(png_ptr, &ioStruct, V_pngReadFunc);

      // read header info
      png_read_info(png_ptr, info_ptr);
      png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                   NULL, NULL, NULL);

      // Strip 16-bit color elements to 8-bit precision
      png_set_strip_16(png_ptr);

      // Extract 1-, 2-, and 4-bit images into 8-bit pixels
      png_set_packing(png_ptr);

      // Scale grayscale values into the range of 0 to 255
      if(color_type == PNG_COLOR_TYPE_GRAY)
         png_set_expand(png_ptr);

      // Set color key if available
      if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
      {
         int num_trans;
         png_bytep trans;

         png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, &transv);

         if(color_type == PNG_COLOR_TYPE_PALETTE)
         {
            int i = 0, t = -1;

            // check opacity of palette
            for(; i < num_trans; i++)
            {
               if(trans[i] == 0)
               {
                  if(t >= 0)
                     break;
                  t = i;
               }
               else if(trans[i] != 255)
                  break;
            }
            if(i == num_trans) // exactly one transparent index
               color_key = t;
            else               // multiple, set to expand
               png_set_expand(png_ptr);
         }
         else
            color_key = 0;
      }

      // Grayscale with alpha -> RGB
      if(color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
         png_set_gray_to_rgb(png_ptr);

      // Update info
      png_read_update_info(png_ptr, info_ptr);
      png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, 
                   NULL, NULL, NULL);

      channels = png_get_channels(png_ptr, info_ptr);
      
      // Allocate surface
      pitch   = width * bit_depth * channels / 8;
      surface = ecalloc(byte *, pitch, height);

      // TODO: colorkey?

      // Create row pointers for dest buffer
      row_pointers = ecalloc(png_bytepp, height, sizeof(png_bytep));
      for(int row = 0; row < (int)height; row++)
         row_pointers[row] = (png_bytep)surface + row * pitch;

      // Read the full image
      png_read_image(png_ptr, row_pointers);

      // Read end.
      png_read_end(png_ptr, info_ptr);

      // Load palette, if any
      if(color_type == PNG_COLOR_TYPE_PALETTE)
      {
         png_colorp png_palette;
         png_get_PLTE(png_ptr, info_ptr, &png_palette, &palette.numColors);

         if(palette.numColors > 0)
         {
            palette.colors = ecalloc(byte *, 3, palette.numColors);

            for(int i = 0; i < palette.numColors; i++)
            {
               palette.colors[i*3+0] = (byte)(png_palette[i].red);
               palette.colors[i*3+1] = (byte)(png_palette[i].green);
               palette.colors[i*3+2] = (byte)(png_palette[i].blue);
            }
         }
      } // end if for palette reading

      // Build a dummy palette for grayscale images
      if(color_type == PNG_COLOR_TYPE_GRAY && !palette.colors)
      {
         palette.numColors = 256;
         palette.colors = ecalloc(byte *, 3, 256);

         for(int i = 0; i < palette.numColors; i++)
         {
            palette.colors[i*3+0] = (byte)i;
            palette.colors[i*3+1] = (byte)i;
            palette.colors[i*3+2] = (byte)i;
         }
      }
   }
   catch(...)
   {
      readSuccess = false;
   }

   // Done, cleanup.
   png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

   if(row_pointers)
      efree(row_pointers);
   row_pointers = NULL;

   return readSuccess;
}

//
// VPNGImagePimpl::freeImage
//
// Free everything allocated for the PNG
//
void VPNGImagePimpl::freeImage()
{
   if(palette.colors)
      efree(palette.colors);
   palette.colors = NULL;

   if(surface)
      efree(surface);
   surface = NULL;
}

//
// VPNGImagePimpl::buildTranslation
//
// The input is the "output" or target palette, which must be 768 bytes.
// The output is always a 256-byte table for re-indexing the image.
//
byte *VPNGImagePimpl::buildTranslation(const byte *outpal) const
{
   int numcolors = 
      palette.numColors > 256 ? palette.numColors : 256;

   byte *newpal = ecalloc(byte *, 1, numcolors);

   for(int i = 0; i < palette.numColors; i++)
   {
      newpal[i] = V_FindBestColor(outpal, 
                                  palette.colors[i*3+0],
                                  palette.colors[i*3+1],
                                  palette.colors[i*3+2]);
   }

   return newpal;
}

//
// VPNGImagePimpl::getAs8Bit
//
// Implements conversion of any source PNG to an 8-bit linear buffer.
//
byte *VPNGImagePimpl::getAs8Bit(const byte *outpal) const
{
   if(color_type == PNG_COLOR_TYPE_GRAY || 
      color_type == PNG_COLOR_TYPE_PALETTE)
   {
      if(!outpal)
      {
         // Pure copy, no requantization
         byte *output = ecalloc(byte *, height, pitch);
         return (byte *)(memcpy(output, surface, height * pitch));
      }
      else
      {
         byte *trtbl  = buildTranslation(outpal);
         byte *output = ecalloc(byte *, height, pitch);

         for(png_uint_32 y = 0; y < height; y++)
         {
            for(png_uint_32 x = 0; x < width; x++)
            {
               byte px = surface[y * width + x];

               output[y * width + x] = trtbl[px];
            }
         }

         // free the translation table
         efree(trtbl);

         return output;
      }
   }
   else
   {
      if(!outpal) // a palette is required for this conversion
         return NULL;

      byte *src  = surface;
      byte *dest = ecalloc(byte *, width, height);

      for(png_uint_32 y = 0; y < height; y++)
      {
         for(png_uint_32 x = 0; x < width; x++)
         {
            // TODO: extremely inefficient
            dest[y * width + x] =
               V_FindBestColor(outpal, *dest, *(dest+1), *(dest+2));
            src += channels;
         }
      }

      return dest;
   }
}

//
// VPNGImagePimpl::getAs24Bit
//
// Implements conversion of any source PNG to a 24-bit linear buffer.
//
byte *VPNGImagePimpl::getAs24Bit() const
{
   if(color_type == PNG_COLOR_TYPE_GRAY || 
      color_type == PNG_COLOR_TYPE_PALETTE)
   {
      if(!palette.colors) // no palette??
         return NULL;

      byte *src    = surface;
      byte *buffer = ecalloc(byte *, width*3, height);
      byte *dest   = buffer;

      for(png_uint_32 y = 0; y < height; y++)
      {
         for(png_uint_32 x = 0; x < width; x++)
         {
            *dest++ = palette.colors[*src * 3 + 0];
            *dest++ = palette.colors[*src * 3 + 1];
            *dest++ = palette.colors[*src * 3 + 2];
            ++src;
         }
      }

      return buffer;
   }
   else
   {
      byte *src    = surface;
      byte *buffer = ecalloc(byte *, width*3, height);
      byte *dest   = buffer;

      for(png_uint_32 y = 0; y < height; y++)
      {
         for(png_uint_32 x = 0; x < width; x++)
         {
            *dest++ = *(src + 0);
            *dest++ = *(src + 1);
            *dest++ = *(src + 2);

            src += channels;
         }
      }

      return buffer;
   }
}

//=============================================================================
//
// VPNGImage Methods
//

// Publics

//
// VPNGImage
//
// Default constructor
//
VPNGImage::VPNGImage() : ZoneObject()
{
   // pImpl object is a POD, so use calloc to create it and
   // initialize all fields to zero
   pImpl = ecalloc(VPNGImagePimpl *, 1, sizeof(VPNGImagePimpl));
}

//
// Destructor
//
VPNGImage::~VPNGImage()
{
   if(pImpl)
   {
      // Destroy the pImpl's internal resources
      pImpl->freeImage();

      // Free the pImpl
      efree(pImpl);
      pImpl = NULL;
   }
}

//
// VPNGImage::readImage
//
// Pass in data that you suspect is a PNG and the VPNGImage object will be
// setup to contain that image and all the data necessary to use it. Returns
// true on success or false on failure.
//
bool VPNGImage::readImage(const void *data)
{
   return pImpl->readImage(data);
}

//
// Accessors
//
// These are implemented in here, rather than as inlines, so that the pImpl
// object can remain hidden.
//

uint32_t VPNGImage::getWidth() const
{
   return (uint32_t)(pImpl->width);
}

uint32_t VPNGImage::getHeight() const
{
   return (uint32_t)(pImpl->height);
}

uint32_t VPNGImage::getPitch() const
{
   return (uint32_t)(pImpl->pitch);
}

int VPNGImage::getBitDepth() const
{
   return pImpl->bit_depth;
}

int VPNGImage::getChannels() const
{
   return (int)(pImpl->channels);
}

byte *VPNGImage::getRawSurface() const
{
   return pImpl->surface;
}

int VPNGImage::getNumColors() const
{
   return pImpl->palette.numColors;
}

//
// VPNGImage::getPalette
//
// If the PNG image is paletted (numColors > 0), this will return the palette.
// Otherwise NULL is returned.
//
byte *VPNGImage::getPalette() const
{
   return pImpl->palette.colors;
}

#define PNGMAX(a, b) ((a) > (b) ? (a) : (b))

//
// VPNGImage::expandPalette
//
// Returns a copy of the PNG's palette (if it has one) which is expanded to
// 256 (or more) colors. The unused indices are all black.
// 
byte *VPNGImage::expandPalette() const
{
   byte *newPalette = NULL;
   
   if(pImpl->palette.colors)
   {
      int numcolorsalloc = PNGMAX(pImpl->palette.numColors, 256);
      
      newPalette = ecalloc(byte *, 3, numcolorsalloc);
      memcpy(newPalette, pImpl->palette.colors, 3*pImpl->palette.numColors);
   }

   return newPalette;
}

//
// Conversion Routines
//

//
// VPNGImage::getAs8Bit
//
// Returns an 8-bit linear version of the PNG graphics. 
// If it is grayscale or 8-bit color:
//  * If outpal is provided, the colors will be requantized to that palette
//  * If outpal is NULL, the colors are left alone
// If it is true color:
//  * outpal must be valid; the colors will be requantized to that palette
//
byte *VPNGImage::getAs8Bit(const byte *outpal) const
{
   return pImpl->getAs8Bit(outpal);
}

//
// VPNGImage::getAs24Bit
//
// Returns a 24-bit linear version of the PNG graphics. 
// If it is grayscale or 8-bit color:
//  * Output is converted to 24-bit
// If it is true color:
//  * Output is directly translated, omitting any alpha bytes
//
byte *VPNGImage::getAs24Bit() const
{
   return pImpl->getAs24Bit();
}

//=============================================================================
//
// Static Methods
//

//
// VPNGImage::CheckPngFormat
//
// Static method.
// Returns true if the block appears to be a PNG based on the magic signature.
//
bool VPNGImage::CheckPNGFormat(const void *data)
{
   return !png_sig_cmp((png_const_bytep)data, 0, 8);
}

// EOF

