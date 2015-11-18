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

// Largely based on the http://www.zlib.net/zpipe.c example

#include "../z_zone.h"

#include "b_compression.h"

static const size_t CHUNK = 16384;

static int zlibLevelForCompressLevel(CompressLevel clevel)
{
    switch (clevel)
    {
    default:
        return Z_DEFAULT_COMPRESSION;
    case CompressLevel_None:
        return Z_NO_COMPRESSION;
    case CompressLevel_Space:
        return Z_BEST_COMPRESSION;
    case CompressLevel_Speed:
        return Z_BEST_SPEED;
    }
}

bool GZCompression::CreateFile(const char *filename, size_t pLen, int pEndian, CompressLevel clevel)
{
    if (!OutBuffer::CreateFile(filename, pLen, pEndian))
        return false;

    int level = zlibLevelForCompressLevel(clevel);

    m_strm.zalloc = Z_NULL;
    m_strm.zfree = Z_NULL;
    m_strm.opaque = Z_NULL;
    int ret = deflateInit(&m_strm, level);
    if (ret != Z_OK)
    {
        memset(&m_strm, 0, sizeof(m_strm));
        Close();
        return false;
    }

    m_init = true;

    return true;
}

bool GZCompression::Flush()
{
    if (idx)
    {
        m_strm.avail_in = idx;
        m_strm.next_in = buffer;

        unsigned char out[CHUNK];
        int ret;
        unsigned have;

        do
        {
            m_strm.avail_out = CHUNK;
            m_strm.next_out = out;
            ret = deflate(&m_strm, Z_NO_FLUSH);
            have = CHUNK - m_strm.avail_out;
            if (fwrite(out, 1, have, f) != have || ferror(f))
            {
                deflateEnd(&m_strm);
                memset(&m_strm, 0, sizeof(m_strm));
                m_init = false;

                Close();

                if (throwing)
                    throw BufferedIOException("gzip fwrite did not write the requested amount");
                
                return false;
            }
        } while (m_strm.avail_out == 0);

        idx = 0;
    }

    return true;
}

void GZCompression::Close()
{
    try
    {
        if (f)
            Flush();

        // now remove it.
        if (f && m_init)
        {
            unsigned char dummy;
            m_strm.avail_in = 0;
            m_strm.next_in = &dummy;
            unsigned char out[CHUNK];
            int ret;
            unsigned have;
            do
            {
                m_strm.avail_out = CHUNK;
                m_strm.next_out = out;
                ret = deflate(&m_strm, Z_FINISH);
                have = CHUNK - m_strm.avail_out;
                if (fwrite(out, 1, have, f) != have || ferror(f))
                {
                    deflateEnd(&m_strm);
                    memset(&m_strm, 0, sizeof(m_strm));
                    m_init = false;
                    break;
                }
            } while (m_strm.avail_out == 0);
        }
    }
    catch (...)
    {
    }

    if (m_init)
    {
        deflateEnd(&m_strm);
        memset(&m_strm, 0, sizeof(m_strm));
        m_init = false;
    }

    BufferedFileBase::Close();
}

GZCompression::~GZCompression()
{
    Close();
}

// EOF

