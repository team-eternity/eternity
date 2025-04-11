//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//      All the clipping: columns, horizontal spans, sky columns.
//
//-----------------------------------------------------------------------------

// 4/25/98, 5/2/98 killough: reformatted, beautified

#include "concurrentqueue/concurrentqueue.h"

#include "z_zone.h"
#include "i_system.h"

#include "doomstat.h"
#include "e_exdata.h"
#include "m_compare.h"
#include "p_info.h"
#include "p_user.h"
#include "r_draw.h"
#include "r_bsp.h"
#include "r_context.h"
#include "r_data.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_portal.h"
#include "r_segs.h"
#include "r_state.h"
#include "r_things.h"
#include "w_wad.h"

static moodycamel::ConcurrentQueue<intptr_t> g_linesToMap;

//
// Take all the lines to map after multithreaded contexts are done,
// and apply the mapped flag to them.
//
void R_FinishMappingLines()
{
   intptr_t lineIndex = -1;
   while(g_linesToMap.try_dequeue(lineIndex))
      lines[lineIndex].flags |= ML_MAPPED;
}

//
// R_RenderMaskedSegRange
//
void R_RenderMaskedSegRange(cmapcontext_t &cmapcontext,
                            const viewpoint_t &viewpoint, drawseg_t *ds, int x1, int x2)
{
   static thread_local rendersector_t tempsec; // killough 4/13/98
   static thread_local Surfaces<pslope_t> tempslopes;

   const texcol_t *col;
   int      lightnum;
   int      texnum;
   float    dist, diststep;
   float    scale, scalestep;
   float    texmidf;
   line_t  *linedef;
   const lighttable_t *const *wlight;
   float   *maskedtexturecol;

   cb_column_t column{};
   cb_seg_t    segclip{};

   // Calculate light table.
   // Use different light tables
   //   for horizontal / vertical / diagonal. Diagonal?

   segclip.line = ds->curline;
   linedef      = segclip.line->linedef;

   R_ColumnFunc colfunc = r_column_engine->DrawColumn;

   // killough 4/11/98: draw translucent 2s normal textures
   if(general_translucency)
   {
      if(linedef->tranlump >= 0)
      {
         colfunc = r_column_engine->DrawTLColumn;
         if(linedef->tranlump > 0)
            column.tranmap = static_cast<byte *>(wGlobalDir.getCachedLumpNum(linedef->tranlump-1));
         else
            column.tranmap = main_tranmap;
      }
      else // haleyjd 11/11/10: flex/additive translucency for linedefs
      {
         if(linedef->alpha == 0.0f)
            return;

         if(linedef->extflags & EX_ML_ADDITIVE) 
         {
            colfunc = r_column_engine->DrawAddColumn;
            column.translevel = M_FloatToFixed(linedef->alpha);
         }
         else if(linedef->alpha < 1.0f)
         {
            colfunc = r_column_engine->DrawFlexColumn;
            column.translevel = M_FloatToFixed(linedef->alpha);
         }
      }
   }
   // killough 4/11/98: end translucent 2s normal code

   segclip.frontsec = segclip.line->frontsector;
   segclip.backsec  = segclip.line->backsector;

   texnum = texturetranslation[segclip.line->sidedef->midtexture];

   // killough 4/13/98: get correct lightlevel for 2s normal textures
   if(segclip.line->sidedef->flags & SDF_LIGHT_MID_ABSOLUTE)
      lightnum = segclip.line->sidedef->light_mid >> LIGHTSEGSHIFT;
   else
   {
      if(segclip.line->sidedef->flags & SDF_LIGHT_BASE_ABSOLUTE)
         lightnum = segclip.line->sidedef->light_base >> LIGHTSEGSHIFT;
      else
      {
         lightnum = R_FakeFlat(viewpoint, segclip.line->frontsector, &tempsec, tempslopes, nullptr, nullptr,
                               false)->lightlevel >> LIGHTSEGSHIFT;
         lightnum += segclip.line->sidedef->light_base >> LIGHTSEGSHIFT;
      }
      lightnum += segclip.line->sidedef->light_mid >> LIGHTSEGSHIFT;

   }
   lightnum += (extralight * LIGHTBRIGHT);


   // haleyjd 08/11/00: optionally skip this to evenly apply colormap
   if(LevelInfo.unevenLight)
   {  
      if(linedef->v1->y == linedef->v2->y)
         lightnum -= LIGHTBRIGHT;
      else if(linedef->v1->x == linedef->v2->x)
         lightnum += LIGHTBRIGHT;
   }

   // SoM 10/19/02: deep water colormap fix
   wlight = 
      ds->colormap[lightnum >= LIGHTLEVELS || cmapcontext.fixedcolormap ?
                   LIGHTLEVELS-1 :
                   lightnum <  0 ? 0 : lightnum ] ;

   maskedtexturecol = ds->maskedtexturecol;

   diststep = ds->diststep;
   dist = ds->dist1 + (x1 - ds->x1) * diststep;

   // find positioning
   if(linedef->flags & ML_DONTPEGBOTTOM)
   {
      column.texmid = segclip.frontsec->srf.floor.height > segclip.backsec->srf.floor.height
         ? segclip.frontsec->srf.floor.height : segclip.backsec->srf.floor.height;
      column.texmid = column.texmid + textures[texnum]->heightfrac - viewpoint.z;
   }
   else
   {
      column.texmid = segclip.frontsec->srf.ceiling.height < segclip.backsec->srf.ceiling.height
         ? segclip.frontsec->srf.ceiling.height : segclip.backsec->srf.ceiling.height;
      column.texmid = column.texmid - viewpoint.z;
   }

   column.texmid += segclip.line->sidedef->offset_base_y + segclip.line->sidedef->offset_mid_y - ds->deltaz;
   
   // SoM 10/19/02: deep water colormap fixes
   //if (fixedcolormap)
   //   column.colormap = fixedcolormap;
   if(ds->fixedcolormap)
      column.colormap = ds->fixedcolormap;

   // SoM: performance tuning (tm Lee Killough 1998)
   scale     = dist * view.yfoc;
   scalestep = diststep * view.yfoc;
   texmidf   = M_FixedToFloat(column.texmid);

   // draw the columns
   for(column.x = x1; column.x <= x2; ++column.x, dist += diststep, scale += scalestep)
   {
      if(maskedtexturecol[column.x] != FLT_MAX)
      {
         if(!ds->fixedcolormap)
         {                             // killough 11/98:
            // SoM: ANYRES
            int index = (int)(dist * 2560.0f);
            
            if(index >=  MAXLIGHTSCALE )
               index = MAXLIGHTSCALE - 1;
            
            column.colormap = wlight[index];
         }


         const cb_maskedcolumn_t maskedcolumn = { view.ycenter - (texmidf * scale), scale };
         column.step = (int)(FPFRACUNIT / scale);

         // killough 1/25/98: here's where Medusa came in, because
         // it implicitly assumed that the column was all one patch.
         // Originally, Doom did not construct complete columns for
         // multipatched textures, so there were no header or trailer
         // bytes in the column referred to below, which explains
         // the Medusa effect. The fix is to construct true columns
         // when forming multipatched textures (see r_data.c).

         // draw the texture
         col = R_GetMaskedColumn(texnum, (int)(maskedtexturecol[column.x]));
         R_DrawNewMaskedColumn(
            colfunc, column, maskedcolumn,
            textures[texnum], col, ds->sprbottomclip, ds->sprtopclip,
            ds->maskedtextureskew[column.x]
         );

         maskedtexturecol[column.x] = FLT_MAX;
      }
   }
}


static const lighttable_t *R_calculateLighting(cmapcontext_t &cmapcontext, const lighttable_t *const *walllights, const float dist)
{
   // calculate lighting
   // SoM: ANYRES
   if(!cmapcontext.fixedcolormap)
   {
      // SoM: it took me about 5 solid minutes of looking at the old doom code
      // and running test levels through it to do the math and get 2560 as the
      // light distance factor.
      int index = int(dist * 2560.0f);

      if(index >= MAXLIGHTSCALE)
         index = MAXLIGHTSCALE - 1;

      return walllights[index];
   }

   return cmapcontext.fixedcolormap;
}



//
// Draws zero, one, or two textures (and possibly a masked texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling textures.
// CALLED: CORE LOOPING ROUTINE.
//
static void R_renderSegLoop(cmapcontext_t &cmapcontext, planecontext_t &planecontext,
                            portalcontext_t &portalcontext, ZoneHeap &heap,
                            const R_ColumnFunc colfunc, const viewpoint_t &viewpoint,
                            const cbviewpoint_t &cb_viewpoint, const contextbounds_t &bounds,
                            drawseg_t *const ds_p, cb_seg_t &segclip)
{
#ifdef RANGECHECK
   if(segclip.x1 < bounds.startcolumn || segclip.x2 >= bounds.endcolumn || segclip.x1 > segclip.x2)
   {
      I_Error("R_renderSegLoop: invalid seg x values!\n"
              "   x1 = %d, x2 = %d, linenum = %d\n", 
              segclip.x1, segclip.x2,
              static_cast<int>(segclip.line->linedef - lines));
   }
#endif

   float     *const floorclip   = planecontext.floorclip;
   float     *const ceilingclip = planecontext.ceilingclip;

   int t, b, line;
   int cliptop, clipbot;
   int i;
   float texx;
   float basescale;

   cb_column_t column{};


   visplane_t *skyplane = nullptr;
   if(segclip.skyflat)
   {
      // Use value -1 which is extremely hard to reach, and different to the hardcoded ceiling 1,
      // to avoid HOM
      skyplane = R_FindPlane(
         cmapcontext, planecontext, heap, viewpoint, cb_viewpoint,
         bounds, viewpoint.z - 1, segclip.skyflat,
         144, {}, { 1, 1 }, 0, nullptr, 0, 255, nullptr
      );
      skyplane = R_CheckPlane(planecontext, heap, skyplane, segclip.x1, segclip.x2);
   }

   // haleyjd 06/30/07: cardboard invuln fix.
   // haleyjd 10/21/08: moved up loop-invariant calculation
   if(cmapcontext.fixedcolormap)
      column.colormap = cmapcontext.fixedcolormap;

   for(i = segclip.x1; i <= segclip.x2; i++)
   {
      cliptop = (int)ceilingclip[i];
      clipbot = (int)floorclip[i];

      t = segclip.top    < ceilingclip[i] ? cliptop : (int)segclip.top;
      b = segclip.bottom > floorclip[i]   ? clipbot : (int)segclip.bottom;

      // SoM 3/10/2005: Only add to the portal of the ceiling is marked
      if(segclip.markflags & (SEG_MARKCPORTAL|SEG_MARKCEILING|SEG_MARKCOVERLAY))
      {
         line = t - 1;
         
         if(line > clipbot)
            line = clipbot;
         
         if(line >= cliptop)
         {
            if(segclip.markflags & SEG_MARKCOVERLAY)
            {
               int otop = ceilingclip[i] > overlaycclip[i] ? cliptop : (int)overlaycclip[i];
               
               if(segclip.plane.ceiling && line >= otop)
               {
                  segclip.plane.ceiling->top[i]    = otop;
                  segclip.plane.ceiling->bottom[i] = line;
               }

               overlaycclip[i] = (float)t;
            }
            
            if(segclip.markflags & SEG_MARKCPORTAL)
            {
               R_WindowAdd(
                  planecontext, portalcontext, heap, viewpoint, bounds,
                  segclip.secwindow.ceiling, i, (float)cliptop, (float)line
               );
               ceilingclip[i] = (float)t;
            }
            else if(segclip.plane.ceiling && segclip.markflags & SEG_MARKCEILING)
            {
               segclip.plane.ceiling->top[i]    = cliptop;
               segclip.plane.ceiling->bottom[i] = line;
               ceilingclip[i] = (float)t;
            }
         }
      }
  
      // SoM 3/10/2005: Only add to the portal of the floor is marked
      if(segclip.markflags & (SEG_MARKFPORTAL|SEG_MARKFLOOR|SEG_MARKFOVERLAY))
      {
         line = b + 1;

         if(line < cliptop)
            line = cliptop;

         if(line <= clipbot)
         {
            if(segclip.markflags & SEG_MARKFOVERLAY)
            {
               int olow = floorclip[i] < overlayfclip[i] ? clipbot : (int)overlayfclip[i];
               
               if(segclip.plane.floor && line <= olow)
               {
                  segclip.plane.floor->top[i]    = line;
                  segclip.plane.floor->bottom[i] = olow;
               }

               overlayfclip[i] = (float)b;
            }
            
            if(segclip.markflags & SEG_MARKFPORTAL)
            {
               R_WindowAdd(
                  planecontext, portalcontext, heap, viewpoint, bounds,
                  segclip.secwindow.floor, i, (float)line, (float)clipbot
               );
               floorclip[i] = (float)b;
            }
            else if(segclip.plane.floor && segclip.markflags & SEG_MARKFLOOR)
            {
               segclip.plane.floor->top[i]    = line;
               segclip.plane.floor->bottom[i] = clipbot;
               floorclip[i] = (float)b;
            }
         }
      }
      
      if(segclip.segtextured)
      {
         basescale = 1.0f / (segclip.dist * view.yfoc);

         column.step = M_FloatToFixed(basescale); // SCALE_TODO: Y scale-factor here
         column.x = i;

         texx = segclip.len * basescale + segclip.toffset_base_x; // SCALE_TODO: X scale-factor here

         if(ds_p->maskedtexturecol)
            ds_p->maskedtexturecol[i] = texx + segclip.toffset_mid_x;

         if(ds_p->maskedtextureskew)
         {
            if(segclip.skew_mid_step && segclip.side->middleSkewType() != SKEW_NONE)
               ds_p->maskedtextureskew[i] = segclip.skew_mid_step * (segclip.len * basescale) + segclip.skew_mid_baseoffset;
            else
               ds_p->maskedtextureskew[i] = 0.0f; // Don't think this is strictly necessary but do this for safety.
         }

         if(!segclip.twosided)
         {
            if(segclip.l_window)
            {
               // ioanch 20160312
               if(segclip.markflags & SEG_MARK1SLPORTAL)
               {
                  if(segclip.toptex)
                  {
                     // ioanch FIXME: copy-paste from other code
                     column.y1 = t;
                     column.y2 = (int)(segclip.high > floorclip[i] ? floorclip[i] : segclip.high);
                     if(column.y2 >= column.y1)
                     {
                        column.colormap = R_calculateLighting(cmapcontext, segclip.walllights_top, segclip.dist);
                        column.texmid = segclip.toptexmid;
                        column.source = R_GetRawColumn(heap, segclip.toptex, int(texx + segclip.toffset_top_x));
                        column.texheight = segclip.toptexh;
                        colfunc(column);
                        ceilingclip[i] = (float)(column.y2 + 1);
                     }
                     else
                        ceilingclip[i] = (float)t;

                     segclip.high += segclip.highstep;
                     
                  }

                  if(segclip.bottomtex)
                  {
                     column.y1 = (int)(segclip.low < ceilingclip[i] ? ceilingclip[i] : segclip.low);
                     column.y2 = b;
                     if(column.y2 >= column.y1)
                     {
                        column.colormap = R_calculateLighting(cmapcontext, segclip.walllights_bottom, segclip.dist);
                        column.texmid = segclip.bottomtexmid;
                        column.source = R_GetRawColumn(heap, segclip.bottomtex, int(texx + segclip.toffset_bottom_x));
                        column.texheight = segclip.bottomtexh;
                        colfunc(column);
                        floorclip[i] = (float)(column.y1 - 1);
                     }
                     else
                        floorclip[i] = (float)b;
                     segclip.low += segclip.lowstep;
                     
                  }

                  R_WindowAdd(
                     planecontext, portalcontext, heap, viewpoint, bounds,
                     segclip.l_window, i, ceilingclip[i], floorclip[i]
                  );
               }
               else
               {
                  R_WindowAdd(
                     planecontext, portalcontext, heap, viewpoint, bounds,
                     segclip.l_window, i, (float)t, (float)b
                  );
               }
               ceilingclip[i] = view.height - 1.0f;
               floorclip[i] = 0.0f;
            }
            else if(segclip.midtex)
            {
               column.y1 = t;
               column.y2 = b;

               column.colormap = R_calculateLighting(cmapcontext, segclip.walllights_mid, segclip.dist);
               column.texmid = segclip.midtexmid;
               if(segclip.skew_mid_step && (segclip.side->middleSkewType() == SKEW_FRONT_FLOOR ||
                                            segclip.side->middleSkewType() == SKEW_FRONT_CEILING))
                  column.texmid += M_FloatToFixed(segclip.skew_mid_step * (segclip.len * basescale) + segclip.skew_mid_baseoffset);


               column.source = R_GetRawColumn(heap, segclip.midtex, int(texx + segclip.toffset_mid_x));
               column.texheight = segclip.midtexh;

               colfunc(column);

               ceilingclip[i] = view.height - 1.0f;
               floorclip[i] = 0.0f;
            }
         }
         else
         {
            if(segclip.t_window)
            {
               column.y1 = t;
               column.y2 = (int)(segclip.high > floorclip[i] ? floorclip[i] : segclip.high);
               if(column.y2 >= column.y1)
               {
                  R_WindowAdd(
                     planecontext, portalcontext, heap, viewpoint, bounds,
                     segclip.t_window, i,
                     static_cast<float>(column.y1), static_cast<float>(column.y2)
                  );
                  ceilingclip[i] = static_cast<float>(column.y2 + 1);
               }
               else
                  ceilingclip[i] = static_cast<float>(t);
               segclip.high += segclip.highstep;
            }
            else if(segclip.toptex)
            {
               column.y1 = t;
               column.y2 = (int)(segclip.high > floorclip[i] ? floorclip[i] : segclip.high);

               if(column.y2 >= column.y1)
               {
                  column.colormap = R_calculateLighting(cmapcontext, segclip.walllights_top, segclip.dist);
                  column.texmid = segclip.toptexmid;

                  if(segclip.skew_top_step && segclip.side->topSkewType() != SKEW_NONE)
                     column.texmid += M_FloatToFixed(segclip.skew_top_step * (segclip.len * basescale) + segclip.skew_top_baseoffset);

                  column.source = R_GetRawColumn(heap, segclip.toptex, int(texx + segclip.toffset_top_x));
                  column.texheight = segclip.toptexh;

                  colfunc(column);

                  ceilingclip[i] = (float)(column.y2 + 1);
               }
               else
                  ceilingclip[i] = (float)t;

               segclip.high += segclip.highstep;
            }
            else if(segclip.markflags & SEG_MARKCEILING)
               ceilingclip[i] = (float)t;


            if(segclip.b_window)
            {
               column.y1 = (int)(segclip.low < ceilingclip[i] ? ceilingclip[i] : segclip.low);
               column.y2 = b;
               if(column.y2 >= column.y1)
               {
                  R_WindowAdd(
                     planecontext, portalcontext, heap, viewpoint, bounds,
                     segclip.b_window, i,
                     static_cast<float>(column.y1), static_cast<float>(column.y2)
                  );
                  floorclip[i] = static_cast<float>(column.y1 - 1);
               }
               else
                  floorclip[i] = static_cast<float>(b);
               segclip.low += segclip.lowstep;
            }
            else if(segclip.bottomtex)
            {
               column.y1 = (int)(segclip.low < ceilingclip[i] ? ceilingclip[i] : segclip.low);
               column.y2 = b;

               if(column.y2 >= column.y1)
               {
                  column.colormap = R_calculateLighting(cmapcontext, segclip.walllights_bottom, segclip.dist);
                  column.texmid = segclip.bottomtexmid;

                  if(segclip.skew_bottom_step && segclip.side->bottomSkewType() != SKEW_NONE)
                     column.texmid += M_FloatToFixed(segclip.skew_bottom_step * (segclip.len * basescale) + segclip.skew_bottom_baseoffset);

                  column.source = R_GetRawColumn(heap, segclip.bottomtex, int(texx + segclip.toffset_bottom_x));
                  column.texheight = segclip.bottomtexh;

                  colfunc(column);

                  floorclip[i] = (float)(column.y1 - 1);
               }
               else
                  floorclip[i] = (float)b;

               segclip.low += segclip.lowstep;
            }
            else if(segclip.markflags & SEG_MARKFLOOR)
               floorclip[i] = (float)b;


            if(segclip.l_window)
            {
               R_WindowAdd(
                  planecontext, portalcontext, heap, viewpoint, bounds,
                  segclip.l_window, i, ceilingclip[i], floorclip[i]
               );
               ceilingclip[i] = view.height - 1.0f;
               floorclip[i] = 0.0f;
            }
            else if(skyplane)
            {
               if(ceilingclip[i] < floorclip[i])
               {
                  skyplane->top[i] = static_cast<int>(ceilingclip[i]);
                  skyplane->bottom[i] = static_cast<int>(floorclip[i]);
               }
               ceilingclip[i] = view.height - 1.0f;
               floorclip[i] = 0.0f;
            }
         }
      }
      else if(segclip.l_window)
      {
         R_WindowAdd(
            planecontext, portalcontext, heap, viewpoint, bounds,
            segclip.l_window, i, (float)t, (float)b
         );
         ceilingclip[i] = view.height - 1.0f;
         floorclip[i] = 0.0f;
      }
      else if(skyplane)
      {
         if(t < b)
         {
            skyplane->top[i] = t;
            skyplane->bottom[i] = b;
         }
         ceilingclip[i] = view.height - 1.0f;
         floorclip[i] = 0.0f;
      }
      else
      {
         if(segclip.markflags & SEG_MARKFLOOR) 
            floorclip[i] = (float)b;
         if(segclip.markflags & SEG_MARKCEILING) 
            ceilingclip[i] = (float)t;
      }

      segclip.len += segclip.lenstep;
      segclip.dist += segclip.diststep;
      segclip.top += segclip.topstep;
      segclip.bottom += segclip.bottomstep;
   }
}

//
// SoM: This function is needed in multiple places now to fix some cases
// of sprites showing up behind walls in some portal areas.
//
static void R_checkDSAlloc(bspcontext_t &context, ZoneHeap &heap)
{
   drawseg_t   *&drawsegs    = context.drawsegs;
   unsigned int &maxdrawsegs = context.maxdrawsegs;
   drawseg_t   *&ds_p        = context.ds_p;

   // drawsegs need to be taken care of here
   if(ds_p == drawsegs + maxdrawsegs)
   {
      unsigned int newmax = maxdrawsegs ? maxdrawsegs * 2 : 128;
      drawsegs    = zhrealloc(heap, drawseg_t *, drawsegs, sizeof(drawseg_t) * newmax);
      ds_p        = drawsegs + maxdrawsegs;
      maxdrawsegs = newmax;
   }
}

//
// Simply sets ds_p's properties to that of a closed drawseg.
//
static void R_closeDSP(drawseg_t *const ds_p)
{

   ds_p->silhouette        = SIL_BOTH;
   ds_p->sprtopclip        = screenheightarray;
   ds_p->sprbottomclip     = zeroarray;
   ds_p->bsilheight        = D_MAXINT;
   ds_p->tsilheight        = D_MININT;
   ds_p->maskedtexturecol  = nullptr;
   ds_p->maskedtextureskew = nullptr;
}

#define NEXTDSP(model, newx1) \
   ds_p++; \
   R_checkDSAlloc(bspcontext, heap); \
   *ds_p = model; \
   ds_p->x1 = newx1; \
   ds_p->dist1 += segclip.diststep * (newx1 - model.x1)

#define SETX2(model, newx2) \
   ds_p->dist2 -= segclip.diststep * (model.x2 - (newx2)); \
   ds_p->x2 = newx2

//
// This function iterates through the x range of segclip, and checks for columns
// that became closed in the clipping arrays after the segclip is rendered. Any
// new closed regions are then added to the solidsegs array to speed up 
// rejection of new segs trying to render to closed areas of clipping space.
//
static void R_detectClosedColumns(bspcontext_t &bspcontext, planecontext_t &planecontext, ZoneHeap &heap,
                                  [[maybe_unused]] const contextbounds_t &bounds, cb_seg_t &segclip)
{
   drawseg_t      *&ds_p        = bspcontext.ds_p;
   float     *const floorclip   = planecontext.floorclip;
   float     *const ceilingclip = planecontext.ceilingclip;

   drawseg_t model  = *ds_p;
   int       startx = segclip.x1;
   int       stop   = segclip.x2 + 1;
   int       i      = segclip.x1;

   // The new code will create new drawsegs for any newly created solidsegs.
   // Determine the initial state (open or closed) of the drawseg.
   if(floorclip[i] < ceilingclip[i])
   {
      // Find the first open column
      while(i < stop && floorclip[i] < ceilingclip[i]) i++;

      // Mark the closed area.
      R_closeDSP(ds_p);
      R_MarkSolidSeg(bspcontext, startx, i - 1);

      // End closed
      if(i == stop)
         return;

      SETX2(model, i - 1);

      // Continue on, at least one open column was found, so copy the model
      // and enter the loop.
      NEXTDSP(model, i);
   }

   for(; i < stop; i++)
   {
      // Find the first closed column
      while(i < stop && floorclip[i] >= ceilingclip[i]) 
         ++i;

      // End open
      if(i == stop)
         break;

      // Mark the close of the open drawseg
      SETX2(model, i - 1);

      // There is at least one closed column, so create another drawseg.
      NEXTDSP(model, i);
      R_closeDSP(ds_p);

      startx = i;

      // Find the first open column
      while(i < stop && floorclip[i] < ceilingclip[i]) 
         ++i;

      // from startx to i - 1 is solid.
#ifdef RANGECHECK
      if(startx > i - 1 || startx < bounds.startcolumn || i - 1 >= bounds.endcolumn ||
         startx >= bounds.endcolumn || i - 1 < bounds.startcolumn)
         I_Error("R_detectClosedColumns: bad range %i, %i\n", startx, i - 1);
#endif

      // SoM: This creates a bug clipping sprites:
      // What happens when there is a solid seg created, but no drawseg marked
      // as solid? Sprites appear through architecture. The solution is to
      // modify the drawseg created before this function was called to only be 
      // open where the seg has not created a solid seg.
      R_MarkSolidSeg(bspcontext, startx, i-1);

      // End closed
      if(i == stop)
         break;

      SETX2(model, i - 1);

      // There is at least one open column, so create another drawseg.
      NEXTDSP(model, i);
   }
}

#undef NEXTDSP
#undef SETX2

static void R_storeTextureColumns(float *const maskedtexturecol, float *const maskedtextureskew, cb_seg_t &segclip)
{
   int i;
   float texx;
   float basescale;

   for(i = segclip.x1; i <= segclip.x2; i++)
   {
      basescale = 1.0f / (segclip.dist * view.yfoc);
      texx = segclip.len * basescale + segclip.toffset_base_x + segclip.toffset_mid_x;

      if(maskedtexturecol)
         maskedtexturecol[i] = texx;
      if(maskedtextureskew)
      {
         if(segclip.skew_mid_step && segclip.side->middleSkewType() != SKEW_NONE)
            maskedtextureskew[i] = segclip.skew_mid_step * (segclip.len * basescale) + segclip.skew_mid_baseoffset;
         else
            maskedtextureskew[i] = 0.0f; // Don't think this is strictly necessary but do this for safety.
      }

      segclip.len  += segclip.lenstep;
      segclip.dist += segclip.diststep;
   }
}

//
// R_PointToDist2
//
// killough 5/2/98: move from r_main.c, made static, simplified
//
fixed_t R_PointToDist2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
   fixed_t dx = D_abs(x2 - x1);
   fixed_t dy = D_abs(y2 - y1);

   if(dy > dx)
   {
      fixed_t t = dx;
      dx = dy;
      dy = t;
   }
   return dx ? FixedDiv(dx, finesine[(tantoangle_acc[FixedDiv(dy,dx) >> DBITS]
                                      + ANG90) >> ANGLETOFINESHIFT]) : 0;
}

//
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
void R_StoreWallRange(bspcontext_t &bspcontext, cmapcontext_t &cmapcontext, planecontext_t &planecontext,
                      portalcontext_t &portalcontext, ZoneHeap &heap,
                      const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                      const contextbounds_t &bounds,
                      const cb_seg_t &seg, const int start, const int stop)
{
   drawseg_t           *&ds_p         = bspcontext.ds_p;
   float               *&lastopening  = planecontext.lastopening;
   float               *&lastskew     = planecontext.lastskew;
   const portalrender_t &portalrender = portalcontext.portalrender;

   float clipx1;
   float clipx2;

   float pstep;

   bool usesegloop;

   cb_seg_t segclip;

   // haleyjd 09/22/07: must be before use of segclip below
   memcpy(&segclip, &seg, sizeof(seg));

   // haleyjd: nullptr segclip line?? shouldn't happen.
#ifdef RANGECHECK
   if(!segclip.line)
      I_Error("R_StoreWallRange: null segclip.line\n");
#endif
   
   clipx1 = (float)(start - segclip.x1frac);

   clipx2 = (float)(segclip.x2frac - stop);

   segclip.x1 = start;
   segclip.x2 = stop;

   if(segclip.plane.floor)
      segclip.plane.floor = R_CheckPlane(planecontext, heap, segclip.plane.floor, start, stop);

   if(segclip.plane.ceiling)
   {
      // From PrBoom
      /* cph 2003/04/18  - ceilingplane and floorplane might be the same
       * visplane (e.g. if both skies); R_CheckPlane doesn't know about
       * modifications to the plane that might happen in parallel with the check
       * being made, so we have to override it and split them anyway if that is
       * a possibility, otherwise the floor marking would overwrite the ceiling
       * marking, resulting in HOM. */

      // ioanch: needed to fix GitHub issue #380 on maps such as sargasso.wad MAP02 coordinates
      // (5953, 10109) where the sky wall shows HOM in Eternity.

      // NOTE: PrBoom sets the floorplane AFTER the ceilingplane, unlike Eternity. So it does this
      // duplication when it encounters the floorplane, not the ceilingplane like here.

      if(segclip.plane.ceiling == segclip.plane.floor)
         segclip.plane.ceiling = R_DupPlane(planecontext, heap, segclip.plane.ceiling, start, stop);
      else
         segclip.plane.ceiling = R_CheckPlane(planecontext, heap, segclip.plane.ceiling, start, stop);
   }

   if(!(segclip.line->linedef->flags & (ML_MAPPED | ML_DONTDRAW)))
      g_linesToMap.enqueue(intptr_t(segclip.line->linedef - lines));

   if(clipx1)
   {
      segclip.dist += clipx1 * segclip.diststep;
      segclip.len += clipx1 * segclip.lenstep;

      segclip.top += clipx1 * segclip.topstep;
      segclip.bottom += clipx1 * segclip.bottomstep;

      if(segclip.toptex || seg.t_window)
         segclip.high += clipx1 * segclip.highstep;
      if(segclip.bottomtex || seg.b_window)
         segclip.low += clipx1 * segclip.lowstep;
   }
   if(clipx2)
   {
      segclip.dist2 -= clipx2 * segclip.diststep;
      segclip.len2 -= clipx2 * segclip.lenstep;

      segclip.top2 -= clipx2 * segclip.topstep;
      segclip.bottom2 -= clipx2 * segclip.bottomstep;

      if(segclip.toptex || seg.t_window)
         segclip.high2 -= clipx2 * segclip.highstep;
      if(segclip.bottomtex || seg.b_window)
         segclip.low2 -= clipx2 * segclip.lowstep;
   }

   // Recalculate the lenstep to help lessen the rounding error.
   if(clipx1 || clipx2)
   {
      if(stop == start)
         pstep = 1.0f;
      else
         pstep = 1.0f / (float)(stop - start);

      segclip.lenstep = (segclip.len2 - segclip.len) * pstep;
      segclip.diststep = (segclip.dist2 - segclip.dist) * pstep;
      segclip.topstep = (segclip.top2 - segclip.top) * pstep;
      segclip.bottomstep = (segclip.bottom2 - segclip.bottom) * pstep;

      if(segclip.toptex || seg.t_window)
         segclip.highstep = (segclip.high2 - segclip.high) * pstep;
      if(segclip.bottomtex || seg.b_window)
         segclip.lowstep = (segclip.low2 - segclip.low) * pstep;
   }

   // Lighting
   // TODO: Modularize the lighting. This function should not handle colormaps 
   // directly.
   // calculate light table
   //  use different light tables
   //  for horizontal / vertical / diagonal
   // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
   if(!cmapcontext.fixedcolormap)
   {
      struct {
         const lighttable_t *const **walllight;
         const int16_t               light;
         const uint16_t              absoluteflag;
      } walllights[3] =
      {
         { &segclip.walllights_top,    segclip.line->sidedef->light_top,    SDF_LIGHT_TOP_ABSOLUTE    },
         { &segclip.walllights_mid,    segclip.line->sidedef->light_mid,    SDF_LIGHT_MID_ABSOLUTE    },
         { &segclip.walllights_bottom, segclip.line->sidedef->light_bottom, SDF_LIGHT_BOTTOM_ABSOLUTE },
      };

      for(auto &walllight : walllights)
      {
         int lightnum;
         if(segclip.line->sidedef->flags & walllight.absoluteflag)
            lightnum = walllight.light >> LIGHTSEGSHIFT;
         else
         {
            if(segclip.line->sidedef->flags & SDF_LIGHT_BASE_ABSOLUTE)
               lightnum = segclip.line->sidedef->light_base >> LIGHTSEGSHIFT;
            else
            {
               lightnum = segclip.frontsec->lightlevel >> LIGHTSEGSHIFT;
               lightnum += segclip.line->sidedef->light_base >> LIGHTSEGSHIFT;
            }
            lightnum += walllight.light >> LIGHTSEGSHIFT;

         }
         lightnum += extralight * LIGHTBRIGHT;

         // haleyjd 08/11/00: optionally skip this to evenly apply colormap
         if(LevelInfo.unevenLight)
         {
            if(segclip.line->linedef->v1->y == segclip.line->linedef->v2->y)
               lightnum -= LIGHTBRIGHT;
            else if(segclip.line->linedef->v1->x == segclip.line->linedef->v2->x)
               lightnum += LIGHTBRIGHT;
         }

         if(lightnum < 0)
            *walllight.walllight = cmapcontext.scalelight[0];
         else if(lightnum >= LIGHTLEVELS)
            *walllight.walllight = cmapcontext.scalelight[LIGHTLEVELS - 1];
         else
            *walllight.walllight = cmapcontext.scalelight[lightnum];
      }
   }


   // drawsegs need to be taken care of here
   R_checkDSAlloc(bspcontext, heap);

   ds_p->x1       = start;
   ds_p->x2       = stop;
   ds_p->curline  = segclip.line;
   ds_p->dist2    = (ds_p->dist1 = segclip.dist) + segclip.diststep * (segclip.x2 - segclip.x1);
   ds_p->diststep = segclip.diststep;
   ds_p->colormap = cmapcontext.scalelight;
   ds_p->fixedcolormap = cmapcontext.fixedcolormap;
   ds_p->deltaz = 0; // init with 0
   
   if(segclip.clipsolid)
      R_closeDSP(ds_p);
   else
   {
      ds_p->sprtopclip = ds_p->sprbottomclip = nullptr;
      ds_p->silhouette = 0;

      // SoM: TODO: This can be a bit problematic for slopes because we'll have 
      // to check the line for textures at both ends...
      if(segclip.maxfrontfloor > segclip.maxbackfloor)
      {
         if(segclip.maxfrontfloor > viewpoint.z)   // above a raising floor slope
         {
            ds_p->silhouette = SIL_BOTTOM;
            ds_p->bsilheight = D_MAXINT;
         }
         else
         {
            ds_p->silhouette = SIL_BOTTOM;
            ds_p->bsilheight = segclip.maxfrontfloor;
         }
      }
      else if(segclip.maxbackfloor > viewpoint.z)
      {
         ds_p->silhouette = SIL_BOTTOM;
         ds_p->bsilheight = D_MAXINT;
      }
      else if(segclip.backsec->srf.floor.slope)
      {
         ds_p->silhouette = SIL_BOTTOM;
         ds_p->bsilheight = emax(segclip.maxfrontfloor, segclip.maxbackfloor);
      }
      if(segclip.minfrontceil < segclip.minbackceil)
      {
         if(segclip.minfrontceil < viewpoint.z)  // under a lowering ceiling slope
         {
            ds_p->silhouette |= SIL_TOP;
            ds_p->tsilheight = D_MININT;
         }
         else
         {
            ds_p->silhouette |= SIL_TOP;
            ds_p->tsilheight = segclip.minfrontceil;
         }
      }
      else if(segclip.minbackceil < viewpoint.z)
      {
         ds_p->silhouette |= SIL_TOP;
         ds_p->tsilheight = D_MININT;
      }
      else if(segclip.backsec->srf.ceiling.slope)
      {
         ds_p->silhouette |= SIL_TOP;
         ds_p->tsilheight = emin(segclip.minfrontceil, segclip.minbackceil);
      }

      if(segclip.maskedtex)
      {
         int i;
         float *mtc;
         float *mts;
         int xlen;
         xlen = segclip.x2 - segclip.x1 + 1;

         ds_p->maskedtexturecol  = lastopening - segclip.x1;
         ds_p->maskedtextureskew = lastskew - segclip.x1;
         if(portalrender.active)
            ds_p->deltaz = viewpoint.z - portalrender.w->vz;
         
         mtc = lastopening;
         mts = lastskew;

         for(i = 0; i < xlen; i++)
         {
            mtc[i] = FLT_MAX;
            mts[i] = 0.0f;
         }

         lastopening += xlen;
         lastskew    += xlen;
      }
      else
      {
         ds_p->maskedtexturecol = nullptr;
         ds_p->maskedtextureskew = nullptr;
      }
   }

   usesegloop = !seg.backsec        || 
                 seg.clipsolid      || 
                 seg.markflags      ||
                 seg.toptex         || 
                 seg.midtex         || 
                 seg.bottomtex      || 
                 seg.f_portalignore || 
                 seg.c_portalignore ||
                 segclip.line->linedef->portal || 
                 ds_p->silhouette   || 
                 !ds_p->maskedtexturecol;

   if(usesegloop)
   {
      const R_ColumnFunc colfunc = r_column_engine->DrawColumn;
      R_renderSegLoop(
         cmapcontext, planecontext, portalcontext, heap, colfunc, viewpoint, cb_viewpoint, bounds, ds_p, segclip
      );
   }
   else
      R_storeTextureColumns(ds_p->maskedtexturecol, ds_p->maskedtextureskew, segclip);

   // store clipping arrays
   float *const floorclip   = planecontext.floorclip;
   float *const ceilingclip = planecontext.ceilingclip;
   if((ds_p->silhouette & SIL_TOP || segclip.maskedtex) && !ds_p->sprtopclip)
   {
      int xlen = segclip.x2 - segclip.x1 + 1;

      if(segclip.markflags & SEG_MARKCOVERLAY)
      {
         for(int i = xlen; i --> 0;)
         {
            float over = overlaycclip[segclip.x1 + i];
            float solid = ceilingclip[segclip.x1 + i];
            lastopening[i] = over > solid ? over : solid;
         }
      }
      else
         memcpy(lastopening, ceilingclip + segclip.x1, sizeof(float) * xlen);

      ds_p->sprtopclip = lastopening - segclip.x1;
      lastopening += xlen;
   }
   if((ds_p->silhouette & SIL_BOTTOM || segclip.maskedtex) && !ds_p->sprbottomclip)
   {
      int xlen = segclip.x2 - segclip.x1 + 1;

      if(segclip.markflags & SEG_MARKFOVERLAY)
      {
         for(int i = xlen; i-- > 0;)
         {
            float over = overlayfclip[segclip.x1 + i];
            float solid = floorclip[segclip.x1 + i];
            lastopening[i] = over < solid ? over : solid;
         }
      }
      else
         memcpy(lastopening, floorclip + segclip.x1, sizeof(float) * xlen);

      ds_p->sprbottomclip = lastopening - segclip.x1;
      lastopening += xlen;
   }
   if (segclip.maskedtex && !(ds_p->silhouette & SIL_TOP))
   {
      ds_p->silhouette |= SIL_TOP;
      ds_p->tsilheight = D_MININT;
   }
   if (segclip.maskedtex && !(ds_p->silhouette & SIL_BOTTOM))
   {
      ds_p->silhouette |= SIL_BOTTOM;
      ds_p->bsilheight = D_MAXINT;
   }

   // ioanch: also check for portalrender, and detect any columns shut by the
   // portal window, which would otherwise be ignored. Necessary for correct
   // sprite rendering.
   if(!segclip.clipsolid && (ds_p->silhouette || portalrender.active))
      R_detectClosedColumns(bspcontext, planecontext, heap, bounds, segclip);

   ++ds_p;
}

//----------------------------------------------------------------------------
//
// $Log: r_segs.c,v $
// Revision 1.16  1998/05/03  23:02:01  killough
// Move R_PointToDist from r_main.c, fix #includes
//
// Revision 1.15  1998/04/27  01:48:37  killough
// Program beautification
//
// Revision 1.14  1998/04/17  10:40:31  killough
// Fix 213, 261 (floor/ceiling lighting)
//
// Revision 1.13  1998/04/16  06:24:20  killough
// Prevent 2s sectors from bleeding across deep water or fake floors
//
// Revision 1.12  1998/04/14  08:17:16  killough
// Fix light levels on 2s textures
//
// Revision 1.11  1998/04/12  02:01:41  killough
// Add translucent walls, add insurance against SIGSEGV
//
// Revision 1.10  1998/04/07  06:43:05  killough
// Optimize: use external doorclosed variable
//
// Revision 1.9  1998/03/28  18:04:31  killough
// Reduce texture offsets vertically
//
// Revision 1.8  1998/03/16  12:41:09  killough
// Fix underwater / dual ceiling support
//
// Revision 1.7  1998/03/09  07:30:25  killough
// Add primitive underwater support, fix scrolling flats
//
// Revision 1.6  1998/03/02  11:52:58  killough
// Fix texturemapping overflow, add scrolling walls
//
// Revision 1.5  1998/02/09  03:17:13  killough
// Make closed door clipping more consistent
//
// Revision 1.4  1998/02/02  13:27:02  killough
// fix openings bug
//
// Revision 1.3  1998/01/26  19:24:47  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/26  06:10:42  killough
// Discard old Medusa hack -- fixed in r_data.c now
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
