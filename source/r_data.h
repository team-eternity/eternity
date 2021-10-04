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
//  Refresh module, data I/O, caching, retrieval of graphics
//  by name.
//
//-----------------------------------------------------------------------------

#ifndef R_DATA_H__
#define R_DATA_H__

// Required for: DLListItem
#include "doomtype.h"
#include "m_dllist.h"

enum
{
    // Flag applied on sector_t topmap/midmap/bottommap when colormap should 
    // render like in Boom. Remove it to get the real colormap index.
    COLORMAP_BOOMKIND = 0x80000000, 
};

// haleyjd 08/30/02: externalized these structures

typedef enum
{
   TC_PATCH,
   TC_FLAT,
} cmptype_e;

// Generalized graphic
struct tcomponent_t
{
   int32_t   originx, originy;  // Block origin, which has already accounted
   uint32_t  width, height;     // Unscaled dimensions of the graphic. 

   int32_t   lump;              // Lump number of the

   cmptype_e type;              // Type of lump
};


// SoM: Columns are used inside the texture struct to reference the linear
// buffer the textures are painted to.
struct texcol_t
{
   uint16_t yoff, len;
   uint32_t ptroff;
   
   texcol_t *next;
};


// A maptexturedef_t describes a rectangular texture, which is composed
// of one or more mappatch_t structures that arrange graphic patches.
// Redesigned by SoM so that the same textures can be used by both walls
// and flats.
typedef enum
{
   // Set if the texture contains see-thru portions
   TF_MASKED    = 0x01u,
   // Set by animation code marks this texture as being swirly
   TF_SWIRLY    = 0x02u,
   // Set if the texture can be used as a flat. 
   TF_CANBEFLAT = 0x04u,
   // Set if the texture is animated
   TF_ANIMATED  = 0x08u,
   // Set if texture width is non-power-of-two
   TF_WIDTHNP2  = 0x10u,
} texflag_e;

struct texture_t
{
   // SoM: New dog's in town
   DLListItem<texture_t> link;
   
   // Index within the texture array of this object.
   int           index;

   // For use with ehash stuff
   char      *name;
   char       namebuf[9];       // Keep name for switch changing, etc.
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
   byte       *bufferalloc;   // ioanch: allocate this one with a leading padding for safety
   byte       *bufferdata;    // SoM: Linear buffer the texture occupies (ioanch: points to real data)
   
   // New texture system can put either textures or flats (or anything, really)
   // into a texture, so the old patches idea has been scrapped for 'graphics'
   // which can be either patch graphics or linear graphics.
   int16_t        ccount;
   tcomponent_t   components[1]; // back-to-front into the cached texture.
};

// Retrieve column data for span blitting.
//byte *R_GetColumn(int tex, int32_t col);

// SoM: This is replaced with two functions. For solid walls/skies, we only 
// need the raw column data (direct buffer ptr). For masked mid-textures, we
// need to return columns from the column list
const byte *R_GetRawColumn(int tex, int32_t col);
const texcol_t *R_GetMaskedColumn(int tex, int32_t col);

// SoM: This function returns the linear texture buffer (recache if needed)
const byte *R_GetLinearBuffer(int tex);

// Cache a given texture
// Returns the texture for chaining.
texture_t *R_CacheTexture(int num);

// SoM: all textures/flats are now stored in a single array (textures)
// Walls start from wallstart to (wallstop - 1) and flats go from flatstart 
// to (flatstop - 1)
extern int         flatstart, flatstop;
extern int         numflats;

// SoM: Index of the BAADF00D invalid texture marker
extern int         badtex;

extern int         texturecount;
extern texture_t **textures;

// SoM: Because all textures and flats are stored in the same array, the 
// translation tables are now combined.
extern int        *texturetranslation;

// I/O, setting up the stuff.
void R_InitData(void);
void R_FreeData(void);
void R_PrecacheLevel(void);

void R_InitSpriteProjSpan();

// Retrieval.
// Floor/ceiling opaque texture tiles,
// lookup by name.
int R_FindFlat(const char *name);   // killough -- const added
int R_CheckForFlat(const char *name);

// Called by P_Ticker for switches and animations,
// returns the texture number for the texture name.
int R_FindWall(const char *name);       // killough -- const added
int R_CheckForWall(const char *name); 

void R_InitTranMap(bool force);      // killough 3/6/98: translucency initialization
void R_InitSubMap(bool force);
int  R_ColormapNumForName(const char *name);      // killough 4/4/98
const char *R_ColormapNameForNum(int index);

// haleyjd: new global colormap method
void R_SetGlobalLevelColormap(void);

extern byte *main_tranmap, *main_submap, *tranmap;

extern int r_precache;

extern int global_cmap_index; // haleyjd
extern int global_fog_index;

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

