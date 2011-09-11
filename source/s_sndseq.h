// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
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

typedef union seqcmd_s
{
   sfxinfo_t *sfx; // pointer to a sound -OR-
   int data;       // some kind of data
} seqcmd_t;

// 10/18/06: sequence flags
enum
{
   SEQ_FLAG_LOOPING = 0x01,
   SEQ_FLAG_ENVIRO  = 0x02,
};

struct ESoundSeq_t;

//
// SndSeq_t -- a running sound sequence
//
struct SndSeq_t
{
   CDLListItem<SndSeq_t> link;   // double-linked list node -- must be first

   ESoundSeq_t *sequence;        // pointer to EDF sound sequence
   seqcmd_t *cmdPtr;             // current position in command sequence

   mobj_t *origin;               // the origin of the sequence
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
extern CDLListItem<SndSeq_t> *SoundSequences;
extern SndSeq_t *EnviroSequence;

void S_StartSequenceNum(mobj_t *mo, int seqnum, int seqtype, int seqOriginType,
                        int seqOriginIdx);
void S_StartSequenceName(mobj_t *mo, const char *seqname, int seqOriginType,
                         int seqOriginIdx);
void S_StopSequence(mobj_t *mo);
void S_SquashSequence(mobj_t *mo);
void S_KillSequence(mobj_t *mo);

void S_StartSectorSequence(sector_t *s, int seqtype);
void S_StartSectorSequenceName(sector_t *s, const char *seqname, boolean fOrC);
void S_ReplaceSectorSequence(sector_t *s, int seqtype);
void S_ReplaceSectorSequenceName(sector_t *s, const char *seqname, boolean fOrC);
void S_StopSectorSequence(sector_t *s, boolean floorOrCeiling);
void S_SquashSectorSequence(sector_t *s, boolean floorOrCeiling);

void S_StartPolySequence(polyobj_t *po);
void S_StopPolySequence(polyobj_t *po);

void S_RunSequences(void);
void S_StopAllSequences(void);
void S_SetSequenceStatus(SndSeq_t *seq);
void S_SequenceGameLoad(void);
void S_InitEnviroSpots(void);

boolean S_CheckSequenceLoop(mobj_t *mo);
boolean S_CheckSectorSequenceLoop(sector_t *s, boolean floorOrCeiling);

// EnviroSeqMgr_t -- environment sequence manager data

typedef struct EnviroSeqMgr_s
{
   int minStartWait;  // minimum wait period at start
   int maxStartWait;  // maximum wait period at start
   int minEnviroWait; // minimum wait period between sequences
   int maxEnviroWait; // maximum wait period between sequences
} EnviroSeqMgr_t;

extern EnviroSeqMgr_t EnviroSeqManager;

#endif

// EOF

