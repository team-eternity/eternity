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
// Purpose: SDL-specific graphics code
// Authors: James Haley, Max Waine
//

#ifdef __APPLE__
#include "SDL2/SDL.h"
#else
#include "SDL.h"
#endif

#include "../hal/i_platform.h"

#include "../z_zone.h"  /* memory allocation wrappers -- killough */

#include "i_sdlvideo.h"

#include "../c_io.h"
#include "../c_runcmd.h"
#include "../d_main.h"
#include "../i_system.h"
#include "../m_argv.h"
#include "../m_misc.h"
#include "../v_misc.h"
#include "../v_video.h"
#include "../version.h"
#include "../w_wad.h"

//=============================================================================
//
// WM-related stuff (see i_input.c)
//

extern int  grabmouse;
extern int  usemouse;   // killough 10/98
extern bool fullscreen;

void UpdateGrab(SDL_Window *window);
bool MouseShouldBeGrabbed();
void UpdateFocus(SDL_Window *window);

//=============================================================================
//
// Graphics Code
//

static SDL_Surface  *primary_surface;
static SDL_Surface  *rgba_surface;
static SDL_Texture  *sdltexture; // the texture to use for rendering
static SDL_Renderer *renderer;
static SDL_Rect     *destrect;

// used when rendering to a subregion, such as for letterboxing
static SDL_Rect staticDestRect;

extern int  use_vsync;     // killough 2/8/98: controls whether vsync is called
extern bool noblit;

static SDL_Color basepal[256], colors[256];
static bool setpalette = false;

extern char *i_resolution;
extern char *i_videomode;

// MaxW: 2017/10/20: display number
int displaynum = 0;

//
// SDLVideoDriver::FinishUpdate
//
// Push the newest frame to the display.
//
void SDLVideoDriver::FinishUpdate()
{
   // haleyjd 10/08/05: from Chocolate DOOM:
   UpdateGrab(window);

   // Don't update the screen if the window isn't visible.
   // Not doing this breaks under Windows when we alt-tab away 
   // while fullscreen.   
   if(!(SDL_GetWindowFlags(window) & SDL_WINDOW_SHOWN))
      return;

   if(setpalette)
   {
      if(primary_surface)
         SDL_SetPaletteColors(primary_surface->format->palette, colors, 0, 256);

      setpalette = false;
   }

   // haleyjd 11/12/09: blit *after* palette set improves behavior.
   if(primary_surface)
   {
      // Don't bother checking for errors. It should just cancel itself in that case.
      SDL_BlitSurface(primary_surface, nullptr, rgba_surface, nullptr);
      SDL_UpdateTexture(sdltexture, nullptr, rgba_surface->pixels, rgba_surface->pitch);
      SDL_RenderCopy(renderer, sdltexture, nullptr, destrect);
   }

   // haleyjd 11/12/09: ALWAYS update. Causes problems with some video surface
   // types otherwise.
   SDL_RenderPresent(renderer);
}

//
// SDLVideoDriver::ReadScreen
//
// Get the current screen contents.
//
void SDLVideoDriver::ReadScreen(byte *scr)
{
   VBuffer temp;

   V_InitVBufferFrom(&temp, vbscreen.width, vbscreen.height, 
                     vbscreen.width, video.bitdepth, scr);
   V_BlitVBuffer(&temp, 0, 0, &vbscreen, 0, 0, vbscreen.width, vbscreen.height);
   V_FreeVBuffer(&temp);
}

//
// I_SDLSetPaletteDirect
//
// haleyjd 11/12/09: make sure surface palettes are set at startup.
//
static void I_SDLSetPaletteDirect(byte *palette)
{
   for(int i = 0; i < 256; i++)
   {
      colors[i].r = gammatable[usegamma][(basepal[i].r = *palette++)];
      colors[i].g = gammatable[usegamma][(basepal[i].g = *palette++)];
      colors[i].b = gammatable[usegamma][(basepal[i].b = *palette++)];
   }

   if(primary_surface)
      SDL_SetPaletteColors(primary_surface->format->palette, colors, 0, 256);
}

//
// SDLVideoDriver::SetPalette
//
// Set the palette, or, if palette is nullptr, update the current palette to use 
// the current gamma setting.
//
void SDLVideoDriver::SetPalette(byte *palette)
{
   if(!palette)
   {
      // Gamma change
      for(int i = 0; i < 256; i++)
      {
         colors[i].r = gammatable[usegamma][basepal[i].r];
         colors[i].g = gammatable[usegamma][basepal[i].g];
         colors[i].b = gammatable[usegamma][basepal[i].b];
      }
   }
   else
   {
      for(int i = 0; i < 256; i++)
      {
         colors[i].r = gammatable[usegamma][(basepal[i].r = *palette++)];
         colors[i].g = gammatable[usegamma][(basepal[i].g = *palette++)];
         colors[i].b = gammatable[usegamma][(basepal[i].b = *palette++)];
      }
   }

   setpalette = true;
}

//
// SDLVideoDriver::UnsetPrimaryBuffer
//
// Free the "primary_surface" SDL_Surface.
//
void SDLVideoDriver::UnsetPrimaryBuffer()
{
   if(sdltexture) // this may have already been deleted, but make sure.
   {
      SDL_DestroyTexture(sdltexture);
      sdltexture = nullptr;
   }
   if(rgba_surface)
   {
      SDL_FreeSurface(rgba_surface);
      rgba_surface = nullptr;
   }
   if(primary_surface)
   {
      SDL_FreeSurface(primary_surface);
      primary_surface = nullptr;
   }
   video.screens[0] = nullptr;
}

//
// SDLVideoDriver::SetPrimaryBuffer
//
// Create a software surface for the game engine to render frames into and
// set it to video.screens[0].
//
void SDLVideoDriver::SetPrimaryBuffer()
{
   int bump = (video.width == 512 || video.width == 1024) ? 4 : 0;

   if(window)
   {
      // SDL_FIXME: This won't be sufficient once a truecolour renderer is implemented
      primary_surface = SDL_CreateRGBSurfaceWithFormat(0, video.height, video.width + bump,
                                                       0, SDL_PIXELFORMAT_INDEX8);
      if(!primary_surface)
         I_Error("SDLVideoDriver::SetPrimaryBuffer: failed to create screen temp buffer\n");

      Uint32 pixelformat = SDL_GetWindowPixelFormat(window);
      if(pixelformat == SDL_PIXELFORMAT_UNKNOWN)
         pixelformat = SDL_PIXELFORMAT_RGBA32;

      rgba_surface = SDL_CreateRGBSurfaceWithFormat(0, video.height, video.width + bump,
                                                    0, pixelformat);
      if(!rgba_surface)
      {
         I_Error("SDLVideoDriver::SetPrimaryBuffer: failed to create true-colour buffer: %s\n",
                 SDL_GetError());
      }
      sdltexture = SDL_CreateTexture(renderer, pixelformat,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     video.height, video.width + bump);
      if(!sdltexture)
      {
         I_Error("SDLVideoDriver::SetPrimaryBuffer: failed to create rendering texture: %s\n",
                 SDL_GetError());
      }

      video.screens[0] = static_cast<byte *>(primary_surface->pixels);
      video.pitch = primary_surface->pitch;
   }
}

//
// SDLVideoDriver::ShutdownGraphicsPartway
//
// haleyjd: It was necessary to separate this code from I_ShutdownGraphics
// so that the ENDOOM screen can be displayed during shutdown. Otherwise,
// the SDL_QuitSubSystem call below would cause a nasty crash.
//
void SDLVideoDriver::ShutdownGraphicsPartway()
{
   // haleyjd 06/21/06: use UpdateGrab here, not release
   UpdateGrab(window);
   if(sdltexture)
   {
      SDL_DestroyTexture(sdltexture);
      sdltexture = nullptr;
   }
   SDL_DestroyRenderer(renderer);
   renderer = nullptr;
   SDL_DestroyWindow(window);
   window = nullptr;
   UnsetPrimaryBuffer();
}

//
// SDLVideoDriver::ShutdownGraphics
//
// Called from I_ShutdownGraphics, which is registered as an atexit callback.
//
void SDLVideoDriver::ShutdownGraphics()
{
   ShutdownGraphicsPartway();
      
   // haleyjd 10/09/05: shut down graphics earlier
   SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

#define BADVID "video mode not supported"

extern bool setsizeneeded;

//
// SDLVideoDriver::InitGraphicsMode
//
// killough 11/98: New routine, for setting hires and page flipping
// sf: now returns true if an error occurred
//
bool SDLVideoDriver::InitGraphicsMode()
{
   // haleyjd 06/19/11: remember characteristics of last successful modeset
   static int fallback_w       = 640;
   static int fallback_h       = 480;
   static int fallback_w_flags = SDL_WINDOW_ALLOW_HIGHDPI;
   static int fallback_r_flags = SDL_RENDERER_TARGETTEXTURE;

   screentype_e screentype     = screentype_e::WINDOWED;
   bool         wantvsync      = false;
   bool         wanthardware   = false;
   bool         wantframe      = true;
   int          v_w            = 640;
   int          v_h            = 480;
   int          v_displaynum   = 0;
   int          window_flags   = SDL_WINDOW_ALLOW_HIGHDPI;
   int          renderer_flags = SDL_RENDERER_TARGETTEXTURE;
   int          r_w = 640;
   int          r_h = 480;

   // haleyjd 04/11/03: "vsync" or page-flipping support
   if(use_vsync)
      wantvsync = true;

   // haleyjd 07/15/09: set defaults using geom string from configuration file
   I_ParseGeom(i_videomode, v_w, v_h, screentype, wantvsync, wanthardware, wantframe);

   // haleyjd 06/21/06: allow complete command line overrides but only
   // on initial video mode set (setting from menu doesn't support this)
   I_CheckVideoCmds(v_w, v_h, screentype, wantvsync, wanthardware, wantframe);

   // Wanting vsync forces framebuffer acceleration on
   if(wantvsync)
   {
      SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
      renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
   }
   else if(wanthardware)
      SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
   else
      SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");

   // haleyjd 10/27/09
   if(!wantframe)
      window_flags |= SDL_WINDOW_BORDERLESS;

   if(displaynum < SDL_GetNumVideoDisplays())
      v_displaynum = displaynum;
   else
      displaynum = 0;

   if(!(window = SDL_CreateWindow(ee_wmCaption,
                                  SDL_WINDOWPOS_CENTERED_DISPLAY(v_displaynum),
                                  SDL_WINDOWPOS_CENTERED_DISPLAY(v_displaynum),
                                  v_h, v_w, window_flags)))
   {
      // try 320x200w safety mode
      if(!(window = SDL_CreateWindow(ee_wmCaption,
                                     SDL_WINDOWPOS_CENTERED_DISPLAY(v_displaynum),
                                     SDL_WINDOWPOS_CENTERED_DISPLAY(v_displaynum),
                                     fallback_h, fallback_w, fallback_w_flags)))
      {
         // SDL_TODO: Trim fat from this error message
         I_FatalError(I_ERR_KILL,
                      "I_SDLInitGraphicsMode: couldn't create window for mode %dx%d;\n"
                      "   Also failed to restore fallback mode %dx%d.\n"
                      "   Check your SDL video driver settings.\n",
                      v_w, v_h, fallback_w, fallback_h);
      }

      // reset these for below population of video struct
      v_w          = fallback_w;
      v_h          = fallback_h;
      window_flags = fallback_w_flags;
   }

   I_ParseResolution(i_resolution, r_w, r_h, v_w, v_h);

#if EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX
   // this and the below #else block are done here as monitor video mode isn't
   // set when SDL_WINDOW_FULLSCREEN (sans desktop) is ORed in during window creation
   if(screentype == screentype_e::FULLSCREEN || screentype == screentype_e::FULLSCREEN_DESKTOP)
      SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
#else
   if(screentype == screentype_e::FULLSCREEN_DESKTOP)
      SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
   else if(screentype == screentype_e::FULLSCREEN)
      SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
#endif

   if(!(renderer = SDL_CreateRenderer(window, -1, renderer_flags)))
   {
      if(!(renderer = SDL_CreateRenderer(window, -1, fallback_r_flags)))
      {
         // SDL_TODO: Trim fat from this error message
         I_FatalError(I_ERR_KILL,
                      "I_SDLInitGraphicsMode: couldn't create renderer for mode %dx%d;\n"
                      "   Also failed to restore fallback mode %dx%d.\n"
                      "   Check your SDL video driver settings.\n",
                      v_w, v_h, fallback_w, fallback_h);
      }

      fallback_r_flags = renderer_flags;
   }

   // Record successful mode set for use as a fallback mode
   fallback_w     = v_w;
   fallback_h     = v_h;
   fallback_w_flags = window_flags;
   fallback_r_flags = renderer_flags;

   // haleyjd 10/09/05: keep track of fullscreen state
   fullscreen = !!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP);

   UpdateFocus(window);
   UpdateGrab(window);

   // check for letterboxing
   if(I_VideoShouldLetterbox(r_w, r_h))
   {
      int hs = I_VideoLetterboxHeight(r_w);

      staticDestRect.x = 0;
      staticDestRect.y = static_cast<Sint16>(I_VideoLetterboxOffset(r_h, hs));
      staticDestRect.w = static_cast<Uint16>(r_w);
      staticDestRect.h = static_cast<Uint16>(hs);

      video.width  = r_w;
      video.height = hs;
      destrect     = &staticDestRect;
   }
   else
   {
      video.width  = r_w;
      video.height = r_h;
      destrect     = nullptr;
   }

   video.bitdepth  = 8;
   video.pixelsize = 1;

   UnsetPrimaryBuffer();
   SetPrimaryBuffer();

   // haleyjd 11/12/09: set surface palettes immediately
   I_SDLSetPaletteDirect(static_cast<byte *>(wGlobalDir.cacheLumpName("PLAYPAL", PU_CACHE)));

   return false;
}

// The one and only global instance of the SDL video driver.
SDLVideoDriver i_sdlvideodriver;

/************************
CONSOLE COMMANDS
************************/

CONSOLE_COMMAND(maxdisplaynum, 0)
{
   C_Printf("%d", SDL_GetNumVideoDisplays() - 1);
}

VARIABLE_INT(displaynum, nullptr, -1, UL, nullptr);
CONSOLE_VARIABLE(displaynum, displaynum, 0)
{
   const int numdisplays = SDL_GetNumVideoDisplays();

   if(displaynum == -1)
      displaynum = numdisplays - 1; // allow scrolling left from 0 to maxdisplaynum
   else if(displaynum == numdisplays)
      displaynum = 0; // allow scrolling right from maxdisplaynum to 0
   else if(displaynum > numdisplays)
   {
      C_Printf(FC_ERROR "Warning: displaynum's current maximum is %d, resetting to 0",
               numdisplays - 1);
      displaynum = 0;
   }

   I_SetMode();
}


// EOF

