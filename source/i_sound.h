//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Stephen McGranahan, Julian Aubourg, et al.
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
//------------------------------------------------------------------------------
//
// Purpose: System interface, sound.
// Authors: James Haley
//

#ifndef I_SOUND_H__
#define I_SOUND_H__

struct sfxinfo_t;

struct i_sounddriver_t
{
   int  (*InitSound)(void);
   void (*CacheSound)(sfxinfo_t *);
   void (*UpdateSound)(void);
   void (*SubmitSound)(void);
   void (*ShutdownSound)(void);
   int  (*StartSound)(sfxinfo_t *, int, int, int, int, int, int, bool);
   int  (*SoundID)(int);
   void (*StopSound)(int, int);
   int  (*SoundIsPlaying)(int);
   void (*UpdateSoundParams)(int, int, int, int);
   void (*UpdateEQParams)(void);
};

// Init at program start...
void I_InitSound();

// ... update sound buffer and audio device at runtime...
void I_UpdateSound();
void I_SubmitSound();

// ... shut down and relase at program termination.
void I_ShutdownSound();

// Cache sound data
void I_CacheSound(sfxinfo_t *sound);

//
//  SFX I/O
//

// Starts a sound in a particular sound channel.
int I_StartSound(sfxinfo_t *sound, int cnum, int vol, int sep, int pitch,
                 int pri, int loop, bool reverb);

// Returns unique instance ID for a playing sound.
int I_SoundID(int handle);

// Stops a sound channel.
void I_StopSound(int handle, int id);

// Called by S_*() functions
//  to see if a channel is still playing.
// Returns 0 if no longer playing, 1 if playing.
int I_SoundIsPlaying(int handle);

// Updates the volume, separation,
//  and pitch of a sound channel.
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch);

//
//  MUSIC I/O
//

struct i_musicdriver_t
{
   int  (*InitMusic)(void);
   void (*ShutdownMusic)(void);
   void (*SetMusicVolume)(int);
   void (*PauseSong)(int);
   void (*ResumeSong)(int);
   int  (*RegisterSong)(void *, int);
   void (*PlaySong)(int, int);
   void (*StopSong)(int);
   void (*UnRegisterSong)(int);
   int  (*QrySongPlaying)(int);
};

void I_InitMusic();
void I_ShutdownMusic();

#define DEFAULT_MIDI_DEVICE -1 // use saved music module device

// Volume.
void I_SetMusicVolume(int volume);

// PAUSE game handling.
void I_PauseSong(int handle);
void I_ResumeSong(int handle);

// Registers a song handle to song data.
// julian: added length parameter for SDL's RWops
int I_RegisterSong(void *data, int length);

// Called by anything that wishes to start music.
//  plays a song, and when the song is done,
//  starts playing it again in an endless loop.
// Horrible thing to do, considering.
void I_PlaySong(int handle, int looping);

// Stops a song over 3 seconds.
void I_StopSong(int handle);

// See above (register), then think backwards
void I_UnRegisterSong(int handle);

// Allegro card support jff 1/18/98
extern  int snd_card;
extern  int mus_card;
extern  int detect_voices; // jff 3/4/98 option to disable voice detection

// haleyjd 04/21/10: equalization parameters
extern double  s_lowfreq;   // low band cutoff frequency
extern double  s_highfreq;  // high band cutoff frequency
extern double  s_eqpreamp;  // preamp factor
extern double  s_lowgain;   // low band gain
extern double  s_midgain;   // mid band gain
extern double  s_highgain;  // high band gain

extern bool    s_reverbactive;

static inline bool I_IsSoundBufferSizePowerOf2(int i)
{
   return (i & (i - 1)) == 0;
}

static inline int I_MakeSoundBufferSize(int i)
{
   --i;
   i |= i >> 1;
   i |= i >> 2;
   i |= i >> 4;
   i |= i >> 8;
   i |= i >> 16;
   ++i;

   return i;
}

#endif

//----------------------------------------------------------------------------
//
// $Log: i_sound.h,v $
// Revision 1.4  1998/05/03  22:31:58  killough
// beautification, add some external declarations
//
// Revision 1.3  1998/02/23  04:27:08  killough
// Add variable pitched sound support
//
// Revision 1.2  1998/01/26  19:26:57  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
