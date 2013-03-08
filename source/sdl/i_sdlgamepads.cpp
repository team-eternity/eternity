// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:  
//    Implementation of SDL classes for Gamepads and Joysticks
//
//-----------------------------------------------------------------------------

#include "SDL.h"

#include "../z_zone.h"

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
// Implements support for SDL DirectInput gamepad devices.
//

//
// SDLGamePadDriver::buildDeviceName
//
// Protected method; builds the device name for an SDL gamepad.
//
void SDLGamePadDriver::buildDeviceName(int idx, qstring &out)
{
   out << "SDL " << SDL_JoystickName(idx);
}

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
   HALGamePad *dev;
   SDLGamePad *sdlDev;
   int numpads = SDL_NumJoysticks();

   // clear the active state of all existing devices
   PODCollection<HALGamePad *>::iterator itr;
   for(itr = devices.begin(); itr != devices.end(); itr++)
   {
      if((sdlDev = runtime_cast<SDLGamePad *>(*itr)))
      {
         sdlDev->sdlIndex = -1;
         sdlDev->active   = false;
      }
   }

   for(int i = 0; i < numpads; i++)
   {
      qstring name;
      buildDeviceName(i, name);

      if((dev = findGamePadNamed(name.constPtr())))
      {
         // already built; check for possible index change
         if((sdlDev = runtime_cast<SDLGamePad *>(dev)))
         {
            sdlDev->sdlIndex = i;
            sdlDev->active   = true; // reactivate it
         }
         continue;
      }

      // instantiate a new one
      sdlDev = new SDLGamePad();
      sdlDev->name     = name;
      sdlDev->sdlIndex = i;

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
SDLGamePad::SDLGamePad() 
   : Super(), sdlIndex(-1), active(true)
{
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

   if(!active || sdlIndex == -1) // not valid?
      return false;

   // remember who is in use internally
   activeIdx = sdlIndex;

   if((joystick = SDL_JoystickOpen(sdlIndex)) != NULL)
   {
      numAxes    = SDL_JoystickNumAxes(joystick);
      numButtons = SDL_JoystickNumButtons(joystick);
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
}

//
// SDLGamePad::buttonDown
//
// Test button state for the indicated button.
//
bool SDLGamePad::buttonDown(int buttonNum)
{
   return !!SDL_JoystickGetButton(joystick, buttonNum);
}

// EOF

