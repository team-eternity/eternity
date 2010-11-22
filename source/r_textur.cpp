// Emacs style mode select   -*- C -*- 
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
//  Generalized texture system
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "c_io.h"
#include "doomstat.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_main.h"
#include "e_hash.h"
#include "m_swap.h"
#include "p_setup.h"
#include "r_data.h"
#include "r_draw.h"
#include "r_ripple.h"
#include "v_video.h"
#include "w_wad.h"

//
// Texture definition.
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
// The lumps are referenced by number, and patched
// into the rectangular texture space using origin
// and possibly other attributes.
//

typedef struct mappatch_s
{
   int16_t originx;
   int16_t originy;
   int16_t patch;
   int16_t stepdir;         // unused in Doom but might be used in Phase 2 Boom
   int16_t colormap;        // unused in Doom but might be used in Phase 2 Boom
} mappatch_t;

//
// Texture definition.
// A DOOM wall texture is a list of patches
// which are to be combined in a predefined order.
//
typedef struct maptexture_s
{
   char       name[8];
   int32_t    masked;
   int16_t    width;
   int16_t    height;
   int8_t     pad[4];       // unused in Doom but might be used in Boom Phase 2
   int16_t    patchcount;
   mappatch_t patches[1];
} maptexture_t;

// SoM: all textures/flats are now stored in a single array (textures)
// Walls start from wallstart to (wallstop - 1) and flats go from flatstart 
// to (flatstop - 1)
int       wallstart, wallstop;
int       flatstart, flatstop;
int       numwalls, numflats;
int       firstflat, lastflat;

// SoM: This is the number of textures/flats loaded from wads.
// This distinction is important because any textures that EE generates
// will not be cachable. 
int       numwadtex;

// SoM: Index of the BAADF00D invalid texture marker
int       badtex;

int       texturecount;
texture_t **textures;

// SoM: Because all textures and flats are stored in the same array, the 
// translation tables are now combined.
int       *texturetranslation;


//=============================================================================
//
// SoM: Allocator for texture_t
//

//
// R_DetermineFlatSize
//
// Figures out the flat dimensions as which a texture may be used.
//
static void R_DetermineFlatSize(texture_t *t)
{   
   // If not powers of two, it can't be a flat.
   if((t->width != 1 && t->width & (t->width - 1)) ||
      (t->height != 1 && t->height & (t->height - 1)))
      return;
      
   t->flags |= TF_CANBEFLAT;
   
   if(t->width != t->height)
   {
      t->flatsize = FLAT_GENERALIZED;
      return;
   }
      
   switch(t->width * t->height)
   {
   case 4096:  // 64x64
      t->flatsize = FLAT_64;
	  break;
   case 16384: // 128x128
      t->flatsize = FLAT_128;
      break;
   case 65536: // 256x256
      t->flatsize = FLAT_256;
      break;
   case 262144: // 512x512
      t->flatsize = FLAT_512;
      break;
   default: // all other sizes get the generalized drawers
      t->flatsize = FLAT_GENERALIZED;
      break;
   }
}


//
// R_AllocTexStruct
//
// Allocates and initializes a new texture_t struct, filling in all needed data
// for the given parameters.
//
static texture_t *R_AllocTexStruct(const char *name, uint16_t width, 
                                   uint16_t height, int16_t compcount)
{
   size_t    size;
   texture_t *ret;
   int       j;
  
#ifdef RANGECHECK
   if(!width || !height || !name || compcount < 0)
   {
      I_Error("R_AllocTexStruct: Invalid parameters: %s, %i, %i, %i\n", 
              name, width, height, compcount);
   }
#endif

   size = sizeof(texture_t) + sizeof(tcomponent_t) * (compcount - 1);
   
   ret = (texture_t *)(Z_Calloc(1, size, PU_RENDERER, NULL));
   
   ret->name = ret->namebuf;
   strncpy(ret->name, name, 8);
   ret->width = width;
   ret->height = height;
   ret->ccount = compcount;
   
   // SoM: no longer use global lists. This is now done for every texture.
   for(j = 1; j * 2 <= ret->width; j <<= 1)
     ;
     
   ret->widthmask = j - 1;
   ret->heightfrac = ret->height << FRACBITS;

   R_DetermineFlatSize(ret);
   
   return ret;
}

//=============================================================================
//
// haleyjd 10/27/08: new texture reading code
//

static mappatch_t   tp; // temporary patch
static maptexture_t tt; // temporary texture

enum
{
   texture_unknown, // not determined yet
   texture_doom,
   texture_strife
};

typedef struct texturelump_s
{
   int  lumpnum;     // number of lump
   int  maxoff;      // max offset, determined from size of lump
   byte *data;       // cached data
   byte *directory;  // directory pointer
   int  numtextures; // number of textures
   int  format;      // format of textures in this lump
} texturelump_t;

// locations of pad[3] and pad[4] relative to start of maptexture_t
#define MTEX_OFFSET_PAD3 18
#define MTEX_OFFSET_PAD4 19

//
// Binary texture reading routines
//
// Unlike the old code, these are data-alignment and structure-packing
// safe, so they should work on any platform. Also, when reading the
// textures this way, we don't need separate storage structures for
// Doom and Strife.
//

#define TEXSHORT(x) (*(x) | (((int16_t)*((x) + 1)) << 8)); (x) += 2

#define TEXINT(x)  \
   (*(x) | \
    (((int32_t)*((x) + 1)) <<  8) | \
    (((int32_t)*((x) + 2)) << 16) | \
    (((int32_t)*((x) + 3)) << 24)); (x) += 4

static byte *R_ReadDoomPatch(byte *rawpatch)
{
   byte *rover = rawpatch;

   tp.originx = TEXSHORT(rover);
   tp.originy = TEXSHORT(rover);
   tp.patch   = TEXSHORT(rover);
   rover     += 4; // skip "stepdir" and "colormap"

   return rover; // positioned at next patch
}

static byte *R_ReadStrifePatch(byte *rawpatch)
{
   byte *rover = rawpatch;

   tp.originx = TEXSHORT(rover);
   tp.originy = TEXSHORT(rover);
   tp.patch   = TEXSHORT(rover);

   // Rogue removed the unused fields

   return rover; // positioned at next patch
}

static byte *R_ReadUnknownPatch(byte *rawpatch)
{
   I_FatalError(I_ERR_KILL, "R_ReadUnknownPatch called\n");

   return NULL;
}

static byte *R_ReadDoomTexture(byte *rawtexture)
{
   byte *rover = rawtexture;
   int i;

   for(i = 0; i < 8; ++i)
      tt.name[i] = (char)*rover++;

   rover += 4; // skip "masked" - btw, using sizeof on an enum type == naughty

   tt.width       = TEXSHORT(rover);
   tt.height      = TEXSHORT(rover);
   rover         += 4;               // skip "pad" bytes
   tt.patchcount  = TEXSHORT(rover);

   return rover; // positioned for patch reading
}

static byte *R_ReadStrifeTexture(byte *rawtexture)
{
   byte *rover = rawtexture;
   int i;

   for(i = 0; i < 8; ++i)
      tt.name[i] = (char)*rover++;
   
   rover += 4; // skip "unused"

   tt.width      = TEXSHORT(rover);
   tt.height     = TEXSHORT(rover);
   tt.patchcount = TEXSHORT(rover);

   // ^^^ Rogue removed the pad bytes, who knows why.

   return rover; // positioned for patch reading
}

static byte *R_ReadUnknownTexture(byte *rawtexture)
{
   I_FatalError(I_ERR_KILL, "R_ReadUnknownTexture called\n");

   return NULL;
}

typedef struct texturehandler_s
{
   byte *(*ReadTexture)(byte *);
   byte *(*ReadPatch)(byte *);
} texturehandler_t;

static texturehandler_t TextureHandlers[] =
{
   { R_ReadUnknownTexture, R_ReadUnknownPatch }, // texture_none (do not call!)
   { R_ReadDoomTexture,    R_ReadDoomPatch    }, // texture_doom
   { R_ReadStrifeTexture,  R_ReadStrifePatch  }, // texture_strife
};

//
// R_InitTextureLump
//
// Sets up a texturelump structure.
//
static texturelump_t *R_InitTextureLump(const char *lname, boolean required)
{
   texturelump_t *tlump = (texturelump_t *)(calloc(1, sizeof(texturelump_t)));

   if(required)
      tlump->lumpnum = W_GetNumForName(lname);
   else
      tlump->lumpnum = W_CheckNumForName(lname);

   if(tlump->lumpnum >= 0)
   {
      byte *temp;

      tlump->maxoff      = W_LumpLength(tlump->lumpnum);
      tlump->data = temp = (byte *)(W_CacheLumpNum(tlump->lumpnum, PU_STATIC));
      tlump->numtextures = TEXINT(temp);
      tlump->directory   = temp;
   }

   return tlump;
}

//
// R_FreeTextureLump
//
// Undoes R_InitTextureLump
//
static void R_FreeTextureLump(texturelump_t *tlump)
{
   if(tlump->data)
      Z_Free(tlump->data);
   Z_Free(tlump);
}

//
// R_DetectTextureFormat
//
// Decides rather arbitrarily whether we are dealing with DOOM or Strife
// format textures, based on the value of two of the bytes at the "pad"
// offset into each texture (according to ZDoom, due to some crappy tools,
// we cannot use the first two pad bytes for the same purpose).
//
// I am not allowing hybrid lumps. The entire lump must be in one format
// or the other - this may differ from ZDoom (or I misread their code).
//
static void R_DetectTextureFormat(texturelump_t *tlump)
{
   int i;
   int format = texture_doom; // we start out assuming DOOM format...
   byte *directory = tlump->directory;

   for(i = 0; i < tlump->numtextures; ++i)
   {
      int offset;
      byte *mtexture;

      offset = TEXINT(directory);

      // technically this check is not strict enough, but it's the best
      // that can be done without knowing the complete layout of the lump
      // a priori, which is impossible of course
      if(offset > tlump->maxoff)
         I_Error("R_DetectTextureFormat: bad texture directory\n");

      mtexture = tlump->data + offset;

      // ...and if we find any non-zero pad[3/4] bytes, we call it Strife
      if(mtexture[MTEX_OFFSET_PAD3] != 0 || mtexture[MTEX_OFFSET_PAD4] != 0)
      {
         format = texture_strife;
         break; // we found one, that's good enough
      }
   }

   // set the format
   tlump->format = format;
}

//
// R_TextureHacks
//
// SoM: This function determines special cases for some textures with known 
// erroneous data.
//
static void R_TextureHacks(texture_t *t)
{   
   // Adapted from Zdoom's FMultiPatchTexture::CheckForHacks
   if(GameModeInfo->type == Game_DOOM &&
      GameModeInfo->missionInfo->id == doom &&
      t->ccount == 1 &&
      t->height == 128 &&
      t->name[0] == 'S' &&
      t->name[1] == 'K' &&
      t->name[2] == 'Y' &&
      t->name[3] == '1' &&
      t->name[4] == 0 &&
      t->height == 128)
   {
      t->components->originy = 0;
      return;
   }
   
   if(GameModeInfo->type == Game_Heretic &&
      t->height == 128 &&
      t->name[0] == 'S' &&
      t->name[1] == 'K' &&
      t->name[2] == 'Y' &&
      t->name[3] >= '1' &&
      t->name[3] <= '3' &&
      t->name[4] == 0)
   {
      t->height = 200;
      t->heightfrac = 200*FRACUNIT;
      return;
   }

   // BIGDOOR7 in Doom also has patches at y offset -4 instead of 0.
   if (GameModeInfo->type == Game_DOOM &&
      GameModeInfo->missionInfo->id == doom &&
      t->ccount == 2 &&
      t->height == 128 &&
      t->components[0].originy == -4 &&
      t->components[1].originy == -4 &&
      t->name[0] == 'B' &&
      t->name[1] == 'I' &&
      t->name[2] == 'G' &&
      t->name[3] == 'D' &&
      t->name[4] == 'O' &&
      t->name[5] == 'O' &&
      t->name[6] == 'R' &&
      t->name[7] == '7')
   {
      t->components[0].originy = t->components[1].originy = 0;
      return;
   }
}

//
// R_ReadTextureLump
//
// haleyjd: Reads a TEXTUREx lump, which may be in either DOOM or Strife format.
// TODO: Walk the wad lump hash chains and support additive logic.
//
static int R_ReadTextureLump(texturelump_t *tlump, int *patchlookup, int texnum,
                             int *errors)
{
   int i, j;
   byte *directory = tlump->directory;

   for(i = 0; i < tlump->numtextures; ++i, ++texnum)
   {
      int            offset;
      byte           *rawtex, *rawpatch;
      texture_t      *texture;
      tcomponent_t   *component;

      if(!(texnum & 127))     // killough
         V_LoadingIncrease(); // sf

      offset = TEXINT(directory);

      rawtex = tlump->data + offset;

      rawpatch = TextureHandlers[tlump->format].ReadTexture(rawtex);

      texture = textures[texnum] = 
         R_AllocTexStruct(tt.name, tt.width, tt.height, tt.patchcount);
         
      texture->index = texnum;
         
      component = texture->components;

      for(j = 0; j < texture->ccount; ++j, ++component)
      {
         patch_t   pheader;
         
         rawpatch = TextureHandlers[tlump->format].ReadPatch(rawpatch);

         component->originx = tp.originx;
         component->originy = tp.originy;
         component->lump    = patchlookup[tp.patch];
         component->type    = TC_PATCH;

         if(component->lump == -1)
         {
            // killough 8/8/98
            // sf: error_printf
            C_Printf(FC_ERROR "R_ReadTextureLump: Missing patch %d in texture %.8s\n",
                         tp.patch, texture->name);
            //++*errors;
            
            component->width = component->height = 0;
         }
         else
         {
            W_ReadLumpHeader(component->lump, &pheader, sizeof(patch_t));
            component->width = SwapShort(pheader.width);
            component->height = SwapShort(pheader.height);
         }
      }
      
      R_TextureHacks(texture);
   }

   return texnum;
}

//
// End new texture reading code
//
//=============================================================================

//=============================================================================
//
// Texture caching code
//

// Note:
// For rotated buffers (the tex->buffer is actually rotated 90 degrees CCW)
// the calculations for x and y coords are a bit different. Consider:
//       y+ -->
//       0  1  2  3
// x  0  x  o  o  x
// +  1  o  o  o  o
// |  2  o  x  x  o
// v  3  x  o  o  x
//
// The address for a column would actually be:
//    x * texture->height
// and the address for a pixel in the buffer would be:
//    x * texture->height + y


// This struct holds the temporary structure of a masked texture while it is
// build assembled. When a texture is complete, new col structs are allocated 
// in a single block to ensure linearity within memory.
struct tempmask_s
{
   // This is the buffer used for masking
   boolean   mask;       // If set to true, FinishTexture should create columns
   int       buffermax;  // size of allocated buffer
   byte      *buffer;    // mask buffer.
   texture_t *tex;
   
   texcol_t  *tempcols;
} tempmask = { false, 0, NULL, NULL, NULL };

//
// AddTexColumn
//
// Copies from src to the tex buffer and optionally marks the temporary mask
//
static void AddTexColumn(texture_t *tex, const byte *src, int srcstep, 
                         int ptroff, int len)
{
   byte *dest = tex->buffer + ptroff;
   
#ifdef RANGECHECK
   if(ptroff < 0 || ptroff + len > tex->width * tex->height ||
      ptroff + len > tempmask.buffermax)
   {
      I_Error("AddTexColumn(%s) invalid ptroff: %i / (%i, %i)\n", tex->name, 
              ptroff + len, tex->width * tex->height, tempmask.buffermax);
   }
#endif
   if(tempmask.mask && tex == tempmask.tex)
   {
      byte *mask = tempmask.buffer + ptroff;
      
      while(len > 0)
      {
         *dest = *src;
         dest++; src += srcstep; 
         
         *mask = 255; mask++;
         len--;
      }
   }
   else
   {
      while(len > 0)
      {
         *dest = *src;
         dest++; src++;
         len--;
      }
   }
}

//
// AddTexFlat
// 
// Paints the given flat-based component to the texture and marks mask info
//
static void AddTexFlat(texture_t *tex, tcomponent_t *component)
{
   byte      *src = (byte *)(W_CacheLumpNum(component->lump, PU_CACHE));
   int       destoff, srcoff, deststep, srcxstep, srcystep;
   int       xstart, ystart, xstop, ystop;
   int       width, height, wcount, hcount;
   
   // If the flat is supposed to be flipped or rotated, do that here
   // For now, just setup the normal way
   {
      width = component->height;
      height = component->width;
      srcystep = component->width;
      srcxstep = 1;
      
      srcoff = 0;//component->width * (component->height - 1);
   }
   
   // Determine starts and stops
   xstart = component->originx;
   ystart = component->originy;
   xstop = xstart + width;
   ystop = ystart + height;
   
   // Make sure component is not entirely off of the texture (should this count
   // as a texture error?)
   if(xstop <= 0 || xstart >= tex->width ||
      ystop <= 0 || ystart >= tex->height)
      return;
   
   // Offset src or dest based on the x offset first.
   srcoff += xstart < 0 ? -xstart * srcxstep : 0;
   destoff = xstart >= 0 ? xstart * tex->height : 0;
   xstart = xstart >= 0 ? xstart : 0;

   // Adjust offsets based on y-offset.
   if(ystart < 0)
   {
      // Remember that component->originy is negative in this case
      srcoff -= ystart * srcystep;
      ystart = 0;
   }
   else
      destoff += ystart;

   deststep = tex->height;
   
   if(xstop > tex->width)
      xstop = tex->width;
   if(ystop > tex->height)
      ystop = tex->height;
      
   wcount = xstop - xstart;
   hcount = ystop - ystart;
      
   while(wcount > 0)
   {
#ifdef RANGECHECK
      if(srcoff < 0 || srcoff + (hcount - 1) * srcystep > tex->width * tex->height)
         I_Error("AddTexFlat(%s): Invalid srcoff %i / %i\n", srcoff, tex->width * tex->height);
#endif
      AddTexColumn(tex, src + srcoff, srcystep, destoff, hcount);
      srcoff += srcxstep;
      destoff += deststep;
      wcount--;
   }
}

//
// AddTexPatch
// 
// Paints the given flat-based component to the texture and marks mask info
//
static void AddTexPatch(texture_t *tex, tcomponent_t *component)
{
   patch_t    *patch = (patch_t *)(W_CacheLumpNum(component->lump, PU_CACHE));
   int        destoff;
   int        xstart, ystart, xstop;
   int        colindex, colstep;
   int        x, xstep;
   
   // Make sure component is not entirely off of the texture (should this count
   // as a texture error?)
   if(component->originx + component->width <= 0 || 
      component->originx >= tex->width ||
      component->originy + component->height <= 0 ||
      component->originy >= tex->height)
      return;
   
   // Determine starts and stops
   colindex = component->originx < 0 ? -component->originx : 0;
   xstart = component->originx >= 0 ? component->originx : 0;
   xstop = component->originx + component->width;
   
   ystart = component->originy;
   
   if(xstop > tex->width)
      xstop = tex->width;
     
   // if flipped, do so here.
   xstep = 1;
   colstep = 1;
      
   for(x = xstart; x < xstop; x += xstep, colindex += colstep)
   {
      int top, y1, y2, destbase;
      const column_t *column = 
         (const column_t *)((byte *)patch + SwapLong(patch->columnofs[colindex]));
         
      destbase = x * tex->height;
      top = 0;
      
      while(column->topdelta != 0xff)
      {
         const byte *src = (const byte *)column + 3;
         int        srcoff = 0;
         
         top = column->topdelta <= top ? 
               column->topdelta + top :
               column->topdelta;
         
         y1 = ystart + top;
         y2 = y1 + column->length;
         destoff = destbase;
               
         if(y2 <= 0)
         {
            column = (const column_t *)(src + column->length + 1);
            continue;
         }
         
         // Done if this happens
         if(y1 >= tex->height)
            break;
            
         if(y1 < 0)
         {
            srcoff += -(y1);
            y1 = 0;
         }
         else
            destoff += y1;
            
         if(y2 > tex->height)
            y2 = tex->height;

#ifdef RANGECHECK
      if(srcoff < 0 || srcoff + y2 - y1 > column->length)
         I_Error("AddTexFlat(%s): Invalid srcoff %i / %i\n", srcoff, column->length);
#endif
            
         if(y2 - y1 > 0)
            AddTexColumn(tex, src + srcoff, 1, destoff, y2 - y1);
            
         column = (const column_t *)(src + column->length + 1);
      }
   }
}

//
// StartTexture
//
// Allocates the texture buffer, as well as managing the temporary structs and
// the mask buffer.
//
static void StartTexture(texture_t *tex, boolean mask)
{
   int bufferlen = tex->width * tex->height;
   
   // Static for now
   tex->buffer = (byte *)(Z_Malloc(bufferlen, PU_STATIC, (void **)&tex->buffer));
   memset(tex->buffer, 0, sizeof(byte) * bufferlen);
   
   if((tempmask.mask = mask))
   {
      tempmask.tex = tex;
      
      // Setup the temprary mask
      if(bufferlen > tempmask.buffermax || !tempmask.buffer)
      {
         tempmask.buffermax = bufferlen;
         tempmask.buffer = (byte *)(Z_Realloc(tempmask.buffer, bufferlen, 
                                        PU_RENDERER, (void **)&tempmask.buffer));
      }
      memset(tempmask.buffer, 0, bufferlen);
   }
}

//
// NextTempCol
//
// Returns either the next element in the chain or a new element which is
// then added to the chain.
//
static texcol_t *NextTempCol(texcol_t *current)
{
   if(!current)
   {
      if(!tempmask.tempcols)
         return tempmask.tempcols = (texcol_t *)(Z_Calloc(sizeof(texcol_t), 1, PU_STATIC, 0));
      else
         return tempmask.tempcols;
   }
   
   if(!current->next)
      return current->next = (texcol_t *)(Z_Calloc(sizeof(texcol_t), 1, PU_STATIC, 0));
   
   return current->next;
}

//
// FinishTexture
//
// Called after R_CacheTexture is finished drawing a texture. This function
// builds the columns (if needed) of a texture from the temporary mask buffer.
//
static void FinishTexture(texture_t *tex)
{
   int        x, y, i, colcount;
   texcol_t   *col, *tcol;
   byte       *maskp;
   
   Z_ChangeTag(tex->buffer, PU_CACHE);
   
   if(!tempmask.mask)
      return;
      
   if(tempmask.tex != tex)
   {
      // SoM: ERROR?
      return;
   }
   
   // Allocate column pointers
   tex->columns = (texcol_t **)(Z_Calloc(sizeof(texcol_t **), tex->width, PU_RENDERER, 0));
   
   // Build the columsn based on mask info
   maskp = tempmask.buffer;

   for(x = 0; x < tex->width; x++)
   {
      y = 0;
      col = NULL;
      colcount = 0;
      
      while(y < tex->height)
      {
         // Skip transparent pixels
         while(y < tex->height && !*maskp)
         {
            maskp++;
            y++;
         }
         
         // Build a column
         if(y < tex->height && *maskp > 0)
         {
            colcount++;
            col = NextTempCol(col);
            
            col->yoff = y;
            col->ptroff = maskp - tempmask.buffer;
            
            while(y < tex->height && *maskp > 0)
            {
               maskp++; y++;
            }
            
            col->len = y - col->yoff;
         }
      }
      
      // No columns? No problem!
      if(!colcount)
      {
         tex->columns[x] = NULL;
         continue;
      }
         
      // Now allocate and build the actual column structs in the texture
      tcol = tex->columns[x] 
           = (texcol_t *)(Z_Calloc(sizeof(texcol_t), colcount, PU_RENDERER, NULL));
           
      col = NULL;
      for(i = 0; i < colcount; i++)
      {
         col = NextTempCol(col);
         memcpy(tcol, col, sizeof(texcol_t));
         
         tcol->next = i + 1 < colcount ? tcol + 1 : NULL;
         tcol = tcol->next;
      }
   }
}

//
// R_CacheTexture
// 
// Caches a texture in memory, building it from component parts.
//
texture_t *R_CacheTexture(int num)
{
   texture_t  *tex;
   int        i;
   
#ifdef RANGECHECK
   if(num < 0 || num >= texturecount)
      I_Error("R_CacheTexture: invalid texture num %i\n", num);
#endif

   tex = textures[num];
   if(tex->buffer)
      return tex;
   
   // SoM: This situation would most certainly require an abort.
   if(tex->ccount == 0)
   {
      I_Error("R_CacheTexture: texture %s cached with no buffer and no components.\n", 
              tex->name);
   }

   // This function has two primary branches:
   // 1. There is no buffer, and there are no columns which means the texture
   //    has never been built before and needs a full treatment
   // 2. There is no buffer, but there are columns which means that the buffer
   //    (PU_CACHE) has been freed but the columns (PU_RENDERER) have not. 
   //    This case means we only have to rebuilt the buffer.

   // Start the texture. Check the size of the mask buffer if needed.   
   StartTexture(tex, tex->columns == NULL);
   
   // Add the components to the buffer/mask
   for(i = 0; i < tex->ccount; i++)
   {
      tcomponent_t *component = tex->components + i;
      
      // SoM: Do NOT add lumps with a -1 lump
      if(component->lump == -1)
         continue;
         
      switch(component->type)
      {
      case TC_FLAT:
         AddTexFlat(tex, component);
         break;
      case TC_PATCH:
         AddTexPatch(tex, component);
         break;
      default:
         break;
      }
   }

   // Finish texture
   FinishTexture(tex);
   return tex;
}

//
// R_MakeMissingTexture
//
// Creates a checkerboard texture to fill in for unknown textures.
//
static void R_MakeMissingTexture(int count)
{
   texture_t   *tex;
   int         i;
   byte        c1, c2;
   
   if(count >= texturecount)
   {
      usermsg("R_MakeMissingTexture: count >= texturecount\n");
      return;
   }
   
   badtex = count;
   textures[badtex] = tex = R_AllocTexStruct("BAADF00D", 64, 64, 0);
   tex->buffer = (byte *)(Z_Malloc(64*64, PU_RENDERER, NULL));
   
   // Allocate column pointers
   tex->columns = (texcol_t **)(Z_Calloc(sizeof(texcol_t **), tex->width, PU_RENDERER, 0));

   // Make columns
   for(i = 0; i < tex->width; i++)
   {
      tex->columns[i] = (texcol_t *)(Z_Calloc(sizeof(texcol_t), 1, PU_RENDERER, 0));
      tex->columns[i]->next = NULL;
      tex->columns[i]->yoff = 0;
      tex->columns[i]->len = tex->height;
      tex->columns[i]->ptroff = i * tex->height;
   }
   
   // Fill pixels
   c1 = GameModeInfo->whiteIndex;
   c2 = GameModeInfo->blackIndex;
   for(i = 0; i < 4096; i++)
      tex->buffer[i] = ((i & 8) == 8) != ((i & 512) == 512) ? c1 : c2;
}

//=============================================================================
//
// Texture/Flat Init Code
//

//
// R_InitLoading
//
// haleyjd 10/27/08: split out of R_InitTextures
//
static void R_InitLoading(void)
{
   // Really complex printing shit...
   int temp1 = W_GetNumForName("S_START");
   int temp2 = W_GetNumForName("S_END") - 1;
   
   // 1/18/98 killough:  reduce the number of initialization dots
   // and make more accurate
   // sf: reorganised to use loading pic
   int temp3 = 6 + (temp2 - temp1 + 255) / 128 + (numwalls + 255) / 128;
   
   V_SetLoading(temp3, "R_Init:");
}

//
// R_LoadPNames
//
// haleyjd 10/27/08: split out of R_InitTextures
//
static int *R_LoadPNames(void)
{
   int  i;
   int  nummappatches;
   int  *patchlookup;
   char name[9];
   char *names;
   char *name_p;

   // Load the patch names from pnames.lmp.
   name[8] = 0;
   names = (char *)W_CacheLumpName("PNAMES", PU_STATIC);
   nummappatches = SwapLong(*((int *)names));
   name_p = names + 4;
   patchlookup = (int *)(malloc(nummappatches * sizeof(*patchlookup))); // killough
   
   for(i = 0; i < nummappatches; ++i)
   {
      strncpy(name, name_p + i * 8, 8);
      
      patchlookup[i] = W_CheckNumForName(name);
      
      if(patchlookup[i] == -1)
      {
         // killough 4/17/98:
         // Some wads use sprites as wall patches, so repeat check and
         // look for sprites this time, but only if there were no wall
         // patches found. This is the same as allowing for both, except
         // that wall patches always win over sprites, even when they
         // appear first in a wad. This is a kludgy solution to the wad
         // lump namespace problem.
         
         patchlookup[i] = W_CheckNumForNameNS(name, lumpinfo_t::ns_sprites);
         
         if(patchlookup[i] == -1 && devparm)    // killough 8/8/98
            usermsg("\nWarning: patch %.8s, index %d does not exist", name, i);
      }
   }

   // done with PNAMES
   Z_Free(names);

   return patchlookup;
}

//
// R_InitTranslationLUT
//
// haleyjd 10/27/08: split out of R_InitTextures
// SoM: Renamed because now it does only one thing :D
//
static void R_InitTranslationLUT(void)
{
   int i;

   // Create translation table for global animation.
   // killough 4/9/98: make column offsets 32-bit;
   // clean up malloc-ing to use sizeof   
   texturetranslation =
      (int *)(Z_Malloc((texturecount + 1) * sizeof(*texturetranslation), PU_RENDERER, 0));

   for(i = 0; i < texturecount; ++i)
      texturetranslation[i] = i;
}


E_KEYFUNC(texture_t, name);
static ehash_t walltable, flattable;

//
// R_InitTextureHash
//
// This function now inits the two ehash tables and inserts the loaded textures
// into them.
//
static void R_InitTextureHash(void)
{
   int i;
   
   E_HashDestroy(&walltable);
   E_HashDestroy(&flattable);
   
   E_NCStrHashInit(&walltable, 599, E_KEYFUNCNAME(texture_t, name), NULL);
   E_NCStrHashInit(&flattable, 599, E_KEYFUNCNAME(texture_t, name), NULL);
   
   for(i = wallstart; i < wallstop; i++)
      E_HashAddObject(&walltable, textures[i]);
      
   for(i = flatstart; i < flatstop; i++)
      E_HashAddObject(&flattable, textures[i]);
}

//
// R_CountFlats
// 
// SoM: This was split out of R_InitFlats which has been combined with 
// R_InitTextures
//
static void R_CountFlats()
{
   firstflat = W_GetNumForName("F_START") + 1;
   lastflat  = W_GetNumForName("F_END") - 1;
   numflats  = lastflat - firstflat + 1;
}

//
// R_AddFlats
//
// Second half of the old R_InitFlats. This is also called by R_InitTextures
//
static void R_AddFlats(void)
{
   int       i;
   byte      flatsize;
   uint16_t  width, height;
   
   for(i = 0; i < numflats; ++i)
   {
      lumpinfo_t *lump = w_GlobalDir.lumpinfo[i + firstflat];
      texture_t  *tex;
      
      switch(lump->size)
      {
      case 16384: // 128x128
         flatsize = FLAT_128;
         width = height = 128;
         break;
      case 65536: // 256x256
         flatsize = FLAT_256;
         width = height = 256;
         break;
      case 262144: // 512x512
         flatsize = FLAT_512;
         width = height = 512;
         break;
      default: // all other sizes are treated as 64x64
         flatsize = FLAT_64;
         width = height = 64;
         break;
      }
      
      tex = textures[i + flatstart] = 
         R_AllocTexStruct(lump->name, width, height, 1);
     
      tex->flatsize = flatsize;
      tex->index = i + flatstart;
      tex->components[0].lump = i + firstflat;
      tex->components[0].type = TC_FLAT;
      tex->components[0].width = width;
      tex->components[0].height = height;
   }
}


//
// R_InitTextures
//
// Initializes the texture list with the textures from the world map.
//
void R_InitTextures(void)
{
   int *patchlookup;
   int errors = 0;
   int i, texnum = 0;   
   
   texturelump_t *maptex1;
   texturelump_t *maptex2;

   // load PNAMES
   patchlookup = R_LoadPNames();

   // Load the map texture definitions from textures.lmp.
   // The data is contained in one or two lumps,
   //  TEXTURE1 for shareware, plus TEXTURE2 for commercial.
   maptex1 = R_InitTextureLump("TEXTURE1", true);
   maptex2 = R_InitTextureLump("TEXTURE2", false);

   // calculate total textures
   numwalls  = maptex1->numtextures + maptex2->numtextures;
   wallstart = 0;
   wallstop  = wallstart + numwalls;
   
   // Count flats
   R_CountFlats();
   flatstart = numwalls;
   flatstop = flatstart + numflats;
      
   // SoM: Add one more for the missing texture texture
   numwadtex = numwalls + numflats;
   texturecount = numwalls + numflats + 1;
   
   // Allocate textures
   textures = (texture_t **)(Z_Malloc(sizeof(texture_t *) * texturecount, PU_RENDERER, NULL));
   memset(textures, 0, sizeof(texture_t *) * texturecount);

   // init lookup tables
   R_InitTranslationLUT();

   // initialize loading dots / bar
   R_InitLoading();

   // detect texture formats
   R_DetectTextureFormat(maptex1);
   R_DetectTextureFormat(maptex2);

   // read texture lumps
   texnum = R_ReadTextureLump(maptex1, patchlookup, texnum, &errors);
   R_ReadTextureLump(maptex2, patchlookup, texnum, &errors);

   // done with patch lookup
   free(patchlookup);

   // done with texturelumps
   R_FreeTextureLump(maptex1);
   R_FreeTextureLump(maptex2);
   
   if(errors)
      I_Error("\n\n%d texture errors.\n", errors);

   // SoM: This REALLY hits us when starting EE with large wads. Caching 
   // textures on map start would probably be preferable 99.9% of the time...
   // Precache textures
   for(i = wallstart; i < wallstop; ++i)
      R_CacheTexture(i);
   
   if(errors)
      I_Error("\n\n%d texture errors.\n", errors); 
      
   // Load flats
   R_AddFlats();     

   // Create the bad texture texture
   R_MakeMissingTexture(texturecount - 1);
   
   // initialize texture hashing
   R_InitTextureHash();
}

//=============================================================================
//
// Texture/Flat Lookup
//

static int R_Doom1Texture(const char *name);
const char *level_error = NULL;

//
// R_GetRawColumn
//
byte *R_GetRawColumn(int tex, int32_t col)
{
   texture_t  *t = textures[tex];

   col = (col & t->widthmask) * t->height;

   // Lee Killough, eat your heart out! ... well this isn't really THAT bad...
   return (t->flags & TF_SWIRLY && t->flatsize == FLAT_64) ?
          R_DistortedFlat(tex) + col :
          !t->buffer ? R_GetLinearBuffer(tex) + col :
          t->buffer + col;
}

//
// R_GetMaskedColumn
//
texcol_t *R_GetMaskedColumn(int tex, int32_t col)
{
   texture_t *t = textures[tex];
   
   if(!t->buffer)
      R_CacheTexture(tex);
   
   return t->columns[col & t->widthmask];
}

//
// R_GetLinearBuffer
//
byte *R_GetLinearBuffer(int tex)
{
   texture_t *t = textures[tex];
   
   if(!t->buffer)
      R_CacheTexture(tex);

   return t->buffer;
}

//
// R_SearchFlats
//
// Looks up a flat texture object in the hash table.
//
static texture_t *R_SearchFlats(const char *name)
{
   return (texture_t *)(E_HashObjectForKey(&flattable, &name));
}

//
// R_SearchWalls
//
// Looks up a wall texture object in the hash table.
//
static texture_t *R_SearchWalls(const char *name)
{
   return (texture_t *)(E_HashObjectForKey(&walltable, &name));
}

//
// R_FindFlat
//
// Retrieval, get a flat number for a flat name.
//
int R_FindFlat(const char *name)    // killough -- const added
{
   texture_t *tex;

   // Search flats first   
   if(!(tex = R_SearchFlats(name)))
      tex = R_SearchWalls(name);
   
   // SoM: Check here for flat-ness!
   if(tex && !(tex->flags & TF_CANBEFLAT))
      tex = NULL;

   // SoM: Return missing texture index if invalid
   return tex ? tex->index : texturecount - 1;
}

//
// R_CheckForFlat
//
// haleyjd 08/25/09
//
int R_CheckForFlat(const char *name)
{
   texture_t *tex;

   // Search flats first   
   if(!(tex = R_SearchFlats(name)))
      tex = R_SearchWalls(name);
      
   return (tex && tex->flags & TF_CANBEFLAT) ? tex->index : -1;
}

//
// R_CheckForWall
//
// Check whether texture is available.
// Filter out NoTexture indicator.
//
// Rewritten by Lee Killough to use hash table for fast lookup. Considerably
// reduces the time needed to start new levels. See w_wad.c for comments on
// the hashing algorithm, which is also used for lump searches.
//
// killough 1/21/98, 1/31/98
//
int R_CheckForWall(const char *name)
{
   texture_t *tex;
   
   if(*name == '-')     // "NoTexture" marker.
      return 0;

   // Search walls before flats.
   if(!(tex = R_SearchWalls(name)))
      tex = R_SearchFlats(name);
      
   return tex ? tex->index : -1;
}

//
// R_FindWall
//
// Calls R_CheckForWall,
//
// haleyjd 06/08/06: no longer aborts and causes HOMs instead.
// The user can look at the console to see missing texture errors.
//
int R_FindWall(const char *name)  // const added -- killough
{
   int i = R_CheckForWall(name);

   if(i == -1)
   {
      i = R_Doom1Texture(name);   // try doom I textures
      
      if(i == -1)
      {
         C_Printf(FC_ERROR "Texture %.8s not found\n", name);
         
         // SoM: Return index of missing texture texture.
         return texturecount - 1;
      }
   }

   return i;
}

//=============================================================================
//
// Doom I Texture Conversion
//
// convert old doom I levels so they will work under doom II
//

typedef struct doom1text_s
{
   char doom1[9];
   char doom2[9];
} doom1text_t;

doom1text_t *txtrconv;
int numconvs = 0;
int numconvsalloc = 0;

// haleyjd 01/27/09: state table for proper FSA parsing
enum
{
   D1_STATE_COMMENT,
   D1_STATE_SCAN,
   D1_STATE_TEX1,
   D1_STATE_TEX2,
};

//
// R_LoadDoom1
//
// Parses the DOOM -> DOOM II texture conversion table.
// haleyjd: rewritten 01/27/09 to remove heap corruption.
//
void R_LoadDoom1(void)
{
   char *lump;
   char *rover;
   int lumplen, lumpnum;
   int state = D1_STATE_SCAN;
   int tx1, tx2;

   char texture1[9];
   char texture2[9];
   
   if((lumpnum = W_CheckNumForName("TXTRCONV")) == -1)
      return;

   memset(texture1, 0, sizeof(texture1));
   memset(texture2, 0, sizeof(texture2));
   tx1 = tx2 = 0;

   lumplen = W_LumpLength(lumpnum);
   lump    = (char *)(calloc(1, lumplen + 1));
   
   W_ReadLump(lumpnum, lump);
   
   rover = lump;
   numconvs = 0;
   
   while(*rover)
   {
      switch(state)
      {
      case D1_STATE_SCAN:
         if(*rover == ';')
            state = D1_STATE_COMMENT;
         else if(isalnum(*rover))
         {
            texture1[tx1++] = *rover;
            state = D1_STATE_TEX1;
         }
         break;

      case D1_STATE_COMMENT:
         if(*rover == 0x0D || *rover == 0x0A)
            state = D1_STATE_SCAN;
         break;

      case D1_STATE_TEX1:
         if(*rover == 0x0D || *rover == 0x0A) // no linebreak during this state
            I_Error("R_LoadDoom1: malformed TXTRCONV lump: bad linebreak\n");

         if(isspace(*rover))
            state = D1_STATE_TEX2;
         else
         {
            if(tx1 >= 8)
               I_Error("R_LoadDoom1: malformed TXTRCONV lump: tx1 >= 8 chars\n");
            texture1[tx1++] = *rover;
         }
         break;

      case D1_STATE_TEX2:
          // finished with this entry?
         if(*rover == 0x0D || *rover == 0x0A || *(rover+1) == '\0')
         {
            // haleyjd 09/07/05: this was wasting tons of memory before with
            // lots of little mallocs; make the whole thing dynamic
            if(numconvs >= numconvsalloc)
            {
               txtrconv = (doom1text_t *)(realloc(txtrconv, 
                                    (numconvsalloc = numconvsalloc ?
                                     numconvsalloc*2 : 56) * sizeof *txtrconv));
            }
            
            strncpy(txtrconv[numconvs].doom1, texture1, 9);
            strncpy(txtrconv[numconvs].doom2, texture2, 9);
            
            numconvs++;

            // clear out temps
            memset(texture1, 0, sizeof(texture1));
            memset(texture2, 0, sizeof(texture2));
            tx1 = tx2 = 0;

            state = D1_STATE_SCAN;
         }
         else if(!isspace(*rover)) // skip spaces
         {
            if(tx2 >= 8)
               I_Error("R_LoadDoom1: malformed TXTRCONV lump: tx2 >= 8 chars\n");
            texture2[tx2++] = *rover;
         }
         break;
      }

      rover++;
   }
   
   // finished with lump
   free(lump);
}

static int R_Doom1Texture(const char *name)
{
   int i;
   
   // slow i know; should be hash tabled
   // mind you who cares? it's only going to be
   // used by a few people and only at the start of 
   // the level
   
   for(i = 0; i < numconvs; i++)
   {
      if(!strncasecmp(name, txtrconv[i].doom1, 8))   // found it
      {
         doom1level = true;
         return R_CheckForWall(txtrconv[i].doom2);
      }
   }
   
   return -1;
}

// EOF

