// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Hardware Abstraction Layer Video Interface
//
//-----------------------------------------------------------------------------

#ifndef I_VIDEO_H__
#define I_VIDEO_H__

#include "doomtype.h"

//
// Video Driver Base Class
//
// All video drivers should inherit this interface.
//
class HALVideoDriver
{
public:
   virtual void FinishUpdate()            = 0;
   virtual void ReadScreen(byte *scr)     = 0;
   virtual void InitDiskFlash()           = 0;
   virtual void BeginRead()               = 0;
   virtual void EndRead()                 = 0;
   virtual void SetPalette(byte *pal)     = 0;
   virtual void SetPrimaryBuffer()        = 0;
   virtual void UnsetPrimaryBuffer()      = 0;
   virtual void ShutdownGraphics()        = 0;
   virtual void ShutdownGraphicsPartway() = 0;
   virtual bool InitGraphicsMode()        = 0;
};


// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode

void I_InitGraphics(void);
void I_ShutdownGraphics(void);

// Takes full 8 bit values.
void I_SetPalette (byte* palette);

void I_FinishUpdate (void);

// Wait for vertical retrace or pause a bit.
void I_WaitVBL(int count);

void I_ReadScreen (byte* scr);

void I_CheckVideoCmds(int *w, int *h, bool *fs, bool *vs, bool *hw, bool *wf);
void I_ParseGeom(const char *geom, int *w, int *h, bool *fs, bool *vs, bool *hw,
                 bool *wf);

extern int use_vsync;  // killough 2/8/98: controls whether vsync is called
extern int disk_icon;  // killough 10/98

// video modes

typedef struct videomode_s
{
  bool hires;
  bool pageflip;
  bool vesa;
  const char *description;
} videomode_t;

void I_SetMode(int i);

extern char *i_videomode;
extern char *i_default_videomode;
extern int   i_videodriverid;

// Driver enumeration
enum
{
   VDR_SDLSOFT,
   VDR_SDLGL2D,
   VDR_MAXDRIVERS
};

extern const char *const i_videohelpstr;

// Below here has been moved to i_input.c
// haleyjd
#ifdef _SDL_VER
extern int joystickSens_x;
extern int joystickSens_y;
#endif

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
