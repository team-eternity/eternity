// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley
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
//  Functions to draw patches (by post)
//
//-----------------------------------------------------------------------------

#include "v_video.h"

// patch rendering globals -- like dc_ in r_draw.c
static VBuffer *vc_buffer;
static int vc_x;
static int vc_yl;
static int vc_yh;
static fixed_t vc_iscale;
static fixed_t vc_texturemid;
static int vc_centery;
static fixed_t vc_centeryfrac;
static int vc_centerx;
static fixed_t vc_centerxfrac;
static byte *vc_source;
static int vc_texheight;
static void (*vc_colfunc)();

// translucency lookups
static unsigned int *v_fg2rgb;
static unsigned int *v_bg2rgb;

// translation table
static byte *vc_translation;

//
// V_DrawPatchColumn
//
// Draws a plain patch column with no remappings.
//
static void V_DrawPatchColumn(void) 
{ 
   int              count;
   register byte    *dest;    // killough
   register fixed_t frac;     // killough
   fixed_t          fracstep;
   
   if((count = vc_yh - vc_yl + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned)vc_x >= (unsigned)vc_buffer->width || 
      vc_yl < 0 || vc_yh >= vc_buffer->height) 
      I_Error("V_DrawPatchColumn: %i to %i at %i", vc_yl, vc_yh, vc_x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = vc_buffer->data + vc_yl * vc_buffer->pitch + vc_x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = vc_iscale; 
   frac = vc_texturemid + 
      FixedMul((vc_yl << FRACBITS) - vc_centeryfrac, fracstep);

   // Inner loop that does the actual texture mapping,
   //  e.g. a DDA-lile scaling.
   // This is as fast as it gets.       (Yeah, right!!! -- killough)
   //
   // killough 2/1/98: more performance tuning

   {
      register const byte *source = vc_source;
      register heightmask = vc_texheight - 1;

      if(vc_texheight & heightmask) // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
         
         if(frac < 0)
            while((frac += heightmask) < 0);
         else
            while(frac >= heightmask)
               frac -= heightmask;
          
         do
         {
            *dest = source[frac>>FRACBITS];
            dest += vc_buffer->pitch; // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = source[(frac>>FRACBITS) & heightmask];
            dest += vc_buffer->pitch;   // killough 11/98
            frac += fracstep;
            *dest = source[(frac>>FRACBITS) & heightmask];
            dest += vc_buffer->pitch;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
            *dest = source[(frac>>FRACBITS) & heightmask];
      }
   }
} 

//
// V_DrawPatchColumnTR
//
// Draws a plain patch column with color translation.
//
static void V_DrawPatchColumnTR(void) 
{ 
   int              count;
   register byte    *dest;    // killough
   register fixed_t frac;     // killough
   fixed_t          fracstep;
   
   if((count = vc_yh - vc_yl + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned)vc_x >= (unsigned)vc_buffer->width || 
      vc_yl < 0 || vc_yh >= vc_buffer->height) 
      I_Error("V_DrawPatchColumn: %i to %i at %i", vc_yl, vc_yh, vc_x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = vc_buffer->data + vc_yl * vc_buffer->pitch + vc_x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = vc_iscale; 
   frac = vc_texturemid + 
      FixedMul((vc_yl << FRACBITS) - vc_centeryfrac, fracstep);

   // Inner loop that does the actual texture mapping,
   //  e.g. a DDA-lile scaling.
   // This is as fast as it gets.       (Yeah, right!!! -- killough)
   //
   // killough 2/1/98: more performance tuning

   {
      register const byte *source = vc_source;
      register heightmask = vc_texheight - 1;

      if(vc_texheight & heightmask) // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
         
         if(frac < 0)
            while((frac += heightmask) < 0);
         else
            while(frac >= heightmask)
               frac -= heightmask;
          
         do
         {
            *dest = vc_translation[source[frac>>FRACBITS]];
            dest += vc_buffer->pitch; // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = vc_translation[source[(frac>>FRACBITS) & heightmask]];
            dest += vc_buffer->pitch;   // killough 11/98
            frac += fracstep;
            *dest = vc_translation[source[(frac>>FRACBITS) & heightmask]];
            dest += vc_buffer->pitch;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
            *dest = vc_translation[source[(frac>>FRACBITS) & heightmask]];
      }
   }
} 

#define DO_COLOR_BLEND()                       \
   fg = v_fg2rgb[source[frac >> FRACBITS]];    \
   bg = v_bg2rgb[*dest];                       \
   fg = (fg + bg) | 0xF07C3E1F;                \
   *dest = RGB8k[0][0][(fg >> 5) & (fg >> 19)]

//
// V_DrawPatchColumnTL
//
// Draws a translucent patch column. The DosDoom/zdoom-style
// translucency lookups must be set before getting here.
//
void V_DrawPatchColumnTL(void)
{ 
   int              count; 
   register byte    *dest;           // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   unsigned int     fg, bg;
   
   if((count = vc_yh - vc_yl + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned)vc_x >= (unsigned)vc_buffer->width || 
      vc_yl < 0 || vc_yh >= vc_buffer->height) 
      I_Error("V_DrawPatchColumnTL: %i to %i at %i", vc_yl, vc_yh, vc_x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = vc_buffer->data + vc_yl * vc_buffer->pitch + vc_x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = vc_iscale; 
   frac = vc_texturemid + 
      FixedMul((vc_yl << FRACBITS) - vc_centeryfrac, fracstep);

   {
      register const byte *source = vc_source;
      register fixed_t heightmask = vc_texheight - 1;

      if(vc_texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= heightmask)
               frac -= heightmask;
        
         do
         {
            // Re-map color indices from wall texture column
            //  using a lighting/special effects LUT.
            
            // heightmask is the Tutti-Frutti fix -- killough
            DO_COLOR_BLEND();
            
            dest += vc_buffer->pitch; // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            DO_COLOR_BLEND();

            dest += vc_buffer->pitch;   // killough 11/98
            frac += fracstep;

            DO_COLOR_BLEND();

            dest += vc_buffer->pitch;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
         {
            DO_COLOR_BLEND();
         }
      }
   }
}

#undef DO_COLOR_BLEND

#define DO_COLOR_BLEND()          \
   fg = v_fg2rgb[vc_translation[source[frac >> FRACBITS]]]; \
   bg = v_bg2rgb[*dest];          \
   fg = (fg + bg) | 0xF07C3E1F;   \
   *dest = RGB8k[0][0][(fg >> 5) & (fg >> 19)]

//
// V_DrawPatchColumnTRTL
//
// Draws translated translucent patch columns.
// Requires both translucency lookups and a translation table.
//
void V_DrawPatchColumnTRTL(void)
{ 
   int              count; 
   register byte    *dest;           // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   unsigned int     fg, bg;
   
   if((count = vc_yh - vc_yl + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned)vc_x >= (unsigned)vc_buffer->width || 
      vc_yl < 0 || vc_yh >= vc_buffer->height) 
      I_Error("V_DrawPatchColumnTRTL: %i to %i at %i", vc_yl, vc_yh, vc_x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = vc_buffer->data + vc_yl * vc_buffer->pitch + vc_x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = vc_iscale; 
   frac = vc_texturemid + 
      FixedMul((vc_yl << FRACBITS) - vc_centeryfrac, fracstep);

   {
      register const byte *source = vc_source;
      register fixed_t heightmask = vc_texheight - 1;

      if(vc_texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= heightmask)
               frac -= heightmask;
        
         do
         {
            // Re-map color indices from wall texture column
            //  using a lighting/special effects LUT.
            
            // heightmask is the Tutti-Frutti fix -- killough
            DO_COLOR_BLEND();
            
            dest += vc_buffer->pitch; // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            DO_COLOR_BLEND();

            dest += vc_buffer->pitch;   // killough 11/98
            frac += fracstep;

            DO_COLOR_BLEND();

            dest += vc_buffer->pitch;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
         {
            DO_COLOR_BLEND();
         }
      }
   }
}

#undef DO_COLOR_BLEND

#define DO_COLOR_BLEND() \
   /* mask out LSBs in green and red to allow overflow */ \
   a = v_fg2rgb[source[frac >> FRACBITS]] & 0xFFBFDFF; \
   b = v_bg2rgb[*dest] & 0xFFBFDFF;   \
   a  = a + b;                      /* add with overflow         */ \
   b  = a & 0x10040200;             /* isolate LSBs              */ \
   b  = (b - (b >> 5)) & 0xF83C1E0; /* convert to clamped values */ \
   a |= 0xF07C3E1F;                 /* apply normal tl mask      */ \
   a |= b;                          /* mask in clamped values    */ \
   *dest = RGB8k[0][0][(a >> 5) & (a >> 19)]


//
// V_DrawPatchColumnAdd
//
// Draws a patch column with additive translucency.
//
void V_DrawPatchColumnAdd(void)
{ 
   int              count; 
   register byte    *dest;           // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   unsigned int     a, b;
   
   if((count = vc_yh - vc_yl + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned)vc_x >= (unsigned)vc_buffer->width || 
      vc_yl < 0 || vc_yh >= vc_buffer->height) 
      I_Error("V_DrawPatchColumnTL: %i to %i at %i", vc_yl, vc_yh, vc_x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = vc_buffer->data + vc_yl * vc_buffer->pitch + vc_x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = vc_iscale; 
   frac = vc_texturemid + 
      FixedMul((vc_yl << FRACBITS) - vc_centeryfrac, fracstep);

   {
      register const byte *source = vc_source;
      register fixed_t heightmask = vc_texheight - 1;

      if(vc_texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= heightmask)
               frac -= heightmask;
        
         do
         {
            // Re-map color indices from wall texture column
            //  using a lighting/special effects LUT.
            
            // heightmask is the Tutti-Frutti fix -- killough
            DO_COLOR_BLEND();
            
            dest += vc_buffer->pitch; // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            DO_COLOR_BLEND();

            dest += vc_buffer->pitch;   // killough 11/98
            frac += fracstep;

            DO_COLOR_BLEND();

            dest += vc_buffer->pitch;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
         {
            DO_COLOR_BLEND();
         }
      }
   }
}

#undef DO_COLOR_BLEND

#define DO_COLOR_BLEND() \
   /* mask out LSBs in green and red to allow overflow */ \
   a = v_fg2rgb[vc_translation[source[frac >> FRACBITS]]] & 0xFFBFDFF; \
   b = v_bg2rgb[*dest] & 0xFFBFDFF;   \
   a  = a + b;                      /* add with overflow         */ \
   b  = a & 0x10040200;             /* isolate LSBs              */ \
   b  = (b - (b >> 5)) & 0xF83C1E0; /* convert to clamped values */ \
   a |= 0xF07C3E1F;                 /* apply normal tl mask      */ \
   a |= b;                          /* mask in clamped values    */ \
   *dest = RGB8k[0][0][(a >> 5) & (a >> 19)]

//
// V_DrawPatchColumnAdd
//
// Draws a patch column with additive translucency and
// translation.
//
void V_DrawPatchColumnAddTR(void)
{ 
   int              count; 
   register byte    *dest;           // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   unsigned int     a, b;
   
   if((count = vc_yh - vc_yl + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned)vc_x >= (unsigned)vc_buffer->width || 
      vc_yl < 0 || vc_yh >= vc_buffer->height) 
      I_Error("V_DrawPatchColumnTL: %i to %i at %i", vc_yl, vc_yh, vc_x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = vc_buffer->data + vc_yl * vc_buffer->pitch + vc_x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = vc_iscale; 
   frac = vc_texturemid + 
      FixedMul((vc_yl << FRACBITS) - vc_centeryfrac, fracstep);

   {
      register const byte *source = vc_source;
      register fixed_t heightmask = vc_texheight - 1;

      if(vc_texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= heightmask)
               frac -= heightmask;
        
         do
         {
            // Re-map color indices from wall texture column
            //  using a lighting/special effects LUT.
            
            // heightmask is the Tutti-Frutti fix -- killough
            DO_COLOR_BLEND();
            
            dest += vc_buffer->pitch; // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            DO_COLOR_BLEND();

            dest += vc_buffer->pitch;   // killough 11/98
            frac += fracstep;

            DO_COLOR_BLEND();

            dest += vc_buffer->pitch;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
         {
            DO_COLOR_BLEND();
         }
      }
   }
}

#undef DO_COLOR_BLEND

static fixed_t v_sprtopscreen;
static fixed_t v_spryscale;

static void V_DrawMaskedColumn(column_t *column)
{
   int topscreen, bottomscreen;
   fixed_t basetexturemid = vc_texturemid;
   
   vc_texheight = 0; // killough 11/98

   while(column->topdelta != 0xff)
   {
      // calculate unclipped screen coordinates for post
      topscreen = v_sprtopscreen + v_spryscale * column->topdelta;
      bottomscreen = topscreen + v_spryscale * column->length;

      // Here's where "sparkles" come in -- killough:
      vc_yl = (topscreen + FRACUNIT - 1) >> FRACBITS;
      vc_yh = (bottomscreen - 1) >> FRACBITS;

      if(vc_yh >= vc_buffer->height)
         vc_yh = vc_buffer->height - 1;
      
      if(vc_yl < 0)
         vc_yl = 0;

      // killough 3/2/98, 3/27/98: Failsafe against overflow/crash:
      if(vc_yl <= vc_yh && vc_yh < vc_buffer->height)
      {
         vc_source = (byte *)column + 3;
         vc_texturemid = basetexturemid - (column->topdelta << FRACBITS);
         vc_colfunc();
      }
      column = (column_t *)((byte *)column + column->length + 4);
   }
   vc_texturemid = basetexturemid;
}

//
// V_DrawPatchInt
//
// Draws patches to the screen via the same vissprite-style scaling
// and clipping used to draw player gun sprites. This results in
// more clean and uniform scaling even in arbitrary resolutions, and
// eliminates mostly unnecessary special cases for certain resolutions
// like 640x400.
//
void V_DrawPatchInt(PatchInfo *pi, VBuffer *buffer)
{
   fixed_t    tx;
   int        x1, x2, vc_x2;
   fixed_t    scale, iscale, xiscale, startfrac = 0;
   patch_t    *patch = pi->patch;
   
   vc_buffer      = buffer;
   vc_centerx     = vc_buffer->width  >> 1;
   vc_centerxfrac = vc_centerx << FRACBITS;
   vc_centery     = vc_buffer->height >> 1;
   vc_centeryfrac = vc_centery << FRACBITS;

   // calculate edges of the shape
   tx =  pi->x << FRACBITS;
   tx -= (SHORT(patch->leftoffset) << FRACBITS);

   scale  = FixedDiv(vc_buffer->width, SCREENWIDTH);
   iscale = FixedDiv(SCREENWIDTH, vc_buffer->width);
   v_spryscale = 
      FixedMul(scale, 
               FixedDiv((vc_buffer->height << FRACBITS) / SCREENHEIGHT,
                        (vc_buffer->width  << FRACBITS) / SCREENWIDTH));
   
   x1 = FixedMul(tx, scale) >> FRACBITS;

   // off the right side
   if(x1 > vc_buffer->width)
      return;

   tx += SHORT(patch->width) << FRACBITS;
   x2 = (FixedMul(tx, scale) >> FRACBITS) - 1;

   // off the left side
   if(x2 < 0)
      return;
 
   if(pi->flipped)
   {
      int tmpx = x1;
      x1 = buffer->width - x2;
      x2 = buffer->width - tmpx;
   }

   vc_texturemid = (100 << FRACBITS) -
                    ((pi->y - SHORT(patch->topoffset)) << FRACBITS);

   vc_x  = x1 < 0 ? 0 : x1;
   vc_x2 = x2 >= buffer->width ? buffer->width - 1 : x2;

   if(pi->flipped)
   {
      xiscale   = -iscale;
      startfrac = (SHORT(patch->width) << FRACBITS) - 1;
   }
   else
      xiscale   = iscale;

   if(vc_x > x1)
      startfrac += xiscale * (vc_x - x1);

   {
      column_t *column;
      int      texturecolumn;
      
      switch(pi->drawstyle)
      {
      case PSTYLE_NORMAL:
         vc_colfunc = V_DrawPatchColumn;
         break;
      case PSTYLE_TLATED:
         vc_colfunc = V_DrawPatchColumnTR;
         break;
      case PSTYLE_TRANSLUC:
         vc_colfunc = V_DrawPatchColumnTL;
         break;
      case PSTYLE_TLTRANSLUC:
         vc_colfunc = V_DrawPatchColumnTRTL;
         break;
      case PSTYLE_ADD:
         vc_colfunc = V_DrawPatchColumnAdd;
         break;
      case PSTYLE_TLADD:
         vc_colfunc = V_DrawPatchColumnAddTR;
         break;
      default:
         I_Error("V_DrawPatchInt: unknown patch drawstyle %d\n", pi->drawstyle);
      }

      vc_iscale = FixedDiv(FRACUNIT, v_spryscale);
      v_sprtopscreen = vc_centeryfrac - FixedMul(vc_texturemid, v_spryscale);
      
      for(; vc_x <= vc_x2; vc_x++, startfrac += xiscale)
      {
         texturecolumn = startfrac >> FRACBITS;
         
#ifdef RANGECHECK
         if(texturecolumn < 0 || texturecolumn >= SHORT(patch->width))
            I_Error("V_DrawPatchInt: bad texturecolumn");
#endif
         
         column = (column_t *)((byte *)patch +
            LONG(patch->columnofs[texturecolumn]));
         V_DrawMaskedColumn(column);
      }
   }
}

void V_SetPatchColrng(byte *colrng)
{
   vc_translation = colrng;
}

void V_SetPatchTL(unsigned int *fg, unsigned int *bg)
{
   v_fg2rgb = fg;
   v_bg2rgb = bg;
}

//
// V_SetupBufferFuncs
//
// VBuffer setup function
//
// Call to determine the type of drawing your VBuffer object will have
//
void V_SetupBufferFuncs(VBuffer *buffer, int drawtype)
{
   // call other setting functions
   V_SetBlockFuncs(buffer, drawtype);
}


// EOF

