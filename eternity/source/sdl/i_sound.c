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

#ifdef LINUX
// haleyjd 01/28/07: I don't understand why this is needed here...
extern DECLSPEC Mix_Music * SDLCALL Mix_LoadMUS_RW(SDL_RWops *rw);
#endif

#if 0
// haleyjd: debug log for essel

static FILE *sound_log = NULL;

static void DebugOpenSndLog(void)
{
   sound_log = fopen("sound.log", "w");
}

static void DebugCloseSndLog(void)
{
   if(sound_log)
      fclose(sound_log);
}

static void DebugSndLog(const char *msg, ...)
{
   if(sound_log)
   {
      va_list args;
      
      va_start(args, msg);
      vfprintf(sound_log, msg, args);
      fflush(sound_log);
      va_end(args);
   }
}
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
  // Time/gametic that the channel started playing,
  //  used to determine oldest, which automatically
  //  has lowest priority.
  // In case number of active sounds exceeds
  //  available channels.
  int starttime;
  // Hardware left and right channel volume lookup.
  int *leftvol_lookup;
  int *rightvol_lookup;
  // haleyjd 02/18/05: channel lock -- do not modify when locked!
  volatile int lock;
  // haleyjd 06/03/06: looping
  int loop;
  int idnum;
} channel_info_t;

channel_info_t channelinfo[MAX_CHANNELS];

// Pitch to stepping lookup, unused.
int steptable[256];

// Volume lookups.
int vol_lookup[128*256];

/* cph 
 * stopchan
 * Stops a sound, unlocks the data 
 */
static void stopchan(int handle)
{
   int cnum;

   // haleyjd 02/18/05: bounds checking
   if(!snd_init || handle < 0 || handle >= MAX_CHANNELS)
      return;

   // haleyjd 02/18/05: sound channel locking in case of 
   // multithreaded access to channelinfo[].data. Make Eternity
   // sleep for the minimum timeslice to give another thread
   // chance to clear the lock.
   while(channelinfo[handle].lock)
      SDL_Delay(1);

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
               return; // still being used by some channel
         }
         
         // set sample to PU_CACHE level
         Z_ChangeTag(channelinfo[handle].id->data, PU_CACHE);
      }
   }

   channelinfo[handle].id = NULL;
}

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
static boolean addsfx(sfxinfo_t *sfx, int channel)
{
   size_t len;
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
      lump = W_GetNumForName(gameModeInfo->defSoundName);
   
   len = W_LumpLength(lump);
   
   // haleyjd 10/08/04: do not play zero-length sound lumps
   if(len <= 8)
      return false;

   // haleyjd 06/03/06: rewrote again to make sound data properly freeable
   if(sfx->data == NULL)
   {   
      byte *data;

      // haleyjd: this should always be called (if lump is already loaded,
      // W_CacheLumpNum handles that for us).
      data = (byte *)W_CacheLumpNum(lump, PU_STATIC);

      // Check the header, and ensure this is a valid sound
      if(data[0] != 0x03 || data[1] != 0x00 || 
         data[6] != 0x00 || data[7] != 0x00)
      {
         Z_ChangeTag(data, PU_CACHE);
         return false;
      }
      
      // haleyjd 06/03/06
      sfx->data = Z_Malloc(len, PU_STATIC, &sfx->data);
      memcpy(sfx->data, data, len);

      // haleyjd 06/03/06: don't need original lump data any more
      Z_ChangeTag(data, PU_CACHE);

   }
   else
      Z_ChangeTag(sfx->data, PU_STATIC); // reset to static cache level

   channelinfo[channel].data = sfx->data;

   /* Find padded length */   
   len -= 8;
   
   /* Set pointer to end of raw data. */
   channelinfo[channel].enddata = channelinfo[channel].data + len - 1;
   channelinfo[channel].samplerate = 
      (channelinfo[channel].data[3] << 8) + channelinfo[channel].data[2];
   channelinfo[channel].data += 8; /* Skip header */

   // haleyjd 06/03/06: keep track of start of sound
   channelinfo[channel].startdata = channelinfo[channel].data;
   
   channelinfo[channel].stepremainder = 0;
   // Should be gametic, I presume.
   channelinfo[channel].starttime = gametic;
   
   // Preserve sound SFX id
   channelinfo[channel].id = sfx;
   
   return true;
}

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

   // Set stepping
   // MWM 2000-12-24: Calculates proportion of channel samplerate
   // to global samplerate for mixing purposes.
   // Patched to shift left *then* divide, to minimize roundoff errors
   // as well as to use SAMPLERATE as defined above, not to assume 11025 Hz
   if(pitched_sounds)
      channelinfo[slot].step = step + (((channelinfo[slot].samplerate<<16)/snd_samplerate)-65536);
   else
      channelinfo[slot].step = ((channelinfo[slot].samplerate<<16)/snd_samplerate);
   
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
   
   // Get the proper lookup table piece
   //  for this volume level???
   channelinfo[slot].leftvol_lookup = &vol_lookup[leftvol*256];
   channelinfo[slot].rightvol_lookup = &vol_lookup[rightvol*256];
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
   // I fail to see that this is currently used.
   for(i=-128 ; i<128 ; i++)
   {
      steptablemid[i] = (int)(pow(1.2, ((double)i/(64.0*snd_samplerate/11025)))*65536.0);
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
}

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
      if(!I_SoundIsPlaying(handle))
         break;
   }

   // all used? don't play the sound. It's preferable to miss a sound
   // than it is to cut off one already playing, which sounds weird.
   if(handle == MAX_CHANNELS)
      return -1;

   // haleyjd 02/18/05: cannot proceed until channel is unlocked
   while(channelinfo[handle].lock)
      SDL_Delay(1);
 
   if(addsfx(sound, handle))
   {
      channelinfo[handle].idnum = id++; // give the sound a unique id
      channelinfo[handle].loop = loop;
      updateSoundParams(handle, vol, sep, pitch);
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
// haleyjd: wow, this can actually do something in the Windows
// version :P
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
// haleyjd: this version does nothing with respect to digital sound
// in Windows; sound-updating is done via the SDL callback function 
// below
// 
void I_UpdateSound(void)
{
}

static void I_SDLUpdateSound(void *userdata, Uint8 *stream, int len)
{
   // Mix current sound data.
   // Data, from raw sound, for right and left.
   register unsigned char sample;
   register int dl;
   register int dr;
   
   // Pointers in audio stream, left, right, end.
   short *leftout;
   short *rightout;
   short *leftend;

   // Step in stream, left and right, thus two.
   int step;
   
   // Mixing channel index.
   int chan;

   // haleyjd 02/18/05: error check
   if(!stream)
      return;
   
   // Left and right channel
   //  are in audio stream, alternating.
   leftout  = (signed short *)stream;
   rightout = ((signed short *)stream)+1;
   step = 2;
   
   // Determine end, for left channel only
   //  (right channel is implicit).
   leftend = leftout + (len / 4) * step;

#if 0
   DebugSndLog("update sound start: ud=%p, s=%p, len=%d; lo=%p, ro=%p, le=%p\n", 
               userdata, stream, len, leftout, rightout, leftend);
#endif

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
         if(channelinfo[chan].data)
         {
            // haleyjd 02/18/05: lock the channel to prevent possible race 
            // conditions in the below loop that could modify 
            // channelinfo[chan].data while it's being used here.
            channelinfo[chan].lock = 1;

            // Get the raw data from the channel. 
            // no filtering
            // sample = *channelinfo[chan].data;
            // linear filtering
            sample = (((unsigned int)channelinfo[chan].data[0] * (0x10000 - channelinfo[chan].stepremainder))
               + ((unsigned int)channelinfo[chan].data[1] * (channelinfo[chan].stepremainder))) >> 16;
            
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

#if 0
            DebugSndLog("channel %d: dl=%d, dr=%d, step=%d, stepremainder=%d, data=%p\n",
                        chan, dl, dr, channelinfo[chan].step, 
                        channelinfo[chan].stepremainder, channelinfo[chan].data);
#endif
            
            // Check whether we are done.
            if(channelinfo[chan].data >= channelinfo[chan].enddata)
            {
#if 0
               DebugSndLog("stopping channel %d\n", chan);
#endif
               // haleyjd 02/18/05: unlock channel
               channelinfo[chan].lock = 0;

               if(channelinfo[chan].loop &&
                  !(paused || (menuactive && !demoplayback && !netgame)))
               {
                  // haleyjd 06/03/06: restart a looping sample if not paused
                  channelinfo[chan].data = channelinfo[chan].startdata;
                  channelinfo[chan].stepremainder = 0;
                  channelinfo[chan].starttime = gametic;
#if 0
                  DebugSndLog("looping channel %d\n", chan);
#endif
               }
               else
                  stopchan(chan);
            }
            // haleyjd 02/18/05: unlock channel
            channelinfo[chan].lock = 0;
         }
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
      leftout += step;
      rightout += step;

#if 0
      DebugSndLog("leftout = %p, rightout = %p\n", leftout, rightout);
#endif
   }

#if 0
   DebugSndLog("end of updatesound\n");
#endif
}

// This would be used to write out the mixbuffer
//  during each game loop update.
// Updates sound buffer and audio device at runtime.
// It is called during Timer interrupt with SNDINTR.

void I_SubmitSound(void)
{
   // haleyjd: whatever it is, SDL doesn't need it either
}

void I_ShutdownSound(void)
{
   if(snd_init)
   {
      Mix_CloseAudio();
      snd_init = 0;

#if 0
      DebugCloseSndLog();
#endif
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
         lump = W_GetNumForName(gameModeInfo->defSoundName);

      W_CacheLumpNum(lump, PU_CACHE);
   }
}

// SoM 9/14/02: Rewrite. code taken from prboom to use SDL_Mixer

void I_InitSound(void)
{   
   if(!nosfxparm)
   {
      int audio_buffers;

      puts("I_InitSound: ");

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

      SAMPLECOUNT = audio_buffers;
      Mix_SetPostMix(I_SDLUpdateSound, NULL);
      printf("Configured audio device with %d samples/slice.\n", SAMPLECOUNT);

      atexit(I_ShutdownSound);

      snd_init = true;

      // haleyjd 04/11/03: don't use music if sfx aren't init'd
      // (may be dependent, docs are unclear)
      if(!nomusicparm)
         I_InitMusic();

#if 0
      DebugOpenSndLog();
#endif
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

void I_ShutdownMusic(void)
{
   I_UnRegisterSong(1);
}

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

void I_PlaySong(int handle, int looping)
{
   if(CHECK_MUSIC(handle) && Mix_PlayMusic(music, looping ? -1 : 0) == -1)
   {
      doom_printf("I_PlaySong: Mix_PlayMusic failed\n");
      return;
   }
   
   // haleyjd 10/28/05: make sure volume settings remain consistent
   I_SetMusicVolume(snd_MusicVolume);
}

void I_SetMusicVolume(int volume)
{
   // haleyjd 09/04/06: adjust to use scale from 0 to 15
   Mix_VolumeMusic((volume * 128) / 15);
}

static int paused_midi_volume;

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
}

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
}

void I_StopSong(int handle)
{
   if(CHECK_MUSIC(handle))
      Mix_HaltMusic();
}

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
}

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
         // Nope, not a mus
         doom_printf(FC_ERROR "Error loading midi: %d", err);
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

// Is the song playing?
int I_QrySongPlaying(int handle)
{
   // haleyjd: this is never called
   // julian: and is that a reason not to code it?!?
   // haleyjd: ::shrugs::
   return CHECK_MUSIC(handle);
}

/************************
        CONSOLE COMMANDS
 ************************/

// system specific sound console commands

static char *sndcardstr[] = { "SDL mixer", "none" };
static char *muscardstr[] = { "SDL mixer", "none" };

VARIABLE_INT(snd_card, NULL,           -1, 0, sndcardstr);
VARIABLE_INT(mus_card, NULL,           -1, 0, muscardstr);
VARIABLE_INT(detect_voices, NULL,       0, 1, yesno);

CONSOLE_VARIABLE(snd_card, snd_card, 0) 
{
   if(!snd_init && menuactive)
      MN_ErrorMsg("you must restart the program to turn on sound");
}

CONSOLE_VARIABLE(mus_card, mus_card, 0)
{
   if(!mus_init && menuactive)
      MN_ErrorMsg("you must restart the program to turn on music");

   if(mus_card == 0)
      S_StopMusic();
}

CONSOLE_VARIABLE(detect_voices, detect_voices, 0) {}

void I_Sound_AddCommands(void)
{
   C_AddCommand(snd_card);
   C_AddCommand(mus_card);
   C_AddCommand(detect_voices);
}


//----------------------------------------------------------------------------
//
// $Log: i_sound.c,v $
//
//----------------------------------------------------------------------------
