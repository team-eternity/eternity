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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:  
//    SDL Timer Implementation
//
// Some of this code is taken from Woof!'s i_timer.c, authored by Roman Fomin
// and Fabian Greffrath, and used under terms of the GPLv2+.
//
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

static Uint64 basecounter = 0;
static Uint64 basefreq    = 0;

static int MSToTic(uint32_t time)
{
   return time * TICRATE / 1000;
}

// FIXME: unused
/*
static uint64_t TicToCounter(int tic)
{
   return uint64_t(tic) * basefreq / TICRATE;
}
*/

//
// I_SDLGetTicks
//
// haleyjd 10/26/08: Return time in milliseconds
//
static unsigned int I_SDLGetTicks()
{
   const Uint64 counter = SDL_GetPerformanceCounter();

   if(basecounter == 0)
      basecounter = counter;

   return int(((counter - basecounter) * 1000ull) / basefreq);
}

static int time_scale = 100;

//
// Gets the current performance counter, but for scaled time
//
static uint64_t I_SDLGetPerfCounter_Scaled()
{
   uint64_t counter;

   counter = SDL_GetPerformanceCounter() * time_scale / 100;

   if(basecounter == 0)
      basecounter = counter;

   return counter - basecounter;
}

//
// As I_SDLGetTicks but for scaled time
//
static unsigned int I_SDLGetTicks_Scaled()
{
   const Uint64 counter = SDL_GetPerformanceCounter() * time_scale / 100;

   if(basecounter == 0)
      basecounter = counter;

   return int(((counter - basecounter) * 1000ull) / basefreq);
}

//
// I_SDLGetTime_RealTime
//
static int I_SDLGetTime_RealTime()
{
   return MSToTic(I_SDLGetTicks());
}

//
// I_SDLGetTime_Scaled
//
static int I_SDLGetTime_Scaled()
{
   return MSToTic(I_SDLGetTicks_Scaled());
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
   rendertic_msec = (float)time_scale * TICRATE / 100000.0f;
}

//
// I_SDLGetTimeFrac
//
// Calculate the fractional multiplier for interpolating the current frame.
//
static fixed_t I_SDLGetTimeFrac()
{
   return I_SDLGetTicks_Scaled() * TICRATE % 1000 * FRACUNIT / 1000;
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
   basefreq   = SDL_GetPerformanceFrequency();
   time_scale = realtic_clock_rate;

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

   const uint64_t counter = I_SDLGetPerfCounter_Scaled();
   time_scale = realtic_clock_rate;
   basecounter += (I_SDLGetPerfCounter_Scaled() - counter);

   if(I_GetTime_Scale != CLOCK_UNIT)
      i_haltimer.GetTime = I_SDLGetTime_Scaled;
   else
      i_haltimer.GetTime = I_SDLGetTime_RealTime;

   I_SDLSetMSec();
}

// EOF

