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
// Some code is derived from Rum & Raisin Doom, by Ethan Watson, used under
// terms of the GPLv3.
//
// Authors: Max Waine
//

#ifndef R_CONTEXT_H__
#define R_CONTEXT_H__

#include "m_surf.h"
#include "r_defs.h"
#include "r_lighting.h"
#include "r_portal.h"

struct cliprange_t;
struct contextportalinfo_t;
struct drawseg_t;
struct drawsegs_xrange_t;
struct maskedrange_t;
struct poststack_t;
struct pwindow_t;
struct sectorbox_t;
struct vissprite_t;
class  ZoneHeap;

struct contextbounds_t
{
   int   startcolumn, endcolumn; // for(int x = startcolumn; x < endcolumn; x++)
   float fstartcolumn, fendcolumn;
   int   numcolumns; // cached endcolumn - startcolumn
};

struct bspcontext_t
{
   drawseg_t   *drawsegs;
   unsigned int maxdrawsegs;
   drawseg_t   *ds_p;

   // newend is one past the last valid seg
   cliprange_t *newend;
   cliprange_t *solidsegs;

   // addend is one past the last valid added seg.
   cliprange_t *addedsegs;
   cliprange_t *addend;

   float *slopemark;
};

struct cmapcontext_t
{
   // killough 3/20/98: Allow colormaps to be dynamic (e.g. underwater)
   lighttable_t *(*scalelight)[MAXLIGHTSCALE];
   lighttable_t *(*zlight)[MAXLIGHTZ];
   lighttable_t *fullcolormap;
   // killough 3/20/98, 4/4/98: end dynamic colormaps

   lighttable_t *fixedcolormap;
};

struct planecontext_t
{
   visplane_t  *floorplane, *ceilingplane;
   visplane_t  *freetail;
   visplane_t **freehead;

   // SoM: New visplane hash
   // This is the main hash object used by the normal scene.
   visplane_t **mainchains;
   planehash_t  mainhash;

   // Free list of overlay portals. Used by portal windows and the post-BSP stack.
   planehash_t *r_overlayfreesets;

   float *openings, *lastopening;

   // SoM 12/8/03: floorclip and ceilingclip changed to pointers so they can be set
   // to the clipping arrays of portals.
   float *floorclip, *ceilingclip;

   // spanstart holds the start of a plane span; initialized to 0 at start
   int *spanstart;
};

struct portalcontext_t
{
   uint16_t renderdepth; // THREAD_TODO: Rename this, it's a misnomer

   pwindow_t *unusedhead, *windowhead, *windowlast;

   sectorboxvisit_t *visitids; // THREAD_FIXME: Does this go here?

   int            numportalstates;
   portalstate_t *portalstates;

   // This flag is set when a portal is being rendered. This flag is checked in
   // r_bsp.c when rendering camera portals (skybox, anchored, linked) so that an
   // extra function (R_ClipSegToPortal) is called to prevent certain types of HOM
   // in portals.

   portalrender_t portalrender;
};

struct spritecontext_t
{
   // haleyjd 04/25/10: drawsegs optimization
   drawsegs_xrange_t *drawsegs_xrange;
   unsigned int       drawsegs_xrange_size;
   int                drawsegs_xrange_count;

   vissprite_t *vissprites, **vissprite_ptrs;  // killough
   size_t num_vissprite, num_vissprite_alloc, num_vissprite_ptrs;

   // SoM 12/13/03: the post-BSP stack
   poststack_t   *pstack;
   int            pstacksize;
   int            pstackmax;
   maskedrange_t *unusedmasked;

   bool *sectorvisited;
};

struct viewpoint_t
{
   fixed_t x, y, z;
   angle_t angle; // No pitch as portals don't modify it
   fixed_t sin, cos;
};

// Separate for now. Maybe merge later with viewpoint_t
struct cbviewpoint_t
{
   float x, y, z;
   float angle; // No pitch as portals don't modify it
   float sin, cos;
};

struct rendercontext_t
{
   int16_t         bufferindex;

   bspcontext_t    bspcontext;
   cmapcontext_t   cmapcontext;
   planecontext_t  planecontext;
   portalcontext_t portalcontext;
   spritecontext_t spritecontext;

   // Hi-ho, the mem'ry o!
   // The heap stands alone.
   ZoneHeap *heap;

   contextbounds_t bounds;
   viewpoint_t     view;
   cbviewpoint_t   cb_view;
};

// The global context is for single-threaded things that still require a context
// It doesn't contribute to r_numcontexts
inline rendercontext_t r_globalcontext;

inline int  r_numcontexts;
inline bool r_hascontexts;

rendercontext_t &R_GetContext(int context);
void R_FreeContexts();
void R_InitContexts(const int width);
void R_RefreshContexts();
void R_UpdateContextBounds();
void R_RunContexts();

template<typename F>
void R_ForEachContext(F &&f)
{
   if(!r_hascontexts)
      return;

   f(r_globalcontext);

   if(r_numcontexts > 1)
   {
      for(int i = 0; i < r_numcontexts; i++)
         f(R_GetContext(i));
   }
}

#endif

// EOF
