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
#include "m_swap.h"
#include "r_patch.h"
#include "v_patchfmt.h"

// A global instance of PatchLoader for passing to WadDirectory methods
PatchLoader PatchLoader::patchFmt;

//
// PatchLoader::verifyData
//
// Not implemented yet; should do formatting verification on a patch lump.
//
bool PatchLoader::verifyData(const void *date, size_t size) const
{
   return true;
}

//
// PatchLoader::formatData
//
// Format the patch_t header by swapping the short and long integer fields.
//
bool PatchLoader::formatData(void *data, size_t size) const
{
   patch_t *patch = (patch_t *)data;

   patch->width      = SwapShort(patch->width);
   patch->height     = SwapShort(patch->height);
   patch->leftoffset = SwapShort(patch->leftoffset);
   patch->topoffset  = SwapShort(patch->topoffset);

   for(int i = 0; i < patch->width; i++)
      patch->columnofs[i] = SwapLong(patch->columnofs[i]);

   return true;
}

//
// PatchLoader::getErrorMode
//
// For now, errors will be ignored, but this should be changed in the future
// as is appropriate to prevent crashes.
//
int PatchLoader::getErrorMode() const 
{
   return EM_IGNORE;
}

//
// PatchLoader::CacheName
//
// Static method to cache a lump as a patch_t
//
patch_t *PatchLoader::CacheName(WadDirectory &dir, const char *name, int tag)
{
   return (patch_t *)(dir.CacheLumpName(name, tag, &patchFmt));
}

patch_t *PatchLoader::CacheNum(WadDirectory &dir, int lumpnum, int tag)
{
   return (patch_t *)(dir.CacheLumpNum(lumpnum, tag, &patchFmt));
}

// EOF

