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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Creating, managing, and rendering portals.
//      SoM created 12/8/03
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "c_io.h"
#include "d_gi.h"
#include "e_exdata.h"
#include "e_things.h"
#include "m_bbox.h"
#include "m_collection.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_bsp.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_portal.h"
#include "r_sky.h"
#include "r_state.h"
#include "r_things.h"
#include "v_alloc.h"
#include "v_misc.h"

enum
{
   PORTAL_RECURSION_LIMIT = 128, // maximum times same portal is drawn in a
                                 // recursion
};

//
// Portal type used in special portal_define()
//
enum portaltype_e
{
   portaltype_plane,
   portaltype_horizon,
   portaltype_skybox,
   portaltype_anchor,
   portaltype_twoway,
   portaltype_linked,
   portaltype_MAX
};

//
// Defined portal (using Portal_Define)
//
struct portalentry_t
{
   int id;
   portal_t *portal;
};

static PODCollection<portalentry_t> gPortals;   // defined portals

//=============================================================================
//
// Portal Spawning and Management
//

static portal_t *portals = NULL, *last = NULL;
static pwindow_t *unusedhead = NULL, *windowhead = NULL, *windowlast = NULL;

//
// VALLOCATION(portals)
//
// haleyjd 04/30/13: when the resolution changes, all portals need notification.
//
VALLOCATION(portals)
{
   planehash_t *hash;
   for(portal_t *p = portals; p; p = p->next)
   {
      // clear portal overlay visplane hash tables
      if((hash = p->poverlay))
      {
         for(int i = 0; i < hash->chaincount; i++)
            hash->chains[i] = NULL;
      }
   }

   // free portal window structures on the main list
   pwindow_t *rover = windowhead;
   while(rover)
   {
      pwindow_t *child = rover->child;
      pwindow_t *next;

      // free any child windows
      while(child)
      {
         next = child->child;
         if((hash = child->poverlay))
            for(int i = 0; i < hash->chaincount; ++i)
               hash->chains[i] = nullptr;
         efree(child->top);
         efree(child);
         child = next;
      }

      // free this window
      next = rover->next;
      if((hash = rover->poverlay))
         for(int i = 0; i < hash->chaincount; ++i)
            hash->chains[i] = nullptr;
      efree(rover->top);
      efree(rover);
      rover = next;
   }

   // free portal window structures on the freelist
   rover = unusedhead;
   while(rover)
   {
      pwindow_t *next = rover->next;
      if((hash = rover->poverlay))
         for(int i = 0; i < hash->chaincount; ++i)
            hash->chains[i] = nullptr;
      efree(rover->top);
      efree(rover);
      rover = next;
   }

   windowhead = windowlast = unusedhead = NULL;   
}

// This flag is set when a portal is being rendered. This flag is checked in 
// r_bsp.c when rendering camera portals (skybox, anchored, linked) so that an
// extra function (R_ClipSegToPortal) is called to prevent certain types of HOM
// in portals.

portalrender_t portalrender = { false, MAX_SCREENWIDTH, 0 };

static void R_RenderPortalNOP(pwindow_t *window)
{
   I_Error("R_RenderPortalNOP called\n");
}

static void R_SetPortalFunction(pwindow_t *window);

static void R_ClearPortalWindow(pwindow_t *window, bool noplanes)
{
   window->maxx = 0;
   window->minx = viewwindow.width - 1;

   for(int i = 0; i < video.width; i++)
   {
      window->top[i]    = view.height;
      window->bottom[i] = -1.0f;
   }

   window->child    = NULL;
   window->next     = NULL;
   window->portal   = NULL;
   window->line     = NULL;
   window->func     = R_RenderPortalNOP;
   window->clipfunc = NULL;
   window->vx = window->vy = window->vz = 0;
   window->vangle = 0;
   memset(&window->barrier, 0, sizeof(window->barrier));
   if(!noplanes)
   {
      if(!window->poverlay)
         window->poverlay = R_NewOverlaySet();
      else
         R_ClearPlaneHash(window->poverlay);
   }
}

static pwindow_t *newPortalWindow(bool noplanes = false)
{
   pwindow_t *ret;

   if(unusedhead)
   {
      ret = unusedhead;
      unusedhead = unusedhead->next;
   }
   else
   {
      ret = estructalloctag(pwindow_t, 1, PU_LEVEL);
      
      float *buf  = emalloctag(float *, 2*video.width*sizeof(float), PU_LEVEL, NULL);
      ret->top    = buf;
      ret->bottom = buf + video.width;
   }

   R_ClearPortalWindow(ret, noplanes);
   
   return ret;
}

//
// Applies portal transform based on whether it's an anchored or linked portal
//
inline static void R_applyPortalTransformTo(const portal_t *portal, 
   fixed_t &x, fixed_t &y, bool applyTranslation)
{
   if(portal->type == R_ANCHORED || portal->type == R_TWOWAY)
   {
      const portaltransform_t &tr = portal->data.anchor.transform;
      tr.applyTo(x, y, nullptr, nullptr, !applyTranslation);
   }
   else if(portal->type == R_LINKED && applyTranslation)
   {
      const linkdata_t &link = portal->data.link;
      x += link.deltax;
      y += link.deltay;
   }
}
inline static void R_applyPortalTransformTo(const portal_t *portal,
   float &x, float &y, bool applyTranslation)
{
   if(portal->type == R_ANCHORED || portal->type == R_TWOWAY)
   {
      const portaltransform_t &tr = portal->data.anchor.transform;
      tr.applyTo(x, y, !applyTranslation);
   }
   else if(portal->type == R_LINKED && applyTranslation)
   {
      const linkdata_t &link = portal->data.link;
      x += M_FixedToFloat(link.deltax);
      y += M_FixedToFloat(link.deltay);
   }
}

//
// Calculates the render barrier based on portal type and source linedef
// (assuming line portal)
//
static void R_calcRenderBarrier(const portal_t *portal, const line_t *line,
   renderbarrier_t &barrier)
{
   P_MakeDivline(line, &barrier.dln.dl);
   barrier.dln.nx = line->nx;
   barrier.dln.ny = line->ny;
   R_applyPortalTransformTo(portal, barrier.dln.dl.x, barrier.dln.dl.y, true);
   R_applyPortalTransformTo(portal, barrier.dln.dl.dx, barrier.dln.dl.dy, false);
   R_applyPortalTransformTo(portal, barrier.dln.nx, barrier.dln.ny, false);
}

//
// Expands a portal barrier BBox. For sector portals
//
void R_CalcRenderBarrier(pwindow_t &window, const sectorbox_t &box)
{
   const portal_t *portal = window.portal;
   if(!R_portalIsAnchored(portal))
      return;

   static const int ind[2][4] = { 
      {BOXLEFT, BOXRIGHT, BOXRIGHT, BOXLEFT}, 
      {BOXBOTTOM, BOXBOTTOM, BOXTOP, BOXTOP} 
   };
   
   renderbarrier_t &barrier = window.barrier;
   fixed_t x, y;
   for(int i = 0; i < 4; ++i)
   {
      // Use the corners of the box when applying transformation
      x = box.box[ind[0][i]];
      y = box.box[ind[1][i]];
      R_applyPortalTransformTo(portal, x, y, true);
      M_AddToBox(barrier.bbox, x, y);
   }
}

static pwindow_t *R_NewPortalWindow(portal_t *p, line_t *l, pwindowtype_e type)
{
   pwindow_t *ret = newPortalWindow();
   
   ret->portal = p;
   ret->line   = l;
   ret->type   = type;
   ret->head   = ret;
   if(type == pw_line)
   {
#ifdef RANGECHECK
      if(!l)
         I_Error("R_NewPortalWindow: Null line despite type == pw_line!");
#endif
      R_calcRenderBarrier(p, l, ret->barrier);
   }
   
   R_SetPortalFunction(ret);
   
   if(!windowhead)
      windowhead = windowlast = ret;
   else
   {
      windowlast->next = ret;
      windowlast = ret;
   }
   
   return ret;
}

//
// R_CreateChildWindow
//
// Spawns a child portal for an existing portal. Each portal can only
// have one child.
//
static void R_CreateChildWindow(pwindow_t *parent)
{
#ifdef RANGECHECK
   if(parent->child)
      I_Error("R_CreateChildWindow: child portal displaced\n");
#endif

   auto child = newPortalWindow(true);

   parent->child   = child;
   child->head     = parent->head;
   child->portal   = parent->portal;
   child->line     = parent->line;
   child->barrier  = parent->barrier;  // FIXME: not sure if necessary
   child->type     = parent->type;
   child->func     = parent->func;
   child->clipfunc = parent->clipfunc;
}

//
// R_WindowAdd
//
// Adds a column to a portal for rendering. A child portal may
// be created.
//
void R_WindowAdd(pwindow_t *window, int x, float ytop, float ybottom)
{
   float windowtop, windowbottom;

#ifdef RANGECHECK
   if(!window)
      I_Error("R_WindowAdd: null portal window\n");

   if(x < 0 || x >= video.width)
      I_Error("R_WindowAdd: column out of bounds (%d)\n", x);

   if((ybottom >= view.height || ytop < 0) && ytop < ybottom)
   {
      I_Error("R_WindowAdd portal supplied with bad column data.\n"
              "\tx:%d, top:%f, bottom:%f\n", x, ytop, ybottom);
   }
#endif

   windowtop    = window->top[x];
   windowbottom = window->bottom[x];

#ifdef RANGECHECK
   if(windowbottom > windowtop && 
      (windowtop < 0 || windowbottom < 0 || 
       windowtop >= view.height || windowbottom >= view.height))
   {
      I_Error("R_WindowAdd portal had bad opening data.\n"
              "\tx:%i, top:%f, bottom:%f\n", x, windowtop, windowbottom);
   }
#endif

   if(ybottom < 0.0f || ytop >= view.height)
      return;

   if(x <= window->maxx && x >= window->minx)
   {
      // column falls inside the range of the portal.

      // check to see if the portal column isn't occupied
      if(windowtop > windowbottom)
      {
         window->top[x]    = ytop;
         window->bottom[x] = ybottom;
         return;
      }

      // if the column lays completely outside the existing portal, create child
      if(ytop > windowbottom || ybottom < windowtop)
      {
         if(!window->child)
            R_CreateChildWindow(window);

         R_WindowAdd(window->child, x, ytop, ybottom);
         return;
      }

      // because a check has already been made to reject the column, the columns
      // must intersect; expand as needed
      if(ytop < windowtop)
         window->top[x] = ytop;

      if(ybottom > windowbottom)
         window->bottom[x] = ybottom;
      return;
   }

   if(window->minx > window->maxx)
   {
      // Portal is empty so place the column anywhere (first column added to 
      // the portal)
      window->minx = window->maxx = x;
      window->top[x]    = ytop;
      window->bottom[x] = ybottom;

      // SoM 3/10/2005: store the viewz in the portal struct for later use
      window->vx = viewx;
      window->vy = viewy;
      window->vz = viewz;
      window->vangle = viewangle;
      return;
   }

   if(x > window->maxx)
   {
      window->maxx = x;

      window->top[x]    = ytop;
      window->bottom[x] = ybottom;
      return;
   }

   if(x < window->minx)
   {
      window->minx = x;

      window->top[x]    = ytop;
      window->bottom[x] = ybottom;
      return;
   }
}

//
// R_CreatePortal
//
// Function to internally create a new portal.
//
static portal_t *R_CreatePortal()
{
   portal_t *ret = estructalloctag(portal_t, 1, PU_LEVEL);

   if(!portals)
      portals = last = ret;
   else
   {
      last->next = ret;
      last = ret;
   }
   
   ret->poverlay  = R_NewPlaneHash(31);
   ret->globaltex = 1;

   return ret;
}

//
// R_CalculateDeltas
//
// Calculates the deltas (offset) between two linedefs.
//
static void R_CalculateDeltas(int markerlinenum, int anchorlinenum, 
                              fixed_t *dx, fixed_t *dy, fixed_t *dz)
{
   const line_t *m = lines + markerlinenum;
   const line_t *a = lines + anchorlinenum;

   // Because of overflows, we should divide each term instead of accumulating them
   *dx = (m->v1->x / 2 + m->v2->x / 2) - (a->v1->x / 2 + a->v2->x / 2);
   *dy = (m->v1->y / 2 + m->v2->y / 2) - (a->v1->y / 2 + a->v2->y / 2);
   *dz = 0; /// ???
}

static void R_calculateTransform(int markerlinenum, int anchorlinenum,
                                 portaltransform_t *transf, bool allowrotate,
                                 bool flipped, fixed_t zoffset)
{
   // TODO: define Z offset
   transf->targetline = lines + markerlinenum;
   transf->sourceline = lines + anchorlinenum;
   transf->flipped = flipped;
   transf->zoffset = zoffset;

   transf->updateFromLines(allowrotate);
}

//
// R_GetAnchoredPortal
//
// Either finds a matching existing anchored portal matching the
// parameters, or creates a new one. Used in p_spec.c.
//
portal_t *R_GetAnchoredPortal(int markerlinenum, int anchorlinenum,
   bool allowrotate, bool flipped, fixed_t zoffset)
{
   portal_t *rover, *ret;
   edefstructvar(anchordata_t, adata);

   R_calculateTransform(markerlinenum, anchorlinenum, &adata.transform, 
      allowrotate, flipped, zoffset);

   adata.maker = markerlinenum;
   adata.anchor = anchorlinenum;

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_ANCHORED ||
         memcmp(&adata.transform, &rover->data.anchor.transform,
                sizeof(adata.transform)))
         continue;

      return rover;
   }

   ret = R_CreatePortal();
   ret->type = R_ANCHORED;
   ret->data.anchor = adata;

   // haleyjd: temporary debug
   ret->tainted = 0;

   return ret;
}

//
// R_GetTwoWayPortal
//
// Either finds a matching existing two-way anchored portal matching the
// parameters, or creates a new one. Used in p_spec.c.
//
portal_t *R_GetTwoWayPortal(int markerlinenum, int anchorlinenum,
   bool allowrotate, bool flipped, fixed_t zoffset)
{
   portal_t *rover, *ret;
   edefstructvar(anchordata_t, adata);

   R_calculateTransform(markerlinenum, anchorlinenum, &adata.transform, 
      allowrotate, flipped, zoffset);

   adata.maker = markerlinenum;
   adata.anchor = anchorlinenum;

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type  != R_TWOWAY                  || 
         memcmp(&adata.transform, &rover->data.anchor.transform,
                sizeof(adata.transform)))
         continue;

      return rover;
   }

   ret = R_CreatePortal();
   ret->type = R_TWOWAY;
   ret->data.anchor = adata;

   // haleyjd: temporary debug
   ret->tainted = 0;

   return ret;
}

//
// Returns the portal head, as a const pointer.
//
const portal_t *R_GetPortalHead()
{
   return portals;
}

//
// R_GetSkyBoxPortal
//
// Either finds a portal for the provided camera object, or creates
// a new one for it. Used in p_spec.c.
//
portal_t *R_GetSkyBoxPortal(Mobj *camera)
{
   portal_t *rover, *ret;

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_SKYBOX || rover->data.camera != camera)
         continue;

      return rover;
   }

   ret = R_CreatePortal();
   ret->type = R_SKYBOX;
   ret->data.camera = camera;
   return ret;
}

//
// Gets or creates a horizon portal from two given surfaces
//
portal_t *R_GetHorizonPortal(const sector_t *sector)
{
   I_Assert(!!sector, "Expected non-null sector");

   for(portal_t *rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_HORIZON || rover->data.sector != sector)
         continue;
      return rover;
   }

   portal_t *ret = R_CreatePortal();
   ret->type = R_HORIZON;
   ret->data.sector = sector;
   return ret;
}

//
// Get plane portal
// We really need a reference to the entire sector
//
portal_t *R_GetPlanePortal(const sector_t *sector)
{
   I_Assert(!!sector, "Expected non-null sector");

   for(portal_t *rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_PLANE || rover->data.sector != sector)
         continue;
      return rover;
   }

   portal_t *ret = R_CreatePortal();
   ret->type = R_PLANE;
   ret->data.sector = sector;
   return ret;
}

//
// R_InitPortals
//
// Called before P_SetupLevel to reset the portal list.
// Portals are allocated at PU_LEVEL cache level, so they'll
// be implicitly freed.
//
void R_InitPortals()
{
   portals = last = NULL;
   windowhead = unusedhead = windowlast = NULL;
   R_MapInitOverlaySets();

   gPortals.clear(); // clear the portal list
}

//=============================================================================
//
// Plane and Horizon Portals
//

//
// R_RenderPlanePortal
//
static void R_RenderPlanePortal(pwindow_t *window)
{
   visplane_t *vplane;
   int x;
   portal_t *portal = window->portal;

   if(portal->type != R_PLANE)
      return;

   if(window->maxx < window->minx)
      return;

   fixed_t lastx = viewx;
   fixed_t lasty = viewy;
   fixed_t lastz = viewz;
   float lastxf = view.x;
   float lastyf = view.y;
   float lastzf = view.z;
   angle_t lastangle = viewangle;
   float lastanglef = view.angle;

   viewx = window->vx;
   viewy = window->vy;
   viewz = window->vz;
   view.x = M_FixedToFloat(viewx);
   view.y = M_FixedToFloat(viewy);
   view.z = M_FixedToFloat(viewz);
   if(window->vangle != viewangle)
   {
      viewangle = window->vangle;
      viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
      viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];
      view.angle = (ANG90 - viewangle) * PI / ANG180;
      view.sin = sinf(view.angle);
      view.cos = cosf(view.angle);
   }

   const sector_t *sector = portal->data.sector;
   vplane = R_FindPlane(
      sector->srf.ceiling.height + viewz,
      sector->intflags & SIF_SKY && sector->sky & PL_SKYFLAT
      ? sector->sky : sector->srf.ceiling.pic,
      R_GetSurfaceLightLevel(surf_ceil, sector),
      sector->srf.ceiling.offset,
      sector->srf.ceiling.scale,
      // haleyjd 01/05/08: flat angle
      sector->srf.ceiling.angle + sector->srf.ceiling.baseangle,
      sector->srf.ceiling.slope,
      0,
      255,
      nullptr
   );

   vplane = R_CheckPlane(vplane, window->minx, window->maxx);

   for(x = window->minx; x <= window->maxx; x++)
   {
      if(window->top[x] < window->bottom[x])
      {
         vplane->top[x]    = (int)window->top[x];
         vplane->bottom[x] = (int)window->bottom[x];
      }
   }

   if(window->head == window && window->poverlay)
      R_PushPost(false, window);
      
   viewx = lastx;
   viewy = lasty;
   viewz = lastz;
   viewangle = lastangle;
   viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
   viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];
   view.x = lastxf;
   view.y = lastyf;
   view.z = lastzf;
   view.angle = lastanglef;
   view.sin = (float)sin(view.angle);
   view.cos = (float)cos(view.angle);

   if(window->child)
      R_RenderPlanePortal(window->child);
}

//
// R_RenderHorizonPortal
//
static void R_RenderHorizonPortal(pwindow_t *window)
{
   fixed_t lastx, lasty, lastz; // SoM 3/10/2005
   float   lastxf, lastyf, lastzf;
   visplane_t *topplane, *bottomplane;
   int x;
   portal_t *portal = window->portal;

   if(portal->type != R_HORIZON)
      return;

   if(window->maxx < window->minx)
      return;

   lastx  = viewx; 
   lasty  = viewy; 
   lastz  = viewz;
   lastxf = view.x;
   lastyf = view.y;
   lastzf = view.z;
   angle_t lastangle = viewangle;
   float lastanglef = view.angle;

   viewx = window->vx;   
   viewy = window->vy;   
   viewz = window->vz;   
   view.x = M_FixedToFloat(viewx);
   view.y = M_FixedToFloat(viewy);
   view.z = M_FixedToFloat(viewz);
   if(window->vangle != viewangle)
   {
      viewangle = window->vangle;
      viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
      viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];
      view.angle = (ANG90 - viewangle) * PI / ANG180;
      view.sin = sinf(view.angle);
      view.cos = cosf(view.angle);
   }

   const sector_t *sector = portal->data.sector;
   // haleyjd 01/05/08: angles
   
   const float angles[surf_NUM] = {
      sector->srf.floor.baseangle + sector->srf.ceiling.angle,
      sector->srf.ceiling.baseangle + sector->srf.ceiling.angle
   };
      
   topplane = R_FindPlane(
      sector->srf.ceiling.height,
      sector->intflags & SIF_SKY && sector->sky & PL_SKYFLAT 
      ? sector->sky : sector->srf.ceiling.pic,
      R_GetSurfaceLightLevel(surf_ceil, sector),
      sector->srf.ceiling.offset,
      sector->srf.ceiling.scale,
      angles[surf_ceil],
      nullptr, 0, 255, nullptr
   );
   bottomplane = R_FindPlane(
      sector->srf.floor.height,
      R_IsSkyFlat(sector->srf.floor.pic) && sector->sky & PL_SKYFLAT
      ? sector->sky : sector->srf.floor.pic,
      R_GetSurfaceLightLevel(surf_floor, sector),
      sector->srf.floor.offset,
      sector->srf.floor.scale,
      angles[surf_floor],
      nullptr, 0, 255, nullptr
   );

   topplane = R_CheckPlane(topplane, window->minx, window->maxx);
   bottomplane = R_CheckPlane(bottomplane, window->minx, window->maxx);

   for(x = window->minx; x <= window->maxx; x++)
   {
      if(window->top[x] > window->bottom[x])
         continue;

      if(window->top[x]    <= view.ycenter - 1.0f && 
         window->bottom[x] >= view.ycenter)
      {
         topplane->top[x]       = (int)window->top[x];
         topplane->bottom[x]    = centery - 1;
         bottomplane->top[x]    = centery;
         bottomplane->bottom[x] = (int)window->bottom[x];
      }
      else if(window->top[x] <= view.ycenter - 1.0f)
      {
         topplane->top[x]    = (int)window->top[x];
         topplane->bottom[x] = (int)window->bottom[x];
      }
      else if(window->bottom[x] > view.ycenter - 1.0f)
      {
         bottomplane->top[x]    = (int)window->top[x];
         bottomplane->bottom[x] = (int)window->bottom[x];
      }
   }

   if(window->head == window && window->poverlay)
      R_PushPost(false, window);
      
   viewx  = lastx; 
   viewy  = lasty; 
   viewz  = lastz;
   viewangle = lastangle;
   viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
   viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];
   view.x = lastxf;
   view.y = lastyf;
   view.z = lastzf;
   view.angle = lastanglef;
   view.sin = (float)sin(view.angle);
   view.cos = (float)cos(view.angle);

   if(window->child)
      R_RenderHorizonPortal(window->child);
}

//=============================================================================
//
// Skybox Portals
//

extern void R_ClearSlopeMark(int minx, int maxx, pwindowtype_e type);

//
// R_RenderSkyboxPortal
//
static void R_RenderSkyboxPortal(pwindow_t *window)
{
   fixed_t lastx, lasty, lastz, lastangle;
   float   lastxf, lastyf, lastzf, lastanglef;
   portal_t *portal = window->portal;

   if(portal->type != R_SKYBOX)
      return;

   if(window->maxx < window->minx)
      return;

#ifdef RANGECHECK
   for(int i = 0; i < video.width; i++)
   {
      if(window->bottom[i] > window->top[i] && (window->bottom[i] < -1 
         || window->bottom[i] > viewwindow.height || window->top[i] < -1 
         || window->top[i] > viewwindow.height))
      {
         I_Error("R_RenderSkyboxPortal: clipping array contained invalid "
                 "information:\n"
                 "   x:%i, ytop:%f, ybottom:%f\n", 
                 i, window->top[i], window->bottom[i]);
      }
   }
#endif

   if(!R_SetupPortalClipsegs(window->minx, window->maxx, window->top, window->bottom))
      return;

   R_ClearSlopeMark(window->minx, window->maxx, window->type);

   floorclip   = window->bottom;
   ceilingclip = window->top;
   
   R_ClearOverlayClips();

   portalrender.minx = window->minx;
   portalrender.maxx = window->maxx;

   ++validcount;
   R_SetMaskedSilhouette(ceilingclip, floorclip);

   lastx = viewx;
   lasty = viewy;
   lastz = viewz;
   lastangle = viewangle;
   lastxf = view.x;
   lastyf = view.y;
   lastzf = view.z;
   lastanglef = view.angle;

   viewx = portal->data.camera->x;
   viewy = portal->data.camera->y;
   viewz = portal->data.camera->z;
   view.x = M_FixedToFloat(viewx);
   view.y = M_FixedToFloat(viewy);
   view.z = M_FixedToFloat(viewz);

   // SoM: The viewangle should also be offset by the skybox camera angle.
   viewangle += portal->data.camera->angle;
   viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
   viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];

   view.angle = (ANG90 - viewangle) * PI / ANG180;
   view.sin = (float)sin(view.angle);
   view.cos = (float)cos(view.angle);

   R_IncrementFrameid();
   R_RenderBSPNode(numnodes - 1);
   
   // Only push the overlay if this is the head window
   R_PushPost(true, window->head == window ? window : NULL);

   floorclip   = floorcliparray;
   ceilingclip = ceilingcliparray;

   // SoM: "pop" the view state.
   viewx = lastx;
   viewy = lasty;
   viewz = lastz;
   viewangle = lastangle;
   view.x = lastxf;
   view.y = lastyf;
   view.z = lastzf;
   view.angle = lastanglef;

   viewsin  = finesine[viewangle>>ANGLETOFINESHIFT];
   viewcos  = finecosine[viewangle>>ANGLETOFINESHIFT];
   view.sin = (float)sin(view.angle);
   view.cos = (float)cos(view.angle);

   if(window->child)
      R_RenderSkyboxPortal(window->child);
}

//=============================================================================
//
// Anchored and Linked Portals
//

extern int    showtainted;

static void R_ShowTainted(pwindow_t *window)
{
   int y1, y2, count;

   if(window->line)
   {
      const sector_t *sector = window->line->frontsector;
      float floorangle = sector->srf.floor.baseangle + sector->srf.floor.angle;
      float ceilingangle = sector->srf.ceiling.baseangle + sector->srf.ceiling.angle;
      visplane_t *topplane = R_FindPlane(sector->srf.ceiling.height,
         sector->srf.ceiling.pic, sector->lightlevel, sector->srf.ceiling.offset,
         sector->srf.ceiling.scale, ceilingangle, nullptr, 0, 255, nullptr);
      visplane_t *bottomplane = R_FindPlane(sector->srf.floor.height,
         sector->srf.floor.pic, sector->lightlevel, sector->srf.floor.offset,
         sector->srf.floor.scale, floorangle, nullptr, 0, 255, nullptr);
      topplane = R_CheckPlane(topplane, window->minx, window->maxx);
      bottomplane = R_CheckPlane(bottomplane, window->minx, window->maxx);

      for(int x = window->minx; x <= window->maxx; x++)
      {
         if(window->top[x] > window->bottom[x])
            continue;
         if(window->top[x] <= view.ycenter - 1.0f &&
            window->bottom[x] >= view.ycenter)
         {
            topplane->top[x] = static_cast<int>(window->top[x]);
            topplane->bottom[x] = centery - 1;
            bottomplane->top[x] = centery;
            bottomplane->bottom[x] = static_cast<int>(window->bottom[x]);
         }
         else if(window->top[x] <= view.ycenter - 1.0f)
         {
            topplane->top[x] = static_cast<int>(window->top[x]);
            topplane->bottom[x] = static_cast<int>(window->bottom[x]);
         }
         else if(window->bottom[x] > view.ycenter - 1.0f)
         {
            bottomplane->top[x] = static_cast<int>(window->top[x]);
            bottomplane->bottom[x] = static_cast<int>(window->bottom[x]);
         }
      }
      
      return;
   }

   for(int i = window->minx; i <= window->maxx; i++)
   {
      byte *dest;

      y1 = (int)window->top[i];
      y2 = (int)window->bottom[i];

      count = y2 - y1 + 1;
      if(count <= 0)
         continue;

      dest = R_ADDRESS(i, y1);

      while(count > 0)
      {
         *dest = GameModeInfo->blackIndex;
         dest += video.pitch;

         count--;
      }
   }
}

//
// R_RenderAnchoredPortal
//
static void R_RenderAnchoredPortal(pwindow_t *window)
{
   fixed_t lastx, lasty, lastz;
   float   lastxf, lastyf, lastzf;
   portal_t *portal = window->portal;

   if(portal->type != R_ANCHORED && portal->type != R_TWOWAY)
      return;

   if(window->maxx < window->minx)
      return;

   // haleyjd: temporary debug
   if(portal->tainted > PORTAL_RECURSION_LIMIT)
   {
      R_ShowTainted(window);         

      portal->tainted++;
      doom_warningf("Refused to draw portal (line=%i) (t=%d)", portal->data.anchor.maker,
                    portal->tainted);
      return;
   } 

#ifdef RANGECHECK
   for(int i = 0; i < video.width; i++)
   {
      if(window->bottom[i] > window->top[i] && (window->bottom[i] < -1 
         || window->bottom[i] > viewwindow.height || window->top[i] < -1 
         || window->top[i] > viewwindow.height))
      {
         I_Error("R_RenderAnchoredPortal: clipping array contained invalid "
                 "information:\n" 
                 "   x:%i, ytop:%f, ybottom:%f\n", 
                 i, window->top[i], window->bottom[i]);
      }
   }
#endif
   
   if(!R_SetupPortalClipsegs(window->minx, window->maxx, window->top, window->bottom))
      return;

   R_ClearSlopeMark(window->minx, window->maxx, window->type);

   // haleyjd: temporary debug
   portal->tainted++;

   floorclip   = window->bottom;
   ceilingclip = window->top;

   R_ClearOverlayClips();
   
   portalrender.minx = window->minx;
   portalrender.maxx = window->maxx;

   ++validcount;
   R_SetMaskedSilhouette(ceilingclip, floorclip);

   lastx = viewx;
   lasty = viewy;
   lastz = viewz;
   lastxf = view.x;
   lastyf = view.y;
   lastzf = view.z;
   angle_t lastangle = viewangle;
   float lastanglef = view.angle;

   // SoM 3/10/2005: Use the coordinates stored in the portal struct
   const portaltransform_t &tr = portal->data.anchor.transform;
   viewx = window->vx;
   viewy = window->vy;
   tr.applyTo(viewx, viewy, &view.x, &view.y);
   double vz = M_FixedToDouble(window->vz) + tr.move.z;
   viewz = M_DoubleToFixed(vz);
   view.z = static_cast<float>(vz);

   viewangle = window->vangle + static_cast<angle_t>(tr.angle * ANG180 / PI);
   viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
   viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];
   view.angle = (ANG90 - viewangle) * PI / ANG180;
   view.sin = sinf(view.angle);
   view.cos = cosf(view.angle);

   R_IncrementFrameid();
   R_RenderBSPNode(numnodes - 1);

   // Only push the overlay if this is the head window
   R_PushPost(true, window->head == window ? window : NULL);

   floorclip = floorcliparray;
   ceilingclip = ceilingcliparray;

   viewx  = lastx;
   viewy  = lasty;
   viewz  = lastz;
   viewangle = lastangle;
   viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
   viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];
   view.x = lastxf;
   view.y = lastyf;
   view.z = lastzf;
   view.angle = lastanglef;

   view.sin = (float)sin(view.angle);
   view.cos = (float)cos(view.angle);

   if(window->child)
      R_RenderAnchoredPortal(window->child);
}

static void R_RenderLinkedPortal(pwindow_t *window)
{
   fixed_t lastx, lasty, lastz;
   angle_t lastangle;
   float   lastxf, lastyf, lastzf;
   float   lastanglef;
   portal_t *portal = window->portal;

   if(portal->type != R_LINKED || window->maxx < window->minx)
      return;

   // haleyjd: temporary debug
   if(portal->tainted > PORTAL_RECURSION_LIMIT)
   {
      R_ShowTainted(window);         

      portal->tainted++;
      doom_warningf("Refused to draw portal (line=%i) (t=%d)", portal->data.link.maker,
                    portal->tainted);
      return;
   } 

#ifdef RANGECHECK
   for(int i = 0; i < video.width; i++)
   {
      if(window->bottom[i] > window->top[i] && (window->bottom[i] < -1 
         || window->bottom[i] > viewwindow.height || window->top[i] < -1 
         || window->top[i] > viewwindow.height))
      {
         I_Error("R_RenderAnchoredPortal: clipping array contained invalid "
                 "information:\n" 
                 "   x:%i, ytop:%f, ybottom:%f\n", 
                 i, window->top[i], window->bottom[i]);
      }
   }
#endif
   
   if(!R_SetupPortalClipsegs(window->minx, window->maxx, window->top, window->bottom))
      return;

   R_ClearSlopeMark(window->minx, window->maxx, window->type);

   // haleyjd: temporary debug
   portal->tainted++;

   floorclip   = window->bottom;
   ceilingclip = window->top;

   R_ClearOverlayClips();
   
   portalrender.minx = window->minx;
   portalrender.maxx = window->maxx;

   ++validcount;
   R_SetMaskedSilhouette(ceilingclip, floorclip);

   lastx  = viewx;
   lasty  = viewy;
   lastz  = viewz;
   lastxf = view.x;
   lastyf = view.y;
   lastzf = view.z;
   lastangle = viewangle;
   lastanglef = view.angle;

   // SoM 3/10/2005: Use the coordinates stored in the portal struct
   viewx  = window->vx + portal->data.link.deltax;
   viewy  = window->vy + portal->data.link.deltay;
   viewz  = window->vz + portal->data.link.deltaz;
   view.x = M_FixedToFloat(viewx);
   view.y = M_FixedToFloat(viewy);
   view.z = M_FixedToFloat(viewz);

   if(window->vangle != viewangle)
   {
      viewangle = window->vangle;
      viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
      viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];
      view.angle = (ANG90 - viewangle) * PI / ANG180;
      view.sin = sinf(view.angle);
      view.cos = cosf(view.angle);
   }

   R_IncrementFrameid();
   R_RenderBSPNode(numnodes - 1);

   // Only push the overlay if this is the head window
   R_PushPost(true, window->head == window ? window : NULL);

   floorclip = floorcliparray;
   ceilingclip = ceilingcliparray;

   viewx  = lastx;
   viewy  = lasty;
   viewz  = lastz;
   viewangle = lastangle;
   viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
   viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];
   view.x = lastxf;
   view.y = lastyf;
   view.z = lastzf;
   view.angle = lastanglef;
   view.sin = (float)sin(view.angle);
   view.cos = (float)cos(view.angle);

   if(window->child)
      R_RenderLinkedPortal(window->child);
}

//
// R_UntaintPortals
//
// haleyjd: temporary debug (maybe)
// Clears the tainted count for all portals to zero.
// This allows the renderer to keep track of how many times a portal has been
// rendered during a frame. If that count exceeds a given limit (which is
// currently somewhat arbitrarily set to the screen width), the renderer will
// refuse to render the portal any more during that frame. This prevents run-
// away recursion between multiple portals, as well as run-away recursion into
// the same portal due to floor/ceiling overlap caused by using non-two-way
// anchored portals in two-way situations. Only anchored portals and skyboxes
// are susceptible to this problem.
//
void R_UntaintPortals()
{
   portal_t *r;

   for(r = portals; r; r = r->next)
   {
      r->tainted = 0;
   }
}

static void R_SetPortalFunction(pwindow_t *window)
{
   switch(window->portal->type)
   {
   case R_PLANE:
      window->func     = R_RenderPlanePortal;
      window->clipfunc = NULL;
      break;
   case R_HORIZON:
      window->func     = R_RenderHorizonPortal;
      window->clipfunc = NULL;
      break;
   case R_SKYBOX:
      window->func     = R_RenderSkyboxPortal;
      window->clipfunc = NULL;
      break;
   case R_ANCHORED:
   case R_TWOWAY:
      window->func     = R_RenderAnchoredPortal;
      window->clipfunc = segclipfuncs[window->type];
      break;
   case R_LINKED:
      window->func     = R_RenderLinkedPortal;
      window->clipfunc = segclipfuncs[window->type];
      break;
   default:
      window->func     = R_RenderPortalNOP;
      window->clipfunc = NULL;
      break;
   }
}

//
// Get sector portal window. 
//
pwindow_t *R_GetSectorPortalWindow(surf_e surf, const surface_t &surface)
{
   // SoM: TODO: There could be the possibility of multiple portals
   // being able to share a single window set.
   // ioanch: also added plane checks
   for(pwindow_t *rover = windowhead; rover; rover = rover->next)
      if(rover->portal == surface.portal && rover->type == pw_surface[surf] &&
         rover->planez == surface.height)
      {
         return rover;
      }

   // not found, so make it
   pwindow_t *window = R_NewPortalWindow(surface.portal, nullptr, pw_surface[surf]);
   window->planez = surface.height;
   M_ClearBox(window->barrier.bbox);

   return window;
}

pwindow_t *R_GetLinePortalWindow(portal_t *portal, line_t *line)
{
   pwindow_t *rover = windowhead;

   while(rover)
   {
      if(rover->portal == portal && rover->type == pw_line && 
         rover->line == line)
         return rover;

      rover = rover->next;
   }

   // not found, so make it
   return R_NewPortalWindow(portal, line, pw_line);
}

//
// Moves portal overlay to window, clearing data from portal.
//
void R_MovePortalOverlayToWindow(surf_e surf)
{
//   const portal_t *portal = isceiling ? seg.c_portal : seg.f_portal;
   pwindow_t *window = surf == surf_ceil ? seg.c_window : seg.f_window;
   visplane_t *&plane = surf == surf_ceil ? seg.ceilingplane : seg.floorplane;
   if(plane)
   {
      plane = R_FindPlane(plane->height, plane->picnum, plane->lightlevel,
         plane->offs, plane->scale, plane->angle,
         plane->pslope, plane->bflags, plane->opacity, window->head->poverlay);
   }
//   if(portal)
//      R_ClearPlaneHash(portal->poverlay);
}

//
// R_ClearPortals
//
// Called at the start of each frame
//
void R_ClearPortals()
{
   portal_t *r = portals;
   
   while(r)
   {
      R_ClearPlaneHash(r->poverlay);
      r = r->next;
   }
}

//
// R_RenderPortals
//
// Primary portal rendering function.
//
void R_RenderPortals()
{
   pwindow_t *w;

   while(windowhead)
   {
      portalrender.active = true;
      portalrender.w = windowhead;
      portalrender.segClipFunc = windowhead->clipfunc;
//      portalrender.overlay = windowhead->portal->poverlay;

      if(windowhead->maxx >= windowhead->minx)
         windowhead->func(windowhead);

      portalrender.active = false;
      portalrender.w = NULL;
      portalrender.segClipFunc = NULL;
//      portalrender.overlay = NULL;

      // free the window structs
      w = windowhead->child;
      while(w)
      {
         w->next = unusedhead;
         unusedhead = w;
         w = w->child;
         unusedhead->child = NULL;
      }

      w = windowhead->next;
      windowhead->next = unusedhead;
      unusedhead = windowhead;
      unusedhead->child = NULL;

      windowhead = w;
   }

   windowlast = windowhead;
}

//=============================================================================
//
// Portal matrix methods
//

//
// Apply transform to fixed-point values
//
void portaltransform_t::applyTo(fixed_t &x, fixed_t &y, 
   float *fx, float *fy, bool nomove) const
{
   double wx = M_FixedToDouble(x);
   double wy = M_FixedToDouble(y);
   double vx = rot[0][0] * wx + rot[0][1] * wy;
   double vy = rot[1][0] * wx + rot[1][1] * wy;
   if(!nomove)
   {
      vx += move.x;
      vy += move.y;
   }
   x = M_DoubleToFixed(vx);
   y = M_DoubleToFixed(vy);
   if(fx && fy)
   {
      *fx = static_cast<float>(vx);
      *fy = static_cast<float>(vy);
   }
}
void portaltransform_t::applyTo(float &x, float &y, bool nomove) const
{
   double vx = rot[0][0] * x + rot[0][1] * y;
   double vy = rot[1][0] * x + rot[1][1] * y;
   if(!nomove)
   {
      vx += move.x;
      vy += move.y;
   }
   x = static_cast<float>(vx);
   y = static_cast<float>(vy);
}
void portaltransform_t::updateFromLines(bool allowrotate)
{
   // origin point
   double mx = (targetline->v1->fx + targetline->v2->fx) / 2;
   double my = (targetline->v1->fy + targetline->v2->fy) / 2;
   double ax = (sourceline->v1->fx + sourceline->v2->fx) / 2;
   double ay = (sourceline->v1->fy + sourceline->v2->fy) / 2;

   // angle delta between the normals behind the linedefs
   // TODO: add support for flipped anchors (line portals)
   double arot, cosval, sinval;
   if(allowrotate)
   {
      arot = atan2(flipped ? targetline->ny : -targetline->ny, 
         flipped ? targetline->nx : -targetline->nx) - 
         atan2(-sourceline->ny, -sourceline->nx);
      cosval = cos(arot);
      sinval = sin(arot);
   }
   else
   {
      arot = 0;
      cosval = 1;
      sinval = 0;
   }

   rot[0][0] = cosval;
   rot[0][1] = -sinval;
   rot[1][0] = sinval;
   rot[1][1] = cosval;
   move.x = -ax * cosval + ay * sinval + mx;
   move.y = -ax * sinval - ay * cosval + my;
   move.z = M_FixedToDouble(zoffset);

   angle = arot;
}

//=============================================================================
//
// SoM: Begin linked portals
//

portal_t *R_GetLinkedPortal(int markerlinenum, int anchorlinenum, 
                            fixed_t planez,    int fromid,
                            int toid)
{
   portal_t *rover, *ret;
   edefstructvar(linkdata_t, ldata);

   ldata.fromid = fromid;
   ldata.toid   = toid;
   ldata.planez = planez;

   R_CalculateDeltas(markerlinenum, anchorlinenum, 
                     &ldata.deltax, &ldata.deltay, &ldata.deltaz);

   ldata.maker = markerlinenum;
   ldata.anchor = anchorlinenum;

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type  != R_LINKED                || 
         ldata.deltax != rover->data.link.deltax ||
         ldata.deltay != rover->data.link.deltay ||
         ldata.deltaz != rover->data.link.deltaz ||
         ldata.fromid != rover->data.link.fromid ||
         ldata.toid   != rover->data.link.toid   ||
         ldata.planez != rover->data.link.planez)
         continue;

      return rover;
   }

   ret = R_CreatePortal();
   ret->type = R_LINKED;
   ret->data.link = ldata;

   // haleyjd: temporary debug
   ret->tainted = 0;

   return ret;
}

//=============================================================================
//
// Spawn portals from specials (legacy ones still in p_spec.cpp / P_SpawnPortal)
//

//
// Actually pairs the lines. Make sure to call from both lines
//
static void R_pairPortalLines(line_t &line, line_t &pline)
{
   line.beyondportalline = &pline;  // used with MLI_1SPORTALLINE
   if(!line.backsector)
   {
      line.intflags |= MLI_1SPORTALLINE;   // for rendering
      if(line.portal && line.portal->type == R_LINKED)
      {
         // MAKE IT PASSABLE
         line.backsector = line.frontsector;
         line.sidenum[1] = line.sidenum[0];
         line.flags &= ~ML_BLOCKING;
         line.flags |= ML_TWOSIDED;
      }
   }

   if(line.portal && pline.portal)
   {
      if(line.portal->type == R_LINKED &&
         pline.portal->type == R_LINKED)
      {
         line.portal->data.link.polyportalpartner = pline.portal;
         pline.portal->data.link.polyportalpartner = line.portal;
      }
      else if(line.portal->type == R_TWOWAY &&
         pline.portal->type == R_TWOWAY)
      {
         line.portal->data.anchor.polyportalpartner = pline.portal;
         pline.portal->data.anchor.polyportalpartner = line.portal;
      }
   }
}

//
// Implements Line_QuickPortal
//
// Convenience special for easily creating a line portal connecting two areas
// mutually. Apply this on two linedefs of equal length belonging to separate
// areas. A pair of portals will be created for these two lines, visually (and
// possibly spatially) connecting them. If the portal is NOT interactive, the
// lines will also be possible to have different angles.
//
// Due to potential future changes, do NOT place the two linked (interactive)
// portal linedefs at different angles, assuming that the resulting portal won't
// have angles. For now, always place interactive portal lines at the same angle.
// You can use different angles for non-interactive anchored portals.
//
// Arguments:
// * type: portal type (0: interactive, 1: only visual)
//
int P_CreatePortalGroup(sector_t *from);
void R_SpawnQuickLinePortal(line_t &line)
{
   if(!line.tag)
   {
      C_Printf(FC_ERROR "Line_QuickPortal can't use tag 0 (line=%d)\a\n", eindex(&line - lines));
      return;
   }
   if(line.args[0] != 0 && line.args[0] != 1)
   {
      C_Printf(FC_ERROR "Line_QuickPortal first argument must be 0 or 1 (line=%d)\a\n",
               eindex(&line - lines));
      return;
   }
   int searchPosition = -1;
   line_t *otherline;
   while((otherline = P_FindLine(line.tag, &searchPosition)))
   {
      if(otherline == &line || otherline->special != line.special ||
         otherline->args[0] != line.args[0])
      {
         continue;
      }
      break;
   }
   if(!otherline)
   {
      C_Printf(FC_ERROR "Line_QuickPortal couldn't find the other like-tagged line (line=%d)\a\n",
               eindex(&line - lines));
      return;
   }
   bool linked = line.args[0] == 0;
   // Strict requirement for angle equality for linked portals
   if(linked && otherline->dx != -line.dx && otherline->dy != -line.dy)
   {
      C_Printf(FC_ERROR "Line_QuickPortal linked portal changing angle not currently supported "
               "(line=%d other=%d)\a\n", eindex(&line - lines), eindex(otherline - lines));
      return;
   }

   int linenum = static_cast<int>(&line - lines);
   int otherlinenum = static_cast<int>(otherline - lines);

   portal_t *portals[2];
   if(!linked)
   {
      portals[0] = R_GetTwoWayPortal(otherlinenum, linenum, true, true, 0);
      portals[1] = R_GetTwoWayPortal(linenum, otherlinenum, true, true, 0);
   }
   else
   {

      if(line.frontsector->groupid == R_NOGROUP)
         P_CreatePortalGroup(line.frontsector);
      if(otherline->frontsector->groupid == R_NOGROUP)
         P_CreatePortalGroup(otherline->frontsector);
      int fromid = line.frontsector->groupid;
      int toid = otherline->frontsector->groupid;

      portals[0] = R_GetLinkedPortal(otherlinenum, linenum, 0, fromid, toid);
      portals[1] = R_GetLinkedPortal(linenum, otherlinenum, 0, toid, fromid);
   }

   P_SetPortal(line.frontsector, &line, portals[0], portal_lineonly);
   P_SetPortal(otherline->frontsector, otherline, portals[1], portal_lineonly);

   R_pairPortalLines(line, *otherline);
   R_pairPortalLines(*otherline, line);

   // Now delete the special
   line.special = 0;
   memset(line.args, 0, sizeof(line.args));
   otherline->special = 0;
   memset(otherline->args, 0, sizeof(otherline->args));
}

//
// Finds the first free portal id. Useful for map designers who are having 
// trouble finding one.
//
static int R_findFreePortalId()
{
   int free = 1;
   PODCollection<int> busylist;
   for(const auto &entry : gPortals)
   {
      busylist.add(abs(entry.id));
   }
   qsort(&busylist[0], busylist.getLength(), sizeof(int), [](const void *a, const void *b) {
      return *(int *)a - *(int *)b;
   });
   for(int b : busylist)
   {
      if(b == free)
         ++free;
      else if(b > free)
         break;
   }
   return free;
}

//
// Defines a portal without placing it in the map
//
void R_DefinePortal(const line_t &line)
{
   int portalid = line.args[0];
   int int_type = line.args[1];
   int anchorid = line.args[2];
   fixed_t zoffset = line.args[3] << FRACBITS;
   bool flipped = line.args[4] == 1;

   if(int_type < 0 || int_type >= static_cast<int>(portaltype_MAX))
   {
      C_Printf(FC_ERROR "Wrong portal type %d for line %d\a\n", int_type, eindex(&line - lines));
      return;
   }

   if(portalid <= 0)
   {
      C_Printf(FC_ERROR "Portal id 0 or negative not allowed (line=%d)\a\n", eindex(&line - lines));
      return;
   }

   for(const auto &entry : gPortals)
   {
      if(abs(entry.id) == portalid) // also check against negatives
      {
         int freeid = R_findFreePortalId();
         C_Printf(FC_ERROR "Portal id %d was already set. Use %d instead on linedef %d.\a\n", 
            portalid, freeid, (int)(&line - lines));
         return;
      }
   }

   auto type = static_cast<portaltype_e>(int_type);
   portal_t *portal, *mirrorportal = nullptr;
   sector_t *sector = line.frontsector;
   sector_t *othersector;
   Mobj *skycam;
   int destlinenum;
   const int thislinenum = static_cast<int>(&line - lines);
   int toid, fromid;
   fixed_t planez;

   const int CamType = E_ThingNumForName("EESkyboxCam");

   switch(type)
   {
   case portaltype_plane:
      portal = R_GetPlanePortal(sector);
      break;
   case portaltype_horizon:
      portal = R_GetHorizonPortal(sector);
      break;
   case portaltype_skybox:
      skycam = sector->thinglist;
      while(skycam)
      {
         if(skycam->type == CamType)
            break;
         skycam = skycam->snext;
      }
      if(!skycam)
      {
         C_Printf(FC_ERROR "Skybox found with no camera (line=%d)\a\n", eindex(&line - lines));
         return;
      }
      portal = R_GetSkyBoxPortal(skycam);
      break;
   case portaltype_anchor:
   case portaltype_twoway:
   case portaltype_linked:
      if(!anchorid)
      {
         C_Printf(FC_ERROR "Anchored portal must have anchor line id (line=%d)\a\n",
                  eindex(&line - lines));
         return;
      }
      for(destlinenum = -1; 
         (destlinenum = P_FindLineFromTag(anchorid, destlinenum)) >= 0; )
      {
         if(&lines[destlinenum] == &line)  // don't allow same target
            continue;
         break;
      }
      if(destlinenum == -1)
      {
         C_Printf(FC_ERROR "No anchor line found (tag=%d, line=%d)\a\n", anchorid,
                  eindex(&line - ::lines));
         return;
      }
      if(type == portaltype_anchor)
         portal = R_GetAnchoredPortal(destlinenum, thislinenum, true, flipped, zoffset);
      else if(type == portaltype_twoway)
      {
         portal = R_GetTwoWayPortal(destlinenum, thislinenum, true, flipped,
            zoffset);
         mirrorportal = R_GetTwoWayPortal(thislinenum, destlinenum, true,
            flipped, -zoffset);
      }
      else
      {
         othersector = lines[destlinenum].frontsector;
         if(sector->groupid == R_NOGROUP)
            P_CreatePortalGroup(sector);
         if(othersector->groupid == R_NOGROUP)
            P_CreatePortalGroup(othersector);
         fromid = sector->groupid;
         toid = othersector->groupid;

         if(othersector->srf.floor.height / 2 + othersector->srf.ceiling.height / 2 <=
            sector->srf.floor.height / 2 + sector->srf.ceiling.height / 2)
         {
            // this sector is above the other
            planez = sector->srf.floor.height + zoffset;
         }
         else
            planez = sector->srf.ceiling.height + zoffset;

         portal = R_GetLinkedPortal(destlinenum, thislinenum, planez, fromid, 
            toid);
         mirrorportal = R_GetLinkedPortal(thislinenum, destlinenum, planez, 
            toid, fromid);
      }
      break;
   default:
      C_Printf(FC_ERROR "Unknown portal type (type=%d, line=%d)\a\n", type, eindex(&line - ::lines));
      return;
   }
   
   // Done
   edefstructvar(portalentry_t, entry);
   entry.id = portalid;
   entry.portal = portal;
   gPortals.add(entry);
   if(mirrorportal)
   {
      entry.portal = mirrorportal;
      entry.id = -entry.id;
      gPortals.add(entry);
   }
}

//
// Applies the portals defined with IDs
//
void R_ApplyPortals(sector_t &sector, int portalceiling, int portalfloor)
{
   if(portalceiling || portalfloor)
   {
      for(const auto &entry : gPortals)
      {
         if(portalceiling && entry.id == portalceiling)
         {
            P_SetPortal(&sector, nullptr, entry.portal, portal_ceiling);
            portalceiling = 0;
         }
         if(portalfloor && entry.id == portalfloor)
         {
            P_SetPortal(&sector, nullptr, entry.portal, portal_floor);
            portalfloor = 0;
         }
         if(!portalfloor && !portalceiling)
            return;
      }
   }
}

//
// Bonds two portal lines for some features depending on it to work. Does 
// blockmap search to find the partner.
//
static void R_findPairPortalLines(line_t &line)
{
   v2fixed_t tv1 = { line.v1->x, line.v1->y };
   v2fixed_t tv2 = { line.v2->x, line.v2->y };
   R_applyPortalTransformTo(line.portal, tv1.x, tv1.y, true);
   R_applyPortalTransformTo(line.portal, tv2.x, tv2.y, true);
   // Do a blockmap search
   int bx = (tv1.x - bmaporgx) >> MAPBLOCKSHIFT;
   int by = (tv1.y - bmaporgy) >> MAPBLOCKSHIFT;
   if(bx < 0 || bx >= bmapwidth || by < 0 || by >= bmapheight)
      return;  // sanity check

   int neighbourhood[3] = { 0, -1, 1 };
   int i, j;

   // Also search neighbouring blocks. A bit overkill and shouldn't happen
   // in well-built maps, but the idea is to treat edge cases where a line
   // is right on a block boundary. Normally I should check the proximity of
   // the vector on that boundary, but you know map authors in the wild...
   for(i = 0; i < 3; ++i)
      for(j = 0; j < 3; ++j)
      {
         int nbx = bx + neighbourhood[j];
         int nby = by + neighbourhood[i];
         if(nbx < 0 || nbx >= bmapwidth || nby < 0 || nby >= bmapheight)
            continue;  // sanity check

         int offset = nby * bmapwidth + nbx;
         offset = *(blockmap + offset);
         const int *list = blockmaplump + offset;

         if(skipblstart)
            ++list;
         for(; *list != -1; ++list)
         {
            if(*list >= numlines || *list < 0)
               continue;
            line_t &pline = lines[*list];

            // As close as possible, but not exaggerated
            if(D_abs(pline.v2->x - tv1.x) > FRACUNIT / 16 ||
               D_abs(pline.v2->y - tv1.y) > FRACUNIT / 16 ||
               D_abs(pline.v1->x - tv2.x) > FRACUNIT / 16 ||
               D_abs(pline.v1->y - tv2.y) > FRACUNIT / 16)
            {
               continue;
            }

            R_pairPortalLines(line, pline);
            return;
         }
      }
}

//
// Applies portal marked id on a line
//
void R_ApplyPortal(line_t &line, int portal)
{
   if(portal)
      for(const auto &entry : gPortals)
         if(entry.id == portal)
         {
            P_SetPortal(line.frontsector, &line, entry.portal, portal_lineonly);
            if(R_portalIsAnchored(entry.portal))
               R_findPairPortalLines(line);
            return;
         }
}

//
// True if ceiling is a basic portal without overlay
//
bool R_IsSkyLikePortalCeiling(const sector_t &sector)
{
   return sector.srf.ceiling.portal && !(sector.srf.ceiling.pflags & (PF_DISABLED | PF_NOPASS)) &&
   (!(sector.srf.ceiling.pflags & PS_OVERLAY) || !(sector.srf.ceiling.pflags & PO_OPACITYMASK)) &&
   (sector.srf.ceiling.portal->type == R_SKYBOX || sector.srf.ceiling.portal->type == R_HORIZON ||
    sector.srf.ceiling.portal->type == R_PLANE);
}

//
// True if floor is a basic portal without overlay
//
bool R_IsSkyLikePortalFloor(const sector_t &sector)
{
   return sector.srf.floor.portal && !(sector.srf.floor.pflags & (PF_DISABLED | PF_NOPASS)) &&
      (!(sector.srf.floor.pflags & PS_OVERLAY) || !(sector.srf.floor.pflags & PO_OPACITYMASK)) &&
      (sector.srf.floor.portal->type == R_SKYBOX || sector.srf.floor.portal->type == R_HORIZON ||
         sector.srf.floor.portal->type == R_PLANE);
}

//
// True if line is a basic portal
//
bool R_IsSkyWall(const line_t &line)
{
   // Just use the same flags, even if they may not be available from UDMF
   // BLOCKALL lines count as solid to everything, so they will just explode
   // stuff.
   return R_IsSkyFlat(sides[line.sidenum[0]].midtexture) ||
   (line.portal && !(line.pflags & (PF_DISABLED | PF_NOPASS)) &&
    !(line.extflags & EX_ML_BLOCKALL) && (line.portal->type == R_SKYBOX ||
                                          line.portal->type == R_HORIZON ||
                                          line.portal->type == R_PLANE));
}

// EOF

