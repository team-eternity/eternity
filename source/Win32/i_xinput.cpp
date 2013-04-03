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
// XInput Gamepad Support
//
// By James Haley
//
//-----------------------------------------------------------------------------

#ifdef EE_FEATURE_XINPUT

#include "../z_zone.h"

#include "i_xinput.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <XInput.h>

//=============================================================================
//
// XInput API
//

// We load everything dynamically because XInput 9.1.0 is not available on
// Windows XP, as far as I'm aware.

typedef DWORD (WINAPI *PXINPUTGETSTATE)(DWORD, XINPUT_STATE *);
typedef DWORD (WINAPI *PXINPUTSETSTATE)(DWORD, XINPUT_VIBRATION *);
typedef DWORD (WINAPI *PXINPUTGETCAPS )(DWORD, XINPUT_CAPABILITIES *);

static PXINPUTGETSTATE pXInputGetState;
static PXINPUTSETSTATE pXInputSetState;
static PXINPUTGETCAPS  pXInputGetCapabilities;

//=============================================================================
//
// XInputGamePadDriver Class
//

#define LOADXINPUTAPI(func, type) \
   p ## func = reinterpret_cast<type>(GetProcAddress(pXInputLib, #func)); \
   if(!(p ## func)) \
      return false

// Driver-wide constants
// Only four devices are (currently?) supported via XInput. Some API :P
#define MAX_XINPUT_PADS 4

// Number of buttons and axes is predefined
#define MAX_XINPUT_BUTTONS 14
#define MAX_XINPUT_AXES    6

//
// XInputGamePadDriver::initialize
//
// Load the XInput library if it is available.
//
bool XInputGamePadDriver::initialize()
{
   HMODULE pXInputLib = LoadLibrary(XINPUT_DLL_A);

   if(!pXInputLib)
      return false;

   LOADXINPUTAPI(XInputGetState,        PXINPUTGETSTATE);
   LOADXINPUTAPI(XInputSetState,        PXINPUTSETSTATE);
   LOADXINPUTAPI(XInputGetCapabilities, PXINPUTGETCAPS );

   return true;
}

void XInputGamePadDriver::shutdown()
{
}
   
void XInputGamePadDriver::enumerateDevices()
{
   XINPUT_STATE testState;

   for(DWORD i = 0; i < MAX_XINPUT_PADS; i++)
   {
      memset(&testState, 0, sizeof(testState));

      if(!pXInputGetState(i, &testState))
      {
         XInputGamePad *pad = new XInputGamePad(i);
         addDevice(pad);
      }  
   }
}

// The one and only instance of XInputGamePadDriver
XInputGamePadDriver i_xinputGamePadDriver;

//=============================================================================
//
// XInputGamePad Class
//

IMPLEMENT_RTTI_TYPE(XInputGamePad)

//
// Constructor
//
XInputGamePad::XInputGamePad(unsigned long userIdx)
   : Super(), dwUserIndex(userIdx)
{
   int index = static_cast<int>(dwUserIndex);
   name << "XInput Gamepad " << index + 1;
   num = i_xinputGamePadDriver.getBaseDeviceNum() + index;

   // number of axes and buttons supported is constant
   numAxes    = MAX_XINPUT_AXES;
   numButtons = MAX_XINPUT_BUTTONS;
}

//
// XInputGamePad::select
//
bool XInputGamePad::select()
{
   // TODO
   return false;
}

//
// XInputGamePad::deselect
//
void XInputGamePad::deselect()
{
   // TODO
}

//
// XInputGamePad::poll
//
void XInputGamePad::poll()
{
   // TODO
}

#endif

// EOF

