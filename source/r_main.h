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
//------------------------------------------------------------------------------
//
// Purpose: Main rendering module.
// Authors: James Haley, Stephen McGranahan, Ioan Chera, Max Waine
//

#ifndef R_MAIN_H__
#define R_MAIN_H__

// NEEDED BY R_doubleToUint32 below
#include "SDL_endian.h"

#include "tables.h"

#include "m_surf.h"
#include "m_vector.h"
// haleyjd 12/15/2010: Lighting data is required
#include "r_lighting.h"

struct pwindow_t;
struct columndrawer_t;
struct spandrawer_t;
struct rendercontext_t;
struct cmapcontext_t;

//
// POV related.
//

extern int      centerx;
extern int      centery;
extern fixed_t  centerxfrac;
extern fixed_t  centeryfrac;

extern int      validcount;
extern int      linecount;
extern int      loopcount;

extern bool     showpsprites;
extern bool     centerfire;
extern bool     r_boomcolormaps;

// haleyjd 11/21/09: enumeration for R_DoomTLStyle
enum
{
   R_TLSTYLE_NONE,
   R_TLSTYLE_BOOM,
   R_TLSTYLE_NEW,
   R_TLSTYLE_NUM
};

extern int r_tlstyle;

void R_DoomTLStyle();

void R_ResetTrans();

// Enumeration for sprite projection style
enum
{
   R_SPRPROJSTYLE_DEFAULT,
   R_SPRPROJSTYLE_FAST,
   R_SPRPROJSTYLE_THOROUGH,
   R_SPRPROJSTYLE_NUM
};

inline int r_sprprojstyle;

//
// Utility functions.
//

struct node_t;
struct rendersector_t;
struct seg_t;
struct subsector_t;
struct sector_t;
struct viewpoint_t;

int R_PointOnSideClassic(fixed_t x, fixed_t y, const node_t *node);
int R_PointOnSidePrecise(fixed_t x, fixed_t y, const node_t *node);
extern int (*R_PointOnSide)(fixed_t, fixed_t, const node_t *);

int R_PointOnSegSide(fixed_t x, fixed_t y, const seg_t *line);

int SlopeDiv(unsigned int num, unsigned int den);
angle_t R_PointToAngle(const fixed_t viewx, const fixed_t viewy, const fixed_t x, const fixed_t y);
angle_t R_PointToAngle2(fixed_t pviewx, fixed_t pviewy, fixed_t x, fixed_t y);
subsector_t *R_PointInSubsector(fixed_t x, fixed_t y);
fixed_t R_GetLerp(bool ignorepause);
void R_SectorColormap(cmapcontext_t &context, const viewpoint_t &viewpoint, const rendersector_t *s);

inline static subsector_t *R_PointInSubsector(v2fixed_t v)
{
   return R_PointInSubsector(v.x, v.y);
}

//
// REFRESH - the actual rendering functions.
//

struct camera_t;
struct player_t;

void R_AddMappedLine(const intptr_t lineIndex);

void R_RenderViewContext(rendercontext_t &context);
void R_RenderPlayerView(player_t *player, camera_t *viewcamera);

//
// R_ResetFOV
// 
// SoM: Called by I_InitGraphicsMode when the video mode is changed.
// Sets the base-line fov for the given screen ratio.
//
void R_ResetFOV(int width, int height);

void R_Init();                           // Called by startup code.
void R_SetViewSize(int blocks);          // Called by M_Responder.

void R_InitLightTables();                // killough 8/9/98

extern bool setsizeneeded;
// SoM
void R_SetupViewScaling();
void R_ExecuteSetViewSize();

angle_t R_WadToAngle(int wadangle);

extern int viewdir;

#define NUMCOLUMNENGINES 1
#define NUMSPANENGINES 1
extern int r_column_engine_num;
extern int r_span_engine_num;
extern columndrawer_t *r_column_engine;
extern spandrawer_t *r_span_engine;

void R_SetColumnEngine();
void R_SetSpanEngine();

// haleyjd 09/19/07: missing extern!
extern const float PI;

struct cb_view_t
{
   float pitch;

   float width, height;
   float xcenter, ycenter;
   
   float xfoc, yfoc, focratio;
   float fov;
   float tan;

   float pspritexscale, pspriteyscale;
   float pspriteystep;

   fixed_t   lerp;   // haleyjd: linear interpolation factor
   const sector_t *sector; // haleyjd: view sector, because of interpolation
};

// haleyjd 3/11/10: markflags
enum
{
   SEG_MARKCEILING = 0x01,
   SEG_MARKCPORTAL = 0x02,
   SEG_MARKFLOOR   = 0x04,
   SEG_MARKFPORTAL = 0x08,
   
   // SoM
   SEG_MARKCOVERLAY = 0x10,
   SEG_MARKFOVERLAY = 0x20,

   // ioanch
   SEG_MARK1SLPORTAL = 0x40,
};

struct side_t;
struct visplane_t;
struct portal_t;

struct cb_seg_t
{
   int x1, x2;
   float x1frac, x2frac;

   float toffset_base_x,   toffset_base_y;
   float toffset_top_x,    toffset_top_y;
   float toffset_mid_x,    toffset_mid_y;
   float toffset_bottom_x, toffset_bottom_y;

   float dist, dist2, diststep;
   float len, len2, lenstep;

   float top, top2, topstep;
   float high, high2, highstep;
   float low, low2, lowstep;
   float bottom, bottom2, bottomstep;

   bool twosided, clipsolid, maskedtex;
   int16_t toptex, midtex, bottomtex;

   unsigned int markflags; // haleyjd 03/11/10

   bool segtextured;

   int toptexmid, midtexmid, bottomtexmid;
   int toptexh, midtexh, bottomtexh;

   // The portal ignore flags. If a portal should be rendered even if the camera
   // is on the backface of it...
   bool f_portalignore, c_portalignore;

   // 8 bit tables
   const lighttable_t *const *walllights_top;
   const lighttable_t *const *walllights_mid;
   const lighttable_t *const *walllights_bottom;

   const side_t *side;
   const rendersector_t *frontsec, *backsec;
   Surfaces<visplane_t *> plane;
   const seg_t *line;

   Surfaces<const portal_t *> portal;
   pwindow_t *l_window, *b_window, *t_window;
   Surfaces<pwindow_t *> secwindow;   // surface windows
   // ioanch: added b_window for bottom edge portal

   // Extreme plane point Z for sloped sectors: used for sprite-clipping silhouettes.
   fixed_t maxfrontfloor, minfrontceil, maxbackfloor, minbackceil;

   // Skew-related values
   float skew_top_step, skew_mid_step, skew_bottom_step;
   float skew_top_baseoffset, skew_mid_baseoffset, skew_bottom_baseoffset;

   // If nonzero, require f_sky1 rendering
   int skyflat;
};


extern cb_view_t  view;

// SoM: frameid frame counter.
extern unsigned   frameid;

uint64_t R_GetVisitID(const rendercontext_t &context);

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
// Common routine to convert fixed-point angle to floating-point angle, by Cardboard's rules.
//
inline static float cb_fixedAngleToFloat(angle_t angle)
{
   return (ANG90 - angle) * PI / ANG180;
}

#endif

//----------------------------------------------------------------------------
//
// $Log: r_main.h,v $
// Revision 1.7  1998/05/03  23:00:42  killough
// beautification
//
// Revision 1.6  1998/04/06  04:43:17  killough
// Make colormaps fully dynamic
//
// Revision 1.5  1998/03/23  03:37:44  killough
// Add support for arbitrary number of colormaps
//
// Revision 1.4  1998/03/09  07:27:23  killough
// Avoid using FP for point/line queries
//
// Revision 1.3  1998/02/02  13:29:10  killough
// performance tuning
//
// Revision 1.2  1998/01/26  19:27:41  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
