// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2008 James Haley, Stephen McGranahan, et al.
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
//      Inline linked portal predicates.
//      SoM created 12/23/07
//
//-----------------------------------------------------------------------------

#ifndef R_PORTALCHECK_H__
#define R_PORTALCHECK_H__

extern int demo_version;

d_inline static boolean R_LinkedFloorActive(sector_t *sector)
{
   return (useportalgroups && 
           sector->f_portal && 
           sector->f_portal->type == R_LINKED &&
           sector->floorheight <= sector->f_portal->data.camera.planez);
}


d_inline static boolean R_LinkedCeilingActive(sector_t *sector)
{
   return (useportalgroups && 
           sector->c_portal && 
           sector->c_portal->type == R_LINKED &&
           sector->ceilingheight >= sector->c_portal->data.camera.planez);
}


d_inline static boolean R_LinkedLineActive(line_t *line)
{
   return (useportalgroups && 
           line->portal && line->portal->type == R_LINKED);
}

d_inline static boolean R_FloorPortalActive(sector_t *sector)
{
   // FIXME: possible to eliminate branch?
   return 
      (sector->f_portal && sector->f_portal->type == R_LINKED && 
       sector->floorheight > sector->f_portal->data.camera.planez) ? 
      false : 
      (sector->f_portal != NULL);
}

d_inline static boolean R_CeilingPortalActive(sector_t *sector)
{
   // FIXME: possible to eliminate branch?
   return 
      (sector->c_portal && sector->c_portal->type == R_LINKED && 
       sector->ceilingheight < sector->c_portal->data.camera.planez) ? 
      false : 
      (sector->c_portal != NULL);
}

d_inline static boolean R_RenderFloorPortal(sector_t *sector)
{
   const portal_t *fp = sector->f_portal;

#ifdef R_LINKEDPORTALS
   return 
      (fp &&
       ((fp->type != R_TWOWAY && fp->type != R_LINKED) ||
        (fp->type == R_TWOWAY && sector->floorheight < viewz) ||         
        (fp->type == R_LINKED && sector->floorheight <= viewz && 
                                 sector->floorheight <= fp->data.camera.planez)));
#else
   return (fp && (fp->type != R_TWOWAY || sector->floorheight < viewz));
#endif
}

d_inline static boolean R_RenderCeilingPortal(sector_t *sector)
{
   const portal_t *cp = sector->c_portal;

#ifdef R_LINKEDPORTALS
   return 
      (cp &&
       ((cp->type != R_TWOWAY && cp->type != R_LINKED) ||
        (cp->type == R_TWOWAY && sector->ceilingheight > viewz) ||         
        (cp->type == R_LINKED && sector->ceilingheight > viewz && 
                                 sector->ceilingheight >= cp->data.camera.planez)));
#else
   return (cp && (cp->type != R_TWOWAY || sector->ceilingheight > viewz));
#endif
}

//
// R_FPCam
//
// haleyjd 3/17/08: Convenience routine to clean some shit up.
//
d_inline static cameraportal_t *R_FPCam(sector_t *s)
{
   return &(s->f_portal->data.camera);
}

//
// R_CPCam
//
// ditto
//
d_inline static cameraportal_t *R_CPCam(sector_t *s)
{
   return &(s->c_portal->data.camera);
}


#endif

//----------------------------------------------------------------------------
//
// $Log: r_pcheck.h,v $
//
//----------------------------------------------------------------------------
