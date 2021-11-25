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

#include <optional>

#include "SDL.h"
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
#include "../m_compare.h"
#include "../d_gi.h"
#include "../s_sound.h"
#include "../mn_engin.h"
#include "../m_utils.h"

#ifdef HAVE_SPCLIB
#include "../../snes_spc/spc.h"
#endif

#ifdef HAVE_ADLMIDILIB
#include "adlmidi.h"
#endif

#if EE_FEATURE_MIDIRPC
#include <windows.h>
#pragma comment(lib, "Winmm.lib")

#include "../Win32/midifile.h"
#include "../Win32/i_winmusic.h"

#include "../Win32/i_winversion.h"
#endif

extern int audio_buffers;
extern bool float_samples;
extern SDL_AudioSpec audio_spec;

#define STEP 2
#define STEPSHIFT 1

#ifdef HAVE_SPCLIB
// haleyjd 05/02/08: SPC support
static SNES_SPC   *snes_spc   = nullptr;
static SPC_Filter *spc_filter = nullptr;

int spc_preamp     = 1;
int spc_bass_boost = 8;

void I_SDLMusicSetSPCBass(void)
{
   if(snes_spc)
      spc_filter_set_bass(spc_filter, spc_bass_boost);
}
//
// SDL_mixer effect processor; mixes SPC data into the postmix stream.
//
template<typename T>
static void I_effectSPC(void *udata, Uint8 *stream, int len)
{
   static constexpr int SAMPLESIZE = sizeof(T);

   T *leftout, *rightout, *leftend;
   Sint16 *datal, *datar;
   int numsamples, spcsamples;
   int stepremainder = 0, i = 0;

   static Sint16 *spc_buffer = nullptr;
   static int lastspcsamples = 0;

   leftout  = reinterpret_cast<T *>(stream);
   rightout = reinterpret_cast<T *>(stream) + 1;

   numsamples = len / SAMPLESIZE;
   leftend = leftout + numsamples;

   // round samples up to higher even number
   spcsamples = ((static_cast<int>(numsamples * 320.0 / 441.0) & ~1) + 2) * 2 / audio_spec.channels;

   // realloc spc buffer?
   if(spcsamples != lastspcsamples)
   {
      // add extra buffer samples at end for filtering safety; stereo channels
      spc_buffer = static_cast<Sint16 *>(Z_SysRealloc(spc_buffer,
                                                      (spcsamples + 2) * 2 * sizeof(Sint16)));
      lastspcsamples = spcsamples;
   }

   // get spc samples
   if(spc_play(snes_spc, spcsamples, static_cast<short *>(spc_buffer)))
      return;

   spc_filter_run(spc_filter, static_cast<short *>(spc_buffer), spcsamples);

   datal = spc_buffer;
   datar = spc_buffer + 1;

   while(leftout != leftend)
   {
      // linear filter spc samples to the output buffer, since the
      // sample rate is higher (44.1 kHz vs. 32 kHz).

      if constexpr(std::is_same_v<T, Sint16>)
      {
         const Sint32 dl = *leftout  + ((static_cast<Sint32>(datal[0]) * (0x10000 - stepremainder) +
            static_cast<Sint32>(datal[2]) * stepremainder) >> 16);
         const Sint32 dr = *rightout + ((static_cast<Sint32>(datar[0]) * (0x10000 - stepremainder) +
            static_cast<Sint32>(datar[2]) * stepremainder) >> 16);
         *leftout  = static_cast<Sint16>(eclamp(dl, SHRT_MIN, SHRT_MAX));
         *rightout = static_cast<Sint16>(eclamp(dr, SHRT_MIN, SHRT_MAX));

      }
      else if constexpr(std::is_same_v<T, float>)
      {
         const float dl = *leftout  + ((static_cast<Sint32>(datal[0]) * (0x10000 - stepremainder) +
            static_cast<Sint32>(datal[2]) * stepremainder) >> 16) * (1.0f / 32768.0f);
         const float dr = *rightout + ((static_cast<Sint32>(datar[0]) * (0x10000 - stepremainder) +
            static_cast<Sint32>(datar[2]) * stepremainder) >> 16) * (1.0f / 32768.0f);
         *leftout  = eclamp(dl, -1.0f, 1.0f);
         *rightout = eclamp(dr, -1.0f, 1.0f);
      }
      static_assert(std::is_same_v<T, Sint16> || std::is_same_v<T, float>,
                    "I_effectSPC called with incompatible template parameter");

      stepremainder += ((32000 << 16) / 44100);

      i += (stepremainder >> 16);

      datal = spc_buffer + (i << STEPSHIFT);
      datar = datal + 1;

      // trim remainder to decimal portion
      stepremainder &= 0xffff;

      // step to next sample in mixer buffer
      leftout  += audio_spec.channels;
      rightout += audio_spec.channels;
   }
}
#endif

#ifdef HAVE_ADLMIDILIB
static ADL_MIDIPlayer *adlmidi_player = nullptr;
volatile bool adlplaying = false;

int midi_device      = 0;
int adlmidi_numchips = 2;
int adlmidi_bank     = 72;
int adlmidi_emulator = 0;

//
// Play a MIDI via libADLMIDI
//
template<typename T>
static void I_effectADLMIDI(void *udata, Uint8 *stream, int len)
{
   static constexpr unsigned int ADLMIDISTEP = sizeof(T);
   static T   *adlmidi_buffer     = nullptr;
   static int  lastadlmidisamples = 0;

   adlplaying = true;
   // TODO: Remove the exiting check once all atexit calls are erradicated
   if(Mix_PausedMusic()) //if(exiting || Mix_PausedMusic())
   {
      adlplaying = false;
      return;
   }

   ADLMIDI_AudioFormat fmt = { ADLMIDI_SampleType_S16, ADLMIDISTEP, ADLMIDISTEP * audio_spec.channels };
   if constexpr(std::is_same_v<T, Sint16>)
      fmt.type = ADLMIDI_SampleType_S16;
   else if constexpr(std::is_same_v<T, float>)
      fmt.type = ADLMIDI_SampleType_F32;
   static_assert(std::is_same_v<T, Sint16> || std::is_same_v<T, float>,
                 "I_effectADLMIDI called with incompatible template parameter");

   const int numsamples = (len * 2) / fmt.sampleOffset;

   // realloc ADLMIDI buffer?
   if(numsamples != lastadlmidisamples)
   {
      // add extra buffer samples at end for filtering safety; stereo channels
      adlmidi_buffer = static_cast<T *>(Z_SysRealloc(adlmidi_buffer, len));
      lastadlmidisamples = numsamples;
   }

   ADL_UInt8 *const l_out = reinterpret_cast<ADL_UInt8 *>(adlmidi_buffer);
   ADL_UInt8 *const r_out = reinterpret_cast<ADL_UInt8 *>(adlmidi_buffer + 1);
   const int gotlen = adl_playFormat(adlmidi_player, numsamples, l_out, r_out, &fmt) *
                      fmt.sampleOffset / 2;
   if(snd_MusicVolume == SND_MAXVOLUME)
      memcpy(stream, reinterpret_cast<Uint8 *>(adlmidi_buffer), size_t(gotlen));
   else
   {
      SDL_MixAudioFormat(
         stream, reinterpret_cast<Uint8 *>(adlmidi_buffer), audio_spec.format,
         gotlen, (snd_MusicVolume * 128) / SND_MAXVOLUME
      );
   }
   adlplaying = false;
}

#endif

//
// MUSIC API.
//

#ifdef EE_FEATURE_MIDIRPC
static bool winMIDIStreamOpened = false;

static std::optional<DWORD> initialVolume;
#endif

// julian (10/25/2005): rewrote (nearly) entirely

#include "mmus2mid.h"

// Only one track at a time
static Mix_Music *music = nullptr;

// Some tracks are directly streamed from the RWops;
// we need to free them in the end
static SDL_RWops *rw = nullptr;

// Same goes for buffers that were allocated to convert music;
// since this concerns mus, we could do otherwise but this 
// approach is better for consistency
static void *music_block = nullptr;

// Macro to make code more readable
#define CHECK_MUSIC(h) ((h) && music != nullptr)

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
   if(winMIDIStreamOpened)
   {
      I_WIN_ShutdownMusic();
      winMIDIStreamOpened = false;
   }
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

extern bool I_GenSDLAudioSpec(int, SDL_AudioFormat, int, int);

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

   // Figure out mix buffer sizes
   if(!I_GenSDLAudioSpec(44100, MIX_DEFAULT_FORMAT, 2, audio_buffers))
   {
      printf("Couldn't determine sound mixing buffer size.\n");
      nomusicparm = true;
      return 0;
   }

   if(Mix_OpenAudio(audio_spec.freq, audio_spec.format, audio_spec.channels, audio_spec.samples) < 0)
      return 0;

   float_samples = SDL_AUDIO_ISFLOAT(audio_spec.format);

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
   winMIDIStreamOpened = I_WIN_InitMusic();
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
      Mix_HookMusic(float_samples ? I_effectSPC<float> : I_effectSPC<Sint16>, nullptr);
   else
#endif
#ifdef HAVE_ADLMIDILIB
      if(adlmidi_player)
      {
         Mix_HookMusic(float_samples ? I_effectADLMIDI<float> : I_effectADLMIDI<Sint16>, nullptr);
         adl_setLoopEnabled(adlmidi_player, looping);
      }
      else
#endif
#ifdef EE_FEATURE_MIDIRPC
   if(winMIDIStreamOpened)
      I_WIN_PlaySong(!!looping);
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
   // haleyjd 09/04/06: adjust to use scale from 0 to SND_MAXVOLUME
   Mix_VolumeMusic((volume * 128) / SND_MAXVOLUME);

#ifdef EE_FEATURE_MIDIRPC
   // adjust server volume
   I_WIN_SetMusicVolume(volume);
#endif

#ifdef HAVE_SPCLIB
   if(snes_spc)
   {
      // if a SPC is playing, set its volume too
      spc_filter_set_gain(spc_filter, volume * (256 * spc_preamp) / SND_MAXVOLUME);
   }
#endif
}

static int paused_midi_volume;

//
// I_SDLPauseSong
//
static void I_SDLPauseSong(int handle)
{
//#ifdef EE_FEATURE_MIDIRPC
//   if(serverMidiPlaying)
//   {
//      I_MidiRPCPauseSong();
//      return;
//   }
//#endif

   if(CHECK_MUSIC(handle))
   {
      // Not for mids (MaxW: except libADLMIDI!)
#ifdef HAVE_ADLMIDILIB
      if(Mix_GetMusicType(music) != MUS_MID || midi_device == 0)
#else
      if(Mix_GetMusicType(music) != MUS_MID)
#endif
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
//#ifdef EE_FEATURE_MIDIRPC
//   if(serverMidiPlaying)
//   {
//      I_MidiRPCResumeSong();
//      return;
//   }
//#endif

   if(CHECK_MUSIC(handle))
   {
      // Not for mids (MaxW: except libADLMIDI!)
#ifdef HAVE_ADLMIDILIB
      if(Mix_GetMusicType(music) != MUS_MID || midi_device == 0)
#else
      if(Mix_GetMusicType(music) != MUS_MID)
#endif
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
   if(winMIDIStreamOpened)
      I_WIN_StopSong();
#endif

   if(CHECK_MUSIC(handle))
      Mix_HaltMusic();

#ifdef HAVE_SPCLIB
   if(snes_spc)
      Mix_HookMusic(nullptr, nullptr);
#endif

#ifdef HAVE_ADLMIDILIB
   if(adlmidi_player)
      Mix_HookMusic(nullptr, nullptr);
#endif
}

//
// I_SDLUnRegisterSong
//
static void I_SDLUnRegisterSong(int handle)
{
#ifdef EE_FEATURE_MIDIRPC
   if(winMIDIStreamOpened)
      I_WIN_UnRegisterSong();
#endif

#ifdef HAVE_ADLMIDILIB
   if(adlmidi_player)
   {
      Mix_HookMusic(nullptr, nullptr);
      adl_close(adlmidi_player);
      adlmidi_player = nullptr;
   }
#endif

   if(CHECK_MUSIC(handle))
   {   
      // Stop and free song
      I_SDLStopSong(handle);
      Mix_FreeMusic(music);
     
      // Reinitialize all this
      music = nullptr;
      rw    = nullptr;
   }

   // Free music block
   if(music_block != nullptr)
   {
      efree(music_block);
      music_block = nullptr;
   }

#ifdef HAVE_SPCLIB
   if(snes_spc)
   {
      // be certain the callback is unregistered first
      Mix_HookMusic(nullptr, nullptr);

      // free the spc and filter objects
      spc_delete(snes_spc);
      spc_filter_delete(spc_filter);

      snes_spc   = nullptr;
      spc_filter = nullptr;
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

   if(snes_spc == nullptr)
   {
      doom_printf("Failed to allocate snes_spc");
      return -1;
   }
   
   if((err = spc_load_spc(snes_spc, data, (long)size)))
   {
      spc_delete(snes_spc);
      snes_spc = nullptr;
      return -1;
   }

   // It is a SPC, so set everything up for SPC playing
   spc_filter = spc_filter_new();

   if(spc_filter == nullptr)
   {
      doom_printf("Failed to allocate spc_filter");
      spc_delete(snes_spc);
      snes_spc = nullptr;
      return -1;
   }

   // clear echo buffer garbage, init filter
   spc_clear_echo(snes_spc);
   spc_filter_clear(spc_filter);

   // set initial gain and bass parameters
   spc_filter_set_gain(spc_filter, snd_MusicVolume * (256 * spc_preamp) / SND_MAXVOLUME);
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

   if(music != nullptr)
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

      rw = nullptr;
      memset(&mididata, 0, sizeof(MIDI));

      if((err = mmus2mid((byte *)data, (size_t)size, &mididata, 89, 0)))
      {
         doom_printf("Error loading music: %d", err);
         return 0;
      }

      // Hurrah! Let's make it a mid and give it to SDL_mixer
      MIDIToMidi(&mididata, &mid, &midlen);

      FreeMIDIData(&mididata);

      // save memory block to free when unregistering
      music_block = mid;

      data   = mid;
      size   = midlen;
      isMIDI = true;   // now it's a MIDI.
   }

#ifdef EE_FEATURE_MIDIRPC
#ifdef HAVE_ADLMIDILIB
   if(isMIDI && winMIDIStreamOpened && midi_device == -1)
#else
   if(isMIDI && winMIDIStreamOpened)
#endif
   {
      char *filename = M_TempFile("doom.mid");

      M_WriteFile(filename, data, size);

      if(winMIDIStreamOpened)
      {
         if(I_WIN_RegisterSong(filename))
            return 1;
         else
            music = nullptr;
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

#ifdef HAVE_ADLMIDILIB
   if(isMIDI && midi_device == 0)
   {
      adlmidi_player = adl_init(audio_spec.freq);
      adl_setNumChips(adlmidi_player, adlmidi_numchips);
      adl_setBank(adlmidi_player, adlmidi_bank);
      adl_switchEmulator(adlmidi_player, adlmidi_emulator);
      adl_setNumFourOpsChn(adlmidi_player, -1);
      if(adl_openData(adlmidi_player, data, static_cast<unsigned long>(size)) == 0)
         return 1;
      // Opening data went wrong
      adl_close(adlmidi_player);
      adlmidi_player = nullptr;
   }
#endif

   return music != nullptr;
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
   return
#ifdef HAVE_ADLMIDILIB
      adlmidi_player != nullptr ||
#endif
#ifdef HAVE_SPCLIB
      snes_spc != nullptr ||
#endif
      CHECK_MUSIC(handle);
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

