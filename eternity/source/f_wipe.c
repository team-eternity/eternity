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
// Mission begin melt/wipe screen special effect.
//
// Rewritten by Simon Howard
// Portions which deal with the movement of the columns adapted
// from the original sources
//
//-----------------------------------------------------------------------------

// 13/12/99: restored movement of columns to being the same as in the
// original, while retaining the new 'engine'

#include "c_io.h"
#include "doomdef.h"
#include "d_main.h"
#include "i_video.h"
#include "v_video.h"
#include "m_random.h"
#include "f_wipe.h"

// array of pointers to the
// column data for 'superfast' melt
static byte *start_screen[MAX_SCREENWIDTH] = {0};

// y co-ordinate of various columns
static int worms[SCREENWIDTH];

boolean        inwipe = false;
static int     starting_height;

static void Wipe_initWipe(void)
{
   int x;
   
   inwipe = true;
   
   // SoM 2-4-04: ANYRES
   // use console height
   // SoM: wtf? Why did I scale this before??? This should be within the 320x200
   // space unscaled!
   starting_height = current_height;
   
   worms[0] = starting_height - M_Random()%16;
   
   for(x = 1; x < SCREENWIDTH; ++x)
   {
      int r = (M_Random() % 3) - 1;
      worms[x] = worms[x-1] + r;
      if(worms[x] > 0)
         worms[x] = 0;
      else if(worms[x] == -16)
         worms[x] = -15;
   }
}

static byte *wipe_buffer;

void Wipe_StartScreen(void)
{
   register int x, y;
   register byte *dest, *src;

   Wipe_initWipe();
  
   if(!wipe_buffer)
   {
      // SoM: Reformatted and cleaned up (ANYRES)
      // haleyjd: make purgable, allocate at required size
      wipe_buffer = Z_Malloc(video.height * video.width, PU_STATIC, 
                             (void **)&wipe_buffer);
      
      for(x = 0; x < video.width; ++x)
         start_screen[x] = wipe_buffer + (x * video.height);
   }
   else
      Z_ChangeTag(wipe_buffer, PU_STATIC); // buffer is in use

   // SoM 2-4-04: ANYRES
   for(x = 0; x < video.width; ++x)
   {
      // limit check
      int wormx = (x << FRACBITS) / video.xscale;
      int wormy = video.y1lookup[worms[wormx] > 0 ? worms[wormx] : 0];
      
      src = video.screens[0] + x;
      dest = start_screen[x];
      
      for(y = 0; y < video.height - wormy; y++)
      {
         *dest = *src;
         src += video.width;
         dest++;
      }
   }
   
   return;
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
   register int x, y;
   register byte *dest, *src;

   // SoM 2-4-04: ANYRES
   for(x = 0; x < video.width; ++x)
   {
      int wormy, wormx;
      
      wormx = (x << FRACBITS) / video.xscale;
      wormy = worms[wormx] > 0 ? worms[wormx] : 0;  // limit check

      wormy = video.y1lookup[wormy];

      src = start_screen[x];
      dest = video.screens[0] + video.width * wormy + x;
      
      for(y = video.height - wormy; y--;)
      {
         *dest = *src++;
         dest += video.width;
      }
   }
 
   redrawsbar = true; // clean up status bar
}

void Wipe_Ticker(void)
{
   boolean done;
   int x;
  
   done = true;  // default to true

   // SoM 2-4-04: ANYRES
   for(x = 0; x < SCREENWIDTH; ++x)
   {
      if(worms[x] < 0)
      {
         ++worms[x];
         done = false;
      }
      else if(worms[x] < SCREENHEIGHT)
      {
         int dy;

         dy = (worms[x] < 16) ? worms[x] + 1 : 8;

         if(worms[x] + dy >= SCREENHEIGHT)
            dy = SCREENHEIGHT - worms[x];
         worms[x] += dy;
         done = false;
      }
   }
  
   if(done)
   {
      inwipe = false;
      Z_ChangeTag(wipe_buffer, PU_CACHE); // haleyjd: make purgable
   }
}

// EOF

