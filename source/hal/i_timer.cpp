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
// Purpose: Hardware abstraction layer for timing.
// Authors: James Haley
//

#include "../z_zone.h"
#include "../c_runcmd.h"
#include "../i_system.h"
#include "../m_argv.h"

#include "i_timer.h"

// drivers
#ifdef _SDL_VER
#include "../sdl/i_sdltimer.h"
#endif

// Singleton instance of HALTimer
HALTimer i_haltimer;

// killough 4/13/98: Make clock rate adjustable by scale factor
int     realtic_clock_rate = 100;
int64_t I_GetTime_Scale    = CLOCK_UNIT;

static void I_HALTimerError()
{
    I_Error("I_HALTimerError: no timer driver is available.\n");
}

//=============================================================================
//
// Global Interface
//

//
// HAL Timer Driver Struct
//
struct haltimerdriveritem_t
{
    int                     id;         // HAL driver ID number
    const char             *name;       // name of driver
    HAL_TimerInitFunc       Init;       // pointer to driver init routine, if supported
    HAL_ChangeClockRateFunc ChangeRate; // runtime rate change function
};

static haltimerdriveritem_t *timer;

// clang-format off

static haltimerdriveritem_t halTimerDrivers[] =
{
    // SDL Timer Driver
    {
        0, 
        "SDL Timer",
#ifdef _SDL_VER
        I_SDLInitTimer,
        I_SDLChangeClockRate
#else
        nullptr,
        nullptr
#endif
    },

   // Dummy
    {
       1,
        "No Timer",
        I_HALTimerError
    }
};

// clang-format on

//
// I_InitHALTimer
//
// Initialize the timer subsystem.
//
void I_InitHALTimer()
{
    int clockRate = realtic_clock_rate;
    int p;

    if((p = M_CheckParm("-speed")) && p < myargc - 1 && (p = atoi(myargv[p + 1])) >= 10 && p <= 1000)
        clockRate = p;

    if(clockRate != 100)
        I_GetTime_Scale = ((int64_t)clockRate << CLOCK_BITS) / 100;

    // choose the first available timer driver
    for(size_t i = 0; i < earrlen(halTimerDrivers); i++)
    {
        if(halTimerDrivers[i].Init)
        {
            timer = &halTimerDrivers[i];
            break;
        }
    }

    // initialize the timer
    timer->Init();
}

VARIABLE_INT(realtic_clock_rate, nullptr, 0, 500, nullptr);
CONSOLE_VARIABLE(i_gamespeed, realtic_clock_rate, 0)
{
    if(realtic_clock_rate != 100)
        I_GetTime_Scale = ((int64_t)realtic_clock_rate << CLOCK_BITS) / 100;
    else
        I_GetTime_Scale = CLOCK_UNIT;

    timer->ChangeRate();
}

// EOF

