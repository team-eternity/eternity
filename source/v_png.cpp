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
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "doomtype.h"
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
   unsigned int width;
   unsigned int height;
   png_byte     color_type;
   png_byte     bit_depth;
   png_size_t   row_bytes;
   png_byte     channels;

   // PNG reading structures (valid during read only)
   png_structp   png_ptr;
   png_infop     info_ptr;
   png_infop     end_ptr;
   png_byte    **row_pointers;

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

bool VPNGImagePimpl::readImage(byte *data)
{
   bool readSuccess = true;
   vpngiostruct_t ioStruct;

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

   end_ptr = png_create_info_struct(png_ptr);
   if(!end_ptr)
   {
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return false;
   }

   // TODO: Warning - the concept of throwing an exception through libpng is
   // still untested. One of the libpng authors claims it should work, in lieu
   // of using setjmp/longjmp. Need to feed it a bad PNG to see what happens.
   try
   {
      // set read function, since we are reading from a data source in memory
      png_set_read_fn(png_ptr, &ioStruct, V_pngReadFunc);

      // read full PNG
      png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

      // get row_pointers
      row_pointers = png_get_rows(png_ptr, info_ptr);

      // get basic PNG metrics
      width      = png_get_image_width(png_ptr, info_ptr);
      height     = png_get_image_height(png_ptr, info_ptr);
      color_type = png_get_color_type(png_ptr, info_ptr);
      bit_depth  = png_get_bit_depth(png_ptr, info_ptr);
      row_bytes  = png_get_rowbytes(png_ptr, info_ptr);
      channels   = png_get_channels(png_ptr, info_ptr);

      // TODO: stuff...
   }
   catch(...)
   {
      readSuccess = false;
   }

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
   return !png_sig_cmp((png_const_bytep)data, (png_size_t)0, 8);
}



// EOF

