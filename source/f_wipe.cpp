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
// Mission begin melt/wipe screen special effect.
//
// Rewritten by Simon Howard
// Portions which deal with the movement of the columns adapted
// from the original sources
//
//-----------------------------------------------------------------------------

// 13/12/99: restored movement of columns to being the same as in the
// original, while retaining the new 'engine'

#include "z_zone.h"

#include "c_runcmd.h"
#include "doomdef.h"
#include "d_main.h"
#include "f_wipe.h"
#include "i_video.h"
#include "m_random.h"
#include "v_alloc.h"
#include "v_misc.h"
#include "v_video.h"

// common globals
bool inwipe = false;
int wipetype;

// common statics
static int current_wipetype;
static byte *wipe_buffer = NULL;

//==============================================================================
//
// haleyjd 10/12/08: melt wipe
//
// Separated from main functions below to allow for multiple wipe routines.
//

// array of column pointers into wipe_buffer for 'superfast' melt
static byte **start_screen;
VALLOCATION(start_screen)
{
   start_screen = ecalloctag(byte **, w, sizeof(byte *), PU_VALLOC, NULL);
}

// y co-ordinate of various columns
static int worms[SCREENWIDTH];

static int starting_height;

static void Wipe_meltStartScreen(void)
{
   int x, y;
   byte *dest, *src;

   // SoM 2-4-04: ANYRES
   // use console height
   // SoM: wtf? Why did I scale this before??? This should be within the 320x200
   // space unscaled!
   starting_height = Console.current_height;
   
   worms[0] = starting_height - (M_Random() & 15);
   
   for(x = 1; x < SCREENWIDTH; ++x)
   {
      int r = (M_Random() % 3) - 1;
      worms[x] = worms[x-1] + r;
      if(worms[x] > 0)
         worms[x] = 0;
      else if(worms[x] == -16)
         worms[x] = -15;
   }

   for(x = 0; x < video.width; ++x)
      start_screen[x] = wipe_buffer + (x * video.height);

   // SoM 2-4-04: ANYRES
   for(x = 0; x < video.width; ++x)
   {
      // limit check
      int wormx = (x << FRACBITS) / video.xscale;
      int wormy = video.y1lookup[worms[wormx] > 0 ? worms[wormx] : 0];
      
      src = vbscreen.data + x;
      dest = start_screen[x];
      
      for(y = 0; y < video.height - wormy; y++)
      {
         *dest = *src;
         src += vbscreen.pitch;
         dest++;
      }
   }
}

static void Wipe_meltDrawer(void)
{
   int x, y;
   byte *dest, *src;

   // SoM 2-4-04: ANYRES
   for(x = 0; x < video.width; ++x)
   {
      int wormy, wormx;
      
      wormx = (x << FRACBITS) / video.xscale;
      wormy = worms[wormx] > 0 ? worms[wormx] : 0;  // limit check

      wormy = video.y1lookup[wormy];

      src = start_screen[x];
      dest = vbscreen.data + vbscreen.pitch * wormy + x;
      
      for(y = video.height - wormy; y--;)
      {
         *dest = *src++;
         dest += vbscreen.pitch;
      }
   }
}

static bool Wipe_meltTicker(void)
{
   bool done;

   done = true;  // default to true

   // SoM 2-4-04: ANYRES
   for(int &worm : worms)
   {
      if(worm < 0)
      {
         ++worm;
         done = false;
      }
      else if(worm < SCREENHEIGHT)
      {
         int dy;

         dy = (worm < 16) ? worm + 1 : 8;

         if(worm + dy >= SCREENHEIGHT)
            dy = SCREENHEIGHT - worm;
         worm += dy;
         done = false;
      }
   }
   return done;
}

//==============================================================================
//
// haleyjd 10/12/08: crossfade wipe reimplementation
//
// Vanilla Doom had an attempt at this, but it was not being done in a way that
// would actually work in 8-bit, and so it was left disabled in the commercial
// releases. This provided direct inspiration for Rogue during Strife 
// development, however; the hub transition crossfade wipe used there was
// implemented directly on top of the unfinished code according to the
// disassembly. This wipe might also be useful for Heretic and Hexen, where
// it would be more fitting than Doom's melt wipe (those games used no wipe by
// default, which to me is odd).
//

// SoM: Wait, why is this not defined in the header where Col2RGB8 is defined?!
#define MAXFADE 64 // there are 64 levels in the Col2RGB8 table
static int fadelvl;

static void Wipe_fadeStartScreen(void)
{
   fadelvl = 0;
   I_ReadScreen(wipe_buffer);
}

static void Wipe_fadeDrawer(void)
{
   if(fadelvl <= MAXFADE)
   {
      byte *src, *dest;
      unsigned int *fg2rgb = Col2RGB8[fadelvl];
      unsigned int *bg2rgb = Col2RGB8[MAXFADE - fadelvl];
      int x, y;

      src = wipe_buffer;

      for(y = 0; y < vbscreen.height; ++y)
      {
         dest = vbscreen.data + y * vbscreen.pitch;

         for(x = 0; x < vbscreen.width; ++x)
         {
            unsigned int fg, bg;
            
            fg = fg2rgb[*dest];
            bg = bg2rgb[*src++];
            fg = (fg + bg) | 0x1f07c1f;
            *dest++ = RGB32k[0][0][fg & (fg >> 15)];
         }
      }
   }
}

static bool Wipe_fadeTicker(void)
{
   fadelvl += 2;

   return fadelvl > MAXFADE;
}


//==============================================================================
//
// Wipe Objects
//

struct fwipe_t
{
   void (*StartScreen)(void);
   void (*Drawer)(void);
   bool (*Ticker)(void);
};

static fwipe_t wipers[] =
{
   // none
   { NULL, NULL, NULL },

   // melt wipe
   {
      Wipe_meltStartScreen,
      Wipe_meltDrawer,
      Wipe_meltTicker,
   },

   // crossfade wipe
   {
      Wipe_fadeStartScreen,
      Wipe_fadeDrawer,
      Wipe_fadeTicker,
   },
};

//==============================================================================
//
// Core Wipe Routines - External Interface
//

void Wipe_StartScreen(void)
{
   if(wipetype == 0)
      return;

   inwipe = true;

   // echo wipetype to current_wipetype so that configuration changes do not
   // occur during a non-blocking screenwipe (would be disasterous)
   current_wipetype = wipetype;

   if(!wipe_buffer)
   {
      // SoM: Reformatted and cleaned up (ANYRES)
      // haleyjd: make purgable, allocate at required size
      wipe_buffer = (byte *)(Z_Malloc(video.height * video.width, PU_STATIC, 
                                      (void **)&wipe_buffer));
   }
   else
      Z_ChangeTag(wipe_buffer, PU_STATIC); // buffer is in use


   wipers[current_wipetype].StartScreen();
}

//
// Wipe_SaveEndScreen
//
void Wipe_SaveEndScreen(void)
{
   V_BlitVBuffer(&backscreen3, 0, 0, &vbscreen, 0, 0, video.width, video.height);
}

//
// Wipe_BlitEndScreen
//
void Wipe_BlitEndScreen(void)
{
   V_BlitVBuffer(&vbscreen, 0, 0, &backscreen3, 0, 0, video.width, video.height);
}

//
// Wipe_ScreenReset
//
// Must be called when the screen resolution changes.
//
void Wipe_ScreenReset(void)
{
   // if a buffer exists, destroy it
   if(wipe_buffer)
   {
      Z_Free(wipe_buffer);
      wipe_buffer = NULL;
   }

   // cancel any current wipe (screen contents have been lost)
   inwipe = false;
}

void Wipe_Drawer(void)
{
   if(!inwipe)
      return;

   wipers[current_wipetype].Drawer();
}

void Wipe_Ticker(void)
{
   bool done = wipers[current_wipetype].Ticker();

   if(done)
   {
      inwipe = false;
      Z_ChangeTag(wipe_buffer, PU_CACHE); // haleyjd: make purgable
   }
}

// EOF

