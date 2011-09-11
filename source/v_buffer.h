// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2004 James Haley
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
//  VBuffer has become a much more all-purpose solution for many video handling
//  needs. Simple, but powerful pixelbuffer object.
//
//-----------------------------------------------------------------------------

#ifndef V_BUFFER_H__
#define V_BUFFER_H__

#include "m_fixed.h"

struct VBuffer
{
   int  width;
   int  height;
   int  pitch, pixelsize;

   byte     *data; // video memory
   boolean  owndata;
   int      subx, suby;

   void (*BlockDrawer)(int, int, VBuffer *, int, int, byte *);
   void (*MaskedBlockDrawer)(int, int, VBuffer *, int, int, int, 
                             byte *, byte *);
   void (*TileBlock64)(VBuffer *, byte *);

   // SoM: Include the screen size
   boolean  scaled, freelookups;
   int  unscaledw, unscaledh;
   int  *x1lookup;
   int  *y1lookup;
   int  *x2lookup;
   int  *y2lookup;
   fixed_t ixscale;
   fixed_t iyscale;

   byte **ylut;
   int  *xlut;

   // Only change this if you want memory leaks and/or crashes :P
   boolean needfree;
}; 

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
void V_BlitVBuffer(VBuffer *dest, int dx, int dy, VBuffer *src, 
                   unsigned int sx, unsigned int sy, unsigned int width, 
                   unsigned int height);

#endif //V_BUFFER_H__

// EOF

