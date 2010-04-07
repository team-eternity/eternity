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
//      Preparation of data for rendering,
//      generation of lookups, caching, retrieval by name.
//
//-----------------------------------------------------------------------------

#include <time.h>

#include "z_zone.h"
#include "doomstat.h"
#include "c_io.h"
#include "d_main.h"
#include "w_wad.h"
#include "p_skin.h"
#include "p_setup.h"
#include "r_main.h"
#include "r_sky.h"
#include "v_video.h"
#include "p_info.h" // haleyjd
#include "d_io.h" // SoM 3/14/2002: strncasecmp
#include "m_misc.h"


static void R_LoadDoom1();
static int R_Doom1Texture(const char *name);
static void error_printf(char *s, ...);
static FILE *error_file = NULL;
static char *error_filename;

//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
//

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

// killough 4/17/98: make firstcolormaplump,lastcolormaplump external
int firstcolormaplump, lastcolormaplump;      // killough 4/17/98

int         firstflat, lastflat, numflats;
int         firstspritelump, lastspritelump, numspritelumps;
int         numtextures;
texture_t **textures;
int32_t    *texturewidthmask;
fixed_t    *textureheight; //needed for texture pegging (and TFE fix - killough)
int32_t    *texturecompositesize;
int16_t   **texturecolumnlump;
uint32_t  **texturecolumnofs;  // killough 4/9/98: make 32-bit
byte      **texturecomposite;
int        *flattranslation;             // for global animation
int        *texturetranslation;

// SoM: large flats
// haleyjd: changed to byte
byte       *flatsize;

// needed for pre-rendering
fixed_t   *spritewidth, *spriteoffset, *spritetopoffset;
// SoM: used by cardboard
float     *spriteheight;

//
// MAPTEXTURE_T CACHING
// When a texture is first needed,
//  it counts the number of composite columns
//  required in the texture and allocates space
//  for a column directory and any new columns.
// The directory will simply point inside other patches
//  if there is only one patch in a given column,
//  but any columns with multiple patches
//  will have new column_t's generated.
//

//
// R_DrawColumnInCache
// Clip and draw a column
//  from a patch into a cached post.
//
// Rewritten by Lee Killough for performance and to fix Medusa bug
//
static void R_DrawColumnInCache(const column_t *patch, byte *cache,
                                int originy, int cacheheight, byte *marks)
{
   while(patch->topdelta != 0xff)
   {
      int count = patch->length;
      int position = originy + patch->topdelta;

      if(position < 0)
      {
         count += position;
         position = 0;
      }

      if(position + count > cacheheight)
         count = cacheheight - position;
      
      if(count > 0)
      {
         memcpy(cache + position, (byte *)patch + 3, count);

         // killough 4/9/98: remember which cells in column have been drawn,
         // so that column can later be converted into a series of posts, to
         // fix the Medusa bug.

         memset(marks + position, 0xff, count);
      }
      
      patch = (column_t *)((byte *) patch + patch->length + 4);
   }
}

//
// R_GenerateComposite
//
// Using the texture definition,
//  the composite texture is created from the patches,
//  and each column is cached.
//
// Rewritten by Lee Killough for performance and to fix Medusa bug
//
static void R_GenerateComposite(int texnum)
{
   byte *block = Z_Malloc(texturecompositesize[texnum], PU_STATIC,
                          (void **) &texturecomposite[texnum]);

   texture_t *texture = textures[texnum];
   
   // Composite the columns together.
   texpatch_t   *patch   = texture->patches;
   int16_t      *collump = texturecolumnlump[texnum];
   uint32_t     *colofs  = texturecolumnofs[texnum]; // killough 4/9/98: make 32-bit
   int          i        = texture->patchcount;
   
   // killough 4/9/98: marks to identify transparent regions in merged textures
   byte *marks = calloc(texture->width, texture->height), *source;

   for(; --i >= 0; ++patch)
   {
      patch_t *realpatch = W_CacheLumpNum(patch->patch, PU_CACHE);
      int x;
      int x1 = patch->originx;
      int x2 = x1 + SwapShort(realpatch->width);
      const int *cofs = realpatch->columnofs - x1;
      
      if(x1 < 0)
         x1 = 0;
      
      if(x2 > texture->width)
         x2 = texture->width;
      
      for(x = x1; x < x2; ++x)
      {
         // Column has multiple patches?
         if(collump[x] == -1)
         {
            // killough 1/25/98, 4/9/98: Fix medusa bug.
            R_DrawColumnInCache((column_t *)((byte *) realpatch + SwapLong(cofs[x])),
                                block + colofs[x], 
                                patch->originy,
                                texture->height, 
                                marks + x * texture->height);
         }
      }
   }

   // killough 4/9/98: Next, convert multipatched columns into true columns,
   // to fix Medusa bug while still allowing for transparent regions.

   source = malloc(texture->height); // temporary column
   
   for(i = 0; i < texture->width; ++i)
   {
      if(collump[i] == -1) // process only multipatched columns
      {
         column_t *col = (column_t *)(block + colofs[i] - 3); // cached column
         const byte *mark = marks + i * texture->height;
         int j = 0;
         
         // save column in temporary so we can shuffle it around
         memcpy(source, (byte *) col + 3, texture->height);
         
         for(;;) // reconstruct the column by scanning transparency marks
         {
            unsigned int len;        // killough 12/98
            
            while(j < texture->height && !mark[j]) // skip transparent cells
               ++j;
            
            if(j >= texture->height)  // if at end of column
            {
               col->topdelta = -1;    // end-of-column marker
               break;
            }

            col->topdelta = j;        // starting offset of post

            // killough 12/98:
            // Use 32-bit len counter, to support tall 1s multipatched textures

            for(len = 0; j < texture->height && mark[j]; ++j)
               ++len; // count opaque cells

            col->length = len; // killough 12/98: intentionally truncate length

            // copy opaque cells from the temporary back into the column
            memcpy((byte *) col + 3, source + col->topdelta, len);
            col = (column_t *)((byte *) col + len + 4); // next post
          }
      }
   }

   free(source); // free temporary column
   free(marks);  // free transparency marks
   
   // Now that the texture has been built in column cache,
   // it is purgable from zone memory.
   
   Z_ChangeTag(block, PU_CACHE);
}

//
// R_GenerateLookup
//
// Rewritten by Lee Killough for performance and to fix Medusa bug
//
static void R_GenerateLookup(int texnum, int *const errors)
{
   const texture_t *texture = textures[texnum];
   
   // Composited texture not created yet.
   
   int16_t  *collump = texturecolumnlump[texnum];
   uint32_t *colofs  = texturecolumnofs[texnum]; // killough 4/9/98: make 32-bit
   
   // killough 4/9/98: keep count of posts in addition to patches.
   // Part of fix for medusa bug for multipatched 2s normals.

   struct lookupcount_s
   {
      unsigned int patches, posts;
   } *count = calloc(sizeof *count, texture->width);

   // killough 12/98: First count the number of patches per column.
   
   const texpatch_t *patch = texture->patches;
   int i = texture->patchcount;
   
   while(--i >= 0)
   {
      int pat = patch->patch;
      const patch_t *realpatch = W_CacheLumpNum(pat, PU_CACHE);
      int x;
      int x1 = patch++->originx;
      int x2 = x1 + SwapShort(realpatch->width);
      const int *cofs = realpatch->columnofs - x1;
      
      if(x2 > texture->width)
         x2 = texture->width;

      if(x1 < 0)
         x1 = 0;

      for(x = x1; x < x2; ++x)
      {
         count[x].patches++;
         collump[x] = pat;
         colofs[x] = SwapLong(cofs[x]) + 3;
      }
   }

   // killough 4/9/98: keep a count of the number of posts in column,
   // to fix Medusa bug while allowing for transparent multipatches.
   //
   // killough 12/98:
   // Post counts are only necessary if column is multipatched,
   // so skip counting posts if column comes from a single patch.
   // This allows arbitrarily tall textures for 1s walls.
   //
   // If texture is >= 256 tall, assume it's 1s, and hence it has
   // only one post per column. This avoids crashes while allowing
   // for arbitrarily tall multipatched 1s textures.
   
   if(texture->patchcount > 1 && texture->height < 256)
   {
      // killough 12/98: Warn about a common column construction bug
      unsigned int limit = texture->height*3+3; // absolute column size limit
      int badcol = devparm;                     // warn only if -devparm used
      
      for(i = texture->patchcount, patch = texture->patches; --i >= 0;)
      {
         int pat = patch->patch;
         const patch_t *realpatch = W_CacheLumpNum(pat, PU_CACHE);
         int x, x1 = patch++->originx, x2 = x1 + SwapShort(realpatch->width);
         const int *cofs = realpatch->columnofs - x1;
         
         if(x2 > texture->width)
            x2 = texture->width;
         if(x1 < 0)
            x1 = 0;
         
         for(x = x1; x < x2; ++x)
         {
            if(count[x].patches > 1)        // Only multipatched columns
            {
               const column_t *col =
                  (column_t*)((byte*) realpatch+SwapLong(cofs[x]));
               const byte *base = (const byte *) col;

               // count posts
               for(; col->topdelta != 0xff; count[x].posts++)
               {
                  if((unsigned int)((byte *) col - base) <= limit)
                     col = (column_t *)((byte *) col + col->length + 4);
                  else
                  { 
                     // killough 12/98: warn about column construction bug
                     if(badcol)
                     {
                        badcol = 0;
                        // sf: changed to usermsg
                        usermsg("\nWarning: Texture %8.8s "
                                "(height %d) has bad column(s)"
                                " starting at x = %d.",
                                texture->name, texture->height, x);
                     }
                     break;
                  }
               }
            }
         }
      }
   }

   // Now count the number of columns
   //  that are covered by more than one patch.
   // Fill in the lump / offset, so columns
   //  with only a single patch are all done.
   
   texturecomposite[texnum] = 0;
   
   {
      int x = texture->width;
      int height = texture->height;
      int32_t csize = 0;
      int err = 0;        // killough 10/98

      while(--x >= 0)
      {
         if(!count[x].patches)     // killough 4/9/98
         {
            if(devparm)
            {
               // killough 8/8/98
               // sf: changed to use error_printf for graphical startup
               error_printf("\nR_GenerateLookup:"
                            " Column %d is without a patch in texture %.8s",
                            x, texture->name);
               ++*errors;
            }
            else
               err = 1;               // killough 10/98
         }

         if(count[x].patches > 1)       // killough 4/9/98
         {
            // killough 1/25/98, 4/9/98:
            //
            // Fix Medusa bug, by adding room for column header
            // and trailer bytes for each post in merged column.
            // For now, just allocate conservatively 4 bytes
            // per post per patch per column, since we don't
            // yet know how many posts the merged column will
            // require, and it's bounded above by this limit.
            
            collump[x] = -1;              // mark lump as multipatched
            colofs[x] = csize + 3;        // three header bytes in a column
            // killough 12/98: add room for one extra post
            csize += 4*count[x].posts+5;  // 1 stop byte plus 4 bytes per post
         }
         csize += height;                  // height bytes of texture data
      }

      texturecompositesize[texnum] = csize;
      
      if(err)       // killough 10/98: non-verbose output
      {
         // sf: error_printf
         error_printf("\nR_GenerateLookup: Column without a patch in texture %.8s",
                      texture->name);
         ++*errors;
      }
   }

   free(count); // killough 4/9/98
}

//
// R_GetColumn
//
byte *R_GetColumn(int tex, int32_t col)
{
   int lump = texturecolumnlump[tex][col &= texturewidthmask[tex]];
   int ofs  = texturecolumnofs[tex][col];
   
   if(lump > 0)
      return (byte *)W_CacheLumpNum(lump, PU_CACHE) + ofs;
   
   if(!texturecomposite[tex])
      R_GenerateComposite(tex);
   
   return texturecomposite[tex] + ofs;
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
      int offset;
      byte *rawtex, *rawpatch;
      texture_t  *texture;
      texpatch_t *patch;

      if(!(texnum & 127))     // killough
         V_LoadingIncrease(); // sf

      offset = TEXINT(directory);

      rawtex = tlump->data + offset;

      rawpatch = TextureHandlers[tlump->format].ReadTexture(rawtex);

      texture = textures[texnum] = 
         Z_Malloc(sizeof(texture_t) + 
                  sizeof(texpatch_t) * (tt.patchcount - 1), 
                  PU_RENDERER, NULL);

      texture->width      = tt.width;
      texture->height     = tt.height;
      texture->patchcount = tt.patchcount;

      memcpy(texture->name, tt.name, sizeof(texture->name));

      patch = texture->patches;

      for(j = 0; j < texture->patchcount; ++j, ++patch)
      {
         rawpatch = TextureHandlers[tlump->format].ReadPatch(rawpatch);

         patch->originx = tp.originx;
         patch->originy = tp.originy;
         patch->patch   = patchlookup[tp.patch];
         
         if(patch->patch == -1)
         {
            // killough 8/8/98
            // sf: error_printf
            error_printf("\nR_ReadTextureLump: Missing patch %d in texture %.8s",
                         tp.patch, texture->name);
            ++*errors;
         }
      }

      // killough 4/9/98: make column offsets 32-bit;
      // clean up malloc-ing to use sizeof
      // killough 12/98: fix sizeofs
      texturecolumnlump[texnum] =
        Z_Malloc(texture->width * sizeof(**texturecolumnlump), PU_RENDERER, NULL);
      texturecolumnofs[texnum] =
        Z_Malloc(texture->width * sizeof(**texturecolumnofs), PU_RENDERER, NULL);

      for(j = 1; j * 2 <= texture->width; j <<= 1)
        ;
      texturewidthmask[texnum] = j - 1;
      textureheight[texnum]    = texture->height << FRACBITS;
   }

   return texnum;
}

//
// End new texture reading code
//
//=============================================================================

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
   int temp3 = 6 + (temp2 - temp1 + 255) / 128 + (numtextures + 255) / 128;
   
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
// R_InitGlobalLookups
//
// haleyjd 10/27/08: split out of R_InitTextures
//
static void R_InitGlobalLookups(void)
{
   int i;

   // allocate global lookups
   // killough 4/9/98: make column offsets 32-bit;
   // clean up malloc-ing to use sizeof
   textures = 
      Z_Malloc(numtextures * sizeof(*textures),             PU_RENDERER, 0);
   texturecolumnlump =
      Z_Malloc(numtextures * sizeof(*texturecolumnlump),    PU_RENDERER, 0);
   texturecolumnofs =
      Z_Malloc(numtextures * sizeof(*texturecolumnofs),     PU_RENDERER, 0);
   texturecomposite =
      Z_Malloc(numtextures * sizeof(*texturecomposite),     PU_RENDERER, 0);
   texturecompositesize =
      Z_Malloc(numtextures * sizeof(*texturecompositesize), PU_RENDERER, 0);
   texturewidthmask =
      Z_Malloc(numtextures * sizeof(*texturewidthmask),     PU_RENDERER, 0);
   textureheight = 
      Z_Malloc(numtextures * sizeof(*textureheight),        PU_RENDERER, 0);

   // Create translation table for global animation.
   // killough 4/9/98: make column offsets 32-bit;
   // clean up malloc-ing to use sizeof   
   texturetranslation =
      Z_Malloc((numtextures + 1) * sizeof(*texturetranslation), PU_RENDERER, 0);

   for(i = 0; i < numtextures; ++i)
      texturetranslation[i] = i;
}

//
// R_InitTextureHash
//
// haleyjd 10/27/08: split out of R_InitTextures
//
static void R_InitTextureHash(void)
{
   int i;

   // killough 1/31/98: Initialize texture hash table
   for(i = 0; i < numtextures; ++i)
      textures[i]->index = -1;

   while(--i >= 0)
   {
      int j = W_LumpNameHash(textures[i]->name) % (unsigned int)numtextures;
      textures[i]->next  = textures[j]->index;   // Prepend to chain
      textures[j]->index = i;
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
   numtextures = maptex1->numtextures + maptex2->numtextures;

   // init lookup tables
   R_InitGlobalLookups();

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

   // Precalculate whatever possible.
   for(i = 0; i < numtextures; ++i)
      R_GenerateLookup(i, &errors);
   
   if(errors)
      I_Error("\n\n%d texture errors.", errors);      

   // initialize texture hashing
   R_InitTextureHash();
}

//
// R_InitFlats
//
void R_InitFlats(void)
{
   int i;
   
   firstflat = W_GetNumForName("F_START") + 1;
   lastflat  = W_GetNumForName("F_END") - 1;
   numflats  = lastflat - firstflat + 1;
   
   // Create translation table for global animation.
   // killough 4/9/98: make column offsets 32-bit;
   // clean up malloc-ing to use sizeof
   
   flattranslation =
      Z_Malloc((numflats+1) * sizeof(*flattranslation), PU_RENDERER, 0);

   flatsize = Z_Malloc((numflats+1) * sizeof(*flatsize), PU_RENDERER, 0);
   
   for(i = 0; i < numflats; ++i)
   {
      int size;
      
      flattranslation[i] = i;
      size = W_LumpLength(firstflat + i);

      switch(size)
      {
      case 16384: // 128x128
         flatsize[i] = FLAT_128;
         break;
      case 65536: // 256x256
         flatsize[i] = FLAT_256;
         break;
      case 262144: // 512x512
         flatsize[i] = FLAT_512;
         break;
      default: // all other sizes are treated as 64x64
         flatsize[i] = FLAT_64;
         break;
      }
   }
}

//
// R_InitSpriteLumps
//
// Finds the width and hoffset of all sprites in the wad,
// so the sprite does not need to be cached completely
// just for having the header info ready during rendering.
//
void R_InitSpriteLumps(void)
{
   int i;
   patch_t *patch;
   
   firstspritelump = W_GetNumForName("S_START") + 1;
   lastspritelump = W_GetNumForName("S_END") - 1;
   numspritelumps = lastspritelump - firstspritelump + 1;
   
   // killough 4/9/98: make columnd offsets 32-bit;
   // clean up malloc-ing to use sizeof
   
   spritewidth = 
      Z_Malloc(numspritelumps * sizeof(*spritewidth), PU_RENDERER, 0);
   spriteoffset = 
      Z_Malloc(numspritelumps * sizeof(*spriteoffset), PU_RENDERER, 0);
   spritetopoffset =
      Z_Malloc(numspritelumps * sizeof(*spritetopoffset), PU_RENDERER, 0);
   spriteheight = 
      Z_Malloc(numspritelumps * sizeof(float), PU_RENDERER, 0);
   
   for(i = 0; i < numspritelumps; ++i)
   {
      // sf: loading pic
      if(!(i&127))            // killough
         V_LoadingIncrease();
      
      patch = W_CacheLumpNum(firstspritelump + i, PU_CACHE);

      spritewidth[i]     = SwapShort(patch->width) << FRACBITS;
      spriteoffset[i]    = SwapShort(patch->leftoffset) << FRACBITS;
      spritetopoffset[i] = SwapShort(patch->topoffset) << FRACBITS;
      spriteheight[i]    = (float)(SwapShort(patch->height));
   }
}

//
// R_InitColormaps
//
// killough 3/20/98: rewritten to allow dynamic colormaps
// and to remove unnecessary 256-byte alignment
//
// killough 4/4/98: Add support for C_START/C_END markers
//
void R_InitColormaps(void)
{
   int i;

   firstcolormaplump = W_GetNumForName("C_START");
   lastcolormaplump  = W_GetNumForName("C_END");
   numcolormaps      = lastcolormaplump - firstcolormaplump;
   colormaps = Z_Malloc(sizeof(*colormaps) * numcolormaps, PU_RENDERER, 0);
   
   colormaps[0] = W_CacheLumpNum(W_GetNumForName("COLORMAP"), PU_RENDERER);
   
   for(i = 1; i < numcolormaps; ++i)
      colormaps[i] = W_CacheLumpNum(i + firstcolormaplump, PU_RENDERER);
}

// haleyjd: new global colormap system -- simply sets an index to
//   the appropriate colormap and the rendering code checks this
//   instead of assuming it should always use colormap 0 -- much
//   cleaner but requires more modification to the rendering code.
//
// 03/04/07: added outdoor fogmap

int global_cmap_index = 0;
int global_fog_index  = 0;

void R_SetGlobalLevelColormap(void)
{
   global_cmap_index = R_ColormapNumForName(LevelInfo.colorMap);
   global_fog_index  = R_ColormapNumForName(LevelInfo.outdoorFog);

   // 04/15/08: this error can be tolerated; default to COLORMAP.
   if(global_cmap_index < 0)
      global_cmap_index = 0;
   if(global_fog_index < 0)
      global_fog_index = 0;
}

// killough 4/4/98: get colormap number from name
// killough 4/11/98: changed to return -1 for illegal names
// killough 4/17/98: changed to use ns_colormaps tag

int R_ColormapNumForName(const char *name)
{
   register int i = 0;
   if(strncasecmp(name, "COLORMAP", 8))     // COLORMAP predefined to return 0
      if((i = W_CheckNumForNameNS(name, ns_colormaps)) != -1)
         i -= firstcolormaplump;
   return i;
}


int tran_filter_pct = 66;       // filter percent

#define TSC 12        /* number of fixed point digits in filter percent */

// haleyjd 06/09/09: moved outside and added packing pragmas

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

struct trmapcache_s 
{
   char signature[4];          // haleyjd 06/09/09: added
   byte pct;
   byte playpal[768]; // haleyjd 06/09/09: corrected 256->768
} __attribute__((packed));

#ifdef _MSC_VER
#pragma pack(pop)
#endif

// haleyjd 06/09/09: for version control of tranmap.dat
#define TRANMAPSIG "Etm1"

//
// R_InitTranMap
//
// Initialize translucency filter map
//
// By Lee Killough 2/21/98
//
void R_InitTranMap(int progress)
{
   int lump = W_CheckNumForName("TRANMAP");
   
   // If a translucency filter map lump is present, use it
   
   if(lump != -1)  // Set a pointer to the translucency filter maps.
      main_tranmap = W_CacheLumpNum(lump, PU_RENDERER);   // killough 4/11/98
   else
   {
      // Compose a default transparent filter map based on PLAYPAL.
      unsigned char *playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);
      
      char *fname = NULL;
      unsigned int fnamesize;
      
      struct trmapcache_s cache;
      
      FILE *cachefp;

      // haleyjd 11/23/06: use basegamepath
      // haleyjd 12/06/06: use Z_Alloca for path length limit removal
      fnamesize = M_StringAlloca(&fname, 1, 12, basegamepath);

      psnprintf(fname, fnamesize, "%s/tranmap.dat", basegamepath);
      
      cachefp = fopen(fname, "r+b");

      main_tranmap = Z_Malloc(256*256, PU_RENDERER, 0);  // killough 4/11/98
      
      // Use cached translucency filter if it's available

      if(!cachefp ? cachefp = fopen(fname, "wb") , 1 :
         fread(&cache, 1, sizeof cache, cachefp) != sizeof cache ||
         strncmp(cache.signature, TRANMAPSIG, 4) ||
         cache.pct != tran_filter_pct ||
         memcmp(cache.playpal, playpal, sizeof cache.playpal) ||
         fread(main_tranmap, 256, 256, cachefp) != 256 ) // killough 4/11/98
      {
         int pal[3][256], tot[256], pal_w1[3][256];
         int w1 = ((unsigned int) tran_filter_pct<<TSC)/100;
         int w2 = (1l<<TSC)-w1;

         // First, convert playpal into long int type, and transpose array,
         // for fast inner-loop calculations. Precompute tot array.

         {
            register int i = 255;
            register const unsigned char *p = playpal + 255 * 3;
            do
            {
               register int t,d;
               pal_w1[0][i] = (pal[0][i] = t = p[0]) * w1;
               d = t*t;
               pal_w1[1][i] = (pal[1][i] = t = p[1]) * w1;
               d += t*t;
               pal_w1[2][i] = (pal[2][i] = t = p[2]) * w1;
               d += t*t;
               p -= 3;
               tot[i] = d << (TSC - 1);
            }
            while (--i >= 0);
         }

         // Next, compute all entries using minimum arithmetic.
         
         {
            int i,j;
            byte *tp = main_tranmap;
            for(i = 0; i < 256; ++i)
            {
               int r1 = pal[0][i] * w2;
               int g1 = pal[1][i] * w2;
               int b1 = pal[2][i] * w2;
               
               if(!(i & 31) && progress)
                  V_LoadingIncrease();        //sf 
               
               if(!(~i & 15))
               {
                  if (i & 32)       // killough 10/98: display flashing disk
                     I_EndRead();
                  else
                     I_BeginRead();
               }

               for(j = 0; j < 256; ++j, ++tp)
               {
                  register int color = 255;
                  register int err;
                  int r = pal_w1[0][j] + r1;
                  int g = pal_w1[1][j] + g1;
                  int b = pal_w1[2][j] + b1;
                  int best = LONG_MAX;
                  do
                  {
                     if((err = tot[color] - pal[0][color]*r
                        - pal[1][color]*g - pal[2][color]*b) < best)
                        best = err, *tp = color;
                  }
                  while(--color >= 0);
               }
            }
         }

         if(cachefp)        // write out the cached translucency map
         {
            strncpy(cache.signature, TRANMAPSIG, 4);
            cache.pct = tran_filter_pct;
            memcpy(cache.playpal, playpal, 768); // haleyjd: corrected 256->768
            fseek(cachefp, 0, SEEK_SET);
            fwrite(&cache, 1, sizeof cache, cachefp);
            fwrite(main_tranmap, 256, 256, cachefp);
         }
      }
      else if(progress)
      {
         int i;
         for(i = 0; i < 8; ++i)
            V_LoadingIncrease();    // 8 '.'s
      }
      
      if(cachefp)              // killough 11/98: fix filehandle leak
         fclose(cachefp);

      Z_ChangeTag(playpal, PU_CACHE);
   }
}

//
// R_InitData
//
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//
void R_InitData(void)
{
   static boolean firsttime = true;

   P_InitSkins();
   R_InitColormaps();                    // killough 3/20/98
   R_ClearSkyTextures();                 // haleyjd  8/30/02
   R_InitTextures();
   R_InitFlats();
   R_InitSpriteLumps();

   if(general_translucency)             // killough 3/1/98, 10/98
   {
      R_InitTranMap(1);          // killough 2/21/98, 3/6/98
   }
   else
   {
      // sf: fill in dots missing from no translucency build
      int i;
      for(i = 0; i < 8; ++i)
         V_LoadingIncrease();    // 8 '.'s
   }

   // haleyjd 11/21/09: first time through here, set DOOM thingtype translucency
   // styles. Why only the first time? We don't need to do this if R_Init is 
   // invoked through adding a new wad file.
   
   // haleyjd 01/28/10: also initialize tantoangle_acc table
   
   if(firsttime)
   {
      Table_InitTanToAngle();
      R_DoomTLStyle();
      firsttime = false;
   }
   
   // process SMMU Doom->Doom2 texture conversion table
   R_LoadDoom1();
}

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
      i = textures[W_LumpNameHash(name) % (unsigned int)numtextures]->index;
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

int r_precache = 1;     //sf: option not to precache the levels

//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//
// Totally rewritten by Lee Killough to use less memory,
// to avoid using alloca(), and to improve performance.

void R_PrecacheLevel(void)
{
   register int i;
   register byte *hitlist;
   unsigned int unumtextures = (unsigned int)numtextures;

   if(demoplayback)
      return;
   
   if(!r_precache)
      return;

   {
      size_t size = numflats > numsprites  ? numflats : numsprites;
      hitlist = malloc(unumtextures > size ? unumtextures : size);
   }

   // Precache flats.
   
   memset(hitlist, 0, numflats);
   
   for(i = numsectors; --i >= 0; )
      hitlist[sectors[i].floorpic] = hitlist[sectors[i].ceilingpic] = 1;

   for(i = numflats; --i >= 0; )
   {
      if(hitlist[i])
         W_CacheLumpNum(firstflat + i, PU_CACHE);
   }

   // Precache textures.
   
   memset(hitlist, 0, numtextures);

   for(i = numsides; --i >= 0; )
   {
      hitlist[sides[i].bottomtexture] =
        hitlist[sides[i].toptexture] =
        hitlist[sides[i].midtexture] = 1;
   }

   // Sky texture is always present.
   // Note that F_SKY1 is the name used to
   //  indicate a sky floor/ceiling as a flat,
   //  while the sky texture is stored like
   //  a wall texture, with an episode dependend
   //  name.
   
   hitlist[skytexture] = 1;
   hitlist[sky2texture] = 1; // haleyjd

   for(i = numtextures; --i >= 0; )
   {
      if(hitlist[i])
      {
         texture_t *texture = textures[i];
         int j = texture->patchcount;
         while(--j >= 0)
            W_CacheLumpNum(texture->patches[j].patch, PU_CACHE);
      }
   }

   // Precache sprites.
   memset(hitlist, 0, numsprites);

   {
      thinker_t *th;
      for(th = thinkercap.next ; th != &thinkercap ; th=th->next)
         if(th->function == P_MobjThinker)
            hitlist[((mobj_t *)th)->sprite] = 1;
   }

   for(i = numsprites; --i >= 0; )
   {
      if (hitlist[i])
      {
         int j = sprites[i].numframes;
         
         while (--j >= 0)
         {
            int16_t *sflump = sprites[i].spriteframes[j].lump;
            int k = 7;
            do
               W_CacheLumpNum(firstspritelump + sflump[k], PU_CACHE);
            while(--k >= 0);
         }
      }
   }
   free(hitlist);
}

//
// R_FreeData
//
// Called when adding a new wad file. Frees all data loaded through R_Init.
// R_Init must then be immediately called again.
//
void R_FreeData(void)
{
   // haleyjd: let's harness the power of the zone heap and make this simple.
   Z_FreeTags(PU_RENDERER, PU_RENDERER);
}

/********************************
        Doom I texture conversion
 *********************************/

// convert old doom I levels so they will
// work under doom II

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
static void R_LoadDoom1(void)
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
   lump    = calloc(1, lumplen + 1);
   
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
               txtrconv = realloc(txtrconv, 
                                  (numconvsalloc = numconvsalloc ?
                                   numconvsalloc*2 : 56) * sizeof *txtrconv);
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
         return R_CheckTextureNumForName(txtrconv[i].doom2);
      }
   }
   
   return -1;
}

// sf: error printf
// for use w/graphical startup

static void error_printf(char *s, ...)
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

//-----------------------------------------------------------------------------
//
// $Log: r_data.c,v $
// Revision 1.23  1998/05/23  08:05:57  killough
// Reformatting
//
// Revision 1.21  1998/05/07  00:52:03  killough
// beautification
//
// Revision 1.20  1998/05/03  22:55:15  killough
// fix #includes at top
//
// Revision 1.19  1998/05/01  18:23:06  killough
// Make error messages look neater
//
// Revision 1.18  1998/04/28  22:56:07  killough
// Improve error handling of bad textures
//
// Revision 1.17  1998/04/27  01:58:08  killough
// Program beautification
//
// Revision 1.16  1998/04/17  10:38:58  killough
// Tag lumps with namespace tags to resolve collisions
//
// Revision 1.15  1998/04/16  10:47:40  killough
// Improve missing flats error message
//
// Revision 1.14  1998/04/14  08:12:31  killough
// Fix seg fault
//
// Revision 1.13  1998/04/12  09:52:51  killough
// Fix ?bad merge? causing seg fault
//
// Revision 1.12  1998/04/12  02:03:51  killough
// rename tranmap main_tranmap, better colormap support
//
// Revision 1.11  1998/04/09  13:19:35  killough
// Fix Medusa for transparent middles, and remove 64K composite texture size limit
//
// Revision 1.10  1998/04/06  04:39:58  killough
// Support multiple colormaps and C_START/C_END
//
// Revision 1.9  1998/03/23  03:33:29  killough
// Add support for an arbitrary number of colormaps, e.g. WATERMAP
//
// Revision 1.8  1998/03/09  07:26:03  killough
// Add translucency map caching
//
// Revision 1.7  1998/03/02  11:54:26  killough
// Don't initialize tranmap until needed
//
// Revision 1.6  1998/02/23  04:54:03  killough
// Add automatic translucency filter generator
//
// Revision 1.5  1998/02/02  13:35:36  killough
// Improve hashing algorithm
//
// Revision 1.4  1998/01/26  19:24:38  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/26  06:11:42  killough
// Fix Medusa bug, tune hash function
//
// Revision 1.2  1998/01/22  05:55:56  killough
// Improve hashing algorithm
//
// Revision 1.3  1997/01/29 20:10
// ???
//
//-----------------------------------------------------------------------------
