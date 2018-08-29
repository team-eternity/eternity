// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2008 James Haley, Stephen McGranahan, Simon Howard, et al.
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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//      PC Speaker Sound
//
//      Largely derived from Chocolate Doom.
//
//-----------------------------------------------------------------------------

#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_thread.h"

#include "../z_zone.h"
#include "../w_wad.h"
#include "../sounds.h"
#include "../i_sound.h"
#include "../psnprntf.h"

//=============================================================================
//
// PC Sound Library Routines
//
// haleyjd 11/12/08: I would just include fraggle's entire library, along with
// its support for native PC speaker under BSD and Linux, but I can't be
// bothered to fight with the Linux build system to make it work right now.
// So for now at least, this code is being included here directly.
//

typedef void (*pcsound_callback_func)(int *duration, int *frequency);

#define SQUARE_WAVE_AMP 0x2000

// Callback function to invoke when we want new sound data

static pcsound_callback_func callback;

// MaxW: 2018/08/27: ID of the open audio device
static SDL_AudioDeviceID devid;

// MaxW: 2018/08/27: Desired audiospec and the audiospec we actually have
static SDL_AudioSpec want, have;

// Currently playing sound
// current_remaining is the number of remaining samples that must be played
// before we invoke the callback to get the next frequency.

static int current_remaining;
static int current_freq;

static int phase_offset = 0;

static int pcsound_sample_rate;

static void PCSound_SetSampleRate(int rate)
{
    pcsound_sample_rate = rate;
}


//
// PCSound_Mix_Callback
//
// Mixer function that does the PC speaker emulation
//
static void PCSound_Mix_Callback(void *udata, Uint8 *stream, int len)
{
   Sint16 *leftptr;
   Sint16 *rightptr;
   Sint16 this_value;
   int oldfreq;
   int i;
   int nsamples;
   
   memset(stream, 0 , len);

   // Number of samples is quadrupled, because of 16-bit and stereo   
   nsamples = len / 4;
   
   leftptr = (Sint16 *) stream;
   rightptr = ((Sint16 *) stream) + 1;
    
   // Fill the output buffer   
   for(i = 0; i < nsamples; i++)
   {
      // Has this sound expired? If so, invoke the callback to get 
      // the next frequency.      
      while(current_remaining == 0) 
      {
         oldfreq = current_freq;
         
         // Get the next frequency to play
         
         callback(&current_remaining, &current_freq);
         
         if(current_freq != 0)
         {
            // Adjust phase to match to the new frequency.
            // This gives us a smooth transition between different tones,
            // with no impulse changes.            
            phase_offset = (phase_offset * oldfreq) / current_freq;
         }
         
         current_remaining = (current_remaining * have.freq) / 1000;
      }
      
      // Set the value for this sample.      
      if(current_freq == 0)
      {
         // Silence         
         this_value = 0;
      }
      else 
      {
         int frac;
         
         // Determine whether we are at a peak or trough in the current
         // sound.  Multiply by 2 so that frac % 2 will give 0 or 1 
         // depending on whether we are at a peak or trough.
         
         frac = (phase_offset * current_freq * 2) / have.freq;
         
         if((frac % 2) == 0) 
            this_value =  SQUARE_WAVE_AMP;
         else
            this_value = -SQUARE_WAVE_AMP;
         
         ++phase_offset;
      }
      
      --current_remaining;
      
      // Use the same value for the left and right channels.      
      *leftptr  += this_value;
      *rightptr += this_value;
      
      leftptr  += 2;
      rightptr += 2;
   }
}

static void PCSound_SDL_Shutdown()
{
   SDL_CloseAudio();
   SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

static int PCSound_SDL_Init(pcsound_callback_func callback_func)
{
   if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
   {
      fprintf(stderr, "Unable to set up sound.\n");
      return 0;
   }

   want.freq     = pcsound_sample_rate;
   want.format   = AUDIO_S16SYS;
   want.channels = 2;
   want.samples  = 1024;
   want.callback = PCSound_Mix_Callback;
   if (SDL_OpenAudio(&want, &have) < 0)
   {
      fprintf(stderr, "Error opening SDL audio: %s\n", SDL_GetError());

      SDL_QuitSubSystem(SDL_INIT_AUDIO);
      return 0;
   }

   SDL_PauseAudio(0);

    // Only supports AUDIO_S16SYS

    if (have.format != AUDIO_S16SYS || have.channels != 2)
    {
        fprintf(stderr,
                "PCSound_SDL only supports native signed 16-bit LSB, "
                "stereo format!\n");

        PCSound_SDL_Shutdown();
        return 0;
    }

    callback = callback_func;
    current_freq = 0;
    current_remaining = 0;

    return 1;
}


//=============================================================================
//
// Static Data
//

static bool pcs_initialised = false;

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

#define NUMFREQUENCIES earrlen(frequencies)

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
   bool nameToTry = false;

   // 1. use explicit PC speaker sound name if provided
   // 2. if sound name is prefixed, use DP%s
   // 3. if sound name starts with DS, replace DS with DP
   // Otherwise, there is no locatable PC speaker sound for this effect

   if(sfx->pcslump[0] != '\0')
   {
      nameToTry = true;
      psnprintf(soundName, 9, "%s", sfx->pcslump);
   }
   else if(sfx->flags & SFXF_PREFIX)
   {
      nameToTry = true;
      psnprintf(soundName, 9, "DP%s", sfx->name);
   }
   else if(ectype::toUpper(sfx->name[0]) == 'D' && 
           ectype::toUpper(sfx->name[1]) == 'S')
   {
      nameToTry = true;
      psnprintf(soundName, 9, "DP%s", sfx->name + 2);
   }

   if(nameToTry)
      lumpnum = W_CheckNumForName(soundName);

   return lumpnum;
}

static bool CachePCSLump(sfxinfo_t *sfx)
{
   int lumpnum;
   int lumplen;
   int headerlen;
   
   // Free the current sound lump back to the cache   
   if(current_sound_lump != NULL)
   {
      Z_ChangeTag(current_sound_lump, PU_CACHE);
      current_sound_lump = NULL;
   }
   
   // Load from WAD
   
   // haleyjd: check for validity
   if((lumpnum = I_PCSGetSfxLumpNum(sfx)) == -1)
      return false;

   current_sound_lump = (Uint8 *)(wGlobalDir.cacheLumpNum(lumpnum, PU_STATIC));
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

static int I_PCSInitSound()
{
   // Use the sample rate from the configuration file   
   PCSound_SetSampleRate(44100);
   
   // Initialise the PC speaker subsystem.
   
   pcs_initialised = !!PCSound_SDL_Init(PCSCallbackFunc);
   
   if(pcs_initialised)
   {
      sound_lock = SDL_CreateMutex();
      printf("Using PC speaker emulation\n");
   }
   else
      printf("Failed to initialize PC speaker emulation\n");
   
   return pcs_initialised;
}

static void I_PCSCacheSound(sfxinfo_t *sfx)
{
   // no-op
}

static void I_PCSUpdateSound()
{
   // no-op
}

static void I_PCSSubmitSound()
{
   // no-op
}

static void I_PCSShutdownSound()
{
   if(pcs_initialised)
      PCSound_SDL_Shutdown();
}

static int I_PCSStartSound(sfxinfo_t *sfx, int cnum, int vol, int sep, 
                          int pitch, int pri, int loop, bool reverb)
{
   int result;

   if(!pcs_initialised)
      return -1;

   // haleyjd: check for "nopcsound" flag
   if(sfx->flags & SFXF_NOPCSOUND)
      return -1;

   if(SDL_LockMutex(sound_lock) < 0)
      return -1;

   if((result = CachePCSLump(sfx)))
      current_sound_handle = cnum;

   SDL_UnlockMutex(sound_lock);

   return result ? cnum : -1;
}

static int I_PCSSoundID(int handle)
{
   return handle;
}

static void I_PCSStopSound(int handle, int id)
{
   if(!pcs_initialised)
      return;

   if(SDL_LockMutex(sound_lock) < 0)
      return;

   // If this is the channel currently playing, immediately end it.   
   if(current_sound_handle == handle)
      current_sound_remaining = 0;

   SDL_UnlockMutex(sound_lock);
}

static int I_PCSSoundIsPlaying(int handle)
{
   return pcs_initialised && handle == current_sound_handle &&
          current_sound_lump != NULL && current_sound_remaining > 0;
}

static void I_PCSUpdateSoundParams(int handle, int vol, int sep, int pitch)
{
   // no-op
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
   NULL,                   // UpdateEQParams
};

// EOF

