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

#include <sys/stat.h>

#include "z_zone.h"
#include "d_io.h"
#include "d_gi.h"
#include "s_sound.h"
#include "w_wad.h"
#include "m_buffer.h"
#include "m_misc.h"

// jff 3/30/98: option to output screenshot as pcx or bmp
// haleyjd 12/28/09: selects from any number of formats now.
int screenshot_pcx; 

//=============================================================================
//
// PCX
//

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

// PCX header structure
struct pcx_s
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
} __attribute__((packed));

typedef struct pcx_s pcx_t;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

//
// pcx_Writer
//
static boolean pcx_Writer(outbuffer_t *ob, byte *data, int width, int height, 
                          byte *palette)
{
   int     i;
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
   pcx.xmax           = SwapShort((short)(width-1));
   pcx.ymax           = SwapShort((short)(height-1));
   pcx.hres           = SwapShort((short)width);
   pcx.vres           = SwapShort((short)height);
   pcx.color_planes   = 1; // chunky image
   pcx.bytes_per_line = SwapShort((short)width);
   pcx.palette_type   = SwapShort(1); // not a gray scale
   
   // Write header to buffer
   if(!M_BufferWrite(ob, &pcx, sizeof(pcx_t)))
      return false;

   // Pack the image
   for(i = 0; i < width*height; ++i)
   {
      if((*data & 0xc0) != 0xc0)
      {
         if(!M_BufferWriteUint8(ob, *data))
            return false;
         ++data;
      }
      else
      {
         if(!M_BufferWriteUint8(ob, 0xc1) || 
            !M_BufferWriteUint8(ob, *data))
            return false;
         ++data;
      }
   }

   // Write the palette   
   if(!M_BufferWriteUint8(ob, 0x0c)) // palette ID byte
      return false;

   // haleyjd 11/16/04: make gamma correction optional
   if(screenshot_gamma)
   {
      palptr = temppal;
      for(i = 0; i < 768; ++i)
      {
         *palptr++ = gammatable[usegamma][*palette];   // killough
         ++palette;
      }
      palptr = temppal; // reset to beginning
   }
   else
      palptr = palette;

   if(!M_BufferWrite(ob, palptr, 768))
      return false;
     
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
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

struct tagBITMAPFILEHEADER
{
  uint16_t bfType;
  uint32_t bfSize;
  uint16_t bfReserved1;
  uint16_t bfReserved2;
  uint32_t bfOffBits;
} __attribute__((packed));

typedef struct tagBITMAPFILEHEADER BITMAPFILEHEADER;

struct tagBITMAPINFOHEADER
{
  uint32_t biSize;
  int32_t  biWidth;
  int32_t  biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  int32_t  biXPelsPerMeter;
  int32_t  biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
} __attribute__((packed));

typedef struct tagBITMAPINFOHEADER BITMAPINFOHEADER;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

// jff 3/30/98 binary file write with error detection
// killough 10/98: changed into macro to return failure instead of aborting

#define SafeWrite(ob, data, size) \
   do { if(!M_BufferWrite(ob, data, size)) return false; } while(0)

//
// bmp_Writer
//
// jff 3/30/98 Add capability to write a .BMP file (256 color uncompressed)
//
static boolean bmp_Writer(outbuffer_t *ob, byte *data, int width, int height, 
                          byte *palette)
{
   int i, j, wid;
   BITMAPFILEHEADER bmfh;
   BITMAPINFOHEADER bmih;
   int fhsiz, ihsiz;
   byte temppal[1024];

   fhsiz = sizeof(BITMAPFILEHEADER);
   ihsiz = sizeof(BITMAPINFOHEADER);
   wid   = 4 * ((width + 3) / 4);

   //jff 4/22/98 add endian macros
   bmfh.bfType      = SwapShort(19778);
   bmfh.bfSize      = SwapLong(fhsiz + ihsiz + 256L * 4 + width * height);
   bmfh.bfReserved1 = 0;
   bmfh.bfReserved2 = 0;
   bmfh.bfOffBits   = SwapLong(fhsiz + ihsiz + 256L * 4);

   bmih.biSize          = SwapLong(ihsiz);
   bmih.biWidth         = SwapLong(width);
   bmih.biHeight        = SwapLong(height);
   bmih.biPlanes        = SwapShort(1);
   bmih.biBitCount      = SwapShort(8);
   bmih.biCompression   = BI_RGB;
   bmih.biSizeImage     = SwapLong(wid*height);
   bmih.biXPelsPerMeter = 0;
   bmih.biYPelsPerMeter = 0;
   bmih.biClrUsed       = SwapLong(256);
   bmih.biClrImportant  = SwapLong(256);

   // Write the header
   SafeWrite(ob, &bmfh.bfType,      sizeof(bmfh.bfType));
   SafeWrite(ob, &bmfh.bfSize,      sizeof(bmfh.bfSize));
   SafeWrite(ob, &bmfh.bfReserved1, sizeof(bmfh.bfReserved1));
   SafeWrite(ob, &bmfh.bfReserved2, sizeof(bmfh.bfReserved2));
   SafeWrite(ob, &bmfh.bfOffBits,   sizeof(bmfh.bfOffBits));
      
   SafeWrite(ob, &bmih.biSize,          sizeof(bmih.biSize));
   SafeWrite(ob, &bmih.biWidth,         sizeof(bmih.biWidth));
   SafeWrite(ob, &bmih.biHeight,        sizeof(bmih.biHeight));
   SafeWrite(ob, &bmih.biPlanes,        sizeof(bmih.biPlanes));
   SafeWrite(ob, &bmih.biBitCount,      sizeof(bmih.biBitCount));
   SafeWrite(ob, &bmih.biCompression,   sizeof(bmih.biCompression));
   SafeWrite(ob, &bmih.biSizeImage,     sizeof(bmih.biSizeImage));
   SafeWrite(ob, &bmih.biXPelsPerMeter, sizeof(bmih.biXPelsPerMeter));
   SafeWrite(ob, &bmih.biYPelsPerMeter, sizeof(bmih.biYPelsPerMeter));
   SafeWrite(ob, &bmih.biClrUsed,       sizeof(bmih.biClrUsed));
   SafeWrite(ob, &bmih.biClrImportant,  sizeof(bmih.biClrImportant));

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
      SafeWrite(ob, data + (height-1-i)*width, (unsigned)wid);

   return true; // killough 10/98
}

//=============================================================================
//
// Shared Code
//

typedef boolean (*ShotWriter_t)(outbuffer_t *, byte *, int, int, byte *);

typedef struct shotformat_s
{
   const char   *extension; // file extension
   ShotWriter_t  writer;    // writing method
} shotformat_t;

enum
{
   SHOT_BMP,
   SHOT_PCX,
   SHOT_NUMSHOTFORMATS
};

static shotformat_t shotFormats[SHOT_NUMSHOTFORMATS] =
{
   { "bmp", bmp_Writer }, // Windows / OS/2 Bitmap (BMP)
   { "pcx", pcx_Writer }, // ZSoft PC Paint (PCX)
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
   boolean success = false;
   char   *path = NULL;
   size_t  len;
   outbuffer_t ob;
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

      if(tries && M_BufferCreateFile(&ob, lbmname, 512*1024))
      {
         // killough 4/18/98: make palette stay around
         // (PU_CACHE could cause crash)         
         byte *pal = W_CacheLumpName("PLAYPAL", PU_STATIC);

         // get screen graphics
         V_BlitVBuffer(&backscreen2, 0, 0, &vbscreen, 0, 0, 
                       vbscreen.width, vbscreen.height);
         
         I_BeginRead();

         // killough 10/98: detect failure and remove file if error
         success = format->writer(&ob, backscreen2.data, 
                                  backscreen2.width, backscreen2.height, pal);

         // haleyjd: close the buffer
         M_BufferClose(&ob);

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

