// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Stephen McGranahan, James Haley, et al.
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
//-----------------------------------------------------------------------------
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
#include "m_compare.h"
#include "m_swap.h"
#include "p_setup.h"
#include "p_skin.h"
#include "r_data.h"
#include "r_draw.h"
#include "r_patch.h"
#include "r_ripple.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"
#include "w_iterator.h"

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
static texture_t *R_AllocTexStruct(const char *name, int16_t width, 
                                   int16_t height, int16_t compcount)
{
   size_t    size;
   texture_t *ret;
  
#ifdef RANGECHECK
   if(!name || compcount < 0)
   {
      I_Error("R_AllocTexStruct: Invalid parameters: %s, %i, %i, %i\n", 
              name, width, height, compcount);
   }
#endif

   size = sizeof(texture_t) + sizeof(tcomponent_t) * (compcount - 1);
   
   ret = (texture_t *)(Z_Calloc(1, size, PU_RENDERER, NULL));
   
   ret->name = ret->namebuf;
   strncpy(ret->namebuf, name, 8);
   ret->width  = emax<int16_t>(1, width);
   ret->height = emax<int16_t>(1, height);
   ret->ccount = compcount;

   // haleyjd 05/28/14: support non-power-of-two widths
   if(ret->width & (ret->width - 1))
   {
      ret->flags |= TF_WIDTHNP2;
      ret->widthmask = ret->width - 1;
   }
   else
   {
      int j;

      // SoM: no longer use global lists. This is now done for every texture.
      for(j = 1; j * 2 <= ret->width; j <<= 1)
         ;
      ret->widthmask = j - 1;
   }

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
   I_Error("R_ReadUnknownPatch called\n");

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
   I_Error("R_ReadUnknownTexture called\n");

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
static texturelump_t *R_InitTextureLump(const char *lname)
{
   texturelump_t *tlump = ecalloc(texturelump_t *, 1, sizeof(texturelump_t));

   tlump->lumpnum = W_CheckNumForName(lname);

   if(tlump->lumpnum >= 0)
   {
      byte *temp;

      tlump->maxoff      = W_LumpLength(tlump->lumpnum);
      tlump->data = temp = (byte *)(wGlobalDir.cacheLumpNum(tlump->lumpnum, PU_STATIC));
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
   int format = texture_doom; // we start out assuming DOOM format...
   byte *directory = tlump->directory;

   for(int i = 0; i < tlump->numtextures; i++)
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
// R_DoomTextureHacks
//
// GameModeInfo routine to fix up bad Doom textures
//
void R_DoomTextureHacks(texture_t *t)
{
   // Adapted from Zdoom's FMultiPatchTexture::CheckForHacks
   if(t->ccount == 1 &&
      t->height == 128 &&
      t->name[0] == 'S' &&
      t->name[1] == 'K' &&
      t->name[2] == 'Y' &&
      t->name[3] == '1' &&
      t->name[4] == 0)
   {
      t->components->originy = 0;
   }

   // BIGDOOR7 in Doom also has patches at y offset -4 instead of 0.
   if(t->ccount == 2 &&
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
   }
}

//
// R_HticTextureHacks
//
// GameModeInfo routine to fix up bad Heretic textures
//
void R_HticTextureHacks(texture_t *t)
{
   if(t->height == 128 &&
      t->name[0] == 'S' &&
      t->name[1] == 'K' &&
      t->name[2] == 'Y' &&
      t->name[3] >= '1' &&
      t->name[3] <= '3' &&
      t->name[4] == 0)
   {
      t->height = 200;
      t->heightfrac = 200*FRACUNIT;
   }
}

//
// R_ReadTextureLump
//
// haleyjd: Reads a TEXTUREx lump, which may be in either DOOM or Strife format.
// TODO: Walk the wad lump hash chains and support additive logic.
//
static int R_ReadTextureLump(texturelump_t *tlump, int *patchlookup,
                             int nummappatches, int texnum, int *errors)
{
   int i, j;
   byte *directory = tlump->directory;

   for(i = 0; i < tlump->numtextures; i++, texnum++)
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

      for(j = 0; j < texture->ccount; j++, component++)
      {
         rawpatch = TextureHandlers[tlump->format].ReadPatch(rawpatch);

         component->originx = tp.originx;
         component->originy = tp.originy;
         component->type    = TC_PATCH;

         // check range
         if(tp.patch < 0 || tp.patch >= nummappatches)
         {
            C_Printf(FC_ERROR "R_ReadTextureLump: Bad patch %d in texture %.8s\n",
                     tp.patch, (const char *)(texture->name));
            component->width = component->height = 0;
            continue;
         }
         component->lump    = patchlookup[tp.patch];

         if(component->lump == -1)
         {
            // killough 8/8/98
            // sf: error_printf
            C_Printf(FC_ERROR "R_ReadTextureLump: Missing patch %d in texture %.8s\n",
                         tp.patch, (const char *)(texture->name));
            
            component->width = component->height = 0;
         }
         else
         {
            patch_t *p = PatchLoader::CacheNum(wGlobalDir, component->lump, PU_CACHE);
            component->width  = p->width;
            component->height = p->height;
         }
      }
      
      // haleyjd: apply texture hacks on a gamemode-dependent basis
      if(GameModeInfo->TextureHacks)
         GameModeInfo->TextureHacks(texture);
   }

   return texnum;
}

//
// R_ReadTextureNamespace
//
// Add lone-patch textures that exist in the TX_START/TX_END or textures/
// namespace.
//
int R_ReadTextureNamespace(int texnum)
{
   WadNamespaceIterator wni(wGlobalDir, lumpinfo_t::ns_textures);

   for(wni.begin(); wni.current(); wni.next())
   {
      if(!(texnum & 127))
         V_LoadingIncrease();

      lumpinfo_t *lump = wni.current();
      texture_t  *texture;
      patch_t    *patch  = PatchLoader::CacheNum(wGlobalDir, lump->selfindex, PU_CACHE);
      uint16_t    width  = patch->width;
      uint16_t    height = patch->height;

      texture = textures[texnum] = R_AllocTexStruct(lump->name, width, height, 1);
      texture->index = texnum;

      auto component = texture->components;
      component->originx = 0;
      component->originy = 0;
      component->lump    = lump->selfindex;
      component->type    = TC_PATCH;
      component->width   = width;
      component->height  = height;

      ++texnum;
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
   bool       mask;       // If set to true, FinishTexture should create columns
   int        buffermax;  // size of allocated buffer
   byte      *buffer;     // mask buffer.
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
      I_Error("AddTexColumn(%s) invalid ptroff: %i / (%i, %i)\n", 
              (const char *)(tex->name), 
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

      // Repeat last texel to cover one-pixel overflows in column drawing
      // Accept from the ENTIRE buffer, see haleyjd's note in StartTexture()
      if(dest > tex->buffer + ptroff &&
         dest < tex->buffer + tex->width * tex->height + 4)
      {
         *dest = *(dest - 1);
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
   byte      *src = (byte *)(wGlobalDir.cacheLumpNum(component->lump, PU_CACHE));
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
         I_Error("AddTexFlat(%s): Invalid srcoff %i / %i\n", 
                 (const char *)(tex->name), srcoff, tex->width * tex->height);
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
   patch_t *patch = PatchLoader::CacheNum(wGlobalDir, component->lump, PU_CACHE);
   int      destoff;
   int      xstart, ystart, xstop;
   int      colindex, colstep;
   int      x, xstep;
   
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
         (const column_t *)((byte *)patch + patch->columnofs[colindex]);
         
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
            column = reinterpret_cast<const column_t *>(src + column->length + 1);
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
         I_Error("AddTexFlat(%s): Invalid srcoff %i / %i\n", 
                 (const char *)(tex->name), srcoff, column->length);
#endif
            
         if(y2 - y1 > 0)
            AddTexColumn(tex, src + srcoff, 1, destoff, y2 - y1);
            
         column = reinterpret_cast<const column_t *>(src + column->length + 1);
      }
   }
}

//
// StartTexture
//
// Allocates the texture buffer, as well as managing the temporary structs and
// the mask buffer.
//
static void StartTexture(texture_t *tex, bool mask)
{
   // haleyjd 11/18/12: We *must* allocate some pad space in the texture buffer.
   // Due to intermixed use of float and fixed_t in Cardboard, it is impossible
   // to make sure that fracstep is perfectly in sync with y1/y2 values in the
   // column drawers. This can result in a read of up to one additional pixel
   // more than what is available. :/

   int bufferlen = tex->width * tex->height + 4;
   
   // Static for now
   tex->buffer = ecalloctag(byte *, 1, bufferlen, PU_STATIC, (void **)&tex->buffer);
   
   if((tempmask.mask = mask))
   {
      tempmask.tex = tex;
      
      // Setup the temporary mask
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
         return tempmask.tempcols = estructalloc(texcol_t, 1);
      else
         return tempmask.tempcols;
   }
   
   if(!current->next)
      return current->next = estructalloc(texcol_t, 1);
   
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
   tex->columns = ecalloctag(texcol_t **, sizeof(texcol_t **), tex->width, PU_RENDERER, NULL);
   
   // Build the columns based on mask info
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
            col->ptroff = uint32_t(maskp - tempmask.buffer);
            
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
      tcol = tex->columns[x] = estructalloctag(texcol_t, colcount, PU_RENDERER);
           
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
              (const char *)(tex->name));
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
      
      // SoM: Do NOT add lumps with a -1 lumpnum
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
// R_checkerBoardTexture
//
// Fill in a 64x64 texture with a checkerboard pattern.
//
static void R_checkerBoardTexture(texture_t *tex)
{
   // allocate buffer
   tex->buffer = emalloctag(byte *, 64*64, PU_RENDERER, nullptr);

   // allocate column pointers
   tex->columns = ecalloctag(texcol_t **, sizeof(texcol_t *), tex->width, 
                             PU_RENDERER, nullptr);

   // make columns
   for(int i = 0; i < tex->width; i++)
   {
      tex->columns[i] = estructalloctag(texcol_t, 1, PU_RENDERER);
      tex->columns[i]->next = NULL;
      tex->columns[i]->yoff = 0;
      tex->columns[i]->len = tex->height;
      tex->columns[i]->ptroff = i * tex->height;
   }
   
   // fill pixels
   byte c1 = GameModeInfo->whiteIndex;
   byte c2 = GameModeInfo->blackIndex;
   for(int i = 0; i < 4096; i++)
      tex->buffer[i] = ((i & 8) == 8) != ((i & 512) == 512) ? c1 : c2;
}

//
// R_MakeDummyTexture
//
// haleyjd 06/20/14: creates a dummy texture in textures[0] if there are no
// textures defined via TEXTURE1/2 lumps, to prevent problems with mods making
// exclusive use of the ns_textures namespace.
//
static void R_MakeDummyTexture()
{
   textures[0] = R_AllocTexStruct("-BADTEX-", 64, 64, 0);
   R_checkerBoardTexture(textures[0]);
}

//
// R_MakeMissingTexture
//
// Creates a checkerboard texture to fill in for unknown textures.
//
static void R_MakeMissingTexture(int count)
{
   if(count >= texturecount)
   {
      usermsg("R_MakeMissingTexture: count >= texturecount\n");
      return;
   }
   
   badtex = count;
   textures[badtex] = R_AllocTexStruct("BAADF00D", 64, 64, 0);
   R_checkerBoardTexture(textures[badtex]);   
}

//
// Checks if a texture is invalid and replaces it with a missing texture
// Must be called at PU_RENDER init time.
//
static void R_checkInvalidTexture(int num)
{
   // don't care about leaking; invalid texture memory are reclaimed at
   // PU_RENDER purge time.
   if(textures[num] && !textures[num]->buffer && !textures[num]->ccount)
      R_MakeMissingTexture(num);
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
static void R_InitLoading()
{
   // Really complex printing shit...
   const WadDirectory::namespace_t &ns =
      wGlobalDir.getNamespace(lumpinfo_t::ns_sprites);

   int temp1 = ns.firstLump - 1;
   int temp2 = ns.firstLump + ns.numLumps - 1;
   
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
static int *R_LoadPNames(int &nummappatches)
{
   int  i, lumpnum;
   int  *patchlookup;
   char name[9];
   char *names;
   char *name_p;

   if((lumpnum = wGlobalDir.checkNumForName("PNAMES")) < 0)
   {
      nummappatches = 0;
      return nullptr;
   }

   int lumpsize = wGlobalDir.lumpLength(lumpnum);
   if(lumpsize < 4)
   {
      usermsg("\nError: PNAMES lump too small\n");
      nummappatches = 0;
      return nullptr;
   }

   // Load the patch names from pnames.lmp.
   name[8] = 0;
   names = (char *)wGlobalDir.cacheLumpNum(lumpnum, PU_STATIC);
   nummappatches = SwapLong(*((int *)names));

   if(nummappatches * 8 + 4 > lumpsize)
   {
      usermsg("\nError: PNAMES size %d smaller than expected %d\n", lumpsize,
              nummappatches * 8 + 4);
      efree(names);
      nummappatches = 0;
      return nullptr;
   }

   name_p = names + 4;
   patchlookup = emalloc(int *, nummappatches * sizeof(*patchlookup)); // killough
   
   for(i = 0; i < nummappatches; i++)
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
static void R_InitTranslationLUT()
{
   // Create translation table for global animation.
   // killough 4/9/98: make column offsets 32-bit;
   // clean up malloc-ing to use sizeof   
   texturetranslation = 
      emalloctag(int *, (texturecount + 1) * sizeof(*texturetranslation), PU_RENDERER, NULL);

   for(int i = 0; i < texturecount; i++)
      texturetranslation[i] = i;
}

//=============================================================================
//
// Texture Hashing
//

static EHashTable<texture_t, ENCStringHashKey, &texture_t::name, &texture_t::link> walltable;
static EHashTable<texture_t, ENCStringHashKey, &texture_t::name, &texture_t::link> flattable;

//
// R_InitTextureHash
//
// This function now inits the two ehash tables and inserts the loaded textures
// into them.
//
static void R_InitTextureHash()
{
   int i;
   
   walltable.destroy();
   flattable.destroy();

   // haleyjd 12/12/10: For efficiency, allocate as many chains as there are 
   // entries, plus a few more for breathing room.
   walltable.initialize(wallstop - wallstart + 31);
   flattable.initialize(flatstop - flatstart + 31);
   
   for(i = wallstart; i < wallstop; i++)
      walltable.addObject(textures[i]);
      
   for(i = flatstart; i < flatstop; i++)
      flattable.addObject(textures[i]);
}

//
// R_CountFlats
// 
// SoM: This was split out of R_InitFlats which has been combined with 
// R_InitTextures
//
static void R_CountFlats()
{
   const WadDirectory::namespace_t &ns =
      wGlobalDir.getNamespace(lumpinfo_t::ns_flats);

   firstflat = ns.firstLump;
   numflats  = ns.numLumps;
   lastflat  = ns.firstLump + ns.numLumps - 1;
}

//
// R_linearOptimalSize
//
// Try to find the most rectangular size for a linear raw graphic, preferring
// a wider width than height when the graphic is not square.
//
// Rational log2(N) courtesy of Sean Eron Anderson's famous bit hacks:
// http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogObvious
//
static void R_linearOptimalSize(size_t lumpsize, int16_t &w, int16_t &h)
{
   if(!lumpsize)
   {
      w = h = 0;
      return;
   }

   size_t v = lumpsize;
   size_t r = 0;
   
   while(v >>= 1)
      ++r;

   h = static_cast<int16_t>(1 << (r / 2));
   w = static_cast<int16_t>(lumpsize / h);
}

//
// R_AddFlats
//
// Second half of the old R_InitFlats. This is also called by R_InitTextures
//
static void R_AddFlats()
{
   byte     flatsize;
   int16_t width, height;
   lumpinfo_t **lumpinfo = wGlobalDir.getLumpInfo();
   
   for(int i = 0; i < numflats; i++)
   {
      lumpinfo_t *lump = lumpinfo[i + firstflat];
      texture_t  *tex;
      
      switch(lump->size)
      {
      case 4096: // normal 64x64 flats
      case 4160: // Heretic 64x65 scrolling flats
      case 8192: // Hexen 64x128 scrolling flats
         flatsize = FLAT_64;
         width = height = 64;
         break;
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
      default: // something odd...
         flatsize = FLAT_GENERALIZED;
         R_linearOptimalSize(lump->size, width, height);
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
void R_InitTextures()
{
   auto &tns = wGlobalDir.getNamespace(lumpinfo_t::ns_textures);
   int *patchlookup;
   int errors = 0;
   int i, texnum = 0;
   bool needDummy = false;
   
   texturelump_t *maptex1;
   texturelump_t *maptex2;

   // load PNAMES
   int nummappatches;
   patchlookup = R_LoadPNames(nummappatches);

   // Load the map texture definitions from textures.lmp.
   // The data is contained in one or two lumps,
   //  TEXTURE1 for shareware, plus TEXTURE2 for commercial.
   maptex1 = R_InitTextureLump("TEXTURE1");
   maptex2 = R_InitTextureLump("TEXTURE2");

   // calculate total textures before ns_textures namespace
   numwalls = maptex1->numtextures + maptex2->numtextures;

   // if there are no TEXTURE1/2 lookups, we need to create a dummy texture
   if(!numwalls)
   {
      ++numwalls;
      needDummy = true;
   }

   // add in ns_textures namespace
   numwalls += tns.numLumps;

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

   // if we need a dummy texture, add it now.
   if(needDummy)
   {
      R_MakeDummyTexture();
      ++texnum;
   }

   // read texture lumps
   texnum = R_ReadTextureLump(maptex1, patchlookup, nummappatches, texnum, &errors);
   texnum = R_ReadTextureLump(maptex2, patchlookup, nummappatches, texnum, &errors);
   R_ReadTextureNamespace(texnum);

   // done with patch lookup
   if(patchlookup)
      efree(patchlookup);

   // done with texturelumps
   R_FreeTextureLump(maptex1);
   R_FreeTextureLump(maptex2);
   
   if(errors)
      I_Error("\n\n%d texture errors.\n", errors);

   // SoM: This REALLY hits us when starting EE with large wads. Caching 
   // textures on map start would probably be preferable 99.9% of the time...
   // Precache textures
   for(i = wallstart; i < wallstop; i++)
   {
      R_checkInvalidTexture(i);
      R_CacheTexture(i);
   }
   
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

   // haleyjd 05/28/14: support non-power-of-two widths
   if(t->flags & TF_WIDTHNP2)
      col = (col % t->width) * t->height;
   else
      col = (col & t->widthmask) * t->height;

   // Lee Killough, eat your heart out! ... well this isn't really THAT bad...
   return (t->flags & TF_SWIRLY) ?
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

   // haleyjd 05/28/14: support non-power-of-two widths
   return t->columns[(t->flags & TF_WIDTHNP2) ? col % t->width : col & t->widthmask];
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
   return flattable.objectForKey(name);
}

//
// R_SearchWalls
//
// Looks up a wall texture object in the hash table.
//
static texture_t *R_SearchWalls(const char *name)
{
   return walltable.objectForKey(name);
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
void R_LoadDoom1()
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
   lump    = ecalloc(char *, 1, lumplen + 1);
   
   wGlobalDir.readLump(lumpnum, lump);
   
   rover = lump;
   numconvs = 0;
   
   while(*rover)
   {
      switch(state)
      {
      case D1_STATE_SCAN:
         if(*rover == ';')
            state = D1_STATE_COMMENT;
         else if(ectype::isAlnum(*rover))
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

         if(ectype::isSpace((unsigned char)*rover))
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
               numconvsalloc = numconvsalloc ? numconvsalloc*2 : 56;
               txtrconv = erealloc(doom1text_t *, txtrconv, 
                                   numconvsalloc * sizeof *txtrconv);
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
         else if(!ectype::isSpace(*rover)) // skip spaces
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
   efree(lump);
}

static int R_Doom1Texture(const char *name)
{
   // slow i know; should be hash tabled
   // mind you who cares? it's only going to be
   // used by a few people and only at the start of 
   // the level
   
   for(int i = 0; i < numconvs; i++)
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

