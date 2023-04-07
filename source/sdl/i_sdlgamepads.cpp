//
// The Eternity Engine
// Copyright (C) 2023 James Haley, Max Waine, et al.
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
// Purpose:Implementation of SDL classes for Gamepads and Joysticks
// Authors: James Haley, Max Waine
//

#ifdef __APPLE__
#include "SDL2/SDL.h"
#include "SDL2/SDL_gamecontroller.h"
#else
#include "SDL.h"
#include "SDL_gamecontroller.h"
#endif

#include "../z_zone.h"
#include "../doomdef.h"
#include "../m_dllist.h"

#include "i_gamepads.h"
#include "i_sdlgamepads.h"
#include "i_timer.h"

// Assertions
static_assert(HALGamePad::MAXBUTTONS >= SDL_CONTROLLER_BUTTON_MAX);
static_assert(HALGamePad::MAXAXES >= SDL_CONTROLLER_AXIS_MAX);

// Module-private globals

// SDL_GameController structure for the currently open GameController device (if any).
// Singleton resource.
static SDL_GameController *gamecontroller;

// Index of active SDL GameController structure. -1 while not valid.
static int activeIdx = -1;

//=============================================================================
//
// SDLGamePadDriver
//
// Implements support for SDL MMSYSTEM gamepad devices.
//

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
// Perform shutdown actions for SDL GameController support.
//
void SDLGamePadDriver::shutdown()
{
   // if the GameController is still active, shut it down.
   if(gamecontroller)
   {
      SDL_GameControllerClose(gamecontroller);
      gamecontroller = nullptr;
   }
}

//
// SDLGamePadDriver::enumerateDevices
//
// Instantiate SDLGamePad objects for all supported GameController
// devices.
//
void SDLGamePadDriver::enumerateDevices()
{
   int numpads = 0;
   SDLGamePad *sdlDev;

   for(int i = 0; i < SDL_NumJoysticks(); i++)
   {
      // Use only valid gamepads
      if(SDL_IsGameController(i))
         numpads++;
   }

   for(int i = 0; i < numpads; i++)
   {
      sdlDev = new SDLGamePad(i);
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
SDLGamePad::SDLGamePad(int idx)
   : Super(), sdlIndex(idx), haptics()
{
   name << "SDL " << SDL_GameControllerNameForIndex(sdlIndex);
   num = i_sdlGamePadDriver.getBaseDeviceNum() + sdlIndex;
}

//
// SDLGamePad::select
//
// Select this gamepad as the active input device for the game engine.
//
bool SDLGamePad::select()
{
   if(gamecontroller) // one is still open? (should not happen)
      return false;

   // remember who is in use internally
   activeIdx = sdlIndex;

   if((gamecontroller = SDL_GameControllerOpen(sdlIndex)) != nullptr)
   {
      numAxes    = SDL_CONTROLLER_AXIS_MAX;
      numButtons = SDL_CONTROLLER_BUTTON_MAX;
      return true;
   }
   else
      return false;
}

//
// SDLGamePad::deselect
//
// Remove this GameController from its status as the active input device.
//
void SDLGamePad::deselect()
{
   if(activeIdx != sdlIndex) // should not happen
      return;

   if(gamecontroller)
   {
      SDL_GameControllerClose(gamecontroller);
      gamecontroller = nullptr;
      activeIdx      = -1;
   }
}

//
// SDLGamePad::poll
//
// Refresh input state data by polling the device.
//
void SDLGamePad::poll()
{
   SDL_GameControllerUpdate();

   // save old button and axis states
   backupState();

   // get button states
   for(int i = 0; i < numButtons && i < SDL_CONTROLLER_BUTTON_MAX; i++)
      state.buttons[i] = !!SDL_GameControllerGetButton(gamecontroller, SDL_GameControllerButton(i));

   // get axis states
   for(int i = 0; i < numAxes && i < SDL_CONTROLLER_AXIS_MAX; i++)
   {
      Sint16 val = SDL_GameControllerGetAxis(gamecontroller, SDL_GameControllerAxis(i));

      if(val > i_joysticksens || val < -i_joysticksens)
      {
         // unbias on the low end, so that +32767 means 1.0
         if(val == -32768)
            val = -32767;

         state.axes[i] = static_cast<float>(val) / 32767.0f;
      }
      else
         state.axes[i] = 0.0f;
   }
}

//=============================================================================
//
// SDLHapticInterface Effects
//
// Support for force feedback through the SDL API.
//

IMPLEMENT_RTTI_TYPE(SDLHapticInterface)

// motor types
enum motor_e
{
   MOTOR_LEFT,
   MOTOR_RIGHT
};

struct sdlRumbleVibration_t
{
   uint16_t leftMotorSpeed;
   uint16_t rightMotorSpeed;
};

//
// Effect Base Class
//
class SDLBaseEffect : public ZoneObject
{
protected:
   uint32_t startTime;
   uint32_t duration;
   static void AddClamped(sdlRumbleVibration_t &sdlvid, motor_e which, uint16_t addValue);

   bool checkDone(uint32_t curTime) { return (curTime > startTime + duration); }

public:
   SDLBaseEffect(uint32_t p_startTime, uint32_t p_duration);
   virtual ~SDLBaseEffect();

   DLListItem<SDLBaseEffect> links;

   virtual void evolve(sdlRumbleVibration_t &sdlvid, uint32_t curTime) = 0;

   static void RunEffectsList(sdlRumbleVibration_t &sdlvid, uint32_t curTime);
   static void ClearEffectsList();
};

static DLList<SDLBaseEffect, &SDLBaseEffect::links> effects;

//
// SDLBaseEffect Constructor
//
SDLBaseEffect::SDLBaseEffect(uint32_t p_startTime, uint32_t p_duration)
   : ZoneObject(), startTime(p_startTime), duration(p_duration), links()
{
   effects.insert(this);
}

//
// SDLBaseEffect Destructor
//
SDLBaseEffect::~SDLBaseEffect()
{
   effects.remove(this);
}

//
// Static routine to do saturating math on the motor values.
//
void SDLBaseEffect::AddClamped(sdlRumbleVibration_t &sdlvid, motor_e which, uint16_t addValue)
{
   uint32_t expanded;
   uint16_t sdlRumbleVibration_t::* value;

   switch(which)
   {
   case MOTOR_LEFT:
      value = &sdlRumbleVibration_t::leftMotorSpeed;
      break;
   case MOTOR_RIGHT:
      value = &sdlRumbleVibration_t::rightMotorSpeed;
      break;
   default:
      return;
   }

   expanded = sdlvid.*value;
   expanded += addValue;

   if(expanded > 65535)
      expanded = 65535;

   sdlvid.*value = uint16_t(expanded);
}

//
// Apply all the active effects to the SDL_VIBRATION structure. Effects are
// deleted from the list when they expire automatically.
//
void SDLBaseEffect::RunEffectsList(sdlRumbleVibration_t &sdlvid, uint32_t curTime)
{
   auto link = effects.head;

   while(link)
   {
      auto next = link->dllNext;
      (*link)->evolve(sdlvid, curTime);
      link = next;
   }
}

//
// Delete all active effects.
//
void SDLBaseEffect::ClearEffectsList()
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
// SDLLinearEffect
//
// Tracks a single-motor effect that starts at a given strength and moves up or
// down to another strength with a linear slope over the time period.
//
class SDLLinearEffect : public SDLBaseEffect
{
protected:
   motor_e  which;
   uint16_t initStrength;
   uint16_t endStrength;
   int      direction;

public:
   SDLLinearEffect(uint32_t p_startTime, uint32_t p_duration, motor_e p_which,
                  uint16_t p_initStrength, uint16_t p_endStrength)
      : SDLBaseEffect(p_startTime, p_duration), which(p_which),
        initStrength(p_initStrength), endStrength(p_endStrength)
   {
      direction = (initStrength < endStrength) ? 1 : -1;
   }
   virtual ~SDLLinearEffect() {}

   virtual void evolve(sdlRumbleVibration_t &sdlvid, uint32_t curTime);
};

//
// Tracks a single motor effect that sends randomized impulses to a single motor.
//
class SDLRumbleEffect : public SDLBaseEffect
{
protected:
   motor_e  which;          // which motor to apply the effect to
   uint16_t minStrength;    // minimum strength
   uint16_t maxStrength;    // maximum strength

public:
   SDLRumbleEffect(uint32_t p_startTime, uint32_t p_duration, motor_e p_which,
                  uint16_t p_minStrength, uint16_t p_maxStrength)
      : SDLBaseEffect(p_startTime, p_duration), which(p_which), 
        minStrength(p_minStrength), maxStrength(p_maxStrength)
   {
   }
   virtual ~SDLRumbleEffect() {}

   virtual void evolve(sdlRumbleVibration_t &sdlvid, uint32_t curTime);
};

//
// Pulses the motor with a steady pattern.
//
class SDLConstantEffect : public SDLBaseEffect
{
protected:
   motor_e  which;
   uint16_t strength;
   static SDLConstantEffect *CurrentLeft;
   static SDLConstantEffect *CurrentRight;

public:
   SDLConstantEffect(uint32_t p_startTime, uint32_t p_duration, motor_e p_which, uint16_t p_strength)
      : SDLBaseEffect(p_startTime, p_duration), which(p_which), strength(p_strength)
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
   virtual ~SDLConstantEffect()
   {
      if(CurrentLeft == this)
         CurrentLeft = nullptr;
      if(CurrentRight == this)
         CurrentRight = nullptr;
   }

   virtual void evolve(sdlRumbleVibration_t &sdlvid, uint32_t curTime);
};

SDLConstantEffect *SDLConstantEffect::CurrentLeft;
SDLConstantEffect *SDLConstantEffect::CurrentRight;

//
// Does a damage hit effect.
//
class SDLDamageEffect : public SDLBaseEffect
{
protected:
   uint16_t strength;

public:
   SDLDamageEffect(uint32_t p_startTime, uint32_t p_duration, uint16_t p_strength)
      : SDLBaseEffect(p_startTime, p_duration), strength(p_strength)
   {
   }
   virtual ~SDLDamageEffect() {}

   virtual void evolve(sdlRumbleVibration_t &sdlvid, uint32_t curtime);
};

//
// Evolution functions
//

//
// A linear effect ramps up or down from the initial value to the end value
// over a set course of time.
//
void SDLLinearEffect::evolve(sdlRumbleVibration_t &sdlvid, uint32_t curTime)
{
   if(checkDone(curTime))
   {
      delete this;
      return;
   }

   uint16_t curStrength;

   if(direction < 0)
   {
      // slope down
      uint16_t deltaStrength = (initStrength - endStrength);
      curStrength = initStrength - deltaStrength * (curTime - startTime) / duration;
   }
   else
   {
      // slope up
      const uint16_t deltaStrength = (endStrength - initStrength);
      curStrength = initStrength + deltaStrength * (curTime - startTime) / duration;
   }

   AddClamped(sdlvid, which, curStrength);
}

//
// The rumble effect is entirely chaotic within a given min to max range.
//
void SDLRumbleEffect::evolve(sdlRumbleVibration_t &sdlvid, uint32_t curTime)
{
   if(checkDone(curTime))
   {
      delete this;
      return;
   }

   int minStr = minStrength;
   int rndStr = abs(rand()) % (maxStrength - minStrength);

   const uint16_t totStr = uint16_t(minStr + rndStr);
   AddClamped(sdlvid, which, totStr);
}

//
// Pulse the motor at a constant strength.
//
void SDLConstantEffect::evolve(sdlRumbleVibration_t &sdlvid, uint32_t curTime)
{
   if(checkDone(curTime))
   {
      delete this;
      return;
   }
   AddClamped(sdlvid, which, strength);
}

//
// Damage utilizes both motors to do effects that decay at different rates.
//
void SDLDamageEffect::evolve(sdlRumbleVibration_t &sdlvid, uint32_t curTime)
{
   if(checkDone(curTime))
   {
      delete this;
      return;
   }

   // Do left motor processing
   const uint32_t endStrength   = strength * 40 / 64;
   const uint32_t deltaStrength = (strength - endStrength);
   uint32_t       curStrength   = strength - deltaStrength * (curTime - startTime) / duration;

   AddClamped(sdlvid, MOTOR_LEFT, uint16_t(curStrength));

   // Do right motor processing
   if(curTime - startTime < duration / 2)
   {
      // Right motor is constant during first half of effect
      AddClamped(sdlvid, MOTOR_RIGHT, strength / 2);
   }
   else
   {
      // Linear descent to zero
      curStrength = strength / 2;
      curStrength -= curStrength * (curTime - startTime) / duration;
      AddClamped(sdlvid, MOTOR_RIGHT, uint16_t(curStrength));
   }
}

//=============================================================================
//
// SDLHapticInterface
//
// Implements HALHapticInterface using SDL_GameControllerRumble
//

//
// Constructor
//
SDLHapticInterface::SDLHapticInterface()
   : Super(), pauseState(false)
{
}

//
// SDLHapticInterface::zeroState
//
// Protected method. Zeroes out the vibration state.
//
void SDLHapticInterface::zeroState()
{
   SDL_GameControllerRumble(gamecontroller, 0, 0, 0);
}

//
// Schedule a haptic effect for execution.
//
void SDLHapticInterface::startEffect(effect_e effect, int data1, int data2)
{
   uint32_t curTime = i_haltimer.GetTicks();

   switch(effect)
   {
   case EFFECT_FIRE:
      // weapon fire
      // * data1 should be power scale from 1 to 10
      // * data2 should be duration scale from 1 to 10
      new SDLLinearEffect(curTime, 175 + 20 * data2, MOTOR_LEFT, 21000 + 4400 * data1, 0);
      break;
   case EFFECT_RAMPUP:
      // ramping up effect, ie. for BFG warmup
      // * data1 is max strength, scale 1 to 10
      // * data2 is duration in ms
      new SDLLinearEffect(curTime, data2, MOTOR_RIGHT, 1000, 21000 + 3100 * data1);
      break;
   case EFFECT_RUMBLE:
      // rumble effect
      // * data1 should be richters from 1 to 10
      // * data2 should be duration in ms
      new SDLRumbleEffect(curTime, data2, MOTOR_LEFT, 0, 5000 + 6700 * (data1 - 1));
      break;
   case EFFECT_BUZZ:
      // buzz; same as rumble, but uses high frequency motor
      new SDLRumbleEffect(curTime, data2, MOTOR_RIGHT, 0, 5000 + 6700 * (data1 - 1));
      break;
   case EFFECT_CONSTANT:
      // constant: continue pulsing the motor at a steady rate
      // * data1 should be strength from 1 to 10
      // * data2 should be duration in ms
      new SDLConstantEffect(curTime, data2, MOTOR_RIGHT, 6500 * data1);
      break;
   case EFFECT_DAMAGE:
      // damage: taking a hit from something
      // * data1 should be strength from 1 to 100
      // * data2 should be duration in ms
      new SDLDamageEffect(curTime, data2, 25000 + data1 * 400);
      break;
   default:
      break;
   }
}

//
// If effectsPaused is true, then silence any current vibration and stop
// scheduled effects from running, but don't cancel them so that they can
// be resumed. Pass false to restart the effects.
//
void SDLHapticInterface::pauseEffects(bool effectsPaused)
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
// Called from the main loop; push the newest summation of state to the
// SDL force motors (high and low frequency) while ticking the 
// scheduled effects. Remove effects once they have completed.
//
void SDLHapticInterface::updateEffects()
{
   // paused?
   if(pauseState)
   {
      zeroState();
      return;
   }

   sdlRumbleVibration_t sdlvib = { 0, 0 };
   auto curTime = i_haltimer.GetTicks();

   SDLBaseEffect::RunEffectsList(sdlvib, curTime);

   static int prevx, prevy = 0;

   // set state to the device using the summation of the effects
   SDL_GameControllerRumble(gamecontroller, sdlvib.leftMotorSpeed, sdlvib.rightMotorSpeed, 1000);
}

//
// Stop any current vibration and also remove all scheduled effects.
//
void SDLHapticInterface::clearEffects()
{
   zeroState();

   // clear effects
   SDLBaseEffect::ClearEffectsList();
}

// EOF

