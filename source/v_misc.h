// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------
//
// Miscellaneous Video Functions
//
//-----------------------------------------------------------------------------

#ifndef V_MISC_H__
#define V_MISC_H__

#include "m_fixed.h"

void V_InitMisc();

//=============================================================================
//
// Font
//

extern const char *small_font_name;
extern const char *big_font_name;
extern const char *big_num_font_name;

// haleyjd 10/30/06: enum for text control characters
enum
{
   TEXT_COLOR_BRICK = 0x80,
   TEXT_COLOR_TAN,
   TEXT_COLOR_GRAY,
   TEXT_COLOR_GREEN,
   TEXT_COLOR_BROWN,
   TEXT_COLOR_GOLD,
   TEXT_COLOR_RED,
   TEXT_COLOR_BLUE,
   TEXT_COLOR_ORANGE,
   TEXT_COLOR_YELLOW,
   TEXT_COLOR_CUSTOM1,
   TEXT_COLOR_CUSTOM2,
   TEXT_COLOR_CUSTOM3,
   TEXT_COLOR_CUSTOM4,

   TEXT_COLOR_MIN = TEXT_COLOR_BRICK,
   TEXT_COLOR_MAX = TEXT_COLOR_CUSTOM4,

   TEXT_CONTROL_TRANS = 0xfa,
   TEXT_COLOR_NORMAL,
   TEXT_COLOR_HI,
   TEXT_COLOR_ERROR,
   TEXT_CONTROL_SHADOW,
   TEXT_CONTROL_ABSCENTER,

   TEXT_CONTROL_MAX
};

// normal font colors -- 128 through 137
#define FC_BRICK        "\x80"
#define FC_TAN          "\x81"
#define FC_GRAY         "\x82"
#define FC_GREEN        "\x83"
#define FC_BROWN        "\x84"
#define FC_GOLD         "\x85"
#define FC_RED          "\x86"
#define FC_BLUE         "\x87"
#define FC_ORANGE       "\x88"
#define FC_YELLOW       "\x89"
// haleyjd 09/17/12: custom colors
#define FC_CUSTOM1      "\x8a"
#define FC_CUSTOM2      "\x8b"
#define FC_CUSTOM3      "\x8c"
#define FC_CUSTOM4      "\x8d"

// haleyjd: translucent text support 
#define FC_TRANS        "\xfa"
// haleyjd 08/20/02: new characters for internal color usage 
#define FC_NORMAL       "\xfb"
#define FC_HI           "\xfc"
#define FC_ERROR        "\xfd"
// haleyjd 03/14/06: shadow toggle via text
#define FC_SHADOW       "\xfe"
// haleyjd 03/29/06: absolute centering toggle
#define FC_ABSCENTER    "\xff"

//=============================================================================
//
// Box Drawing
//

void V_DrawBox(int, int, int, int);

//=============================================================================
//
// Loading box
//

void V_DrawLoading();
void V_SetLoading(int total, const char *mess);
void V_LoadingIncrease();
void V_LoadingSetTo(int amount);

// High-level calls
void V_LoadingUpdateTicked(int amount);

//=============================================================================
//
// FPS ticker
//

void V_FPSDrawer();
void V_FPSTicker();
extern int v_ticker;

//=============================================================================
//
// Background 'tile' fill
//

struct VBuffer;

void V_DrawBackgroundCached(byte *src, VBuffer *back_dest);
void V_DrawBackground(const char *patchname, VBuffer *back_dest);
void V_DrawDistortedBackground(const char* patchname, VBuffer *back_dest);

// SoM: replaced globals with a struct and a single global
struct cb_video_t
{
   // SoM: Not implemented (yet)
   int         bitdepth, pixelsize;

   int         width, height;
   int         pitch;
   fixed_t     widthfrac, heightfrac;
   fixed_t     xscale, yscale;
   fixed_t     xstep, ystep;

   float       xscalef, yscalef;
   float       xstepf, ystepf;
   bool        scaled; // SoM: should be set when the scale values are

   byte        *screens[4];

   // SoM 1-31-04: This will insure that scaled patches and such are put in the right places
   int x1lookup[321];
   int y1lookup[201];
   int x2lookup[321];
   int y2lookup[201];
};

extern cb_video_t video;

#endif

// EOF
