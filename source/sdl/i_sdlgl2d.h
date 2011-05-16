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

public:
   // Overrides
   virtual void FinishUpdate();
   virtual void ReadScreen(byte *scr);
   virtual void InitDiskFlash();
   virtual void BeginRead();
   virtual void EndRead();
   virtual void SetPalette(byte *pal);
   virtual void SetPrimaryBuffer();
   virtual void UnsetPrimaryBuffer();
   virtual void ShutdownGraphics();
   virtual void ShutdownGraphicsPartway();
   virtual bool InitGraphicsMode();

   // Accessors
   void SetColorDepth(int cd) { colordepth = cd; }
};


#endif

// EOF

