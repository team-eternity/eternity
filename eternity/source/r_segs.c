// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//      All the clipping: columns, horizontal spans, sky columns.
//
//-----------------------------------------------------------------------------
//
// 4/25/98, 5/2/98 killough: reformatted, beautified

static const char
rcsid[] = "$Id: r_segs.c,v 1.16 1998/05/03 23:02:01 killough Exp $";

#include "doomstat.h"
#include "r_main.h"
#include "r_bsp.h"
#include "r_plane.h"
#include "r_things.h"
#include "r_draw.h"
#include "w_wad.h"
#include "p_user.h"
#include "p_info.h"

// OPTIMIZE: closed two sided lines as single sided
// SoM: Done.
// SoM: Cardboard globals
cb_column_t       column;
cb_seg_t          seg;
cb_seg_t          segclip;

// killough 1/6/98: replaced globals with statics where appropriate
lighttable_t    **walllights;
static int *maskedtexturecol;

//
// R_RenderMaskedSegRange
//

void R_RenderMaskedSegRange(drawseg_t *ds, int x1, int x2)
{
   column_t *col;
   int      lightnum;
   int      texnum;
   sector_t tempsec;      // killough 4/13/98
   float    dist, diststep;
   float    scale, scalestep;
   float    texmidf;
   lighttable_t **wlight;

   // Calculate light table.
   // Use different light tables
   //   for horizontal / vertical / diagonal. Diagonal?

   segclip.line = ds->curline;
   
   // killough 4/11/98: draw translucent 2s normal textures
   colfunc = r_column_engine->DrawColumn;
   if(segclip.line->linedef->tranlump >= 0 && general_translucency)
   {
      colfunc = r_column_engine->DrawTLColumn;
      tranmap = main_tranmap;
      if(segclip.line->linedef->tranlump > 0)
         tranmap = W_CacheLumpNum(segclip.line->linedef->tranlump-1, PU_STATIC);
   }
   // killough 4/11/98: end translucent 2s normal code

   segclip.frontsec = segclip.line->frontsector;
   segclip.backsec = segclip.line->backsector;

   texnum = texturetranslation[segclip.line->sidedef->midtexture];
   
   // killough 4/13/98: get correct lightlevel for 2s normal textures
   lightnum = (R_FakeFlat(segclip.frontsec, &tempsec, NULL, NULL, false)
               ->lightlevel >> LIGHTSEGSHIFT)+extralight;

   // haleyjd 08/11/00: optionally skip this to evenly apply colormap
   if(LevelInfo.unevenLight)
   {  
      if(ds->curline->linedef->v1->y == ds->curline->linedef->v2->y)
         --lightnum;
      else if(ds->curline->linedef->v1->x == ds->curline->linedef->v2->x)
         ++lightnum;
   }

   // SoM 10/19/02: deep water colormap fix
   wlight = 
      ds->colormap[lightnum >= LIGHTLEVELS || fixedcolormap ? 
                   LIGHTLEVELS-1 :
                   lightnum <  0 ? 0 : lightnum ] ;

   maskedtexturecol = ds->maskedtexturecol;

   mfloorclip = ds->sprbottomclip;
   mceilingclip = ds->sprtopclip;

   diststep = ds->diststep;
   dist = ds->dist1 + (x1 - ds->x1) * diststep;

#ifdef R_PORTALS
   // find positioning
   if(segclip.line->linedef->flags & ML_DONTPEGBOTTOM)
   {
      column.texmid = segclip.frontsec->floorheight > segclip.backsec->floorheight
         ? segclip.frontsec->floorheight : segclip.backsec->floorheight;
      column.texmid = column.texmid + textureheight[texnum] - ds->viewz;
   }
   else
   {
      column.texmid = segclip.frontsec->ceilingheight < segclip.backsec->ceilingheight
         ? segclip.frontsec->ceilingheight : segclip.backsec->ceilingheight;
      column.texmid = column.texmid - ds->viewz;
   }
#else
   // find positioning
   if(segclip.line->linedef->flags & ML_DONTPEGBOTTOM)
   {
      column.texmid = segclip.frontsec->floorheight > segclip.backsec->floorheight
         ? segclip.frontsec->floorheight : segclip.backsec->floorheight;
      column.texmid = column.texmid + textureheight[texnum] - viewz;
   }
   else
   {
      column.texmid = segclip.frontsec->ceilingheight < segclip.backsec->ceilingheight
         ? segclip.frontsec->ceilingheight : segclip.backsec->ceilingheight;
      column.texmid = column.texmid - viewz;
   }
#endif

   column.texmid += segclip.line->sidedef->rowoffset;
   
   // SoM 10/19/02: deep water colormap fixes
   //if (fixedcolormap)
   //   column.colormap = fixedcolormap;
   if(fixedcolormap)
   {
      // haleyjd 10/31/02: invuln fix
      if(fixedcolormap == 
         fullcolormap + INVERSECOLORMAP*256*sizeof(lighttable_t))
         column.colormap = fixedcolormap;
      else
         column.colormap = walllights[MAXLIGHTSCALE-1];
   }

   // SoM: performance tuning (tm Lee Killough 1998)
   scale = dist * view.yfoc;
   scalestep = diststep * view.yfoc;
   texmidf = column.texmid / 65536.0f;

   // draw the columns
   for(column.x = x1; column.x <= x2; ++column.x, dist += diststep, scale += scalestep)
   {
      // haleyjd:DEBUG
      if(maskedtexturecol[column.x] != 0x7fffffff)
      {
         if(!fixedcolormap)      // calculate lighting
         {                             // killough 11/98:
            // SoM: ANYRES
            int index = (int)(dist * 2560.0f);
            
            if(index >=  MAXLIGHTSCALE )
               index = MAXLIGHTSCALE - 1;
            
            column.colormap = wlight[index];
         }

         maskedcolumn.scale = scale;
         maskedcolumn.ytop = view.ycenter - (texmidf * scale);
         column.step = (int)(65536.0f / scale);

         // killough 1/25/98: here's where Medusa came in, because
         // it implicitly assumed that the column was all one patch.
         // Originally, Doom did not construct complete columns for
         // multipatched textures, so there were no header or trailer
         // bytes in the column referred to below, which explains
         // the Medusa effect. The fix is to construct true columns
         // when forming multipatched textures (see r_data.c).

         // draw the texture
         col = (column_t *)((byte *)
                            R_GetColumn(texnum,maskedtexturecol[column.x]) - 3);
         R_DrawMaskedColumn(col);
         
         maskedtexturecol[column.x] = 0x7fffffff;
      }
   }

   // Except for main_tranmap, mark others purgable at this point
   if(segclip.line->linedef->tranlump > 0 && general_translucency)
      Z_ChangeTag(tranmap, PU_CACHE); // killough 4/11/98*/
}

//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling textures.
// CALLED: CORE LOOPING ROUTINE.
//


static void R_RenderSegLoop(void)
{
   float t, b, h, l, top, bottom;
   int i, texx;
   float basescale;

   for(i = segclip.x1; i <= segclip.x2; i++)
   {
      top = ceilingclip[i];

      t = segclip.top;
      b = segclip.bottom;
      if(t < top)
         t = top;

      if(b > floorclip[i])
         b = floorclip[i];


#ifdef R_PORTALS
      // SoM 3/10/2005: Only add to the portal of the ceiling is marked
      if((segclip.markceiling || segclip.c_portalignore) && segclip.frontsec->c_portal)
      {
         bottom = t - 1;
         
         if(bottom > floorclip[i])
            bottom = floorclip[i];
         
         if(bottom - top > -1.0f)
            R_PortalAdd(segclip.frontsec->c_portal, i, top, bottom);

         ceilingclip[i] = t;
      }
      else
#endif
      if(segclip.markceiling && segclip.ceilingplane)
      {
         bottom = t - 1;

         if(bottom > floorclip[i])
            bottom = floorclip[i];

         // ahh the fraction... Some times the top can be greater than the bottom by less than
         // 1 but the pixel should still be included because the fractional portion is being
         // discarded anyway.
         if(bottom - top > -1.0f)
         {
            segclip.ceilingplane->top[i] = (int)top;
            segclip.ceilingplane->bottom[i] = (int)bottom;
         }
   
         ceilingclip[i] = t;
      }

#ifdef R_PORTALS
      // SoM 3/10/2005: Only add to the portal of the floor is marked
      if((segclip.markfloor || segclip.f_portalignore)  && segclip.frontsec->f_portal)
      {
         top = b + 1;
         bottom = floorclip[i];

         if(top < ceilingclip[i])
            top = ceilingclip[i];

         if(bottom - top > -1.0f)
            R_PortalAdd(segclip.frontsec->f_portal, i, top, bottom);

         floorclip[i] = b;
      }
      else
#endif
      if(segclip.markfloor && segclip.floorplane)
      {
         top = b + 1;
         bottom = floorclip[i];

         if(top < ceilingclip[i])
            top = ceilingclip[i];

         // ahh the fraction... Some times the top can be greater than the bottom by less than
         // 1 but the pixel should still be included because the fractional portion is being
         // discarded anyway.
         if(bottom - top > -1.0f)
         {
            segclip.floorplane->top[i] = (int)top;
            segclip.floorplane->bottom[i] = (int)bottom;
         }

         floorclip[i] = b;
      }

#ifdef R_PORTALS
      if(segclip.line->linedef->portal)
         R_PortalAdd(segclip.line->linedef->portal, i, t, b);
      else
#endif
      if(segclip.segtextured)
      {
         int index;

         basescale = 1.0f / (segclip.dist * view.yfoc);

         column.step = (int)(basescale * FRACUNIT);
         column.x = i;

         texx = segclip.len < 0 ? (int)segclip.toffsetx :
                (int)(((segclip.len) * basescale) + segclip.toffsetx);

         if(ds_p->maskedtexturecol)
            ds_p->maskedtexturecol[i] = texx;

         // calculate lighting
         // SoM: ANYRES
         if(!fixedcolormap)
         {
            // SoM: it took me about 5 solid minutes of looking at the old doom code
            // and running test levels through it to do the math and get 2560 as the
            // light distance factor.
            index = (int)(segclip.dist * 2560.0f);
         
            if(index >=  MAXLIGHTSCALE)
               index = MAXLIGHTSCALE - 1;

            column.colormap = segclip.walllights[index];
         }

         if(segclip.twosided == false && segclip.midtex)
         {
            column.y1 = (int)t;
            column.y2 = (int)b;

            column.texmid = segclip.midtexmid;

            column.source = R_GetColumn(segclip.midtex, texx);
            column.texheight = segclip.midtexh;

            colfunc();

            ceilingclip[i] = view.height - 1.0f;
            floorclip[i] = 0.0f;
         }
         else if(segclip.twosided)
         {
            if(segclip.toptex)
            {
               h = segclip.high;
               if(h > floorclip[i])
                  h = floorclip[i];

               column.y1 = (int)t;
               column.y2 = (int)h;

               if(column.y2 >= column.y1)
               {
                  column.texmid = segclip.toptexmid;

                  column.source = R_GetColumn(segclip.toptex, texx);
                  column.texheight = segclip.toptexh;

                  colfunc();

                  ceilingclip[i] = h + 1.0f;
               }
               else
                  ceilingclip[i] = t;

               segclip.high += segclip.highstep;
            }
            else if(segclip.markceiling)
               ceilingclip[i] = t;


            if(segclip.bottomtex)
            {
               l = segclip.low;
               if(l < ceilingclip[i])
                  l = ceilingclip[i];

               column.y1 = (int)l;
               column.y2 = (int)b;

               if(column.y2 >= column.y1)
               {
                  column.texmid = segclip.bottomtexmid;

                  column.source = R_GetColumn(segclip.bottomtex, texx);
                  column.texheight = segclip.bottomtexh;

                  colfunc();

                  floorclip[i] = l - 1.0f;
               }
               else
                  floorclip[i] = b;

               segclip.low += segclip.lowstep;
            }
            else if(segclip.markfloor)
               floorclip[i] = b;
         }
      }
      else
      {
         if(segclip.markfloor) floorclip[i] = b;
         if(segclip.markceiling) ceilingclip[i] = t;
      }

      segclip.len += segclip.lenstep;
      segclip.dist += segclip.diststep;
      segclip.top += segclip.topstep;
      segclip.bottom += segclip.bottomstep;
   }
}



static void R_StoreTextureColumns(void)
{
   int i, texx;
   float basescale;
   
   for(i = segclip.x1; i <= segclip.x2; i++)
   {
      basescale = 1.0f / (segclip.dist * view.yfoc);
      texx = segclip.len < 0 ? (int)segclip.toffsetx :
             (int)(((segclip.len) * basescale) + segclip.toffsetx);

      if(ds_p->maskedtexturecol)
         ds_p->maskedtexturecol[i] = texx;

      segclip.len += segclip.lenstep;
      segclip.dist += segclip.diststep;
   }
}

// killough 5/2/98: move from r_main.c, made static, simplified
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
   return dx ? FixedDiv(dx, finesine[(tantoangle[FixedDiv(dy,dx) >> DBITS]
                                      + ANG90) >> ANGLETOFINESHIFT]) : 0;
}


//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
#define DISTSTEPLIMIT 0.00005f

void R_StoreWallRange(const int start, const int stop)
{
   float clipx1;
   float clipx2;

   float y1, y2, i1, i2;
   float pstep;
   side_t *side = seg.side;
   boolean usesegloop = !seg.backsec || seg.clipsolid || seg.markceiling || seg.markfloor || 
                        seg.toptex || seg.midtex || seg.bottomtex || seg.f_portalignore || 
                        seg.c_portalignore || segclip.line->linedef->portal ? true : false;

   memcpy(&segclip, &seg, sizeof(seg));

   clipx1 = (float)fabs(segclip.diststep) > DISTSTEPLIMIT ? 
            (float)(start - segclip.x1) : (float)(start - segclip.x1frac);

   clipx2 = (float)(segclip.x2frac - stop);

   segclip.x1 = start;
   segclip.x2 = stop;

   if(segclip.floorplane)
      segclip.floorplane = R_CheckPlane(segclip.floorplane, start, stop);

   if(segclip.ceilingplane)
      segclip.ceilingplane = R_CheckPlane(segclip.ceilingplane, start, stop);

   if(!(segclip.line->linedef->flags & (ML_MAPPED | ML_DONTDRAW)))
      segclip.line->linedef->flags |= ML_MAPPED;

   if(clipx1)
   {
      segclip.dist += clipx1 * segclip.diststep;
      segclip.len += clipx1 * segclip.lenstep;
   }
   if(clipx2)
   {
      segclip.dist2 -= clipx2 * segclip.diststep;
      segclip.len2 -= clipx2 * segclip.lenstep;
   }


   i1 = segclip.dist * view.yfoc;
   i2 = segclip.dist2 * view.yfoc;

   if(stop == start)
      pstep = 1.0f;
   else
      pstep = 1.0f / (float)(stop - start);

   // Recalculate the lenstep to help lessen the rounding error.
   if(clipx1 || clipx2)
   {
      segclip.lenstep = (segclip.len2 - segclip.len) * pstep;
      segclip.diststep = (segclip.dist2 - segclip.dist) * pstep;
   }

   if(!segclip.backsec)
   {
      segclip.top = 
      y1 = view.ycenter - (seg.top * i1);
      y2 = view.ycenter - (seg.top * i2);
      segclip.topstep = (y2 - y1) * pstep;

      segclip.bottom = 
      y1 = view.ycenter - (seg.bottom * i1) - 1;
      y2 = view.ycenter - (seg.bottom * i2) - 1;
      segclip.bottomstep = (y2 - y1) * pstep;
   }
   else
   {
      segclip.top = 
      y1 = view.ycenter - (seg.top * i1);
      y2 = view.ycenter - (seg.top * i2);
      segclip.topstep = (y2 - y1) * pstep;
      if(segclip.toptex)
      {
         segclip.high = 
         y1 = view.ycenter - (seg.high * i1) - 1;
         y2 = view.ycenter - (seg.high * i2) - 1;
         segclip.highstep = (y2 - y1) * pstep;
      }

      segclip.bottom = 
      y1 = view.ycenter - (seg.bottom * i1) - 1;
      y2 = view.ycenter - (seg.bottom * i2) - 1;
      segclip.bottomstep = (y2 - y1) * pstep;
      if(segclip.bottomtex)
      {
         segclip.low = 
         y1 = view.ycenter - (seg.low * i1);
         y2 = view.ycenter - (seg.low * i2);
         segclip.lowstep = (y2 - y1) * pstep;
      }
   }

   // Lighting
   // TODO: Modularize the lighting. This function should not handle colormaps directly.
   // calculate light table
   //  use different light tables
   //  for horizontal / vertical / diagonal
   // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
   if(!fixedcolormap)
   {
      int lightnum = (segclip.frontsec->lightlevel >> LIGHTSEGSHIFT) + extralight;

      // haleyjd 08/11/00: optionally skip this to evenly apply colormap
      if(LevelInfo.unevenLight)
      {  
         if(segclip.line->linedef->v1->y == segclip.line->linedef->v2->y)
            --lightnum;
         else if(segclip.line->linedef->v1->x == segclip.line->linedef->v2->x)
            ++lightnum;
      }

      if(lightnum < 0)
         segclip.walllights = scalelight[0];
      else if(lightnum >= LIGHTLEVELS)
         segclip.walllights = scalelight[LIGHTLEVELS-1];
      else
         segclip.walllights = scalelight[lightnum];
   }


   // drawsegs need to be taken care of here
   if(ds_p == drawsegs + maxdrawsegs)
   {
      unsigned newmax = maxdrawsegs ? maxdrawsegs * 2 : 128;
      drawsegs = realloc(drawsegs, sizeof(drawseg_t) * newmax);
      ds_p = drawsegs + maxdrawsegs;
      maxdrawsegs = newmax;
   }

   ds_p->x1 = start;
   ds_p->x2 = stop;
   ds_p->viewx = viewx;
   ds_p->viewy = viewy;
   ds_p->viewz = viewz;
   ds_p->curline = segclip.line;
   ds_p->dist2 = (ds_p->dist1 = segclip.dist) + segclip.diststep * (segclip.x2 - segclip.x1);
   ds_p->diststep = segclip.diststep;
   ds_p->colormap = scalelight;
   
   if(segclip.clipsolid)
   {
      ds_p->silhouette = SIL_BOTH;
      ds_p->sprtopclip = screenheightarray;
      ds_p->sprbottomclip = zeroarray;
      ds_p->bsilheight = D_MAXINT;
      ds_p->tsilheight = D_MININT;
      ds_p->maskedtexturecol = NULL;
   }
   else
   {
      ds_p->sprtopclip = ds_p->sprbottomclip = NULL;
      ds_p->silhouette = 0;

      if(segclip.frontsec->floorheight > segclip.backsec->floorheight)
      {
         ds_p->silhouette = SIL_BOTTOM;
         ds_p->bsilheight = segclip.frontsec->floorheight;
      }
      else if(segclip.backsec->floorheight > viewz)
      {
         ds_p->silhouette = SIL_BOTTOM;
         ds_p->bsilheight = D_MAXINT;
      }
      if(segclip.frontsec->ceilingheight < segclip.backsec->ceilingheight)
      {
         ds_p->silhouette |= SIL_TOP;
         ds_p->tsilheight = segclip.frontsec->ceilingheight;
      }
      else if(segclip.backsec->ceilingheight < viewz)
      {
         ds_p->silhouette |= SIL_TOP;
         ds_p->tsilheight = D_MININT;
      }


      if(segclip.maskedtex)
      {
         register int i, *mtc;
         int xlen;
         xlen = segclip.x2 - segclip.x1 + 1;

         ds_p->maskedtexturecol = (int *)(lastopening - segclip.x1);
         i = xlen;

         mtc = (int *)lastopening;

         while(i--)
            mtc[i] = 0x7fffffff;

         lastopening += xlen;
      }
      else
         ds_p->maskedtexturecol = NULL;
   }

   if(ds_p->silhouette || usesegloop || !ds_p->maskedtexturecol)
      R_RenderSegLoop();
   else
      R_StoreTextureColumns();
   
   // store clipping arrays
   if((ds_p->silhouette & SIL_TOP || segclip.maskedtex) && !ds_p->sprtopclip)
   {
      int xlen = segclip.x2 - segclip.x1 + 1;

      memcpy(lastopening, ceilingclip + segclip.x1, sizeof(float) * xlen);
      ds_p->sprtopclip = lastopening - segclip.x1;
      lastopening += xlen;
   }
   if((ds_p->silhouette & SIL_BOTTOM || segclip.maskedtex) && !ds_p->sprbottomclip)
   {
      int xlen = segclip.x2 - segclip.x1 + 1;

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
   ++ds_p;
}



#ifdef R_PORTALS
//
// R_ClipSeg
//
// SoM 3/14/2005: This function will reject segs that are completely 
// outside the portal window based on a few conditions. It will also 
// clip the start and stop values of the seg based on what range it is 
// actually visible in. This function is sound and Doom could even use 
// this for normal rendering, but it adds some overhead.
//
boolean R_ClipSeg()
{
   int   i;
   float clipx;
   // The way we handle segs depends on relative camera position. If the 
   // camera is above we need to reject segs based on the top of the seg.
   // If the camera is below the bottom of the seg the bottom edge needs 
   // to be clipped. This is done so visplanes will still be rendered 
   // fully.
   if(viewz > seg.frontsec->ceilingheight)
   {
      float top, topstep, topstop;
      float y1, y2;

      // I totally overlooked this when I moved all the wall panel projection to r_segs.c
      top = y1 = view.ycenter - (seg.top * seg.dist * view.yfoc);
      topstop = y2 = view.ycenter - (seg.top * seg.dist2 * view.yfoc);
      topstep = seg.x2frac > seg.x1frac ? (y2 - y1) / (seg.x2frac - seg.x1frac) : 0.0f;

      for(i = seg.x1; i <= seg.x2; i++)
      {
         if(floorclip[i] < ceilingclip[i] || top > floorclip[i])
         {
            // column is not visible so increment and continue
            top += topstep;
            continue;
         }

         // First visible column has been found, so set seg to start here.
         if(i != seg.x1)
         {
            clipx = (float)fabs(segclip.diststep) > DISTSTEPLIMIT ? 
                    (float)(i - seg.x1) : (float)(i - seg.x1frac);

            seg.dist += seg.diststep * clipx;
            seg.len += seg.lenstep * clipx;
            seg.x1 = i;
            seg.x1frac = (float)i;
         }

         // next count back from the right end of the seg to see if there are
         // hidden columns which could be removed.
         top = topstop;
         for(i = seg.x2; i > seg.x1; i--)
         {
            if(floorclip[i] < ceilingclip[i] || top > floorclip[i])
            {
               top -= topstep;
               continue;
            }

            // visible
            break;
         }

         if(i != seg.x2)
         {
            clipx = (float)(seg.x2frac - i);

            seg.dist2 -= clipx * seg.diststep;
            seg.len2 -= clipx * seg.lenstep;
            seg.x2 = i;
            seg.x2frac = (float)i;
         }
         return true;
      }

      // No visible column could be found
      return false;
   }
   else if(viewz < seg.frontsec->floorheight)
   {
      float bottom, bottomstep, stopbottom;
      float y1, y2;

      bottom = y1 = view.ycenter - (seg.bottom * seg.dist * view.yfoc) - 1;
      stopbottom = y2 = view.ycenter - (seg.bottom * seg.dist2 * view.yfoc) - 1;
      bottomstep = seg.x2frac > seg.x1frac ? (y2 - y1) / (seg.x2frac - seg.x1frac) : 0.0f;

      for(i = seg.x1; i <= seg.x2; i++)
      {
         if(floorclip[i] < ceilingclip[i] || bottom < ceilingclip[i])
         {
            // column is not visible so increment and continue
            bottom += bottomstep;
            continue;
         }

         if(i != seg.x1)
         {
            // First visible column has been found, so set seg to start here.
            clipx = (float)fabs(segclip.diststep) > DISTSTEPLIMIT ? 
                    (float)(i - seg.x1) : (float)(i - seg.x1frac);

            seg.dist += seg.diststep * clipx;
            seg.len += seg.lenstep * clipx;
            seg.x1 = i;
            seg.x1frac = (float)i;
         }

         // next count back from the right end of the seg to see if there are
         // hidden columns which could be removed.
         bottom = stopbottom;
         for(i = seg.x2; i > seg.x1; i--)
         {
            if(floorclip[i] < ceilingclip[i] || bottom < ceilingclip[i])
            {
               bottom -= bottomstep;
               continue;
            }

            // visible
            break;
         }

         if(i != seg.x2)
         {
            clipx = (float)(seg.x2frac - i);
            seg.dist2 -= clipx * seg.diststep;
            seg.len2 -= clipx * seg.lenstep;
            seg.x2 = i;
            seg.x2frac = (float)i;
         }
         return true;
      }

      // No visible column could be found
      return false;
   }

   return true;
}
#endif // ifdef R_PORTALS

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
