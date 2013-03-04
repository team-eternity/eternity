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

#ifndef I_SDLGAMEPADS_H__
#define I_SDLGAMEPADS_H__

// Need Gamepad/Joystick HAL
#include "../hal/i_gamepads.h"

//
// SDLGamePadDriver
//
// Implements the abstract HAL gamepad/joystick interface for SDL's DirectInput
// based gamepad support.
//
class SDLGamePadDriver : public HALGamePadDriver
{
protected:
   void buildDeviceName(int idx, qstring &out);

public:
   virtual bool initialize();
   virtual void shutdown();
   virtual void enumerateDevices();
};

extern SDLGamePadDriver i_sdlGamePadDriver;

//
// SDLGamePad
//
// Implements the abstract HAL gamepad class, for devices that are driven by
// the SDL DirectInput driver.
//
class SDLGamePad : public HALGamePad
{
   DECLARE_RTTI_TYPE(SDLGamePad, HALGamePad)

public:
   SDLGamePad();

   virtual bool select();
   virtual void deselect();

   int  sdlIndex; // SDL gamepad number
   bool active;
};

#endif

// EOF

