// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2009 James Haley
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
//   
//    Hash routines for data integrity and cryptographic purposes.
//
//-----------------------------------------------------------------------------

#ifndef M_HASH_H__
#define M_HASH_H__

typedef enum
{
   HASH_CRC32,
   HASH_ADLER32,
   HASH_MD5,
   HASH_SHA1,
   HASH_NUMTYPES
} hashtype_e;

#define NUMDIGEST 5

typedef struct hashdata_s
{
   uint32_t digest[NUMDIGEST]; // the hash of the message (not all are always used)
   uint32_t messagelen;        // total length of message processed so far
   uint8_t  message[64];       // current 512-bit message block
   int      messageidx;        // current index into message (accumulate until == 64)
   boolean  gonebad;           // an error occurred in the computation
   
   hashtype_e type;            // algorithm being used for computation   
} hashdata_t;

void    M_HashInitialize(hashdata_t *hash, hashtype_e type);
void    M_HashData(hashdata_t *hash, const uint8_t *data, uint32_t size);
void    M_HashWrapUp(hashdata_t *hash);
boolean M_HashCompare(hashdata_t *h1, hashdata_t *h2);
void    M_HashStrToDigest(hashdata_t *hash, const char *str);
char   *M_HashDigestToStr(hashdata_t *hash);

#endif

// EOF

