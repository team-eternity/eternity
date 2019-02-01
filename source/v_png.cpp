// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Samuel Villarreal
// Copyright(C) 2013 James Haley et al.
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

#include "autopalette.h"
#include "c_io.h"
#include "doomtype.h"
#include "m_swap.h"
#include "v_misc.h"
#include "v_png.h"
#include "v_video.h"
#include "r_patch.h"
#include "w_wad.h"
#include "z_auto.h"

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
   int32_t     xoffset;
   int32_t     yoffset;

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
   bool  readFromLumpNum(WadDirectory &dir, int lump);
   bool  readFromLumpName(WadDirectory &dir, const char *name);
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
   const byte    *data;

   struct offsets_t
   {
      int32_t x;
      int32_t y;
   } offsets;
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
   auto io = static_cast<vpngiostruct_t *>(png_get_io_ptr(png_ptr));
   
   memcpy(area, io->data, size);
   io->data += size;
}

//
// V_pngReadUnknownChunk
//
// Read DOOM-community-standard chunks, including offsets.
//
static int V_pngReadUnknownChunk(png_structp png_ptr, png_unknown_chunkp chunk)
{
   int res = 0;
   static png_byte grAb_id[4] = { 'g', 'r', 'A', 'b' };
   auto io = static_cast<vpngiostruct_t *>(png_get_user_chunk_ptr(png_ptr));

   // grAb offsets chunk
   if(!memcmp(chunk->name, grAb_id, sizeof(grAb_id)) && chunk->size >= 8)
   {
      io->offsets.x = SwapBigLong(*reinterpret_cast<int32_t *>(chunk->data));
      io->offsets.y = SwapBigLong(*reinterpret_cast<int32_t *>(chunk->data + 4));
      res = 1;
   }

   return res;
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
   edefstructvar(vpngiostruct_t, ioStruct);
   bool readSuccess = true;   
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

   // DONE: I have successfully tested the concept of throwing an exception 
   // through libpng. One of the libpng authors claimed it should work, in lieu
   // of using setjmp/longjmp. I have fed EE some bad PNGs and the exception
   // propagates past the C calls on the stack and ends up here without incident.
   try
   {
      // set unknown chunk handler
      png_set_read_user_chunk_fn(png_ptr, &ioStruct, V_pngReadUnknownChunk);

      // set read function, since we are reading from a data source in memory
      png_set_read_fn(png_ptr, &ioStruct, V_pngReadFunc);

      // read header info
      png_read_info(png_ptr, info_ptr);
      png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                   NULL, NULL, NULL);

      // Set offsets (retrieved via V_pngReadUnknownChunk)
      xoffset = ioStruct.offsets.x;
      yoffset = ioStruct.offsets.y;

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
// VPNGImagePimpl::readFromLumpNum
//
// Read a PNG resource from a lump, by number.
//
bool VPNGImagePimpl::readFromLumpNum(WadDirectory &dir, int lump)
{
   ZAutoBuffer lumpBuffer;
   bool result = false;

   if(lump >= 0)
   {
      dir.cacheLumpAuto(lump, lumpBuffer);
      result = readImage(lumpBuffer.get());
   }

   return result;
}

//
// VPNGImagePimpl::readFromLumpName
//
// Read a PNG resource from a lump, by name.
//
bool VPNGImagePimpl::readFromLumpName(WadDirectory &dir, const char *name)
{
   return readFromLumpNum(dir, dir.checkNumForName(name));
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
   pImpl = estructalloc(VPNGImagePimpl, 1);
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
// VPNGImage::readFromLump
//
// Calls readImage after caching the given lump number in the given
// wad directory. Overload for lump numbers.
//
bool VPNGImage::readFromLump(WadDirectory &dir, int lumpnum)
{
   return pImpl->readFromLumpNum(dir, lumpnum);
}

//
// VPNGImage::readFromLump
//
// Overload for lump names.
//
bool VPNGImage::readFromLump(WadDirectory &dir, const char *lumpname)
{
   return pImpl->readFromLumpName(dir, lumpname);
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

int32_t VPNGImage::getXOffset() const
{
   return pImpl->xoffset;
}

int32_t VPNGImage::getYOffset() const
{
   return pImpl->yoffset;
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

patch_t *VPNGImage::getAsPatch(int tag, void **user, size_t *size) const
{
   AutoPalette pal(wGlobalDir);
   patch_t *patch;
   byte    *linear = getAs8Bit(pal.get());
   int      w      = static_cast<int>(getWidth());
   int      h      = static_cast<int>(getHeight());

   patch = V_LinearToTransPatch(linear, w, h, size, pImpl->color_key, tag, user);

   patch->leftoffset = static_cast<int16_t>(pImpl->xoffset);
   patch->topoffset  = static_cast<int16_t>(pImpl->yoffset);

   efree(linear);

   return patch;
}

//=============================================================================
//
// Static Methods
//

//
// VPNGImage::CheckPNGFormat
//
// Static method.
// Returns true if the block appears to be a PNG based on the magic signature.
//
bool VPNGImage::CheckPNGFormat(const void *data)
{
   return !png_sig_cmp((png_const_bytep)data, 0, 8);
}

//
// VPNGImage::LoadAsPatch
//
// Load a PNG image and convert it to patch_t format.
//
patch_t *VPNGImage::LoadAsPatch(int lumpnum, int tag, void **user, size_t *size)
{   
   patch_t *patch = NULL;
   int len;
  
   if((len = wGlobalDir.lumpLength(lumpnum)) > 8)
   {
      VPNGImage   png;
      ZAutoBuffer buffer(len, true);
      wGlobalDir.readLump(lumpnum, buffer.get());
      if(png.readImage(buffer.get()))
         patch = png.getAsPatch(tag, user, size);
   }

   return patch;
}

patch_t *VPNGImage::LoadAsPatch(const char *lumpname, int tag, void **user,
                                size_t *size)
{
   int lumpnum = wGlobalDir.checkNumForName(lumpname);

   return lumpnum >= 0 ? LoadAsPatch(lumpnum, tag, user, size) : NULL;
}

//=============================================================================
//
// PNG Writer
//

// PNG writing private data structure
struct pngwrite_t
{
   FILE *outf;
   bool  errorFlag;
   int   errnoVal;
};

//
// V_pngDataWrite
//
// File writing callback for libpng.
//
static void V_pngDataWrite(png_structp pngptr, png_bytep data, png_size_t len)
{
   pngwrite_t *iodata = static_cast<pngwrite_t *>(png_get_io_ptr(pngptr));

   if(fwrite(data, 1, len, iodata->outf) != len)
   {
      iodata->errorFlag = true;
      iodata->errnoVal  = errno;
   }
}

//
// V_pngDataFlush
//
// Flush callback for libpng.
//
static void V_pngDataFlush(png_structp pngptr)
{
   pngwrite_t *iodata = static_cast<pngwrite_t *>(png_get_io_ptr(pngptr));
 
   if(fflush(iodata->outf) != 0)
   {
      iodata->errorFlag = true;
      iodata->errnoVal  = errno;
   }
}

//
// V_pngWriteWarning
//
// Warning message callback for libpng.
//
static void V_pngWriteWarning(png_structp pngptr, png_const_charp warningMsg)
{
   C_Printf(FC_ERROR "libpng warning: %s", warningMsg);
}

//
// V_pngWriteError
//
// Error callback for libpng. An exception will be thrown so that control
// jumps back to V_WritePNG without returning through libpng.
//
static void V_pngWriteError(png_structp pngptr, png_const_charp errorMsg)
{
   C_Printf(FC_ERROR "libpng error: %s", errorMsg);

   throw 0;
}

//
// V_pngRemoveFile
//
// Closes and removes the PNG file if an error occurs.
//
static void V_pngRemoveFile(pngwrite_t *writeData, const char *filename)
{
   if(writeData->outf)
   {
      fclose(writeData->outf);
      writeData->outf = NULL;
      if(writeData->errorFlag)
      {
         int error = writeData->errnoVal;
         C_Printf(FC_ERROR "V_pngRemoveFile: IO error: %s\a", 
                  error ? strerror(error) : "unknown error");
      }
   }

   // Try to remove it.
   if(remove(filename) != 0)
   {
      C_Printf(FC_ERROR "V_pngRemoveFile: error deleting bad PNG file: %s\a",
               errno ? strerror(errno) : "unknown error");
   }
}

//
// V_WritePNG
//
// Write a linear graphic as a PNG file.
//
bool V_WritePNG(byte *linear, int width, int height, const char *filename)
{
   AutoPalette palcache(wGlobalDir);
   png_structp pngStruct;
   png_infop   pngInfo;
   png_colorp  pngPalette;
   pngwrite_t  writeData;
   bool        libpngError = false;
   bool        retval;
      
   byte  *palette;
   byte **rowpointers;

   // Load palette
   palette = palcache.get();

   // Open file for output
   if(!(writeData.outf = fopen(filename, "wb")))
   {
      C_Printf(FC_ERROR "V_WritePNG: could not open %s for write\a", filename);
      return false;
   }
   writeData.errorFlag = false;
   writeData.errnoVal  = 0;

   // Create the libpng write struct
   if(!(pngStruct = 
        png_create_write_struct(PNG_LIBPNG_VER_STRING, &writeData,
                                V_pngWriteError, V_pngWriteWarning)))
   {
      V_pngRemoveFile(&writeData, filename);
      return false;
   }

   // Create the libpng info struct
   if(!(pngInfo = png_create_info_struct(pngStruct)))
   {
      png_destroy_write_struct(&pngStruct, NULL);
      V_pngRemoveFile(&writeData, filename);
      return false;
   }

   // Allocate row pointers and palette
   rowpointers = ecalloc(byte **,    height, sizeof(byte *));
   pngPalette  = ecalloc(png_colorp, 256,    sizeof(png_color));

   try
   {
      // Set write functions
      png_set_write_fn(pngStruct, &writeData, V_pngDataWrite, V_pngDataFlush);

      // Set IHDR data
      png_set_IHDR(pngStruct, pngInfo, width, height, 8, PNG_COLOR_TYPE_PALETTE,
                   PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, 
                   PNG_FILTER_TYPE_DEFAULT);

      // Copy palette
      for(int color = 0; color < 256; color++)
      {
         pngPalette[color].red   = palette[color * 3 + 0];
         pngPalette[color].green = palette[color * 3 + 1];
         pngPalette[color].blue  = palette[color * 3 + 2];
      }
      png_set_PLTE(pngStruct, pngInfo, pngPalette, 256);

      // Write info
      png_write_info(pngStruct, pngInfo);

      // Set packing and swapping attributes as needed
      png_set_packing(pngStruct);
      png_set_packswap(pngStruct);

      // Set row pointers
      for(int row = 0; row < height; row++)
         rowpointers[row] = &linear[row * width];

      // Write image
      png_write_image(pngStruct, rowpointers);

      // Write end
      png_write_end(pngStruct, pngInfo);
   }
   catch(...)
   {
      libpngError = true;
   }

   // Remove the file if something went wrong.
   if(libpngError || writeData.errorFlag)
   {
      V_pngRemoveFile(&writeData, filename);
      retval = false;
   }
   else
   {
      fclose(writeData.outf);
      retval = true;
   }

   // Clean up.
   png_destroy_write_struct(&pngStruct, &pngInfo);
   efree(rowpointers);
   efree(pngPalette);

   return retval;
}

// EOF

