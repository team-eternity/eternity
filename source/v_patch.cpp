// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//  Functions to draw patches (by post)
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "c_io.h"
#include "m_collection.h"
#include "m_swap.h"
#include "r_patch.h"
#include "v_block.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_png.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_auto.h"

// patch rendering globals -- like dc_ in r_draw.c
static cb_patch_column_t patchcol;
static int ytop;


//
// V_DrawPatchColumn
//
// Draws a plain patch column with no remappings.
//
static void V_DrawPatchColumn() 
{ 
   int      count;
   byte    *dest;    // killough
   fixed_t  frac;    // killough
   fixed_t  fracstep;
   
   if((count = patchcol.y2 - patchcol.y1 + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned int)patchcol.x  >= (unsigned int)patchcol.buffer->width || 
      (unsigned int)patchcol.y1 >= (unsigned int)patchcol.buffer->height) 
      I_Error("V_DrawPatchColumn: %i to %i at %i\n", patchcol.y1, patchcol.y2, patchcol.x); 
#endif 

   dest = VBADDRESS(patchcol.buffer, patchcol.x, patchcol.y1);

   // Determine scaling, which is the only mapping to be done.
   fracstep = patchcol.step;
   frac = patchcol.frac + ((patchcol.y1 * fracstep) & 0xFFFF);

   // Inner loop that does the actual texture mapping,
   //  e.g. a DDA-lile scaling.
   // This is as fast as it gets.       (Yeah, right!!! -- killough)
   //
   // killough 2/1/98: more performance tuning
   // haleyjd 06/21/06: rewrote and specialized for screen patches

   {
      const byte *source = patchcol.source;
            
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
static void V_DrawPatchColumnTR() 
{ 
   int      count;
   byte    *dest;    // killough
   fixed_t  frac;    // killough
   fixed_t  fracstep;
   
   if((count = patchcol.y2 - patchcol.y1 + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned int)patchcol.x  >= (unsigned int)patchcol.buffer->width || 
      (unsigned int)patchcol.y1 >= (unsigned int)patchcol.buffer->height) 
      I_Error("V_DrawPatchColumnTR: %i to %i at %i\n", patchcol.y1, patchcol.y2, patchcol.x); 
#endif 

   dest = VBADDRESS(patchcol.buffer, patchcol.x, patchcol.y1);

   // Determine scaling, which is the only mapping to be done.
   fracstep = patchcol.step;
   frac = patchcol.frac + ((patchcol.y1 * fracstep) & 0xFFFF);

   // Inner loop that does the actual texture mapping,
   //  e.g. a DDA-lile scaling.
   // This is as fast as it gets.       (Yeah, right!!! -- killough)
   //
   // killough 2/1/98: more performance tuning
   // haleyjd 06/21/06: rewrote and specialized for screen patches

   {
      const byte *source = patchcol.source;
            
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

//
// V_DrawPatchColumnTRLit
//
// Draws a plain patch column with color translation and light remapping
//
static void V_DrawPatchColumnTRLit() 
{ 
   int      count;
   byte    *dest;    // killough
   fixed_t  frac;    // killough
   fixed_t  fracstep;
   
   if((count = patchcol.y2 - patchcol.y1 + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned int)patchcol.x  >= (unsigned int)patchcol.buffer->width || 
      (unsigned int)patchcol.y1 >= (unsigned int)patchcol.buffer->height) 
      I_Error("V_DrawPatchColumnTR: %i to %i at %i\n", patchcol.y1, patchcol.y2, patchcol.x); 
#endif 

   dest = VBADDRESS(patchcol.buffer, patchcol.x, patchcol.y1);

   // Determine scaling, which is the only mapping to be done.
   fracstep = patchcol.step;
   frac = patchcol.frac + ((patchcol.y1 * fracstep) & 0xFFFF);

   // Inner loop that does the actual texture mapping,
   //  e.g. a DDA-lile scaling.
   // This is as fast as it gets.       (Yeah, right!!! -- killough)
   //
   // killough 2/1/98: more performance tuning
   // haleyjd 06/21/06: rewrote and specialized for screen patches

   {
      const byte *source = patchcol.source;
            
      while((count -= 2) >= 0)
      {
         *dest = patchcol.light[patchcol.translation[source[frac >> FRACBITS]]];
         dest += patchcol.buffer->pitch;
         frac += fracstep;
         *dest = patchcol.light[patchcol.translation[source[frac >> FRACBITS]]];
         dest += patchcol.buffer->pitch;
         frac += fracstep;
      }
      if(count & 1)
         *dest = patchcol.light[patchcol.translation[source[frac >> FRACBITS]]];
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
   int      count; 
   byte    *dest;           // killough
   fixed_t  frac;           // killough
   fixed_t  fracstep;
   unsigned int fg, bg;
   
   if((count = patchcol.y2 - patchcol.y1 + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned int)patchcol.x  >= (unsigned int)patchcol.buffer->width || 
      (unsigned int)patchcol.y1 >= (unsigned int)patchcol.buffer->height) 
      I_Error("V_DrawPatchColumnTL: %i to %i at %i\n", patchcol.y1, patchcol.y2, patchcol.x); 
#endif 

   dest = VBADDRESS(patchcol.buffer, patchcol.x, patchcol.y1);

   // Determine scaling, which is the only mapping to be done.
   fracstep = patchcol.step;
   frac = patchcol.frac + ((patchcol.y1 * fracstep) & 0xFFFF);

   // haleyjd 06/21/06: rewrote and specialized for screen patches
   {
      const byte *source = patchcol.source;
            
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
void V_DrawPatchColumnTRTL()
{ 
   int      count; 
   byte    *dest;           // killough
   fixed_t  frac;           // killough
   fixed_t  fracstep;
   unsigned int fg, bg;
   
   if((count = patchcol.y2 - patchcol.y1 + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned int)patchcol.x  >= (unsigned int)patchcol.buffer->width || 
      (unsigned int)patchcol.y1 >= (unsigned int)patchcol.buffer->height) 
      I_Error("V_DrawPatchColumnTRTL: %i to %i at %i\n", patchcol.y1, patchcol.y2, patchcol.x); 
#endif 

   dest = VBADDRESS(patchcol.buffer, patchcol.x, patchcol.y1);

   // Determine scaling, which is the only mapping to be done.
   fracstep = patchcol.step; 
   frac = patchcol.frac + ((patchcol.y1 * fracstep) & 0xFFFF);

   // haleyjd 06/21/06: rewrote and specialized for screen patches
   {
      const byte *source = patchcol.source;
      
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
void V_DrawPatchColumnAdd()
{ 
   int      count; 
   byte    *dest;           // killough
   fixed_t  frac;           // killough
   fixed_t  fracstep;
   unsigned int a, b;
   
   if((count = patchcol.y2 - patchcol.y1 + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned int)patchcol.x  >= (unsigned int)patchcol.buffer->width || 
      (unsigned int)patchcol.y1 >= (unsigned int)patchcol.buffer->height) 
      I_Error("V_DrawPatchColumnAdd: %i to %i at %i\n", patchcol.y1, patchcol.y2, patchcol.x); 
#endif 

   dest = VBADDRESS(patchcol.buffer, patchcol.x, patchcol.y1);

   // Determine scaling, which is the only mapping to be done.
   fracstep = patchcol.step; 
   frac = patchcol.frac + ((patchcol.y1 * fracstep) & 0xFFFF);

   // haleyjd 06/21/06: rewrote and specialized for screen patches
   {
      const byte *source = patchcol.source;
      
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
void V_DrawPatchColumnAddTR()
{ 
   int      count; 
   byte    *dest;           // killough
   fixed_t  frac;           // killough
   fixed_t  fracstep;
   unsigned int a, b;
   
   if((count = patchcol.y2 - patchcol.y1 + 1) <= 0)
      return; // Zero length, column does not exceed a pixel.
                                 
#ifdef RANGECHECK 
   if((unsigned int)patchcol.x  >= (unsigned int)patchcol.buffer->width || 
      (unsigned int)patchcol.y1 >= (unsigned int)patchcol.buffer->height) 
      I_Error("V_DrawPatchColumnAddTR: %i to %i at %i\n", patchcol.y1, patchcol.y2, patchcol.x); 
#endif 

   dest = VBADDRESS(patchcol.buffer, patchcol.x, patchcol.y1);

   // Determine scaling, which is the only mapping to be done.
   fracstep = patchcol.step; 
   frac = patchcol.frac + ((patchcol.y1 * fracstep) & 0xFFFF);

   // haleyjd 06/21/06: rewrote and specialized for screen patches
   {
      const byte *source = patchcol.source;
      
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
   for(;column->topdelta != 0xff; column = (column_t *)((byte *)column + column->length + 4))
   {
      // calculate unclipped screen coordinates for post

      int columntop = ytop + column->topdelta;

      if(columntop >= 0)
      {
         // SoM: Make sure the lut is never referenced out of range
         if(columntop >= patchcol.buffer->unscaledh)
            return;
            
         patchcol.y1 = patchcol.buffer->y1lookup[columntop];
         patchcol.frac = 0;
      }
      else
      {
         patchcol.frac = (-columntop) << FRACBITS;
         patchcol.y1 = 0;
      }

      if(columntop + column->length - 1 < 0)
         continue;
      if(columntop + column->length - 1 < patchcol.buffer->unscaledh)
         patchcol.y2 = patchcol.buffer->y2lookup[columntop + column->length - 1];
      else
         patchcol.y2 = patchcol.buffer->y2lookup[patchcol.buffer->unscaledh - 1];

      // SoM: The failsafes should be completely redundant now...
      // haleyjd 05/13/08: fix clipping; y2lookup not clamped properly
      if((column->length > 0 && patchcol.y2 < patchcol.y1) ||
         patchcol.y2 >= patchcol.buffer->height)
         patchcol.y2 = patchcol.buffer->height - 1;
      
      // killough 3/2/98, 3/27/98: Failsafe against overflow/crash:
      if(patchcol.y1 <= patchcol.y2 && patchcol.y2 < patchcol.buffer->height)
      {
         patchcol.source = (byte *)column + 3;
         patchcol.colfunc();
      }
   }
}

static void V_DrawMaskedColumnUnscaled(column_t *column)
{
   for(;column->topdelta != 0xff; column = (column_t *)((byte *)column + column->length + 4))
   {
      // calculate unclipped screen coordinates for post

      int columntop = ytop + column->topdelta;

      if(columntop >= 0)
      {
         patchcol.y1 = columntop;
         patchcol.frac = 0;
      }
      else
      {
         patchcol.frac = (-columntop) << FRACBITS;
         patchcol.y1 = 0;
      }

      if(columntop + column->length - 1 < patchcol.buffer->height)
         patchcol.y2 = columntop + column->length - 1;
      else
         patchcol.y2 = patchcol.buffer->height - 1;

      // killough 3/2/98, 3/27/98: Failsafe against overflow/crash:
      if(patchcol.y1 <= patchcol.y2 && patchcol.y2 < patchcol.buffer->height)
      {
         patchcol.source = (byte *)column + 3;
         patchcol.colfunc();
      }
   }
}

typedef void (*patchcolfunc_t)(void);

static patchcolfunc_t colfuncfordrawstyle[PSTYLE_NUMSTYLES] =
{
   V_DrawPatchColumn,
   V_DrawPatchColumnTR,
   V_DrawPatchColumnTL,
   V_DrawPatchColumnTRTL,
   V_DrawPatchColumnAdd,
   V_DrawPatchColumnAddTR,
   V_DrawPatchColumnTRLit
};

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
   int        x1, x2, w;
   fixed_t    iscale, xiscale, startfrac = 0;
   patch_t    *patch = pi->patch;
   int        maxw;
   void       (*maskcolfunc)(column_t *);

   w = patch->width;
   
   patchcol.buffer = buffer;

   // calculate edges of the shape
   if(pi->flipped)
   {
      // If flipped, then offsets are flipped as well which means they 
      // technically offset from the right side of the patch (x2)
      x2 = pi->x + patch->leftoffset;
      x1 = x2 - (w - 1);
   }
   else
   {
      x1 = pi->x - patch->leftoffset;
      x2 = x1 + w - 1;
   }

   // haleyjd 08/16/08: scale and step values must come from the VBuffer, NOT
   // the Cardboard video structure...

   if(buffer->scaled)
   {      
      iscale        = buffer->ixscale;
      patchcol.step = buffer->iyscale;
      maxw          = buffer->unscaledw;
      maskcolfunc   = V_DrawMaskedColumn;
   }
   else
   {
      iscale = patchcol.step = FRACUNIT;
      maxw = buffer->width;
      maskcolfunc   = V_DrawMaskedColumnUnscaled;
   }

   // off the left or right side?
   if(x2 < 0 || x1 >= maxw)
      return;

   xiscale = pi->flipped ? -iscale : iscale;

   if(buffer->scaled)
   {
      // haleyjd 10/10/08: must handle coordinates outside the screen buffer
      // very carefully here.
      if(x1 >= 0)
         x1 = buffer->x1lookup[x1];
      else
         x1 = -buffer->x2lookup[-x1 - 1];

      if(x2 < buffer->unscaledw)
         x2 = buffer->x2lookup[x2];
      else
         x2 = buffer->x2lookup[buffer->unscaledw - 1];
   }

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
      startfrac = (w << 16) - ((x1 * iscale) & 0xffff) - 1;
   else
      startfrac = (x1 * iscale) & 0xffff;

   if(patchcol.x > x1)
      startfrac += xiscale * (patchcol.x - x1);

   {
      column_t *column;
      int      texturecolumn;

#ifdef RANGECHECK
      if(pi->drawstyle < 0 || pi->drawstyle >= PSTYLE_NUMSTYLES)
         I_Error("V_DrawPatchInt: unknown patch drawstyle %d\n", pi->drawstyle);
#endif
      patchcol.colfunc = colfuncfordrawstyle[pi->drawstyle];

      ytop = pi->y - patch->topoffset;
      
      for(; patchcol.x <= x2; patchcol.x++, startfrac += xiscale)
      {
         texturecolumn = startfrac >> FRACBITS;
         
#ifdef RANGECHECK
         if(texturecolumn < 0 || texturecolumn >= w)
            I_Error("V_DrawPatchInt: bad texturecolumn %d\n", texturecolumn);
#endif
         
         column = (column_t *)((byte *)patch + patch->columnofs[texturecolumn]);
         maskcolfunc(column);
      }
   }
}

void V_SetPatchColrng(byte *colrng)
{
   patchcol.translation = colrng;
}

void V_SetPatchLight(byte *lighttable)
{
   patchcol.light = lighttable;
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
// V_PatchToLinear
//
// haleyjd 11/02/08: converts a patch into a linear buffer
//
byte *V_PatchToLinear(patch_t *patch, bool flipped, byte fillcolor,
                      int *width, int *height)
{
   int w = patch->width;
   int h = patch->height;
   int col = w - 1, colstop = -1, colstep = -1;

   byte *desttop;
   byte *buffer = emalloc(byte *, w * h);

   memset(buffer, fillcolor, w * h);
  
   if(!flipped)
   {
      col = 0;
      colstop = w;
      colstep = 1;
   }

   desttop = buffer;
      
   for(; col != colstop; col += colstep, ++desttop)
   {
      const column_t *column = 
         (const column_t *)((byte *)patch + patch->columnofs[col]);
      
      // step through the posts in a column
      while(column->topdelta != 0xff)
      {
         // killough 2/21/98: Unrolled and performance-tuned         
         const byte *source = (byte *)column + 3;
         byte *dest = desttop + column->topdelta * w;
         int count = column->length;
         int diff;

         // haleyjd 11/02/08: clip to indicated height
         if((diff = (count + column->topdelta) - h) > 0)
            count -= diff;

         // haleyjd: make sure there's something left to draw
         if(count <= 0)
            break;
         
         if((count -= 4) >= 0)
         {
            do
            {
               byte s0, s1;
               s0 = source[0];
               s1 = source[1];
               dest[0] = s0;
               dest[w] = s1;
               dest += w * 2;
               s0 = source[2];
               s1 = source[3];
               source += 4;
               dest[0] = s0;
               dest[w] = s1;
               dest += w * 2;
            }
            while((count -= 4) >= 0);
         }
         if(count += 4)
         {
            do
            {
               *dest = *source++;
               dest += w;
            }
            while(--count);
         }
         column = reinterpret_cast<const column_t *>(source + 1); // killough 2/21/98 even faster
      }
   }

   // optionally return width and height
   if(width)
      *width = w;
   if(height)
      *height = h;

   // voila!
   return buffer;
}


//
// V_LinearToPatch
//
// haleyjd 07/07/07: converts a linear graphic to a patch
// TODO: Does this need removal? Perhaps just use V_LinearToTransPatch as default
//
patch_t *V_LinearToPatch(byte *linear, int w, int h, size_t *memsize, 
                         int tag, void **user)
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
      4 * sizeof(int16_t) + w * (h + sizeof(int32_t) + sizeof(column_t) + 3);
   
   byte *out = ecalloctag(byte *, 1, total_size, tag, user);

   p = reinterpret_cast<patch_t *>(out);

   // set basic header information
   p->width      = w;
   p->height     = h;
   p->topoffset  = 0;
   p->leftoffset = 0;

   // get pointer to columnofs table
   columnofs = (int *)(out + 4 * sizeof(int16_t));

   // skip past columnofs table
   dest = out + 4 * sizeof(int16_t) + w * sizeof(int32_t);

   // convert columns of linear graphic into true columns
   for(x = 0; x < w; ++x)
   {
      // set entry in columnofs table
      columnofs[x] = int(dest - out);

      // set basic column properties
      c = reinterpret_cast<column_t *>(dest);
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

class VPatchPost
{
public:
   uint8_t row_off;
   PODCollection<uint8_t> pixels;
};

class VPatchColumn
{
public:
   Collection<VPatchPost> posts;
};

#define PUTBYTE(r, v) *r = (uint8_t)(v); ++r

#define PUTSHORT(r, v)                          \
   *(r+0) = (byte)(((uint16_t)(v) >> 0) & 0xff); \
   *(r+1) = (byte)(((uint16_t)(v) >> 8) & 0xff); \
   r += 2

#define PUTLONG(r, v)                             \
   *(r+0) = (byte)(((uint32_t)(v) >>  0) & 0xff); \
   *(r+1) = (byte)(((uint32_t)(v) >>  8) & 0xff); \
   *(r+2) = (byte)(((uint32_t)(v) >> 16) & 0xff); \
   *(r+3) = (byte)(((uint32_t)(v) >> 24) & 0xff); \
   r += 4


//
// Converts a linear graphic to a patch with transparency.
// Mostly straight from psxwadgen, which is mostly straight from SLADE.
//
patch_t *V_LinearToTransPatch(const byte *linear, int width, int height, size_t *memsize,
                              int color_key, int tag, void **user)
{
   Collection<VPatchColumn> columns;

   // Go through columns
   uint32_t offset = 0;
   for(int c = 0; c < width; c++)
   {
      VPatchColumn col;
      VPatchPost   post;
      post.row_off = 0;
      bool ispost = false;
      bool first_254 = true;  // first 254 pixels use absolute offsets

      offset = c;
      uint8_t row_off = 0;
      for(int r = 0; r < height; r++)
      {
         // if we're at offset 254, create a dummy post for tall doom gfx support
         if(row_off == 254)
         {
            // Finish current post if any
            if(ispost)
            {
               col.posts.add(post);
               post.pixels.makeEmpty();
               ispost = false;
            }

            // Begin relative offsets
            first_254 = false;

            // Create dummy post
            post.row_off = 254;
            col.posts.add(post);

            // Clear post
            row_off = 0;
            ispost = false;
         }

         // If the current pixel is not transparent, add it to the current post
         // FIXME: Make this check mask-based (check mask[offset] > 0)
         if(linear[offset] != color_key)
         {
            // If we're not currently building a post, begin one and set its offset
            if(!ispost)
            {
               // Set offset
               post.row_off = row_off;

               // Reset offset if we're in relative offsets mode
               if(!first_254)
                  row_off = 0;

               // Start post
               ispost = true;
            }

            // Add the pixel to the post
            post.pixels.add(linear[offset]);
         }
         else if(ispost)
         {
            // If the current pixel is transparent and we are currently building
            // a post, add the current post to the list and clear it
            col.posts.add(post);
            post.pixels.makeEmpty();
            ispost = false;
         }

         // Go to next row
         offset += width;
         ++row_off;
      }

      // If the column ended with a post, add it
      if(ispost)
         col.posts.add(post);

      // Add the column data
      columns.add(col);

      // Go to next column
      ++offset;
   }

   size_t size;

   // Calculate needed memory size to allocate patch buffer
   size = 0;
   size += 4 * sizeof(int16_t);                   // 4 header shorts
   size += columns.getLength() * sizeof(int32_t); // offsets table

   for(size_t c = 0; c < columns.getLength(); c++)
   {
      for(size_t p = 0; p < columns[c].posts.getLength(); p++)
      {
         size_t post_len = 0;

         post_len += 2; // two bytes for post header
         post_len += 1; // dummy byte
         post_len += columns[c].posts[p].pixels.getLength(); // pixels
         post_len += 1; // dummy byte

         size += post_len;
      }

      size += 1; // room for 0xff cap byte
   }

   byte *output = ecalloctag(byte *, size, 1, tag, user);
   byte *rover = output;

   // write header fields
   PUTSHORT(rover, width);
   PUTSHORT(rover, height);
   // This is written to afterwards
   PUTSHORT(rover, 0);
   PUTSHORT(rover, 0);

   // set starting position of column offsets table, and skip over it
   byte *col_offsets = rover;
   rover += columns.getLength() * 4;

   for(size_t c = 0; c < columns.getLength(); c++)
   {
      // write column offset to offset table
      uint32_t offs = (uint32_t)(rover - output);
      PUTLONG(col_offsets, offs);

      // write column posts
      for(size_t p = 0; p < columns[c].posts.getLength(); p++)
      {
         // Write row offset
         PUTBYTE(rover, columns[c].posts[p].row_off);

         // Write number of pixels
         size_t numPixels = columns[c].posts[p].pixels.getLength();
         PUTBYTE(rover, numPixels);

         // Write pad byte
         byte lastval = numPixels ? columns[c].posts[p].pixels[0] : 0;
         PUTBYTE(rover, lastval);

         // Write pixels
         for(size_t a = 0; a < numPixels; a++)
         {
            lastval = columns[c].posts[p].pixels[a];
            PUTBYTE(rover, lastval);
         }

         // Write pad byte
         PUTBYTE(rover, lastval);
      }

      // Write 255 cap byte
      PUTBYTE(rover, 0xff);
   }

   // allow returning size of allocation in *memsize
   if(memsize)
      *memsize = size;

   // Done!
   return reinterpret_cast<patch_t *>(output);
}

//
// V_WritePatchAsPNG
//
// Does what it says on the box.
//
bool V_WritePatchAsPNG(const char *lump, const char *filename, byte fillcolor)
{
   int lumpnum;
   ZAutoBuffer patch;
   byte *data;
   byte *linear;
   int width, height;

   // make sure it exists
   if((lumpnum = wGlobalDir.checkNumForName(lump)) < 0)
   {
      C_Printf(FC_ERROR "No such lump %s", lump);
      return false;
   }
   
   // cache the target lump into an autobuffer. We want to do it this way here,
   // versus loading it normally, because we don't want to deal with caching of
   // formatted lumps, substitution of the default patch, etc
   wGlobalDir.cacheLumpAuto(lumpnum, patch);
   if(!(data = patch.getAs<byte *>()))
   {
      C_Printf(FC_ERROR "Failed to load lump %s", lump);
      return false;
   }

   // verify and format the patch data
   if(!PatchLoader::VerifyAndFormat(data, patch.getSize()))
   {
      C_Printf(FC_ERROR "Lump %s is not a valid patch", lump);
      return false;
   }

   // convert to linear
   linear = V_PatchToLinear(reinterpret_cast<patch_t *>(data), false, fillcolor, 
                            &width, &height);

   // Finally, write out the linear as a PNG.
   bool res;
   if((res = V_WritePNG(linear, width, height, filename)))
      C_Printf("Wrote patch %s to file %s", lump, filename);
   else
      C_Printf(FC_ERROR "Write of patch %s to file %s failed", lump, filename);

   efree(linear);
   return res;
}
 

// EOF

