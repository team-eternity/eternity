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
// Purpose: Zone Memory Allocation, perhaps NeXT ObjectiveC inspired.
//  Remark: This was the only stuff that, according
//   to John Carmack, might have been useful for Quake.
//
//  Rewritten by Lee Killough, though, since it was not efficient enough.
//
// Authors: James Haley, Max Waine
//

#ifndef Z_AUTO_H__
#define Z_AUTO_H__

#include "z_zone.h"

//
// ZAutoBuffer
//
// This object wraps a generic zone allocation and gives it a stack lifetime.
//
class ZAutoBuffer
{
protected:
    void  *buffer;
    size_t size;

public:
    ZAutoBuffer() : buffer(nullptr), size(0) {}

    ZAutoBuffer(size_t pSize, bool initZero) : buffer(nullptr), size(pSize) { alloc(size, initZero); }

    // Efficiency notice: if you copy a ZAutoBuffer, you copy its buffer;
    // there is no attempt at a reference counting scheme.
    ZAutoBuffer(const ZAutoBuffer &other)
    {
        size   = other.size;
        buffer = emalloc(void *, size);
        memcpy(buffer, other.buffer, size);
    }

    ~ZAutoBuffer()
    {
        if(buffer)
        {
            efree(buffer);
            buffer = nullptr;
        }
    }

    void *alloc(size_t pSize, bool initZero)
    {
        if(buffer)
        {
            efree(buffer);
            buffer = nullptr;
        }

        if((size = pSize))
        {
            buffer = emalloc(void *, size);
            if(initZero)
                memset(buffer, 0, size);
        }

        return buffer;
    }

    void  *get() const { return buffer; }
    size_t getSize() const { return size; }

    template<typename T>
    T getAs() const
    {
        return static_cast<T>(buffer);
    }
};

#endif

// EOF

