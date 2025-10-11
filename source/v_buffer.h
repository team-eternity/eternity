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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: VBuffer has become a much more all-purpose solution for many video
//  handling needs. Simple, but powerful pixelbuffer object.
//
// Authors: Stephen McGranahan, James Haley, Ioan Chera
//

#ifndef V_BUFFER_H__
#define V_BUFFER_H__

#include "m_fixed.h"

struct VBuffer
{
    int width;
    int height;
    int pitch, pixelsize; // This is TRANSPOSED, PRE-TRANSFORMED pitch

    byte *data; // video memory
    bool  owndata;
    int   subx, suby;

    void (*BlockDrawer)(int, int, VBuffer *, int, int, const byte *);
    void (*MaskedBlockDrawer)(int, int, VBuffer *, int, int, int, const byte *, byte *);
    void (*TileBlock64)(VBuffer *, const byte *);

    // SoM: Include the screen size
    bool    scaled, freelookups;
    int     unscaledw, unscaledh;
    int    *x1lookup;
    int    *y1lookup;
    int    *x2lookup;
    int    *y2lookup;
    fixed_t ixscale;
    fixed_t iyscale;

    // Only change this if you want memory leaks and/or crashes :P
    bool needfree;

    fixed_t getRealAspectRatio() const;
    fixed_t getVirtualAspectRatio() const;

    int mapXFromOther(const int x, const VBuffer &other) const;
    int mapYFromOther(const int y, const VBuffer &other) const;
};

inline static byte *VBADDRESS(VBuffer *vb, const int x, const int y)
{
    return vb->data + vb->pitch * x + vb->pixelsize * y;
}

// V_InitVBuffer
// Initializes the given vbuffer and allocates pixeldata for it.
void V_InitVBuffer(VBuffer *vb, int width, int height, int bitdepth);

// V_CreateVBuffer
// Allocates a new VBuffer object and returns it
VBuffer *V_CreateVBuffer(int width, int height, int bitdepth);

// V_InitVBufferFrom
// Gives a VBuffer object the given pixeldata. The VBuffer does not OWN the
// given data and so it will not be freed by V_FreeVBuffer
void V_InitVBufferFrom(VBuffer *vb, int width, int height, int pitch, int bitdepth, byte *data);

// V_CreateVBufferFrom
// Allocates a new VBuffer object with the given pixeldata. The VBuffer created
// by this funciton does not OWN the given data and so it will not be freed by
// V_FreeVBuffer
VBuffer *V_CreateVBufferFrom(int width, int height, int pitch, int bitdepth, byte *data);

// V_InitSubVBuffer
//
void V_InitSubVBuffer(VBuffer *vb, VBuffer *parent, int x, int y, int width, int height);

// V_SubVBuffer
// Allocates a new VBuffer object that shares pixel data with another.
VBuffer *V_SubVBuffer(VBuffer *parent, int x, int y, int width, int height);

// V_FreeVBuffer
// Frees the given VBuffer object. If the VBuffer owns it's pixeldata, the data
// will be freed. If the VBuffer has child buffers (created with V_SubVBuffer)
// the child buffers will be given their own pixeldata.
void V_FreeVBuffer(VBuffer *buffer);

// V_SetScaling
// Creates and sets up scaling information in the given VBuffer
void V_SetScaling(VBuffer *buffer, int scaledw, int scaledh);

// V_UnsetScaling
// Deletes the scaling information in the given VBuffer
void V_UnsetScaling(VBuffer *buffer);

// V_BlitVBuffer
// Copies the contents of one VBuffer to the other.
void V_BlitVBuffer(VBuffer *dest, int dx, int dy, VBuffer *src, unsigned int sx, unsigned int sy, unsigned int width,
                   unsigned int height);

#endif // V_BUFFER_H__

// EOF

