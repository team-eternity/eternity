//
// The Eternity Engine
// Copyright (C) 2016 James Haley et al.
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
// Purpose: Buffered file input/output
// Authors: James Haley
//

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
// Sets up the buffer
//
void BufferedFileBase::initBuffer(size_t pLen, int pEndian)
{
   buffer = ecalloc(byte *, pLen, sizeof(byte));
   len    = pLen;
   idx    = 0;
   endian = pEndian;
}

//
// Gives the current file offset; this does not account for any data that might
// be currently pending in an output buffer.
//
long BufferedFileBase::tell()
{
   return ftell(f);
}

//
// Common functionality for closing the file.
//
void BufferedFileBase::close()
{
   if(f)
   {
      fclose(f);
      f = nullptr;
   }

   idx = 0;
   len = 0;

   if(buffer)
   {
      efree(buffer);
      buffer = nullptr;
   }

   ownFile = false;
}

//
// Transform a uint32 value based on the 'endian' setting.
//
void BufferedFileBase::swapULong(uint32_t &x)
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
// As above but for signed ints.
//
void BufferedFileBase::swapLong(int32_t &x)
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

//
// Transform a uint16 value based on the 'endian' setting.
//
void BufferedFileBase::swapUShort(uint16_t &x)
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
// As above but for signed 16-bit shorts.
//
void BufferedFileBase::swapShort(int16_t &x)
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
// Opens a file for buffered binary output with the given filename. The buffer
// size is determined by the len parameter.
//
bool OutBuffer::createFile(const char *filename, size_t pLen, int pEndian)
{
   if(!(f = fopen(filename, "wb")))
      return false;

   initBuffer(pLen, pEndian);

   ownFile = true;

   return true;
}

//
// Call to flush the contents of the buffer to the output file. This will be
// called automatically before the file is closed, but must be called explicitly
// if a current file offset is needed. Returns false if an IO error occurs.
//
bool OutBuffer::flush()
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
// Overrides BufferedFileBase::Close()
// Closes the output file, writing any pending data in the buffer first.
// The output buffer is also freed.
//
void OutBuffer::close()
{
   try
   {
     if(f)
        flush();
   }
   catch(...)
   {
   }
      
   BufferedFileBase::close();
}

//
// Buffered writing function.
//
bool OutBuffer::write(const void *data, size_t size)
{
   const byte *lSrc = (const byte *)data;
   size_t lWriteAmt;
   size_t lBytesToWrite = size;

   while(lBytesToWrite)
   {
      lWriteAmt = len - idx;
      
      if(!lWriteAmt)
      {
         if(!flush())
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
// Convenience routine to write an unsigned long long into the buffer.
//
bool OutBuffer::writeUint64(uint64_t num)
{
   SwapUInt64(num);
   return write(&num, sizeof(uint64_t));
}

//
// Convenience routine to write an integer into the buffer.
//
bool OutBuffer::writeSint64(int64_t num)
{
   SwapInt64(num);
   return write(&num, sizeof(int64_t));
}

//
// Convenience routine to write an unsigned integer into the buffer.
//
bool OutBuffer::writeUint32(uint32_t num)
{
   SwapULong(num);
   return write(&num, sizeof(uint32_t));
}

//
// Convenience routine to write an integer into the buffer.
//
bool OutBuffer::writeSint32(int32_t num)
{
   SwapLong(num);
   return write(&num, sizeof(int32_t));
}

//
// Convenience routine to write an unsigned short int into the buffer.
//
bool OutBuffer::writeUint16(uint16_t num)
{
   SwapUShort(num);
   return write(&num, sizeof(uint16_t));
}

//
// Convenience routine to write a short int into the buffer.
//
bool OutBuffer::writeSint16(int16_t num)
{
   SwapShort(num);
   return write(&num, sizeof(int16_t));
}

//
// Routine to write an unsigned byte into the buffer.
// This is much more efficient than calling M_BufferWrite for individual bytes.
//
bool OutBuffer::writeUint8(uint8_t num)
{     
   if(idx == len)
   {
      if(!flush())
         return false;
   }

   buffer[idx] = num;
   ++idx;
 
   return true;
}

//
// Routine to write a byte into the buffer.
// This is much more efficient than calling M_BufferWrite for individual bytes.
//
bool OutBuffer::writeSint8(int8_t num)
{     
   return writeUint8((uint8_t)num);
}

//=============================================================================
//
// InBuffer
//
// haleyjd 11/26/10: Buffered file input
//

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

//
// Seeks inside the file via fseek, and then clears the internal buffer.
//
int InBuffer::seek(long offset, int origin)
{
   return fseek(f, offset, origin);
}

//
// Read 'size' amount of bytes from the file. Reads are done from the physical
// medium in chunks of the buffer's length.
//
size_t InBuffer::read(void *dest, size_t size)
{
   return fread(dest, 1, size, f);
}

//
// Skip a given number of bytes forward.
//
int InBuffer::skip(size_t skipAmt)
{
   return fseek(f, static_cast<long>(skipAmt), SEEK_CUR);
}

//
// Read a uint64 value from the input file.
//
bool InBuffer::readUint64(uint64_t &num)
{
   uint64_t lNum;

   if(read(&lNum, sizeof(lNum)) != sizeof(lNum))
      return false;

   SwapUInt64(lNum);
   num = lNum;
   return true;
}

//
// Read an int64 value from the input file.
//
bool InBuffer::readSint64(int64_t &num)
{
   int64_t lNum;

   if(read(&lNum, sizeof(lNum)) != sizeof(lNum))
      return false;

   SwapInt64(lNum);
   num = lNum;
   return true;
}


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
// Read a uint8 value from input file.
//
bool InBuffer::readUint8(uint8_t &num)
{
   if(read(&num, sizeof(num)) != sizeof(num))
      return false;

   return true;
}

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

