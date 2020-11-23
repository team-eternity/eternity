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

struct cb_seg_t;
struct line_t;
class  Mobj;
struct planehash_t;
struct pwindow_t;
struct rendercontext_t;
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
   v3fixed_t delta;
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

// The portal struct. This is what is assigned to sectors and can represent any
// kind of portal.
struct portal_t
{
   rportaltype_e type;

   union portaldata_u
   {
      const sector_t *sector; // when a single sector reference is enough
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
   return portal->type == R_ANCHORED || portal->type == R_TWOWAY || portal->type == R_LINKED;
}

const portal_t *R_GetPortalHead();

portal_t *R_GetSkyBoxPortal(Mobj *camera);
portal_t *R_GetAnchoredPortal(int markerlinenum, int anchorlinenum,
   bool allowrotate, bool flipped, fixed_t zoffset);
portal_t *R_GetTwoWayPortal(int markerlinenum, int anchorlinenum, 
   bool allowrotate, bool flipped, fixed_t zoffset);

portal_t *R_GetHorizonPortal(const sector_t *sector);

portal_t *R_GetPlanePortal(const sector_t *sector);

void R_MovePortalOverlayToWindow(rendercontext_t &context, cb_seg_t &seg, surf_e surf);
void R_ClearPortals(rendercontext_t &context);
void R_RenderPortals(rendercontext_t &context);

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

enum pwindowtype_e
{
   pw_floor,
   pw_ceiling,
   pw_line
};

static const pwindowtype_e pw_surface[surf_NUM] = { pw_floor, pw_ceiling };

typedef void (*R_WindowFunc)(rendercontext_t &context, pwindow_t *);
typedef void (*R_ClipSegFunc)(rendercontext_t &context, const cb_seg_t &seg);

extern R_ClipSegFunc segclipfuncs[];

//
// Line portal window barrier generator. Starts based on source linedef and is shifted by each seg,
// if dented away from the line (can happen with dynasegs and result in infinite recursions or HOMs
// if not taken care of). It's always on the source viewer's space, not the portal target's space,
// unlike renderbarrier.
//
struct windowlinegen_t
{
   v2float_t start;  // start vertex
   v2float_t delta;  // delta to end vertex
   v2float_t normal; // line normal. 1 length.

   void makeFrom(const line_t *line)
   {
      start.x = line->v1->fx;
      start.y = line->v1->fy;
      delta.x = line->v2->fx - start.x;
      delta.y = line->v2->fy - start.y;
      normal.x = line->nx;
      normal.y = line->ny;
   }

   operator bool() const
   {
      return start || delta || normal;
   }
};

//
// Render barrier: used by anchored portals to mark limits for rendering
// geometry and sprites.
//
struct renderbarrier_t
{
   // Selection depends on context
   windowlinegen_t linegen;
   float fbox[4]; // for sector portals (very rough, won't cover all cases)
};

// SoM: TODO: Overlays go in here.
struct pwindow_t
{
   portal_t *portal;
   const line_t *line;
   windowlinegen_t linegen;   // Generator, prepared to be stored on barrier after transforming
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
void R_WindowAdd(rendercontext_t &context, pwindow_t *window, int x, float ytop, float ybottom);

pwindow_t *R_GetSectorPortalWindow(rendercontext_t &context, surf_e surf, const surface_t &surface);
pwindow_t *R_GetLinePortalWindow(rendercontext_t &context, portal_t *portal, const seg_t *seg);

// SoM 3/14/2004: flag if we are rendering portals.
struct portalrender_t
{
   bool  active;
   int   minx, maxx;
   float miny, maxy;

   pwindow_t *w;

   void (*segClipFunc)(rendercontext_t &context, const cb_seg_t &);

//   planehash_t *overlay;
};

extern portalrender_t  portalrender;
#endif

//----------------------------------------------------------------------------
//
// $Log: r_portals.h,v $
//
//----------------------------------------------------------------------------
