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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//  Functions to manipulate linear blocks of graphics data.
//
//-----------------------------------------------------------------------------

#ifndef V_BLOCK_H__
#define V_BLOCK_H__

#include "doomtype.h"

struct VBuffer;

// Scaled color block drawers.

void V_ColorBlockScaled(VBuffer *buffer, byte color, int x, int y, int w, int h);

void V_ColorBlockTLScaled(VBuffer *dest, byte color, int x, int y, int w, int h, 
                          int tl);

// haleyjd 02/02/05: color block drawing functions

void V_ColorBlock(VBuffer *buffer, byte color, int x, int y, int w, int h);

void V_ColorBlockTL(VBuffer *buffer, byte color, int x, int y, int w, 
                    int h, int tl);

// buffer fill w/texture
void V_FillBuffer(VBuffer *buffer, const byte *src, int texw, int texh);

// sets block function pointers for a VBuffer object
void V_SetBlockFuncs(VBuffer *, int);

#endif

// EOF



