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
// haleyjd 06/21/06: removed unused vc_texheight
static byte *vc_source;
static void (*vc_colfunc)();

// haleyjd 06/21/06: vc_maxfrac: this variable keeps track of the length of the
// column currently being drawn and allows the V_DrawPatchColumn variants to cap
// "frac" to this value and avoid the "sparkles" phenomenon, which for screen
// patches is quite terrible when it does occur
static fixed_t vc_maxfrac;

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
   if((unsigned)vc_x  >= (unsigned)vc_buffer->width || 
      (unsigned)vc_yl >= (unsigned)vc_buffer->height) 
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
   // haleyjd 06/21/06: rewrote and specialized for screen patches

   {
      register const byte *source = vc_source;
      
      if(frac < 0)
         frac = 0;
      if(frac > vc_maxfrac)
         frac = vc_maxfrac;
      
      while((count -= 2) >= 0)
      {
         *dest = source[frac >> FRACBITS];
         dest += vc_buffer->pitch;
         if((frac += fracstep) > vc_maxfrac)
         {
            frac = vc_maxfrac;
            fracstep = 0;
         }
         *dest = source[frac >> FRACBITS];
         dest += vc_buffer->pitch;
         if((frac += fracstep) > vc_maxfrac)
         {
            frac = vc_maxfrac;
            fracstep = 0;
         }
      }
      if(count & 1)
         *dest = source[frac >> FRACBITS];
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
   if((unsigned)vc_x  >= (unsigned)vc_buffer->width || 
      (unsigned)vc_yl >= (unsigned)vc_buffer->height) 
      I_Error("V_DrawPatchColumnTR: %i to %i at %i", vc_yl, vc_yh, vc_x); 
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
   // haleyjd 06/21/06: rewrote and specialized for screen patches

   {
      register const byte *source = vc_source;
      
      if(frac < 0)
         frac = 0;
      if(frac > vc_maxfrac)
         frac = vc_maxfrac;
      
      while((count -= 2) >= 0)
      {
         *dest = vc_translation[source[frac >> FRACBITS]];
         dest += vc_buffer->pitch;
         if((frac += fracstep) > vc_maxfrac)
         {
            frac = vc_maxfrac;
            fracstep = 0;
         }
         *dest = vc_translation[source[frac >> FRACBITS]];
         dest += vc_buffer->pitch;
         if((frac += fracstep) > vc_maxfrac)
         {
            frac = vc_maxfrac;
            fracstep = 0;
         }
      }
      if(count & 1)
         *dest = vc_translation[source[frac >> FRACBITS]];
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
   if((unsigned)vc_x  >= (unsigned)vc_buffer->width || 
      (unsigned)vc_yl >= (unsigned) vc_buffer->height) 
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

   // haleyjd 06/21/06: rewrote and specialized for screen patches
   {
      register const byte *source = vc_source;
      
      if(frac < 0)
         frac = 0;
      if(frac > vc_maxfrac)
         frac = vc_maxfrac;
      
      while((count -= 2) >= 0)
      {
         DO_COLOR_BLEND();

         dest += vc_buffer->pitch;
         if((frac += fracstep) > vc_maxfrac)
         {
            frac = vc_maxfrac;
            fracstep = 0;
         }

         DO_COLOR_BLEND();

         dest += vc_buffer->pitch;
         if((frac += fracstep) > vc_maxfrac)
         {
            frac = vc_maxfrac;
            fracstep = 0;
         }
      }
      if(count & 1)
      {
         DO_COLOR_BLEND();
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
   if((unsigned)vc_x  >= (unsigned)vc_buffer->width || 
      (unsigned)vc_yl >= (unsigned)vc_buffer->height) 
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

   // haleyjd 06/21/06: rewrote and specialized for screen patches
   {
      register const byte *source = vc_source;
      
      if(frac < 0)
         frac = 0;
      if(frac > vc_maxfrac)
         frac = vc_maxfrac;
      
      while((count -= 2) >= 0)
      {
         DO_COLOR_BLEND();

         dest += vc_buffer->pitch;
         if((frac += fracstep) > vc_maxfrac)
         {
            frac = vc_maxfrac;
            fracstep = 0;
         }

         DO_COLOR_BLEND();

         dest += vc_buffer->pitch;
         if((frac += fracstep) > vc_maxfrac)
         {
            frac = vc_maxfrac;
            fracstep = 0;
         }
      }
      if(count & 1)
      {
         DO_COLOR_BLEND();
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
   if((unsigned)vc_x  >= (unsigned)vc_buffer->width || 
      (unsigned)vc_yl >= (unsigned)vc_buffer->height) 
      I_Error("V_DrawPatchColumnAdd: %i to %i at %i", vc_yl, vc_yh, vc_x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = vc_buffer->data + vc_yl * vc_buffer->pitch + vc_x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = vc_iscale; 
   frac = vc_texturemid + 
      FixedMul((vc_yl << FRACBITS) - vc_centeryfrac, fracstep);

   // haleyjd 06/21/06: rewrote and specialized for screen patches
   {
      register const byte *source = vc_source;
      
      if(frac < 0)
         frac = 0;
      if(frac > vc_maxfrac)
         frac = vc_maxfrac;
      
      while((count -= 2) >= 0)
      {
         DO_COLOR_BLEND();

         dest += vc_buffer->pitch;
         if((frac += fracstep) > vc_maxfrac)
         {
            frac = vc_maxfrac;
            fracstep = 0;
         }

         DO_COLOR_BLEND();

         dest += vc_buffer->pitch;
         if((frac += fracstep) > vc_maxfrac)
         {
            frac = vc_maxfrac;
            fracstep = 0;
         }
      }
      if(count & 1)
      {
         DO_COLOR_BLEND();
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
// V_DrawPatchColumnAddTR
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
   if((unsigned)vc_x  >= (unsigned)vc_buffer->width || 
      (unsigned)vc_yl >= (unsigned)vc_buffer->height) 
      I_Error("V_DrawPatchColumnAddTR: %i to %i at %i", vc_yl, vc_yh, vc_x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = vc_buffer->data + vc_yl * vc_buffer->pitch + vc_x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = vc_iscale; 
   frac = vc_texturemid + 
      FixedMul((vc_yl << FRACBITS) - vc_centeryfrac, fracstep);

   // haleyjd 06/21/06: rewrote and specialized for screen patches
   {
      register const byte *source = vc_source;
      
      if(frac < 0)
         frac = 0;
      if(frac > vc_maxfrac)
         frac = vc_maxfrac;
      
      while((count -= 2) >= 0)
      {
         DO_COLOR_BLEND();

         dest += vc_buffer->pitch;
         if((frac += fracstep) > vc_maxfrac)
         {
            frac = vc_maxfrac;
            fracstep = 0;
         }
         
         DO_COLOR_BLEND();
         
         dest += vc_buffer->pitch;
         if((frac += fracstep) > vc_maxfrac)
         {
            frac = vc_maxfrac;
            fracstep = 0;
         }
      }
      if(count & 1)
      {
         DO_COLOR_BLEND();
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
 
   // haleyjd 06/21/06: this isn't used any more
   // vc_texheight = 0; // killough 11/98

   while(column->topdelta != 0xff)
   {
      // calculate unclipped screen coordinates for post
      topscreen = v_sprtopscreen + v_spryscale * column->topdelta;
      bottomscreen = topscreen + v_spryscale * column->length;

      // haleyjd 06/21/06: calculate the maximum allowed value for the frac
      // column index; this avoids potential overflow leading to "sparkles",
      // which for most screen patches look more like mud. Note this solution
      // probably isn't suitable for adaptation to the R_DrawColumn system
      // due to efficiency concerns.
      vc_maxfrac = (column->length - 1) << FRACBITS;

      // Here's where "sparkles" come in -- killough:
      // haleyjd: but not with vc_maxfrac :)
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

   // haleyjd 10/01/06: round up the inverse scaling factors by 1/65536. This
   // ensures that fracstep steps up to the next pixel just fast enough to
   // prevent non-uniform scaling in modes where the inverse scaling factor is
   // not accurately represented in fixed point (ie. should be 0.333...).
   // The error is one pixel per every 65536, so it's totally irrelevant.

   scale  = FixedDiv(vc_buffer->width, SCREENWIDTH);
   iscale = FixedDiv(SCREENWIDTH, vc_buffer->width) + 1;

   // haleyjd 06/21/06: simplified redundant math
   v_spryscale = FixedDiv(vc_buffer->height, SCREENHEIGHT);
   vc_iscale   = FixedDiv(SCREENHEIGHT, vc_buffer->height) + 1;
   
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

//=============================================================================
//
// Conversion Routines
//
// haleyjd: this stuff turns other graphic formats into patches :)
//

//
// V_LinearToPatch
//
// haleyjd 07/07/07: converts a linear graphic to a patch
//
patch_t *V_LinearToPatch(byte *linear, int w, int h, size_t *memsize)
{
   int      x, y;
   patch_t  *p;
   column_t *c;
   int      *columnofs;
   byte     *src, *dest;

   // 1. no source bytes are lost (no transparency)
   // 2. patch_t header is 4 shorts plus width * int for columnofs array
   // 3. one post per vertical slice plus 2 padding bytes and 1 byte for
   //    a -1 post to cap the column are required
   size_t total_size = 
      4 * sizeof(short) + w * (h + sizeof(long) + sizeof(column_t) + 3);
   
   byte *out = malloc(total_size);

   p = (patch_t *)out;

   // set basic header information
   p->width      = w;
   p->height     = h;
   p->topoffset  = 0;
   p->leftoffset = 0;

   // get pointer to columnofs table
   columnofs = (int *)(out + 4 * sizeof(short));

   // skip past columnofs table
   dest = out + 4 * sizeof(short) + w * sizeof(long);

   // convert columns of linear graphic into true columns
   for(x = 0; x < w; ++x)
   {
      // set entry in columnofs table
      columnofs[x] = dest - out;

      // set basic column properties
      c = (column_t *)dest;
      c->length   = h;
      c->topdelta = 0;

      // skip past column header
      dest += sizeof(column_t) + 1;

      // copy bytes
      for(y = 0, src = linear + x; y < h; ++y, src += w)
         *dest++ = *src;

      // create end post
      *(dest + 1) = 255;

      // skip to next column location 
      dest += 2;
   }

   // allow returning size of allocation in *memsize
   if(memsize)
      *memsize = total_size;

   // voila!
   return p;
}

// haleyjd: GBA sprite patch structure. Similar to patch_t, but all fields are
// big-endian shorts, and the pixel data for columns is in an entirely separate
// wad lump.

typedef struct gbasprite_s
{
   short width;
   short height;
   short leftoffset;
   short topoffset;
   unsigned short columnofs[8];
} gbasprite_t;

// haleyjd: GBA sprite column structure. Same as column_t but has an unsigned
// short offset into the pixel data lump instead of the actual pixels.

typedef struct gbacolumn_s
{
   byte topdelta;
   byte length;
   unsigned short dataofs;
} gbacolumn_t;

//
// V_GBASpriteToPatch
//
// haleyjd 07/08/07: converts a two-lump GBA-format sprite to a normal patch.
// Special thanks to Kaiser for help understanding this.
//
patch_t *V_GBASpriteToPatch(byte *sprlump, byte *datalump, size_t *memsize)
{
   int x, pixelcount = 0, columncount = 0;
   size_t total_size = 0;
   gbasprite_t *gbapatch = (gbasprite_t *)sprlump;
   gbacolumn_t *gbacolumn;
   patch_t  *p;
   column_t *c;
   byte *out, *dest, *src;
   int *columnofs;

   // normalize big-endian fields in patch header
   gbapatch->width      = BIGSHORT(gbapatch->width);
   gbapatch->height     = BIGSHORT(gbapatch->height);
   gbapatch->leftoffset = BIGSHORT(gbapatch->leftoffset);
   gbapatch->topoffset  = BIGSHORT(gbapatch->topoffset);

   // first things first, we must figure out how much memory to allocate for
   // the Doom patch -- best way to do this is to use the same process that
   // would be used to draw it, and count columns and pixels. We can also
   // normalize all the big-endian shorts in the data at the same time.

   for(x = 0; x < gbapatch->width; ++x)
   {
      gbapatch->columnofs[x] = BIGSHORT(gbapatch->columnofs[x]);

      gbacolumn = (gbacolumn_t *)(sprlump + gbapatch->columnofs[x]);

      for(; gbacolumn->topdelta != 0xff; ++gbacolumn)
      {
         gbacolumn->dataofs = BIGSHORT(gbacolumn->dataofs);
         pixelcount += gbacolumn->length;
         ++columncount;
      }
   }

   // calculate total_size --
   // 1. patch header - 4 shorts + width * int for columnofs array
   // 2. number of pixels counted above
   // 3. columncount column_t headers
   // 4. columncount times 2 padding bytes
   // 5. one -1 post to cap each vertical slice
   total_size = 4 * sizeof(short) + gbapatch->width * sizeof(long) + 
                pixelcount + columncount * (sizeof(column_t) + 2) + 
                gbapatch->width;

   out = malloc(total_size);

   p = (patch_t *)out;

   // store header info
   p->width      = gbapatch->width;
   p->height     = gbapatch->height;
   p->leftoffset = gbapatch->leftoffset;
   p->topoffset  = gbapatch->topoffset;

   // get pointer to columnofs table
   columnofs = (int *)(out + 4 * sizeof(short));

   // skip past columnofs table
   dest = out + 4 * sizeof(short) + p->width * sizeof(long);

   // repeat drawing process to construct PC-format patch
   for(x = 0; x < gbapatch->width; ++x)
   {
      // set columnofs entry for this column in the PC patch
      columnofs[x] = dest - out;

      gbacolumn = (gbacolumn_t *)(sprlump + gbapatch->columnofs[x]);

      for(; gbacolumn->topdelta != 0xff; ++gbacolumn)
      {
         int count;
         byte lastbyte;

         // set basic column properties
         c = (column_t *)dest;
         c->topdelta = gbacolumn->topdelta;
         count = c->length = gbacolumn->length;

         // skip past column header
         dest += sizeof(column_t);

         // gbacolumn->dataofs gives an offset into the pixel data lump
         src = datalump + gbacolumn->dataofs;

         // copy first pixel into padding byte at start of column
         *dest++ = *src;

         // copy pixels
         while(count--)
         {
            lastbyte = *src++;
            *dest++ = lastbyte;
         }

         // copy last pixel into padding byte at end of column
         *dest++ = lastbyte;
      }

      // create end post for this column
      *dest++ = 0xff;
   }

   // allow returning size of allocation in *memsize
   if(memsize)
      *memsize = total_size;

   // phew!
   return p;
}

// EOF

