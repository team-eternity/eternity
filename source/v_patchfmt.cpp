// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
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
//  Patch format verification and load-time processing code
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_gi.h"
#include "m_swap.h"
#include "r_patch.h"
#include "v_patch.h"
#include "v_patchfmt.h"
#include "v_png.h"

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
   static patch_t *defaultPatch = NULL;

   if(!defaultPatch)
   {
      byte patchdata[4];
      patchdata[0] = patchdata[3] = GameModeInfo->blackIndex;
      patchdata[1] = patchdata[2] = GameModeInfo->whiteIndex;
      defaultPatch = V_LinearToPatch(patchdata, 2, 2, &DefaultPatchSize, PU_PERMANENT);
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
   byte    *bp     = static_cast<byte    *>(data);
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
      column_t *col = reinterpret_cast<column_t *>(bp + offset);
      while(col->topdelta != 0xff)
      {
         byte *nextPost = reinterpret_cast<byte *>(col) + col->length + 4;

         if(nextPost >= bp + size)
            return false; // Unterminated series of posts, or too long

         col = reinterpret_cast<column_t *>(nextPost);
      }
   }

   // Patch is valid!
   return true;
}

//
// PatchLoader::verifyData
//
// Do formatting verification on a patch lump.
//
WadLumpLoader::Code PatchLoader::verifyData(lumpinfo_t *lump) const
{
   if(!checkData(lump->cache, lump->size))
   {
      // Maybe it's a PNG?
      if(lump->size > 8 && VPNGImage::CheckPNGFormat(lump->cache))
      {
         int curTag = Z_CheckTag(lump->cache);
         Z_Free(lump->cache);
         lump->cache = VPNGImage::LoadAsPatch(lump->selfindex, curTag, &lump->cache);
         if(lump->cache)
            return CODE_NOFMT;
      }

      // Return default patch.
      if(lump->cache)
         Z_Free(lump->cache);
      lump->cache = GetDefaultPatch();
      lump->size  = DefaultPatchSize;
      return CODE_NOFMT;
   }

   return CODE_OK;
}

//
// PatchLoader::formatData
//
// Format the patch_t header by swapping the short and long integer fields.
//
WadLumpLoader::Code PatchLoader::formatData(lumpinfo_t *lump) const
{
   patch_t *patch = static_cast<patch_t *>(lump->cache);

   patch->width      = SwapShort(patch->width);
   patch->height     = SwapShort(patch->height);
   patch->leftoffset = SwapShort(patch->leftoffset);
   patch->topoffset  = SwapShort(patch->topoffset);

   for(int i = 0; i < patch->width; i++)
      patch->columnofs[i] = SwapLong(patch->columnofs[i]);

   return CODE_OK;
}

//
// PatchLoader::CacheNum
//
// Static method to cache a lump as a patch_t, by number
//
patch_t *PatchLoader::CacheNum(WadDirectory &dir, int lumpnum, int tag)
{
   return static_cast<patch_t *>(dir.cacheLumpNum(lumpnum, tag, &patchFmt));
}

//
// PatchLoader::CacheName
//
// Static method to cache a lump as a patch_t, by name
//
patch_t *PatchLoader::CacheName(WadDirectory &dir, const char *name, int tag)
{
   int lumpnum;
   patch_t *ret;

   if((lumpnum = dir.checkNumForName(name)) >= 0)
      ret = PatchLoader::CacheNum(dir, lumpnum, tag);
   else
      ret = GetDefaultPatch();

   return ret;
}

// EOF

