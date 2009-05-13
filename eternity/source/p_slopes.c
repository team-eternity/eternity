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




// P_MakeSlope
static pslope_t *P_MakeSlope(float ox, float oy, float oz, float dx, float dy, float dz)
{
   pslope_t *ret = Z_Malloc(sizeof(pslope_t), PU_LEVEL, NULL);
   memset(ret, 0, sizeof(*ret));

   ret->ox = M_FloatToFixed(ret->oxf = ox);
   ret->oy = M_FloatToFixed(ret->oyf = oy);
   ret->oz = M_FloatToFixed(ret->ozf = oz);

   ret->dx = M_FloatToFixed(ret->dxf = dx);
   ret->dy = M_FloatToFixed(ret->dyf = dy);
   ret->dz = M_FloatToFixed(ret->dzf = dz);

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
   float ox, oy;
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

   ox = (line->v2->fx + line->v1->x) * 0.5f;
   oy = (line->v2->fy + line->v1->y) * 0.5f;

   if(frontfloor)
   {
      // SoM: TODO this is for testing and development.
      dz = -0.2f;
      fslope = line->frontsector->f_slope = P_MakeSlope(ox, oy, line->backsector->floorheightf, line->nx, line->ny, dz);
   }
   else if(backfloor)
   {
      // SoM: TODO this is for testing and development.
      dz = -0.2f;
      fslope = line->backsector->f_slope = P_MakeSlope(ox, oy, line->frontsector->floorheightf, line->ny, -line->nx, dz);
   }

   if(frontceil)
   {
      // SoM: TODO this is for testing and development.
      dz = 0.2f;
      cslope = line->frontsector->c_slope = P_MakeSlope(ox, oy, line->backsector->ceilingheightf, line->nx, line->ny, dz);
   }
   else if(backceil)
   {
      // SoM: TODO this is for testing and development.
      dz = 0.2f;
      cslope = line->backsector->c_slope = P_MakeSlope(ox, oy, line->frontsector->ceilingheightf, line->ny, -line->nx, dz);
   }


   // Check for copy linedefs
   for(i = -1; (i = P_FindLineFromLineTag(line, i)) >= 0; )
   {
      line_t *dline = lines + i;

      if(!dline->frontsector || dline->special < 394 || dline->special > 396)
         continue;

      if((dline->special - 393) & 1 && fslope)
         dline->frontsector->f_slope = P_MakeSlope(fslope->oxf, fslope->oyf, fslope->ozf, fslope->dxf, fslope->dyf, fslope->dzf);
      if((dline->special - 393) & 2 && cslope)
         dline->frontsector->c_slope = P_MakeSlope(cslope->oxf, cslope->oyf, cslope->ozf, cslope->dxf, cslope->dyf, cslope->dzf);
   }
}
