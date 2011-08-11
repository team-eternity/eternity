// Emacs style mode select -*- C -*-
//----------------------------------------------------------------------------
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

#ifndef __V_MISC_H__
#define __V_MISC_H__

#include "v_patch.h"
#include "v_block.h"

void V_InitBox(void);
void V_InitMisc(void);

/////////////////////////////////////////////////////////////////////////////
//
// Mode setting
//

void V_ResetMode(void);

/////////////////////////////////////////////////////////////////////////////
//
// Font
//

const char *small_font_name;
const char *big_font_name;
const char *big_num_font_name;

// haleyjd 10/30/06: enum for text control characters
enum
{
   TEXT_COLOR_BRICK = 128,
   TEXT_COLOR_TAN,
   TEXT_COLOR_GRAY,
   TEXT_COLOR_GREEN,
   TEXT_COLOR_BROWN,
   TEXT_COLOR_GOLD,
   TEXT_COLOR_RED,
   TEXT_COLOR_BLUE,
   TEXT_COLOR_ORANGE,
   TEXT_COLOR_YELLOW,
   TEXT_CONTROL_TRANS,
   TEXT_COLOR_NORMAL,
   TEXT_COLOR_HI,
   TEXT_COLOR_ERROR,
   TEXT_CONTROL_SHADOW,
   TEXT_CONTROL_ABSCENTER,

   TEXT_COLOR_MIN = TEXT_COLOR_BRICK,
   TEXT_COLOR_MAX = TEXT_COLOR_YELLOW,
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
// haleyjd: translucent text support (138)
#define FC_TRANS        "\x8a"
// haleyjd 08/20/02: new characters for internal color usage (139-141)
#define FC_NORMAL       "\x8b"
#define FC_HI           "\x8c"
#define FC_ERROR        "\x8d"
// haleyjd 03/14/06: shadow toggle via text (142)
#define FC_SHADOW       "\x8e"
// haleyjd 03/29/06: absolute centering toggle (143)
#define FC_ABSCENTER    "\x8f"

///////////////////////////////////////////////////////////////////////////
//
// Box Drawing
//

void V_DrawBox(int, int, int, int);

///////////////////////////////////////////////////////////////////////////
//
// Loading box
//

void V_DrawLoading();
void V_SetLoading(int total, char *mess);
void V_LoadingIncrease();
void V_LoadingSetTo(int amount);

///////////////////////////////////////////////////////////////////////////
//
// FPS ticker
//

void V_FPSDrawer();
void V_FPSTicker();
extern int v_ticker;

///////////////////////////////////////////////////////////////////////////
//
// Background 'tile' fill
//

void V_DrawBackgroundCached(byte *src, VBuffer *back_dest);
void V_DrawBackground(const char *patchname, VBuffer *back_dest);
void V_DrawDistortedBackground(const char* patchname, VBuffer *back_dest);

#endif

// EOF
