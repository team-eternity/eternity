// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2010 James Haley
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
//   Screenshots.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_gi.h"
#include "d_io.h"
#include "doomstat.h"
#include "m_buffer.h"
#include "m_misc.h"
#include "p_skin.h"
#include "s_sound.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

// jff 3/30/98: option to output screenshot as pcx or bmp
// haleyjd 12/28/09: selects from any number of formats now.
int screenshot_pcx; 

// haleyjd 11/16/04: allow disabling gamma correction in screenshots
int screenshot_gamma;

// jff 3/30/98 binary file write with error detection
// killough 10/98: changed into macro to return failure instead of aborting

#define SafeWrite(ob, data, size) \
   if(!ob->Write(data, size)) return false

#define SafeWrite32(ob, data) \
   if(!ob->WriteUint32(data)) return false

#define SafeWrite16(ob, data) \
   if(!ob->WriteUint16(data)) return false

#define SafeWrite8(ob, data) \
   if(!ob->WriteUint8(data)) return false

//=============================================================================
//
// PCX
//

// PCX header structure
typedef struct pcx_s
{
   uint8_t  manufacturer;
   uint8_t  version;
   uint8_t  encoding;
   uint8_t  bits_per_pixel;
   uint16_t xmin;
   uint16_t ymin;
   uint16_t xmax;
   uint16_t ymax;
   uint16_t hres;
   uint16_t vres;
   uint8_t  palette[48];
   uint8_t  reserved;
   uint8_t  color_planes;
   uint16_t bytes_per_line;
   uint16_t palette_type;
   uint8_t  filler[58];
} pcx_t;

//
// pcx_Writer
//
static bool pcx_Writer(OutBuffer *ob, byte *data, 
                       uint32_t width, uint32_t height, byte *palette)
{
   unsigned int i;
   pcx_t   pcx;
   byte    temppal[768], *palptr;

   // Setup PCX Header
   // haleyjd 09/27/07: Changed pcx.palette_type from 2 to 1.
   // According to authoritative ZSoft documentation, 1 = Color, 2 == Grayscale.

   memset(&pcx, 0, sizeof(pcx_t));

   pcx.manufacturer   = 0x0a; // PCX id
   pcx.version        = 5;    // 256 color
   pcx.encoding       = 1;    // uncompressed
   pcx.bits_per_pixel = 8;    // 8-bit
   pcx.xmin           = 0;
   pcx.ymin           = 0;
   pcx.xmax           = (uint16_t)(width - 1);
   pcx.ymax           = (uint16_t)(height - 1);
   pcx.hres           = (uint16_t)width;
   pcx.vres           = (uint16_t)height;
   pcx.color_planes   = 1; // chunky image
   pcx.bytes_per_line = (uint16_t)width;
   pcx.palette_type   = 1; // not a gray scale
   
   SafeWrite8( ob, pcx.manufacturer);
   SafeWrite8( ob, pcx.version);
   SafeWrite8( ob, pcx.encoding);
   SafeWrite8( ob, pcx.bits_per_pixel);
   SafeWrite16(ob, pcx.xmin);
   SafeWrite16(ob, pcx.ymin);
   SafeWrite16(ob, pcx.xmax);
   SafeWrite16(ob, pcx.ymax);
   SafeWrite16(ob, pcx.hres);
   SafeWrite16(ob, pcx.vres);
   SafeWrite(  ob, pcx.palette, sizeof(pcx.palette));
   SafeWrite8( ob, pcx.reserved);
   SafeWrite8( ob, pcx.color_planes);
   SafeWrite16(ob, pcx.bytes_per_line);
   SafeWrite16(ob, pcx.palette_type);
   SafeWrite(  ob, pcx.filler, sizeof(pcx.filler));

   // Pack the image
   for(i = 0; i < width*height; ++i)
   {
      if((*data & 0xc0) != 0xc0)
      {
         SafeWrite8(ob, *data);
         ++data;
      }
      else
      {
         if(!ob->WriteUint8(0xc1) || 
            !ob->WriteUint8(*data))
            return false;
         ++data;
      }
   }

   // Write the palette   
   SafeWrite8(ob, 0x0c); // palette ID byte

   // haleyjd 11/16/04: make gamma correction optional
   if(screenshot_gamma)
   {
      for(i = 0; i < 768; i += 3)
      {
         temppal[i+0] = gammatable[usegamma][palette[i+0]]; // killough
         temppal[i+1] = gammatable[usegamma][palette[i+1]];
         temppal[i+2] = gammatable[usegamma][palette[i+2]];
      }
      palptr = temppal;
   }
   else
      palptr = palette;

   SafeWrite(ob, palptr, 768);
     
   // Done!
   return true;
}

//=============================================================================
//
// BMP
//
// jff 3/30/98 types and data structures for BMP output of screenshots
// killough 5/2/98:
// Changed type names to avoid conflicts with endianess functions
//

#define BI_RGB 0L

// SoM 6/5/02: Chu-Chu-Chu-Chu-Chu-Changes... heh

typedef struct tagBITMAPFILEHEADER
{
  uint16_t bfType;
  uint32_t bfSize;
  uint16_t bfReserved1;
  uint16_t bfReserved2;
  uint32_t bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER
{
  uint32_t biSize;
  uint32_t biWidth;
  uint32_t biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  uint32_t biXPelsPerMeter;
  uint32_t biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
} BITMAPINFOHEADER;

//
// bmp_Writer
//
// jff 3/30/98 Add capability to write a .BMP file (256 color uncompressed)
//
static bool bmp_Writer(OutBuffer *ob, byte *data, 
                       uint32_t width, uint32_t height, byte *palette)
{
   unsigned int i, j, wid;
   BITMAPFILEHEADER bmfh;
   BITMAPINFOHEADER bmih;
   unsigned int fhsiz, ihsiz;
   byte temppal[1024];

   // haleyjd: use precomputed packed structure sizes
   fhsiz = 14; // sizeof(BITMAPFILEHEADER)
   ihsiz = 40; // sizeof(BITMAPINFOHEADER)
   wid   = 4 * ((width + 3) / 4);

   //jff 4/22/98 add endian macros
   bmfh.bfType      = 19778;
   bmfh.bfSize      = fhsiz + ihsiz + 256L * 4 + width * height;
   bmfh.bfReserved1 = 0;
   bmfh.bfReserved2 = 0;
   bmfh.bfOffBits   = fhsiz + ihsiz + 256L * 4;

   bmih.biSize          = ihsiz;
   bmih.biWidth         = width;
   bmih.biHeight        = height;
   bmih.biPlanes        = 1;
   bmih.biBitCount      = 8;
   bmih.biCompression   = BI_RGB;
   bmih.biSizeImage     = wid * height;
   bmih.biXPelsPerMeter = 0;
   bmih.biYPelsPerMeter = 0;
   bmih.biClrUsed       = 256;
   bmih.biClrImportant  = 256;

   // Write the file header
   SafeWrite16(ob, bmfh.bfType);
   SafeWrite32(ob, bmfh.bfSize);
   SafeWrite16(ob, bmfh.bfReserved1);
   SafeWrite16(ob, bmfh.bfReserved2);
   SafeWrite32(ob, bmfh.bfOffBits);
      
   // Write the info header
   SafeWrite32(ob, bmih.biSize);
   SafeWrite32(ob, bmih.biWidth);
   SafeWrite32(ob, bmih.biHeight);
   SafeWrite16(ob, bmih.biPlanes);
   SafeWrite16(ob, bmih.biBitCount);
   SafeWrite32(ob, bmih.biCompression);
   SafeWrite32(ob, bmih.biSizeImage);
   SafeWrite32(ob, bmih.biXPelsPerMeter);
   SafeWrite32(ob, bmih.biYPelsPerMeter);
   SafeWrite32(ob, bmih.biClrUsed);
   SafeWrite32(ob, bmih.biClrImportant);

   // haleyjd 11/16/04: make gamma correction optional
   if(screenshot_gamma)
   {
      // write the palette, in blue-green-red order, gamma corrected
      for(i = j = 0; i < 768; i += 3, j += 4)
      {
         temppal[j+0] = gammatable[usegamma][palette[i+2]];
         temppal[j+1] = gammatable[usegamma][palette[i+1]];
         temppal[j+2] = gammatable[usegamma][palette[i+0]];
         temppal[j+3] = 0;
      }
   }
   else
   {
      for(i = j = 0; i < 768; i += 3, j += 4)
      {
         temppal[j+0] = palette[i+2];
         temppal[j+1] = palette[i+1];
         temppal[j+2] = palette[i+0];
         temppal[j+3] = 0;
      }
   }
   SafeWrite(ob, temppal, 1024);

   for(i = 0; i < height; ++i)
      SafeWrite(ob, data + (height-1-i)*width, wid);

   return true; // killough 10/98
}

//=============================================================================
//
// TGA
//
// haleyjd 12/28/09: Targa is such a simple format, there's basically no reason
// to not support it :)
//

typedef struct tgaheader_s
{
   uint8_t  idlength;        // size of image id field
   uint8_t  colormaptype;    // color map type
   uint8_t  imagetype;       // image type code
   uint16_t cmapstart;       // color map origin
   uint16_t cmaplength;      // color map length
   uint8_t  cmapdepth;       // depth of color map entries
   uint16_t xoffset;         // x origin of image
   uint16_t yoffset;         // y origin of image
   uint16_t width;           // width of image
   uint16_t height;          // height of image
   uint8_t  pixeldepth;      // image pixel size
   uint8_t  imagedescriptor; // image descriptor byte
} tgaheader_t;

//
// tga_Writer
//
// haleyjd 12/28/09
//
static bool tga_Writer(OutBuffer *ob, byte *data, 
                       uint32_t width, uint32_t height, byte *palette)
{
   tgaheader_t tga;
   unsigned int i;
   byte temppal[768];

   tga.idlength        = 0;                // no image identification field
   tga.colormaptype    = 1;                // colormapped image, palette is present
   tga.imagetype       = 1;                // data type 1 - uncompressed colormapped
   tga.cmapstart       = 0;                // palette begins at index 0
   tga.cmaplength      = 256;              // 256 color palette
   tga.cmapdepth       = 24;               // colormap entry bitdepth (24-bit)
   tga.xoffset         = 0;                // x offset 0
   tga.yoffset         = 0;                // y offset 0
   tga.width           = (uint16_t)width;  // width
   tga.height          = (uint16_t)height; // height
   tga.pixeldepth      = 8;                // 8 bits per pixel
   tga.imagedescriptor = 0;                // no special properties

   // Write header
   SafeWrite8( ob, tga.idlength);
   SafeWrite8( ob, tga.colormaptype);
   SafeWrite8( ob, tga.imagetype);
   SafeWrite16(ob, tga.cmapstart);
   SafeWrite16(ob, tga.cmaplength);
   SafeWrite8( ob, tga.cmapdepth);
   SafeWrite16(ob, tga.xoffset);
   SafeWrite16(ob, tga.yoffset);
   SafeWrite16(ob, tga.width);
   SafeWrite16(ob, tga.height);
   SafeWrite8( ob, tga.pixeldepth);
   SafeWrite8( ob, tga.imagedescriptor);

   // Write colormap
   if(screenshot_gamma)
   {
      for(i = 0; i < 768; i += 3)
      {
         temppal[i+0] = gammatable[usegamma][palette[i+2]];
         temppal[i+1] = gammatable[usegamma][palette[i+1]];
         temppal[i+2] = gammatable[usegamma][palette[i+0]];
      }
   }
   else
   {
      for(i = 0; i < 768; i += 3)
      {
         temppal[i+0] = palette[i+2];
         temppal[i+1] = palette[i+1];
         temppal[i+2] = palette[i+0];
      }
   }
   SafeWrite(ob, temppal, 768);

   // Write image data
   for(i = 0; i < height; ++i)
      SafeWrite(ob, data + (height-1-i)*width, width);

   return true;
}

//=============================================================================
//
// Shared Code
//

typedef bool (*ShotWriter_t)(OutBuffer *, byte *, uint32_t, uint32_t, byte *);

typedef struct shotformat_s
{
   const char   *extension; // file extension
   int           endian;    // endianness of file format
   ShotWriter_t  writer;    // writing method
} shotformat_t;

enum
{
   SHOT_BMP,
   SHOT_PCX,
   SHOT_TGA,
   SHOT_NUMSHOTFORMATS
};

static shotformat_t shotFormats[SHOT_NUMSHOTFORMATS] =
{
   { "bmp", OutBuffer::LENDIAN, bmp_Writer }, // Windows / OS/2 Bitmap
   { "pcx", OutBuffer::LENDIAN, pcx_Writer }, // ZSoft PC Paint
   { "tga", OutBuffer::LENDIAN, tga_Writer }, // Truevision TARGA
};

//
// M_ScreenShot
//
// Modified by Lee Killough so that any number of shots can be taken,
// the code is faster, and no annoying "screenshot" message appears.
//
// killough 10/98: improved error-handling
//
void M_ScreenShot(void)
{
   bool success = false;
   char   *path = NULL;
   size_t  len;
   OutBuffer ob;
   shotformat_t *format = &shotFormats[screenshot_pcx];
   
   errno = 0;

   len = M_StringAlloca(&path, 1, 6, basepath);

   // haleyjd 11/23/06: use basepath/shots
   psnprintf(path, len, "%s/shots", basepath);
   
   // haleyjd 05/23/02: corrected uses of access to use defined
   // constants rather than integers, some of which were not even
   // correct under DJGPP to begin with (it's a wonder it worked...)
   
   if(!access(path, W_OK))
   {
      static int shot;
      char *lbmname = NULL;
      int tries = 10000;

      len = M_StringAlloca(&lbmname, 2, 16, path, format->extension);
      
      do
      {
         // jff 3/30/98 pcx or bmp?
         // haleyjd: use format extension.
         psnprintf(lbmname, len, "%s/etrn%02d.%s", path, shot++, format->extension);
      }
      while(!access(lbmname, F_OK) && --tries);

      if(tries && ob.CreateFile(lbmname, 512*1024, format->endian))
      {
         // killough 4/18/98: make palette stay around
         // (PU_CACHE could cause crash)         
         byte *pal = (byte *)W_CacheLumpName("PLAYPAL", PU_STATIC);

         // get screen graphics
         V_BlitVBuffer(&backscreen2, 0, 0, &vbscreen, 0, 0, 
                       vbscreen.width, vbscreen.height);
         
         I_BeginRead();

         // killough 10/98: detect failure and remove file if error
         success = format->writer(&ob, backscreen2.data, 
                                  (uint32_t)(backscreen2.width), 
                                  (uint32_t)(backscreen2.height), 
                                  pal);

         // haleyjd: close the buffer
         ob.Close();

         // if not successful, remove the file now
         if(!success)
         {
            int t = errno;
            remove(lbmname);
            errno = t;
         }

         I_EndRead();

         // killough 4/18/98: now you can mark it PU_CACHE
         Z_ChangeTag(pal, PU_CACHE);
      }
   }

   // 1/18/98 killough: replace "SCREEN SHOT" acknowledgement with sfx
   // players[consoleplayer].message = "screen shot"
   
   // killough 10/98: print error message and change sound effect if error
   if(!success)
   {
      doom_printf("%s",
         errno ? strerror(errno) : FC_ERROR "Could not take screenshot");
      S_StartSound(NULL, GameModeInfo->playerSounds[sk_oof]);
   }
   else
      S_StartSound(NULL, GameModeInfo->c_BellSound);
}

// EOF

