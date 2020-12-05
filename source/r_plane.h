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
//      Refresh, visplane stuff (floor, ceilings).
//
//-----------------------------------------------------------------------------

#ifndef R_PLANE_H__
#define R_PLANE_H__

struct cb_column_t;
struct cb_plane_t;
struct cbviewpoint_t;
struct contextbounds_t;
struct planecontext_t;
struct planehash_t;
struct rslope_t;
struct texture_t;
struct viewpoint_t;

// killough 10/98: special mask indicates sky flat comes from sidedef
#define PL_SKYFLAT (0x80000000)

// Visplane related.

// SoM 12/8/03
extern float *floorcliparray, *ceilingcliparray;

// SoM: We have to use secondary clipping arrays for portal overlays
extern float *overlayfclip, *overlaycclip;

void R_ClearPlanes(planecontext_t &context, const contextbounds_t &bounds);
void R_ClearOverlayClips(const contextbounds_t &bounds);
void R_DrawPlanes(planehash_t &mainhash, void (*&colfunc)(cb_column_t &),
                  int *const spanstart, const angle_t viewangle, planehash_t *table);

// Planehash stuff
planehash_t *R_NewPlaneHash(int chaincount);
void R_ClearPlaneHash(visplane_t **&freehead, planehash_t *table);


visplane_t *R_FindPlane(planecontext_t &context,
                        const viewpoint_t &viewpoint,
                        const cbviewpoint_t &cb_viewpoint,
                        const contextbounds_t &bounds,
                        fixed_t height,
                        int picnum,
                        int lightlevel,
                        v2fixed_t offs,       // killough 2/28/98: add x-y offsets
                        v2float_t scale,
                        float angle,         // haleyjd 01/08/05: add angle
                        pslope_t *slope,     // SoM: slopes
                        int blendflags,      // SoM: Blending flags for the plane
                        byte opacity,        // SoM: Opacity for translucent planes
                        planehash_t *table); // SoM: Table. Can be nullptr

visplane_t *R_DupPlane(planecontext_t &context, const visplane_t *pl, int start, int stop);
visplane_t *R_CheckPlane(planecontext_t &context, visplane_t *pl, int start, int stop);

bool R_CompareSlopes(const pslope_t *s1, const pslope_t *s2);

struct cb_span_t
{
   int x1, x2, y;
   unsigned xfrac, yfrac, xstep, ystep;
   void *source;
   lighttable_t *colormap;
   unsigned int *fg2rgb, *bg2rgb; // haleyjd 06/20/08: tl lookups

   // SoM: some values for the generalizede span drawers
   unsigned int xshift, xmask, yshift, ymask;

   // ioanch: more ptrs
   const void *alphamask;  // pointer to alphamask if applicable
};

struct cb_slopespan_t
{
   int y, x1, x2;

   double iufrac, ivfrac, idfrac;
   double iustep, ivstep, idstep;

   void *source;

   static inline lighttable_t **colormap;
};

using R_FlatFunc  = void (*)(const cb_span_t &);
using R_SlopeFunc = void (*)(const cb_slopespan_t &, const cb_span_t &);
using R_MapFunc   = void (*)(const R_FlatFunc, const R_SlopeFunc, cb_span_t &,
                             cb_slopespan_t &, const cb_plane_t &, int, int, int);

struct cb_plane_t
{
   float xoffset, yoffset;
   float xscale, yscale;
   float height;
   float pviewx, pviewy, pviewz, pviewsin, pviewcos;
   int   picnum;

   // SoM: we use different fixed point numbers for different flat sizes
   float fixedunitx, fixedunity;
   
   int lightlevel;
   float startmap;
   lighttable_t **planezlight;
   lighttable_t *colormap;
   lighttable_t *fixedcolormap;

   // SoM: Texture that covers the plane
   texture_t *tex;
   void      *source;

   // SoM: slopes.
   rslope_t *slope;

   R_MapFunc MapFunc;
};


planehash_t *R_NewOverlaySet(planecontext_t &context);
void R_FreeOverlaySet(planehash_t *&r_overlayfreesets, planehash_t *set);
void R_MapInitOverlaySets();

#endif

//----------------------------------------------------------------------------
//
// $Log: r_plane.h,v $
// Revision 1.6  1998/04/27  01:48:34  killough
// Program beautification
//
// Revision 1.5  1998/03/02  11:47:16  killough
// Add support for general flats xy offsets
//
// Revision 1.4  1998/02/09  03:16:06  killough
// Change arrays to use MAX height/width
//
// Revision 1.3  1998/02/02  14:20:45  killough
// Made some functions static
//
// Revision 1.2  1998/01/26  19:27:42  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
