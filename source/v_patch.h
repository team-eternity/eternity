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

#ifndef V_PATCH_H__
#define V_PATCH_H__

#include "r_data.h"
#include "doomtype.h"

struct patch_t;

struct PatchInfo
{
   patch_t *patch;
   int x, y;        // screen coordinates
   bool flipped;    // flipped?
   int drawstyle;   // drawing style (normal, tr, tl, trtl, etc.)
};


// VBuffer moved
#include "v_buffer.h"

enum
{
   PSTYLE_NORMAL,
   PSTYLE_TLATED,
   PSTYLE_TRANSLUC,
   PSTYLE_TLTRANSLUC,
   PSTYLE_ADD,
   PSTYLE_TLADD,
   PSTYLE_TLATEDLIT,
   PSTYLE_NUMSTYLES
};

void V_SetPatchColrng(byte *colrng);
void V_SetPatchLight(byte *lighttable);
void V_SetPatchTL(unsigned int *fg, unsigned int *bg);
void V_DrawPatchInt(PatchInfo *pi, VBuffer *buffer);

enum
{
   DRAWTYPE_UNSCALED,
   DRAWTYPE_GENSCALED
};

void V_SetupBufferFuncs(VBuffer *buffer, int drawtype);
void V_InitUnscaledBuffer(VBuffer *vbuf, byte *data);

// SoM: In my continual effort to weed out multiple column drawers I discovered
// the new patch system is derived from the old screen sprite code. I've 
// cleaned it up a bit.

struct cb_patch_column_t  // It's cardboard now, bitches!
{
   int x;
   int y1, y2;

   fixed_t frac; 
   fixed_t step;

   byte   *source;
   byte   *translation;
   byte   *light;        // haleyjd 01/22/12: lighting

   VBuffer *buffer;

   void (*colfunc)(void);

   // haleyjd: translucency lookups
   unsigned int *fg2rgb;
   unsigned int *bg2rgb;
}; 

// Conversion routines

byte *V_PatchToLinear(patch_t *patch, bool flipped, byte fillcolor,
                      int *width, int *height);

patch_t *V_LinearToPatch(byte *linear, int w, int h, size_t *memsize, 
                         int tag, void **user = NULL);

patch_t *V_LinearToTransPatch(const byte *linear, int w, int h, size_t *memsize,
                              int color_key, int tag, void **user = NULL);

bool V_WritePatchAsPNG(const char *lump, const char *filename, byte fillcolor);

#endif

// EOF

