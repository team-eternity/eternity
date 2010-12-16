// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2003 James Haley
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
// EDF Sound Module
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_SOUND_H
#define E_SOUND_H

#include "doomtype.h"
#include "sounds.h"
#include "m_dllist.h"
#include "s_sndseq.h"

// haleyjd 04/13/08: this replaces S_sfx[0].
extern sfxinfo_t NullSound;

sfxinfo_t *E_SoundForName(const char *);
sfxinfo_t *E_EDFSoundForName(const char *name);
sfxinfo_t *E_SoundForDEHNum(int);
sfxinfo_t *E_FindSoundForDEH(char *inbuffer, unsigned int fromlen);

void E_NewWadSound(const char *);
void E_PreCacheSounds(void);
void E_UpdateSoundCache(void);

// haleyjd: EDF ambience types
enum
{
   E_AMBIENCE_CONTINUOUS,
   E_AMBIENCE_PERIODIC,
   E_AMBIENCE_RANDOM
};

// haleyjd 05/30/06: EDF ambience objects
typedef struct EAmbience_s
{
   int index;        // numeric id

   sfxinfo_t *sound; // sound to use
   int type;         // continuous, periodic, random
   int volume;       // scale value from 0 to 127
   int attenuation;  // normal/idle, static, or none
   int period;       // used for periodic only
   int minperiod;    // minimum period length for random
   int maxperiod;    // maximum period length for random

   struct EAmbience_s *next; // for hashing
} EAmbience_t;

EAmbience_t *E_AmbienceForNum(int num);

// sequence types
enum
{
   SEQ_SECTOR,      // for use by sector movement
   SEQ_DOORTYPE,    // specifically meant for doors
   SEQ_PLATTYPE,    // specifically meant for plats
   SEQ_ENVIRONMENT, // for use by scripted ambience
};

// haleyjd 06/04/06: EDF sound sequences
struct ESoundSeq_t
{
   CDLListItem<ESoundSeq_t> numlinks; // next, prev links for numeric hash chain

   int index;                    // numeric id
   int type;                     // type of sequence (see above enum)
   char name[33];                // mnemonic

   union seqcmd_s *commands;     // the compiled commands

   boolean randvol;              // use random starting volume?
   int volume;                   // starting volume (or max vol if randomized)
   int minvolume;                // minimum volume if randomized
   int attenuation;              // starting attenuation
   sfxinfo_t *stopsound;         // stopsound, if any
   boolean nostopcutoff;         // if true, sounds aren't cut off at end

   ESoundSeq_t *doorseq;  // redirect for door sequence use
   ESoundSeq_t *platseq;  // redirect for platform sequence use
   ESoundSeq_t *floorseq; // redirect for floor sequence use
   ESoundSeq_t *ceilseq;  // redirect for ceiling sequence use

   ESoundSeq_t *namenext; // for hashing by name
};

ESoundSeq_t *E_SequenceForName(const char *name);
ESoundSeq_t *E_SequenceForNum(int id);
ESoundSeq_t *E_SequenceForNumType(int id, int type);
ESoundSeq_t *E_EnvironmentSequence(int id);

#ifdef NEED_EDF_DEFINITIONS
extern cfg_opt_t edf_sound_opts[];
extern cfg_opt_t edf_sdelta_opts[];
extern cfg_opt_t edf_ambience_opts[];
extern cfg_opt_t edf_sndseq_opts[];
extern cfg_opt_t edf_seqmgr_opts[];

void    E_ProcessSounds(cfg_t *cfg);
void    E_ProcessSoundDeltas(cfg_t *cfg, boolean add);
void    E_ProcessSndSeqs(cfg_t *cfg);
void    E_ProcessAmbience(cfg_t *cfg);
boolean E_AutoAllocSoundDEHNum(sfxinfo_t *sfx);

#define EDF_SEC_SOUND     "sound"
#define EDF_SEC_SDELTA    "sounddelta"
#define EDF_SEC_AMBIENCE  "ambience"
#define EDF_SEC_SNDSEQ    "soundsequence"
#define EDF_SEC_ENVIROMGR "enviromanager"
#endif

#endif

// EOF

