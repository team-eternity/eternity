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
//   Some code derived from Doom 64 EX, by Kaiser. Used under the GPL.
//   Some code derived from SDL_image, by Sam Lantinga. Used under the GPL
//    (via license upgrade option of the LGPL v2.1 license)
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "doomtype.h"
#include "v_buffer.h"
#include "v_misc.h"
#include "v_png.h"

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
   int         interlace_method;
   int         color_key;
   png_byte    channels;
   byte       *surface;
   png_uint_32 rmask;
   png_uint_32 gmask;
   png_uint_32 bmask;
   png_uint_32 amask;

   struct palette_t
   {
      png_colorp colors;
      int        numColors;
   };

   palette_t palette;

   // PNG reading structures (valid during read only)
   png_structp png_ptr;
   png_infop   info_ptr;
   png_bytepp  row_pointers;

   // Methods
   bool readImage(byte *data);
};

//=============================================================================
//
// Static callbacks for libpng
//

struct vpngiostruct_t
{
   byte *data;
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

static inline bool is_big_endian()
{
   uint32_t i = 0xAABBCCDD;

   return (((byte *)&i)[0] == 0xAA);
}

bool VPNGImagePimpl::readImage(byte *data)
{
   bool readSuccess = true;
   vpngiostruct_t ioStruct;
   png_color_16 *transv;

   ioStruct.data = data;

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
                   &interlace_method, NULL, NULL);

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
                   &interlace_method, NULL, NULL);

      channels = png_get_channels(png_ptr, info_ptr);
      
      // Allocate surface
      pitch   = width * bit_depth * channels;
      surface = (byte *)(calloc(pitch, height));

      // Determine color masks
      rmask = gmask = bmask = amask = 0;
      if(color_type != PNG_COLOR_TYPE_PALETTE)
      {
         if(is_big_endian())
         {
            int s = (channels == 4 ? 0 : 8);
            rmask = 0xFF000000 >> s;
            gmask = 0x00FF0000 >> s;
            bmask = 0x0000FF00 >> s;
            amask = 0x000000FF >> s;
         }
         else
         {
            rmask = 0x000000FF;
            gmask = 0x0000FF00;
            bmask = 0x00FF0000;
            amask = (channels == 4 ? 0xFF000000 : 0);
         }
      }

      // TODO: colorkey?

      // Create row pointers for dest buffer
      row_pointers = (png_bytepp)(calloc(height, sizeof(png_bytep)));
      for(int row = 0; row < (int)height; row++)
         row_pointers[row] = (png_bytep)surface + row * pitch;

      // Read the full image
      png_read_image(png_ptr, row_pointers);

      // Read end.
      png_read_end(png_ptr, info_ptr);

      // Load palette, if any
      if(color_type == PNG_COLOR_TYPE_PALETTE ||
         color_type == PNG_COLOR_MASK_PALETTE)
      {
         png_colorp png_palette;
         png_get_PLTE(png_ptr, info_ptr, &png_palette, &palette.numColors);

         if(palette.numColors > 0)
         {
            palette.colors = (png_colorp)(calloc(palette.numColors, sizeof(png_color)));
            memcpy(palette.colors, png_palette, palette.numColors * sizeof(png_color));
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
      free(row_pointers);

   return readSuccess;
}

//=============================================================================
//
// VPNGImage Methods
//

// Publics

bool VPNGImage::readImage(byte *data)
{
   return pImpl->readImage(data);
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
bool VPNGImage::CheckPNGFormat(byte *data)
{
   return !png_sig_cmp((png_const_bytep)data, 0, 8);
}



// EOF

