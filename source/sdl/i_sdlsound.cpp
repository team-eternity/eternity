//
// The Eternity Engine
// Copyright (C) 2017 James Haley, Stephen McGranahan, Julian Aubourg, et al.
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
// Purpose: SDL sound system interface
// Authors: James Haley, Stephen McGranahan, Julian Aubourg
//

#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_thread.h"

#include "../z_zone.h"

#include "../c_io.h"
#include "../c_runcmd.h"
#include "../d_main.h"
#include "../d_gi.h"
#include "../d_io.h"
#include "../doomstat.h"
#include "../g_game.h"     //jff 1/21/98 added to use dprintf in I_RegisterSong
#include "../i_sound.h"
#include "../i_system.h"
#include "../m_argv.h"
#include "../m_compare.h"
#include "../mn_engin.h"
#include "../s_reverb.h"
#include "../s_formats.h"
#include "../s_sound.h"
#include "../v_misc.h"
#include "../w_wad.h"

extern bool snd_init;

// Needed for calling the actual sound output.
#define MAX_CHANNELS 32

int audio_buffers;

// haleyjd 12/18/13: size at which mix buffers must be allocated
static Uint32 mixbuffer_size;

// haleyjd 12/18/13: primary floating point mixing buffers
static float *mixbuffer[2];

static SDL_AudioDeviceID devid;

// MaxW: 2018/08/27: Desired audiospec and the audiospec we actually have
static SDL_AudioSpec want, have;

// MWM 2000-01-08: Sample rate in samples/second
// haleyjd 10/28/05: updated for Julian's music code, need full quality now
static const int snd_samplerate = 44100;

typedef struct channel_info_s
{
  // SFX id of the playing sound effect.
  // Used to catch duplicates (like chainsaw).
  sfxinfo_t *id;
  // The channel step amount...
  unsigned int step;
  // ... and a 0.16 bit remainder of last step.
  unsigned int stepremainder;
  unsigned int samplerate;
  // The channel data pointers, start and end.
  float *data;
  float *startdata; // haleyjd
  float *enddata;
  // Hardware left and right channel volume lookup.
  float  leftvol, rightvol;
  // haleyjd 06/03/06: looping
  int loop;
  // unique instance id
  unsigned int idnum;
  // if true, channel is affected by reverb
  bool reverb;

  // haleyjd 10/02/08: SDL semaphore to protect channel
  SDL_sem *semaphore;
  bool shouldstop; // haleyjd 05/16/11

} channel_info_t;

static channel_info_t channelinfo[MAX_CHANNELS+1];

// Pitch to stepping lookup, unused.
static int steptable[256];

// Volume lookups.
//static int vol_lookup[128*256];

//
// addsfx
//
// This function adds a sound to the
//  list of currently active sounds,
//  which is maintained as a given number
//  (eight, usually) of internal channels.
// Returns a handle.
//
// haleyjd: needs to take a sfxinfo_t ptr, not a sound id num
// haleyjd 06/03/06: changed to return boolean for failure or success
//
static bool addsfx(sfxinfo_t *sfx, int channel, int loop, unsigned int id, bool reverb)
{
#ifdef RANGECHECK
   if(channel < 0 || channel >= MAX_CHANNELS)
      I_Error("addsfx: channel out of range!\n");
#endif

   // haleyjd 02/18/05: null ptr check
   if(!snd_init || !sfx)
      return false;

   // haleyjd 12/23/13: invoke high-level PCM loader
   if(!S_LoadDigitalSoundEffect(sfx))
      return false;

   // haleyjd 10/02/08: critical section
   if(SDL_SemWait(channelinfo[channel].semaphore) == 0)
   {
      channelinfo[channel].data = (float *)sfx->data;
      
      // Set pointer to end of raw data.
      channelinfo[channel].enddata = (float *)sfx->data + sfx->alen - 1;
      
      // haleyjd 06/03/06: keep track of start of sound
      channelinfo[channel].startdata = channelinfo[channel].data;
      
      channelinfo[channel].stepremainder = 0;
      
      // Preserve sound SFX id
      channelinfo[channel].id = sfx;
      
      // Set looping
      channelinfo[channel].loop = loop;

      // Set reverb
      channelinfo[channel].reverb = reverb;
      
      // Set instance ID
      channelinfo[channel].idnum = id;

      // Shouldn't be stopped - haleyjd 05/16/11
      channelinfo[channel].shouldstop = false;

      SDL_SemPost(channelinfo[channel].semaphore);

      return true;
   }
   else
      return false; // acquisition failed
}

//
// updateSoundParams
//
// Changes sound parameters in response to stereo panning and relative location
// change.
//
static void updateSoundParams(int handle, int volume, int separation, int pitch)
{
   int slot = handle;
   int rightvol;
   int leftvol;
   int step = steptable[pitch];
   
   if(!snd_init)
      return;

#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_UpdateSoundParams: handle out of range\n");
#endif
   
   // Separation, that is, orientation/stereo.
   //  range is: 1 - 256
   separation += 1;

   // SoM 7/1/02: forceFlipPan accounted for here
   if(forceFlipPan)
      separation = 257 - separation;
   
   // Per left/right channel.
   //  x^2 separation,
   //  adjust volume properly.

   leftvol    = volume - ((volume*separation*separation) >> 16);
   separation = separation - 257;
   rightvol   = volume - ((volume*separation*separation) >> 16);  

   // volume levels are softened slightly by dividing by 191 rather than ideal 127
   channelinfo[slot].leftvol  = (float)(eclamp((double)leftvol  / 191.0, 0.0, 1.0));
   channelinfo[slot].rightvol = (float)(eclamp((double)rightvol / 191.0, 0.0, 1.0));

   // haleyjd 06/07/09: critical section is not needed here because this data
   // can be out of sync without affecting the sound update loop. This may cause
   // momentary out-of-sync volume between the left and right sound channels, but
   // the impact would be practically unnoticeable.

   // Set stepping
   // MWM 2000-12-24: Calculates proportion of channel samplerate
   // to global samplerate for mixing purposes.
   // Patched to shift left *then* divide, to minimize roundoff errors
   // as well as to use SAMPLERATE as defined above, not to assume 11025 Hz
   if(pitched_sounds)
      channelinfo[slot].step = step;
   else
      channelinfo[slot].step = 1 << 16;   
}

//=============================================================================
//
// Three-Band Equalization
//

static double preampmul;

struct EQSTATE
{
  // Filter #1 (Low band)

  double  lf;       // Frequency
  double  f1p0;     // Poles ...
  double  f1p1;    
  double  f1p2;
  double  f1p3;

  // Filter #2 (High band)

  double  hf;       // Frequency
  double  f2p0;     // Poles ...
  double  f2p1;
  double  f2p2;
  double  f2p3;

  // Sample history buffer

  double  sdm1;     // Sample data minus 1
  double  sdm2;     //                   2
  double  sdm3;     //                   3

  // Gain Controls

  double  lg;       // low  gain
  double  mg;       // mid  gain
  double  hg;       // high gain
  
};  

// haleyjd 04/21/10: equalizers for each stereo channel
static EQSTATE eqstate[2];

//
// rational_tanh
//
// cschueler
// http://www.musicdsp.org/showone.php?id=238
//
// Notes :
// This is a rational function to approximate a tanh-like soft clipper. It is
// based on the pade-approximation of the tanh function with tweaked 
// coefficients.
// The function is in the range x=-3..3 and outputs the range y=-1..1. Beyond
// this range the output must be clamped to -1..1.
// The first two derivatives of the function vanish at -3 and 3, so the 
// transition to the hard clipped region is C2-continuous.
//
static double rational_tanh(double x)
{
   if(x < -3)
      return -1;
   else if(x > 3)
      return 1;
   else
      return x * ( 27 + x * x ) / ( 27 + 9 * x * x );
}

//
// do_3band
//
// EQ.C - Main Source file for 3 band EQ
// http://www.musicdsp.org/showone.php?id=236
//
// (c) Neil C / Etanza Systems / 2K6
// Shouts / Loves / Moans = etanza at lycos dot co dot uk
//
// This work is hereby placed in the public domain for all purposes, including
// use in commercial applications.
// The author assumes NO RESPONSIBILITY for any problems caused by the use of
// this software.
//
// haleyjd 12/19/13: rewritten to loop over the sample buffer and do output
// directly back to the SDL audio stream.
//
static void do_3band(float *stream, float *end, Sint16 *dest)
{
   int esnum = 0;

   // haleyjd: This "very small addend" is supposed to take care of P4
   // denormalization problems. Do we actually need it?
   static double vsa = (1.0 / 4294967295.0);
   // Locals
   double sample, l, m, h;    // Low / Mid / High - Sample Values

   while(stream != end)
   {
      auto es = &eqstate[esnum];
      esnum ^= 1; // haleyjd: toggle between equalizer channels

      sample = *stream++ * preampmul;

      // Filter #1 (lowpass)
      es->f1p0  += (es->lf * (sample   - es->f1p0)) + vsa;
      es->f1p1  += (es->lf * (es->f1p0 - es->f1p1));
      es->f1p2  += (es->lf * (es->f1p1 - es->f1p2));
      es->f1p3  += (es->lf * (es->f1p2 - es->f1p3));

      l          = es->f1p3;

      // Filter #2 (highpass)
      es->f2p0  += (es->hf * (sample   - es->f2p0)) + vsa;
      es->f2p1  += (es->hf * (es->f2p0 - es->f2p1));
      es->f2p2  += (es->hf * (es->f2p1 - es->f2p2));
      es->f2p3  += (es->hf * (es->f2p2 - es->f2p3));

      h          = es->sdm3 - es->f2p3;

      // Calculate midrange (signal - (low + high))
      m          = es->sdm3 - (h + l); // haleyjd 07/05/10: which is right?
      //m          = sample - (h + l); // the one above seems more correct to me.

      // Scale, Combine and store
      l         *= es->lg;
      m         *= es->mg;
      h         *= es->hg;

      // Shuffle history buffer
      es->sdm3   = es->sdm2;
      es->sdm2   = es->sdm1;
      es->sdm1   = sample;                

      // Return result
      // haleyjd: use rational_tanh for soft clipping
      *dest++ = (Sint16)(rational_tanh(l + m + h) * 32767.0);
   }
}

//
// End Equalizer Code
//
//============================================================================

//============================================================================
//
// MAIN ROUTINE - AUDIOSPEC CALLBACK ROUTINE
//

// size of a single sample
#define SAMPLESIZE sizeof(Sint16) 

// step to next stereo sample pair (2 samples)
#define STEP 2

//
// I_SDLConvertSoundBuffer
//
// Convert the input buffer to floating point
//
static void inline I_SDLConvertSoundBuffer(Uint8 *stream, int len)
{
   // Pointers in audio stream, left, right, end.
   Sint16 *leftout, *leftend;
   
   leftout  = (Sint16 *)stream;
         
   // Determine end, for left channel only
   //  (right channel is implicit).
   leftend = (Sint16 *)(stream + len);

   // convert input to mixbuffer
   float *bptr0 = mixbuffer[0];
   float *bptr1 = mixbuffer[1];
   while(leftout != leftend)
   {
      *(bptr0 + 0) = (float)(*(leftout + 0)) * (1.0f/32768.0f); // L
      *(bptr0 + 1) = (float)(*(leftout + 1)) * (1.0f/32768.0f); // R
      *bptr1 = *(bptr1 + 1) = 0.0f; // clear secondary reverb buffer
      leftout += STEP;
      bptr0   += STEP;
      bptr1   += STEP;
   }
}

//
// I_SDLMixBuffers
//
// Mix the two primary mixing buffers together. This allows sounds to bypass
// environmental effects on a per-channel basis.
//
static inline void I_SDLMixBuffers()
{
   float *bptr = mixbuffer[0];
   float *end  = bptr + mixbuffer_size;
   while(bptr != end)
   {
      *bptr = *bptr + *(bptr + mixbuffer_size);
      ++bptr;
   }
}

//
// I_SDLUpdateSoundCB
//
// SDL_mixer postmix callback routine. Possibly dispatched asynchronously.
// We do our own mixing on up to 32 digital sound channels.
//
static void I_SDLUpdateSoundCB(void *userdata, Uint8 *stream, int len)
{
   memset(stream, 0 , len);

   // convert input samples to floating point
   I_SDLConvertSoundBuffer(stream, len);

   float *leftout;
   // Pointer to end of mixbuffer
   float *leftend0 = mixbuffer[0] + (len/SAMPLESIZE);
   float *leftend1 = mixbuffer[1] + (len/SAMPLESIZE);
   float *leftend;

   // Mix audio channels
   for(channel_info_t *chan = channelinfo; chan != &channelinfo[numChannels]; chan++)
   {
      // fast rejection before semaphore lock
      if(chan->shouldstop || !chan->data)
         continue;
         
      // try to acquire semaphore, but do not block; if the main thread is using
      // this channel we'll just skip it for now - safer and faster.
      if(SDL_SemTryWait(chan->semaphore) != 0)
         continue;

      // Left and right channel are in audio stream, alternating.
      if(chan->reverb)
      {
         leftout  = mixbuffer[1];
         leftend  = leftend1;
      }
      else
      {
         leftout = mixbuffer[0];
         leftend = leftend0;
      }

      // Lost before semaphore acquired? (very unlikely, but must check for 
      // safety). BTW, don't move this up or you'll chew major CPU whenever this
      // does happen.
      if(chan->shouldstop || !chan->data)
      {
         SDL_SemPost(chan->semaphore);
         continue;
      }

      while(leftout != leftend)
      {
         float sample  = *(chan->data);         
         *(leftout + 0) = *(leftout + 0) + sample * chan->leftvol;
         *(leftout + 1) = *(leftout + 1) + sample * chan->rightvol;
         
         // Increment current pointers in stream
         leftout += STEP;
         
         // Increment index
         chan->stepremainder += chan->step;
         
         // MSB is next sample
         chan->data += chan->stepremainder >> 16;
         
         // Limit to LSB
         chan->stepremainder &= 0xffff;
         
         // Check whether we are done
         if(chan->data >= chan->enddata)
         {
            if(chan->loop && !paused && 
               ((!menuactive && !consoleactive) || demoplayback || netgame))
            {
               // haleyjd 06/03/06: restart a looping sample if not paused
               chan->data = chan->startdata;
               chan->stepremainder = 0;
            }
            else
            {
               // flag the channel to be stopped by the main thread ASAP
               chan->data = NULL;
               break;
            }
         }
      }

      // release semaphore and move on to the next channel
      SDL_SemPost(chan->semaphore);
   }

   // do reverberation if an effect is active
   if(s_reverbactive)
      S_ProcessReverb(mixbuffer[1], mixbuffer_size/2);

   // mix reverberated sound with unreverberated buffer
   I_SDLMixBuffers();

   // haleyjd 04/21/10: equalization output pass
   do_3band(mixbuffer[0], leftend0, (Sint16 *)stream);
}

//
// End Audiospec Callback
//
//============================================================================

#define SND_PI 3.14159265

//
// I_SetChannels
//
// Init internal lookups (raw data, mixing buffer, channels).
// This function sets up internal lookups used during
//  the mixing process. 
//
static void I_SetChannels()
{
   int i;
   
   int *steptablemid = steptable + 128;
   
   // Okay, reset internal mixing channels to zero.
   for(i = 0; i < MAX_CHANNELS; i++)
      memset(&channelinfo[i], 0, sizeof(channel_info_t));
   
   // This table provides step widths for pitch parameters.
   for(i = -128; i < 128; i++)
      steptablemid[i] = (int)(pow(1.2, ((double)i/(64.0)))*FPFRACUNIT);
   
   // allocate mixing buffers
   auto buf = ecalloc(float *, 2*mixbuffer_size, sizeof(float));
   mixbuffer[0] = buf;
   mixbuffer[1] = buf + mixbuffer_size;

   // haleyjd 10/02/08: create semaphores
   for(i = 0; i < MAX_CHANNELS; i++)
   {
      channelinfo[i].semaphore = SDL_CreateSemaphore(1);

      if(!channelinfo[i].semaphore)
         I_Error("I_SetChannels: failed to create semaphore for channel %d\n", i);
   }

   // haleyjd 04/21/10: initialize equalizers

   // Set Low/Mid/High gains 
   eqstate[0].lg = eqstate[1].lg = s_lowgain;
   eqstate[0].mg = eqstate[1].mg = s_midgain;
   eqstate[0].hg = eqstate[1].hg = s_highgain;

   // Calculate filter cutoff frequencies
   eqstate[0].lf = eqstate[1].lf = 2 * sin(SND_PI * (s_lowfreq  / (double)snd_samplerate));
   eqstate[0].hf = eqstate[1].hf = 2 * sin(SND_PI * (s_highfreq / (double)snd_samplerate));

   // Calculate preamplification factor
   preampmul = s_eqpreamp;
}

//=============================================================================
// 
// Driver Routines
//

//
// I_SDLUpdateEQParams
//
// haleyjd 04/21/10
//
static void I_SDLUpdateEQParams()
{
   // flush out state of equalizers
   memset(eqstate, 0, sizeof(eqstate));

   // Set Low/Mid/High gains 
   eqstate[0].lg = eqstate[1].lg = s_lowgain;
   eqstate[0].mg = eqstate[1].mg = s_midgain;
   eqstate[0].hg = eqstate[1].hg = s_highgain;

   // Calculate filter cutoff frequencies
   eqstate[0].lf = eqstate[1].lf = 2 * sin(SND_PI * (s_lowfreq  / (double)snd_samplerate));
   eqstate[0].hf = eqstate[1].hf = 2 * sin(SND_PI * (s_highfreq / (double)snd_samplerate));

   // Calculate preamp factor
   preampmul = s_eqpreamp;
}

//
// I_SDLUpdateSoundParams
//
// Update the sound parameters. Used to control volume,
// pan, and pitch changes such as when a player turns.
//
static void I_SDLUpdateSoundParams(int handle, int vol, int sep, int pitch)
{
   updateSoundParams(handle, vol, sep, pitch);
}

//
// I_SDLStartSound
//
static int I_SDLStartSound(sfxinfo_t *sound, int cnum, int vol, int sep, 
                           int pitch, int pri, int loop, bool reverb)
{
   static unsigned int id = 1;
   int handle;

   // haleyjd 06/03/06: look for an unused hardware channel
   for(handle = 0; handle < numChannels; handle++)
   {
      if(channelinfo[handle].data == NULL || channelinfo[handle].shouldstop)
         break;
   }

   // all used? don't play the sound. It's preferable to miss a sound
   // than to cut off one already playing, which sounds weird.
   if(handle == numChannels)
      return -1;
 
   if(addsfx(sound, handle, loop, id, reverb))
   {
      updateSoundParams(handle, vol, sep, pitch);
      ++id; // increment id to keep each sound instance unique
   }
   else
      handle = -1;
   
   return handle;
}

//
// I_SDLStopSound
//
// Stop the sound. Necessary to prevent runaway chainsaw,
// and to stop rocket launches when an explosion occurs.
//
static void I_SDLStopSound(int handle, int id)
{
#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_SDLStopSound: handle out of range\n");
#endif
   
   if(channelinfo[handle].idnum == (unsigned int)id)
      channelinfo[handle].shouldstop = true;
}

//
// I_SDLSoundIsPlaying
//
// haleyjd: wow, this can actually do something in the Windows version :P
//
static int I_SDLSoundIsPlaying(int handle)
{
#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_SDLSoundIsPlaying: handle out of range\n");
#endif
 
   return !channelinfo[handle].shouldstop && channelinfo[handle].data;
}

//
// I_SDLSoundID
//
// haleyjd: returns the unique id number assigned to a specific instance
// of a sound playing on a given channel. This is required to make sure
// that the higher-level sound code doesn't start updating sounds that have
// been displaced without it noticing.
//
static int I_SDLSoundID(int handle)
{
#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_SDLSoundID: handle out of range\n");
#endif

   return channelinfo[handle].idnum;
}

//
// I_SDLUpdateSound
//
// haleyjd 10/02/08: This function is now responsible for stopping channels
// that have expired. The I_SDLUpdateSound function does sound updating, but
// cannot be allowed to modify the zone heap due to being dispatched from a
// separate thread.
// 
static void I_SDLUpdateSound()
{
   // 10/30/10: Moved channel stopping logic to I_StartSound to avoid problems
   // with thread contention when running with d_fastrefresh enabled. Calling
   // this from the main loop too often caused the sound to stutter.
}

//
// I_SDLSubmitSound
//
static void I_SDLSubmitSound()
{
   // haleyjd: whatever it is, SDL doesn't need it either
}

//
// I_SDLShutdownSound
//
// atexit handler.
//
static void I_SDLShutdownSound()
{
   SDL_CloseAudioDevice(devid);
}

//
// I_SDLCacheSound
//
// haleyjd 11/05/03: fixed for SDL sound engine
//
static void I_SDLCacheSound(sfxinfo_t *sound)
{
   // 12/23/13: invoke high level lump cacher
   S_CacheDigitalSoundLump(sound);
}

//
// I_SDLInitSound
//
// SoM 9/14/02: Rewrite. code taken from prboom to use SDL_Mixer
//
static int I_SDLInitSound()
{
   // haleyjd: the docs say we should do this
   if(SDL_InitSubSystem(SDL_INIT_AUDIO))
   {
      printf("Couldn't initialize SDL audio.\n");
      nosfxparm   = true;
      nomusicparm = true;
      return 0;
   }

   if(!I_IsSoundBufferSizePowerOf2(audio_buffers))
      audio_buffers = I_MakeSoundBufferSize(audio_buffers);

   want.freq     = snd_samplerate;
   want.format   = AUDIO_S16SYS;
   want.channels = 2;
   want.samples  = audio_buffers;
   want.callback = I_SDLUpdateSoundCB;
   devid = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
   mixbuffer_size = have.size / SAMPLESIZE;
   if(devid < 0)
   {
      printf("Couldn't open audio with desired format: %s.\n", SDL_GetError());
      nosfxparm   = true;
      nomusicparm = true;
      return 0;
   }

   // haleyjd 10/02/08: this must be done as early as possible.
   I_SetChannels();

   SDL_PauseAudioDevice(devid, 0);

   printf("Configured audio device with %d samples/slice.\n", audio_buffers);

   return 1;
}

//
// SDL Sound Driver Object
//
i_sounddriver_t i_sdlsound_driver =
{
   I_SDLInitSound,         // InitSound
   I_SDLCacheSound,        // CacheSound
   I_SDLUpdateSound,       // UpdateSound
   I_SDLSubmitSound,       // SubmitSound
   I_SDLShutdownSound,     // ShutdownSound
   I_SDLStartSound,        // StartSound
   I_SDLSoundID,           // SoundID
   I_SDLStopSound,         // StopSound
   I_SDLSoundIsPlaying,    // SoundIsPlaying
   I_SDLUpdateSoundParams, // UpdateSoundParams
   I_SDLUpdateEQParams,    // UpdateEQParams
};

// EOF

