// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2015 Ioan Chera
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Compression class
//
//-----------------------------------------------------------------------------

#ifndef B_COMPRESSION_H_
#define B_COMPRESSION_H_

#include "../m_buffer.h"

#include "../../zlib/zlib.h"

enum CompressLevel
{
    CompressLevel_None,
    CompressLevel_Speed,
    CompressLevel_Space,
    CompressLevel_Default,
};

class GZCompression : public OutBuffer
{
    z_stream m_strm;
    bool m_init;

public:
    GZCompression() : m_strm(), m_init(false), OutBuffer()
    {
    }
    ~GZCompression();

    bool CreateFile(const char *filename, size_t pLen, int pEndian, CompressLevel level = CompressLevel_Default);
    bool Flush();
    void Close();

    // Write inherited from OutBuffer
    
};

class GZExpansion : public InBuffer
{
   z_stream m_strm;
   bool m_init;

   bool initZStream();


public:
   GZExpansion() : m_strm(), m_init(false), InBuffer()
   {
   }

   bool openFile(const char *filename, int pEndian);
   bool openExisting(FILE *f, int pEndian);

   // Not seekable (at least not for now). You can still call parent class's
   // seek method, but it won't do anything about expansion.
   int seek(long offset, int origin) = delete;

   // TODO: reading and buffering routines
};

#endif
// EOF

