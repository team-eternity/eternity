// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright (C) 2017 James Haley, Max Waine, et al.
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
//    Implementation of SDL classes for Gamepads and Joysticks
//
//-----------------------------------------------------------------------------

#include "SDL.h"

#include "../z_zone.h"

#include "../g_bind.h"
#include "i_sdlgamepads.h"

// Module-private globals

// SDL_Joystick structure for the currently open joystick device (if any).
// Singleton resource.
static SDL_Joystick *joystick;

// Index of active SDL joystick structure. -1 while not valid.
static int activeIdx = -1;

//=============================================================================
//
// SDLGamePadDriver
//
// Implements support for SDL MMSYSTEM gamepad devices.
//

//
// SDLGamePadDriver::initialize
//
// Start up the SDL gamepad driver.
//
bool SDLGamePadDriver::initialize()
{
   return true;
}

//
// SDLGamePadDriver::shutdown
//
// Perform shutdown actions for SDL joystick support.
//
void SDLGamePadDriver::shutdown()
{
   // if the joystick is still active, shut it down.
   if(joystick)
   {
      SDL_JoystickClose(joystick);
      joystick = NULL;
   }
}

//
// SDLGamePadDriver::enumerateDevices
//
// Instantiate SDLGamePad objects for all supported joystick
// devices.
//
void SDLGamePadDriver::enumerateDevices()
{
   int numpads = SDL_NumJoysticks();
   SDLGamePad *sdlDev;

   for(int i = 0; i < numpads; i++)
   {
      sdlDev = new SDLGamePad(i);
      addDevice(sdlDev);
   }
}

// The one and only instance of SDLGamePadDriver
SDLGamePadDriver i_sdlGamePadDriver;

//=============================================================================
//
// SDLGamePad Methods
//

IMPLEMENT_RTTI_TYPE(SDLGamePad)

// 
// Constructor
//
SDLGamePad::SDLGamePad(int idx) 
   : Super(), sdlIndex(idx)
{
   name << "SDL " << SDL_JoystickNameForIndex(sdlIndex);
   num = i_sdlGamePadDriver.getBaseDeviceNum() + sdlIndex;
}

//
// SDLGamePad::select
//
// Select this gamepad as the active input device for the game engine.
//
bool SDLGamePad::select()
{
   if(joystick) // one is still open? (should not happen)
      return false;

   // remember who is in use internally
   activeIdx = sdlIndex;

   if((joystick = SDL_JoystickOpen(sdlIndex)) != NULL)
   {
      numAxes    = SDL_JoystickNumAxes(joystick);
      numButtons = SDL_JoystickNumButtons(joystick);
      numHats    = SDL_JoystickNumHats(joystick);
      return true;
   }
   else
      return false;
}

//
// SDLGamePad::deselect
//
// Remove this joystick from its status as the active input device.
//
void SDLGamePad::deselect()
{
   if(activeIdx != sdlIndex) // should not happen
      return;

   if(joystick)
   {
      SDL_JoystickClose(joystick);
      joystick  = NULL;
      activeIdx = -1;
   }
}

//
// SDLGamePad::poll
//
// Refresh input state data by polling the device.
//
void SDLGamePad::poll()
{
   SDL_JoystickUpdate();

   // save old button and axis states
   backupState();

   // get button states
   for(int i = 0; i < numButtons && i < MAXBUTTONS; i++)
      state.buttons[i] = !!SDL_JoystickGetButton(joystick, i);
   for(int i = 0; i < numHats && i < MAXHATS; i++)
   {
      Uint8 sdlhat = SDL_JoystickGetHat(joystick, i);
      state.hats[i] = 0;
      if(sdlhat & SDL_HAT_RIGHT)
         state.hats[i] |= HAT_RIGHT;
      if(sdlhat & SDL_HAT_UP)
         state.hats[i] |= HAT_UP;
      if(sdlhat & SDL_HAT_LEFT)
         state.hats[i] |= HAT_LEFT;
      if(sdlhat & SDL_HAT_DOWN)
         state.hats[i] |= HAT_DOWN;
   }

   // get axis states
   for(int i = 0; i < numAxes && i < MAXAXES; i++)
   {
      Sint16 val = SDL_JoystickGetAxis(joystick, i);
      int threshold = int(round(axisDeadZone[i] * 32767));

      if(val > threshold || val < -threshold)
      {
         // unbias on the low end, so that +32767 means 1.0
         if(val == -32768)
            val = -32767;

         state.axes[i] = static_cast<float>(val) / 32767.0f;
      }
      else
         state.axes[i] = 0.0f;
   }
}

// EOF

