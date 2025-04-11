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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
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

#include "m_dllist.h"
#include "sounds.h"

// haleyjd 04/13/08: this replaces S_sfx[0].
extern sfxinfo_t NullSound;

sfxinfo_t *E_SoundForName(const char *);
sfxinfo_t *E_EDFSoundForName(const char *name);
sfxinfo_t *E_SoundForDEHNum(int);
sfxinfo_t *E_GetAddSoundForAddDEHNum(int dehnum, bool forceupdate);
sfxinfo_t *E_FindSoundForDEH(char *inbuffer, unsigned int fromlen);
sfxinfo_t *E_NewWadSound(const char *);
sfxinfo_t *E_NewSndInfoSound(const char *mnemonic, const char *name);

void E_UpdateAddSoundNameForNum(const int num, const char *name, bool forceupdate);

void E_PreCacheSounds();
void E_UpdateSoundCache();

// haleyjd: EDF ambience types
enum
{
   E_AMBIENCE_CONTINUOUS,
   E_AMBIENCE_PERIODIC,
   E_AMBIENCE_RANDOM
};

// haleyjd 05/30/06: EDF ambience objects
struct EAmbience_t
{
   int index;        // numeric id

   sfxinfo_t *sound; // sound to use
   int  type;        // continuous, periodic, random
   int  volume;      // scale value from 0 to 127
   int  attenuation; // normal/idle, static, or none
   int  period;      // used for periodic only
   int  minperiod;   // minimum period length for random
   int  maxperiod;   // maximum period length for random
   bool reverb;      // whether or not ambience is affected by environments

   EAmbience_t *next; // for hashing
};

EAmbience_t *E_AmbienceForNum(int num);

// sequence types
enum
{
   SEQ_SECTOR,      // for use by sector movement
   SEQ_DOORTYPE,    // specifically meant for doors
   SEQ_PLATTYPE,    // specifically meant for plats
   SEQ_ENVIRONMENT, // for use by scripted ambience
};

union seqcmd_t;

// haleyjd 06/04/06: EDF sound sequences
struct ESoundSeq_t
{
   DLListItem<ESoundSeq_t> numlinks; // next, prev links for numeric hash chain

   int index;                    // numeric id
   int type;                     // type of sequence (see above enum)
   char name[129];               // mnemonic

   seqcmd_t *commands;           // the compiled commands

   bool randvol;                 // use random starting volume?
   int volume;                   // starting volume (or max vol if randomized)
   int minvolume;                // minimum volume if randomized
   int attenuation;              // starting attenuation
   sfxinfo_t *stopsound;         // stopsound, if any
   bool nostopcutoff;            // if true, sounds aren't cut off at end
   bool randomplayvol;           // if true, volume is randomized on most play cmds
   bool reverb;                  // whether or not to apply environments

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

void E_ProcessSounds(cfg_t *cfg);
void E_ProcessSoundDeltas(cfg_t *cfg, bool add);
void E_ProcessSndSeqs(cfg_t *cfg);
void E_ProcessAmbience(cfg_t *cfg);
bool E_AutoAllocSoundDEHNum(sfxinfo_t *sfx);

constexpr const char EDF_SEC_SOUND[]     = "sound";
constexpr const char EDF_SEC_SDELTA[]    = "sounddelta";
constexpr const char EDF_SEC_AMBIENCE[]  = "ambience";
constexpr const char EDF_SEC_SNDSEQ[]    = "soundsequence";
constexpr const char EDF_SEC_ENVIROMGR[] = "enviromanager";
#endif

#endif

// EOF

