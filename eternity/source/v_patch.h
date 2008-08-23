// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
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
// DESCRIPTION:
//  Functions to draw patches (by post)
//
//-----------------------------------------------------------------------------

#ifndef V_PATCH_H__
#define V_PATCH_H__

#include "r_data.h"
#include "doomtype.h"

typedef struct PatchInfo_s
{
   patch_t *patch;
   int x, y;        // screen coordinates
   boolean flipped; // flipped?
   int drawstyle;   // drawing style (normal, tr, tl, trtl, etc.)
} PatchInfo;

typedef struct VBuffer_s
{
   byte *data; // video memory
   int width;
   int height;
   int pitch;

   void (*BlockDrawer)(int, int, struct VBuffer_s *, int, int, byte *);
   void (*MaskedBlockDrawer)(int, int, struct VBuffer_s *, int, int, int, 
                             byte *, byte *);
   void (*TileBlock64)(struct VBuffer_s *, byte *);

   int  *x1lookup;
   int  *y1lookup;
   int  *x2lookup;
   int  *y2lookup;
   fixed_t ixscale;
   fixed_t iyscale;
} VBuffer;

enum
{
   PSTYLE_NORMAL,
   PSTYLE_TLATED,
   PSTYLE_TRANSLUC,
   PSTYLE_TLTRANSLUC,
   PSTYLE_ADD,
   PSTYLE_TLADD,
   PSTYLE_NUMSTYLES
};

void V_SetPatchColrng(byte *colrng);
void V_SetPatchTL(unsigned int *fg, unsigned int *bg);

void V_DrawPatchInt(PatchInfo *pi, VBuffer *buffer);

enum
{
   DRAWTYPE_UNSCALED,
   DRAWTYPE_2XSCALED,
   DRAWTYPE_GENSCALED,
};

void V_SetupBufferFuncs(VBuffer *buffer, int drawtype);
void V_InitUnscaledBuffer(VBuffer *vbuf, byte *data);

// SoM: In my continual effort to weed out multiple column drawers I discovered
// the new patch system is derrived from the old screen sprite code. I've 
// cleaned it up a bit.
typedef struct
{
   int x;
   int y1, y2;

   fixed_t frac, step;

   byte *source, *translation;

   // SoM: Let's see if we can get rid of this...
   //fixed_t maxfrac;

   VBuffer *buffer;

   void (*colfunc)(void);

   // haleyjd: translucency lookups
   unsigned int *fg2rgb;
   unsigned int *bg2rgb;

} cb_patch_column_t; // It's cardboard now, bitches!


#endif

// EOF
