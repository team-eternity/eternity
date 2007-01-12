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
//      Rendering main loop and setup functions,
//       utility functions (BSP, geometry, trigonometry).
//      See tables.c, too.
//
//-----------------------------------------------------------------------------

static const char rcsid[] = "$Id: r_main.c,v 1.13 1998/05/07 00:47:52 killough Exp $";

#include "doomstat.h"
#include "i_video.h"
#include "c_runcmd.h"
#include "g_game.h"
#include "hu_over.h"
#include "mn_engin.h" 
#include "r_main.h"
#include "r_things.h"
#include "r_plane.h"
#include "r_ripple.h"
#include "r_bsp.h"
#include "r_draw.h"
#include "r_drawq.h"
#include "r_drawl.h"
#include "m_bbox.h"
#include "r_sky.h"
#include "s_sound.h"
#include "v_video.h"
#include "w_wad.h"
#include "d_deh.h"
#include "d_gi.h"



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
lighttable_t *fixedcolormap;
int      centerx, centery;
fixed_t  centerxfrac, centeryfrac;
fixed_t  yaspectmul; // ANYRES aspect ratio
fixed_t  viewx, viewy, viewz;
angle_t  viewangle;
fixed_t  viewcos, viewsin;
player_t *viewplayer;
extern lighttable_t **walllights;
boolean  showpsprites=1; //sf
camera_t *viewcamera;
int detailshift; // haleyjd 09/10/06: low detail mode restoration
int c_detailshift;
// SoM: removed the old zoom code infavor of actual field of view!
int fov = 90;

extern int screenSize;

extern int global_cmap_index; // haleyjd: NGCS

void R_HOMdrawer(void);

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

angle_t xtoviewangle[MAX_SCREENWIDTH+1];   // killough 2/8/98

// killough 3/20/98: Support dynamic colormaps, e.g. deep water
// killough 4/4/98: support dynamic number of them as well

int numcolormaps;
lighttable_t *(*c_scalelight)[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t *(*c_zlight)[LIGHTLEVELS][MAXLIGHTZ];
lighttable_t *(*scalelight)[MAXLIGHTSCALE];
lighttable_t *(*zlight)[MAXLIGHTZ];
lighttable_t *fullcolormap;
lighttable_t **colormaps;

// killough 3/20/98, 4/4/98: end dynamic colormaps

int extralight;                           // bumped light from gun blasts

void (*colfunc)(void);                    // current column draw function

// haleyjd 09/04/06: column drawing engines
columndrawer_t *r_column_engine;
int r_column_engine_num;

static columndrawer_t *r_column_engines[NUMCOLUMNENGINES] =
{
   &r_normal_drawer, // normal engine
   &r_quad_drawer,   // quad cache engine
};

//
// R_SetColumnEngine
//
// Sets r_column_engine to the appropriate set of column drawers.
//
void R_SetColumnEngine(void)
{
   //if(detailshift == 0)
      r_column_engine = r_column_engines[r_column_engine_num];
   //else
   //   r_column_engine = &r_lowdetail_drawer;
}

// haleyjd 09/10/06: span drawing engines
spandrawer_t *r_span_engine;
int r_span_engine_num;

static spandrawer_t *r_span_engines[NUMSPANENGINES] =
{
   &r_spandrawer,    // normal engine
   &r_lpspandrawer,  // low-precision optimized
   &r_olpspandrawer, // old low-precision (unoptimized)
};

//
// R_SetSpanEngine
//
// Sets r_span_engine to the appropriate set of span drawers.
//
void R_SetSpanEngine(void)
{
   //if(detailshift == 0)
      r_span_engine = r_span_engines[r_span_engine_num];
   //else
   //   r_span_engine = &r_lowspandrawer;
}

//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// killough 5/2/98: reformatted
//

int R_PointOnSide(fixed_t x, fixed_t y, node_t *node)
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

// killough 5/2/98: reformatted

int R_PointOnSegSide(fixed_t x, fixed_t y, seg_t *line)
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
   return FixedMul(y, ldx>>FRACBITS) >= FixedMul(ldy>>FRACBITS, x);
}

//
// R_PointToAngle
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

angle_t R_PointToAngle(fixed_t x, fixed_t y)
{       
  return (y -= viewy, (x -= viewx) || y) ?
    x >= 0 ?
      y >= 0 ? 
        (x > y) ? tantoangle[SlopeDiv(y,x)] :                      // octant 0 
                ANG90-1-tantoangle[SlopeDiv(x,y)] :                // octant 1
        x > (y = -y) ? -tantoangle[SlopeDiv(y,x)] :                // octant 8
                       ANG270+tantoangle[SlopeDiv(x,y)] :          // octant 7
      y >= 0 ? (x = -x) > y ? ANG180-1-tantoangle[SlopeDiv(y,x)] : // octant 3
                            ANG90 + tantoangle[SlopeDiv(x,y)] :    // octant 2
        (x = -x) > (y = -y) ? ANG180+tantoangle[ SlopeDiv(y,x)] :  // octant 4
                              ANG270-1-tantoangle[SlopeDiv(x,y)] : // octant 5
    0;
}

angle_t R_PointToAngle2(fixed_t viewx, fixed_t viewy, fixed_t x, fixed_t y)
{       
  return (y -= viewy, (x -= viewx) || y) ?
    x >= 0 ?
      y >= 0 ? 
        (x > y) ? tantoangle[SlopeDiv(y,x)] :                      // octant 0 
                ANG90-1-tantoangle[SlopeDiv(x,y)] :                // octant 1
        x > (y = -y) ? -tantoangle[SlopeDiv(y,x)] :                // octant 8
                       ANG270+tantoangle[SlopeDiv(x,y)] :          // octant 7
      y >= 0 ? (x = -x) > y ? ANG180-1-tantoangle[SlopeDiv(y,x)] : // octant 3
                            ANG90 + tantoangle[SlopeDiv(x,y)] :    // octant 2
        (x = -x) > (y = -y) ? ANG180+tantoangle[ SlopeDiv(y,x)] :  // octant 4
                              ANG270-1-tantoangle[SlopeDiv(x,y)] : // octant 5
    0;
}

//
// R_InitTextureMapping
//
// killough 5/2/98: reformatted

static void R_InitTextureMapping (void)
{
   register int i, x, limit;
   float vtan, ratio;
   
   // Use tangent table to generate viewangletox:
   //  viewangletox will give the next greatest x
   //  after the view angle.
   //
   // Calc focal length so fov angles cover SCREENWIDTH

   // Cardboard
   ratio = 1.6f / ((float)v_width / (float)v_height);
   view.fov = (float)fov * PI / 180.0f; // 90 degrees
   view.tan = vtan = (float)tan(view.fov / 2);
   view.xfoc = view.xcenter / vtan;
   view.yfoc = view.xfoc * ratio;
   view.focratio = view.yfoc / view.xfoc;

   // Unfortunately, cardboard still has to co-exist with the old fixed point code
   focallen_x = (int)(view.xfoc * FRACUNIT);
   focallen_y = (int)(view.yfoc * FRACUNIT);

   // SoM: rewrote old LUT generation code to work with variable FOV
   i = 0;
   limit = -(int)((view.xcenter / view.xfoc) * 65536.0f);
   while(i < FINEANGLES/2 && finetangent[i] < limit)
      viewangletox[i++] = viewwidth + 1;

   limit = -limit;
   while(i < FINEANGLES/2 && finetangent[i] <= limit)
   {
      int t;

      t = FixedMul(finetangent[i], focallen_x);
      t = (centerxfrac - t + FRACUNIT-1) >> FRACBITS;
      if(t < -1)
         viewangletox[i++] = -1;
      else if(t > viewwidth+1)
         viewangletox[i++] = viewwidth+1;
      else
         viewangletox[i++] = t;
   }

   while(i < FINEANGLES/2)
      viewangletox[i++] = -1;
    
   // Scan viewangletox[] to generate xtoviewangle[]:
   //  xtoviewangle will give the smallest view angle
   //  that maps to x.
   
   for(x = 0; x <= viewwidth; ++x)
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
         if(viewangletox[i] == viewwidth+1)
            viewangletox[i] = viewwidth;
        
   clipangle = xtoviewangle[0];
}

//
// R_InitLightTables
// Only inits the zlight table,
//  because the scalelight table changes with view size.
//

#define DISTMAP 2

void R_InitLightTables (void)
{
   int i;
   
   // killough 4/4/98: dynamic colormaps
   c_zlight = malloc(sizeof(*c_zlight) * numcolormaps);
   c_scalelight = malloc(sizeof(*c_scalelight) * numcolormaps);
   
   // Calculate the light levels to use
   //  for each level / distance combination.
   for(i = 0; i < LIGHTLEVELS; ++i)
   {
      int j, startmap = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
      for(j = 0; j < MAXLIGHTZ; ++j)
      {
         int scale = FixedDiv ((SCREENWIDTH/2*FRACUNIT), (j+1)<<LIGHTZSHIFT);
         int t, level = startmap - (scale >>= LIGHTSCALESHIFT)/DISTMAP;
         
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

boolean setsizeneeded;
int     setblocks;
int     setdetail; // haleyjd 09/10/06: low detail mode restoration

//
// R_SetViewSize
//
// Do not really change anything here,
//  because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
void R_SetViewSize(int blocks, int detail)
{
   setsizeneeded = true;
   setblocks = blocks;
   setdetail = detail; // haleyjd 09/10/06
}

//
// R_SetupViewScaling
//
void R_SetupViewScaling(void)
{
   int i;
   // SoM: ANYRES
   // Moved stuff, reformatted a bit
   // haleyjd 04/03/05: removed unnecessary FixedDiv calls

   globalxscale  = (v_width << FRACBITS) / SCREENWIDTH;
   globalixscale = (SCREENWIDTH << FRACBITS) / v_width;
   globalyscale  = (v_height << FRACBITS) / SCREENHEIGHT;
   globaliyscale = (SCREENHEIGHT << FRACBITS) / v_height;

   realxarray[320] = v_width;
   realyarray[200] = v_height;

   for(i = 0; i < 320; ++i)
      realxarray[i] = (i * globalxscale) >> FRACBITS;

   for(i = 0; i < 200; ++i)
      realyarray[i] = (i * globalyscale) >> FRACBITS;

   if(setblocks == 11)
   {
      scaledviewwidth  = SCREENWIDTH;
      scaledviewheight = SCREENHEIGHT;                    // killough 11/98
      viewwidth  = v_width;// >> detailshift;                // haleyjd 09/10/06
      viewheight = v_height;
   }
   else
   {
      scaledviewwidth  = setblocks * 32;
      scaledviewheight = (setblocks * 168 / 10) & ~7;     // killough 11/98
      viewwidth  = realxarray[scaledviewwidth];// >> detailshift; // haleyjd 09/10/06
      viewheight = realyarray[scaledviewheight];
   }

   centerx     = viewwidth  / 2;
   centery     = viewheight / 2;
   centerxfrac = centerx << FRACBITS;
   centeryfrac = centery << FRACBITS;

   // haleyjd 04/03/05: Renamed yprojection to yaspectmul;
   // this matches zdoom and is more descriptive. 
   // It still works the same, though.
   
   yaspectmul = FixedDiv(globalyscale, globalxscale);

   // SoM: Cardboard
   view.xcenter = (view.width = (float)viewwidth) * 0.5f;
   view.ycenter = (view.height = (float)viewheight) * 0.5f;

   R_InitBuffer(scaledviewwidth, scaledviewheight);       // killough 11/98
}

//
// R_ExecuteSetViewSize
//
void R_ExecuteSetViewSize (void)
{
   int i;

   setsizeneeded = false;

   // haleyjd 09/10/06: low detail mode restoration
   detailshift = setdetail;
   
   R_SetupViewScaling();
   
   R_InitTextureMapping();
    
   // thing clipping
   for(i = 0; i < viewwidth; ++i)
      screenheightarray[i] = view.height - 1.0f;

   // Calculate the light levels to use
   //  for each level / scale combination.
   for(i = 0; i < LIGHTLEVELS; ++i)
   {
      int j, startmap = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
      for(j = 0; j < MAXLIGHTSCALE; ++j)
      {                                       // killough 11/98:
         int t, level = startmap - j*1/DISTMAP;
         
         if(level < 0)
            level = 0;
         
         if(level >= NUMCOLORMAPS)
            level = NUMCOLORMAPS-1;
         
         // killough 3/20/98: initialize multiple colormaps
         level *= 256;
         
         for(t = 0; t < numcolormaps; ++t)     // killough 4/4/98
            c_scalelight[t][i][j] = colormaps[t] + level;
      }
   }

   if(view.tan < 1.0f)
   {
      view.pspritexscale = view.width / ((float)SCREENWIDTH * view.tan);
      view.pspritexstep = ((float)SCREENWIDTH * view.tan) / view.width;
      view.pspriteyscale = view.pspritexscale * view.focratio;
      view.pspriteystep = 1.0f / view.pspriteyscale;
   }
   else
   {
      view.pspritexscale = view.width / (float)SCREENWIDTH;
      view.pspritexstep = (float)SCREENWIDTH / view.width;
      view.pspriteyscale = view.pspritexscale * view.focratio;
      view.pspriteystep = 1.0f / view.pspriteyscale;
   }
}

//
// R_Init
//

void R_Init(void)
{
   R_InitData();
   R_SetViewSize(screenSize+3, c_detailshift);
   R_InitPlanes();
   R_InitLightTables();
   R_InitTranslationTables();
   R_InitParticles(); // haleyjd
}

//
// R_PointInSubsector
//
// killough 5/2/98: reformatted, cleaned up

subsector_t *R_PointInSubsector(fixed_t x, fixed_t y)
{
   int nodenum = numnodes-1;
   while(!(nodenum & NF_SUBSECTOR))
      nodenum = nodes[nodenum].children[R_PointOnSide(x, y, nodes+nodenum)];
   return &subsectors[nodenum & ~NF_SUBSECTOR];
}

int autodetect_hom = 0;       // killough 2/7/98: HOM autodetection flag


//
// R_SetupFrame
//

void R_SetupFrame(player_t *player, camera_t *camera)
{               
   mobj_t *mobj;
   fixed_t pitch;
   fixed_t dy;
   fixed_t viewheightfrac;
   
   // haleyjd 09/04/06: set or change column drawing engine
   // haleyjd 09/10/06: set or change span drawing engine
   R_SetColumnEngine();
   R_SetSpanEngine();
   
   viewplayer = player;
   mobj = player->mo;

   viewcamera = camera;
   if(!camera)
   {
      viewx = mobj->x;
      viewy = mobj->y;
      viewz = player->viewz;
      viewangle = mobj->angle;// + viewangleoffset;
      pitch = player->pitch;
   }
   else
   {
      viewx = camera->x;
      viewy = camera->y;
      viewz = camera->z;
      viewangle = camera->angle;
      pitch = camera->pitch;
   }

   extralight = player->extralight;
   viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
   viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];

   // SoM: Cardboard
   view.x = viewx / 65536.0f;
   view.y = viewy / 65536.0f;
   view.z = viewz / 65536.0f;
   view.angle = (ANG90 - viewangle) * PI / (ANGLE_1 * 180);
   view.pitch = (ANG90 - pitch) * PI / (ANGLE_1 * 180);
   view.sin = (float)sin(view.angle);
   if(view.angle == PI * 0.5f || view.angle == PI * 1.5f)
      view.cos = 0.0f;
   else
      view.cos = (float)cos(view.angle);

   // y shearing
   // haleyjd 04/03/05: perform calculation for true pitch angle

   // make fixed-point viewheight and divide by 2
   viewheightfrac = viewheight << (FRACBITS - 1);

   // haleyjd 10/08/06: use simpler calculation for pitch == 0 to avoid 
   // unnecessary roundoff error. This is what was causing sky textures to
   // appear a half-pixel too low (the entire display was too low actually).
   if(pitch)
   {
      dy = FixedMul(focallen_y, 
                    finetangent[(ANG90 - pitch) >> ANGLETOFINESHIFT]);
            
      // haleyjd: must bound after zooming
      if(dy < -viewheightfrac)
         dy = -viewheightfrac;
      else if(dy > viewheightfrac)
         dy = viewheightfrac;
      
      centeryfrac = viewheightfrac + dy;
   }
   else
   {
      centeryfrac = viewheightfrac;

      yslope = origyslope + (viewheight >> 1);
   }
   
   centery = centeryfrac >> FRACBITS;

   view.ycenter = (float)centery;

   // use drawcolumn
   colfunc = r_column_engine->DrawColumn; // haleyjd 09/04/06
   
   ++validcount;
}

//

typedef enum
{
   area_normal,
   area_below,
   area_above
} area_t;

void R_SectorColormap(sector_t *s)
{
   int cm;
   // killough 3/20/98, 4/4/98: select colormap based on player status
   
   // haleyjd: NGCS
   if(s->heightsec == -1) 
      cm = global_cmap_index;
   else
   {
      sector_t *viewsector;
      area_t viewarea;
      viewsector = R_PointInSubsector(viewx,viewy)->sector;

      // find which area the viewpoint (player) is in
      viewarea =
         viewsector->heightsec == -1 ? area_normal :
         viewz < sectors[viewsector->heightsec].floorheight ? area_below :
         viewz > sectors[viewsector->heightsec].ceilingheight ? area_above :
         area_normal;

      s = s->heightsec + sectors;

      cm = viewarea==area_normal ? s->midmap :
            viewarea==area_above ? s->topmap : s->bottommap;

      // haleyjd: NGCS -- sector colormaps may be 0
      if(!cm)
         cm = global_cmap_index;
   }

   fullcolormap = colormaps[cm];
   zlight = c_zlight[cm];
   scalelight = c_scalelight[cm];

   if(viewplayer->fixedcolormap)
   {
      int i;
      // killough 3/20/98: localize scalelightfixed (readability/optimization)
      static lighttable_t *scalelightfixed[MAXLIGHTSCALE];

      fixedcolormap = fullcolormap   // killough 3/20/98: use fullcolormap
        + viewplayer->fixedcolormap*256*sizeof(lighttable_t);
        
      walllights = scalelightfixed;

      for(i = 0; i < MAXLIGHTSCALE; ++i)
         scalelightfixed[i] = fixedcolormap;
   }
   else
      fixedcolormap = NULL;   
}

angle_t R_WadToAngle(int wadangle)
{
   if(demo_version < 302)            // maintain compatibility
      return (wadangle / 45) * ANG45;

   // haleyjd: FIXME: needs comp option
   // allows wads to specify angles to
   // the nearest degree, not nearest 45   
   return wadangle * (ANG45 / 45);     
}

static int render_ticker = 0;

extern void R_ResetColumnBuffer(void);

// haleyjd: temporary debug
extern void R_UntaintPortals(void);

//
// R_RenderView
//
void R_RenderPlayerView(player_t* player, camera_t *camerapoint)
{
   R_SetupFrame(player, camerapoint);
   
   // haleyjd: untaint portals
   R_UntaintPortals();

   // Clear buffers.
   R_ClearClipSegs();
   R_ClearDrawSegs();
   R_ClearPlanes();
   R_ClearSprites();
   
    if(autodetect_hom)
      R_HOMdrawer();
   
   // check for new console commands.
   NetUpdate();

   // The head node is the last node output.
   R_RenderBSPNode(numnodes-1);
   
   // Check for new console commands.
   NetUpdate();

#ifdef R_PORTALS
   R_SetMaskedSilhouette(NULL, NULL);
   R_PushMasked();
   
   // SoM 12/9/03: render the portals.
   R_RenderPortals();
#endif

   R_DrawPlanes();
   
   // Check for new console commands.
   NetUpdate();

   R_DrawMasked();
   
   // haleyjd 09/04/06: handle through column engine
   if(r_column_engine->ResetBuffer)
      r_column_engine->ResetBuffer();
   
   // Check for new console commands.
   NetUpdate();
   
   render_ticker++;
}

//
// R_HOMdrawer
//
// sf: rewritten
//
void R_HOMdrawer(void)
{
   int y, colour;
   char *dest;
   
   colour = !flashing_hom || (gametic % 20) < 9 ? 0xb0 : 0;
   dest = screens[0] + viewwindowy*v_width + viewwindowx;

   for(y = viewwindowy; y < viewwindowy+viewheight; ++y)
   {
      memset(dest, colour, viewwidth);
      dest += v_width;
   }
}

//
// R_ResetTrans
//
// Builds BOOM tranmap and initializes flex tran table if 
// necessary. Called when general_translucency is changed at 
// run-time.
//
void R_ResetTrans(void)
{
   if(general_translucency)
   {
      R_InitTranMap(0);

      // this only ever needs to be done once
      if(!flexTranInit)
      {
         byte *palette = W_CacheLumpName("PLAYPAL", PU_STATIC);
         V_InitFlexTranTable(palette);
         Z_ChangeTag(palette, PU_CACHE);
      }
   }
}

//
//  Console Commands
//

static char *handedstr[] = { "right", "left" };
static char *ptranstr[]  = { "none", "smooth", "general" };
static char *coleng[]    = { "normal", "quad" };
static char *spaneng[]   = { "highprecision", "lowprecision", "old" };
static char *detailstr[] = { "high", "low" };

VARIABLE_BOOLEAN(lefthanded, NULL,                  handedstr);
VARIABLE_BOOLEAN(r_blockmap, NULL,                  onoff);
VARIABLE_BOOLEAN(flashing_hom, NULL,                onoff);
VARIABLE_BOOLEAN(visplane_view, NULL,               onoff);
VARIABLE_BOOLEAN(r_precache, NULL,                  onoff);
VARIABLE_BOOLEAN(showpsprites, NULL,                yesno);
VARIABLE_BOOLEAN(stretchsky, NULL,                  onoff);
VARIABLE_BOOLEAN(r_swirl, NULL,                     onoff);
VARIABLE_BOOLEAN(general_translucency, NULL,        onoff);
VARIABLE_BOOLEAN(autodetect_hom, NULL,              yesno);
VARIABLE_BOOLEAN(c_detailshift,       NULL,         detailstr);

// SoM: Variable FOV
VARIABLE_INT(fov, NULL, 20, 179, NULL);

VARIABLE_INT(tran_filter_pct, NULL,     0, 100, NULL);
VARIABLE_INT(screenSize, NULL,          0, 8, NULL);
VARIABLE_INT(usegamma, NULL,            0, 4, NULL);
VARIABLE_INT(particle_trans, NULL,      0, 2, ptranstr);
VARIABLE_INT(r_column_engine_num, NULL, 0, NUMCOLUMNENGINES - 1, coleng);
VARIABLE_INT(r_span_engine_num,   NULL, 0, NUMSPANENGINES - 1,  spaneng);



CONSOLE_VARIABLE(r_fov, fov, 0)
{
   setsizeneeded = true;
}


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
   I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
}

CONSOLE_VARIABLE(lefthanded, lefthanded, 0) {}
CONSOLE_VARIABLE(r_blockmap, r_blockmap, 0) {}
CONSOLE_VARIABLE(r_homflash, flashing_hom, 0) {}
CONSOLE_VARIABLE(r_planeview, visplane_view, 0) {}
CONSOLE_VARIABLE(r_precache, r_precache, 0) {}
CONSOLE_VARIABLE(r_showgun, showpsprites, 0) {}

CONSOLE_VARIABLE(r_showhom, autodetect_hom, 0)
{
   doom_printf("hom detection %s", autodetect_hom ? "on" : "off");
}

CONSOLE_VARIABLE(r_stretchsky, stretchsky, 0) {}
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
   S_StartSound(NULL, gameModeInfo->menuSounds[MN_SND_KEYLEFTRIGHT]);
   
   if(gamestate == GS_LEVEL) // not in intercam
   {
      hide_menu = 20;             // hide the menu for a few tics
      
      R_SetViewSize(screenSize + 3, detailshift);

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
   }
}

// haleyjd: particle translucency on/off
CONSOLE_VARIABLE(r_ptcltrans, particle_trans, 0) {}

CONSOLE_VARIABLE(r_columnengine, r_column_engine_num, 0) {}
CONSOLE_VARIABLE(r_spanengine,   r_span_engine_num,   0) {}

CONSOLE_VARIABLE(r_detail, c_detailshift, 0)
{
   R_SetViewSize(screenSize + 3, c_detailshift);
   player_printf(&players[consoleplayer], 
                 "%s detail", detailstr[c_detailshift]);
}

VARIABLE_INT(r_vissprite_limit, NULL, -1, D_MAXINT, NULL);
CONSOLE_VARIABLE(r_vissprite_limit, r_vissprite_limit, 0) {}

CONSOLE_COMMAND(p_dumphubs, 0)
{
   extern void P_DumpHubs();
   P_DumpHubs();
}

void R_AddCommands(void)
{
   C_AddCommand(r_fov);
   C_AddCommand(lefthanded);
   C_AddCommand(r_blockmap);
   C_AddCommand(r_homflash);
   C_AddCommand(r_planeview);
   C_AddCommand(r_precache);
   C_AddCommand(r_showgun);
   C_AddCommand(r_showhom);
   C_AddCommand(r_stretchsky);
   C_AddCommand(r_swirl);
   C_AddCommand(r_trans);
   C_AddCommand(r_tranpct);
   C_AddCommand(screensize);
   C_AddCommand(gamma);
   C_AddCommand(r_ptcltrans);
   C_AddCommand(r_columnengine);
   C_AddCommand(r_spanengine);
   C_AddCommand(r_detail);
   C_AddCommand(r_vissprite_limit);

   C_AddCommand(p_dumphubs);
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
