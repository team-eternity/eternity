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
//      Preparation of data for rendering,
//      generation of lookups, caching, retrieval by name.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "autopalette.h"
#include "c_io.h"
#include "d_gi.h"
#include "d_io.h"     // SoM 3/14/2002: strncasecmp
#include "d_main.h"
#include "doomstat.h"
#include "e_hash.h"
#include "m_compare.h"
#include "m_swap.h"
#include "p_info.h"   // haleyjd
#include "p_skin.h"
#include "p_setup.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_patch.h"
#include "r_sky.h"
#include "r_state.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"

// Need wad iterators for colormaps
#include "w_iterator.h"

// SoM: This has moved to r_textur.c
void R_LoadDoom1();

//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
//

// ============================================================================
// SoM: Moved textures to r_textur.c

void R_InitTextures();

// killough 4/17/98: make firstcolormaplump,lastcolormaplump external
int         firstcolormaplump; // killough 4/17/98
int         firstspritelump, lastspritelump, numspritelumps;

// needed for pre-rendering
fixed_t     *spritewidth, *spriteoffset, *spritetopoffset;
// SoM: used by cardboard
float       *spriteheight;
// ioanch: portal sprite copying cache info
spritespan_t **r_spritespan;

//
// R_InitSpriteLumps
//
// Finds the width and hoffset of all sprites in the wad,
// so the sprite does not need to be cached completely
// just for having the header info ready during rendering.
//
static void R_InitSpriteLumps(void)
{
   int i;
   patch_t *patch;

   const WadDirectory::namespace_t &ns = 
      wGlobalDir.getNamespace(lumpinfo_t::ns_sprites);
   
   firstspritelump = ns.firstLump;
   numspritelumps  = ns.numLumps;
   lastspritelump  = ns.firstLump + ns.numLumps - 1;
   
   // killough 4/9/98: make columnd offsets 32-bit;
   // clean up malloc-ing to use sizeof
   
   spritewidth     = emalloctag(fixed_t *, numspritelumps * sizeof(*spritewidth),     PU_RENDERER, nullptr);
   spriteoffset    = emalloctag(fixed_t *, numspritelumps * sizeof(*spriteoffset),    PU_RENDERER, nullptr);
   spritetopoffset = emalloctag(fixed_t *, numspritelumps * sizeof(*spritetopoffset), PU_RENDERER, nullptr);
   spriteheight    = emalloctag(float *,   numspritelumps * sizeof(*spriteheight),    PU_RENDERER, nullptr);
   
   for(i = 0; i < numspritelumps; ++i)
   {
      // sf: loading pic
      if(!(i&127))            // killough
         V_LoadingIncrease();
      
      patch = PatchLoader::CacheNum(wGlobalDir, firstspritelump + i, PU_CACHE);

      spritewidth[i]     = patch->width << FRACBITS;
      spriteoffset[i]    = patch->leftoffset << FRACBITS;
      spritetopoffset[i] = patch->topoffset << FRACBITS;
      spriteheight[i]    = (float)patch->height;
   }
}

//
// Init height cache for rendering across sector portals
//
void R_InitSpriteProjSpan()
{
   r_spritespan = emalloctag(decltype(r_spritespan),
                              numsprites * sizeof(*r_spritespan), PU_RENDERER,
                              nullptr);
   for(int i = 0; i < numsprites; ++i)
   {
      const spritedef_t &sprite = sprites[i];
      r_spritespan[i] = emalloctag(spritespan_t *,
                                    sprite.numframes * sizeof(**r_spritespan),
                                    PU_RENDERER, nullptr);
      for(int j = 0; j < sprite.numframes; ++j)
      {
         const spriteframe_t &frame = sprite.spriteframes[j];
         spritespan_t &span = r_spritespan[i][j];
         if(frame.rotate)
         {
            span.bottom = FLT_MAX;
            span.top = -FLT_MAX;
            span.side = 0;
            span.sideFixed = 0;
            for(int16_t lump : frame.lump)
            {
               float height = spriteheight[lump];
               auto yofs = M_FixedToFloat(spritetopoffset[lump]);
               fixed_t sideFixed = emax(spritewidth[lump] - spriteoffset[lump], spriteoffset[lump]);
               float side = M_FixedToFloat(sideFixed);

               if(yofs - height < span.bottom)
                  span.bottom = yofs - height;
               if(yofs > span.top)
                  span.top = yofs;
               if(side > span.side)
                  span.side = side;
               if(sideFixed > span.sideFixed)
                  span.sideFixed = sideFixed;
            }
         }
         else
         {
            int16_t lump = frame.lump[0];
            span.top = M_FixedToFloat(spritetopoffset[lump]);
            span.bottom = span.top - spriteheight[lump];
            span.sideFixed = emax(spritewidth[lump] - spriteoffset[lump], spriteoffset[lump]);
            span.side = M_FixedToFloat(span.sideFixed);
         }
      }
   }
}

static int r_numglobalmaps;

//
// R_InitColormaps
//
// killough 3/20/98: rewritten to allow dynamic colormaps
// and to remove unnecessary 256-byte alignment
//
// killough 4/4/98: Add support for C_START/C_END markers
//
static void R_InitColormaps()
{
   const WadDirectory::namespace_t &ns =
      wGlobalDir.getNamespace(lumpinfo_t::ns_colormaps);

   r_numglobalmaps = 1; // always at least 1, for COLORMAP

   // check for global FOGMAP, for Hexen's benefit
   int fogmap;
   if((fogmap = W_CheckNumForName("FOGMAP")) >= 0)
      ++r_numglobalmaps;

   numcolormaps = ns.numLumps + r_numglobalmaps;

   // colormaps[0] is always the global COLORMAP lump
   size_t numbytes = sizeof(*colormaps) * numcolormaps;
   int    cmlump   = W_GetNumForName("COLORMAP");

   colormaps    = emalloctag(lighttable_t **, numbytes, PU_RENDERER, nullptr);
   colormaps[0] = (lighttable_t *)(wGlobalDir.cacheLumpNum(cmlump, PU_RENDERER));

   // colormaps[1] is FOGMAP, if it exists
   if(fogmap >= 0)
      colormaps[1] = (lighttable_t *)(wGlobalDir.cacheLumpNum(fogmap, PU_RENDERER));

   // load other colormaps from the colormaps namespace
   int i = r_numglobalmaps;
   WadNamespaceIterator nsi(wGlobalDir, lumpinfo_t::ns_colormaps);
  
   for(nsi.begin(); nsi.current(); nsi.next(), i++)
      colormaps[i] = (lighttable_t *)(wGlobalDir.cacheLumpNum((*nsi)->selfindex, PU_RENDERER));

   firstcolormaplump = ns.firstLump;
}

// haleyjd: new global colormap system -- simply sets an index to
//   the appropriate colormap and the rendering code checks this
//   instead of assuming it should always use colormap 0 -- much
//   cleaner but requires more modification to the rendering code.
//
// 03/04/07: added outdoor fogmap

int global_cmap_index = 0;
int global_fog_index  = 0;

void R_SetGlobalLevelColormap()
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
   int i = 0;

   // special cases
   if(!strncasecmp(name, "COLORMAP", 8))
      return 0;
   if(r_numglobalmaps > 1 && !strncasecmp(name, "FOGMAP", 8))
      return 1;

   if((i = W_CheckNumForNameNS(name, lumpinfo_t::ns_colormaps)) != -1)
      i = (i - firstcolormaplump) + r_numglobalmaps;

   return i;
}

//
// Get name of colormap from index. Returns nullptr if nothing.
//
const char *R_ColormapNameForNum(int index)
{
   if(index < 0)
      return nullptr;
   if(index == 0)
      return "COLORMAP";
   if(r_numglobalmaps > 1 && index == 1)
      return "FOGMAP";
   index -= r_numglobalmaps;
   const WadDirectory::namespace_t &ns = wGlobalDir.getNamespace(lumpinfo_t::ns_colormaps);
   if(index >= ns.numLumps)
      return nullptr;
   index += firstcolormaplump;
   if(index >= 0 && index < wGlobalDir.getNumLumps())
      return wGlobalDir.getLumpName(index);
   return nullptr;
}

int tran_filter_pct = 66;       // filter percent

#define TSC 12        /* number of fixed point digits in filter percent */

//
// R_InitTranMap
//
// Initialize translucency filter map
//
// By Lee Killough 2/21/98
//
void R_InitTranMap(bool force)
{
   static bool prev_fromlump = false;
   static int  prev_lumpnum  = -1;
   static bool prev_built    = false;
   static int  prev_tran_pct = -1;
   static byte prev_palette[768];

   AutoPalette pal(wGlobalDir);
   byte *playpal = pal.get();

   // if forced to rebuild, do it now regardless of settings
   if(force)
   {
      prev_fromlump = false;
      prev_lumpnum  = -1;
      prev_built    = false;
      prev_tran_pct = -1;
      memset(prev_palette, 0, sizeof(prev_palette));
   }

   int lump = W_CheckNumForName("TRANMAP");
   
   // If a translucency filter map lump is present, use it
   
   if(lump != -1)  // Set a pointer to the translucency filter maps.
   {
      // check if is same as already loaded
      if(prev_fromlump && prev_lumpnum == lump && 
         prev_tran_pct == tran_filter_pct && 
         !memcmp(playpal, prev_palette, 768))
         return;

      if(main_tranmap)
         efree(main_tranmap);
      main_tranmap  = (byte *)(wGlobalDir.cacheLumpNum(lump, PU_STATIC));   // killough 4/11/98
      prev_fromlump = true;
      prev_lumpnum  = lump;
      prev_built    = false;
      prev_tran_pct = tran_filter_pct;
      memcpy(prev_palette, playpal, 768);
   }
   else
   {
      // check if is same as already loaded
      if(prev_built && prev_tran_pct == tran_filter_pct &&
         !memcmp(playpal, prev_palette, 768))
         return;

      // Compose a default transparent filter map based on PLAYPAL.
      if(main_tranmap)
         efree(main_tranmap);
      main_tranmap  = ecalloc(byte *, 256, 256);  // killough 4/11/98
      prev_fromlump = false;
      prev_lumpnum  = -1;
      prev_built    = true;
      prev_tran_pct = tran_filter_pct;
      memcpy(prev_palette, playpal, 768);
      
      int pal[3][256], tot[256], pal_w1[3][256];
      int w1 = ((unsigned int) tran_filter_pct<<TSC)/100;
      int w2 = (1l<<TSC)-w1;

      // First, convert playpal into long int type, and transpose array,
      // for fast inner-loop calculations. Precompute tot array.
      {
         int i = 255;
         const unsigned char *p = playpal + 255 * 3;
         do
         {
            int t,d;
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
      byte *tp = main_tranmap;
      for(int i = 0; i < 256; ++i)
      {
         int r1 = pal[0][i] * w2;
         int g1 = pal[1][i] * w2;
         int b1 = pal[2][i] * w2;

         if(!(i & 31) && force)
            V_LoadingIncrease();        //sf 

         for(int j = 0; j < 256; j++, tp++)
         {
            int color = 255;
            int err;
            int r = pal_w1[0][j] + r1;
            int g = pal_w1[1][j] + g1;
            int b = pal_w1[2][j] + b1;
            int best = INT_MAX;
            do
            {
               if((err = tot[color] - pal[0][color]*r
                  - pal[1][color]*g - pal[2][color]*b) < best)
               {
                  best = err;
                  *tp = color;
               }
            }
            while(--color >= 0);
         }
      }
   }
}

//
// R_InitSubMap
//
// Initialize translucency filter map
//
void R_InitSubMap(bool force)
{
   static bool prev_fromlump = false;
   static int  prev_lumpnum  = -1;
   static bool prev_built    = false;
   static byte prev_palette[768];

   AutoPalette pal(wGlobalDir);
   byte *playpal = pal.get();

   // if forced to rebuild, do it now regardless of settings
   if(force)
   {
      prev_fromlump = false;
      prev_lumpnum  = -1;
      prev_built    = false;
      memset(prev_palette, 0, sizeof(prev_palette));
   }

   int lump = W_CheckNumForName("SUBMAP");
   
   // If a translucency filter map lump is present, use it
   
   if(lump != -1)  // Set a pointer to the translucency filter maps.
   {
      // check if is same as already loaded
      if(prev_fromlump && prev_lumpnum == lump && 
         !memcmp(playpal, prev_palette, 768))
         return;

      if(main_submap)
         efree(main_submap);
      main_submap  = (byte *)(wGlobalDir.cacheLumpNum(lump, PU_STATIC));   // killough 4/11/98
      prev_fromlump = true;
      prev_lumpnum  = lump;
      prev_built    = false;
      memcpy(prev_palette, playpal, 768);
   }
   else
   {
      // check if is same as already loaded
      if(prev_built && !memcmp(playpal, prev_palette, 768))
         return;

      // Compose a default transparent filter map based on PLAYPAL.
      if(main_submap)
         efree(main_submap);
      main_submap  = ecalloc(byte *, 256, 256);  // killough 4/11/98
      prev_fromlump = false;
      prev_lumpnum  = -1;
      prev_built    = true;
      memcpy(prev_palette, playpal, 768);
      
      int pal[3][256], tot[256];

      // First, convert playpal into long int type, and transpose array,
      // for fast inner-loop calculations. Precompute tot array.
      {
         int i = 255;
         const unsigned char *p = playpal + 255 * 3;
         do
         {
            int t,d;
            pal[0][i] = t = p[0];
            d = t*t;
            pal[1][i] = t = p[1];
            d += t*t;
            pal[2][i] = t = p[2];
            d += t*t;
            p -= 3;
            tot[i] = d/2;
         }
         while (--i >= 0);
      }

      // Next, compute all entries using minimum arithmetic.
      byte *tp = main_submap;
      for(int i = 0; i < 256; i++)
      {
         int r1 = pal[0][i];
         int g1 = pal[1][i];
         int b1 = pal[2][i];

         for(int j = 0; j < 256; j++, tp++)
         {
            int color = 255;
            int err;
            // haleyjd: subtract and clamp to 0
            int r = emax(r1 - pal[0][j], 0);
            int g = emax(g1 - pal[1][j], 0);
            int b = emax(b1 - pal[2][j], 0);
            int best = INT_MAX;
            do
            {
               if((err = tot[color] - pal[0][color]*r
                  - pal[1][color]*g - pal[2][color]*b) < best)
               {
                  best = err;
                  *tp = color;
               }
            }
            while(--color >= 0);
         }
      }
   }
}

//
// R_InitData
//
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//
void R_InitData()
{
   static bool firsttime = true;

   P_InitSkins();
   R_InitColormaps();                    // killough 3/20/98
   R_ClearSkyTextures();                 // haleyjd  8/30/02
   R_InitTextures();
   R_InitSpriteLumps();

   if(general_translucency)             // killough 3/1/98, 10/98
   {
      R_InitTranMap(true);          // killough 2/21/98, 3/6/98
      R_InitSubMap(true);
   }
   else
   {
      // sf: fill in dots missing from no translucency build
      for(int i = 0; i < 8; i++)
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


// ============================================================================
// SoM: Moved texture/flat lookup functions to r_textur.c
// ============================================================================


int r_precache = 1;     //sf: option not to precache the levels

//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//
// Totally rewritten by Lee Killough to use less memory,
// to avoid using alloca(), and to improve performance.

void R_PrecacheLevel(void)
{
   int i;
   byte *hitlist;
   int numalloc;

   // IMPORTANT: we must precache textures now, no exceptions
//   if(demoplayback)
//      return;
//   
//   if(!r_precache)
//      return;

   // SoM: Hey, you never know, it could happen....
   numalloc = (texturecount > numsprites ? texturecount : numsprites);
   hitlist = emalloc(byte *, numalloc);

   // Precache textures.
   memset(hitlist, 0, texturecount);
   
   // Mark floors and ceilings
   for(i = numsectors; --i >= 0; )
      hitlist[sectors[i].srf.floor.pic] = hitlist[sectors[i].srf.ceiling.pic] = 1;
      
   // Mark walls
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
   
   skyflat_t *sky = GameModeInfo->skyFlats;
   while(sky->flatname)
   {
      R_CacheSkyTexture(sky->flatnum);
      hitlist[sky->texture] = 1;
      ++sky;
   }

   // Precache textures.
   for(i = texturecount; --i >= 0; )
   {
      if(hitlist[i])
         R_CacheTexture(i);
   }


   // Precache sprites.
   memset(hitlist, 0, numsprites);

   {
      Thinker *th;
      for(th = thinkercap.next ; th != &thinkercap ; th=th->next)
      {
         Mobj *mo;
         if((mo = thinker_cast<Mobj *>(th)))
            hitlist[mo->sprite] = 1;
      }
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
               wGlobalDir.cacheLumpNum(firstspritelump + sflump[k], PU_CACHE);
            while(--k >= 0);
         }
      }
   }
   efree(hitlist);
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
