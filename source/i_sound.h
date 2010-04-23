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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      System interface, sound.
//
//-----------------------------------------------------------------------------

#ifndef I_SOUND_H__
#define I_SOUND_H__

typedef struct i_sounddriver_s
{
   int  (*InitSound)(void);
   void (*CacheSound)(sfxinfo_t *);
   void (*UpdateSound)(void);
   void (*SubmitSound)(void);
   void (*ShutdownSound)(void);
   int  (*StartSound)(sfxinfo_t *, int, int, int, int, int, int);
   int  (*SoundID)(int);
   void (*StopSound)(int);
   int  (*SoundIsPlaying)(int);
   void (*UpdateSoundParams)(int, int, int, int);
   void (*UpdateEQParams)(void);
} i_sounddriver_t;

// Init at program start...
void I_InitSound(void);

// ... update sound buffer and audio device at runtime...
void I_UpdateSound(void);
void I_SubmitSound(void);

// ... shut down and relase at program termination.
void I_ShutdownSound(void);

// Cache sound data
void I_CacheSound(sfxinfo_t *sound);

//
//  SFX I/O
//

// Starts a sound in a particular sound channel.
int I_StartSound(sfxinfo_t *sound, int cnum, int vol, int sep, int pitch,
                 int pri, int loop);

// Returns unique instance ID for a playing sound.
int I_SoundID(int handle);

// Stops a sound channel.
void I_StopSound(int handle);

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

typedef struct i_musicdriver_s
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
} i_musicdriver_t;

void I_InitMusic(void);
void I_ShutdownMusic(void);

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
extern int     s_equalizer; // if true, use equalizer
extern double  s_lowfreq;   // low band cutoff frequency
extern double  s_highfreq;  // high band cutoff frequency
extern double  s_eqpreamp;  // preamp factor
extern double  s_lowgain;   // low band gain
extern double  s_midgain;   // mid band gain
extern double  s_highgain;  // high band gain

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
