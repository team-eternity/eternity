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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:  Sound Sequences
//
// Sound Sequences, which are powered by EDF-defined data, implement a way to
// script the sound behavior of sectors and global ambience effects. Measures
// have been taken to keep it all fully backward-compatible, as well as to
// make it more flexible than Hexen's implementation.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "d_gi.h"
#include "doomstat.h"
#include "i_system.h"
#include "c_runcmd.h"
#include "p_info.h"
#include "p_mobjcol.h"
#include "s_sndseq.h"
#include "e_things.h"
#include "r_state.h"

// Global data

DLListItem<SndSeq_t> *SoundSequences; // head of running sndseq list

SndSeq_t *EnviroSequence; // currently playing environmental sequence

int s_enviro_volume; // controls volume of sequences using randomplayvol, 0-16

// Macros

#define SECTOR_ORIGIN(s, b) \
   ((b) ? &((s)->csoundorg) : &((s)->soundorg))

//
// S_CheckSequenceLoop
//
// Returns true if the thing is playing a sequence and the sequence is looping,
// and false in any other circumstance.
//
bool S_CheckSequenceLoop(PointThinker *mo)
{
   DLListItem<SndSeq_t> *link = SoundSequences, *next;

   while(link)
   {
      next = link->dllNext;

      if((*link)->origin == mo)
         return (((*link)->flags & SEQ_FLAG_LOOPING) == SEQ_FLAG_LOOPING);

      link = next;
   }

   return false; // if no sequence is playing, it's sure not looping o_O
}

//
// S_CheckSectorSequenceLoop
//
// Convenience routine.
//
bool S_CheckSectorSequenceLoop(sector_t *s, int originType)
{
   return S_CheckSequenceLoop(SECTOR_ORIGIN(s, originType));
}

//
// S_StopSequence
//
// Stops any sound sequence being played by the given object.
//
void S_StopSequence(const PointThinker *mo)
{
   DLListItem<SndSeq_t> *link = SoundSequences, *next;
   SndSeq_t *curSeq;

   while(link)
   {
      next   =  link->dllNext;
      curSeq = *link;

      if(curSeq->origin == mo)
      {
         // if allowed, stop any other sound playing
         if(curSeq->sequence->nostopcutoff == false)
            S_StopSound(curSeq->origin, CHAN_ALL);

         // if a stopsound is defined, play it
         if(curSeq->sequence->stopsound)
         {
            soundparams_t params;
            params.origin      = curSeq->origin;
            params.sfx         = curSeq->sequence->stopsound;
            params.volumeScale = curSeq->volume;
            params.attenuation = curSeq->attenuation;
            params.loop        = false;
            params.subchannel  = CHAN_AUTO;
            params.reverb      = curSeq->sequence->reverb;
            S_StartSfxInfo(params);
         }

         // unlink and delete this object
         curSeq->link.remove();
         Z_Free(curSeq);
      }

      link = next;
   }
}

//
// S_SquashSequence
//
// Stops any sound sequence being played without playing the stop sound or
// cutting off the currently playing sound. This is needed by doors when
// they bounce, or weird stuff happens.
//
void S_SquashSequence(const PointThinker *mo)
{
   DLListItem<SndSeq_t> *link = SoundSequences, *next;

   while(link)
   {
      next = link->dllNext;

      if((*link)->origin == mo)
      {
         // unlink and delete this object
         link->remove();
         Z_Free(link->dllObject);
      }

      link = next;
   }
}

//
// S_KillSequence
//
// Totally kills any sound sequence being played by the object.
// Not only is the stop sound not played, but any sound being currently played
// is cut off regardless of the nostopcutoff value.
//
void S_KillSequence(PointThinker *mo)
{
   DLListItem<SndSeq_t> *link = SoundSequences, *next;

   while(link)
   {
      next = link->dllNext;

      if((*link)->origin == mo)
      {
         // stop any sound playing
         S_StopSound((*link)->origin, CHAN_ALL);

         // unlink and delete this object
         link->remove();
         Z_Free(link->dllObject);
      }

      link = next;
   }
}

//
// S_StopSectorSequence
//
// Convenience routine.
//
void S_StopSectorSequence(sector_t *s, int originType)
{
   S_StopSequence(SECTOR_ORIGIN(s, originType));
}

//
// S_SquashSectorSequence
//
// Convenience routine.
//
void S_SquashSectorSequence(const sector_t *s, int originType)
{
   S_SquashSequence(SECTOR_ORIGIN(s, originType));
}

//
// S_StopPolySequence
//
// Convenience routine.
//
void S_StopPolySequence(polyobj_t *po)
{
   S_StopSequence(&po->spawnSpot);
}

//
// S_StartSequenceNum
//
// Starts a sound sequence by index number. The actual sequence started may be
// altered by sound sequence redirects, depending on the sequence activation
// type.
//
void S_StartSequenceNum(PointThinker *mo, int seqnum, int seqtype, int seqOriginType,
                        int seqOriginIdx)
{
   ESoundSeq_t *edfSeq;
   SndSeq_t *newSeq;

   // find sequence by number, if none, return
   if(!(edfSeq = E_SequenceForNumType(seqnum, seqtype)))
      return;

   // check for redirection for certain activation types
   // this allows a single sector to have different sequences for different
   // action types; more flexible than what "other ports" have implemented
   switch(seqtype)
   {
   case SEQ_DOOR:
      if(edfSeq->doorseq)
         edfSeq = edfSeq->doorseq;
      break;
   case SEQ_PLAT:
      if(edfSeq->platseq)
         edfSeq = edfSeq->platseq;
      break;
   case SEQ_FLOOR:
      if(edfSeq->floorseq)
         edfSeq = edfSeq->floorseq;
      break;
   case SEQ_CEILING:
      if(edfSeq->ceilseq)
         edfSeq = edfSeq->ceilseq;
      break;
   default:
      break;
   }

   // stop any sequence the object is already playing
   S_StopSequence(mo);

   // allocate a new SndSeq object and link it
   newSeq = estructalloctag(SndSeq_t, 1, PU_LEVEL);

   newSeq->link.insert(newSeq, &SoundSequences);

   // set up all fields
   newSeq->origin       = mo;                  // set origin
   newSeq->sequence     = edfSeq;              // set sequence pointer
   newSeq->cmdPtr       = edfSeq->commands;    // set command pointer
   newSeq->attenuation  = edfSeq->attenuation; // use starting attenuation
   newSeq->delayCounter = 0;                   // no delay at start
   newSeq->flags        = 0;                   // no special flags
   newSeq->originType   = seqOriginType;       // set origin type
   newSeq->originIdx    = seqOriginIdx;        // set origin index

   // 06/16/06: possibly randomize starting volume
   newSeq->volume = 
      edfSeq->randvol ? M_RangeRandom(edfSeq->minvolume, edfSeq->volume)
                      : edfSeq->volume;
}

//
// S_StartSectorSequence
//
// Convenience routine. Starts the sequence indicated in the sector by number.
//
void S_StartSectorSequence(sector_t *s, int seqtype)
{
   int origintype;

   if(seqtype == SEQ_CEILING || seqtype == SEQ_DOOR)
      origintype = SEQ_ORIGIN_SECTOR_C;
   else
      origintype = SEQ_ORIGIN_SECTOR_F;
   
   S_StartSequenceNum(SECTOR_ORIGIN(s, origintype), s->sndSeqID, seqtype, 
                      origintype, int(s - sectors));
}

//
// S_ReplaceSectorSequence
//
// Squashes any currently playing sequence and starts a new one.
// Used by bouncing doors.
//
void S_ReplaceSectorSequence(sector_t *s, int seqtype)
{
   int originType;
   
   if(seqtype == SEQ_CEILING || seqtype == SEQ_DOOR)
      originType = SEQ_ORIGIN_SECTOR_C;
   else
      originType = SEQ_ORIGIN_SECTOR_F;

   S_SquashSectorSequence(s, originType);
   
   S_StartSequenceNum(SECTOR_ORIGIN(s, originType), s->sndSeqID, seqtype,
                      originType, int(s - sectors));
}

//
// S_StartPolySequence
//
// Convenience routine. Starts a polyobject sound sequence
//
void S_StartPolySequence(polyobj_t *po)
{
   S_StartSequenceNum(&po->spawnSpot, po->seqId, SEQ_DOOR, 
                      SEQ_ORIGIN_POLYOBJ, po->id);
}

//
// S_StartSequenceName
//
// Starts the named sound sequence.
//
void S_StartSequenceName(PointThinker *mo, const char *seqname, int seqOriginType, 
                         int seqOriginIdx)
{
   ESoundSeq_t *edfSeq;
   SndSeq_t *newSeq;

   // find sequence by name, if none, return
   if(!(edfSeq = E_SequenceForName(seqname)))
      return;

   // note that we do *not* do any redirections when playing sequences by name;
   // starting a sequence by name should be unambiguous at all times, and is
   // only done via scripting or by the game engine itself

   // stop any sequence the object is already playing
   S_StopSequence(mo);

   // allocate a new SndSeq object and link it
   newSeq = estructalloctag(SndSeq_t, 1, PU_LEVEL);

   newSeq->link.insert(newSeq, &SoundSequences);

   // set up all fields
   newSeq->origin       = mo;                  // set origin
   newSeq->sequence     = edfSeq;              // set sequence pointer
   newSeq->cmdPtr       = edfSeq->commands;    // set command pointer
   newSeq->attenuation  = edfSeq->attenuation; // use starting attenuation
   newSeq->delayCounter = 0;                   // no delay at start
   newSeq->flags        = 0;                   // no special flags
   newSeq->originType   = seqOriginType;       // origin type
   newSeq->originIdx    = seqOriginIdx;        // origin index

   // possibly randomize starting volume
   newSeq->volume = 
      edfSeq->randvol ? M_RangeRandom(edfSeq->minvolume, edfSeq->volume)
                      : edfSeq->volume;
}

//
// S_StartSectorSequenceName
//
// Convenience routine for starting a sector sequence by name.
//
void S_StartSectorSequenceName(sector_t *s, const char *seqname, int originType)
{
   S_StartSequenceName(SECTOR_ORIGIN(s, originType), seqname, originType, 
                       int(s - sectors));
}

//
// S_ReplaceSectorSequenceName
//
// Convenience routine for starting a sector sequence by name without playing
// the stop sound of any currently playing sequence.
//
void S_ReplaceSectorSequenceName(sector_t *s, const char *seqname, int originType)
{
   S_SquashSectorSequence(s, originType);

   S_StartSequenceName(SECTOR_ORIGIN(s, originType), seqname, originType,
                       int(s - sectors));
}

//
// S_StartSeqSound
//
// Starts a sound in the usual manner for a sound sequence.
//
static void S_StartSeqSound(SndSeq_t *seq, bool loop)
{
   if(seq->currentSound)
   {
      // haleyjd 01/12/11: randomplayvol supports proper Heretic randomization
      if(seq->sequence->randomplayvol && !(seq->flags & SEQ_FLAG_NORANDOM))
         seq->volume = M_VHereticPRandom(pr_envirospot) / 4 + 96 * s_enviro_volume / 16;

      // clear the NORANDOM flag
      seq->flags &= ~SEQ_FLAG_NORANDOM;

      soundparams_t params;
      params.origin      = seq->origin;
      params.sfx         = seq->currentSound;
      params.volumeScale = seq->volume;
      params.attenuation = seq->attenuation;
      params.loop        = loop;
      params.subchannel  = CHAN_AUTO;
      params.reverb      = seq->sequence->reverb;
      S_StartSfxInfo(params);
   }
}

// Command argument macros: we peek ahead in the command stream.

#define CMD_ARG1(field) ((curSeq->cmdPtr + 1)-> field )
#define CMD_ARG2(field) ((curSeq->cmdPtr + 2)-> field )

// when true, the current environmental sequence has ended
static bool enviroSeqFinished;

static void S_clearEnviroSequence();

//
// S_RunSequence
//
// Runs a single sound sequence. This is another one of those miniature
// virtual machines, although this one's not so miniature really O_O
//
static void S_RunSequence(SndSeq_t *curSeq)
{
   bool isPlaying = false;
   
   // if delaying, count down delay
   if(vanilla_heretic)
   {
      // Different, buggier way of counting down in vanilla Heretic compatibility mode
      if(curSeq->delayCounter && --curSeq->delayCounter)
         return;
   }
   else if(curSeq->delayCounter)
   {
      curSeq->delayCounter--;
      return;
   }

   // see if a sound is playing
   if(curSeq->currentSound)
      isPlaying = S_CheckSoundPlaying(curSeq->origin, curSeq->currentSound);

   // set looping to false here; looping instructions will set it to true
   curSeq->flags &= ~SEQ_FLAG_LOOPING;

   switch(curSeq->cmdPtr->data)
   {
   case SEQ_CMD_PLAY: // basic "play sound" using all parameters
      if(!isPlaying)
      {
         curSeq->currentSound = CMD_ARG1(sfx);
         S_StartSeqSound(curSeq, false);
      }
      curSeq->cmdPtr += 2;
      if(vanilla_heretic && curSeq == EnviroSequence)
         S_RunSequence(curSeq);  // continue immediately in vanilla Heretic mode
      break;
   case SEQ_CMD_WAITSOUND: // waiting on a sound to finish
      if(!isPlaying)
      {
         curSeq->cmdPtr += 1;
         curSeq->currentSound = nullptr;
      }
      break;
   case SEQ_CMD_PLAYREPEAT: // play the sound continuously (doesn't advance)
      if(!isPlaying)
      {
         curSeq->currentSound = CMD_ARG1(sfx);
         S_StartSeqSound(curSeq, true);
      }
      curSeq->flags |= SEQ_FLAG_LOOPING;
      break;
   case SEQ_CMD_PLAYLOOP: // play sound in a delay loop (doesn't advance)
      curSeq->flags |= SEQ_FLAG_LOOPING;
      curSeq->currentSound = CMD_ARG1(sfx);
      curSeq->delayCounter = CMD_ARG2(data);
      S_StartSeqSound(curSeq, false);
      break;
   case SEQ_CMD_DELAY: // delay for a while
      curSeq->currentSound = nullptr;
      curSeq->delayCounter = CMD_ARG1(data);
      curSeq->cmdPtr += 2;
      break;
   case SEQ_CMD_DELAYRANDOM: // delay a random amount within given range
      curSeq->currentSound = nullptr;
      {
         int min = CMD_ARG1(data);
         int max = CMD_ARG2(data);
         curSeq->delayCounter = vanilla_heretic ? min +
               (P_Random(pr_envirotics) % (max - min + 1)) : (int)M_RangeRandomEx(min, max);

         // Emulate bug of enviroTics being set to 0 and causing the sounds to stop forever
         if(vanilla_heretic && curSeq == EnviroSequence && curSeq->delayCounter == 0)
            curSeq->delayCounter = INT_MAX;
      }
      curSeq->cmdPtr += 3;
      break;
   case SEQ_CMD_SETVOLUME: // set volume
      curSeq->volume = CMD_ARG1(data);
      curSeq->cmdPtr += 2;
      if(curSeq->volume < 0)
         curSeq->volume = 0;
      else if(curSeq->volume > 127)
         curSeq->volume = 127;
      curSeq->flags |= SEQ_FLAG_NORANDOM; // don't randomize the next volume
      if(vanilla_heretic && curSeq == EnviroSequence)
         S_RunSequence(curSeq);  // continue immediately in vanilla Heretic mode
      break;
   case SEQ_CMD_SETVOLUMEREL: // set relative volume
      curSeq->volume += CMD_ARG1(data);
      curSeq->cmdPtr += 2;
      if(curSeq->volume < 0)
         curSeq->volume = 0;
      else if(curSeq->volume > 127)
         curSeq->volume = 127;
      curSeq->flags |= SEQ_FLAG_NORANDOM; // don't randomize the next volume
      if(vanilla_heretic && curSeq == EnviroSequence)
         S_RunSequence(curSeq);  // continue immediately in vanilla Heretic mode
      break;
   case SEQ_CMD_SETATTENUATION: // set attenuation
      curSeq->attenuation = CMD_ARG1(data);
      curSeq->cmdPtr += 2;
      break;
   case SEQ_CMD_RESTART: // restart the sequence
      curSeq->cmdPtr = curSeq->sequence->commands;
      break;
   case SEQ_CMD_END:
      // sequences without a stopsound are ended here
      if(curSeq == EnviroSequence)
      {
         enviroSeqFinished = true;
         if(vanilla_heretic)
            S_clearEnviroSequence();   // clear it now if demo
      }
      else if(curSeq->sequence->stopsound == nullptr)
      {
         // if allowed, stop any other sound playing
         if(curSeq->sequence->nostopcutoff == false)
            S_StopSound(curSeq->origin, CHAN_ALL);
         
         // unlink and delete this object
         curSeq->link.remove();
         Z_Free(curSeq);
      }
      break;
   default: // unknown command? (shouldn't happen)
      I_Error("S_RunSequence: internal error - unknown sequence command\n");
      break;
   }
}

// prototypes for enviro functions from below
static void S_RunEnviroSequence();
static void S_StopEnviroSequence();

//
// S_RunSequences
//
// Updates all running sound sequences.
//
void S_RunSequences()
{
   DLListItem<SndSeq_t> *link = SoundSequences;

   while(link)
   {
      DLListItem<SndSeq_t> *next = link->dllNext;

      S_RunSequence(*link);

      link = next;
   } // end while

   // run the environmental sequence, if any exists
   S_RunEnviroSequence();
}

//
// S_StopAllSequences
//
// Stops all running sound sequences. Called at the end of a level.
//
void S_StopAllSequences()
{
   // Because everything is allocated PU_LEVEL, simply disconnecting the list
   // head is all that is needed to stop all sequences from playing. The sndseq
   // nodes will all be destroyed by P_SetupLevel.
   SoundSequences = nullptr;

   // also stop any running environmental sequence
   S_StopEnviroSequence();
}

//=============================================================================
//
// Environmental Ambience Sequences
//
// haleyjd 06/06/06: At long last, I can implement Heretic's global ambience
// effects, but folded within the Hexen-like sound sequence engine.
//

// The environment sequence manager data. This is overridable via a special
// block in EDF.
EnviroSeqMgr_t EnviroSeqManager =
{
   10*TICRATE,        // minimum start wait
   10*TICRATE,        // maximum start wait
    6*TICRATE,        // minimum wait between sequences
    6*TICRATE + 255,  // maximum wait between sequences
};

static MobjCollection enviroSpots;
static int enviroTics;
static Mobj *nextEnviroSpot;

static SndSeq_t enviroSeq;

//
// S_ResetEnviroSeqEngine
//
// Resets the environmental sequence engine.
//
static void S_ResetEnviroSeqEngine()
{
   EnviroSequence    = nullptr;
   enviroSeqFinished = true;

   // At startup we don't know the first spot. It will be known at the end of this turn.
   nextEnviroSpot = nullptr; 

   enviroTics = (int)M_RangeRandomEx(EnviroSeqManager.minStartWait,
                                     EnviroSeqManager.maxStartWait);
}

//
// S_InitEnviroSpots
//
// Puts all the environmental sequence spots on the map into an MobjCollection
// and gets the environmental sequence engine ready to run.
//
void S_InitEnviroSpots()
{
   enviroSpots.mobjType = "EEEnviroSequence";
   enviroSpots.makeEmpty();
   enviroSpots.collectThings();

   S_ResetEnviroSeqEngine();
}

//
// Clears the current Heretic-like environment sequence, if ready. Needed to
// support all entry-points, depending on demo
//
static void S_clearEnviroSequence()
{
   memset(&enviroSeq, 0, sizeof(SndSeq_t));
   EnviroSequence = NULL;

   int min = EnviroSeqManager.minEnviroWait;
   int max = EnviroSeqManager.maxEnviroWait;

   if(vanilla_heretic)
      enviroTics = min + P_Random(pr_enviroticsend) % (max - min + 1);
   else
      enviroTics = (int)M_RangeRandomEx(min, max);

   nextEnviroSpot = enviroSpots.getRandom(vanilla_heretic ? pr_envirospot : pr_misc);
}

//
// S_RunEnviroSequence
//
// Runs the current environmental sound sequence, or schedules another to run
// if it has finished. Note that environmental sequences can't use looping
// commands or else they'll lock out any other sequence for the rest of the
// map.
//
static void S_RunEnviroSequence()
{
   // nothing to do?
   if(enviroSpots.isEmpty())
      return;

   // if waiting, count down the wait time
   if(vanilla_heretic)
   {
      if(enviroTics > 0 && --enviroTics)
         return;
   }
   else if(enviroTics)
   {
      enviroTics--;
      return;
   }

   // are we currently playing a sequence?
   if(EnviroSequence)
   {
      // is it finished?
      if(enviroSeqFinished)
         S_clearEnviroSequence();
      else
         S_RunSequence(EnviroSequence);
   }
   else if(nextEnviroSpot)
   {
      // start a new sequence
      ESoundSeq_t *edfSeq = E_EnvironmentSequence(nextEnviroSpot->args[0]);

      if(!edfSeq) // woops, bad sequence, try another next time.
      {
         nextEnviroSpot = enviroSpots.getRandom(vanilla_heretic ? pr_envirospot : pr_misc);
         return;
      }

      enviroSeq.sequence     = edfSeq;
      enviroSeq.cmdPtr       = edfSeq->commands;
      enviroSeq.currentSound = nullptr;
      enviroSeq.origin       = nextEnviroSpot;
      enviroSeq.attenuation  = edfSeq->attenuation;
      enviroSeq.delayCounter = 0;
      enviroSeq.flags        = SEQ_FLAG_ENVIRO; // started by enviro engine
      enviroSeq.originType   = SEQ_ORIGIN_OTHER;
      enviroSeq.originIdx    = -1;

      // possibly randomize the starting volume
      enviroSeq.volume = edfSeq->randvol ? 
         vanilla_heretic ? P_Random(pr_envirospot) / 4 : 
         M_RangeRandom(edfSeq->minvolume, edfSeq->volume) : edfSeq->volume;

      EnviroSequence    = &enviroSeq; // now playing an enviro sequence
      enviroSeqFinished = false;      // sequence is not finished
      if(vanilla_heretic)
         S_RunSequence(EnviroSequence);   // on vanilla Heretic mode, already play it
   }
   else  // We don't have a sequence playing and there's no spot. Wait for next spot then.
      S_clearEnviroSequence();
}

//
// S_StopEnviroSequence
//
// Unconditionally stops the environmental sequence engine. Called from
// S_StopAllSequences above.
//
static void S_StopEnviroSequence()
{
   // stomp on everything to stop it from running any more sequences
   EnviroSequence = nullptr;  // no playing sequence
   nextEnviroSpot = nullptr;  // no spot chosen
   enviroSeqFinished = true;  // finished playing
   enviroTics = D_MAXINT;     // wait more or less forever
}

//=============================================================================
//
// Savegame Loading Stuff
//

//
// S_SetSequenceStatus
//
// Restores a sound sequence's status using data extracted from a game save.
//
void S_SetSequenceStatus(SndSeq_t *seq)
{
   // if it is an environment sequence, copy this sequence into the enviro
   // sequence and then destroy the one that was created by the savegame code
   if(seq->flags & SEQ_FLAG_ENVIRO)
   {
      memcpy(&enviroSeq, seq, sizeof(SndSeq_t));
      EnviroSequence = &enviroSeq;
      enviroSeqFinished = false;
      enviroTics = 0;

      Z_Free(seq);
   }
   else
   {
      // link this sequence
      seq->link.insert(seq, &SoundSequences);
   }
}

//
// S_SequenceGameLoad
//
// This is called from the savegame loading code to reset the sound sequence
// engine.
//
void S_SequenceGameLoad()
{
   DLListItem<SndSeq_t> *link;

   // kill all running sequences
   // note the loop restarts from the beginning each time because S_KillSequence
   // modifies the double-linked list; it'll stop running when the last sequence
   // is deleted.
   while((link = SoundSequences))
      S_KillSequence((*link)->origin);

   // reset the enviro sequence engine in a way that lets it start up again
   S_ResetEnviroSeqEngine();
}

//=============================================================================
//
// Console commands
//

VARIABLE_INT(s_enviro_volume, nullptr, 0, 16, nullptr);
CONSOLE_VARIABLE(s_enviro_volume, s_enviro_volume, 0) {}

// EOF

