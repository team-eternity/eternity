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
    static const size_t CHUNK = 16384;

    z_stream m_strm;
    bool m_init;

public:
    GZCompression() : m_strm(), m_init(false), OutBuffer()
    {
    }
    ~GZCompression();

    bool createFile(const char *filename, size_t pLen, int pEndian, CompressLevel level = CompressLevel_Default);
    bool flush();
    void close();

    // Write inherited from OutBuffer
    
};

class GZExpansion : public InBuffer
{
   // increased from default 16384 (from zpipe.c common
   // example) because otherwise inflate() may fail. I might need a more robust
   // way, though.
   enum { CHUNK = 32768, };

   z_stream m_strm;
   bool m_init;

   unsigned char m_ongoingBuffer[CHUNK];
   bool m_ongoing;

   bool initZStream(int pEndian, size_t pLen);
   bool inflateToBuffer();

public:
   GZExpansion() : m_strm(), m_init(false), m_ongoing(false), InBuffer()
   {
   }
   ~GZExpansion();

   bool openFile(const char *filename, int pEndian, size_t pLen = CHUNK);
   bool openExisting(FILE *f, int pEndian, size_t pLen = CHUNK);

   // Not seekable (at least not for now). You can still call parent class's
   // seek method, but it won't do anything about expansion.
   int seek(long offset, int origin) = delete;
   size_t read(void *dest, size_t size);
   int skip(size_t skipAmt) = delete;

   void close();
};

#endif
// EOF

