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

#include "../z_zone.h"
#include "i_sdlgl2d.h"

//
// SDLGL2DVideoDriver::FinishUpdate
//
void SDLGL2DVideoDriver::FinishUpdate()
{
}

//
// SDLGL2DVideoDriver::ReadScreen
//
void SDLGL2DVideoDriver::ReadScreen(byte *scr)
{
}

//
// SDLGL2DVideoDriver::InitDiskFlash
//
void SDLGL2DVideoDriver::InitDiskFlash()
{
}

//
// SDLGL2DVideoDriver::BeginRead
//
void SDLGL2DVideoDriver::BeginRead()
{
}

//
// SDLGL2DVideoDriver::EndRead
//
void SDLGL2DVideoDriver::EndRead()
{
}

//
// SDLGL2DVideoDriver::SetPalette
//
void SDLGL2DVideoDriver::SetPalette(byte *pal)
{
}

//
// SDLGL2DVideoDriver::SetPrimaryBuffer
//
void SDLGL2DVideoDriver::SetPrimaryBuffer()
{
}

//
// SDLGL2DVideoDriver::UnsetPrimaryBuffer
//
void SDLGL2DVideoDriver::UnsetPrimaryBuffer()
{
}

//
// SDLGL2DVideoDriver::ShutdownGraphics
//
void SDLGL2DVideoDriver::ShutdownGraphics()
{
}

//
// SDLGL2DVideoDriver::ShutdownGraphicsPartway
//
void SDLGL2DVideoDriver::ShutdownGraphicsPartway()
{
}

//
// SDLGL2DVideoDriver::InitGraphicsMode
//
bool SDLGL2DVideoDriver::InitGraphicsMode()
{
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

