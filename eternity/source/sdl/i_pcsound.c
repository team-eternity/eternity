// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2008 James Haley, Stephen McGranahan, Simon Howard, et al.
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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//      PC Speaker Sound
//
//      Largely derived from Chocolate Doom.
//
//-----------------------------------------------------------------------------

#if 0 // not finished

#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_thread.h"
#include "SDL_mixer.h"

#include "../z_zone.h"
#include "../sounds.h"
#include "../i_sound.h"
#include "../psnprntf.h"

//=============================================================================
//
// Static Data
//

static boolean pcs_initialised = false;

static SDL_mutex *sound_lock;

static Uint8 *current_sound_lump = NULL;
static Uint8 *current_sound_pos = NULL;
static unsigned int current_sound_remaining = 0;
static int current_sound_handle = 0;
static int current_sound_lump_num = -1;

static const float frequencies[] = 
{
       0.0f,  175.00f,  180.02f,  185.01f,  190.02f,  196.02f,  202.02f,  208.01f,
    214.02f,  220.02f,  226.02f,  233.04f,  240.02f,  247.03f,  254.03f,  262.00f,
    269.03f,  277.03f,  285.04f,  294.03f,  302.07f,  311.04f,  320.05f,  330.06f,
    339.06f,  349.08f,  359.06f,  370.09f,  381.08f,  392.10f,  403.10f,  415.01f,
    427.05f,  440.12f,  453.16f,  466.08f,  480.15f,  494.07f,  508.16f,  523.09f,
    539.16f,  554.19f,  571.17f,  587.19f,  604.14f,  622.09f,  640.11f,  659.21f,
    679.10f,  698.17f,  719.21f,  740.18f,  762.41f,  784.47f,  807.29f,  831.48f,
    855.32f,  880.57f,  906.67f,  932.17f,  960.69f,  988.55f, 1017.20f, 1046.64f,
   1077.85f, 1109.93f, 1141.79f, 1175.54f, 1210.12f, 1244.19f, 1281.61f, 1318.43f,
   1357.42f, 1397.16f, 1439.30f, 1480.37f, 1523.85f, 1569.97f, 1614.58f, 1661.81f,
   1711.87f, 1762.45f, 1813.34f, 1864.34f, 1921.38f, 1975.46f, 2036.14f, 2093.29f,
   2157.64f, 2217.80f, 2285.78f, 2353.41f, 2420.24f, 2490.98f, 2565.97f, 2639.77f
};

#define NUMFREQUENCIES (sizeof(frequencies) / sizeof(float))

//=============================================================================
//
// Support Routines
//

static void PCSCallbackFunc(int *duration, int *freq)
{
   unsigned int tone;
   
   *duration = 1000 / 140;
   
   if(SDL_LockMutex(sound_lock) < 0)
   {
      *freq = 0;
      return;
   }
   
   if(current_sound_lump != NULL && current_sound_remaining > 0)
   {
      // Read the next tone
      
      tone = *current_sound_pos;
      
      // Use the tone -> frequency lookup table.  See pcspkr10.zip
      // for a full discussion of this.
      // Check we don't overflow the frequency table.
      
      if(tone < NUMFREQUENCIES)
      {
         *freq = (int) frequencies[tone];
      }
      else
      {
         *freq = 0;
      }
      
      ++current_sound_pos;
      --current_sound_remaining;
   }
   else
      *freq = 0;
   
   SDL_UnlockMutex(sound_lock);
}

static int I_PCSGetSfxLumpNum(sfxinfo_t *sfx)
{
   int lumpnum = -1;
   char soundName[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
   boolean nameToTry = false;

   // 1. use explicit PC speaker sound name if provided
   // 2. if sound name is prefixed, use DP%s
   // 3. if sound name starts with DS, replace DS with DP
   // Otherwise, there is no locatable PC speaker sound for this effect

   if(sfx->pcslump[0] != '\0')
   {
      nameToTry = true;
      psnprintf(soundName, 9, "%s", sfx->pcslump);
   }
   else if(sfx->prefix)
   {
      nameToTry = true;
      psnprintf(soundName, 9, "DP%s", sfx->name);
   }
   else if(toupper(sfx->name[0]) == 'D' && toupper(sfx->name[1]) == 'S')
   {
      nameToTry = true;
      psnprintf(soundName, 9, "DP%s", sfx->name + 2);
   }

   if(nameToTry)
      lumpnum = W_CheckNumForName(soundName);

   return lumpnum;
}

static boolean CachePCSLump(sfxinfo_t *sfx)
{
   int lumpnum;
   int lumplen;
   int headerlen;
   
   // Free the current sound lump back to the cache   
   if(current_sound_lump != NULL)
   {
      Z_ChangeTag(current_sound_lump_num, PU_CACHE);
      current_sound_lump = NULL;
   }
   
   // Load from WAD   
   lumpnum            = I_PCSGetSfxLumpNum(sfx);
   current_sound_lump = W_CacheLumpNum(lumpnum, PU_STATIC);
   lumplen            = W_LumpLength(lumpnum);
   
   // Read header   

   if(current_sound_lump[0] != 0x00 || current_sound_lump[1] != 0x00)
      return false;
   
   headerlen = (current_sound_lump[3] << 8) | current_sound_lump[2];
   
   if(headerlen > lumplen - 4)
      return false;
   
   // Header checks out ok   
   current_sound_remaining = headerlen;
   current_sound_pos       = current_sound_lump + 4;
   current_sound_lump_num  = lumpnum;
   
   return true;
}

//=============================================================================
//
// Driver Routines
//

static int I_PCSInitSound(void)
{
   return 0;
}

static void I_PCSCacheSound(sfxinfo_t *)
{
}

static void I_PCSUpdateSound(void)
{
}

static void I_PCSSubmitSound(void)
{
}

static void I_PCSShutdownSound(void)
{
}

static int I_PCSStartSound(sfxinfo_t *sfx, int cnum, int vol, int sep, 
                          int pitch, int pri, int loop)
{
   return 0;
}

static int I_PCSSoundID(int handle)
{
   return 0;
}

static void I_PCSStopSound(int handle)
{
}

static int I_PCSSoundIsPlaying(int handle)
{
   return 0;
}

static void I_PCSUpdateSoundParams(int handle, int vol, int sep, int pitch)
{
}

//
// Sound driver object
//

i_sounddriver_t i_pcsound_driver =
{
   I_PCSInitSound,         // InitSound
   I_PCSCacheSound,        // CacheSound
   I_PCSUpdateSound,       // UpdateSound
   I_PCSSubmitSound,       // SubmitSound
   I_PCSShutdownSound,     // ShutdownSound
   I_PCSStartSound,        // StartSound
   I_PCSSoundID,           // SoundID
   I_PCSStopSound,         // StopSound
   I_PCSSoundIsPlaying,    // SoundIsPlaying
   I_PCSUpdateSoundParams, // UpdateSoundParams
};

#endif

// EOF

