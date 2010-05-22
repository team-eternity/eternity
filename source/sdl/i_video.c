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
//   System-specific graphics code, along with some ill-placed
//   keyboard, mouse, and joystick code.
//
//-----------------------------------------------------------------------------

#include "SDL.h"

#include "../z_zone.h"  /* memory allocation wrappers -- killough */
#include "../doomstat.h"
#include "../am_map.h"
#include "../c_io.h"
#include "../c_runcmd.h"
#include "../d_main.h"
#include "../f_wipe.h"
#include "../i_video.h"
#include "../m_argv.h"
#include "../m_bbox.h"
#include "../m_qstr.h"
#include "../mn_engin.h"
#include "../r_draw.h"
#include "../st_stuff.h"
#include "../v_video.h"
#include "../w_wad.h"
#include "../z_zone.h"
#include "../version.h"

// haleyjd 04/15/02:
#include "../i_system.h"
#include "../in_lude.h"
#include "../d_gi.h"

#include "../r_main.h"

// ----------------------------------------------------------------------------
// WM-related stuff (see i_input.c)

extern int     usejoystick;
extern int     grabmouse;
extern int     usemouse;   // killough 10/98
extern boolean fullscreen;

void    UpdateGrab(void);
boolean MouseShouldBeGrabbed(void);
void    UpdateFocus(void);
void    I_InitKeyboard();


SDL_Surface *sdlscreen;


// ----------------------------------------------------------------------------
// Graphics Code

int      use_vsync;     // killough 2/8/98: controls whether vsync is called
boolean  noblit;

static boolean in_graphics_mode;

static SDL_Color basepal[256], colors[256];
static boolean   setpalette = false;

// haleyjd 12/03/07: 8-on-32 graphics support
static boolean crossbitdepth;


static SDL_Surface *primary_surface = NULL;

// haleyjd 07/15/09
char *i_default_videomode;
char *i_videomode;

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit(void)
{
}

//
// I_FinishUpdate
//
void I_FinishUpdate(void)
{
   if(noblit || !in_graphics_mode)
      return;

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
// I_ReadScreen
//
void I_ReadScreen(byte *scr)
{
   VBuffer temp;

   V_InitVBufferFrom(&temp, vbscreen.width, vbscreen.height, 
                     vbscreen.width, video.bitdepth, scr);
   V_BlitVBuffer(&temp, 0, 0, &vbscreen, 0, 0, vbscreen.width, vbscreen.height);
   V_FreeVBuffer(&temp);
}


int disk_icon;
static SDL_Rect drect;
static SDL_Surface *disk = NULL, *disk_bg = NULL;

//
// killough 10/98: init disk icon
//
static void I_InitDiskFlash(void)
{
   SDL_Surface *disktmp = NULL;
   VBuffer diskvb;

   // haleyjd 05/21/06: no disk in some game modes...
   if(!(GameModeInfo->flags & GIF_HASDISK))
      return;

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
                     disktmp->pixels);
   V_SetScaling(&diskvb, 16, 15);

   // draw the disk graphic into the VBuffer
   V_DrawPatch(0, -1, &diskvb,
               W_CacheLumpName(cdrom_mode ? "STCDROM" : "STDISK", PU_CACHE));

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
// killough 10/98: draw disk icon
//
void I_BeginRead(void)
{
   if(!disk_icon || !disk || !disk_bg || !in_graphics_mode)
      return;

   SDL_BlitSurface(sdlscreen, &drect, disk_bg, NULL);
   SDL_BlitSurface(disk, NULL, sdlscreen, &drect);
   SDL_UpdateRect(sdlscreen, drect.x, drect.y, drect.w, drect.h);
}

//
// killough 10/98: erase disk icon
//
void I_EndRead(void)
{
   if(!disk_icon || !disk_bg || !in_graphics_mode)
      return;
   
   SDL_BlitSurface(disk_bg, NULL, sdlscreen, &drect);
   SDL_UpdateRect(sdlscreen, drect.x, drect.y, drect.w, drect.h);
}

//
// I_SetPaletteDirect
//
// haleyjd 11/12/09: make sure surface palettes are set at startup.
//
static void I_SetPaletteDirect(byte *palette)
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
// I_SetPalette
//
void I_SetPalette(byte *palette)
{
   int i;
   
   if(!in_graphics_mode)             // killough 8/11/98
      return;

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


void I_UnsetPrimaryBuffer(void)
{
   if(primary_surface)
   {
      SDL_FreeSurface(primary_surface);
      primary_surface = NULL;
   }
}


void I_SetPrimaryBuffer(void)
{
   int bump = (video.width == 512 || video.width == 1024) ? 4 : 0;

   if(sdlscreen)
   {
      primary_surface = 
         SDL_CreateRGBSurface(SDL_SWSURFACE, video.width + bump, video.height,
                              8, 0, 0, 0, 0);
      video.screens[0] = (byte *)primary_surface->pixels;
      video.pitch = primary_surface->pitch;
   }
}

//
// I_ShutdownGraphicsPartway
//
// haleyjd: It was necessary to separate this code from I_ShutdownGraphics
// so that the ENDOOM screen can be displayed during shutdown. Otherwise,
// the SDL_QuitSubSystem call below would cause a nasty crash.
//
static void I_ShutdownGraphicsPartway(void)
{
   if(in_graphics_mode)
   {
      // haleyjd 06/21/06: use UpdateGrab here, not release
      UpdateGrab();
      in_graphics_mode = false;
      in_textmode = true;
      sdlscreen = NULL;
      I_UnsetPrimaryBuffer();
   }
}

void I_ShutdownGraphics(void)
{
   if(in_graphics_mode)  // killough 10/98
   {
      I_ShutdownGraphicsPartway();
      
      // haleyjd 10/09/05: shut down graphics earlier
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
   }
}


#define BADVID "video mode not supported"

extern boolean setsizeneeded;

// states for geometry parser
enum
{
   STATE_WIDTH,
   STATE_HEIGHT,
   STATE_FLAGS
};

//
// I_ParseGeom
//
// Function to parse geometry description strings in the form [wwww]x[hhhh][f].
// This is now the primary way in which Eternity stores its video mode setting.
//
static void I_ParseGeom(const char *geom, 
                        int *w, int *h, boolean *fs, boolean *vs, boolean *hw,
                        boolean *wf)
{
   const char *c = geom;
   int state = STATE_WIDTH;
   int tmpwidth = 320, tmpheight = 200;
   qstring_t qstr;
   boolean errorflag = false;

   M_QStrInitCreate(&qstr);

   while(*c)
   {
      switch(state)
      {
      case STATE_WIDTH:
         if(*c >= '0' && *c <= '9')
            M_QStrPutc(&qstr, *c);
         else
         {
            int width = atoi(qstr.buffer);
            if(width < 320 || width > MAX_SCREENWIDTH)
            {
               state = STATE_FLAGS;
               errorflag = true;
            }
            else
            {
               tmpwidth = width;
               M_QStrClear(&qstr);
               state = STATE_HEIGHT;
            }
         }
         break;
      case STATE_HEIGHT:
         if(*c >= '0' && *c <= '9')
            M_QStrPutc(&qstr, *c);
         else
         {
            int height = atoi(qstr.buffer);
            if(height < 200 || height > MAX_SCREENHEIGHT)
            {
               state = STATE_FLAGS;
               errorflag = true;
            }
            else
            {
               tmpheight = height;
               state = STATE_FLAGS;
               continue; // don't increment the pointer
            }
         }
         break;
      case STATE_FLAGS:
         switch(tolower(*c))
         {
         case 'w': // window
            *fs = false;
            break;
         case 'f': // fullscreen
            *fs = true;
            break;
         case 'a': // async update
            *vs = false;
            break;
         case 'v': // vsync update
            *vs = true;
            break;
         case 's': // software
            *hw = false;
            break;
         case 'h': // hardware 
            *hw = true;
            break;
         case 'n': // noframe
            *wf = false;
            break;
         default:
            break;
         }
         break;
      }
      ++c;
   }

   // handle termination of loop during STATE_HEIGHT (no flags)
   if(state == STATE_HEIGHT)
   {
      int height = atoi(qstr.buffer);

      if(height < 200 || height > MAX_SCREENHEIGHT)
         errorflag = true;
      else
         tmpheight = height;
   }

   // if an error occurs setting w/h, we default.
   if(errorflag)
   {
      tmpwidth  = 640;
      tmpheight = 480;
   }

   *w = tmpwidth;
   *h = tmpheight;

   M_QStrFree(&qstr);
}

//
// I_CheckVideoCmds
//
// Checks for all video-mode-related command-line parameters in one
// convenient location. Though called from I_InitGraphicsMode, this
// function will only run once at startup. Resolution changes made at
// runtime want to use the precise settings specified through the UI
// instead.
//
static void I_CheckVideoCmds(int *w, int *h, boolean *fs, boolean *vs, 
                             boolean *hw, boolean *wf)
{
   static boolean firsttime = true;
   int p;

   if(firsttime)
   {
      firsttime = false;

      if((p = M_CheckParm("-geom")) && p < myargc - 1)
         I_ParseGeom(myargv[p + 1], w, h, fs, vs, hw, wf);

      if((p = M_CheckParm("-vwidth")) && p < myargc - 1 &&
         (p = atoi(myargv[p + 1])) >= 320 && p <= MAX_SCREENWIDTH)
         *w = p;
      
      if((p = M_CheckParm("-vheight")) && p < myargc - 1 &&
         (p = atoi(myargv[p + 1])) >= 200 && p <= MAX_SCREENHEIGHT)
         *h = p;
      
      if(M_CheckParm("-fullscreen"))
         *fs = true;
      if(M_CheckParm("-nofullscreen") || M_CheckParm("-window"))
         *fs = false;
      
      if(M_CheckParm("-vsync"))
         *vs = true;
      if(M_CheckParm("-novsync"))
         *vs = false;

      if(M_CheckParm("-hardware"))
         *hw = true;
      if(M_CheckParm("-software"))
         *hw = false;

      if(M_CheckParm("-frame"))
         *wf = true;
      if(M_CheckParm("-noframe"))
         *wf = false;
   }
}

#ifdef _MSC_VER
extern void I_DisableSysMenu(void);
#endif

//
// I_InitGraphicsMode
//
// killough 11/98: New routine, for setting hires and page flipping
// sf: now returns true if an error occurred
//
static boolean I_InitGraphicsMode(void)
{
   boolean  wantfullscreen = false;
   boolean  wantvsync      = false;
   boolean  wanthardware   = false;
   boolean  wantframe      = true;
   int      v_w            = 640;
   int      v_h            = 480;
   int      v_bd           = 8;
   int      flags          = SDL_SWSURFACE;

   if(!i_default_videomode)
      i_default_videomode = strdup("640x480w");

   if(!i_videomode)
      i_videomode = strdup(i_default_videomode);

   // haleyjd 10/09/05: from Chocolate DOOM
   // mouse grabbing   
   if(M_CheckParm("-grabmouse"))
      grabmouse = 1;
   else if(M_CheckParm("-nograbmouse"))
      grabmouse = 0;

   // haleyjd 12/03/07: cross-bit-depth support
   if(M_CheckParm("-8in32"))
   {
      v_bd = 32;
      crossbitdepth = true;
   }

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
      if(!SDL_VideoModeOK(320, 200, 8, SDL_SWSURFACE) ||
         !(sdlscreen = SDL_SetVideoMode(320, 200, 8, SDL_SWSURFACE)))
      {
         I_FatalError(I_ERR_KILL,
                      "I_InitGraphicsMode: couldn't set mode %dx%dx%d;\n"
                      "   Also failed to set safety mode 320x200x8.\n"
                      "   Check your SDL video driver settings.\n",
                      v_w, v_h, v_bd);
      }

      // reset these for below population of video struct
      v_w = 320;
      v_h = 200;
   }

   // haleyjd 10/14/09: wait for a bit so the screen can settle
   if(flags & SDL_FULLSCREEN)
      I_Sleep(500);

   R_ResetFOV(v_w, v_h);

   // haleyjd 10/09/05: keep track of fullscreen state
   fullscreen = (sdlscreen->flags & SDL_FULLSCREEN) == SDL_FULLSCREEN;

   // haleyjd 12/03/07: if the video surface is not actually 32-bit, we
   // must disable cross-bit-depth drawing
   if(sdlscreen->format->BitsPerPixel != 32)
      crossbitdepth = false;

   SDL_WM_SetCaption(ee_wmCaption, ee_wmCaption);

#ifdef _MSC_VER
   // Win32 specific hack: disable system menu
   I_DisableSysMenu();
#endif

   UpdateFocus();
   UpdateGrab();

   video.width     = v_w;
   video.height    = v_h;
   video.bitdepth  = 8;
   video.pixelsize = 1;
   
   V_Init();      
   
   in_graphics_mode = true;
   in_textmode = false;
   
   setsizeneeded = true;
   
   // haleyjd 11/12/09: set surface palettes immediately
   I_SetPaletteDirect(W_CacheLumpName("PLAYPAL", PU_CACHE));

   I_InitDiskFlash();        // Initialize disk icon
    
   return false;
}

//
// I_ResetScreen
//
// Changes the resolution by getting out of the old mode and into the new one,
// and then waking up any interested game-code modules that need to know about
// the screen resolution.
//
static void I_ResetScreen(void)
{  
   // Switch out of old graphics mode
   // haleyjd 10/15/05: WOOPS!
   I_ShutdownGraphicsPartway();
   
   // Switch to new graphics mode
   // check for errors -- we may be setting to a different mode instead
   if(I_InitGraphicsMode())
      return;
   
   // reset other modules
   
   // Reset automap dimensions
   if(automapactive)
      AM_Start();
   
   // Reset palette
   ST_Start();
   
   // Redraw cached intermission buffer if needed
   if(gamestate == GS_INTERMISSION)
   {
      IN_DrawBackground();
      V_CopyRect(0, 0, &backscreen1, SCREENWIDTH, SCREENHEIGHT, 0, 0, &vbscreen);
   }

   // haleyjd: reset wipe engine
   Wipe_ScreenReset();
   
   // A LOT of heap activity just happened, so check it.
   Z_CheckHeap();
}

void I_InitGraphics(void)
{
   static int firsttime = true;
   
   if(!firsttime)
      return;
   
   firsttime = false;
   
   // haleyjd: not a good idea for SDL :(
   // if(nodrawers) // killough 3/2/98: possibly avoid gfx mode
   //    return;

   // init keyboard
   I_InitKeyboard();

   // enable key repeat
   SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY/2, SDL_DEFAULT_REPEAT_INTERVAL*4);

   //
   // enter graphics mode
   //
   
   atexit(I_ShutdownGraphics);
   
   V_ResetMode();
   
   Z_CheckHeap();
}




// the list of video modes is stored here in i_video.c
// the console commands to change them are in v_misc.c,
// so that all the platform-specific stuff is in here.
// v_misc.c does not care about the format of the videomode_t,
// all it asks is that it contains a text value 'description'
// which describes the mode
        
videomode_t videomodes[]=
{
  // [hires, pageflip, vesa (unused for SDL)], description
  {0, 0, 0, "320x200 Windowed"},
  {0, 0, 0, "320x200 Fullscreen"},
  {0, 0, 0, "320x240 Windowed"},
  {0, 0, 0, "320x240 Fullscreen"},
  {0, 0, 0, "640x400 Windowed"},
  {0, 0, 0, "640x400 Fullscreen"},
  {0, 0, 0, "640x480 Windowed"},
  {0, 0, 0, "640x480 Fullscreen"},
  {0, 0, 0, "800x600 Windowed"},
  {0, 0, 0, "800x600 Fullscreen"},
  {0, 0, 0, "1024x768 Windowed"},
  {0, 0, 0, "1024x768 Fullscreen"},
  {0, 0, 0,  NULL}  // last one has NULL description
};

void I_SetMode(int i)
{
   static int firsttime = true;    // the first time to set mode
   
   if(firsttime)
      I_InitGraphicsMode();
   else
      I_ResetScreen();
   
   firsttime = false;
}        

/************************
        CONSOLE COMMANDS
 ************************/

VARIABLE_BOOLEAN(use_vsync, NULL,  yesno);
VARIABLE_BOOLEAN(disk_icon, NULL,  onoff);

CONSOLE_VARIABLE(v_diskicon, disk_icon, 0) {}
CONSOLE_VARIABLE(v_retrace, use_vsync, 0)
{
   V_ResetMode();
}

VARIABLE_BOOLEAN(usemouse,    NULL, yesno);
VARIABLE_BOOLEAN(usejoystick, NULL, yesno);

CONSOLE_VARIABLE(i_usemouse, usemouse, 0) {}
CONSOLE_VARIABLE(i_usejoystick, usejoystick, 0) {}

// haleyjd 04/15/02: joystick sensitivity variables
VARIABLE_INT(joystickSens_x, NULL, -32768, 32767, NULL);
VARIABLE_INT(joystickSens_y, NULL, -32768, 32767, NULL);

CONSOLE_VARIABLE(joySens_x, joystickSens_x, 0) {}
CONSOLE_VARIABLE(joySens_y, joystickSens_y, 0) {}

// haleyjd 03/27/06: mouse grabbing
VARIABLE_BOOLEAN(grabmouse, NULL, yesno);
CONSOLE_VARIABLE(i_grabmouse, grabmouse, 0) {}

VARIABLE_STRING(i_videomode, NULL, UL);
CONSOLE_VARIABLE(i_videomode, i_videomode, cf_buffered)
{
   V_ResetMode();
}

CONSOLE_COMMAND(i_default_videomode, 0)
{
   if(i_default_videomode)
      free(i_default_videomode);

   i_default_videomode = strdup(i_videomode);
}

void I_Video_AddCommands(void)
{
   C_AddCommand(i_usemouse);
   C_AddCommand(i_usejoystick);
   
   C_AddCommand(v_diskicon);
   C_AddCommand(v_retrace);
   
   C_AddCommand(joySens_x);
   C_AddCommand(joySens_y);

   C_AddCommand(i_grabmouse);

   C_AddCommand(i_videomode);
   C_AddCommand(i_default_videomode);
}

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
