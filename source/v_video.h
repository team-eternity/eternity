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
//  Gamma correction LUT.
//  Color range translation support
//  Functions to draw patches (by post) directly to screen.
//  Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------

#ifndef V_VIDEO_H__
#define V_VIDEO_H__

#include "doomtype.h"
#include "doomdef.h"
#include "v_patch.h"
// Needed because we are refering to patches.
#include "r_data.h"
#include "v_buffer.h"

//
// VIDEO
//

//jff 2/16/98 palette color ranges for translation
//jff 2/18/98 conversion to palette lookups for speed
//jff 4/24/98 now pointers to lumps loaded 
extern byte *cr_brick;
extern byte *cr_tan;
extern byte *cr_gray;
extern byte *cr_green;
extern byte *cr_brown;
extern byte *cr_gold;
extern byte *cr_red;
extern byte *cr_blue;
extern byte *cr_blue_status; //killough 2/28/98
extern byte *cr_orange;
extern byte *cr_yellow;

// symbolic indices into color translation table pointer array
enum crange_idx_e
{
   CR_BRICK,   //0
   CR_TAN,     //1
   CR_GRAY,    //2
   CR_GREEN,   //3
   CR_BROWN,   //4
   CR_GOLD,    //5
   CR_RED,     //6
   CR_BLUE,    //7
   CR_ORANGE,  //8
   CR_YELLOW,  //9

   CR_BUILTIN = CR_YELLOW,

   // haleyjd: "custom" colors do not have built-in translations;
   // they are free for user definition via EDF.
   CR_CUSTOM1, //10
   CR_CUSTOM2, //11
   CR_CUSTOM3, //12
   CR_CUSTOM4, //13

   CR_MAXCUSTOM = CR_CUSTOM4,

   CR_LIMIT    //jff 2/27/98 added for range check
};
//jff 1/16/98 end palette color range additions

// array of pointers to color translation tables
extern byte *colrngs[CR_LIMIT];

#define CR_DEFAULT CR_RED   /* default value for out of range colors */

extern byte gammatable[5][256];
extern int  usegamma;

// ----------------------------------------------------------------------------
// haleyjd: DOSDoom-style translucency lookup tables

extern bool flexTranInit;
extern unsigned int Col2RGB8[65][256];
extern unsigned int *Col2RGB8_LessPrecision[65];
extern byte RGB32k[32][32][32];


// ----------------------------------------------------------------------------
// Initalization

// V_InitColorTranslation (jff 4/24/98)
// Loads color translation lumps
void V_InitColorTranslation(void);

// Initialises or updates the modern HUD's subscreen
void V_InitSubScreenModernHUD();

// V_Init
// Allocates buffer screens and sets up the VBuffers for the screen surfaces.
void V_Init (void);

// V_InitFlexTranTable
// Initializes the tables used in Flex translucency calculations, given the
// data in the given palette.
void V_InitFlexTranTable(const byte *palette);

// ----------------------------------------------------------------------------
// Screen patch, block, and pixel related functions

// V_CopyRect
// Copies an area from one VBuffer to another. If the destination VBuffer has
// scaling enabled, the coordinates of the rectangle will all be scaled
// according to the destination scaling information, but the actual pixels are
// copied directly.
void V_CopyRect(int srcx,  int srcy,  VBuffer *src, int width, int height,
		          int destx, int desty, VBuffer *dest);

// V_DrawPatchGeneral (killough 11/98)
// Consolidated V_DrawPatch and V_DrawPatchFlipped. This function renders a
// patch to a VBuffer object utilizing any scaling information the buffer has.
void V_DrawPatchGeneral(int x, int y, VBuffer *buffer, patch_t *patch, 
                        bool flipped);

// V_DrawPatch
// Macro-ized version of V_DrawPatchGeneral
#define V_DrawPatch(x,y,s,p)        V_DrawPatchGeneral(x,y,s,p,false)

// V_DrawPatchFlipped
// Macro-ized version of V_DrawPatchGeneral
#define V_DrawPatchFlipped(x,y,s,p) V_DrawPatchGeneral(x,y,s,p,true)


// V_DrawPatchTranslated
// Renders a patch to the given VBuffer like V_DrawPatchGeneral, but applies
// The given color translation to the patch.
void V_DrawPatchTranslated(int x, int y, VBuffer *buffer, patch_t *patch, 
                           byte *outr, bool flipped);

// V_DrawPatchTranslatedLit
// As above, but with an additional lighttable remapping.
void V_DrawPatchTranslatedLit(int x, int y, VBuffer *buffer, patch_t *patch,
                              byte *outr, byte *lighttable, bool flipped);

// V_DrawPatchTL
// Renders a patch to the given VBuffer like V_DrawPatchGeneral, but renders
// the patch with the given opacity. If a color translation table is supplied
// (outr != nullptr) the patch is translated as well.
void V_DrawPatchTL(int x, int y, VBuffer *buffer, patch_t *patch, 
                   byte *outr, int tl);

// V_DrawPatchAdd
// Renders a patch to the given VBuffer like V_DrawPatchGeneral, but renders
// the patch with additive blending of the given amount. If a color translation
// table is supplied (outr != nullptr) the patch is translated as well.
void V_DrawPatchAdd(int x, int y, VBuffer *buffer, patch_t *patch,
                    byte *outr, int tl);

// V_DrawPatchShadowed
// Renders a patch to the given VBuffer like V_DrawPatchGeneral, but renders
// the patch twice, first 2 units to the left and down using colormap 33, and
// then again at the specified location with the normal parameters.
void V_DrawPatchShadowed(int x, int y, VBuffer *buffer, patch_t *patch,
                         byte *outr, int tl);


// V_DrawBlock
// Draw a linear block of pixels into the view buffer, using the buffer's
// scaling information (if present)
void V_DrawBlock(int x, int y, VBuffer *buffer, int width, int height, 
                 const byte *src);

// V_DrawMaskedBlockTR
// Draw a translated, masked linear block of pixels into a view buffer, using
//  the buffer's scaling information (if present)
void V_DrawMaskedBlockTR(int x, int y, VBuffer *buffer, int width, int height,
                         int srcpitch, const byte *src, byte *cmap);

// haleyjd 05/18/09: Fullscreen background drawing helpers
void V_DrawBlockFS(VBuffer *buffer, const byte *src);
void V_DrawPatchFS(VBuffer *buffer, patch_t *patch);
void V_DrawFSBackground(VBuffer *dest, int lumpnum);

// V_FindBestColor (haleyjd)
// A function that requantizes a color into the default game palette
byte V_FindBestColor(const byte *palette, int r, int g, int b);


// V_CacheBlock
// Copies a block of pixels from the source linear buffer into the destination
// linear buffer.
void V_CacheBlock(int x, int y, int width, int height, byte *src, byte *bdest);

extern VBuffer vbscreen;
extern VBuffer backscreen1;
extern VBuffer backscreen2;
extern VBuffer backscreen3;
extern VBuffer subscreen43;
extern VBuffer vbscreenyscaled;
extern VBuffer vbscreenfullres;
extern VBuffer vbscreenmodernhud;

#endif

//----------------------------------------------------------------------------
//
// $Log: v_video.h,v $
// Revision 1.9  1998/05/06  11:12:54  jim
// Formattted v_video.*
//
// Revision 1.8  1998/05/03  22:53:58  killough
// beautification
//
// Revision 1.7  1998/04/24  08:09:44  jim
// Make text translate tables lumps
//
// Revision 1.6  1998/03/02  11:43:06  killough
// Add cr_blue_status for blue statusbar numbers
//
// Revision 1.5  1998/02/27  19:22:11  jim
// Range checked hud/sound card variables
//
// Revision 1.4  1998/02/19  16:55:06  jim
// Optimized HUD and made more configurable
//
// Revision 1.3  1998/02/17  23:00:41  jim
// Added color translation machinery and data
//
// Revision 1.2  1998/01/26  19:27:59  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:05  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

