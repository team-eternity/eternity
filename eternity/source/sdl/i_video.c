// Emacs style mode select   -*- C++ -*-
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
//   System-specific graphics code, along with some ill-placed
//   keyboard, mouse, and joystick code.
//
//-----------------------------------------------------------------------------

#include "SDL.h"

#include <stdio.h>

#include "../z_zone.h"  /* memory allocation wrappers -- killough */
#include "../doomstat.h"
#include "../am_map.h"
#include "../c_io.h"
#include "../c_runcmd.h"
#include "../d_main.h"
#include "../f_wipe.h"
#include "../i_video.h"
#include "../m_argv.h"
#include "../m_bbox.h"
#include "../m_qstr.h"
#include "../mn_engin.h"
#include "../r_draw.h"
#include "../st_stuff.h"
#include "../v_video.h"
#include "../w_wad.h"
#include "../z_zone.h"
#include "../version.h"

// haleyjd 04/15/02:
#include "../i_system.h"
#include "../in_lude.h"

extern SDL_Joystick *sdlJoystick;

// haleyjd 10/08/05: Chocolate DOOM application focus state code added

// Grab the mouse? (int type for config code)
int grabmouse;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen
boolean screenvisible;
static boolean window_focused;
static boolean fullscreen;

SDL_Surface *sdlscreen;


// SoM: Added empty cursor code from Choclate DOOM with a little
// modification... It seems the cursors were being created each time the
// graphics mode was set. In EE, this is not an option because 
// I_InitGraphicsMode is called each time the screen resolution changes. This
// could result in a memory leak... 
// Empty mouse cursor
static SDL_Cursor *cursors[2] = {NULL, NULL};

///////////////////////////////////////////////////////////////////////////
//
// Input Code
//
//////////////////////////////////////////////////////////////////////////

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
// MouseShouldBeGrabbed
//
// haleyjd 10/08/05: From Chocolate DOOM, fairly self-explanatory.
//
static boolean MouseShouldBeGrabbed(void)
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
   if(menuactive || paused)
      return false;
   
   // only grab mouse when playing levels (but not demos)
   return (gamestate == GS_LEVEL) && !demoplayback;
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
static void UpdateFocus(void)
{
   Uint8 state = SDL_GetAppState();
   
   // We should have input (keyboard) focus and be visible 
   // (not minimised)   
   window_focused = (state & SDL_APPINPUTFOCUS) && (state & SDL_APPACTIVE);
   
   // Should the screen be grabbed?   
   screenvisible = (state & SDL_APPACTIVE) != 0;
}

extern void I_InitKeyboard();      // i_system.c

//
// I_TranslateKey
//
// For SDL, translates from SDL keysyms to DOOM key values.
//
static int I_TranslateKey(int sym)
{
   int rc = 0;
   switch (sym) 
   {  
   case SDLK_LEFT:     rc = KEYD_LEFTARROW;  break;
   case SDLK_RIGHT:    rc = KEYD_RIGHTARROW; break;
   case SDLK_DOWN:     rc = KEYD_DOWNARROW;  break;
   case SDLK_UP:       rc = KEYD_UPARROW;    break;
   case SDLK_ESCAPE:   rc = KEYD_ESCAPE;     break;
   case SDLK_RETURN:   rc = KEYD_ENTER;      break;
   case SDLK_TAB:      rc = KEYD_TAB;        break;
   case SDLK_F1:       rc = KEYD_F1;         break;
   case SDLK_F2:       rc = KEYD_F2;         break;
   case SDLK_F3:       rc = KEYD_F3;         break;
   case SDLK_F4:       rc = KEYD_F4;         break;
   case SDLK_F5:       rc = KEYD_F5;         break;
   case SDLK_F6:       rc = KEYD_F6;         break;
   case SDLK_F7:       rc = KEYD_F7;         break;
   case SDLK_F8:       rc = KEYD_F8;         break;
   case SDLK_F9:       rc = KEYD_F9;         break;
   case SDLK_F10:      rc = KEYD_F10;        break;
   case SDLK_F11:      rc = KEYD_F11;        break;
   case SDLK_F12:      rc = KEYD_F12;        break;
   case SDLK_BACKSPACE:
   // FIXME/TODO: delete
   case SDLK_DELETE:   rc = KEYD_BACKSPACE;  break;
   case SDLK_PAUSE:    rc = KEYD_PAUSE;      break;
   case SDLK_EQUALS:   rc = KEYD_EQUALS;     break;
   case SDLK_MINUS:    rc = KEYD_MINUS;      break;
      
   // FIXME/TODO: numeric keypad
   //  case SDLK_KP0:      rc = KEYD_KEYPAD0;	break;
   //  case SDLK_KP1:      rc = KEYD_KEYPAD1;	break;
   //  case SDLK_KP2:      rc = KEYD_KEYPAD2;	break;
   //  case SDLK_KP3:      rc = KEYD_KEYPAD3;	break;
   //  case SDLK_KP4:      rc = KEYD_KEYPAD4;	break;
   //  case SDLK_KP5:      rc = KEYD_KEYPAD0;	break;
   //  case SDLK_KP6:      rc = KEYD_KEYPAD6;	break;
   //  case SDLK_KP7:      rc = KEYD_KEYPAD7;	break;
   //  case SDLK_KP8:      rc = KEYD_KEYPAD8;	break;
   //  case SDLK_KP9:      rc = KEYD_KEYPAD9;	break;
   //  case SDLK_KP_PLUS:  rc = KEYD_KEYPADPLUS;	break;
   //  case SDLK_KP_MINUS: rc = KEYD_KEYPADMINUS;	break;
   //  case SDLK_KP_DIVIDE:	rc = KEYD_KEYPADDIVIDE;	break;
   //  case SDLK_KP_MULTIPLY: rc = KEYD_KEYPADMULTIPLY; break;
   //  case SDLK_KP_ENTER:	rc = KEYD_KEYPADENTER;	break;
   //  case SDLK_KP_PERIOD:	rc = KEYD_KEYPADPERIOD;	break;
      
   case SDLK_NUMLOCK:    rc = KEYD_NUMLOCK;  break;
   case SDLK_SCROLLOCK:  rc = KEYD_SCROLLLOCK; break;
   case SDLK_CAPSLOCK:   rc = KEYD_CAPSLOCK; break;
   case SDLK_LSHIFT:
   case SDLK_RSHIFT:     rc = KEYD_RSHIFT;   break;
   case SDLK_LCTRL:
   case SDLK_RCTRL:      rc = KEYD_RCTRL;    break;
      
   case SDLK_LALT:
   case SDLK_RALT:
   case SDLK_LMETA:
   case SDLK_RMETA:      rc = KEYD_RALT;     break;
   case SDLK_PAGEUP:     rc = KEYD_PAGEUP;   break;
   case SDLK_PAGEDOWN:   rc = KEYD_PAGEDOWN; break;
   case SDLK_HOME:       rc = KEYD_HOME;     break;
   case SDLK_END:        rc = KEYD_END;      break;
   case SDLK_INSERT:     rc = KEYD_INSERT;   break;
   // FIXME/TODO: delete
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

extern int usemouse;   // killough 10/98

/////////////////////////////////////////////////////////////////////////////
//
// JOYSTICK                                                  // phares 4/3/98
//
/////////////////////////////////////////////////////////////////////////////

extern int usejoystick;
extern int joystickpresent;

int joystickSens_x;
int joystickSens_y;

// I_JoystickEvents() gathers joystick data and creates an event_t for
// later processing by G_Responder().

static void I_JoystickEvents(void)
{
   // haleyjd 04/15/02: SDL joystick support

   event_t event;
   int joy_b1, joy_b2, joy_b3, joy_b4;
   Sint16 joy_x, joy_y;
   static int old_joy_b1, old_joy_b2, old_joy_b3, old_joy_b4;
   
   if(!joystickpresent || !usejoystick || !sdlJoystick)
      return;
   
   SDL_JoystickUpdate(); // read the current joystick settings
   event.type = ev_joystick;
   event.data1 = 0;
   
   // read the button settings
   if((joy_b1 = SDL_JoystickGetButton(sdlJoystick, 0)))
      event.data1 |= 1;
   if((joy_b2 = SDL_JoystickGetButton(sdlJoystick, 1)))
      event.data1 |= 2;
   if((joy_b3 = SDL_JoystickGetButton(sdlJoystick, 2)))
      event.data1 |= 4;
   if((joy_b4 = SDL_JoystickGetButton(sdlJoystick, 3)))
      event.data1 |= 8;
   
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
   
   if(joy_b1 != old_joy_b1)
   {
      event.type = joy_b1 ? ev_keydown : ev_keyup;
      event.data1 = KEYD_JOY1;
      D_PostEvent(&event);
      old_joy_b1 = joy_b1;
   }
   if(joy_b2 != old_joy_b2)
   {
      event.type = joy_b2 ? ev_keydown : ev_keyup;
      event.data1 = KEYD_JOY2;
      D_PostEvent(&event);
      old_joy_b2 = joy_b2;
   }
   if(joy_b3 != old_joy_b3)
   {
      event.type = joy_b3 ? ev_keydown : ev_keyup;
      event.data1 = KEYD_JOY3;
      D_PostEvent(&event);
      old_joy_b3 = joy_b3;
   }
   if(joy_b4 != old_joy_b4)
   {
      event.type = joy_b4 ? ev_keydown : ev_keyup;
      event.data1 = KEYD_JOY4;
      D_PostEvent(&event);
      old_joy_b4 = joy_b4;
   }   
}


//
// I_StartFrame
//
void I_StartFrame(void)
{
   I_JoystickEvents(); // Obtain joystick data                 phares 4/3/98
}

/////////////////////////////////////////////////////////////////////////////
//
// END JOYSTICK                                              // phares 4/3/98
//
/////////////////////////////////////////////////////////////////////////////

extern void MN_QuitDoom(void);
extern int mouseAccel_type;

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

// SoM 3/14/2002: Rewrote event function for use with SDL
static void I_GetEvent(void)
{
   SDL_Event event;
   
   event_t    d_event    = { 0, 0, 0, 0 };
   event_t    mouseevent = { ev_mouse, 0, 0, 0 };
   static int buttons = 0;
   fixed_t    mousefrac;
   
   // This might be only a windows problem... but it seems the 
   int mousexden = fullscreen ? video.width : video.width >> 1;
   int mouseyden = fullscreen ? video.height : video.height >> 1;
   
   int sendmouseevent = 0;
   
   while(SDL_PollEvent(&event))
   {
      // haleyjd 10/08/05: from Chocolate DOOM
      if(!window_focused && 
         (event.type == SDL_MOUSEMOTION || 
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
         // haleyjd 08/29/03: don't post out-of-range keys
         // FIXME/TODO: eliminate shiftxform, etc.
         if(d_event.data1 > 0 && d_event.data1 < 256)
            D_PostEvent(&d_event);
         break;
      
      case SDL_KEYUP:
         d_event.type = ev_keyup;
         d_event.data1 = I_TranslateKey(event.key.keysym.sym);
         // haleyjd 08/29/03: don't post out-of-range keys
         // FIXME/TODO: eliminate shiftxform, etc.
         if(d_event.data1 > 0 && d_event.data1 < 256)
            D_PostEvent(&d_event);
         break;
      
      case SDL_MOUSEMOTION:       
         if(!usemouse || mouseAccel_type == 2)
            continue;

         // SoM 1-20-04 Ok, use xrel/yrel for mouse movement because most people like it the most.
         if(mouseAccel_type == 0)
         {
            // haleyjd 09/29/08: NOT IN WINDOWED MODE!
            if(fullscreen)
            {
               mouseevent.data3 -= (event.motion.yrel << FRACBITS) / mouseyden * 200;
               mouseevent.data2 += (event.motion.xrel << FRACBITS) / mousexden * 320;
            }
            else
            {
               mouseevent.data3 -= (event.motion.yrel << FRACBITS);
               mouseevent.data2 += (event.motion.xrel << FRACBITS);
            }
         }
         else if(mouseAccel_type == 1)
         {
            mousefrac = (event.motion.yrel << FRACBITS) / mouseyden * 7; 
            mouseevent.data3 -= (FixedMul(D_abs(mousefrac), mousefrac) + mousefrac) * 20;

            mousefrac = (event.motion.xrel << FRACBITS) / mousexden * 5; 
            mouseevent.data2 += (FixedMul(D_abs(mousefrac), mousefrac) + 6 * mousefrac) * 20;
         }
         sendmouseevent = 1;
         break;
      
      case SDL_MOUSEBUTTONUP:      
         if(!usemouse)
            continue;
         sendmouseevent = 1;
         d_event.type = ev_keyup;

         if(event.button.button == SDL_BUTTON_LEFT)
         {
            buttons &= ~1;            
            d_event.data1 = KEYD_MOUSE1;
         }
         else if(event.button.button == SDL_BUTTON_MIDDLE)
         {
            // haleyjd 05/28/06: swapped MOUSE3/MOUSE2
            buttons &= ~4;            
            d_event.data1 = KEYD_MOUSE3;
         }
         else
         {
            buttons &= ~2;            
            d_event.data1 = KEYD_MOUSE2;
         }
         D_PostEvent(&d_event);
         break;

      case SDL_MOUSEBUTTONDOWN:
         if(!usemouse)
            continue;
         sendmouseevent = 1;
         d_event.type =  ev_keydown;
         if(event.button.button == SDL_BUTTON_LEFT)
         {
            buttons |= 1;            
            d_event.data1 = KEYD_MOUSE1;
         }
         else if(event.button.button == SDL_BUTTON_MIDDLE)
         {
            // haleyjd 05/28/06: swapped MOUSE3/MOUSE2
            buttons |= 4;            
            d_event.data1 = KEYD_MOUSE3;
         }
         else
         {
            buttons |= 2;            
            d_event.data1 = KEYD_MOUSE2;
         }
         D_PostEvent(&d_event);
         break;

      case SDL_QUIT:
         MN_QuitDoom();
         break;

      case SDL_ACTIVEEVENT:
         // haleyjd 10/08/05: from Chocolate DOOM:
         // need to update our focus state
         UpdateFocus();
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
   if(paused && !window_focused)
      I_WaitVBL(1);
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
   
   SDL_WarpMouse((short)(video.width / 2), (short)(video.height / 2));
   
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
   
   SDL_GetRelativeMouseState(&x, &y);
   
   if(x != 0 || y != 0) 
   {
      ev.type = ev_mouse;
      ev.data1 = 0; // FIXME?
      // SoM: So the values that go to Eternity should be 16.16 fixed point...
      ev.data2 = AccelerateMouse(x) << FRACBITS;
      ev.data3 = -AccelerateMouse(y) << FRACBITS;
      
      D_PostEvent(&ev);
   }

   if(MouseShouldBeGrabbed())
      CenterMouse();
}
//
// I_StartTic
//
void I_StartTic(void)
{
   I_GetEvent();

   if(usemouse && mouseAccel_type == 2)
      I_ReadMouse();
}

////////////////////////////////////////////////////////////////////////////
//
// Graphics Code
//
////////////////////////////////////////////////////////////////////////////

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit(void)
{
}

// 
// UpdateGrab
//
// haleyjd 10/08/05: from Chocolate DOOM
//
static void UpdateGrab(void)
{
   static boolean currently_grabbed = false;
   boolean grab;
   
   grab = MouseShouldBeGrabbed();
   
   if(grab && !currently_grabbed)
   {
      SDL_SetCursor(cursors[0]);
      SDL_WM_GrabInput(SDL_GRAB_ON);
   }
   
   if(!grab && currently_grabbed)
   {
      SDL_SetCursor(cursors[1]);
      SDL_WM_GrabInput(SDL_GRAB_OFF);
   }
   
   currently_grabbed = grab;   
}

int use_vsync;     // killough 2/8/98: controls whether vsync is called
int page_flip;     // killough 8/15/98: enables page flipping
// SoM: ANYRES
int vesamode;
boolean noblit;

static boolean in_graphics_mode;
static boolean in_page_flip, linear;
static int scroll_offset;
static unsigned destscreen;

// haleyjd 12/03/07: 8-on-32 graphics support
static boolean crossbitdepth;
Uint32 RGB8to32[256];          // palette to true-color lookup table

#define PRIMARY_BUFFER 0

static SDL_Surface *primary_surface = NULL;

void I_FinishUpdate(void)
{
   if(noblit || !in_graphics_mode)
      return;

   // haleyjd 10/08/05: from Chocolate DOOM:

   UpdateGrab();

   // Don't update the screen if the window isn't visible.
   // Not doing this breaks under Windows when we alt-tab away 
   // while fullscreen.   
   if(!(SDL_GetAppState() & SDL_APPACTIVE))
      return;

   if(primary_surface)
   {
      SDL_BlitSurface(primary_surface, NULL, sdlscreen, NULL);
   }

   SDL_Flip(sdlscreen);
}

//
// I_ReadScreen
//
void I_ReadScreen(byte *scr)
{
   VBuffer temp;

   memcpy(&temp, &vbscreen, sizeof(VBuffer));
   temp.data = scr;
   temp.pitch = temp.width;

   V_BlitVBuffer(&temp, 0, 0, &vbscreen, 0, 0, vbscreen.width, vbscreen.height);
}

//
// killough 10/98: init disk icon
//

int disk_icon;

static SDL_Surface *diskflash, *old_data;

static void I_InitDiskFlash(void)
{
/*  byte temp[32*32];

  if (diskflash)
    {
      SDL_FreeSurface(diskflash);
      SDL_FreeSurface(old_data);
    }

  //sf : disk is actually 16x15
  diskflash = SDL_CreateRGBSurface(SDL_SWSURFACE, 16<<hires, 15<<hires, 8, 0, 0, 0, 0);
  old_data = SDL_CreateRGBSurface(SDL_SWSURFACE, 16<<hires, 15<<hires, 8, 0, 0, 0, 0);

  V_GetBlock(0, 0, 0, 16, 15, temp);
  V_DrawPatchDirect(0, -1, &vbscreen, 
                    W_CacheLumpName(cdrom_mode ? "STCDROM" : "STDISK", 
                                    PU_CACHE));
  V_GetBlock(0, 0, 0, 16, 15, (char *)diskflash->pixels);
  V_DrawBlock(0, 0, &vbscreen, 16, 15, temp);*/
}

//
// killough 10/98: draw disk icon
//

void I_BeginRead(void)
{
   if(!disk_icon || !in_graphics_mode)
      return;

  /*blit(screen, old_data,
       (SCREENWIDTH-16) << hires,
       scroll_offset + ((SCREENHEIGHT-15)<<hires),
       0, 0, 16 << hires, 15 << hires);

  blit(diskflash, screen, 0, 0, (SCREENWIDTH-16) << hires,
       scroll_offset + ((SCREENHEIGHT-15)<<hires), 16 << hires, 15 << hires);*/
}

//
// killough 10/98: erase disk icon
//

void I_EndRead(void)
{
   if(!disk_icon || !in_graphics_mode)
      return;

/*  blit(old_data, screen, 0, 0, (SCREENWIDTH-16) << hires,
       scroll_offset + ((SCREENHEIGHT-15)<<hires), 16 << hires, 15 << hires);*/
}


void I_SetPalette(byte *palette)
{
   int i;
   SDL_Color colors[256];
   byte *basepal = palette;
   
   if(!in_graphics_mode)             // killough 8/11/98
      return;

   for(i = 0; i < 256; ++i)
   {
      colors[i].r = gammatable[usegamma][*palette++];
      colors[i].g = gammatable[usegamma][*palette++];
      colors[i].b = gammatable[usegamma][*palette++];
   }

   if(!crossbitdepth)
      SDL_SetPalette(sdlscreen, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);

   if(primary_surface)
      SDL_SetPalette(primary_surface, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);
}


void I_UnsetPrimaryBuffer(void)
{
   if(primary_surface)
   {
      SDL_FreeSurface(primary_surface);
      primary_surface = NULL;
   }
}

void I_SetPrimaryBuffer(void)
{
   // Don't use this just yet...
   /*if(sdlscreen && sdlscreen->format->BitsPerPixel == 8 && 
      !SDL_MUSTLOCK(sdlscreen))
   {
      video.usescreen = true;
      video.screens[0] = (byte *)sdlscreen->pixels;
      video.pitch = sdlscreen->pitch;
   }
   else*/
   if(sdlscreen)
   {
      video.usescreen = false;
      primary_surface = 
         SDL_CreateRGBSurface(SDL_SWSURFACE, video.width, video.height, 8, 
                              0, 0, 0, 0);
      video.screens[0] = (byte *)primary_surface->pixels;
      video.pitch = primary_surface->pitch;
   }
}

//
// I_ShutdownGraphicsPartway
//
// haleyjd: It was necessary to separate this code from I_ShutdownGraphics
// so that the ENDOOM screen can be displayed during shutdown. Otherwise,
// the SDL_QuitSubSystem call below would cause a nasty crash.
//
static void I_ShutdownGraphicsPartway(void)
{
   if(in_graphics_mode)
   {
      // haleyjd 06/21/06: use UpdateGrab here, not release
      UpdateGrab();
      in_graphics_mode = false;
      in_textmode = true;
      sdlscreen = NULL;
      I_UnsetPrimaryBuffer();
   }
}

void I_ShutdownGraphics(void)
{
   if(in_graphics_mode)  // killough 10/98
   {
      I_ShutdownGraphicsPartway();
      
      // haleyjd 10/09/05: shut down graphics earlier
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
   }
}


#define BADVID "video mode not supported"

extern boolean setsizeneeded;

// states for geometry parser
enum
{
   STATE_WIDTH,
   STATE_HEIGHT,
   STATE_FLAGS,
};

static void I_ParseGeom(const char *geom, 
                        int *w, int *h, boolean *fs, boolean *vs, boolean *hw)
{
   const char *c = geom;
   int state = STATE_WIDTH;
   qstring_t qstr;

   M_QStrInitCreate(&qstr);

   while(*c)
   {
      switch(state)
      {
      case STATE_WIDTH:
         if(*c >= '0' && *c <= '9')
            M_QStrPutc(&qstr, *c);
         else
         {
            *w = atoi(qstr.buffer);
            M_QStrClear(&qstr);
            state = STATE_HEIGHT;
         }
         break;
      case STATE_HEIGHT:
         if(*c >= '0' && *c <= '9')
            M_QStrPutc(&qstr, *c);
         else
         {
            *h = atoi(qstr.buffer);
            state = STATE_FLAGS;
            continue; // don't increment the pointer
         }
         break;
      case STATE_FLAGS:
         switch(tolower(*c))
         {
         case 'w': // window
            *fs = false;
            break;
         case 'f': // fullscreen
            *fs = true;
            break;
         case 'a': // async update
            *vs = false;
            break;
         case 'v': // vsync update
            *vs = true;
            break;
         case 's': // software
            *hw = false;
            break;
         case 'h': // hardware 
            *hw = true;
            break;
         }
         break;
      }
      ++c;
   }

   M_QStrFree(&qstr);
}

static void I_CheckVideoCmds(int *w, int *h, boolean *fs, boolean *vs, 
                             boolean *hw)
{
   static boolean firsttime = true;
   int p;

   if(firsttime)
   {
      firsttime = false;

      if((p = M_CheckParm("-geom")) && p < myargc - 1)
         I_ParseGeom(myargv[p + 1], w, h, fs, vs, hw);

      if((p = M_CheckParm("-vwidth")) && p < myargc - 1 &&
         (p = atoi(myargv[p + 1])) >= 320 && p <= 1024)
         *w = p;
      
      if((p = M_CheckParm("-vheight")) && p < myargc - 1 &&
         (p = atoi(myargv[p + 1])) >= 200 && p <= 768)
         *h = p;
      
      if(M_CheckParm("-fullscreen"))
         *fs = true;
      if(M_CheckParm("-nofullscreen") || M_CheckParm("-window"))
         *fs = false;
      
      if(M_CheckParm("-vsync"))
         *vs = true;
      if(M_CheckParm("-novsync"))
         *vs = false;

      if(M_CheckParm("-hardware"))
         *hw = true;
      if(M_CheckParm("-software"))
         *hw = false;
   }
}




static void CreateCursors(void)
{
   static Uint8 empty_cursor_data = 0;

   if(cursors[0])
      SDL_FreeCursor(cursors[0]);

   // Save the default cursor so it can be recalled later
   if(!cursors[1])
      cursors[1] = SDL_GetCursor();

   // Create an empty cursor
   cursors[0] = SDL_CreateCursor(&empty_cursor_data,
                                 &empty_cursor_data,
                                 1, 1, 0, 0);
}
//
// killough 11/98: New routine, for setting hires and page flipping
//

// sf: now returns true if an error occurred
static boolean I_InitGraphicsMode(void)
{
   int v_w   = SCREENWIDTH;
   int v_h   = SCREENHEIGHT;
   int v_bd  = 8;
   int flags = SDL_SWSURFACE;
   SDL_Event dummy;
   boolean wantfullscreen = false, wantvsync = false, wanthardware = false;

   // haleyjd 10/09/05: from Chocolate DOOM
   // mouse grabbing   
   if(M_CheckParm("-grabmouse"))
      grabmouse = 1;
   else if(M_CheckParm("-nograbmouse"))
      grabmouse = 0;

   // haleyjd 12/03/07: cross-bit-depth support
   if(M_CheckParm("-8in32"))
   {
      v_bd = 32;
      crossbitdepth = true;
   }

   // haleyjd 04/11/03: "vsync" or page-flipping support
   if(use_vsync)
      wantvsync = true;
   
   scroll_offset = 0;
   switch(v_mode)
   {
   case 2:
   case 3:
      v_w = 320;
      v_h = 240;
      break;
   case 4:
   case 5:
      v_w = 640;
      v_h = 400;
      break;
   case 6:
   case 7:
      v_w = 640;
      v_h = 480;
      break;
   case 8:
   case 9:
      v_w = 800;
      v_h = 600;
      break;
   case 10:
   case 11:
      v_w = 1024;
      v_h = 768;
      break;
   }
   
   // odd modes are fullscreen
   if(v_mode & 1)
      wantfullscreen = true;

   // haleyjd 06/21/06: allow complete command line overrides but only
   // on initial video mode set (setting from menu doesn't support this)
   I_CheckVideoCmds(&v_w, &v_h, &wantfullscreen, &wantvsync, &wanthardware);

   if(wanthardware)
      flags = SDL_HWSURFACE;

   if(wantvsync)
      flags = SDL_HWSURFACE | SDL_DOUBLEBUF;

   if(wantfullscreen)
      flags |= SDL_FULLSCREEN;
     
   // SoM: 4/15/02: Saftey mode
   if(v_mode > 0 && SDL_VideoModeOK(v_w, v_h, v_bd, flags))
   {
      sdlscreen = SDL_SetVideoMode(v_w, v_h, v_bd, flags);
      if(!sdlscreen)
      {
         I_SetMode(0);
         MN_ErrorMsg(BADVID);
         return true;
      }
   }
   else if(v_mode == 0 && SDL_VideoModeOK(v_w, v_h, v_bd, flags))
   {
      sdlscreen = SDL_SetVideoMode(v_w, v_h, v_bd, flags);
      if(!sdlscreen)
         I_Error("Couldn't set video mode %ix%i\n", v_w, v_h);
   }
   else 
      I_Error("Couldn't set video mode %ix%i\n", v_w, v_h);

   // haleyjd 10/09/05: keep track of fullscreen state
   fullscreen = (sdlscreen->flags & SDL_FULLSCREEN) == SDL_FULLSCREEN;

   // haleyjd 12/03/07: if the video surface is not actually 32-bit, we
   // must disable cross-bit-depth drawing
   if(sdlscreen->format->BitsPerPixel != 32)
      crossbitdepth = false;

   MN_ErrorMsg("");       // clear any error messages

   SDL_WM_SetCaption(ee_wmCaption, NULL);

   UpdateFocus();
   UpdateGrab();

   video.width  = v_w;
   video.height = v_h;
   
   V_Init();      
   
   in_graphics_mode = true;
   in_textmode = false;
   in_page_flip = false; //page_flip;
   
   setsizeneeded = true;
   
   I_InitDiskFlash();        // Initialize disk icon
   I_SetPalette(W_CacheLumpName("PLAYPAL",PU_CACHE));
   CreateCursors();
   
   // haleyjd 10/09/05: from Chocolate DOOM:
   // clear out any events waiting at the start   
   while(SDL_PollEvent(&dummy));
   
   return false;
}

static void I_ResetScreen(void)
{
   // Switch out of old graphics mode
   
   // haleyjd 10/15/05: WOOPS!
   I_ShutdownGraphicsPartway();
   
   // Switch to new graphics mode
   // check for errors -- we may be setting to a different mode instead
   
   if(I_InitGraphicsMode())
      return;
   
   // reset other modules
   
   if(automapactive)
      AM_Start();             // Reset automap dimensions
   
   ST_Start();               // Reset palette
   
   if(gamestate == GS_INTERMISSION)
   {
      IN_DrawBackground();
      V_CopyRect(0, 0, 1, SCREENWIDTH, SCREENHEIGHT, 0, 0, 0);
   }

   Wipe_ScreenReset(); // haleyjd: reset wipe engine
   
   Z_CheckHeap();
}

void I_InitGraphics(void)
{
   static int firsttime = true;
   
   if(!firsttime)
      return;
   
   firsttime = false;
   
   // haleyjd: not a good idea for SDL :(
   // if(nodrawers) // killough 3/2/98: possibly avoid gfx mode
   //    return;

   // init keyboard
   I_InitKeyboard();

   //
   // enter graphics mode
   //
   
   atexit(I_ShutdownGraphics);
   
   in_page_flip = page_flip;
   
   V_ResetMode();
   
   Z_CheckHeap();
}




// the list of video modes is stored here in i_video.c
// the console commands to change them are in v_misc.c,
// so that all the platform-specific stuff is in here.
// v_misc.c does not care about the format of the videomode_t,
// all it asks is that it contains a text value 'description'
// which describes the mode
        
videomode_t videomodes[]=
{
  // [hires, pageflip, vesa (unused for SDL)], description
  {0, 0, 0, "320x200 Windowed"},
  {0, 0, 0, "320x200 Fullscreen"},
  {0, 0, 0, "320x240 Windowed"},
  {0, 0, 0, "320x240 Fullscreen"},
  {0, 0, 0, "640x400 Windowed"},
  {0, 0, 0, "640x400 Fullscreen"},
  {0, 0, 0, "640x480 Windowed"},
  {0, 0, 0, "640x480 Fullscreen"},
  {0, 0, 0, "800x600 Windowed"},
  {0, 0, 0, "800x600 Fullscreen"},
  {0, 0, 0, "1024x768 Windowed"},
  {0, 0, 0, "1024x768 Fullscreen"},
  {0, 0, 0,  NULL}  // last one has NULL description
};

void I_SetMode(int i)
{
   static int firsttime = true;    // the first time to set mode
   
   v_mode = i;
   
   page_flip = videomodes[i].pageflip;
   vesamode = videomodes[i].vesa;
   
   if(firsttime)
      I_InitGraphicsMode();
   else
      I_ResetScreen();
   
   firsttime = false;
}        

/************************
        CONSOLE COMMANDS
 ************************/

VARIABLE_BOOLEAN(use_vsync, NULL,  yesno);
VARIABLE_BOOLEAN(disk_icon, NULL,  onoff);

CONSOLE_VARIABLE(v_diskicon, disk_icon, 0) {}
CONSOLE_VARIABLE(v_retrace, use_vsync, 0)
{
   V_ResetMode();
}

VARIABLE_BOOLEAN(usemouse,    NULL, yesno);
VARIABLE_BOOLEAN(usejoystick, NULL, yesno);

CONSOLE_VARIABLE(i_usemouse, usemouse, 0) {}
CONSOLE_VARIABLE(i_usejoystick, usejoystick, 0) {}

// haleyjd 04/15/02: joystick sensitivity variables
VARIABLE_INT(joystickSens_x, NULL, -32768, 32767, NULL);
VARIABLE_INT(joystickSens_y, NULL, -32768, 32767, NULL);

CONSOLE_VARIABLE(joySens_x, joystickSens_x, 0) {}
CONSOLE_VARIABLE(joySens_y, joystickSens_y, 0) {}

// haleyjd 03/27/06: mouse grabbing
VARIABLE_BOOLEAN(grabmouse, NULL, yesno);
CONSOLE_VARIABLE(i_grabmouse, grabmouse, 0) {}

void I_Video_AddCommands(void)
{
   C_AddCommand(i_usemouse);
   C_AddCommand(i_usejoystick);
   
   C_AddCommand(v_diskicon);
   C_AddCommand(v_retrace);
   
   C_AddCommand(joySens_x);
   C_AddCommand(joySens_y);

   C_AddCommand(i_grabmouse);
}

//----------------------------------------------------------------------------
//
// $Log: i_video.c,v $
// Revision 1.12  1998/05/03  22:40:35  killough
// beautification
//
// Revision 1.11  1998/04/05  00:50:53  phares
// Joystick support, Main Menu re-ordering
//
// Revision 1.10  1998/03/23  03:16:10  killough
// Change to use interrupt-driver keyboard IO
//
// Revision 1.9  1998/03/09  07:13:35  killough
// Allow CTRL-BRK during game init
//
// Revision 1.8  1998/03/02  11:32:22  killough
// Add pentium blit case, make -nodraw work totally
//
// Revision 1.7  1998/02/23  04:29:09  killough
// BLIT tuning
//
// Revision 1.6  1998/02/09  03:01:20  killough
// Add vsync for flicker-free blits
//
// Revision 1.5  1998/02/03  01:33:01  stan
// Moved __djgpp_nearptr_enable() call from I_video.c to i_main.c
//
// Revision 1.4  1998/02/02  13:33:30  killough
// Add support for -noblit
//
// Revision 1.3  1998/01/26  19:23:31  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/26  05:59:14  killough
// New PPro blit routine
//
// Revision 1.1.1.1  1998/01/19  14:02:50  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
