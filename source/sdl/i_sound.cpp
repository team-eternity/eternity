// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley, Stephen McGranahan, Julian Aubourg, et al.
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
//      Main system interface for sound.
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"

#include "../c_runcmd.h"
#include "../c_io.h"
#include "../d_main.h"
#include "../doomstat.h"
#include "../g_game.h"     //jff 1/21/98 added to use dprintf in I_RegisterSong
#include "../i_sound.h"
#include "../i_system.h"
#include "../m_argv.h"
#include "../m_misc.h"
#include "../mn_engin.h"
#include "../s_sound.h"

#ifdef HAVE_ADLMIDILIB
#include "adlmidi.hpp"
#endif

int snd_card;   // default.cfg variables for digi and midi drives
int mus_card;   // jff 1/18/98

// haleyjd: safety variables to keep changes to *_card from making
// these routines think that sound has been initialized when it hasn't
bool snd_init = false;
bool mus_init = false;

int detect_voices; //jff 3/4/98 enables voice detection prior to install_sound
//jff 1/22/98 make these visible here to disable sound/music on install err

// haleyjd 04/21/10: equalization parameters
double  s_lowfreq;   // low band cutoff frequency
double  s_highfreq;  // high band cutoff frequency
double  s_eqpreamp;  // preamp factor
double  s_lowgain;   // low band gain
double  s_midgain;   // mid band gain
double  s_highgain;  // high band gain

bool    s_reverbactive; // reverberation effects processing is active

// haleyjd 11/07/08: driver objects
static i_sounddriver_t *i_sounddriver;
static i_musicdriver_t *i_musicdriver;

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
   if(snd_init)
      i_sounddriver->UpdateSoundParams(handle, vol, sep, pitch);
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
// I_StartSound
//
int I_StartSound(sfxinfo_t *sound, int cnum, int vol, int sep, int pitch, 
                 int pri, int loop, bool reverb)
{   
   return snd_init ? 
      i_sounddriver->StartSound(sound, cnum, vol, sep, pitch, pri, loop, reverb) : -1;
}

//
// I_StopSound
//
// Stop the sound. Necessary to prevent runaway chainsaw,
// and to stop rocket launches when an explosion occurs.
//
void I_StopSound(int handle, int id)
{
   if(snd_init)
      i_sounddriver->StopSound(handle, id);
}

//
// I_SoundIsPlaying
//
int I_SoundIsPlaying(int handle)
{ 
   return snd_init ? i_sounddriver->SoundIsPlaying(handle) : 0;
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
   return snd_init ? i_sounddriver->SoundID(handle) : 0;
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
   if(snd_init)
      i_sounddriver->UpdateSound();
}


//
// I_SubmitSound
//
// This would be used to write out the mixbuffer
//  during each game loop update.
// Updates sound buffer and audio device at runtime.
// It is called during Timer interrupt with SNDINTR.
//
void I_SubmitSound(void)
{
   if(snd_init)
      i_sounddriver->SubmitSound();
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
      i_sounddriver->ShutdownSound();
      snd_init = false;
   }
}

//
// I_CacheSound
//
// Precache sound lumps for faster loading.
//
void I_CacheSound(sfxinfo_t *sound)
{
   if(!snd_init)
      return;

   // Note that the entire sound directory is swept by the EDF code when it
   // calls this function, so it shouldn't be necessary to deal with sound
   // aliases, links, or random redirections, as those all refer to other
   // sounds. So will will simply ignore such sounds here.

   if(!(sound->alias || sound->link || sound->randomsounds))
      i_sounddriver->CacheSound(sound);
}

// haleyjd 11/07/08: sound driver objects

#ifdef _SDL_VER
extern i_sounddriver_t i_sdlsound_driver;
extern i_sounddriver_t i_pcsound_driver;
#endif

//
// I_InitSound
//
void I_InitSound(void)
{   
   if(!nosfxparm)
   {
      printf("I_InitSound: ");

      // FIXME/TODO: initialize sound driver
      switch(snd_card)
      {
#ifdef _SDL_VER
      case -1:
         i_sounddriver = &i_sdlsound_driver;
         if(i_sounddriver->InitSound())
         {
            atexit(I_ShutdownSound);  
            snd_init = true;
         }
         break;

      case 1:
         i_sounddriver = &i_pcsound_driver;
         if(i_sounddriver->InitSound())
         {
            atexit(I_ShutdownSound);
            snd_init = true;
         }
         break;
#endif
      default:
         printf("Sound is disabled.\n");
         i_sounddriver = NULL;
         snd_init = false;
         break;
      }
   }

   // initialize music also
   if(!nomusicparm)
      I_InitMusic();
}

//
// MUSIC API.
//

//
// I_ShutdownMusic
//
// atexit handler.
//
void I_ShutdownMusic(void)
{
   if(mus_init)
   {
      i_musicdriver->ShutdownMusic();
      mus_init = false;
   }
}

// haleyjd 11/07/08: music drivers

#ifdef _SDL_VER
extern i_musicdriver_t i_sdlmusicdriver;
#endif

//
// I_InitMusic
//
void I_InitMusic(void)
{
   switch(mus_card)
   {
#ifdef _SDL_VER
   case -1:
      printf("I_InitMusic: Using SDL_mixer.\n");
      i_musicdriver = &i_sdlmusicdriver;
      if(i_musicdriver->InitMusic())
      {
         atexit(I_ShutdownMusic);
         mus_init = true;
      }
      break;
#endif
   default:
      printf("I_InitMusic: Music is disabled.\n");
      i_musicdriver = NULL;
      mus_init = false;
      break;
   }
}

//
// I_PlaySong
//
// Plays the currently registered song.
//
void I_PlaySong(int handle, int looping)
{
   if(mus_init)
   {
      i_musicdriver->PlaySong(handle, looping);

      // haleyjd 10/28/05: make sure volume settings remain consistent
      I_SetMusicVolume(snd_MusicVolume);
   }
}

//
// I_SetMusicVolume
//
void I_SetMusicVolume(int volume)
{
   if(mus_init)
      i_musicdriver->SetMusicVolume(volume);
}

//
// I_PauseSong
//
void I_PauseSong(int handle)
{
   if(mus_init)
      i_musicdriver->PauseSong(handle);
}

//
// I_ResumeSong
//
void I_ResumeSong(int handle)
{
   if(mus_init)
      i_musicdriver->ResumeSong(handle);
}

//
// I_StopSong
//
void I_StopSong(int handle)
{
   if(mus_init)
      i_musicdriver->StopSong(handle);
}

//
// I_UnRegisterSong
//
void I_UnRegisterSong(int handle)
{
   if(mus_init)
      i_musicdriver->UnRegisterSong(handle);
}

//
// I_RegisterSong
//
int I_RegisterSong(void *data, int size)
{
   return mus_init ? i_musicdriver->RegisterSong(data, size) : 0;
}

//
// I_QrySongPlaying
//
// Is the song playing?
//
int I_QrySongPlaying(int handle)
{
   return mus_init ? i_musicdriver->QrySongPlaying(handle) : 0;
}

/************************
        CONSOLE COMMANDS
 ************************/

// system specific sound console commands

static const char *sndcardstr[] = { "SDL mixer", "none", "PC Speaker" };
static const char *muscardstr[] = { "SDL mixer", "none" };

VARIABLE_INT(snd_card,       NULL,      -1,  1, sndcardstr);
VARIABLE_INT(mus_card,       NULL,      -1,  0, muscardstr);
VARIABLE_INT(detect_voices,  NULL,       0,  1, yesno);

#ifdef HAVE_SPCLIB
extern int spc_preamp;
extern int spc_bass_boost;

VARIABLE_INT(spc_preamp,     NULL,       1,  6, NULL);
VARIABLE_INT(spc_bass_boost, NULL,       0, 31, NULL);
#endif

#ifdef HAVE_ADLMIDILIB
static const char *mididevicestr[] = { "Default", "ADLMIDI" };
static const char **adlbankstr = const_cast<const char **>(adl_getBankNames());

extern int midi_device;
//extern int adlmidi_numcards;
extern int adlmidi_bank;

VARIABLE_INT(midi_device, NULL, -1, 0, mididevicestr);
//VARIABLE_INT(adlmidi_numcards, NULL, 1, 100, NULL);
VARIABLE_INT(adlmidi_bank, NULL, 0, BANKS_MAX, adlbankstr);
#endif

// Equalizer variables

VARIABLE_FLOAT(s_lowfreq,  NULL, 0.0, UL);
VARIABLE_FLOAT(s_highfreq, NULL, 0.0, UL);
VARIABLE_FLOAT(s_eqpreamp, NULL, 0.0, 1.0);
VARIABLE_FLOAT(s_lowgain,  NULL, 0.0, 3.0);
VARIABLE_FLOAT(s_midgain,  NULL, 0.0, 3.0);
VARIABLE_FLOAT(s_highgain, NULL, 0.0, 3.0);

CONSOLE_VARIABLE(snd_card, snd_card, 0) 
{
   if(snd_card != 0 && menuactive)
      MN_ErrorMsg("takes effect after restart");
}

CONSOLE_VARIABLE(mus_card, mus_card, 0)
{
   if(mus_card != 0 && menuactive)
      MN_ErrorMsg("takes effect after restart");
}

CONSOLE_VARIABLE(detect_voices, detect_voices, 0) {}

#ifdef _SDL_VER
#ifdef HAVE_SPCLIB
CONSOLE_VARIABLE(snd_spcpreamp, spc_preamp, 0) 
{
   I_SetMusicVolume(snd_MusicVolume);
}

extern void I_SDLMusicSetSPCBass(void);

CONSOLE_VARIABLE(snd_spcbassboost, spc_bass_boost, 0)
{
   I_SDLMusicSetSPCBass();
}
#endif

#ifdef HAVE_ADLMIDILIB
CONSOLE_VARIABLE(snd_mididevice, midi_device, 0) {}
//CONSOLE_VARIABLE(snd_numcards, adlmidi_numcards, 0) {}
CONSOLE_VARIABLE(snd_bank, adlmidi_bank, 0) {}
#endif
#endif

//
// I_UpdateEQ
//
// haleyjd 04/21/10: mini routine to update the sound driver's equalizer, if it
// has one, when a console command is issued to change one of its parameters.
//
static void I_UpdateEQ(void)
{
   if(snd_init && i_sounddriver->UpdateEQParams)
      i_sounddriver->UpdateEQParams();
}

CONSOLE_VARIABLE(s_lowfreq,   s_lowfreq,   0) { I_UpdateEQ(); }
CONSOLE_VARIABLE(s_highfreq,  s_highfreq,  0) { I_UpdateEQ(); }
CONSOLE_VARIABLE(s_eqpreamp,  s_eqpreamp,  0) { I_UpdateEQ(); }
CONSOLE_VARIABLE(s_lowgain,   s_lowgain,   0) { I_UpdateEQ(); }
CONSOLE_VARIABLE(s_midgain,   s_midgain,   0) { I_UpdateEQ(); }
CONSOLE_VARIABLE(s_highgain,  s_highgain,  0) { I_UpdateEQ(); }

//----------------------------------------------------------------------------
//
// $Log: i_sound.c,v $
//
//----------------------------------------------------------------------------
