// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//      Refresh module, BSP traversal and handling.
//
//-----------------------------------------------------------------------------

#ifndef __R_BSP__
#define __R_BSP__

extern seg_t    *curline;
extern side_t   *sidedef;
extern line_t   *linedef;
extern sector_t *frontsector;
extern sector_t *backsector;

struct drawseg_t;

// old code -- killough:
// extern drawseg_t drawsegs[MAXDRAWSEGS];
// new code -- killough:
extern drawseg_t *drawsegs;
extern unsigned int maxdrawsegs;

extern drawseg_t *ds_p;

// SoM: mark a range of the screen as being solid (closed).
// these marks are then added to the solidsegs list by R_AddLine after all segments
// of the line are rendered and the solidsegs array isn't being traversed.. >_<
void R_MarkSolidSeg(int x1, int x2);

bool R_SetupPortalClipsegs(int minx, int maxx, float *top, float *bottom);

void R_ClearClipSegs(void);
void R_ClearDrawSegs(void);

// SoM: This is called by portal rendering functions to clear the array that marks 
// the tops of slopes so the clipsegtoportal functions show all the slope that's 
// in the window.
void R_ClearSlopeMark(int minx, int maxx, pwindowtype_e type);

void R_RenderBSPNode(int bspnum);
int R_DoorClosed(void);   // killough 1/17/98

// killough 4/13/98: fake floors/ceilings for deep water / fake ceilings:
sector_t *R_FakeFlat(sector_t *, sector_t *, int *, int *, bool);

extern int detaillevel;

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
