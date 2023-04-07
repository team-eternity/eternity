//
// The Eternity Engine
// Copyright (C) 2023 James Haley, Max Waine, et al.
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
// Purpose:Implementation of SDL classes for Gamepads and Joysticks
// Authors: James Haley, Max Waine
//

#ifdef __APPLE__
#include "SDL2/SDL.h"
#include "SDL2/SDL_gamecontroller.h"
#else
#include "SDL.h"
#include "SDL_gamecontroller.h"
#endif

#include "../z_zone.h"
#include "../doomdef.h"

#include "i_sdlgamepads.h"

// Assertions
static_assert(HALGamePad::MAXBUTTONS >= SDL_CONTROLLER_BUTTON_MAX);
static_assert(HALGamePad::MAXAXES >= SDL_CONTROLLER_AXIS_MAX);

// Module-private globals

// SDL_GameController structure for the currently open GameController device (if any).
// Singleton resource.
static SDL_GameController *gamecontroller;

// Index of active SDL GameController structure. -1 while not valid.
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
// Perform shutdown actions for SDL GameController support.
//
void SDLGamePadDriver::shutdown()
{
   // if the GameController is still active, shut it down.
   if(gamecontroller)
   {
      SDL_GameControllerClose(gamecontroller);
      gamecontroller = nullptr;
   }
}

//
// SDLGamePadDriver::enumerateDevices
//
// Instantiate SDLGamePad objects for all supported GameController
// devices.
//
void SDLGamePadDriver::enumerateDevices()
{
   int numpads = 0;
   SDLGamePad *sdlDev;

   for(int i = 0; i < SDL_NumJoysticks(); i++)
   {
      // Use only valid gamepads
      if(SDL_IsGameController(i))
         numpads++;
   }

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
   name << "SDL " << SDL_GameControllerNameForIndex(sdlIndex);
   num = i_sdlGamePadDriver.getBaseDeviceNum() + sdlIndex;
}

//
// SDLGamePad::select
//
// Select this gamepad as the active input device for the game engine.
//
bool SDLGamePad::select()
{
   if(gamecontroller) // one is still open? (should not happen)
      return false;

   // remember who is in use internally
   activeIdx = sdlIndex;

   if((gamecontroller = SDL_GameControllerOpen(sdlIndex)) != nullptr)
   {
      numAxes    = SDL_CONTROLLER_AXIS_MAX;
      numButtons = SDL_CONTROLLER_BUTTON_MAX;
      return true;
   }
   else
      return false;
}

//
// SDLGamePad::deselect
//
// Remove this GameController from its status as the active input device.
//
void SDLGamePad::deselect()
{
   if(activeIdx != sdlIndex) // should not happen
      return;

   if(gamecontroller)
   {
      SDL_GameControllerClose(gamecontroller);
      gamecontroller = nullptr;
      activeIdx      = -1;
   }
}

//
// SDLGamePad::poll
//
// Refresh input state data by polling the device.
//
void SDLGamePad::poll()
{
   SDL_GameControllerUpdate();

   // save old button and axis states
   backupState();

   // get button states
   for(int i = 0; i < numButtons && i < SDL_CONTROLLER_BUTTON_MAX; i++)
      state.buttons[i] = !!SDL_GameControllerGetButton(gamecontroller, SDL_GameControllerButton(i));

   // get axis states
   for(int i = 0; i < numAxes && i < SDL_CONTROLLER_AXIS_MAX; i++)
   {
      Sint16 val = SDL_GameControllerGetAxis(gamecontroller, SDL_GameControllerAxis(i));

      if(val > i_joysticksens || val < -i_joysticksens)
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

