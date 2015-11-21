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

#include "../m_compare.h"
#include "b_compression.h"

//
// zlibLevelForCompressLevel
//
// Utility for converting abstract level to concrete one
//
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

//
// GZCompression::CreateFile
//
// Initializes the compressed file
//
bool GZCompression::CreateFile(const char *filename, size_t pLen, int pEndian, CompressLevel clevel)
{

   // FIXME: do something in case it's open already

    if (!OutBuffer::CreateFile(filename, pLen, pEndian))
        return false;

    int level = zlibLevelForCompressLevel(clevel);

    if (m_init)
    {
        deflateEnd(&m_strm);
        m_init = false;
    }
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

//
// GZCompression::Flush
//
// Flush from buffer, doing the actual writing
//
bool GZCompression::Flush()
{
    if (idx)
    {
        m_strm.avail_in = static_cast<uInt>(idx);
        m_strm.next_in = buffer;

        unsigned char out[CHUNK];
        int ret;
        unsigned have;

        do
        {
            m_strm.avail_out = CHUNK;
            m_strm.next_out = out;
            ret = deflate(&m_strm, Z_NO_FLUSH);
            if (ret < 0)
            {
                if (throwing)
                    throw BufferedIOException("gzip deflate failed");
                return false;
            }
            have = CHUNK - m_strm.avail_out;
            if (fwrite(out, 1, have, f) != have || ferror(f))
            {
                if (throwing)
                    throw BufferedIOException("gzip fwrite did not write the requested amount");                
                return false;
            }
        } while (m_strm.avail_out == 0);

        idx = 0;
    }

    return true;
}

//
// GZCompression::Close
//
// Flush everything, finalize compression and close file
//
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

//
// GZCompression::~GZCompression
//
// Destructor that closes the stream safely
//
GZCompression::~GZCompression()
{
    Close();
}

///////////////////////////////////////////////////////////////////////////////
//
// GZExpansion
//
///////////////////////////////////////////////////////////////////////////////

//
// GZExpansion::initZStream
//
// Common routine for initializing the zlib stream, from the two "open" methods
//
bool GZExpansion::initZStream(int pEndian, size_t pLen)
{
   InitBuffer(pLen, pEndian);
   idx = len;  // start with idx at the end

   if (m_init)
   {
       inflateEnd(&m_strm);
       m_init = false;
   }

   m_strm.zalloc = Z_NULL;
   m_strm.zfree = Z_NULL;
   m_strm.opaque = Z_NULL;
   m_strm.avail_in = 0;
   m_strm.next_in = Z_NULL;
   int ret = inflateInit(&m_strm);
   if(ret != Z_OK)
   {
      memset(&m_strm, 0, sizeof(m_strm));
      Close();
      return false;
   }

   m_init = true;
   return true;
}

//
// GZExpansion::openFile
//
// Open file and init z_strm
//
bool GZExpansion::openFile(const char *filename, int pEndian, size_t pLen)
{

   // FIXME: do something in case it's open already

   if(!InBuffer::openFile(filename, pEndian))
      return false;
   return initZStream(pEndian, pLen);
}

//
// GZExpansion::openExisting
//
bool GZExpansion::openExisting(FILE *f, int pEndian, size_t pLen)
{
   if(!InBuffer::openExisting(f, pEndian))
      return false;
   return initZStream(pEndian, pLen);
}

//
// GZExpansion::inflateToBuffer
//
// Updates the buffer now
//
bool GZExpansion::inflateToBuffer()
{
   unsigned char out[CHUNK];

   if (!m_ongoing)
   {
       m_strm.avail_in = static_cast<uInt>(fread(m_ongoingBuffer, 1, CHUNK, f));
       if (ferror(f))
       {
           if (throwing)
               throw BufferedIOException("Error inflating");
           return false;
       }
       if (m_strm.avail_in == 0)
       {
           return true;
       }
       m_strm.next_in = m_ongoingBuffer;
   }
   
   int ret;
   unsigned have;
   uInt minOut;
   do
   {
      minOut = emin(CHUNK, idx);

      m_strm.avail_out = minOut;
      m_strm.next_out = out;
      ret = inflate(&m_strm, Z_NO_FLUSH);
      switch (ret)
      {
      case Z_NEED_DICT:
          ret = Z_DATA_ERROR;
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
      case Z_STREAM_ERROR:  // IOANCH: added because it can enter this bad state
          // TODO: finalize
          return false;
      }
      have = minOut - m_strm.avail_out;

      idx -= have;
      memcpy(buffer + idx, out, have);

   } while (m_strm.avail_out == 0 && idx);

   m_ongoing = m_strm.avail_out == 0;

   return true;
}

//
// GZExpansion::read
//
// Uncompresses some data into the buffer and outputs the "size" bytes.
// As of again, it returns the valid data.
//
size_t GZExpansion::read(void *vdest, size_t size)
{
   auto dest = static_cast<unsigned char*>(vdest);
   size_t curleft = len - idx;
   if(size > curleft)
   {
      if(curleft)
      {
         memcpy(vdest, buffer + idx, curleft);
         idx = len;
      }

      // Still some to read

      bool success = inflateToBuffer();

      size_t retval = curleft + (success && idx < len ? read(dest + curleft, size - curleft) : 0);

      if (throwing && retval != size)
          throw BufferedIOException("Error reading gzip");

      // Call recursively
      return retval;
   }
   else
   {
      memcpy(vdest, buffer + idx, size);
      idx += size;
      return size;
   }
}

void GZExpansion::Close()
{
    if (m_init)
    {
        inflateEnd(&m_strm);
        m_init = false;
    }
    m_ongoing = false;
    BufferedFileBase::Close();
}

GZExpansion::~GZExpansion()
{
    Close();
}

// EOF

