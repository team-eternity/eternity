// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2004 Stephen McGranahan
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
//      Creating, managing and rendering portals.
//      SoM created 12/8/03
//
//-----------------------------------------------------------------------------

#ifndef R_PORTALS_H__
#define R_PORTALS_H__

typedef enum
{
   R_NONE,
   R_SKYBOX,
   R_ANCHORED,
   R_HORIZON,
   R_PLANE,
   R_TWOWAY, // SoM: two-way non-linked anchored portals
   R_LINKED, // SoM: interactive portals  
} rportaltype_e;



typedef struct
{
   mobj_t    *mobj;
   fixed_t   deltax, deltay, deltaz;
#ifdef R_LINKEDPORTALS
   int       groupid; // SoM: linked portals are camera portals
   fixed_t   planez;
#endif
} cameraportal_t;




typedef struct
{
   short   *floorpic, *ceilingpic;
   fixed_t *floorz, *ceilingz;
   short   *floorlight, *ceilinglight;
   fixed_t *floorxoff, *flooryoff;
   fixed_t *ceilingxoff, *ceilingyoff;
   float   *floorbaseangle, *floorangle;     // haleyjd 01/05/08: flat angles
   float   *ceilingbaseangle, *ceilingangle;
} horizonportal_t;




typedef struct
{
   short   *pic;
   fixed_t *delta;
   short   *lightlevel;
   fixed_t *xoff, *yoff;
   float   *baseangle, *angle; // haleyjd 01/05/08: angles
} skyplaneportal_t;




typedef struct portal_s
{
   rportaltype_e type;

   union portaldata_u
   {
      cameraportal_t   camera;
      horizonportal_t  horizon;
      skyplaneportal_t plane;
   } data;

   struct portal_s *next;

   // haleyjd: temporary debug
   short tainted;
} portal_t;




portal_t *R_GetSkyBoxPortal(mobj_t *camera);
portal_t *R_GetAnchoredPortal(fixed_t deltax, fixed_t deltay, fixed_t deltaz);
portal_t *R_GetTwoWayPortal(fixed_t deltax, fixed_t deltay, fixed_t deltaz);

portal_t *R_GetHorizonPortal(short *floorpic, short *ceilingpic, 
                             fixed_t *floorz, fixed_t *ceilingz, 
                             short *floorlight, short *ceilinglight, 
                             fixed_t *floorxoff, fixed_t *flooryoff, 
                             fixed_t *ceilingxoff, fixed_t *ceilingyoff,
                             float *floorbaseangle, float *floorangle,
                             float *ceilingbaseangle, float *ceilingangle);

portal_t *R_GetPlanePortal(short *pic, fixed_t *delta, short *lightlevel, 
                           fixed_t *xoff, fixed_t *yoff, float *baseangle,
                           float *angle);

void R_ClearPortals(void);
void R_RenderPortals(void);


#ifdef R_LINKEDPORTALS
portal_t *R_GetLinkedPortal(fixed_t deltax, fixed_t deltay, fixed_t deltaz, 
                            fixed_t planez, int groupid);
#endif


typedef enum
{
   pw_floor,
   pw_ceiling,
   pw_line
} pwindowtype_e;

struct pwindow_s;	//prototype to shut up gcc warnings
typedef void (*R_WindowFunc)(struct pwindow_s *);

typedef struct pwindow_s
{
   portal_t *portal;
   struct line_s *line;
   pwindowtype_e type;

   fixed_t  vx, vy, vz;

   float top[MAX_SCREENWIDTH];
   float bottom[MAX_SCREENWIDTH];
   int minx, maxx;

   R_WindowFunc func;

   struct pwindow_s *next, *child;
} pwindow_t;

// SoM: Cardboard
void R_WindowAdd(pwindow_t *window, int x, float ytop, float ybottom);


pwindow_t *R_GetFloorPortalWindow(portal_t *portal);
pwindow_t *R_GetCeilingPortalWindow(portal_t *portal);
pwindow_t *R_GetLinePortalWindow(portal_t *portal, struct line_s *line);


// SoM 3/14/2004: flag if we are rendering portals.
typedef struct
{
   boolean active;
   int     minx, maxx;
   float   miny, maxy;

   pwindow_t *w;
} portalrender_t;

extern portalrender_t  portalrender;
#endif

//----------------------------------------------------------------------------
//
// $Log: r_portals.h,v $
//
//----------------------------------------------------------------------------
