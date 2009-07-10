// Emacs style mode select -*- C++ -*-
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
//
// Misc Video stuff.
//
// Font. Loading box. FPS ticker, etc
//
//---------------------------------------------------------------------------

#include <stdio.h>
#include "c_io.h"
#include "c_runcmd.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_video.h"
#include "v_video.h"
#include "v_misc.h"
#include "w_wad.h"
#include "d_gi.h"
#include "r_main.h" // haleyjd
#include "e_fonts.h"

extern int gamma_correct;

/////////////////////////////////////////////////////////////////////////////
//
// Console Video Mode Commands
//
// non platform-specific stuff is here in v_misc.c
// platform-specific stuff is in i_video.c
// videomode_t is platform specific although it must
// contain a member of type char* called description:
// see i_video.c for more info

int v_mode = 0;
static int prevmode = 0;

cb_video_t  video = 
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
   {NULL, NULL, NULL, NULL, NULL}
};

//
// V_NumModes
//
// Counts the number of video modes for the build platform.
// Used various places, including the -v_mode code and the
// menus.
//
int V_NumModes(void)
{
   int count = 0;
   
   while(videomodes[count].description)
      ++count;
   
   return count;
}

//
// V_ResetMode
//
// Called after changing video mode
//
void V_ResetMode(void)
{
   // check for invalid mode
   
   if(v_mode >= V_NumModes() || v_mode < 0)
   {
      C_Printf(FC_ERROR"invalid mode %i\n", v_mode);
      v_mode = prevmode;
      return;
   }
   
   prevmode = v_mode;
   
   I_SetMode(v_mode);
}

patch_t *bgp[9];        // background for boxes

////////////////////////////////////////////////////////////////////////////
//
// Box Drawing
//
// Originally from the Boom heads up code
//

#define FG 0

void V_DrawBox(int x, int y, int w, int h)
{
   int xs = SHORT(bgp[0]->width);
   int ys = SHORT(bgp[0]->height);
   int i,j;
   
   // top rows
   V_DrawPatch(x, y, &vbscreen, bgp[0]);    // ul
   for(j = x+xs; j < x+w-xs; j += xs)     // uc
      V_DrawPatch(j, y, &vbscreen, bgp[1]);
   V_DrawPatch(j, y, &vbscreen, bgp[2]);    // ur
   
   // middle rows
   for(i = y+ys; i < y+h-ys; i += ys)
   {
      V_DrawPatch(x, i, &vbscreen, bgp[3]);    // cl
      for(j = x+xs; j < x+w-xs; j += xs)     // cc
         V_DrawPatch(j, i, &vbscreen, bgp[4]);
      V_DrawPatch(j, i, &vbscreen, bgp[5]);    // cr
   }
   
   // bottom row
   V_DrawPatch(x, i, &vbscreen, bgp[6]);    // ll
   for(j = x+xs; j < x+w-xs; j += xs)     // lc
      V_DrawPatch(j, i, &vbscreen, bgp[7]);
   V_DrawPatch(j, i, &vbscreen, bgp[8]);    // lr
}

void V_InitBox(void)
{
   bgp[0] = (patch_t *) W_CacheLumpName("BOXUL", PU_STATIC);
   bgp[1] = (patch_t *) W_CacheLumpName("BOXUC", PU_STATIC);
   bgp[2] = (patch_t *) W_CacheLumpName("BOXUR", PU_STATIC);
   bgp[3] = (patch_t *) W_CacheLumpName("BOXCL", PU_STATIC);
   bgp[4] = (patch_t *) W_CacheLumpName("BOXCC", PU_STATIC);
   bgp[5] = (patch_t *) W_CacheLumpName("BOXCR", PU_STATIC);
   bgp[6] = (patch_t *) W_CacheLumpName("BOXLL", PU_STATIC);
   bgp[7] = (patch_t *) W_CacheLumpName("BOXLC", PU_STATIC);
   bgp[8] = (patch_t *) W_CacheLumpName("BOXLR", PU_STATIC);
}

//////////////////////////////////////////////////////////////////////////
//
// "Loading" Box
//

static int loading_amount = 0;
static int loading_total = -1;
static char *loading_message;

// SoM: ANYRES
void V_DrawLoading(void)
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
  
   V_DrawBox((SCREENWIDTH/2)-50, (SCREENHEIGHT/2)-30, 100, 40);

   font = E_FontForName("ee_smallfont");
   
   V_FontWriteText(font, loading_message, (SCREENWIDTH/2)-30, 
                   (SCREENHEIGHT/2)-20);
  
   x = ((SCREENWIDTH/2)-45);
   y = (SCREENHEIGHT/2);
   linelen = (90*loading_amount) / loading_total;

   // White line
   if(linelen > 0)
      V_ColorBlockScaled(&vbscreen, (byte)white, x, y, linelen, 1);
   // Black line
   if(linelen < 90)
      V_ColorBlockScaled(&vbscreen, (byte)black, x + linelen, y, 90 - linelen, 1);

   I_FinishUpdate();
}


void V_SetLoading(int total, char *mess)
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


void V_LoadingIncrease(void)
{
   loading_amount++;
   if(in_textmode)
   {
      putchar('.');
      if(loading_amount == loading_total) putchar('\n');
   }
   else
      V_DrawLoading();

   if(loading_amount == loading_total) loading_message = NULL;
}

void V_LoadingSetTo(int amount)
{
   loading_amount = amount;
   if(!in_textmode) V_DrawLoading();
}

/////////////////////////////////////////////////////////////////////////////
//
// Framerate Ticker
//
// show dots at the bottom of the screen which represent
// an approximation to the current fps of doom.
// moved from i_video.c to make it a bit more
// system non-specific

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

void V_ClassicFPSDrawer(void);
void V_TextFPSDrawer(void);

void V_FPSDrawer(void)
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

void V_FPSTicker(void)
{
   static int lasttic;
   int thistic;
   int i;
   
   thistic = I_GetTime() / 7;
   
   if(lasttic != thistic)
   {
      lasttic = thistic;
      
      for(i = 0; i < FPS_HISTORY - 1; i++)
         history[i] = history[i+1];
      
      history[FPS_HISTORY-1] = current_count;
      current_count = 0;
   }
}

// sf: classic fps ticker kept seperate

void V_ClassicFPSDrawer(void)
{
  static int lasttic;
  
  int i = I_GetTime();
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

void V_TextFPSDrawer(void)
{
   static char fpsStr[16];
   static int  fhistory[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
   static int  lasttic = 0, slot = 0;
   vfont_t *font;
   
   float fps = 0;
   int   i, thistic, totaltics = 0;
   
   thistic = I_GetTime();
   
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

// haleyjd: VBuffer stuff

VBuffer vbscreen;
VBuffer backscreen1;
VBuffer backscreen2;
VBuffer backscreen3;

static boolean vbscreenneedsfree = false;

//
// V_InitScreenVBuffer
//
static void V_InitScreenVBuffer(void)
{
   if(vbscreenneedsfree)
   {
      V_FreeVBuffer(&vbscreen);
      V_FreeVBuffer(&backscreen1);
      V_FreeVBuffer(&backscreen2);
      V_FreeVBuffer(&backscreen3);
   }
   else
      vbscreenneedsfree = true;

   V_InitVBufferFrom(&vbscreen, video.width, video.height, video.pitch, 
                     video.bitdepth, video.screens[0]);
   V_SetScaling(&vbscreen, SCREENWIDTH, SCREENHEIGHT);

   V_InitVBufferFrom(&backscreen1, video.width, video.height, video.width, 
                     video.bitdepth, video.screens[1]);
   V_SetScaling(&backscreen1, SCREENWIDTH, SCREENHEIGHT);

   // Only vbscreen and backscreen1 need scaling set.
   V_InitVBufferFrom(&backscreen2, video.width, video.height, video.width, 
                     video.bitdepth, video.screens[2]);
   V_InitVBufferFrom(&backscreen3, video.width, video.height, video.width, 
                     video.bitdepth, video.screens[3]);
}

extern void I_SetPrimaryBuffer(void);
extern void I_UnsetPrimaryBuffer(void);

//
// V_Init
//
// Allocates the 4 full screen buffers in low DOS memory
// No return value
//
void V_Init(void)
{
   static byte *s = NULL;
   
   int size = video.width * video.height;

   // SoM: ANYRES is ganna have to work in DJGPP too
   // haleyjd: why isn't this in i_video.c, anyways? oh well.
#ifdef DJGPP
   if(s)
      free(s), destroy_bitmap(screens0_bitmap);

   video.screens[4] =
      (video.screens[3] = 
         (video.screens[2] = 
            (video.screens[1] = s = calloc(size,4)) + size) + size) + size;
#else
   // haleyjd 05/30/08: removed screens from zone heap
   if(s)
   {
      Z_SysFree(s);
      I_UnsetPrimaryBuffer();
   }

   video.screens[4] =
      (video.screens[3] =
         (video.screens[2] =
            (video.screens[1] = s = Z_SysCalloc(size, 4)) + size) + size) + size;
#endif
   
#ifdef DJGPP
   screens0_bitmap = create_bitmap_ex(8, video.width, video.height);
   memset(video.screens[0] = screens0_bitmap->line[0], 0, size);
   video.pitch = video.width;
#else
   // SoM: TODO: implement direct to SDL surface drawing.
   I_SetPrimaryBuffer();
#endif

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
//
void V_DrawBackground(const char *patchname, VBuffer *back_dest)
{
   byte *src = W_CacheLumpNum(firstflat + R_FlatNumForName(patchname),
                              PU_CACHE);

   back_dest->TileBlock64(back_dest, src);
}

byte *R_DistortedFlat(int);

//
// V_DrawDistortedBackground
//
// As above, but uses the ultra-cool water warping effect.
// Created by fraggle.
//
void V_DrawDistortedBackground(const char *patchname, VBuffer *back_dest)
{
   byte *src = R_DistortedFlat(R_FlatNumForName(patchname));
   
   back_dest->TileBlock64(back_dest, src);
}

////////////////////////////////////////////////////////////////////////////
//
// Init
//

void V_InitMisc(void)
{
   V_InitBox();

   // this only ever needs to be done once
   if(!flexTranInit)
   {
      byte *palette = W_CacheLumpName("PLAYPAL", PU_STATIC);
      V_InitFlexTranTable(palette);
      Z_ChangeTag(palette, PU_CACHE);
   }
}

//////////////////////////////////////////////////////////////////////////
//
// Console Commands
//

VARIABLE_INT(v_mode,   NULL, 0, 11, NULL);

char *str_ticker[] = { "off", "chart", "classic", "text" };
VARIABLE_INT(v_ticker, NULL, 0, 3,  str_ticker);

CONSOLE_VARIABLE(v_mode, v_mode, cf_buffered)
{
   V_ResetMode();
}

CONSOLE_COMMAND(v_modelist, 0)
{
   videomode_t* videomode = videomodes;
   
   C_Printf(FC_HI "video modes:\n");
   
   while(videomode->description)
   {
      C_Printf("%i: %s\n",(int)(videomode-videomodes),
               videomode->description);
      ++videomode;
   }
}

CONSOLE_VARIABLE(v_ticker, v_ticker, 0) {}

void V_AddCommands(void)
{
   C_AddCommand(v_mode);
   C_AddCommand(v_modelist);
   C_AddCommand(v_ticker);
}

// EOF
