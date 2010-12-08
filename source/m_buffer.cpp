// Emacs style mode select   -*- C -*-
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
// CBufferedFileBase
//
// haleyjd 11/26/10: Having converted to C++ with the aim of adding an input
// buffer as well, these base class methods implement shared functionality.
//

//
// CBufferedFileBase::InitBuffer
//
// Sets up the buffer
//
void CBufferedFileBase::InitBuffer(size_t pLen, int pEndian)
{
   buffer = (byte *)(calloc(pLen, sizeof(byte)));
   len    = pLen;
   idx    = 0;
   endian = pEndian;
}

//
// CBufferedFileBase::Tell
//
// Gives the current file offset, minus any data that might be currently
// pending in an output buffer.
//
long CBufferedFileBase::Tell()
{
   return ftell(f);
}

//
// CBufferedFileBase::Close
//
// Common functionality for closing the file.
//
void CBufferedFileBase::Close()
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
      free(buffer);
      buffer = NULL;
   }
}

//
// CBufferedFileBase::SwapULong
//
// Transform a uint32 value based on the 'endian' setting.
//
void CBufferedFileBase::SwapULong(uint32_t &x)
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
// CBufferedFileBase::SwapLong
//
// As above but for signed ints.
//
void CBufferedFileBase::SwapLong(int32_t &x)
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
// CBufferedFileBase::SwapUShort
//
// Transform a uint16 value based on the 'endian' setting.
//
void CBufferedFileBase::SwapUShort(uint16_t &x)
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
// CBufferedFileBase::SwapShort
//
// As above but for signed 16-bit shorts.
//
void CBufferedFileBase::SwapShort(int16_t &x)
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
// COutBuffer
//
// Buffered file output
//

//
// COutBuffer::CreateFile
//
// Opens a file for buffered binary output with the given filename. The buffer
// size is determined by the len parameter.
//
boolean COutBuffer::CreateFile(const char *filename, size_t pLen, int pEndian)
{
   if(!(f = fopen(filename, "wb")))
      return false;

   InitBuffer(pLen, pEndian);

   return true;
}

//
// COutBuffer::Flush
//
// Call to flush the contents of the buffer to the output file. This will be
// called automatically before the file is closed, but must be called explicitly
// if a current file offset is needed. Returns false if an IO error occurs.
//
boolean COutBuffer::Flush()
{
   if(idx)
   {
      if(fwrite(buffer, sizeof(byte), idx, f) < idx)
      {
         if(throwing)
            throw CBufferedIOException("fwrite did not write the requested amount");
         return false;
      }
      idx = 0;
   }

   return true;
}

//
// COutBuffer::Close
//
// Overrides CBufferedFileBase::Close()
// Closes the output file, writing any pending data in the buffer first.
// The output buffer is also freed.
//
void COutBuffer::Close()
{
   try
   {
     if(f)
        Flush();
      
     CBufferedFileBase::Close();
   }
   catch(CBufferedIOException)
   {
      // if it didn't close, close it now. 
      // CPP_FIXME: should really use a proper RAII idiom
      if(f)
         CBufferedFileBase::Close();

      // propagate the exception
      throw; 
   }
}

//
// COutBuffer::Write
//
// Buffered writing function.
//
boolean COutBuffer::Write(const void *data, size_t size)
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
// COutBuffer::WriteUint32
//
// Convenience routine to write an unsigned integer into the buffer.
//
boolean COutBuffer::WriteUint32(uint32_t num)
{
   SwapULong(num);
   return Write(&num, sizeof(uint32_t));
}

//
// COutBuffer::WriteSint32
//
// Convenience routine to write an integer into the buffer.
//
boolean COutBuffer::WriteSint32(int32_t num)
{
   SwapLong(num);
   return Write(&num, sizeof(int32_t));
}

//
// COutBuffer::WriteUint16
//
// Convenience routine to write an unsigned short int into the buffer.
//
boolean COutBuffer::WriteUint16(uint16_t num)
{
   SwapUShort(num);
   return Write(&num, sizeof(uint16_t));
}

//
// COutBuffer::WriteSint16
//
// Convenience routine to write a short int into the buffer.
//
boolean COutBuffer::WriteSint16(int16_t num)
{
   SwapShort(num);
   return Write(&num, sizeof(int16_t));
}

//
// COutBuffer::WriteUint8
//
// Routine to write an unsigned byte into the buffer.
// This is much more efficient than calling M_BufferWrite for individual bytes.
//
boolean COutBuffer::WriteUint8(uint8_t num)
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
// COutBuffer::WriteSint8
//
// Routine to write a byte into the buffer.
// This is much more efficient than calling M_BufferWrite for individual bytes.
//
boolean COutBuffer::WriteSint8(int8_t num)
{     
   return WriteUint8((uint8_t)num);
}

//=============================================================================
//
// CInBuffer
//
// haleyjd 11/26/10: Buffered file input
//

//
// CInBuffer::OpenFile
//
// Opens a file for binary input.
//
boolean CInBuffer::OpenFile(const char *filename, size_t pLen, int pEndian)
{
   if(!(f = fopen(filename, "rb")))
      return false;

   InitBuffer(pLen, pEndian);
   readlen = 0;
   atEOF = false;
   return true;
}

//
// CInBuffer::ReadFile
//
// Read the buffer's amount of data from the file or as much as is left.
// If has hit EOF, no further reads are made.
//
boolean CInBuffer::ReadFile()
{
   if(!atEOF)
   {
      readlen = fread(buffer, 1, len, f);

      if(readlen != len)
         atEOF = true;
      idx = 0;
   }
   else
   {
      // exhausted input
      idx = 0;
      readlen = 0; 
   }

   return atEOF;
}

//
// CInBuffer::Read
//
// Read 'size' amount of bytes from the file. Reads are done from the physical
// medium in chunks of the buffer's length.
//
boolean CInBuffer::Read(void *dest, unsigned int size)
{
   byte *lDest = (byte *)dest;
   size_t lReadAmt;
   size_t lBytesToRead = size;

   while(lBytesToRead)
   {
      lReadAmt = readlen - idx;

      if(!lReadAmt) // nothing left in the buffer?
      {
         if(!ReadFile()) // try to read more
         {
            if(!readlen) // nothing was read? uh oh!
               return false;
         }
         lReadAmt = readlen;
      }

      if(lBytesToRead < lReadAmt)
         lReadAmt = lBytesToRead;

      memcpy(lDest, &(buffer[idx]), lReadAmt);

      idx += lReadAmt;
      lDest += lReadAmt;
      lBytesToRead -= lReadAmt;
   }

   return true;
}

//
// CInBuffer::ReadUint32
//
// Read a uint32 value from the input file.
//
boolean CInBuffer::ReadUint32(uint32_t &num)
{
   uint32_t lNum;

   if(!Read(&lNum, sizeof(lNum)))
      return false;

   SwapULong(lNum);
   num = lNum;
   return true;
}

//
// CInBuffer::ReadSint32
//
// Read an int32 value from the input file.
//
boolean CInBuffer::ReadSint32(int32_t &num)
{
   int32_t lNum;

   if(!Read(&lNum, sizeof(lNum)))
      return false;

   SwapLong(lNum);
   num = lNum;
   return true;
}

//
// CInBuffer::ReadUint16
//
// Read a uint16 value from the input file.
//
boolean CInBuffer::ReadUint16(uint16_t &num)
{
   uint16_t lNum;

   if(!Read(&lNum, sizeof(lNum)))
      return false;

   SwapUShort(lNum);
   num = lNum;
   return true;
}

//
// CInBuffer::ReadSint16
//
// Read an int16 value from the input file.
//
boolean CInBuffer::ReadSint16(int16_t &num)
{
   int16_t lNum;

   if(!Read(&lNum, sizeof(lNum)))
      return false;

   SwapShort(lNum);
   num = lNum;
   return true;
}

//
// CInBuffer::ReadUint8
//
// Read a uint8 value from input file.
//
boolean CInBuffer::ReadUint8(uint8_t &num)
{
   if(idx == readlen) // nothing left in the buffer?
   {
      if(!ReadFile()) // try to read more
      {
         if(!readlen) // nothing was read? uh oh!
            return false;
      }
   }

   num = buffer[idx];
   ++idx;
    return true;
}

//
// CInBuffer::ReadSint8
//
// Read an int8 value from input file.
//
boolean CInBuffer::ReadSint8(int8_t &num)
{
   if(idx == readlen) // nothing left in the buffer?
   {
      if(!ReadFile()) // try to read more
      {
         if(!readlen) // nothing was read? uh oh!
            return false;
      }
   }

   num = (int8_t)(buffer[idx]);
   ++idx;
    return true;
}

// EOF

