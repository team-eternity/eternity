// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley, Stephen McGranahan, Julian Aubourg, et al.
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
//      System interface for sound.
//
//-----------------------------------------------------------------------------

#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_thread.h"
#include "SDL_mixer.h"

#include "../z_zone.h"
#include "../d_io.h"
#include "../c_runcmd.h"
#include "../c_io.h"
#include "../doomstat.h"
#include "../i_sound.h"
#include "../i_system.h"
#include "../w_wad.h"
#include "../g_game.h"     //jff 1/21/98 added to use dprintf in I_RegisterSong
#include "../d_main.h"
#include "../v_misc.h"
#include "../m_argv.h"
#include "../d_gi.h"
#include "../s_sound.h"
#include "../mn_engin.h"

#ifdef HAVE_SPCLIB
#include "../../snes_spc/spc.h"
#endif

#ifdef LINUX
// haleyjd 01/28/07: I don't understand why this is needed here...
extern DECLSPEC Mix_Music * SDLCALL Mix_LoadMUS_RW(SDL_RWops *rw);
#endif

void I_CacheSound(sfxinfo_t *sound);

// Needed for calling the actual sound output.
static int SAMPLECOUNT = 512;
#define MAX_CHANNELS 32

        // sf: adjust temp when changing gamespeed
extern int realtic_clock_rate;

int snd_card;   // default.cfg variables for digi and midi drives
int mus_card;   // jff 1/18/98

// haleyjd: safety variables to keep changes to *_card from making
// these routines think that sound has been initialized when it hasn't
boolean snd_init = false;
boolean mus_init = false;

int detect_voices; //jff 3/4/98 enables voice detection prior to install_sound
//jff 1/22/98 make these visible here to disable sound/music on install err

// MWM 2000-01-08: Sample rate in samples/second
// haleyjd 10/28/05: updated for Julian's music code, need full quality now
int snd_samplerate = 44100;

// The actual output device.
int audio_fd;

typedef struct {
  // SFX id of the playing sound effect.
  // Used to catch duplicates (like chainsaw).
  sfxinfo_t *id;
  // The channel step amount...
  unsigned int step;
  // ... and a 0.16 bit remainder of last step.
  unsigned int stepremainder;
  unsigned int samplerate;
  // The channel data pointers, start and end.
  unsigned char *data;
  unsigned char *startdata; // haleyjd
  unsigned char *enddata;
  // Hardware left and right channel volume lookup.
  int *leftvol_lookup;
  int *rightvol_lookup;
  // haleyjd 10/02/08: channel is waiting to be stopped
  boolean stopChannel;
  // haleyjd 06/03/06: looping
  int loop;
  int idnum;

  // haleyjd 10/02/08: SDL semaphore to protect channel
  SDL_sem *semaphore;

} channel_info_t;

channel_info_t channelinfo[MAX_CHANNELS+1];

// Pitch to stepping lookup, unused.
int steptable[256];

// Volume lookups.
int vol_lookup[128*256];

//
// stopchan
//
// cph 
// Stops a sound, unlocks the data 
//
static void stopchan(int handle)
{
   int cnum;
   boolean freeSound = true;
   
#ifdef RANGECHECK
   // haleyjd 02/18/05: bounds checking
   if(handle < 0 || handle >= MAX_CHANNELS)
      return;
#endif
   
   // haleyjd 10/02/08: critical section
   if(SDL_SemWait(channelinfo[handle].semaphore) == 0)
   {
      channelinfo[handle].stopChannel = false;

      if(channelinfo[handle].data)
      {
         channelinfo[handle].data = NULL;
         
         if(channelinfo[handle].id)
         {
            // haleyjd 06/03/06: see if we can free the sound
            for(cnum = 0; cnum < MAX_CHANNELS; ++cnum)
            {
               if(cnum == handle)
                  continue;
               if(channelinfo[cnum].id &&
                  channelinfo[cnum].id->data == channelinfo[handle].id->data)
               {
                  freeSound = false; // still being used by some channel
                  break;
               }
            }
            
            // set sample to PU_CACHE level
            if(freeSound)
            {
               Z_ChangeTag(channelinfo[handle].id->data, PU_CACHE);
               channelinfo[handle].id = NULL;
            }
         }
      }

      SDL_SemPost(channelinfo[handle].semaphore);
   }
}

#define SOUNDHDRSIZE 8

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
static boolean addsfx(sfxinfo_t *sfx, int channel, int loop, int id)
{
   size_t lumplen;
   int lump;

#ifdef RANGECHECK
   if(channel < 0 || channel >= MAX_CHANNELS)
      I_Error("addsfx: channel out of range!\n");
#endif

   // haleyjd 02/18/05: null ptr check
   if(!snd_init || !sfx)
      return false;

   stopchan(channel);
   
   // We will handle the new SFX.
   // Set pointer to raw data.

   // haleyjd: Eternity sfxinfo_t does not have a lumpnum field
   lump = I_GetSfxLumpNum(sfx);
   
   // replace missing sounds with a reasonable default
   if(lump == -1)
      lump = W_GetNumForName(GameModeInfo->defSoundName);
   
   lumplen = W_LumpLength(lump);
   
   // haleyjd 10/08/04: do not play zero-length sound lumps
   if(lumplen <= SOUNDHDRSIZE)
      return false;

   // haleyjd 06/03/06: rewrote again to make sound data properly freeable
   if(sfx->data == NULL)
   {   
      byte *data;
      Uint32 samplerate, samplelen;

      // haleyjd: this should always be called (if lump is already loaded,
      // W_CacheLumpNum handles that for us).
      data = (byte *)W_CacheLumpNum(lump, PU_STATIC);

      // Check the header, and ensure this is a valid sound
      if(data[0] != 0x03 || data[1] != 0x00)
      {
         Z_ChangeTag(data, PU_CACHE);
         return false;
      }

      samplerate = (data[3] << 8) | data[2];
      samplelen  = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];

      // don't play sounds that think they're longer than they really are
      if(samplelen > lumplen - SOUNDHDRSIZE)
      {
         Z_ChangeTag(data, PU_CACHE);
         return false;
      }

      sfx->alen = (Uint32)(((ULong64)samplelen * snd_samplerate) / samplerate);
      sfx->data = Z_Malloc(sfx->alen, PU_STATIC, &sfx->data);

      // haleyjd 04/23/08: Convert sound to target samplerate
      if(sfx->alen != samplelen)
      {  
         unsigned int i;
         byte *dest = (byte *)sfx->data;
         byte *src  = data + SOUNDHDRSIZE;
         
         unsigned int step = (samplerate << 16) / snd_samplerate;
         unsigned int stepremainder = 0, j = 0;

         // do linear filtering operation
         for(i = 0; i < sfx->alen && j < samplelen - 1; ++i)
         {
            int d = (((unsigned int)src[j  ] * (0x10000 - stepremainder)) +
                     ((unsigned int)src[j+1] * stepremainder)) >> 16;

            if(d > 255)
               dest[i] = 255;
            else if(d < 0)
               dest[i] = 0;
            else
               dest[i] = (byte)d;

            stepremainder += step;
            j += (stepremainder >> 16);

            stepremainder &= 0xffff;
         }
         // fill remainder (if any) with final sample byte
         for(; i < sfx->alen; ++i)
            dest[i] = src[j];
      }
      else
      {
         // sound is already at target samplerate, copy data
         memcpy(sfx->data, data + SOUNDHDRSIZE, samplelen);
      }

      // haleyjd 06/03/06: don't need original lump data any more
      Z_ChangeTag(data, PU_CACHE);
   }
   else
      Z_ChangeTag(sfx->data, PU_STATIC); // reset to static cache level

   // haleyjd 10/02/08: critical section
   if(SDL_SemWait(channelinfo[channel].semaphore) == 0)
   {
      channelinfo[channel].data = sfx->data;
      
      // Set pointer to end of raw data.
      channelinfo[channel].enddata = (byte *)sfx->data + sfx->alen - 1;
      
      // haleyjd 06/03/06: keep track of start of sound
      channelinfo[channel].startdata = channelinfo[channel].data;
      
      channelinfo[channel].stepremainder = 0;
      
      // Preserve sound SFX id
      channelinfo[channel].id = sfx;
      
      // Set looping
      channelinfo[channel].loop = loop;
      
      // Set instance ID
      channelinfo[channel].idnum = id;

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
      I_Error("I_UpdateSoundParams: handle out of range");
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
   //volume *= 8;

   leftvol = volume - ((volume*separation*separation) >> 16);
   separation = separation - 257;
   rightvol= volume - ((volume*separation*separation) >> 16);  

   // Sanity check, clamp volume.
   if(rightvol < 0 || rightvol > 127)
      I_Error("rightvol out of bounds");
   
   if(leftvol < 0 || leftvol > 127)
      I_Error("leftvol out of bounds");

   // haleyjd 10/02/08: critical section
   if(SDL_SemWait(channelinfo[slot].semaphore) == 0)
   {
      // Set stepping
      // MWM 2000-12-24: Calculates proportion of channel samplerate
      // to global samplerate for mixing purposes.
      // Patched to shift left *then* divide, to minimize roundoff errors
      // as well as to use SAMPLERATE as defined above, not to assume 11025 Hz
      if(pitched_sounds)
         channelinfo[slot].step = step;
      else
         channelinfo[slot].step = 1 << 16;
      
      // Get the proper lookup table piece
      //  for this volume level???
      channelinfo[slot].leftvol_lookup  = &vol_lookup[leftvol*256];
      channelinfo[slot].rightvol_lookup = &vol_lookup[rightvol*256];

      SDL_SemPost(channelinfo[slot].semaphore);
   }
}

//
// SFX API
//

//
// I_UpdateSoundParams
//
// Update the sound parameters. Used to control volume,
// pan, and pitch changes such as when a player turns.
//
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
   if(!snd_init)
      return;

   updateSoundParams(handle, vol, sep, pitch);
}

//
// I_SetChannels
//
// Init internal lookups (raw data, mixing buffer, channels).
// This function sets up internal lookups used during
//  the mixing process. 
//
void I_SetChannels(void)
{
   int i;
   int j;
   
   int *steptablemid = steptable + 128;
   
   // Okay, reset internal mixing channels to zero.
   for(i = 0; i < MAX_CHANNELS; ++i)
   {
      memset(&channelinfo[i], 0, sizeof(channel_info_t));
   }
   
   // This table provides step widths for pitch parameters.
   for(i=-128 ; i<128 ; i++)
   {
      steptablemid[i] = (int)(pow(1.2, ((double)i/(64.0 /* *snd_samplerate/11025 */)))*65536.0);
   }   
   
   // Generates volume lookup tables
   //  which also turn the unsigned samples
   //  into signed samples.
   for(i = 0; i < 128; i++)
   {
      for(j = 0; j < 256; j++)
      {
         // proff - made this a little bit softer, because with
         // full volume the sound clipped badly (191 was 127)
         vol_lookup[i*256+j] = (i*(j-128)*256)/191;
      }
   }

   // haleyjd 10/02/08: create semaphores
   for(i = 0; i < MAX_CHANNELS; ++i)
   {
      channelinfo[i].semaphore = SDL_CreateSemaphore(1);

      if(!channelinfo[i].semaphore)
         I_Error("I_SetChannels: failed to create semaphore for channel %d\n", i);
   }
}

//
// I_SetSfxVolume
//
void I_SetSfxVolume(int volume)
{
  // Identical to DOS.
  // Basically, this should propagate
  //  the menu/config file setting
  //  to the state variable used in
  //  the mixing.
  
   snd_SfxVolume = volume;
}

// jff 1/21/98 moved music volume down into MUSIC API with the rest

//
// I_GetSfxLumpNum
//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t *sfx)
{
   char namebuf[16];

   memset(namebuf, 0, sizeof(namebuf));

   // haleyjd 09/03/03: determine whether to apply DS prefix to
   // name or not using new prefix flag
   if(sfx->prefix)
      psnprintf(namebuf, sizeof(namebuf), "ds%s", sfx->name);
   else
      strcpy(namebuf, sfx->name);

   return W_CheckNumForName(namebuf);
}


//
// I_StartSound
//
int I_StartSound(sfxinfo_t *sound, int cnum, int vol, int sep, int pitch, 
                 int pri, int loop)
{
   static unsigned int id = 0;
   int handle;
   
   if(!snd_init)
      return -1;

   // haleyjd: turns out this is too simplistic. see below.
   /*
   // SoM: reimplement hardware channel wrap-around
   if(++handle >= MAX_CHANNELS)
      handle = 0;
   */

   // haleyjd 06/03/06: look for an unused hardware channel
   for(handle = 0; handle < MAX_CHANNELS; ++handle)
   {
      if(channelinfo[handle].data == NULL)
         break;
   }

   // all used? don't play the sound. It's preferable to miss a sound
   // than it is to cut off one already playing, which sounds weird.
   if(handle == MAX_CHANNELS)
      return -1;
 
   if(addsfx(sound, handle, loop, id))
   {
      updateSoundParams(handle, vol, sep, pitch);
      ++id; // increment id to keep each sound instance unique
   }
   else
      handle = -1;
   
   return handle;
}

//
// I_StopSound
//
// Stop the sound. Necessary to prevent runaway chainsaw,
// and to stop rocket launches when an explosion occurs.
//
void I_StopSound(int handle)
{
   if(!snd_init)
      return;

#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_StopSound: handle out of range");
#endif
   
   stopchan(handle);
}

//
// I_SoundIsPlaying
//
// haleyjd: wow, this can actually do something in the Windows version :P
//
int I_SoundIsPlaying(int handle)
{
   if(!snd_init)
      return false;

#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_SoundIsPlaying: handle out of range");
#endif
 
   return (channelinfo[handle].data != NULL);
}

//
// I_SoundID
//
// haleyjd: returns the unique id number assigned to a specific instance
// of a sound playing on a given channel. This is required to make sure
// that the higher-level sound code doesn't start updating sounds that have
// been displaced without it noticing.
//
int I_SoundID(int handle)
{
   if(!snd_init)
      return 0;

#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_SoundID: handle out of range\n");
#endif

   return channelinfo[handle].idnum;
}

//
// I_UpdateSound
//
// haleyjd 10/02/08: This function is now responsible for stopping channels
// that have expired. The I_SDLUpdateSound function does sound updating, but
// cannot be allowed to modify the zone heap due to being dispatched from a
// separate thread.
// 
void I_UpdateSound(void)
{
   int chan;

   for(chan = 0; chan < MAX_CHANNELS; ++chan)
   {
      if(channelinfo[chan].stopChannel == true)
         stopchan(chan);
   }
}

#define STEP sizeof(Sint16)
#define STEPSHIFT 1

#ifdef HAVE_SPCLIB
// haleyjd 05/02/08: SPC support
static SNES_SPC   *snes_spc   = NULL;
static SPC_Filter *spc_filter = NULL;

int spc_preamp     = 1;
int spc_bass_boost = 8;

//
// I_EffectSPC
//
// SDL_mixer effect processor; mixes SPC data into the postmix stream.
//
static void I_EffectSPC(int chan, void *stream, int len, void *udata)
{
   Sint16 *leftout, *rightout, *leftend, *datal, *datar;
   int numsamples, spcsamples;
   int stepremainder = 0, i = 0;
   int dl, dr;

   static Sint16 *spc_buffer = NULL;
   static int lastspcsamples = 0;

   leftout  =  (Sint16 *)stream;
   rightout = ((Sint16 *)stream) + 1;

   numsamples = len / STEP;
   leftend = leftout + numsamples;

   // round samples up to higher even number
   spcsamples = ((int)(numsamples * 320.0 / 441.0) & ~1) + 2;

   // realloc spc buffer?
   if(spcsamples != lastspcsamples)
   {
      // add extra buffer samples at end for filtering safety; stereo channels
      spc_buffer = (short *)Z_SysRealloc(spc_buffer, 
                                         (spcsamples + 2) * 2 * sizeof(Sint16));
      lastspcsamples = spcsamples;
   }

   // get spc samples
   if(spc_play(snes_spc, spcsamples, spc_buffer))
      return;

   // filter samples
   spc_filter_run(spc_filter, spc_buffer, spcsamples);

   datal = spc_buffer;
   datar = spc_buffer + 1;

   while(leftout != leftend)
   {
      // linear filter spc samples to the output buffer, since the
      // sample rate is higher (44.1 kHz vs. 32 kHz).

      dl = *leftout + (((int)datal[0] * (0x10000 - stepremainder) +
                        (int)datal[2] * stepremainder) >> 16);

      dr = *rightout + (((int)datar[0] * (0x10000 - stepremainder) +
                         (int)datar[2] * stepremainder) >> 16);

      if(dl > SHRT_MAX)
         *leftout = SHRT_MAX;
      else if(dl < SHRT_MIN)
         *leftout = SHRT_MIN;
      else
         *leftout = (Sint16)dl;

      if(dr > SHRT_MAX)
         *rightout = SHRT_MAX;
      else if(dr < SHRT_MIN)
         *rightout = SHRT_MIN;
      else
         *rightout = (Sint16)dr;

      stepremainder += ((32000 << 16) / 44100);

      i += (stepremainder >> 16);
      
      datal = spc_buffer + (i << STEPSHIFT);
      datar = datal + 1;

      // trim remainder to decimal portion
      stepremainder &= 0xffff;

      // step to next sample in mixer buffer
      leftout  += STEP;
      rightout += STEP;
   }
}
#endif

//
// I_SDLUpdateSound
//
// SDL_mixer postmix callback routine. Possibly dispatched asynchronously.
// We do our own mixing on up to 32 digital sound channels.
//
static void I_SDLUpdateSound(void *userdata, Uint8 *stream, int len)
{
   // Mix current sound data.
   // Data, from raw sound, for right and left.
   Uint8  sample;
   Sint32 dl;
   Sint32 dr;
   
   // Pointers in audio stream, left, right, end.
   Sint16 *leftout, *rightout, *leftend;
   
   // haleyjd: channel pointer for speed++
   channel_info_t *chan;
         
   // Determine end, for left channel only
   //  (right channel is implicit).
   leftend = (Sint16 *)(stream) + len / STEP;

   // Love thy L2 cache - made this a loop.
   // Now more channels could be set at compile time
   //  as well. Thus loop those channels.
   for(chan = channelinfo; chan != &channelinfo[MAX_CHANNELS]; ++chan)
   {
      // fast rejection before semaphore lock
      if(!chan->data || chan->stopChannel)
         continue;
         
      // try to acquire semaphore, but do not block; if the main thread is using
      // this channel we'll just skip it for now - safer and faster.
      if(SDL_SemTryWait(chan->semaphore) != 0)
         continue;
      
      // Left and right channel
      //  are in audio stream, alternating.
      leftout  = (Sint16 *)stream;
      rightout = leftout + 1;
      
      // lost before semaphore acquired? (very unlikely, but must check for safety)
      // BTW, don't move this up or you'll chew major CPU whenever this does happen.
      if(!chan->data)
      {
         SDL_SemPost(chan->semaphore);
         continue;
      }
      
      // Mix sounds into the mixing buffer.
      // Loop over step*SAMPLECOUNT,
      //  that is 512 values for two channels.
      while(leftout != leftend)
      {
         // Get the raw data from the channel. 
         // Sounds are now prefiltered.
         sample = *(chan->data);

         // Reset left/right value. 
         // Add left and right part
         //  for this channel (sound)
         //  to the current data.
         // Adjust volume accordingly.
         dl = (Sint32)(*leftout)  + chan->leftvol_lookup[sample];
         dr = (Sint32)(*rightout) + chan->rightvol_lookup[sample];
                  
         // Clamp to range. Left hardware channel.
         if(dl > SHRT_MAX)
            *leftout = SHRT_MAX;
         else if(dl < SHRT_MIN)
            *leftout = SHRT_MIN;
         else
            *leftout = (Sint16)dl;
            
         // Same for right hardware channel.
         if(dr > SHRT_MAX)
            *rightout = SHRT_MAX;
         else if(dr < SHRT_MIN)
            *rightout = SHRT_MIN;
         else
            *rightout = (Sint16)dr;

         // Increment current pointers in stream
         leftout  += STEP;
         rightout += STEP;
         
         // Increment index
         chan->stepremainder += chan->step;
         
         // MSB is next sample
         chan->data += chan->stepremainder >> 16;
         
         // Limit to LSB
         chan->stepremainder &= 0xffff;
         
         // Check whether we are done
         if(chan->data >= chan->enddata)
         {
            if(chan->loop && !paused && (!menuactive || demoplayback || netgame))
            {
               // haleyjd 06/03/06: restart a looping sample if not paused
               chan->data = chan->startdata;
               chan->stepremainder = 0;
            }
            else
            {
               // flag the channel to be stopped by the main thread ASAP
               chan->stopChannel = true;
               break;
            }
         }
      }
      
      // release semaphore and move on to the next channel
      SDL_SemPost(chan->semaphore);
   }
}

/*
//
// I_SDLUpdateSound
//
// SDL_mixer postmix callback routine. Possibly dispatched asynchronously.
// We do our own mixing on up to 32 digital sound channels.
//
static void I_SDLUpdateSound(void *userdata, Uint8 *stream, int len)
{
   // Mix current sound data.
   // Data, from raw sound, for right and left.
   register Uint8  sample;
   register Sint32 dl;
   register Sint32 dr;
   
   // Pointers in audio stream, left, right, end.
   Sint16 *leftout;
   Sint16 *rightout;
   Sint16 *leftend;
   
   // Mixing channel index.
   int chan;

   // Left and right channel
   //  are in audio stream, alternating.
   leftout  = (Sint16 *)stream;
   rightout = leftout + 1;
   
   // Determine end, for left channel only
   //  (right channel is implicit).
   leftend = leftout + len / STEP;

   // Mix sounds into the mixing buffer.
   // Loop over step*SAMPLECOUNT,
   //  that is 512 values for two channels.
   while(leftout != leftend)
   {
      // Reset left/right value. 
      dl = *leftout;
      dr = *rightout;
      
      // Love thy L2 cache - made this a loop.
      // Now more channels could be set at compile time
      //  as well. Thus loop those  channels.
      for(chan = 0; chan < MAX_CHANNELS; ++chan)
      {
         // Check channel, if active.
         if(!channelinfo[chan].data || channelinfo[chan].stopChannel)
            continue;

         // haleyjd 10/02/08: try to acquire semaphore, but do not block
         if(SDL_SemTryWait(channelinfo[chan].semaphore) != 0)
            continue;
         
         // Get the raw data from the channel. 
         // Sounds are now prefiltered.
         sample = *(channelinfo[chan].data);
            
         // Add left and right part
         //  for this channel (sound)
         //  to the current data.
         // Adjust volume accordingly.
         dl += channelinfo[chan].leftvol_lookup[sample];
         dr += channelinfo[chan].rightvol_lookup[sample];
         
         // Increment index ???
         channelinfo[chan].stepremainder += channelinfo[chan].step;
         
         // MSB is next sample???
         channelinfo[chan].data += channelinfo[chan].stepremainder >> 16;
         
         // Limit to LSB???
         channelinfo[chan].stepremainder &= 0xffff;
         
         // Check whether we are done.
         if(channelinfo[chan].data >= channelinfo[chan].enddata)
         {            
            if(channelinfo[chan].loop &&
               !(paused || (menuactive && !demoplayback && !netgame)))
            {
               // haleyjd 06/03/06: restart a looping sample if not paused
               channelinfo[chan].data = channelinfo[chan].startdata;
               channelinfo[chan].stepremainder = 0;
            }
            else
            {
               // haleyjd 10/02/08: NO!!!!!!!!
               // Why not? This could modify the zone heap, and this function
               // is dispatched from a separate thread. WOOPS.
               //stopchan(chan);

               // flag the channel to be stopped by the main thread ASAP
               channelinfo[chan].stopChannel = true;
            }
         }

         // release semaphore
         SDL_SemPost(channelinfo[chan].semaphore);
      }
      
      // Clamp to range. Left hardware channel.
      if(dl > SHRT_MAX)
         *leftout = SHRT_MAX;
      else if(dl < SHRT_MIN)
         *leftout = SHRT_MIN;
      else
         *leftout = (short)dl;
      
      // Same for right hardware channel.
      if(dr > SHRT_MAX)
         *rightout = SHRT_MAX;
      else if(dr < SHRT_MIN)
         *rightout = SHRT_MIN;
      else
         *rightout = (short)dr;
      
      // Increment current pointers in stream
      leftout  += STEP;
      rightout += STEP;
   }
}
*/

// This would be used to write out the mixbuffer
//  during each game loop update.
// Updates sound buffer and audio device at runtime.
// It is called during Timer interrupt with SNDINTR.

void I_SubmitSound(void)
{
   // haleyjd: whatever it is, SDL doesn't need it either
}

//
// I_ShutdownSound
//
// atexit handler.
//
void I_ShutdownSound(void)
{
   if(snd_init)
   {
      Mix_CloseAudio();
      snd_init = 0;
   }
}

//
// I_CacheSound
//
// haleyjd 11/05/03: fixed for SDL sound engine
// haleyjd 09/24/06: added sound aliases
//
void I_CacheSound(sfxinfo_t *sound)
{
   if(sound->alias)
      I_CacheSound(sound->alias);
   else if(sound->link)
      I_CacheSound(sound->link);
   else
   {
      int lump = I_GetSfxLumpNum(sound);
 
      // replace missing sounds with a reasonable default
      if(lump == -1)
         lump = W_GetNumForName(GameModeInfo->defSoundName);

      W_CacheLumpNum(lump, PU_CACHE);
   }
}

//
// I_InitSound
//
// SoM 9/14/02: Rewrite. code taken from prboom to use SDL_Mixer
//
void I_InitSound(void)
{   
   if(!nosfxparm)
   {
      int audio_buffers;

      printf("I_InitSound: ");

      /* Initialize variables */
      audio_buffers = SAMPLECOUNT * snd_samplerate / 11025;

      // haleyjd: the docs say we should do this
      if(SDL_InitSubSystem(SDL_INIT_AUDIO))
      {
         printf("Couldn't initialize SDL audio.\n");
         snd_card = 0;
         mus_card = 0;
         return;
      }
  
      if(Mix_OpenAudio(snd_samplerate, MIX_DEFAULT_FORMAT, 2, audio_buffers) < 0)
      {
         printf("Couldn't open audio with desired format.\n");
         snd_card = 0;
         mus_card = 0;
         return;
      }

      // haleyjd 10/02/08: this must be done as early as possible.
      I_SetChannels();

      SAMPLECOUNT = audio_buffers;
      Mix_SetPostMix(I_SDLUpdateSound, NULL);
      printf("Configured audio device with %d samples/slice.\n", SAMPLECOUNT);

      atexit(I_ShutdownSound);

      snd_init = true;

      // haleyjd 04/11/03: don't use music if sfx aren't init'd
      // (may be dependent, docs are unclear)
      if(!nomusicparm)
         I_InitMusic();
   }   
}

//
// MUSIC API.
//

// julian (10/25/2005): rewrote (nearly) entirely

#include "mmus2mid.h"
#include "../m_misc.h"

// Only one track at a time
static Mix_Music *music = NULL;

// Some tracks are directly streamed from the RWops;
// we need to free them in the end
static SDL_RWops *rw = NULL;

// Same goes for buffers that were allocated to convert music;
// since this concerns mus, we could do otherwise but this 
// approach is better for consistency
static void *music_block = NULL;

// Macro to make code more readable
#define CHECK_MUSIC(h) ((h) && music != NULL)

//
// I_ShutdownMusic
//
// atexit handler.
//
void I_ShutdownMusic(void)
{
   I_UnRegisterSong(1);
}

//
// I_InitMusic
//
void I_InitMusic(void)
{
   switch(mus_card)
   {
   case -1:
      printf("I_InitMusic: Using SDL_mixer.\n");
      mus_init = true;
      break;   
   default:
      printf("I_InitMusic: Music is disabled.\n");
      break;
   }
   
   atexit(I_ShutdownMusic);
}

//
// I_PlaySong
//
// Plays the currently registered song.
//
void I_PlaySong(int handle, int looping)
{
#ifdef HAVE_SPCLIB
   // if a SPC is set up, play it.
   if(snes_spc)
   {
      if(!Mix_RegisterEffect(MIX_CHANNEL_POST, I_EffectSPC, NULL, NULL))
      {
         doom_printf("I_PlaySong: Mix_RegisterEffect failed\n");
         return;
      }
   }
   else
#endif
   if(CHECK_MUSIC(handle) && Mix_PlayMusic(music, looping ? -1 : 0) == -1)
   {
      doom_printf("I_PlaySong: Mix_PlayMusic failed\n");
      return;
   }
   
   // haleyjd 10/28/05: make sure volume settings remain consistent
   I_SetMusicVolume(snd_MusicVolume);
}

//
// I_SetMusicVolume
//
void I_SetMusicVolume(int volume)
{
   // haleyjd 09/04/06: adjust to use scale from 0 to 15
   Mix_VolumeMusic((volume * 128) / 15);

#ifdef HAVE_SPCLIB
   if(snes_spc)
   {
      // if a SPC is playing, set its volume too
      spc_filter_set_gain(spc_filter, volume * (256 * spc_preamp) / 15);
   }
#endif
}

static int paused_midi_volume;

//
// I_PauseSong
//
void I_PauseSong(int handle)
{
   if(CHECK_MUSIC(handle))
   {
      // Not for mids
      if(Mix_GetMusicType(music) != MUS_MID)
         Mix_PauseMusic();
      else
      {
         // haleyjd 03/21/06: set MIDI volume to zero on pause
         paused_midi_volume = Mix_VolumeMusic(-1);
         Mix_VolumeMusic(0);
      }
   }

#ifdef HAVE_SPCLIB
   if(snes_spc)
      Mix_UnregisterEffect(MIX_CHANNEL_POST, I_EffectSPC);
#endif
}

//
// I_ResumeSong
//
void I_ResumeSong(int handle)
{
   if(CHECK_MUSIC(handle))
   {
      // Not for mids
      if(Mix_GetMusicType(music) != MUS_MID)
         Mix_ResumeMusic();
      else
         Mix_VolumeMusic(paused_midi_volume);
   }

#ifdef HAVE_SPCLIB
   if(snes_spc)
      Mix_RegisterEffect(MIX_CHANNEL_POST, I_EffectSPC, NULL, NULL);
#endif
}

//
// I_StopSong
//
void I_StopSong(int handle)
{
   if(CHECK_MUSIC(handle))
      Mix_HaltMusic();

#ifdef HAVE_SPCLIB
   if(snes_spc)
      Mix_UnregisterEffect(MIX_CHANNEL_POST, I_EffectSPC);
#endif
}

//
// I_UnRegisterSong
//
void I_UnRegisterSong(int handle)
{
   if(CHECK_MUSIC(handle))
   {   
      // Stop and free song
      I_StopSong(handle);
      Mix_FreeMusic(music);
      
      // Free RWops
      if(rw != NULL)
         SDL_FreeRW(rw);
      
      // Free music block
      if(music_block != NULL)
         free(music_block);
      
      // Reinitialize all this
      music = NULL;
      rw = NULL;
      music_block = NULL;
   }

#ifdef HAVE_SPCLIB
   if(snes_spc)
   {
      // be certain the callback is unregistered first
      Mix_UnregisterEffect(MIX_CHANNEL_POST, I_EffectSPC);

      // free the spc and filter objects
      spc_delete(snes_spc);
      spc_filter_delete(spc_filter);

      snes_spc   = NULL;
      spc_filter = NULL;
   }
#endif
}

#ifdef HAVE_SPCLIB
//
// I_TryLoadSPC
//
// haleyjd 04/02/08: Tries to load the music data as a SPC file.
//
int I_TryLoadSPC(void *data, int size)
{
   spc_err_t err;

#ifdef RANGECHECK
   if(snes_spc)
      I_Error("I_TryLoadSPC: snes_spc object displaced\n");
#endif

   snes_spc = spc_new();

   if(snes_spc == NULL)
   {
      doom_printf("Failed to allocate snes_spc");
      return -1;
   }
   
   if((err = spc_load_spc(snes_spc, data, size)))
   {
      spc_delete(snes_spc);
      snes_spc = NULL;
      return -1;
   }

   // It is a SPC, so set everything up for SPC playing
   spc_filter = spc_filter_new();

   if(spc_filter == NULL)
   {
      doom_printf("Failed to allocate spc_filter");
      spc_delete(snes_spc);
      snes_spc = NULL;
      return -1;
   }

   // clear echo buffer garbage, init filter
   spc_clear_echo(snes_spc);
   spc_filter_clear(spc_filter);

   // set initial gain and bass parameters
   spc_filter_set_gain(spc_filter, snd_MusicVolume * (256 * spc_preamp) / 15);
   spc_filter_set_bass(spc_filter, spc_bass_boost);

   return 0;
}
#endif

//
// I_RegisterSong
//
int I_RegisterSong(void *data, int size)
{
   if(music != NULL)
      I_UnRegisterSong(1);
   
   rw    = SDL_RWFromMem(data, size);
   music = Mix_LoadMUS_RW(rw);
   
   // It's not recognized by SDL_mixer, is it a mus?
   if(music == NULL)
   {      
      int err;
      MIDI mididata;
      UBYTE *mid;
      int midlen;
      
      SDL_FreeRW(rw);
      rw = NULL;

      memset(&mididata, 0, sizeof(MIDI));
      
      if((err = mmus2mid((byte *)data, &mididata, 89, 0))) 
      {         
         // Nope, not a mus.

#ifdef HAVE_SPCLIB
         // Is it a SPC?
         if(!(err = I_TryLoadSPC(data, size)))
            return 1;
#endif
         doom_printf(FC_ERROR "Error loading music: %d", err);
         return 0;
      }

      // Hurrah! Let's make it a mid and give it to SDL_mixer
      MIDIToMidi(&mididata, &mid, &midlen);
      rw    = SDL_RWFromMem(mid, midlen);
      music = Mix_LoadMUS_RW(rw);

      if(music == NULL) 
      {   
         // Conversion failed, free everything
         SDL_FreeRW(rw);
         rw = NULL;
         free(mid);         
      } 
      else 
      {   
         // Conversion succeeded
         // -> save memory block to free when unregistering
         music_block = mid;
      }
   }
   
   // the handle is a simple boolean
   return music != NULL;
}

//
// I_QrySongPlaying
//
// Is the song playing?
//
int I_QrySongPlaying(int handle)
{
   // haleyjd: this is never called
   // julian: and is that a reason not to code it?!?
   // haleyjd: ::shrugs::
#ifdef HAVE_SPCLIB
   return CHECK_MUSIC(handle) || snes_spc != NULL;
#else
   return CHECK_MUSIC(handle);
#endif
}

/************************
        CONSOLE COMMANDS
 ************************/

// system specific sound console commands

static char *sndcardstr[] = { "SDL mixer", "none" };
static char *muscardstr[] = { "SDL mixer", "none" };

VARIABLE_INT(snd_card,       NULL,      -1,  0, sndcardstr);
VARIABLE_INT(mus_card,       NULL,      -1,  0, muscardstr);
VARIABLE_INT(detect_voices,  NULL,       0,  1, yesno);

#ifdef HAVE_SPCLIB
VARIABLE_INT(spc_preamp,     NULL,       1,  6, NULL);
VARIABLE_INT(spc_bass_boost, NULL,       0, 31, NULL);
#endif

CONSOLE_VARIABLE(snd_card, snd_card, 0) 
{
   if(snd_card != 0 && !snd_init && menuactive)
      MN_ErrorMsg("you must restart the program to turn on sound");
}

CONSOLE_VARIABLE(mus_card, mus_card, 0)
{
   if(mus_card != 0 && !mus_init && menuactive)
      MN_ErrorMsg("you must restart the program to turn on music");

   if(mus_card == 0)
      S_StopMusic();
}

CONSOLE_VARIABLE(detect_voices, detect_voices, 0) {}

#ifdef HAVE_SPCLIB
CONSOLE_VARIABLE(snd_spcpreamp, spc_preamp, 0) 
{
   I_SetMusicVolume(snd_MusicVolume);
}

CONSOLE_VARIABLE(snd_spcbassboost, spc_bass_boost, 0)
{
   if(snes_spc)
      spc_filter_set_bass(spc_filter, spc_bass_boost);
}
#endif

void I_Sound_AddCommands(void)
{
   C_AddCommand(snd_card);
   C_AddCommand(mus_card);
   C_AddCommand(detect_voices);

#ifdef HAVE_SPCLIB
   C_AddCommand(snd_spcpreamp);
   C_AddCommand(snd_spcbassboost);
#endif
}


//----------------------------------------------------------------------------
//
// $Log: i_sound.c,v $
//
//----------------------------------------------------------------------------
