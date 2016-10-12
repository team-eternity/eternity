// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//   Buffered file output.
//
//-----------------------------------------------------------------------------

#ifndef M_BUFFER_H__
#define M_BUFFER_H__

// Required for: byte
#include "doomtype.h"

//
// An exception class for buffered IO errors
//
class BufferedIOException
{
protected:
   const char *message;

public:
   BufferedIOException(const char *pMsg) : message(pMsg) {}
   const char *GetMessage() { return message; }
};

//
// BufferedFileBase
//
// Base class for buffers. Eventually I hpe to add an extra layer of indirection
// so that this can function on more than just raw physical files (for example a
// compressed data source).
//
class BufferedFileBase
{
protected:
   FILE *f;       // destination or source file
   byte *buffer;  // buffer
   size_t len;    // total buffer length
   size_t idx;    // current index
   int endian;    // endianness indicator
   bool throwing; // throws exceptions on IO errors
   bool ownFile;  // buffer owns the file
   
   void initBuffer(size_t pLen, int pEndian);

public:
   BufferedFileBase() 
      : f(nullptr), buffer(nullptr), len(0), idx(0), endian(0), throwing(false),
        ownFile(false)
   {
   }

   virtual ~BufferedFileBase()
   {
      if(ownFile && f)
         fclose(f);

      if(buffer)
      {
         efree(buffer);
         buffer = nullptr;
      }
   }

   long tell();
   virtual void close();

   void swapLong  (int32_t  &x);
   void swapLongLong(int64_t &x);
   void swapShort (int16_t  &x);
   void swapULong (uint32_t &x);
   void swapUShort(uint16_t &x);

   void setThrowing(bool val) { throwing = val;  }
   bool getThrowing() const   { return throwing; }

   // endianness values
   enum
   {
      NENDIAN, // doesn't swap shorts or ints
      LENDIAN, // swaps shorts/ints to little endian
      BENDIAN  // swaps shorts/ints to big endian
   };
};

//
// OutBuffer
//
// Buffered binary file output.
// IOANCH 20151118: made virtual
//
class OutBuffer : public BufferedFileBase
{
public:
   virtual bool createFile(const char *filename, size_t pLen, int pEndian);
   virtual bool flush();
   virtual void close();

   bool write(const void *data, size_t size);
   bool writeSint64(int64_t  num);
   bool writeUint64(uint64_t num);
   bool writeSint32(int32_t  num);
   bool writeUint32(uint32_t num);
   bool writeSint16(int16_t  num);
   bool writeUint16(uint16_t num);
   bool writeSint8 (int8_t   num);
   bool writeUint8 (uint8_t  num);
};

//
// InBuffer
//
// Buffered binary file input.
// IOANCH 20151118: made virtual
//
class InBuffer : public BufferedFileBase
{
public:
   InBuffer() : BufferedFileBase()
   {
   }

   virtual bool openFile(const char *filename, int pEndian);
   virtual bool openExisting(FILE *f, int pEndian);

   int    seek(long offset, int origin);
   virtual size_t read(void *dest, size_t size);
   int    skip(size_t skipAmt);
   bool   readSint64(int64_t  &num);
   bool   readUint64(uint64_t &num);
   bool   readSint32(int32_t  &num);
   bool   readUint32(uint32_t &num);
   bool   readSint16(int16_t  &num);
   bool   readUint16(uint16_t &num);
   bool   readSint8 (int8_t   &num);
   bool   readUint8 (uint8_t  &num);

   // IOANCH: convenient templates
   template<typename T>
   bool   readSint32T(T& num)
   {
      int32_t i32;
      if(!readSint32(i32))
         return false;
      // FIXME: can't use reinterpret_cast
      num = (T)(i32);  // force conversion
      return true;
   }

   template<typename T>
   bool   readUint8T(T& num)
   {
      uint8_t u8;
      if(!readUint8(u8))
         return false;
      num = (T)u8;
      return true;
   }
};

#endif

// EOF


