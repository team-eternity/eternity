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
//  Refresh module, data I/O, caching, retrieval of graphics
//  by name.
//
//-----------------------------------------------------------------------------

#ifndef __R_DATA__
#define __R_DATA__

#include "r_defs.h"
#include "r_state.h"

// haleyjd 08/30/02: externalized these structures

typedef enum
{
   TC_PATCH,
   TC_FLAT,
} cmptype_e;

// Generalized graphic
typedef struct tcomponent_s
{
   int32_t   originx, originy;  // Block origin, which has already accounted
   uint32_t  width, height;     // Unscaled dimensions of the graphic. 
   
   int32_t   lump;              // Lump number of the
    
   cmptype_e type;              // Type of lump
} tcomponent_t;


// SoM: Columns are used inside the texture struct to reference the linear
// buffer the textures are painted to.
typedef struct texcol_s
{
   uint16_t yoff, len;
   uint32_t ptroff;
   
   struct texcol_s *next;
} texcol_t;


// A maptexturedef_t describes a rectangular texture, which is composed
// of one or more mappatch_t structures that arrange graphic patches.
// Redesigned by SoM so that the same textures can be used by both walls
// and flats.
typedef enum
{
   // Set if the texture contains see-thru portions
   TF_MASKED    = 0x1,
   // Set if the texture can be used by the optimized flat drawers
   TF_SQUAREFLAT = 0x2,
} texflag_e;

typedef struct texture_s
{
   // SoM: New dog's in town
   mdllistitem_t link;

   char       name[9];       // Keep name for switch changing, etc.
   int16_t    width, height;
   
   // SoM: These are no longer kept in separate arrays
   int32_t    widthmask;
   int32_t    heightfrac;
   
   // SoM: texture attributes
   uint32_t   flags;
   // SoM: If the texture can be used as an optimized flat, this is the size
   // of the flat
   byte       flatsize;
   
   texcol_t   **columns;     // SoM: width length list of columns
   byte       *buffer;       // SoM: Linear buffer the texture occupies
   
   // New texture system can put either textures or flats (or anything, really)
   // into a texture, so the old patches idea has been scrapped for 'graphics'
   // which can be either patch graphics or linear graphics.
   int16_t        ccount;
   tcomponent_t   components[1]; // back-to-front into the cached texture.
} texture_t;

// Retrieve column data for span blitting.
byte *R_GetColumn(int tex, int32_t col);

// I/O, setting up the stuff.
void R_InitData(void);
void R_FreeData(void);
void R_PrecacheLevel(void);

// Retrieval.
// Floor/ceiling opaque texture tiles,
// lookup by name.
int R_FlatNumForName(const char *name);   // killough -- const added
int R_CheckFlatNumForName(const char *name);

// Called by P_Ticker for switches and animations,
// returns the texture number for the texture name.
int R_TextureNumForName(const char *name);       // killough -- const added
int R_CheckTextureNumForName (const char *name); 

void R_InitTranMap(int);      // killough 3/6/98: translucency initialization
int R_ColormapNumForName(const char *name);      // killough 4/4/98

void R_InitColormaps(void);   // killough 8/9/98

// haleyjd: new global colormap method
void R_SetGlobalLevelColormap(void);

extern byte *main_tranmap, *tranmap;

extern int r_precache;

extern int numflats; // haleyjd

extern int global_cmap_index; // haleyjd
extern int global_fog_index;

extern texture_t **textures;

#endif

//----------------------------------------------------------------------------
//
// $Log: r_data.h,v $
// Revision 1.6  1998/05/03  22:55:43  killough
// Add tranmap external declarations
//
// Revision 1.5  1998/04/06  04:48:25  killough
// Add R_ColormapNumForName() prototype
//
// Revision 1.4  1998/03/09  07:26:34  killough
// Add translucency map caching
//
// Revision 1.3  1998/03/02  12:10:05  killough
// Add R_InitTranMap prototype
//
// Revision 1.2  1998/01/26  19:27:34  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

