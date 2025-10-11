//
// The Eternity Engine
// Copyright (C) 2025 Stephen McGranahan, James Haley, et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Generalized texture system.
// Authors: Stephen McGranahan, James Haley, Ioan Chera, Max Waine,
//  Derek MacDonald.
//

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
#include "m_utils.h"
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

// Texture hash table, used in several places here
using texturehash_t = EHashTable<texture_t, ENCStringHashKey, &texture_t::name, &texture_t::link>;

//
// Texture definition.
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
// The lumps are referenced by number, and patched
// into the rectangular texture space using origin
// and possibly other attributes.
//

struct mappatch_t
{
    int16_t originx;
    int16_t originy;
    int16_t patch;
    int16_t stepdir;  // unused in Doom but might be used in Phase 2 Boom
    int16_t colormap; // unused in Doom but might be used in Phase 2 Boom
};

//
// Texture definition.
// A DOOM wall texture is a list of patches
// which are to be combined in a predefined order.
//
struct maptexture_t
{
    char       name[8];
    int32_t    masked;
    int16_t    width;
    int16_t    height;
    int8_t     pad[4]; // unused in Doom but might be used in Boom Phase 2
    int16_t    patchcount;
    mappatch_t patches[1];
};

// SoM: all textures/flats are now stored in a single array (textures)
// Walls start from wallstart to (wallstop - 1) and flats go from flatstart
// to (flatstop - 1)
static int wallstart, wallstop;
static int numwalls;
int        flatstart, flatstop;
int        numflats;
int        firstflat, lastflat;

// SoM: Index of the BAADF00D invalid texture marker
int badtex;

int         texturecount;
texture_t **textures;

// SoM: Because all textures and flats are stored in the same array, the
// translation tables are now combined.
int *texturetranslation;

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
    if((t->width != 1 && t->width & (t->width - 1)) || (t->height != 1 && t->height & (t->height - 1)))
        return;

    t->flags |= TF_CANBEFLAT;

    if(t->width != t->height)
    {
        t->flatsize = FLAT_GENERALIZED;
        return;
    }

    switch(t->width * t->height)
    {
    case 4096: // 64x64
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
static texture_t *R_AllocTexStruct(const char *name, int16_t width, int16_t height, int16_t compcount)
{
    size_t     size;
    texture_t *ret;

#ifdef RANGECHECK
    if(!name || compcount < 0)
    {
        I_Error("R_AllocTexStruct: Invalid parameters: %s, %i, %i, %i\n", name, width, height, compcount);
    }
#endif

    size = sizeof(texture_t) + sizeof(tcomponent_t) * (compcount - 1);

    ret = ecalloctag(texture_t *, 1, size, PU_RENDERER, nullptr);

    ret->name = ret->namebuf;
    strncpy(ret->namebuf, name, 8);
    ret->width  = emax<int16_t>(1, width);
    ret->height = emax<int16_t>(1, height);
    ret->ccount = compcount;

    // haleyjd 05/28/14: support non-power-of-two widths
    if(ret->width & (ret->width - 1))
    {
        ret->flags     |= TF_WIDTHNP2;
        ret->widthmask  = ret->width - 1;
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

enum
{
    texture_unknown, // not determined yet
    texture_doom,
    texture_strife
};

struct texturelump_t
{
    int   lumpnum;     // number of lump
    int   maxoff;      // max offset, determined from size of lump
    byte *data;        // cached data
    byte *directory;   // directory pointer
    int   numtextures; // number of textures
    int   format;      // format of textures in this lump
};

// locations of pad[3] and pad[4] relative to start of maptexture_t
static constexpr int MTEX_OFFSET_PAD3 = 18;
static constexpr int MTEX_OFFSET_PAD4 = 19;

//
// Binary texture reading routines
//
// Unlike the old code, these are data-alignment and structure-packing
// safe, so they should work on any platform. Also, when reading the
// textures this way, we don't need separate storage structures for
// Doom and Strife.
//

inline static int16_t TEXSHORT(byte *&x)
{
    const int16_t ret = *x | (int16_t(*(x + 1)) << 8);

    x += 2;
    return ret;
}

inline static int32_t TEXINT(byte *&x)
{
    int32_t ret = *x | (int32_t(*(x + 1)) << 8) | (int32_t(*(x + 2)) << 16) | (int32_t(*(x + 3)) << 24);

    x += 4;
    return ret;
}

static byte *R_ReadDoomPatch(byte *rawpatch, mappatch_t &tp)
{
    byte *rover = rawpatch;

    tp.originx  = TEXSHORT(rover);
    tp.originy  = TEXSHORT(rover);
    tp.patch    = TEXSHORT(rover);
    rover      += 4; // skip "stepdir" and "colormap"

    return rover; // positioned at next patch
}

static byte *R_ReadStrifePatch(byte *rawpatch, mappatch_t &tp)
{
    byte *rover = rawpatch;

    tp.originx = TEXSHORT(rover);
    tp.originy = TEXSHORT(rover);
    tp.patch   = TEXSHORT(rover);

    // Rogue removed the unused fields

    return rover; // positioned at next patch
}

static byte *R_ReadUnknownPatch(byte *rawpatch, mappatch_t &tp)
{
    I_Error("R_ReadUnknownPatch called\n");

    return nullptr;
}

static byte *R_ReadDoomTexture(byte *rawtexture, maptexture_t &tt)
{
    byte *rover = rawtexture;
    int   i;

    for(i = 0; i < 8; ++i)
        tt.name[i] = (char)*rover++;

    rover += 4; // skip "masked" - btw, using sizeof on an enum type == naughty

    tt.width       = TEXSHORT(rover);
    tt.height      = TEXSHORT(rover);
    rover         += 4; // skip "pad" bytes
    tt.patchcount  = TEXSHORT(rover);

    return rover; // positioned for patch reading
}

static byte *R_ReadStrifeTexture(byte *rawtexture, maptexture_t &tt)
{
    byte *rover = rawtexture;
    int   i;

    for(i = 0; i < 8; ++i)
        tt.name[i] = (char)*rover++;

    rover += 4; // skip "unused"

    tt.width      = TEXSHORT(rover);
    tt.height     = TEXSHORT(rover);
    tt.patchcount = TEXSHORT(rover);

    // ^^^ Rogue removed the pad bytes, who knows why.

    return rover; // positioned for patch reading
}

static byte *R_ReadUnknownTexture(byte *rawtexture, maptexture_t &tt)
{
    I_Error("R_ReadUnknownTexture called\n");

    return nullptr;
}

struct texturehandler_t
{
    byte *(*ReadTexture)(byte *, maptexture_t &tt);
    byte *(*ReadPatch)(byte *, mappatch_t &tp);
};

static texturehandler_t TextureHandlers[] = {
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

        tlump->maxoff = W_LumpLength(tlump->lumpnum);
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
    int   format    = texture_doom; // we start out assuming DOOM format...
    byte *directory = tlump->directory;

    for(int i = 0; i < tlump->numtextures; i++)
    {
        int   offset;
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
// R_Doom2TextureHacks
//
// GameModeInfo routine to fix up bad Doom II textures
//
void R_Doom2TextureHacks(texture_t *t)
{
    if(t->ccount == 8 && t->height == 128 && t->components[5].originy == -16 && t->components[6].originy == -1 &&
       strcmp(t->name, "BROWN144") == 0)
    {
        t->components[5].originy = t->components[6].originy = 0;
    }
    else if(t->ccount == 3 && t->height == 72 && t->components[0].originy == -8 && strcmp(t->name, "GRAY2") == 0)
    {
        t->components[0].originy = 0;
    }
    else if(t->ccount == 5 && t->height == 128 && t->components[2].originy == -16 && t->components[3].originy == -16 &&
            strcmp(t->name, "GRAYVINE") == 0)
    {
        t->components[2].originy = t->components[3].originy = 0;
    }
    else if(t->ccount == 2 && t->height == 16 && t->components[0].originy == -112 && strcmp(t->name, "STEP2") == 0)
    {
        t->components[0].originy = 0;
    }
    else if(t->ccount == 5 && t->height == 128 && t->components[3].originy == -16 && strcmp(t->name, "SW1DIRT") == 0)
    {
        t->components[3].originy = 0;
    }
    else if(t->ccount == 5 && t->height == 128 && t->components[3].originy == -1 && strcmp(t->name, "SW2DIRT") == 0)
    {
        t->components[3].originy = 0;
    }
    else if(t->ccount == 3 && t->height == 128 && t->components[0].originy == -16 &&
            (strcmp(t->name, "SW1VINE") == 0 || strcmp(t->name, "SW2VINE") == 0))
    {
        t->components[0].originy = 0;
    }
    else if(t->ccount == 2 && t->height == 128 && t->components[0].originy == -27 && strcmp(t->name, "TEKWALL1") == 0)
    {
        t->components[0].originy = 0;
    }
}

//
// R_DoomTextureHacks
//
// GameModeInfo routine to fix up bad Doom textures
//
void R_DoomTextureHacks(texture_t *t)
{
    // Adapted from Zdoom's FMultiPatchTexture::CheckForHacks
    if(t->ccount == 1 && t->height == 128 && strcmp(t->name, "SKY1") == 0)
    {
        t->components->originy = 0;
    }
    else if(t->ccount == 2 && t->height == 128 && t->components[0].originy == -4 && t->components[1].originy == -4 &&
            strcmp(t->name, "BIGDOOR7") == 0)
    {
        t->components[0].originy = t->components[1].originy = 0;
    }
    else if(t->ccount == 8 && t->height == 128 && t->components[5].originy == -16 && t->components[6].originy == -1 &&
            strcmp(t->name, "BROWN144") == 0)
    {
        t->components[5].originy = t->components[6].originy = 0;
    }
    else if(t->ccount == 4 && t->height == 128 && t->components[2].originy == -2 && t->components[3].originy == -2 &&
            strcmp(t->name, "COMPOHSO") == 0)
    {
        t->components[2].originy = t->components[3].originy = 0;
    }
    else if(t->ccount == 1 && t->height == 128 && t->components[0].originy == -8 && strcmp(t->name, "TEKWALL5") == 0)
    {
        t->components[0].originy = 0;
    }
    else
    {
        R_Doom2TextureHacks(t);
    }
}

//
// R_HticTextureHacks
//
// GameModeInfo routine to fix up bad Heretic textures
//
void R_HticTextureHacks(texture_t *t)
{
    if(t->height == 128 && t->name[0] == 'S' && t->name[1] == 'K' && t->name[2] == 'Y' && t->name[3] >= '1' &&
       t->name[3] <= '3' && t->name[4] == 0)
    {
        t->height     = 200;
        t->heightfrac = 200 * FRACUNIT;
    }
}

//
// R_ReadTextureLump
//
// haleyjd: Reads a TEXTUREx lump, which may be in either DOOM or Strife format.
// TODO: Walk the wad lump hash chains and support additive logic.
//
static int R_ReadTextureLump(texturelump_t *tlump, int *patchlookup, int nummappatches, int texnum, int *errors,
                             texturehash_t &duptable)
{
    int          i, j;
    byte        *directory = tlump->directory;
    maptexture_t tt        = {};
    mappatch_t   tp        = {};

    for(i = 0; i < tlump->numtextures; i++, texnum++)
    {
        int           offset;
        byte         *rawtex, *rawpatch;
        texture_t    *texture;
        tcomponent_t *component;

        if(!(texnum & 127))      // killough
            V_LoadingIncrease(); // sf

        offset = TEXINT(directory);

        rawtex = tlump->data + offset;

        rawpatch = TextureHandlers[tlump->format].ReadTexture(rawtex, tt);

        // Like vanilla DOOM, make sure to only keep the first texture from the TEXTURE1/TEXTURE2 lump

        texture = textures[texnum] = R_AllocTexStruct(tt.name, tt.width, tt.height, tt.patchcount);
        duptable.addObject(texture); // add to the duplicate list for later detection

        texture->index = texnum;

        component = texture->components;

        for(j = 0; j < texture->ccount; j++, component++)
        {
            rawpatch = TextureHandlers[tlump->format].ReadPatch(rawpatch, tp);

            component->originx = tp.originx;
            component->originy = tp.originy;
            component->type    = TC_PATCH;

            // check range
            if(tp.patch < 0 || tp.patch >= nummappatches)
            {
                C_Printf(FC_ERROR "R_ReadTextureLump: Bad patch %d in texture %.8s\n", tp.patch,
                         (const char *)(texture->name));
                component->width = component->height = 0;
                continue;
            }
            component->lump = patchlookup[tp.patch];

            if(component->lump == -1)
            {
                // killough 8/8/98
                // sf: error_printf
                C_Printf(FC_ERROR "R_ReadTextureLump: Missing patch %d in texture %.8s\n", tp.patch,
                         (const char *)(texture->name));

                component->width = component->height = 0;
            }
            else
            {
                patch_t *p        = PatchLoader::CacheNum(wGlobalDir, component->lump, PU_CACHE);
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
static int R_ReadTextureNamespace(int texnum)
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

        texture = textures[texnum]  = R_AllocTexStruct(lump->name, width, height, 1);
        texture->index              = texnum;
        texture->flags             |= TF_NONVANILLA;

        auto component     = texture->components;
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
    bool       mask;      // If set to true, FinishTexture should create columns
    int        buffermax; // size of allocated buffer
    byte      *buffer;    // mask buffer.
    texture_t *tex;

    texcol_t *tempcols;
} tempmask = { false, 0, nullptr, nullptr, nullptr };

//
// AddTexColumn
//
// Copies from src to the tex buffer and optionally marks the temporary mask
//
static void AddTexColumn(texture_t *tex, const byte *src, int srcstep, int ptroff, int len)
{
    byte *dest = tex->bufferdata + ptroff;

#ifdef RANGECHECK
    if(ptroff < 0 || ptroff + len > tex->width * tex->height || ptroff + len > tempmask.buffermax)
    {
        I_Error("AddTexColumn(%s) invalid ptroff: %i / (%i, %i)\n", (const char *)(tex->name), ptroff + len,
                tex->width * tex->height, tempmask.buffermax);
    }
#endif

    if(tempmask.mask && tex == tempmask.tex)
    {
        byte *mask = tempmask.buffer + ptroff;

        while(len > 0)
        {
            *dest = *src;
            dest++;
            src += srcstep;

            *mask = 255;
            mask++;
            len--;
        }
    }
    else
    {
        while(len > 0)
        {
            *dest = *src;
            dest++;
            src++;
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
    byte *src = (byte *)(wGlobalDir.cacheLumpNum(component->lump, PU_CACHE));
    int   destoff, srcoff, deststep, srcxstep, srcystep;
    int   xstart, ystart, xstop, ystop;
    int   width, height, wcount, hcount;

    // If the flat is supposed to be flipped or rotated, do that here
    // For now, just setup the normal way
    {
        width    = component->height;
        height   = component->width;
        srcystep = component->width;
        srcxstep = 1;

        srcoff = 0; // component->width * (component->height - 1);
    }

    // Determine starts and stops
    xstart = component->originx;
    ystart = component->originy;
    xstop  = xstart + width;
    ystop  = ystart + height;

    // Make sure component is not entirely off of the texture (should this count
    // as a texture error?)
    if(xstop <= 0 || xstart >= tex->width || ystop <= 0 || ystart >= tex->height)
        return;

    // Offset src or dest based on the x offset first.
    srcoff  += xstart < 0 ? -xstart * srcxstep : 0;
    destoff  = xstart >= 0 ? xstart * tex->height : 0;
    xstart   = xstart >= 0 ? xstart : 0;

    // Adjust offsets based on y-offset.
    if(ystart < 0)
    {
        // Remember that component->originy is negative in this case
        srcoff -= ystart * srcystep;
        ystart  = 0;
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
            I_Error("AddTexFlat(%s): Invalid srcoff %i / %i\n", (const char *)(tex->name), srcoff,
                    tex->width * tex->height);
#endif
        AddTexColumn(tex, src + srcoff, srcystep, destoff, hcount);
        srcoff  += srcxstep;
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
    if(component->originx + component->width <= 0 || component->originx >= tex->width ||
       component->originy + component->height <= 0 || component->originy >= tex->height)
        return;

    // Determine starts and stops
    colindex = component->originx < 0 ? -component->originx : 0;
    xstart   = component->originx >= 0 ? component->originx : 0;
    xstop    = component->originx + component->width;

    ystart = component->originy;

    if(xstop > tex->width)
        xstop = tex->width;

    // if flipped, do so here.
    xstep   = 1;
    colstep = 1;

    for(x = xstart; x < xstop; x += xstep, colindex += colstep)
    {
        int             top, y1, y2, destbase;
        const column_t *column = (const column_t *)((byte *)patch + patch->columnofs[colindex]);

        destbase = x * tex->height;
        top      = 0;

        while(column->topdelta != 0xff)
        {
            const byte *src    = (const byte *)column + 3;
            int         srcoff = 0;

            top = column->topdelta <= top ? column->topdelta + top : column->topdelta;

            y1      = ystart + top;
            y2      = y1 + column->length;
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
                y1      = 0;
            }
            else
                destoff += y1;

            if(y2 > tex->height)
                y2 = tex->height;

#ifdef RANGECHECK
            if(srcoff < 0 || srcoff + y2 - y1 > column->length)
                I_Error("AddTexFlat(%s): Invalid srcoff %i / %i\n", (const char *)(tex->name), srcoff, column->length);
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
    tex->bufferalloc = ecalloctag(byte *, 1, bufferlen + 8, PU_STATIC, (void **)&tex->bufferalloc);
    tex->bufferdata  = tex->bufferalloc + 8;

    if((tempmask.mask = mask))
    {
        tempmask.tex = tex;

        // Setup the temporary mask
        if(bufferlen > tempmask.buffermax || !tempmask.buffer)
        {
            tempmask.buffermax = bufferlen;
            tempmask.buffer    = erealloctag(byte *, tempmask.buffer, bufferlen, PU_RENDERER,
                                             reinterpret_cast<void **>(&tempmask.buffer));
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
// Appends alpha mask to the buffer (by reallocating it as necessary). Needed for masked texture
// portal overlays (visplanes)
//
static void R_appendAlphaMask(texture_t *tex)
{
    int size = tex->width * tex->height;
    // Add space for the mask
    tex->bufferalloc =
        (byte *)Z_Realloc(tex->bufferalloc, 8 + size + (size + 7) / 8 + 4, PU_STATIC, (void **)&tex->bufferalloc);
    tex->bufferdata = tex->bufferalloc + 8;

    const byte *tempmaskp = tempmask.buffer;
    byte       *maskplane = tex->bufferdata + size;
    memset(maskplane, 0, (size + 7) / 8);

    int pos = 0;
    for(int x = 0; x < tex->width; ++x)
        for(int y = 0; y < tex->height; ++y)
        {
            if(*tempmaskp)
                maskplane[pos >> 3] |= 1 << (pos & 7);
            ++pos;
            ++tempmaskp;
        }

    tex->flags |= TF_MASKED; // Finally used here!
}

//
// FinishTexture
//
// Called after R_CacheTexture is finished drawing a texture. This function
// builds the columns (if needed) of a texture from the temporary mask buffer.
//
static void FinishTexture(texture_t *tex)
{
    int         x, y, i, colcount;
    texcol_t   *col, *tcol;
    const byte *maskp;

    if(!tempmask.mask)
        return;

    if(tempmask.tex != tex)
    {
        // SoM: ERROR?
        return;
    }

    // Allocate column pointers
    tex->columns = ecalloctag(texcol_t **, sizeof(texcol_t *), tex->width, PU_RENDERER, nullptr);

    // Build the columns based on mask info
    maskp = tempmask.buffer;

    bool masked = false; // true if texture has holes (more processing needed for portal overlays)

    for(x = 0; x < tex->width; x++)
    {
        y        = 0;
        col      = nullptr;
        colcount = 0;

        while(y < tex->height)
        {
            // Skip transparent pixels
            while(y < tex->height && !*maskp)
            {
                maskp++;
                y++;
                masked = true;
            }

            // Build a column
            if(y < tex->height && *maskp > 0)
            {
                colcount++;
                col = NextTempCol(col);

                col->yoff   = y;
                col->ptroff = uint32_t(maskp - tempmask.buffer);

                while(y < tex->height && *maskp > 0)
                {
                    maskp++;
                    y++;
                }

                col->len = y - col->yoff;
            }
        }

        // No columns? No problem!
        if(!colcount)
        {
            tex->columns[x] = nullptr;
            continue;
        }

        // Now allocate and build the actual column structs in the texture
        tcol = tex->columns[x] = estructalloctag(texcol_t, colcount, PU_RENDERER);

        col = nullptr;
        for(i = 0; i < colcount; i++)
        {
            col = NextTempCol(col);
            memcpy(tcol, col, sizeof(texcol_t));

            tcol->next = i + 1 < colcount ? tcol + 1 : nullptr;
            tcol       = tcol->next;
        }
    }

    if(masked)
        R_appendAlphaMask(tex);
}

//
// Gets a cached texture, which MUST exist at this stage.
//
texture_t *R_GetTexture(int num)
{
#ifdef RANGECHECK
    if(num < 0 || num >= texturecount)
        I_Error("R_GetTexture: invalid texture num %i\n", num);
#endif

    texture_t *tex = textures[num];
    if(tex->bufferalloc)
        return tex;
    else
        I_Error("R_GetTexture: Texture %s is not cached\n", tex->name);
}

//
// Caches a texture in memory, building it from component parts.
//
void R_CacheTexture(int num)
{
    texture_t *tex;
    int        i;

#ifdef RANGECHECK
    if(num < 0 || num >= texturecount)
        I_Error("R_CacheTexture: invalid texture num %i\n", num);
#endif

    tex = textures[num];
    if(tex->bufferalloc)
        return;

    // SoM: This situation would most certainly require an abort.
    if(tex->ccount == 0)
    {
        I_Error("R_CacheTexture: texture %s cached with no buffer and no components.\n", (const char *)(tex->name));
    }

    // This function has two primary branches:
    // 1. There is no buffer, and there are no columns which means the texture
    //    has never been built before and needs a full treatment
    // 2. There is no buffer, but there are columns which means that the buffer
    //    (PU_CACHE) has been freed but the columns (PU_RENDERER) have not.
    //    This case means we only have to rebuilt the buffer.

    // Start the texture. Check the size of the mask buffer if needed.
    StartTexture(tex, tex->columns == nullptr);

    // Add the components to the buffer/mask
    for(i = 0; i < tex->ccount; i++)
    {
        tcomponent_t *component = tex->components + i;

        // SoM: Do NOT add lumps with a -1 lumpnum
        if(component->lump == -1)
            continue;

        switch(component->type)
        {
        case TC_FLAT: //
            AddTexFlat(tex, component);
            break;
        case TC_PATCH: //
            AddTexPatch(tex, component);
            break;
        default: //
            break;
        }
    }

    // Finish texture
    FinishTexture(tex);
    Z_ChangeTag(tex->bufferalloc, PU_CACHE);

    return;
}

//
// R_checkerBoardTexture
//
// Fill in a 64x64 texture with a checkerboard pattern.
//
static void R_checkerBoardTexture(texture_t *tex)
{
    // allocate buffer (also allow some slack space _after_ the buffer)
    tex->bufferalloc = emalloctag(byte *, 64 * 64 + 16, PU_RENDERER, nullptr);
    tex->bufferdata  = tex->bufferalloc + 8;

    // allocate column pointers
    tex->columns = ecalloctag(texcol_t **, sizeof(texcol_t *), tex->width, PU_RENDERER, nullptr);

    // make columns
    for(int i = 0; i < tex->width; i++)
    {
        tex->columns[i]         = estructalloctag(texcol_t, 1, PU_RENDERER);
        tex->columns[i]->next   = nullptr;
        tex->columns[i]->yoff   = 0;
        tex->columns[i]->len    = tex->height;
        tex->columns[i]->ptroff = i * tex->height;
    }

    // fill pixels
    byte c1 = GameModeInfo->whiteIndex;
    byte c2 = GameModeInfo->blackIndex;
    for(int i = 0; i < 4096; i++)
        tex->bufferdata[i] = ((i & 8) == 8) != ((i & 512) == 512) ? c1 : c2;
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

    badtex           = count;
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
    if(textures[num] && !textures[num]->bufferalloc && !textures[num]->ccount)
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
    const WadDirectory::namespace_t &ns = wGlobalDir.getNamespace(lumpinfo_t::ns_sprites);

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
    int   i, lumpnum;
    int  *patchlookup;
    char  name[9];
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
    name[8]       = 0;
    names         = (char *)wGlobalDir.cacheLumpNum(lumpnum, PU_STATIC);
    nummappatches = SwapLong(*((int *)names));

    if(nummappatches * 8 + 4 > lumpsize)
    {
        usermsg("\nError: PNAMES size %d smaller than expected %d\n", lumpsize, nummappatches * 8 + 4);
        efree(names);
        nummappatches = 0;
        return nullptr;
    }

    name_p      = names + 4;
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

            if(patchlookup[i] == -1 && devparm) // killough 8/8/98
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
    texturetranslation = emalloctag(int *, (texturecount + 1) * sizeof(*texturetranslation), PU_RENDERER, nullptr);

    for(int i = 0; i < texturecount; i++)
        texturetranslation[i] = i;
}

//=============================================================================
//
// Texture Hashing
//

static texturehash_t walltable;
static texturehash_t flattable;

//
// Returns true if this is marked as a TEXTURE1/TEXTURE2 duplicate. Needed to work like vanilla.
//
static bool R_checkTexLumpDup(const texture_t *srctexture, const texturehash_t &texlumpdups)
{
    const texture_t *next;
    for(const texture_t *texture = texlumpdups.keyIterator(nullptr, srctexture->name); texture; texture = next)
    {
        next = texlumpdups.keyIterator(texture, srctexture->name);
        if(texture == srctexture)
            return !!next; // the last one in the chain is the first found from TEXTUREx
    }
    return false; // if it's outside TEXTURE1/TEXTURE2, it's not in the control hash table.
}

//
// R_InitTextureHash
//
// This function now inits the two ehash tables and inserts the loaded textures
// into them. Also avoids the TEXTURE1/TEXTURE2 lump duplicates like vanilla.
//
static void R_InitTextureHash(const texturehash_t &texlumpdups)
{
    int i;

    walltable.destroy();
    flattable.destroy();

    // haleyjd 12/12/10: For efficiency, allocate as many chains as there are
    // entries, plus a few more for breathing room.
    walltable.initialize(wallstop - wallstart + 31);
    flattable.initialize(flatstop - flatstart + 31);

    // NOTE: vanilla DOOM rules that if there are duplicate texture names in the TEXTURE* lumps, only
    // the first texture is used. So don't add more textures if one is already there.
    for(i = wallstart; i < wallstop; i++)
        if(!R_checkTexLumpDup(textures[i], texlumpdups))
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
    const WadDirectory::namespace_t &ns = wGlobalDir.getNamespace(lumpinfo_t::ns_flats);

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
    byte         flatsize;
    int16_t      width, height;
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

        tex = textures[i + flatstart] = R_AllocTexStruct(lump->name, width, height, 1);

        tex->flatsize             = flatsize;
        tex->index                = i + flatstart;
        tex->components[0].lump   = i + firstflat;
        tex->components[0].type   = TC_FLAT;
        tex->components[0].width  = width;
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

    // load PNAMES
    int  nummappatches;
    int *patchlookup = R_LoadPNames(nummappatches);

    // Load the map texture definitions from textures.lmp.
    // The data is contained in one or two lumps,
    //  TEXTURE1 for shareware, plus TEXTURE2 for commercial.
    texturelump_t *maptex1 = R_InitTextureLump("TEXTURE1");
    texturelump_t *maptex2 = R_InitTextureLump("TEXTURE2");

    // calculate total textures before ns_textures namespace
    numwalls = maptex1->numtextures + maptex2->numtextures;

    // if there are no TEXTURE1/2 lookups, we need to create a dummy texture
    bool needDummy = false;
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
    flatstop  = flatstart + numflats;

    // SoM: Add one more for the missing texture texture
    texturecount = numwalls + numflats + 1;

    // Allocate textures
    textures = emalloctag(texture_t **, sizeof(texture_t *) * texturecount, PU_RENDERER, nullptr);
    memset(textures, 0, sizeof(texture_t *) * texturecount);

    // init lookup tables
    R_InitTranslationLUT();

    // initialize loading dots / bar
    R_InitLoading();

    // detect texture formats
    R_DetectTextureFormat(maptex1);
    R_DetectTextureFormat(maptex2);

    // if we need a dummy texture, add it now.
    int texnum = 0;
    if(needDummy)
    {
        R_MakeDummyTexture();
        ++texnum;
    }

    // Keep track of duplicate textures
    texturehash_t duptable;
    duptable.initialize(wallstop - wallstart + 31);

    // read texture lumps
    int errors = 0;
    texnum     = R_ReadTextureLump(maptex1, patchlookup, nummappatches, texnum, &errors, duptable);
    texnum     = R_ReadTextureLump(maptex2, patchlookup, nummappatches, texnum, &errors, duptable);
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
    for(int i = wallstart; i < wallstop; i++)
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
    R_InitTextureHash(duptable);
    duptable.destroy();
}

//=============================================================================
//
// Texture/Flat Lookup
//

static int  R_Doom1Texture(const char *name);
const char *level_error = nullptr;

//
// R_GetRawColumn
//
const byte *R_GetRawColumn(ZoneHeap &heap, int tex, int32_t col)
{
    const texture_t *t = textures[tex];

    // haleyjd 05/28/14: support non-power-of-two widths
    if(t->flags & TF_WIDTHNP2)
        col = M_PositiveModPositiveRight(col, t->width) * t->height;
    else
        col = (col & t->widthmask) * t->height;

    if(!t->bufferalloc)
        I_Error("R_GetRawColumn: Texture %s not already cached\n", t->name);

    // Lee Killough, eat your heart out! ... well this isn't really THAT bad...
    return (t->flags & TF_SWIRLY) ? R_DistortedFlat(heap, tex) + col : t->bufferdata + col;
}

//
// R_GetMaskedColumn
//
const texcol_t *R_GetMaskedColumn(int tex, int32_t col)
{
    const texture_t *t = textures[tex];

    if(!t->bufferalloc)
        I_Error("R_GetMaskedColumn: Texture %s not already cached\n", t->name);

    // haleyjd 05/28/14: support non-power-of-two widths
    return t->columns[(t->flags & TF_WIDTHNP2) ? M_PositiveModPositiveRight(col, t->width) : col & t->widthmask];
}

//
// R_GetLinearBuffer
//
const byte *R_GetLinearBuffer(int tex)
{
    const texture_t *t = textures[tex];

    if(!t->bufferalloc)
        R_CacheTexture(tex);

    return t->bufferdata;
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
int R_FindFlat(const char *name) // killough -- const added
{
    texture_t *tex;

    // Search flats first
    if(!(tex = R_SearchFlats(name)))
        tex = R_SearchWalls(name);

    // SoM: Check here for flat-ness!
    if(tex && !(tex->flags & TF_CANBEFLAT))
        tex = nullptr;

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

    if(*name == '-') // "NoTexture" marker.
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
int R_FindWall(const char *name) // const added -- killough
{
    int i = R_CheckForWall(name);

    if(i == -1)
    {
        i = R_Doom1Texture(name); // try doom I textures

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

struct doom1text_t
{
    char doom1[9];
    char doom2[9];
};

doom1text_t *txtrconv;
int          numconvs      = 0;
int          numconvsalloc = 0;

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
    int   lumplen, lumpnum;
    int   state = D1_STATE_SCAN;
    int   tx1, tx2;

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

    rover    = lump;
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
                state           = D1_STATE_TEX1;
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
            if(*rover == 0x0D || *rover == 0x0A || *(rover + 1) == '\0')
            {
                // haleyjd 09/07/05: this was wasting tons of memory before with
                // lots of little mallocs; make the whole thing dynamic
                if(numconvs >= numconvsalloc)
                {
                    numconvsalloc = numconvsalloc ? numconvsalloc * 2 : 56;
                    txtrconv      = erealloc(doom1text_t *, txtrconv, numconvsalloc * sizeof *txtrconv);
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
        if(!strncasecmp(name, txtrconv[i].doom1, 8)) // found it
        {
            return R_CheckForWall(txtrconv[i].doom2);
        }
    }

    return -1;
}

// EOF

