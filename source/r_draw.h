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
//      System specific interface stuff.
//
//-----------------------------------------------------------------------------

#ifndef __R_DRAW__
#define __R_DRAW__

#include "r_defs.h"

// haleyjd 05/02/13
struct rrect_t
{
   int x;
   int y;
   int width;
   int height;

   void scaledFromScreenBlocks(int blocks);
   void viewFromScaled(int blocks, int vwidth, int vheight, 
                       const rrect_t &scaled);
};

extern rrect_t scaledwindow;
extern rrect_t viewwindow;

// haleyjd 01/22/11: vissprite drawstyles
enum
{
   VS_DRAWSTYLE_NORMAL,  // Normal
   VS_DRAWSTYLE_SHADOW,  // Spectre draw
   VS_DRAWSTYLE_ALPHA,   // Flex translucent
   VS_DRAWSTYLE_ADD,     // Additive flex translucent
   VS_DRAWSTYLE_SUB,     // Static SUBMAP translucent
   VS_DRAWSTYLE_TRANMAP, // Static TRANMAP translucent
   VS_NUMSTYLES
};

//
// columndrawer_t
//
// haleyjd 09/04/06: This structure is used to allow the game engine to use
// multiple sets of column drawing functions (ie., normal, low detail, and
// quad buffer optimized).
//
struct columndrawer_t
{
   void (*DrawColumn)();       // normal
   void (*DrawNewSkyColumn)(); // double-sky drawing (index 0 = transparent)
   void (*DrawTLColumn)();     // translucent
   void (*DrawTRColumn)();     // translated
   void (*DrawTLTRColumn)();   // translucent/translated
   void (*DrawFuzzColumn)();   // spectre fuzz
   void (*DrawFlexColumn)();   // flex translucent
   void (*DrawFlexTRColumn)(); // flex translucent/translated
   void (*DrawAddColumn)();    // additive flextran
   void (*DrawAddTRColumn)();  // additive flextran/translated

   void (*ResetBuffer)();      // reset function (may be null)
   
   void (*ByVisSpriteStyle[VS_NUMSTYLES][2])();
};

extern columndrawer_t r_normal_drawer;

#define TRANSLATIONCOLOURS 14

extern int   linesize;     // killough 11/98
extern byte *renderscreen; // haleyjd 07/02/14

void R_VideoErase(unsigned int x, unsigned int y, unsigned int w, unsigned int h);
void R_VideoEraseScaled(unsigned int x, unsigned int y, unsigned int w, unsigned int h);

// start of a 64*64 tile image
extern byte **translationtables; // haleyjd 01/12/04: now ptr-to-ptr

extern int rTintTableIndex;   // check if we have a TINTTAB lump in the directory

// haleyjd 06/22/08: Span styles enumeration
enum
{
   SPAN_STYLE_NORMAL,
   SPAN_STYLE_TL,
   SPAN_STYLE_ADD,
   SPAN_STYLE_NORMAL_MASKED,
   SPAN_STYLE_TL_MASKED,
   SPAN_STYLE_ADD_MASKED,
   SPAN_NUMSTYLES
};

// haleyjd 06/22/08: Flat sizes enumeration
enum
{
   FLAT_64,           // 64x64
   FLAT_128,          // 128x128
   FLAT_256,          // 256x256
   FLAT_512,          // 512x512
   FLAT_GENERALIZED,  // rectangular texture, power of two dims.
   FLAT_NUMSIZES
};

//
// spandrawer_t
//
// haleyjd 09/04/06: This structure is used to allow the game engine to use
// multiple sets of span drawing functions (ie, low detail, low precision,
// high precision, etc.)
//
struct spandrawer_t
{
   void (*DrawSpan [SPAN_NUMSTYLES][FLAT_NUMSIZES])();
   void (*DrawSlope[SPAN_NUMSTYLES][FLAT_NUMSIZES])();
};

extern spandrawer_t r_lpspandrawer;  // low-precision
extern spandrawer_t r_spandrawer;    // normal

void R_InitBuffer(int width, int height);

// Initialize color translation tables, for player rendering etc.
void R_InitTranslationTables();

// haleyjd 09/13/09: translation num-for-name lookup function
int R_TranslationNumForName(const char *name);

// haleyjd: 09/08/12: global identity translation map
byte *R_GetIdentityMap();

// Rendering function.
void R_FillBackScreen(const rrect_t &window);

// If the view size is not full screen, draws a border around it.
void R_DrawViewBorder();

extern byte  *tranmap;       // translucency filter maps 256x256  // phares 
extern byte  *main_tranmap;  // killough 4/11/98
extern byte  *main_submap;   // haleyjd 11/30/13

#define R_ADDRESS(px, py) \
   (renderscreen + (viewwindow.y + (py)) * linesize + (viewwindow.x + (px)))

#define FUZZTABLE 50 
#define FUZZOFF (SCREENWIDTH)

extern const int fuzzoffset[];
extern int fuzzpos;

// Cardboard
typedef struct cb_column_s
{
   int x, y1, y2;

   fixed_t step;
   int texheight;

   int texmid;

   // 8-bit lighting
   const lighttable_t *colormap;
   const byte *translation;
   fixed_t translevel; // haleyjd: zdoom style trans level

   const void *source;
} cb_column_t;


extern cb_column_t column;

#endif

//----------------------------------------------------------------------------
//
// $Log: r_draw.h,v $
// Revision 1.5  1998/05/03  22:42:23  killough
// beautification, extra declarations
//
// Revision 1.4  1998/04/12  01:58:11  killough
// Add main_tranmap
//
// Revision 1.3  1998/03/02  11:51:55  killough
// Add translucency declarations
//
// Revision 1.2  1998/01/26  19:27:38  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
