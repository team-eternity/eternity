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

#ifdef R_PORTALS

#include "c_io.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_bsp.h"
#include "r_things.h"

static rportal_t *portals = NULL, *last = NULL;

// This flag is set when a portal is being rendered. This flag is checked in r_bsp.c when
// rendering camera portals (skybox, anchored, linked) so that an extra function (R_ClipSegToPortal) 
// is called to prevent certain types of HOM in portals.
portalrender_t portalrender = {false, MAX_SCREENWIDTH, 0};

//
// R_ClearPortal
//
// Initializes or re-initializes a portal.
//
static void R_ClearPortal(rportal_t *portal)
{
   int x;
   float t = (float)viewheight;

   if(portal->maxx < portal->minx)
      return;

   portal->maxx = 0;
   portal->minx = viewwidth;

   for(x = 0; x < MAX_SCREENWIDTH; x++)
   {
      portal->top[x] = t;
      portal->bottom[x] = -1.0f;
   }

   if(portal->child)
      R_ClearPortal(portal->child);
}

//
// R_CreatePortal
//
// Function to internally create a new portal.
//
static rportal_t *R_CreatePortal(void)
{
   rportal_t *ret;

   ret = Z_Malloc(sizeof(rportal_t), PU_LEVEL, NULL);

   if(!portals)
      portals = last = ret;
   else
   {
      last->next = ret;
      last = ret;
   }

   ret->child = NULL;
   ret->type = R_NONE;
   ret->next = NULL;

   ret->minx = 0;
   ret->maxx = viewwidth;

   ret->tainted = 0; // haleyjd 05/19/06

   memset(&ret->data, 0, sizeof(ret->data));

   R_ClearPortal(ret);

   return ret;
}

//
// R_GetAnchoredPortal
//
// Either finds a matching existing anchored portal matching the
// parameters, or creates a new one. Used in p_spec.c.
//
rportal_t *R_GetAnchoredPortal(fixed_t deltax, fixed_t deltay, fixed_t deltaz)
{
   rportal_t *rover, *ret;
   cameraportal_t cam;

   memset(&cam, 0, sizeof(cam));
   cam.deltax = deltax;
   cam.deltay = deltay;
   cam.deltaz = deltaz;

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_ANCHORED || memcmp(&cam, &(rover->data.camera), sizeof(cam)))
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
// R_GetAnchoredPortal
//
// Either finds a matching existing anchored portal matching the
// parameters, or creates a new one. Used in p_spec.c.
//
rportal_t *R_GetTwoWayPortal(fixed_t deltax, fixed_t deltay, fixed_t deltaz)
{
   rportal_t *rover, *ret;
   cameraportal_t cam;

   memset(&cam, 0, sizeof(cam));
   cam.deltax = deltax;
   cam.deltay = deltay;
   cam.deltaz = deltaz;

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_TWOWAY || memcmp(&cam, &(rover->data.camera), sizeof(cam)))
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
rportal_t *R_GetSkyBoxPortal(mobj_t *camera)
{
   rportal_t *rover, *ret;

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
rportal_t *R_GetHorizonPortal(short *floorpic, short *ceilingpic, 
                              fixed_t *floorz, fixed_t *ceilingz, 
                              short *floorlight, short *ceilinglight, 
                              fixed_t *floorxoff, fixed_t *flooryoff, 
                              fixed_t *ceilingxoff, fixed_t *ceilingyoff)
{
   rportal_t *rover, *ret;
   horizonportal_t horizon;

   if(!floorpic || !ceilingpic || !floorz || !ceilingz || 
      !floorlight || !ceilinglight || !floorxoff || !flooryoff || 
      !ceilingxoff || !ceilingyoff)
      return NULL;

   memset(&horizon, 0, sizeof(horizon));
   horizon.ceilinglight = ceilinglight;
   horizon.floorlight = floorlight;
   horizon.ceilingpic = ceilingpic;
   horizon.floorpic = floorpic;
   horizon.ceilingz = ceilingz;
   horizon.floorz = floorz;
   horizon.ceilingxoff = ceilingxoff;
   horizon.ceilingyoff = ceilingyoff;
   horizon.floorxoff = floorxoff;
   horizon.flooryoff = flooryoff;

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_HORIZON || memcmp(&rover->data.horizon, &horizon, sizeof(horizon)))
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
rportal_t *R_GetPlanePortal(short *pic, fixed_t *delta, 
                            short *lightlevel, 
                            fixed_t *xoff, fixed_t *yoff)
{
   rportal_t *rover, *ret;
   skyplaneportal_t plane;

   if(!pic || !delta || !lightlevel || !xoff || !yoff)
      return NULL;
      
   memset(&plane, 0, sizeof(plane));
   plane.pic = pic;
   plane.delta = delta;
   plane.lightlevel = lightlevel;
   plane.xoff = xoff;
   plane.yoff = yoff;
    

   for(rover = portals; rover; rover = rover->next)
   {
      if(rover->type != R_PLANE || memcmp(&rover->data.plane, &plane, sizeof(plane)))
         continue;

      return rover;
   }

   ret = R_CreatePortal();
   ret->type = R_PLANE;
   ret->data.plane = plane;
   return ret;
}

//
// R_CreateChild
//
// Spawns a child portal for an existing portal. Each portal can only
// have one child.
//
static void R_CreateChild(rportal_t *parent)
{
   rportal_t *child;

   if(parent->child)
      I_Error("R_CreateChild: called for parent that already had a child.\n");

   parent->child = child = Z_Malloc(sizeof(rportal_t), PU_LEVEL, NULL);

   child->next = child->child = NULL;

   memset(&child->data, 0, sizeof(child->data));

   // haleyjd 01/25/04: must initialize these
   child->minx = 0;
   child->maxx = viewwidth;
   
   child->type = parent->type;
   child->data = parent->data;

   child->tainted = 0; // haleyjd 05/19/06

   R_ClearPortal(child);
}

//
// R_PortalAdd
//
// Adds a column to a portal for rendering. A child portal may
// be created.
//
void R_PortalAdd(rportal_t *portal, int x, float ytop, float ybottom)
{
   float ptop = portal->top[x];
   float pbottom = portal->bottom[x];

#ifdef RANGECHECK
   if((ytop >= view.height || ybottom >= view.height || ytop < 0 || ybottom < 0) && 
      ytop <= ybottom)
   {
      I_Error("R_PortalAdd portal supplied with bad column data.\n"
              "\tx:%i, top:%i, bottom:%i\n", x, ytop, ybottom);
   }
   
   if(pbottom > ptop && 
      (ptop < 0 || pbottom < 0 || ptop >= view.height || pbottom >= view.height))
   {
      I_Error("R_PortalAdd portal had bad opening data.\n"
              "\tx:%i, top:%i, bottom:%i\n", x, ptop, pbottom);
   }
#endif

   if(ybottom < 0.0f || ytop >= view.height)
      return;

   if(x <= portal->maxx && x >= portal->minx)
   {
      // column falls inside the range of the portal.

      // check to see if the portal column isn't occupied
      if(ptop > pbottom)
      {
         portal->top[x] = ytop;
         portal->bottom[x] = ybottom;
         return;
      }

      // if the column lays completely outside the existing portal, create child
      if(ytop > pbottom || ybottom < ptop)
      {
         if(!portal->child)
            R_CreateChild(portal);

         R_PortalAdd(portal->child, x, ytop, ybottom);
         return;
      }

      // because a check has already been made to reject the column, the columns must intersect
      // expand as needed
      if(ytop < ptop)
         portal->top[x] = ytop;

      if(ybottom > pbottom)
         portal->bottom[x] = ybottom;
      return;
   }

   if(portal->maxx < portal->minx)
   {
      // Portal is empty so place the column anywhere (first column added to the portal)
      portal->minx = portal->maxx = x;
      portal->top[x] = ytop;
      portal->bottom[x] = ybottom;

      // SoM 3/10/2005: store the viewz in the portal struct for later use
      portal->vx = viewx;
      portal->vy = viewy;
      portal->vz = viewz;
      return;
   }

   if(x > portal->maxx)
   {
      portal->maxx = x;

      portal->top[x] = ytop;
      portal->bottom[x] = ybottom;
      return;
   }

   if(x < portal->minx)
   {
      portal->minx = x;

      portal->top[x] = ytop;
      portal->bottom[x] = ybottom;
      return;
   }
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
}

//
// R_RenderPlanePortal
//
static void R_RenderPlanePortal(rportal_t *portal)
{
   visplane_t *plane;
   int x;

   if(portal->type != R_PLANE)
      return;

   if(portal->maxx < portal->minx)
      return;

   plane = R_FindPlane(*portal->data.plane.delta + viewz, *portal->data.plane.pic, *portal->data.plane.lightlevel, *portal->data.plane.xoff - viewx, *portal->data.plane.yoff + viewy);

   plane = R_CheckPlane(plane, portal->minx, portal->maxx);

   for(x = portal->minx; x <= portal->maxx; x++)
   {
      if(portal->top[x] < portal->bottom[x])
      {
         plane->top[x]    = (int)portal->top[x];
         plane->bottom[x] = (int)portal->bottom[x];
      }
   }

   if(portal->child)
      R_RenderPlanePortal(portal->child);
}

//
// R_RenderHorizonPortal
//
static void R_RenderHorizonPortal(rportal_t *portal)
{
   fixed_t lastx, lasty, lastz; // SoM 3/10/2005 
   visplane_t *topplane, *bottomplane;
   int x;

   if(portal->type != R_HORIZON)
      return;

   if(portal->maxx < portal->minx)
      return;

   topplane = R_FindPlane(*portal->data.horizon.ceilingz, *portal->data.horizon.ceilingpic, *portal->data.horizon.ceilinglight, *portal->data.horizon.ceilingxoff, *portal->data.horizon.ceilingyoff);
   bottomplane = R_FindPlane(*portal->data.horizon.floorz, *portal->data.horizon.floorpic, *portal->data.horizon.floorlight, *portal->data.horizon.floorxoff, *portal->data.horizon.flooryoff);

   topplane = R_CheckPlane(topplane, portal->minx, portal->maxx);
   bottomplane = R_CheckPlane(bottomplane, portal->minx, portal->maxx);

   for(x = portal->minx; x <= portal->maxx; x++)
   {
      if(portal->top[x] > portal->bottom[x])
         continue;
      if(portal->top[x] <= view.ycenter && portal->bottom[x] >= view.ycenter + 1.0f)
      {
         topplane->top[x] = (int)portal->top[x];
         topplane->bottom[x] = centery;
         bottomplane->top[x] = centery + 1;
         bottomplane->bottom[x] = (int)portal->bottom[x];
      }
      else if(portal->top[x] <= view.ycenter)
      {
         topplane->top[x] = (int)portal->top[x];
         topplane->bottom[x] = (int)portal->bottom[x];
      }
      else if(portal->bottom[x] > view.ycenter)
      {
         bottomplane->top[x] = (int)portal->top[x];
         bottomplane->bottom[x] = (int)portal->bottom[x];
      }
   }

   lastx = viewx; lasty = viewy; lastz = viewz;
   
   viewx = portal->vx;   
   viewy = portal->vy;   
   viewz = portal->vz;   

   if(portal->child)
      R_RenderHorizonPortal(portal->child);

   viewx = lastx; viewy = lasty; viewz = lastz;
}

//
// R_RenderSkyboxPortal
//
static void R_RenderSkyboxPortal(rportal_t *portal)
{
   fixed_t lastx, lasty, lastz;

   if(portal->type != R_SKYBOX)
      return;

   if(portal->maxx < portal->minx)
      return;

#ifdef RANGECHECK
   {
      int i;
      for(i = 0; i < MAX_SCREENWIDTH; i++)
      {
         if(portal->bottom[i] > portal->top[i] && (portal->bottom[i] < -1 
            || portal->bottom[i] > viewheight || portal->top[i] < -1 
            || portal->top[i] > viewheight))
            I_Error("R_RenderSkyboxPortal: clipping array contained invalid information. x:%i, ytop:%i, ybottom:%i\n", i, portal->top[i], portal->bottom[i]);
      }
   }
#endif

   if(!R_SetupPortalClipsegs(portal->top, portal->bottom))
      return;

   floorclip = portal->bottom;
   ceilingclip = portal->top;

   ++validcount;
   R_SetMaskedSilhouette(ceilingclip, floorclip);

   lastx = viewx;
   lasty = viewy;
   lastz = viewz;

   viewx = portal->data.camera.mobj->x;
   viewy = portal->data.camera.mobj->y;
   viewz = portal->data.camera.mobj->z;

   R_IncrementFrameid();
   R_RenderBSPNode(numnodes-1);
   R_PushMasked();

   floorclip = floorcliparray;
   ceilingclip = ceilingcliparray;

   viewx = lastx;
   viewy = lasty;
   viewz = lastz;

   if(portal->child)
      R_RenderSkyboxPortal(portal->child);
}

//
// R_RenderAnchoredPortal
//
static void R_RenderAnchoredPortal(rportal_t *portal)
{
   fixed_t lastx, lasty, lastz;
   float   lastxf, lastyf, lastzf;
   int     i;

#ifdef R_LINKEDPORTALS
   if(portal->type != R_ANCHORED && portal->type != R_TWOWAY && portal->type != R_LINKED)
#else
   if(portal->type != R_ANCHORED && portal->type != R_TWOWAY)
#endif
      return;

   if(portal->maxx < portal->minx)
      return;

   // haleyjd: temporary debug
   if(portal->tainted > v_width)
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
         if(portal->bottom[i] > portal->top[i] && (portal->bottom[i] < -1 
            || portal->bottom[i] > viewheight || portal->top[i] < -1 
            || portal->top[i] > viewheight))
            I_Error("R_RenderAnchoredPortal: clipping array contained invalid information. x:%i, ytop:%i, ybottom:%i\n", i, portal->top[i], portal->bottom[i]);
      }
   }
#endif

   if(!R_SetupPortalClipsegs(portal->top, portal->bottom))
      return;

   // haleyjd: temporary debug
   portal->tainted++;

   floorclip = portal->bottom;
   ceilingclip = portal->top;

   portalrender.minx = portal->minx;
   portalrender.maxx = portal->maxx;

   portalrender.miny = MAX_SCREENHEIGHT;
   portalrender.maxy = 0;
   for(i = portal->minx; i <= portal->maxx; i++)
   {
      if(portal->top[i] < portalrender.miny)
         portalrender.miny = portal->top[i];

      if(portal->bottom[i] > portalrender.maxy)
         portalrender.maxy = portal->bottom[i];
   }


   ++validcount;
   R_SetMaskedSilhouette(ceilingclip, floorclip);

   lastx = viewx;
   lasty = viewy;
   lastz = viewz;
   lastxf = view.x;
   lastyf = view.y;
   lastzf = view.z;


   // SoM 3/10/2005: Use the coordinates stored in the portal struct
   viewx = portal->vx - portal->data.camera.deltax;
   viewy = portal->vy - portal->data.camera.deltay;
   viewz = portal->vz - portal->data.camera.deltaz;
   view.x = viewx / 65536.0f;
   view.y = viewy / 65536.0f;
   view.z = viewz / 65536.0f;

   R_IncrementFrameid();
   R_RenderBSPNode(numnodes-1);

   R_PushMasked();

   floorclip = floorcliparray;
   ceilingclip = ceilingcliparray;

   viewx = lastx;
   viewy = lasty;
   viewz = lastz;
   view.x = lastxf;
   view.y = lastyf;
   view.z = lastzf;

   if(portal->child)
      R_RenderAnchoredPortal(portal->child);
}

// haleyjd: temporary debug
void R_UntaintPortals(void)
{
   rportal_t *r;
   rportal_t *child;

   for(r = portals; r; r = r->next)
   {
      r->tainted = 0;

      // haleyjd 05/19/06: must clear ALL children
      child = r->child;
      while(child)
      {
         child->tainted = 0;
         child = child->child;
      }
   }
}

//
// R_RenderPortals
//
// Primary portal rendering function.
//
void R_RenderPortals(void)
{
   rportal_t *r;

   while(1)
   {
      // SoM 3/14/2005: Set the portal rendering flag
      portalrender.active = true;
      for(r = portals; r; r = r->next)
      {
         if(r->maxx < r->minx)
            continue;

         switch(r->type)
         {
         case R_PLANE:
            R_RenderPlanePortal(r);
            break;
         case R_HORIZON:
            R_RenderHorizonPortal(r);
            break;
         case R_SKYBOX:
            R_RenderSkyboxPortal(r);
            break;
         case R_ANCHORED:
         case R_TWOWAY:
#ifdef R_LINKEDPORTALS
         case R_LINKED:
#endif
            R_RenderAnchoredPortal(r);
            break;
         }

         R_ClearPortal(r);
         break;
      }

      // SoM 3/14/2005: Unset the portal rendering flag
      portalrender.active = false;
      if(!r)
         return;
   }
}


#ifdef R_LINKEDPORTALS
// ----------------------------------------------------------------------------
// SoM: Begin linked portals

rportal_t *R_GetLinkedPortal(fixed_t deltax, fixed_t deltay, fixed_t deltaz, 
                             int groupid)
{
   rportal_t *rover, *ret;
   cameraportal_t cam;

   memset(&cam, 0, sizeof(cam));
   cam.deltax = deltax;
   cam.deltay = deltay;
   cam.deltaz = deltaz;
   cam.groupid = groupid;

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
#endif

//----------------------------------------------------------------------------
//
// $Log: r_portals.c,v $
//
//----------------------------------------------------------------------------
