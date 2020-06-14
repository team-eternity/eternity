// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Stephen McGranahan et al.
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
//      Creating, managing and rendering portals.
//      SoM created 12/8/03
//
//-----------------------------------------------------------------------------

#ifndef R_PORTALS_H__
#define R_PORTALS_H__

#include "doomdef.h"
#include "p_maputl.h"
#include "r_defs.h"

#define SECTOR_PORTAL_LOOP_PROTECTION 128

struct line_t;
class  Mobj;
struct planehash_t;
struct pwindow_t;
struct sectorbox_t;

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

   // More flags added along...
   PF_ATTACHEDPORTAL     = 0x400,

   // Mask for the flags portion
   PF_FLAGMASK           = PF_DISABLED | PF_NORENDER | PF_NOPASS | PF_BLOCKSOUND
   | PF_ATTACHEDPORTAL,


   // -- Opactiy --
   // The left-most 8 bits are reserved for the opacity value of the portal overlay
   PO_OPACITYSHIFT       = 24,
   PO_OPACITYMASK        = 0xFF000000,
   
   // All overlay and blending flags
   PS_BLENDFLAGS         = PS_OVERLAYFLAGS | PO_OPACITYMASK,
} portalflag_e;


// Contains information representing a link from one portal group to another
struct linkdata_t
{
   // SoM: linked portals are similar to anchored portals
   fixed_t   deltax, deltay, deltaz;
   fixed_t   planez;
   
   // fromid is the group where the portal actually is, toid is the group on 
   // the 'other side' of the portal.
   int       fromid, toid;
      
   // These are for debug purposes (so mappers can find the portats 
   // causing problems)
   int       maker, anchor;

   // ioanch 20160226: access the partner portal (if any) in case of polyobject
   // cars
   portal_t *polyportalpartner;

   inline bool deltaEquals(const linkdata_t &data) const
   {
      return deltax == data.deltax && deltay == data.deltay &&
             deltaz == data.deltaz;
   }
};

struct portaltransform_t
{
   double rot[2][2];
   v3double_t move;   // TODO: z offset
   double angle;

   const line_t *sourceline;
   const line_t *targetline;   // for rotating anchored portals
   bool flipped;
   fixed_t zoffset;
   
   void applyTo(fixed_t &x, fixed_t &y,
      float *fx = nullptr, float *fy = nullptr, bool nomove = false) const;
   void applyTo(float &x, float &y, bool nomove = false) const;

   void updateFromLines(bool allowrotate);
};

// Represents the information needed for an anchored portal
struct anchordata_t
{
   // affine 3D transform. Last row is omitted. Includes translate(x, y, z) and
   // rotation around the Z axis
   portaltransform_t transform;
   // ioanch 20160226: access the partner portal (if any) in case of polyobject
   // cars
   portal_t *polyportalpartner;

   // These are for debug purposes (so mappers can find the portats 
   // causing problems)
   int       maker, anchor;
};


// Represents the data needed for a horizon portal's surface (floor and ceiling)
struct horizondata_t
{
   int *pic;
   fixed_t *z;
   int16_t *light;
   v2fixed_t *off;
   float *baseangle;
   float *angle; // haleyjd 01/05/08: flat angles
   const v2float_t *scale;
};


// The data needed for a skyplane portal
struct skyplanedata_t
{
   int     *pic;
   fixed_t *delta;
   int16_t *lightlevel;
   fixed_t *xoff, *yoff;
   float   *baseangle, *angle; // haleyjd 01/05/08: angles
   const float *xscale, *yscale;
};


// The portal struct. This is what is assigned to sectors and can represent any
// kind of portal.
struct portal_t
{
   rportaltype_e type;

   union portaldata_u
   {
      skyplanedata_t plane;
      horizondata_t  horizon[surf_NUM];
      anchordata_t   anchor;
      linkdata_t     link;
      Mobj          *camera;
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

//
// Nicer way to determine if a portal has an anchor
//
inline static bool R_portalIsAnchored(const portal_t *portal)
{
   return portal->type == R_ANCHORED || portal->type == R_TWOWAY || 
      portal->type == R_LINKED;
}

const portal_t *R_GetPortalHead();

portal_t *R_GetSkyBoxPortal(Mobj *camera);
portal_t *R_GetAnchoredPortal(int markerlinenum, int anchorlinenum,
   bool allowrotate, bool flipped, fixed_t zoffset);
portal_t *R_GetTwoWayPortal(int markerlinenum, int anchorlinenum, 
   bool allowrotate, bool flipped, fixed_t zoffset);

portal_t *R_GetHorizonPortal(int *floorpic, int *ceilingpic,
                             fixed_t *floorz, fixed_t *ceilingz,
                             int16_t *floorlight, int16_t *ceilinglight,
                             v2fixed_t *flooroff, v2fixed_t *ceilingoff,
                             float *floorbaseangle, float *floorangle,
                             float *ceilingbaseangle, float *ceilingangle,
                             const v2float_t *floorscale, const v2float_t *ceilingscale);

portal_t *R_GetPlanePortal(int *pic, fixed_t *delta, int16_t *lightlevel, 
                           fixed_t *xoff, fixed_t *yoff, float *baseangle,
                           float *angle, const float *xscale, const float *yscale);

void R_MovePortalOverlayToWindow(bool isceiling);
void R_ClearPortals();
void R_RenderPortals();

portal_t *R_GetLinkedPortal(int markerlinenum, int anchorlinenum, 
                            fixed_t planez, int fromid, int toid);

void R_CalcRenderBarrier(pwindow_t &window, const sectorbox_t &box);

bool R_IsSkyLikePortalCeiling(const sector_t &sector);
bool R_IsSkyLikePortalFloor(const sector_t &sector);
bool R_IsSkyWall(const line_t &line);

//=============================================================================
//
// Spawning portals from advanced specials (not in p_spec.cpp)
//

void R_SpawnQuickLinePortal(line_t &line);
void R_DefinePortal(const line_t &line);
void R_ApplyPortals(sector_t &sector, int portalceiling, int portalfloor);
void R_ApplyPortal(line_t &line, int portal);

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

typedef void (*R_WindowFunc)(pwindow_t *);
typedef void (*R_ClipSegFunc)();

extern R_ClipSegFunc segclipfuncs[];

//
// Render barrier: used by anchored portals to mark limits for rendering
// geometry and sprites.
//
struct renderbarrier_t
{
   // Selection depends on context
   union
   {
      dlnormal_t dln;
      fixed_t bbox[4];  // for sector portals (very rough, won't cover all cases)
   };
};

// SoM: TODO: Overlays go in here.
struct pwindow_t
{
   portal_t *portal;
   line_t *line;
   // rendering barrier: blocks unwanted objects from showing
   renderbarrier_t barrier;
   pwindowtype_e type;

   fixed_t planez;   // if line == nullptr, this is the sector portal plane z

   fixed_t  vx, vy, vz;
   angle_t  vangle;

   float *top;
   float *bottom;
   int minx, maxx;

   R_WindowFunc func;
   R_ClipSegFunc clipfunc;

   // Next window in the main chain
   pwindow_t *next;
   
   // Families of windows. Head is the main window, and child is the next
   // child down the chain.
   pwindow_t *head, *child;

   planehash_t *poverlay;  // Portal overlays are now stored per window
};

// SoM: Cardboard
void R_WindowAdd(pwindow_t *window, int x, float ytop, float ybottom);

pwindow_t *R_GetFloorPortalWindow(portal_t *portal, fixed_t planez);
pwindow_t *R_GetCeilingPortalWindow(portal_t *portal, fixed_t planez);
pwindow_t *R_GetLinePortalWindow(portal_t *portal, line_t *line);


// SoM 3/14/2004: flag if we are rendering portals.
struct portalrender_t
{
   bool  active;
   int   minx, maxx;
   float miny, maxy;

   pwindow_t *w;

   void (*segClipFunc)();

//   planehash_t *overlay;
};

extern portalrender_t  portalrender;
#endif

//----------------------------------------------------------------------------
//
// $Log: r_portals.h,v $
//
//----------------------------------------------------------------------------
