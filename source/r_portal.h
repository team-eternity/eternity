// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3: 
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

#include "doomdef.h"

class  Mobj;
struct planehash_t;

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


// These are flags used to represent configurable options for portals
typedef enum
{
   // -- Portal behavior flags --
   // Portal is completely disabled
   PF_DISABLED           = 0x001,
   // Portal does not render
   PF_NORENDER           = 0x002,
   // Portal does not allow passage
   PF_NOPASS             = 0x004,
   // Portal does not allow recursive sound to pass through
   PF_BLOCKSOUND         = 0x008,
   // Mask for the flags portion
   PF_FLAGMASK           = PF_DISABLED | PF_NORENDER | PF_NOPASS | PF_BLOCKSOUND,
   
   // -- Overlay flags --
   // Only used per-surface and indicate various overlay options for the portal
   // Portal has a blended texture overlay (alpha is default)
   PS_OVERLAY            = 0x010,
   // Overlay uses additive blending (must be used with PS_OVERLAY)
   PS_ADDITIVE           = 0x020,
   // Mask for overlay blending flags
   PS_OBLENDFLAGS        = PS_OVERLAY | PS_ADDITIVE,
   // Surface uses the global texture in the portal struct
   PS_USEGLOBALTEX       = 0x040,
   // Mask for all overlay flags
   PS_OVERLAYFLAGS       = PS_OBLENDFLAGS | PS_USEGLOBALTEX,
   
   // -- State flags --
   // These are only used per-surface and indicate the current state of the portal
  
   // Portal can be rendered
   PS_VISIBLE            = 0x080,
   // Portal can be passed through
   PS_PASSABLE           = 0x100,
   // Portal allows recursive sound
   PS_PASSSOUND          = 0x200,
   // Mask for state flags
   PS_STATEMASK          = PS_VISIBLE | PS_PASSABLE | PS_PASSSOUND,
   
   // -- Opactiy -- 
   // The left-most 8 bits are reserved for the opacity value of the portal overlay
   PO_OPACITYSHIFT       = 24,
   PO_OPACITYMASK        = 0xFF000000,
   
   // All overlay and blending flags
   PS_BLENDFLAGS         = PS_OVERLAYFLAGS | PO_OPACITYMASK,
} portalflag_e;


// Contains information representing a link from one portal group to another
typedef struct linkdata_s
{
   // SoM: linked portals are similar to anchored portals
   fixed_t   deltax, deltay, deltaz;
   fixed_t   planez;
   
   // gromid is the group where the portal actually is, toid is the group on 
   // the 'other side' of the portal.
   int       fromid, toid;
      
   // These are for debug purposes (so mappers can find the portats 
   // causing problems)
   int       maker, anchor;
} linkdata_t;


// Represents the information needed for an anchored portal
typedef struct anchordata_s
{
   fixed_t   deltax, deltay, deltaz;
   
   // These are for debug purposes (so mappers can find the portats 
   // causing problems)
   int       maker, anchor;
} anchordata_t;


// Represents the data needed for a horizon portal
typedef struct horizondata_s
{
   int     *floorpic, *ceilingpic;
   fixed_t *floorz, *ceilingz;
   int16_t *floorlight, *ceilinglight;
   fixed_t *floorxoff, *flooryoff;
   fixed_t *ceilingxoff, *ceilingyoff;
   float   *floorbaseangle, *floorangle;     // haleyjd 01/05/08: flat angles
   float   *ceilingbaseangle, *ceilingangle;
} horizondata_t;


// The data needed for a skyplane portal
typedef struct skyplanedata_s
{
   int     *pic;
   fixed_t *delta;
   int16_t *lightlevel;
   fixed_t *xoff, *yoff;
   float   *baseangle, *angle; // haleyjd 01/05/08: angles
} skyplanedata_t;


// The portal struct. This is what is assigned to sectors and can represent any
// kind of portal.
struct portal_t
{
   rportaltype_e type;

   union portaldata_u
   {
      skyplanedata_t plane;
      horizondata_t  horizon;
      anchordata_t   anchor;
      linkdata_t     link;
      Mobj         *camera;
   } data;

   // See: portalflag_e
   int    flags;
   
   // Planes that makeup a blended overlay
   int          globaltex;
   planehash_t *poverlay;

   portal_t *next;

   // haleyjd: temporary debug
   int16_t tainted;
};

portal_t *R_GetSkyBoxPortal(Mobj *camera);
portal_t *R_GetAnchoredPortal(int markerlinenum, int anchorlinenum);
portal_t *R_GetTwoWayPortal(int markerlinenum, int anchorlinenum);

portal_t *R_GetHorizonPortal(int *floorpic, int *ceilingpic, 
                             fixed_t *floorz, fixed_t *ceilingz, 
                             int16_t *floorlight, int16_t *ceilinglight, 
                             fixed_t *floorxoff, fixed_t *flooryoff, 
                             fixed_t *ceilingxoff, fixed_t *ceilingyoff,
                             float *floorbaseangle, float *floorangle,
                             float *ceilingbaseangle, float *ceilingangle);

portal_t *R_GetPlanePortal(int *pic, fixed_t *delta, int16_t *lightlevel, 
                           fixed_t *xoff, fixed_t *yoff, float *baseangle,
                           float *angle);

void R_ClearPortals(void);
void R_RenderPortals(void);

portal_t *R_GetLinkedPortal(int markerlinenum, int anchorlinenum, 
                            fixed_t planez, int fromid, int toid);

//=============================================================================
//
// Portal windows
//
// A portal window represents the screen reigon through which the player is 
// 'looking' at the portal.
//

typedef enum
{
   pw_floor,
   pw_ceiling,
   pw_line
} pwindowtype_e;

struct line_t;
struct pwindow_t;	// prototype to shut up gcc warnings
typedef void (*R_WindowFunc)(pwindow_t *);
typedef void (*R_ClipSegFunc)();

extern R_ClipSegFunc segclipfuncs[];

// SoM: TODO: Overlays go in here.
struct pwindow_t
{
   portal_t *portal;
   line_t *line;
   pwindowtype_e type;

   fixed_t  vx, vy, vz;

   float top[MAX_SCREENWIDTH];
   float bottom[MAX_SCREENWIDTH];
   int minx, maxx;

   R_WindowFunc func;
   R_ClipSegFunc clipfunc;

   // Next window in the main chain
   pwindow_t *next;
   
   // Families of windows. Head is the main window, and child is the next
   // child down the chain.
   pwindow_t *head, *child;
};

// SoM: Cardboard
void R_WindowAdd(pwindow_t *window, int x, float ytop, float ybottom);

pwindow_t *R_GetFloorPortalWindow(portal_t *portal);
pwindow_t *R_GetCeilingPortalWindow(portal_t *portal);
pwindow_t *R_GetLinePortalWindow(portal_t *portal, line_t *line);


// SoM 3/14/2004: flag if we are rendering portals.
typedef struct portalrender_s
{
   bool  active;
   int   minx, maxx;
   float miny, maxy;

   pwindow_t *w;

   void (*segClipFunc)();
   
   planehash_t *overlay;
} portalrender_t;

extern portalrender_t  portalrender;
#endif

//----------------------------------------------------------------------------
//
// $Log: r_portals.h,v $
//
//----------------------------------------------------------------------------
