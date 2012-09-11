// Emacs style mode select   -*- C++ -*- vi:ts=3:sw=3:set et: 
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
//      Slopes
//      SoM created 05/10/09
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "doomdef.h"
#include "m_bbox.h"
#include "p_slopes.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_state.h"
#include "v_misc.h"

//
// P_MakeSlope
//
// Alocates and fill the contents of a slope structure.
//
static pslope_t *P_MakeSlope(const v3float_t *o, const v2float_t *d, 
                             const float zdelta, bool isceiling)
{
   pslope_t *ret = (pslope_t *)(Z_Malloc(sizeof(pslope_t), PU_LEVEL, NULL));
   memset(ret, 0, sizeof(*ret));

   ret->o.x = M_FloatToFixed(ret->of.x = o->x);
   ret->o.y = M_FloatToFixed(ret->of.y = o->y);
   ret->o.z = M_FloatToFixed(ret->of.z = o->z);

   ret->d.x = M_FloatToFixed(ret->df.x = d->x);
   ret->d.y = M_FloatToFixed(ret->df.y = d->y);

   ret->zdelta = M_FloatToFixed(ret->zdeltaf = zdelta);

   {
      v3float_t v1, v2, v3, d1, d2;
      float len;

      v1.x = o->x;
      v1.y = o->y;
      v1.z = o->z;

      v2.x = v1.x;
      v2.y = v1.y + 10.0f;
      v2.z = P_GetZAtf(ret, v2.x, v2.y);

      v3.x = v1.x + 10.0f;
      v3.y = v1.y;
      v3.z = P_GetZAtf(ret, v3.x, v3.y);

      if(isceiling)
      {
         M_SubVec3f(&d1, &v1, &v3);
         M_SubVec3f(&d2, &v2, &v3);
      }
      else
      {
         M_SubVec3f(&d1, &v1, &v2);
         M_SubVec3f(&d2, &v3, &v2);
      }

      M_CrossProduct3f(&ret->normalf, &d1, &d2);

      len = (float)sqrt(ret->normalf.x * ret->normalf.x +
                        ret->normalf.y * ret->normalf.y + 
                        ret->normalf.z * ret->normalf.z);

      ret->normalf.x /= len;
      ret->normalf.y /= len;
      ret->normalf.z /= len;
   }

   return ret;
}

//
// P_CopySlope
//
// Allocates and returns a copy of the given slope structure.
//
static pslope_t *P_CopySlope(const pslope_t *src)
{
   pslope_t *ret = (pslope_t *)(Z_Malloc(sizeof(pslope_t), PU_LEVEL, NULL));
   memcpy(ret, src, sizeof(*ret));

   return ret;
}

//
// P_MakeLineNormal
//
// Calculates a 2D normal for the given line and stores it in the line
//
void P_MakeLineNormal(line_t *line)
{
   float linedx, linedy, length;

   linedx = line->v2->fx - line->v1->fx;
   linedy = line->v2->fy - line->v1->fy;

   length   = (float)sqrt(linedx * linedx + linedy * linedy);
   line->nx =  linedy / length;
   line->ny = -linedx / length;
}

//
// P_GetExtent
//
// Returns the distance to the first line within the sector that
// is intersected by a line parallel to the plane normal with the point (ox, oy)
//
float P_GetExtent(sector_t *sector, line_t *line, v3float_t *o, v2float_t *d)
{
   float fardist = -1.0f;
   int i;

   // Poll all the lines and find the vertex that is the furthest away from
   // the slope line.
   for(i = 0; i < sector->linecount; i++)
   {
      line_t *li = sector->lines[i];
      float dist;
      
      // Don't compare to the slope line.
      if(li == line)
         continue;
      
      dist = (float)fabs((li->v1->fx - o->x) * d->x + (li->v1->fy - o->y) * d->y);
      if(dist > fardist)
         fardist = dist;

      dist = (float)fabs((li->v2->fx - o->x) * d->x + (li->v2->fy - o->y) * d->y);
      if(dist > fardist)
         fardist = dist;
   }

   return fardist;
}

//
// P_SpawnSlope_Line
//
// Creates one or more slopes based on the given line type and front/back
// sectors.
//
void P_SpawnSlope_Line(int linenum)
{
   line_t *line = lines + linenum;
   int special = line->special;
   v3float_t origin, point;
   v2float_t direction;
   float dz, extent;

   bool frontfloor = (special == 386 || special == 388 || special == 393);
   bool backfloor  = (special == 389 || special == 391 || special == 392);
   bool frontceil  = (special == 387 || special == 388 || special == 392);
   bool backceil   = (special == 390 || special == 391 || special == 393);

   // SoM: We don't need the line to retain its special type
   line->special = 0;

   if(!(frontfloor || backfloor || frontceil || backceil))
   {
      C_Printf(FC_ERROR "P_SpawnSlope_Line: called with non-slope line special.");
      return;
   }

   if(!line->backsector)
   {
      C_Printf(FC_ERROR "P_SpawnSlope_Line: used on one-sided line.");
      return;
   }

   origin.x = (line->v2->fx + line->v1->fx) * 0.5f;
   origin.y = (line->v2->fy + line->v1->fy) * 0.5f;

   if(frontfloor || frontceil)
   {
      // Do the front sector
      direction.x = line->nx;
      direction.y = line->ny;

      extent = P_GetExtent(line->frontsector, line, &origin, &direction);

      if(extent < 0.0f)
      {
         C_Printf(FC_ERROR "P_SpawnSlope_Line: no frontsector extent for line %d\n", linenum);
         return;
      }

      // reposition the origin according to the extent
      point.x = origin.x + direction.x * extent;
      point.y = origin.y + direction.y * extent;
      direction.x = -direction.x;
      direction.y = -direction.y;

      if(frontfloor)
      {
         point.z = line->frontsector->floorheightf;
         dz = (line->backsector->floorheightf - point.z) / extent;

         line->frontsector->f_slope = P_MakeSlope(&point, &direction, dz, false);
      }
      if(frontceil)
      {
         point.z = line->frontsector->ceilingheightf;
         dz = (line->backsector->ceilingheightf - point.z) / extent;

         line->frontsector->c_slope = P_MakeSlope(&point, &direction, dz, true);
      }
   }

   if(backfloor || backceil)
   {
      // Backsector
      direction.x = -line->nx;
      direction.y = -line->ny;

      extent = P_GetExtent(line->backsector, line, &origin, &direction);

      if(extent < 0.0f)
      {
         C_Printf(FC_ERROR "P_SpawnSlope_Line: no backsector extent for line %d\n", linenum);
         return;
      }

      // reposition the origin according to the extent
      point.x = origin.x + direction.x * extent;
      point.y = origin.y + direction.y * extent;
      direction.x = -direction.x;
      direction.y = -direction.y;

      if(backfloor)
      {
         point.z = line->backsector->floorheightf;
         dz = (line->frontsector->floorheightf - point.z) / extent;

         line->backsector->f_slope = P_MakeSlope(&point, &direction, dz, false);
      }
      if(backceil)
      {
         point.z = line->backsector->ceilingheightf;
         dz = (line->frontsector->ceilingheightf - point.z) / extent;

         line->backsector->c_slope = P_MakeSlope(&point, &direction, dz, true);
      }
   }

   /*
   // haleyjd: what's this?? inconsequential.
   if(!line->tag)
      return;
   */
}



//
// P_CopySectorSlope
//
// Searches through tagged sectors and copies
//
void P_CopySectorSlope(line_t *line)
{
   sector_t *fsec = line->frontsector;
   int i, special = line->special;

   // Check for copy linedefs
   for(i = -1; (i = P_FindSectorFromLineTag(line, i)) >= 0;)
   {
      sector_t *srcsec = sectors + i;

      if((special - 393) & 1 && !fsec->f_slope && srcsec->f_slope)
         fsec->f_slope = P_CopySlope(srcsec->f_slope);
      if((special - 393) & 2 && !fsec->c_slope && srcsec->c_slope)
         fsec->c_slope = P_CopySlope(srcsec->c_slope);
   }

   line->special = 0;
}


// ============================================================================
//
// Various utilities related to slopes
//

//
// P_GetZAt
//
// Returns the height of the sloped plane at (x, y) as a fixed_t
//
fixed_t P_GetZAt(pslope_t *slope, fixed_t x, fixed_t y)
{
   fixed_t dist = FixedMul(x - slope->o.x, slope->d.x) +
                  FixedMul(y - slope->o.y, slope->d.y);

   return slope->o.z + FixedMul(dist, slope->zdelta);
}

//
// P_GetZAtf
//
// Returns the height of the sloped plane at (x, y) as a float
//
float P_GetZAtf(pslope_t *slope, float x, float y)
{
   float dist = (x - slope->of.x) * slope->df.x + (y - slope->of.y) * slope->df.y;
   return slope->of.z + (dist * slope->zdeltaf);
}

//
// P_DistFromPlanef
//
float P_DistFromPlanef(const v3float_t *point, const v3float_t *pori, 
                       const v3float_t *pnormal)
{
   return (point->x - pori->x) * pnormal->x + 
          (point->y - pori->y) * pnormal->y +
          (point->z - pori->z) * pnormal->z;
}

// EOF

