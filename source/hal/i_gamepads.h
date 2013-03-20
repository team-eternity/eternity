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
//    Hardware Abstraction Layer for Gamepads and Joysticks
//
//-----------------------------------------------------------------------------

#ifndef I_GAMEPADS_H__
#define I_GAMEPADS_H__

#include "../e_rtti.h"
#include "../m_collection.h"
#include "../m_qstr.h"

class HALGamePad;

// Joystick device number, for config file
extern int i_joysticknum;

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
// HALGamePad
//
// Base class for gamepad devices.
//
class HALGamePad : public RTTIObject
{
   DECLARE_ABSTRACT_TYPE(HALGamePad, RTTIObject)

public:
   HALGamePad() : Super(), num(-1), name(), numAxes(0), numButtons(0), state() {}

   // Selection
   virtual bool select()   = 0; // Select as the input device
   virtual void deselect() = 0; // Deselect from input device status
   
   // Input
   virtual void poll() = 0;     // Refresh all input state data

   // Data
   int     num;         // Device number
   qstring name;        // Device name
   int     numAxes;     // Number of axes supported
   int     numButtons;  // Number of buttons supported

   enum
   {
      // In interest of efficiency, we have caps on the number of device inputs
      // we will monitor.
      MAXAXES = 8,
      MAXBUTTONS = 16
   };

   struct padstate_t
   {
      bool  prevbuttons[MAXBUTTONS]; // backed-up previous button states
      bool  buttons[MAXBUTTONS];     // current button states
      float axes[MAXAXES];           // normalized axis states (-1.0 : 1.0)
   };
   padstate_t state;
};

// Global interface

bool I_SelectDefaultGamePad();
void I_InitGamePads();
void I_ShutdownGamePads();
HALGamePad::padstate_t *I_PollActiveGamePad();

#endif

// EOF

