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
// SoM: AAAHHHHHHH
static cb_patch_column_t patchcol;
static int ytop;

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
   
   if((count = patchcol.y2 - patchcol.y1 + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned)patchcol.x  >= (unsigned)patchcol.buffer->width || 
      (unsigned)patchcol.y1 >= (unsigned)patchcol.buffer->height) 
      I_Error("V_DrawPatchColumn: %i to %i at %i", patchcol.y1, patchcol.y2, patchcol.x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = patchcol.buffer->data + patchcol.y1 * patchcol.buffer->pitch + patchcol.x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = patchcol.step;
   frac = (patchcol.y1 * fracstep) & 0xFFFF;

   // Inner loop that does the actual texture mapping,
   //  e.g. a DDA-lile scaling.
   // This is as fast as it gets.       (Yeah, right!!! -- killough)
   //
   // killough 2/1/98: more performance tuning
   // haleyjd 06/21/06: rewrote and specialized for screen patches

   {
      register const byte *source = patchcol.source;
            
      while((count -= 2) >= 0)
      {
         *dest = source[frac >> FRACBITS];
         dest += patchcol.buffer->pitch;
         frac += fracstep;
         *dest = source[frac >> FRACBITS];
         dest += patchcol.buffer->pitch;
         frac += fracstep;
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
   
   if((count = patchcol.y2 - patchcol.y1 + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned)patchcol.x  >= (unsigned)patchcol.buffer->width || 
      (unsigned)patchcol.y1 >= (unsigned)patchcol.buffer->height) 
      I_Error("V_DrawPatchColumnTR: %i to %i at %i", patchcol.y1, patchcol.y2, patchcol.x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = patchcol.buffer->data + patchcol.y1 * patchcol.buffer->pitch + patchcol.x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = patchcol.step;
   frac = (patchcol.y1 * fracstep) & 0xFFFF;

   // Inner loop that does the actual texture mapping,
   //  e.g. a DDA-lile scaling.
   // This is as fast as it gets.       (Yeah, right!!! -- killough)
   //
   // killough 2/1/98: more performance tuning
   // haleyjd 06/21/06: rewrote and specialized for screen patches

   {
      register const byte *source = patchcol.source;
            
      while((count -= 2) >= 0)
      {
         *dest = patchcol.translation[source[frac >> FRACBITS]];
         dest += patchcol.buffer->pitch;
         frac += fracstep;
         *dest = patchcol.translation[source[frac >> FRACBITS]];
         dest += patchcol.buffer->pitch;
         frac += fracstep;
      }
      if(count & 1)
         *dest = patchcol.translation[source[frac >> FRACBITS]];
   }
} 

#define DO_COLOR_BLEND()                       \
   fg = patchcol.fg2rgb[source[frac >> FRACBITS]];    \
   bg = patchcol.bg2rgb[*dest];                       \
   fg = (fg + bg) | 0x1f07c1f;                 \
   *dest = RGB32k[0][0][fg & (fg >> 15)]

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
   
   if((count = patchcol.y2 - patchcol.y1 + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned)patchcol.x  >= (unsigned)patchcol.buffer->width || 
      (unsigned)patchcol.y1 >= (unsigned)patchcol.buffer->height) 
      I_Error("V_DrawPatchColumnTL: %i to %i at %i", patchcol.y1, patchcol.y2, patchcol.x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = patchcol.buffer->data + patchcol.y1 * patchcol.buffer->pitch + patchcol.x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = patchcol.step;
   frac = (patchcol.y1 * fracstep) & 0xFFFF;

   // haleyjd 06/21/06: rewrote and specialized for screen patches
   {
      register const byte *source = patchcol.source;
            
      while((count -= 2) >= 0)
      {
         DO_COLOR_BLEND();

         dest += patchcol.buffer->pitch;
         frac += fracstep;

         DO_COLOR_BLEND();

         dest += patchcol.buffer->pitch;
         frac += fracstep;
      }
      if(count & 1)
      {
         DO_COLOR_BLEND();
      }
   }
}

#undef DO_COLOR_BLEND

#define DO_COLOR_BLEND()          \
   fg = patchcol.fg2rgb[patchcol.translation[source[frac >> FRACBITS]]]; \
   bg = patchcol.bg2rgb[*dest];          \
   fg = (fg + bg) | 0x1f07c1f;    \
   *dest = RGB32k[0][0][fg & (fg >> 15)]

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
   
   if((count = patchcol.y2 - patchcol.y1 + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned)patchcol.x  >= (unsigned)patchcol.buffer->width || 
      (unsigned)patchcol.y1 >= (unsigned)patchcol.buffer->height) 
      I_Error("V_DrawPatchColumnTRTL: %i to %i at %i", patchcol.y1, patchcol.y2, patchcol.x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = patchcol.buffer->data + patchcol.y1 * patchcol.buffer->pitch + patchcol.x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = patchcol.step; 
   frac = (patchcol.y1 * fracstep) & 0xFFFF;

   // haleyjd 06/21/06: rewrote and specialized for screen patches
   {
      register const byte *source = patchcol.source;
      
      while((count -= 2) >= 0)
      {
         DO_COLOR_BLEND();

         dest += patchcol.buffer->pitch;
         frac += fracstep;

         DO_COLOR_BLEND();

         dest += patchcol.buffer->pitch;
         frac += fracstep;
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
   a = patchcol.fg2rgb[source[frac >> FRACBITS]] + patchcol.bg2rgb[*dest]; \
   b = a; \
   a |= 0x01f07c1f; \
   b &= 0x40100400; \
   a &= 0x3fffffff; \
   b  = b - (b >> 5); \
   a |= b; \
   *dest = RGB32k[0][0][a & (a >> 15)]


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
   
   if((count = patchcol.y2 - patchcol.y1 + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned)patchcol.x  >= (unsigned)patchcol.buffer->width || 
      (unsigned)patchcol.y1 >= (unsigned)patchcol.buffer->height) 
      I_Error("V_DrawPatchColumnAdd: %i to %i at %i", patchcol.y1, patchcol.y2, patchcol.x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = patchcol.buffer->data + patchcol.y1 * patchcol.buffer->pitch + patchcol.x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = patchcol.step; 
   frac = (patchcol.y1 * fracstep) & 0xFFFF;

   // haleyjd 06/21/06: rewrote and specialized for screen patches
   {
      register const byte *source = patchcol.source;
      
      while((count -= 2) >= 0)
      {
         DO_COLOR_BLEND();

         dest += patchcol.buffer->pitch;
         frac += fracstep;

         DO_COLOR_BLEND();

         dest += patchcol.buffer->pitch;
         frac += fracstep;
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
   a = patchcol.fg2rgb[patchcol.translation[source[frac >> FRACBITS]]] + patchcol.bg2rgb[*dest]; \
   b = a; \
   a |= 0x01f07c1f; \
   b &= 0x40100400; \
   a &= 0x3fffffff; \
   b  = b - (b >> 5); \
   a |= b; \
   *dest = RGB32k[0][0][a & (a >> 15)]

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
   
   if((count = patchcol.y2 - patchcol.y1 + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned)patchcol.x  >= (unsigned)patchcol.buffer->width || 
      (unsigned)patchcol.y1 >= (unsigned)patchcol.buffer->height) 
      I_Error("V_DrawPatchColumnAddTR: %i to %i at %i", patchcol.y1, patchcol.y2, patchcol.x); 
#endif 

   // Framebuffer destination address.
   // haleyjd: no ylookup or columnofs here due to ability to draw to
   // arbitrary buffers -- would probably use too much memory.
   dest = patchcol.buffer->data + patchcol.y1 * patchcol.buffer->pitch + patchcol.x;

   // Determine scaling, which is the only mapping to be done.
   fracstep = patchcol.step; 
   frac = (patchcol.y1 * fracstep) & 0xFFFF;

   // haleyjd 06/21/06: rewrote and specialized for screen patches
   {
      register const byte *source = patchcol.source;
      
      while((count -= 2) >= 0)
      {
         DO_COLOR_BLEND();

         dest += patchcol.buffer->pitch;
         frac += fracstep;
         
         DO_COLOR_BLEND();
         
         dest += patchcol.buffer->pitch;
         frac += fracstep;
      }
      if(count & 1)
      {
         DO_COLOR_BLEND();
      }
   }
}

#undef DO_COLOR_BLEND

static void V_DrawMaskedColumn(column_t *column)
{
   while(column->topdelta != 0xff)
   {
      // calculate unclipped screen coordinates for post

      int columntop = ytop + column->topdelta;
      patchcol.y1 = patchcol.buffer->y1lookup[columntop];
      patchcol.y2 = patchcol.buffer->y2lookup[columntop + column->length - 1];

      // haleyjd 05/13/08: fix clipping; y2lookup not clamped properly
      if((column->length > 0 && patchcol.y2 < patchcol.y1) ||
         patchcol.y2 >= patchcol.buffer->height)
         patchcol.y2 = patchcol.buffer->height - 1;
      
      if(patchcol.y1 < 0)
         patchcol.y1 = 0;

      // killough 3/2/98, 3/27/98: Failsafe against overflow/crash:
      if(patchcol.y1 <= patchcol.y2 && patchcol.y2 < patchcol.buffer->height)
      {
         patchcol.source = (byte *)column + 3;
         patchcol.colfunc();
      }
      column = (column_t *)((byte *)column + column->length + 4);
   }
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
   int        x1, x2, tx, w;
   fixed_t    scale, iscale, xiscale, startfrac = 0;
   patch_t    *patch = pi->patch;

   w = SHORT(patch->width); // haleyjd: holy crap, stop calling this 800 times
   
   patchcol.buffer = buffer;

   // haleyjd 08/16/08: scale and step values must come from the VBuffer, NOT
   // the Cardboard video structure...

   scale         = (buffer->width << FRACBITS) / SCREENWIDTH;
   iscale        = buffer->ixscale;
   patchcol.step = buffer->iyscale;

   // calculate edges of the shape
   if(pi->flipped)
   {
      // If flipped, then offsets are flipped as well which means they 
      // technically offset from the right side of the patch (x2)
      x2 = pi->x + SHORT(patch->leftoffset);
      x1 = tx = x2 - (SHORT(patch->width) - 1);
   }
   else
   {
      x1 = tx = pi->x - SHORT(patch->leftoffset);
      x2 = tx + SHORT(patch->width) - 1;
   }

   // off the left side
   if(x2 < 0 || x1 >= SCREENWIDTH)
      return;

   xiscale = pi->flipped ? -iscale : iscale;

   // haleyjd 10/10/08: must handle coordinates outside the screen buffer
   // very carefully here.
   if(x1 >= 0)
      x1 = buffer->x1lookup[x1];
   else
      x1 = -buffer->x2lookup[-x1 - 1];

   if(x2 < 320)
      x2 = buffer->x2lookup[x2];
   else
      x2 = buffer->x2lookup[319];

   patchcol.x  = x1 < 0 ? 0 : x1;
   
   // SoM: Any time clipping occurs on screen coords, the resulting clipped 
   // coords should be checked to make sure we are still on screen.
   if(x2 < x1)
      return;

   // SoM: Ok, so the startfrac should ALWAYS be the last post of the patch 
   // when the patch is flipped minus the fractional "bump" from the screen
   // scaling, then the patchcol.x to x1 clipping will place the frac in the
   // correct column no matter what. This also ensures that scaling will be
   // uniform. If the resolution is 320x2X0 the iscale will be 65537 which
   // will create some fractional bump down, so it is safe to assume this 
   // puts us just below patch->width << 16
   if(pi->flipped)
      startfrac = (SHORT(patch->width) << 16) - ((x1 * iscale) & 0xffff) - 1;
   else
      startfrac = (x1 * iscale) & 0xffff;

   if(patchcol.x > x1)
      startfrac += xiscale * (patchcol.x - x1);

   {
      column_t *column;
      int      texturecolumn;
      
      switch(pi->drawstyle)
      {
      case PSTYLE_NORMAL:
         patchcol.colfunc = V_DrawPatchColumn;
         break;
      case PSTYLE_TLATED:
         patchcol.colfunc = V_DrawPatchColumnTR;
         break;
      case PSTYLE_TRANSLUC:
         patchcol.colfunc = V_DrawPatchColumnTL;
         break;
      case PSTYLE_TLTRANSLUC:
         patchcol.colfunc = V_DrawPatchColumnTRTL;
         break;
      case PSTYLE_ADD:
         patchcol.colfunc = V_DrawPatchColumnAdd;
         break;
      case PSTYLE_TLADD:
         patchcol.colfunc = V_DrawPatchColumnAddTR;
         break;
      default:
         I_Error("V_DrawPatchInt: unknown patch drawstyle %d\n", pi->drawstyle);
      }

      ytop = pi->y - SHORT(patch->topoffset);
      
      for(; patchcol.x <= x2; patchcol.x++, startfrac += xiscale)
      {
         texturecolumn = startfrac >> FRACBITS;
         
#ifdef RANGECHECK
         if(texturecolumn < 0 || texturecolumn >= w)
            I_Error("V_DrawPatchInt: bad texturecolumn %i", texturecolumn);
#endif
         
         column = (column_t *)((byte *)patch +
            LONG(patch->columnofs[texturecolumn]));
         V_DrawMaskedColumn(column);
      }
   }
}

void V_SetPatchColrng(byte *colrng)
{
   patchcol.translation = colrng;
}

void V_SetPatchTL(unsigned int *fg, unsigned int *bg)
{
   patchcol.fg2rgb = fg;
   patchcol.bg2rgb = bg;
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

