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
//   Buffered file output.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "m_buffer.h"
#include "m_swap.h"

//
// COutBuffer::CreateFile
//
// Opens a file for buffered binary output with the given filename. The buffer
// size is determined by the len parameter.
//
boolean COutBuffer::CreateFile(const char *filename, unsigned int len, int endian)
{
   if(!(this->f = fopen(filename, "wb")))
      return false;

   this->buffer = (byte *)(calloc(len, sizeof(byte)));

   this->len = len;
   this->idx = 0;

   this->endian = endian;

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
      if(fwrite(buffer, sizeof(byte), idx, f) < (size_t)idx)
         return false;

      idx = 0;
   }

   return true;
}

//
// COutBuffer::Close
//
// Closes the output file, writing any pending data in the buffer first.
// The output buffer is also freed.
//
void COutBuffer::Close()
{
   if(this->f)
   {
      this->Flush();
      fclose(this->f);
      f = NULL;
   }

   this->idx = 0;
   this->len = 0;

   if(this->buffer)
   {
      free(this->buffer);
      this->buffer = NULL;
   }
}

//
// COutBuffer::Tell
//
// Gives the current file offset, minus any data that might be currently
// pending in the output buffer. Call M_BufferFlush first if you need an
// absolute file offset.
//
long COutBuffer::Tell()
{
   return ftell(this->f);
}

//
// COutBuffer::Write
//
// Buffered writing function.
//
boolean COutBuffer::Write(const void *data, unsigned int size)
{
   const byte *src = (const byte *)data;
   unsigned int writeAmt;
   unsigned int bytesToWrite = size;

   while(bytesToWrite)
   {
      writeAmt = this->len - this->idx;
      
      if(!writeAmt)
      {
         if(!this->Flush())
            return false;
         writeAmt = this->len;
      }

      if(bytesToWrite < writeAmt)
         writeAmt = bytesToWrite;

      memcpy(&(this->buffer[this->idx]), src, writeAmt);

      this->idx += writeAmt;
      src += writeAmt;
      bytesToWrite -= writeAmt;
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
   switch(this->endian)
   {
   case LENDIAN:
      num = SwapULong(num);
      break;
   case BENDIAN:
      num = SwapBigULong(num);
      break;
   default:
      break;
   }
   return this->Write(&num, sizeof(uint32_t));
}

//
// COutBuffer::WriteUint16
//
// Convenience routine to write an unsigned short int into the buffer.
//
boolean COutBuffer::WriteUint16(uint16_t num)
{
   switch(this->endian)
   {
   case LENDIAN:
      num = SwapUShort(num);
      break;
   case BENDIAN:
      num = SwapBigUShort(num);
      break;
   default:
      break;
   }
   return this->Write(&num, sizeof(uint16_t));
}

//
// COutBuffer::WriteUint8
//
// Routine to write an unsigned byte into the buffer.
// This is much more efficient than calling M_BufferWrite for individual bytes.
//
boolean COutBuffer::WriteUint8(uint8_t num)
{     
   if(this->idx == this->len)
   {
      if(!this->Flush())
         return false;
   }

   this->buffer[this->idx] = num;
   this->idx++;
 
   return true;
}

// EOF

