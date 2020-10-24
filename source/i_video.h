// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Hardware Abstraction Layer Video Interface
//
//-----------------------------------------------------------------------------

#ifndef I_VIDEO_H__
#define I_VIDEO_H__

#include "doomtype.h"

struct SDL_Window;

enum class screentype_e : int
{
   WINDOWED,
   FULLSCREEN_DESKTOP,
   FULLSCREEN,
};

//
// Video Driver Base Class
//
// All video drivers should inherit this interface.
//
class HALVideoDriver
{
protected:
   virtual void SetPrimaryBuffer()        = 0;
   virtual void UnsetPrimaryBuffer()      = 0;

public:
   virtual void FinishUpdate()            = 0;
   virtual void ReadScreen(byte *scr)     = 0;
   virtual void SetPalette(byte *pal)     = 0;
   virtual void ShutdownGraphics()        = 0;
   virtual void ShutdownGraphicsPartway() = 0;
   virtual bool InitGraphicsMode()        = 0;

   SDL_Window *window = nullptr;
};

void I_StartTic();

// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode

void I_InitGraphics();
void I_ShutdownGraphics();

// Takes full 8 bit values.
void I_SetPalette(byte *palette);

void I_FinishUpdate();

void I_ReadScreen(byte *scr);

void I_CheckVideoCmds(int &w, int &h, screentype_e &st, bool &vs, bool &hw, bool &wf);
void I_ParseGeom(const char *geom, int &w, int &h, screentype_e &st, bool &vs, bool &hw, bool &wf);
void I_ParseResolution(const char *resolution, int &w, int &h, const int window_w, const int window_h);

// letterboxing utilities
bool I_VideoShouldLetterbox(int w, int h);
int  I_VideoLetterboxHeight(int w);
int  I_VideoLetterboxOffset(int h, int hl);

void I_ToggleFullscreen();

extern int use_vsync;  // killough 2/8/98: controls whether vsync is called

// video modes

void I_SetMode();

extern char *i_default_resolution;
extern char *i_resolution;
extern char *i_videomode;
extern char *i_default_videomode;
extern int   i_videodriverid;
extern bool  i_letterbox;
extern int   displaynum;

// Driver enumeration
enum halvdr_e
{
   VDR_SDLDEFAULT,
   VDR_SDLGL2D,
   VDR_MAXDRIVERS
};

extern const char *const i_videohelpstr;

// Below here has been moved to i_input.c
// haleyjd

int I_DoomCode2ScanCode(int);   // killough
int I_ScanCode2DoomCode(int);   // killough

#endif

//----------------------------------------------------------------------------
//
// $Log: i_video.h,v $
// Revision 1.4  1998/05/03  22:40:58  killough
// beautification
//
// Revision 1.3  1998/02/09  03:01:51  killough
// Add vsync for flicker-free blits
//
// Revision 1.2  1998/01/26  19:27:01  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
