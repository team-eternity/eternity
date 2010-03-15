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

#include "z_zone.h"
#include "doomtype.h"
#include "m_hash.h"

//
// Shared Macros
//

#define leftrotate(w, b) (((w) << (b)) | ((w) >> (32 - (b))))

//=============================================================================
//
// CRC32 - For IEEE Types
//

#define CRC32_IEEE_POLY 0xEDB88320

static uint32_t crc32_table[256];

//
// M_CRC32BuildTable
//
// Builds the polynomial table for CRC32.
//
static void M_CRC32BuildTable(void)
{
   uint32_t i, j;
   
   for(i = 0; i < 256; ++i)
   {
      uint32_t val = i;
      
      for(j = 0; j < 8; ++j)
      {
         if(val & 1)
            val = (val >> 1) ^ CRC32_IEEE_POLY;
         else
            val >>= 1;
      }
      
      crc32_table[i] = val;
   }
}

//
// M_CRC32Initialize
//
// Builds the table if it hasn't been built yet. Doesn't do anything
// else, since CRC32 starting value has already been set properly by
// the main hash init routine.
//
static void M_CRC32Initialize(hashdata_t *hash)
{
   static boolean tablebuilt = false;
   
   // build the CRC32 table if it hasn't been built yet
   if(!tablebuilt)
   {
      M_CRC32BuildTable();
      tablebuilt = true;
   }
   
   // zero is the appropriate starting value for CRC32, so we need do nothing
   // special here
}

//
// M_CRC32HashData
//
// Calculates a running CRC32 for the provided block of data.
//
static void M_CRC32HashData(hashdata_t *hash, uint8_t *data, uint32_t len)
{
   uint32_t crc = hash->digest[0];
   
   crc ^= 0xFFFFFFFF;
   
   while(len)
   {
      uint8_t idx = (uint8_t)(((int)crc ^ *data++) & 0xff);
      
      crc = crc32_table[idx] ^ (crc >> 8);
      
      --len;
   }
   
   hash->digest[0] = crc ^ 0xFFFFFFFF;
}

static void M_CRC32WrapUp(hashdata_t *hash)
{
   // Nothing required.
}

//=============================================================================
//
// Adler-32 - For Mark Adler
//

#define MOD_ADLER 65521

//
// M_Adler32Initialize
//
// Sets the digest to the starting value of 1.
//
static void M_Adler32Initialize(hashdata_t *hash)
{
   hash->digest[0] = 1;
}

//
// M_Adler32HashData
//
// Calculates a running Adler32 checksum for the provided block of data.
//
static void M_Adler32HashData(hashdata_t *hash, uint8_t *data, uint32_t len)
{
   uint32_t a, b, idx;

   a = hash->digest[0] & 0xFFFF;
   b = (hash->digest[0] >> 16) & 0xFFFF;
         
   for(idx = 0; idx < len; ++idx)
   {
      a = (a + data[idx]) % MOD_ADLER;
      b = (b + a) % MOD_ADLER;
   }
   
   hash->digest[0] = (b << 16) | a;
}

static void M_Adler32WrapUp(hashdata_t *data)
{
   // Nothing required.
}

//=============================================================================
//
// MD5 - For Korean hax0rz ^________^ kekeke
//

// md5_r specifies per-round shift amounts
static int md5_r[64] =
{
   7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
   5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
   4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
   6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
};

// md5_k specifies standard constants (precomputed)
static uint32_t md5_k[64] =
{
   0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 
   0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
   0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,

   0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x2441453,
   0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
   0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 

   0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 
   0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x4881d05,
   0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,

   0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
   0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
   0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

//
// M_MD5ProcessBlock
//
// Called when the MD5 hash has accumulated 512 bits of input.
// This is the main block "cipher" algorithm.
//
static void M_MD5ProcessBlock(hashdata_t *hash)
{
   int i;
   uint32_t temp;   
   uint32_t work[16];   // 16-word workspace
   uint32_t a, b, c, d; // current hash for chunk
   uint32_t f, g;
   uint8_t  *message = hash->message;

   // Step 1: turn the 64 bytes into 16 dwords
   for(i = 0; i < 16; ++i)
   {
      work[i]  = ((uint32_t)message[i * 4    ]);
      work[i] |= ((uint32_t)message[i * 4 + 1]) <<  8;
      work[i] |= ((uint32_t)message[i * 4 + 2]) << 16;
      work[i] |= ((uint32_t)message[i * 4 + 3]) << 24;
   }
   
   // hash for this chunk starts out as current message hash
   a = hash->digest[0];
   b = hash->digest[1];
   c = hash->digest[2];
   d = hash->digest[3];
   
   // Step 2: compute hash
   
   for(i = 0; i < 16; ++i)
   {
      f = (b & c) | (~b & d);
      g = i;
      
      temp = d;
      d = c;
      c = b;
      b = b + leftrotate((a + f + md5_k[i] + work[g]), md5_r[i]);
      a = temp;
   }
   
   for(; i < 32; ++i)
   {
      f = (d & b) | (~d & c);
      g = (5 * i + 1) % 16;
      
      temp = d;
      d = c;
      c = b;
      b = b + leftrotate((a + f + md5_k[i] + work[g]), md5_r[i]);
      a = temp;
   }
   
   for(; i < 48; ++i)
   {
      f = b ^ c ^ d;
      g = (3 * i + 5) % 16;
      
      temp = d;
      d = c;
      c = b;
      b = b + leftrotate((a + f + md5_k[i] + work[g]), md5_r[i]);
      a = temp;
   }
   
   for(; i < 64; ++i)
   {
      f = c ^ (b | ~d);
      g = (7 * i) % 16;
      
      temp = d;
      d = c;
      c = b;
      b = b + leftrotate((a + f + md5_k[i] + work[g]), md5_r[i]);
      a = temp;
   }
   
   // Step 4: add chunk hash to message hash
   hash->digest[0] += a;
   hash->digest[1] += b;
   hash->digest[2] += c;
   hash->digest[3] += d;
   
   // current buffered block has been digested, so clear the index
   hash->messageidx = 0;
}


//
// M_MD5Initialize
//
// Gets an MD5 hash operation ready to run
//
static void M_MD5Initialize(hashdata_t *hash)
{
   // initialize the digest with the standard initialization vector
   hash->digest[0] = 0x67452301;
   hash->digest[1] = 0xEFCDAB89;
   hash->digest[2] = 0x98BADCFE;
   hash->digest[3] = 0x10325476;
}

//
// M_MD5HashData
//
// Given data, this function will buffer it in the md5hash object until
// 512 bits of input have been accumulated, and then it will compute the
// hash for that block. If data is > 512 bits, it will be processed 512
// bits at a time. Call this routine until your input is exhausted.
//
static void M_MD5HashData(hashdata_t *hash, uint8_t *data, uint32_t len)
{
   if(!len || hash->gonebad)
      return;
   
   while(len--)
   {
      hash->message[hash->messageidx++] = *data;
      
      // added 8 bits
      hash->messagelen += 8;
      
      // If messagelen wraps to 0, the message is too long.
      // This implementation supports a maximum message length of 512 MB.
      if(!hash->messagelen)
      {
         hash->gonebad = true;
         break;
      }
      
      // when 512 bits of message is accumulated, process it
      if(hash->messageidx == 64)
         M_MD5ProcessBlock(hash);
      
      ++data;
   }   
}

//
// M_MD5WrapUp
//
// Finishes up the MD5 hash algorithm by padding the message as needed and
// completing the hash computation on any data left in the message buffer.
//
static void M_MD5WrapUp(hashdata_t *hash)
{
   // The message must be padded out to an even multiple of 512 bits, and the
   // padding must include an initial "1" bit followed by zeroes, and then 
   // wrapped up with 8 bytes representing the length of the message. However,
   // this implementation uses a 32-bit length, so the first 4 bytes of the
   // message length are also always zero.
   
   // This means we need at least 9 bytes available in the buffer. If there
   // are not 9 bytes available, we need to pad out the current block and then
   // continue padding into a new block.
   
   if(hash->messageidx > 64 - 9)
   {
      // begin the padding in the current block; add a 1 bit at the start...
      hash->message[hash->messageidx++] = 0x80;
      
      // ... and fill the rest with 0
      while(hash->messageidx < 64)
         hash->message[hash->messageidx++] = 0;
         
      // we filled the current block, so process it
      M_MD5ProcessBlock(hash);
      
      // finish padding in the new block with zeroes
      while(hash->messageidx <= 64 - 5)
         hash->message[hash->messageidx++] = 0;
   }
   else
   {
      // we have enough room in the current block; add a 1 bit at the start...
      hash->message[hash->messageidx++] = 0x80;
      
      // .. and fill the rest with 0, up to the space needed for the msg length
      while(hash->messageidx <= 64 - 5)
         hash->message[hash->messageidx++] = 0;
   }
   
   // fill the remainder of the final block with the encoded length
   hash->message[60] = (uint8_t)((hash->messagelen      ) & 0xFF);
   hash->message[61] = (uint8_t)((hash->messagelen >>  8) & 0xFF);
   hash->message[62] = (uint8_t)((hash->messagelen >> 16) & 0xFF);
   hash->message[63] = (uint8_t)((hash->messagelen >> 24) & 0xFF);
   
   // process the final padded block.
   M_MD5ProcessBlock(hash);
}


//=============================================================================
//
// SHA-1 - For Men in Black
//

//
// M_SHA1ProcessBlock
//
// Called when the SHA1 hash has accumulated 512 bits of input.
// This is the main block "cipher" algorithm.
//
static void M_SHA1ProcessBlock(hashdata_t *hash)
{
   int i;
   uint32_t temp;   
   uint32_t work[80];      // 80-word extension workspace
   uint32_t a, b, c, d, e; // current hash for chunk
   uint32_t f;
   uint8_t  *message = hash->message;

   // Step 1: turn the 64 bytes into 16 dwords
   for(i = 0; i < 16; ++i)
   {
      work[i]  = ((uint32_t)message[i * 4    ]) << 24;
      work[i] |= ((uint32_t)message[i * 4 + 1]) << 16;
      work[i] |= ((uint32_t)message[i * 4 + 2]) <<  8;
      work[i] |= ((uint32_t)message[i * 4 + 3]);
   }

   // Step 2: extend the message with XOR shuffling
   for(; i < 80; ++i)
   {
      temp = work[i - 3] ^ work[i - 8] ^ work[i - 14] ^ work[i - 16];
      
      work[i] = leftrotate(temp, 1);
   }
   
   // hash for this chunk starts out as current message hash
   a = hash->digest[0];
   b = hash->digest[1];
   c = hash->digest[2];
   d = hash->digest[3];
   e = hash->digest[4];
   
   // Step 3: compute hash
   
   for(i = 0; i < 20; ++i)
   {
      f = (b & c) | (~b & d);
      
      temp = leftrotate(a, 5) + f + e + 0x5A827999 + work[i];
      e = d;
      d = c;
      c = leftrotate(b, 30);
      b = a;
      a = temp;
   }
   
   for(; i < 40; ++i)   
   {
      f = b ^ c ^ d;
      
      temp = leftrotate(a, 5) + f + e + 0x6ED9EBA1 + work[i];
      e = d;
      d = c;
      c = leftrotate(b, 30);
      b = a;
      a = temp;
   }
   
   for(; i < 60; ++i)
   {
      f = (b & c) | (b & d) | (c & d);
      
      temp = leftrotate(a, 5) + f + e + 0x8F1BBCDC + work[i];
      e = d;
      d = c;
      c = leftrotate(b, 30);
      b = a;
      a = temp;
   }
   
   for(; i < 80; ++i)
   {
      f = b ^ c ^ d;
      
      temp = leftrotate(a, 5) + f + e + 0xCA62C1D6 + work[i];
      e = d;
      d = c;
      c = leftrotate(b, 30);
      b = a;
      a = temp;
   }
   
   // Step 4: add chunk hash to message hash
   hash->digest[0] += a;
   hash->digest[1] += b;
   hash->digest[2] += c;
   hash->digest[3] += d;
   hash->digest[4] += e;
   
   // current buffered block has been digested, so clear the index
   hash->messageidx = 0;
}

//
// M_SHA1Initialize
//
// Gets a SHA1 hash operation ready to run
//
static void M_SHA1Initialize(hashdata_t *hash)
{
   // initialize the digest with the standard initialization vector
   hash->digest[0] = 0x67452301;
   hash->digest[1] = 0xEFCDAB89;
   hash->digest[2] = 0x98BADCFE;
   hash->digest[3] = 0x10325476;
   hash->digest[4] = 0xC3D2E1F0;
}

//
// M_SHA1HashData
//
// Given data, this function will buffer it in the sha1hash object until
// 512 bits of input have been accumulated, and then it will compute the
// hash for that block. If data is > 512 bits, it will be processed 512
// bits at a time. Call this routine until your input is exhausted.
//
static void M_SHA1HashData(hashdata_t *hash, uint8_t *data, uint32_t len)
{
   if(!len || hash->gonebad)
      return;
   
   while(len--)
   {
      hash->message[hash->messageidx++] = *data;
      
      // added 8 bits
      hash->messagelen += 8;
      
      // If messagelen wraps to 0, the message is too long.
      // This implementation supports a maximum message length of 512 MB.
      if(!hash->messagelen)
      {
         hash->gonebad = true;
         break;
      }
      
      // when 512 bits of message is accumulated, process it
      if(hash->messageidx == 64)
         M_SHA1ProcessBlock(hash);
      
      ++data;
   }   
}

//
// M_SHA1WrapUp
//
// Finishes up the SHA-1 hash algorithm by padding the message as needed and
// completing the hash computation on any data left in the message buffer.
//
static void M_SHA1WrapUp(hashdata_t *hash)
{
   // The message must be padded out to an even multiple of 512 bits, and the
   // padding must include an initial "1" bit followed by zeroes, and then 
   // wrapped up with 8 bytes representing the length of the message. However,
   // this implementation uses a 32-bit length, so the first 4 bytes of the
   // message length are also always zero.
   
   // This means we need at least 9 bytes available in the buffer. If there
   // are not 9 bytes available, we need to pad out the current block and then
   // continue padding into a new block.
   
   if(hash->messageidx > 64 - 9)
   {
      // begin the padding in the current block; add a 1 bit at the start...
      hash->message[hash->messageidx++] = 0x80;
      
      // ... and fill the rest with 0
      while(hash->messageidx < 64)
         hash->message[hash->messageidx++] = 0;
         
      // we filled the current block, so process it
      M_SHA1ProcessBlock(hash);
      
      // finish padding in the new block with zeroes
      while(hash->messageidx <= 64 - 5)
         hash->message[hash->messageidx++] = 0;
   }
   else
   {
      // we have enough room in the current block; add a 1 bit at the start...
      hash->message[hash->messageidx++] = 0x80;
      
      // .. and fill the rest with 0, up to the space needed for the msg length
      while(hash->messageidx <= 64 - 5)
         hash->message[hash->messageidx++] = 0;
   }
   
   // fill the remainder of the final block with the encoded length
   hash->message[60] = (uint8_t)((hash->messagelen >> 24) & 0xFF);
   hash->message[61] = (uint8_t)((hash->messagelen >> 16) & 0xFF);
   hash->message[62] = (uint8_t)((hash->messagelen >>  8) & 0xFF);
   hash->message[63] = (uint8_t)((hash->messagelen      ) & 0xFF);
   
   // process the final padded block.
   M_SHA1ProcessBlock(hash);
}

//=============================================================================
//
// Generic Hashing Interface
//

typedef struct hashalgorithm_s
{
   void (*Initialize)(hashdata_t *);
   void (*HashData)(hashdata_t *, uint8_t *, uint32_t);
   void (*WrapUp)(hashdata_t *);
   int numdigest;
} hashalgorithm_t;


// Algorithms - They're Grrrreat!
static hashalgorithm_t HashAlgorithms[HASH_NUMTYPES] =
{
   { M_CRC32Initialize,   M_CRC32HashData,   M_CRC32WrapUp,   1 }, // CRC32
   { M_Adler32Initialize, M_Adler32HashData, M_Adler32WrapUp, 1 }, // Adler-32
   { M_MD5Initialize,     M_MD5HashData,     M_MD5WrapUp,     4 }, // MD5
   { M_SHA1Initialize,    M_SHA1HashData,    M_SHA1WrapUp,    5 }, // SHA-1
};

//
// M_HashInitialize
//
// Call this to start up the hash algorithm of your choice, indicated via
// the "type" parameter.
//
void M_HashInitialize(hashdata_t *hash, hashtype_e type)
{
   memset(hash, 0, sizeof(hashdata_t));
   
   hash->type = type;
   
   HashAlgorithms[type].Initialize(hash);
}

//
// M_HashData
//
// Call this to add the provided data to the calculation of the message
// digest. You can call this with all your data in one chunk, or call it
// repeatedly until data provided in smaller chunks is exhausted.
//
void M_HashData(hashdata_t *hash, uint8_t *data, uint32_t size)
{
   HashAlgorithms[hash->type].HashData(hash, data, size);
}

//
// M_HashWrapUp
//
// Call this to end your digest calculation. Some algorithms don't
// actually require you to call this: namely, CRC32 and Adler-32.
//
void M_HashWrapUp(hashdata_t *hash)
{
   HashAlgorithms[hash->type].WrapUp(hash);
}

//
// M_HashCompare
//
// This routine will compare two digests. The algorithms must be the same
// or the comparison will evaluate as false.
//
boolean M_HashCompare(hashdata_t *h1, hashdata_t *h2)
{
   boolean ret = false;
   
   if(h1->type == h2->type)
   {
      int i, nummatch = 0;
      
      for(i = 0; i < HashAlgorithms[h1->type].numdigest; ++i)
         if(h1->digest[i] == h2->digest[i])
            ++nummatch;
            
      if(nummatch == i)
         ret = true;
   }
   
   return ret;
}

//=============================================================================
//
// String Digest Conversion Routines
//
// We do not resort to libc functions for the conversions to or from string,
// as printf-style functions are slow, and other string<->number conversion
// functions are often wrong when it comes to dealing with such problems as
// leading zeroes or signed-vs-unsigned issues.
//

#define HexCharToNum(c) \
   (uint32_t)(((c) >= '0' && (c) <= '9') ? (c) - '0' : \
              ((c) >= 'a' && (c) <= 'f') ? ((c) - 'a') + 10 : \
              ((c) >= 'A' && (c) <= 'F') ? ((c) - 'A') + 10 : 0)

//
// M_HashStrToDigest
//
// This routine will convert a given hex string into a hash digest, regardless
// of the algorithm by which it was created.
//
void M_HashStrToDigest(hashdata_t *hash, const char *str)
{
   int digestidx = 0;
   int digestshift = 28;
   
   memset(hash->digest, 0, NUMDIGEST * sizeof(*(hash->digest)));
   
   while(*str)
   {
      if(digestidx >= NUMDIGEST)
         return;
         
      hash->digest[digestidx] |= (HexCharToNum(*str) << digestshift);
      digestshift -= 4;

      if(digestshift < 0) // step to next digest
      {
         ++digestidx;
         digestshift = 28;
      }
   
      ++str;
   }   
}

//
// M_HashDigestToStr
//
// This routine will allocate and return a string holding the message digest
// from the hashdata object.
//
char *M_HashDigestToStr(hashdata_t *hash)
{
   char *buffer, *c;
   size_t size = 0;
   int numdigest;
   int i;
   
   numdigest = HashAlgorithms[hash->type].numdigest;
   size = (8 * numdigest + 1) * sizeof(char);
   
   c = buffer = calloc(1, size);
   
   for(i = 0; i < numdigest; ++i)
   {
      int shift;
      
      for(shift = 28; shift >= 0; shift -= 4)
         *c++ = "0123456789abcdef"[(hash->digest[i] >> shift) & 0x0F];
   }
   
   return buffer;
}

// EOF

