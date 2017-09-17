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
//   SDL-specific graphics code.
//
//-----------------------------------------------------------------------------

#include "SDL.h"

#include "../z_zone.h"  /* memory allocation wrappers -- killough */

#include "i_sdlvideo.h"

#include "../d_main.h"
#include "../i_system.h"
#include "../m_argv.h"
#include "../v_misc.h"
#include "../v_patchfmt.h"
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

static SDL_Surface *primary_surface;
static SDL_Renderer *renderer;
static SDL_Rect    *destrect;

// used when rendering to a subregion, such as for letterboxing
static SDL_Rect staticDestRect;

extern int  use_vsync;     // killough 2/8/98: controls whether vsync is called
extern bool noblit;

static SDL_Color basepal[256], colors[256];
static bool setpalette = false;

// haleyjd 07/15/09
extern char *i_default_videomode;
extern char *i_videomode;

// haleyjd 12/03/07: 8-on-32 graphics support
static bool crossbitdepth;

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
      if(!crossbitdepth)
      {
         SDL_SetPaletteColors(SDL_GetWindowSurface(window)->format->palette,
                             colors, 0, 256);
      }

      if(primary_surface)
         SDL_SetPaletteColors(primary_surface->format->palette, colors, 0, 256);

      setpalette = false;
   }

   // haleyjd 11/12/09: blit *after* palette set improves behavior.
   if(primary_surface)
   {
      SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, primary_surface);
      SDL_RenderCopy(renderer, texture, nullptr, destrect);
      SDL_DestroyTexture(texture);
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
static void I_SDLSetPaletteDirect(SDL_Window *window, byte *palette)
{
   for(int i = 0; i < 256; i++)
   {
      colors[i].r = gammatable[usegamma][(basepal[i].r = *palette++)];
      colors[i].g = gammatable[usegamma][(basepal[i].g = *palette++)];
      colors[i].b = gammatable[usegamma][(basepal[i].b = *palette++)];
   }

   if(window && !crossbitdepth)
   {
      SDL_SetPaletteColors(SDL_GetWindowSurface(window)->format->palette,
         colors, 0, 256);
   }

   if(primary_surface)
      SDL_SetPaletteColors(primary_surface->format->palette, colors, 0, 256);
}

//
// SDLVideoDriver::SetPalette
//
// Set the palette, or, if palette is NULL, update the current palette to use 
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
   if(primary_surface)
   {
      SDL_FreeSurface(primary_surface);
      primary_surface = NULL;
   }
   video.screens[0] = NULL;
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
      primary_surface = 
         SDL_CreateRGBSurface(SDL_SWSURFACE, video.width + bump, video.height,
                              8, 0, 0, 0, 0);
      if(!primary_surface)
         I_Error("SDLVideoDriver::SetPrimaryBuffer: failed to create screen temp buffer\n");

      video.screens[0] = (byte *)primary_surface->pixels;
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
   SDL_DestroyWindow(window);
   window = nullptr;
   SDL_DestroyRenderer(renderer);
   renderer = nullptr;
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
   static int fallback_bd      =   8;
   static int fallback_w_flags = SDL_WINDOW_ALLOW_HIGHDPI;
   static int fallback_r_flags = SDL_RENDERER_SOFTWARE;

   bool wantfullscreen = false;
   bool wantvsync      = false;
   bool wanthardware   = false;
   bool wantframe      = true;
   int  v_w            = 640;
   int  v_h            = 480;
   int  v_bd           = 8;
   int  window_flags   = SDL_WINDOW_ALLOW_HIGHDPI;
   int  renderer_flags = SDL_RENDERER_TARGETTEXTURE;

   // haleyjd 12/03/07: cross-bit-depth support
   if(M_CheckParm("-8in32"))
     v_bd = 32;
   else if(i_softbitdepth > 8)
   {
      switch(i_softbitdepth)
      {
      case 16: // Valid screen bitdepth settings
      case 24:
      case 32:
         v_bd = i_softbitdepth;
         break;
      default:
         break;
      }
   }

   if(v_bd != 8)
      crossbitdepth = true;

   // haleyjd 04/11/03: "vsync" or page-flipping support
   if(use_vsync)
      wantvsync = true;

   // haleyjd 07/15/09: set defaults using geom string from configuration file
   I_ParseGeom(i_videomode, &v_w, &v_h, &wantfullscreen, &wantvsync, 
               &wanthardware, &wantframe);
   
   // haleyjd 06/21/06: allow complete command line overrides but only
   // on initial video mode set (setting from menu doesn't support this)
   I_CheckVideoCmds(&v_w, &v_h, &wantfullscreen, &wantvsync, &wanthardware,
                    &wantframe);

   // FIXME: Wat do about SDL_HWSURFACE
   if(wanthardware)
      SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1"); // FIXME: Is this right?
   else
      renderer_flags = SDL_RENDERER_SOFTWARE;

   // FIXME: Wat do about SDL_HWSURFACE
   if(wantvsync)
   {
      SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
      renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
   }

   if(wantfullscreen)
      window_flags |= SDL_WINDOW_FULLSCREEN;

   // haleyjd 10/27/09
   if(!wantframe)
      window_flags |= SDL_WINDOW_BORDERLESS;
     
   if(!(window = SDL_CreateWindow(ee_wmCaption,
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  v_w, v_h, window_flags)))
   {
      // try 320x200w safety mode
      if(!(window = SDL_CreateWindow(ee_wmCaption,
                                     SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                     fallback_w, fallback_h, fallback_w_flags)))
      {
         I_FatalError(I_ERR_KILL,
                      "I_SDLInitGraphicsMode: couldn't set mode %dx%dx%d;\n"
                      "   Also failed to restore fallback mode %dx%dx%d.\n"
                      "   Check your SDL video driver settings.\n",
                      v_w, v_h, v_bd,
                      fallback_w, fallback_h, fallback_bd);
      }

      // reset these for below population of video struct
      v_w          = fallback_w;
      v_h          = fallback_h;
      v_bd         = fallback_bd;
      window_flags = fallback_w_flags;
   }

   if(!(renderer = SDL_CreateRenderer(window, -1, renderer_flags)))
   {
      if(!(renderer = SDL_CreateRenderer(window, -1, fallback_r_flags)))
      {
         I_FatalError(I_ERR_KILL, "Renderer creation explod: %s\n", SDL_GetError());
      }

      fallback_r_flags = renderer_flags;
   }

   // Record successful mode set for use as a fallback mode
   fallback_w     = v_w;
   fallback_h     = v_h;
   fallback_bd    = v_bd;
   fallback_w_flags = window_flags;
   fallback_r_flags = renderer_flags;

   // haleyjd 10/09/05: keep track of fullscreen state
   fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN;

   // haleyjd 12/03/07: if the video surface is not high-color, we
   // disable cross-bit-depth drawing for efficiency
   if(SDL_GetWindowSurface(window)->format->BitsPerPixel == 8)
      crossbitdepth = false;

   UpdateFocus(window);
   UpdateGrab(window);

   // check for letterboxing
   if(I_VideoShouldLetterbox(v_w, v_h))
   {
      int hs = I_VideoLetterboxHeight(v_w);

      staticDestRect.x = 0;
      staticDestRect.y = static_cast<Sint16>(I_VideoLetterboxOffset(v_h, hs));
      staticDestRect.w = static_cast<Uint16>(v_w);
      staticDestRect.h = static_cast<Uint16>(hs);

      video.width  = v_w;
      video.height = hs;
      destrect     = &staticDestRect;
   }
   else
   {
      video.width  = v_w;
      video.height = v_h;
      destrect     = NULL;
   }

   video.bitdepth  = 8;
   video.pixelsize = 1;

   UnsetPrimaryBuffer();
   SetPrimaryBuffer();
   
   // haleyjd 11/12/09: set surface palettes immediately
   I_SDLSetPaletteDirect(window, (byte *)wGlobalDir.cacheLumpName("PLAYPAL", PU_CACHE));

   return false;
}

// The one and only global instance of the SDL video driver.
SDLVideoDriver i_sdlvideodriver;

// EOF

