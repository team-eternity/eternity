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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// XInput Gamepad Support
//
// By James Haley
//
//-----------------------------------------------------------------------------

#ifndef I_XINPUT_H__
#define I_XINPUT_H__

#ifdef EE_FEATURE_XINPUT

#include "../hal/i_gamepads.h"

//
// XInputGamePadDriver
//
// Implements support for XBox 360 controller and compatible devices through
// their native interface.
//
class XInputGamePadDriver : public HALGamePadDriver
{
public:
   virtual bool initialize();
   virtual void shutdown();
   virtual void enumerateDevices();
   virtual int  getBaseDeviceNum() { return 0x10000; }
};

extern XInputGamePadDriver i_xinputGamePadDriver;

//
// XInputHapticInterface
//
// Exposes support for force feedback effects through XInput gamepads.
//
class XInputHapticInterface : public HALHapticInterface
{
   DECLARE_RTTI_TYPE(XInputHapticInterface, HALHapticInterface)

protected:
   unsigned long dwUserIndex;
   bool pauseState;

   void zeroState();

public:
   XInputHapticInterface(unsigned long userIdx = 0);
   virtual void startEffect(effect_e effect, int data1, int data2);
   virtual void pauseEffects(bool effectsPaused);
   virtual void updateEffects();
   virtual void clearEffects();
};

//
// XInputGamePad
//
// Represents an actual XInput device.
//
class XInputGamePad : public HALGamePad
{
   DECLARE_RTTI_TYPE(XInputGamePad, HALGamePad)

protected:
   unsigned long dwUserIndex;
   XInputHapticInterface haptics;

   float normAxis(int value, int threshold, int maxvalue);
   void  normAxisPair(float &axisx, float &axisy, int threshold, int min, int max);

   static inline constexpr const char *stringForAxis[6] =
   {
      "Left X",
      "Left Y",
      "Right X",
      "Right Y",
      "Left Trigger",
      "Right Trigger",
   };

public:
   XInputGamePad(unsigned long userIdx = 0);

   virtual bool select();
   virtual void deselect();
   virtual void poll();

   virtual HALHapticInterface *getHapticInterface() { return &haptics; }

   //
   // Gets a name for an axis (if possible)
   //
   virtual const char *getAxisName(const int axis) override
   {
      if(axis >= 0 && axis < 6)
         return stringForAxis[axis];
      else
         return Super::getAxisName(axis);
   }
};

#endif

#endif

// EOF

