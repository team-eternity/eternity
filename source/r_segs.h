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
//------------------------------------------------------------------------------
//
// Purpose: Refresh module, drawing LineSegs from BSP.
// Authors: James Haley, Stephen McGranahan, Ioan Chera, Max Waine
//

#ifndef __R_SEGS__
#define __R_SEGS__

struct bspcontext_t;
struct cb_seg_t;
struct cbviewpoint_t;
struct cmapcontext_t;
struct contextbounds_t;
struct drawseg_t;
struct planecontext_t;
struct portalcontext_t;
struct viewpoint_t;
class ZoneHeap;

void R_FinishMappingLines();

void R_RenderMaskedSegRange(cmapcontext_t &cmapcontext, const viewpoint_t &viewpoint, drawseg_t *ds, int x1, int x2);
void R_StoreWallRange(bspcontext_t &bspcontext, cmapcontext_t &cmapcontext, planecontext_t &planecontext,
                      portalcontext_t &portalcontext, ZoneHeap &heap, const viewpoint_t &viewpoint,
                      const cbviewpoint_t &cb_viewpoint, const contextbounds_t &bounds, const cb_seg_t &seg,
                      const int start, const int stop);

fixed_t R_PointToDist2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);

//
// Masked 2s linedefs
//

struct drawseg_t
{
    const seg_t *curline;
    int          x1, x2;
    float        dist1, dist2, diststep;
    fixed_t      bsilheight; // do not clip sprites above this
    fixed_t      tsilheight; // do not clip sprites below this

    // sf: colormap to be used when drawing the drawseg
    // for coloured lighting
    const lighttable_t *const (*colormap)[MAXLIGHTSCALE];
    const lighttable_t       *fixedcolormap;

    // Pointers to lists for sprite clipping,
    // all three adjusted so [x1] is first value.
    const float *sprtopclip, *sprbottomclip;
    float       *maskedtexturecol;
    float       *maskedtextureskew;

    byte silhouette; // 0=none, 1=bottom, 2=top, 3=both

    fixed_t deltaz; // z offset if seen behind anchored portals
};

#endif

//----------------------------------------------------------------------------
//
// $Log: r_segs.h,v $
// Revision 1.5  1998/05/03  23:02:40  killough
// beautification, add R_StoreWallRange() decl
//
// Revision 1.4  1998/04/27  02:01:28  killough
// Program beautification
//
// Revision 1.3  1998/03/02  11:53:29  killough
// add scrolling walls
//
// Revision 1.2  1998/01/26  19:27:44  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
