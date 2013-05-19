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

#include "../m_vector.h"
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
   XINPUT_STATE testState;

   memset(&testState, 0, sizeof(testState));

   return !pXInputGetState(dwUserIndex, &testState);
}

//
// XInputGamePad::deselect
//
void XInputGamePad::deselect()
{
   // Nothing to be done here.
}

struct buttonenum_t
{
   int xInputButton;
   int halButton;
};

static buttonenum_t buttonTable[] =
{
   { XINPUT_GAMEPAD_DPAD_UP,         0 },
   { XINPUT_GAMEPAD_DPAD_DOWN,       1 },
   { XINPUT_GAMEPAD_DPAD_LEFT,       2 },
   { XINPUT_GAMEPAD_DPAD_RIGHT,      3 },
   { XINPUT_GAMEPAD_START,           4 },
   { XINPUT_GAMEPAD_BACK,            5 },
   { XINPUT_GAMEPAD_LEFT_THUMB,      6 },
   { XINPUT_GAMEPAD_RIGHT_THUMB,     7 },
   { XINPUT_GAMEPAD_LEFT_SHOULDER,   8 },
   { XINPUT_GAMEPAD_RIGHT_SHOULDER,  9 },
   { XINPUT_GAMEPAD_A,              10 },
   { XINPUT_GAMEPAD_B,              11 },
   { XINPUT_GAMEPAD_X,              12 },
   { XINPUT_GAMEPAD_Y,              13 },
};

//
// XInputGamePad::normAxis
//
// Normalize an analog axis value after clipping to the minimum threshold value.
//
float XInputGamePad::normAxis(int value, int threshold, int maxvalue)
{
   if(abs(value) > threshold)
      return static_cast<float>(value) / maxvalue;
   else
      return 0.0f;
}

//
// XInputGamePad::normAxisPair
//
// Normalize a bonded pair of analog axes.
//
void XInputGamePad::normAxisPair(float &axisx, float &axisy, int threshold, 
                                 int min, int max)
{
   float deadzone = (float)threshold / max;
   v2float_t vec = { axisx, axisy };

   // put components into the range of -1.0 to 1.0
   vec.x = (vec.x - min) * 2.0f / (max - min) - 1.0f;
   vec.y = (vec.y - min) * 2.0f / (max - min) - 1.0f;

   float magnitude = M_MagnitudeVec2(vec);

   if(magnitude < deadzone)
   {
      axisx = 0.0f;
      axisy = 0.0f;
   }
   else
   {
      M_NormalizeVec2(vec, magnitude);

      // rescale to smooth edge of deadzone
      axisx = vec.x * ((magnitude - deadzone) / (1 - deadzone));
      axisy = vec.y * ((magnitude - deadzone) / (1 - deadzone));
   }
}

//
// XInputGamePad::poll
//
void XInputGamePad::poll()
{
   XINPUT_STATE xstate;
   XINPUT_GAMEPAD &pad = xstate.Gamepad;
   
   memset(&xstate, 0, sizeof(xstate));

   if(!pXInputGetState(dwUserIndex, &xstate))
   {
      // save old button and axis states
      backupState();

      // read button states
      for(size_t i = 0; i < earrlen(buttonTable); i++)
      {
         state.buttons[buttonTable[i].halButton] = 
            ((pad.wButtons & buttonTable[i].xInputButton) == buttonTable[i].xInputButton);
      }

      // read axis states
      
      state.axes[0] = pad.sThumbLX;
      state.axes[1] = pad.sThumbLY;
      state.axes[2] = pad.sThumbRX;
      state.axes[3] = pad.sThumbRY;

      normAxisPair(state.axes[0], state.axes[1], XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE,  -32768, 32767);
      normAxisPair(state.axes[2], state.axes[3], XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, -32768, 32767);
      
      state.axes[4] = normAxis(pad.bLeftTrigger,  XINPUT_GAMEPAD_TRIGGER_THRESHOLD, 255);
      state.axes[5] = normAxis(pad.bRightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD, 255);
   }
}

#endif

// EOF

