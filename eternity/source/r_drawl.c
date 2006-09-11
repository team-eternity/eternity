// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
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
//    Low detail column drawers.
//
//-----------------------------------------------------------------------------

#include "r_draw.h"
#include "doomstat.h"
#include "w_wad.h"
#include "r_main.h"
#include "v_video.h"

#define MAXWIDTH  MAX_SCREENWIDTH          /* kilough 2/8/98 */
#define MAXHEIGHT MAX_SCREENHEIGHT

extern int columnofs[MAXWIDTH]; 

static void R_LowDrawColumn(void)
{ 
   int              x, count; 
   register byte    *dest, *dest2;
   register fixed_t frac;
   fixed_t          fracstep;
   
   count = dc_yh - dc_yl + 1;
   
   // Zero length.
   if(count <= 0) 
      return; 

   // Blocky mode, need to multiply by 2.
   x = dc_x << 1;

#ifdef RANGECHECK 
   if(x < 0 || x >= v_width || dc_yl < 0 || dc_yh >= v_height)
      I_Error("R_LowDrawColumn: %i to %i at %i", dc_yl, dc_yh, x);
#endif 
   
   dest  = ylookup[dc_yl] + columnofs[x];
   dest2 = ylookup[dc_yl] + columnofs[x + 1];
   
   fracstep = dc_iscale; 
   frac = dc_texturemid + 
      FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep);

   {
      register const byte *source = dc_source;            
      register const lighttable_t *colormap = dc_colormap; 
      register heightmask = dc_texheight-1;
      
      if(dc_texheight & heightmask)   // not a power of 2 -- killough
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
            
            *dest2 = *dest = colormap[source[frac>>FRACBITS]];
            dest += linesize;                     // killough 11/98
            dest2 += linesize;
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest2 = *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;
            *dest2 = *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;
         }
         if(count & 1)
            *dest2 = *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
      }
   }   
}

static void R_LowDrawTLColumn(void)
{ 
   int              x, count; 
   register byte    *dest, *dest2;           // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   
   count = dc_yh - dc_yl + 1; 
   
   // Zero length, column does not exceed a pixel.
   if(count <= 0)
      return; 
   
   x = dc_x << 1;

#ifdef RANGECHECK 
   if(x < 0 || x >= v_width || dc_yl < 0 || dc_yh >= v_height)
      I_Error("R_LowDrawTLColumn: %i to %i at %i", dc_yl, dc_yh, x);
#endif 

   // Framebuffer destination address.
   // Use ylookup LUT to avoid multiply with ScreenWidth.
   // Use columnofs LUT for subwindows? 
   
   dest  = ylookup[dc_yl] + columnofs[x];
   dest2 = ylookup[dc_yl] + columnofs[x + 1];
   
   // Determine scaling,
   //  which is the only mapping to be done.
   
   fracstep = dc_iscale; 
   frac = dc_texturemid +
      FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep);

   // Inner loop that does the actual texture mapping,
   //  e.g. a DDA-lile scaling.
   // This is as fast as it gets.       (Yeah, right!!! -- killough)
   //
   // killough 2/1/98, 2/21/98: more performance tuning
   
   {
      register const byte *source = dc_source;            
      register const lighttable_t *colormap = dc_colormap; 
      register heightmask = dc_texheight-1;
      
      if(dc_texheight & heightmask)   // not a power of 2 -- killough
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
            
            *dest2 = *dest 
               = tranmap[(*dest<<8) + colormap[source[frac>>FRACBITS]]]; // phares
            dest += linesize;          // killough 11/98
            dest2 += linesize;
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest2 = *dest = 
               tranmap[(*dest<<8)+colormap[source[(frac>>FRACBITS) & heightmask]]]; // phares
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;
            *dest2 = *dest = 
               tranmap[(*dest<<8)+colormap[source[(frac>>FRACBITS) & heightmask]]]; // phares
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;
         }
         if(count & 1)
            *dest2 = *dest = 
               tranmap[(*dest<<8)+colormap[source[(frac>>FRACBITS) & heightmask]]]; // phares
      }
   }   
} 

static void R_LowDrawTLTRColumn(void)
{ 
   int              x, count; 
   register byte    *dest, *dest2;           // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   
   count = dc_yh - dc_yl + 1; 
   
   // Zero length, column does not exceed a pixel.
   if(count <= 0)
      return; 
   
   x = dc_x << 1;

#ifdef RANGECHECK 
   if(x < 0 || x >= v_width || dc_yl < 0 || dc_yh >= v_height)
      I_Error("R_LowDrawTLTRColumn: %i to %i at %i", dc_yl, dc_yh, x);    
#endif 

   // Framebuffer destination address.
   // Use ylookup LUT to avoid multiply with ScreenWidth.
   // Use columnofs LUT for subwindows? 
   
   dest  = ylookup[dc_yl] + columnofs[x];
   dest2 = ylookup[dc_yl] + columnofs[x + 1];
   
   // Determine scaling,
   //  which is the only mapping to be done.
   
   fracstep = dc_iscale; 
   frac = dc_texturemid +
      FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep);

   // Inner loop that does the actual texture mapping,
   //  e.g. a DDA-lile scaling.
   // This is as fast as it gets.       (Yeah, right!!! -- killough)
   //
   // killough 2/1/98, 2/21/98: more performance tuning
   
   {
      register const byte *source = dc_source;            
      register const lighttable_t *colormap = dc_colormap; 
      register heightmask = dc_texheight-1;
      if(dc_texheight & heightmask)   // not a power of 2 -- killough
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
            
            *dest2 = *dest = 
               tranmap[(*dest<<8) + colormap[dc_translation[source[frac>>FRACBITS]]]]; // phares
            dest += linesize;          // killough 11/98
            dest2 += linesize;
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest2 = *dest = 
               tranmap[(*dest<<8)+colormap[dc_translation[source[(frac>>FRACBITS) & heightmask]]]]; // phares
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;
            *dest2 = *dest = 
               tranmap[(*dest<<8)+colormap[dc_translation[source[(frac>>FRACBITS) & heightmask]]]]; // phares
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;
         }
         if(count & 1)
            *dest2 = *dest = 
               tranmap[(*dest<<8)+colormap[dc_translation[source[(frac>>FRACBITS) & heightmask]]]]; // phares
      }
   }   
} 

static void R_LowDrawFuzzColumn(void) 
{ 
   int              x, count; 
   register byte    *dest, *dest2;           // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;

   // Adjust borders. Low...
   if(!dc_yl) 
      dc_yl = 1;
   
   // .. and high.
   if(dc_yh == viewheight - 1) 
      dc_yh = viewheight - 2; 

   count = dc_yh - dc_yl; 

    // Zero length.
    if(count < 0) 
	return; 

   x = dc_x << 1;
    
#ifdef RANGECHECK 
   // haleyjd: these should apparently be adjusted for hires
   // SoM: DONE
   if(x  < 0 || x >= v_width || dc_yl < 0 || dc_yh >= v_height)
      I_Error("R_DrawFuzzColumn: %i to %i at %i", dc_yl, dc_yh, x);
#endif

   dest  = ylookup[dc_yl] + columnofs[x];
   dest2 = ylookup[dc_yl] + columnofs[x + 1];
   
   // Looks familiar.
   fracstep = dc_iscale; 
   frac = dc_texturemid + (dc_yl-centery)*fracstep; 
   
   // Looks like an attempt at dithering,
   // using the colormap #6 (of 0-31, a bit brighter than average).
   
   do 
   {      
      //sf : hires
      *dest  = dc_colormap[6*256+dest [fuzzoffset[fuzzpos] ? v_width : -v_width]];
      *dest2 = dc_colormap[6*256+dest2[fuzzoffset[fuzzpos] ? v_width : -v_width]];
      
      // Clamp table lookup index.
      if(++fuzzpos == FUZZTABLE) 
         fuzzpos = 0;
      
      dest += linesize;
      dest2 += linesize;
      frac += fracstep; 
   } 
   while(count--);
}

static void R_LowDrawTRColumn(void) 
{ 
   int      x, count; 
   byte     *dest, *dest2; 
   fixed_t  frac;
   fixed_t  fracstep;     
   
   count = dc_yh - dc_yl + 1; 
   if(count <= 0) 
      return; 

   x = dc_x << 1;
                                 
#ifdef RANGECHECK 
   if(x  < 0 || x  >= v_width || dc_yl < 0 || dc_yh >= v_height)
      I_Error("R_LowDrawTRColumn: %i to %i at %i", dc_yl, dc_yh, x);
#endif 

   dest  = ylookup[dc_yl] + columnofs[x];
   dest2 = ylookup[dc_yl] + columnofs[x + 1];
   
   // Looks familiar.
   fracstep = dc_iscale; 
   frac = dc_texturemid +
      FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep);

   // Here we do an additional index re-mapping.
   {
      register const byte *source = dc_source;            
      register const lighttable_t *colormap = dc_colormap; 
      register heightmask = dc_texheight-1;
      if(dc_texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
         
         if(frac < 0)
            while ((frac += heightmask) <  0);
         else
            while (frac >= (int)heightmask)
               frac -= heightmask;   
         do
         {
            *dest2 = *dest = colormap[dc_translation[source[frac>>FRACBITS]]]; // phares
            dest += linesize;          // killough 11/98
            dest2 += linesize;
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest2 = *dest = 
               colormap[dc_translation[source[(frac>>FRACBITS) & heightmask]]]; // phares
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;
            *dest2 = *dest = 
               colormap[dc_translation[source[(frac>>FRACBITS) & heightmask]]]; // phares
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;
         }
         if(count & 1)
            *dest2 = *dest = 
               colormap[dc_translation[source[(frac>>FRACBITS) & heightmask]]]; // phares
      }
   }
} 

//
// R_DrawFlexTLColumn
//
// haleyjd 09/01/02: zdoom-style translucency
//
static void R_LowDrawFlexColumn(void)
{ 
   int              x, count; 
   register byte    *dest, *dest2;   // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   unsigned int *fg2rgb, *bg2rgb;
   register unsigned int fg, bg;
   
   count = dc_yh - dc_yl + 1; 

   // Zero length, column does not exceed a pixel.
   if(count <= 0)
      return;

   x = dc_x << 1;
                                 
#ifdef RANGECHECK 
   if(x  < 0 || x  >= v_width || dc_yl < 0 || dc_yh >= v_height)
      I_Error("R_DrawFlexColumn: %i to %i at %i", dc_yl, dc_yh, x);
#endif 

   {
      fixed_t fglevel, bglevel;
      
      fglevel = dc_translevel & ~0x3ff;
      bglevel = FRACUNIT - fglevel;
      fg2rgb  = Col2RGB[fglevel >> 10];
      bg2rgb  = Col2RGB[bglevel >> 10];
   }

   dest  = ylookup[dc_yl] + columnofs[x];
   dest2 = ylookup[dc_yl] + columnofs[x + 1];
  
   fracstep = dc_iscale; 
   frac = dc_texturemid +
      FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep);

   {
      register const byte *source = dc_source;            
      register const lighttable_t *colormap = dc_colormap; 
      register heightmask = dc_texheight-1;
      if (dc_texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if (frac < 0)
            while ((frac += heightmask) <  0);
         else
            while (frac >= (int)heightmask)
               frac -= heightmask;          

         do
         {
            fg = colormap[source[frac>>FRACBITS]];

            // FIXME: test color averaging
            bg = ((unsigned int)*dest + *dest2) >> 1;

            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;
            *dest2 = *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
            
            dest += linesize;          // killough 11/98
            dest2 += linesize;
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0)   // texture height is a power of 2 -- killough
         {
            fg = colormap[source[(frac>>FRACBITS) & heightmask]];
            bg = ((unsigned int)*dest + *dest2) >> 1;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;

            *dest2 = *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;
            
            fg = colormap[source[(frac>>FRACBITS) & heightmask]];
            bg = ((unsigned int)*dest + *dest2) >> 1;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;

            *dest2 = *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;
         }
         if(count & 1)
         {
            fg = colormap[source[(frac>>FRACBITS) & heightmask]];
            bg = ((unsigned int)*dest + *dest2) >> 1;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;

            *dest2 = *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
         }
      }
   }
}

//
// R_DrawFlexTlatedColumn
//
// haleyjd 11/05/02: zdoom-style translucency w/translation, for
// player sprites
//
static void R_LowDrawFlexTRColumn(void) 
{ 
   int              x, count; 
   register byte    *dest, *dest2;   // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   unsigned int *fg2rgb, *bg2rgb;
   register unsigned int fg, bg;
   
   count = dc_yh - dc_yl + 1; 

   // Zero length, column does not exceed a pixel.
   if(count <= 0)
      return;

   x = dc_x << 1;
                                 
#ifdef RANGECHECK 
   if(x  < 0 || x  >= v_width || dc_yl < 0 || dc_yh >= v_height)
      I_Error("R_DrawFlexTRColumn: %i to %i at %i", dc_yl, dc_yh, x);
#endif 

   {
      fixed_t fglevel, bglevel;
      
      fglevel = dc_translevel & ~0x3ff;
      bglevel = FRACUNIT - fglevel;
      fg2rgb  = Col2RGB[fglevel >> 10];
      bg2rgb  = Col2RGB[bglevel >> 10];
   }

   dest  = ylookup[dc_yl] + columnofs[x];
   dest2 = ylookup[dc_yl] + columnofs[x + 1];
  
   fracstep = dc_iscale; 
   frac = dc_texturemid +
      FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep);

   {
      register const byte *source = dc_source;            
      register const lighttable_t *colormap = dc_colormap; 
      register heightmask = dc_texheight-1;
      if (dc_texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if (frac < 0)
            while ((frac += heightmask) <  0);
         else
            while (frac >= (int)heightmask)
               frac -= heightmask;          

         do
         {
            fg = colormap[dc_translation[source[frac>>FRACBITS]]];
            bg = ((unsigned int)*dest + *dest2) >> 1;

            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;
            *dest2 = *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
            
            dest += linesize;          // killough 11/98
            dest2 += linesize;
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            fg = colormap[dc_translation[source[(frac>>FRACBITS) & heightmask]]];
            bg = ((unsigned int)*dest + *dest2) >> 1;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;

            *dest2 = *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;
            
            fg = colormap[dc_translation[source[(frac>>FRACBITS) & heightmask]]];
            bg = ((unsigned int)*dest + *dest2) >> 1;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;

            *dest2 = *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;
         }
         if(count & 1)
         {
            fg = colormap[dc_translation[source[(frac>>FRACBITS) & heightmask]]];
            bg = ((unsigned int)*dest + *dest2) >> 1;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;

            *dest2 = *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
         }
      }
   }
} 

//
// R_DrawAddColumn
//
// haleyjd 02/08/05: additive translucency
//
static void R_LowDrawAddColumn(void)
{ 
   int              x, count; 
   register byte    *dest, *dest2;   // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   unsigned int *fg2rgb, *bg2rgb;
   unsigned int a, b;
   
   count = dc_yh - dc_yl + 1; 

   // Zero length, column does not exceed a pixel.
   if(count <= 0)
      return; 

   x = dc_x << 1;
                                 
#ifdef RANGECHECK 
   if(x  < 0 || x  >= v_width || dc_yl < 0 || dc_yh >= v_height)
      I_Error("R_DrawAddColumn: %i to %i at %i", dc_yl, dc_yh, x);
#endif 

   {
      fixed_t fglevel, bglevel;
      
      fglevel = dc_translevel & ~0x3ff;
      bglevel = FRACUNIT;
      fg2rgb  = Col2RGB[fglevel >> 10];
      bg2rgb  = Col2RGB[bglevel >> 10];
   }

   dest  = ylookup[dc_yl] + columnofs[x];
   dest2 = ylookup[dc_yl] + columnofs[x + 1];
  
   fracstep = dc_iscale; 
   frac = dc_texturemid +
      FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep);

   {
      register const byte *source = dc_source;            
      register const lighttable_t *colormap = dc_colormap; 
      register heightmask = dc_texheight-1;
      if(dc_texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if (frac < 0)
            while ((frac += heightmask) <  0);
         else
            while (frac >= (int)heightmask)
               frac -= heightmask;          

         do
         {
            // mask out LSBs in green and red to allow overflow
            a = fg2rgb[colormap[source[frac>>FRACBITS]]] & 0xFFBFDFF;
            b = bg2rgb[((unsigned int)*dest + *dest2) >> 1] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest2 = *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
            
            dest += linesize;          // killough 11/98
            dest2 += linesize;
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0)   // texture height is a power of 2 -- killough
         {
            a = fg2rgb[colormap[source[(frac>>FRACBITS) & heightmask]]] & 0xFFBFDFF;
            b = bg2rgb[((unsigned int)*dest + *dest2) >> 1] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest2 = *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;

            a = fg2rgb[colormap[source[(frac>>FRACBITS) & heightmask]]] & 0xFFBFDFF;
            b = bg2rgb[((unsigned int)*dest + *dest2) >> 1] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest2 = *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;            
         }
         if(count & 1)
         {
            a = fg2rgb[colormap[source[(frac>>FRACBITS) & heightmask]]] & 0xFFBFDFF;
            b = bg2rgb[((unsigned int)*dest + *dest2) >> 1] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest2 = *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
         }
      }
   }   
}

//
// R_DrawAddTlatedColumn
//
// haleyjd 02/08/05: additive translucency + translation
// The slowest of all column drawers!
//
static void R_LowDrawAddTRColumn(void) 
{ 
   int              x, count; 
   register byte    *dest, *dest2;   // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   unsigned int *fg2rgb, *bg2rgb;
   unsigned int a, b;
   
   count = dc_yh - dc_yl + 1; 

   // Zero length, column does not exceed a pixel.
   if(count <= 0)
      return;

   x = dc_x << 1;
                                 
#ifdef RANGECHECK 
   if(x  < 0 || x  >= v_width || dc_yl < 0 || dc_yh >= v_height)
      I_Error("R_DrawAddTRColumn: %i to %i at %i", dc_yl, dc_yh, x);
#endif 

   {
      fixed_t fglevel, bglevel;
      
      fglevel = dc_translevel & ~0x3ff;
      bglevel = FRACUNIT;
      fg2rgb  = Col2RGB[fglevel >> 10];
      bg2rgb  = Col2RGB[bglevel >> 10];
   }

   dest  = ylookup[dc_yl] + columnofs[x];
   dest2 = ylookup[dc_yl] + columnofs[x + 1];
  
   fracstep = dc_iscale; 
   frac = dc_texturemid +
      FixedMul((dc_yl << FRACBITS) - centeryfrac, fracstep);

   {
      register const byte *source = dc_source;            
      register const lighttable_t *colormap = dc_colormap; 
      register heightmask = dc_texheight-1;
      if(dc_texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if (frac < 0)
            while ((frac += heightmask) <  0);
         else
            while (frac >= (int)heightmask)
               frac -= heightmask;          

         do
         {
            // mask out LSBs in green and red to allow overflow
            a = fg2rgb[colormap[dc_translation[source[frac>>FRACBITS]]]] & 0xFFBFDFF;
            b = bg2rgb[((unsigned int)*dest + *dest2) >> 1] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest2 = *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
            
            dest += linesize;          // killough 11/98
            dest2 += linesize;
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            a = fg2rgb[colormap[dc_translation[source[(frac>>FRACBITS) & heightmask]]]] & 0xFFBFDFF;
            b = bg2rgb[((unsigned int)*dest + *dest2) >> 1] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest2 = *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;

            a = fg2rgb[colormap[dc_translation[source[(frac>>FRACBITS) & heightmask]]]] & 0xFFBFDFF;
            b = bg2rgb[((unsigned int)*dest + *dest2) >> 1] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest2 = *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
            dest += linesize;   // killough 11/98
            dest2 += linesize;
            frac += fracstep;            
         }
         if(count & 1)
         {
            a = fg2rgb[colormap[dc_translation[source[(frac>>FRACBITS) & heightmask]]]] & 0xFFBFDFF;
            b = bg2rgb[((unsigned int)*dest + *dest2) >> 1] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest2 = *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
         }
      }
   }
} 

//
// haleyjd 09/04/06: Quad Column Drawer Object
//
columndrawer_t r_lowdetail_drawer =
{
   R_LowDrawColumn,
   R_LowDrawTLColumn,
   R_LowDrawTRColumn,
   R_LowDrawTLTRColumn,
   R_LowDrawFuzzColumn,
   R_LowDrawFlexColumn,
   R_LowDrawFlexTRColumn,
   R_LowDrawAddColumn,
   R_LowDrawAddTRColumn,

   NULL
};

// EOF

