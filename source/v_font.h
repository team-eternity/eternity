// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2004 James Haley
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
// Unified Font Engine
//
// haleyjd 01/14/05
//
//----------------------------------------------------------------------------

#ifndef V_FONT_H__
#define V_FONT_H__

#include "doomtype.h"
#include "r_defs.h"
#include "m_dllist.h"

enum
{
   VFONT_SMALL,
   VFONT_HUD,
   VFONT_BIG,
   VFONT_BIG_NUM,
};

typedef struct vfontfilter_s
{
   unsigned int start;    // first character
   unsigned int end;      // last character
   unsigned int *chars;   // chars array
   unsigned int numchars; // number of chars
   const char   *mask;    // loading string

} vfontfilter_t;

//
// vfont structure
//
// Contains all the data necessary to allow generalized font drawing.
//
struct vfont_t
{
   DLListItem<vfont_t> numlinks; // for EDF hashing

   unsigned int start; // first character in font
   unsigned int end;   // last character in font
   unsigned int size;  // number of characters in font

   int cy;    // step amount for \n
   int space; // step for blank space
   int dw;    // width delta (can move characters together)
   int absh;  // absolute maximum height of any character

   boolean color;    // supports color translations?
   boolean upper;    // uses uppercase only?
   boolean centered; // characters are centered in position?

   patch_t **fontgfx; // graphics patches for font (not owned)

   int cw;  // constant width, used only when centering is on
   
   boolean linear;  // linear graphic lump?
   byte    *data;   // data for linear graphic
   int     lsize;   // character size in linear graphic

   int  num;                 // numeric id
   char name[33];            // EDF mnemonic
   vfont_t *namenext;        // next by name
   vfontfilter_t *filters;   // graphic loading filters
   unsigned int numfilters;  // number of filters
   int patchnumoffset;       // used during font loading only
   char lumpname[9];         // [CG] Used to reload linear fonts.
   int linear_font_format;   // [CG] Used to reload linear fonts.
};

void  V_FontWriteText(vfont_t *font, const char *s, int x, int y);
void  V_FontWriteTextColored(vfont_t *font, const char *s, int color, int x, int y);
void  V_FontWriteTextMapped(vfont_t *font, const char *s, int x, int y, char *map);
void  V_FontWriteTextShadowed(vfont_t *font, const char *s, int x, int y);
int   V_FontStringHeight(vfont_t *font, const char *s);
int   V_FontStringWidth(vfont_t *font, const char *s);
int   V_FontCharWidth(vfont_t *font, char pChar);
void  V_FontSetAbsCentered(void);
short V_FontMaxWidth(vfont_t *font);

#endif

// EOF

