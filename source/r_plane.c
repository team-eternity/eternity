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

#include "z_zone.h"  /* memory allocation wrappers -- killough */

#include "doomstat.h"

#include "c_io.h"
#include "d_gi.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_draw.h"
#include "r_things.h"
#include "r_sky.h"
#include "r_ripple.h"
#include "r_plane.h"
#include "v_video.h"

#include "p_info.h"
#include "p_anim.h"
#include "p_user.h"
#include "p_slopes.h"

#define MAXVISPLANES 128    /* must be a power of 2 */

static visplane_t *visplanes[MAXVISPLANES];   // killough
static visplane_t *freetail;                  // killough
static visplane_t **freehead = &freetail;     // killough
visplane_t *floorplane, *ceilingplane;

int num_visplanes;      // sf: count visplanes

// killough -- hash function for visplanes
// Empirically verified to be fairly uniform:

#define visplane_hash(picnum,lightlevel,height) \
  (((unsigned int)(picnum)*3+(unsigned int)(lightlevel)+(unsigned int)(height)*7) & (MAXVISPLANES-1))

// killough 8/1/98: set static number of openings to be large enough
// (a static limit is okay in this case and avoids difficulties in r_segs.c)
#define MAXOPENINGS (MAX_SCREENWIDTH*MAX_SCREENHEIGHT)

float openings[MAXOPENINGS], *lastopening;

// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1

// SoM 12/8/03: floorclip and ceilingclip changed to pointers so they can be set
// to the clipping arrays of portals.
float floorcliparray[MAX_SCREENWIDTH], ceilingcliparray[MAX_SCREENWIDTH];
float *floorclip = floorcliparray, *ceilingclip = ceilingcliparray;

// spanstart holds the start of a plane span; initialized to 0 at start

static int spanstart[MAX_SCREENHEIGHT];                // killough 2/8/98

//
// texture mapping
//

// killough 2/8/98: make variables static

static fixed_t cachedheight[MAX_SCREENHEIGHT];

fixed_t *yslope;
fixed_t origyslope[MAX_SCREENHEIGHT*2];

fixed_t distscale[MAX_SCREENWIDTH];
int     visplane_view=0;


cb_span_t  span;
cb_plane_t plane;
cb_slopespan_t slopespan;

float slopevis; // SoM: used in slope lighting

// BIG FLATS
void R_Throw(void)
{
   I_Error("R_Throw called.\n");
}

void (*flatfunc)(void) = R_Throw;
void (*slopefunc)(void) = R_Throw;



static struct flatdims_s
{
   int i;
   float f;
} flatdims[FLAT_NUMSIZES] = {
   // FLAT_64
   { 64,  64.0f},

   // FLAT_128
   {128, 128.0f},

   // FLAT_256
   {256, 256.0f},

   // FLAT_512
   {512, 512.0f}
};


//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes(void)
{
}

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
static void R_PlaneLight(void)
{
   // This formula was taken (almost) directly from r_main.c where the zlight
   // table is generated.
   plane.startmap = 2.0f * (30.0f - (plane.lightlevel / 8.0f));
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
   if(x2 < x1 || x1 < 0 || x2 >= viewwidth || y < 0 || y >= viewheight)
      I_Error("R_MapPlane: %i, %i at %i", x1, x2, y);
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

   xstep = plane.pviewcos * slope * view.focratio;
   ystep = plane.pviewsin * slope * view.focratio;

#ifdef __APPLE__
   {
      double value;
      
      value = fmod(plane.pviewx + plane.xoffset + (plane.pviewsin * realy)
                   + (x1 - view.xcenter) * xstep, (double)plane.tex->width); 
      if(value < 0) value += plane.tex->width;
      span.xfrac = (int)(value * plane.fixedunitx);
      span.xfrac <<= 2;

      value = fmod(-plane.pviewy + plane.yoffset + (-plane.pviewcos * realy)
                   + (x1 - view.xcenter) * ystep, (double)plane.tex->height);
      if(value < 0) value += plane.tex->height;
      span.yfrac = (int)(value * plane.fixedunity);
      span.yfrac <<= 2;
      
      value = fmod(xstep, (double)plane.tex->width);
      if(value < 0) value += plane.tex->width;
      span.xstep = (int)(value * plane.fixedunitx);
      span.xstep <<= 2;
            
      value = fmod(ystep, (double)plane.tex->height);
      if(value < 0) value += plane.tex->height;
      span.ystep = (int)(value * plane.fixedunity);
      span.ystep <<= 2;
   }
#else
   span.xfrac = 
      (unsigned int)((plane.pviewx + plane.xoffset + (plane.pviewsin * realy)
                      + ((x1 - view.xcenter) * xstep)) * plane.fixedunitx);
   span.yfrac = 
      (unsigned int)((-plane.pviewy + plane.yoffset + (-plane.pviewcos * realy)
                      + ((x1 - view.xcenter) * ystep)) * plane.fixedunity);
   span.xstep = (unsigned int)(xstep * plane.fixedunitx);
   span.ystep = (unsigned int)(ystep * plane.fixedunity);
#endif

   // killough 2/28/98: Add offsets
   if((span.colormap = plane.fixedcolormap) == NULL) // haleyjd 10/16/06
      span.colormap = plane.colormap + R_SpanLight(realy) * 256;
   
   span.y  = y;
   span.x1 = x1;
   span.x2 = x2;
   span.source = plane.source;
   
   // BIG FLATS
   flatfunc();

   // visplane viewing -- sf
   if(visplane_view)
   {
      if(y >= 0 && y < viewheight)
      {
         // SoM: ANYRES
         if(x1 >= 0 && x1 <= viewwidth)
            *(vbscreen.data + y*vbscreen.pitch + x1) = GameModeInfo->blackIndex;
         if(x2 >= 0 && x2 <= viewwidth)
            *(vbscreen.data + y*vbscreen.pitch + x2) = GameModeInfo->blackIndex;
      }
   }
}

// haleyjd: NOTE: This version below has scaling implemented. Don't delete it!
// TODO: integrate flat scaling.
/*
static void R_MapPlane(int y, int x1, int x2)
{
   float dy, xstep, ystep, realy, slope;

   static float scale = 2.0f

#ifdef RANGECHECK
   if(x2 < x1 || x1 < 0 || x2 >= viewwidth || y < 0 || y >= viewheight)
      I_Error("R_MapPlane: %i, %i at %i", x1, x2, y);
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
   realy = slope * view.yfoc * scale;

   xstep = view.sin * slope * view.focratio * scale;
   ystep = view.cos * slope * view.focratio * scale;

   span.xfrac = (unsigned int)((((-plane.pviewy + plane.yoffset) * scale) + (-view.cos * realy) 
                            + ((x1 - view.xcenter) * xstep)) * plane.fixedunit);
   span.yfrac = (unsigned int)((((plane.pviewx + plane.xoffset) * scale) + (view.sin * realy) 
                            + ((x1 - view.xcenter) * ystep)) * plane.fixedunit);
   span.xstep = (unsigned int)(xstep * plane.fixedunit);
   span.ystep = (unsigned int)(ystep * plane.fixedunit);

   // killough 2/28/98: Add offsets
   if((span.colormap = plane.fixedcolormap) == NULL) // haleyjd 10/16/06
   {
      int index = (int)(realy / 16.0f);
      if(index >= MAXLIGHTZ )
         index = MAXLIGHTZ-1;
      span.colormap = plane.planezlight[index];
   }
   
   span.y  = y;
   span.x1 = x1;
   span.x2 = x2;
   span.source = plane.source;
   
   // BIG FLATS
   flatfunc();

   // visplane viewing -- sf
   if(visplane_view)
   {
      if(y >= 0 && y < viewheight)
      {
         // SoM: ANYRES
         if(x1 >= 0 && x1 <= viewwidth)
            *(video.screens[0] + y*video.width + x1) = gameModeInfo->blackIndex;
         if(x2 >= 0 && x2 <= viewwidth)
            *(video.screens[0] + y*video.width + x2) = gameModeInfo->blackIndex;
      }
   }
}
*/

//
// R_SlopeLights
//
static void R_SlopeLights(int len, float startmap, float endmap)
{
   int i;
   fixed_t map, map2, step;

   if(plane.fixedcolormap)
   {
      for(i = 0; i < len; i++)
         slopespan.colormap[i] = plane.fixedcolormap;
      return;
   }

   map = M_FloatToFixed((startmap / 256.0f * NUMCOLORMAPS));
   map2 = M_FloatToFixed((endmap / 256.0f * NUMCOLORMAPS));

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
   v3float_t s;
   float map1, map2;
   float base;

   s.x = x1 - view.xcenter;
   s.y = y - view.ycenter + 1;
   s.z = view.xfoc;

   slopespan.iufrac = M_DotVec3f(&s, &slope->A) * (float)plane.tex->width;
   slopespan.ivfrac = M_DotVec3f(&s, &slope->B) * (float)plane.tex->height;
   slopespan.idfrac = M_DotVec3f(&s, &slope->C);

   slopespan.iustep = slope->A.x * (float)plane.tex->width;
   slopespan.ivstep = slope->B.x * (float)plane.tex->height;
   slopespan.idstep = slope->C.x;

   slopespan.source = plane.source;
   slopespan.x1 = x1;
   slopespan.x2 = x2;
   slopespan.y = y;

   // Setup lighting
   base = 4.0f * (plane.lightlevel) - 448.0f;

   map1 = 256.0f - (slope->shade - slope->plight * slopespan.idfrac);
   if(count > 0)
   {
      float id = slopespan.idfrac + slopespan.idstep * (x2 - x1);
      map2 = 256.0f - (slope->shade - slope->plight * id);
   }
   else
      map2 = map1;

   R_SlopeLights(x2 - x1 + 1, (256.0f - map1), (256.0f - map2));
 
   slopefunc();
}

#define CompFloats(x, y) (fabs(x - y) < 0.001f)

//
// R_CompareSlopes
// 
// SoM: Returns true if the texture spaces of the give slope structs are the
// same.
//
boolean R_CompareSlopes(const pslope_t *s1, const pslope_t *s2)
{
   if(!s1 != !s2)
      return false;
   
   if(s1 == s2)
      return true;
   
   if(!CompFloats(s1->normalf.x, s2->normalf.x) ||
      !CompFloats(s1->normalf.y, s2->normalf.y) ||
      !CompFloats(s1->normalf.z, s2->normalf.z) ||
      fabs(P_DistFromPlanef(&s2->of, &s1->of, &s1->normalf)) >= 0.001f)
      return false;

   return true;
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
   float          x, y, tsin, tcos;
   float          ixscale, iyscale;
   rslope_t       *rslope = &pl->rslope;
   texture_t      *tex = textures[pl->picnum];

   if(!pl->pslope)
      return;

   
   tsin = (float)sin(pl->angle);
   tcos = (float)cos(pl->angle);

#if 1
   // SoM: To change the origin of rotation, add an offset to P.x and P.z
   rslope->P.x = 0;
   rslope->P.z = 0;
   rslope->P.y = P_GetZAtf(pl->pslope, 0, 0);

   rslope->M.x = rslope->P.x - tex->width * tsin;
   rslope->M.z = rslope->P.z + tex->width * tcos;
   rslope->M.y = P_GetZAtf(pl->pslope, rslope->M.x, rslope->M.z);

   rslope->N.x = rslope->P.x + tex->height * tcos;
   rslope->N.z = rslope->P.z + tex->height * tsin;
   rslope->N.y = P_GetZAtf(pl->pslope, rslope->N.x, rslope->N.z);
#else
   // TODO: rotation/offsets
   rslope->P.x = x * tcos + y * tsin;
   rslope->P.z = y * tcos - x * tsin;
   rslope->P.y = P_GetZAtf(pl->pslope, rslope->P.x, rslope->P.z);

   rslope->M.x = rslope->P.x - tex->width * tsin;
   rslope->M.z = rslope->P.z + tex->width * tcos;
   rslope->M.y = P_GetZAtf(pl->pslope, rslope->M.x, rslope->M.z);

   rslope->N.x = rslope->P.x + tex->height * tcos;
   rslope->N.z = rslope->P.z + tex->height * tsin;
   rslope->N.y = P_GetZAtf(pl->pslope, rslope->N.x, rslope->N.z);
#endif

   M_TranslateVec3f(&rslope->P);
   M_TranslateVec3f(&rslope->M);
   M_TranslateVec3f(&rslope->N);

   M_SubVec3f(&rslope->M, &rslope->M, &rslope->P);
   M_SubVec3f(&rslope->N, &rslope->N, &rslope->P);
   
   M_CrossProduct3f(&rslope->A, &rslope->P, &rslope->N);
   M_CrossProduct3f(&rslope->B, &rslope->P, &rslope->M);
   M_CrossProduct3f(&rslope->C, &rslope->M, &rslope->N);

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
   ixscale = iyscale = view.tan / (float)tex->width;

   rslope->plight = (slopevis * ixscale * iyscale) / (rslope->zat - pl->viewzf);
   rslope->shade = 256.0f * 2.0f - (pl->lightlevel + 16.0f) * 256.0f / 128.0f;
}

//
// R_ClearPlanes
//
// At begining of frame.
//
void R_ClearPlanes(void)
{
   int i;
   float a;
   int scaled_height = consoleactive ? video.x1lookup[current_height] : 0;

   a = (float)(consoleactive ? 
         (scaled_height-viewwindowy) < 0 ? 0 : scaled_height-viewwindowy : 0);

   floorclip = floorcliparray;
   ceilingclip = ceilingcliparray;

   // opening / clipping determination
   for(i = 0; i < MAX_SCREENWIDTH; ++i)
   {
      floorclip[i] = view.height - 1.0f;
      ceilingclip[i] = a;
   }

   for(i = 0; i < MAXVISPLANES; ++i)    // new code -- killough
      for(*freehead = visplanes[i], visplanes[i] = NULL; *freehead; )
         freehead = &(*freehead)->next;

   lastopening = openings;

   // texture calculation
   memset(cachedheight, 0, sizeof(cachedheight));
   
   num_visplanes = 0;    // reset
}


//
// new_visplane
//
// New function, by Lee Killough
//
static visplane_t *new_visplane(unsigned hash)
{
   visplane_t *check = freetail;

   if(!check)
      check = calloc(1, sizeof *check);
   else 
      if(!(freetail = freetail->next))
         freehead = &freetail;
   
   check->next = visplanes[hash];
   visplanes[hash] = check;

   if(check->max_width < (unsigned int)video.width)
   {
      if(check->pad1)
         free(check->pad1);
      if(check->pad3)
         free(check->pad3);

      check->max_width = video.width;
      check->pad1 = calloc(1, (video.width + 2) * sizeof(int));
      check->top = check->pad1 + 1;
      check->pad2 = check->pad1 + video.width + 1;

      check->pad3 = calloc(1, (video.width + 2) * sizeof(int));
      check->bottom = check->pad3 + 1;
      check->pad4 = check->pad3 + video.width + 1;
   }
   
   num_visplanes++;      // keep track of how many for counter
   
   return check;
}

//
// R_FindPlane
//
// killough 2/28/98: Add offsets
// haleyjd 01/05/08: Add angle
//
visplane_t *R_FindPlane(fixed_t height, int picnum, int lightlevel,
                        fixed_t xoffs, fixed_t yoffs, float angle,
                        pslope_t *slope)
{
   visplane_t *check;
   unsigned int hash;                      // killough
   float tsin, tcos;

   // SoM: TEST
   angle = 2*3.14159265f * (gametic % 512) / 512.0f;
      
   // killough 10/98: PL_SKYFLAT
   if(picnum == skyflatnum || picnum == sky2flatnum || picnum & PL_SKYFLAT)
   {
      lightlevel = 0;   // killough 7/19/98: most skies map together

      // haleyjd 05/06/08: but not all. If height > viewz, set height to
      // 1 instead of 0, to keep ceilings mapping with ceilings, and floors
      // mapping with floors.
      if(height > viewz)
         height = 1;
   }

   // New visplane algorithm uses hash table -- killough
   hash = visplane_hash(picnum, lightlevel, height >> 16);

   for(check = visplanes[hash]; check; check = check->next)  // killough
   {
      // FIXME: ok this has gotten out of hand now!
      // We need a more intelligent hash function or something.
      if(height == check->height &&
         picnum == check->picnum &&
         lightlevel == check->lightlevel &&
         xoffs == check->xoffs &&      // killough 2/28/98: Add offset checks
         yoffs == check->yoffs &&
         angle == check->angle &&      // haleyjd 01/05/08: Add angle
         zlight == check->colormap &&
         fixedcolormap == check->fixedcolormap && 
         viewx == check->viewx && 
         viewy == check->viewy && 
         viewz == check->viewz &&
         R_CompareSlopes(check->pslope, slope)
        )
        return check;
   }

   check = new_visplane(hash);         // killough

   check->height = height;
   check->picnum = picnum;
   check->lightlevel = lightlevel;
   check->minx = viewwidth;            // Was SCREENWIDTH -- killough 11/98
   check->maxx = -1;
   check->xoffs = xoffs;               // killough 2/28/98: Save offsets
   check->yoffs = yoffs;
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
   
   // SoM: memset should use the check->max_width
   //memset(check->top, 0xff, sizeof(unsigned int) * check->max_width);
   {
      register unsigned int i = 0;
      register int *p = check->top;
      while(i < check->max_width) p[i++] = 0x7FFFFFFF;
   }
   
   return check;
}

//
// R_CheckPlane
//
visplane_t *R_CheckPlane(visplane_t *pl, int start, int stop)
{
   int intrl, intrh, unionl, unionh, x;
   
   if(start < pl->minx)
      intrl   = pl->minx, unionl = start;
   else
      unionl  = pl->minx,  intrl = start;
   
   if(stop  > pl->maxx)
      intrh   = pl->maxx, unionh = stop;
   else
      unionh  = pl->maxx, intrh  = stop;

   for(x = intrl; x <= intrh && pl->top[x] == 0x7FFFFFFF; ++x)
      ;

   if(x > intrh)
      pl->minx = unionl, pl->maxx = unionh;
   else
   {
      unsigned hash = visplane_hash(pl->picnum, pl->lightlevel, pl->height);
      visplane_t *new_pl = new_visplane(hash);
      
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
      new_pl->xoffsf = pl->xoffsf;
      new_pl->yoffsf = pl->yoffsf;

      new_pl->pslope = pl->pslope;
      memcpy(&new_pl->rslope, &pl->rslope, sizeof(rslope_t));
      new_pl->fullcolormap = pl->fullcolormap;

      pl = new_pl;
      pl->minx = start;
      pl->maxx = stop;
      {
         register int *p = pl->top;
         register unsigned i = 0;
         while(i < pl->max_width) p[i++] = 0x7FFFFFFF;
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
   if(b2 >= MAX_SCREENHEIGHT)
      I_Error("R_MakeSpans: b2 >= MAX_SCREENHEIGHT\n");
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

extern void R_DrawNewSkyColumn(void);

// haleyjd: moved here from r_newsky.c
void do_draw_newsky(visplane_t *pl)
{
   int x, offset, skyTexture, offset2, skyTexture2;
   angle_t an;
   skytexture_t *sky1, *sky2;
   
   an = viewangle;
   
   if(LevelInfo.doubleSky) // render two layers
   {
      // get scrolling offsets and textures
      offset = Sky1ColumnOffset>>16;
      skyTexture = texturetranslation[skytexture];
      offset2 = Sky2ColumnOffset>>16;
      skyTexture2 = texturetranslation[sky2texture];

      sky1 = R_GetSkyTexture(skyTexture);
      sky2 = R_GetSkyTexture(skyTexture2);
      
      if(comp[comp_skymap] || !(column.colormap = fixedcolormap))
         column.colormap = fullcolormap;
      
      // first draw sky 2 with R_DrawColumn (unmasked)
      column.texmid = sky2->texturemid;      
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
   }
   else // one layer only -- use pl->picnum
   {
      if(pl->picnum == skyflatnum)
      {
         offset = Sky1ColumnOffset>>16;
         skyTexture = texturetranslation[skytexture];
      }
      else
      {
         offset = Sky2ColumnOffset>>16;
         skyTexture = texturetranslation[sky2texture];
      }

      sky1 = R_GetSkyTexture(skyTexture);
      
      column.texmid = sky1->texturemid;    // Default y-offset
      
      if(comp[comp_skymap] || !(column.colormap = fixedcolormap))
         column.colormap = fullcolormap;
      
      column.texheight = sky1->height;

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
               R_GetRawColumn(skyTexture,
               (((an + xtoviewangle[x])) >> (ANGLETOSKYSHIFT))+offset);

            colfunc();
         }
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
// do_draw_plane
//
// New function, by Lee Killough
// haleyjd 08/30/02: slight restructuring to use hashed sky texture info cache.
//
static void do_draw_plane(visplane_t *pl)
{
   register int x;

   if(!(pl->minx <= pl->maxx))
      return;

   // haleyjd: hexen-style skies:
   // * Always for sky2
   // * Use for sky1 IF double skies or sky delta set
   if(pl->picnum == sky2flatnum ||
      (pl->picnum == skyflatnum && 
       (LevelInfo.doubleSky || LevelInfo.skyDelta)))
   {
      do_draw_newsky(pl);
      return;
   }

   if(pl->picnum == skyflatnum || pl->picnum & PL_SKYFLAT)  // sky flat
   {
      int texture;
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
         
         flip = l->special == 272 ? 0u : ~0u;
      }
      else 	 // Normal Doom sky, only one allowed per level
      {
         texture = skytexture;             // Default texture
         sky = R_GetSkyTexture(texture);   // haleyjd 08/30/02         
         column.texmid = sky->texturemid;  // Default y-offset
         flip = 0;                         // Doom flips it
      }

      // Sky is always drawn full bright, i.e. colormaps[0] is used.
      // Because of this hack, sky is not affected by INVUL inverse mapping.
      //
      // killough 7/19/98: fix hack to be more realistic:

      if(comp[comp_skymap] || !(column.colormap = fixedcolormap))
         column.colormap = fullcolormap;          // killough 3/20/98

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
      for(x = pl->minx; x <= pl->maxx; ++x)
      {
         column.x = x;

         column.y1 = pl->top[x];
         column.y2 = pl->bottom[x];

         if(column.y1 <= column.y2)
         {
            column.source = R_GetRawColumn(texture,
               ((an + xtoviewangle[x])^flip) >> (ANGLETOSKYSHIFT));
            
            colfunc();
         }
      }
   }
   else      // regular flat
   {  
      texture_t *tex;
      int stop, light;

      int picnum = texturetranslation[pl->picnum];

      // haleyjd 05/19/06: rewritten to avoid crashes
      if(textures[pl->picnum]->flags & TF_SWIRLY 
         && textures[pl->picnum]->flatsize == FLAT_64)
      {
         plane.source = R_DistortedFlat(pl->picnum);
         tex = plane.tex = textures[pl->picnum];
      }
      else
      {
         // SoM: Handled outside
         tex = plane.tex = R_CacheTexture(picnum);
         plane.source = tex->buffer;
      }

      // haleyjd: TODO: feed pl->drawstyle to the first dimension to enable
      // span drawstyles (ie. translucency)

      flatfunc        = r_span_engine->DrawSpan[0][tex->flatsize];
      slopefunc       = r_span_engine->DrawSlope[0][tex->flatsize];

      if(pl->pslope)
         plane.slope = &pl->rslope;
      else
         plane.slope = NULL;
         
      {
         int rw, rh;
         
         rh = MultiplyDeBruijnBitPosition2[(uint32_t)(tex->height * 0x077CB531U) >> 27];
         rw = MultiplyDeBruijnBitPosition2[(uint32_t)(tex->width * 0x077CB531U) >> 27];

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
            
#ifdef __APPLE__
            plane.fixedunitx = (float)(1 << (30 - rw));
            plane.fixedunity = (float)(1 << (30 - rh));
#else
            plane.fixedunitx = (float)(1 << (32 - rw));
            plane.fixedunity = (float)(1 << span.yshift);
#endif

         }
      }
       
        
      plane.xoffset = pl->xoffsf;  // killough 2/28/98: Add offsets
      plane.yoffset = pl->yoffsf;

      plane.pviewx   = pl->viewxf;
      plane.pviewy   = pl->viewyf;
      plane.pviewz   = pl->viewzf;
      plane.pviewsin = pl->viewsin; // haleyjd 01/05/08: Add angle
      plane.pviewcos = pl->viewcos;
      plane.height   = pl->heightf - pl->viewzf;
      
      //light = (pl->lightlevel >> LIGHTSEGSHIFT) + (extralight * LIGHTBRIGHT);

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

      plane.planezlight = pl->colormap[light]; //zlight[light];
      plane.colormap = pl->fullcolormap;
      // haleyjd 10/16/06
      plane.fixedcolormap = pl->fixedcolormap;

      plane.lightlevel = pl->lightlevel;

      R_PlaneLight();

      plane.MapFunc = plane.slope == NULL ? R_MapPlane : R_MapSlope;

      for(x = pl->minx ; x <= stop ; x++)
         R_MakeSpans(x, pl->top[x-1], pl->bottom[x-1], pl->top[x], pl->bottom[x]);
   }
}

//
// R_DrawPlanes
//
// Called at the end of each frame.
//
void R_DrawPlanes(void)
{
   visplane_t *pl;
   int i;
   
   for(i = 0; i < MAXVISPLANES; ++i)
   {
      for(pl = visplanes[i]; pl; pl = pl->next)
         do_draw_plane(pl);
   }
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
