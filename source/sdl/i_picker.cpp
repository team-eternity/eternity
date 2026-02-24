//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Max Waine, et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Startup IWAD picker.
// Authors: James Haley, Max Waine
//

#ifdef __APPLE__
#include "SDL2/SDL.h"
#else
#include "SDL.h"
#endif

#include "../z_zone.h"
#include "../i_system.h"
#include "../hal/i_picker.h"

#include "../doomstat.h"
#include "../doomtype.h"
#include "../m_utils.h"
#include "../v_png.h"
#include "../w_wad.h"

static SDL_Window   *pickwindow;    // SDL window
static SDL_Renderer *pickrenderer;  // SDL renderer
static WadDirectory  pickwad;       // private directory for startup.wad
static int           currentiwad;   // currently selected IWAD
static bool         *haveIWADArray; // valid IWADs, passed here from d_main.c

static SDL_GameController **controllers; // All controllers

extern int displaynum; // What number display to place windows on

// name of title screen lumps in startup.wad
static const char *iwadPicNames[NUMPICKIWADS] = {
    "DOOMSW",   //
    "DOOMREG",  //
    "UDOOM",    //
    "DOOM2",    //
    "BFGDOOM2", //
    "TNT",      //
    "PLUTONIA", //
    "HACX",     //
    "HTICSW",   //
    "HERETIC",  //
    "HTICSOSR", //
    "FREEDOOM", //
    "ULTFD",    //
    "FREEDM",   //
    "REKKR",    //
};

// IWAD game names
static const char *titles[NUMPICKIWADS] = {
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
    "Freedoom Phase 2",
    "Freedoom Phase 1",
    "FreeDM",
    "Rekkr",
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
        void     *lump = pickwad.cacheLumpNum(lumpnum, PU_STATIC);

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
    int         lumpnum;
    const char *lumpname;

    lumpname = iwadPicNames[num];

    if((lumpnum = pickwad.checkNumForName(lumpname)) != -1)
    {
        VPNGImage png;
        void     *lump = pickwad.cacheLumpNum(lumpnum, PU_STATIC);

        if(png.readImage(lump))
        {
            byte *pngPalette = png.expandPalette();

            if(png.getWidth() == 320 && png.getHeight() == 240 && pngPalette)
            {
                iwadpics[num] = png.getAs8Bit(nullptr, nullptr);
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
    int   size;

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
        bgframe = nullptr;
    }

    for(int i = 0; i < NUMPICKIWADS; i++)
    {
        if(iwadpics[i])
            efree(iwadpics[i]);
        iwadpics[i] = nullptr;

        if(pals[i])
            efree(pals[i]);
        pals[i] = nullptr;
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
static void I_Pick_ClearScreen()
{
    SDL_SetRenderDrawColor(pickrenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(pickrenderer);
}

//
// I_Pick_DrawBG
//
// Blits the 24-bit linear RGB background picture to the picker screen.
// Yes, the graphic is pretty large.
//
static void I_Pick_DrawBG()
{
    byte *src;
    int   x, y;

    if(!bgframe)
        return;

    src = bgframe;

    for(y = 0; y < 380; ++y)
    {
        for(x = 0; x < 540; ++x)
        {
            byte r, g, b;
            r = *src++;
            g = *src++;
            b = *src++;

            SDL_SetRenderDrawColor(pickrenderer, r, g, b, SDL_ALPHA_OPAQUE);
            SDL_RenderDrawPoint(pickrenderer, x, y);
        }
    }
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
    byte *src;
    byte *pal;
    int   x, y;

    if(!iwadpics[pic] || !pals[pic])
        return;

    src = iwadpics[pic];
    pal = pals[pic];

    for(y = 19; y < 240 + 19; ++y)
    {
        for(x = 202; x < 522; ++x)
        {
            Uint32 color;
            byte   r, g, b;

            color = *src++;

            r = pal[color * 3 + 0];
            g = pal[color * 3 + 1];
            b = pal[color * 3 + 2];

            SDL_SetRenderDrawColor(pickrenderer, r, g, b, SDL_ALPHA_OPAQUE);
            SDL_RenderDrawPoint(pickrenderer, x, y);
        }
    }
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

    SDL_RenderPresent(pickrenderer);
}

//=============================================================================
//
// Event Handling
//

//
// Temporarily open all controllers
//
static void I_Pick_InitControllers()
{
    controllers = ecalloc(SDL_GameController **, SDL_NumJoysticks(), sizeof(SDL_GameController *));
    ;
    for(int i = 0; i < SDL_NumJoysticks(); i++)
    {
        if(SDL_IsGameController(i))
            controllers[i] = SDL_GameControllerOpen(i);
    }
}

//
// Close the previously-opened controllers
//
static void I_Pick_CloseControllers()
{
    for(int i = 0; i < SDL_NumJoysticks(); i++)
    {
        if(controllers[i])
            SDL_GameControllerClose(controllers[i]);
    }
    efree(controllers);
}

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

    SDL_SetWindowTitle(pickwindow, titles[currentiwad]);
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

    SDL_SetWindowTitle(pickwindow, titles[currentiwad]);
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
    return (x >= rect->x && x <= rect->x + rect->w && y >= rect->y && y <= rect->y + rect->h);
}

//
// I_Pick_MouseEvent
//
// Tests mouse button down events against all valid button rectangles.
//
static void I_Pick_MouseEvent(SDL_Event *ev, bool *doloop)
{
    SDL_Rect r;
    Uint16   x, y;

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
        *doloop     = false;
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
    bool      doloop = true;
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
            case SDL_QUIT: //
                I_Pick_DoAbort();
                break;
            case SDL_MOUSEBUTTONDOWN: //
                I_Pick_MouseEvent(&ev, &doloop);
                break;
            case SDL_KEYDOWN: //
                switch(ev.key.keysym.scancode)
                {
                case SDL_SCANCODE_ESCAPE: //
                    I_Pick_DoAbort();
                    break;
                case SDL_SCANCODE_BACKSPACE: //
                    doloop      = false;
                    currentiwad = -1; // erase selection
                    break;
                case SDL_SCANCODE_KP_ENTER:
                case SDL_SCANCODE_RETURN: //
                    doloop = false;
                    break;
                case SDL_SCANCODE_KP_6:
                case SDL_SCANCODE_RIGHT: //
                    I_Pick_DoRight();
                    break;
                case SDL_SCANCODE_KP_4:
                case SDL_SCANCODE_LEFT: //
                    I_Pick_DoLeft();
                    break;
                default: //
                    break;
                }
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                switch(ev.cbutton.button)
                {
                case SDL_CONTROLLER_BUTTON_Y: //
                    I_Pick_DoAbort();
                    break;
                case SDL_CONTROLLER_BUTTON_A:
                case SDL_CONTROLLER_BUTTON_B:
                case SDL_CONTROLLER_BUTTON_X:
                case SDL_CONTROLLER_BUTTON_START: //
                    doloop = false;
                    break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: //
                    I_Pick_DoLeft();
                    break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: //
                    I_Pick_DoRight();
                    break;
                default: //
                    break;
                }
                break;
            default: //
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

    // the window needs to be destroyed
    if(pickvideoinit)
        SDL_DestroyWindow(pickwindow);

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
    int v_displaynum = 0;

    haveIWADArray = haveIWADs;

    pickvideoinit = true;

    if(displaynum < SDL_GetNumVideoDisplays())
        v_displaynum = displaynum;
    else
        displaynum = 0;

    // create the window
    if(!(pickwindow = SDL_CreateWindow(nullptr, SDL_WINDOWPOS_CENTERED_DISPLAY(v_displaynum),
                                       SDL_WINDOWPOS_CENTERED_DISPLAY(v_displaynum), 540, 380, 0)))
        return -1;

    // create the renderer
    if(!(pickrenderer = SDL_CreateRenderer(pickwindow, -1, SDL_RENDERER_SOFTWARE)))
        return -1;

    // bring the window to the front
    SDL_RaiseWindow(pickwindow);

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
        while(currentiwad < NUMPICKIWADS && !haveIWADs[currentiwad]);
    }

    // this really shouldn't happen, but I check for safety
    if(currentiwad < 0 || currentiwad >= NUMPICKIWADS)
    {
        I_Pick_Shutdown();
        return -1;
    }

    // set window title to currently selected game
    SDL_SetWindowTitle(pickwindow, titles[currentiwad]);

    // initialise controllers for input handling
    I_Pick_InitControllers();

    // run the program
    I_Pick_MainLoop();

    // user is finished, free stuff and get everything back to normal
    I_Pick_Shutdown();

    // close the controllers since we're done with them for now
    I_Pick_CloseControllers();

    // the currently selected file is returned to d_main.c
    return currentiwad;
}

// EOF

