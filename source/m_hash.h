//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Hash routines for data integrity and cryptographic purposes.
// Authors: James Haley
//

#ifndef M_HASH_H__
#define M_HASH_H__

#include "doomtype.h"

class HashData
{
public:
   enum hashtype_e
   {
      CRC32,
      ADLER32,
      MD5,
      SHA1,
      NUMHASHTYPES
   };

   static const int numdigest;

protected:
   uint32_t digest[5];   // the hash of the message (not all are always used)
   uint32_t messagelen;  // total length of message processed so far
   uint8_t  message[64]; // current 512-bit message block
   int      messageidx;  // current index into message (accumulate until == 64)
   bool     gonebad;     // an error occurred in the computation
   hashtype_e type;      // algorithm being used for computation   

   friend class CRC32Hash;
   friend class Adler32Hash;
   friend class MD5Hash;
   friend class SHA1Hash;

public:
   HashData();
   explicit HashData(hashtype_e pType);
   HashData(hashtype_e pType, const uint8_t *data, uint32_t size, 
            bool doWrapUp = true);

   void      initialize(hashtype_e type);
   void      addData(const uint8_t *data, uint32_t size);
   void      wrapUp();
   bool      compare(const HashData &other) const;
   void      stringToDigest(const char *str);
   char     *digestToString() const;
   uint32_t  getDigestPart(int i) const { return digest[i]; }

   bool  operator == (const HashData &other) const { return compare(other); }
};

#endif

// EOF

