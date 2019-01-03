// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2017 James Haley, Stephen McGranahan, Julian Aubourg, et al.
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
//      SDL_mixer music interface
//
//-----------------------------------------------------------------------------

// haleyjd 11/22/08: I don't understand why this is needed here...
#define USE_RWOPS

#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_thread.h"
#include "SDL_mixer.h"

#include "i_midirpc.h"

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
#include "../../subprojects/snes_spc/spc.h"
#endif

extern int audio_buffers;

#define STEP sizeof(Sint16)
#define STEPSHIFT 1

#ifdef HAVE_SPCLIB
// haleyjd 05/02/08: SPC support
static SNES_SPC   *snes_spc   = NULL;
static SPC_Filter *spc_filter = NULL;

int spc_preamp     = 1;
int spc_bass_boost = 8;

void I_SDLMusicSetSPCBass(void)
{
   if(snes_spc)
      spc_filter_set_bass(spc_filter, spc_bass_boost);
}

//
// I_EffectSPC
//
// SDL_mixer effect processor; mixes SPC data into the postmix stream.
//
//static void I_EffectSPC(int chan, void *stream, int len, void *udata)
static void I_EffectSPC(void *udata, Uint8 *stream, int len)
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
      spc_buffer = (Sint16 *)(Z_SysRealloc(spc_buffer, 
                                          (spcsamples + 2) * 2 * sizeof(Sint16)));
      lastspcsamples = spcsamples;
   }

   // get spc samples
   if(spc_play(snes_spc, spcsamples, (short *)spc_buffer))
      return;

   // filter samples
   spc_filter_run(spc_filter, (short *)spc_buffer, spcsamples);

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
// MUSIC API.
//

#ifdef EE_FEATURE_MIDIRPC
static bool haveMidiServer;
static bool haveMidiClient;
static bool serverMidiPlaying;
#endif

// julian (10/25/2005): rewrote (nearly) entirely

#include "mmus2mid.h"

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

static void I_SDLUnRegisterSong(int);

//
// I_SDLShutdownMusic
//
// atexit handler.
//
static void I_SDLShutdownMusic(void)
{
   I_SDLUnRegisterSong(1);

#ifdef EE_FEATURE_MIDIRPC
   I_MidiRPCClientShutDown();
#endif
}

//
// I_SDLShutdownSoundForMusic
//
// Registered only if sound is initialized by the music module.
//
static void I_SDLShutdownSoundForMusic(void)
{
   Mix_CloseAudio();
}

//
// I_SDLInitSoundForMusic
//
// Called only if sound was not initialized by I_InitSound
//
static int I_SDLInitSoundForMusic(void)
{
   // haleyjd: the docs say we should do this
   if(SDL_InitSubSystem(SDL_INIT_AUDIO))
      return 0;

   if(!I_IsSoundBufferSizePowerOf2(audio_buffers))
      audio_buffers = I_MakeSoundBufferSize(audio_buffers);
   
   if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, audio_buffers) < 0)
      return 0;

   return 1;
}

extern bool snd_init;

//
// I_SDLInitMusic
//
static int I_SDLInitMusic(void)
{
   int success = 0;

   // if sound was not initialized, we must initialize it
   if(!snd_init)
   {
      if((success = I_SDLInitSoundForMusic()))
         atexit(I_SDLShutdownSoundForMusic);
   }
   else
      success = 1;

#ifdef EE_FEATURE_MIDIRPC
   // Initialize RPC server
   haveMidiServer = I_MidiRPCInitServer();
#endif

   return success;
}

//
// I_SDLPlaySong
//
// Plays the currently registered song.
//
static void I_SDLPlaySong(int handle, int looping)
{
#ifdef HAVE_SPCLIB
   // if a SPC is set up, play it.
   if(snes_spc)
      Mix_HookMusic(I_EffectSPC, NULL);
   else
#endif
#ifdef EE_FEATURE_MIDIRPC
   if(serverMidiPlaying)
      I_MidiRPCPlaySong(!!looping);
   else
#endif
   if(CHECK_MUSIC(handle) && Mix_PlayMusic(music, looping ? -1 : 0) == -1)
   {
      doom_printf("I_PlaySong: Mix_PlayMusic failed\n");
      return;
   }
}

//
// I_SDLSetMusicVolume
//
static void I_SDLSetMusicVolume(int volume)
{
   // haleyjd 09/04/06: adjust to use scale from 0 to 15
   Mix_VolumeMusic((volume * 128) / 15);

#ifdef EE_FEATURE_MIDIRPC
   // adjust server volume
   I_MidiRPCSetVolume(volume);
#endif

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
// I_SDLPauseSong
//
static void I_SDLPauseSong(int handle)
{
#ifdef EE_FEATURE_MIDIRPC
   if(serverMidiPlaying)
   {
      I_MidiRPCPauseSong();
      return;
   }
#endif

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

//
// I_SDLResumeSong
//
static void I_SDLResumeSong(int handle)
{
#ifdef EE_FEATURE_MIDIRPC
   if(serverMidiPlaying)
   {
      I_MidiRPCResumeSong();
      return;
   }
#endif

   if(CHECK_MUSIC(handle))
   {
      // Not for mids
      if(Mix_GetMusicType(music) != MUS_MID)
         Mix_ResumeMusic();
      else
         Mix_VolumeMusic(paused_midi_volume);
   }
}

//
// I_SDLStopSong
//
static void I_SDLStopSong(int handle)
{
#ifdef EE_FEATURE_MIDIRPC
   if(serverMidiPlaying)
   {
      I_MidiRPCStopSong();
      serverMidiPlaying = false;
   }
#endif

   if(CHECK_MUSIC(handle))
      Mix_HaltMusic();

#ifdef HAVE_SPCLIB
   if(snes_spc)
      Mix_HookMusic(NULL, NULL);
#endif
}

//
// I_SDLUnRegisterSong
//
static void I_SDLUnRegisterSong(int handle)
{
#ifdef EE_FEATURE_MIDIRPC
   if(serverMidiPlaying)
   {
      I_MidiRPCStopSong();
      serverMidiPlaying = false;
   }
#endif

   if(CHECK_MUSIC(handle))
   {   
      // Stop and free song
      I_SDLStopSong(handle);
      Mix_FreeMusic(music);
     
      // Reinitialize all this
      music = NULL;
      rw    = NULL;
   }

   // Free music block
   if(music_block != NULL)
   {
      efree(music_block);
      music_block = NULL;
   }

#ifdef HAVE_SPCLIB
   if(snes_spc)
   {
      // be certain the callback is unregistered first
      Mix_HookMusic(NULL, NULL);

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
static int I_TryLoadSPC(void *data, int size)
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
   
   if((err = spc_load_spc(snes_spc, data, (long)size)))
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
// I_SDLRegisterSong
//
static int I_SDLRegisterSong(void *data, int size)
{
   int  err;
   bool isMIDI = false;
   bool isMUS  = false;

   if(music != NULL)
      I_UnRegisterSong(1);

   // Check for MIDI or MUS format first:
   if(size >= 14)
   {
      if(!memcmp(data, "MThd", 4)) // Is it a MIDI?
         isMIDI = true;
      else if(mmuscheckformat((byte *)data, size)) // Is it a MUS?
         isMUS = true;
   }

   // If it's a MUS, convert it to MIDI now.
   if(isMUS)
   {
      MIDI mididata;
      UBYTE *mid;
      int midlen;

      rw = NULL;
      memset(&mididata, 0, sizeof(MIDI));

      if((err = mmus2mid((byte *)data, (size_t)size, &mididata, 89, 0)))
      {
         doom_printf("Error loading music: %d", err);
         return 0;
      }

      // Hurrah! Let's make it a mid and give it to SDL_mixer
      MIDIToMidi(&mididata, &mid, &midlen);

      // save memory block to free when unregistering
      music_block = mid;

      data   = mid;
      size   = midlen;
      isMIDI = true;   // now it's a MIDI.
   }
   
#ifdef EE_FEATURE_MIDIRPC
   // Check for option to invoke RPC server if isMIDI
   if(isMIDI && haveMidiServer)
   {
      // Init client if not yet started
      if(!haveMidiClient)
         haveMidiClient = I_MidiRPCInitClient();

      if(I_MidiRPCRegisterSong(data, size))
      {
         serverMidiPlaying = true;
         return 1; // server will play this song.
      }
   }
#endif

   // Try SDL_mixer locally
   rw    = SDL_RWFromMem(data, size);
   music = Mix_LoadMUS_RW(rw, true);

#ifdef HAVE_SPCLIB
   if(!music)
   {
      // Is it a SPC?
      if(!(err = I_TryLoadSPC(data, size)))
         return 1;
   }
#endif

   return music != NULL;
}

//
// I_SDLQrySongPlaying
//
// Is the song playing?
//
static int I_SDLQrySongPlaying(int handle)
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

i_musicdriver_t i_sdlmusicdriver =
{
   I_SDLInitMusic,      // InitMusic
   I_SDLShutdownMusic,  // ShutdownMusic
   I_SDLSetMusicVolume, // SetMusicVolume
   I_SDLPauseSong,      // PauseSong
   I_SDLResumeSong,     // ResumeSong
   I_SDLRegisterSong,   // RegisterSong
   I_SDLPlaySong,       // PlaySong
   I_SDLStopSong,       // StopSong
   I_SDLUnRegisterSong, // UnRegisterSong
   I_SDLQrySongPlaying, // QrySongPlaying
};

// EOF

