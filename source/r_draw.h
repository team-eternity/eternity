// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//      System specific interface stuff.
//
//-----------------------------------------------------------------------------

#ifndef __R_DRAW__
#define __R_DRAW__

#include "r_defs.h"

// haleyjd 01/22/11: vissprite drawstyles
enum
{
   VS_DRAWSTYLE_NORMAL,  // Normal
   VS_DRAWSTYLE_SHADOW,  // Spectre draw
   VS_DRAWSTYLE_ALPHA,   // Flex translucent
   VS_DRAWSTYLE_ADD,     // Additive flex translucent
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
   void (*DrawColumn)(void);       // normal
   void (*DrawTLColumn)(void);     // translucent
   void (*DrawTRColumn)(void);     // translated
   void (*DrawTLTRColumn)(void);   // translucent/translated
   void (*DrawFuzzColumn)(void);   // spectre fuzz
   void (*DrawFlexColumn)(void);   // flex translucent
   void (*DrawFlexTRColumn)(void); // flex translucent/translated
   void (*DrawAddColumn)(void);    // additive flextran
   void (*DrawAddTRColumn)(void);  // additive flextran/translated

   void (*ResetBuffer)(void);      // reset function (may be null)
   
   void (*ByVisSpriteStyle[VS_NUMSTYLES][2])(void);
};

extern columndrawer_t r_normal_drawer;

#define TRANSLATIONCOLOURS 15

extern int      linesize;        // killough 11/98

void R_VideoErase(unsigned int x, unsigned int y, unsigned int w, unsigned int h);

// start of a 64*64 tile image
extern byte **translationtables; // haleyjd 01/12/04: now ptr-to-ptr

// haleyjd 06/22/08: Span styles enumeration
enum
{
   SPAN_STYLE_NORMAL,
   SPAN_STYLE_TL,
   SPAN_STYLE_ADD,
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
   void (*DrawSpan [SPAN_NUMSTYLES][FLAT_NUMSIZES])(void);
   void (*DrawSlope[SPAN_NUMSTYLES][FLAT_NUMSIZES])(void);

   bool haslp64; // haleyjd: if true, 64x64 flat drawer is low-precision
};

extern spandrawer_t r_lpspandrawer;  // low-precision
extern spandrawer_t r_spandrawer;    // normal
extern spandrawer_t r_lowspandrawer; // low-detail

void R_InitBuffer(int width, int height);

// Initialize color translation tables, for player rendering etc.
void R_InitTranslationTables(void);

// haleyjd 09/13/09: translation num-for-name lookup function
int R_TranslationNumForName(const char *name);

// Rendering function.
void R_FillBackScreen(void);

// If the view size is not full screen, draws a border around it.
void R_DrawViewBorder(void);

extern byte *tranmap;         // translucency filter maps 256x256  // phares 
extern byte *main_tranmap;    // killough 4/11/98
extern byte *ylookup[];       // killough 11/98

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
   lighttable_t *colormap;
   byte *translation;
   fixed_t translevel; // haleyjd: zdoom style trans level

   void *source;
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
