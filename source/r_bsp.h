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
//      Refresh module, BSP traversal and handling.
//
//-----------------------------------------------------------------------------

#ifndef R_BSP_H__
#define R_BSP_H__

struct windowlinegen_t;
struct drawseg_t;
struct line_t;
struct seg_t;
struct sector_t;
struct side_t;
struct bspcontext_t;
struct contextbounds_t;
struct portalrender_t;
struct cbviewpoint_t;
struct rendercontext_t;
struct rpolynode_t;

// SoM: mark a range of the screen as being solid (closed).
// these marks are then added to the solidsegs list by R_addLine after all segments
// of the line are rendered and the solidsegs array isn't being traversed.. >_<
void R_MarkSolidSeg(bspcontext_t &context, int x1, int x2);

bool R_SetupPortalClipsegs(bspcontext_t &context, const contextbounds_t &bounds,
                           portalrender_t &portalrender,
                           int minx, int maxx, const float *top, const float *bottom);

void R_SetupSolidSegs();
void R_ClearClipSegs(bspcontext_t &context, const contextbounds_t &bounds);
void R_ClearDrawSegs(bspcontext_t &context);

void R_ForEachPolyNode(void (*func)(rpolynode_t *bsp));

void R_PreRenderBSP();
void R_RenderBSPNode(rendercontext_t &context, int bspnum);

// killough 4/13/98: fake floors/ceilings for deep water / fake ceilings:
int R_GetSurfaceLightLevel(surf_e surf, const sector_t *sec);
const sector_t *R_FakeFlat(const fixed_t, const sector_t *, sector_t *, int *, int *, bool);
bool R_PickNearestBoxLines(const cbviewpoint_t &cb_viewpoint,
                           const float fbox[4], windowlinegen_t &linegen1,
                           windowlinegen_t &linegen2, slopetype_t *slope = nullptr);

#endif

//----------------------------------------------------------------------------
//
// $Log: r_bsp.h,v $
// Revision 1.5  1998/05/03  22:48:03  killough
// beautification, use new headers, change decls
//
// Revision 1.4  1998/04/14  08:16:15  killough
// Fix light levels on 2s textures
//
// Revision 1.3  1998/02/02  13:31:53  killough
// Add HOM detector
//
// Revision 1.2  1998/01/26  19:27:33  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:10  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
