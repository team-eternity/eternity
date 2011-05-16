// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
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
//   SDL-specific GL 2D-in-3D video code
//
//-----------------------------------------------------------------------------

#include "SDL.h"
#include "SDL_opengl.h"

#include "../z_zone.h"
#include "../i_system.h"
#include "../v_misc.h"
#include "../version.h"

#include "i_sdlgl2d.h"

//=============================================================================
//
// WM-related stuff (see i_input.c)
//

void UpdateGrab(void);
bool MouseShouldBeGrabbed(void);
void UpdateFocus(void);

//=============================================================================
//
// Static Data
//

// Surface returned from SDL_SetVideoMode; not really useful for anything.
static SDL_Surface *surface;

// Temporary screen surface; this is what the game will draw itself into.
static SDL_Surface *screen; 

//=============================================================================
//
// Graphics Code
//

//
// SDLGL2DVideoDriver::FinishUpdate
//
void SDLGL2DVideoDriver::FinishUpdate()
{
   // haleyjd 10/08/05: from Chocolate DOOM:
   UpdateGrab();

   // Don't update the screen if the window isn't visible.
   // Not doing this breaks under Windows when we alt-tab away 
   // while fullscreen.   
   if(!(SDL_GetAppState() & SDL_APPACTIVE))
      return;

   // TODO:
   // * bind framebuffer texture
   // * push screen quad w/tex coords
   // * draw disk if necessary?

   glFinish();
   SDL_GL_SwapBuffers();
}

//
// SDLGL2DVideoDriver::ReadScreen
//
void SDLGL2DVideoDriver::ReadScreen(byte *scr)
{
   // TODO: glReadPixels
}

//
// SDLGL2DVideoDriver::InitDiskFlash
//
void SDLGL2DVideoDriver::InitDiskFlash()
{
   // TODO: load disk texture
}

//
// SDLGL2DVideoDriver::BeginRead
//
void SDLGL2DVideoDriver::BeginRead()
{
   // TODO
}

//
// SDLGL2DVideoDriver::EndRead
//
void SDLGL2DVideoDriver::EndRead()
{
   // TODO
}

//
// SDLGL2DVideoDriver::SetPalette
//
void SDLGL2DVideoDriver::SetPalette(byte *pal)
{
   // TODO: Cache palette and create 32-bit translation lookup
}

//
// SDLGL2DVideoDriver::SetPrimaryBuffer
//
void SDLGL2DVideoDriver::SetPrimaryBuffer()
{
   // Create screen surface for the high-level code to render the game into
   screen = SDL_CreateRGBSurface(SDL_SWSURFACE, video.width, video.height, 
                                 8, 0, 0, 0, 0);

   if(!screen)
      I_FatalError(I_ERR_KILL, "Failed to create screen temp buffer\n");

   // Point screens[0] to 8-bit temp buffer
   video.screens[0] = (byte *)(screen->pixels);
   video.pitch      = screen->pitch;
}

//
// SDLGL2DVideoDriver::UnsetPrimaryBuffer
//
void SDLGL2DVideoDriver::UnsetPrimaryBuffer()
{
   if(screen)
   {
      SDL_FreeSurface(screen);
      screen = NULL;
   }
   video.screens[0] = NULL;
}

//
// SDLGL2DVideoDriver::ShutdownGraphics
//
void SDLGL2DVideoDriver::ShutdownGraphics()
{
   ShutdownGraphicsPartway();

   // TODO: proper GL shutdown needed?

   // quit SDL video
   SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

//
// SDLGL2DVideoDriver::ShutdownGraphicsPartway
//
void SDLGL2DVideoDriver::ShutdownGraphicsPartway()
{   
   // haleyjd 06/21/06: use UpdateGrab here, not release
   UpdateGrab();

   // TODO: code to allow changing resolutions in OpenGL...
   // Must shutdown everything.

   UnsetPrimaryBuffer();
}

//
// SDLGL2DVideoDriver::InitGraphicsMode
//
bool SDLGL2DVideoDriver::InitGraphicsMode()
{
   bool wantfullscreen = false;
   bool wantvsync      = false;
   bool wanthardware   = false; // Not used - this is always "hardware".
   bool wantframe      = true;
   int  v_w            = 640;
   int  v_h            = 480;
   int  flags          = SDL_OPENGL;

   // Get video commands and geometry settings

   // haleyjd 04/11/03: "vsync" or page-flipping support
   if(use_vsync)
      wantvsync = true;
   
   // set defaults using geom string from configuration file
   I_ParseGeom(i_videomode, &v_w, &v_h, &wantfullscreen, &wantvsync, 
               &wanthardware, &wantframe);
   
   // haleyjd 06/21/06: allow complete command line overrides but only
   // on initial video mode set (setting from menu doesn't support this)
   I_CheckVideoCmds(&v_w, &v_h, &wantfullscreen, &wantvsync, &wanthardware,
                    &wantframe);

   if(wantfullscreen)
      flags |= SDL_FULLSCREEN;
   
   if(!wantframe)
      flags |= SDL_NOFRAME;
   
   // Set GL attributes through SDL
   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE,  colordepth);
   SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, wantvsync ? 1 : 0); // OMG vsync!

   // Set GL video mode
   if(!(surface = SDL_SetVideoMode(v_w, v_h, colordepth, flags)))
   {
      I_FatalError(I_ERR_KILL, "Couldn't set OpenGL video mode %dx%dx%d\n", 
                   v_w, v_h, colordepth);
   }

   // wait for a bit so the screen can settle
   if(flags & SDL_FULLSCREEN)
      I_Sleep(500);

   // Enable two-dimensional texturing
   glEnable(GL_TEXTURE_2D);

   // TODO:
   // * Set ortho projection
   // * Create textures

   SDL_WM_SetCaption(ee_wmCaption, ee_wmCaption);
   UpdateFocus();
   UpdateGrab();

   // Init Cardboard video metrics
   video.width     = v_w;
   video.height    = v_h;
   video.bitdepth  = 8;
   video.pixelsize = 1;

   return false;
}

//
// SDLGL2DVideoDriver::InitGraphics
//
void SDLGL2DVideoDriver::InitGraphics()
{
}

// The one and only global instance of the SDL GL 2D-in-3D video driver.
SDLGL2DVideoDriver i_sdlgl2dvideodriver;

// EOF

