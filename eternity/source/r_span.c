// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley, Steven McGranahan, et al.
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
//      The actual span drawing functions.
//      Here find the main potential for optimization,
//       e.g. inline assembly, different algorithms.
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "w_wad.h"
#include "r_draw.h"
#include "r_main.h"
#include "v_video.h"
#include "mn_engin.h"
#include "d_gi.h"

#define MAXWIDTH  MAX_SCREENWIDTH          /* kilough 2/8/98 */
#define MAXHEIGHT MAX_SCREENHEIGHT

//#ifdef DJGPP
//#define USEASM /* sf: changed #ifdef DJGPP to #ifdef USEASM */
//#endif

extern byte *ylookup[MAXHEIGHT]; 
extern int  columnofs[MAXWIDTH]; 


//
// R_DrawSpan_64
// With DOOM style restrictions on view orientation,
//  the floors and ceilings consist of horizontal slices
//  or spans with constant z depth.
// However, rotation around the world z axis is possible,
//  thus this mapping, while simpler and faster than
//  perspective correct texture mapping, has to traverse
//  the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
//  and the inner loop has to step in texture space u and v.
//

int  ds_y; 
int  ds_x1; 
int  ds_x2;

lighttable_t *ds_colormap; 

fixed_t ds_xfrac; 
fixed_t ds_yfrac; 
fixed_t ds_xstep; 
fixed_t ds_ystep;

// start of a 64*64 tile image 
byte *ds_source;        

#ifndef USEASM      // killough 2/15/98

// SoM: This is the high precision flat renderer, it will not distort the flat
// when looking down or dying.
static void R_DrawSpan_64(void) 
{ 
   unsigned xposition;
   unsigned yposition;
   unsigned xstep, ystep;
   
   byte *source;
   byte *colormap;
   byte *dest;
   
   unsigned count;
   
   // SoM: we only need 6 bits for the integer part (0 thru 63) so the rest
   // can be used for the fraction part. This allows calculation of the memory
   // address in the texture with two shifts, an OR and one AND. (see below)
   // for texture sizes > 64 the amount of precision we can allow will decrease,
   // but only by one bit per power of two (obviously)
   // Ok, because I was able to eliminate the variable spot below, this function
   // is now FASTER than doom's original span renderer. Whodathunkit?
   
   xposition = ds_xfrac << 10; yposition = ds_yfrac << 10;
   xstep = ds_xstep << 10; ystep = ds_ystep << 10;
   
   source = ds_source;
   colormap = ds_colormap;
   dest = ylookup[ds_y] + columnofs[ds_x1];       
   count = ds_x2 - ds_x1 + 1;
   
   while(count >= 4)
   {
      // SoM: Why didn't I see this earlier? the spot variable is a waste now
      // because we don't have the uber complicated math to calculate it now, 
      // so that was a memory write we didn't need!
      dest[0] = colormap[source[((yposition >> 20) & 0xFC0) | (xposition >> 26)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[1] = colormap[source[((yposition >> 20) & 0xFC0) | (xposition >> 26)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[2] = colormap[source[((yposition >> 20) & 0xFC0) | (xposition >> 26)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[3] = colormap[source[((yposition >> 20) & 0xFC0) | (xposition >> 26)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest += 4;
      count -= 4;
      
   }
   while(count--)
   { 
      *dest++ = colormap[source[((yposition >> 20) & 0xFC0) | (xposition >> 26)]]; 
      xposition += xstep;
      yposition += ystep;
   } 
}

// SoM: This is the high precision flat renderer, it will not distort the flat
// when looking down or dying.
static void R_DrawSpan_128(void) 
{ 
   unsigned xposition;
   unsigned yposition;
   unsigned xstep, ystep;
   
   byte *source;
   byte *colormap;
   byte *dest;
   
   unsigned count;
   
   // SoM: we only need 7 bits for the integer part (0 thru 127) so the rest
   // can be used for the fraction part. This allows calculation of the memory
   // address in the texture with two shifts, an OR and one AND. (see below)
   // for texture sizes > 128 the amount of precision we can allow will 
   // decrease, but only by one bit per power of two (obviously)
   // Ok, because I was able to eliminate the variable spot below, this function
   // is now FASTER than doom's original span renderer. Whodathunkit?

   xposition = ds_xfrac << 9; yposition = ds_yfrac << 9;
   xstep = ds_xstep << 9; ystep = ds_ystep << 9;
   
   source = ds_source;
   colormap = ds_colormap;
   dest = ylookup[ds_y] + columnofs[ds_x1];       
   count = ds_x2 - ds_x1 + 1;
   
   while (count >= 4)
   {
      // SoM: Why didn't I see this earlier? the spot variable is a waste now
      // because we don't have the uber complicated math to calculate it now, 
      // so that was a memory write we didn't need!
      dest[0] = colormap[source[((yposition >> 18) & 0x3F80) | (xposition >> 25)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[1] = colormap[source[((yposition >> 18) & 0x3F80) | (xposition >> 25)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[2] = colormap[source[((yposition >> 18) & 0x3F80) | (xposition >> 25)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[3] = colormap[source[((yposition >> 18) & 0x3F80) | (xposition >> 25)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest += 4;
      count -= 4;
      
   }
   while (count--)
   { 
      *dest++ = colormap[source[((yposition >> 18) & 0x3F80) | (xposition >> 25)]]; 
      xposition += xstep;
      yposition += ystep;
   } 
}

// SoM: This is the high precision flat renderer, it will not distort the flat
// when looking down or dying.
static void R_DrawSpan_256(void) 
{ 
   unsigned xposition;
   unsigned yposition;
   unsigned xstep, ystep;
   
   byte *source;
   byte *colormap;
   byte *dest;
   
   unsigned count;
   
   // SoM: we only need 8 bits for the integer part (0 thru 255) so the rest
   // can be used for the fraction part. This allows calculation of the memory
   // address in the texture with two shifts, an OR and one AND. (see below)
   // for texture sizes > 256 the amount of precision we can allow will 
   // decrease, but only by one bit per power of two (obviously)
   // Ok, because I was able to eliminate the variable spot below, this function
   // is now FASTER than doom's original span renderer. Whodathunkit?

   xposition = ds_xfrac << 8; yposition = ds_yfrac << 8;
   xstep = ds_xstep << 8; ystep = ds_ystep << 8;
   
   source = ds_source;
   colormap = ds_colormap;
   dest = ylookup[ds_y] + columnofs[ds_x1];       
   count = ds_x2 - ds_x1 + 1;
   
   while (count >= 4)
   {
      // SoM: Why didn't I see this earlier? the spot variable is a waste now
      // because we don't have the uber complicated math to calculate it now,
      // so that was a memory write we didn't need!
      dest[0] = colormap[source[((yposition >> 16) & 0xFF00) | (xposition >> 24)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[1] = colormap[source[((yposition >> 16) & 0xFF00) | (xposition >> 24)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[2] = colormap[source[((yposition >> 16) & 0xFF00) | (xposition >> 24)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[3] = colormap[source[((yposition >> 16) & 0xFF00) | (xposition >> 24)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest += 4;
      count -= 4;
      
   }
   while (count--)
   { 
      *dest++ = colormap[source[((yposition >> 16) & 0xFF00) | (xposition >> 24)]]; 
      xposition += xstep;
      yposition += ystep;
   } 
}

// SoM: This is the high precision flat renderer, it will not distort the flat
// when looking down or dying.
static void R_DrawSpan_512(void) 
{ 
   unsigned xposition;
   unsigned yposition;
   unsigned xstep, ystep;
   
   byte *source;
   byte *colormap;
   byte *dest;
   
   unsigned count;
   
   // SoM: we only need 8 bits for the integer part (0 thru 255) so the rest
   // can be used for the fraction part. This allows calculation of the memory
   // address in the texture with two shifts, an OR and one AND. (see below)
   // for texture sizes > 256 the amount of precision we can allow will decrease,
   // but only by one bit per power of two (obviously)
   // Ok, because I was able to eliminate the variable spot below, this function
   // is now FASTER than doom's original span renderer. Whodathunkit?
   
   xposition = ds_xfrac << 7; yposition = ds_yfrac << 7;
   xstep = ds_xstep << 7; ystep = ds_ystep << 7;
   
   source = ds_source;
   colormap = ds_colormap;
   dest = ylookup[ds_y] + columnofs[ds_x1];       
   count = ds_x2 - ds_x1 + 1;
   
   while (count >= 4)
   {
      // SoM: Why didn't I see this earlier? the spot variable is a waste now
      // because we don't have the uber complicated math to calculate it now, 
      // so that was a memory write we didn't need!
      dest[0] = colormap[source[((yposition >> 14) & 0x3FE00) | (xposition >> 23)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[1] = colormap[source[((yposition >> 14) & 0x3FE00) | (xposition >> 23)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[2] = colormap[source[((yposition >> 14) & 0x3FE00) | (xposition >> 23)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[3] = colormap[source[((yposition >> 14) & 0x3FE00) | (xposition >> 23)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest += 4;
      count -= 4;
      
   }
   while (count--)
   { 
      *dest++ = colormap[source[((yposition >> 14) & 0x3FE00) | (xposition >> 23)]]; 
      xposition += xstep;
      yposition += ystep;
   } 
}

// SoM: Archive
// This is the optimized version of the original flat drawing function.
static void R_DrawSpan_OLD(void) 
{ 
   register unsigned position;
   unsigned step;
   
   byte *source;
   byte *colormap;
   byte *dest;
   
   unsigned count;
   
   position = ((ds_xfrac<<10)&0xffff0000) | ((ds_yfrac>>6)&0xffff);
   step = ((ds_xstep<<10)&0xffff0000) | ((ds_ystep>>6)&0xffff);
   
   source = ds_source;
   colormap = ds_colormap;
   dest = ylookup[ds_y] + columnofs[ds_x1];       
   count = ds_x2 - ds_x1 + 1; 
   
   // SoM: So I went back and realized the error of ID's ways and this is even
   // faster now! This is by far the fastest way because it uses the exact same
   // number of ops mine does except it only has one addition and one memory 
   // write for the position variable. Now we only have two writes to memory
   // (one for the pixel, one for position) 
   while (count >= 4)
   { 
      dest[0] = colormap[source[(position>>26) | ((position>>4) & 4032)]]; 
      position += step;

      dest[1] = colormap[source[(position>>26) | ((position>>4) & 4032)]]; 
      position += step;
        
      dest[2] = colormap[source[(position>>26) | ((position>>4) & 4032)]]; 
      position += step;
        
      dest[3] = colormap[source[(position>>26) | ((position>>4) & 4032)]]; 
      position += step;
                
      dest += 4;
      count -= 4;
    } 

   while (count)
   { 
      *dest++ = colormap[source[(position>>26) | ((position>>4) & 4032)]]; 
      position += step;
      count--;
   } 
}

static void R_DrawSpan_ORIGINAL(void) 
{ 
   register unsigned position;
   unsigned step;
   
   byte *source;
   byte *colormap;
   byte *dest;
   
   unsigned count;
   unsigned spot; 
   unsigned xtemp;
   unsigned ytemp;
   
   position = ((ds_xfrac<<10)&0xffff0000) | ((ds_yfrac>>6)&0xffff);
   step = ((ds_xstep<<10)&0xffff0000) | ((ds_ystep>>6)&0xffff);
   
   source = ds_source;
   colormap = ds_colormap;
   dest = ylookup[ds_y] + columnofs[ds_x1];       
   count = ds_x2 - ds_x1 + 1; 
   
   while (count >= 4)
   { 
      ytemp = position>>4;
      ytemp = ytemp & 4032;
      xtemp = position>>26;
      spot = xtemp | ytemp;
      position += step;
      dest[0] = colormap[source[spot]]; 
      
      ytemp = position>>4;
      ytemp = ytemp & 4032;
      xtemp = position>>26;
      spot = xtemp | ytemp;
      position += step;
      dest[1] = colormap[source[spot]];
      
      ytemp = position>>4;
      ytemp = ytemp & 4032;
      xtemp = position>>26;
      spot = xtemp | ytemp;
      position += step;
      dest[2] = colormap[source[spot]];
      
      ytemp = position>>4;
      ytemp = ytemp & 4032;
      xtemp = position>>26;
      spot = xtemp | ytemp;
      position += step;
      dest[3] = colormap[source[spot]]; 
      
      dest += 4;
      count -= 4;
   } 
   
   while (count)
   { 
      ytemp = position>>4;
      ytemp = ytemp & 4032;
      xtemp = position>>26;
      spot = xtemp | ytemp;
      position += step;
      *dest++ = colormap[source[spot]]; 
      count--;
   } 
}

// olpspandrawer: uses the old low-precision span drawing routine for
// 64x64 flats. All other sizes use the normal routines.
spandrawer_t r_olpspandrawer =
{
   R_DrawSpan_ORIGINAL,
   R_DrawSpan_128,
   R_DrawSpan_256,
   R_DrawSpan_512
};

// lpspandrawer: uses the optimized but low-precision span drawing
// routine for 64x64 flats. Same as above for others.
spandrawer_t r_lpspandrawer =
{
   R_DrawSpan_OLD,
   R_DrawSpan_128,
   R_DrawSpan_256,
   R_DrawSpan_512
};

// the normal, high-precision span drawer
spandrawer_t r_spandrawer =
{
   R_DrawSpan_64,
   R_DrawSpan_128,
   R_DrawSpan_256,
   R_DrawSpan_512
};

//==============================================================================
//
// Low-Detail Span Drawers
//
// haleyjd 09/10/06: These are for low-detail mode. They use the high-precision
// drawing code but double up on pixels, making it blocky.
//

static void R_DrawSpan_LD64(void) 
{ 
   unsigned xposition;
   unsigned yposition;
   unsigned xstep, ystep;
   
   byte *source;
   byte *colormap;
   byte *dest;
   
   unsigned count;
      
   xposition = ds_xfrac << 10; yposition = ds_yfrac << 10;
   xstep = ds_xstep << 10; ystep = ds_ystep << 10;
   
   source = ds_source;
   colormap = ds_colormap;
   count = ds_x2 - ds_x1 + 1;

   // low detail: must double x coordinate
   ds_x1 <<= 1;
   dest = ylookup[ds_y] + columnofs[ds_x1];       
   
   while(count >= 4)
   {
      dest[0] = dest[1] = 
         colormap[source[((yposition >> 20) & 0xFC0) | (xposition >> 26)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[2] = dest[3] = 
         colormap[source[((yposition >> 20) & 0xFC0) | (xposition >> 26)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[4] = dest[5] = 
         colormap[source[((yposition >> 20) & 0xFC0) | (xposition >> 26)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[6] = dest[7] = 
         colormap[source[((yposition >> 20) & 0xFC0) | (xposition >> 26)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest += 8;
      count -= 4;
      
   }
   while(count--)
   { 
      *dest = *(dest + 1) = 
         colormap[source[((yposition >> 20) & 0xFC0) | (xposition >> 26)]]; 
      dest += 2;
      
      xposition += xstep;
      yposition += ystep;
   } 
}

static void R_DrawSpan_LD128(void) 
{ 
   unsigned xposition;
   unsigned yposition;
   unsigned xstep, ystep;
   
   byte *source;
   byte *colormap;
   byte *dest;
   
   unsigned count;
   
   xposition = ds_xfrac << 9; yposition = ds_yfrac << 9;
   xstep = ds_xstep << 9; ystep = ds_ystep << 9;
   
   source = ds_source;
   colormap = ds_colormap;
   dest = ylookup[ds_y] + columnofs[ds_x1];       
   count = ds_x2 - ds_x1 + 1;

   // low detail: must double x coordinate
   ds_x1 <<= 1;
   dest = ylookup[ds_y] + columnofs[ds_x1];       
   
   while(count >= 4)
   {
      dest[0] = dest[1] =
         colormap[source[((yposition >> 18) & 0x3F80) | (xposition >> 25)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[2] = dest[3] =
         colormap[source[((yposition >> 18) & 0x3F80) | (xposition >> 25)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[4] = dest[5] =
         colormap[source[((yposition >> 18) & 0x3F80) | (xposition >> 25)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[6] = dest[7] =
         colormap[source[((yposition >> 18) & 0x3F80) | (xposition >> 25)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest += 8;
      count -= 4;
   }
   while (count--)
   { 
      *dest = *(dest + 1) =
         colormap[source[((yposition >> 18) & 0x3F80) | (xposition >> 25)]]; 
      dest += 2;
      xposition += xstep;
      yposition += ystep;
   } 
}

static void R_DrawSpan_LD256(void) 
{ 
   unsigned xposition;
   unsigned yposition;
   unsigned xstep, ystep;
   
   byte *source;
   byte *colormap;
   byte *dest;
   
   unsigned count;
   
   xposition = ds_xfrac << 8; yposition = ds_yfrac << 8;
   xstep = ds_xstep << 8; ystep = ds_ystep << 8;
   
   source = ds_source;
   colormap = ds_colormap;
   count = ds_x2 - ds_x1 + 1;

   // low detail: must double x coordinate
   ds_x1 <<= 1;
   dest = ylookup[ds_y] + columnofs[ds_x1];       
   
   while(count >= 4)
   {
      dest[0] = dest[1] =
         colormap[source[((yposition >> 16) & 0xFF00) | (xposition >> 24)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[2] = dest[3] =
         colormap[source[((yposition >> 16) & 0xFF00) | (xposition >> 24)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[4] = dest[5] =
         colormap[source[((yposition >> 16) & 0xFF00) | (xposition >> 24)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[6] = dest[7] =
         colormap[source[((yposition >> 16) & 0xFF00) | (xposition >> 24)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest += 8;
      count -= 4;
      
   }
   while(count--)
   { 
      *dest = *(dest + 1) =
         colormap[source[((yposition >> 16) & 0xFF00) | (xposition >> 24)]]; 
      dest += 2;
      xposition += xstep;
      yposition += ystep;
   } 
}

static void R_DrawSpan_LD512(void) 
{ 
   unsigned xposition;
   unsigned yposition;
   unsigned xstep, ystep;
   
   byte *source;
   byte *colormap;
   byte *dest;
   
   unsigned count;
      
   xposition = ds_xfrac << 7; yposition = ds_yfrac << 7;
   xstep = ds_xstep << 7; ystep = ds_ystep << 7;
   
   source = ds_source;
   colormap = ds_colormap;
   count = ds_x2 - ds_x1 + 1;

   // low detail: must double x coordinate
   ds_x1 <<= 1;
   dest = ylookup[ds_y] + columnofs[ds_x1];       
   
   while(count >= 4)
   {
      dest[0] = dest[1] =
         colormap[source[((yposition >> 14) & 0x3FE00) | (xposition >> 23)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[2] = dest[3] =
         colormap[source[((yposition >> 14) & 0x3FE00) | (xposition >> 23)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[4] = dest[5] =
         colormap[source[((yposition >> 14) & 0x3FE00) | (xposition >> 23)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest[6] = dest[7] =
         colormap[source[((yposition >> 14) & 0x3FE00) | (xposition >> 23)]]; 
      xposition += xstep;
      yposition += ystep;
      
      dest += 8;
      count -= 4;
      
   }
   while(count--)
   { 
      *dest = *(dest + 1) =
         colormap[source[((yposition >> 14) & 0x3FE00) | (xposition >> 23)]]; 
      dest += 2;
      xposition += xstep;
      yposition += ystep;
   } 
}

// low-detail spandrawer
spandrawer_t r_lowspandrawer =
{
   R_DrawSpan_LD64,
   R_DrawSpan_LD128,
   R_DrawSpan_LD256,
   R_DrawSpan_LD512
};

#endif

// EOF

