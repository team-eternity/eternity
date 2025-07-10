//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: Patch format verification and load-time processing code.
// Authors: James Haley, Alison Gray Watson
//

#include "z_zone.h"

#include "d_gi.h"
#include "m_swap.h"
#include "r_patch.h"
#include "v_patch.h"
#include "v_patchfmt.h"
#include "v_png.h"
#include "z_auto.h"

// A global instance of PatchLoader for passing to WadDirectory methods
PatchLoader PatchLoader::patchFmt;

size_t PatchLoader::DefaultPatchSize;

//
// PatchLoader::GetDefaultPatch
//
// haleyjd 02/05/12: static, returns a default patch graphic to use in
// place of a missing patch. The graphic is allocated using the PU_PERMANENT
// zone tag (as of 06/09/12) so that client code cannot free or change the
// tag of the data even if it tries (which it generally will).
//
patch_t *PatchLoader::GetDefaultPatch()
{
    static patch_t *defaultPatch = nullptr;

    if(!defaultPatch)
    {
        byte patchdata[4];
        patchdata[0] = patchdata[3] = GameModeInfo->blackIndex;
        patchdata[1] = patchdata[2] = GameModeInfo->whiteIndex;
        defaultPatch                = V_LinearToPatch(patchdata, 2, 2, &DefaultPatchSize, PU_PERMANENT);
    }

    return defaultPatch;
}

//
// PatchLoader::checkData
//
// Check the format of patch_t data for validity
//
bool PatchLoader::checkData(void *data, size_t size) const
{
    // Must be at least as large as the header.
    if(size < 8)
        return false; // invalid header

    patch_t *patch  = static_cast<patch_t *>(data);
    short    width  = SwapShort(patch->width);
    short    height = SwapShort(patch->height);

    // Need valid width and height
    if(width < 0 || height < 0)
        return false; // invalid graphic size

    // Number of bytes needed for columnofs
    size_t numBytesNeeded = width * sizeof(int32_t);
    if(size - 8 < numBytesNeeded)
        return false; // invalid columnofs table size

    // Verify all columns
    for(int i = 0; i < width; i++)
    {
        size_t offset = static_cast<size_t>(SwapLong(patch->columnofs[i]));

        if(offset >= size)
            return false; // offset lies outside the data

        // Verify the series of posts at that offset
        byte *base  = reinterpret_cast<byte *>(patch);
        byte *rover = base + offset;
        while(*rover != 0xff)
        {
            byte *nextPost = rover + *(rover + 1) + 4;

            if(nextPost >= base + size)
                return false; // Unterminated series of posts, or too long

            rover = nextPost;
        }
    }

    // Patch is valid!
    return true;
}

static bool isLinearSize(size_t size, int &w, int &h)
{
    switch(size)
    {
    case 4096:
    case 4160: //
        w = h = 64;
        return true;
    case 8192: //
        w = 64;
        h = 128;
        return true;
    case 16384: //
        w = h = 128;
        return true;
    case 64000: //
        w = 320;
        h = 200;
        return true;
    case 65536: //
        w = h = 256;
        return true;
    case 262144: //
        w = h = 512;
        return true;
    default: //
        return false;
    }
}

//
// PatchLoader::verifyData
//
// Do formatting verification on a patch lump.
//
WadLumpLoader::Code PatchLoader::verifyData(lumpinfo_t *lump) const
{
    lumpinfo_t::lumpformat fmt = formatIndex();

    if(!checkData(lump->cache[fmt], lump->size))
    {
        // Maybe it's a PNG?
        if(lump->size > 8 && VPNGImage::CheckPNGFormat(lump->cache[fmt]))
        {
            int curTag = Z_CheckTag(lump->cache[fmt]);
            Z_Free(lump->cache[fmt]);
            VPNGImage::LoadAsPatch(lump->selfindex, curTag, &lump->cache[fmt]);
            if(lump->cache[fmt])
                return CODE_NOFMT;
        }

        // Maybe it's linear?
        int w = 0, h = 0;
        if(isLinearSize(lump->size, w, h))
        {
            int curTag = Z_CheckTag(lump->cache[fmt]);
            Z_ChangeTag(lump->cache[fmt], PU_STATIC);
            ZAutoBuffer buf;
            buf.alloc(lump->size, false);
            memcpy(buf.get(), lump->cache[fmt], lump->size);
            Z_Free(lump->cache[fmt]);
            V_LinearToPatch(buf.getAs<byte *>(), w, h, nullptr, curTag, &lump->cache[fmt]);
            if(lump->cache[fmt])
                return CODE_NOFMT;
        }

        // Return default patch.
        if(lump->cache[fmt])
            Z_Free(lump->cache[fmt]);
        lump->cache[fmt] = GetDefaultPatch();
        return CODE_NOFMT;
    }

    return CODE_OK;
}

//
// PatchLoader::formatRaw
//
// Format the patch_t header by swapping the short and long integer fields.
//
void PatchLoader::formatRaw(void *data) const
{
    patch_t *patch = static_cast<patch_t *>(data);

    patch->width      = SwapShort(patch->width);
    patch->height     = SwapShort(patch->height);
    patch->leftoffset = SwapShort(patch->leftoffset);
    patch->topoffset  = SwapShort(patch->topoffset);

    for(int i = 0; i < patch->width; i++)
        patch->columnofs[i] = SwapLong(patch->columnofs[i]);
}

//
// PatchLoader::formatData
//
// Call formatRaw on the lump's patch-format cache.
//
WadLumpLoader::Code PatchLoader::formatData(lumpinfo_t *lump) const
{
    formatRaw(lump->cache[formatIndex()]);
    return CODE_OK;
}

//
// PatchLoader::VerifyAndFormat
//
// Static utility for use outside of w_wad code; verifies the format without
// doing any possible substitutions (PNG -> patch, default patch, etc.) and
// then, if it's valid, formats it.
//
bool PatchLoader::VerifyAndFormat(void *data, size_t size)
{
    if(patchFmt.checkData(data, size))
    {
        patchFmt.formatRaw(data);
        return true;
    }
    return false;
}

//
// PatchLoader::CacheNum
//
// Static method to cache a lump as a patch_t, by number
//
patch_t *PatchLoader::CacheNum(WadDirectory &dir, int lumpnum, int tag)
{
    if(lumpnum >= 0)
        return static_cast<patch_t *>(dir.cacheLumpNum(lumpnum, tag, &patchFmt));
    else
        return GetDefaultPatch();
}

//
// PatchLoader::CacheName
//
// Static method to cache a lump as a patch_t, by name
//
patch_t *PatchLoader::CacheName(WadDirectory &dir, const char *name, int tag, int ns)
{
    return PatchLoader::CacheNum(dir, dir.checkNumForName(name, ns), tag);
}

//
// PatchLoader::GetUsedColors
//
// Utility method. Sets pal[index] to 1 for every color that is used in the
// given patch graphic. You should allocate and initialize pal as a 256-byte
// array before calling this; it won't be touched outside the used color
// indices, allowing multiple calls to this function to composite the range
// of used colors in multiple patches.
//
void PatchLoader::GetUsedColors(patch_t *patch, byte *pal)
{
    int16_t width = patch->width;

    for(int i = 0; i < width; i++)
    {
        size_t offset = static_cast<size_t>(patch->columnofs[i]);
        byte  *rover  = reinterpret_cast<byte *>(patch) + offset;

        while(*rover != 0xff)
        {
            int   count  = *(rover + 1);
            byte *source = rover + 3;

            while(count--)
                pal[*source++] = 1;

            rover = source + 1;
        }
    }
}

// EOF

