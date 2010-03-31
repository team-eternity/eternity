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
//  Generalized texture system
//
//-----------------------------------------------------------------------------



#include "r_data.h"
#include "r_draw.h"
#include "doomstat.h"
#include "e_hash.h"
#include "w_wad.h"
#include "v_video.h"
#include "d_io.h"
#include "d_main.h"


static void error_printf(char *s, ...);
static FILE *error_file = NULL;
static char *error_filename;

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
   int8_t     name[8];
   int32_t    masked;
   int16_t    width;
   int16_t    height;
   int8_t     pad[4];       // unused in Doom but might be used in Boom Phase 2
   int16_t    patchcount;
   mappatch_t patches[1];
} maptexture_t;

int         firstflat, lastflat;

// SoM: all textures/flats are now stored in a single array (textures) with the 
// flats indexed from i = numwalls to numwalls + numflats - 1
int         numwalls, numflats;
int         texturecount;
texture_t   **textures;

// SoM: Because all textures and flats are stored in the same array, the 
// translation tables are now combined.
int         *texturetranslation;


// ============================================================================
// SoM: Allocator for texture_t

//
// R_AllocTexStruct
//
// Allocates and initializes a new texture_t struct, filling in all needed data
// for the given parameters.
static texture_t *R_AllocTexStruct(const char *name, uint16_t width, uint16_t height, int16_t compcount)
{
   size_t    size;
   texture_t *ret;
  
#ifdef RANGECHECK
   if(!width || !height || !name || compcount <= 0)
      I_Error("R_AllocTexStruct: Invalid parameters: %s, %i, %i, %i\n", name, width, height, compcount);
#endif

   size = sizeof(texture_t) + sizeof(tcomponent_t) * (compcount - 1);
   
   ret = Z_Malloc(size, PU_RENDERER, NULL);
   memset(ret, 0, size);
      
   ret->width = width;
   ret->height = height;
   ret->ccount = compcount;
   
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
static texturelump_t *R_InitTextureLump(const char *lname, boolean required)
{
   texturelump_t *tlump = calloc(1, sizeof(texturelump_t));

   if(required)
      tlump->lumpnum = W_GetNumForName(lname);
   else
      tlump->lumpnum = W_CheckNumForName(lname);

   if(tlump->lumpnum >= 0)
   {
      byte *temp;

      tlump->maxoff      = W_LumpLength(tlump->lumpnum);
      tlump->data = temp = W_CacheLumpNum(tlump->lumpnum, PU_STATIC);
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
// R_ReadTextureLump
//
static int R_ReadTextureLump(texturelump_t *tlump, int startnum, int *patchlookup,
                             int *errors)
{
   int i, j;
   int texnum = startnum;
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

      component = texture->components;

      for(j = 0; j < texture->ccount; ++j, ++component)
      {
         patch_t   pheader;
         
         rawpatch = TextureHandlers[tlump->format].ReadPatch(rawpatch);

         component->originx = tp.originx;
         component->originy = tp.originy;
         component->lump    = patchlookup[tp.patch];
         component->type    = TC_PATCH;

         W_ReadLumpHeader(component->lump, &pheader, sizeof(patch_t));
         component->width = SwapShort(pheader.width);
         component->height = SwapShort(pheader.height);
         
         if(texture->height >= 128 && 
            component->originy + (int)component->height > texture->height)
         {
            // SoM: Sadly, I must hack even this system
            texture->height = component->originy + component->height;
         }
         
         if(component->lump == -1)
         {
            // killough 8/8/98
            // sf: error_printf
            error_printf("\nR_ReadTextureLump: Missing patch %d in texture %.8s",
                         tp.patch, texture->name);
            ++*errors;
         }
      }

      // SoM: no longer use global lists.
      for(j = 1; j * 2 <= texture->width; j <<= 1)
        ;
        
      texture->widthmask = j - 1;
      texture->heightfrac = texture->height << FRACBITS;
   }

   return texnum;
}

//
// End new texture reading code
//
//=============================================================================

// ============================================================================
//
// Texture caching code


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
struct
{
   // This is the buffer used for masking
   boolean   mask;       // If set to true, FinishTexture should create columns
   int       buffermax;  // size of allocated buffer
   byte      *buffer;    // mask buffer.
   texture_t *tex;
   
   texcol_t  *tempcols;
} tempmask = {false, 0, NULL, NULL, NULL};



//
// AddTexColumn
//
// Copies from src to the tex buffer and optionally marks the temporary mask
static void AddTexColumn(texture_t *tex, const byte *src, int ptroff, int len)
{
   byte *dest = tex->buffer + ptroff;
   
   if(tempmask.mask && tex == tempmask.tex)
   {
      byte *mask = tempmask.buffer + ptroff;
      
      while(len > 0)
      {
         *dest = *src;
         dest++; src++; 
         
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
static void AddTexFlat(texture_t *tex, tcomponent_t *component, int *errornum)
{
   byte      *src = W_CacheLumpNum(component->lump, PU_CACHE);
   byte      *dest = tex->buffer;
   int       destoff, srcoff, deststep, srcstep;
   int       xstart, ystart, xstop, ystop, wcount, hcount;
   
   // Make sure component is not entirely off of the texture (should this count
   // as a texture error?)
   if(component->originx + component->width <= 0 || 
      component->originx >= tex->width ||
      component->originy + component->height <= 0 ||
      component->originy >= tex->height)
      return;
   
   // Determine starts and stops
   xstart = component->originx;
   ystart = component->originy;
   xstop = xstart + component->width;
   ystop = ystart + component->height;
   
   // Offset src or dest based on the x offset first.
   srcoff = xstart < 0 ? -xstart * component->width : 0;
   destoff = xstart >= 0 ? xstart * tex->height : 0;
   xstart = xstart >= 0 ? xstart : 0;

   // Adjust offsets based on y-offset.
   if(ystart < 0)
   {
      // Remember that component->originy is negative in this case
      srcoff -= ystart;
      ystart = 0;
   }
   else
      destoff += ystart;

   srcstep = component->width;
   deststep = tex->height;
   
   if(xstop > tex->width)
      xstop = tex->width;
   if(ystop > tex->height)
      ystop = tex->height;
      
   wcount = xstop - xstart;
   hcount = ystop - ystart;
      
   while(wcount > 0)
   {
      AddTexColumn(tex, src + srcoff, destoff, hcount);
      srcoff += srcstep;
      destoff += deststep;
   }
}



//
// AddTexPatch
// 
// Paints the given flat-based component to the texture and marks mask info
static void AddTexPatch(texture_t *tex, tcomponent_t *component, int *errornum)
{
   patch_t    *patch = W_CacheLumpNum(component->lump, PU_CACHE);
   byte       *dest = tex->buffer;
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
      xstep = tex->width;
     
   // if flipped, do so here.
   xstep = 1;
      
   for(x = xstart; x < xstop; x += xstep, colindex += colstep)
   {
      int top, bot, destbase;
      const column_t *column = 
         (const column_t *)((byte *)patch + SwapLong(patch->columnofs[colindex]));
         
      top = 0;
      destbase = x * tex->height;
      
      while(column->topdelta != 0xff)
      {
         const byte *src = (const byte *)column + 3;
         int        srcoff = 0;
         
         top = column->topdelta <= top ? 
               column->topdelta + top :
               column->topdelta;
         
         bot = ystart + top + column->length;
               
         if(bot <= 0)
         {
            column = (const column_t *)(src + column->length + 1);
            continue;
         }
         
         // Done if this happens
         if(ystart + top >= tex->height)
            break;
            
         if(ystart + top < 0)
            srcoff += -(ystart + top);
         else
            destoff += ystart + top;
            
         if(bot > tex->height)
            bot = tex->height;
            
         if((bot - ystart - top) > 0)
            AddTexColumn(tex, src + srcoff, destbase + destoff, 
                         bot - ystart - top);
            
         column = (const column_t *)(src + column->length + 1);
      }
   }
}



//
// StartTexture
//
// Allocates the texture buffer, as well as managing the temporary structs and
// the mask buffer.
static void StartTexture(texture_t *tex, boolean mask)
{
   int bufferlen = tex->width * tex->height;
   
   // Static for now
   tex->buffer = Z_Malloc(bufferlen, PU_STATIC, (void **)&tex->buffer);
   memset(tex->buffer, 0, sizeof(byte) * bufferlen);
   
   if(tempmask.mask = mask)
   {
      tempmask.tex = tex;
      
      // Setup the temprary mask
      if(bufferlen > tempmask.buffermax || !tempmask.buffer)
      {
         tempmask.buffermax = bufferlen;
         tempmask.buffer = Z_Realloc(tempmask.buffer, bufferlen, 
                                     PU_RENDERER, (void **)&tempmask.buffer);
      }
      memset(tempmask.buffer, 0, bufferlen);
   }
}



// Returns either the next element in the chain or a new element which is
// then added to the chain.
static texcol_t *NextTempCol(texcol_t *current)
{
   if(!current && !tempmask.tempcols)
      return tempmask.tempcols = Z_Calloc(sizeof(texcol_t), 1, PU_STATIC, 0);
   
   if(!current->next)
      return current->next = Z_Calloc(sizeof(texcol_t), 1, PU_STATIC, 0);
   
   return current->next;
}




//
// FinishTexture
//
// Called after R_CacheTexture is finished drawing a texture. This function
// builds the columns (if needed) of a texture from the temporary mask buffer.
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
   tex->columns = Z_Calloc(sizeof(texcol_t **), tex->width, PU_RENDERER, 0);
   
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
           = Z_Calloc(sizeof(texcol_t), colcount, PU_RENDERER, NULL);
           
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
void R_CacheTexture(int num, int *errornum)
{
   texture_t  *tex;
   int        i;
   
#ifdef RANGECHECK
   if(num < 0 || num >= texturecount)
      I_Error("R_CacheTexture given an invalid texture num: %i\n", num);
#endif

   tex = textures[num];
   if(tex->buffer)
      return;

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
      
      switch(component->type)
      {
         case TC_FLAT:
            AddTexFlat(tex, component, errornum);
            break;
         case TC_PATCH:
            AddTexPatch(tex, component, errornum);
            break;
         default:
            break;
      }
   }

   // Finish texture
   FinishTexture(tex);
}


// ============================================================================
// Texture/Flat init code


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
   names = W_CacheLumpName("PNAMES", PU_STATIC);
   nummappatches = SwapLong(*((int *)names));
   name_p = names + 4;
   patchlookup = malloc(nummappatches * sizeof(*patchlookup)); // killough
   
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
         
         patchlookup[i] = W_CheckNumForNameNS(name, ns_sprites);
         
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
      Z_Malloc((texturecount + 1) * sizeof(*texturetranslation), PU_RENDERER, 0);

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
static void R_InitTextureHash(void)
{
   int i;
   
   E_HashDestroy(&walltable);
   E_HashDestroy(&flattable);
   
   E_NCStrHashInit(&walltable, 599, E_KEYFUNCNAME(texture_t, name), NULL);
   E_NCStrHashInit(&flattable, 599, E_KEYFUNCNAME(texture_t, name), NULL);
   
   for(i = 0; i < numwalls; i++)
      E_HashAddObject(&walltable, textures[i]);
      
   for(i = 0; i < numflats; i++)
      E_HashAddObject(&flattable, textures[i + numwalls]);
}



//
// R_CountFlats
// 
// SoM: This was split out of R_InitFlats which is going to get combined with 
// R_InitTextures
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
void R_AddFlats(void)
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
      
      tex = textures[i + numwalls] = 
         R_AllocTexStruct(lump->name, width, height, 1);
     
      tex->flags = TF_SQUAREFLAT;
      tex->flatsize = flatsize;
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
   int texnum = 0;
   int i;   
   
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
   numwalls = maptex1->numtextures + maptex2->numtextures;
   
   // Count flats
   R_CountFlats();
   
   texturecount = numwalls + numflats;
   
   // Allocate textures
   textures = Z_Malloc(sizeof(texture_t *) * texturecount, PU_RENDERER, NULL);
   memset(textures, 0, sizeof(texture_t *) * texturecount);

   // init lookup tables
   R_InitTranslationLUT();

   // initialize loading dots / bar
   R_InitLoading();

   // detect texture formats
   R_DetectTextureFormat(maptex1);
   R_DetectTextureFormat(maptex2);

   // read texture lumps
   texnum = R_ReadTextureLump(maptex1, texnum, patchlookup, &errors);
   texnum = R_ReadTextureLump(maptex2, texnum, patchlookup, &errors);

   // done with patch lookup
   free(patchlookup);

   // done with texturelumps
   R_FreeTextureLump(maptex1);
   R_FreeTextureLump(maptex2);
   
   if(errors)
   {
      fclose(error_file);
      I_Error("\n\n%d texture errors.\nerrors dumped to %s\n", errors, error_filename);
   }

   // Precache textures
   for(i = 0; i < numwalls; ++i)
      R_CacheTexture(i, &errors);
   
   if(errors)
      I_Error("\n\n%d texture errors.", errors); 
      
   // Load flats
   R_AddFlats();     

   // initialize texture hashing
   R_InitTextureHash();
}




// ============================================================================
// Texture/flat lookup

const char *level_error = NULL;

//
// R_FlatNumForName
// Retrieval, get a flat number for a flat name.
//
// killough 4/17/98: changed to use ns_flats namespace
//
int R_FlatNumForName(const char *name)    // killough -- const added
{
   static char errormsg[64];
   int i = W_CheckNumForNameNS(name, ns_flats);
   if(i == -1)
   {
      if(!level_error)
      {
         psnprintf(errormsg, sizeof(errormsg), 
                   "R_FlatNumForName: %.8s not found\n", name);
         level_error = errormsg;
      }
      return -1;
   }
   return (i - firstflat);
}

//
// R_CheckFlatNumForName
//
// haleyjd 08/25/09
//
int R_CheckFlatNumForName(const char *name)
{
   int ret;
   int i = W_CheckNumForNameNS(name, ns_flats);

   if(i != -1)
      ret = i - firstflat;
   else
      ret = -1;

   return ret;
}

//
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
// Rewritten by Lee Killough to use hash table for fast lookup. Considerably
// reduces the time needed to start new levels. See w_wad.c for comments on
// the hashing algorithm, which is also used for lump searches.
//
// killough 1/21/98, 1/31/98
//

int R_CheckTextureNumForName(const char *name)
{
   int i = 0;
   if(*name != '-')     // "NoTexture" marker.
   {
      i = textures[W_LumpNameHash(name) % (unsigned)texturecount]->index;
      while(i >= 0 && strncasecmp(textures[i]->name,name,8))
         i = textures[i]->next;
   }
   return i;
}

//
// R_TextureNumForName
//
// Calls R_CheckTextureNumForName,
//
// haleyjd 06/08/06: no longer aborts and causes HOMs instead.
// The user can look at the console to see missing texture errors.
//
int R_TextureNumForName(const char *name)  // const added -- killough
{
   int i = R_CheckTextureNumForName(name);

   if(i == -1)
   {
      i = R_Doom1Texture(name);   // try doom I textures
      
      if(i == -1)
      {
         C_Printf(FC_ERROR "Texture %.8s not found\n", name);
         return 0; // haleyjd: zero means no texture
      }
   }

   return i;
}



// sf: error printf
// for use w/graphical startup
void error_printf(char *s, ...)
{
   va_list v;
   
   if(!error_file)
   {
      time_t nowtime = time(NULL);
      
      error_filename = "etrn_err.txt";
      error_file = fopen(error_filename, "w");
      fprintf(error_file, "Eternity textures error file\n%s\n",
         ctime(&nowtime));
   }
   
   // haleyjd 09/30/03: changed to vfprintf
   va_start(v, s);
   vfprintf(error_file, s, v);
   va_end(v);
}
