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
// M_BufferCreateFile
//
// Opens a file for buffered binary output with the given filename. The buffer
// size is determined by the len parameter.
//
boolean M_BufferCreateFile(outbuffer_t *ob, const char *filename, 
                           unsigned int len, int endian)
{
   if(!(ob->f = fopen(filename, "wb")))
      return false;

   ob->buffer = calloc(len, sizeof(byte));

   ob->len = len;
   ob->idx = 0;

   ob->endian = endian;

   return true;
}

//
// M_BufferFlush
//
// Call to flush the contents of the buffer to the output file. This will be
// called automatically before the file is closed, but must be called explicitly
// if a current file offset is needed. Returns false if an IO error occurs.
//
boolean M_BufferFlush(outbuffer_t *ob)
{
   if(ob->idx)
   {
      if(fwrite(ob->buffer, sizeof(byte), ob->idx, ob->f) < (size_t)ob->idx)
         return false;

      ob->idx = 0;
   }

   return true;
}

//
// M_BufferClose
//
// Closes the output file, writing any pending data in the buffer first.
// The output buffer is also freed.
//
void M_BufferClose(outbuffer_t *ob)
{
   M_BufferFlush(ob);
   fclose(ob->f);
   free(ob->buffer);

   memset(ob, 0, sizeof(outbuffer_t));
}

//
// M_BufferTell
//
// Gives the current file offset, minus any data that might be currently
// pending in the output buffer. Call M_BufferFlush first if you need an
// absolute file offset.
//
long M_BufferTell(outbuffer_t *ob)
{
   return ftell(ob->f);
}

//
// M_BufferWrite
//
// Buffered writing function.
//
boolean M_BufferWrite(outbuffer_t *ob, const void *data, unsigned int size)
{
   const byte *src = data;
   unsigned int writeAmt;
   unsigned int bytesToWrite = size;

   while(bytesToWrite)
   {
      writeAmt = ob->len - ob->idx;
      
      if(!writeAmt)
      {
         if(!M_BufferFlush(ob))
            return false;
         writeAmt = ob->len;
      }

      if(bytesToWrite < writeAmt)
         writeAmt = bytesToWrite;

      memcpy(&(ob->buffer[ob->idx]), src, writeAmt);

      ob->idx += writeAmt;
      src += writeAmt;
      bytesToWrite -= writeAmt;
   }

   return true;
}

//
// M_BufferWriteUint32
//
// Convenience routine to write an unsigned integer into the buffer.
//
boolean M_BufferWriteUint32(outbuffer_t *ob, uint32_t num)
{
   switch(ob->endian)
   {
   case OUTBUFFER_LENDIAN:
      num = SwapULong(num);
      break;
   case OUTBUFFER_BENDIAN:
      num = SwapBigULong(num);
      break;
   default:
      break;
   }
   return M_BufferWrite(ob, &num, sizeof(uint32_t));
}

//
// M_BufferWriteUint16
//
// Convenience routine to write an unsigned short int into the buffer.
//
boolean M_BufferWriteUint16(outbuffer_t *ob, uint16_t num)
{
   switch(ob->endian)
   {
   case OUTBUFFER_LENDIAN:
      num = SwapUShort(num);
      break;
   case OUTBUFFER_BENDIAN:
      num = SwapBigUShort(num);
      break;
   default:
      break;
   }
   return M_BufferWrite(ob, &num, sizeof(uint16_t));
}

//
// M_BufferWriteUint8
//
// Routine to write an unsigned byte into the buffer.
// This is much more efficient than calling M_BufferWrite for individual bytes.
//
boolean M_BufferWriteUint8(outbuffer_t *ob, uint8_t num)
{     
   if(ob->idx == ob->len)
   {
      if(!M_BufferFlush(ob))
         return false;
   }

   ob->buffer[ob->idx] = num;
   ob->idx++;
 
   return true;
}

// EOF

