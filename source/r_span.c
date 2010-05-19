// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley, Stephen McGranahan, et al.
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
#include "r_plane.h"

#define MAXWIDTH  MAX_SCREENWIDTH          /* kilough 2/8/98 */
#define MAXHEIGHT MAX_SCREENHEIGHT

extern byte *ylookup[MAXHEIGHT]; 
extern int  columnofs[MAXWIDTH]; 

//==============================================================================
//
// Span Macros
//

//
// SPAN_PROLOGUE_8
//
// 8-bit prologue code. This is shared between all span drawers.
//
#define SPAN_PROLOGUE_8() \
   unsigned int xf = span.xfrac, xs = span.xstep; \
   unsigned int yf = span.yfrac, ys = span.ystep; \
   register byte *dest; \
   byte *source = (byte *)span.source; \
   lighttable_t *colormap = span.colormap; \
   int count = span.x2 - span.x1 + 1;

//
// TL_SPAN_PROLOGUE_8
//
// Extra prologue for translucent spans
//
#define TL_SPAN_PROLOGUE_8() \
   unsigned int t; \
   SPAN_PROLOGUE_8()

//
// ADD_SPAN_PROLOGUE_8
//
// Extra prologue for additive spans
//
#define ADD_SPAN_PROLOGUE_8() \
   unsigned int a, b; \
   SPAN_PROLOGUE_8()

//
// SPAN_PRIMEDEST_8
//
// Should follow prologue code.
// Sets up the destination pointer.
//
#define SPAN_PRIMEDEST_8() dest = ylookup[span.y] + columnofs[span.x1];

//
// LD_SPAN_PRIMEDEST_8
//
// For low detail. The x coordinate is multiplied by a factor of two.
//
#define LD_SPAN_PRIMEDEST_8() dest = ylookup[span.y] + columnofs[span.x1 << 1];

//
// PUTPIXEL_8
//
// Normal pixel write, and step to the next texel.
//
#define PUTPIXEL_8(i, xshift, yshift, xmask) \
   dest[i] = colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]; \
   xf += xs; \
   yf += ys;

//
// PUTPIXEL_EXTRA_8
//
// Pixel write for wrap-up after unrolled loop.
//
#define PUTPIXEL_EXTRA_8(xshift, yshift, xmask) \
   *dest++ = colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]; \
   xf += xs; \
   yf += ys;

//
// TL_PUTPIXEL_8
//
// Translucent pixel write.
//
#define TL_PUTPIXEL_8(i, xshift, yshift, xmask) \
   t = span.bg2rgb[dest[i]] + \
       span.fg2rgb[colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]]; \
   t |= 0x01f07c1f; \
   dest[i] = RGB32k[0][0][t & (t >> 15)]; \
   xf += xs; \
   yf += ys;

//
// TL_PUTPIXEL_EXTRA_8
//
#define TL_PUTPIXEL_EXTRA_8(xshift, yshift, xmask) \
   t = span.bg2rgb[*dest] + \
       span.fg2rgb[colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]]; \
   t |= 0x01f07c1f; \
   *dest++ = RGB32k[0][0][t & (t >> 15)]; \
   xf += xs; \
   yf += ys;

//
// ADD_PUTPIXEL_8
//
// Additive blend pixel write.
//
#define ADD_PUTPIXEL_8(i, xshift, yshift, xmask) \
   a = span.bg2rgb[dest[i]] + \
       span.fg2rgb[colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]]; \
   b = a; \
   a |= 0x01f07c1f; \
   b &= 0x40100400; \
   a &= 0x3fffffff; \
   b  = b - (b >> 5); \
   a |= b; \
   dest[i] = RGB32k[0][0][a & (a >> 15)]; \
   xf += xs; \
   yf += ys;

//
// ADD_PUTPIXEL_EXTRA_8
//
#define ADD_PUTPIXEL_EXTRA_8(xshift, yshift, xmask) \
   a = span.bg2rgb[*dest] + \
       span.fg2rgb[colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]]; \
   b = a; \
   a |= 0x01f07c1f; \
   b &= 0x40100400; \
   a &= 0x3fffffff; \
   b  = b - (b >> 5); \
   a |= b; \
   *dest++ = RGB32k[0][0][a & (a >> 15)]; \
   xf += xs; \
   yf += ys;

//
// LD_PUTPIXEL_8
//
// Low detail pixel write.
// This has been made slightly suboptimal in interest of using the same loop macro.
// This is only supported as a gimmick and so I don't really care too much ;)
// Two writes are accomplished at once due to half-width rendering.
//
#define LD_PUTPIXEL_8(i, xshift, yshift, xmask) \
   dest[i<<1] = dest[(i<<1)+1] = colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]; \
   xf += xs; \
   yf += ys;

//
// LD_PUTPIXEL_EXTRA_8
//
#define LD_PUTPIXEL_EXTRA_8(xshift, yshift, xmask) \
   dest[0] = dest[1] = colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]; \
   dest += 2; \
   xf += xs; \
   yf += ys;

//
// LDTL_PUTPIXEL_8
//
#define LDTL_PUTPIXEL_8(i, xshift, yshift, xmask) \
   t = span.bg2rgb[dest[i<<1]] + \
       span.fg2rgb[colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]]; \
   t |= 0x01f07c1f; \
   dest[i<<1] = dest[(i<<1)+1] = RGB32k[0][0][t & (t >> 15)]; \
   xf += xs; \
   yf += ys;

//
// LDTL_PUTPIXEL_EXTRA_8
//
#define LDTL_PUTPIXEL_EXTRA_8(xshift, yshift, xmask) \
   t = span.bg2rgb[*dest] + \
       span.fg2rgb[colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]]; \
   t |= 0x01f07c1f; \
   dest[0] = dest[1] = RGB32k[0][0][t & (t >> 15)]; \
   dest += 2; \
   xf += xs; \
   yf += ys;

//
// LDA_PUTPIXEL_8
//
#define LDA_PUTPIXEL_8(i, xshift, yshift, xmask) \
   a = span.bg2rgb[dest[i<<1]] + \
       span.fg2rgb[colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]]; \
   b = a; \
   a |= 0x01f07c1f; \
   b &= 0x40100400; \
   a &= 0x3fffffff; \
   b  = b - (b >> 5); \
   a |= b; \
   dest[i<<1] = dest[(i<<1)+1] = RGB32k[0][0][a & (a >> 15)]; \
   xf += xs; \
   yf += ys;

//
// LDA_PUTPIXEL_EXTRA_8
//
#define LDA_PUTPIXEL_EXTRA_8(xshift, yshift, xmask) \
   a = span.bg2rgb[*dest] + \
       span.fg2rgb[colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]]; \
   b = a; \
   a |= 0x01f07c1f; \
   b &= 0x40100400; \
   a &= 0x3fffffff; \
   b  = b - (b >> 5); \
   a |= b; \
   dest[0] = dest[1] = RGB32k[0][0][a & (a >> 15)]; \
   dest += 2; \
   xf += xs; \
   yf += ys;

//
// DESTSTEP_8
//
// Steps to next destination and counts down pixels in unrolled loop.
//
#define DESTSTEP_8() \
   dest += 4; \
   count -= 4;

//
// LD_DESTSTEP_8
//
// As above, but for low detail mode. Steps twice as far in screenspace.
//
#define LD_DESTSTEP_8() \
   dest += 8; \
   count -= 4;

//
// SPAN_FUNC_8
//
// Defines a span drawer with an unrolled span drawing loop.
// Note: this may not be ideal for use with complex blending modes.
// We may need to define a normal loop for those, as code complexity may
// negate the speed benefit of unrolling by causing a cache miss.
//
// SoM: Why didn't I see this earlier? the spot variable is a waste now
// because we don't have the uber complicated math to calculate it now, 
// so that was a memory write we didn't need!
//
#define SPAN_FUNC(name, plog, pdest, xshift, yshift, xmask, pp, ppx, dstp) \
   static void name (void) \
   { \
      plog() \
      pdest() \
      while(count >= 4) \
      { \
         pp(0, xshift, yshift, xmask) \
         \
         pp(1, xshift, yshift, xmask) \
         \
         pp(2, xshift, yshift, xmask) \
         \
         pp(3, xshift, yshift, xmask) \
         \
         dstp() \
      } \
      while(count-- > 0) \
      { \
         ppx(xshift, yshift, xmask) \
      } \
   }

//==============================================================================
//
// R_DrawSpan_*
//
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

// SoM: we only need 6 bits for the integer part (0 thru 63) so the rest
// can be used for the fraction part. This allows calculation of the memory
// address in the texture with two shifts, an OR and one AND. (see below)
// for texture sizes > 64 the amount of precision we can allow will decrease,
// but only by one bit per power of two (obviously)
// Ok, because I was able to eliminate the variable spot below, this function
// is now FASTER than doom's original span renderer. Whodathunkit?

SPAN_FUNC(R_DrawSpanCB_8_64,  SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, 20, 26, 0xFC0, 
          PUTPIXEL_8, PUTPIXEL_EXTRA_8, DESTSTEP_8)

SPAN_FUNC(R_DrawSpanCB_8_128, SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, 18, 25, 0x3F80, 
          PUTPIXEL_8, PUTPIXEL_EXTRA_8, DESTSTEP_8)

SPAN_FUNC(R_DrawSpanCB_8_256, SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, 16, 24, 0xFF00, 
          PUTPIXEL_8, PUTPIXEL_EXTRA_8, DESTSTEP_8)

SPAN_FUNC(R_DrawSpanCB_8_512, SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, 14, 23, 0x3FE00,
          PUTPIXEL_8, PUTPIXEL_EXTRA_8, DESTSTEP_8)

SPAN_FUNC(R_DrawSpanCB_8_GEN, SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, (span.xshift), 
          (span.yshift), (span.xmask), PUTPIXEL_8, PUTPIXEL_EXTRA_8, DESTSTEP_8)

// SoM: Archive
// This is the optimized version of the original flat drawing function.
static void R_DrawSpan_OLD(void) 
{ 
   register unsigned int position;
   unsigned int step;
   
   byte *source;
   byte *colormap;
   byte *dest;
   
   unsigned int count;
   
   position = ((span.yfrac<<10)&0xffff0000) | ((span.xfrac>>6)&0xffff);
   step = ((span.ystep<<10)&0xffff0000) | ((span.xstep>>6)&0xffff);
   
   source = span.source;
   colormap = span.colormap;
   dest = ylookup[span.y] + columnofs[span.x1];       
   count = span.x2 - span.x1 + 1; 
   
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

// SoM: Removed the original span drawing code.

//==============================================================================
//
// TL Span Drawers
//
// haleyjd 06/21/08: TL span drawers are needed for double flats and for portal
// visplane layering.
//

SPAN_FUNC(R_DrawSpanTL_8_64,  TL_SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, 20, 26, 0xFC0, 
          TL_PUTPIXEL_8, TL_PUTPIXEL_EXTRA_8, DESTSTEP_8)

SPAN_FUNC(R_DrawSpanTL_8_128, TL_SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, 18, 25, 0x3F80, 
          TL_PUTPIXEL_8, TL_PUTPIXEL_EXTRA_8, DESTSTEP_8)

SPAN_FUNC(R_DrawSpanTL_8_256, TL_SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, 16, 24, 0xFF00, 
          TL_PUTPIXEL_8, TL_PUTPIXEL_EXTRA_8, DESTSTEP_8)

SPAN_FUNC(R_DrawSpanTL_8_512, TL_SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, 14, 23, 0x3FE00,
          TL_PUTPIXEL_8, TL_PUTPIXEL_EXTRA_8, DESTSTEP_8)

SPAN_FUNC(R_DrawSpanTL_8_GEN, TL_SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, (span.xshift), 
          (span.yshift), (span.xmask), TL_PUTPIXEL_8, TL_PUTPIXEL_EXTRA_8, DESTSTEP_8)
// Additive blending

SPAN_FUNC(R_DrawSpanAdd_8_64,  ADD_SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, 20, 26, 0xFC0, 
          ADD_PUTPIXEL_8, ADD_PUTPIXEL_EXTRA_8, DESTSTEP_8)

SPAN_FUNC(R_DrawSpanAdd_8_128, ADD_SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, 18, 25, 0x3F80, 
          ADD_PUTPIXEL_8, ADD_PUTPIXEL_EXTRA_8, DESTSTEP_8)

SPAN_FUNC(R_DrawSpanAdd_8_256, ADD_SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, 16, 24, 0xFF00, 
          ADD_PUTPIXEL_8, ADD_PUTPIXEL_EXTRA_8, DESTSTEP_8)

SPAN_FUNC(R_DrawSpanAdd_8_512, ADD_SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, 14, 23, 0x3FE00,
          ADD_PUTPIXEL_8, ADD_PUTPIXEL_EXTRA_8, DESTSTEP_8)

SPAN_FUNC(R_DrawSpanAdd_8_GEN, ADD_SPAN_PROLOGUE_8, SPAN_PRIMEDEST_8, (span.xshift), 
          (span.yshift), (span.xmask), ADD_PUTPIXEL_8, ADD_PUTPIXEL_EXTRA_8, DESTSTEP_8)

//==============================================================================
//
// Low-Detail Span Drawers
//
// haleyjd 09/10/06: These are for low-detail mode. They use the high-precision
// drawing code but double up on pixels, making it blocky.
// Low detail shall only be supported in 8-bit color.
//

SPAN_FUNC(R_DrawSpan_LD64,  SPAN_PROLOGUE_8, LD_SPAN_PRIMEDEST_8, 20, 26, 0xFC0, 
          LD_PUTPIXEL_8, LD_PUTPIXEL_EXTRA_8, LD_DESTSTEP_8)

SPAN_FUNC(R_DrawSpan_LD128, SPAN_PROLOGUE_8, LD_SPAN_PRIMEDEST_8, 18, 25, 0x3F80, 
          LD_PUTPIXEL_8, LD_PUTPIXEL_EXTRA_8, LD_DESTSTEP_8)

SPAN_FUNC(R_DrawSpan_LD256, SPAN_PROLOGUE_8, LD_SPAN_PRIMEDEST_8, 16, 24, 0xFF00, 
          LD_PUTPIXEL_8, LD_PUTPIXEL_EXTRA_8, LD_DESTSTEP_8)

SPAN_FUNC(R_DrawSpan_LD512, SPAN_PROLOGUE_8, LD_SPAN_PRIMEDEST_8, 14, 23, 0x3FE00,
          LD_PUTPIXEL_8, LD_PUTPIXEL_EXTRA_8, LD_DESTSTEP_8)

// Translucent variants

SPAN_FUNC(R_DrawSpanTL_LD64,  TL_SPAN_PROLOGUE_8, LD_SPAN_PRIMEDEST_8, 20, 26, 0xFC0, 
          LDTL_PUTPIXEL_8, LDTL_PUTPIXEL_EXTRA_8, LD_DESTSTEP_8)

SPAN_FUNC(R_DrawSpanTL_LD128, TL_SPAN_PROLOGUE_8, LD_SPAN_PRIMEDEST_8, 18, 25, 0x3F80, 
          LDTL_PUTPIXEL_8, LDTL_PUTPIXEL_EXTRA_8, LD_DESTSTEP_8)

SPAN_FUNC(R_DrawSpanTL_LD256, TL_SPAN_PROLOGUE_8, LD_SPAN_PRIMEDEST_8, 16, 24, 0xFF00, 
          LDTL_PUTPIXEL_8, LDTL_PUTPIXEL_EXTRA_8, LD_DESTSTEP_8)

SPAN_FUNC(R_DrawSpanTL_LD512, TL_SPAN_PROLOGUE_8, LD_SPAN_PRIMEDEST_8, 14, 23, 0x3FE00,
          LDTL_PUTPIXEL_8, LDTL_PUTPIXEL_EXTRA_8, LD_DESTSTEP_8)

// Additive variants

SPAN_FUNC(R_DrawSpanAdd_LD64,  ADD_SPAN_PROLOGUE_8, LD_SPAN_PRIMEDEST_8, 20, 26, 0xFC0, 
          LDA_PUTPIXEL_8, LDA_PUTPIXEL_EXTRA_8, LD_DESTSTEP_8)

SPAN_FUNC(R_DrawSpanAdd_LD128, ADD_SPAN_PROLOGUE_8, LD_SPAN_PRIMEDEST_8, 18, 25, 0x3F80, 
          LDA_PUTPIXEL_8, LDA_PUTPIXEL_EXTRA_8, LD_DESTSTEP_8)

SPAN_FUNC(R_DrawSpanAdd_LD256, ADD_SPAN_PROLOGUE_8, LD_SPAN_PRIMEDEST_8, 16, 24, 0xFF00, 
          LDA_PUTPIXEL_8, LDA_PUTPIXEL_EXTRA_8, LD_DESTSTEP_8)

SPAN_FUNC(R_DrawSpanAdd_LD512, ADD_SPAN_PROLOGUE_8, LD_SPAN_PRIMEDEST_8, 14, 23, 0x3FE00,
          LDA_PUTPIXEL_8, LDA_PUTPIXEL_EXTRA_8, LD_DESTSTEP_8)


//==============================================================================
//
// Slope span drawers
//

#define SPANJUMP 16
#define INTERPSTEP (0.0625f)

void R_DrawSlope_8_64(void)
{
   double iu = slopespan.iufrac, iv = slopespan.ivfrac;
   double ius = slopespan.iustep, ivs = slopespan.ivstep;
   double id = slopespan.idfrac, ids = slopespan.idstep;
   
   byte *src, *dest, *colormap;
   int count;
   fixed_t mapindex = 0;

   if((count = slopespan.x2 - slopespan.x1 + 1) < 0)
      return;

   src = (byte *)slopespan.source;
   dest = ylookup[slopespan.y] + columnofs[slopespan.x1];

#if 0
   // Perfect *slow* render
   while(count--)
   {
      float mul = 1.0f / id;

      int u = (int)(iu * mul);
      int v = (int)(iv * mul);
      unsigned texl = (v & 63) * 64 + (u & 63);

      *dest = src[texl];
      dest++;

      iu += ius;
      iv += ivs;
      id += ids;
   }
#else
   while(count >= SPANJUMP)
   {
      double ustart, uend;
      double vstart, vend;
      double mulstart, mulend;
      unsigned int ustep, vstep, ufrac, vfrac;
      int incount;

      // TODO: 
      mulstart = 65536.0f / id;
      id += ids * SPANJUMP;
      mulend = 65536.0f / id;

      ufrac = (int)(ustart = iu * mulstart);
      vfrac = (int)(vstart = iv * mulstart);
      iu += ius * SPANJUMP;
      iv += ivs * SPANJUMP;
      uend = iu * mulend;
      vend = iv * mulend;

      ustep = (int)((uend - ustart) * INTERPSTEP);
      vstep = (int)((vend - vstart) * INTERPSTEP);

      incount = SPANJUMP;
      while(incount--)
      {
         colormap = slopespan.colormap[mapindex ++];
         *dest++ = colormap[src[((vfrac >> 10) & 0xFC0) | ((ufrac >> 16) & 63)]];
         ufrac += ustep;
         vfrac += vstep;
      }

      count -= SPANJUMP;
   }
   if(count > 0)
   {
      double ustart, uend;
      double vstart, vend;
      double mulstart, mulend;
      unsigned int ustep, vstep, ufrac, vfrac;
      int incount;

      mulstart = 65536.0f / id;
      id += ids * count;
      mulend = 65536.0f / id;

      ufrac = (int)(ustart = iu * mulstart);
      vfrac = (int)(vstart = iv * mulstart);
      iu += ius * count;
      iv += ivs * count;
      uend = iu * mulend;
      vend = iv * mulend;

      ustep = (int)((uend - ustart) / count);
      vstep = (int)((vend - vstart) / count);

      incount = count;
      while(incount--)
      {
         colormap = slopespan.colormap[mapindex ++];
         *dest++ = colormap[src[((vfrac >> 10) & 0xFC0) | ((ufrac >> 16) & 63)]];
         ufrac += ustep;
         vfrac += vstep;
      }
   }
#endif
}


void R_DrawSlope_8_128(void)
{
   double iu = slopespan.iufrac, iv = slopespan.ivfrac;
   double ius = slopespan.iustep, ivs = slopespan.ivstep;
   double id = slopespan.idfrac, ids = slopespan.idstep;
   
   byte *src, *dest, *colormap;
   int count;
   fixed_t mapindex = 0;

   if((count = slopespan.x2 - slopespan.x1 + 1) < 0)
      return;

   src = (byte *)slopespan.source;
   dest = ylookup[slopespan.y] + columnofs[slopespan.x1];

   while(count >= SPANJUMP)
   {
      double ustart, uend;
      double vstart, vend;
      double mulstart, mulend;
      unsigned int ustep, vstep, ufrac, vfrac;
      int incount;

      // TODO: 
      mulstart = 65536.0f / id;
      id += ids * SPANJUMP;
      mulend = 65536.0f / id;

      ufrac = (int)(ustart = iu * mulstart);
      vfrac = (int)(vstart = iv * mulstart);
      iu += ius * SPANJUMP;
      iv += ivs * SPANJUMP;
      uend = iu * mulend;
      vend = iv * mulend;

      ustep = (int)((uend - ustart) * INTERPSTEP);
      vstep = (int)((vend - vstart) * INTERPSTEP);

      incount = SPANJUMP;
      while(incount--)
      {
         colormap = slopespan.colormap[mapindex ++];
         *dest++ = colormap[src[((vfrac >> 9) & 0x3F80) | ((ufrac >> 16) & 0x7F)]];
         ufrac += ustep;
         vfrac += vstep;
      }

      count -= SPANJUMP;
   }
   if(count > 0)
   {
      double ustart, uend;
      double vstart, vend;
      double mulstart, mulend;
      unsigned int ustep, vstep, ufrac, vfrac;
      int incount;

      mulstart = 65536.0f / id;
      id += ids * count;
      mulend = 65536.0f / id;

      ufrac = (int)(ustart = iu * mulstart);
      vfrac = (int)(vstart = iv * mulstart);
      iu += ius * count;
      iv += ivs * count;
      uend = iu * mulend;
      vend = iv * mulend;

      ustep = (int)((uend - ustart) / count);
      vstep = (int)((vend - vstart) / count);

      incount = count;
      while(incount--)
      {
         colormap = slopespan.colormap[mapindex ++];
         *dest++ = colormap[src[((vfrac >> 9) & 0x3F80) | ((ufrac >> 16) & 0x7F)]];
         ufrac += ustep;
         vfrac += vstep;
      }
   }
}

void R_DrawSlope_8_256(void)
{
   double iu = slopespan.iufrac, iv = slopespan.ivfrac;
   double ius = slopespan.iustep, ivs = slopespan.ivstep;
   double id = slopespan.idfrac, ids = slopespan.idstep;
   
   byte *src, *dest, *colormap;
   int count;
   fixed_t mapindex = 0;

   if((count = slopespan.x2 - slopespan.x1 + 1) < 0)
      return;

   src = (byte *)slopespan.source;
   dest = ylookup[slopespan.y] + columnofs[slopespan.x1];

   while(count >= SPANJUMP)
   {
      double ustart, uend;
      double vstart, vend;
      double mulstart, mulend;
      unsigned int ustep, vstep, ufrac, vfrac;
      int incount;

      // TODO: 
      mulstart = 65536.0f / id;
      id += ids * SPANJUMP;
      mulend = 65536.0f / id;

      ufrac = (int)(ustart = iu * mulstart);
      vfrac = (int)(vstart = iv * mulstart);
      iu += ius * SPANJUMP;
      iv += ivs * SPANJUMP;
      uend = iu * mulend;
      vend = iv * mulend;

      ustep = (int)((uend - ustart) * INTERPSTEP);
      vstep = (int)((vend - vstart) * INTERPSTEP);

      incount = SPANJUMP;
      while(incount--)
      {
         colormap = slopespan.colormap[mapindex ++];
         *dest++ = colormap[src[((vfrac >> 8) & 0xFF00) | ((ufrac >> 16) & 0xFF)]];
         ufrac += ustep;
         vfrac += vstep;
      }

      count -= SPANJUMP;
   }
   if(count > 0)
   {
      double ustart, uend;
      double vstart, vend;
      double mulstart, mulend;
      unsigned int ustep, vstep, ufrac, vfrac;
      int incount;

      mulstart = 65536.0f / id;
      id += ids * count;
      mulend = 65536.0f / id;

      ufrac = (int)(ustart = iu * mulstart);
      vfrac = (int)(vstart = iv * mulstart);
      iu += ius * count;
      iv += ivs * count;
      uend = iu * mulend;
      vend = iv * mulend;

      ustep = (int)((uend - ustart) / count);
      vstep = (int)((vend - vstart) / count);

      incount = count;
      while(incount--)
      {
         colormap = slopespan.colormap[mapindex ++];
         *dest++ = colormap[src[((vfrac >> 8) & 0xFF00) | ((ufrac >> 16) & 0xFF)]];
         ufrac += ustep;
         vfrac += vstep;
      }
   }
}

void R_DrawSlope_8_512(void)
{
   double iu = slopespan.iufrac, iv = slopespan.ivfrac;
   double ius = slopespan.iustep, ivs = slopespan.ivstep;
   double id = slopespan.idfrac, ids = slopespan.idstep;
   
   byte *src, *dest, *colormap;
   int count;
   fixed_t mapindex = 0;

   if((count = slopespan.x2 - slopespan.x1 + 1) < 0)
      return;

   src = (byte *)slopespan.source;
   dest = ylookup[slopespan.y] + columnofs[slopespan.x1];

   while(count >= SPANJUMP)
   {
      double ustart, uend;
      double vstart, vend;
      double mulstart, mulend;
      unsigned int ustep, vstep, ufrac, vfrac;
      int incount;

      // TODO: 
      mulstart = 65536.0f / id;
      id += ids * SPANJUMP;
      mulend = 65536.0f / id;

      ufrac = (int)(ustart = iu * mulstart);
      vfrac = (int)(vstart = iv * mulstart);
      iu += ius * SPANJUMP;
      iv += ivs * SPANJUMP;
      uend = iu * mulend;
      vend = iv * mulend;

      ustep = (int)((uend - ustart) * INTERPSTEP);
      vstep = (int)((vend - vstart) * INTERPSTEP);

      incount = SPANJUMP;
      while(incount--)
      {
         colormap = slopespan.colormap[mapindex ++];
         *dest++ = colormap[src[((vfrac >> 7) & 0x3FE00) | ((ufrac >> 16) & 0x1FF)]];
         ufrac += ustep;
         vfrac += vstep;
      }

      count -= SPANJUMP;
   }
   if(count > 0)
   {
      double ustart, uend;
      double vstart, vend;
      double mulstart, mulend;
      unsigned int ustep, vstep, ufrac, vfrac;
      int incount;

      mulstart = 65536.0f / id;
      id += ids * count;
      mulend = 65536.0f / id;

      ufrac = (int)(ustart = iu * mulstart);
      vfrac = (int)(vstart = iv * mulstart);
      iu += ius * count;
      iv += ivs * count;
      uend = iu * mulend;
      vend = iv * mulend;

      ustep = (int)((uend - ustart) / count);
      vstep = (int)((vend - vstart) / count);

      incount = count;
      while(incount--)
      {
         colormap = slopespan.colormap[mapindex ++];
         *dest++ = colormap[src[((vfrac >> 7) & 0x3FE00) | ((ufrac >> 16) & 0x1FF)]];
         ufrac += ustep;
         vfrac += vstep;
      }
   }
}



void R_DrawSlope_8_GEN(void)
{
   double iu = slopespan.iufrac, iv = slopespan.ivfrac;
   double ius = slopespan.iustep, ivs = slopespan.ivstep;
   double id = slopespan.idfrac, ids = slopespan.idstep;
   
   byte *src, *dest, *colormap;
   int count;
   fixed_t mapindex = 0;

   if((count = slopespan.x2 - slopespan.x1 + 1) < 0)
      return;

   src = (byte *)slopespan.source;
   dest = ylookup[slopespan.y] + columnofs[slopespan.x1];

   while(count >= SPANJUMP)
   {
      double ustart, uend;
      double vstart, vend;
      double mulstart, mulend;
      unsigned int ustep, vstep, ufrac, vfrac;
      int incount;

      // TODO: 
      mulstart = 65536.0f / id;
      id += ids * SPANJUMP;
      mulend = 65536.0f / id;

      ufrac = (int)(ustart = iu * mulstart);
      vfrac = (int)(vstart = iv * mulstart);
      iu += ius * SPANJUMP;
      iv += ivs * SPANJUMP;
      uend = iu * mulend;
      vend = iv * mulend;

      ustep = (int)((uend - ustart) * INTERPSTEP);
      vstep = (int)((vend - vstart) * INTERPSTEP);

      incount = SPANJUMP;
      while(incount--)
      {
         colormap = slopespan.colormap[mapindex ++];
         *dest++ = colormap[src[((vfrac >> span.xshift) & span.xmask) | ((ufrac >> 16) & span.ymask)]];
         ufrac += ustep;
         vfrac += vstep;
      }

      count -= SPANJUMP;
   }
   if(count > 0)
   {
      double ustart, uend;
      double vstart, vend;
      double mulstart, mulend;
      unsigned int ustep, vstep, ufrac, vfrac;
      int incount;

      mulstart = 65536.0f / id;
      id += ids * count;
      mulend = 65536.0f / id;

      ufrac = (int)(ustart = iu * mulstart);
      vfrac = (int)(vstart = iv * mulstart);
      iu += ius * count;
      iv += ivs * count;
      uend = iu * mulend;
      vend = iv * mulend;

      ustep = (int)((uend - ustart) / count);
      vstep = (int)((vend - vstart) / count);

      incount = count;
      while(incount--)
      {
         colormap = slopespan.colormap[mapindex ++];
         *dest++ = colormap[src[((vfrac >> span.xshift) & span.xmask) | ((ufrac >> 16) & span.ymask)]];
         ufrac += ustep;
         vfrac += vstep;
      }
   }
}


// NOP
void R_DrawSpan_CB_NOP(void)
{
}

#undef SPANJUMP
#undef INTERPSTEP


//==============================================================================
//
// Span Engine Objects
//

// x-shift factors; precision decreases with texture size
#define XS64  (1 << 26)
#define XS128 (1 << 25)
#define XS256 (1 << 24)
#define XS512 (1 << 23)
#define XSOLD (1 << 16)

// lpspandrawer: uses the optimized but low-precision span drawing
// routine for opaque 64x64 flats. Same as above for others.
spandrawer_t r_lpspandrawer =
{
   {
      { R_DrawSpan_OLD,     R_DrawSpanCB_8_128,  R_DrawSpanCB_8_256,  R_DrawSpanCB_8_512,  R_DrawSpanCB_8_GEN},
      { R_DrawSpanTL_8_64,  R_DrawSpanTL_8_128,  R_DrawSpanTL_8_256,  R_DrawSpanTL_8_512,  R_DrawSpanTL_8_GEN},
      { R_DrawSpanAdd_8_64, R_DrawSpanAdd_8_128, R_DrawSpanAdd_8_256, R_DrawSpanAdd_8_512, R_DrawSpanAdd_8_GEN},
   },

   // SoM: Sloped span drawers
   // TODO: TL and ADD drawers
   {
      { R_DrawSlope_8_64,   R_DrawSlope_8_128,   R_DrawSlope_8_256,   R_DrawSlope_8_512,  R_DrawSlope_8_GEN},
      { R_DrawSlope_8_64,   R_DrawSlope_8_128,   R_DrawSlope_8_256,   R_DrawSlope_8_512,  R_DrawSlope_8_GEN},
      { R_DrawSlope_8_64,   R_DrawSlope_8_128,   R_DrawSlope_8_256,   R_DrawSlope_8_512,  R_DrawSlope_8_GEN},
   },

   {
      { XSOLD, XS128, XS256, XS512, 0},
      { XS64,  XS128, XS256, XS512, 0},
      { XS64,  XS128, XS256, XS512, 0},
   },
};

// the normal, high-precision span drawer
spandrawer_t r_spandrawer =
{
   {
      { R_DrawSpanCB_8_64,  R_DrawSpanCB_8_128,  R_DrawSpanCB_8_256,  R_DrawSpanCB_8_512,  R_DrawSpanCB_8_GEN},
      { R_DrawSpanTL_8_64,  R_DrawSpanTL_8_128,  R_DrawSpanTL_8_256,  R_DrawSpanTL_8_512,  R_DrawSpanTL_8_GEN},
      { R_DrawSpanAdd_8_64, R_DrawSpanAdd_8_128, R_DrawSpanAdd_8_256, R_DrawSpanAdd_8_512, R_DrawSpanAdd_8_GEN},
   },

   // SoM: Sloped span drawers
   // TODO: TL and ADD drawers
   {
      { R_DrawSlope_8_64,   R_DrawSlope_8_128,   R_DrawSlope_8_256,   R_DrawSlope_8_512,  R_DrawSlope_8_GEN},
      { R_DrawSlope_8_64,   R_DrawSlope_8_128,   R_DrawSlope_8_256,   R_DrawSlope_8_512,  R_DrawSlope_8_GEN},
      { R_DrawSlope_8_64,   R_DrawSlope_8_128,   R_DrawSlope_8_256,   R_DrawSlope_8_512,  R_DrawSlope_8_GEN},
   },

   {
      { XS64,  XS128, XS256, XS512, 0},
      { XS64,  XS128, XS256, XS512, 0},
      { XS64,  XS128, XS256, XS512, 0},
   },
};

// low-detail spandrawer
spandrawer_t r_lowspandrawer =
{
   {
      { R_DrawSpan_LD64,    R_DrawSpan_LD128,    R_DrawSpan_LD256,    R_DrawSpan_LD512,    R_DrawSpan_CB_NOP},
      { R_DrawSpanTL_LD64,  R_DrawSpanTL_LD128,  R_DrawSpanTL_LD256,  R_DrawSpanTL_LD512,  R_DrawSpan_CB_NOP},
      { R_DrawSpanAdd_LD64, R_DrawSpanAdd_LD128, R_DrawSpanAdd_LD256, R_DrawSpanAdd_LD512, R_DrawSpan_CB_NOP},
   },

   // SoM: Sloped span drawers
   // TODO: TL and ADD drawers
   {
      { R_DrawSlope_8_64,   R_DrawSlope_8_128,   R_DrawSlope_8_256,   R_DrawSlope_8_512,  R_DrawSpan_CB_NOP},
      { R_DrawSlope_8_64,   R_DrawSlope_8_128,   R_DrawSlope_8_256,   R_DrawSlope_8_512,  R_DrawSpan_CB_NOP},
      { R_DrawSlope_8_64,   R_DrawSlope_8_128,   R_DrawSlope_8_256,   R_DrawSlope_8_512,  R_DrawSpan_CB_NOP},
   },

   {
      { XS64, XS128, XS256, XS512, 0},
      { XS64, XS128, XS256, XS512, 0},
      { XS64, XS128, XS256, XS512, 0},
   },
};

// EOF

