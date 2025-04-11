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
//--------------------------------------------------------------------------
//
// DESCRIPTION:  Sound Sequences
//
// Sound Sequences, which are powered by EDF-defined data, implement a way to
// script the sound behavior of sectors and global ambience effects. Measures
// have been taken to keep it all fully backward-compatible.
//
//-----------------------------------------------------------------------------

#ifndef S_SNDSEQ_H__
#define S_SNDSEQ_H__

#include "m_dllist.h"
#include "e_sound.h"
#include "polyobj.h"
#include "s_sound.h"

// sound sequence commands
enum
{
   SEQ_CMD_PLAY,            // plays named sound at next index
   SEQ_CMD_WAITSOUND,       // waits until current sound ends
   SEQ_CMD_PLAYREPEAT,      // loops sound seamlessly til sequence stops
   SEQ_CMD_PLAYLOOP,        // plays sound every n tics til sequence stops
   SEQ_CMD_DELAY,           // delays for n tics
   SEQ_CMD_DELAYRANDOM,     // delays between m and n tics
   SEQ_CMD_SETVOLUME,       // sets volume to n
   SEQ_CMD_SETVOLUMEREL,    // adds/subtracts n from volume (for Heretic)
   SEQ_CMD_SETATTENUATION,  // sets attenuation type to n
   SEQ_CMD_RESTART,         // restart the sequence
   SEQ_CMD_END,             // end of sequence: do nothing
};

// sound sequence playing types (for redirection)
enum
{
   SEQ_GENERIC, // don't redirect
   SEQ_DOOR,
   SEQ_PLAT,
   SEQ_FLOOR,
   SEQ_CEILING,
};

// sound sector origin types (needed for savegames)
enum
{
   SEQ_ORIGIN_SECTOR_F,
   SEQ_ORIGIN_SECTOR_C,
   SEQ_ORIGIN_POLYOBJ,
   SEQ_ORIGIN_OTHER,
};

// seqcmd_t -- a single sound sequence command

union seqcmd_t
{
   sfxinfo_t *sfx; // pointer to a sound -OR-
   int data;       // some kind of data
};

// 10/18/06: sequence flags
enum
{
   SEQ_FLAG_LOOPING  = 0x01, // currently looping a sound
   SEQ_FLAG_ENVIRO   = 0x02, // is an environmental sequence
   SEQ_FLAG_NORANDOM = 0x04  // 01/12/11: don't randomize next sound
};

struct ESoundSeq_t;

//
// SndSeq_t -- a running sound sequence
//
struct SndSeq_t
{
   DLListItem<SndSeq_t> link;   // double-linked list node -- must be first

   ESoundSeq_t *sequence;        // pointer to EDF sound sequence
   seqcmd_t *cmdPtr;             // current position in command sequence

   PointThinker *origin;         // the origin of the sequence
   sfxinfo_t *currentSound;      // current sound being played
   
   int delayCounter;             // delay time counter
   int volume;                   // current volume
   int attenuation;              // current attenuation type

   int flags;                    // sequence flags

   // 10/17/06: data needed for savegames
   int originType;               // type of origin (sector, polyobj, other)
   int originIdx;                // sector or polyobj number, (or -1)
};

// Sound sequence pointers, needed for savegame support
extern DLListItem<SndSeq_t> *SoundSequences;
extern SndSeq_t *EnviroSequence;

void S_StartSequenceNum(PointThinker *mo, int seqnum, int seqtype, 
                        int seqOriginType, int seqOriginIdx);
void S_StartSequenceName(PointThinker *mo, const char *seqname, 
                         int seqOriginType, int seqOriginIdx);
void S_StopSequence(const PointThinker *mo);
void S_SquashSequence(PointThinker *mo);
void S_KillSequence(PointThinker *mo);

void S_StartSectorSequence(sector_t *s, int seqtype);
void S_StartSectorSequenceName(sector_t *s, const char *seqname, int originType);
void S_ReplaceSectorSequence(sector_t *s, int seqtype);
void S_ReplaceSectorSequenceName(sector_t *s, const char *seqname, int originType);
void S_StopSectorSequence(sector_t *s, int originType);
void S_SquashSectorSequence(const sector_t *s, int originType);

void S_StartPolySequence(polyobj_t *po);
void S_StopPolySequence(polyobj_t *po);

void S_RunSequences(void);
void S_StopAllSequences(void);
void S_SetSequenceStatus(SndSeq_t *seq);
void S_SequenceGameLoad(void);
void S_InitEnviroSpots(void);

bool S_CheckSequenceLoop(PointThinker *mo);
bool S_CheckSectorSequenceLoop(sector_t *s, int originType);

// EnviroSeqMgr_t -- environment sequence manager data

struct EnviroSeqMgr_t
{
   int minStartWait;  // minimum wait period at start
   int maxStartWait;  // maximum wait period at start
   int minEnviroWait; // minimum wait period between sequences
   int maxEnviroWait; // maximum wait period between sequences
};

extern EnviroSeqMgr_t EnviroSeqManager;

// boost for environmental sequences that use randomplayvol
extern int s_enviro_volume;

#endif

// EOF

