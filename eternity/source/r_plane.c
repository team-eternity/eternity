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

#define MAXVISPLANES 128    /* must be a power of 2 */

static visplane_t *visplanes[MAXVISPLANES];   // killough
static visplane_t *freetail;                  // killough
static visplane_t **freehead = &freetail;     // killough
visplane_t *floorplane, *ceilingplane;

int num_visplanes;      // sf: count visplanes

// killough -- hash function for visplanes
// Empirically verified to be fairly uniform:

#define visplane_hash(picnum,lightlevel,height) \
  (((unsigned)(picnum)*3+(unsigned)(lightlevel)+(unsigned)(height)*7) & (MAXVISPLANES-1))

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

// BIG FLATS
void R_Throw(void)
{
   I_Error("R_Throw called.\n");
}

void (*flatfunc)(void) = R_Throw;

//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes(void)
{
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

   xstep = plane.pviewsin * slope * view.focratio;
   ystep = plane.pviewcos * slope * view.focratio;

   span.xfrac = (unsigned)((-plane.pviewy + plane.yoffset + (-plane.pviewcos * realy) 
                            + ((x1 - view.xcenter) * xstep)) * plane.fixedunit);
   span.yfrac = (unsigned)((plane.pviewx + plane.xoffset + (plane.pviewsin * realy) 
                            + ((x1 - view.xcenter) * ystep)) * plane.fixedunit);
   span.xstep = (unsigned)(xstep * plane.fixedunit);
   span.ystep = (unsigned)(ystep * plane.fixedunit);

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
            *(video.screens[0] + y*video.width + x1) = GameModeInfo->blackIndex;
         if(x2 >= 0 && x2 <= viewwidth)
            *(video.screens[0] + y*video.width + x2) = GameModeInfo->blackIndex;
      }
   }
}


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

   span.xfrac = (unsigned)((((-plane.pviewy + plane.yoffset) * scale) + (-view.cos * realy) 
                            + ((x1 - view.xcenter) * xstep)) * plane.fixedunit);
   span.yfrac = (unsigned)((((plane.pviewx + plane.xoffset) * scale) + (view.sin * realy) 
                            + ((x1 - view.xcenter) * ystep)) * plane.fixedunit);
   span.xstep = (unsigned)(xstep * plane.fixedunit);
   span.ystep = (unsigned)(ystep * plane.fixedunit);

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


/*
//
// R_MapPlane
//
// BASIC PRIMITIVE
//
static void R_MapPlane(int y, int x1, int x2)
{
   float dy, xstep, ystep, realy, slope;

   float tviewx, tviewy, tviewcos, tviewsin;
   float t_a;

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

   tviewx = plane.pviewx;
   tviewy = plane.pviewy;
   tviewsin   = view.sin;
   tviewcos   = view.cos;

#define PROT (45*PI/180.0f)

   // signs of sine terms must be reversed to flip y axis
   plane.pviewx = (float)( tviewx * cos(PROT) + tviewy * sin(PROT));
   plane.pviewy = (float)(-tviewx * sin(PROT) + tviewy * cos(PROT));

   t_a = ((ANG90 - viewangle) + ANG45) * PI / ANG180;
   view.cos = (float)cos(t_a);
   view.sin = (float)sin(t_a);

   xstep = view.sin * slope * view.focratio;
   ystep = view.cos * slope * view.focratio;

   span.xfrac = (unsigned)((-plane.pviewy + plane.yoffset + (-view.cos * realy) 
                            + ((x1 - view.xcenter + 0.2) * xstep)) * plane.fixedunit);
   span.yfrac = (unsigned)((plane.pviewx + plane.xoffset + (view.sin * realy) 
                            + ((x1 - view.xcenter + 0.2) * ystep)) * plane.fixedunit);
   span.xstep = (unsigned)(xstep * plane.fixedunit);
   span.ystep = (unsigned)(ystep * plane.fixedunit);

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

   plane.pviewx = tviewx;
   plane.pviewy = tviewy;
   view.sin = tviewsin;
   view.cos = tviewcos;
}
*/

//
// R_ClearPlanes
// At begining of frame.
//
// SoM: uses double floating point for calculation of the global scales because
// it is much more accurate. This is done only once per frame so this really has
// no effect on speed.
//
void R_ClearPlanes(void)
{
   int i;
   float a;

   a = (float)(consoleactive ? 
         (current_height-viewwindowy) < 0 ? 0 : current_height-viewwindowy : 0);

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
   memset (cachedheight, 0, sizeof(cachedheight));
   
   num_visplanes = 0;    // reset
}

// New function, by Lee Killough

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

   if(check->max_width < (unsigned)video.width)
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
                        fixed_t xoffs, fixed_t yoffs, float angle)
{
   visplane_t *check;
   unsigned int hash;                      // killough
   float tsin, tcos;
   
   if(picnum == skyflatnum || picnum == sky2flatnum || picnum & PL_SKYFLAT)  // killough 10/98
      lightlevel = height = 0;   // killough 7/19/98: most skies map together

   // New visplane algorithm uses hash table -- killough
   hash = visplane_hash(picnum, lightlevel, height);

   for(check = visplanes[hash]; check; check = check->next)  // killough
   {
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
         viewz == check->viewz
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

   check->viewx = viewx;
   check->viewy = viewy;
   check->viewz = viewz;
   
   // haleyjd 01/05/08: rotate viewpoint by flat angle.
   // note that the signs of the sine terms must be reversed in order to flip
   // the y-axis of the flat relative to world coordinates

   tsin = (float) sin(check->angle);
   tcos = (float) cos(check->angle);

   check->viewxf = (float)( view.x * tcos + view.y * tsin);
   check->viewyf = (float)(-view.x * tsin + view.y * tcos);
   check->viewzf = view.z;

   // haleyjd 01/05/08: modify viewing angle with respect to flat angle
   check->viewsin = (float) sin(view.angle + check->angle);
   check->viewcos = (float) cos(view.angle + check->angle);

   check->heightf = (float)height / 65536.0f;
   check->xoffsf  = (float)xoffs / 65536.0f;
   check->yoffsf  = (float)yoffs / 65536.0f;
   
   // SoM: memset should use the check->max_width
   //memset(check->top, 0xff, sizeof(unsigned int) * check->max_width);
   {
      register unsigned i = 0;
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
      R_MapPlane(t1, spanstart[t1], x - 1);
   for(; b2 < b1 && t1 <= b1; b1--)
      R_MapPlane(b1, spanstart[b1], x - 1);
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
         column.step = (int)(view.pspriteystep * 0.5f * 65536.0f);
      else
         column.step = (int)(view.pspriteystep * 65536.0f);
      
      for(x = pl->minx; (column.x = x) <= pl->maxx; x++)
      {
         if((column.y1 = pl->top[x]) <= (column.y2 = pl->bottom[x]))
         {
            column.source =
               R_GetColumn(skyTexture2,
               (((an + xtoviewangle[x])) >> (ANGLETOSKYSHIFT))+offset2);
            
            colfunc();
         }
      }
      
      // now draw sky 1 with R_DrawNewSkyColumn (masked)
      column.texmid = sky1->texturemid;
      column.texheight = sky1->height;

      // haleyjd: don't stretch textures over 200 tall
      if(demo_version >= 300 && column.texheight < 200 && stretchsky)
         column.step = (int)(view.pspriteystep * 0.5f * 65536.0f);
      else
         column.step = (int)(view.pspriteystep * 65536.0f);
      
      for(x = pl->minx; (column.x = x) <= pl->maxx; x++)
      {
         if((column.y1 = pl->top[x]) <= (column.y2 = pl->bottom[x]))
         {
            column.source =
               R_GetColumn(skyTexture,
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
         column.step = (int)(view.pspriteystep * 0.5f * 65536.0f);
      else
         column.step = (int)(view.pspriteystep * 65536.0f);
      
      for(x = pl->minx; (column.x = x) <= pl->maxx; x++)
      {
         if((column.y1 = pl->top[x]) <= (column.y2 = pl->bottom[x]))
         {
            column.source =
               R_GetColumn(skyTexture,
               (((an + xtoviewangle[x])) >> (ANGLETOSKYSHIFT))+offset);

            colfunc();
         }
      }
   }
}

// New function, by Lee Killough
// haleyjd 08/30/02: slight restructuring to use hashed
// sky texture info cache

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
         
         flip = l->special==272 ? 0u : ~0u;
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
         column.step = (int)(view.pspriteystep * 0.5f * 65536.0f);
      else
         column.step = (int)(view.pspriteystep * 65536.0f);


      // killough 10/98: Use sky scrolling offset, and possibly flip picture
      for(x = pl->minx; (column.x = x) <= pl->maxx; ++x)
      {
         if((column.y1 = pl->top[x]) <= (column.y2 = pl->bottom[x]))
         {
            column.source = R_GetColumn(texture,
               ((an + xtoviewangle[x])^flip) >> (ANGLETOSKYSHIFT));
            
            colfunc();
         }
      }
   }
   else      // regular flat
   {
      int stop, light;
      int swirling;

      // haleyjd 05/19/06: rewritten to avoid crashes
      swirling = (flattranslation[pl->picnum] == -1) && flatsize[pl->picnum] == 4096;

      if(swirling)
         plane.source = R_DistortedFlat(pl->picnum);
      else
      {
         if(flattranslation[pl->picnum] == -1)
            flattranslation[pl->picnum] = pl->picnum;

         // haleyjd 09/16/06: this was being allocated at PU_STATIC and changed
         // to PU_CACHE below, generating a lot of unnecessary allocator noise.
         // As long as no other memory ops are needed between here and the end
         // of this function (including called functions), this can be PU_CACHE.
         plane.source = 
            W_CacheLumpNum(firstflat + flattranslation[pl->picnum], PU_CACHE);
      }

      // SoM: support for flats of different sizes!!
      switch(flatsize[flattranslation[pl->picnum]])
      {
      case 16384:
         flatfunc = r_span_engine->DrawSpan128;
         plane.fixedunit = r_span_engine->fixedunit128;
         break;
      case 65536:
         flatfunc = r_span_engine->DrawSpan256;
         plane.fixedunit = r_span_engine->fixedunit256;
         break;
      case 262144:
         flatfunc = r_span_engine->DrawSpan512;
         plane.fixedunit = r_span_engine->fixedunit512;
         break;
      default:
         flatfunc = r_span_engine->DrawSpan64;
         plane.fixedunit = r_span_engine->fixedunit64;
      };
        
      plane.xoffset = pl->xoffsf;  // killough 2/28/98: Add offsets
      plane.yoffset = pl->yoffsf;

      plane.pviewx   = pl->viewxf;
      plane.pviewy   = pl->viewyf;
      plane.pviewz   = pl->viewzf;
      plane.pviewsin = pl->viewsin; // haleyjd 01/05/08: Add angle
      plane.pviewcos = pl->viewcos;
      plane.height   = pl->heightf - pl->viewzf;
      
      //light = (pl->lightlevel >> LIGHTSEGSHIFT) + extralight;

      // SoM 10/19/02: deep water colormap fix
      if(fixedcolormap)
         light = (255  >> LIGHTSEGSHIFT);
      else
         light = (pl->lightlevel >> LIGHTSEGSHIFT) + extralight;

      if(light >= LIGHTLEVELS)
         light = LIGHTLEVELS-1;

      if(light < 0)
         light = 0;

      stop = pl->maxx + 1;
      pl->top[pl->minx-1] = pl->top[stop] = 0x7FFFFFFF;

      plane.planezlight = pl->colormap[light];//zlight[light];
      // haleyjd 10/16/06
      plane.fixedcolormap = pl->fixedcolormap;

      for(x = pl->minx ; x <= stop ; x++)
         R_MakeSpans(x,pl->top[x-1],pl->bottom[x-1],pl->top[x],pl->bottom[x]);
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
