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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:  
//    SDL Timer Implementation
//
//-----------------------------------------------------------------------------

#include "SDL.h"

#include "../z_zone.h"

// Need timer HAL
#include "../hal/i_timer.h"

#include "../doomdef.h"
#include "../doomstat.h"
#include "../m_compare.h"

//=============================================================================
//
// I_GetTime
// Most of the following has been rewritten by Lee Killough
//

static Uint32 basetime = 0;

//
// I_SDLGetTime_RealTime
//
static int I_SDLGetTime_RealTime()
{
   Uint32 t = SDL_GetTicks();

   // e6y: removing startup delay
   if(!basetime)
      basetime = t;

   // milliseconds since SDL initialization
   return (int)(((t - basetime) * TICRATE) / 1000);
}

//
// I_SDLGetTime_Scaled
//
static int I_SDLGetTime_Scaled()
{
   return (int)(((int64_t)I_SDLGetTime_RealTime() * I_GetTime_Scale) >> CLOCK_BITS);
}

//
// I_SDLGetTime_FastDemo
//
static int I_SDLGetTime_FastDemo()
{
   static int fasttic;
   return fasttic++;
}

//
// I_SDLGetTicks
//
// haleyjd 10/26/08: Return time in milliseconds
//
static unsigned int I_SDLGetTicks()
{
   return SDL_GetTicks();
}

//
// I_SDLSleep
//
// haleyjd: routine to sleep a fixed number of milliseconds.
//
static void I_SDLSleep(int ms)
{
   SDL_Delay(ms);
}

//=============================================================================
//
// Interpolation
//

static unsigned int start_displaytime;
static unsigned int displaytime;

static unsigned int rendertic_start;
static unsigned int rendertic_step;
static unsigned int rendertic_next;
static float        rendertic_msec;

//
// I_SDLSetMSec
//
// Module private.
// Set the value for milliseconds per render frame.
//
static void I_SDLSetMSec()
{
   rendertic_msec = (float)realtic_clock_rate * TICRATE / 100000.0f;
}

//
// I_SDLGetTimeFrac
//
// Calculate the fractional multiplier for interpolating the current frame.
//
static fixed_t I_SDLGetTimeFrac()
{
   fixed_t frac = FRACUNIT;

   if(!singletics && rendertic_step != 0)
   {
      unsigned int now = SDL_GetTicks();
      frac = (fixed_t)((now - rendertic_start + displaytime) * FRACUNIT / rendertic_step);
      frac = eclamp(frac, 0, FRACUNIT);
   }

   return frac;
}

//
// I_SDLStartDisplay
//
// Calculate the starting display time.
//
static void I_SDLStartDisplay()
{
   start_displaytime = SDL_GetTicks();
}

//
// I_SDLEndDisplay
//
// Calculate the ending display time.
//
static void I_SDLEndDisplay()
{
   displaytime = SDL_GetTicks() - start_displaytime;
}

//
// I_SDLSaveMS
//
// Update interpolation state variables at the end of gamesim logic.
//
static void I_SDLSaveMS()
{
   rendertic_start = SDL_GetTicks();
   rendertic_next  = (unsigned int)((rendertic_start * rendertic_msec + 1.0f) / rendertic_msec);
   rendertic_step  = rendertic_next - rendertic_start;
}

//=============================================================================
//
// Global Interface
//

//
// I_SDLInitTimer
//
// Initialize SDL timing.
//
void I_SDLInitTimer()
{
   // initialize GetTime, which gets time in gametics
   // killough 4/14/98: Adjustable speedup based on realtic_clock_rate
   if(fastdemo)
      i_haltimer.GetTime = I_SDLGetTime_FastDemo;
   else
   {
      if(I_GetTime_Scale != CLOCK_UNIT)
         i_haltimer.GetTime = I_SDLGetTime_Scaled;
      else
         i_haltimer.GetTime = I_SDLGetTime_RealTime;
   }

   I_SDLSetMSec();

   // initialize constant methods
   i_haltimer.GetRealTime  = I_SDLGetTime_RealTime;
   i_haltimer.GetTicks     = I_SDLGetTicks;
   i_haltimer.Sleep        = I_SDLSleep;
   i_haltimer.StartDisplay = I_SDLStartDisplay;
   i_haltimer.EndDisplay   = I_SDLEndDisplay;
   i_haltimer.GetFrac      = I_SDLGetTimeFrac;
   i_haltimer.SaveMS       = I_SDLSaveMS;
}

//
// I_SDLChangeClockRate
//
// Change the clock rate at runtime.
//
void I_SDLChangeClockRate()
{
   if(fastdemo)
      return;

   if(I_GetTime_Scale != CLOCK_UNIT)
      i_haltimer.GetTime = I_SDLGetTime_Scaled;
   else
      i_haltimer.GetTime = I_SDLGetTime_RealTime;

   I_SDLSetMSec();
}

// EOF

