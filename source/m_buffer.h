// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2010 James Haley
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
protected:
   size_t readlen; // amount actually read (may be less than len)
   bool atEOF;
   bool ReadFile();

public:
   InBuffer() : BufferedFileBase(), readlen(0), atEOF(false)
   {
   }

   bool OpenFile(const char *filename, size_t pLen, int pEndian);
   bool OpenExisting(FILE *f, size_t pLen, int pEndian);

   int  Seek(long offset, int origin);
   bool Read(void *dest, size_t size);
   bool Read(void *dest, size_t size, size_t &amtRead);
   bool Skip(size_t skipAmt);
   bool ReadSint32(int32_t  &num);
   bool ReadUint32(uint32_t &num);
   bool ReadSint16(int16_t  &num);
   bool ReadUint16(uint16_t &num);
   bool ReadSint8 (int8_t   &num);
   bool ReadUint8 (uint8_t  &num);
};

#endif

// EOF


