// Emacs style mode select   -*- C++ -*-
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

#define I_SDL_TYPES_ACTIVE

#include "SDL.h"

#include "../z_zone.h"
#include "../doomtype.h"
#include "../m_misc.h"
#include "../w_wad.h"

static SDL_Surface *pickscreen; // SDL screen surface
static waddir_t pickwad;        // private directory for startup.wad
static int currentiwad;         // currently selected IWAD
static boolean *haveIWADArray;  // valid IWADs, passed here from d_main.c

// picker iwad enumeration
enum
{
   IWAD_DOOMSW,
   IWAD_DOOMREG,
   IWAD_DOOMU,
   IWAD_DOOM2,
   IWAD_TNT,
   IWAD_PLUT,
   IWAD_HTICSW,
   IWAD_HTICREG,
   IWAD_HTICSOSR,
   NUMPICKIWADS
};

// name of title screen lumps in startup.wad
static const char *iwadPicNames[NUMPICKIWADS] =
{
   "DOOMSW",
   "DOOMREG",
   "UDOOM",
   "DOOM2",
   "TNT",
   "PLUTONIA",
   "HERETIC",
   "HERETIC",
   "HTICSOSR",
};

// palette enumeration
enum
{
   PAL_DOOM,
   PAL_HTIC,
   NUMPICKPALS,
};

// name of palette lumps in startup.wad
static const char *palNames[NUMPICKPALS] =
{
   "DOOMPAL",
   "HTICPAL",
};

// palette-for-pic lookup table
static int iwadPicPals[NUMPICKIWADS] =
{
   PAL_DOOM, 
   PAL_DOOM, 
   PAL_DOOM,
   PAL_DOOM,
   PAL_DOOM,
   PAL_DOOM,
   PAL_HTIC,
   PAL_HTIC,
   PAL_HTIC,
};

// IWAD game names
static const char *titles[NUMPICKIWADS] =
{
   "DOOM Shareware Version",
   "DOOM Registered Version",
   "The Ultimate DOOM",
   "DOOM II: Hell on Earth",
   "Final DOOM: TNT - Evilution",
   "Final DOOM: The Plutonia Experiment",
   "Heretic Shareware Version",
   "Heretic Registered Version",
   "Heretic: Shadow of the Serpent Riders",
};

static byte *bgframe;                // background graphics
static byte *iwadpics[NUMPICKIWADS]; // iwad title pics
static byte *pals[NUMPICKPALS];      // palettes

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

   if((lumpnum = W_CheckNumForNameInDir(&pickwad, "FRAME", ns_global)) != -1)
      bgframe = W_CacheLumpNumInDir(&pickwad, lumpnum, PU_STATIC);
}

//
// I_Pick_LoadIWAD
//
// Caches a particular IWAD titlescreen pic from startup.wad. If the 
// corresponding palette hasn't been loaded, that is also cached.
//
static void I_Pick_LoadIWAD(int num)
{
   int lumpnum;
   const char *lumpname;
   int palnum;
   const char *palname;

   lumpname = iwadPicNames[num];

   palnum   = iwadPicPals[num];
   palname  = palNames[palnum];

   if((lumpnum = W_CheckNumForNameInDir(&pickwad, lumpname, ns_global)) != -1)
      iwadpics[num] = W_CacheLumpNumInDir(&pickwad, lumpnum, PU_STATIC);

   // load palette if needed also
   if(!pals[palnum])
   {
      if((lumpnum = W_CheckNumForNameInDir(&pickwad, palname, ns_global)) != -1)
         pals[palnum] = W_CacheLumpNumInDir(&pickwad, lumpnum, PU_STATIC);
   }
}

//
// I_Pick_OpenWad
//
// Opens base/startup.wad into the private pickwad directory. Nothing in
// startup.wad carries over into the main game; it is completely closed and
// freed below.
//
static boolean I_Pick_OpenWad(void)
{
   char *filename;
   int size;

   size = M_StringAlloca(&filename, 2, 1, basepath, "/startup.wad");
   psnprintf(filename, size, "%s/startup.wad", basepath);

   if(W_AddNewFile(&pickwad, filename))
      return false;

   return true;
}

//
// I_Pick_FreeWad
//
// Frees all resources loaded from startup.wad, closes the physical WAD
// file, and destroys the pickwad private wad directory. All of this is
// failsafe in case initialization of the picker subsystem fails partway
// through.
//
//
static void I_Pick_FreeWad(void)
{
   // close the wad file if it is open
   if(pickwad.lumpinfo)
   {
      // free all resources loaded from the wad
      W_FreeDirectoryLumps(&pickwad);

      if(pickwad.lumpinfo[0]->file) // kind of redundant, but who knows
         fclose(pickwad.lumpinfo[0]->file);

      // free the private wad directory
      Z_Free(pickwad.lumpinfo);
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
   static boolean firsttime = true;
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
   boolean locked = false;
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
   boolean locked = false;
   byte   *src;
   byte   *pal;
   Uint32 *dest;
   int x, y;

   if(!iwadpics[pic] || !pals[iwadPicPals[pic]])
      return;
   
   src = iwadpics[pic];
   pal = pals[iwadPicPals[pic]];

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

         r = pal[color * 3];
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
   
   SDL_WM_SetCaption(titles[currentiwad], NULL);
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
   
   SDL_WM_SetCaption(titles[currentiwad], NULL);
}

//
// I_Pick_DoAbort
//
// Called for escape keydown and mouse clicks. Exits the program.
//
static void I_Pick_DoAbort(void)
{
   I_Error("Eternity Engine aborted.\n");
}

//
// I_Pick_MouseInRect
//
// Tests a mouse button down event for a location within the specified
// rectangle.
//
static boolean I_Pick_MouseInRect(Uint16 x, Uint16 y, SDL_Rect *rect)
{
   return (x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h);
}

//
// I_Pick_MouseEvent
//
// Tests mouse button down events against all valid button rectangles.
//
static void I_Pick_MouseEvent(SDL_Event *ev, boolean *doloop)
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
   boolean doloop = true;
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

static boolean pickvideoinit = false;

//
// I_Pick_Shutdown
//
// Called when the picker subprogram is finished. Frees resources.
//
static void I_Pick_Shutdown(void)
{
   I_Pick_FreeWad();

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
int I_Pick_DoPicker(boolean haveIWADs[])
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

   // find first valid iwad
   currentiwad = -1;
   do
   {
      ++currentiwad;
   }
   while(!haveIWADs[currentiwad] && currentiwad < NUMPICKIWADS);

   // this really shouldn't happen, but I check for safety
   if(currentiwad == NUMPICKIWADS)
   {
      I_Pick_Shutdown();
      return -1;
   }

   // set window title to currently selected game
   SDL_WM_SetCaption(titles[currentiwad], NULL);

   // run the program
   I_Pick_MainLoop();

   // user is finished, free stuff and get everything back to normal
   I_Pick_Shutdown();

   // the currently selected file is returned to d_main.c
   return currentiwad;
}

// EOF

