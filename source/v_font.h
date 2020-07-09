// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
#include "v_video.h"  // required for CR_LIMIT

struct patch_t;
class  qstring;
struct VBuffer;

enum
{
   VFONT_SMALL,
   VFONT_HUD,
   VFONT_BIG,
   VFONT_BIG_NUM,
};

struct vfontfilter_t
{
   unsigned int start;    // first character
   unsigned int end;      // last character
   unsigned int *chars;   // chars array
   unsigned int numchars; // number of chars
   const char   *mask;    // loading string

};

//
// Linear lump wad reload data
//
struct vfont_linearindex_t
{
   size_t nameIndex; // index of the lump name in the collection
   int format;       // fmt
   bool requantize;  // requantize flag
};

//
// vfont structure
//
// Contains all the data necessary to allow generalized font drawing.
//
struct vfont_t
{
   DLListItem<vfont_t> numlinks;  // for EDF hashing by number
   DLListItem<vfont_t> namelinks; // for EDF hashing by name

   unsigned int start; // first character in font
   unsigned int end;   // last character in font
   unsigned int size;  // number of characters in font

   int   cy;           // step amount for \n
   int   space;        // step for blank space
   int   dw;           // width delta (can move characters together)
   int   absh;         // absolute maximum height of any character

   bool  color;        // supports color translations?
   bool  upper;        // uses uppercase only?
   bool  centered;     // characters are centered in position?

   patch_t **fontgfx;  // graphics patches for font (not owned)

   int   cw;           // constant width, used only when centering is on
   
   bool  linear;       // linear graphic lump?
   byte *data;         // data for linear graphic
   int   lsize;        // character size in linear graphic

   int   colorDefault;       // default font color
   int   colorNormal;        // normal font color
   int   colorHigh;          // highlighted font color
   int   colorError;         // error font color
   byte *colrngs[CR_LIMIT];  // color translation tables

   int   num;                // numeric id
   char *name;               // EDF mnemonic
   vfontfilter_t *filters;   // graphic loading filters
   unsigned int numfilters;  // number of filters
   int patchnumoffset;       // used during font loading only

   vfont_linearindex_t linearreload; // data useful to reload linear font data
};

enum vtextflags_e
{
   VTXT_NORMAL     = 0,
   VTXT_FIXEDCOLOR = 0x01,
   VTXT_SHADOW     = 0x02,
   VTXT_ABSCENTER  = 0x04
};

struct vtextdraw_t
{
   vfont_t     *font;
   const char  *s;
   int          x;
   int          y;
   VBuffer     *screen;
   int          fixedColNum;
   char        *altMap;
   unsigned int flags;
};

void    V_FontWriteTextEx(const vtextdraw_t &textdraw);
void    V_FontWriteText(vfont_t *font, const char *s, int x, int y, VBuffer *screen = nullptr);
void    V_FontWriteTextColored(vfont_t *font, const char *s, int color, int x, int y,
                               VBuffer *screen = nullptr);
void    V_FontWriteTextMapped(vfont_t *font, const char *s, int x, int y, char *map,
                              VBuffer *screen = nullptr);
void    V_FontWriteTextShadowed(vfont_t *font, const char *s, int x, int y, 
                                VBuffer *screen = nullptr, int color = -1);
int     V_FontStringHeight(vfont_t *font, const char *s);
int     V_FontStringWidth(vfont_t *font, const char *s);
int     V_FontCharWidth(vfont_t *font, char pChar);
int16_t V_FontMaxWidth(vfont_t *font);

void V_FontFitTextToRect(vfont_t *font, char *msg, int x1, int y1, int x2, int y2);
void V_FontFitTextToRect(vfont_t *font, qstring &msg, int x1, int y1, int x2, int y2);

byte *V_FontGetUsedColors(vfont_t *font);

#endif

// EOF

