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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   Buffered file output.
//
//-----------------------------------------------------------------------------

#ifndef M_BUFFER_H__
#define M_BUFFER_H__

typedef struct outbuffer_s
{
   FILE *f;          // destination file
   byte *buffer;     // buffer
   unsigned int len; // total buffer length
   unsigned int idx; // current index
} outbuffer_t;

boolean M_BufferCreateFile(outbuffer_t *ob, const char *filename, 
                           unsigned int len);
boolean M_BufferFlush(outbuffer_t *ob);
void    M_BufferClose(outbuffer_t *ob);
long    M_BufferTell(outbuffer_t *ob);
boolean M_BufferWrite(outbuffer_t *ob, const void *data, unsigned int size);
void    M_BufferWriteUint32(outbuffer_t *ob, uint32_t num);
void    M_BufferWriteUint16(outbuffer_t *ob, uint16_t num);
boolean M_BufferWriteUint8(outbuffer_t *ob, uint8_t num);

#endif

// EOF


