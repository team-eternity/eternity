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
//      Creating, managing, and rendering portals.
//      SoM created 12/8/03
//
//-----------------------------------------------------------------------------

#include "c_io.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_bsp.h"
#include "r_things.h"

static portal_t *portals = NULL, *last = NULL;
static pwindow_t *unusedhead = NULL, *windowhead = NULL, *windowlast = NULL;

// This flag is set when a portal is being rendered. This flag is checked in 
// r_bsp.c when rendering camera portals (skybox, anchored, linked) so that an
// extra function (R_ClipSegToPortal) is called to prevent certain types of HOM
// in portals.

portalrender_t portalrender = { false, MAX_SCREENWIDTH, 0 };


static void R_RenderPortalNOP(pwindow_t *window)
{
   I_Error("R_RenderPortalNOP called\n");
}

static void R_ClearPortalWindow(pwindow_t *window)
{
   int i;
   window->maxx = 0;
   window->minx = viewwidth - 1;

   for(i = 0; i < MAX_SCREENWIDTH; i++)
   {
      window->top[i] = view.height;
      window->bottom[i] = -1.0f;
   }

   window->child = NULL;
   window->next = NULL;
   window->portal = NULL;
   window->line = NULL;
   window->func = R_RenderPortalNOP;
   window->vx = window->vy = window->vz = 0;
}


static pwindow_t *R_NewPortalWindow()
{
   pwindow_t *ret;

   if(unusedhead)
   {
      ret = unusedhead;
      unusedhead = unusedhead->next;
   }
   else
      ret = Z_Malloc(sizeof(pwindow_t), PU_LEVEL, 0);

   R_ClearPortalWindow(ret);
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
   pwindow_t *child;

   if(parent->child)
      I_Error("R_CreateChildWindow: called for parent that already had a child.\n");

   child = R_NewPortalWindow();

   parent->child = child;
   child->portal = parent->portal;
   child->line = parent->line;
   child->type = parent->type;
   child->func = parent->func;
}


//
// R_WindowAdd
//
// Adds a column to a portal for rendering. A child portal may
// be created.
//
void R_WindowAdd(pwindow_t *window, int x, float ytop, float ybottom)
{
   float ptop = window->top[x];
   float pbottom = window->bottom[x];

#ifdef RANGECHECK
   if((ybottom >= view.height || ytop < 0) && 
      ytop < ybottom)
   {
      I_Error("R_WindowAdd portal supplied with bad column data.\n"
              "\tx:%i, top:%i, bottom:%i\n", x, ytop, ybottom);
   }
   
   if(pbottom > ptop && 
      (ptop < 0 || pbottom < 0 || ptop >= view.height || pbottom >= view.height))
   {
      I_Error("R_WindowAdd portal had bad opening data.\n"
              "\tx:%i, top:%i, bottom:%i\n", x, ptop, pbottom);
   }
#endif

   if(ybottom < 0.0f || ytop >= view.height)
      return;


   if(x <= window->maxx && x >= window->minx)
   {
      // column falls inside the range of the portal.

      // check to see if the portal column isn't occupied
      if(ptop > pbottom)
      {
         window->top[x] = ytop;
         window->bottom[x] = ybottom;
         return;
      }

      // if the column lays completely outside the existing portal, create child
      if(ytop > pbottom || ybottom < ptop)
      {
         if(!window->child)
            R_CreateChildWindow(window);

         R_WindowAdd(window->child, x, ytop, ybottom);
         return;
      }

      // because a check has already been made to reject the column, the columns
      // must intersect; expand as needed
      if(ytop < ptop)
         window->top[x] = ytop;

      if(ybottom > pbottom)
         window->bottom[x] = ybottom;
      return;
   }

   if(window->minx > window->maxx)
   {
      // Portal is empty so place the column anywhere (first column added to 
      // the portal)
      window->minx = window->maxx = x;
      window->top[x] = ytop;
      window->bottom[x] = ybottom;

      // SoM 3/10/2005: store the viewz in the portal struct for later use
      window->vx = viewx;
      window->vy = viewy;
      window->vz = viewz;
      return;
   }

   if(x > window->maxx)
   {
      window->maxx = x;

      window->top[x] = ytop;
      window->bottom[x] = ybottom;
      return;
   }

   if(x < window->minx)
   {
      window->minx = x;

      window->top[x] = ytop;
      window->bottom[x] = ybottom;
      return;
   }
}







//
// R_CreatePortal
//
// Function to internally create a new portal.
//
static portal_t *R_CreatePortal(void)
{
   portal_t *ret;

   ret = Z_Malloc(sizeof(portal_t), PU_LEVEL, NULL);

   if(!portals)
      portals = last = ret;
   else
   {
      last->next = ret;
      last = ret;
   }

   ret->type = R_NONE;
   ret->next = NULL;

   ret->tainted = 0; // haleyjd 05/19/06

   memset(&ret->data, 0, sizeof(ret->data));

   return ret;
}

//
// R_GetAnchoredPortal
//
// Either finds a matching existing anchored portal matching the
// parameters, or creates a new one. Used in p_spec.c.
//
portal_t *R_GetAnchoredPortal(fixed_t deltax, fixed_t deltay, fixed_t deltaz)
{
   portal_t *rover, *ret;
   cameraportal_t cam;

   memset(&cam, 0, sizeof(cam));
   cam.deltax = deltax;
   cam.deltay = deltay;
   cam.deltaz = deltaz;

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_ANCHORED || 
         memcmp(&cam, &(rover->data.camera), sizeof(cam)))
         continue;

      return rover;
   }

   ret = R_CreatePortal();
   ret->type = R_ANCHORED;
   ret->data.camera = cam;

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
portal_t *R_GetTwoWayPortal(fixed_t deltax, fixed_t deltay, fixed_t deltaz)
{
   portal_t *rover, *ret;
   cameraportal_t cam;

   memset(&cam, 0, sizeof(cam));
   cam.deltax = deltax;
   cam.deltay = deltay;
   cam.deltaz = deltaz;

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_TWOWAY || 
         memcmp(&cam, &(rover->data.camera), sizeof(cam)))
         continue;

      return rover;
   }

   ret = R_CreatePortal();
   ret->type = R_TWOWAY;
   ret->data.camera = cam;

   // haleyjd: temporary debug
   ret->tainted = 0;

   return ret;
}


//
// R_GetSkyBoxPortal
//
// Either finds a portal for the provided camera object, or creates
// a new one for it. Used in p_spec.c.
//
portal_t *R_GetSkyBoxPortal(mobj_t *camera)
{
   portal_t *rover, *ret;

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_SKYBOX || rover->data.camera.mobj != camera)
         continue;

      return rover;
   }

   ret = R_CreatePortal();
   ret->type = R_SKYBOX;
   ret->data.camera.mobj = camera;
   return ret;
}

//
// R_GetHorizonPortal
//
// Either finds an existing horizon portal matching the parameters,
// or creates a new one. Used in p_spec.c.
//
portal_t *R_GetHorizonPortal(short *floorpic, short *ceilingpic, 
                             fixed_t *floorz, fixed_t *ceilingz, 
                             short *floorlight, short *ceilinglight, 
                             fixed_t *floorxoff, fixed_t *flooryoff, 
                             fixed_t *ceilingxoff, fixed_t *ceilingyoff,
                             float *floorbaseangle, float *floorangle,
                             float *ceilingbaseangle, float *ceilingangle)
{
   portal_t *rover, *ret;
   horizonportal_t horizon;

   if(!floorpic || !ceilingpic || !floorz || !ceilingz || 
      !floorlight || !ceilinglight || !floorxoff || !flooryoff || 
      !ceilingxoff || !ceilingyoff || !floorbaseangle || !floorangle ||
      !ceilingbaseangle || !ceilingangle)
      return NULL;

   memset(&horizon, 0, sizeof(horizon));
   horizon.ceilinglight     = ceilinglight;
   horizon.floorlight       = floorlight;
   horizon.ceilingpic       = ceilingpic;
   horizon.floorpic         = floorpic;
   horizon.ceilingz         = ceilingz;
   horizon.floorz           = floorz;
   horizon.ceilingxoff      = ceilingxoff;
   horizon.ceilingyoff      = ceilingyoff;
   horizon.floorxoff        = floorxoff;
   horizon.flooryoff        = flooryoff;
   horizon.floorbaseangle   = floorbaseangle; // haleyjd 01/05/08
   horizon.floorangle       = floorangle;
   horizon.ceilingbaseangle = ceilingbaseangle;
   horizon.ceilingangle     = ceilingangle;

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_HORIZON || 
         memcmp(&rover->data.horizon, &horizon, sizeof(horizon)))
         continue;

      return rover;
   }

   ret = R_CreatePortal();
   ret->type = R_HORIZON;
   ret->data.horizon = horizon;
   return ret;
}

//
// R_GetPlanePortal
//
// Either finds a plane portal matching the parameters, or creates a
// new one. Used in p_spec.c.
//
portal_t *R_GetPlanePortal(short *pic, fixed_t *delta, 
                           short *lightlevel, 
                           fixed_t *xoff, fixed_t *yoff,
                           float *baseangle, float *angle)
{
   portal_t *rover, *ret;
   skyplaneportal_t plane;

   if(!pic || !delta || !lightlevel || !xoff || !yoff || !baseangle || !angle)
      return NULL;
      
   memset(&plane, 0, sizeof(plane));
   plane.pic = pic;
   plane.delta = delta;
   plane.lightlevel = lightlevel;
   plane.xoff = xoff;
   plane.yoff = yoff;
   plane.baseangle = baseangle; // haleyjd 01/05/08: flat angles
   plane.angle = angle;
    

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_PLANE || 
         memcmp(&rover->data.plane, &plane, sizeof(plane)))
         continue;

      return rover;
   }

   ret = R_CreatePortal();
   ret->type = R_PLANE;
   ret->data.plane = plane;
   return ret;
}



//
// R_InitPortals
//
// Called before P_SetupLevel to reset the portal list.
// Portals are allocated at PU_LEVEL cache level, so they'll
// be implicitly freed.
//
void R_InitPortals(void)
{
   portals = last = NULL;
   windowhead = unusedhead = windowlast = NULL;
}

//
// R_RenderPlanePortal
//
static void R_RenderPlanePortal(pwindow_t *window)
{
   visplane_t *plane;
   int x;
   float angle;
   portal_t *portal = window->portal;

   if(portal->type != R_PLANE)
      return;

   if(window->maxx < window->minx)
      return;

   // haleyjd 01/05/08: flat angle
   angle = *portal->data.plane.baseangle + *portal->data.plane.angle;

   plane = R_FindPlane(*portal->data.plane.delta + viewz, 
                       *portal->data.plane.pic, 
                       *portal->data.plane.lightlevel, 
                       *portal->data.plane.xoff - viewx, 
                       *portal->data.plane.yoff + viewy,
                       angle, NULL);

   plane = R_CheckPlane(plane, window->minx, window->maxx);

   for(x = window->minx; x <= window->maxx; x++)
   {
      if(window->top[x] < window->bottom[x])
      {
         plane->top[x]    = (int)window->top[x];
         plane->bottom[x] = (int)window->bottom[x];
      }
   }

   if(window->child)
      R_RenderPlanePortal(window->child);
}

//
// R_RenderHorizonPortal
//
static void R_RenderHorizonPortal(pwindow_t *window)
{
   fixed_t lastx, lasty, lastz; // SoM 3/10/2005
   float   lastxf, lastyf, lastzf, floorangle, ceilingangle;
   visplane_t *topplane, *bottomplane;
   int x;
   portal_t *portal = window->portal;

   if(portal->type != R_HORIZON)
      return;

   if(window->maxx < window->minx)
      return;

   // haleyjd 01/05/08: angles
   floorangle = *portal->data.horizon.floorbaseangle + 
                *portal->data.horizon.floorangle;

   ceilingangle = *portal->data.horizon.ceilingbaseangle +
                  *portal->data.horizon.ceilingangle;

   topplane = R_FindPlane(*portal->data.horizon.ceilingz, 
                          *portal->data.horizon.ceilingpic, 
                          *portal->data.horizon.ceilinglight, 
                          *portal->data.horizon.ceilingxoff, 
                          *portal->data.horizon.ceilingyoff,
                          ceilingangle, NULL);

   bottomplane = R_FindPlane(*portal->data.horizon.floorz, 
                             *portal->data.horizon.floorpic, 
                             *portal->data.horizon.floorlight, 
                             *portal->data.horizon.floorxoff, 
                             *portal->data.horizon.flooryoff,
                             floorangle, NULL);

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

   lastx = viewx; lasty = viewy; lastz = viewz;
   lastxf = view.x;
   lastyf = view.y;
   lastzf = view.z;

   viewx = window->vx;   
   viewy = window->vy;   
   viewz = window->vz;   
   view.x = M_FixedToFloat(viewx);
   view.y = M_FixedToFloat(viewy);
   view.z = M_FixedToFloat(viewz);

   if(window->child)
      R_RenderHorizonPortal(window->child);

   viewx = lastx; viewy = lasty; viewz = lastz;
   view.x = lastxf;
   view.y = lastyf;
   view.z = lastzf;
}

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
   {
      int i;
      for(i = 0; i < MAX_SCREENWIDTH; ++i)
      {
         if(window->bottom[i] > window->top[i] && (window->bottom[i] < -1 
            || window->bottom[i] > viewheight || window->top[i] < -1 
            || window->top[i] > viewheight))
         {
            I_Error("R_RenderSkyboxPortal: clipping array contained invalid "
                    "information. x:%i, ytop:%i, ybottom:%i\n", 
                    i, window->top[i], window->bottom[i]);
         }
      }
   }
#endif

   if(!R_SetupPortalClipsegs(window->minx, window->maxx, window->top, window->bottom))
      return;

   floorclip = window->bottom;
   ceilingclip = window->top;

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

   viewx = portal->data.camera.mobj->x;
   viewy = portal->data.camera.mobj->y;
   viewz = portal->data.camera.mobj->z;
   view.x = M_FixedToFloat(viewx);
   view.y = M_FixedToFloat(viewy);
   view.z = M_FixedToFloat(viewz);

   // SoM: The viewangle should also be offset by the skybox camera angle.
   viewangle += portal->data.camera.mobj->angle;
   viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
   viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];

   view.angle = (ANG90 - viewangle) * PI / ANG180;
   view.sin = (float)sin(view.angle);
   view.cos = (float)cos(view.angle);

   R_IncrementFrameid();
   R_RenderBSPNode(numnodes-1);
   R_PushMasked();

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

   viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
   viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];
   view.sin = (float)sin(view.angle);
   view.cos = (float)cos(view.angle);

   if(window->child)
      R_RenderSkyboxPortal(window->child);
}

//
// R_RenderAnchoredPortal
//
static void R_RenderAnchoredPortal(pwindow_t *window)
{
   fixed_t lastx, lasty, lastz;
   float   lastxf, lastyf, lastzf;
   portal_t *portal = window->portal;

#ifdef R_LINKEDPORTALS
   if(portal->type != R_ANCHORED && portal->type != R_TWOWAY && 
      portal->type != R_LINKED)
#else
   if(portal->type != R_ANCHORED && portal->type != R_TWOWAY)
#endif
      return;

   if(window->maxx < window->minx)
      return;

   // haleyjd: temporary debug
   if(portal->tainted > 6)
   {
      portal->tainted++;
      doom_printf("refused to draw portal %p (t=%d)", portal, portal->tainted);
      return;
   } 

#ifdef RANGECHECK
   {
      int i;
      for(i = 0; i < MAX_SCREENWIDTH; i++)
      {
         if(window->bottom[i] > window->top[i] && (window->bottom[i] < -1 
            || window->bottom[i] > viewheight || window->top[i] < -1 
            || window->top[i] > viewheight))
         {
            I_Error("R_RenderAnchoredPortal: clipping array contained invalid "
                    "information. x:%i, ytop:%i, ybottom:%i\n", 
                    i, window->top[i], window->bottom[i]);
         }
      }
   }
#endif
   
   if(!R_SetupPortalClipsegs(window->minx, window->maxx, window->top, window->bottom))
      return;

   // haleyjd: temporary debug
   portal->tainted++;

   floorclip = window->bottom;
   ceilingclip = window->top;

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


   // SoM 3/10/2005: Use the coordinates stored in the portal struct
   viewx  = window->vx - portal->data.camera.deltax;
   viewy  = window->vy - portal->data.camera.deltay;
   viewz  = window->vz - portal->data.camera.deltaz;
   view.x = M_FixedToFloat(viewx);
   view.y = M_FixedToFloat(viewy);
   view.z = M_FixedToFloat(viewz);

   R_IncrementFrameid();
   R_RenderBSPNode(numnodes-1);

   R_PushMasked();

   floorclip = floorcliparray;
   ceilingclip = ceilingcliparray;

   viewx  = lastx;
   viewy  = lasty;
   viewz  = lastz;
   view.x = lastxf;
   view.y = lastyf;
   view.z = lastzf;

   if(window->child)
      R_RenderAnchoredPortal(window->child);
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
void R_UntaintPortals(void)
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
      window->func = R_RenderPlanePortal;
      break;
   case R_HORIZON:
      window->func = R_RenderHorizonPortal;
      break;
   case R_SKYBOX:
      window->func = R_RenderSkyboxPortal;
      break;
   case R_ANCHORED:
   case R_TWOWAY:
#ifdef R_LINKEDPORTALS
   case R_LINKED:
#endif
      window->func = R_RenderAnchoredPortal;
      break;
   default:
      window->func = R_RenderPortalNOP;
      break;
   }
}



//
// R_Get*PortalWindow
//
// functions return a portal window based on the given parameters.
//
pwindow_t *R_GetFloorPortalWindow(portal_t *portal)
{
   pwindow_t *rover = windowhead;

   while(rover)
   {
      if(rover->portal == portal && rover->type == pw_floor)
         return rover;
   
      rover = rover->next;
   }

   // not found, so make it
   rover = R_NewPortalWindow();

   rover->type = pw_floor;
   rover->portal = portal;
   R_SetPortalFunction(rover);

   if(!windowhead)
      windowhead = windowlast = rover;
   else
   {
      windowlast->next = rover;
      windowlast = rover;
   }

   return rover;
}


pwindow_t *R_GetCeilingPortalWindow(portal_t *portal)
{
   pwindow_t *rover = windowhead;

   while(rover)
   {
      if(rover->portal == portal && rover->type == pw_ceiling)
         return rover;

      rover = rover->next;
   }

   // not found, so make it
   rover = R_NewPortalWindow();

   rover->type = pw_ceiling;
   rover->portal = portal;
   R_SetPortalFunction(rover);

   if(!windowhead)
      windowhead = windowlast = rover;
   else
   {
      windowlast->next = rover;
      windowlast = rover;
   }

   return rover;
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
   rover = R_NewPortalWindow();

   rover->type = pw_line;
   rover->portal = portal;
   rover->line = line;
   R_SetPortalFunction(rover);

   if(!windowhead)
      windowhead = windowlast = rover;
   else
   {
      windowlast->next = rover;
      windowlast = rover;
   }

   return rover;
}





//
// R_RenderPortals
//
// Primary portal rendering function.
//
void R_RenderPortals(void)
{
   pwindow_t *w;

   while(windowhead)
   {
      portalrender.active = true;
      portalrender.w = windowhead;

      if(windowhead->maxx >= windowhead->minx)
         windowhead->func(windowhead);

      portalrender.active = false;
      portalrender.w = NULL;

      // free the window structs
      w = windowhead->child;
      while(w)
      {
         w->next = unusedhead;
         unusedhead = w;
         w = w->child;
      }

      w = windowhead->next;
      windowhead->next = unusedhead;
      unusedhead = windowhead;

      windowhead = w;
   }

   windowlast = windowhead;
}

#ifdef R_LINKEDPORTALS
// ----------------------------------------------------------------------------
// SoM: Begin linked portals

portal_t *R_GetLinkedPortal(fixed_t deltax, fixed_t deltay, fixed_t deltaz, 
                             fixed_t planez, int groupid)
{
   portal_t *rover, *ret;
   cameraportal_t cam;

   memset(&cam, 0, sizeof(cam));
   cam.deltax = deltax;
   cam.deltay = deltay;
   cam.deltaz = deltaz;
   cam.groupid = groupid;
   cam.planez = planez;

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_LINKED || 
         memcmp(&cam, &(rover->data.camera), sizeof(cam)))
         continue;

      return rover;
   }

   ret = R_CreatePortal();
   ret->type = R_LINKED;
   ret->data.camera = cam;
   return ret;
}

#endif

//----------------------------------------------------------------------------
//
// $Log: r_portals.c,v $
//
//----------------------------------------------------------------------------
