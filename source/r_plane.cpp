// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//      Here is a core component: drawing the floors and ceilings,
//       while maintaining a per column clipping list only.
//      Moreover, the sky areas have to be determined.
//
// MAXVISPLANES is no longer a limit on the number of visplanes,
// but a limit on the number of hash slots; larger numbers mean
// better performance usually but after a point they are wasted,
// and memory and time overheads creep in.
//
// For more information on visplanes, see:
//
// http://classicgaming.com/doom/editing/
//
// Lee Killough
//
//-----------------------------------------------------------------------------

#include "z_zone.h"    /* memory allocation wrappers -- killough */
#include "i_system.h"

#include "c_io.h"
#include "d_gi.h"
#include "doomstat.h"
#include "ev_specials.h"
#include "p_anim.h"
#include "p_info.h"
#include "p_slopes.h"
#include "p_user.h"
#include "r_context.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_portal.h"
#include "r_ripple.h"
#include "r_sky.h"
#include "r_state.h"
#include "r_things.h"
#include "v_alloc.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

#define MAINHASHCHAINS 1021 // prime numbers are good for hashes with modulo-based functions

//
// VALLOCATION(mainhash)
//
// haleyjd 04/30/13: When the screen resolution is changed, we need to
// reset the main visplane hash and related data to its default state,
// because all visplanes have been destroyed on account of being 
// allocated with a PU_VALLOC tag.
//
VALLOCATION(mainhash)
{
   R_ForEachContext([](rendercontext_t &basecontext) {
      planecontext_t &context =  basecontext.planecontext;
      ZoneHeap       &heap    = *basecontext.heap;

      context.freetail = nullptr;
      context.freehead = &context.freetail;
      context.floorplane = context.ceilingplane = nullptr;

      context.mainchains = zhcalloctag(
         heap, visplane_t **, MAINHASHCHAINS, sizeof(visplane_t *), PU_VALLOC, nullptr
      );
      context.mainhash = { MAINHASHCHAINS, context.mainchains, nullptr };
   });
}

// killough -- hash function for visplanes
// Empirically verified to be fairly uniform:
constexpr unsigned int visplane_hash(const unsigned int picnum, const unsigned int lightlevel,
                                     const unsigned int height, const int chains)
{
   return ((picnum * 3) + lightlevel + (height * 7)) % chains;
}

static float *g_openings = nullptr;
static float *g_skews    = nullptr;

// killough 8/1/98: set static number of openings to be large enough
// (a static limit is okay in this case and avoids difficulties in r_segs.c)
VALLOCATION(openings)
{

   g_openings = ecalloctag(float *, w * h, sizeof(float), PU_VALLOC, nullptr);
   g_skews    = ecalloctag(float *, w * h, sizeof(float), PU_VALLOC, nullptr);

   R_ForEachContext([w, h](rendercontext_t &context)
   {
      context.planecontext.openings    = g_openings + context.bounds.startcolumn * h;
      context.planecontext.lastopening = context.planecontext.openings;
      context.planecontext.skews       = g_skews + context.bounds.startcolumn * h;
      context.planecontext.lastskew = context.planecontext.skews;
   });
}


// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1

float *floorcliparray, *ceilingcliparray;

VALLOCATION(floorcliparray)
{
   float *buffer = ecalloctag(float *, w*2, sizeof(float), PU_VALLOC, nullptr);

   floorcliparray   = buffer;
   ceilingcliparray = buffer + w;

   R_ForEachContext([](rendercontext_t &basecontext) {
      planecontext_t &context = basecontext.planecontext;

      context.floorclip   = floorcliparray;
      context.ceilingclip = ceilingcliparray;
   });
}

// SoM: We have to use secondary clipping arrays for portal overlays
float *overlayfclip, *overlaycclip;

VALLOCATION(overlayfclip)
{
   float *buffer = ecalloctag(float *, w*2, sizeof(float), PU_VALLOC, nullptr);
   overlayfclip = buffer;
   overlaycclip = buffer + w;
}

VALLOCATION(spanstart)
{
   R_ForEachContext([h](rendercontext_t &context) {
      context.planecontext.spanstart = zhcalloctag(*context.heap, int *, h, sizeof(int), PU_VALLOC, nullptr);
   });
}

//
// texture mapping
//

VALLOCATION(slopespan)
{
   cb_slopespan_t::colormap = ecalloctag(const lighttable_t **, w, sizeof(const lighttable_t *), PU_VALLOC, nullptr);
}

float slopevis; // SoM: used in slope lighting

// BIG FLATS
static void R_Throw(const cb_span_t &)
{
   I_Error("R_Throw called.\n");
}

static void R_ThrowSlope(const cb_slopespan_t &, const cb_span_t &)
{
   I_Error("R_Throw called.\n");
}

//
// Returns a colormap index from the given distance and lightlevel info
//
static int R_spanLight(const cb_plane_t &plane, float dist)
{
   int map = 
      (int)(plane.startmap - (1280.0f / dist)) + 1 - (extralight * LIGHTBRIGHT);

   return map < 0 ? 0 : map >= NUMCOLORMAPS ? NUMCOLORMAPS - 1 : map;
}

//
// Sets up the internal light level barriers inside the plane struct
//
static void R_planeLight(cb_plane_t &plane)
{
   // This formula was taken (almost) directly from r_main.c where the zlight
   // table is generated.
   plane.startmap = 2.0f * (30.0f - (plane.lightlevel / 8.0f));
}

//
// BASIC PRIMITIVE
//
static void R_mapPlane(const R_FlatFunc flatfunc, const R_SlopeFunc, cb_span_t &span,
                       cb_slopespan_t &, const cb_plane_t &plane, int y, int x1, int x2)
{
   float dy, xstep, ystep, realy, slope;

#ifdef RANGECHECK
   if(x2 < x1 || x1 < 0 || x2 >= viewwindow.width || y < 0 || y >= viewwindow.height)
      I_Error("R_mapPlane: %i, %i at %i\n", x1, x2, y);
#endif
  
   // SoM: because ycenter is an actual row of pixels (and it isn't really the 
   // center row because there are an even number of rows) some corrections need
   // to be made depending on where the row lies relative to the ycenter row.
   if(view.ycenter == y)
      dy = 0.01f;
   else if(y < view.ycenter)
      dy = (float)fabs(view.ycenter - y) - 1;
   else
      dy = (float)fabs(view.ycenter - y) + 1;

   slope = (float)fabs(plane.height / dy);
   realy = slope * view.yfoc;

   xstep = plane.pviewcos * slope * view.focratio * plane.xscale;
   ystep = plane.pviewsin * slope * view.focratio * plane.yscale;

   // Use fast hack routine for portable double->uint32 conversion
   // iff we know host endianness, otherwise use Mozilla routine
   {
      const double xfrac = (
         ((plane.pviewx + plane.xoffset) * plane.xscale) +
         (plane.pviewsin * realy * plane.xscale) +
         ((double(x1) - view.xcenter) * xstep)
      ) * plane.fixedunitx;

      span.xfrac = R_doubleToUint32(xfrac);

      const double yfrac = (
         ((-plane.pviewy + plane.yoffset) * plane.yscale) +
         (-plane.pviewcos * realy * plane.yscale) +
         ((double(x1) - view.xcenter) * ystep)
      ) * plane.fixedunity;

      span.yfrac = R_doubleToUint32(yfrac);

      span.xstep = R_doubleToUint32(xstep * plane.fixedunitx);
      span.ystep = R_doubleToUint32(ystep * plane.fixedunity);
   }

   // killough 2/28/98: Add offsets
   if((span.colormap = plane.fixedcolormap) == nullptr) // haleyjd 10/16/06
      span.colormap = plane.colormap + R_spanLight(plane, realy) * 256;
   
   span.y  = y;
   span.x1 = x1;
   span.x2 = x2;
   span.source = plane.source;
   
   // BIG FLATS
   flatfunc(span);
}

//
// R_slopeLights
//
static void R_slopeLights(const cb_plane_t &plane, int x1, int x2, double startcmap, double endcmap)
{
   int i;
   fixed_t map, map2, step;
   const int len = x2 - x1 + 1;

#ifdef RANGECHECK
   if(len > video.width)
      I_Error("R_slopeLights: len > video.width (%d)\n", len);
#endif

   if(plane.fixedcolormap)
   {
      for(i = 0; i < len; i++)
         cb_slopespan_t::colormap[i + x1] = plane.fixedcolormap;
      return;
   }

   map  = M_FloatToFixed((startcmap / 256.0 * NUMCOLORMAPS));
   map2 = M_FloatToFixed((endcmap   / 256.0 * NUMCOLORMAPS));

   if(len > 1)
      step = (map2 - map) / (len - 1);
   else
      step = 0;

   for(i = 0; i < len; i++)
   {
      int index = (int)(map >> FRACBITS) + 1;

      index -= (extralight * LIGHTBRIGHT);

      if(index < 0)
         cb_slopespan_t::colormap[i + x1] = (byte *)(plane.colormap);
      else if(index >= NUMCOLORMAPS)
         cb_slopespan_t::colormap[i + x1] = (byte *)(plane.colormap + ((NUMCOLORMAPS - 1) * 256));
      else
         cb_slopespan_t::colormap[i + x1] = (byte *)(plane.colormap + (index * 256));

      map += step;
   }
}

//
// R_mapSlope
//
static void R_mapSlope(const R_FlatFunc, const R_SlopeFunc slopefunc,
                       cb_span_t &span, cb_slopespan_t &slopespan, const cb_plane_t &plane,
                       int y, int x1, int x2)
{
   const rslope_t *slope = plane.slope;
   int count = x2 - x1;
   v3double_t s;
   double map1, map2;

   s.x = x1 - view.xcenter;
   s.y = y - view.ycenter + 1;
   s.z = view.xfoc;

   slopespan.iufrac = M_DotVec3(&s, &slope->A) * double(plane.tex->width) * double(plane.yscale);
   slopespan.ivfrac = M_DotVec3(&s, &slope->B) * double(plane.tex->height) * double(plane.xscale);
   slopespan.idfrac = M_DotVec3(&s, &slope->C);

   slopespan.iustep = slope->A.x * double(plane.tex->width) * double(plane.yscale);
   slopespan.ivstep = slope->B.x * double(plane.tex->height) * double(plane.xscale);
   slopespan.idstep = slope->C.x;

   slopespan.source = plane.source;
   slopespan.x1 = x1;
   slopespan.x2 = x2;
   slopespan.y = y;

   // Setup lighting

   map1 = 256.0 - (slope->shade - slope->plight * slopespan.idfrac);
   if(count > 0)
   {
      double id = slopespan.idfrac + slopespan.idstep * (x2 - x1);
      map2 = 256.0 - (slope->shade - slope->plight * id);
   }
   else
      map2 = map1;

   R_slopeLights(plane, x1, x2, (256.0 - map1), (256.0 - map2));

   slopefunc(slopespan, span);
}

#define CompFloats(x, y) (fabs(x - y) < 0.001f)

//
// R_CompareSlopes
// 
// SoM: Returns true if the texture spaces of the give slope structs are the
// same.
//
bool R_CompareSlopes(const pslope_t *s1, const pslope_t *s2)
{
   return 
      (s1 == s2) ||                 // both are equal, including both nullptr; OR:
       (s1 && s2 &&                 // both are valid and...
        CompFloats(s1->normalf.x, s2->normalf.x) &&  // components are equal and...
        CompFloats(s1->normalf.y, s2->normalf.y) &&
        CompFloats(s1->normalf.z, s2->normalf.z) &&
        fabs(P_DistFromPlanef(&s2->of, &s1->of, &s1->normalf)) < 0.001f); // this.
}

//
// Compares slopes by swapping one's normal. Useful for checking if two slopes are touching
// each other in the same sector.
//
bool R_CompareSlopesFlipped(const pslope_t *s1, const pslope_t *s2)
{
   return
      (s1 == s2) ||                 // both are equal, including both nullptr; OR:
       (s1 && s2 &&                 // both are valid and...
        CompFloats(s1->normalf.x, -s2->normalf.x) &&  // components are equal and...
        CompFloats(s1->normalf.y, -s2->normalf.y) &&
        CompFloats(s1->normalf.z, -s2->normalf.z) &&
        fabs(P_DistFromPlanef(&s2->of, &s1->of, &s1->normalf)) < 0.001f); // this.
}

#undef CompFloats

//
// SoM: Calculates the rslope info from the OHV vectors and rotation/offset 
// information in the plane struct
//
static void R_calcSlope(const cbviewpoint_t &cb_viewpoint, visplane_t *pl)
{
   // This is where the crap gets calculated. Yay
   double         xl, yl, tsin, tcos;
   double         ixscale, iyscale;
   rslope_t       *rslope = &pl->rslope;

   if(!pl->pslope)
      return;

   
   tsin = sin(pl->angle);
   tcos = cos(pl->angle);

   if(pl->picnum & PL_SKYFLAT)
      xl = yl = 64;  // just choose a default, it won't matter
   else
   {
      const texture_t *tex = textures[texturetranslation[pl->picnum]];
      xl = tex->width;
      yl = tex->height;
   }

   // SoM: To change the origin of rotation, add an offset to P.x and P.z
   // SoM: Add offsets? YAH!

   // Need to reduce them to the visible range, because otherwise it may overflow
   double xoffsf = fmod(pl->xoffsf, xl / pl->scale.x);
   double yoffsf = fmod(pl->yoffsf, yl / pl->scale.y);

   // P, M, and N are basically three points on the plane which then become two directional vectors:
   // (M = M - P, N = N - P) and then the cross product between the origin vector P and the directional vectors M,
   // and N then are used to define this coordinate space translation

   // There are various nuances along the way to define things in terms of 64x64 texture space and into screen space,
   // but that's the gist of it

   v3double_t P;
   P.x = -xoffsf * tcos - yoffsf * tsin;
   P.z = -xoffsf * tsin + yoffsf * tcos;
   P.y = P_GetZAtf(pl->pslope, (float)P.x, (float)P.z);

   v3double_t M;
   M.x = P.x - xl * tsin;
   M.z = P.z + xl * tcos;
   M.y = P_GetZAtf(pl->pslope, (float)M.x, (float)M.z);

   v3double_t N;
   N.x = P.x + yl * tcos;
   N.z = P.z + yl * tsin;
   N.y = P_GetZAtf(pl->pslope, (float)N.x, (float)N.z);

   M_TranslateVec3(cb_viewpoint, &P);
   M_TranslateVec3(cb_viewpoint, &M);
   M_TranslateVec3(cb_viewpoint, &N);

   M_SubVec3(&M, &M, &P);
   M_SubVec3(&N, &N, &P);
   
   M_CrossProduct3(&rslope->A, &P, &N);
   M_CrossProduct3(&rslope->B, &P, &M);
   M_CrossProduct3(&rslope->C, &M, &N);

   // This is helpful for removing some of the muls when calculating light.

   rslope->A.x *= 0.5f;
   rslope->A.y *= 0.5f / view.focratio;
   rslope->A.z *= 0.5f;

   rslope->B.x *= 0.5f;
   rslope->B.y *= 0.5f / view.focratio;
   rslope->B.z *= 0.5f;

   rslope->C.x *= 0.5f;
   rslope->C.y *= 0.5f / view.focratio;
   rslope->C.z *= 0.5f;

   rslope->zat = P_GetZAtf(pl->pslope, pl->viewxf, pl->viewyf);

   // More help from randy. I was totally lost on this... 
   ixscale = view.tan / (float)xl;
   iyscale = view.tan / (float)yl;

   rslope->plight = (slopevis * ixscale * iyscale) / (rslope->zat - pl->viewzf);
   rslope->shade = 256.0 * 2.0 - (pl->lightlevel + 16.0) * 256.0 / 128.0;
}

//
// Allocates and returns a new planehash_t object. The hash object is allocated
// PU_LEVEL
//
planehash_t *R_NewPlaneHash(ZoneHeap &heap, int chaincount)
{
   planehash_t*  ret;
   int           i;
   
   // Make sure chaincount is a power of 2
   if((chaincount - 1) & chaincount)
   {
      int c = 2;
      while(c < chaincount)
         c <<= 1;
      
      chaincount = c;
   }
   
   ret = zhmalloctag(heap, planehash_t *, sizeof(planehash_t), PU_LEVEL, nullptr);
   ret->chaincount = chaincount;
   ret->chains = zhmalloctag(heap, visplane_t **, sizeof(visplane_t *) * chaincount, PU_LEVEL, nullptr);
   ret->next = nullptr;
   
   for(i = 0; i < chaincount; i++)
      ret->chains[i] = nullptr;
      
   return ret;
}

//
// Empties the chains of the given hash table and places the planes within 
// in the free stack.
//
void R_ClearPlaneHash(visplane_t **&freehead, planehash_t *table)
{
   for(int i = 0; i < table->chaincount; i++)    // new code -- killough
   {
      for(*freehead = table->chains[i], table->chains[i] = nullptr; *freehead; )
         freehead = &(*freehead)->next;
   }
}

//
// Clears the arrays used to clip portal overlays. This function is called before the start of
// each portal rendering.
//
void R_ClearOverlayClips(const contextbounds_t &bounds)
{
   int i;

   // opening / clipping determination
   for(i = bounds.startcolumn; i < bounds.endcolumn; ++i)
   {
      overlayfclip[i] = view.height - 1.0f;
      overlaycclip[i] = 0.0f;
   }
}



//
// At begining of frame.
//
void R_ClearPlanes(planecontext_t &context, const contextbounds_t &bounds)
{
   int i;
   float a = 0.0f;

   context.floorclip   = floorcliparray;
   context.ceilingclip = ceilingcliparray;

   // opening / clipping determination
   for(i = bounds.startcolumn; i < bounds.endcolumn; ++i)
   {
      context.floorclip[i]   = overlayfclip[i] = view.height - 1.0f;
      context.ceilingclip[i] = overlaycclip[i] = a;
   }

   R_ClearPlaneHash(context.freehead, &context.mainhash);

   context.lastopening = context.openings;
   context.lastskew    = context.skews;
}


//
// New function, by Lee Killough
//
static visplane_t *new_visplane(planecontext_t &context, ZoneHeap &heap,
                                unsigned hash, planehash_t *table)
{
   visplane_t  *&freetail = context.freetail;
   visplane_t **&freehead = context.freehead;

   visplane_t *check = freetail;

   if(!check)
      check = zhcalloctag(heap, visplane_t *, 1, sizeof * check, PU_VALLOC, nullptr);
   else if(!(freetail = freetail->next))
      freehead = &freetail;

   check->next = table->chains[hash];
   table->chains[hash] = check;

   check->table = table;

   if(!check->top)
   {
      int *paddedTop, *paddedBottom;

      // THREAD_TODO: Try make this use context.numcolumns again
      check->max_width = static_cast<unsigned int>(video.width);
      paddedTop = zhcalloctag(heap, int *, 2 * (video.width + 2), sizeof(int), PU_VALLOC, nullptr);
      paddedBottom     = paddedTop + video.width + 2;

      check->top    = paddedTop    + 1;
      check->bottom = paddedBottom + 1;
   }

   check->chainnum = hash;

   return check;
}

//
// killough 2/28/98: Add offsets
// haleyjd 01/05/08: Add angle
//
visplane_t *R_FindPlane(cmapcontext_t &cmapcontext, planecontext_t &planecontext, ZoneHeap &heap,
                        const viewpoint_t &viewpoint, const cbviewpoint_t &cb_viewpoint,
                        const contextbounds_t &bounds,
                        fixed_t height, int picnum, int lightlevel,
                        v2fixed_t offs, v2float_t scale, float angle,
                        pslope_t *slope, int blendflags, byte opacity,
                        planehash_t *table)
{
   visplane_t *check;
   unsigned int hash;                      // killough
   float tsin, tcos;

   // SoM: table == nullptr means use main table
   if(!table)
      table = &planecontext.mainhash;
      
   blendflags &= PS_OBLENDFLAGS;

   // haleyjd: tweak opacity/blendflags when 100% opaque is specified
   if(!(blendflags & PS_ADDITIVE) && opacity == 255 &&
      (picnum & PL_SKYFLAT || !(textures[texturetranslation[picnum]]->flags & TF_MASKED)))
   {
      blendflags = 0;
   }
      
   // killough 10/98: PL_SKYFLAT
   if(R_IsSkyFlat(picnum) || picnum & PL_SKYFLAT)
   {
      lightlevel = 0;   // killough 7/19/98: most skies map together

      // haleyjd 05/06/08: but not all. If height > viewpoint.z, set height to
      // 1 instead of 0, to keep ceilings mapping with ceilings, and floors
      // mapping with floors.
      if(height > viewpoint.z)
         height = 1;
   }

   // New visplane algorithm uses hash table -- killough
   hash = visplane_hash(picnum, lightlevel, height >> 16, table->chaincount);

   for(check = table->chains[hash]; check; check = check->next)  // killough
   {
      // FIXME: ok this has gotten out of hand now!
      // We need a more intelligent hash function or something.
      if(height == check->height &&
         picnum == check->picnum &&
         lightlevel == check->lightlevel &&
         offs == check->offs &&      // killough 2/28/98: Add offset checks
         scale == check->scale &&
         angle == check->angle &&      // haleyjd 01/05/08: Add angle
         cmapcontext.zlight == check->colormap &&
         cmapcontext.fixedcolormap == check->fixedcolormap &&
         viewpoint.x == check->viewx &&
         viewpoint.y == check->viewy &&
         viewpoint.z == check->viewz &&
         blendflags == check->bflags &&
         opacity == check->opacity &&
         R_CompareSlopes(check->pslope, slope)
        )
        return check;
   }

   check = new_visplane(planecontext, heap, hash, table);         // killough

   check->height = height;
   check->picnum = picnum;
   check->lightlevel = lightlevel;
   check->minx = bounds.endcolumn;     // Was SCREENWIDTH -- killough 11/98
   check->maxx = bounds.startcolumn - 1;
   check->offs = offs;               // killough 2/28/98: Save offsets
   check->scale = scale;
   check->angle = angle;               // haleyjd 01/05/08: Save angle
   check->colormap      = cmapcontext.zlight;
   check->fixedcolormap = cmapcontext.fixedcolormap; // haleyjd 10/16/06
   check->fullcolormap  = cmapcontext.fullcolormap;
   
   check->viewx = viewpoint.x;
   check->viewy = viewpoint.y;
   check->viewz = viewpoint.z;
   
   check->heightf = M_FixedToFloat(height);
   check->xoffsf  = M_FixedToFloat(offs.x);
   check->yoffsf  = M_FixedToFloat(offs.y);
   
   check->bflags = blendflags;
   check->opacity = opacity;

   // haleyjd 01/05/08: modify viewing angle with respect to flat angle
   check->viewsin = sinf(cb_viewpoint.angle + check->angle);
   check->viewcos = cosf(cb_viewpoint.angle + check->angle);
   
   // SoM: set up slope type stuff
   if((check->pslope = slope))
   {
      check->viewxf = cb_viewpoint.x;
      check->viewyf = cb_viewpoint.y;
      check->viewzf = cb_viewpoint.z;
      R_calcSlope(cb_viewpoint, check);
   }
   else
   {
      // haleyjd 01/05/08: rotate viewpoint by flat angle.
      // note that the signs of the sine terms must be reversed in order to flip
      // the y-axis of the flat relative to world coordinates

      tsin = (float) sin(check->angle);
      tcos = (float) cos(check->angle);
      check->viewxf =  cb_viewpoint.x * tcos + cb_viewpoint.y * tsin;
      check->viewyf = -cb_viewpoint.x * tsin + cb_viewpoint.y * tcos;
      check->viewzf =  cb_viewpoint.z;
   }
   
   {
      unsigned int i = 0;
      int *p = check->top;
      while(i < check->max_width) 
         p[i++] = 0x7FFFFFFF;
   }
   
   return check;
}

//
// From PrBoom+
// cph 2003/04/18 - create duplicate of existing visplane and set initial range
//
visplane_t *R_DupPlane(planecontext_t &context, ZoneHeap &heap, const visplane_t *pl, int start, int stop)
{
   planehash_t *table  = pl->table;
   visplane_t  *new_pl = new_visplane(context, heap, pl->chainnum, table);

   new_pl->height = pl->height;
   new_pl->picnum = pl->picnum;
   new_pl->lightlevel = pl->lightlevel;
   new_pl->colormap = pl->colormap;
   new_pl->fixedcolormap = pl->fixedcolormap; // haleyjd 10/16/06
   new_pl->offs = pl->offs;                 // killough 2/28/98
   new_pl->angle = pl->angle;                 // haleyjd 01/05/08

   new_pl->viewsin = pl->viewsin;             // haleyjd 01/06/08
   new_pl->viewcos = pl->viewcos;

   new_pl->viewx = pl->viewx;
   new_pl->viewy = pl->viewy;
   new_pl->viewz = pl->viewz;

   new_pl->viewxf = pl->viewxf;
   new_pl->viewyf = pl->viewyf;
   new_pl->viewzf = pl->viewzf;

   // SoM: copy converted stuffs too
   new_pl->heightf = pl->heightf;
   new_pl->scale = pl->scale;
   new_pl->xoffsf = pl->xoffsf;
   new_pl->yoffsf = pl->yoffsf;

   new_pl->bflags = pl->bflags;
   new_pl->opacity = pl->opacity;

   new_pl->pslope = pl->pslope;
   memcpy(&new_pl->rslope, &pl->rslope, sizeof(rslope_t));
   new_pl->fullcolormap = pl->fullcolormap;

   visplane_t *retpl = new_pl;
   retpl->minx = start;
   retpl->maxx = stop;
   {
      int *p = retpl->top;
      const int *const p_end  = p + retpl->max_width;
      // Unrolling this loop makes performance WORSE for optimised MSVC builds.
      // Nothing makes sense any more.
      while(p < p_end)
         *(p++) = 0x7FFFFFFF;
   }
   return retpl;
}

//
// R_CheckPlane
//
visplane_t *R_CheckPlane(planecontext_t &context, ZoneHeap &heap, visplane_t *pl, int start, int stop)
{
   int intrl, intrh, unionl, unionh, x;
   
   if(start < pl->minx)
   {
      intrl   = pl->minx;
      unionl = start;
   }
   else
   {
      unionl  = pl->minx;
      intrl = start;
   }
   
   if(stop  > pl->maxx)
   {
      intrh   = pl->maxx;
      unionh = stop;
   }
   else
   {
      unionh  = pl->maxx;
      intrh  = stop;
   }

   for(x = intrl; x <= intrh && pl->top[x] == 0x7FFFFFFF; ++x)
      ;

   if(x > intrh)
   {
      pl->minx = unionl;
      pl->maxx = unionh;
   }
   else
      pl = R_DupPlane(context, heap, pl, start, stop);

   return pl;
}

//
// R_makeSpans
//
static void R_makeSpans(const R_FlatFunc flatfunc, const R_SlopeFunc slopefunc,
                        cb_span_t &span, cb_slopespan_t &slopespan, const cb_plane_t &plane,
                        int *const spanstart, int x, int t1, int b1, int t2, int b2)
{
#ifdef RANGECHECK
   // haleyjd: do not allow this loop to trash the BSS data
   if(b2 >= video.height)
      I_Error("R_makeSpans: b2 >= video.height\n");
#endif

   for(; t2 > t1 && t1 <= b1; t1++)
      plane.MapFunc(flatfunc, slopefunc, span, slopespan, plane, t1, spanstart[t1], x - 1);
   for(; b2 < b1 && t1 <= b1; b1--)
      plane.MapFunc(flatfunc, slopefunc, span, slopespan, plane, b1, spanstart[b1], x - 1);
   while(t2 < t1 && t2 <= b2)
      spanstart[t2++] = x;
   while(b2 > b1 && t2 <= b2)
      spanstart[b2--] = x;
}

//
// Get the sky column from input parms. Shared by the sky drawers here.
//
inline static int32_t R_getSkyColumn(angle_t an, int x, angle_t flip, int offset)
{
   return ((((an + xtoviewangle[x]) ^ flip) / (1 << (ANGLETOSKYSHIFT - FRACBITS))) + offset)
         / FRACUNIT;
}

// haleyjd: moved here from r_newsky.c
static void do_draw_newsky(cmapcontext_t &context, ZoneHeap &heap, const angle_t viewangle, visplane_t *pl)
{
   cb_column_t column{};

   R_ColumnFunc colfunc = r_column_engine->DrawColumn;

   angle_t an = viewangle;

   // render two layers

   // get scrolling offsets and textures
   skyflat_t *skyflat1 = R_SkyFlatForIndex(0);
   skyflat_t *skyflat2 = R_SkyFlatForIndex(1);

   if(!(skyflat1 && skyflat2))
      return; // feh!

   int offset      = skyflat1->columnoffset;
   int skyTexture  = texturetranslation[skyflat1->texture];
   int offset2     = skyflat2->columnoffset;
   int skyTexture2 = texturetranslation[skyflat2->texture];

   const skytexture_t *sky1 = R_GetSkyTexture(skyTexture);
   const skytexture_t *sky2 = R_GetSkyTexture(skyTexture2);
      
   if(getComp(comp_skymap) || !(column.colormap = context.fixedcolormap))
      column.colormap = context.fullcolormap;
      
   // first draw sky 2 with R_DrawColumn (unmasked)
   if(LevelInfo.sky2RowOffset != SKYROWOFFSET_DEFAULT)
      column.texmid = LevelInfo.sky2RowOffset * FRACUNIT;
   else
      column.texmid = sky2->texturemid;
   column.texheight = sky2->height;
   column.skycolor = sky2->medianColor;
   column.step = M_FloatToFixed(view.pspriteystep);

   colfunc = r_column_engine->DrawSkyColumn;
   for(int x = pl->minx; (column.x = x) <= pl->maxx; x++)
   {
      if((column.y1 = pl->top[x]) <= (column.y2 = pl->bottom[x]))
      {
         column.source = R_GetRawColumn(heap, skyTexture2, R_getSkyColumn(an, x, 0, offset2));

         colfunc(column);
      }
   }
      
   // now draw sky 1 with R_DrawNewSkyColumn (masked)
   if(LevelInfo.skyRowOffset != SKYROWOFFSET_DEFAULT)
      column.texmid = LevelInfo.skyRowOffset * FRACUNIT;
   else
      column.texmid = sky1->texturemid;
   column.texheight = sky1->height;

   column.step = M_FloatToFixed(view.pspriteystep);
      
   colfunc = r_column_engine->DrawNewSkyColumn;
   for(int x = pl->minx; (column.x = x) <= pl->maxx; x++)
   {
      if((column.y1 = pl->top[x]) <= (column.y2 = pl->bottom[x]))
      {
         column.source = R_GetRawColumn(heap, skyTexture, R_getSkyColumn(an, x, 0, offset));

         colfunc(column);
      }
   }
}

// Log base 2 LUT
static const int MultiplyDeBruijnBitPosition2[32] =
{
   0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
   31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};

//
// Drawing sky as a background texture instead of a visplane.
//
static void R_drawSky(ZoneHeap &heap, angle_t viewangle, const visplane_t *pl, const skyflat_t *skyflat)
{
   int texture;
   int offset = 0;
   angle_t flip;
   const skytexture_t *sky;

   // killough 10/98: allow skies to come from sidedefs.
   // Allows scrolling and/or animated skies, as well as
   // arbitrary multiple skies per level without having
   // to use info lumps.

   angle_t an = viewangle;

   cb_column_t column{};
   bool tilevert = false;
   if(pl->picnum & PL_SKYFLAT)
   {
      // Sky Linedef
      const line_t *l = &lines[pl->picnum & ~PL_SKYFLAT];

      // Sky transferred from first sidedef
      const side_t *s = *l->sidenum + sides;

      // Texture comes from upper texture of reference sidedef
      texture = texturetranslation[s->toptexture];

      // haleyjd 08/30/02: set skytexture info pointer
      sky = R_GetSkyTexture(texture);

      // Horizontal offset is turned into an angle offset,
      // to allow sky rotation as well as careful positioning.
      // However, the offset is scaled very small, so that it
      // allows a long-period of sky rotation.

      an += s->offset_base_x;

      // Vertical offset allows careful sky positioning.

      column.texmid = s->offset_base_y - 28*FRACUNIT;

      // Adjust it upwards to make sure the fade-to-color effect doesn't happen too early
      tilevert = !!(s->intflags & SDI_VERTICALLYSCROLLING);
      if(!tilevert && column.texmid < SCREENHEIGHT / 2 * FRACUNIT)
      {
         fixed_t diff = column.texmid - SCREENHEIGHT / 2 * FRACUNIT;
         diff %= textures[sky->texturenum]->heightfrac;
         if(diff < 0)
            diff += textures[sky->texturenum]->heightfrac;
         column.texmid = SCREENHEIGHT / 2 * FRACUNIT + diff;
      }

      // We sometimes flip the picture horizontally.
      //
      // Doom always flipped the picture, so we make it optional,
      // to make it easier to use the new feature, while to still
      // allow old sky textures to be used.
      int staticFn = EV_StaticInitForSpecial(l->special);

      bool flipCond = staticFn == EV_STATIC_SKY_TRANSFER_FLIPPED
      || (staticFn == EV_STATIC_INIT_PARAM
          && l->args[ev_StaticInit_Arg_Flip]);

      flip = flipCond ? 0u : ~0u;
   }
   else     // Normal Doom sky, only one allowed per level
   {
      texture       = skyflat->texture;            // Default texture
      sky           = R_GetSkyTexture(texture);    // haleyjd 08/30/02
      flip          = 0;                           // Doom flips it
      // Hexen-style scrolling
      offset        = skyflat->columnoffset;

      // Set y offset depending on level info or depending on R_GetSkyTexture
      if(skyflat == R_SkyFlatForIndex(0) && LevelInfo.skyRowOffset != SKYROWOFFSET_DEFAULT)
         column.texmid = LevelInfo.skyRowOffset * FRACUNIT;
      else if(skyflat == R_SkyFlatForIndex(1) && LevelInfo.sky2RowOffset != SKYROWOFFSET_DEFAULT)
         column.texmid = LevelInfo.sky2RowOffset * FRACUNIT;
      else
         column.texmid = sky->texturemid;
   }

   // Sky is always drawn full bright, i.e. colormaps[0] is used.
   // Because of this hack, sky is not affected by INVUL inverse mapping.
   //
   // killough 7/19/98: fix hack to be more realistic:
   // haleyjd 10/31/10: use plane colormaps, not global vars!
   if(getComp(comp_skymap) || !(column.colormap = pl->fixedcolormap))
      column.colormap = pl->fullcolormap;

   //dc_texheight = (textureheight[texture])>>FRACBITS; // killough
   // haleyjd: use height determined from patches in texture
   column.texheight = sky->height;

   column.step = M_FloatToFixed(view.pspriteystep);
   column.skycolor = sky->medianColor;

   R_ColumnFunc colfunc = tilevert ? r_column_engine->DrawColumn :
                                     r_column_engine->DrawSkyColumn;

   // We need the translucency map to exist because we fade the sky to a single color when looking
   // above it.
   if(!main_tranmap)
      R_InitTranMap(false);

   // killough 10/98: Use sky scrolling offset, and possibly flip picture
   for(int x = pl->minx; x <= pl->maxx; x++)
   {
      column.x = x;

      column.y1 = pl->top[x];
      column.y2 = pl->bottom[x];

      if(column.y1 <= column.y2)
      {
         column.source = R_GetRawColumn(heap, texture, R_getSkyColumn(an, x, flip, offset));
         colfunc(column);
      }
   }
}

//
// New function, by Lee Killough
// haleyjd 08/30/02: slight restructuring to use hashed sky texture info cache.
//
static void do_draw_plane(cmapcontext_t &context, ZoneHeap &heap, int *const spanstart,
                          const angle_t viewangle, visplane_t *pl)
{
   if(!(pl->minx <= pl->maxx))
      return;

   // haleyjd: hexen-style skies
   if(R_IsSkyFlat(pl->picnum) && LevelInfo.doubleSky)
   {
      // NOTE: MBF sky transfers change pl->picnum so it won't go here if set to transfer.
      do_draw_newsky(context, heap, viewangle, pl);
      return;
   }
   
   skyflat_t *skyflat = R_SkyFlatForPicnum(pl->picnum);
   
   if(skyflat || pl->picnum & PL_SKYFLAT)  // sky flat
      R_drawSky(heap, viewangle, pl, skyflat);
   else // regular flat
   {
      const texture_t *tex;
      int stop, light;
      int stylenum;

      cb_span_t      span{};
      cb_slopespan_t slopespan{};
      cb_plane_t     plane{};

      R_FlatFunc  flatfunc  = R_Throw;
      R_SlopeFunc slopefunc = R_ThrowSlope;

      const int picnum = texturetranslation[pl->picnum];

      // haleyjd 05/19/06: rewritten to avoid crashes
      // ioanch: apply swirly if original (pl->picnum) has the flag. This is so
      // Hexen animations can control only their own sequence swirling.
      if((r_swirl && textures[picnum]->flags & TF_ANIMATED)
         || textures[pl->picnum]->flags & TF_SWIRLY)
      {
         plane.source = R_DistortedFlat(heap, picnum);
         tex = plane.tex = textures[picnum];
      }
      else
      {
         // SoM: Handled outside
         tex = plane.tex = R_GetTexture(picnum);
         plane.source = tex->bufferdata;
      }

      // haleyjd: TODO: feed pl->drawstyle to the first dimension to enable
      // span drawstyles (ie. translucency)

      stylenum = (pl->bflags & PS_ADDITIVE) ? SPAN_STYLE_ADD : 
                 (pl->opacity < 255)  ? SPAN_STYLE_TL :
                 SPAN_STYLE_NORMAL;

      if(plane.tex->flags & TF_MASKED && pl->bflags & PS_OVERLAY)
      {
         switch(stylenum)
         {
            case SPAN_STYLE_TL:
               stylenum = SPAN_STYLE_TL_MASKED;
               break;
            case SPAN_STYLE_ADD:
               stylenum = SPAN_STYLE_ADD_MASKED;
               break;
            default:
               stylenum = SPAN_STYLE_NORMAL_MASKED;
         }
         span.alphamask = static_cast<const byte *>(plane.source) + tex->width * tex->height;
      }
                
      flatfunc  = r_span_engine->DrawSpan[stylenum][tex->flatsize];
      slopefunc = r_span_engine->DrawSlope[stylenum][tex->flatsize];
      
      if(stylenum == SPAN_STYLE_TL || stylenum == SPAN_STYLE_TL_MASKED)
      {
         int level = (pl->opacity + 1) >> 2;
         
         span.fg2rgb = Col2RGB8[level];
         span.bg2rgb = Col2RGB8[64 - level];
      }
      else if(stylenum == SPAN_STYLE_ADD || stylenum == SPAN_STYLE_ADD_MASKED)
      {
         int level = (pl->opacity + 1) >> 2;
         
         span.fg2rgb = Col2RGB8_LessPrecision[level];
         span.bg2rgb = Col2RGB8_LessPrecision[64];
      }
      else
         span.fg2rgb = span.bg2rgb = nullptr;

      if(pl->pslope)
         plane.slope = &pl->rslope;
      else
         plane.slope = nullptr;
         
      {
         int rw, rh;
         
         rh = MultiplyDeBruijnBitPosition2[(uint32_t)(tex->height * 0x077CB531U) >> 27];
         rw = MultiplyDeBruijnBitPosition2[(uint32_t)(tex->width  * 0x077CB531U) >> 27];

         if(plane.slope)
         {
            span.ymask = tex->height - 1;
            
            span.xshift = 16 - rh;
            span.xmask = (tex->width - 1) << (16 - span.xshift);
         }
         else
         {
            span.yshift = 32 - rh;
            
            span.xshift = span.yshift - rw;
            span.xmask = (tex->width - 1) << (32 - rw - span.xshift);
            
            plane.fixedunitx = (float)(1 << (32 - rw));
            plane.fixedunity = (float)(1 << span.yshift);
         }
      }
       
        
      plane.xoffset = pl->xoffsf;  // killough 2/28/98: Add offsets
      plane.yoffset = pl->yoffsf;

      plane.xscale = pl->scale.x;
      plane.yscale = pl->scale.y;

      plane.pviewx   = pl->viewxf;
      plane.pviewy   = pl->viewyf;
      plane.pviewz   = pl->viewzf;
      plane.pviewsin = pl->viewsin; // haleyjd 01/05/08: Add angle
      plane.pviewcos = pl->viewcos;
      plane.height   = pl->heightf - pl->viewzf;
      
      // SoM 10/19/02: deep water colormap fix
      if(context.fixedcolormap)
         light = (255  >> LIGHTSEGSHIFT);
      else
         light = (pl->lightlevel >> LIGHTSEGSHIFT) + (extralight * LIGHTBRIGHT);

      if(light >= LIGHTLEVELS)
         light = LIGHTLEVELS-1;

      if(light < 0)
         light = 0;

      stop = pl->maxx + 1;
      pl->top[pl->minx-1] = pl->top[stop] = 0x7FFFFFFF;

      plane.planezlight   = pl->colormap[light]; //zlight[light];
      plane.colormap      = pl->fullcolormap;
      plane.fixedcolormap = pl->fixedcolormap; // haleyjd 10/16/06
      plane.lightlevel    = pl->lightlevel;

      R_planeLight(plane);

      plane.MapFunc = (plane.slope == nullptr ? R_mapPlane : R_mapSlope);

      for(int x = pl->minx; x <= stop; x++)
      {
         R_makeSpans(
            flatfunc, slopefunc, span, slopespan, plane, spanstart, x,
            pl->top[x - 1], pl->bottom[x - 1], pl->top[x], pl->bottom[x]
         );
      }
   }
}

//
// Called after the BSP has been traversed and walls have rendered. This 
// function is also now used to render portal overlays.
//
void R_DrawPlanes(cmapcontext_t &context, ZoneHeap &heap, planehash_t &mainhash,
                  int *const spanstart, const angle_t viewangle, planehash_t *table)
{
   visplane_t *pl;
   int i;
   
   if(!table)
      table = &mainhash;
   
   for(i = 0; i < table->chaincount; ++i)
   {
      for(pl = table->chains[i]; pl; pl = pl->next)
         do_draw_plane(context, heap, spanstart, viewangle, pl);
   }
}

VALLOCATION(overlaySets)
{
   R_ForEachContext([](rendercontext_t &basecontext) {
      for(planehash_t *set = basecontext.planecontext.r_overlayfreesets; set; set = set->next)
         memset(set->chains, 0, set->chaincount * sizeof(*set->chains));
   });
}

//
// Gets a free portal overlay plane set
//
planehash_t *R_NewOverlaySet(planecontext_t &context, ZoneHeap &heap)
{
   planehash_t *&r_overlayfreesets = context.r_overlayfreesets;

   planehash_t *set;
   if(!r_overlayfreesets)
   {
      set = R_NewPlaneHash(heap, 131);
      return set;
   }
   set = r_overlayfreesets;
   r_overlayfreesets = r_overlayfreesets->next;
   R_ClearPlaneHash(context.freehead, set);
   return set;
}

//
// Puts the overlay set in the free list
//
void R_FreeOverlaySet(planehash_t *&r_overlayfreesets, planehash_t *set)
{
   set->next = r_overlayfreesets;
   r_overlayfreesets = set;
}

//
// Called from P_SetupLevel, considering that overlay sets are PU_LEVEL
//
void R_MapInitOverlaySets()
{
   R_ForEachContext([](rendercontext_t &basecontext) {
      planecontext_t &context = basecontext.planecontext;

      context.r_overlayfreesets = nullptr;
   });
}

//----------------------------------------------------------------------------
//
// $Log: r_plane.c,v $
// Revision 1.8  1998/05/03  23:09:53  killough
// Fix #includes at the top
//
// Revision 1.7  1998/04/27  01:48:31  killough
// Program beautification
//
// Revision 1.6  1998/03/23  03:38:26  killough
// Use 'fullcolormap' for fully-bright F_SKY1
//
// Revision 1.5  1998/03/02  11:47:13  killough
// Add support for general flats xy offsets
//
// Revision 1.4  1998/02/09  03:16:03  killough
// Change arrays to use MAX height/width
//
// Revision 1.3  1998/02/02  13:28:40  killough
// performance tuning
//
// Revision 1.2  1998/01/26  19:24:45  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
