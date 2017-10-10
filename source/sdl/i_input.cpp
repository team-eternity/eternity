//
// The Eternity Engine
// Copyright (C) 2017 James Haley, Max Waine, et al.
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
// Purpose: Keyboard, mouse, and joystick code
// Authors: James Haley, Max Waine
//

#include "SDL.h"

// HAL modules
#include "../hal/i_platform.h"
#include "../hal/i_gamepads.h"

#include "../c_io.h"
#include "../c_runcmd.h"
#include "../d_event.h"
#include "../d_main.h"
#include "../doomdef.h"
#include "../doomstat.h"
#include "../g_bind.h"
#include "../m_argv.h"
#include "../m_dllist.h"
#include "../p_chase.h"
#include "../v_misc.h"
#include "../v_video.h"

// Grab the mouse? (int type for config code)
int         grabmouse;
extern int  usemouse;   // killough 10/98

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen
bool screenvisible;
bool window_focused;
bool fullscreen;

// haleyjd 10/25/09
bool unicodeinput;

void I_InitKeyboard();      // i_system.c

bool MouseShouldBeGrabbed();

//=============================================================================
//
// WM
//

//
// UpdateGrab
//
// haleyjd 10/08/05: from Chocolate DOOM
//
void UpdateGrab(SDL_Window *window)
{
   static bool currently_grabbed = false;
   bool grab;

   grab = MouseShouldBeGrabbed();

   if(grab && !currently_grabbed)
   {
      // When the cursor is hidden, grab the input.
      // Relative mode implicitly hides the cursor.
      SDL_SetRelativeMouseMode(SDL_TRUE);
      SDL_GetRelativeMouseState(nullptr, nullptr);
   }
   else if(!grab && currently_grabbed)
   {
      int window_w, window_h;

      SDL_SetRelativeMouseMode(SDL_FALSE);
      SDL_GetRelativeMouseState(nullptr, nullptr);

      // When releasing the mouse from grab, warp the mouse cursor to
      // the bottom-right of the screen. This is a minimally distracting
      // place for it to appear - we may only have released the grab
      // because we're at an end of level intermission screen, for
      // example.

      SDL_GetWindowSize(window, &window_w, &window_h);
      SDL_WarpMouseInWindow(window, window_w - 16, window_h - 16);
      SDL_GetRelativeMouseState(nullptr, nullptr);
   }

   currently_grabbed = grab;
}

//
// MouseShouldBeGrabbed
//
// haleyjd 10/08/05: From Chocolate DOOM, fairly self-explanatory.
//
bool MouseShouldBeGrabbed()
{
   // if the window doesnt have focus, never grab it
   if(!window_focused)
      return false;

   // always grab the mouse when full screen (dont want to
   // see the mouse pointer)
   if(fullscreen)
      return true;

   // if we specify not to grab the mouse, never grab
   if(!grabmouse)
      return false;

   // when menu is active or game is paused, release the mouse, but:
   // * menu and console do not pause during netgames
   // * walkcam needs mouse when game is paused
   if(((menuactive || consoleactive) && !netgame) || 
      (paused && !walkcam_active))
      return false;

   // only grab mouse when playing levels (but not demos (if walkcam isn't active))
   return (gamestate == GS_LEVEL) && (!demoplayback || walkcam_active);
}

//
// UpdateFocus
//
// haleyjd 10/08/05: From Chocolate Doom
// Update the value of window_focused when we get a focus event
//
// We try to make ourselves be well-behaved: the grab on the mouse
// is removed if we lose focus (such as a popup window appearing),
// and we dont move the mouse around if we aren't focused either.
//
void UpdateFocus(SDL_Window *window)
{
   Uint32 state;

   SDL_PumpEvents();

   state = window ? SDL_GetWindowFlags(window) : 0;
   screenvisible = ((state & SDL_WINDOW_SHOWN) && !(state & SDL_WINDOW_MINIMIZED));

   window_focused = (screenvisible &&
                     ((state & (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS)) != 0));
}

//=============================================================================
//
// Keyboard
//

//
// I_TranslateKey
//
// For SDL, translates from SDL keysyms to DOOM key values.
//
static int I_TranslateKey(SDL_Keysym *sym)
{
   int rc = 0;
   int scancode = sym->scancode;

   // This is Chocolote Doom's code for I_TranslateKey
   //switch(scancode)
   //{
   //case SDL_SCANCODE_LCTRL:
   //case SDL_SCANCODE_RCTRL:
   //   return KEYD_RCTRL;
   //case SDL_SCANCODE_LSHIFT:
   //case SDL_SCANCODE_RSHIFT:
   //   return KEYD_RSHIFT;
   //case SDL_SCANCODE_LALT:
   //   return KEYD_LALT;
   //case SDL_SCANCODE_RALT:
   //   return KEYD_RALT;
   //default:
   //   if(scancode >= 0 && scancode < earrlen(scancode_translate_table))
   //      return scancode_translate_table[scancode];
   //   else
   //      return 0;
   //}

   switch(sym->scancode)
   {
   case SDL_SCANCODE_LEFT:         rc = KEYD_LEFTARROW;  break;
   case SDL_SCANCODE_RIGHT:        rc = KEYD_RIGHTARROW; break;
   case SDL_SCANCODE_DOWN:         rc = KEYD_DOWNARROW;  break;
   case SDL_SCANCODE_UP:           rc = KEYD_UPARROW;    break;
   case SDL_SCANCODE_ESCAPE:       rc = KEYD_ESCAPE;     break;
   case SDL_SCANCODE_RETURN:       rc = KEYD_ENTER;      break;
   case SDL_SCANCODE_TAB:          rc = KEYD_TAB;        break;
   case SDL_SCANCODE_F1:           rc = KEYD_F1;         break;
   case SDL_SCANCODE_F2:           rc = KEYD_F2;         break;
   case SDL_SCANCODE_F3:           rc = KEYD_F3;         break;
   case SDL_SCANCODE_F4:           rc = KEYD_F4;         break;
   case SDL_SCANCODE_F5:           rc = KEYD_F5;         break;
   case SDL_SCANCODE_F6:           rc = KEYD_F6;         break;
   case SDL_SCANCODE_F7:           rc = KEYD_F7;         break;
   case SDL_SCANCODE_F8:           rc = KEYD_F8;         break;
   case SDL_SCANCODE_F9:           rc = KEYD_F9;         break;
   case SDL_SCANCODE_F10:          rc = KEYD_F10;        break;
   case SDL_SCANCODE_F11:          rc = KEYD_F11;        break;
   case SDL_SCANCODE_F12:          rc = KEYD_F12;        break;
   case SDL_SCANCODE_BACKSPACE:    rc = KEYD_BACKSPACE;  break;
   case SDL_SCANCODE_PAUSE:        rc = KEYD_PAUSE;      break;
   case SDL_SCANCODE_EQUALS:       rc = KEYD_EQUALS;     break;
   case SDL_SCANCODE_MINUS:        rc = KEYD_MINUS;      break;

   case SDL_SCANCODE_KP_0:         rc = KEYD_KP0;        break;
   case SDL_SCANCODE_KP_1:         rc = KEYD_KP1;        break;
   case SDL_SCANCODE_KP_2:         rc = KEYD_KP2;        break;
   case SDL_SCANCODE_KP_3:         rc = KEYD_KP3;        break;
   case SDL_SCANCODE_KP_4:         rc = KEYD_KP4;        break;
   case SDL_SCANCODE_KP_5:         rc = KEYD_KP5;        break;
   case SDL_SCANCODE_KP_6:         rc = KEYD_KP6;        break;
   case SDL_SCANCODE_KP_7:         rc = KEYD_KP7;        break;
   case SDL_SCANCODE_KP_8:         rc = KEYD_KP8;        break;
   case SDL_SCANCODE_KP_9:         rc = KEYD_KP9;        break;
   case SDL_SCANCODE_KP_PERIOD:    rc = KEYD_KPPERIOD;   break;
   case SDL_SCANCODE_KP_DIVIDE:    rc = KEYD_KPDIVIDE;   break;
   case SDL_SCANCODE_KP_MULTIPLY:  rc = KEYD_KPMULTIPLY; break;
   case SDL_SCANCODE_KP_MINUS:     rc = KEYD_KPMINUS;    break;
   case SDL_SCANCODE_KP_PLUS:      rc = KEYD_KPPLUS;     break;
   case SDL_SCANCODE_KP_ENTER:     rc = KEYD_KPENTER;    break;
   case SDL_SCANCODE_KP_EQUALS:    rc = KEYD_KPEQUALS;   break;

   case SDL_SCANCODE_NUMLOCKCLEAR: rc = KEYD_NUMLOCK;    break;
   case SDL_SCANCODE_SCROLLLOCK:   rc = KEYD_SCROLLLOCK; break;
   case SDL_SCANCODE_CAPSLOCK:     rc = KEYD_CAPSLOCK;   break;
   case SDL_SCANCODE_LSHIFT:
   case SDL_SCANCODE_RSHIFT:       rc = KEYD_RSHIFT;     break;
   case SDL_SCANCODE_LCTRL:
   case SDL_SCANCODE_RCTRL:        rc = KEYD_RCTRL;      break;

   case SDL_SCANCODE_LALT:
   case SDL_SCANCODE_RALT:
   case SDL_SCANCODE_LGUI:
   case SDL_SCANCODE_RGUI:        rc = KEYD_RALT;        break;
   case SDL_SCANCODE_PAGEUP:      rc = KEYD_PAGEUP;      break;
   case SDL_SCANCODE_PAGEDOWN:    rc = KEYD_PAGEDOWN;    break;
   case SDL_SCANCODE_HOME:        rc = KEYD_HOME;        break;
   case SDL_SCANCODE_END:         rc = KEYD_END;         break;
   case SDL_SCANCODE_INSERT:      rc = KEYD_INSERT;      break;
   case SDL_SCANCODE_DELETE:      rc = KEYD_DEL;         break;
   default:
      rc = sym->sym;
      break;
   }
   return rc;
}

int I_ScanCode2DoomCode(int a)
{
   return a;
}

int I_DoomCode2ScanCode(int a)
{
   return a;
}

//=============================================================================
//
// Joystick                                                    // phares 4/3/98
//

//
// I_JoystickEvents
//
// Gathers joystick data and creates an event_t for later processing
// by G_Responder().
//
static void I_JoystickEvents()
{
   HALGamePad::padstate_t *padstate;

   if(!(padstate = I_PollActiveGamePad()))
      return;

   // turn padstate into button input events
   for(int button = 0; button < HALGamePad::MAXBUTTONS; button++)
   {
      edefstructvar(event_t, ev);

      if(padstate->buttons[button] != padstate->prevbuttons[button])
      {
         ev.type  = padstate->buttons[button] ? ev_keydown : ev_keyup;
         ev.data1 = KEYD_JOY01 + button;
         D_PostEvent(&ev);
      }
   }

   // read axes
   for(int axis = 0; axis < HALGamePad::MAXAXES; axis++)
   {
      // fire axis state change events
      if(padstate->axes[axis] != padstate->prevaxes[axis])
      {
         edefstructvar(event_t, ev);

         // if previous state was off, key down
         if(padstate->prevaxes[axis] == 0.0)
            ev.type = ev_keydown;

         // if new state is off, key up
         if(padstate->axes[axis] == 0.0)
            ev.type = ev_keyup;

         ev.data1 = KEYD_AXISON01 + axis;
         D_PostEvent(&ev);
      }

      // post analog axis state
      edefstructvar(event_t, ev);
      ev.type  = ev_joystick;
      ev.data1 = axis;
      ev.data2 = padstate->axes[axis];
      if(axisOrientation[axis]) // may need to flip, if orientation == -1
         ev.data2 *= axisOrientation[axis];
      D_PostEvent(&ev);
   }
}


//
// I_StartFrame
//
void I_StartFrame()
{
   I_JoystickEvents(); // Obtain joystick data                 phares 4/3/98
   I_UpdateHaptics();  // Run haptic output                   haleyjd 6/4/13
}

//=============================================================================
//
// Mouse
//

extern void MN_QuitDoom();
extern int mouseAccel_type;
extern int mouseAccel_threshold;
extern double mouseAccel_value;

//
// Mouse acceleration
//
// This emulates some of the behavior of DOS mouse drivers by increasing
// the speed when the mouse is moved fast.
//
// The mouse input values are input directly to the game, but when
// the values exceed the value of mouse_threshold, they are multiplied
// by mouse_acceleration to increase the speed.

float mouse_acceleration = 2.0;
int   mouse_threshold = 10;

//
// AccelerateMouse
//
// haleyjd 10/23/08: From Choco-Doom
//
static int AccelerateMouse(int val)
{
   if(val < 0)
      return -AccelerateMouse(-val);

   return (val > mouse_threshold) ?
             (int)((val - mouse_threshold) * mouse_acceleration + mouse_threshold) :
             val;
}

static double CustomAccelerateMouse(int val)
{
   if(val < 0)
      return -CustomAccelerateMouse(-val);

   if((mouseAccel_value != 1.0) && (val > mouseAccel_threshold))
      return val * mouseAccel_value;

   return val;
}

//
// CenterMouse
//
// haleyjd 10/23/08: from Choco-Doom:
// Warp the mouse back to the middle of the screen
//
static void CenterMouse(SDL_Window *window)
{
   // Warp the the screen center

   SDL_WarpMouseInWindow(window, int(video.width / 2), int(video.height / 2));

   // Clear any relative movement caused by warping

   SDL_PumpEvents();
   SDL_GetRelativeMouseState(nullptr, nullptr);
}

//
// I_ReadMouse
//
// haleyjd 10/23/08: from Choco-Doom:
//
// Read the change in mouse state to generate mouse motion events
//
// This is to combine all mouse movement for a tic into one mouse
// motion event.
//
static void I_ReadMouse(SDL_Window *window)
{
   int x, y;
   event_t ev;
   static Uint8 previous_state = 137;
   Uint8 state;

   SDL_PumpEvents();

   state = SDL_GetRelativeMouseState(&x, &y);

   if(state != previous_state)
      previous_state = state;

   if(x != 0 || y != 0)
   {
      ev.type = ev_mouse;
      ev.data1 = SDL_MOUSEMOTION;
      if(mouseAccel_type == 2)
      {
         // SoM: So the values that go to Eternity should be 16.16 fixed
         //      point...
         ev.data2 =  AccelerateMouse(x);
         ev.data3 = -AccelerateMouse(y);
      }
      else if(mouseAccel_type == 3) // [CG] 01/20/12 Custom acceleration
      {
         ev.data2 =  CustomAccelerateMouse(x);
         ev.data3 = -CustomAccelerateMouse(y);
      }

      D_PostEvent(&ev);
   }

   if(MouseShouldBeGrabbed())
      CenterMouse(window);
}

//
// I_InitMouse
//
// Once upon a time this function existed in vanilla DOOM, and now here it is
// again.
// haleyjd 05/10/11: Moved -grabmouse check here from the video subsystem.
//
void I_InitMouse()
{
   // haleyjd 10/09/05: from Chocolate DOOM
   // mouse grabbing
   if(M_CheckParm("-grabmouse"))
      grabmouse = 1;
   else if(M_CheckParm("-nograbmouse"))
      grabmouse = 0;
}

//=============================================================================
//
// Events
//

extern int gametic;

struct deferredevent_t
{
   DLListItem<deferredevent_t> links;
   event_t ev;
   int tic;
};

static DLList<deferredevent_t, &deferredevent_t::links> i_deferredevents;
static DLList<deferredevent_t, &deferredevent_t::links> i_deferredfreelist;

//
// I_AddDeferredEvent
//
// haleyjd 03/06/13: Some received input events need to be deferred until at
// least one tic has passed before they are posted to the event queue. 
// "Trigger" style keys such as mousewheel up and down are the chief offenders.
// Rather than shoehorning a bunch of code for this into I_GetEvent, it is
// now handled here uniformly for all event types.
//
static void I_AddDeferredEvent(const event_t &ev, int tic)
{
   deferredevent_t *de;

   if(i_deferredfreelist.head)
   {
      de = *i_deferredfreelist.head;
      i_deferredfreelist.remove(de);
   }
   else
      de = estructalloc(deferredevent_t, 1);

   de->ev  = ev;
   de->tic = tic;
   i_deferredevents.insert(de);
}

//
// I_PutDeferredEvent
//
// Put a deferredevent_t back on the freelist.
//
static void I_PutDeferredEvent(deferredevent_t *de)
{
   i_deferredevents.remove(de);
   i_deferredfreelist.insert(de);
}

//
// I_RunDeferredEvents
//
// Check the deferred events queue for events that are ready to be posted.
//
static void I_RunDeferredEvents()
{
   DLListItem<deferredevent_t> *rover = i_deferredevents.head;
   static int lasttic;

   // Only run once per tic.
   if(lasttic == gametic)
      return;

   lasttic = gametic;

   while(rover)
   {
      DLListItem<deferredevent_t> *next = rover->dllNext;

      deferredevent_t *de = *rover;
      if(de->tic <= gametic)
      {
         D_PostEvent(&de->ev);
         I_PutDeferredEvent(de);
      }

      rover = next;
   }
}

static void I_GetEvent(SDL_Window *window)
{
   SDL_Event  ev;
   int        sendmouseevent = 0;
   int        buttons        = 0;
   event_t    d_event        = { ev_keydown, 0, 0, 0, '\0', false };
   event_t    mouseevent     = { ev_mouse,   0, 0, 0, '\0', false };
   event_t    tempevent      = { ev_keydown, 0, 0, 0, '\0', false };

   // [CG] 01/31/2012: Ensure we have the latest info about focus and mouse
   //                  grabbing.
   UpdateFocus(window);
   UpdateGrab(window);

   while(SDL_PollEvent(&ev))
   {
      // haleyjd 10/08/05: from Chocolate DOOM
      if(!window_focused &&
         (ev.type == SDL_MOUSEMOTION     ||
          ev.type == SDL_MOUSEBUTTONDOWN ||
          ev.type == SDL_MOUSEBUTTONUP))
      {
         continue;
      }

      switch(ev.type)
      {
      case SDL_KEYDOWN:
         d_event.type = ev_keydown;
         d_event.repeat = ev.key.repeat;
         d_event.data1 = I_TranslateKey(&ev.key.keysym);

#if (EE_CURRENT_PLATFORM != EE_PLATFORM_MACOSX)
         // This #if block is adapted from PRBoom+
         // TODO: A fullscreen toggle would be nice, but geom string might need setting.
         // See PRBoom+'s I_GetEvent for a cross-platform implementation of how to get that input.
         if(ev.key.keysym.mod & KMOD_LALT)
         {
            // Prevent executing action on Alt-Tab
            if(ev.key.keysym.scancode == SDL_SCANCODE_TAB)
               break;
            // Immediately exit on Alt+F4 ("Boss Key")
            else if(ev.key.keysym.scancode == SDL_SCANCODE_F4)
            {
               I_QuitFast();
               break;
            }
         }
#else
         // Also provide macOS option
         if(ev.key.keysym.mod & KMOD_LGUI)
         {
            if(ev.key.keysym.scancode == SDL_SCANCODE_Q)
            {
               I_QuitFast();
               break;
            }
         }
#endif

         // SDL_FIXME: Should this #if and the block therein be removed?
         // Seems to act the same regardless of if it's there or not...
#if (EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS)
         // Capslock on Windows alternates between key down and key up
         // events. When we get a keydown, we need to defer a keyup event.
         if(d_event.data1 == KEYD_CAPSLOCK)
         {
            // oPS I HITTED TEH CAPDLOCK!
            tempevent.type  = ev_keyup;
            tempevent.data1 = KEYD_CAPSLOCK;
            I_AddDeferredEvent(tempevent, gametic + 1);
         }
#endif
         if(unicodeinput && ev.key.keysym.sym > 31 && ev.key.keysym.sym < 127)
            d_event.character = ev.key.keysym.sym;
         else
            d_event.character = 0;

         D_PostEvent(&d_event);
         break;

      case SDL_KEYUP:
         d_event.type = ev_keyup;
         d_event.data1 = I_TranslateKey(&ev.key.keysym);

         D_PostEvent(&d_event);
         break;

      case SDL_MOUSEMOTION:
         if(!usemouse || ((mouseAccel_type == 2) || mouseAccel_type == 3))
            continue;

         // haleyjd 06/14/10: no mouse motion at startup.
         if(gametic == 0)
            continue;

         // SoM 1-20-04 Ok, use xrel/yrel for mouse movement because most
         // people like it the most.
         if(mouseAccel_type == 0)
         {
            mouseevent.data2 += ev.motion.xrel;
            mouseevent.data3 -= ev.motion.yrel;
         }
         else if(mouseAccel_type == 1)
         {
            // Simple linear acceleration
            // Evaluates to 1.25 * x. So Why don't I just do that? .... shut up
            mouseevent.data2 += (ev.motion.xrel + (float)(ev.motion.xrel * 0.25f));
            mouseevent.data3 -= (ev.motion.yrel + (float)(ev.motion.yrel * 0.25f));
         }

         sendmouseevent = 1;
         break;

      case SDL_MOUSEBUTTONDOWN:
         if(!usemouse)
            continue;
         d_event.type =  ev_keydown;

         switch(ev.button.button)
         {
         case SDL_BUTTON_LEFT:
            sendmouseevent = 1;
            buttons |= 1;
            d_event.data1 = KEYD_MOUSE1;
            break;
         case SDL_BUTTON_MIDDLE:
            // haleyjd 05/28/06: swapped MOUSE3/MOUSE2
            sendmouseevent = 1;
            buttons |= 4;
            d_event.data1 = KEYD_MOUSE3;
            break;
         case SDL_BUTTON_RIGHT:
            sendmouseevent = 1;
            buttons |= 2;
            d_event.data1 = KEYD_MOUSE2;
            break;
         case SDL_BUTTON_X1:
            d_event.data1 = KEYD_MOUSE4;
            break;
         case SDL_BUTTON_X2:
            d_event.data1 = KEYD_MOUSE5;
            break;
         }

         D_PostEvent(&d_event);
         break;

      case SDL_MOUSEWHEEL:
         if(!usemouse)
            continue;
         d_event.type = ev_keydown;

        if(ev.wheel.y == 1)
        {
            d_event.data1 = KEYD_MWHEELUP;
            // WHEELUP sends a button up event immediately. That won't work;
            // we need an input latency gap of at least one gametic.
            tempevent.type = ev_keyup;
            tempevent.data1 = KEYD_MWHEELUP;
            I_AddDeferredEvent(tempevent, gametic + 1);
            break;
        }
        else if(ev.wheel.y == -1)
        {
           d_event.data1 = KEYD_MWHEELDOWN;
           // ditto, as above.
           tempevent.type = ev_keyup;
           tempevent.data1 = KEYD_MWHEELDOWN;
           I_AddDeferredEvent(tempevent, gametic + 1);
           break;
        }

      case SDL_MOUSEBUTTONUP:
         if(!usemouse)
            continue;
         d_event.type = ev_keyup;
         d_event.data1 = 0;

         switch(ev.button.button)
         {
         case SDL_BUTTON_LEFT:
            sendmouseevent = 1;
            buttons &= ~1;
            d_event.data1 = KEYD_MOUSE1;
            break;
         case SDL_BUTTON_MIDDLE:
            // haleyjd 05/28/06: swapped MOUSE3/MOUSE2
            sendmouseevent = 1;
            buttons &= ~4;
            d_event.data1 = KEYD_MOUSE3;
            break;
         case SDL_BUTTON_RIGHT:
            sendmouseevent = 1;
            buttons &= ~2;
            d_event.data1 = KEYD_MOUSE2;
            break;
         case SDL_BUTTON_X1:
            d_event.data1 = KEYD_MOUSE4;
            break;
         case SDL_BUTTON_X2:
            d_event.data1 = KEYD_MOUSE5;
            break;
         }

         if(d_event.data1)
            D_PostEvent(&d_event);
         break;

      case SDL_QUIT:
         MN_QuitDoom();
         break;

      case SDL_WINDOWEVENT:
         // haleyjd 10/08/05: from Chocolate DOOM:
         // need to update our focus state
         // 2/14/2011: Update mouse grabbing as well (thanks Catoptromancy)
         UpdateFocus(window);
         UpdateGrab(window);
         break;

      default:
         break;
      }
   }

   if(sendmouseevent)
   {
      mouseevent.data1 = buttons;
      D_PostEvent(&mouseevent);
   }

   // SoM: if paused, delay for a short amount of time to allow other threads
   // to process on the system. Otherwise Eternity will use almost 100% of the
   // CPU even while paused.
   if(paused || !window_focused)
      SDL_Delay(1);
}

//
// I_StartTicInWindow
//
void I_StartTicInWindow(SDL_Window *window)
{
   I_RunDeferredEvents();
   I_GetEvent(window);
   I_UpdateHaptics();

   if(usemouse && ((mouseAccel_type == 2) || (mouseAccel_type == 3)))
      I_ReadMouse(window);
}

// EOF

