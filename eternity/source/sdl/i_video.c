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

#include <stdio.h>

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


#define NEWSETPAL

int      use_vsync;     // killough 2/8/98: controls whether vsync is called
boolean  noblit;

static boolean in_graphics_mode;
static int     scroll_offset;

static SDL_Color basepal[256], colors[256];
static boolean   setpalette = false;

// haleyjd 12/03/07: 8-on-32 graphics support
static boolean crossbitdepth;


static SDL_Surface *primary_surface = NULL;


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit(void)
{
}



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

   if(primary_surface)
   {
      SDL_BlitSurface(primary_surface, NULL, sdlscreen, NULL);
   }


#ifdef NEWSETPAL
   if(setpalette)
   {
      if(!crossbitdepth)
         SDL_SetPalette(sdlscreen, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);

      if(primary_surface)
         SDL_SetPalette(primary_surface, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);

      setpalette = false;
   }
   else if(!setpalette || crossbitdepth || (sdlscreen->flags & SDL_DOUBLEBUF))
#endif
      SDL_Flip(sdlscreen);
}

//
// I_ReadScreen
//
void I_ReadScreen(byte *scr)
{
   VBuffer temp;

   memcpy(&temp, &vbscreen, sizeof(VBuffer));
   temp.data = scr;
   temp.pitch = temp.width;

   V_BlitVBuffer(&temp, 0, 0, &vbscreen, 0, 0, vbscreen.width, vbscreen.height);
}

//
// killough 10/98: init disk icon
//

int disk_icon;
static SDL_Surface *diskflash, *old_data;

static void I_InitDiskFlash(void)
{
/*  byte temp[32*32];

  if (diskflash)
    {
      SDL_FreeSurface(diskflash);
      SDL_FreeSurface(old_data);
    }

  //sf : disk is actually 16x15
  diskflash = SDL_CreateRGBSurface(SDL_SWSURFACE, 16<<hires, 15<<hires, 8, 0, 0, 0, 0);
  old_data = SDL_CreateRGBSurface(SDL_SWSURFACE, 16<<hires, 15<<hires, 8, 0, 0, 0, 0);

  V_GetBlock(0, 0, 0, 16, 15, temp);
  V_DrawPatchDirect(0, -1, &vbscreen, 
                    W_CacheLumpName(cdrom_mode ? "STCDROM" : "STDISK", 
                                    PU_CACHE));
  V_GetBlock(0, 0, 0, 16, 15, (char *)diskflash->pixels);
  V_DrawBlock(0, 0, &vbscreen, 16, 15, temp);*/
}

//
// killough 10/98: draw disk icon
//

void I_BeginRead(void)
{
   if(!disk_icon || !in_graphics_mode)
      return;

  /*blit(screen, old_data,
       (SCREENWIDTH-16) << hires,
       scroll_offset + ((SCREENHEIGHT-15)<<hires),
       0, 0, 16 << hires, 15 << hires);

  blit(diskflash, screen, 0, 0, (SCREENWIDTH-16) << hires,
       scroll_offset + ((SCREENHEIGHT-15)<<hires), 16 << hires, 15 << hires);*/
}

//
// killough 10/98: erase disk icon
//

void I_EndRead(void)
{
   if(!disk_icon || !in_graphics_mode)
      return;

/*  blit(old_data, screen, 0, 0, (SCREENWIDTH-16) << hires,
       scroll_offset + ((SCREENHEIGHT-15)<<hires), 16 << hires, 15 << hires);*/
}


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

#ifndef NEWSETPAL
   if(!crossbitdepth)
      SDL_SetPalette(sdlscreen, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);

   if(primary_surface)
      SDL_SetPalette(primary_surface, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);
#endif
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
         SDL_CreateRGBSurface(SDL_SWSURFACE, video.width + bump, video.height, 8, 
                              0, 0, 0, 0);
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
   STATE_FLAGS,
};

static void I_ParseGeom(const char *geom, 
                        int *w, int *h, boolean *fs, boolean *vs, boolean *hw)
{
   const char *c = geom;
   int state = STATE_WIDTH;
   qstring_t qstr;

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
            if(width < 320)
               width = 320;
            else if(width > 1024)
               width = 1024;
            *w = width;
            M_QStrClear(&qstr);
            state = STATE_HEIGHT;
         }
         break;
      case STATE_HEIGHT:
         if(*c >= '0' && *c <= '9')
            M_QStrPutc(&qstr, *c);
         else
         {
            int height = atoi(qstr.buffer);
            if(height < 200)
               height = 200;
            else if(height > 768)
               height = 768;
            *h = height;
            state = STATE_FLAGS;
            continue; // don't increment the pointer
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
         }
         break;
      }
      ++c;
   }

   M_QStrFree(&qstr);
}

static void I_CheckVideoCmds(int *w, int *h, boolean *fs, boolean *vs, 
                             boolean *hw)
{
   static boolean firsttime = true;
   int p;

   if(firsttime)
   {
      firsttime = false;

      if((p = M_CheckParm("-geom")) && p < myargc - 1)
         I_ParseGeom(myargv[p + 1], w, h, fs, vs, hw);

      if((p = M_CheckParm("-vwidth")) && p < myargc - 1 &&
         (p = atoi(myargv[p + 1])) >= 320 && p <= 1024)
         *w = p;
      
      if((p = M_CheckParm("-vheight")) && p < myargc - 1 &&
         (p = atoi(myargv[p + 1])) >= 200 && p <= 768)
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
   }
}

//
// killough 11/98: New routine, for setting hires and page flipping
//

// sf: now returns true if an error occurred
static boolean I_InitGraphicsMode(void)
{
   boolean  wantfullscreen = false;
   boolean  wantvsync      = false;
   boolean  wanthardware   = false;
   int      v_w            = SCREENWIDTH;
   int      v_h            = SCREENHEIGHT;
   int      v_bd           = 8;
   int      flags          = SDL_SWSURFACE;
   SDL_Event dummy;


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
   
   scroll_offset = 0;
   switch(v_mode)
   {
   case 2:
   case 3:
      v_w = 320;
      v_h = 240;
      break;
   case 4:
   case 5:
      v_w = 640;
      v_h = 400;
      break;
   case 6:
   case 7:
      v_w = 640;
      v_h = 480;
      break;
   case 8:
   case 9:
      v_w = 800;
      v_h = 600;
      break;
   case 10:
   case 11:
      v_w = 1024;
      v_h = 768;
      break;
   }
   
   // odd modes are fullscreen
   if(v_mode & 1)
      wantfullscreen = true;

   // haleyjd 06/21/06: allow complete command line overrides but only
   // on initial video mode set (setting from menu doesn't support this)
   I_CheckVideoCmds(&v_w, &v_h, &wantfullscreen, &wantvsync, &wanthardware);

   if(wanthardware)
      flags = SDL_HWSURFACE;

   if(wantvsync)
      flags = SDL_HWSURFACE | SDL_DOUBLEBUF;

   if(wantfullscreen)
      flags |= SDL_FULLSCREEN;
     
   // SoM: 4/15/02: Saftey mode
   if(v_mode > 0 && SDL_VideoModeOK(v_w, v_h, v_bd, flags))
   {
      sdlscreen = SDL_SetVideoMode(v_w, v_h, v_bd, flags);
      if(!sdlscreen)
      {
         I_SetMode(0);
         MN_ErrorMsg(BADVID);
         return true;
      }
   }
   else if(v_mode == 0 && SDL_VideoModeOK(v_w, v_h, v_bd, flags))
   {
      sdlscreen = SDL_SetVideoMode(v_w, v_h, v_bd, flags);
      if(!sdlscreen)
         I_Error("Couldn't set video mode %ix%i\n", v_w, v_h);
   }
   else 
      I_Error("Couldn't set video mode %ix%i\n", v_w, v_h);

   // haleyjd 10/09/05: keep track of fullscreen state
   fullscreen = (sdlscreen->flags & SDL_FULLSCREEN) == SDL_FULLSCREEN;

   // haleyjd 12/03/07: if the video surface is not actually 32-bit, we
   // must disable cross-bit-depth drawing
   if(sdlscreen->format->BitsPerPixel != 32)
      crossbitdepth = false;

   MN_ErrorMsg("");       // clear any error messages

   SDL_WM_SetCaption(ee_wmCaption, NULL);

   UpdateFocus();
   UpdateGrab();

   video.width  = v_w;
   video.height = v_h;
   video.bitdepth = 8;
   video.pixelsize = 1;
   
   V_Init();      
   
   in_graphics_mode = true;
   in_textmode = false;
   
   setsizeneeded = true;
   
   I_InitDiskFlash();        // Initialize disk icon
   I_SetPalette(W_CacheLumpName("PLAYPAL",PU_CACHE));
   
   // haleyjd 10/09/05: from Chocolate DOOM:
   // clear out any events waiting at the start   
   while(SDL_PollEvent(&dummy));
   
   return false;
}

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
   
   if(automapactive)
      AM_Start();             // Reset automap dimensions
   
   ST_Start();               // Reset palette
   
   if(gamestate == GS_INTERMISSION)
   {
      IN_DrawBackground();
      V_CopyRect(0, 0, 1, SCREENWIDTH, SCREENHEIGHT, 0, 0, 0);
   }

   Wipe_ScreenReset(); // haleyjd: reset wipe engine
   
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
   
   v_mode = i;
   
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

void I_Video_AddCommands(void)
{
   C_AddCommand(i_usemouse);
   C_AddCommand(i_usejoystick);
   
   C_AddCommand(v_diskicon);
   C_AddCommand(v_retrace);
   
   C_AddCommand(joySens_x);
   C_AddCommand(joySens_y);

   C_AddCommand(i_grabmouse);
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
