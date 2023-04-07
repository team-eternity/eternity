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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
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
// Implements the abstract HAL gamepad/joystick interface for SDL's MMSYSTEM
// based gamepad support.
//
class SDLGamePadDriver : public HALGamePadDriver
{
public:
   virtual bool initialize();
   virtual void shutdown();
   virtual void enumerateDevices();
   virtual int  getBaseDeviceNum() { return 0x10000; }
};

extern SDLGamePadDriver i_sdlGamePadDriver;

//
// Exposes support for force feedback effects through SDL gamepads.
//
class SDLHapticInterface : public HALHapticInterface
{
   DECLARE_RTTI_TYPE(SDLHapticInterface, HALHapticInterface)

protected:
   bool pauseState;

   void zeroState();

public:
   SDLHapticInterface();
   virtual void startEffect(effect_e effect, int data1, int data2);
   virtual void pauseEffects(bool effectsPaused);
   virtual void updateEffects();
   virtual void clearEffects();
};

//
// SDLGamePad
//
// Implements the abstract HAL gamepad class, for devices that are driven by
// the SDL DirectInput driver.
//
class SDLGamePad : public HALGamePad
{
   DECLARE_RTTI_TYPE(SDLGamePad, HALGamePad)

protected:
   int sdlIndex; // SDL gamepad number
   SDLHapticInterface haptics;

public:
   SDLGamePad(int idx = 0);

   virtual bool select() override;
   virtual void deselect() override;
   virtual void poll() override;

   virtual HALHapticInterface *getHapticInterface() { return &haptics; }
};

#endif

// EOF

