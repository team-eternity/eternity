//
// The Eternity Engine
// Copyright(C) 2020 James Haley, Max Waine, et al.
// Copyright(C) 2020 Ethan Watson
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
//----------------------------------------------------------------------------
//
// Purpose: Renderer context
// Some code is derived from Rum & Raisin Doom, Ethan Watson, used under
// terms of the GPLv3.
//
// Authors: Max Waine
//

#ifndef R_CONTEXT_H__
#define R_CONTEXT_H__

#include "r_defs.h"

struct cb_column_t;
struct cliprange_t;
struct pwindow_t;

struct rendercontext_t
{
   int bufferindex;
   int startcolumn, endcolumn; // for(int x = startcolumn; x < endcolumn; x++)
   int numcolumns; // cached endcolumn - startcolumn

   // r_bsp.cpp
   // newend is one past the last valid seg
   cliprange_t *newend;
   cliprange_t *solidsegs;

   // addend is one past the last valid added seg.
   cliprange_t *addedsegs;
   cliprange_t *addend;

   float *slopemark;


   // r_plane.cpp
   visplane_t *floorplane, *ceilingplane;
   visplane_t *freetail;
   visplane_t **freehead = &freetail;

   // SoM: New visplane hash
   // This is the main hash object used by the normal scene.
   visplane_t **mainchains;
   planehash_t  mainhash;

   // Free list of overlay portals. Used by portal windows and the post-BSP stack.
   planehash_t *r_overlayfreesets;


   // r_portal.cpp
   pwindow_t *unusedhead, *windowhead, *windowlast;


   // Currently uncategorised
   void (*colfunc)(cb_column_t &);
   void (*flatfunc)();
};

inline int r_numcontexts = 0;

rendercontext_t &R_GetContext(int context);

void R_InitContexts(const int width);

template<typename F>
void R_ForEachContext(F &&f)
{
   for(int i = 0; i < r_numcontexts; i++)
      f(R_GetContext(i));
}

#endif

// EOF
