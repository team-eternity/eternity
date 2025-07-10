//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
// Purpose: Functions to manipulate linear blocks of graphics data.
// Authors: James Haley, Stephen McGranahan, Max Waine
//

#include "z_zone.h"

#include "i_system.h"
#include "m_compare.h"
#include "v_video.h"

//==============================================================================
//
// V_DrawBlock Implementors
//
// * V_blockDrawer   -- unscaled
// * V_blockDrawerS  -- general scaling
//

static void V_blockDrawer(int x, int y, VBuffer *buffer, int width, int height, const byte *source)
{
    const byte *src;
    byte       *dest;
    int         cx1, cy1, cx2, cy2, cw, ch;
    int         dx, dy;

    // clip to screen

    // entirely off-screen?
    if(x + width < 0 || y + height < 0 || x >= buffer->width || y >= buffer->height)
        return;

    cx1 = x >= 0 ? x : 0;
    cy1 = y >= 0 ? y : 0;
    cx2 = x + width - 1;
    cy2 = y + height - 1;

    if(cx2 >= buffer->width)
        cx2 = buffer->width - 1;

    if(cy2 >= buffer->height)
        cy2 = buffer->height - 1;

    // change in origin due to clipping
    dx = cx1 - x;
    dy = cy1 - y;

    // clipped rect width/height
    cw = cx2 - cx1 + 1;
    ch = cy2 - cy1 + 1;

    // zero-size rect?
    if(cw <= 0 || ch <= 0)
        return;

    src  = source + dy * width + dx;
    dest = VBADDRESS(buffer, cx1, cy1);

    while(cw--)
    {
        byte *col = dest;
        for(int i = 0; i < ch; i++)
        {
            *col  = src[i * width];
            col  += 1;
        }
        src  += 1;
        dest += buffer->pitch;
    }
}

static void V_blockDrawerS(int x, int y, VBuffer *buffer, int width, int height, const byte *source)
{
    const byte *src;
    byte       *dest, *col;
    fixed_t     xstep, ystep, xfrac, yfrac;
    int         xtex, ytex, w, h, i, realx, realy;
    int         cx1, cy1, cx2, cy2, cw, ch;
    int         dx, dy;

    // clip to screen within scaled coordinate space

    // entirely off-screen?
    if(x + width < 0 || y + height < 0 || x >= buffer->unscaledw || y >= buffer->unscaledh)
        return;

    cx1 = x >= 0 ? x : 0;
    cy1 = y >= 0 ? y : 0;
    cx2 = x + width - 1;
    cy2 = y + height - 1;

    if(cx2 >= buffer->unscaledw)
        cx2 = buffer->unscaledw - 1;

    if(cy2 >= buffer->unscaledh)
        cy2 = buffer->unscaledh - 1;

    // change in origin due to clipping
    dx = cx1 - x;
    dy = cy1 - y;

    // clipped rect width/height
    cw = cx2 - cx1 + 1;
    ch = cy2 - cy1 + 1;

    // zero-size rect?
    if(cw <= 0 || ch <= 0)
        return;

    realx = buffer->x1lookup[cx1];
    realy = buffer->y1lookup[cy1];
    w     = buffer->x2lookup[cx2] - realx + 1;
    h     = buffer->y2lookup[cy2] - realy + 1;
    xstep = buffer->ixscale;
    ystep = buffer->iyscale;
    xfrac = 0;

    src  = source + dy * width + dx;
    dest = VBADDRESS(buffer, realx, realy);

#ifdef RANGECHECK
    // sanity check
    if(realx < 0 || realx + w > buffer->width || realy < 0 || realy + h > buffer->height)
    {
        I_Error("V_blockDrawerS: block exceeds buffer boundaries.\n");
    }
#endif

    while(w--)
    {
        col   = dest;
        i     = h;
        yfrac = 0;
        xtex  = (xfrac >> FRACBITS);

        while(i--)
        {
            ytex   = (yfrac >> FRACBITS) * width;
            *col   = src[ytex + xtex];
            col   += 1;
            yfrac += ystep;
        }

        dest  += buffer->pitch;
        xfrac += xstep;
    }
}

//==============================================================================
//
// Masked block drawers
//
// haleyjd 06/29/08
//

static void V_maskedBlockDrawer(int x, int y, VBuffer *buffer, int width, int height, int srcpitch, const byte *source,
                                byte *cmap)
{
    const byte *src;
    byte       *dest;
    int         cx1, cy1, cx2, cy2, cw, ch;
    int         dx, dy, i;

    // clip to screen

    // entirely off-screen?
    if(x + width < 0 || y + height < 0 || x >= buffer->width || y >= buffer->height)
        return;

    cx1 = x >= 0 ? x : 0;
    cy1 = y >= 0 ? y : 0;
    cx2 = x + width - 1;
    cy2 = y + height - 1;

    if(cx2 >= buffer->width)
        cx2 = buffer->width - 1;

    if(cy2 >= buffer->height)
        cy2 = buffer->height - 1;

    // change in origin due to clipping
    dx = cx1 - x;
    dy = cy1 - y;

    // clipped rect width/height
    cw = cx2 - cx1 + 1;
    ch = cy2 - cy1 + 1;

    // zero-size rect?
    if(cw <= 0 || ch <= 0)
        return;

    src  = source + dy * srcpitch + dx;
    dest = VBADDRESS(buffer, cx1, cy1);

    while(ch--)
    {
        for(i = 0; i < cw; ++i)
        {
            if(*(src + i))
                *(dest + i * buffer->pitch) = cmap[*(src + i)];
        }
        src  += srcpitch;
        dest += 1;
    }
}

static void V_maskedBlockDrawerS(int x, int y, VBuffer *buffer, int width, int height, int srcpitch, const byte *source,
                                 byte *cmap)
{
    const byte *src;
    byte       *dest, *col;
    fixed_t     xstep, ystep, xfrac, yfrac;
    int         xtex, ytex, w, h, i, realx, realy;
    int         cx1, cy1, cx2, cy2, cw, ch;
    int         dx, dy;

    // clip to screen within scaled coordinate space

    // entirely off-screen?
    if(x + width < 0 || y + height < 0 || x >= buffer->unscaledw || y >= buffer->unscaledh)
        return;

    cx1 = x >= 0 ? x : 0;
    cy1 = y >= 0 ? y : 0;
    cx2 = x + width - 1;
    cy2 = y + height - 1;

    if(cx2 >= buffer->unscaledw)
        cx2 = buffer->unscaledw - 1;

    if(cy2 >= buffer->unscaledh)
        cy2 = buffer->unscaledh - 1;

    // change in origin due to clipping
    dx = cx1 - x;
    dy = cy1 - y;

    // clipped rect width/height
    cw = cx2 - cx1 + 1;
    ch = cy2 - cy1 + 1;

    // zero-size rect?
    if(cw <= 0 || ch <= 0)
        return;

    realx = buffer->x1lookup[cx1];
    realy = buffer->y1lookup[cy1];
    w     = buffer->x2lookup[cx2] - realx + 1;
    h     = buffer->y2lookup[cy2] - realy + 1;
    xstep = buffer->ixscale;
    ystep = buffer->iyscale;
    xfrac = 0;
    yfrac = 0;

    src  = source + dy * srcpitch + dx;
    dest = VBADDRESS(buffer, realx, realy);

#ifdef RANGECHECK
    // sanity check
    if(realx < 0 || realx + w > buffer->width || realy < 0 || realy + h > buffer->height)
    {
        I_Error("V_maskedBlockDrawerS: block exceeds buffer boundaries.\n");
    }
#endif

    while(w--)
    {
        col   = dest;
        i     = h;
        yfrac = 0;
        xtex  = (xfrac >> FRACBITS);

        while(i--)
        {
            ytex = (yfrac >> FRACBITS) * srcpitch;
            if(src[ytex + xtex])
                *col = cmap[src[ytex + xtex]];
            col   += 1;
            yfrac += ystep;
        }

        dest  += buffer->pitch;
        xfrac += xstep;
    }
}

//==============================================================================
//
// Color block drawing
//

void V_ColorBlockScaled(VBuffer *dest, byte color, int x, int y, int w, int h)
{
    byte *d;
    int   i, size;
    int   x2, y2;

    x2 = x + w - 1;
    y2 = y + h - 1;

    if(x < 0)
        x = 0;

    if(y < 0)
        y = 0;

    if(x2 >= dest->unscaledw)
        x2 = dest->unscaledw - 1;

    if(y2 >= dest->unscaledh)
        y2 = dest->unscaledh - 1;

    if(x > x2 || y > y2)
        return;

    x  = dest->x1lookup[x];
    x2 = dest->x2lookup[x2];

    y  = dest->y1lookup[y];
    y2 = dest->y2lookup[y2];

    w = x2 - x + 1;
    h = y2 - y + 1;

    d    = VBADDRESS(dest, x, y);
    size = h;

    for(i = 0; i < w; i++)
    {
        memset(d, color, size);
        d += dest->pitch;
    }
}

void V_ColorBlockTLScaled(VBuffer *dest, byte color, int x, int y, int w, int h, int tl)
{
    byte         *d, *col;
    int           i, th;
    int           x2, y2;
    unsigned int *fg2rgb, *bg2rgb, row;

    // haleyjd 05/06/09: optimizations for opaque and invisible
    if(tl == FRACUNIT)
    {
        V_ColorBlockScaled(dest, color, x, y, w, h);
        return;
    }
    else if(tl == 0)
        return;

    unsigned int fglevel = tl & ~0x3ff;
    unsigned int bglevel = FRACUNIT - fglevel;
    fg2rgb               = Col2RGB8[fglevel >> 10];
    bg2rgb               = Col2RGB8[bglevel >> 10];

    x2 = x + w - 1;
    y2 = y + h - 1;

    if(x < 0)
        x = 0;

    if(y < 0)
        y = 0;

    if(x2 >= dest->unscaledw)
        x2 = dest->unscaledw - 1;

    if(y2 >= dest->unscaledh)
        y2 = dest->unscaledh - 1;

    if(x > x2 || y > y2)
        return;

    x  = dest->x1lookup[x];
    x2 = dest->x2lookup[x2];

    y  = dest->y1lookup[y];
    y2 = dest->y2lookup[y2];

    w = x2 - x + 1;
    h = y2 - y + 1;

    d = VBADDRESS(dest, x, y);

    for(i = 0; i < w; i++)
    {
        col = d;
        th  = h;

        while(th--)
        {
            row    = (fg2rgb[color] + bg2rgb[*col]) | 0x1f07c1f;
            *col++ = RGB32k[0][0][row & (row >> 15)];
        }

        d += dest->pitch;
    }
}

//
// V_ColorBlock
//
// Draws a block of solid color.
//
void V_ColorBlock(VBuffer *buffer, byte color, int x, int y, int w, int h)
{
    byte *dest;

#ifdef RANGECHECK
    if(x < 0 || x + w > buffer->width || y < 0 || y + h > buffer->height)
        I_Error("V_ColorBlock: block exceeds buffer boundaries.\n");
#endif

    dest = VBADDRESS(buffer, x, y);

    while(w--)
    {
        memset(dest, color, h);
        dest += buffer->pitch;
    }
}

//
// V_ColorBlockTL
//
// Draws a block of solid color with alpha blending.
//
void V_ColorBlockTL(VBuffer *buffer, byte color, int x, int y, int w, int h, int tl)
{
    byte         *dest, *row;
    int           th;
    unsigned int  col;
    unsigned int *fg2rgb, *bg2rgb;

#ifdef RANGECHECK
    if(x < 0 || x + w > buffer->width || y < 0 || y + h > buffer->height)
        I_Error("V_ColorBlockTL: block exceeds buffer boundaries.\n");
#endif

    // haleyjd 10/15/05: optimizations for opaque and invisible
    if(tl == FRACUNIT)
    {
        V_ColorBlock(buffer, color, x, y, w, h);
        return;
    }
    else if(tl == 0)
        return;

    unsigned int fglevel = tl & ~0x3ff;
    unsigned int bglevel = FRACUNIT - fglevel;
    fg2rgb               = Col2RGB8[fglevel >> 10];
    bg2rgb               = Col2RGB8[bglevel >> 10];

    dest = VBADDRESS(buffer, x, y);

    while(w--)
    {
        row = dest;
        th  = h;

        while(th--)
        {
            col   = (fg2rgb[color] + bg2rgb[*row]) | 0x1f07c1f;
            *row  = RGB32k[0][0][col & (col >> 15)];
            row  += buffer->width;
        }

        dest += 1;
    }
}

//
// V_TileFlat implementors
//

//
// Unscaled
//
// Works for any video mode.
//
static void V_tileBlock64(VBuffer *buffer, const byte *src)
{
    byte *col, *dest = buffer->data;

    for(int x = 0; x < buffer->width; x++)
    {
        col = dest;
        for(int y = 0; y < buffer->height; y++)
        {
            *col  = src[((y & 63) << 6) + (x & 63)];
            col  += 1;
        }
        dest += buffer->pitch;
    }
}

//
// General scaling
//
static void V_tileBlock64S(VBuffer *buffer, const byte *src)
{
    byte   *dest, *col;
    fixed_t xstep, ystep, xfrac = 0, yfrac;
    int     xtex, ytex, w, h;

    w     = buffer->width;
    h     = buffer->height;
    xstep = buffer->ixscale;
    ystep = buffer->iyscale;

    dest = buffer->data;

    while(w--)
    {
        int i = h;
        col   = dest;
        yfrac = 0;
        xtex  = ((xfrac >> FRACBITS) & 63);

        while(i--)
        {
            ytex   = ((yfrac >> FRACBITS) & 63) << 6;
            *col   = src[ytex + xtex];
            col   += 1;
            yfrac += ystep;
        }

        xfrac += xstep;
        dest  += buffer->pitch;
    }
}

//=============================================================================
//
// Buffer fill
//
// Fill a VBuffer with a texture.
//

void V_FillBuffer(VBuffer *buffer, const byte *src, int texw, int texh)
{
    byte   *dest  = buffer->data, *col;
    int     w     = buffer->width;
    int     h     = buffer->height;
    fixed_t xstep = (texw << FRACBITS) / w;
    fixed_t ystep = (texh << FRACBITS) / h;
    fixed_t xfrac, yfrac = 0;
    int     xtex, ytex;

    while(h--)
    {
        int x = w;
        col   = dest;
        xfrac = 0;
        ytex  = eclamp(yfrac >> FRACBITS, 0, texh - 1);

        while(x--)
        {
            xtex   = eclamp(xfrac >> FRACBITS, 0, texw - 1);
            *col   = src[ytex * texw + xtex];
            col   += buffer->pitch;
            xfrac += xstep;
        }

        yfrac += ystep;
        dest  += 1;
    }
}

//=============================================================================
//
// Function pointer assignment
//

//
// V_SetBlockFuncs
//
// Sets the block drawing function pointers in a VBuffer object
// based on the size of the buffer. Called from V_SetupBufferFuncs.
//
void V_SetBlockFuncs(VBuffer *buffer, int drawtype)
{
    switch(drawtype)
    {
    case DRAWTYPE_UNSCALED:
        buffer->BlockDrawer       = V_blockDrawer;
        buffer->MaskedBlockDrawer = V_maskedBlockDrawer;
        buffer->TileBlock64       = V_tileBlock64;
        break;
    case DRAWTYPE_GENSCALED:
        buffer->BlockDrawer       = V_blockDrawerS;
        buffer->MaskedBlockDrawer = V_maskedBlockDrawerS;
        buffer->TileBlock64       = V_tileBlock64S;
        break;
    default: //
        break;
    }
}

// EOF

