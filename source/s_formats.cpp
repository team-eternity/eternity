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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Sound loading
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomtype.h"
#include "d_gi.h"
#include "m_binary.h"
#include "m_compare.h"
#include "m_swap.h"
#include "s_sound.h"
#include "w_wad.h"

// sample formats
enum sampleformat_e
{
   S_FMT_U8,
   S_FMT_16,
   S_FMT_24,
   S_FMT_32
};

struct sounddata_t
{
   unsigned int    samplerate;
   size_t          samplecount;
   byte           *samplestart;
   sampleformat_e  fmt;
};

//=============================================================================
//
// DMX Sample Format 0x03
//
// All DOOM-engine games use the DMX headered unsigned 8-bit mono PCM sample
// format.
//

#define DMX_SOUNDHDRSIZE 8

//
// S_checkDMXPadded
//
// Most canonical DMX samples are padded with 16 lead-in bytes and 16 lead-out
// bytes. All such samples with this property have the 17th byte (the first
// real sample) equal in value to the first 16 bytes (and likewise, the 17th
// byte from the end is repeated 16 more times).
//
// We'll consider the sample to be in canonical padded form if it passes the
// 17th byte from the start test.
//
static bool S_checkDMXPadded(byte *data)
{
   byte  testarray[16];
   byte *pcmstart = data + DMX_SOUNDHDRSIZE;
   byte *fsample  = pcmstart + 16;

   memset(testarray, *fsample, sizeof(testarray));

   return !memcmp(pcmstart, testarray, sizeof(testarray));
}

//
// S_isDMXSample
//
// Test against sane properties for DMX sample format 0x03:
// * Must have valid header
// * Minimum sample that will play must be > 32 bytes (rf. DSSMFIRE in strife1.wad)
// * Sanity check length and samplerate
// * Account for possible presence of DMX's DMA alignment padding bytes
//
static bool S_isDMXSample(byte *data, size_t len, sounddata_t &sd)
{
   // must have a header
   if(len < DMX_SOUNDHDRSIZE)
      return false;

   // Check the header, and ensure this is a valid sound
   if(data[0] != 0x03 || data[1] != 0x00)
      return false;

   byte *r = data + 2;
   sd.samplerate  = GetBinaryUWord(r);
   sd.samplecount = GetBinaryUDWord(r);

   if(!sd.samplerate)
      return false;

   // don't play samples that are less than one chunk in size, or state that
   // they are larger than the actual data source
   if(sd.samplecount <= 32 || sd.samplecount > len - DMX_SOUNDHDRSIZE)
      return false;

   sd.samplestart = data + DMX_SOUNDHDRSIZE;

   // check for padding
   if(S_checkDMXPadded(data))
   {
      // remove DMX padding bytes from length if the sample is padded, and advance
      // the sample pointer
      sd.samplecount -= 32;
      sd.samplestart += 16;
   }

   // always 8-bit unsigned samples
   sd.fmt = S_FMT_U8;

   return true;
}

//=============================================================================
//
// Wave
//
// There are a billion and one variants of the RIFF WAVE format. We accept only
// so-called canonical waves in either 8- or 16-bit PCM, mono only.
//

//
// S_isWaveSample
//
// * Need at least a 44-byte header and at least some sample information.
// * Must be a canonical wave, or have a known extended header format.
// * Currently 8- or 16-bit mono PCM only.
// * Samplerate and sample count are sanity-checked.
//
static bool S_isWaveSample(byte *data, size_t len, sounddata_t &sd)
{
   size_t minLength    = 44u; // current minimum required length
   int    factOffset   = 36;  // offset at which to check for a fact chunk
   int    dataOffset   = 36;  // offset at which to check for a data chunk
   int    sampleOffset = 44;  // offset at which to find samples
   int    totalMDSize  = 44;  // total size of file metadata

   // absolute minimum length check for smallest format
   if(len <= minLength)
      return false;

   // check RIFF chunk magic
   if(memcmp(data +  0, "RIFF", 4) || // not a RIFF
      memcmp(data +  8, "WAVE", 4) || // not a WAVE
      memcmp(data + 12, "fmt ", 4))   // no fmt information
      return false;

   byte *r = data + 4;
   size_t chunksize = GetBinaryUDWord(r);
   if(chunksize < len - 8) // need correct chunk size; will tolerate overage
      return false;

   r = data + 16;
   int headerSize = GetBinaryDWord(r);
   switch(headerSize)
   {
   case 16: // normal PCM header
      break; // no changes necessary
   case 18: // WAVEFORMATEX header
      minLength     = 46u;
      factOffset    = 38;
      dataOffset    = 38;
      sampleOffset  = 46;
      totalMDSize  +=  2;
      break;
   case 40: // EXTENSIBLE header
      minLength     = 68u;
      factOffset    = 60;
      dataOffset    = 60;
      sampleOffset  = 68;
      totalMDSize  += 22;
      break;
   default:
      return false; // unknown header size
   }

   // check again against possibly new minimum length requirement
   if(len <= minLength)
      return false;

   uint16_t fmt = GetBinaryUWord(r);
   if(fmt != 0x0001 && fmt != 0xFFFE) // PCM or EXTENSIBLE formats only
      return false;
   if(GetBinaryWord(r) != 1)    // mono only
      return false;

   sd.samplerate = GetBinaryUDWord(r);
   if(!sd.samplerate)
      return false;

   r = data + 34;
   int bps = GetBinaryWord(r);
   if(bps != 8 && bps != 16) // 8- or 16-bit only
      return false;

   switch(bps)
   {
   case 8:
      sd.fmt = S_FMT_U8;
      break;
   case 16:
      sd.fmt = S_FMT_16;
      break;
   // TODO: support 24- and 32-bit PCM?
   // TODO: support IEEE PCM?
   }

   // check for extended format information in larger headers
   if(headerSize > 16)
   {
      int cbSize = GetBinaryWord(r);
      if(cbSize != 0 && cbSize != 22)
         return false; // unknown cbSize
      if(cbSize == 22) // check for EXTENSIBLE subformat
      {
         if(headerSize != 40)
            return false; // illegal combination.
         r = data + 44;
         if(GetBinaryWord(r) != 1) // must be PCM
            return false;
      }
   }

   // check for presence of a fact chunk
   r = data + factOffset;
   if(!memcmp(r, "fact", 4))
   {
      minLength    += 12u;
      dataOffset   += 12;
      sampleOffset += 12;
      totalMDSize  += 12;

      // if there is a fact chunk, we need to check minimum length again
      if(len <= minLength)
         return false;
   }

   // check that data marker is at new position
   r = data + dataOffset;
   if(memcmp(r, "data", 4))
      return false; // no data marker

   r += 4;
   size_t bytes = GetBinaryUDWord(r);
   if(!bytes) // empty sample stream
      return false;

   if(bytes + totalMDSize > len) // truncated sample stream
      sd.samplecount = (len - totalMDSize) / (bps / 8);
   else
      sd.samplecount = bytes / (bps / 8);

   sd.samplestart = data + sampleOffset;

   return true;
}

//
// S_detectSoundFormat
//
// Tries the lump data against every supported sound format in a predetermined
// order. If true is returned, then sd contains information needed to transform
// the lump data into floating point PCM for playback. If false is returned,
// the contents of sd are undefined, and the sound should not be played.
//
static bool S_detectSoundFormat(sounddata_t &sd, byte *data, size_t len)
{
   // Is it a DMX digital sample?
   if(S_isDMXSample(data, len, sd))
      return true;

   // Is it a wave sample?
   if(S_isWaveSample(data, len, sd))
      return true;

   // not a supported format
   return false;
}

//=============================================================================
//
// PCM Conversion
//

#define TARGETSAMPLERATE 44100u

//
// S_alenForSample
//
// Calculate the "actual" sample length for a digital sound effect after
// conversion from its native samplerate to the samplerate used for output.
//
static unsigned int S_alenForSample(const sounddata_t &sd)
{
   return (unsigned int)(((uint64_t)sd.samplecount * TARGETSAMPLERATE) / sd.samplerate);
}

//
// S_convertPCMU8
//
// Convert unsigned 8-bit PCM to double precision floating point.
//
static void S_convertPCMU8(sfxinfo_t *sfx, const sounddata_t &sd)
{
   sfx->alen = S_alenForSample(sd);
   sfx->data = Z_Malloc(sfx->alen*sizeof(float), PU_STATIC, &sfx->data);

   // haleyjd 12/18/13: Convert sound to target samplerate and into floating
   // point samples.
   if(sfx->alen != sd.samplecount)
   {
      unsigned int i;
      float *dest = static_cast<float *>(sfx->data);
      byte  *src  = sd.samplestart;

      unsigned int step = (sd.samplerate << 16) / TARGETSAMPLERATE;
      unsigned int stepremainder = 0, j = 0;

      // do linear filtering operation
      for(i = 0; i < sfx->alen && j < sd.samplecount - 1; i++)
      {
         double d = (((unsigned int)src[j  ] * (0x10000 - stepremainder)) +
                     ((unsigned int)src[j+1] * stepremainder));
         d /= 65536.0;
         dest[i] = static_cast<float>(eclamp(d * 2.0 / 255.0 - 1.0, -1.0, 1.0));

         stepremainder += step;
         j += (stepremainder >> 16);

         stepremainder &= 0xffff;
      }
      // fill remainder (if any) with final sample byte
      for(; i < sfx->alen; i++)
         dest[i] = static_cast<float>(eclamp(src[j] * 2.0 / 255.0 - 1.0, -1.0, 1.0));
   }
   else
   {
      // sound is already at target samplerate, just convert to doubles
      float *dest = static_cast<float *>(sfx->data);
      byte  *src  = sd.samplestart;

      for(unsigned int i = 0; i < sfx->alen; i++)
         dest[i] = static_cast<float>(eclamp(src[i] * 2.0 / 255.0 - 1.0, -1.0, 1.0));
   }
}

//
// S_convertPCM16
//
// Convert signed 16-bit PCM to double precision floating point.
//
static void S_convertPCM16(sfxinfo_t *sfx, const sounddata_t &sd)
{
   sfx->alen = S_alenForSample(sd);
   sfx->data = Z_Malloc(sfx->alen*sizeof(float), PU_STATIC, &sfx->data);

   // haleyjd 12/18/13: Convert sound to target samplerate and into floating
   // point samples.
   if(sfx->alen != sd.samplecount)
   {
      unsigned int i;
      float   *dest = static_cast<float *>(sfx->data);
      int16_t *src  = reinterpret_cast<int16_t *>(sd.samplestart);

      unsigned int step = (sd.samplerate << 16) / TARGETSAMPLERATE;
      unsigned int stepremainder = 0, j = 0;

      // do linear filtering operation
      for(i = 0; i < sfx->alen && j < sd.samplecount - 1; i++)
      {
         double s1 = SwapShort(src[j]);
         double s2 = SwapShort(src[j+1]);
         double d  = (s1 * (0x10000 - stepremainder)) + (s2 * stepremainder);
         d /= 65536.0;
         dest[i] = static_cast<float>(eclamp((d + 32768.0) * 2.0 / 65535.0 - 1.0, -1.0, 1.0));

         stepremainder += step;
         j += (stepremainder >> 16);

         stepremainder &= 0xffff;
      }
      // fill remainder (if any) with final sample byte
      for(; i < sfx->alen; i++)
      {
         double s = SwapShort(src[j]);
         dest[i] = static_cast<float>(eclamp((s + 32768.0) * 2.0 / 65535.0 - 1.0, -1.0, 1.0));
      }
   }
   else
   {
      // sound is already at target samplerate, just convert to doubles
      float   *dest = static_cast<float *>(sfx->data);
      int16_t *src  = reinterpret_cast<int16_t *>(sd.samplestart);

      for(unsigned int i = 0; i < sfx->alen; i++)
      {
         double s = SwapShort(src[i]);
         dest[i] = static_cast<float>(eclamp((s + 32768.0) * 2.0 / 65535.0 - 1.0, -1.0, 1.0));
      }
   }
}

//=============================================================================
//
// Loading
//

//
// S_getSfxLumpNum
//
// Retrieve the raw data lump index for a given SFX name.
//
static int S_getSfxLumpNum(sfxinfo_t *sfx)
{
   char namebuf[16] = { 0 };

   // alison: check long file name
   if(sfx->lfn)
      return wGlobalDir.checkNumForLFNNSG(sfx->lfn, lumpinfo_t::ns_sounds);

   // haleyjd 09/03/03: determine whether to apply DS prefix to
   // name or not using new prefix flag
   if(sfx->flags & SFXF_PREFIX)
      psnprintf(namebuf, sizeof(namebuf), "DS%s", sfx->name);
   else
      strncpy(namebuf, sfx->name, 9);

   return wGlobalDir.checkNumForNameNSG(namebuf, lumpinfo_t::ns_sounds);
}

//=============================================================================
//
// Interface
//

//
// S_LoadDigitalSoundEffect
//
// Function to load supported digital sound effects from the WadDirectory.
// Returns true if sound is loaded and ready to play; false otherwise.
//
bool S_LoadDigitalSoundEffect(sfxinfo_t *sfx)
{
   bool  res = false;
   int   lump = S_getSfxLumpNum(sfx);

   // replace missing sounds with a reasonable default
   if(lump == -1)
      lump = wGlobalDir.getNumForNameNSG(GameModeInfo->defSoundName, lumpinfo_t::ns_sounds);

   size_t lumplen = (size_t)wGlobalDir.lumpLength(lump);
   if(!lumplen)
      return false;

   if(!sfx->data)
   {
      edefstructvar(sounddata_t, sd);
      byte *lumpdata = (byte *)wGlobalDir.cacheLumpNum(lump, PU_STATIC);

      if(S_detectSoundFormat(sd, lumpdata, lumplen))
      {
         switch(sd.fmt)
         {
         case S_FMT_U8:
            S_convertPCMU8(sfx, sd);
            res = true;
            break;
         case S_FMT_16:
            S_convertPCM16(sfx, sd);
            res = true;
            break;
         default: // unsupported PCM format
            break;
         }
      }

      // haleyjd 06/03/06: don't need original lump data any more if loaded
      Z_ChangeTag(lumpdata, PU_CACHE);
   }
   else
   {
      Z_ChangeTag(sfx->data, PU_STATIC); // mark as in-use
      res = true;
   }

   return res;
}

//
// S_CacheDigitalSoundLump
//
// Invoke when caching sound lumps at startup or after a wad load.
//
void S_CacheDigitalSoundLump(sfxinfo_t *sfx)
{
   int lump = S_getSfxLumpNum(sfx);

   // replace missing sounds with a reasonable default
   if(lump == -1)
      lump = wGlobalDir.getNumForNameNSG(GameModeInfo->defSoundName, lumpinfo_t::ns_sounds);

   wGlobalDir.cacheLumpNum(lump, PU_CACHE);
}

// EOF

