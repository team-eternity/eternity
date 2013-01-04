// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2009 James Haley
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
//      Startup IWAD picker.
//
//-----------------------------------------------------------------------------

#include "SDL.h"

#include "../z_zone.h"
#include "../i_system.h"
#include "../hal/i_picker.h"

#include "../doomstat.h"
#include "../doomtype.h"
#include "../m_misc.h"
#include "../v_png.h"
#include "../w_wad.h"

static SDL_Surface  *pickscreen;    // SDL screen surface
static WadDirectory  pickwad;       // private directory for startup.wad
static int           currentiwad;   // currently selected IWAD
static bool         *haveIWADArray; // valid IWADs, passed here from d_main.c

// name of title screen lumps in startup.wad
static const char *iwadPicNames[NUMPICKIWADS] =
{
   "DOOMSW",
   "DOOMREG",
   "UDOOM",
   "DOOM2",
   "BFGDOOM2",
   "TNT",
   "PLUTONIA",
   "HACX",
   "HTICSW",
   "HERETIC",
   "HTICSOSR",
   "FREEDOOM",
   "ULTFD",
   "FREEDM",
};

// IWAD game names
static const char *titles[NUMPICKIWADS] =
{
   "DOOM Shareware Version",
   "DOOM Registered Version",
   "The Ultimate DOOM",
   "DOOM II: Hell on Earth",
   "DOOM II: Hell on Earth - BFG Edition",
   "Final DOOM: TNT - Evilution",
   "Final DOOM: The Plutonia Experiment",
   "HACX - Twitch 'n Kill",
   "Heretic Shareware Version",
   "Heretic Registered Version",
   "Heretic: Shadow of the Serpent Riders",
   "Freedoom",
   "Ultimate Freedoom",
   "FreeDM",
};

static byte *bgframe;                // background graphics
static byte *iwadpics[NUMPICKIWADS]; // iwad title pics
static byte *pals[NUMPICKIWADS];     // palettes

//=============================================================================
//
// Data
//

//
// I_Pick_LoadGfx
//
// Caches the background graphics into the private directory for startup.wad
//
static void I_Pick_LoadGfx(void)
{
   int lumpnum;

   if((lumpnum = pickwad.checkNumForName("FRAME")) != -1)
   {
      VPNGImage png;
      void *lump = pickwad.cacheLumpNum(lumpnum, PU_STATIC);

      if(png.readImage(lump))
      {
         if(png.getWidth() == 540 && png.getHeight() == 380)
            bgframe = png.getAs24Bit();
      }

      efree(lump);
   }
}

//
// I_Pick_LoadIWAD
//
// Caches a particular IWAD titlescreen pic from startup.wad.
//
static void I_Pick_LoadIWAD(int num)
{
   int lumpnum;
   const char *lumpname;

   lumpname = iwadPicNames[num];

   if((lumpnum = pickwad.checkNumForName(lumpname)) != -1)
   {
      VPNGImage png;
      void *lump = pickwad.cacheLumpNum(lumpnum, PU_STATIC);

      if(png.readImage(lump))
      {
         byte *pngPalette = png.expandPalette();

         if(png.getWidth() == 320 && png.getHeight() == 240 && pngPalette)
         {
            iwadpics[num] = png.getAs8Bit(NULL);
            pals[num]     = pngPalette;
         }
      }

      efree(lump);
   }
}

//
// I_Pick_OpenWad
//
// Opens base/startup.wad into the private pickwad directory. Nothing in
// startup.wad carries over into the main game; it is completely closed and
// freed below.
//
static bool I_Pick_OpenWad(void)
{
   char *filename;
   int size;

   size = M_StringAlloca(&filename, 2, 1, basepath, "/startup.wad");
   psnprintf(filename, size, "%s/startup.wad", basepath);

   return pickwad.addNewFile(filename);
}

//
// I_Pick_FreeWad
//
// Frees all resources loaded from startup.wad, closes the physical WAD
// file, and destroys the pickwad private wad directory. All of this is
// failsafe in case initialization of the picker subsystem fails partway
// through.
//
static void I_Pick_FreeWad(void)
{
   // close the wad file if it is open
   pickwad.close();
}

static void I_Pick_FreeImages(void)
{
   if(bgframe)
   {
      efree(bgframe);
      bgframe = NULL;
   }

   for(int i = 0; i < NUMPICKIWADS; i++)
   {
      if(iwadpics[i])
         efree(iwadpics[i]);
      iwadpics[i] = NULL;

      if(pals[i])
         efree(pals[i]);
      pals[i] = NULL;
   }
}

//=============================================================================
//
// Drawing
//

//
// I_Pick_ClearScreen
//
// Blanks the picker screen.
//
static void I_Pick_ClearScreen(void)
{
   static bool firsttime = true;
   Uint32 color;
   SDL_Rect dstrect;

   if(firsttime)
   {
      firsttime = false;
      color = SDL_MapRGB(pickscreen->format, 0, 0, 0);
   }

   dstrect.x = dstrect.y = 0;
   dstrect.w = 540;
   dstrect.h = 380;

   SDL_FillRect(pickscreen, &dstrect, color);
}

//
// I_Pick_DrawBG
//
// Blits the 24-bit linear RGB background picture to the picker screen.
// Yes, the graphic is pretty large.
//
static void I_Pick_DrawBG(void)
{
   bool locked = false;
   byte   *src;
   Uint32 *dest;
   int x, y;

   if(!bgframe)
      return;

   src = bgframe;

   if(SDL_MUSTLOCK(pickscreen))
   {
      if(SDL_LockSurface(pickscreen) < 0)
         return;
      locked = true;
   }

   for(y = 0; y < 380; ++y)
   {
      dest = (Uint32 *)((byte *)pickscreen->pixels + y * pickscreen->pitch);

      for(x = 0; x < 540; ++x)
      {
         Uint32 color;
         byte r, g, b;
         r = *src++;
         g = *src++;
         b = *src++;

         color = ((Uint32)r << pickscreen->format->Rshift) |
                 ((Uint32)g << pickscreen->format->Gshift) |
                 ((Uint32)b << pickscreen->format->Bshift);

         *dest++ = color;
      }
   }

   if(locked)
      SDL_UnlockSurface(pickscreen);
}

//
// I_Pick_DrawIWADPic
//
// Draws the 8-bit raw linear titlepic for the currently selected IWAD in the
// upper right corner. The titlescreens are 320x240 so that they appear in the
// proper aspect ratio.
//
static void I_Pick_DrawIWADPic(int pic)
{
   bool locked = false;
   byte   *src;
   byte   *pal;
   Uint32 *dest;
   int x, y;

   if(!iwadpics[pic] || !pals[pic])
      return;

   src = iwadpics[pic];
   pal = pals[pic];

   if(SDL_MUSTLOCK(pickscreen))
   {
      if(SDL_LockSurface(pickscreen) < 0)
         return;
      locked = true;
   }

   for(y = 19; y < 240 + 19; ++y)
   {
      dest = (Uint32 *)((byte *)pickscreen->pixels + y * pickscreen->pitch +
                         202 * pickscreen->format->BytesPerPixel);

      for(x = 0; x < 320; ++x)
      {
         Uint32 color;
         byte r, g, b;

         color = *src++;

         r = pal[color * 3 + 0];
         g = pal[color * 3 + 1];
         b = pal[color * 3 + 2];

         color = ((Uint32)r << pickscreen->format->Rshift) |
                 ((Uint32)g << pickscreen->format->Gshift) |
                 ((Uint32)b << pickscreen->format->Bshift);

         *dest++ = color;
      }
   }

   if(locked)
      SDL_UnlockSurface(pickscreen);
}

//
// I_Pick_Drawer
//
// Main drawing routine.
//
static void I_Pick_Drawer(void)
{
   I_Pick_DrawBG();

   if(!iwadpics[currentiwad])
      I_Pick_LoadIWAD(currentiwad);

   I_Pick_DrawIWADPic(currentiwad);

   SDL_UpdateRect(pickscreen, 0, 0, 0, 0);
}

//=============================================================================
//
// Event Handling
//

//
// I_Pick_DoLeft
//
// Called for left arrow keydown and mouse click events. Moves the IWAD
// selection back to the previous valid IWAD.
//
static void I_Pick_DoLeft(void)
{
   int startwad = currentiwad;

   do
   {
      --currentiwad;
      if(currentiwad < 0)
         currentiwad = NUMPICKIWADS - 1;
   }
   while(!haveIWADArray[currentiwad] && currentiwad != startwad);

   SDL_WM_SetCaption(titles[currentiwad], titles[currentiwad]);
}

//
// I_Pick_DoRight
//
// Called for right arrow keydown and mouse click events. Moves the IWAD
// selection forward to the next valid IWAD.
//
static void I_Pick_DoRight(void)
{
   int startwad = currentiwad;

   do
   {
      ++currentiwad;
      if(currentiwad >= NUMPICKIWADS)
         currentiwad = 0;
   }
   while(!haveIWADArray[currentiwad] && currentiwad != startwad);

   SDL_WM_SetCaption(titles[currentiwad], titles[currentiwad]);
}

//
// I_Pick_DoAbort
//
// Called for escape keydown and mouse clicks. Exits the program.
//
static void I_Pick_DoAbort(void)
{
   I_ExitWithMessage("Eternity Engine aborted.\n");
}

//
// I_Pick_MouseInRect
//
// Tests a mouse button down event for a location within the specified
// rectangle.
//
static bool I_Pick_MouseInRect(Uint16 x, Uint16 y, SDL_Rect *rect)
{
   return (x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h);
}

//
// I_Pick_MouseEvent
//
// Tests mouse button down events against all valid button rectangles.
//
static void I_Pick_MouseEvent(SDL_Event *ev, bool *doloop)
{
   SDL_Rect r;
   Uint16 x, y;

   x = ev->button.x;
   y = ev->button.y;

   r.y = 293;
   r.h = 340 - 293 + 1;

   // check for left arrow
   r.x = 24;
   r.w = 76 - 24 + 1;

   if(I_Pick_MouseInRect(x, y, &r))
   {
      I_Pick_DoLeft();
      return;
   }

   // check for right arrow
   r.x = 86;
   r.w = 139 - 86 + 1;

   if(I_Pick_MouseInRect(x, y, &r))
   {
      I_Pick_DoRight();
      return;
   }

   // check for escape
   r.x = 201;
   r.w = 267 - 201 + 1;

   if(I_Pick_MouseInRect(x, y, &r))
   {
      I_Pick_DoAbort();
      return;
   }

   // check for backspace
   r.x = 284;
   r.w = 412 - 284 + 1;

   if(I_Pick_MouseInRect(x, y, &r))
   {
      *doloop = false;
      currentiwad = -1;
      return;
   }

   // check for enter
   r.x = 429;
   r.w = 515 - 429 + 1;

   if(I_Pick_MouseInRect(x, y, &r))
      *doloop = false;
}

//=============================================================================
//
// Main Loop
//

//
// I_Pick_MainLoop
//
// Drives the IWAD picker subprogram, drawing the graphics and getting input.
//
static void I_Pick_MainLoop(void)
{
   bool doloop = true;
   SDL_Event ev;

   while(doloop)
   {
      // draw
      I_Pick_Drawer();

      // get input
      while(SDL_PollEvent(&ev))
      {
         switch(ev.type)
         {
         case SDL_MOUSEBUTTONDOWN:
            I_Pick_MouseEvent(&ev, &doloop);
            break;
         case SDL_KEYDOWN:
            switch(ev.key.keysym.sym)
            {
            case SDLK_ESCAPE:
               I_Pick_DoAbort();
               break;
            case SDLK_BACKSPACE:
               doloop = false;
               currentiwad = -1; // erase selection
               break;
            case SDLK_RETURN:
               doloop = false;
               break;
            case SDLK_RIGHT:
               I_Pick_DoRight();
               break;
            case SDLK_LEFT:
               I_Pick_DoLeft();
               break;
            default:
               break;
            }
            break;
         default:
            break;
         }
      }

      // sleep
      SDL_Delay(1);
   }
}

//=============================================================================
//
// Shutdown
//

static bool pickvideoinit = false;

//
// I_Pick_Shutdown
//
// Called when the picker subprogram is finished. Frees resources.
//
static void I_Pick_Shutdown(void)
{
   I_Pick_FreeWad();
   I_Pick_FreeImages();

//   haleyjd: I hate SDL.
//   if(pickvideoinit)
//      SDL_QuitSubSystem(SDL_INIT_VIDEO);

   pickvideoinit = false;
}

//=============================================================================
//
// Global routines
//

//
// I_Pick_DoPicker
//
// Called from d_main.c if the user has specified at least one valid IWAD path
// in the system.cfg file under the Eternity base directory. The valid IWAD
// paths are marked in the haveIWADs array as "true" values.
//
int I_Pick_DoPicker(bool haveIWADs[], int startchoice)
{
   haveIWADArray = haveIWADs;

   pickvideoinit = true;

   // open the screen
   if(!(pickscreen = SDL_SetVideoMode(540, 380, 32, SDL_SWSURFACE)))
      return -1;

   // open startup.wad (private to this module)
   if(!I_Pick_OpenWad())
   {
      I_Pick_Shutdown();
      return -1;
   }

   // load basic graphics
   I_Pick_LoadGfx();

   // clear the screen
   I_Pick_ClearScreen();

   // see if prior choice is valid
   if(startchoice != -1 && haveIWADs[startchoice])
      currentiwad = startchoice;
   else
   {
      // find first valid iwad
      currentiwad = -1;
      do
      {
         ++currentiwad;
      }
      while(!haveIWADs[currentiwad] && currentiwad < NUMPICKIWADS);
   }

   // this really shouldn't happen, but I check for safety
   if(currentiwad < 0 || currentiwad >= NUMPICKIWADS)
   {
      I_Pick_Shutdown();
      return -1;
   }

   // set window title to currently selected game
   SDL_WM_SetCaption(titles[currentiwad], titles[currentiwad]);

   // run the program
   I_Pick_MainLoop();

   // user is finished, free stuff and get everything back to normal
   I_Pick_Shutdown();

   // the currently selected file is returned to d_main.c
   return currentiwad;
}

// EOF