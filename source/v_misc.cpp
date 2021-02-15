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
//--------------------------------------------------------------------------
//
// Misc Video stuff.
//
// Font. Loading box. FPS ticker, etc
//
//---------------------------------------------------------------------------

#include "z_zone.h"

#include "autopalette.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_gi.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_fonts.h"
#include "hal/i_timer.h"
#include "i_system.h"
#include "i_video.h"
#include "m_qstr.h"
#include "m_swap.h"
#include "r_main.h" // haleyjd
#include "r_patch.h"
#include "r_state.h"
#include "v_alloc.h"
#include "v_block.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patch.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"


extern int gamma_correct;

//
// Cardboard Video Structure
//
// Holds all metrics related to the current video mode.
//
cb_video_t video = 
{
   8, 1,
   SCREENWIDTH, 
   SCREENHEIGHT, 
   SCREENWIDTH,
   SCREENWIDTH * FRACUNIT,
   SCREENHEIGHT * FRACUNIT,
   FRACUNIT, 
   FRACUNIT, 
   FRACUNIT, 
   FRACUNIT,
   1.0f, 1.0f, 1.0f, 1.0f, 
   false,
   {nullptr, nullptr, nullptr, nullptr}
};

//=============================================================================
//
// Box Drawing
//
// Originally from the Boom heads up code
//

static patch_t *bgp[9];        // background for boxes

#define FG 0

void V_DrawBox(int x, int y, int w, int h)
{
   int xs = bgp[0]->width;
   int ys = bgp[0]->height;
   int i, j;
   
   // top rows
   V_DrawPatch(x, y, &subscreen43, bgp[0]);    // ul
   for(j = x+xs; j < x+w-xs; j += xs)       // uc
      V_DrawPatch(j, y, &subscreen43, bgp[1]);
   V_DrawPatchShadowed(j, y, &subscreen43, bgp[2], nullptr, 65536);    // ur
   
   // middle rows
   for(i = y+ys; i < y+h-ys; i += ys)
   {
      V_DrawPatch(x, i, &subscreen43, bgp[3]);    // cl
      for(j = x+xs; j < x+w-xs; j += xs)       // cc
         V_DrawPatch(j, i, &subscreen43, bgp[4]);
      V_DrawPatchShadowed(j, i, &subscreen43, bgp[5], nullptr, 65536);    // cr
   }
   
   // bottom row
   V_DrawPatchShadowed(x, i, &subscreen43, bgp[6], nullptr, 65536);
   for(j = x+xs; j < x+w-xs; j += xs)
      V_DrawPatchShadowed(j, i, &subscreen43, bgp[7], nullptr, 65536);
   V_DrawPatchShadowed(j, i, &subscreen43, bgp[8], nullptr, 65536);
}

static void V_InitBox()
{
   bgp[0] = PatchLoader::CacheName(wGlobalDir, "BOXUL", PU_STATIC);
   bgp[1] = PatchLoader::CacheName(wGlobalDir, "BOXUC", PU_STATIC);
   bgp[2] = PatchLoader::CacheName(wGlobalDir, "BOXUR", PU_STATIC);
   bgp[3] = PatchLoader::CacheName(wGlobalDir, "BOXCL", PU_STATIC);
   bgp[4] = PatchLoader::CacheName(wGlobalDir, "BOXCC", PU_STATIC);
   bgp[5] = PatchLoader::CacheName(wGlobalDir, "BOXCR", PU_STATIC);
   bgp[6] = PatchLoader::CacheName(wGlobalDir, "BOXLL", PU_STATIC);
   bgp[7] = PatchLoader::CacheName(wGlobalDir, "BOXLC", PU_STATIC);
   bgp[8] = PatchLoader::CacheName(wGlobalDir, "BOXLR", PU_STATIC);
}

//=============================================================================
//
// "Loading" Box
//

static int loading_amount = 0;
static int loading_total = -1;
static const char *loading_message;

//
// V_DrawLoading
//
void V_DrawLoading()
{
   int x, y;
   int linelen;
   vfont_t *font;

   // haleyjd 11/30/02: get palette indices from GameModeInfo
   int white = GameModeInfo->whiteIndex;
   int black = GameModeInfo->blackIndex;

   // haleyjd 01/29/09: not if -nodraw was used
   if(nodrawers)
      return;

   if(!loading_message)
      return;

   // 05/02/10: update console
   C_Drawer();
  
   V_DrawBox((SCREENWIDTH/2)-50, (SCREENHEIGHT/2)-30, 100, 40);

   font = E_FontForName("ee_smallfont");
   
   V_FontWriteText(font, loading_message, (SCREENWIDTH/2)-30, 
                   (SCREENHEIGHT/2)-20, &subscreen43);
  
   x = ((SCREENWIDTH/2)-45);
   y = (SCREENHEIGHT/2);
   linelen = (90*loading_amount) / loading_total;

   // White line
   if(linelen > 0)
      V_ColorBlockScaled(&subscreen43, (byte)white, x, y, linelen, 1);
   // Black line
   if(linelen < 90)
      V_ColorBlockScaled(&subscreen43, (byte)black, x + linelen, y, 90 - linelen, 1);

   I_FinishUpdate();
}

//
// V_SetLoading
//
void V_SetLoading(int total, const char *mess)
{
   loading_total = total ? total : 1;
   loading_amount = 0;
   loading_message = mess;

   if(in_textmode)
   {
      int i;
      printf(" %s ", mess);
      putchar('[');
      for(i=0; i<total; i++) putchar(' ');     // gap
      putchar(']');
      for(i=0; i<=total; i++) putchar('\b');    // backspace
   }
   else
      V_DrawLoading();
}

//
// V_LoadingIncrease
//
void V_LoadingIncrease()
{
   loading_amount++;
   if(in_textmode)
   {
      putchar('.');
      if(loading_amount == loading_total) putchar('\n');
   }
   else
      V_DrawLoading();

   if(loading_amount == loading_total) loading_message = nullptr;
}

//
// V_LoadingSetTo
//
void V_LoadingSetTo(int amount)
{
   loading_amount = amount;
   if(!in_textmode)
      V_DrawLoading();
}

//=============================================================================
//
// Framerate Ticker
//
// show dots at the bottom of the screen which represent
// an approximation to the current fps of doom.
// moved from i_video.c to make it a bit more
// system non-specific
//

// haleyjd 11/30/02: altered BLACK, WHITE defines to use GameModeInfo
#define BLACK (GameModeInfo->blackIndex)
#define WHITE (GameModeInfo->whiteIndex)
#define FPS_HISTORY 80
#define CHART_HEIGHT 40
#define X_OFFSET 20
#define Y_OFFSET 20

int v_ticker = 0;
static int history[FPS_HISTORY];
int current_count = 0;

static void V_ClassicFPSDrawer();
static void V_TextFPSDrawer();

//
// V_FPSDrawer
//
void V_FPSDrawer()
{
   int i;
   int x,y;          // screen x,y
   int cx, cy;       // chart x,y
   
   if(v_ticker == 2)
   {
      V_ClassicFPSDrawer();
      return;
   }

   if(v_ticker == 3)
   {
      V_TextFPSDrawer();
      return;
   }
  
   current_count++;
 
   // render the chart
   for(cx=0, x = X_OFFSET; cx<FPS_HISTORY; x++, cx++)
   {
      for(cy=0, y = Y_OFFSET; cy<CHART_HEIGHT; y++, cy++)
      {
         i = cy > (CHART_HEIGHT-history[cx]) ? BLACK : WHITE;
         V_ColorBlock(&vbscreen, (byte)i, x, y, 1, 1);
      }
   }
}

//
// V_FPSTicker
//
void V_FPSTicker()
{
   static int lasttic;
   int thistic;
   int i;
   
   thistic = i_haltimer.GetTime() / 7;
   
   if(lasttic != thistic)
   {
      lasttic = thistic;
      
      for(i = 0; i < FPS_HISTORY - 1; i++)
         history[i] = history[i+1];
      
      history[FPS_HISTORY-1] = current_count;
      current_count = 0;
   }
}

//
// V_ClassicFPSDrawer
//
// sf: classic fps ticker kept seperate
//
static void V_ClassicFPSDrawer()
{
  static int lasttic;
  
  int i = i_haltimer.GetTime();
  int tics = i - lasttic;
  lasttic = i;
  if (tics > 20)
    tics = 20;

   // SoM: ANYRES
   if(vbscreen.scaled)
   {
      for (i=0 ; i<tics*2 ; i+=2)
         V_ColorBlockScaled(&vbscreen, 0xff, i, SCREENHEIGHT-1, 1, 1);
      for ( ; i<20*2 ; i+=2)
         V_ColorBlockScaled(&vbscreen, 0x00, i, SCREENHEIGHT-1, 1, 1);
   }
   else
   {
      for (i=0 ; i<tics*2 ; i+=2)
         V_ColorBlock(&vbscreen, 0xff, i, SCREENHEIGHT-1, 1, 1);
      for ( ; i<20*2 ; i+=2)
         V_ColorBlock(&vbscreen, 0x00, i, SCREENHEIGHT-1, 1, 1);
   }
}

//
// V_TextFPSDrawer
//
static void V_TextFPSDrawer()
{
   static char fpsStr[16];
   static int  fhistory[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
   static int  lasttic = 0, slot = 0;
   vfont_t *font;
   
   float fps = 0;
   int   i, thistic, totaltics = 0;
   
   thistic = i_haltimer.GetTime();
   
   fhistory[slot & 15] = thistic != lasttic ? thistic - lasttic : 1;
   slot++;

   for(i = 0; i < 16; i++)
      totaltics += fhistory[i];
   
   if(totaltics)
      fps = (float)TICRATE / (totaltics / 16.0f);
   
   psnprintf(fpsStr, sizeof(fpsStr), FC_GRAY "FPS: %.2f", fps);
   
   lasttic = thistic;

   font = E_FontForName("ee_smallfont");
      
   V_FontWriteText(font, fpsStr, 5, 10);
}

//=============================================================================
//
// haleyjd: VBuffer
//
// VBuffer is Eternity's primary surface type. Patches, block blits, and some
// other fundamental operations can target a VBuffer in order to benefit from
// automatic scaling.
//

VBuffer vbscreen;         // vbscreen encapsulates the primary video surface
VBuffer backscreen1;      // backscreen1 is a temporary buffer for in_lude, border
VBuffer backscreen2;      // backscreen2 is a temporary buffer for screenshots
VBuffer backscreen3;      // backscreen3 is a temporary buffer for f_wipe
VBuffer subscreen43;      // provides a 4:3 sub-surface on vbscreen
VBuffer vbscreenyscaled;  // fits whole vbscreen but stretches pixels vertically by 20%
VBuffer vbscreenunscaled; // hi-res unscaled screen for whatever you wanna draw 1:1


static bool vbscreenneedsfree = false;

//
// V_initSubScreen43
//
// Initialize a 4:3 subscreen on top of the vbscreen VBuffer.
//
static void V_initSubScreen43()
{
   int subwidth;
   int offset;
   int unscaledw;

   if(vbscreen.getVirtualAspectRatio() <= 4 * FRACUNIT / 3)
   {
      subwidth  = vbscreen.width;
      offset    = 0;
      unscaledw = SCREENWIDTH;
   }
   else
   {
      subwidth = vbscreen.height * 4 / 3;
      offset   = (vbscreen.width - subwidth) / 2;

      const double scaleaspect = 1.2 * static_cast<double>(vbscreen.width) /
                                 static_cast<double>(vbscreen.height);
      unscaledw = static_cast<int>(round(SCREENHEIGHT * scaleaspect));

      // FIXME(?): vbscreenyscaled doesn't work if unscaledw is larger than vbscreen.width,
      // which happens if the vbscreen.height < SCREENHEIGHT * 1.2 (roughly)
      if(unscaledw > vbscreen.width)
         unscaledw = vbscreen.width;
      // FIXME(?): our scaling code cannot handle a subscreen smaller than 320x200
      if(subwidth < SCREENWIDTH)
      {
         subwidth = vbscreen.width;
         offset   = 0;
      }
   }

   V_InitSubVBuffer(&subscreen43, &vbscreen, offset, 0, subwidth, vbscreen.height);
   V_SetScaling(&subscreen43, SCREENWIDTH, SCREENHEIGHT);

   V_InitSubVBuffer(&vbscreenyscaled, &vbscreen, 0, 0, vbscreen.width, vbscreen.height);
   V_SetScaling(&vbscreenyscaled, unscaledw, SCREENHEIGHT);
}

//
// V_InitScreenVBuffer
//
static void V_InitScreenVBuffer()
{
   if(vbscreenneedsfree)
   {
      V_FreeVBuffer(&vbscreen);
      V_FreeVBuffer(&backscreen1);
      V_FreeVBuffer(&backscreen2);
      V_FreeVBuffer(&backscreen3);
      V_FreeVBuffer(&subscreen43);
      V_FreeVBuffer(&vbscreenyscaled);
      V_FreeVBuffer(&vbscreenunscaled);
   }
   else
      vbscreenneedsfree = true;

   V_InitVBufferFrom(&vbscreen, video.width, video.height, video.pitch, 
                     video.bitdepth, video.screens[0]);
   V_SetScaling(&vbscreen, SCREENWIDTH, SCREENHEIGHT);

   V_InitVBufferFrom(&vbscreenunscaled, video.width, video.height, video.pitch,
                     video.bitdepth, video.screens[0]);

   V_InitVBufferFrom(&backscreen1, video.width, video.height, video.height,
                     video.bitdepth, video.screens[1]);
   V_SetScaling(&backscreen1, SCREENWIDTH, SCREENHEIGHT);

   // Only vbscreen and backscreen1 need scaling set.
   V_InitVBufferFrom(&backscreen2, video.width, video.height, video.height,
                     video.bitdepth, video.screens[2]);
   V_InitVBufferFrom(&backscreen3, video.width, video.height, video.height,
                     video.bitdepth, video.screens[3]);

   // Init subscreen43
   V_initSubScreen43();
}

//
// V_Init
//
// Allocates the 4 full screen buffers in low DOS memory
// No return value
//
void V_Init()
{
   static byte *s = nullptr;
   
   int size = video.width * video.height;

   // haleyjd 04/29/13: purge and reallocate all VAllocItem instances
   VAllocItem::FreeAllocs();
   VAllocItem::SetNewMode(video.width, video.height);

   if(s)
      efree(s);

   video.screens[3] =
      (video.screens[2] =
         (video.screens[1] = s = (ecalloc(byte *, size, 3))) + size) + size;  

   R_SetupViewScaling();
   
   V_InitScreenVBuffer(); // haleyjd
}

//
// V_DrawBackgroundCached
//
// Tiles a 64 x 64 flat over the entirety of the provided VBuffer
// surface. Used by menus, intermissions, finales, etc.
//
void V_DrawBackgroundCached(byte *src, VBuffer *back_dest)
{
   back_dest->TileBlock64(back_dest, src);
}

//
// V_DrawBackground
//
void V_DrawBackground(const char *patchname, VBuffer *back_dest)
{
   int         tnum = R_FindFlat(patchname) - flatstart;
   byte        *src;
   
   // SoM: Extra protection, I don't think this should ever actually happen.
   if(tnum < 0 || tnum >= numflats)   
      src = R_GetLinearBuffer(badtex);
   else
      src = (byte *)(wGlobalDir.cacheLumpNum(firstflat + tnum, PU_CACHE));

   back_dest->TileBlock64(back_dest, src);
}

byte *R_DistortedFlat(int, bool);

//
// V_DrawDistortedBackground
//
// As above, but uses the ultra-cool water warping effect.
// Created by fraggle.
//
void V_DrawDistortedBackground(const char *patchname, VBuffer *back_dest)
{
   byte *src = R_DistortedFlat(R_FindFlat(patchname), true);
   
   back_dest->TileBlock64(back_dest, src);
}

//=============================================================================
//
// Init
//

void V_InitMisc()
{
   V_InitBox();

   // this only ever needs to be done once
   if(!flexTranInit)
   {
      AutoPalette palette(wGlobalDir);
      V_InitFlexTranTable(palette.get());      
   }
}

//=============================================================================
//
// Console Commands
//

const char *str_ticker[] = { "off", "chart", "classic", "text" };
VARIABLE_INT(v_ticker, nullptr, 0, 3,  str_ticker);

CONSOLE_COMMAND(v_fontcolors, 0)
{
   vfont_t *font;
   byte    *colors;
   FILE    *f;
   qstring  path;
   const char *fontName;

   if(Console.argc != 2)
   {
      C_Puts(FC_ERROR "Usage: v_fontcolors fontname filename");
      return;
   }
   
   fontName = Console.argv[0]->constPtr();
   if(!(font = E_FontForName(fontName)))
   {
      C_Printf(FC_ERROR "Unknown font %s", fontName);
      return;
   }

   if(!(colors = V_FontGetUsedColors(font)))
   {
      C_Puts(FC_ERROR "Cannot get used colors for this font");
      return;
   }

   path = userpath;
   path.pathConcatenate(Console.argv[1]->constPtr());

   if((f = fopen(path.constPtr(), "w")))
   {
      fprintf(f, "Font %s uses the following colors:\n", fontName);
      for(int i = 0; i < 256; i++)
      {
         if(colors[i] == 1)
            fprintf(f, "%d\n", i);
      }
      fclose(f);
      C_Printf(FC_HI "Wrote output to %s", path.constPtr());
   }
   else
      C_Puts(FC_ERROR "Could not open file for output");

   efree(colors);
}

CONSOLE_VARIABLE(v_ticker, v_ticker, 0) {}

// Dump a patch to file as a PNG. This involves a *lot* of code.
CONSOLE_COMMAND(v_dumppatch, 0)
{
   qstring filename;
   const char *lump;
   int fillcolor;

   if(Console.argc < 3)
   {
      C_Puts("Usage: v_dumppatch lumpname filename fillcolor");
      return;
   }

   lump = Console.argv[0]->constPtr();
   filename = usergamepath;
   filename.pathConcatenate(Console.argv[1]->constPtr());
   filename.addDefaultExtension(".png");

   fillcolor = Console.argv[2]->toInt();
   if(fillcolor < 0 || fillcolor >= 255)
      fillcolor = 247;

   V_WritePatchAsPNG(lump, filename.constPtr(), static_cast<byte>(fillcolor));
}

// EOF
