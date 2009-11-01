// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley, Stephen McGranahan
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
//
// Optimized quad column buffer code.
// By SoM.
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

// SoM: OPTIMIZE for ANYRES
typedef enum
{
   COL_NONE,
   COL_OPAQUE,
   COL_TRANS,
   COL_FLEXTRANS,
   COL_FUZZ,
   COL_FLEXADD
} columntype_e;

static int    temp_x = 0;
static int    tempyl[4], tempyh[4];
static byte   tempbuf[MAX_SCREENHEIGHT * 4];
static int    startx = 0;
static int    temptype = COL_NONE;
static int    commontop, commonbot;
static byte   *temptranmap = NULL;
static fixed_t temptranslevel;
// haleyjd 09/12/04: optimization -- precalculate flex tran lookups
static unsigned int *temp_fg2rgb;
static unsigned int *temp_bg2rgb;
// SoM 7-28-04: Fix the fuzz problem.
static byte   *tempfuzzmap;

//
// Error functions that will abort if R_FlushColumns tries to flush 
// columns without a column type.
//

static void R_FlushWholeError(void)
{
   I_Error("R_FlushWholeColumns called without being initialized.\n");
}

static void R_FlushHTError(void)
{
   I_Error("R_FlushHTColumns called without being initialized.\n");
}

static void R_QuadFlushError(void)
{
   I_Error("R_FlushQuadColumn called without being initialized.\n");
}

//
// R_FlushWholeOpaque
//
// Flushes the entire columns in the buffer, one at a time.
// This is used when a quad flush isn't possible.
// Opaque version -- no remapping whatsoever.
//
static void R_FlushWholeOpaque(void)
{
   register byte *source;
   register byte *dest;
   register int  count, yl;

   while(--temp_x >= 0)
   {
      yl     = tempyl[temp_x];
      source = tempbuf + temp_x + (yl << 2);
      dest   = ylookup[yl] + columnofs[startx + temp_x];
      count  = tempyh[temp_x] - yl + 1;
      
      while(--count >= 0)
      {
         *dest = *source;
         source += 4;
         dest += linesize;
      }
   }
}

//
// R_FlushHTOpaque
//
// Flushes the head and tail of columns in the buffer in
// preparation for a quad flush.
// Opaque version -- no remapping whatsoever.
//
static void R_FlushHTOpaque(void)
{
   register byte *source;
   register byte *dest;
   register int count, colnum = 0;
   int yl, yh;

   while(colnum < 4)
   {
      yl = tempyl[colnum];
      yh = tempyh[colnum];
      
      // flush column head
      if(yl < commontop)
      {
         source = tempbuf + colnum + (yl << 2);
         dest   = ylookup[yl] + columnofs[startx + colnum];
         count  = commontop - yl;
         
         while(--count >= 0)
         {
            *dest = *source;
            source += 4;
            dest += linesize;
         }
      }
      
      // flush column tail
      if(yh > commonbot)
      {
         source = tempbuf + colnum + ((commonbot + 1) << 2);
         dest   = ylookup[(commonbot + 1)] + columnofs[startx + colnum];
         count  = yh - commonbot;
         
         while(--count >= 0)
         {
            *dest = *source;
            source += 4;
            dest += linesize;
         }
      }         
      ++colnum;
   }
}

static void R_FlushWholeTL(void)
{
   register byte *source;
   register byte *dest;
   register int  count, yl;

   while(--temp_x >= 0)
   {
      yl     = tempyl[temp_x];
      source = tempbuf + temp_x + (yl << 2);
      dest   = ylookup[yl] + columnofs[startx + temp_x];
      count  = tempyh[temp_x] - yl + 1;

      while(--count >= 0)
      {
         // haleyjd 09/11/04: use temptranmap here
         *dest = temptranmap[(*dest<<8) + *source];
         source += 4;
         dest += linesize;
      }
   }
}

static void R_FlushHTTL(void)
{
   register byte *source;
   register byte *dest;
   register int count;
   int colnum = 0, yl, yh;

   while(colnum < 4)
   {
      yl = tempyl[colnum];
      yh = tempyh[colnum];

      // flush column head
      if(yl < commontop)
      {
         source = tempbuf + colnum + (yl << 2);
         dest   = ylookup[yl] + columnofs[startx + colnum];
         count  = commontop - yl;

         while(--count >= 0)
         {
            // haleyjd 09/11/04: use temptranmap here
            *dest = temptranmap[(*dest<<8) + *source];
            source += 4;
            dest += linesize;
         }
      }

      // flush column tail
      if(yh > commonbot)
      {
         source = tempbuf + colnum + ((commonbot + 1) << 2);
         dest   = ylookup[(commonbot + 1)] + columnofs[startx + colnum];
         count  = yh - commonbot;

         while(--count >= 0)
         {
            // haleyjd 09/11/04: use temptranmap here
            *dest = temptranmap[(*dest<<8) + *source];
            source += 4;
            dest += linesize;
         }
      }
      
      ++colnum;
   }
}

#define SRCPIXEL \
   tempfuzzmap[6*256+dest[fuzzoffset[fuzzpos] ? video.pitch: -video.pitch]]

static void R_FlushWholeFuzz(void)
{
   register byte *source;
   register byte *dest;
   register int  count, yl;

   while(--temp_x >= 0)
   {
      yl     = tempyl[temp_x];
      source = tempbuf + temp_x + (yl << 2);
      dest   = ylookup[yl] + columnofs[startx + temp_x];
      count  = tempyh[temp_x] - yl + 1;

      while(--count >= 0)
      {
         // SoM 7-28-04: Fix the fuzz problem.
         *dest = SRCPIXEL;
         
         // Clamp table lookup index.
         if(++fuzzpos == FUZZTABLE) 
            fuzzpos = 0;
         
         source += 4;
         dest += linesize;
      }
   }
}

/*
static void R_FlushHTFuzz(void)
{
   register byte *source;
   register byte *dest;
   register int count;
   int colnum = 0, yl, yh;

   while(colnum < 4)
   {
      yl = tempyl[colnum];
      yh = tempyh[colnum];

      // flush column head
      if(yl < commontop)
      {
         source = tempbuf + colnum + (yl << 2);
         dest   = ylookup[yl] + columnofs[startx + colnum];
         count  = commontop - yl;

         while(--count >= 0)
         {
            // SoM 7-28-04: Fix the fuzz problem.
            *dest = SRCPIXEL;
            
            // Clamp table lookup index.
            if(++fuzzpos == FUZZTABLE) 
               fuzzpos = 0;
            
            source += 4;
            dest += linesize;
         }
      }

      // flush column tail
      if(yh > commonbot)
      {
         source = tempbuf + colnum + ((commonbot + 1) << 2);
         dest   = ylookup[(commonbot + 1)] + columnofs[startx + colnum];
         count  = yh - commonbot;

         while(--count >= 0)
         {
            // SoM 7-28-04: Fix the fuzz problem.
            *dest = SRCPIXEL;
            
            // Clamp table lookup index.
            if(++fuzzpos == FUZZTABLE) 
               fuzzpos = 0;
            
            source += 4;
            dest += linesize;
         }
      }
      
      ++colnum;
   }
}
*/

#undef SRCPIXEL

static void R_FlushWholeFlex(void)
{
   register byte *source;
   register byte *dest;
   register int  count, yl;
   unsigned int fg, bg;

   while(--temp_x >= 0)
   {
      yl     = tempyl[temp_x];
      source = tempbuf + temp_x + (yl << 2);
      dest   = ylookup[yl] + columnofs[startx + temp_x];
      count  = tempyh[temp_x] - yl + 1;

      while(--count >= 0)
      {
         // haleyjd 09/12/04: use precalculated lookups
         fg = temp_fg2rgb[*source];
         bg = temp_bg2rgb[*dest];
         fg = (fg+bg) | 0x1f07c1f;
         *dest = RGB32k[0][0][fg & (fg>>15)];
         
         source += 4;
         dest += linesize;
      }
   }
}

static void R_FlushHTFlex(void)
{
   register byte *source;
   register byte *dest;
   register int count;
   int colnum = 0, yl, yh;
   unsigned int fg, bg;

   while(colnum < 4)
   {
      yl = tempyl[colnum];
      yh = tempyh[colnum];

      // flush column head
      if(yl < commontop)
      {
         source = tempbuf + colnum + (yl << 2);
         dest   = ylookup[yl] + columnofs[startx + colnum];
         count  = commontop - yl;

         while(--count >= 0)
         {
            // haleyjd 09/12/04: use precalculated lookups
            fg = temp_fg2rgb[*source];
            bg = temp_bg2rgb[*dest];
            fg = (fg+bg) | 0x1f07c1f;
            *dest = RGB32k[0][0][fg & (fg>>15)];
            
            source += 4;
            dest += linesize;
         }
      }

      // flush column tail
      if(yh > commonbot)
      {
         source = tempbuf + colnum + ((commonbot + 1) << 2);
         dest   = ylookup[(commonbot + 1)] + columnofs[startx + colnum];
         count  = yh - commonbot;

         while(--count >= 0)
         {
            // haleyjd 09/12/04: use precalculated lookups
            fg = temp_fg2rgb[*source];
            bg = temp_bg2rgb[*dest];
            fg = (fg+bg) | 0x1f07c1f;
            *dest = RGB32k[0][0][fg & (fg>>15)];
            
            source += 4;
            dest += linesize;
         }
      }
      
      ++colnum;
   }
}

static void R_FlushWholeFlexAdd(void)
{
   register byte *source;
   register byte *dest;
   register int  count, yl;
   unsigned int a, b;

   while(--temp_x >= 0)
   {
      yl     = tempyl[temp_x];
      source = tempbuf + temp_x + (yl << 2);
      dest   = ylookup[yl] + columnofs[startx + temp_x];
      count  = tempyh[temp_x] - yl + 1;

      while(--count >= 0)
      {
         // mask out LSBs in green and red to allow overflow
         a = temp_fg2rgb[*source] + temp_bg2rgb[*dest];
         b = a;
         
         a |= 0x01f07c1f;
         b &= 0x40100400;
         a &= 0x3fffffff;
         b  = b - (b >> 5);
         a |= b;
         
         *dest = RGB32k[0][0][a & (a >> 15)];
         
         source += 4;
         dest += linesize;
      }
   }
}

static void R_FlushHTFlexAdd(void)
{
   register byte *source;
   register byte *dest;
   register int count;
   int colnum = 0, yl, yh;
   unsigned int a, b;

   while(colnum < 4)
   {
      yl = tempyl[colnum];
      yh = tempyh[colnum];

      // flush column head
      if(yl < commontop)
      {
         source = tempbuf + colnum + (yl << 2);
         dest   = ylookup[yl] + columnofs[startx + colnum];
         count  = commontop - yl;

         while(--count >= 0)
         {
            // mask out LSBs in green and red to allow overflow
            a = temp_fg2rgb[*source] + temp_bg2rgb[*dest];
            b = a;
            
            a |= 0x01f07c1f;
            b &= 0x40100400;
            a &= 0x3fffffff;
            b  = b - (b >> 5);
            a |= b;
            
            *dest = RGB32k[0][0][a & (a >> 15)];
            
            source += 4;
            dest += linesize;
         }
      }

      // flush column tail
      if(yh > commonbot)
      {
         source = tempbuf + colnum + ((commonbot + 1) << 2);
         dest   = ylookup[(commonbot + 1)] + columnofs[startx + colnum];
         count  = yh - commonbot;

         while(--count >= 0)
         {
            // mask out LSBs in green and red to allow overflow
            a = temp_fg2rgb[*source] + temp_bg2rgb[*dest];
            b = a;
            
            a |= 0x01f07c1f;
            b &= 0x40100400;
            a &= 0x3fffffff;
            b  = b - (b >> 5);
            a |= b;
            
            *dest = RGB32k[0][0][a & (a >> 15)];
            
            source += 4;
            dest += linesize;
         }
      }
      
      ++colnum;
   }
}

static void (*R_FlushWholeColumns)(void) = R_FlushWholeError;
static void (*R_FlushHTColumns)(void)    = R_FlushHTError;

// Begin: Quad column flushing functions.
static void R_FlushQuadOpaque(void)
{
   register int *source = (int *)(tempbuf + (commontop << 2));
   register int *dest = (int *)(ylookup[commontop] + columnofs[startx]);
   register int count;
   register int deststep = linesize / 4;

   count = commonbot - commontop + 1;

   while(--count >= 0)
   {
      *dest = *source++;
      dest += deststep;
   }
}

static void R_FlushQuadTL(void)
{
   register byte *source = tempbuf + (commontop << 2);
   register byte *dest = ylookup[commontop] + columnofs[startx];
   register int count;

   count = commonbot - commontop + 1;

   while(--count >= 0)
   {
      *dest   = temptranmap[(*dest<<8) + *source];
      dest[1] = temptranmap[(dest[1]<<8) + source[1]];
      dest[2] = temptranmap[(dest[2]<<8) + source[2]];
      dest[3] = temptranmap[(dest[3]<<8) + source[3]];
      source += 4;
      dest += linesize;
   }
}

/*
#define SRCPIXEL(n, p) \
   tempfuzzmap[6*256+dest[(n) + fuzzoffset[(p)] ? video.pitch: -video.pitch]];

static void R_FlushQuadFuzz(void)
{
   register byte *source = tempbuf + (commontop << 2);
   register byte *dest = ylookup[commontop] + columnofs[startx];
   register int count;
   int fuzz1, fuzz2, fuzz3, fuzz4;

   // haleyjd 11/11/06: changed to be more like original;
   // special thanks to proff from the PrBoom team
   fuzz1 = fuzzpos;
   fuzz2 = (fuzz1 + tempyl[1]) % FUZZTABLE;
   fuzz3 = (fuzz2 + tempyl[2]) % FUZZTABLE;
   fuzz4 = (fuzz3 + tempyl[3]) % FUZZTABLE;


   count = commonbot - commontop + 1;

   while(--count >= 0)
   {
      // SoM 7-28-04: Fix the fuzz problem.
      *dest = SRCPIXEL(0, fuzz1);
      if(++fuzz1 == FUZZTABLE) fuzz1 = 0;
      dest[1] = SRCPIXEL(1, fuzz2);
      if(++fuzz2 == FUZZTABLE) fuzz2 = 0;
      dest[2] = SRCPIXEL(2, fuzz3);
      if(++fuzz3 == FUZZTABLE) fuzz3 = 0;
      dest[3] = SRCPIXEL(3, fuzz4);
      if(++fuzz4 == FUZZTABLE) fuzz4 = 0;

      source += 4;
      dest += linesize;
   }

   fuzzpos = fuzz4;
}

#undef SRCPIXEL
*/

static void R_FlushQuadFlex(void)
{
   register byte *source = tempbuf + (commontop << 2);
   register byte *dest = ylookup[commontop] + columnofs[startx];
   register int count;
   unsigned int fg, bg;

   count = commonbot - commontop + 1;

   while(--count >= 0)
   {
      // haleyjd 09/12/04: use precalculated lookups
      fg = temp_fg2rgb[*source];
      bg = temp_bg2rgb[*dest];
      fg = (fg+bg) | 0x1f07c1f;
      *dest = RGB32k[0][0][fg & (fg>>15)];

      fg = temp_fg2rgb[source[1]];
      bg = temp_bg2rgb[dest[1]];
      fg = (fg+bg) | 0x1f07c1f;
      dest[1] = RGB32k[0][0][fg & (fg>>15)];

      fg = temp_fg2rgb[source[2]];
      bg = temp_bg2rgb[dest[2]];
      fg = (fg+bg) | 0x1f07c1f;
      dest[2] = RGB32k[0][0][fg & (fg>>15)];

      fg = temp_fg2rgb[source[3]];
      bg = temp_bg2rgb[dest[3]];
      fg = (fg+bg) | 0x1f07c1f;
      dest[3] = RGB32k[0][0][fg & (fg>>15)];

      source += 4;
      dest += linesize;
   }
}

static void R_FlushQuadFlexAdd(void)
{
   register byte *source = tempbuf + (commontop << 2);
   register byte *dest = ylookup[commontop] + columnofs[startx];
   register int count;
   unsigned int a, b;

   count = commonbot - commontop + 1;

   while(--count >= 0)
   {
      // haleyjd 02/08/05: this is NOT gonna be very fast.
      a = temp_fg2rgb[*source] + temp_bg2rgb[*dest];
      b = a;
      a |= 0x01f07c1f;
      b &= 0x40100400;
      a &= 0x3fffffff;
      b  = b - (b >> 5);
      a |= b;
      *dest = RGB32k[0][0][a & (a >> 15)];

      a = temp_fg2rgb[source[1]] + temp_bg2rgb[dest[1]];
      b = a;
      a |= 0x01f07c1f;
      b &= 0x40100400;
      a &= 0x3fffffff;
      b  = b - (b >> 5);
      a |= b;
      dest[1] = RGB32k[0][0][a & (a >> 15)];

      a = temp_fg2rgb[source[2]] + temp_bg2rgb[dest[2]];
      b = a;
      a |= 0x01f07c1f;
      b &= 0x40100400;
      a &= 0x3fffffff;
      b  = b - (b >> 5);
      a |= b;
      dest[2] = RGB32k[0][0][a & (a >> 15)];

      a = temp_fg2rgb[source[3]] + temp_bg2rgb[dest[3]];
      b = a;
      a |= 0x01f07c1f;
      b &= 0x40100400;
      a &= 0x3fffffff;
      b  = b - (b >> 5);
      a |= b;
      dest[3] = RGB32k[0][0][a & (a >> 15)];

      source += 4;
      dest += linesize;
   }
}

static void (*R_FlushQuadColumn)(void) = R_QuadFlushError;

static void R_FlushColumns(void)
{
   if(temp_x != 4 || commontop >= commonbot || temptype == COL_FUZZ)
      R_FlushWholeColumns();
   else
   {
      R_FlushHTColumns();
      R_FlushQuadColumn();
   }
   temp_x = 0;
}

//
// R_QResetColumnBuffer
//
// haleyjd 09/13/04: new function to call from main rendering loop
// which gets rid of the unnecessary reset of various variables during
// column drawing.
//
static void R_QResetColumnBuffer(void)
{
   // haleyjd 10/06/05: this must not be done if temp_x == 0!
   if(temp_x)
      R_FlushColumns();
   temptype = COL_NONE;
   R_FlushWholeColumns = R_FlushWholeError;
   R_FlushHTColumns    = R_FlushHTError;
   R_FlushQuadColumn   = R_QuadFlushError;
}

// haleyjd 09/12/04: split up R_GetBuffer into various different
// functions to minimize the number of branches and take advantage
// of as much precalculated information as possible.

static byte *R_GetBufferOpaque(void)
{
   // haleyjd: reordered predicates
   if(temp_x == 4 ||
      (temp_x && (temptype != COL_OPAQUE || temp_x + startx != column.x)))
      R_FlushColumns();

   if(!temp_x)
   {
      ++temp_x;
      startx = column.x;
      *tempyl = commontop = column.y1;
      *tempyh = commonbot = column.y2;
      temptype = COL_OPAQUE;
      R_FlushWholeColumns = R_FlushWholeOpaque;
      R_FlushHTColumns    = R_FlushHTOpaque;
      R_FlushQuadColumn   = R_FlushQuadOpaque;
      return tempbuf + (column.y1 << 2);
   }

   tempyl[temp_x] = column.y1;
   tempyh[temp_x] = column.y2;
   
   if(column.y1 > commontop)
      commontop = column.y1;
   if(column.y2 < commonbot)
      commonbot = column.y2;
      
   return tempbuf + (column.y1 << 2) + temp_x++;
}

static byte *R_GetBufferTrans(void)
{
   // haleyjd: reordered predicates
   if(temp_x == 4 || tranmap != temptranmap ||
      (temp_x && (temptype != COL_TRANS || temp_x + startx != column.x)))
      R_FlushColumns();

   if(!temp_x)
   {
      ++temp_x;
      startx = column.x;
      *tempyl = commontop = column.y1;
      *tempyh = commonbot = column.y2;
      temptype = COL_TRANS;
      temptranmap = tranmap;
      R_FlushWholeColumns = R_FlushWholeTL;
      R_FlushHTColumns    = R_FlushHTTL;
      R_FlushQuadColumn   = R_FlushQuadTL;
      return tempbuf + (column.y1 << 2);
   }

   tempyl[temp_x] = column.y1;
   tempyh[temp_x] = column.y2;
   
   if(column.y1 > commontop)
      commontop = column.y1;
   if(column.y2 < commonbot)
      commonbot = column.y2;
      
   return tempbuf + (column.y1 << 2) + temp_x++;
}

static byte *R_GetBufferFlexTrans(void)
{
   // haleyjd: reordered predicates
   if(temp_x == 4 || temptranslevel != column.translevel ||
      (temp_x && (temptype != COL_FLEXTRANS || temp_x + startx != column.x)))
      R_FlushColumns();

   if(!temp_x)
   {
      ++temp_x;
      startx = column.x;
      *tempyl = commontop = column.y1;
      *tempyh = commonbot = column.y2;
      temptype = COL_FLEXTRANS;
      temptranslevel = column.translevel;
      
      // haleyjd 09/12/04: optimization -- calculate flex tran lookups
      // here instead of every time a column is flushed.
      {
         unsigned int fglevel, bglevel;
         
         fglevel = temptranslevel & ~0x3ff;
         bglevel = FRACUNIT - fglevel;
         temp_fg2rgb  = Col2RGB8[fglevel >> 10];
         temp_bg2rgb  = Col2RGB8[bglevel >> 10];
      }

      R_FlushWholeColumns = R_FlushWholeFlex;
      R_FlushHTColumns    = R_FlushHTFlex;
      R_FlushQuadColumn   = R_FlushQuadFlex;
      return tempbuf + (column.y1 << 2);
   }

   tempyl[temp_x] = column.y1;
   tempyh[temp_x] = column.y2;
   
   if(column.y1 > commontop)
      commontop = column.y1;
   if(column.y2 < commonbot)
      commonbot = column.y2;
      
   return tempbuf + (column.y1 << 2) + temp_x++;
}

static byte *R_GetBufferFlexAdd(void)
{
   // haleyjd: reordered predicates
   if(temp_x == 4 || temptranslevel != column.translevel ||
      (temp_x && (temptype != COL_FLEXADD || temp_x + startx != column.x)))
      R_FlushColumns();

   if(!temp_x)
   {
      ++temp_x;
      startx = column.x;
      *tempyl = commontop = column.y1;
      *tempyh = commonbot = column.y2;
      temptype = COL_FLEXADD;
      temptranslevel = column.translevel;
      
      {
         unsigned int fglevel, bglevel;
         
         fglevel = temptranslevel & ~0x3ff;
         bglevel = FRACUNIT;
         temp_fg2rgb  = Col2RGB8_LessPrecision[fglevel >> 10];
         temp_bg2rgb  = Col2RGB8_LessPrecision[bglevel >> 10];
      }

      R_FlushWholeColumns = R_FlushWholeFlexAdd;
      R_FlushHTColumns    = R_FlushHTFlexAdd;
      R_FlushQuadColumn   = R_FlushQuadFlexAdd;
      return tempbuf + (column.y1 << 2);
   }

   tempyl[temp_x] = column.y1;
   tempyh[temp_x] = column.y2;
   
   if(column.y1 > commontop)
      commontop = column.y1;
   if(column.y2 < commonbot)
      commonbot = column.y2;
      
   return tempbuf + (column.y1 << 2) + temp_x++;
}

static byte *R_GetBufferFuzz(void)
{
   // haleyjd: reordered predicates
   if(temp_x == 4 ||
      (temp_x && (temptype != COL_FUZZ || temp_x + startx != column.x)))
      R_FlushColumns();

   if(!temp_x)
   {
      ++temp_x;
      startx = column.x;
      *tempyl = commontop = column.y1;
      *tempyh = commonbot = column.y2;
      temptype = COL_FUZZ;
      tempfuzzmap = column.colormap; // SoM 7-28-04: Fix the fuzz problem.
      R_FlushWholeColumns = R_FlushWholeFuzz;
      R_FlushHTColumns    = R_FlushHTError;
      R_FlushQuadColumn   = R_QuadFlushError;
      return tempbuf + (column.y1 << 2);
   }

   tempyl[temp_x] = column.y1;
   tempyh[temp_x] = column.y2;
   
   if(column.y1 > commontop)
      commontop = column.y1;
   if(column.y2 < commonbot)
      commonbot = column.y2;
      
   return tempbuf + (column.y1 << 2) + temp_x++;
}

static void R_QDrawColumn(void) 
{ 
   int              count; 
   register byte    *dest;            // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;     

   count = column.y2 - column.y1 + 1; 

   if(count <= 0)    // Zero length, column does not exceed a pixel.
      return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height) 
      I_Error("R_QDrawColumn: %i to %i at %i", column.y1, column.y2, column.x);    
#endif 

   // Framebuffer destination address.
   // Use ylookup LUT to avoid multiply with ScreenWidth.
   // Use columnofs LUT for subwindows? 

   // SoM: MAGIC
   dest = R_GetBufferOpaque();

   // Determine scaling, which is the only mapping to be done.

   fracstep = column.step;    
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   // Inner loop that does the actual texture mapping,
   //  e.g. a DDA-lile scaling.
   // This is as fast as it gets.       (Yeah, right!!! -- killough)
   //
   // killough 2/1/98: more performance tuning

   {
      register const byte *source = column.source;            
      register const lighttable_t *colormap = column.colormap; 
      register unsigned heightmask = column.texheight-1;
      if(column.texheight & heightmask)   // not a power of 2 -- killough
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
            // Re-map color indices from wall texture column
            //  using a lighting/special effects LUT.
            
            // heightmask is the Tutti-Frutti fix -- killough
            
            *dest = colormap[source[frac>>FRACBITS]];
            dest += 4; //SoM: Oh, Oh it's MAGIC! You know...
            if((frac += fracstep) >= (int)heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0)   // texture height is a power of 2 -- killough
         {
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += 4; //SoM: MAGIC 
            frac += fracstep;
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += 4;
            frac += fracstep;
         }
         if(count & 1)
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
      }
   }
} 

static void R_QDrawTLColumn(void)                                           
{ 
   int              count; 
   register byte    *dest;           // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   
   count = column.y2 - column.y1 + 1; 
   
   // Zero length, column does not exceed a pixel.
   if(count <= 0)
      return; 
                                 
#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height) 
      I_Error("R_QDrawTLColumn: %i to %i at %i", column.y1, column.y2, column.x);    
#endif 
   
   // SoM: MAGIC
   dest = R_GetBufferTrans();
      
   fracstep = column.step; 
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);
     
   {
      register const byte *source = column.source;            
      register const lighttable_t *colormap = column.colormap; 
      register unsigned heightmask = column.texheight-1;
      if(column.texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if(frac < 0)
         {
            while((frac += heightmask) <  0);
         }
         else
         {
            while (frac >= (int)heightmask)
               frac -= heightmask;
         }
          
         do
         {
            *dest = colormap[source[frac>>FRACBITS]];
            dest += 4; //SoM: Oh, Oh it's MAGIC! You know...
            if((frac += fracstep) >= (int)heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += 4; //SoM: MAGIC 
            frac += fracstep;
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += 4;
            frac += fracstep;
         }
         if(count & 1)
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
      }
   }
} 


#define SRCPIXEL \
   colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]

static void R_QDrawTLTRColumn(void)
{ 
   int              count; 
   register byte    *dest;           // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   
   count = column.y2 - column.y1 + 1; 
   
   // Zero length, column does not exceed a pixel.
   if(count <= 0)
      return; 
                                 
#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height) 
      I_Error("R_QDrawTLTRColumn: %i to %i at %i", column.y1, column.y2, column.x);    
#endif 

   // SoM: MAGIC
   dest = R_GetBufferTrans();
   
   fracstep = column.step; 
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

  
   {
      register const byte *source = column.source;            
      register const lighttable_t *colormap = column.colormap; 
      register unsigned heightmask = column.texheight-1;
      if(column.texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= (int)heightmask)
               frac -= heightmask;
          
         do
         {
            *dest = colormap[column.translation[source[frac>>FRACBITS]]];
            dest += 4; //SoM: Oh, Oh it's MAGIC! You know...
            if((frac += fracstep) >= (int)heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = SRCPIXEL;
            dest += 4; //SoM: MAGIC 
            frac += fracstep;
            *dest = SRCPIXEL;
            dest += 4;
            frac += fracstep;
         }
         if(count & 1)
            *dest = SRCPIXEL;
      }
   }
} 

#undef SRCPIXEL

//
// Spectre/Invisibility.
//

static void R_QDrawFuzzColumn(void) 
{ 
   // Adjust borders. Low...
   if(!column.y1) 
      column.y1 = 1;
   
   // .. and high.
   if(column.y2 == viewheight - 1) 
      column.y2 = viewheight - 2; 
   
   // Zero length?
   if((column.y2 - column.y1) < 0) 
      return; 
    
#ifdef RANGECHECK 
   // haleyjd: these should apparently be adjusted for hires
   // SoM: DONE
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("R_QDrawFuzzColumn: %i to %i at %i", column.y1, column.y2, column.x);
#endif

   // SoM: MAGIC
   R_GetBufferFuzz();
   
   // REAL MAGIC... you ready for this?
   return; // DONE
}

#define SRCPIXEL \
   colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]

static void R_QDrawTRColumn(void) 
{ 
   int      count; 
   byte     *dest; 
   fixed_t  frac;
   fixed_t  fracstep;     
   
   count = column.y2 - column.y1; 
   if(count < 0) 
      return; 
                                 
#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("R_QDrawTRColumn: %i to %i at %i", column.y1, column.y2, column.x);
#endif 

   // SoM: MAGIC
   dest = R_GetBufferOpaque();
   
   // Looks familiar.
   fracstep = column.step; 
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   count++;        // killough 1/99: minor tuning

   // Here we do an additional index re-mapping.
   {
      register const byte *source = column.source;            
      register const lighttable_t *colormap = column.colormap; 
      register unsigned heightmask = column.texheight-1;
      if(column.texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= (int)heightmask)
               frac -= heightmask;
          
         do
         {
            *dest = colormap[column.translation[source[frac>>FRACBITS]]];
            dest += 4; //SoM: Oh, Oh it's MAGIC! You know...
            if((frac += fracstep) >= (int)heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = SRCPIXEL;
            dest += 4; //SoM: MAGIC 
            frac += fracstep;
            *dest = SRCPIXEL;
            dest += 4;
            frac += fracstep;
         }
         if(count & 1)
            *dest = SRCPIXEL;
      }
   }
} 

#undef SRCPIXEL

//
// R_DrawFlexTLColumn
//
// haleyjd 09/01/02: zdoom-style translucency
//
static void R_QDrawFlexColumn(void)
{ 
   int              count; 
   register byte    *dest;           // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   
   count = column.y2 - column.y1 + 1; 

   // Zero length, column does not exceed a pixel.
   if(count <= 0)
      return; 
                                 
#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("R_QDrawFlexColumn: %i to %i at %i", column.y1, column.y2, column.x);
#endif 
   
   // SoM: MAGIC
   dest = R_GetBufferFlexTrans();
  
   fracstep = column.step; 
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register const byte *source = column.source;            
      register const lighttable_t *colormap = column.colormap; 
      register unsigned heightmask = column.texheight-1;
      if(column.texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= (int)heightmask)
               frac -= heightmask;
          
         do
         {
            *dest = colormap[source[frac>>FRACBITS]];
            dest += 4; //SoM: Oh, Oh it's MAGIC! You know...
            if((frac += fracstep) >= (int)heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += 4; //SoM: MAGIC 
            frac += fracstep;
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += 4;
            frac += fracstep;
         }
         if(count & 1)
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
      }
   }
}

#define SRCPIXEL \
   colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]

//
// R_DrawFlexTlatedColumn
//
// haleyjd 11/05/02: zdoom-style translucency w/translation, for
// player sprites
//
static void R_QDrawFlexTRColumn(void) 
{ 
   int      count; 
   byte     *dest; 
   fixed_t  frac;
   fixed_t  fracstep;     
   
   count = column.y2 - column.y1; 
   if(count < 0) 
      return; 
   
#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("R_QDrawFlexTRColumn: %i to %i at %i", column.y1, column.y2, column.x);
#endif 

   // MAGIC
   dest = R_GetBufferFlexTrans();
   
   // Looks familiar.
   fracstep = column.step; 
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);
   
   count++;        // killough 1/99: minor tuning
   
   // Here we do an additional index re-mapping.
   {
      register const byte *source = column.source;            
      register const lighttable_t *colormap = column.colormap; 
      register unsigned heightmask = column.texheight-1;
      if(column.texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= (int)heightmask)
               frac -= heightmask;
          
         do
         {
            *dest = colormap[column.translation[source[frac>>FRACBITS]]];
            dest += 4; //SoM: Oh, Oh it's MAGIC! You know...
            if((frac += fracstep) >= (int)heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = SRCPIXEL;
            dest += 4; //SoM: MAGIC 
            frac += fracstep;
            *dest = SRCPIXEL;
            dest += 4;
            frac += fracstep;
         }
         if(count & 1)
            *dest = SRCPIXEL;
      }
   }
} 

#undef SRCPIXEL

//
// R_DrawAddColumn
//
// haleyjd 02/08/05: additive translucency
//
static void R_QDrawAddColumn(void)
{ 
   int              count; 
   register byte    *dest;           // killough
   register fixed_t frac;            // killough
   fixed_t          fracstep;
   
   count = column.y2 - column.y1 + 1; 

   // Zero length, column does not exceed a pixel.
   if(count <= 0)
      return; 
                                 
#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("R_QDrawAddColumn: %i to %i at %i", column.y1, column.y2, column.x);
#endif 
   
   // SoM: MAGIC
   dest = R_GetBufferFlexAdd();
  
   fracstep = column.step; 
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register const byte *source = column.source;            
      register const lighttable_t *colormap = column.colormap; 
      register unsigned heightmask = column.texheight-1;
      if(column.texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= (int)heightmask)
               frac -= heightmask;
          
         do
         {            
            *dest = colormap[source[frac>>FRACBITS]];
            dest += 4; //SoM: Oh, Oh it's MAGIC! You know...
            if((frac += fracstep) >= (int)heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += 4; //SoM: MAGIC 
            frac += fracstep;
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += 4;
            frac += fracstep;
         }
         if(count & 1)
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
      }
   }
}

#define SRCPIXEL \
   colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]

//
// R_DrawAddTlatedColumn
//
// haleyjd 02/08/05: additive translucency + translation
//
static void R_QDrawAddTRColumn(void) 
{ 
   int      count; 
   byte     *dest; 
   fixed_t  frac;
   fixed_t  fracstep;     
   
   count = column.y2 - column.y1; 
   if (count < 0) 
      return; 
   
#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("R_QDrawAddTRColumn: %i to %i at %i", column.y1, column.y2, column.x);
#endif 

   // MAGIC
   dest = R_GetBufferFlexAdd();
   
   // Looks familiar.
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);
   
   count++;        // killough 1/99: minor tuning
   
   // Here we do an additional index re-mapping.
   {
      register const byte *source = column.source;            
      register const lighttable_t *colormap = column.colormap; 
      register unsigned heightmask = column.texheight-1;
      if(column.texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= (int)heightmask)
               frac -= heightmask;
          
         do
         {
            *dest = colormap[column.translation[source[frac>>FRACBITS]]];
            dest += 4; //SoM: Oh, Oh it's MAGIC! You know...
            if((frac += fracstep) >= (int)heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = SRCPIXEL;
            dest += 4; //SoM: MAGIC 
            frac += fracstep;
            *dest = SRCPIXEL;
            dest += 4;
            frac += fracstep;
         }
         if(count & 1)
            *dest = SRCPIXEL;
      }
   }
} 

#undef SRCPIXEL

//
// haleyjd 09/04/06: Quad Column Drawer Object
//
columndrawer_t r_quad_drawer =
{
   R_QDrawColumn,
   R_QDrawTLColumn,
   R_QDrawTRColumn,
   R_QDrawTLTRColumn,
   R_QDrawFuzzColumn,
   R_QDrawFlexColumn,
   R_QDrawFlexTRColumn,
   R_QDrawAddColumn,
   R_QDrawAddTRColumn,

   R_QResetColumnBuffer
};

// EOF

