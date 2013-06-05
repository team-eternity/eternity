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

#include "../i_system.h"
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

//
// XInputGamePadDriver::shutdown
//
void XInputGamePadDriver::shutdown()
{
   for(auto it = devices.begin(); it != devices.end(); it++)
      (*it)->getHapticInterface()->clearEffects();
}

//
// XInputGamePadDriver::enumerateDevices
//
// Test connected state of all XInput devices and instantiate an instance of
// XInputGamePad for each one detected.
//
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
   : Super(), dwUserIndex(userIdx), haptics(userIdx)
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
   bool connected = false;

   memset(&testState, 0, sizeof(testState));

   if(!pXInputGetState(dwUserIndex, &testState))
   {
      haptics.clearEffects();
      connected = true;
   }

   return connected;
}

//
// XInputGamePad::deselect
//
void XInputGamePad::deselect()
{
   haptics.clearEffects();
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

//=============================================================================
//
// XInputHapticInterface
//
// Support for force feedback through the XInput API.
//

IMPLEMENT_RTTI_TYPE(XInputHapticInterface)

// motor types
enum motor_e
{
   MOTOR_LEFT,
   MOTOR_RIGHT
};

//
// Effect Structures
//

//
// xilineardecay structure
//
// Tracks a single-motor effect that starts at a given strength and fades to 0
// with a linear decrease over the time period.
//
struct xilineardecay_t
{
   motor_e  which;        // which motor to apply the effect to
   WORD     initStrength; // initial strength of effect
   uint32_t duration;     // duration in milliseconds
   uint32_t startTime;    // starting time
   bool     active;       // currently running?
};

// Current linear decay effect
static xilineardecay_t xiLinearDecay;

//
// xirumble structure
//
// Tracks a single motor effect that sends randomized impulses to a single 
// motor.
//
struct xirumble_t
{
   motor_e  which;          // which motor to apply the effect to
   WORD     maxStrength;    // maximum strength
   WORD     minStrength;    // minimum strength
   uint32_t duration;       // duration in milliseconds
   uint32_t startTime;      // starting time
   bool     active;         // currently running?
};

// Current rumble effect
static xirumble_t xiRumble;

//
// AddClamped
//
// Static routine to do saturating math on the motor values.
//
static WORD AddClamped(WORD currentValue, WORD addValue)
{
   DWORD expanded = currentValue;
   expanded += addValue;
   
   if(expanded > 65535)
      expanded = 65535;

   return static_cast<WORD>(expanded);
}

//
// Evolution functions
//

//
// EvolveLinearDecay
//
// A linear decay decreases constantly to zero from its initial strength.
// Returns true if the effect is still running.
//
static void EvolveLinearDecay(XINPUT_VIBRATION &xvib, uint32_t curTime)
{
   if(curTime > xiLinearDecay.startTime + xiLinearDecay.duration)
   {
      xiLinearDecay.active = false; // done.
      return;
   }

   // calculate state
   WORD curStrength = xiLinearDecay.initStrength;

   curStrength -= curStrength * (curTime - xiLinearDecay.startTime) / xiLinearDecay.duration;

   // which motor?
   switch(xiLinearDecay.which)
   {
   case MOTOR_LEFT:
      xvib.wLeftMotorSpeed  = AddClamped(xvib.wLeftMotorSpeed,  curStrength);
      break;
   case MOTOR_RIGHT:
      xvib.wRightMotorSpeed = AddClamped(xvib.wRightMotorSpeed, curStrength);
      break;
   }
}

//
// EvolveRumble
//
// The rumble effect is entirely chaotic within a given min to max range.
//
static void EvolveRumble(XINPUT_VIBRATION &xvib, uint32_t curTime)
{
   if(curTime > xiRumble.startTime + xiRumble.duration)
   {
      xiRumble.active = false;
      return;
   }

   int minStr = xiRumble.minStrength;
   int rndStr = abs(rand()) % (xiRumble.maxStrength - xiRumble.minStrength);

   WORD totStr = static_cast<WORD>(minStr + rndStr);

   switch(xiRumble.which)
   {
   case MOTOR_LEFT:
      xvib.wLeftMotorSpeed  = AddClamped(xvib.wLeftMotorSpeed, totStr);
      break;
   case MOTOR_RIGHT:
      xvib.wRightMotorSpeed = AddClamped(xvib.wRightMotorSpeed, totStr);
      break;
   }
}

//
// Constructor
//
XInputHapticInterface::XInputHapticInterface(unsigned long userIdx)
   : Super(), dwUserIndex(userIdx), pauseState(false) 
{
}

//
// XInputHapticInterface::zeroState
//
// Protected method. Zeroes out the vibration state.
//
void XInputHapticInterface::zeroState()
{
   XINPUT_VIBRATION xvib = { 0, 0 };
   pXInputSetState(dwUserIndex, &xvib);
}

//
// XInputHapticInterface::startEffect
//
// Schedule a haptic effect for execution.
//
void XInputHapticInterface::startEffect(effect_e effect, int data)
{
   uint32_t curTime = I_GetTicks();

   switch(effect)
   {
   case EFFECT_FIRE: // weapon fire (data should be power scale from 1 to 10)
      xiLinearDecay.startTime    = curTime;
      xiLinearDecay.which        = MOTOR_LEFT;
      xiLinearDecay.initStrength = 21000 + 3100 * data;
      xiLinearDecay.duration     = 200 + 15 * data;
      xiLinearDecay.active       = true;
      break;
   case EFFECT_RUMBLE: // rumble effect (data should be richters from 1 to 10)
   case EFFECT_BUZZ:   // buzz; same deal, but uses high frequency motor
      xiRumble.startTime         = curTime;
      xiRumble.which             = (effect == EFFECT_RUMBLE) ? MOTOR_LEFT : MOTOR_RIGHT;
      xiRumble.minStrength       = 0;
      xiRumble.maxStrength       = 5000 + 6700 * (data - 1);
      xiRumble.duration          = 1000;
      xiRumble.active            = true;
      break;
   default:
      break;
   }
}

//
// XInputHapticInterface::pauseEffects
//
// If effectsPaused is true, then silence any current vibration and stop
// scheduled effects from running, but don't cancel them so that they can
// be resumed. Pass false to restart the effects.
//
void XInputHapticInterface::pauseEffects(bool effectsPaused)
{
   if(!pauseState && effectsPaused)
   {
      zeroState();
      pauseState = true;
   }
   else
      pauseState = false;
}

//
// XInputHapticInterface::updateEffects
//
// Called from the main loop; push the newest summation of state to the
// XInput force motors (high and low frequency) while ticking the 
// scheduled effects. Remove effects once they have completed.
//
void XInputHapticInterface::updateEffects()
{
   // paused?
   if(pauseState)
   {
      zeroState();
      return;
   }

   XINPUT_VIBRATION xvib = { 0, 0 };
   auto curTime = I_GetTicks();

   // run linear decay effect
   if(xiLinearDecay.active)
      EvolveLinearDecay(xvib, curTime);

   // run rumble effect
   if(xiRumble.active)
      EvolveRumble(xvib, curTime);
   
   // set state to the device using the summation of the effects
   pXInputSetState(dwUserIndex, &xvib);
}

//
// XInputHapticInterface::clearEffects
//
// Stop any current vibration and also remove all scheduled effects.
//
void XInputHapticInterface::clearEffects()
{
   zeroState();
   
   // clear effects
   memset(&xiLinearDecay, 0, sizeof(xiLinearDecay));
}

#endif

// EOF

