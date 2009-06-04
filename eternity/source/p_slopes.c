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
//      Slopes
//      SoM created 05/10/09
//
//-----------------------------------------------------------------------------

#include "doomdef.h"
#include "r_defs.h"
#include "r_state.h"
#include "m_bbox.h"
#include "p_spec.h"
#include "p_slopes.h"




// P_CrossProduct3f
// Gets the cross product of v1 and v2 and stores in dest 
void P_CrossProduct3f(v3float_t *dest, const v3float_t *v1, const v3float_t *v2)
{
   v3float_t tmp;
   tmp.x = (v1->y * v2->z) - (v1->z * v2->y);
   tmp.y = (v1->z * v2->x) - (v1->x * v2->z);
   tmp.z = (v1->x * v2->y) - (v1->y * v2->x);
   *dest = tmp;
}


// P_SubVec3f
// Subtracts v2 from v1 and stores the result in dest
void P_SubVec3f(v3float_t *dest, const v3float_t *v1, const v3float_t *v2)
{
   v3float_t tmp;
   tmp.x = (v1->x - v2->x);
   tmp.y = (v1->y - v2->y);
   tmp.z = (v1->z - v2->z);
   *dest = tmp;
}


// P_CrossVec3f
float P_CrossVec3f(const v3float_t *v1, const v3float_t *v2)
{
   return (v1->x * v2->x) + (v1->y * v2->y) + (v1->z * v2->z);
} 




// P_MakeSlope
// Alocates and fill the contents of a slope structure.
static pslope_t *P_MakeSlope(const v3float_t *o, const v2float_t *d, const float zdelta, boolean isceiling)
{
   pslope_t *ret = Z_Malloc(sizeof(pslope_t), PU_LEVEL, NULL);
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
         P_SubVec3f(&d1, &v1, &v3);
         P_SubVec3f(&d2, &v2, &v3);
      }
      else
      {
         P_SubVec3f(&d1, &v1, &v2);
         P_SubVec3f(&d2, &v3, &v2);
      }

      P_CrossProduct3f(&ret->normalf, &d1, &d2);

      len = (float)sqrt(ret->normalf.x * ret->normalf.x +
                        ret->normalf.y * ret->normalf.y + 
                        ret->normalf.z * ret->normalf.z);

      ret->normalf.x /= len;
      ret->normalf.y /= len;
      ret->normalf.z /= len;
   }

   return ret;
}


// P_CopySlope
// Allocates and returns a copy of the given slope structure.
static pslope_t *P_CopySlope(const pslope_t *src)
{
   pslope_t *ret = Z_Malloc(sizeof(pslope_t), PU_LEVEL, NULL);
   memcpy(ret, src, sizeof(*ret));

   return ret;
}



// P_MakeLineNormal
// Calculates a 2D normal for the given line and stores it in the line
void P_MakeLineNormal(line_t *line)
{
   float linedx, linedy, length;

   linedx = line->v2->fx - line->v1->fx;
   linedy = line->v2->fy - line->v1->fy;

   length = (float)sqrt(linedx * linedx + linedy * linedy);
   line->nx = linedy / length;
   line->ny = -linedx / length;
}


// P_GetExtent
// Returns the distance to the first line within the sector that
// is intersected by a line parallel to the plane normal with the point (ox, oy) 
float P_GetExtent(sector_t *sector, line_t *line, float ox, float oy)
{
   return 0.0f;
}




// P_SpawnSlope_Line
// Creates one or more slopes based on the given line type and front/back
// sectors.
void P_SpawnSlope_Line(int linenum)
{
   line_t *line = lines + linenum;
   int special = line->special;
   pslope_t *fslope = NULL, *cslope = NULL;
   v3float_t origin;
   v2float_t direction;
   float dz;
   int   i;

   boolean frontfloor = (special == 386 || special == 388 || special == 393);
   boolean backfloor = (special == 389 || special == 391 || special == 392);
   boolean frontceil = (special == 387 || special == 388 || special == 392);
   boolean backceil = (special == 390 || special == 391 || special == 393);

   if(!frontfloor && !backfloor && !frontceil && !backceil)
      I_Error("P_SpawnSlope_Line called with non-slope line special.");
   if(!line->frontsector || !line->backsector)
      I_Error("P_SpawnSlope_Line used on a line without two sides.");

   origin.x = (line->v2->fx + line->v1->fx) * 0.5f;
   origin.y = (line->v2->fy + line->v1->fy) * 0.5f;

   // Note to self: This is really backwards. The origin really needs to be 
   // from the actual floor/ceiling height of the sector and slope away.
   // Done the way it is currently, moving slopes along with sectors will 
   // be kind of a pain...
   if(frontfloor)
   {
      // SoM: TODO this is for testing and development.
      direction.x = line->nx;
      direction.y = line->ny;
      origin.z = line->backsector->floorheightf;
      dz = -0.2f;

      fslope = line->frontsector->f_slope = 
         P_MakeSlope(&origin, &direction, dz, false);
   }
   else if(backfloor)
   {
      // SoM: TODO this is for testing and development.
      direction.x = line->ny;
      direction.y = -line->nx;
      origin.z = line->frontsector->floorheightf;
      dz = -0.2f;
      fslope = line->backsector->f_slope = 
         P_MakeSlope(&origin, &direction, dz, false);
   }

   if(frontceil)
   {
      // SoM: TODO this is for testing and development.
      direction.x = line->nx;
      direction.y = line->ny;
      origin.z = line->backsector->ceilingheightf;
      dz = 0.2f;
      cslope = line->frontsector->c_slope = 
         P_MakeSlope(&origin, &direction, dz, true);
   }
   else if(backceil)
   {
      // SoM: TODO this is for testing and development.
      direction.x = line->ny;
      direction.y = -line->nx;
      origin.z = line->frontsector->ceilingheightf;
      dz = 0.2f;
      cslope = line->backsector->c_slope = 
         P_MakeSlope(&origin, &direction, dz, true);
   }


   // Check for copy linedefs
   for(i = -1; (i = P_FindLineFromLineTag(line, i)) >= 0; )
   {
      line_t *dline = lines + i;

      if(!dline->frontsector || dline->special < 394 || dline->special > 396)
         continue;

      if((dline->special - 393) & 1 && fslope)
         dline->frontsector->f_slope = P_CopySlope(fslope);
      if((dline->special - 393) & 2 && cslope)
         dline->frontsector->c_slope = P_CopySlope(cslope);
   }
}




// ----------------------------------------------------------------------------
// Various utilities related to slopes


// Returns the height of the sloped plane at (x, y) as a fixed_t
fixed_t P_GetZAt(pslope_t *slope, fixed_t x, fixed_t y)
{
   fixed_t dist = FixedMul(x - slope->o.x, slope->d.x) +
                  FixedMul(y - slope->o.y, slope->d.y);

   return slope->o.z + FixedMul(dist, slope->zdelta);
}



// Returns the height of the sloped plane at (x, y) as a float
float P_GetZAtf(pslope_t *slope, float x, float y)
{
   float dist = (x - slope->of.x) * slope->df.x + (y - slope->of.y) * slope->df.y;
   return slope->of.z + (dist * slope->zdeltaf);
}



float P_DistFromPlanef(const v3float_t *point, const v3float_t *pori, const v3float_t *pnormal)
{
   return (point->x - pori->x) * pnormal->x + 
          (point->y - pori->y) * pnormal->y +
          (point->z - pori->z) * pnormal->z;
}

// EOF

