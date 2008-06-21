// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2004 James Haley
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
//  Functions to manipulate linear blocks of graphics data.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "v_video.h"

//
// V_DrawBlock Implementors
//
// * V_BlockDrawer   -- unscaled
// * V_BlockDrawer2x -- fast 2x scaling
// * V_BlockDrawerS  -- general scaling

static void V_BlockDrawer(int x, int y, VBuffer *buffer, int width, int height, byte *src)
{
   byte *dest;

#ifdef RANGECHECK
   if(x < 0 || x + width > buffer->width ||
      y < 0 || y + height > buffer->height)
   {
      I_Error("V_BlockDrawer: block exceeds buffer boundaries.\n");
   }
#endif

   dest = buffer->data + y * buffer->pitch + x;
   
   while(height--)
   {
      memcpy(dest, src, width);
      src += width;
      dest += buffer->pitch;
   }
}

static void V_BlockDrawer2x(int x, int y, VBuffer *buffer, int width, int height, byte *src)
{
   byte *dest;

#ifdef RANGECHECK
   if(x < 0 || x + width > buffer->width/2 ||
      y < 0 || y + height > buffer->height/2)
   {
      I_Error("V_BlockDrawer2x: block exceeds buffer boundaries.\n");
   }
#endif

   dest = buffer->data + y * (buffer->pitch << 1) + (x << 1);
   
   if(width)
   {
      while(height--)
      {
         byte *d = dest;
         int t = width;
         do
         {
            d[buffer->pitch] 
               = d[buffer->pitch + 1] 
               = d[0] 
               = d[1] 
               = *src++;
         } while(d += 2, --t);
         
         dest += (buffer->pitch << 1);
      }
   }
}

static void V_BlockDrawerS(int x, int y, VBuffer *buffer, int width, int height, byte *src)
{
   byte *dest, *row;
   fixed_t xstep, ystep, xfrac, yfrac;
   int xtex, ytex, w, h, i, realx, realy;
      
   realx = buffer->x1lookup[x];
   realy = buffer->y1lookup[y];
   w     = buffer->x2lookup[x + width - 1] - realx + 1;
   h     = buffer->y2lookup[y + height - 1] - realy + 1;
   xstep = buffer->ixscale;
   ystep = buffer->iyscale;
   yfrac = 0;
   dest  = buffer->data + realy * buffer->pitch + realx;

#ifdef RANGECHECK
   if(realx < 0 || realx + w > buffer->width ||
      realy < 0 || realy + h > buffer->height)
   {
      I_Error("V_BlockDrawerS: block exceeds buffer boundaries.\n");
   }
#endif

   while(h--)
   {
      row = dest;
      i = w;
      xfrac = 0;
      ytex = (yfrac >> FRACBITS) * width;
      
      while(i--)
      {
         xtex = (xfrac >> FRACBITS);
         *row++ = src[ytex + xtex];
         xfrac += xstep;
      }

      dest += buffer->pitch;
      yfrac += ystep;
   }
}

//
// Color block drawing
//

//
// V_ColorBlock
//
// Draws a block of solid color.
//
void V_ColorBlock(VBuffer *buffer, byte color, int x, int y, int w, int h)
{
   byte *dest;

#ifdef RANGECHECK
   if(x < 0 || x + w > buffer->width || y < 0 || y + h > buffer->height)
      I_Error("V_ColorBlock: block exceeds buffer boundaries.\n");
#endif

   dest = buffer->data + y * buffer->pitch + x;
   
   while(h--)
   {
      memset(dest, color, w);
      dest += buffer->pitch;
   }
}

//
// V_ColorBlockTL
//
// Draws a block of solid color with alpha blending.
//
void V_ColorBlockTL(VBuffer *buffer, byte color, int x, int y, 
                    int w, int h, int tl)
{
   byte *dest, *row;
   register int tw;
   register unsigned int col;
   unsigned int *fg2rgb, *bg2rgb;

#ifdef RANGECHECK
   if(x < 0 || x + w > buffer->width || y < 0 || y + h > buffer->height)
      I_Error("V_ColorBlockTL: block exceeds buffer boundaries.\n");
#endif

   // haleyjd 10/15/05: optimizations for opaque and invisible
   if(tl == FRACUNIT)
   {
      V_ColorBlock(buffer, color, x, y, w, h);
      return;
   }
   else if(tl == 0)
      return;

   {
      unsigned int fglevel, bglevel;

      fglevel = tl & ~0x3ff;
      bglevel = FRACUNIT - fglevel;
      fg2rgb  = Col2RGB8[fglevel >> 10];
      bg2rgb  = Col2RGB8[bglevel >> 10];
   }

   dest = buffer->data + y * buffer->pitch + x;
   
   while(h--)
   { 
      row = dest;      
      tw = w;

      while(tw--)
      {
         col    = (fg2rgb[color] + bg2rgb[*row]) | 0x1f07c1f;
         *row++ = RGB32k[0][0][col & (col >> 15)];
      }

      dest += buffer->pitch;
   }
}

//
// V_TileFlat implementors
//

//
// Unscaled
//
// Works for any video mode.
//
static void V_TileBlock64(VBuffer *buffer, byte *src)
{
   int x, y;
   byte *row, *dest = buffer->data;
   int wmod;

   // if width % 64 != 0, we must do some extra copying at the end
   if((wmod = buffer->width & 63))
   {
      for(y = 0; y < buffer->height; ++y)
      {
         row = dest;
         for(x = 0; x < buffer->width >> 6; ++x)
         {
            memcpy(row, src + ((y & 63) << 6), 64);
            row += 64;
         }
         memcpy(row, src + ((y & 63) << 6), wmod);
         dest += buffer->pitch;
      }
   }
   else
   {
      for(y = 0; y < buffer->height; ++y)
      {
         row = dest;         
         for(x = 0; x < buffer->width >> 6; ++x)
         {
            memcpy(row, src + ((y & 63) << 6), 64);
            row += 64;
         }
         dest += buffer->pitch;
      }
   }
}

//
//
// Scaled 2x
//
// NOTE: This version ONLY works for 640x400 buffers.
//
static void V_TileBlock64_2x(VBuffer *buffer, byte *src)
{
   int x, y;
   byte *back_dest = buffer->data;
   byte *back_src  = src;

   for(y = 0; y < SCREENHEIGHT; src = ((++y & 63) << 6) + back_src,
       back_dest += buffer->pitch)
   {
      for(x = 0; x < SCREENWIDTH/64; ++x)
      {
         int i = 63;

         do
         {
            back_dest[i * 2]
               = back_dest[i * 2 + SCREENWIDTH * 2]
               = back_dest[i * 2 + 1]
               = back_dest[i * 2 + SCREENWIDTH * 2 + 1]
               = src[i];
         } while(--i >= 0);
         back_dest += 128;
      }
   }
}

//
// General scaling
//
static void V_TileBlock64S(VBuffer *buffer, byte *src)
{
   byte *dest, *row;
   fixed_t xstep, ystep, xfrac, yfrac = 0;
   int xtex, ytex, w, h;
   
   w = buffer->width;
   h = buffer->height;
   xstep = buffer->ixscale;
   ystep = buffer->iyscale;
   
   dest = buffer->data;

   while(h--)
   {
      int i = w;
      row = dest;
      xfrac = 0;
      ytex = ((yfrac >> FRACBITS) & 63) << 6;
      
      while(i--)
      {
         xtex = (xfrac >> FRACBITS) & 63;
         *row++ = src[ytex + xtex];
         xfrac += xstep;
      }
      
      yfrac += ystep;
      dest += buffer->pitch;
   }
}

//
// V_SetBlockFuncs
//
// Sets the block drawing function pointers in a VBuffer object
// based on the size of the buffer. Called from V_SetupBufferFuncs.
//
void V_SetBlockFuncs(VBuffer *buffer, int drawtype)
{
   switch(drawtype)
   {
   case DRAWTYPE_UNSCALED:
      buffer->BlockDrawer = V_BlockDrawer;
      buffer->TileBlock64 = V_TileBlock64;
      break;
   case DRAWTYPE_2XSCALED:
      buffer->BlockDrawer = V_BlockDrawer2x;
      // 2x version of V_TileBlock only works for 640x400
      if(buffer->width == 640 && buffer->height == 400)
         buffer->TileBlock64 = V_TileBlock64_2x;
      else
         buffer->TileBlock64 = V_TileBlock64S;
      break;
   case DRAWTYPE_GENSCALED:
      buffer->BlockDrawer = V_BlockDrawerS;
      buffer->TileBlock64 = V_TileBlock64S;
      break;
   default:
      break;
   }
}

// EOF

