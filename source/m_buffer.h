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
   
   void InitBuffer(size_t pLen, int pEndian);

public:
   BufferedFileBase() 
      : f(NULL), buffer(NULL), len(0), idx(0), endian(0), throwing(false),
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
         buffer = NULL;
      }
   }

   long Tell();
   virtual void Close();

   void SwapLong  (int32_t  &x);
   void SwapShort (int16_t  &x);
   void SwapULong (uint32_t &x);
   void SwapUShort(uint16_t &x);

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
//
class OutBuffer : public BufferedFileBase
{
public:
   bool CreateFile(const char *filename, size_t pLen, int pEndian);
   bool Flush();
   void Close();

   bool Write(const void *data, size_t size);
   bool WriteSint32(int32_t  num);
   bool WriteUint32(uint32_t num);
   bool WriteSint16(int16_t  num);
   bool WriteUint16(uint16_t num);
   bool WriteSint8 (int8_t   num);
   bool WriteUint8 (uint8_t  num);
};

//
// InBuffer
//
// Buffered binary file input.
//
class InBuffer : public BufferedFileBase
{
public:
   InBuffer() : BufferedFileBase()
   {
   }

   bool openFile(const char *filename, int pEndian);
   bool openExisting(FILE *f, int pEndian);

   int    seek(long offset, int origin);
   size_t read(void *dest, size_t size);
   int    skip(size_t skipAmt);
   bool   readSint32(int32_t  &num);
   bool   readUint32(uint32_t &num);
   bool   readSint16(int16_t  &num);
   bool   readUint16(uint16_t &num);
   bool   readSint8 (int8_t   &num);
   bool   readUint8 (uint8_t  &num);
};

#endif

// EOF


