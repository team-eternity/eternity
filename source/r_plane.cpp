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

#ifdef _SDL_VER
#include "SDL_endian.h"
#endif

#define MAINHASHCHAINS 257 // prime numbers are good for hashes with modulo-based functions

static visplane_t *freetail;                   // killough
static visplane_t **freehead = &freetail;      // killough
visplane_t *floorplane, *ceilingplane;


// SoM: New visplane hash
// This is the main hash object used by the normal scene.
static visplane_t *mainchains[MAINHASHCHAINS];   // killough
static planehash_t  mainhash = { MAINHASHCHAINS,  mainchains, nullptr };

// Free list of overlay portals. Used by portal windows and the post-BSP stack.
static planehash_t *r_overlayfreesets;

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
   freetail = NULL;
   freehead = &freetail;
   floorplane = ceilingplane = NULL;

   memset(mainchains, 0, sizeof(mainchains));
}

// killough -- hash function for visplanes
// Empirically verified to be fairly uniform:
#define visplane_hash(picnum, lightlevel, height, chains) \
  (((unsigned int)(picnum)*3+(unsigned int)(lightlevel)+(unsigned int)(height)*7) % chains)


// killough 8/1/98: set static number of openings to be large enough
// (a static limit is okay in this case and avoids difficulties in r_segs.c)
float *openings, *lastopening;

VALLOCATION(openings)
{
   openings = ecalloctag(float *, w*h, sizeof(float), PU_VALLOC, NULL);
   lastopening = openings;
}


// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1

// SoM 12/8/03: floorclip and ceilingclip changed to pointers so they can be set
// to the clipping arrays of portals.
float *floorcliparray, *ceilingcliparray;
float *floorclip, *ceilingclip;

VALLOCATION(floorcliparray)
{
   float *buffer = ecalloctag(float *, w*2, sizeof(float), PU_VALLOC, NULL);

   floorclip   = floorcliparray   = buffer;
   ceilingclip = ceilingcliparray = buffer + w;
}

// SoM: We have to use secondary clipping arrays for portal overlays
float *overlayfclip, *overlaycclip;

VALLOCATION(overlayfclip)
{
   float *buffer = ecalloctag(float *, w*2, sizeof(float), PU_VALLOC, NULL);
   overlayfclip = buffer;
   overlaycclip = buffer + w;
}

// spanstart holds the start of a plane span; initialized to 0 at start
static int *spanstart;

VALLOCATION(spanstart)
{
   spanstart = ecalloctag(int *, h, sizeof(int), PU_VALLOC, NULL);
}

//
// texture mapping
//

cb_span_t      span;
cb_plane_t     plane;
cb_slopespan_t slopespan;

VALLOCATION(slopespan)
{
   size_t size = sizeof(lighttable_t *) * w;
   slopespan.colormap = ecalloctag(lighttable_t **, 1, size, PU_VALLOC, NULL);
}

float slopevis; // SoM: used in slope lighting

// BIG FLATS
static void R_Throw()
{
   I_Error("R_Throw called.\n");
}

void (*flatfunc)()  = R_Throw;
void (*slopefunc)() = R_Throw;

//
// R_SpanLight
//
// Returns a colormap index from the given distance and lightlevel info
//
static int R_SpanLight(float dist)
{
   int map = 
      (int)(plane.startmap - (1280.0f / dist)) + 1 - (extralight * LIGHTBRIGHT);

   return map < 0 ? 0 : map >= NUMCOLORMAPS ? NUMCOLORMAPS - 1 : map;
}

// 
// R_PlaneLight
//
// Sets up the internal light level barriers inside the plane struct
//
static void R_PlaneLight()
{
   // This formula was taken (almost) directly from r_main.c where the zlight
   // table is generated.
   plane.startmap = 2.0f * (30.0f - (plane.lightlevel / 8.0f));
}

//
// R_doubleToUint32
//
// haleyjd: Derived from Mozilla SpiderMonkey jsnum.c;
// used under the terms of the GPL.
//
// * The Original Code is Mozilla Communicator client code, released
// * March 31, 1998.
// *
// * The Initial Developer of the Original Code is
// * Netscape Communications Corporation.
// * Portions created by the Initial Developer are Copyright (C) 1998
// * the Initial Developer. All Rights Reserved.
// *
// * Contributor(s):
// *   IBM Corp.
//
static inline uint32_t R_doubleToUint32(double d)
{
#ifdef SDL_BYTEORDER
   // TODO: Use C++ std::endian when C++20 can be used
   // This bit (and the ifdef) isn't from SpiderMonkey.
   // Credit goes to Marrub and David Hill
   return reinterpret_cast<uint32_t *>(&(d += 6755399441055744.0))[SDL_BYTEORDER == SDL_BIG_ENDIAN];
#else
   int32_t i;
   bool    neg;
   double  two32;

   // FIXME: should check for finiteness first, but we have no code for 
   // doing that in EE yet.

   //if (!JSDOUBLE_IS_FINITE(d))
   //   return 0;

   // We check whether d fits int32, not uint32, as all but the ">>>" bit
   // manipulation bytecode stores the result as int, not uint. When the
   // result does not fit int jsval, it will be stored as a negative double.
   i = (int32_t)d;
   if((double)i == d)
      return (int32_t)i;

   neg = (d < 0);
   d   = floor(neg ? -d : d);
   d   = neg ? -d : d;

   // haleyjd: This is the important part: reduction modulo UINT_MAX.
   two32 = 4294967296.0;
   d     = fmod(d, two32);

   return (uint32_t)(d >= 0 ? d : d + two32);
#endif
}

//
// R_MapPlane
//
// BASIC PRIMITIVE
//
static void R_MapPlane(int y, int x1, int x2)
{
   float dy, xstep, ystep, realy, slope;

#ifdef RANGECHECK
   if(x2 < x1 || x1 < 0 || x2 >= viewwindow.width || y < 0 || y >= viewwindow.height)
      I_Error("R_MapPlane: %i, %i at %i\n", x1, x2, y);
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
      double value;

      value = (((plane.pviewx + plane.xoffset) * plane.xscale) + (plane.pviewsin * realy * plane.xscale) +
               (((double)x1 - view.xcenter) * xstep)) * plane.fixedunitx;

      span.xfrac = R_doubleToUint32(value);

      value = (((-plane.pviewy + plane.yoffset) * plane.yscale) + (-plane.pviewcos * realy * plane.yscale) +
               (((double)x1 - view.xcenter) * ystep)) * plane.fixedunity;

      span.yfrac = R_doubleToUint32(value);

      span.xstep = R_doubleToUint32(xstep * plane.fixedunitx);
      span.ystep = R_doubleToUint32(ystep * plane.fixedunity);
   }

   // killough 2/28/98: Add offsets
   if((span.colormap = plane.fixedcolormap) == NULL) // haleyjd 10/16/06
      span.colormap = plane.colormap + R_SpanLight(realy) * 256;
   
   span.y  = y;
   span.x1 = x1;
   span.x2 = x2;
   span.source = plane.source;
   
   // BIG FLATS
   flatfunc();
}

//
// R_SlopeLights
//
static void R_SlopeLights(int len, double startcmap, double endcmap)
{
   int i;
   fixed_t map, map2, step;

#ifdef RANGECHECK
   if(len > video.width)
      I_Error("R_SlopeLights: len > video.width (%d)\n", len);
#endif

   if(plane.fixedcolormap)
   {
      for(i = 0; i < len; i++)
         slopespan.colormap[i] = plane.fixedcolormap;
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
         slopespan.colormap[i] = (byte *)(plane.colormap);
      else if(index >= NUMCOLORMAPS)
         slopespan.colormap[i] = (byte *)(plane.colormap + ((NUMCOLORMAPS - 1) * 256));
      else
         slopespan.colormap[i] = (byte *)(plane.colormap + (index * 256));

      map += step;
   }
}

//
// R_MapSlope
//
static void R_MapSlope(int y, int x1, int x2)
{
   rslope_t *slope = plane.slope;
   int count = x2 - x1;
   v3double_t s;
   double map1, map2;

   s.x = x1 - view.xcenter;
   s.y = y - view.ycenter + 1;
   s.z = view.xfoc;

   slopespan.iufrac = M_DotVec3(&s, &slope->A) * static_cast<double>(plane.tex->width) *
                      static_cast<double>(plane.yscale);
   slopespan.ivfrac = M_DotVec3(&s, &slope->B) * static_cast<double>(plane.tex->height) *
                      static_cast<double>(plane.xscale);
   slopespan.idfrac = M_DotVec3(&s, &slope->C);

   slopespan.iustep = slope->A.x * static_cast<double>(plane.tex->width) *
                      static_cast<double>(plane.yscale);
   slopespan.ivstep = slope->B.x * static_cast<double>(plane.tex->height) *
                      static_cast<double>(plane.xscale);
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

   R_SlopeLights(x2 - x1 + 1, (256.0 - map1), (256.0 - map2));

   slopefunc();
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
      (s1 == s2) ||                 // both are equal, including both NULL; OR:
       (s1 && s2 &&                 // both are valid and...
        CompFloats(s1->normalf.x, s2->normalf.x) &&  // components are equal and...
        CompFloats(s1->normalf.y, s2->normalf.y) &&
        CompFloats(s1->normalf.z, s2->normalf.z) &&
        fabs(P_DistFromPlanef(&s2->of, &s1->of, &s1->normalf)) < 0.001f); // this.
}

#undef CompFloats

//
// R_CalcSlope
//
// SoM: Calculates the rslope info from the OHV vectors and rotation/offset 
// information in the plane struct
//
static void R_CalcSlope(visplane_t *pl)
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
   rslope->P.x = -pl->xoffsf * tcos - pl->yoffsf * tsin;
   rslope->P.z = -pl->xoffsf * tsin + pl->yoffsf * tcos;
   rslope->P.y = P_GetZAtf(pl->pslope, (float)rslope->P.x, (float)rslope->P.z);

   rslope->M.x = rslope->P.x - xl * tsin;
   rslope->M.z = rslope->P.z + xl * tcos;
   rslope->M.y = P_GetZAtf(pl->pslope, (float)rslope->M.x, (float)rslope->M.z);

   rslope->N.x = rslope->P.x + yl * tcos;
   rslope->N.z = rslope->P.z + yl * tsin;
   rslope->N.y = P_GetZAtf(pl->pslope, (float)rslope->N.x, (float)rslope->N.z);

   M_TranslateVec3(&rslope->P);
   M_TranslateVec3(&rslope->M);
   M_TranslateVec3(&rslope->N);

   M_SubVec3(&rslope->M, &rslope->M, &rslope->P);
   M_SubVec3(&rslope->N, &rslope->N, &rslope->P);
   
   M_CrossProduct3(&rslope->A, &rslope->P, &rslope->N);
   M_CrossProduct3(&rslope->B, &rslope->P, &rslope->M);
   M_CrossProduct3(&rslope->C, &rslope->M, &rslope->N);

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
   rslope->shade = 256.0f * 2.0f - (pl->lightlevel + 16.0f) * 256.0f / 128.0f;
}

//
// R_NewPlaneHash
//
// Allocates and returns a new planehash_t object. The hash object is allocated
// PU_LEVEL
//
planehash_t *R_NewPlaneHash(int chaincount)
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
   
   ret = (planehash_t *)(Z_Malloc(sizeof(planehash_t), PU_LEVEL, NULL));
   ret->chaincount = chaincount;
   ret->chains = (visplane_t **)(Z_Malloc(sizeof(visplane_t *) * chaincount, PU_LEVEL, NULL));
   ret->next = nullptr;
   
   for(i = 0; i < chaincount; i++)
      ret->chains[i] = NULL;
      
   return ret;
}

//
// R_ClearPlaneHash
//
// Empties the chains of the given hash table and places the planes within 
// in the free stack.
//
void R_ClearPlaneHash(planehash_t *table)
{
   for(int i = 0; i < table->chaincount; i++)    // new code -- killough
   {
      for(*freehead = table->chains[i], table->chains[i] = NULL; *freehead; )
         freehead = &(*freehead)->next;
   }
}

//
// R_ClearOverlayClips
//
// Clears the arrays used to clip portal overlays. This function is called before the start of 
// each portal rendering.
//
void R_ClearOverlayClips()
{
   int i;
   
   // opening / clipping determination
   for(i = 0; i < video.width; ++i)
   {
      overlayfclip[i] = view.height - 1.0f;
      overlaycclip[i] = 0.0f;
   }
}



//
// R_ClearPlanes
//
// At begining of frame.
//
void R_ClearPlanes()
{
   int i;
   float a = 0.0f;

   floorclip   = floorcliparray;
   ceilingclip = ceilingcliparray;

   // opening / clipping determination
   for(i = 0; i < video.width; ++i)
   {
      floorclip[i] = overlayfclip[i] = view.height - 1.0f;
      ceilingclip[i] = overlaycclip[i] = a;
   }

   R_ClearPlaneHash(&mainhash);

   lastopening = openings;
}


//
// new_visplane
//
// New function, by Lee Killough
//
static visplane_t *new_visplane(unsigned hash, planehash_t *table)
{
   visplane_t *check = freetail;

   if(!check)
      check = ecalloctag(visplane_t *, 1, sizeof *check, PU_VALLOC, NULL);
   else 
      if(!(freetail = freetail->next))
         freehead = &freetail;
   
   check->next = table->chains[hash];
   table->chains[hash] = check;
   
   check->table = table;

   if(!check->top)
   {
      int *paddedTop, *paddedBottom;

      check->max_width = (unsigned int)video.width;
      paddedTop    = ecalloctag(int *, 2 * (video.width + 2), sizeof(int), PU_VALLOC, NULL);
      paddedBottom = paddedTop + video.width + 2;

      check->top    = paddedTop    + 1;
      check->bottom = paddedBottom + 1;
   }
   
   return check;
}

//
// R_FindPlane
//
// killough 2/28/98: Add offsets
// haleyjd 01/05/08: Add angle
//
visplane_t *R_FindPlane(fixed_t height, int picnum, int lightlevel,
                        fixed_t xoffs, fixed_t yoffs,
                        float xscale, float yscale, float angle,
                        pslope_t *slope, int blendflags, byte opacity,
                        planehash_t *table)
{
   visplane_t *check;
   unsigned int hash;                      // killough
   float tsin, tcos;

   // SoM: table == NULL means use main table
   if(!table)
      table = &mainhash;
      
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

      // haleyjd 05/06/08: but not all. If height > viewz, set height to
      // 1 instead of 0, to keep ceilings mapping with ceilings, and floors
      // mapping with floors.
      if(height > viewz)
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
         xoffs == check->xoffs &&      // killough 2/28/98: Add offset checks
         yoffs == check->yoffs &&
         xscale == check->xscale &&
         yscale == check->yscale &&
         angle == check->angle &&      // haleyjd 01/05/08: Add angle
         zlight == check->colormap &&
         fixedcolormap == check->fixedcolormap &&
         viewx == check->viewx &&
         viewy == check->viewy &&
         viewz == check->viewz &&
         blendflags == check->bflags &&
         opacity == check->opacity &&
         R_CompareSlopes(check->pslope, slope)
        )
        return check;
   }

   check = new_visplane(hash, table);         // killough

   check->height = height;
   check->picnum = picnum;
   check->lightlevel = lightlevel;
   check->minx = viewwindow.width;     // Was SCREENWIDTH -- killough 11/98
   check->maxx = -1;
   check->xoffs = xoffs;               // killough 2/28/98: Save offsets
   check->yoffs = yoffs;
   check->xscale = xscale;
   check->yscale = yscale;
   check->angle = angle;               // haleyjd 01/05/08: Save angle
   check->colormap = zlight;
   check->fixedcolormap = fixedcolormap; // haleyjd 10/16/06
   check->fullcolormap = fullcolormap;
   
   check->viewx = viewx;
   check->viewy = viewy;
   check->viewz = viewz;
   
   check->heightf = M_FixedToFloat(height);
   check->xoffsf  = M_FixedToFloat(xoffs);
   check->yoffsf  = M_FixedToFloat(yoffs);
   
   check->bflags = blendflags;
   check->opacity = opacity;

   // haleyjd 01/05/08: modify viewing angle with respect to flat angle
   check->viewsin = (float) sin(view.angle + check->angle);
   check->viewcos = (float) cos(view.angle + check->angle);
   
   // SoM: set up slope type stuff
   if((check->pslope = slope))
   {
      check->viewxf = view.x;
      check->viewyf = view.y;
      check->viewzf = view.z;
      R_CalcSlope(check);
   }
   else
   {
      // haleyjd 01/05/08: rotate viewpoint by flat angle.
      // note that the signs of the sine terms must be reversed in order to flip
      // the y-axis of the flat relative to world coordinates

      tsin = (float) sin(check->angle);
      tcos = (float) cos(check->angle);
      check->viewxf =  view.x * tcos + view.y * tsin;
      check->viewyf = -view.x * tsin + view.y * tcos;
      check->viewzf =  view.z;
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
// R_CheckPlane
//
visplane_t *R_CheckPlane(visplane_t *pl, int start, int stop)
{
   int intrl, intrh, unionl, unionh, x;
   
   planehash_t *table = pl->table;
   
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
   {
      unsigned hash = visplane_hash(pl->picnum, pl->lightlevel, pl->height, table->chaincount);
      visplane_t *new_pl = new_visplane(hash, table);
      
      new_pl->height = pl->height;
      new_pl->picnum = pl->picnum;
      new_pl->lightlevel = pl->lightlevel;
      new_pl->colormap = pl->colormap;
      new_pl->fixedcolormap = pl->fixedcolormap; // haleyjd 10/16/06
      new_pl->xoffs = pl->xoffs;                 // killough 2/28/98
      new_pl->yoffs = pl->yoffs;
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
      new_pl->yscale = pl->yscale;
      new_pl->xscale = pl->xscale;
      new_pl->xoffsf = pl->xoffsf;
      new_pl->yoffsf = pl->yoffsf;
      
      new_pl->bflags = pl->bflags;
      new_pl->opacity = pl->opacity;

      new_pl->pslope = pl->pslope;
      memcpy(&new_pl->rslope, &pl->rslope, sizeof(rslope_t));
      new_pl->fullcolormap = pl->fullcolormap;

      pl = new_pl;
      pl->minx = start;
      pl->maxx = stop;
      {
         int *p = pl->top;
         const int *const p_end  = p + pl->max_width;
         // Unrolling this loop makes performance WORSE for optimised MSVC builds.
         // Nothing makes sense any more.
         while(p < p_end)
            *(p++) = 0x7FFFFFFF;
      }
   }
   
   return pl;
}

//
// R_MakeSpans
//
static void R_MakeSpans(int x, int t1, int b1, int t2, int b2)
{
#ifdef RANGECHECK
   // haleyjd: do not allow this loop to trash the BSS data
   if(b2 >= video.height)
      I_Error("R_MakeSpans: b2 >= video.height\n");
#endif

   for(; t2 > t1 && t1 <= b1; t1++)
      plane.MapFunc(t1, spanstart[t1], x - 1);
   for(; b2 < b1 && t1 <= b1; b1--)
      plane.MapFunc(b1, spanstart[b1], x - 1);
   while(t2 < t1 && t2 <= b2)
      spanstart[t2++] = x;
   while(b2 > b1 && t2 <= b2)
      spanstart[b2--] = x;
}

// haleyjd: moved here from r_newsky.c
static void do_draw_newsky(visplane_t *pl)
{
   int x, offset, skyTexture, offset2, skyTexture2;
   skytexture_t *sky1, *sky2;
   
   angle_t an = viewangle;
   
   // render two layers

   // get scrolling offsets and textures
   skyflat_t *skyflat1 = R_SkyFlatForIndex(0);
   skyflat_t *skyflat2 = R_SkyFlatForIndex(1);

   if(!(skyflat1 && skyflat2))
      return; // feh!

   offset      = skyflat1->columnoffset >> 16;
   skyTexture  = texturetranslation[skyflat1->texture];
   offset2     = skyflat2->columnoffset >> 16;
   skyTexture2 = texturetranslation[skyflat2->texture];

   sky1 = R_GetSkyTexture(skyTexture);
   sky2 = R_GetSkyTexture(skyTexture2);
      
   if(comp[comp_skymap] || !(column.colormap = fixedcolormap))
      column.colormap = fullcolormap;
      
   // first draw sky 2 with R_DrawColumn (unmasked)
   column.texmid    = sky2->texturemid;      
   column.texheight = sky2->height;

   // haleyjd: don't stretch textures over 200 tall
   if(demo_version >= 300 && column.texheight < 200 && stretchsky)
      column.step = M_FloatToFixed(view.pspriteystep * 0.5f);
   else
      column.step = M_FloatToFixed(view.pspriteystep);
      
   for(x = pl->minx; (column.x = x) <= pl->maxx; x++)
   {
      if((column.y1 = pl->top[x]) <= (column.y2 = pl->bottom[x]))
      {
         column.source = 
            R_GetRawColumn(skyTexture2,
               (((an + xtoviewangle[x])) >> (ANGLETOSKYSHIFT))+offset2);
            
         colfunc();
      }
   }
      
   // now draw sky 1 with R_DrawNewSkyColumn (masked)
   column.texmid = sky1->texturemid;
   column.texheight = sky1->height;

   // haleyjd: don't stretch textures over 200 tall
   if(demo_version >= 300 && column.texheight < 200 && stretchsky)
      column.step = M_FloatToFixed(view.pspriteystep * 0.5f);
   else
      column.step = M_FloatToFixed(view.pspriteystep);
      
   colfunc = r_column_engine->DrawNewSkyColumn;
   for(x = pl->minx; (column.x = x) <= pl->maxx; x++)
   {
      if((column.y1 = pl->top[x]) <= (column.y2 = pl->bottom[x]))
      {
         column.source =
            R_GetRawColumn(skyTexture,
               (((an + xtoviewangle[x])) >> (ANGLETOSKYSHIFT))+offset);
            
         colfunc();
      }
   }
   colfunc = r_column_engine->DrawColumn;
}

// Log base 2 LUT
static const int MultiplyDeBruijnBitPosition2[32] = 
{
  0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 
  31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};

//
// do_draw_plane
//
// New function, by Lee Killough
// haleyjd 08/30/02: slight restructuring to use hashed sky texture info cache.
//
static void do_draw_plane(visplane_t *pl)
{
   int x;

   if(!(pl->minx <= pl->maxx))
      return;

   // haleyjd: hexen-style skies
   if(R_IsSkyFlat(pl->picnum) && LevelInfo.doubleSky)
   {
      do_draw_newsky(pl);
      return;
   }
   
   skyflat_t *skyflat = R_SkyFlatForPicnum(pl->picnum);
   
   if(skyflat || pl->picnum & PL_SKYFLAT)  // sky flat
   {
      int texture;
      int offset = 0;
      angle_t an, flip;
      skytexture_t *sky;
      
      // killough 10/98: allow skies to come from sidedefs.
      // Allows scrolling and/or animated skies, as well as
      // arbitrary multiple skies per level without having
      // to use info lumps.

      an = viewangle;
      
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
         
         an += s->textureoffset;
         
         // Vertical offset allows careful sky positioning.        
         
         column.texmid = s->rowoffset - 28*FRACUNIT;
         
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
      else 	 // Normal Doom sky, only one allowed per level
      {
         texture       = skyflat->texture;            // Default texture
         sky           = R_GetSkyTexture(texture);    // haleyjd 08/30/02
         column.texmid = sky->texturemid;             // Default y-offset
         flip          = 0;                           // Doom flips it
         offset        = skyflat->columnoffset >> 16; // Hexen-style scrolling
      }

      // Sky is always drawn full bright, i.e. colormaps[0] is used.
      // Because of this hack, sky is not affected by INVUL inverse mapping.
      //
      // killough 7/19/98: fix hack to be more realistic:
      // haleyjd 10/31/10: use plane colormaps, not global vars!
      if(comp[comp_skymap] || !(column.colormap = pl->fixedcolormap))
         column.colormap = pl->fullcolormap;

      //dc_texheight = (textureheight[texture])>>FRACBITS; // killough
      // haleyjd: use height determined from patches in texture
      column.texheight = sky->height;
      
      // haleyjd:  don't stretch textures over 200 tall
      // 10/07/06: don't stretch skies in old demos (no mlook)
      if(demo_version >= 300 && column.texheight < 200 && stretchsky)
         column.step = M_FloatToFixed(view.pspriteystep * 0.5f);
      else
         column.step = M_FloatToFixed(view.pspriteystep);

      // killough 10/98: Use sky scrolling offset, and possibly flip picture
      for(x = pl->minx; x <= pl->maxx; x++)
      {
         column.x = x;

         column.y1 = pl->top[x];
         column.y2 = pl->bottom[x];

         if(column.y1 <= column.y2)
         {
            column.source = R_GetRawColumn(texture,
               (((an + xtoviewangle[x])^flip) >> ANGLETOSKYSHIFT) + offset);
            
            colfunc();
         }
      }
   }
   else // regular flat
   {  
      texture_t *tex;
      int        stop, light;
      int        stylenum;

      int picnum = texturetranslation[pl->picnum];

      // haleyjd 05/19/06: rewritten to avoid crashes
      // ioanch: apply swirly if original (pl->picnum) has the flag. This is so
      // Hexen animations can control only their own sequence swirling.
      if((r_swirl && textures[picnum]->flags & TF_ANIMATED)
         || textures[pl->picnum]->flags & TF_SWIRLY)
      {
         plane.source = R_DistortedFlat(picnum);
         tex = plane.tex = textures[picnum];
      }
      else
      {
         // SoM: Handled outside
         tex = plane.tex = R_CacheTexture(picnum);
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
         span.fg2rgb = span.bg2rgb = NULL;

      if(pl->pslope)
         plane.slope = &pl->rslope;
      else
         plane.slope = NULL;
         
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

      plane.xscale = pl->xscale;
      plane.yscale = pl->yscale;

      plane.pviewx   = pl->viewxf;
      plane.pviewy   = pl->viewyf;
      plane.pviewz   = pl->viewzf;
      plane.pviewsin = pl->viewsin; // haleyjd 01/05/08: Add angle
      plane.pviewcos = pl->viewcos;
      plane.height   = pl->heightf - pl->viewzf;
      
      // SoM 10/19/02: deep water colormap fix
      if(fixedcolormap)
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

      R_PlaneLight();

      plane.MapFunc = (plane.slope == NULL ? R_MapPlane : R_MapSlope);

      for(x = pl->minx ; x <= stop ; x++)
         R_MakeSpans(x, pl->top[x-1], pl->bottom[x-1], pl->top[x], pl->bottom[x]);
   }
}

//
// R_DrawPlanes
//
// Called after the BSP has been traversed and walls have rendered. This 
// function is also now used to render portal overlays.
//
void R_DrawPlanes(planehash_t *table)
{
   visplane_t *pl;
   int i;
   
   if(!table)
      table = &mainhash;
   
   for(i = 0; i < table->chaincount; ++i)
   {
      for(pl = table->chains[i]; pl; pl = pl->next)
         do_draw_plane(pl);
   }
}

VALLOCATION(overlaySets)
{
   for(planehash_t *set = r_overlayfreesets; set; set = set->next)
      memset(set->chains, 0, set->chaincount * sizeof(*set->chains));
}

//
// Gets a free portal overlay plane set
//
planehash_t *R_NewOverlaySet()
{
   planehash_t *set;
   if(!r_overlayfreesets)
   {
      set = R_NewPlaneHash(31);
      return set;
   }
   set = r_overlayfreesets;
   r_overlayfreesets = r_overlayfreesets->next;
   R_ClearPlaneHash(set);
   return set;
}

//
// Puts the overlay set in the free list
//
void R_FreeOverlaySet(planehash_t *set)
{
   set->next = r_overlayfreesets;
   r_overlayfreesets = set;
}

//
// Called from P_SetupLevel, considering that overlay sets are PU_LEVEL
//
void R_MapInitOverlaySets()
{
   r_overlayfreesets = nullptr;
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
