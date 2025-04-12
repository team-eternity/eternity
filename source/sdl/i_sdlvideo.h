//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: SDL-specific graphics code.
// Authors: James Haley
//

#ifndef I_SDLVIDEO_H__
#define I_SDLVIDEO_H__

// Grab the HAL video definitions
#include "../i_video.h" 

//
// SDL Video Driver
//
class SDLVideoDriver : public HALVideoDriver
{
protected:
   virtual void SetPrimaryBuffer();
   virtual void UnsetPrimaryBuffer();

public:
   virtual void FinishUpdate();
   virtual void ReadScreen(byte *scr);
   virtual void SetPalette(byte *pal);
   virtual void ShutdownGraphics();
   virtual void ShutdownGraphicsPartway();
   virtual bool InitGraphicsMode();
};

// Global singleton instance
extern SDLVideoDriver i_sdlvideodriver;

#endif

// EOF

