// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      The not so system specific sound interface.
//
//-----------------------------------------------------------------------------

#ifndef S_SOUND_H__
#define S_SOUND_H__

class Mobj;
class PointThinker;

#include "sounds.h"

// haleyjd 07/13/05: moved these defines to the header

// when to clip out sounds
// Does not fit the large outdoor areas.
#define S_CLIPPING_DIST_I 1200
#define S_CLIPPING_DIST (S_CLIPPING_DIST_I << FRACBITS)

// Distance to origin when sounds should be maxed out.
// This should relate to movement clipping resolution
// (see BLOCKMAP handling).
// Originally: (200*0x10000).
//
// killough 12/98: restore original
// #define S_CLOSE_DIST (160<<FRACBITS)

#define S_CLOSE_DIST_I 200
#define S_CLOSE_DIST (S_CLOSE_DIST_I << FRACBITS)

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//
void S_Init(int sfxVolume, int musicVolume);

//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//
void S_Start();

// haleyjd 05/30/06: sound attenuation types
enum
{
   ATTN_NORMAL, // normal: use the data stored in sfxinfo_t
   ATTN_IDLE,   // idle: use DOOM's old values all the time
   ATTN_STATIC, // static: fade out quickly
   ATTN_NONE,   // none: Moooo!
   ATTN_NUM     // number of types
};

struct soundparams_t
{
   PointThinker *origin;
   sfxinfo_t    *sfx;
   int           volumeScale;
   int           attenuation;
   bool          loop;
   int           subchannel;
   bool          reverb;

   soundparams_t &setNormalDefaults(PointThinker *pOrigin)
   {
      origin      = pOrigin;
      volumeScale = 127;
      attenuation = ATTN_NORMAL;
      loop        = false;
      subchannel  = CHAN_AUTO;
      reverb      = true;

      return *this;
   }
};

//
// Start sound for thing at <origin>
//  using <sound_id> from sounds.h
//
void S_StartSfxInfo(const soundparams_t &params);
void S_StartSound(PointThinker *origin, int sound_id);
void S_StartSoundName(PointThinker *origin, const char *name);
void S_StartSoundAtVolume(PointThinker *origin, int sfx_id, 
                          int volume, int attn, int subchannel);
void S_StartSoundNameAtVolume(PointThinker *origin, const char *name, 
                              int volume, int attn,
                              int subchannel);
void S_StartInterfaceSound(int sound_id);
void S_StartInterfaceSound(const char *name);

// Stop sound for thing at <origin>
void S_StopSound(const PointThinker *origin, int subchannel);

// Start music using <music_id> from sounds.h
void S_StartMusic(int music_id);

// Start music using <music_id> from sounds.h, and set whether looping
void S_ChangeMusicNum(int music_id, int looping);
void S_ChangeMusicName(const char *name, int looping);
void S_ChangeMusic(musicinfo_t *music, int looping);

// Stops the music fer sure.
void S_StopMusic(void);
void S_StopSounds(bool killall);
void S_StopLoopedSounds(void); // haleyjd

// Stop and resume music, during game PAUSE.
void S_PauseSound(void);
void S_ResumeSound(void);

sfxinfo_t *S_SfxInfoForName(const char *name);
void S_Chgun(void);

musicinfo_t *S_MusicForName(const char *name);

//
// Updates music & sounds
//
void S_UpdateSounds(const Mobj *listener);
void S_SetMusicVolume(int volume);
void S_SetSfxVolume(int volume);

// haleyjd: rudimentary sound checker
bool S_CheckSoundPlaying(PointThinker *, sfxinfo_t *aliasinfo);

// precache sound?
extern int s_precache;

// machine-independent sound params
extern int numChannels;
extern int default_numChannels;  // killough 10/98

//jff 3/17/98 holds last IDMUS number, or -1
extern int idmusnum;

// haleyjd 11/03/06: moved/added to header
#define SOUND_HASHSLOTS 257

extern musicinfo_t *musicinfos[];

// haleyjd 12/24/11: hi-def music support
extern bool s_hidefmusic;

// haleyjd 05/18/14: music randomization
extern bool s_randmusic;

//
// GameModeInfo music routines
//
int S_MusicForMapDoom();
int S_MusicForMapDoom2();
int S_MusicForMapHtic();
int S_DoomMusicCheat(const char *buf);
int S_Doom2MusicCheat(const char *buf);
int S_HereticMusicCheat(const char *buf);

#endif

//----------------------------------------------------------------------------
//
// $Log: s_sound.h,v $
// Revision 1.4  1998/05/03  22:57:36  killough
// beautification, add external declarations
//
// Revision 1.3  1998/04/27  01:47:32  killough
// Fix pickups silencing player weapons
//
// Revision 1.2  1998/01/26  19:27:51  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
