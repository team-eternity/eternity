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

#ifdef EE_FEATURE_XINPUT

#include "../z_zone.h"

#include "../m_dllist.h"
#include "../m_vector.h"
#include "i_xinput.h"

// Need timer HAL
#include "../hal/i_timer.h"

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
   for(HALGamePad *&it : devices)
      it->getHapticInterface()->clearEffects();
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
// XInputHapticInterface Effects
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
// Effect Base Class
//
class XIBaseEffect : public ZoneObject
{
protected:
   uint32_t startTime;   
   uint32_t duration;
   static void AddClamped(XINPUT_VIBRATION &xvib, motor_e which, WORD addValue);

   bool checkDone(uint32_t curTime) { return (curTime > startTime + duration); }

public:
   XIBaseEffect(uint32_t p_startTime, uint32_t p_duration);
   virtual ~XIBaseEffect();

   DLListItem<XIBaseEffect> links;

   virtual void evolve(XINPUT_VIBRATION &xvib, uint32_t curTime) = 0;

   static void RunEffectsList(XINPUT_VIBRATION &xvib, uint32_t curTime);
   static void ClearEffectsList();
};

static DLList<XIBaseEffect, &XIBaseEffect::links> effects;

//
// XIBaseEffect Constructor
//
XIBaseEffect::XIBaseEffect(uint32_t p_startTime, uint32_t p_duration)
   : ZoneObject(), startTime(p_startTime), duration(p_duration), links() 
{
   effects.insert(this);
}

//
// XIBaseEffect Destructor
//
XIBaseEffect::~XIBaseEffect()
{
   effects.remove(this);
}

//
// AddClamped
//
// Static routine to do saturating math on the motor values.
//
void XIBaseEffect::AddClamped(XINPUT_VIBRATION &xvib, motor_e which, WORD addValue)
{
   DWORD expanded;
   WORD XINPUT_VIBRATION::* value;

   switch(which)
   {
   case MOTOR_LEFT:
      value = &XINPUT_VIBRATION::wLeftMotorSpeed;
      break;
   case MOTOR_RIGHT:
      value = &XINPUT_VIBRATION::wRightMotorSpeed;
      break;
   default:
      return;
   }

   expanded = xvib.*value;
   expanded += addValue;

   if(expanded > 65535)
      expanded = 65535;

   xvib.*value = static_cast<WORD>(expanded);
}

//
// XIBaseEffect::RunEffectsList
//
// Apply all the active effects to the XINPUT_VIBRATION structure. Effects are
// deleted from the list when they expire automatically.
//
void XIBaseEffect::RunEffectsList(XINPUT_VIBRATION &xvib, uint32_t curTime)
{
   auto link = effects.head;

   while(link)
   {
      auto next = link->dllNext;
      (*link)->evolve(xvib, curTime);
      link = next;
   }
}

// 
// XIBaseEffect::ClearEffectsList
//
// Delete all active effects.
//
void XIBaseEffect::ClearEffectsList()
{
   auto link = effects.head;

   while(link)
   {
      auto next = link->dllNext;
      delete link->dllObject;
      link = next;
   }
}

//
// Effect Classes
//

//
// XILinearEffect
//
// Tracks a single-motor effect that starts at a given strength and moves up or
// down to another strength with a linear slope over the time period.
//
class XILinearEffect : public XIBaseEffect
{
protected:
   motor_e  which;
   WORD     initStrength;
   WORD     endStrength;
   int      direction;

public:
   XILinearEffect(uint32_t p_startTime, uint32_t p_duration, motor_e p_which,
                  WORD p_initStrength, WORD p_endStrength)
      : XIBaseEffect(p_startTime, p_duration), which(p_which), 
        initStrength(p_initStrength), endStrength(p_endStrength)
   {
      direction = (initStrength < endStrength) ? 1 : -1;
   }
   virtual ~XILinearEffect() {}

   virtual void evolve(XINPUT_VIBRATION &xvib, uint32_t curTime);
};

//
// XIRumbleEffect
//
// Tracks a single motor effect that sends randomized impulses to a single 
// motor.
//
class XIRumbleEffect : public XIBaseEffect
{
protected:
   motor_e  which;          // which motor to apply the effect to
   WORD     minStrength;    // minimum strength
   WORD     maxStrength;    // maximum strength

public:
   XIRumbleEffect(uint32_t p_startTime, uint32_t p_duration, motor_e p_which,
                  WORD p_minStrength, WORD p_maxStrength)
      : XIBaseEffect(p_startTime, p_duration), which(p_which), 
        minStrength(p_minStrength), maxStrength(p_maxStrength)
   {
   }
   virtual ~XIRumbleEffect() {}

   virtual void evolve(XINPUT_VIBRATION &xvib, uint32_t curTime);
};

//
// XIConstantEffect
//
// Pulses the motor with a steady pattern.
//
class XIConstantEffect : public XIBaseEffect
{
protected:
   motor_e which;
   WORD    strength;
   static XIConstantEffect *CurrentLeft;
   static XIConstantEffect *CurrentRight;

public:
   XIConstantEffect(uint32_t p_startTime, uint32_t p_duration, motor_e p_which,
                    WORD p_strength)
      : XIBaseEffect(p_startTime, p_duration), which(p_which), strength(p_strength)
   {
      // this effect is singleton
      if(which == MOTOR_LEFT)
      {
         if(CurrentLeft)
            delete CurrentLeft;
         CurrentLeft = this;
      }
      else
      {
         if(CurrentRight)
            delete CurrentRight;
         CurrentRight = this;
      }

   }
   virtual ~XIConstantEffect() 
   {
      if(CurrentLeft == this)
         CurrentLeft = nullptr;
      if(CurrentRight == this)
         CurrentRight = nullptr;
   }

   virtual void evolve(XINPUT_VIBRATION &xvib, uint32_t curTime);
};
   
XIConstantEffect *XIConstantEffect::CurrentLeft;
XIConstantEffect *XIConstantEffect::CurrentRight;

//
// XIDamageEffect
//
// Does a damage hit effect.
//
class XIDamageEffect : public XIBaseEffect
{
protected:
   WORD strength;

public:
   XIDamageEffect(uint32_t p_startTime, uint32_t p_duration, WORD p_strength)
      : XIBaseEffect(p_startTime, p_duration), strength(p_strength)
   {
   }
   virtual ~XIDamageEffect() {}

   virtual void evolve(XINPUT_VIBRATION &xvib, uint32_t curtime);
};

//
// Evolution functions
//

//
// XILinearEffect::evolve
//
// A linear effect ramps up or down from the initial value to the end value
// over a set course of time.
//
void XILinearEffect::evolve(XINPUT_VIBRATION &xvib, uint32_t curTime)
{
   if(checkDone(curTime))
   {
      delete this;
      return;
   }

   WORD curStrength;

   if(direction < 0)
   {
      // slope down
      WORD deltaStrength = (initStrength - endStrength);
      curStrength = initStrength - deltaStrength * (curTime - startTime) / duration;
   }
   else
   {
      // slope up
      WORD deltaStrength = (endStrength - initStrength);
      curStrength = initStrength + deltaStrength * (curTime - startTime) / duration;
   }
   
   AddClamped(xvib, which, curStrength);   
}

//
// XIRumbleEffect::evolve
//
// The rumble effect is entirely chaotic within a given min to max range.
//
void XIRumbleEffect::evolve(XINPUT_VIBRATION &xvib, uint32_t curTime)
{
   if(checkDone(curTime))
   {
      delete this;
      return;
   }

   int minStr = minStrength;
   int rndStr = abs(rand()) % (maxStrength - minStrength);

   WORD totStr = static_cast<WORD>(minStr + rndStr);
   AddClamped(xvib, which, totStr);
}

//
// XIConstantEffect::evolve
//
// Pulse the motor at a constant strength. Due to the behavior of the XBox360
// controller's non-solenoid motors however, there's a bit of "catch" which 
// adds some unavoidable pseudo-random variance into the mix. Depending on 
// where it stopped last time, you get no response at all until it seems to 
// "roll over" the catch, at which point you get more than you asked for.
//
void XIConstantEffect::evolve(XINPUT_VIBRATION &xvib, uint32_t curTime)
{
   if(checkDone(curTime))
   {
      delete this;
      return;
   }
   AddClamped(xvib, which, strength);
}

//
// XIDamageEffect::evolve
//
// Damage utilizes both motors to do effects that decay at different rates.
//
void XIDamageEffect::evolve(XINPUT_VIBRATION &xvib, uint32_t curTime)
{
   if(checkDone(curTime))
   {
      delete this;
      return;
   }

   // Do left motor processing
   DWORD endStrength   = strength * 40 / 64;
   DWORD deltaStrength = (strength - endStrength);
   DWORD curStrength   = strength - deltaStrength * (curTime - startTime) / duration;
   
   AddClamped(xvib, MOTOR_LEFT, static_cast<WORD>(curStrength));

   // Do right motor processing
   if(curTime - startTime < duration / 2)
   {
      // Right motor is constant during first half of effect
      AddClamped(xvib, MOTOR_RIGHT, strength / 2);
   }
   else
   {
      // Linear descent to zero
      curStrength = strength / 2;
      curStrength -= curStrength * (curTime - startTime) / duration;
      AddClamped(xvib, MOTOR_RIGHT, static_cast<WORD>(curStrength));
   }
}

//=============================================================================
//
// XInputHapticInterface
//
// Implements HALHapticInterface with an experience optimized for the XBox360
// controller.
//

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
void XInputHapticInterface::startEffect(effect_e effect, int data1, int data2)
{
   uint32_t curTime = i_haltimer.GetTicks();

   switch(effect)
   {
   case EFFECT_FIRE:   
      // weapon fire 
      // * data1 should be power scale from 1 to 10
      // * data2 should be duration scale from 1 to 10
      new XILinearEffect(curTime, 175 + 20 * data2, MOTOR_LEFT, 21000 + 4400 * data1, 0);
      break;
   case EFFECT_RAMPUP:
      // ramping up effect, ie. for BFG warmup
      // * data1 is max strength, scale 1 to 10
      // * data2 is duration in ms
      new XILinearEffect(curTime, data2, MOTOR_RIGHT, 1000, 21000 + 3100 * data1);
      break;
   case EFFECT_RUMBLE: 
      // rumble effect 
      // * data1 should be richters from 1 to 10
      // * data2 should be duration in ms
      new XIRumbleEffect(curTime, data2, MOTOR_LEFT, 0, 5000 + 6700 * (data1 - 1));
      break;
   case EFFECT_BUZZ:   
      // buzz; same as rumble, but uses high frequency motor
      new XIRumbleEffect(curTime, data2, MOTOR_RIGHT, 0, 5000 + 6700 * (data1 - 1));
      break;
   case EFFECT_CONSTANT:
      // constant: continue pulsing the motor at a steady rate
      // * data1 should be strength from 1 to 10
      // * data2 should be duration in ms
      new XIConstantEffect(curTime, data2, MOTOR_RIGHT, 6500 * data1);
      break;
   case EFFECT_DAMAGE:
      // damage: taking a hit from something
      // * data1 should be strength from 1 to 100
      // * data2 should be duration in ms
      new XIDamageEffect(curTime, data2, 25000 + data1 * 400);
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
   auto curTime = i_haltimer.GetTicks();

   XIBaseEffect::RunEffectsList(xvib, curTime);
   
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
   XIBaseEffect::ClearEffectsList();
}

#endif

// EOF

