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
// Purpose: SDL-specific GL 2D-in-3D video code.
// Authors: James Haley, Max Waine
//

#ifndef I_SDLGL2D_H__
#define I_SDLGL2D_H__

// Grab the HAL video definitions
#include "../i_video.h"

//
// SDL GL "2D-in-3D" Video Driver
//
class SDLGL2DVideoDriver : public HALVideoDriver
{
protected:
    int colordepth;

    void DrawPixels(void *buffer, unsigned int destheight);
    void LoadPBOExtension();

    virtual void SetPrimaryBuffer();
    virtual void UnsetPrimaryBuffer();

public:
    // Overrides
    virtual void FinishUpdate();
    virtual void ReadScreen(byte *scr);
    virtual void SetPalette(byte *pal);
    virtual void ShutdownGraphics();
    virtual void ShutdownGraphicsPartway();
    virtual bool InitGraphicsMode();

    // Accessors
    void SetColorDepth(int cd) { colordepth = cd; }
};

extern SDLGL2DVideoDriver i_sdlgl2dvideodriver;

#endif

// EOF

