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
#include "../v_video.h"
#include "../version.h"
#include "../w_wad.h"

//=============================================================================
//
// WM-related stuff (see i_input.c)
//

extern int  usejoystick;
extern int  grabmouse;
extern int  usemouse;   // killough 10/98
extern bool fullscreen;

void UpdateGrab(void);
bool MouseShouldBeGrabbed(void);
void UpdateFocus(void);

SDL_Surface *sdlscreen;
static SDL_Surface *primary_surface = NULL;

//=============================================================================
//
// Graphics Code
//

extern int  use_vsync;     // killough 2/8/98: controls whether vsync is called
extern bool noblit;

static SDL_Color basepal[256], colors[256];
extern bool setpalette;

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
void SDLVideoDriver::FinishUpdate(void)
{
   // haleyjd 10/08/05: from Chocolate DOOM:
   UpdateGrab();

   // Don't update the screen if the window isn't visible.
   // Not doing this breaks under Windows when we alt-tab away 
   // while fullscreen.   
   if(!(SDL_GetAppState() & SDL_APPACTIVE))
      return;

   if(setpalette)
   {
      if(!crossbitdepth)
         SDL_SetPalette(sdlscreen, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);

      if(primary_surface)
         SDL_SetPalette(primary_surface, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);

      setpalette = false;
   }

   // haleyjd 11/12/09: blit *after* palette set improves behavior.
   if(primary_surface)
      SDL_BlitSurface(primary_surface, NULL, sdlscreen, NULL);

   // haleyjd 11/12/09: ALWAYS update. Causes problems with some video surface
   // types otherwise.
   SDL_Flip(sdlscreen);
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

static SDL_Rect drect;
static SDL_Surface *disk = NULL, *disk_bg = NULL;

//
// SDLVideoDriver::InitDiskFlash
// 
// killough 10/98: init disk icon
//
void SDLVideoDriver::InitDiskFlash()
{
   SDL_Surface *disktmp = NULL;
   VBuffer diskvb;

   if(disk)
   {
      SDL_FreeSurface(disk);
      SDL_FreeSurface(disk_bg);
   }

   if(vbscreen.scaled)
   {
      drect.x = vbscreen.x1lookup[vbscreen.unscaledw - 16];
      drect.y = vbscreen.y1lookup[vbscreen.unscaledh - 15];
      drect.w = vbscreen.x2lookup[vbscreen.unscaledw - 1] - drect.x + 1;
      drect.h = vbscreen.y2lookup[vbscreen.unscaledh - 1] - drect.y + 1;
   }
   else
   {
      drect.x = vbscreen.width - 16;
      drect.y = vbscreen.height - 15;
      drect.w = 16;
      drect.h = 15;
   }

   // haleyjd 12/26/09: create disk in a temporary 8-bit surface, unconditionally
   disktmp = SDL_CreateRGBSurface(0, drect.w, drect.h, 8, 0, 0, 0, 0);
   SDL_SetPalette(disktmp, SDL_LOGPAL, colors, 0, 256);

   // setup VBuffer and point it into the SDL_Surface
   V_InitVBufferFrom(&diskvb, drect.w, drect.h, disktmp->pitch, 8, 
                     (byte *)(disktmp->pixels));
   V_SetScaling(&diskvb, 16, 15);

   // draw the disk graphic into the VBuffer
   V_DrawPatch(0, -1, &diskvb,
               (patch_t *)W_CacheLumpName(cdrom_mode ? "STCDROM" : "STDISK", PU_CACHE));

   // Done with VBuffer object
   V_FreeVBuffer(&diskvb);

   // Convert 8-bit disk into a surface-format surface - let SDL handle the 
   // specifics of the process, as it's designed for it.
   disk    = SDL_DisplayFormat(disktmp);
   disk_bg = SDL_DisplayFormat(disktmp);

   // Done with the temporary 8-bit disk.
   SDL_FreeSurface(disktmp);
}

//
// SDLVideoDriver::BeginRead
//
// killough 10/98: draw disk icon
//
void SDLVideoDriver::BeginRead()
{
   if(!disk || !disk_bg)
      return;

   SDL_BlitSurface(sdlscreen, &drect, disk_bg, NULL);
   SDL_BlitSurface(disk, NULL, sdlscreen, &drect);
   SDL_UpdateRect(sdlscreen, drect.x, drect.y, drect.w, drect.h);
}

//
// SDLVideoDriver::EndRead
//
// killough 10/98: erase disk icon
//
void SDLVideoDriver::EndRead(void)
{
   if(!disk_bg)
      return;
   
   SDL_BlitSurface(disk_bg, NULL, sdlscreen, &drect);
   SDL_UpdateRect(sdlscreen, drect.x, drect.y, drect.w, drect.h);
}

static bool setpalette = false;

//
// I_SDLSetPaletteDirect
//
// haleyjd 11/12/09: make sure surface palettes are set at startup.
//
static void I_SDLSetPaletteDirect(byte *palette)
{
   int i;

   for(i = 0; i < 256; ++i)
   {
      colors[i].r = gammatable[usegamma][*palette++];
      colors[i].g = gammatable[usegamma][*palette++];
      colors[i].b = gammatable[usegamma][*palette++];
   }

   if(sdlscreen && !crossbitdepth)
      SDL_SetPalette(sdlscreen, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);

   if(primary_surface)
      SDL_SetPalette(primary_surface, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);
}

//
// SDLVideoDriver::SetPalette
//
// Set the palette, or, if palette is NULL, update the current palette to use 
// the current gamma setting.
//
void SDLVideoDriver::SetPalette(byte *palette)
{
   int i;

   if(!palette)
   {
      // Gamma change
      for(i = 0; i < 256; ++i)
      {
         colors[i].r = gammatable[usegamma][basepal[i].r];
         colors[i].g = gammatable[usegamma][basepal[i].g];
         colors[i].b = gammatable[usegamma][basepal[i].b];
      }
   }
   else
   {
      for(i = 0; i < 256; ++i)
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

   if(sdlscreen)
   {
      primary_surface = 
         SDL_CreateRGBSurface(SDL_SWSURFACE, video.width + bump, video.height,
                              8, 0, 0, 0, 0);
      if(!primary_surface)
         I_FatalError(I_ERR_KILL, "Failed to create screen temp buffer\n");

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
   UpdateGrab();
   sdlscreen = NULL;
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
   static int fallback_w     = 640;
   static int fallback_h     = 480;
   static int fallback_bd    =   8;
   static int fallback_flags = SDL_SWSURFACE;

   bool wantfullscreen = false;
   bool wantvsync      = false;
   bool wanthardware   = false;
   bool wantframe      = true;
   int  v_w            = 640;
   int  v_h            = 480;
   int  v_bd           = 8;
   int  flags          = SDL_SWSURFACE;

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

   if(wanthardware)
      flags = SDL_HWSURFACE;

   if(wantvsync)
      flags = SDL_HWSURFACE | SDL_DOUBLEBUF;

   if(wantfullscreen)
      flags |= SDL_FULLSCREEN;

   // haleyjd 10/27/09
   if(!wantframe)
      flags |= SDL_NOFRAME;
     
   if(!SDL_VideoModeOK(v_w, v_h, v_bd, flags) ||
      !(sdlscreen = SDL_SetVideoMode(v_w, v_h, v_bd, flags)))
   {
      // try 320x200w safety mode
      if(!SDL_VideoModeOK(fallback_w, fallback_h, fallback_bd, fallback_flags) ||
         !(sdlscreen = SDL_SetVideoMode(fallback_w, fallback_h, fallback_bd, fallback_flags)))
      {
         I_FatalError(I_ERR_KILL,
                      "I_SDLInitGraphicsMode: couldn't set mode %dx%dx%d;\n"
                      "   Also failed to restore fallback mode %dx%dx%d.\n"
                      "   Check your SDL video driver settings.\n",
                      v_w, v_h, v_bd,
                      fallback_w, fallback_h, fallback_bd);
      }

      // reset these for below population of video struct
      v_w   = fallback_w;
      v_h   = fallback_h;
      v_bd  = fallback_bd;
      flags = fallback_flags;
   }

   // Record successful mode set for use as a fallback mode
   fallback_w     = v_w;
   fallback_h     = v_h;
   fallback_bd    = v_bd;
   fallback_flags = flags;

   // haleyjd 10/14/09: wait for a bit so the screen can settle
   if(flags & SDL_FULLSCREEN)
      I_Sleep(500);

   // haleyjd 10/09/05: keep track of fullscreen state
   fullscreen = (sdlscreen->flags & SDL_FULLSCREEN) == SDL_FULLSCREEN;

   // haleyjd 12/03/07: if the video surface is not high-color, we
   // disable cross-bit-depth drawing for efficiency
   if(sdlscreen->format->BitsPerPixel == 8)
      crossbitdepth = false;

   SDL_WM_SetCaption(ee_wmCaption, ee_wmCaption);

   UpdateFocus();
   UpdateGrab();

   video.width     = v_w;
   video.height    = v_h;
   video.bitdepth  = 8;
   video.pixelsize = 1;
   
   // haleyjd 11/12/09: set surface palettes immediately
   I_SDLSetPaletteDirect((byte *)W_CacheLumpName("PLAYPAL", PU_CACHE));

   return false;
}

// The one and only global instance of the SDL video driver.
SDLVideoDriver i_sdlvideodriver;

//----------------------------------------------------------------------------
//
// $Log: i_video.c,v $
// Revision 1.12  1998/05/03  22:40:35  killough
// beautification
//
// Revision 1.11  1998/04/05  00:50:53  phares
// Joystick support, Main Menu re-ordering
//
// Revision 1.10  1998/03/23  03:16:10  killough
// Change to use interrupt-driver keyboard IO
//
// Revision 1.9  1998/03/09  07:13:35  killough
// Allow CTRL-BRK during game init
//
// Revision 1.8  1998/03/02  11:32:22  killough
// Add pentium blit case, make -nodraw work totally
//
// Revision 1.7  1998/02/23  04:29:09  killough
// BLIT tuning
//
// Revision 1.6  1998/02/09  03:01:20  killough
// Add vsync for flicker-free blits
//
// Revision 1.5  1998/02/03  01:33:01  stan
// Moved __djgpp_nearptr_enable() call from I_video.c to i_main.c
//
// Revision 1.4  1998/02/02  13:33:30  killough
// Add support for -noblit
//
// Revision 1.3  1998/01/26  19:23:31  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/26  05:59:14  killough
// New PPro blit routine
//
// Revision 1.1.1.1  1998/01/19  14:02:50  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
