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
//      Rendering main loop and setup functions,
//       utility functions (BSP, geometry, trigonometry).
//      See tables.c, too.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "am_map.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_deh.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_net.h"
#include "doomstat.h"
#include "e_things.h"
#include "g_game.h"
#include "hal/i_platform.h"
#include "hal/i_timer.h"
#include "hu_over.h"
#include "i_video.h"
#include "m_bbox.h"
#include "m_random.h"
#include "mn_engin.h"
#include "p_chase.h"
#include "p_info.h"
#include "p_partcl.h"
#include "p_portal.h"
#include "p_scroll.h"
#include "p_xenemy.h"
#include "r_bsp.h"
#include "r_context.h"
#include "r_draw.h"
#include "r_dynabsp.h"
#include "r_dynseg.h"
#include "r_interpolate.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_portal.h"
#include "r_ripple.h"
#include "r_things.h"
#include "r_segs.h"
#include "r_sky.h"
#include "r_state.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_alloc.h"
#include "v_block.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

// SoM: Cardboard
const float PI = 3.14159265f;
cb_view_t view;

// haleyjd 04/03/05: focal lengths made global, y len added
fixed_t focallen_x;
fixed_t focallen_y;

// killough: viewangleoffset is a legacy from the pre-v1.2 days, when Doom
// had Left/Mid/Right viewing. +/-ANG90 offsets were placed here on each
// node, by d_net.c, to set up a L/M/R session.

int viewdir;    // 0 = forward, 1 = left, 2 = right
int viewangleoffset;
int validcount = 1;         // increment every time a check is made
int      centerx, centery;
fixed_t  centerxfrac, centeryfrac;
fixed_t  viewpitch;
const player_t *viewplayer;
bool     showpsprites = 1; //sf
bool     centerfire = false;
camera_t *viewcamera;

// SoM: removed the old zoom code infavor of actual field of view!
int fov = 90;

// SoM: Displays red on the screen if a portal becomes "tainted"
int showtainted = 0;

extern int screenSize;

extern int global_cmap_index; // haleyjd: NGCS

void R_HOMdrawer();

//
// precalculated math tables
//

angle_t clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X. 

int viewangletox[FINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.

angle_t *xtoviewangle;   // killough 2/8/98
VALLOCATION(xtoviewangle)
{
   xtoviewangle = ecalloctag(angle_t *, w+1, sizeof(angle_t), PU_VALLOC, nullptr);
}


// killough 3/20/98: Support dynamic colormaps, e.g. deep water
// killough 4/4/98: support dynamic number of them as well

int numcolormaps;
lighttable_t *(*c_scalelight)[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t *(*c_zlight)[LIGHTLEVELS][MAXLIGHTZ];
lighttable_t **colormaps;

// killough 3/20/98, 4/4/98: end dynamic colormaps

int extralight;                           // bumped light from gun blasts

// haleyjd 09/04/06: column drawing engines
columndrawer_t *r_column_engine;
int r_column_engine_num;

static columndrawer_t *r_column_engines[NUMCOLUMNENGINES] =
{
   &r_normal_drawer, // normal engine
   // Here lies Quad Cache Engine: 2006/09/04 - 2020/10/31
};

//
// R_SetColumnEngine
//
// Sets r_column_engine to the appropriate set of column drawers.
//
void R_SetColumnEngine()
{
   r_column_engine = r_column_engines[r_column_engine_num];
}

// haleyjd 09/10/06: span drawing engines
spandrawer_t *r_span_engine;
int r_span_engine_num;

static spandrawer_t *r_span_engines[NUMSPANENGINES] =
{
   &r_spandrawer,    // normal engine
};

//
// R_SetSpanEngine
//
// Sets r_span_engine to the appropriate set of span drawers.
//
void R_SetSpanEngine(void)
{
   r_span_engine = r_span_engines[r_span_engine_num];
}

//
// R_PointOnSide
//
// Traverse BSP (sub) tree, check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// killough 5/2/98: reformatted
//

// ioanch 20160423: make variables volatile in OSX, to prevent demo desyncing.
// FIXME: also check if Linux/GCC are affected by this.
// MORE INFO: competn/doom/fp2-3655.lmp E2M3 fails here
#if EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX && defined(__clang__)
int R_PointOnSideClassic(volatile fixed_t x, volatile fixed_t y, const node_t *node)
#else
int R_PointOnSideClassic(fixed_t x, fixed_t y, const node_t *node)
#endif
{
   if(!node->dx)
      return x <= node->x ? node->dy > 0 : node->dy < 0;
   
   if(!node->dy)
      return y <= node->y ? node->dx < 0 : node->dx > 0;
   
   x -= node->x;
   y -= node->y;
  
   // Try to quickly decide by looking at sign bits.
   if((node->dy ^ node->dx ^ x ^ y) < 0)
      return (node->dy ^ x) < 0;  // (left is negative)
   return FixedMul(y, node->dx>>FRACBITS) >= FixedMul(node->dy>>FRACBITS, x);
}

//
// Variant when nodes have non-integer coordinates
// FIXME: do I need volatile here? How can I even test it?
//
int R_PointOnSidePrecise(fixed_t x, fixed_t y, const node_t *node)
{
   if(!node->dx)
      return x <= node->x ? node->dy > 0 : node->dy < 0;

   if(!node->dy)
      return y <= node->y ? node->dx < 0 : node->dx > 0;

   x -= node->x;
   y -= node->y;

   // Try to quickly decide by looking at sign bits.
   if((node->dy ^ node->dx ^ x ^ y) < 0)
      return (node->dy ^ x) < 0;  // (left is negative)
   return (int64_t)y * node->dx >= (int64_t)node->dy * x;
}

int (*R_PointOnSide)(fixed_t, fixed_t, const node_t *) = R_PointOnSideClassic;

// killough 5/2/98: reformatted

int R_PointOnSegSide(fixed_t x, fixed_t y, const seg_t *line)
{
   fixed_t lx = line->v1->x;
   fixed_t ly = line->v1->y;
   fixed_t ldx = line->v2->x - lx;
   fixed_t ldy = line->v2->y - ly;

   if(!ldx)
      return x <= lx ? ldy > 0 : ldy < 0;

   if(!ldy)
      return y <= ly ? ldx < 0 : ldx > 0;

   x -= lx;
   y -= ly;

   // Try to quickly decide by looking at sign bits.
   if((ldy ^ ldx ^ x ^ y) < 0)
      return (ldy ^ x) < 0;          // (left is negative)
   return (int64_t)y * ldx >= (int64_t)ldy * x;
}

//
// SlopeDiv
//
// Utility routine for R_PointToAngle
//
int SlopeDiv(unsigned int num, unsigned int den)
{
   unsigned int ans;

   if(den < 512)
      return SLOPERANGE;
   
   ans = (num << 3) / (den >> 8);
   
   return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

#define R_P2ATHRESHOLD (INT_MAX / 4)

//
// R_PointToAngle
//
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table. The +1 size of tantoangle[]
//  is to handle the case when x==y without additional
//  checking.
//
// killough 5/2/98: reformatted, cleaned up
// haleyjd 01/28/10: restored to Vanilla and made some modifications
//
angle_t R_PointToAngle2(fixed_t pviewx, fixed_t pviewy, fixed_t x, fixed_t y)
{	
   x -= pviewx;
   y -= pviewy;

   if((x | y) == 0)
      return 0;

   if(x < R_P2ATHRESHOLD && x > -R_P2ATHRESHOLD && 
      y < R_P2ATHRESHOLD && y > -R_P2ATHRESHOLD)
   {
      if(x >= 0)
      {
         if (y >= 0)
         {
            if(x > y)
            {
               // octant 0
               return tantoangle_acc[SlopeDiv(y, x)];
            }
            else
            {
               // octant 1
               return ANG90 - 1 - tantoangle_acc[SlopeDiv(x, y)];
            }
         }
         else // y < 0
         {
            y = -y;

            if(x > y)
            {
               // octant 8
               return 0 - tantoangle_acc[SlopeDiv(y, x)];
            }
            else
            {
               // octant 7
               return ANG270 + tantoangle_acc[SlopeDiv(x, y)];
            }
         }
      }
      else // x < 0
      {
         x = -x;

         if(y >= 0)
         {
            if(x > y)
            {
               // octant 3
               return ANG180 - 1 - tantoangle_acc[SlopeDiv(y, x)];
            }
            else
            {
               // octant 2
               return ANG90 + tantoangle_acc[SlopeDiv(x, y)];
            }
         }
         else // y < 0
         {
            y = -y;

            if(x > y)
            {
               // octant 4
               return ANG180 + tantoangle_acc[SlopeDiv(y, x)];
            }
            else
            {
               // octant 5
               return ANG270 - 1 - tantoangle_acc[SlopeDiv(x, y)];
            }
         }
      }
   }
   else
   {
      // Sneakernets: Fix cast issue on ARM
      return angle_t(int(atan2(double(y), x) * (ANG180 / PI)));
   }

   return 0;
}

angle_t R_PointToAngle(const fixed_t viewx, const fixed_t viewy, const fixed_t x, const fixed_t y)
{
   return R_PointToAngle2(viewx, viewy, x, y);
}

//
// SoM: Called by I_InitGraphicsMode when the video mode is changed.
// Sets the base-line fov for the given screen ratio.
//
// SoM: This is used by the sprite code
//
void R_ResetFOV(int width, int height)
{
   const double ratio = static_cast<double>(width) / static_cast<double>(height);

   // Special case for tallscreen modes
   if((width == 320 && height == 200) ||
      (width == 640 && height == 400))
   {
      fov = 90;
      return;
   }

   // The general equation is as follows:
   // y = mx + b -> fov = (75/2) * ratio + 40
   // This gives 90 for 4:3, 100 for 16:10, and 106 for 16:9.
   fov = static_cast<int>((75.0/2.0) * ratio + 40.0);

   if(fov < 20)
      fov = 20;
   else if(fov > 179)
      fov = 179;
}

extern float slopevis; // SoM: used in slope lighting

//
// R_InitTextureMapping
//
// killough 5/2/98: reformatted
//
static void R_InitTextureMapping()
{
   int i, x, limit;
   float vtan, ratio, slopet;
   
   // Use tangent table to generate viewangletox:
   //  viewangletox will give the next greatest x
   //  after the view angle.
   //
   // Calc focal length so fov angles cover SCREENWIDTH

   // Cardboard
   if(video.height == 200 || video.height == 400)
      ratio = 1.0f;
   else 
      ratio = 1.2f;

   view.fov = (float)fov * PI / 180.0f;
   view.tan = vtan = (float)tan(view.fov / 2);
   view.xfoc = view.xcenter / vtan;
   view.yfoc = view.xfoc * ratio;
   view.focratio = view.yfoc / view.xfoc;

   // Unfortunately, cardboard still has to co-exist with the old fixed point
   // code
   focallen_x = M_FloatToFixed(view.xfoc);
   focallen_y = M_FloatToFixed(view.yfoc);

   // SoM: Thanks to 'Randi' of Zdoom fame!
   slopet = (float)tan((90.0f + (float)fov / 2.0f) * PI / 180.0f);
   slopevis = 8.0f * slopet * 16.0f * 320.0f / (float)view.width;

   // SoM: rewrote old LUT generation code to work with variable FOV
   i = 0;
   limit = -M_FloatToFixed(view.xcenter / view.xfoc);
   while(i < FINEANGLES/2 && finetangent[i] < limit)
      viewangletox[i++] = viewwindow.width + 1;

   limit = -limit;
   while(i < FINEANGLES/2 && finetangent[i] <= limit)
   {
      int t;

      t = FixedMul(finetangent[i], focallen_x);
      t = (centerxfrac - t + FRACUNIT-1) >> FRACBITS;
      if(t < -1)
         viewangletox[i++] = -1;
      else if(t > viewwindow.width+1)
         viewangletox[i++] = viewwindow.width+1;
      else
         viewangletox[i++] = t;
   }

   while(i < FINEANGLES/2)
      viewangletox[i++] = -1;
    
   // Scan viewangletox[] to generate xtoviewangle[]:
   //  xtoviewangle will give the smallest view angle
   //  that maps to x.
   
   for(x = 0; x <= viewwindow.width; ++x)
   {
      for(i = 0; viewangletox[i] > x; ++i)
         ;
      xtoviewangle[x] = (i << ANGLETOFINESHIFT) - ANG90;
   }
    
   // Take out the fencepost cases from viewangletox.
   for(i = 0; i < FINEANGLES/2; ++i)
      if(viewangletox[i] == -1)
         viewangletox[i] = 0;
      else 
         if(viewangletox[i] == viewwindow.width+1)
            viewangletox[i] = viewwindow.width;
        
   clipangle = xtoviewangle[0];
}

#define DISTMAP 2

//
// R_InitLightTables
// Only inits the zlight table,
//  because the scalelight table changes with view size.
//
void R_InitLightTables (void)
{
   int i;
   
   // killough 4/4/98: dynamic colormaps
   // haleyjd: FIXME - wtf kind of types ARE these anyway??
   c_zlight     = emalloc(lighttable_t *(*)[LIGHTLEVELS][MAXLIGHTZ],     sizeof(*c_zlight) * numcolormaps);
   c_scalelight = emalloc(lighttable_t *(*)[LIGHTLEVELS][MAXLIGHTSCALE], sizeof(*c_scalelight) * numcolormaps);
   
   // Calculate the light levels to use
   //  for each level / distance combination.
   for(i = 0; i < LIGHTLEVELS; ++i)
   {
      // SoM: the LIGHTBRIGHT constant must be used to scale the start offset of 
      // the colormaps, otherwise the levels are staggered and become slightly 
      // darker.
      int j, startcmap = ((LIGHTLEVELS-LIGHTBRIGHT-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
      for(j = 0; j < MAXLIGHTZ; ++j)
      {
         int scale = FixedDiv((SCREENWIDTH/2*FRACUNIT), (j+1)<<LIGHTZSHIFT);
         int t, level = startcmap - (scale >> LIGHTSCALESHIFT)/DISTMAP;
         
         if(level < 0)
            level = 0;
         else if(level >= NUMCOLORMAPS)
            level = NUMCOLORMAPS-1;

         // killough 3/20/98: Initialize multiple colormaps
         level *= 256;
         for(t = 0; t < numcolormaps; ++t)         // killough 4/4/98
            c_zlight[t][i][j] = colormaps[t] + level;
      }
   }
}

bool setsizeneeded;
int  setblocks;

//
// R_SetViewSize
//
// Do not really change anything here,
//  because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
void R_SetViewSize(int blocks)
{
   setsizeneeded = true;
   setblocks = blocks;
}

//
// R_SetupViewScaling
//
void R_SetupViewScaling()
{
   int i;
   fixed_t frac = 0, lastfrac;
   // SoM: ANYRES
   // Moved stuff, reformatted a bit
   // haleyjd 04/03/05: removed unnecessary FixedDiv calls

   video.xscale  = (video.width << FRACBITS) / SCREENWIDTH;
   video.xstep   = ((SCREENWIDTH << FRACBITS) / video.width) + 1;
   video.yscale  = (video.height << FRACBITS) / SCREENHEIGHT;
   video.ystep   = ((SCREENHEIGHT << FRACBITS) / video.height) + 1;

   video.xscalef = (float)video.width / SCREENWIDTH;
   video.xstepf  = SCREENWIDTH / (float)video.width;
   video.yscalef = (float)video.height / SCREENHEIGHT;
   video.ystepf  = SCREENHEIGHT / (float)video.height;

   video.scaled = (video.xscalef > 1.0f || video.yscalef > 1.0f);

   // SoM: ok, assemble the realx1/x2 arrays differently. To start, we are using
   // floats to do the scaling which is 100 times more accurate, secondly, I 
   // realized that the reason the old single arrays were causing problems was 
   // they were only calculating the top-left corner of the scaled pixels. 
   // Calculating widths through these arrays is wrong because the scaling will
   // change the final scaled widths depending on what their unscaled screen 
   // coords were. Thusly, all rectangles should be converted to unscaled 
   // x1, y1, x2, y2 coords, scaled, and then converted back to x, y, w, h
   video.x1lookup[0] = 0;
   lastfrac = frac = 0;
   for(i = 0; i < video.width; i++)
   {
      if(frac >> FRACBITS > lastfrac >> FRACBITS)
      {
         video.x1lookup[frac >> FRACBITS] = i;
         video.x2lookup[lastfrac >> FRACBITS] = i-1;
         lastfrac = frac;
      }

      frac += video.xstep;
   }
   video.x2lookup[319] = video.width - 1;
   video.x1lookup[320] = video.x2lookup[320] = video.width;


   video.y1lookup[0] = 0;
   lastfrac = frac = 0;
   for(i = 0; i < video.height; i++)
   {
      if(frac >> FRACBITS > lastfrac >> FRACBITS)
      {
         video.y1lookup[frac >> FRACBITS] = i;
         video.y2lookup[lastfrac >> FRACBITS] = i-1;
         lastfrac = frac;
      }

      frac += video.ystep;
   }
   video.y2lookup[199] = video.height - 1;
   video.y1lookup[200] = video.y2lookup[200] = video.height;

   // haleyjd 05/02/13: set scaledwindow properties
   scaledwindow.scaledFromScreenBlocks(setblocks);

   // haleyjd 05/02/13: set viewwindow properties
   viewwindow.viewFromScaled(setblocks, video.width, video.height, scaledwindow);

   centerx     = viewwindow.width  / 2;
   centery     = viewwindow.height / 2;
   centerxfrac = centerx << FRACBITS;
   centeryfrac = centery << FRACBITS;

   // SoM: Cardboard
   view.xcenter = (view.width  = (float)viewwindow.width ) * 0.5f;
   view.ycenter = (view.height = (float)viewwindow.height) * 0.5f;

   R_InitBuffer(scaledwindow.width, scaledwindow.height);       // killough 11/98
   R_UpdateContextBounds();
}

//
// R_calculateVisSpriteScales
//
// haleyjd 12/09/13: FOV-independent vissprite scaling, which works in any
// video mode, including WSVGA and 17:9 modes that previously had floating
// weapons.
//
static void R_calculateVisSpriteScales()
{
   float realxscale = video.xscalef * GameModeInfo->pspriteGlobalScale->x;
   float realyscale = video.yscalef * GameModeInfo->pspriteGlobalScale->y;

   // correct aspect ratio, if necessary
   if(!((video.width == 320 && video.height == 200) ||
        (video.width == 640 && video.height == 400)))
      realxscale = ((float)video.height * 4 / 3) / SCREENWIDTH;

   // determine subwindow scaling for smaller screen sizes
   float swxscale = 1.0f;
   float swyscale = 1.0f;
   if(setblocks < 10)
   {
      float sbheight = GameModeInfo->StatusBar->height * video.yscalef;
      swxscale = (float)viewwindow.width / video.width;
      swyscale = (float)viewwindow.height / (video.height - sbheight);
   }
   
   view.pspritexscale = realxscale * swxscale;
   view.pspriteyscale = realyscale * swyscale;
   view.pspriteystep  = 1.0f / view.pspriteyscale;
}

//
// R_ExecuteSetViewSize
//
void R_ExecuteSetViewSize()
{
   int i;

   setsizeneeded = false;
   
   R_SetupViewScaling();
   
   R_InitTextureMapping();
    
   // thing clipping
   for(i = 0; i < viewwindow.width; i++)
      screenheightarray[i] = view.height - 1.0f;

   // Calculate the light levels to use for each level / scale combination.
   for(i = 0; i < LIGHTLEVELS; i++)
   {
      int startcmap = ((LIGHTLEVELS-LIGHTBRIGHT-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
      for(int j = 0; j < MAXLIGHTSCALE; ++j)
      {                                       // killough 11/98:
         int level = startcmap - j*1/DISTMAP;
         
         if(level < 0)
            level = 0;
         
         if(level >= NUMCOLORMAPS)
            level = NUMCOLORMAPS-1;
         
         // killough 3/20/98: initialize multiple colormaps
         level *= 256;
         
         for(int t = 0; t < numcolormaps; t++)     // killough 4/4/98
            c_scalelight[t][i][j] = colormaps[t] + level;
      }
   }
   
   R_calculateVisSpriteScales();
}

//
// R_Init
//
void R_Init()
{
   R_InitData();
   R_SetViewSize(screenSize+3);
   R_InitLightTables();
   R_InitTranslationTables();
   R_InitParticles(); // haleyjd
}

//
// R_PointInSubsector
//
// killough 5/2/98: reformatted, cleaned up
// haleyjd 12/7/13: restored compatibility for levels with 0 nodes.
//
subsector_t *R_PointInSubsector(fixed_t x, fixed_t y)
{
   int nodenum = numnodes - 1;
   while(!(nodenum & NF_SUBSECTOR))
      nodenum = nodes[nodenum].children[R_PointOnSide(x, y, nodes+nodenum)];
   return &subsectors[(nodenum == -1 ? 0 : nodenum & ~NF_SUBSECTOR)];
}

int autodetect_hom = 0;       // killough 2/7/98: HOM autodetection flag

unsigned int frameid = 0;

//
// SoM: frameid is an unsigned integer that represents the number of the frame 
// currently being rendered for use in caching data used by the renderer which 
// is unique to each frame. frameid is incremented every frame. When the number
// becomes too great and the value wraps back around to 0, the structures should
// be searched and all frameids reset to prevent mishaps in the rendering 
// process.
//
static void R_incrementFrameid()
{
   frameid++;

   if(!frameid)
   {
      // it wrapped!
      C_Printf("Congratulations! You have just played through 4,294,967,295 "
               "rendered frames of Doom. At a constant rate of 35 frames per "
               "second you have spent at least 1,420 days playing DOOM.\n"
               "GET A LIFE.\a\n");

      frameid = 1;

      // Do as the description says...
      R_ForEachContext([](rendercontext_t &context) {
         sectorboxvisit_t *visitids = context.portalcontext.visitids;
         memset(visitids, 0, sizeof(*visitids) * numsectors);
      });
   }
}

//
// Stuffs the renderdepth, the ID of a context, and frame ID into a uint64_t
//
uint64_t R_GetVisitID(const rendercontext_t &context)
{
   const uint64_t upper32 =
      (uint64_t(context.portalcontext.worldwindowid) << 16) | uint64_t(uint16_t(context.bufferindex));
   return (upper32 << 32) | uint64_t(frameid);
}

//
// R_interpolateViewPoint
//
// Interpolate a rendering view point based on the player's location.
//
static void R_interpolateViewPoint(player_t *player, fixed_t lerp)
{
   viewpoint_t &viewpoint = r_globalcontext.view;

   if(lerp == FRACUNIT)
   {
      viewpoint.x     = player->mo->x;
      viewpoint.y     = player->mo->y;
      viewpoint.z     = player->viewz;
      viewpoint.angle = player->mo->angle; //+ viewangleoffset;
      viewpitch       = player->pitch;
   }
   else
   {
      viewpoint.z = lerpCoord(lerp, player->prevviewz, player->viewz);
      Mobj *thing = player->mo;

      if(const linkdata_t * psec = thing->prevpos.ldata)
      {
         v2fixed_t orgtarg =
         {
            thing->x - psec->delta.x,
            thing->y - psec->delta.y
         };
         viewpoint.x = lerpCoord(lerp, thing->prevpos.x, orgtarg.x);
         viewpoint.y = lerpCoord(lerp, thing->prevpos.y, orgtarg.y);

         bool execute = false;
         const line_t *pline = thing->prevpos.portalline;
         if(pline && P_PointOnLineSidePrecise(viewpoint.x, viewpoint.y, pline))
            execute = true;
         if(!execute && !pline)
         {
            const surface_t *psurface = thing->prevpos.portalsurface;
            if(psurface)
            {
               fixed_t planez = P_PortalZ(*psurface);
               execute = FixedMul(viewpoint.z - planez, player->prevviewz - planez) < 0;
            }
         }

         if(execute)
         {
            // Once it crosses it, we're done
            thing->prevpos.portalline = nullptr;
            thing->prevpos.ldata = nullptr;
            thing->prevpos.portalsurface = nullptr;
            thing->prevpos.x += psec->delta.x;
            thing->prevpos.y += psec->delta.y;
            viewpoint.x += psec->delta.x;
            viewpoint.y += psec->delta.y;
         }
      }
      else
      {
         viewpoint.x = lerpCoord(lerp, thing->prevpos.x, thing->x);
         viewpoint.y = lerpCoord(lerp, thing->prevpos.y, thing->y);
      }
      viewpoint.angle = lerpAngle(lerp, thing->prevpos.angle, thing->angle);
      viewpitch = lerpAngle(lerp, player->prevpitch,         player->pitch);
   }
}

//
// R_interpolateViewPoint
//
// Interpolate a rendering view point based on the camera's location.
//
static void R_interpolateViewPoint(camera_t *camera, fixed_t lerp)
{
   viewpoint_t &viewpoint = r_globalcontext.view;

   if(lerp == FRACUNIT)
   {
      viewpoint.x     = camera->x;
      viewpoint.y     = camera->y;
      viewpoint.z     = camera->z;
      viewpoint.angle = camera->angle;
      viewpitch       = camera->pitch;
   }
   else
   {
      viewpoint.x     = lerpCoord(lerp, camera->prevpos.x,     camera->x);
      viewpoint.y     = lerpCoord(lerp, camera->prevpos.y,     camera->y);
      viewpoint.z     = lerpCoord(lerp, camera->prevpos.z,     camera->z);
      viewpoint.angle = lerpAngle(lerp, camera->prevpos.angle, camera->angle);
      viewpitch       = lerpAngle(lerp, camera->prevpitch, camera->pitch);
   }
}

enum secinterpstate_e
{
   SEC_INTERPOLATE,
   SEC_NORMAL
};

//
// R_setSectorInterpolationState
//
// If passed SEC_INTERPOLATE, current floor and ceiling heights are backed up
// and then replaced with interpolated values. If passed SEC_NORMAL, backed up
// sector heights are restored.
//
static void R_setSectorInterpolationState(secinterpstate_e state)
{
   int i;

   switch(state)
   {
   case SEC_INTERPOLATE:
      for(i = 0; i < numsectors; i++)
      {
         auto &si  = sectorinterps[i];
         auto &sec = sectors[i];

         if(si.prevfloorheight   != sec.srf.floor.height ||
            si.prevceilingheight != sec.srf.ceiling.height)
         {
            si.interpolated = true;

            // backup heights
            si.backfloorheight    = sec.srf.floor.height;
            si.backfloorheightf   = sec.srf.floor.heightf;
            si.backceilingheight  = sec.srf.ceiling.height;
            si.backceilingheightf = sec.srf.ceiling.heightf;

            // set interpolated heights
            sec.srf.floor.height = lerpCoord(view.lerp, si.prevfloorheight,   sec.srf.floor.height);
            sec.srf.ceiling.height = lerpCoord(view.lerp, si.prevceilingheight, sec.srf.ceiling.height);
            sec.srf.floor.heightf = M_FixedToFloat(sec.srf.floor.height);
            sec.srf.ceiling.heightf = M_FixedToFloat(sec.srf.ceiling.height);

            // as above, but only for slope origin Z
            if(pslope_t *slope = sec.srf.floor.slope; slope && si.prevfloorslopezf != slope->of.z)
            {
               si.backfloorslopezf = slope->of.z;
               slope->of.z = lerpCoordf(view.lerp, si.prevfloorslopezf, slope->of.z);
            }
            if(pslope_t *slope = sec.srf.ceiling.slope; slope && si.prevceilingslopezf != slope->of.z)
            {
               si.backceilingslopezf = slope->of.z;
               slope->of.z = lerpCoordf(view.lerp, si.prevceilingslopezf, slope->of.z);
            }
         }
         else
            si.interpolated = false;
      }
      break;
   case SEC_NORMAL:
      for(i = 0; i < numsectors; i++)
      {
         auto &si  = sectorinterps[i];
         auto &sec = sectors[i];

         // restore backed up heights
         if(si.interpolated)
         {
            sec.srf.floor.height    = si.backfloorheight;
            sec.srf.floor.heightf   = si.backfloorheightf;
            sec.srf.ceiling.height  = si.backceilingheight;
            sec.srf.ceiling.heightf = si.backceilingheightf;

            if(pslope_t *slope = sec.srf.floor.slope; slope)
               slope->of.z = si.backfloorslopezf;
            if(pslope_t *slope = sec.srf.ceiling.slope; slope)
               slope->of.z = si.backceilingslopezf;
         }
      }
      break;
   }
}

//
// Interpolates sidedef scrolling
//
static void R_setScrollInterpolationState(secinterpstate_e state)
{
   switch(state)
   {
      case SEC_INTERPOLATE:
         P_ForEachScrolledSide([](side_t *side, v2fixed_t offset) {
            side->textureoffset += lerpCoord(view.lerp, -offset.x, 0);
            side->rowoffset += lerpCoord(view.lerp, -offset.y, 0);
         });
         P_ForEachScrolledSector([](sector_t *sector, bool isceiling, v2fixed_t offset) {
            if(isceiling)
            {
               sector->srf.ceiling.offset.x += lerpCoord(view.lerp, -offset.x, 0);
               sector->srf.ceiling.offset.y += lerpCoord(view.lerp, -offset.y, 0);
            }
            else
            {
               sector->srf.floor.offset.x += lerpCoord(view.lerp, -offset.x, 0);
               sector->srf.floor.offset.y += lerpCoord(view.lerp, -offset.y, 0);
            }
         });
         break;
      case SEC_NORMAL:
         P_ForEachScrolledSide([](side_t *side, v2fixed_t offset) {
            side->textureoffset -= lerpCoord(view.lerp, -offset.x, 0);
            side->rowoffset -= lerpCoord(view.lerp, -offset.y, 0);
         });
         P_ForEachScrolledSector([](sector_t *sector, bool isceiling, v2fixed_t offset) {
            if(isceiling)
            {
               sector->srf.ceiling.offset.x -= lerpCoord(view.lerp, -offset.x, 0);
               sector->srf.ceiling.offset.y -= lerpCoord(view.lerp, -offset.y, 0);
            }
            else
            {
               sector->srf.floor.offset.x -= lerpCoord(view.lerp, -offset.x, 0);
               sector->srf.floor.offset.y -= lerpCoord(view.lerp, -offset.y, 0);
            }
         });
         break;
   }
}

//
// Interpolate a rendering view point based on the player's location.
//
static void R_interpolateVertex(dynavertex_t &v, v2fixed_t &org, v2float_t &forg)
{
   org.x  = v.x;
   org.y  = v.y;
   forg.x = v.fx;
   forg.y = v.fy;
   v.x    = lerpCoord(view.lerp, v.backup.x, v.x);
   v.y    = lerpCoord(view.lerp, v.backup.y, v.y);
   v.fx   = M_FixedToFloat(v.x);
   v.fy   = M_FixedToFloat(v.y);
}

//
// Interpolates polyobject dynasegs
//
static void R_setDynaSegInterpolationState(secinterpstate_e state)
{
   switch(state)
   {
      case SEC_INTERPOLATE:
         R_ForEachPolyNode([](rpolynode_t *node) {
            dynaseg_t &dynaseg = *node->partition;
            seg_t     *seg     = &node->partition->seg;

            R_interpolateVertex(*seg->dyv1, dynaseg.prev.org[0], dynaseg.prev.forg[0]);
            R_interpolateVertex(*seg->dyv2, dynaseg.prev.org[1], dynaseg.prev.forg[1]);

            dynaseg.prev.len    = seg->len;
            dynaseg.prev.offset = seg->offset;
            seg->len            = lerpCoordf(view.lerp, dynaseg.prevlen, seg->len);
            seg->offset         = lerpCoordf(view.lerp, dynaseg.prevofs, seg->offset);
         });
         break;
      case SEC_NORMAL:
         R_ForEachPolyNode([](rpolynode_t *node) {
            seginterp_t &prev = node->partition->prev;
            seg_t       *seg  = &node->partition->seg;

            seg->offset = prev.offset;
            seg->len    = prev.len;
            seg->v1->x  = prev.org[0].x;
            seg->v1->y  = prev.org[0].y;
            seg->v2->x  = prev.org[1].x;
            seg->v2->y  = prev.org[1].y;
            seg->v1->fx = prev.forg[0].x;
            seg->v1->fy = prev.forg[0].y;
            seg->v2->fx = prev.forg[1].x;
            seg->v2->fy = prev.forg[1].y;
         });
         break;
   }
}

//
// R_GetLerp
//
fixed_t R_GetLerp(bool ignorepause)
{
   // Interpolation must be disabled during pauses to avoid shaking, unless arg is set
   if(d_fastrefresh && d_interpolate &&
      (ignorepause || (!paused && ((!menuactive && !consoleactive) || demoplayback || netgame))))
      return i_haltimer.GetFrac();
   else
      return FRACUNIT;
}

//
// R_SetupFrame
//
static void R_SetupFrame(player_t *player, camera_t *camera)
{
   viewpoint_t   &viewpoint    = r_globalcontext.view;
   cbviewpoint_t &cb_viewpoint = r_globalcontext.cb_view;

   fixed_t  viewheightfrac;
   fixed_t  lerp = R_GetLerp(false);
   
   // haleyjd 09/04/06: set or change column drawing engine
   // haleyjd 09/10/06: set or change span drawing engine
   R_SetColumnEngine();
   R_SetSpanEngine();
   R_incrementFrameid(); // Cardboard

   
   viewplayer = player;
   viewcamera = camera;

   if(!camera)
   {
      R_interpolateViewPoint(player, lerp);

      // haleyjd 01/21/07: earthquakes
      if(player->quake &&
         !(((menuactive || consoleactive) && !demoplayback && !netgame) || paused))
      {
         int strength = player->quake;

         viewpoint.x += (M_Random() % (strength * 4) - (strength * 2)) << FRACBITS;
         viewpoint.y += (M_Random() % (strength * 4) - (strength * 2)) << FRACBITS;
      }
   }
   else
   {
      R_interpolateViewPoint(camera, walkcam_active ? R_GetLerp(true) : lerp);
   }

   // Bound the pitch here
   int maxpitchup = GameModeInfo->lookPitchUp;
   int maxpitchdown = GameModeInfo->lookPitchDown;
   if(viewpitch < -ANGLE_1 * maxpitchup)
      viewpitch = -ANGLE_1 * maxpitchup;
   else if(viewpitch > ANGLE_1 * maxpitchdown)
      viewpitch = ANGLE_1 * maxpitchdown;

   extralight = player->extralight;
   viewpoint.sin = finesine[viewpoint.angle>>ANGLETOFINESHIFT];
   viewpoint.cos = finecosine[viewpoint.angle>>ANGLETOFINESHIFT];

   // SoM: Cardboard
   cb_viewpoint.x      = M_FixedToFloat(viewpoint.x);
   cb_viewpoint.y      = M_FixedToFloat(viewpoint.y);
   cb_viewpoint.z      = M_FixedToFloat(viewpoint.z);
   cb_viewpoint.angle  = cb_fixedAngleToFloat(viewpoint.angle);
   cb_viewpoint.sin    = sinf(cb_viewpoint.angle);
   cb_viewpoint.cos    = cosf(cb_viewpoint.angle);
   view.pitch  = (ANG90 - viewpitch) * PI / ANG180;
   view.lerp   = lerp;
   view.sector = R_PointInSubsector(viewpoint.x, viewpoint.y)->sector;

   R_SetupSolidSegs();
   R_PreRenderBSP();

   // set interpolated sector heights
   if(view.lerp != FRACUNIT)
   {
      R_setSectorInterpolationState(SEC_INTERPOLATE);
      R_setScrollInterpolationState(SEC_INTERPOLATE);
      R_setDynaSegInterpolationState(SEC_INTERPOLATE);
   }

   // y shearing
   // haleyjd 04/03/05: perform calculation for true pitch angle

   // make fixed-point viewheight and divide by 2
   viewheightfrac = viewwindow.height << (FRACBITS - 1);

   // haleyjd 10/08/06: use simpler calculation for pitch == 0 to avoid 
   // unnecessary roundoff error. This is what was causing sky textures to
   // appear a half-pixel too low (the entire display was too low actually).
   if(viewpitch)
   {
      const fixed_t dy = FixedMul(
         focallen_y,
         finetangent[(ANG90 - viewpitch) >> ANGLETOFINESHIFT]
      );

      centeryfrac = viewheightfrac + dy;
   }
   else
      centeryfrac = viewheightfrac;
   
   centery = centeryfrac >> FRACBITS;

   view.ycenter = (float)centery;

   // use drawcolumn
   R_ForEachContext([](rendercontext_t &context) {
      context.view    = r_globalcontext.view;
      context.cb_view = r_globalcontext.cb_view;
   });
}

//
// Designators for Boom fake-surface sector view area
//
enum class ViewArea
{
   normal,
   below,
   above
};

// CVAR to force Boom viewpoint-dependent global colormaps.
bool r_boomcolormaps;

//
// Get sector colormap based on the view area constant
//
static int R_getSectorColormap(const rendersector_t &sector, ViewArea viewarea)
{
   switch(viewarea)
   {
      case ViewArea::above:
         return sector.topmap;
      case ViewArea::below:
         return sector.bottommap;
      default:
         return sector.midmap;
   }
}

//
// killough 3/20/98, 4/4/98: select colormap based on player status
// haleyjd 03/04/07: rewritten to get colormaps from the sector itself
// instead of from its heightsec if it has one (heightsec colormaps are
// transferred to their affected sectors at level setup now).
//
void R_SectorColormap(cmapcontext_t &context, const fixed_t viewz, const rendersector_t *s)
{
   int colormapIndex = 0;
   bool boomStyleOverride = false;
   ViewArea area = ViewArea::normal;

   // haleyjd: Under BOOM logic, the view sector determines the colormap of all sectors in view.
   // This is supported for backward compatibility.
   if(r_boomcolormaps || demo_version <= 203 || LevelInfo.sectorColormaps == INFO_SECMAP_BOOM)
      s = view.sector;
   else if(LevelInfo.sectorColormaps != INFO_SECMAP_SMMU && view.sector->heightsec != -1 &&
      (view.sector->topmap | view.sector->midmap | view.sector->bottommap) & COLORMAP_BOOMKIND)
   {
      // Boom colormap compatibility disabled from both console and EMAPINFO and game mode is modern
      // Eternity.

      // On the other hand, modern SMMU coloured sectors is not enforced in EMAPINFO, so we may
      // still have Boom-style global colormap changing (as opposed to direct ExtraData/UDMF
      // setting). We're in a fake-surfaces sector which has Boom-style colormaps set on some of the
      // layers. Check each area.
      const sector_t &heightSector = sectors[view.sector->heightsec];

      // Pick area ID the viewer is in
      if(viewz < heightSector.srf.floor.height)
         area = ViewArea::below;
      else if(viewz > heightSector.srf.ceiling.height)
         area = ViewArea::above;
      else
         area = ViewArea::normal;

      colormapIndex = R_getSectorColormap(*view.sector, area);
      if(colormapIndex & COLORMAP_BOOMKIND)  // is it one of those set via the Boom method?
      {
         boomStyleOverride = true;
         s = view.sector;
      }
   }
    
   if(!boomStyleOverride)  // if overridden by Boom transfers, don't process this again
   {
      if(s->heightsec == -1)
         area = ViewArea::normal;
      else
      {
         // find which actual area the viewpoint is in. Must check from the viewer's sector.
         int hs = view.sector->heightsec;

         if(hs == -1)
            area = ViewArea::normal;
         else if(viewz < sectors[hs].srf.floor.height)
            area = ViewArea::below;
         else if(viewz > sectors[hs].srf.ceiling.height)
            area = ViewArea::above;
         else
            area = ViewArea::normal;

      }
      colormapIndex = R_getSectorColormap(*s, area);
   }

   if(colormapIndex & COLORMAP_BOOMKIND)
   {
      // If we got Boom-set colormaps on OTHER sectors than the view sector, then use the view
      // sector's colormap. Needed to prevent Boom-coloured sectors from showing up when seen from
      // non-coloured sectors.
      if(!r_boomcolormaps && !boomStyleOverride && LevelInfo.sectorColormaps != INFO_SECMAP_SMMU)
         colormapIndex = R_getSectorColormap(*view.sector, area);

      colormapIndex &= ~COLORMAP_BOOMKIND;
   }

   context.fullcolormap = colormaps[colormapIndex];
   context.zlight       = c_zlight[colormapIndex];
   context.scalelight   = c_scalelight[colormapIndex];

   if(viewplayer->fixedcolormap)
   {
      // killough 3/20/98: localize scalelightfixed (readability/optimization)
      context.fixedcolormap = context.fullcolormap   // killough 3/20/98: use fullcolormap
        + viewplayer->fixedcolormap*256*sizeof(lighttable_t);
   }
   else
      context.fixedcolormap = nullptr;
}

angle_t R_WadToAngle(int wadangle)
{
   // haleyjd: FIXME: needs comp option
   // allows wads to specify angles to
   // the nearest degree, not nearest 45   

   return (demo_version < 302) 
             ? (wadangle / 45) * ANG45 
             : wadangle * (ANG45 / 45);
}

//
// Render a single context
//
void R_RenderViewContext(rendercontext_t &context)
{
   memset(context.spritecontext.sectorvisited, 0, sizeof(bool) * numsectors);
   R_ClearMarkedSprites(context.spritecontext, *context.heap);
   context.portalcontext.worldwindowid = 0;
   context.portalcontext.numrelations = 0;

   // Clear buffers.
   R_ClearClipSegs(context.bspcontext, context.bounds);
   R_ClearDrawSegs(context.bspcontext);
   R_ClearPlanes(context.planecontext, context.bounds);
   R_ClearSprites(context.spritecontext);

   // check for new console commands.
   //NetUpdate();

   context.portalcontext.postbspwindowid = 1;   // reset to 1 on frame
   
   // The head node is the last node output.
   R_RenderBSPNode(context, numnodes - 1);

   // Check for new console commands.
   //NetUpdate();

   R_SetMaskedSilhouette(context.bounds, nullptr, nullptr);

   // Push the first element on the Post-BSP stack
   R_PushPost(context.bspcontext, context.spritecontext, *context.heap, context.bounds, true,
              nullptr, { false });

   {
      const poststack_t *pstack = context.spritecontext.pstack;
      int pstacksize = context.spritecontext.pstacksize;
      const maskedrange_t &masked = *pstack[pstacksize - 1].masked;
      const vissprite_t *vissprites = context.spritecontext.vissprites;

      const pwindow_t *windowhead = context.portalcontext.windowhead;

      for(int i = masked.firstsprite; i <= masked.lastsprite; ++i)
      {
         for(const pwindow_t *window = windowhead; window; window = window->next)
         {
            if(window->type != pw_line || window->portal->type != R_LINKED)
               continue;
            // NOTE: line windows can't have children, so skip that detail
            int xinter = R_SpriteIntersectsForegroundWindow(vissprites[i], *window);
            if(xinter != INT_MIN)
            {
               // TODO: move this elsewhere
            }
         }
      }
   }

   // SoM 12/9/03: render the portals.
   R_RenderPortals(context);

   R_DrawPlanes(
      context.cmapcontext, *context.heap, context.planecontext.mainhash,
      context.planecontext.spanstart, context.view.angle, nullptr
   );

   // Check for new console commands.
   //NetUpdate();

   // Draw Post-BSP elements such as sprites, masked textures, and portal
   // overlays
   R_DrawPostBSP(context);
}

static int render_ticker = 0;

extern void R_UntaintAndClearPortals();

//
// Primary renderer entry point.
//
void R_RenderPlayerView(player_t* player, camera_t *camerapoint)
{
   bool quake = false;
   unsigned int savedflags = 0;

   R_SetupFrame(player, camerapoint);

   // Untaint and clear portals
   R_UntaintAndClearPortals();

   if(autodetect_hom)
      R_HOMdrawer();

   // haleyjd 01/21/07: earthquakes -- make player invisible to himself
   if(player->quake && !camerapoint)
   {
      quake = true;
      savedflags = player->mo->flags2;
      player->mo->flags2 |= MF2_DONTDRAW;
      player->mo->intflags |= MIF_HIDDENBYQUAKE;   // keep track
   }
   else
      player->mo->intflags &= ~MIF_HIDDENBYQUAKE;  // zero it otherwise

   // We don't need to multithread if we only have one context
   if(r_numcontexts == 1)
      R_RenderViewContext(r_globalcontext);
   else
      R_RunContexts();

   R_FinishMappingLines();
   R_ClearBadSpritesAndFrames();

   if(quake)
      player->mo->flags2 = savedflags;

   // draw the psprites on top of everything
   //  but does not draw on side views
   if(!viewangleoffset)
      R_DrawPlayerSprites();

   // haleyjd 09/04/06: handle through column engine
   if(r_column_engine->ResetBuffer)
      r_column_engine->ResetBuffer();

   // haleyjd: remove sector interpolations
   if(view.lerp != FRACUNIT)
   {
      R_setSectorInterpolationState(SEC_NORMAL);
      R_setScrollInterpolationState(SEC_NORMAL);
      R_setDynaSegInterpolationState(SEC_NORMAL);
   }
   
   // Check for new console commands.
   NetUpdate();
   
   render_ticker++;
}

//
// R_HOMdrawer
//
// sf: rewritten
//
void R_HOMdrawer()
{
   int colour;
   
   colour = !flashing_hom || (gametic % 20) < 9 ? 0xb0 : 0;

   V_ColorBlock(&vbscreen, (byte)colour, viewwindow.x, viewwindow.y, 
                viewwindow.width,
                viewwindow.height);
}

//
// R_ResetTrans
//
// Builds BOOM tranmap. 
// Called when general_translucency is changed at run-time.
//
void R_ResetTrans()
{
   if(general_translucency)
   {
      R_InitTranMap(false);
      R_InitSubMap(false);
   }
}

// action code flags for R_DoomTLStyle
enum
{
   R_CLEARTL  = 0x01, // clears TRANSLUCENCY flag
   R_SETTL    = 0x02, // sets TRANSLUCENCY flag
   R_CLEARADD = 0x04, // clears TLSTYLEADD flag
   R_SETADD   = 0x08, // sets TLSTYLEADD flag

   // special flag combos
   R_MAKENONE   = R_CLEARTL|R_CLEARADD,
   R_MAKEBOOM   = R_SETTL|R_CLEARADD,
   R_MAKENEWADD = R_CLEARTL|R_SETADD

};

// tlstyle struct for R_DoomTLStyle
struct r_tlstyle_t
{
   const char *className; // name of the thingtype
   int actions[3];        // actions
};

static r_tlstyle_t DoomThingStyles[] =
{
   // "Additive" items - these all change to additive in "new" style
   { "VileFire",        { R_MAKENONE, R_MAKEBOOM, R_MAKENEWADD } },
   { "MancubusShot",    { R_MAKENONE, R_MAKEBOOM, R_MAKENEWADD } },
   { "BaronShot",       { R_MAKENONE, R_MAKEBOOM, R_MAKENEWADD } },
   { "BossSpawnFire",   { R_MAKENONE, R_MAKEBOOM, R_MAKENEWADD } },
   { "DoomImpShot",     { R_MAKENONE, R_MAKEBOOM, R_MAKENEWADD } },
   { "CacodemonShot",   { R_MAKENONE, R_MAKEBOOM, R_MAKENEWADD } },
   { "PlasmaShot",      { R_MAKENONE, R_MAKEBOOM, R_MAKENEWADD } },
   { "BFGShot",         { R_MAKENONE, R_MAKEBOOM, R_MAKENEWADD } },
   { "ArachnotronShot", { R_MAKENONE, R_MAKEBOOM, R_MAKENEWADD } },
   { "DoomTeleFog",     { R_MAKENONE, R_MAKEBOOM, R_MAKENEWADD } },
   { "DoomItemFog",     { R_MAKENONE, R_MAKEBOOM, R_MAKENEWADD } },

   // "TL" items - these stay translucent in "new" style
   { "TracerSmoke",     { R_MAKENONE, R_MAKEBOOM, R_MAKEBOOM   } },
   { "BulletPuff",      { R_MAKENONE, R_MAKEBOOM, R_MAKEBOOM   } },
   { "InvisiSphere",    { R_MAKENONE, R_MAKEBOOM, R_MAKEBOOM   } },
   
   // "Normal" items - these return to no blending in "new" style
   { "SoulSphere",      { R_MAKENONE, R_MAKEBOOM, R_MAKENONE   } },
   { "InvulnSphere",    { R_MAKENONE, R_MAKEBOOM, R_MAKENONE   } },
   { "MegaSphere",      { R_MAKENONE, R_MAKEBOOM, R_MAKENONE   } },
};

// Translucency style setting for DOOM thingtypes.
int r_tlstyle;

//
// R_DoomTLStyle
//
// haleyjd 11/21/09: Selects alternate translucency styles for certain
// DOOM gamemode thingtypes which were originally altered in BOOM.
// We support the following styles:
// * None - all things are restored to no blending.
// * Boom - all things use the TRANSLUCENT flag
// * New  - some things are additive, others are TL, others are normal.
//
void R_DoomTLStyle()
{
   static bool firsttime = true;

   // If the first style set is to BOOM-style, then we don't actually need
   // to do anything at all here. This is safest for older mods that might
   // have removed the TRANSLUCENT flag from any thingtypes. This way only
   // a user set of the r_tlstyle variable from the UI will result in the
   // modification of such things.
   if(firsttime && r_tlstyle == R_TLSTYLE_BOOM)
   {
      firsttime = false;
      return;
   }
   
   for(size_t i = 0; i < earrlen(DoomThingStyles); i++)
   {
      int tnum   = E_ThingNumForName(DoomThingStyles[i].className);
      int action = DoomThingStyles[i].actions[r_tlstyle];
      
      if(tnum == -1)
         continue;

      // Do not modify any flags if TLSTYLEADD was set in EDF initially.
      // None of the target thingtypes normally start out with this flag,
      // so if they already have it, then we know an older mod (essel's
      // Eternity enhancement pack, probably) is active.
      if(firsttime && mobjinfo[tnum]->flags3 & MF3_TLSTYLEADD)
         continue;
      
      // Do the action
      if(action & R_CLEARTL)
         mobjinfo[tnum]->flags &= ~MF_TRANSLUCENT;
      else if(action & R_SETTL)
         mobjinfo[tnum]->flags |= MF_TRANSLUCENT;
      
      if(action & R_CLEARADD)
         mobjinfo[tnum]->flags3 &= ~MF3_TLSTYLEADD;
      else if(action & R_SETADD)
         mobjinfo[tnum]->flags3 |= MF3_TLSTYLEADD;
      
      // if we are in-level, update all things of the corresponding type too
      if(gamestate == GS_LEVEL)
      {
         Thinker *th;
         
         for(th = thinkercap.next; th != &thinkercap; th = th->next)
         {
            Mobj *mo;
            if((mo = thinker_cast<Mobj *>(th)))
            {
               if(mo->type == tnum)
               {
                  if(action & R_CLEARTL)
                     mo->flags &= ~MF_TRANSLUCENT;
                  else if(action & R_SETTL)
                     mo->flags |= MF_TRANSLUCENT;
                  
                  if(action & R_CLEARADD)
                     mo->flags3 &= ~MF3_TLSTYLEADD;
                  else if(action & R_SETADD)
                     mo->flags3 |= MF3_TLSTYLEADD;
               }
            }
         } // end thinker loop
      }
   } // end main loop

   firsttime = false;
}

//
// Console Commands
//

static const char *handedstr[]  = { "right", "left" };
static const char *ptranstr[]   = { "none", "smooth", "general" };
static const char *coleng[]     = { "normal" };
static const char *spaneng[]    = { "highprecision" };
static const char *tlstylestr[] = { "opaque", "boom", "additive" };
static const char *sprprojstr[] = { "default", "fast", "thorough" };

VARIABLE_BOOLEAN(lefthanded, nullptr,               handedstr);
VARIABLE_BOOLEAN(r_blockmap, nullptr,               onoff);
VARIABLE_BOOLEAN(flashing_hom, nullptr,             onoff);
VARIABLE_BOOLEAN(r_precache, nullptr,               onoff);
VARIABLE_TOGGLE(showpsprites,  nullptr,             yesno);
VARIABLE_TOGGLE(centerfire, nullptr,                onoff);
VARIABLE_BOOLEAN(stretchsky, nullptr,               onoff);
VARIABLE_BOOLEAN(r_swirl, nullptr,                  onoff);
VARIABLE_BOOLEAN(general_translucency, nullptr,     onoff);
VARIABLE_BOOLEAN(autodetect_hom, nullptr,           yesno);
VARIABLE_TOGGLE(r_boomcolormaps, nullptr,           onoff);

// SoM: Variable FOV
VARIABLE_INT(fov, nullptr, 20, 179, nullptr);

// SoM: Portal tainted
VARIABLE_BOOLEAN(showtainted, nullptr,              onoff);

VARIABLE_INT(tran_filter_pct,     nullptr, 0, 100,                    nullptr);
VARIABLE_INT(screenSize,          nullptr, 0, 8,                      nullptr);
VARIABLE_INT(usegamma,            nullptr, 0, 4,                      nullptr);
VARIABLE_INT(particle_trans,      nullptr, 0, 2,                      ptranstr);
VARIABLE_INT(r_column_engine_num, nullptr, 0, NUMCOLUMNENGINES - 1,   coleng);
VARIABLE_INT(r_span_engine_num,   nullptr, 0, NUMSPANENGINES - 1,     spaneng);
VARIABLE_INT(r_tlstyle,           nullptr, 0, R_TLSTYLE_NUM - 1,      tlstylestr);
VARIABLE_INT(r_sprprojstyle,      nullptr, 0, R_SPRPROJSTYLE_NUM - 1, sprprojstr);

CONSOLE_VARIABLE(r_fov, fov, 0)
{
   setsizeneeded = true;
}

CONSOLE_VARIABLE(r_showrefused, showtainted, 0) {}

CONSOLE_VARIABLE(gamma, usegamma, 0)
{
   const char *msg;

   // haleyjd 09/06/02: restore message
   switch(usegamma)
   {
   case 0:
      msg = DEH_String("GAMMALVL0");
      break;
   case 1:
      msg = DEH_String("GAMMALVL1");
      break;
   case 2:
      msg = DEH_String("GAMMALVL2");
      break;
   case 3:
      msg = DEH_String("GAMMALVL3");
      break;
   case 4:
      msg = DEH_String("GAMMALVL4");
      break;
   default:
      msg = "borked!";
      break;
   }

   player_printf(&players[consoleplayer], "%s", msg);

   // change to new gamma val
   I_SetPalette(nullptr);
}

CONSOLE_VARIABLE(lefthanded, lefthanded, 0) {}
CONSOLE_VARIABLE(r_blockmap, r_blockmap, 0) {}
CONSOLE_VARIABLE(r_homflash, flashing_hom, 0) {}
CONSOLE_VARIABLE(r_precache, r_precache, 0) {}
CONSOLE_VARIABLE(r_showgun, showpsprites, 0) {}
CONSOLE_VARIABLE(r_drawplayersprites, showpsprites, 0) {}
CONSOLE_VARIABLE(r_centerfire, centerfire, 0) {}

CONSOLE_VARIABLE(r_showhom, autodetect_hom, 0)
{
   doom_printf("hom detection %s", autodetect_hom ? "on" : "off");
}

CONSOLE_VARIABLE(r_stretchsky, stretchsky, 0) {}   // DEPRECATED
CONSOLE_VARIABLE(r_swirl, r_swirl, 0) {}

CONSOLE_VARIABLE(r_trans, general_translucency, 0)
{
   R_ResetTrans();
}

CONSOLE_VARIABLE(r_tranpct, tran_filter_pct, 0)
{
   R_ResetTrans();
}

CONSOLE_VARIABLE(screensize, screenSize, cf_buffered)
{
   // haleyjd 10/09/05: get sound from gameModeInfo
   S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_KEYLEFTRIGHT]);
   
   if(gamestate == GS_LEVEL) // not in intercam
   {
      hide_menu = 20;             // hide the menu for a few tics
      
      R_SetViewSize(screenSize + 3);

      // haleyjd 10/19/03: reimplement proper hud behavior
      switch(screenSize)
      {
      case 8: // in fullscreen, toggle the hud
         HU_ToggleHUD();
         break;
      default: // otherwise, disable it
         HU_DisableHUD();
         break;
      }
      if(automapactive)
         AM_UpdateWindowHeight(screenSize == 8);
   }
}

// haleyjd: particle translucency on/off
CONSOLE_VARIABLE(r_ptcltrans, particle_trans, 0) {}

CONSOLE_VARIABLE(r_columnengine, r_column_engine_num, 0) {}
CONSOLE_VARIABLE(r_spanengine,   r_span_engine_num,   0) {}

CONSOLE_COMMAND(p_dumphubs, 0)
{
   extern void P_DumpHubs();
   P_DumpHubs();
}

CONSOLE_VARIABLE(r_tlstyle, r_tlstyle, 0) 
{
   R_DoomTLStyle();
}

CONSOLE_VARIABLE(r_sprprojstyle, r_sprprojstyle, 0) {}

CONSOLE_VARIABLE(r_boomcolormaps, r_boomcolormaps, 0) {}

CONSOLE_COMMAND(r_changesky, 0)
{
   if(Console.argc < 1)
   {
      C_Puts("Usage: r_changesky texturename [index]");
      return;
   }

   qstring name = *Console.argv[0];
   name.toUpper();

   int index = 0;
   if(Console.argc >= 2)
      index = Console.argv[1]->toInt();

   int texnum = R_CheckForWall(name.constPtr());
   skyflat_t *sky1 = R_SkyFlatForIndex(index);
   if(texnum != -1)
   {
      R_CacheTexture(texnum);
      R_CacheSkyTexture(texnum);
   }

   if(sky1 && texnum != -1)
      sky1->texture = texnum;
   else
      C_Printf(FC_ERROR "Cannot set sky %d to %s", index, name.constPtr());
}

//----------------------------------------------------------------------------
//
// $Log: r_main.c,v $
// Revision 1.13  1998/05/07  00:47:52  killough
// beautification
//
// Revision 1.12  1998/05/03  23:00:14  killough
// beautification, fix #includes and declarations
//
// Revision 1.11  1998/04/07  15:24:15  killough
// Remove obsolete HOM detector
//
// Revision 1.10  1998/04/06  04:47:46  killough
// Support dynamic colormaps
//
// Revision 1.9  1998/03/23  03:37:14  killough
// Add support for arbitrary number of colormaps
//
// Revision 1.8  1998/03/16  12:44:12  killough
// Optimize away some function pointers
//
// Revision 1.7  1998/03/09  07:27:19  killough
// Avoid using FP for point/line queries
//
// Revision 1.6  1998/02/17  06:22:45  killough
// Comment out audible HOM alarm for now
//
// Revision 1.5  1998/02/10  06:48:17  killough
// Add flashing red HOM indicator for TNTHOM cheat
//
// Revision 1.4  1998/02/09  03:22:17  killough
// Make TNTHOM control HOM detector, change array decl to MAX_*
//
// Revision 1.3  1998/02/02  13:29:41  killough
// comment out dead code, add HOM detector
//
// Revision 1.2  1998/01/26  19:24:42  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:02  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
