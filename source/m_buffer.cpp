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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   Buffered file input/output.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "m_buffer.h"
#include "m_swap.h"

//=============================================================================
//
// BufferedFileBase
//
// haleyjd 11/26/10: Having converted to C++ with the aim of adding an input
// buffer as well, these base class methods implement shared functionality.
//

//
// BufferedFileBase::InitBuffer
//
// Sets up the buffer
//
void BufferedFileBase::InitBuffer(size_t pLen, int pEndian)
{
   buffer = ecalloc(byte *, pLen, sizeof(byte));
   len    = pLen;
   idx    = 0;
   endian = pEndian;
}

//
// BufferedFileBase::Tell
//
// Gives the current file offset; this does not account for any data that might
// be currently pending in an output buffer.
//
long BufferedFileBase::Tell()
{
   return ftell(f);
}

//
// BufferedFileBase::Close
//
// Common functionality for closing the file.
//
void BufferedFileBase::Close()
{
   if(f)
   {
      fclose(f);
      f = NULL;
   }

   idx = 0;
   len = 0;

   if(buffer)
   {
      efree(buffer);
      buffer = NULL;
   }

   ownFile = false;
}

//
// BufferedFileBase::SwapULong
//
// Transform a uint32 value based on the 'endian' setting.
//
void BufferedFileBase::SwapULong(uint32_t &x)
{
   switch(endian)
   {
   case LENDIAN:
      x = ::SwapULong(x);
      break;
   case BENDIAN:
      x = ::SwapBigULong(x);
      break;
   default:
      break;
   }
}

//
// BufferedFileBase::SwapLong
//
// As above but for signed ints.
//
void BufferedFileBase::SwapLong(int32_t &x)
{
   switch(endian)
   {
   case LENDIAN:
      x = ::SwapLong(x);
      break;
   case BENDIAN:
      x = ::SwapBigLong(x);
      break;
   default:
      break;
   }
}

// IOANCH: more shit
void BufferedFileBase::SwapLongLong(int64_t &x)
{
   switch(endian)
   {
      case LENDIAN:
         x = ::SwapLongLong(x);
         break;
      case BENDIAN:
         I_Error("Big endian BufferedFileBase int64_t not implemented");
         break;
      default:
         break;
   }
}


//
// BufferedFileBase::SwapUShort
//
// Transform a uint16 value based on the 'endian' setting.
//
void BufferedFileBase::SwapUShort(uint16_t &x)
{
   switch(endian)
   {
   case LENDIAN:
      x = ::SwapUShort(x);
      break;
   case BENDIAN:
      x = ::SwapBigUShort(x);
      break;
   default:
      break;
   }
}

//
// BufferedFileBase::SwapShort
//
// As above but for signed 16-bit shorts.
//
void BufferedFileBase::SwapShort(int16_t &x)
{
   switch(endian)
   {
   case LENDIAN:
      x = ::SwapShort(x);
      break;
   case BENDIAN:
      x = ::SwapBigShort(x);
      break;
   default:
      break;
   }
}

//=============================================================================
//
// OutBuffer
//
// Buffered file output
//

//
// OutBuffer::CreateFile
//
// Opens a file for buffered binary output with the given filename. The buffer
// size is determined by the len parameter.
//
bool OutBuffer::CreateFile(const char *filename, size_t pLen, int pEndian)
{
   if(!(f = fopen(filename, "wb")))
      return false;

   InitBuffer(pLen, pEndian);

   ownFile = true;

   return true;
}

//
// OutBuffer::Flush
//
// Call to flush the contents of the buffer to the output file. This will be
// called automatically before the file is closed, but must be called explicitly
// if a current file offset is needed. Returns false if an IO error occurs.
//
bool OutBuffer::Flush()
{
   if(idx)
   {
      if(fwrite(buffer, sizeof(byte), idx, f) < idx)
      {
         if(throwing)
            throw BufferedIOException("fwrite did not write the requested amount");
         return false;
      }
      idx = 0;
   }

   return true;
}

//
// OutBuffer::Close
//
// Overrides BufferedFileBase::Close()
// Closes the output file, writing any pending data in the buffer first.
// The output buffer is also freed.
//
void OutBuffer::Close()
{
   try
   {
     if(f)
        Flush();
   }
   catch(...)
   {
   }
      
   BufferedFileBase::Close();
}

//
// OutBuffer::Write
//
// Buffered writing function.
//
bool OutBuffer::Write(const void *data, size_t size)
{
   const byte *lSrc = (const byte *)data;
   size_t lWriteAmt;
   size_t lBytesToWrite = size;

   while(lBytesToWrite)
   {
      lWriteAmt = len - idx;
      
      if(!lWriteAmt)
      {
         if(!Flush())
            return false;
         lWriteAmt = len;
      }

      if(lBytesToWrite < lWriteAmt)
         lWriteAmt = lBytesToWrite;

      memcpy(&(buffer[idx]), lSrc, lWriteAmt);

      idx  += lWriteAmt;
      lSrc += lWriteAmt;
      lBytesToWrite -= lWriteAmt;
   }

   return true;
}

//
// OutBuffer::WriteUint32
//
// Convenience routine to write an unsigned integer into the buffer.
//
bool OutBuffer::WriteUint32(uint32_t num)
{
   SwapULong(num);
   return Write(&num, sizeof(uint32_t));
}

//
// OutBuffer::WriteSint32
//
// Convenience routine to write an integer into the buffer.
//
bool OutBuffer::WriteSint32(int32_t num)
{
   SwapLong(num);
   return Write(&num, sizeof(int32_t));
}

//
// OutBuffer::WriteUint16
//
// Convenience routine to write an unsigned short int into the buffer.
//
bool OutBuffer::WriteUint16(uint16_t num)
{
   SwapUShort(num);
   return Write(&num, sizeof(uint16_t));
}

//
// OutBuffer::WriteSint16
//
// Convenience routine to write a short int into the buffer.
//
bool OutBuffer::WriteSint16(int16_t num)
{
   SwapShort(num);
   return Write(&num, sizeof(int16_t));
}

//
// OutBuffer::WriteUint8
//
// Routine to write an unsigned byte into the buffer.
// This is much more efficient than calling M_BufferWrite for individual bytes.
//
bool OutBuffer::WriteUint8(uint8_t num)
{     
   if(idx == len)
   {
      if(!Flush())
         return false;
   }

   buffer[idx] = num;
   ++idx;
 
   return true;
}

//
// OutBuffer::WriteSint8
//
// Routine to write a byte into the buffer.
// This is much more efficient than calling M_BufferWrite for individual bytes.
//
bool OutBuffer::WriteSint8(int8_t num)
{     
   return WriteUint8((uint8_t)num);
}

//=============================================================================
//
// InBuffer
//
// haleyjd 11/26/10: Buffered file input
//

//
// InBuffer::openFile
//
// Opens a file for binary input.
//
bool InBuffer::openFile(const char *filename, int pEndian)
{
   if(!(f = fopen(filename, "rb")))
      return false;

   endian  = pEndian;
   ownFile = true;

   return true;
}

//
// InBuffer::openExisting
//
// Attach the input buffer to an already open file.
//
bool InBuffer::openExisting(FILE *pf, int pEndian)
{
   if(!(f = pf))
      return false;

   endian  = pEndian;
   ownFile = false;

   return true;
}

// IOANCH: added support for "throwing" flag

//
// InBuffer::seek
//
// Seeks inside the file via fseek, and then clears the internal buffer.
//
int InBuffer::seek(long offset, int origin)
{
   int r = fseek(f, offset, origin);
   if(throwing && r)
      throw BufferedIOException(strerror(errno));
   return r;
}

//
// InBuffer::read
//
// Read 'size' amount of bytes from the file. Reads are done from the physical
// medium in chunks of the buffer's length.
//
size_t InBuffer::read(void *dest, size_t size)
{
   size_t r = fread(dest, 1, size, f);
   if(throwing && r != size)
      throw BufferedIOException("Error reading");
   return r;
}

//
// InBuffer::skip
//
// Skip a given number of bytes forward.
//
int InBuffer::skip(size_t skipAmt)
{
   int r = fseek(f, skipAmt, SEEK_CUR);
   if(throwing && r)
      throw BufferedIOException(strerror(errno));
   return r;
}

//
// InBuffer::readUint32
//
// Read a uint32 value from the input file.
//
bool InBuffer::readUint32(uint32_t &num)
{
   uint32_t lNum;

   if(read(&lNum, sizeof(lNum)) != sizeof(lNum))
      return false;

   SwapULong(lNum);
   num = lNum;
   return true;
}

//
// InBuffer::readSint32
//
// Read an int32 value from the input file.
//
bool InBuffer::readSint32(int32_t &num)
{
   int32_t lNum;

   if(read(&lNum, sizeof(lNum)) != sizeof(lNum))
      return false;

   SwapLong(lNum);
   num = lNum;
   return true;
}

// IOANCH: add 64-bit
bool InBuffer::readSint64(int64_t &num)
{
   int64_t lNum;
   
   if(read(&lNum, sizeof(lNum)) != sizeof(lNum))
      return false;
   
   SwapLongLong(lNum);
   num = lNum;
   return true;
}

//
// InBuffer::readUint16
//
// Read a uint16 value from the input file.
//
bool InBuffer::readUint16(uint16_t &num)
{
   uint16_t lNum;

   if(read(&lNum, sizeof(lNum)) != sizeof(lNum))
      return false;

   SwapUShort(lNum);
   num = lNum;
   return true;
}

//
// InBuffer::readSint16
//
// Read an int16 value from the input file.
//
bool InBuffer::readSint16(int16_t &num)
{
   int16_t lNum;

   if(read(&lNum, sizeof(lNum)) != sizeof(lNum))
      return false;

   SwapShort(lNum);
   num = lNum;
   return true;
}

//
// InBuffer::readUint8
//
// Read a uint8 value from input file.
//
bool InBuffer::readUint8(uint8_t &num)
{
   if(read(&num, sizeof(num)) != sizeof(num))
      return false;

   return true;
}

//
// InBuffer::readSint8
//
// Read an int8 value from input file.
//
bool InBuffer::readSint8(int8_t &num)
{
   if(read(&num, sizeof(num)) != sizeof(num))
      return false;
   
   return true;
}

// EOF

