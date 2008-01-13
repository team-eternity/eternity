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
//      SoM created 12/23/07
//
//-----------------------------------------------------------------------------

#ifndef R_PORTALCHECK_H__
#define R_PORTALCHECK_H__


#ifdef R_LINKEDPORTALS
d_inline static boolean   R_LinkedFloorActive(sector_t *sector)
{
   return (useportalgroups && sector->f_portal && 
           sector->f_portal->type == R_LINKED &&
           sector->floorz <= sector->f_portal->data.camera.planez);
}


d_inline static boolean   R_LinkedCeilingActive(sector_t *sector)
{
   return (useportalgroups && sector->c_portal && 
           sector->c_portal->type == R_LINKED &&
           sector->ceilingz >= sector->c_portal->data.camera.planez);
}


d_inline static boolean   R_LinkedLineActive(line_t *line)
{
   return (useportalgroups && line->portal && line->portal->type == R_LINKED);
}
#endif


d_inline static fixed_t R_GetFloorPlanez(sector_t *sector)
{
   return 
#ifdef R_LINKEDPORTALS
      sector->f_portal && sector->f_portal->type == R_LINKED && 
      sector->floorz <= sector->f_portal->data.camera.planez ?
      sector->f_portal->data.camera.planez :
#endif
      sector->floorz;
}


d_inline static fixed_t R_GetCeilingPlanez(sector_t *sector)
{
   return
#ifdef R_LINKEDPORTALS
      sector->c_portal && sector->c_portal->type == R_LINKED && 
      sector->ceilingz >= sector->c_portal->data.camera.planez ?
      sector->c_portal->data.camera.planez :
#endif
      sector->ceilingz;
}


d_inline static boolean R_FloorPortalActive(sector_t *sector)
{
   return 
#ifdef R_LINKEDPORTALS
      sector->f_portal && sector->f_portal->type == R_LINKED && 
      sector->floorz > sector->f_portal->data.camera.planez ? false : 
#endif
      (sector->f_portal != NULL);
}


d_inline static boolean R_CeilingPortalActive(sector_t *sector)
{
   return 
#ifdef R_LINKEDPORTALS
      sector->c_portal && sector->c_portal->type == R_LINKED && 
      sector->ceilingz < sector->c_portal->data.camera.planez ? false : 
#endif
      (sector->c_portal != NULL);
}


d_inline static boolean R_RenderFloorPortal(sector_t *sector)
{
   const rportal_t *fp = sector->f_portal;

#ifdef R_LINKEDPORTALS
   return 
      (fp &&
       ((fp->type != R_TWOWAY && fp->type != R_LINKED) ||
        (fp->type == R_TWOWAY && sector->floorheight < viewz) ||         
        (fp->type == R_LINKED && sector->floorheight < viewz && 
                                 sector->floorz <= fp->data.camera.planez)));
#else
   return (fp && (fp->type != R_TWOWAY || sector->floorheight < viewz));
#endif
}




d_inline static boolean R_RenderCeilingPortal(sector_t *sector)
{
   const rportal_t *cp = sector->c_portal;

#ifdef R_LINKEDPORTALS
   return 
      (cp &&
       ((cp->type != R_TWOWAY && cp->type != R_LINKED) ||
        (cp->type == R_TWOWAY && sector->ceilingheight > viewz) ||         
        (cp->type == R_LINKED && sector->ceilingheight > viewz && 
                                 sector->ceilingz >= cp->data.camera.planez)));
#else
   return (cp && (cp->type != R_TWOWAY || sector->ceilingheight > viewz));
#endif
}

#endif

//----------------------------------------------------------------------------
//
// $Log: r_pcheck.h,v $
//
//----------------------------------------------------------------------------
