// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//   
//  Hardware Abstraction Layer for Video
//  haleyjd 05/08/11
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"   /* memory allocation wrappers -- killough */

// Need platform defines
#include "i_platform.h"

#include "../am_map.h"
#include "../c_runcmd.h"
#include "../d_gi.h"
#include "../d_main.h"
#include "../doomstat.h"
#include "../f_wipe.h"
#include "../i_system.h"
#include "../i_video.h"
#include "../in_lude.h"
#include "../m_argv.h"
#include "../m_misc.h"
#include "../m_qstr.h"
#include "../r_main.h"
#include "../st_stuff.h"
#include "../v_misc.h"
#include "../v_video.h"

// Platform-Specific Video Drivers:
#ifdef _SDL_VER
#include "../sdl/i_sdlvideo.h"
#ifdef EE_FEATURE_OPENGL
#include "../sdl/i_sdlgl2d.h"
#endif
#endif

//=============================================================================
//
// Video Driver Object Pointer
//

HALVideoDriver *i_video_driver = nullptr;

//=============================================================================
//
// Video Driver Table
//

// Config variable to select backend video driver module
int i_videodriverid = -1;

struct haldriveritem_t
{
   int id;                 // unique ID # assigned here by the HAL
   const char *name;       // descriptive name of this driver
   HALVideoDriver *driver; // driver object
};

// Help string for system.cfg:
const char *const i_videohelpstr = 
  "Select video backend (-1 = default"
#ifdef _SDL_VER
  ", 0 = SDL Default"
#ifdef EE_FEATURE_OPENGL
  ", 1 = SDL GL2D"
#endif
#endif
  ")";

// Driver table
static haldriveritem_t halVideoDriverTable[VDR_MAXDRIVERS] =
{
   // SDL GL2D Driver
   {
      VDR_SDLGL2D,
      "SDL GL2D",
#if defined(_SDL_VER) && defined(EE_FEATURE_OPENGL)
      &i_sdlgl2dvideodriver
#else
      nullptr
#endif
   },

   // SDL Whatever-it-chooses Driver
   {
      VDR_SDLDEFAULT,
      "SDL Default",
#ifdef _SDL_VER
      &i_sdlvideodriver
#else
      nullptr
#endif
   }
};

//
// Find the currently selected video driver by ID
//
static haldriveritem_t *I_FindHALVDRByID(int id)
{
   for(unsigned int i = 0; i < VDR_MAXDRIVERS; i++)
   {
      if(halVideoDriverTable[i].id == id && halVideoDriverTable[i].driver)
         return &halVideoDriverTable[i];
   }

   return nullptr;
}

//
// I_DefaultVideoDriver
//
// Chooses the default video driver based on user specifications, or on preset
// platform-specific defaults when there is no user specification.
//
static haldriveritem_t *I_DefaultVideoDriver()
{
   haldriveritem_t *item;

   if(!(item = I_FindHALVDRByID(i_videodriverid)))
   {
      // Default or plain invalid setting, or unsupported driver on current
      // compile. Find the lowest-numbered valid driver and use it.
      for(unsigned int i = 0; i < VDR_MAXDRIVERS; i++)
      {
         if(halVideoDriverTable[i].driver)
         {
            item = &halVideoDriverTable[i];
            break;
         }
      }

      // Nothing?! Somebody borked up their configure/makefile.
      if(!item)
        I_Error("I_DefaultVideoDriver: no valid drivers for this platform!\n");
   }

   return item;
}

//=============================================================================
//
// WM-related stuff (see i_input.c)
//

extern int  grabmouse;
extern int  usemouse;   // killough 10/98
extern bool fullscreen;

void I_InitMouse();

void I_StartTic()
{
   if(!D_noWindow())
      I_StartTicInWindow(i_video_driver->window);
}

//=============================================================================
//
// Graphics Code
//

int  use_vsync;     // killough 2/8/98: controls whether vsync is called
bool noblit;

bool in_graphics_mode;

// haleyjd 07/15/09
char *i_default_videomode;
char *i_videomode;

// haleyjd 03/30/14: support for letterboxing narrow resolutions
bool i_letterbox;

//
// I_FinishUpdate
//
void I_FinishUpdate()
{
   if(!noblit && in_graphics_mode)
      i_video_driver->FinishUpdate();
}

//
// I_ReadScreen
//
void I_ReadScreen(byte *scr)
{
   i_video_driver->ReadScreen(scr);
}

//
// I_SetPalette
//
void I_SetPalette(byte *palette)
{
   if(in_graphics_mode)             // killough 8/11/98
      i_video_driver->SetPalette(palette);
}

void I_ShutdownGraphics()
{
   if(in_graphics_mode)
   {
      i_video_driver->ShutdownGraphics();
      in_graphics_mode = false;
      in_textmode = true;
   }
}

#define BADVID "video mode not supported"

extern bool setsizeneeded;

// states for geometry parser
enum
{
   STATE_WIDTH,
   STATE_HEIGHT,
   STATE_FLAGS
};

//
// Function to parse geometry description strings in the form [wwww]x[hhhh][f/d].
// This is now the primary way in which Eternity stores its video mode setting.
//
void I_ParseGeom(const char *geom, int &w, int &h, screentype_e &st, bool &vs, bool &hw, bool &wf)
{
   const char *c = geom;
   int state = STATE_WIDTH;
   int tmpwidth = 320, tmpheight = 200;
   qstring qstr;
   bool errorflag = false;

   while(*c)
   {
      switch(state)
      {
      case STATE_WIDTH:
         if(*c >= '0' && *c <= '9')
            qstr += *c;
         else
         {
            int width = qstr.toInt();
            if(width < 320 || width > MAX_SCREENWIDTH)
            {
               state = STATE_FLAGS;
               errorflag = true;
            }
            else
            {
               tmpwidth = width;
               qstr.clear();
               state = STATE_HEIGHT;
            }
         }
         break;
      case STATE_HEIGHT:
         if(*c >= '0' && *c <= '9')
            qstr += *c;
         else
         {
            int height = qstr.toInt();
            if(height < 200 || height > MAX_SCREENHEIGHT)
            {
               state = STATE_FLAGS;
               errorflag = true;
            }
            else
            {
               tmpheight = height;
               state = STATE_FLAGS;
               continue; // don't increment the pointer
            }
         }
         break;
      case STATE_FLAGS:
         switch(ectype::toLower(*c))
         {
         case 'w': // window
            st = screentype_e::WINDOWED;
            break;
         case 'd': // fullscreen desktop
            st = screentype_e::FULLSCREEN_DESKTOP;
            break;
         case 'f': // fullscreen
            st = screentype_e::FULLSCREEN;
            break;
         case 'a': // async update
            vs = false;
            break;
         case 'v': // vsync update
            vs = true;
            break;
         case 's': // software
            hw = false;
            break;
         case 'h': // hardware 
            hw = true;
            break;
         case 'n': // noframe
            wf = false;
            break;
         default:
            break;
         }
         break;
      }
      ++c;
   }

   // handle termination of loop during STATE_HEIGHT (no flags)
   if(state == STATE_HEIGHT)
   {
      int height = qstr.toInt();

      if(height < 200 || height > MAX_SCREENHEIGHT)
         errorflag = true;
      else
         tmpheight = height;
   }

   // if an error occurs setting w/h, we default.
   if(errorflag)
   {
      tmpwidth  = 640;
      tmpheight = 480;
   }

   w = tmpwidth;
   h = tmpheight;
}

//
// Checks for all video-mode-related command-line parameters in one
// convenient location. Though called from I_InitGraphicsMode, this
// function will only run once at startup. Resolution changes made at
// runtime want to use the precise settings specified through the UI
// instead.
//
void I_CheckVideoCmds(int &w, int &h, screentype_e &st, bool &vs, bool &hw, bool &wf)
{
   static bool firsttime = true;
   int p;

   if(firsttime)
   {
      firsttime = false;

      if((p = M_CheckParm("-geom")) && p < myargc - 1)
         I_ParseGeom(myargv[p + 1], w, h, st, vs, hw, wf);

      if((p = M_CheckParm("-vwidth")) && p < myargc - 1 &&
         (p = atoi(myargv[p + 1])) >= 320 && p <= MAX_SCREENWIDTH)
         w = p;

      if((p = M_CheckParm("-vheight")) && p < myargc - 1 &&
         (p = atoi(myargv[p + 1])) >= 200 && p <= MAX_SCREENHEIGHT)
         h = p;

      if(M_CheckParm("-fullscreen"))
         st = screentype_e::FULLSCREEN_DESKTOP;
      if(M_CheckParm("-nofullscreen") || M_CheckParm("-window"))
         st = screentype_e::WINDOWED;

      if(M_CheckParm("-vsync"))
         vs = true;
      if(M_CheckParm("-novsync"))
         vs = false;

      if(M_CheckParm("-hardware"))
         hw = true;
      if(M_CheckParm("-software"))
         hw = false;

      if(M_CheckParm("-frame"))
         wf = true;
      if(M_CheckParm("-noframe"))
         wf = false;
   }
}

#ifdef _MSC_VER
extern void I_DisableSysMenu(SDL_Window *window);
#endif

//
// I_InitGraphicsMode
//
// killough 11/98: New routine, for setting hires and page flipping
// sf: now returns true if an error occurred
//
static bool I_InitGraphicsMode()
{
   bool result; 

   if(!i_default_videomode)
      i_default_videomode = estrdup("640x480w");

   if(!i_videomode)
      i_videomode = estrdup(i_default_videomode);

   if(D_noWindow())
      return false;

   // A false return value from HALVideoDriver::InitGraphicsMode means that no
   // errors have occured and we should continue with initialization.
   if(!(result = i_video_driver->InitGraphicsMode()))
   {
      // Reset renderer field of view
      R_ResetFOV(video.width, video.height);

#ifdef _MSC_VER
      // Win32 specific hacks
      if(!D_noWindow())
         I_DisableSysMenu(i_video_driver->window);
#endif

      V_Init();                 // initialize high-level video

      in_graphics_mode = true;  // now in graphics mode
      in_textmode      = false; // no longer in text mode
      setsizeneeded    = true;  // should initialize screen size
   }

   return result;
}

//
// I_ResetScreen
//
// Changes the resolution by getting out of the old mode and into the new one,
// and then waking up any interested game-code modules that need to know about
// the screen resolution.
//
static void I_ResetScreen()
{
   // Switch out of old graphics mode
   if(in_graphics_mode)
   {
      i_video_driver->ShutdownGraphicsPartway();
      in_graphics_mode = false;
      in_textmode = true;
   }
   
   // Switch to new graphics mode
   // check for errors -- we may be setting to a different mode instead
   if(I_InitGraphicsMode())
      return;
   
   // reset other modules
   
   // Reset automap dimensions
   if(automapactive)
      AM_Start();
   
   // Reset palette
   ST_Start();

   // haleyjd: reset wipe engine
   Wipe_ScreenReset();
   
   // A LOT of heap activity just happened, so check it.
   Z_CheckHeap();
}

void I_InitGraphics()
{
   static int firsttime = true;
   haldriveritem_t *driveritem = nullptr;
   
   if(!firsttime)
      return;
   
   firsttime = false;
   
   // Select video driver based on configuration (out of those available in 
   // the current compile), or get the default driver if unspecified
   if(!(driveritem = I_DefaultVideoDriver()))
   {
      I_Error("I_InitGraphics: invalid video driver %d\n", i_videodriverid);
   }
   else
   {
      i_video_driver  = driveritem->driver;
      i_videodriverid = driveritem->id;
      usermsg(" (using video driver '%s')", driveritem->name);
   }
   
   // haleyjd: not a good idea for SDL :(
   // if(nodrawers) // killough 3/2/98: possibly avoid gfx mode
   //    return;

   // haleyjd 05/10/11: init mouse
   I_InitMouse();

   //
   // enter graphics mode
   //
   
   atexit(I_ShutdownGraphics);
   
   I_SetMode();
   
   Z_CheckHeap();
}

void I_SetMode()
{
   static int firsttime = true;    // the first time to set mode
   
   if(firsttime)
      I_InitGraphicsMode();
   else
      I_ResetScreen();
   
   firsttime = false;
}

//
// I_VideoShouldLetterbox
//
// Determine from the screen aspect ratio and user settings whether or not 
// letterboxing should be active.
//
bool I_VideoShouldLetterbox(int w, int h)
{
   fixed_t aspect = w * FRACUNIT / h;
   fixed_t cutoff = 5 * FRACUNIT / 4; // 5:4 is the upper bound of letterboxed aspects

   return (i_letterbox && aspect <= cutoff);
}

//
// I_VideoLetterboxHeight
//
// Calculate the height of a 4:3 subregion for a letterboxed resolution.
//
int I_VideoLetterboxHeight(int w)
{
   return (((w * 3 / 4) + 1) & ~1); // round up and make even
}

//
// I_VideoLetterboxOffset
//
// Given the real height and the letterboxed height, calculates the vertical
// offset of the letterboxed subregion.
//
int I_VideoLetterboxOffset(int h, int hl)
{
   return ((h - hl) / 2);
}

//
// Used to avoid multiple usages of "togglefullscreen" literal, scans better.
//
void I_ToggleFullscreen()
{
   C_RunTextCmd("togglefullscreen");
}

/************************
        CONSOLE COMMANDS
 ************************/

CONSOLE_COMMAND(togglefullscreen, cf_buffered)
{
   qstring qgeom = qstring(i_videomode).toLower();
   const char *lastf = qgeom.strRChr('f'), *lastw = qgeom.strRChr('w');

   // Alter the geom string as needed.
   // NOTE: No shortcuts. Relational operators w/ nullptr as an operand is unspecified.
   if(!lastf && !lastw)
      qgeom.Putc('f'); // Currently implicitly windowed, make fullscreen.
   if((lastf && !lastw) || (lastf > lastw))
      qgeom.replace("f", 'w');
   else if((lastw && !lastf) || (lastw > lastf))
      qgeom.replace("w", 'f');

   efree(i_videomode);
   i_videomode = qgeom.duplicate();

   I_SetMode();

   if(i_default_videomode)
      efree(i_default_videomode);
   i_default_videomode = estrdup(i_videomode);
}

VARIABLE_BOOLEAN(use_vsync, nullptr,  yesno);

CONSOLE_VARIABLE(v_retrace, use_vsync, 0)
{
   I_SetMode();
}

VARIABLE_BOOLEAN(usemouse,    nullptr, yesno);

CONSOLE_VARIABLE(i_usemouse, usemouse, 0) {}

// haleyjd 03/27/06: mouse grabbing
VARIABLE_BOOLEAN(grabmouse, nullptr, yesno);
CONSOLE_VARIABLE(i_grabmouse, grabmouse, 0) {}

VARIABLE_STRING(i_videomode, nullptr, UL);
CONSOLE_VARIABLE(i_videomode, i_videomode, cf_buffered)
{
   I_SetMode();

   if(i_default_videomode)
      efree(i_default_videomode);

   i_default_videomode = estrdup(i_videomode);
}

static const char *i_videodrivernames[] = 
{
   "default",
   "SDL Default",
   "SDL GL2D"
};

VARIABLE_INT(i_videodriverid, nullptr, -1, VDR_MAXDRIVERS-1, i_videodrivernames);
CONSOLE_VARIABLE(i_videodriverid, i_videodriverid, 0) {}

VARIABLE_TOGGLE(i_letterbox, nullptr, yesno);
CONSOLE_VARIABLE(i_letterbox, i_letterbox, cf_buffered)
{
   I_SetMode();
}

// EOF

