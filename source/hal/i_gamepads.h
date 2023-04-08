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
//    Hardware Abstraction Layer for Gamepads and Joysticks
//
//-----------------------------------------------------------------------------

#ifndef I_GAMEPADS_H__
#define I_GAMEPADS_H__

#include "../e_rtti.h"
#include "../m_collection.h"
#include "../m_qstr.h"
#include "../psnprntf.h"

class HALGamePad;

extern bool i_joystickenabled; // Use joysticks at all
extern int  i_joysticknum;     // Joystick device number, for config file

// Joystick turning sensitvity
extern double i_joyturnsens;

// Generic sensitivity value, for drivers that need it
extern int i_joysticksens;

//
// HALGamePadDriver
//
// Base class for gamepad device drivers.
//
class HALGamePadDriver
{
protected:
   void addDevice(HALGamePad *device);

public:
   HALGamePadDriver() : devices() {}
   virtual ~HALGamePadDriver() {}

   // Gamepad Driver API
   virtual bool initialize()       = 0; // Perform any required startup init
   virtual void shutdown()         = 0; // Perform shutdown operations
   virtual void enumerateDevices() = 0; // Enumerate supported devices
   virtual int  getBaseDeviceNum() = 0; // Get base device number

   // Data
   PODCollection<HALGamePad *> devices;    // Supported devices
};

//
// HALHapticInterface
//
// A HALGamePad needs to expose one of these in order to support "force
// feedback" effects.
//
class HALHapticInterface : public RTTIObject
{
   DECLARE_ABSTRACT_TYPE(HALHapticInterface, RTTIObject)

public:
   HALHapticInterface() : Super() {}

   // Generic device-independent haptic effect names
   enum effect_e
   {
      EFFECT_DAMAGE,   // taking damage 
      EFFECT_FIRE,     // firing weapon
      EFFECT_IMPACT,   // hitting something (floor or wall)
      EFFECT_BUZZ,     // light shaking
      EFFECT_RUMBLE,   // earthquake or other heavy shaking
      EFFECT_CONSTANT, // constant effect
      EFFECT_RAMPUP,   // ramping up effect
      EFFECT_MAX
   };

   virtual void startEffect(effect_e effect, int data1, int data2) = 0;
   virtual void pauseEffects(bool effectsPaused) = 0;
   virtual void updateEffects() = 0;
   virtual void clearEffects()  = 0;
};

//
// HALGamePad
//
// Base class for gamepad devices.
//
class HALGamePad : public RTTIObject
{
   DECLARE_ABSTRACT_TYPE(HALGamePad, RTTIObject)

protected:
   void backupState();

public:
   HALGamePad();

   // In interest of efficiency, we have caps on the number of device inputs
   // we will monitor.
   static inline constexpr int MAXAXES = 8;
   static inline constexpr int MAXBUTTONS = 24;

   // Selection
   virtual bool select()   = 0; // Select as the input device
   virtual void deselect() = 0; // Deselect from input device status

   // Input
   virtual void poll() = 0;     // Refresh all input state data

   // Haptic interface
   virtual HALHapticInterface *getHapticInterface() { return nullptr; }

   // Data
   int     num;         // Device number
   qstring name;        // Device name
   int     numAxes;     // Number of axes supported
   int     numButtons;  // Number of buttons supported

   virtual const char *getAxisName(const int axis)
   {
      static char output[16];
      if(axis >= 0 && axis < MAXAXES)
         psnprintf(output, sizeof(output), "Axis %d", axis);
      else
         strncpy(output, "Invalid axis", 16);

      return output;
   };

   struct padstate_t
   {
      bool  prevbuttons[MAXBUTTONS]; // backed-up previous button states
      bool  buttons[MAXBUTTONS];     // current button states
      float prevaxes[MAXAXES];       // backed-up previous axis states
      float axes[MAXAXES];           // normalized axis states (-1.0 : 1.0)
   };
   padstate_t state;
};

// Global interface

typedef void (*gamePadChangeCallback_t)();

bool I_SelectDefaultGamePad();
void I_InitGamePads(gamePadChangeCallback_t callback);
void I_ShutdownGamePads();
HALGamePad::padstate_t *I_PollActiveGamePad();

size_t I_GetNumGamePads();
HALGamePad *I_GetGamePad(size_t index);
HALGamePad *I_GetActivePad();

// Haptics

// Enable or disable force feedback regardless of whether the device supports
// it or not.
extern bool i_forcefeedback;

// Start a haptic effect with an argument which may affect the strength or
// duration of the effect, depending on the type of effect being ordered.
void I_StartHaptic(HALHapticInterface::effect_e effect, int data1, int data2);

// Pause all running haptic effects
void I_PauseHaptics(bool effectsPaused);

// Update haptic effects
void I_UpdateHaptics();

// Clear haptic effects
void I_ClearHaptics();

#endif

// EOF

