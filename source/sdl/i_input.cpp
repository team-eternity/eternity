// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//
//   keyboard, mouse, and joystick code.
//
//-----------------------------------------------------------------------------

#include "SDL.h"

#include "../hal/i_platform.h"

#include "../c_io.h"
#include "../c_runcmd.h"
#include "../d_event.h"
#include "../d_main.h"
#include "../doomdef.h"
#include "../doomstat.h"
#include "../g_bind.h"
#include "../g_game.h"
#include "../m_argv.h"
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

bool MouseShouldBeGrabbed(void);

//=============================================================================
//
// WM
//

//
// UpdateGrab
//
// haleyjd 10/08/05: from Chocolate DOOM
//
void UpdateGrab(void)
{
   static bool currently_grabbed = false;
   bool grab;

   grab = MouseShouldBeGrabbed();

   if(grab && !currently_grabbed)
   {
      SDL_ShowCursor(SDL_DISABLE);
      SDL_WM_GrabInput(SDL_GRAB_ON);
   }

   if(!grab && currently_grabbed)
   {
      SDL_ShowCursor(SDL_ENABLE);
      SDL_WM_GrabInput(SDL_GRAB_OFF);
   }

   currently_grabbed = grab;
}

//
// MouseShouldBeGrabbed
//
// haleyjd 10/08/05: From Chocolate DOOM, fairly self-explanatory.
//
bool MouseShouldBeGrabbed(void)
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

   // when menu is active or game is paused, release the mouse
   if(menuactive || consoleactive || paused)
      return false;

   // only grab mouse when playing levels (but not demos)
   return (G_GameStateIs(GS_LEVEL)) && !demoplayback;
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
void UpdateFocus(void)
{
   Uint8 state;
   SDL_Event  event;
   static bool currently_focused = false;

   SDL_PumpEvents();

   state = SDL_GetAppState();

   // We should have input (keyboard) focus and be visible
   // (not minimised)
   window_focused = (state & SDL_APPINPUTFOCUS) && (state & SDL_APPACTIVE);

   // Should the screen be grabbed?
   screenvisible = (state & SDL_APPACTIVE) != 0;

   // [CG] Handle focus changes, this is all necessary to avoid repeat events.
   if(currently_focused != window_focused)
   {
      if(window_focused)
      {
         while(SDL_PollEvent(&event)) {}
         SDL_EnableKeyRepeat(
            SDL_DEFAULT_REPEAT_DELAY / 2, SDL_DEFAULT_REPEAT_INTERVAL * 4
         );
      }
      else
         SDL_EnableKeyRepeat(0, 0);
      G_ClearKeyStates();
      currently_focused = window_focused;
   }
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
static int I_TranslateKey(int sym)
{
   int rc = 0;

   switch(sym)
   {
   case SDLK_LEFT:        rc = KEYD_LEFTARROW;  break;
   case SDLK_RIGHT:       rc = KEYD_RIGHTARROW; break;
   case SDLK_DOWN:        rc = KEYD_DOWNARROW;  break;
   case SDLK_UP:          rc = KEYD_UPARROW;    break;
   case SDLK_ESCAPE:      rc = KEYD_ESCAPE;     break;
   case SDLK_RETURN:      rc = KEYD_ENTER;      break;
   case SDLK_TAB:         rc = KEYD_TAB;        break;
   case SDLK_F1:          rc = KEYD_F1;         break;
   case SDLK_F2:          rc = KEYD_F2;         break;
   case SDLK_F3:          rc = KEYD_F3;         break;
   case SDLK_F4:          rc = KEYD_F4;         break;
   case SDLK_F5:          rc = KEYD_F5;         break;
   case SDLK_F6:          rc = KEYD_F6;         break;
   case SDLK_F7:          rc = KEYD_F7;         break;
   case SDLK_F8:          rc = KEYD_F8;         break;
   case SDLK_F9:          rc = KEYD_F9;         break;
   case SDLK_F10:         rc = KEYD_F10;        break;
   case SDLK_F11:         rc = KEYD_F11;        break;
   case SDLK_F12:         rc = KEYD_F12;        break;
   case SDLK_BACKSPACE:   rc = KEYD_BACKSPACE;  break;
   case SDLK_PAUSE:       rc = KEYD_PAUSE;      break;
   case SDLK_EQUALS:      rc = KEYD_EQUALS;     break;
   case SDLK_MINUS:       rc = KEYD_MINUS;      break;

   case SDLK_KP0:         rc = KEYD_KP0;        break;
   case SDLK_KP1:         rc = KEYD_KP1;        break;
   case SDLK_KP2:         rc = KEYD_KP2;        break;
   case SDLK_KP3:         rc = KEYD_KP3;        break;
   case SDLK_KP4:         rc = KEYD_KP4;        break;
   case SDLK_KP5:         rc = KEYD_KP5;        break;
   case SDLK_KP6:         rc = KEYD_KP6;        break;
   case SDLK_KP7:         rc = KEYD_KP7;        break;
   case SDLK_KP8:         rc = KEYD_KP8;        break;
   case SDLK_KP9:         rc = KEYD_KP9;        break;
   case SDLK_KP_PERIOD:   rc = KEYD_KPPERIOD;   break;
   case SDLK_KP_DIVIDE:   rc = KEYD_KPDIVIDE;   break;
   case SDLK_KP_MULTIPLY: rc = KEYD_KPMULTIPLY; break;
   case SDLK_KP_MINUS:    rc = KEYD_KPMINUS;    break;
   case SDLK_KP_PLUS:     rc = KEYD_KPPLUS;     break;
   case SDLK_KP_ENTER:    rc = KEYD_KPENTER;    break;
   case SDLK_KP_EQUALS:   rc = KEYD_KPEQUALS;   break;

   case SDLK_NUMLOCK:     rc = KEYD_NUMLOCK;    break;
   case SDLK_SCROLLOCK:   rc = KEYD_SCROLLLOCK; break;
   case SDLK_CAPSLOCK:    rc = KEYD_CAPSLOCK;   break;
   case SDLK_LSHIFT:
   case SDLK_RSHIFT:      rc = KEYD_RSHIFT;     break;
   case SDLK_LCTRL:
   case SDLK_RCTRL:       rc = KEYD_RCTRL;      break;

   case SDLK_LALT:
   case SDLK_RALT:
   case SDLK_LMETA:
   case SDLK_RMETA:       rc = KEYD_RALT;       break;
   case SDLK_PAGEUP:      rc = KEYD_PAGEUP;     break;
   case SDLK_PAGEDOWN:    rc = KEYD_PAGEDOWN;   break;
   case SDLK_HOME:        rc = KEYD_HOME;       break;
   case SDLK_END:         rc = KEYD_END;        break;
   case SDLK_INSERT:      rc = KEYD_INSERT;     break;
   case SDLK_DELETE:      rc = KEYD_DEL;        break;

   case SDLK_PRINT:       rc = KEYD_PRINT;      break;
   case SDLK_SYSREQ:      rc = KEYD_SYSRQ;      break;

   default:
      rc = sym;
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

extern SDL_Joystick *sdlJoystick;
extern int          usejoystick;
extern int          joystickpresent;
extern int          sdlJoystickNumButtons;

int joystickSens_x;
int joystickSens_y;

static int keyForButtonNum[8] =
{
   KEYD_JOY1, KEYD_JOY2, KEYD_JOY3, KEYD_JOY4,
   KEYD_JOY5, KEYD_JOY6, KEYD_JOY7, KEYD_JOY8
};

//
// I_JoystickEvents
//
// Gathers joystick data and creates an event_t for later processing
// by G_Responder().
//
static void I_JoystickEvents(void)
{
   // haleyjd 04/15/02: SDL joystick support

   event_t event;
   int i, joyb[8];
   Sint16 joy_x, joy_y;
   static int old_joyb[8];

   memset(joyb, 0, sizeof(joyb));

   if(!joystickpresent || !usejoystick || !sdlJoystick ||
      !sdlJoystickNumButtons)
      return;

   SDL_JoystickUpdate(); // read the current joystick settings
   event.type = ev_joystick;
   event.data1 = 0;

   // read the button settings
   for(i = 0; i < 8 && i < sdlJoystickNumButtons; ++i)
   {
      if((joyb[i] = SDL_JoystickGetButton(sdlJoystick, i)))
         event.data1 |= (1 << i);
   }

   // Read the x,y settings. Convert to -1 or 0 or +1.
   joy_x = SDL_JoystickGetAxis(sdlJoystick, 0);
   joy_y = SDL_JoystickGetAxis(sdlJoystick, 1);

   if(joy_x < -joystickSens_x)
      event.data2 = -1;
   else if(joy_x > joystickSens_x)
      event.data2 = 1;
   else
      event.data2 = 0;

   if(joy_y < -joystickSens_y)
      event.data3 = -1;
   else if(joy_y > joystickSens_y)
      event.data3 = 1;
   else
      event.data3 = 0;

   // post what you found

   D_PostEvent(&event);

   // build button events (make joystick buttons virtual keyboard keys
   // as originally suggested by lee killough in the boom suggestions file)

   for(i = 0; i < 8; ++i)
   {
      if(joyb[i] != old_joyb[i])
      {
         event.type  = joyb[i] ? ev_keydown : ev_keyup;
         event.data1 = keyForButtonNum[i];
         D_PostEvent(&event);
         old_joyb[i] = joyb[i];
      }
   }
}


//
// I_StartFrame
//
void I_StartFrame(void)
{
   I_JoystickEvents(); // Obtain joystick data                 phares 4/3/98
}

//=============================================================================
//
// Mouse
//

extern void MN_QuitDoom(void);
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
static void CenterMouse(void)
{
   // Warp the the screen center

   SDL_WarpMouse((Uint16)(video.width / 2), (Uint16)(video.height / 2));

   // Clear any relative movement caused by warping

   SDL_PumpEvents();
   SDL_GetRelativeMouseState(NULL, NULL);
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
static void I_ReadMouse(void)
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
         ev.data2 = AccelerateMouse(x);
         ev.data3 = -AccelerateMouse(y);
      }
      else if(mouseAccel_type == 3) // [CG] 01/20/12 Custom acceleration
      {
         ev.data2 = CustomAccelerateMouse(x);
         ev.data3 = -CustomAccelerateMouse(y);
      }

      D_PostEvent(&ev);
   }

   if(MouseShouldBeGrabbed())
      CenterMouse();
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

static void I_GetEvent(void)
{
   static int    buttons = 0;
   static Uint32 mwheeluptic = 0, mwheeldowntic = 0;

#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
   static Uint32 capslocktic = 0;
#endif
   Uint32        calltic;

   SDL_Event  event;
   int        sendmouseevent = 0;
   event_t    d_event        = { ev_keydown, 0, 0, 0 };
   event_t    mouseevent     = { ev_mouse,   0, 0, 0 };

   calltic = gametic;

   // [CG] 01/31/2012: Ensure we have the latest info about focus and mouse
   //                  grabbing.
   UpdateFocus();
   UpdateGrab();

   while(SDL_PollEvent(&event))
   {
      // haleyjd 10/08/05: from Chocolate DOOM
      if(!window_focused &&
         (event.type == SDL_MOUSEMOTION     ||
          event.type == SDL_MOUSEBUTTONDOWN ||
          event.type == SDL_MOUSEBUTTONUP))
      {
         continue;
      }

      switch(event.type)
      {
      case SDL_KEYDOWN:
         d_event.type = ev_keydown;
         d_event.data1 = I_TranslateKey(event.key.keysym.sym);
#if (EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS)
         if(d_event.data1 == KEYD_CAPSLOCK)
            capslocktic = gametic;
#endif
         if(unicodeinput &&
            event.key.keysym.unicode > 31 && event.key.keysym.unicode < 127)
            d_event.character = (char)(event.key.keysym.unicode);
         else
            d_event.character = 0;

         D_PostEvent(&d_event);
         break;

      case SDL_KEYUP:
         d_event.type = ev_keyup;
         d_event.data1 = I_TranslateKey(event.key.keysym.sym);
#if (EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS)
         if(d_event.data1 == KEYD_CAPSLOCK)
         {
            d_event.type = ev_keydown;
            capslocktic = gametic;
         }
#endif
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
            mouseevent.data2 += event.motion.xrel;
            mouseevent.data3 -= event.motion.yrel;
         }
         else if(mouseAccel_type == 1)
         {
            // Simple linear acceleration
            // Evaluates to 1.25 * x. So Why don't I just do that? .... shut up
            mouseevent.data2 += (event.motion.xrel +
                                 (float)(event.motion.xrel * 0.25f));
            mouseevent.data3 -= (event.motion.yrel +
                                 (float)(event.motion.yrel * 0.25f));
         }

         sendmouseevent = 1;
         break;

      case SDL_MOUSEBUTTONDOWN:
         if(!usemouse)
            continue;
         d_event.type =  ev_keydown;

         switch(event.button.button)
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
         case SDL_BUTTON_WHEELUP:
            d_event.data1 = KEYD_MWHEELUP;
            mwheeluptic = calltic;
            break;
         case SDL_BUTTON_WHEELDOWN:
            d_event.data1 = KEYD_MWHEELDOWN;
            mwheeldowntic = calltic;
            break;
#if SDL_VERSION_ATLEAST(1, 2, 14)
         case SDL_BUTTON_X1:
            d_event.data1 = KEYD_MOUSE4;
            break;
         case SDL_BUTTON_X2:
            d_event.data1 = KEYD_MOUSE5;
            break;
#endif
         }

         D_PostEvent(&d_event);
         break;

      case SDL_MOUSEBUTTONUP:
         if(!usemouse)
            continue;
         d_event.type = ev_keyup;
         d_event.data1 = 0;

         switch(event.button.button)
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
         case SDL_BUTTON_WHEELUP:
            break;
         case SDL_BUTTON_WHEELDOWN:
            break;
#if SDL_VERSION_ATLEAST(1, 2, 14)
         case SDL_BUTTON_X1:
            d_event.data1 = KEYD_MOUSE4;
            break;
         case SDL_BUTTON_X2:
            d_event.data1 = KEYD_MOUSE5;
            break;
#endif
         }

         if(d_event.data1)
            D_PostEvent(&d_event);
         break;

      case SDL_QUIT:
         MN_QuitDoom();
         break;

      case SDL_ACTIVEEVENT:
         // haleyjd 10/08/05: from Chocolate DOOM:
         // need to update our focus state
         // 2/14/2011: Update mouse grabbing as well (thanks Catoptromancy)
         UpdateFocus();
         UpdateGrab();
         break;

      default:
         break;
      }
   }

   if(mwheeluptic && mwheeluptic + 1 < calltic)
   {
      d_event.type = ev_keyup;
      d_event.data1 = KEYD_MWHEELUP;
      D_PostEvent(&d_event);
      mwheeluptic = 0;
   }

   if(mwheeldowntic && mwheeldowntic + 1 < calltic)
   {
      d_event.type = ev_keyup;
      d_event.data1 = KEYD_MWHEELDOWN;
      D_PostEvent(&d_event);
      mwheeldowntic = 0;
   }

#if (EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS)
   if(capslocktic && capslocktic + 1 < calltic)
   {
      d_event.type  = ev_keyup;
      d_event.data1 = KEYD_CAPSLOCK;
      D_PostEvent(&d_event);
      capslocktic = 0;
   }
#endif

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
// I_StartTic
//
void I_StartTic(void)
{
   I_GetEvent();

   if(usemouse && ((mouseAccel_type == 2) || (mouseAccel_type == 3)))
      I_ReadMouse();
}

// EOF

