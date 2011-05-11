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

#include "../z_zone.h"
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
   // * glFinish
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
   // TODO: Point screens[0] to 8-bit temp buffer
}

//
// SDLGL2DVideoDriver::UnsetPrimaryBuffer
//
void SDLGL2DVideoDriver::UnsetPrimaryBuffer()
{
   // TODO: Clear screens[0]
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
   // mouse grabbing   
   //if(M_CheckParm("-grabmouse"))
   //   grabmouse = 1;
   //else if(M_CheckParm("-nograbmouse"))
   //   grabmouse = 0;

   // TODO:
   // * Set GL attributes
   // * Set GL video mode
   // * Set ortho projection
   // * Create textures

   // wait for a bit so the screen can settle
   //if(flags & SDL_FULLSCREEN)
   //   I_Sleep(500);

   //R_ResetFOV(v_w, v_h);

   //SDL_WM_SetCaption(ee_wmCaption, ee_wmCaption);
   //UpdateFocus();
   //UpdateGrab();

   return false;
}

//
// SDLGL2DVideoDriver::InitGraphics
//
void SDLGL2DVideoDriver::InitGraphics()
{
   // enable key repeat
   SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY/2, SDL_DEFAULT_REPEAT_INTERVAL*4);
}

// The one and only global instance of the SDL GL 2D-in-3D video driver.
SDLGL2DVideoDriver i_sdlgl2dvideodriver;

// EOF

