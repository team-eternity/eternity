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
   // milliseconds since SDL initialization
   return ((SDL_GetTicks() - basetime) * TICRATE) / 1000;
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
void I_SDLSleep(int ms)
{
   SDL_Delay(ms);
}

//
// I_SDLInitTimer
//
// Initialize SDL timing.
//
void I_SDLInitTimer()
{
   basetime = SDL_GetTicks();

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

   // initialize constant methods
   i_haltimer.GetRealTime = I_SDLGetTime_RealTime;
   i_haltimer.GetTicks    = I_SDLGetTicks;
   i_haltimer.Sleep       = I_SDLSleep;
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
}

// EOF

