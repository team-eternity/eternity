//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Max Waine, et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Keyboard, mouse, and joystick code.
// Authors: James Haley, Max Waine
//

#ifdef __APPLE__
#include "SDL2/SDL.h"
#else
#include "SDL.h"
#endif

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
#include "../i_video.h"
#include "../m_argv.h"
#include "../m_dllist.h"
#include "../p_chase.h"
#include "../v_misc.h"
#include "../v_video.h"

// Grab the mouse? (int type for config code)
int        grabmouse;
extern int usemouse; // killough 10/98

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen
bool screenvisible;
bool window_focused;
bool fullscreen;

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
    bool        grab;

    grab = MouseShouldBeGrabbed();

    if(grab && !currently_grabbed)
    {
        // When the cursor is hidden, grab the input.
        // Relative mode implicitly hides the cursor.
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetWindowGrab(window, SDL_TRUE);

        // Do this to prevent mouse-acceleration from moving player when exiting menu.
        SDL_GetRelativeMouseState(nullptr, nullptr);
    }
    else if(!grab && currently_grabbed)
    {
        int window_w, window_h;

        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_SetWindowGrab(window, SDL_FALSE);

        // When releasing the mouse from grab, warp the mouse cursor to
        // the bottom-right of the screen. This is a minimally distracting
        // place for it to appear - we may only have released the grab
        // because we're at an end of level intermission screen, for
        // example.

        SDL_GetWindowSize(window, &window_w, &window_h);

        // Disable handling the mouse during this action
        SDL_EventState(SDL_MOUSEMOTION, SDL_DISABLE);
        SDL_WarpMouseInWindow(window, window_w - 16, window_h - 16);
        SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
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
    if(((menuactive || consoleactive) && !netgame) || (paused && !walkcam_active))
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

    state         = window ? SDL_GetWindowFlags(window) : 0;
    screenvisible = ((state & SDL_WINDOW_SHOWN) && !(state & SDL_WINDOW_MINIMIZED));

    window_focused = (screenvisible &&
                      ((state & (SDL_WINDOW_INPUT_GRABBED | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS)) != 0));
}

//=============================================================================
//
// Keyboard
//

// Adapted for Eternity, based on Chocolate Doom
#define SCANCODE_TO_KEYS_ARRAY {                                                             \
    0,   0,   0,   0,   'a',                                                   /* 0-9 */     \
    'b', 'c', 'd', 'e', 'f',                                                                 \
    'g', 'h', 'i', 'j', 'k',                                                   /* 10-19 */   \
    'l', 'm', 'n', 'o', 'p',                                                                 \
    'q', 'r', 's', 't', 'u',                                                   /* 20-29 */   \
    'v', 'w', 'x', 'y', 'z',                                                                 \
    '1', '2', '3', '4', '5',                                                   /* 30-39 */   \
    '6', '7', '8', '9', '0',                                                                 \
    KEYD_ENTER, KEYD_ESCAPE, KEYD_BACKSPACE, KEYD_TAB, ' ',                    /* 40-49 */   \
    KEYD_MINUS, KEYD_EQUALS, '[', ']', '\\',                                                 \
    KEYD_NONUSHASH,   ';', '\'', KEYD_ACCGRAVE, ',',                           /* 50-59 */   \
    '.', '/', KEYD_CAPSLOCK, KEYD_F1, KEYD_F2,                                               \
    KEYD_F3, KEYD_F4, KEYD_F5, KEYD_F6, KEYD_F7,                               /* 60-69 */   \
    KEYD_F8, KEYD_F9, KEYD_F10, KEYD_F11, KEYD_F12,                                          \
    KEYD_PRINTSCREEN, KEYD_SCROLLLOCK, KEYD_PAUSE, KEYD_INSERT, KEYD_HOME,     /* 70-79 */   \
    KEYD_PAGEUP, KEYD_DEL, KEYD_END, KEYD_PAGEDOWN, KEYD_RIGHTARROW,                         \
    KEYD_LEFTARROW, KEYD_DOWNARROW, KEYD_UPARROW,                              /* 80-89 */   \
    KEYD_NUMLOCK, KEYD_KPDIVIDE,                                                             \
    KEYD_KPMULTIPLY, KEYD_KPMINUS, KEYD_KPPLUS, KEYD_KPENTER, KEYD_KP1,                      \
    KEYD_KP2, KEYD_KP3, KEYD_KP4, KEYD_KP5, KEYD_KP6,                          /* 90-99 */   \
    KEYD_KP7, KEYD_KP8, KEYD_KP9, KEYD_KP0, KEYD_KPPERIOD,                                   \
    KEYD_NONUSBACKSLASH, 0, 0, KEYD_KPEQUALS,                                  /* 100-103 */ \
}

static const int scancode_translate_table[] = SCANCODE_TO_KEYS_ARRAY;

//
// I_TranslateKey
//
// For SDL, translates from SDL keysyms to DOOM key values.
//
static int I_TranslateKey(SDL_Keysym *sym)
{
    const int scancode = sym->scancode;

    // This approach is taken from Chocolate Doom's TranslateKey
    switch(scancode)
    {
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL: //
        return KEYD_RCTRL;
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT: //
        return KEYD_RSHIFT;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_LGUI: //
        return KEYD_LALT;
    case SDL_SCANCODE_RALT:
    case SDL_SCANCODE_RGUI: //
        return KEYD_RALT;
    default:
        if(scancode >= 0 && scancode < static_cast<int>(earrlen(scancode_translate_table)))
            return scancode_translate_table[scancode];
        else
            return 0;
    }
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
        event_t ev = {};

        if(padstate->buttons[button] != padstate->prevbuttons[button])
        {
            ev.type  = padstate->buttons[button] ? ev_keydown : ev_keyup;
            ev.data1 = KEYD_JOY_BASE + button;
            D_PostEvent(&ev);
        }
    }

    // read axes
    for(int axis = 0; axis < HALGamePad::MAXAXES; axis++)
    {
        // fire axis state change events
        if(padstate->axes[axis] != padstate->prevaxes[axis])
        {
            event_t ev = {};

            // if previous state was off, key down
            if(padstate->prevaxes[axis] == 0.0)
                ev.type = ev_keydown;

            // if new state is off, key up
            if(padstate->axes[axis] == 0.0)
                ev.type = ev_keyup;

            ev.data1 = KEYD_AXIS_LEFT_X + axis;
            D_PostEvent(&ev);
        }

        // post analog axis state
        event_t ev = {};
        ev.type    = ev_joystick;
        ev.data1   = axis;
        ev.data2   = padstate->axes[axis];
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

extern acceltype_e mouseAccel_type;
extern int         mouseAccel_threshold;
extern double      mouseAccel_value;

//
// Mouse acceleration
//
// This emulates some of the behavior of DOS mouse drivers by increasing
// the speed when the mouse is moved fast.
//
// The mouse input values are input directly to the game, but when
// the values exceed the value of mouse_threshold, they are multiplied
// by mouse_acceleration to increase the speed.

float mouse_acceleration = 2.0f;
int   mouse_threshold    = 10;

//
// AccelerateMouse
//
// haleyjd 10/23/08: From Choco-Doom
//
static int AccelerateMouse(int val)
{
    if(val < 0)
        return -AccelerateMouse(-val);

    return (val > mouse_threshold) ? (int)((val - mouse_threshold) * mouse_acceleration + mouse_threshold) : val;
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
    int          x, y;
    event_t      ev;
    static Uint8 previous_state = 137;
    Uint8        state;

    SDL_PumpEvents();

    state = SDL_GetRelativeMouseState(&x, &y);

    if(state != previous_state)
        previous_state = state;

    if(x != 0 || y != 0)
    {
        ev.type  = ev_mouse;
        ev.data1 = SDL_MOUSEMOTION;
        if(mouseAccel_type == ACCELTYPE_CHOCO)
        {
            // SoM: So the values that go to Eternity should be 16.16 fixed point...
            ev.data2 = AccelerateMouse(x);
            ev.data3 = -AccelerateMouse(y);
        }
        else if(mouseAccel_type == ACCELTYPE_CUSTOM) // [CG] 01/20/12 Custom acceleration
        {
            ev.data2 = CustomAccelerateMouse(x);
            ev.data3 = -CustomAccelerateMouse(y);
        }

        D_PostEvent(&ev);
    }
}

//
// I_InitMouse
//
// Once upon a time this function existed in vanilla DOOM, and now here it is again.
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
    event_t                     ev;
    int                         tic;
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
    static int                   lasttic;

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

static void I_getEvent(SDL_Window *window)
{
    SDL_Event ev;
    int       sendmouseevent = 0;
    int       buttons        = 0;
    event_t   mouseevent     = { ev_mouse, 0, 0, 0, false };

    // [CG] 01/31/2012: Ensure we have the latest info about focus and mouse grabbing.
    UpdateFocus(window);
    UpdateGrab(window);

    while(SDL_PollEvent(&ev))
    {
        // haleyjd 10/08/05: from Chocolate DOOM
        if(!window_focused &&
           (ev.type == SDL_MOUSEMOTION || ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP))
        {
            continue;
        }

        switch(ev.type)
        {
        case SDL_TEXTINPUT:
            for(unsigned int i = 0; i < SDL_strlen(ev.text.text); i++)
            {
                const char currchar = ev.text.text[i];
                if(ectype::isPrint(currchar))
                {
                    const event_t event_text = { ev_text, currchar, 0, 0, false };
                    D_PostEvent(&event_text);
                }
            }
            break;
        case SDL_KEYDOWN:
        {
            const event_t event_keyDown = { ev_keydown, I_TranslateKey(&ev.key.keysym), 0, 0, !!ev.key.repeat };

#if (EE_CURRENT_PLATFORM != EE_PLATFORM_MACOSX)
            // This quick exit code is adapted from PRBoom+
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
                else if(ev.key.keysym.scancode == SDL_SCANCODE_RETURN)
                {
                    I_ToggleFullscreen();
                    break;
                }
            }
#else
            // Also provide macOS option for quick exit and fullscreen toggle
            if(ev.key.keysym.mod & KMOD_GUI)
            {
                if(ev.key.keysym.scancode == SDL_SCANCODE_Q)
                {
                    I_QuitFast();
                    break;
                }
                else if(ev.key.keysym.scancode == SDL_SCANCODE_F)
                {
                    I_ToggleFullscreen();
                    break;
                }
            }
#endif

            // MaxW: 2017/10/12: Removed deferred event adding for caps lock
            // MaxW: 2017/10/18: Removed character input
            D_PostEvent(&event_keyDown);
            break;
        }
        case SDL_KEYUP:
        {
            const event_t event_keyUp = { ev_keyup, I_TranslateKey(&ev.key.keysym), 0, 0, false };

            D_PostEvent(&event_keyUp);
            break;
        }
        case SDL_MOUSEMOTION:
            if(!usemouse || ((mouseAccel_type == ACCELTYPE_CHOCO) || (mouseAccel_type == ACCELTYPE_CUSTOM)))
                continue;

            // haleyjd 06/14/10: no mouse motion at startup.
            if(gametic == 0)
                continue;

            // SoM 1-20-04 Ok, use xrel/yrel for mouse movement because most
            // people like it the most.
            if(mouseAccel_type == ACCELTYPE_NONE)
            {
                mouseevent.data2 += ev.motion.xrel;
                mouseevent.data3 -= ev.motion.yrel;
            }
            else if(mouseAccel_type == ACCELTYPE_LINEAR)
            {
                // Simple linear acceleration
                // Evaluates to 1.25 * x. So Why don't I just do that? .... shut up
                mouseevent.data2 += (ev.motion.xrel + (float)(ev.motion.xrel * 0.25f));
                mouseevent.data3 -= (ev.motion.yrel + (float)(ev.motion.yrel * 0.25f));
            }

            sendmouseevent = 1;
            break;

        case SDL_MOUSEBUTTONDOWN:
        {
            if(!usemouse)
                continue;

            event_t event_MouseButtonDown = { ev_keydown, 0, 0, 0, false };

            switch(ev.button.button)
            {
            case SDL_BUTTON_LEFT:
                sendmouseevent               = 1;
                buttons                     |= 1;
                event_MouseButtonDown.data1  = KEYD_MOUSE1;
                break;
            case SDL_BUTTON_MIDDLE:
                // haleyjd 05/28/06: swapped MOUSE3/MOUSE2
                sendmouseevent               = 1;
                buttons                     |= 4;
                event_MouseButtonDown.data1  = KEYD_MOUSE3;
                break;
            case SDL_BUTTON_RIGHT:
                sendmouseevent               = 1;
                buttons                     |= 2;
                event_MouseButtonDown.data1  = KEYD_MOUSE2;
                break;
            case SDL_BUTTON_X1: //
                event_MouseButtonDown.data1 = KEYD_MOUSE4;
                break;
            case SDL_BUTTON_X2: //
                event_MouseButtonDown.data1 = KEYD_MOUSE5;
                break;
            case SDL_BUTTON_X2 + 1: //
                event_MouseButtonDown.data1 = KEYD_MOUSE6;
                break;
            case SDL_BUTTON_X2 + 2: //
                event_MouseButtonDown.data1 = KEYD_MOUSE7;
                break;
            case SDL_BUTTON_X2 + 3: //
                event_MouseButtonDown.data1 = KEYD_MOUSE8;
                break;
            }

            D_PostEvent(&event_MouseButtonDown);
            break;
        }
        case SDL_MOUSEWHEEL:
        {
            if(!usemouse)
                continue;

            event_t event_mouseWheelDown = { ev_keydown, 0, 0, 0, false };
            event_t event_mouseWheelUp   = { ev_keyup, 0, 0, 0, false };

            // SDL_TODO: Allow y to correspond to # of weps scrolled through?
            if(ev.wheel.y > 0)
            {
                event_mouseWheelDown.data1 = KEYD_MWHEELUP;
                D_PostEvent(&event_mouseWheelDown);
                // WHEELUP sends a button up event immediately. That won't work;
                // we need an input latency gap of at least one gametic.
                event_mouseWheelUp.data1 = KEYD_MWHEELUP;
                I_AddDeferredEvent(event_mouseWheelUp, gametic + 1);
                break;
            }
            else if(ev.wheel.y < 0)
            {
                event_mouseWheelDown.data1 = KEYD_MWHEELDOWN;
                D_PostEvent(&event_mouseWheelDown);
                // ditto, as above.
                event_mouseWheelUp.data1 = KEYD_MWHEELDOWN;
                I_AddDeferredEvent(event_mouseWheelUp, gametic + 1);
                break;
            }

            break;
        }
        case SDL_MOUSEBUTTONUP:
        {
            if(!usemouse)
                continue;

            event_t event_mouseButtonUp = { ev_keyup, 0, 0, 0, false };

            switch(ev.button.button)
            {
            case SDL_BUTTON_LEFT:
                sendmouseevent             = 1;
                buttons                   &= ~1;
                event_mouseButtonUp.data1  = KEYD_MOUSE1;
                break;
            case SDL_BUTTON_MIDDLE:
                // haleyjd 05/28/06: swapped MOUSE3/MOUSE2
                sendmouseevent             = 1;
                buttons                   &= ~4;
                event_mouseButtonUp.data1  = KEYD_MOUSE3;
                break;
            case SDL_BUTTON_RIGHT:
                sendmouseevent             = 1;
                buttons                   &= ~2;
                event_mouseButtonUp.data1  = KEYD_MOUSE2;
                break;
            case SDL_BUTTON_X1: //
                event_mouseButtonUp.data1 = KEYD_MOUSE4;
                break;
            case SDL_BUTTON_X2: //
                event_mouseButtonUp.data1 = KEYD_MOUSE5;
                break;
            }

            if(event_mouseButtonUp.data1)
                D_PostEvent(&event_mouseButtonUp);
            break;
        }
        case SDL_QUIT:
        {
            static const event_t event_quit = { ev_quit };
            D_PostEvent(&event_quit);
        }
        break;

        case SDL_WINDOWEVENT:
            // haleyjd 10/08/05: from Chocolate DOOM:
            // need to update our focus state
            // 2/14/2011: Update mouse grabbing as well (thanks Catoptromancy)
            UpdateFocus(window);
            UpdateGrab(window);
            break;

        default: //
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
    I_getEvent(window);
    I_UpdateHaptics();

    if(usemouse && ((mouseAccel_type == ACCELTYPE_CHOCO) || (mouseAccel_type == ACCELTYPE_CUSTOM)))
        I_ReadMouse(window);
}

// EOF

