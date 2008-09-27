// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: txt_main.c,v 1.1 2005/10/02 03:16:03 fraggle Exp $
//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
// $Log: txt_main.c,v $
// Revision 1.1  2005/10/02 03:16:03  fraggle
// Text mode emulation code
//
//
//-----------------------------------------------------------------------------
//
// Text mode emulation in SDL
//
//-----------------------------------------------------------------------------

#include <SDL.h>
#include <stdlib.h>
#include <string.h>

#include "txt_main.h"
#include "txt_font.h"

#define SCREEN_W 80
#define SCREEN_H 25
#define CHAR_W 8
#define CHAR_H 16

// Time between character blinks in ms

#define BLINK_PERIOD 250

static SDL_Surface *screen;
static unsigned char *screendata;

// haleyjd 10/09/05: inline is not standard C
#if defined(__GNUC__)
  #define txt_inline __inline__
#elif defined(_MSC_VER)
  #define txt_inline __inline
#else
  #define txt_inline
#endif

static SDL_Color ega_colors[] = 
{
    {0x00, 0x00, 0x00},          // 0: Black
    {0x00, 0x00, 0xa8},          // 1: Blue
    {0x00, 0xa8, 0x00},          // 2: Green
    {0x00, 0xa8, 0xa8},          // 3: Cyan
    {0xa8, 0x00, 0x00},          // 4: Red
    {0xa8, 0x00, 0xa8},          // 5: Magenta
    {0xa8, 0x54, 0x00},          // 6: Brown
    {0xa8, 0xa8, 0xa8},          // 7: Grey
    {0x54, 0x54, 0x54},          // 8: Dark grey
    {0x54, 0x54, 0xfe},          // 9: Bright blue
    {0x54, 0xfe, 0x54},          // 10: Bright green
    {0x54, 0xfe, 0xfe},          // 11: Bright cyan
    {0xfe, 0x54, 0x54},          // 12: Bright red
    {0xfe, 0x54, 0xfe},          // 13: Bright magenta
    {0xfe, 0xfe, 0x54},          // 14: Yellow
    {0xfe, 0xfe, 0xfe},          // 15: Bright white
};

//
// Initialise text mode screen
//
// Returns 1 if successful, 0 if an error occurred
//

int TXT_Init(void)
{
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    
    screen = SDL_SetVideoMode(SCREEN_W * CHAR_W, SCREEN_H * CHAR_H, 8, 0);

    if (screen == NULL)
        return 0;

    SDL_SetColors(screen, ega_colors, 0, 16);

    screendata = malloc(SCREEN_W * SCREEN_H * 2);
    memset(screendata, 0, SCREEN_W * SCREEN_H * 2);

    return 1;
}

void TXT_Shutdown(void)
{
    free(screendata);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

unsigned char *TXT_GetScreenData(void)
{
    return screendata;
}

static txt_inline void UpdateCharacter(int x, int y)
{
    unsigned char character;
    unsigned char *p;
    unsigned char *s, *s1;
    int bg, fg;
    int x1, y1;

    p = &screendata[(y * SCREEN_W + x) * 2];
    character = p[0];

    fg = p[1] & 0xf;
    bg = (p[1] >> 4) & 0xf;

    if (bg & 0x8)
    {
        // blinking

        bg &= ~0x8;

        if (((SDL_GetTicks() / BLINK_PERIOD) % 2) == 0)
        {
            fg = bg;
        }
    }

    p = &int10_font_16[character * CHAR_H];

    // haleyjd: void * math is not standard C
    s = (unsigned char *)screen->pixels + (y * CHAR_H * screen->pitch) + (x * CHAR_W);

    for (y1=0; y1<CHAR_H; ++y1)
    {
        s1 = s;

        for (x1=0; x1<CHAR_W; ++x1)
        {
            if (*p & (1 << (7-x1)))
            {
                *s1++ = fg;
            }
            else
            {
                *s1++ = bg;
            }
        }

        ++p;
        s += screen->pitch;
    }
}

void TXT_UpdateScreenArea(int x, int y, int w, int h)
{
    int x1, y1;

    for (y1=y; y1<y+h; ++y1)
    {
        for (x1=x; x1<x+w; ++x1)
        {
            UpdateCharacter(x1, y1);
        }
    }

    SDL_UpdateRect(screen, x * CHAR_W, y * CHAR_H, w * CHAR_W, h * CHAR_H);
}

void TXT_UpdateScreen(void)
{
    TXT_UpdateScreenArea(0, 0, SCREEN_W, SCREEN_H);
}

void TXT_Sleep(int timeout)
{
    unsigned int start_time;

    // haleyjd: just assume there are blinking characters.
    int time_to_next_blink;
    
    time_to_next_blink = BLINK_PERIOD - (SDL_GetTicks() % BLINK_PERIOD);
    
    // There are blinking characters on the screen, so we 
    // must time out after a while
    
    if (timeout == 0 || timeout > time_to_next_blink)
    {
        // Add one so it is always positive
       
        timeout = time_to_next_blink + 1;
    }

    if (timeout == 0)
    {
        // We can just wait forever until an event occurs

        SDL_WaitEvent(NULL);
    }
    else
    {
        // Sit in a busy loop until the timeout expires or we have to
        // redraw the blinking screen

        start_time = SDL_GetTicks();

        while (SDL_GetTicks() < start_time + timeout)
        {
            if (SDL_PollEvent(NULL) != 0)
            {
                // Received an event, so stop waiting

                break;
            }

            // Don't hog the CPU

            SDL_Delay(1);
        }
    }
}

// EOF

