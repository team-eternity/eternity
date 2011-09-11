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
// have been taken to keep it all fully backward-compatible, as well as to
// make it more flexible than Hexen's implementation.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "s_sndseq.h"
#include "e_things.h"
#include "r_state.h"

// Global data

CDLListItem<SndSeq_t> *SoundSequences; // head of running sndseq list

SndSeq_t *EnviroSequence; // currently playing environmental sequence

// Macros

#define SECTOR_ORIGIN(s, b) \
   (mobj_t *)((b) ? &((s)->csoundorg) : &((s)->soundorg))

//
// S_CheckSequenceLoop
//
// Returns true if the thing is playing a sequence and the sequence is looping,
// and false in any other circumstance.
//
boolean S_CheckSequenceLoop(mobj_t *mo)
{
   CDLListItem<SndSeq_t> *link = SoundSequences, *next;

   while(link)
   {
      next = link->dllNext;

      if(link->dllObject->origin == mo)
         return ((link->dllObject->flags & SEQ_FLAG_LOOPING) == SEQ_FLAG_LOOPING);

      link = next;
   }

   return false; // if no sequence is playing, it's sure not looping o_O
}

//
// S_CheckSectorSequenceLoop
//
// Convenience routine.
//
boolean S_CheckSectorSequenceLoop(sector_t *s, boolean floorOrCeiling)
{
   return S_CheckSequenceLoop(SECTOR_ORIGIN(s, floorOrCeiling));
}

//
// S_StopSequence
//
// Stops any sound sequence being played by the given object.
//
void S_StopSequence(mobj_t *mo)
{
   CDLListItem<SndSeq_t> *link = SoundSequences, *next;
   SndSeq_t *curSeq;

   while(link)
   {
      next   = link->dllNext;
      curSeq = link->dllObject;

      if(curSeq->origin == mo)
      {
         // if allowed, stop any other sound playing
         if(curSeq->sequence->nostopcutoff == false)
            S_StopSound(curSeq->origin, CHAN_ALL);

         // if a stopsound is defined, play it
         if(curSeq->sequence->stopsound)
         {
            S_StartSfxInfo(curSeq->origin, curSeq->sequence->stopsound,
                           curSeq->volume, curSeq->attenuation, false,
                           CHAN_AUTO);
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
void S_SquashSequence(mobj_t *mo)
{
   CDLListItem<SndSeq_t> *link = SoundSequences, *next;

   while(link)
   {
      next = link->dllNext;

      if(link->dllObject->origin == mo)
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
void S_KillSequence(mobj_t *mo)
{
   CDLListItem<SndSeq_t> *link = SoundSequences, *next;

   while(link)
   {
      next = link->dllNext;

      if(link->dllObject->origin == mo)
      {
         // stop any sound playing
         S_StopSound(link->dllObject->origin, CHAN_ALL);

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
void S_StopSectorSequence(sector_t *s, boolean floorOrCeiling)
{
   S_StopSequence(SECTOR_ORIGIN(s, floorOrCeiling));
}

//
// S_SquashSectorSequence
//
// Convenience routine.
//
void S_SquashSectorSequence(sector_t *s, boolean floorOrCeiling)
{
   S_SquashSequence(SECTOR_ORIGIN(s, floorOrCeiling));
}

//
// S_StopPolySequence
//
// Convenience routine.
//
void S_StopPolySequence(polyobj_t *po)
{
   S_StopSequence((mobj_t *)&po->spawnSpot);
}

//
// S_StartSequenceNum
//
// Starts a sound sequence by index number. The actual sequence started may be
// altered by sound sequence redirects, depending on the sequence activation
// type.
//
void S_StartSequenceNum(mobj_t *mo, int seqnum, int seqtype, int seqOriginType,
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
   newSeq = (SndSeq_t *)(Z_Calloc(1, sizeof(SndSeq_t), PU_LEVEL, NULL));

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
   boolean ceil = (seqtype == SEQ_CEILING || seqtype == SEQ_DOOR);

   S_StartSequenceNum(SECTOR_ORIGIN(s, ceil), s->sndSeqID, seqtype,
                      ceil ? SEQ_ORIGIN_SECTOR_C : SEQ_ORIGIN_SECTOR_F, 
                      s - sectors);
}

//
// S_ReplaceSectorSequence
//
// Squashes any currently playing sequence and starts a new one.
// Used by bouncing doors.
//
void S_ReplaceSectorSequence(sector_t *s, int seqtype)
{
   boolean ceil = (seqtype == SEQ_CEILING || seqtype == SEQ_DOOR);

   S_SquashSectorSequence(s, ceil);
   
   S_StartSequenceNum(SECTOR_ORIGIN(s, ceil), s->sndSeqID, seqtype,
                      ceil ? SEQ_ORIGIN_SECTOR_C : SEQ_ORIGIN_SECTOR_F, 
                      s - sectors);
}

//
// S_StartPolySequence
//
// Convenience routine. Starts a polyobject sound sequence
//
void S_StartPolySequence(polyobj_t *po)
{
   S_StartSequenceNum((mobj_t *)&po->spawnSpot, po->seqId, SEQ_DOOR, 
                      SEQ_ORIGIN_POLYOBJ, po->id);
}

//
// S_StartSequenceName
//
// Starts the named sound sequence.
//
void S_StartSequenceName(mobj_t *mo, const char *seqname, int seqOriginType, 
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
   newSeq = (SndSeq_t *)(Z_Calloc(1, sizeof(SndSeq_t), PU_LEVEL, NULL));

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
void S_StartSectorSequenceName(sector_t *s, const char *seqname, boolean fOrC)
{
   S_StartSequenceName(SECTOR_ORIGIN(s, fOrC), seqname, 
                       fOrC ? SEQ_ORIGIN_SECTOR_C : SEQ_ORIGIN_SECTOR_F, 
                       s - sectors);
}

//
// S_ReplaceSectorSequenceName
//
// Convenience routine for starting a sector sequence by name without playing
// the stop sound of any currently playing sequence.
//
void S_ReplaceSectorSequenceName(sector_t *s, const char *seqname, boolean fOrC)
{
   S_SquashSectorSequence(s, fOrC);

   S_StartSequenceName(SECTOR_ORIGIN(s, fOrC), seqname, 
                       fOrC ? SEQ_ORIGIN_SECTOR_C : SEQ_ORIGIN_SECTOR_F, 
                       s - sectors);
}

//
// S_StartSeqSound
//
// Starts a sound in the usual manner for a sound sequence.
//
static void S_StartSeqSound(SndSeq_t *seq, boolean loop)
{
   if(seq->currentSound)
   {
      S_StartSfxInfo(seq->origin, seq->currentSound, seq->volume, 
                     seq->attenuation, loop, CHAN_AUTO);
   }
}

// Command argument macros: we peek ahead in the command stream.

#define CMD_ARG1(field) ((curSeq->cmdPtr + 1)-> field )
#define CMD_ARG2(field) ((curSeq->cmdPtr + 2)-> field )

// when true, the current environmental sequence has ended
static boolean enviroSeqFinished;

//
// S_RunSequence
//
// Runs a single sound sequence. This is another one of those miniature
// virtual machines, although this one's not so miniature really O_O
//
static void S_RunSequence(SndSeq_t *curSeq)
{
   boolean isPlaying = false;
   
   // if delaying, count down delay
   if(curSeq->delayCounter)
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
      break;
   case SEQ_CMD_WAITSOUND: // waiting on a sound to finish
      if(!isPlaying)
      {
         curSeq->cmdPtr += 1;
         curSeq->currentSound = NULL;
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
      curSeq->currentSound = NULL;
      curSeq->delayCounter = CMD_ARG1(data);
      curSeq->cmdPtr += 2;
      break;
   case SEQ_CMD_DELAYRANDOM: // delay a random amount within given range
      curSeq->currentSound = NULL;
      {
         int min = CMD_ARG1(data);
         int max = CMD_ARG2(data);
         curSeq->delayCounter = (int)M_RangeRandomEx(min, max);
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
      break;
   case SEQ_CMD_SETVOLUMEREL: // set relative volume
      curSeq->volume += CMD_ARG1(data);
      curSeq->cmdPtr += 2;
      if(curSeq->volume < 0)
         curSeq->volume = 0;
      else if(curSeq->volume > 127)
         curSeq->volume = 127;
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
         enviroSeqFinished = true;
      else if(curSeq->sequence->stopsound == NULL)
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
static void S_RunEnviroSequence(void);
static void S_StopEnviroSequence(void);

//
// S_RunSequences
//
// Updates all running sound sequences.
//
void S_RunSequences(void)
{
   CDLListItem<SndSeq_t> *link = SoundSequences;

   while(link)
   {
      CDLListItem<SndSeq_t> *next = link->dllNext;

      S_RunSequence(link->dllObject);

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
void S_StopAllSequences(void)
{
   // Because everything is allocated PU_LEVEL, simply disconnecting the list
   // head is all that is needed to stop all sequences from playing. The sndseq
   // nodes will all be destroyed by P_SetupLevel.
   SoundSequences = NULL;

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
   10*TICRATE + 31,   // maximum start wait
    6*TICRATE,        // minimum wait between sequences
    6*TICRATE + 255,  // maximum wait between sequences
};

static MobjCollection enviroSpots;
static int enviroTics;
static mobj_t *nextEnviroSpot;

static SndSeq_t enviroSeq;

//
// S_ResetEnviroSeqEngine
//
// Resets the environmental sequence engine.
//
static void S_ResetEnviroSeqEngine(void)
{
   EnviroSequence    = NULL;
   enviroSeqFinished = true;

   if(!P_CollectionIsEmpty(&enviroSpots))
      nextEnviroSpot = P_CollectionGetRandom(&enviroSpots, pr_misc);
   else
      nextEnviroSpot = NULL; // broken, but shouldn't matter

   enviroTics = (int)M_RangeRandomEx(EnviroSeqManager.minStartWait,
                                     EnviroSeqManager.maxStartWait);
}

//
// S_InitEnviroSpots
//
// Puts all the environmental sequence spots on the map into an MobjCollection
// and gets the environmental sequence engine ready to run.
//
void S_InitEnviroSpots(void)
{
   int enviroType = E_ThingNumForName("EEEnviroSequence");

   P_ReInitMobjCollection(&enviroSpots, enviroType);

   if(enviroType != NUMMOBJTYPES)
      P_CollectThings(&enviroSpots);

   S_ResetEnviroSeqEngine();
}

//
// S_RunEnviroSequence
//
// Runs the current environmental sound sequence, or schedules another to run
// if it has finished. Note that environmental sequences can't use looping
// commands or else they'll lock out any other sequence for the rest of the
// map.
//
static void S_RunEnviroSequence(void)
{
   // nothing to do?
   if(P_CollectionIsEmpty(&enviroSpots))
      return;

   // if waiting, count down the wait time
   if(enviroTics)
   {
      enviroTics--;
      return;
   }

   // are we currently playing a sequence?
   if(EnviroSequence)
   {
      // is it finished?
      if(enviroSeqFinished)
      {
         memset(&enviroSeq, 0, sizeof(SndSeq_t));
         EnviroSequence = NULL;
         enviroTics = (int)M_RangeRandomEx(EnviroSeqManager.minEnviroWait,
                                           EnviroSeqManager.maxEnviroWait);
         nextEnviroSpot = P_CollectionGetRandom(&enviroSpots, pr_misc);
      }
      else
         S_RunSequence(EnviroSequence);
   }
   else if(nextEnviroSpot) // next spot must be valid (always should be)
   {
      // start a new sequence
      ESoundSeq_t *edfSeq = E_EnvironmentSequence(nextEnviroSpot->args[0]);

      if(!edfSeq) // woops, bad sequence, try another next time.
      {
         nextEnviroSpot = P_CollectionGetRandom(&enviroSpots, pr_misc);
         return;
      }

      enviroSeq.sequence     = edfSeq;
      enviroSeq.cmdPtr       = edfSeq->commands;
      enviroSeq.currentSound = NULL;
      enviroSeq.origin       = nextEnviroSpot;
      enviroSeq.attenuation  = edfSeq->attenuation;
      enviroSeq.delayCounter = 0;
      enviroSeq.flags        = SEQ_FLAG_ENVIRO; // started by enviro engine
      enviroSeq.originType   = SEQ_ORIGIN_OTHER;
      enviroSeq.originIdx    = -1;

      // possibly randomize the starting volume
      enviroSeq.volume = 
         edfSeq->randvol ? M_RangeRandom(edfSeq->minvolume, edfSeq->volume)
                         : edfSeq->volume;

      EnviroSequence    = &enviroSeq; // now playing an enviro sequence
      enviroSeqFinished = false;      // sequence is not finished
   }
}

//
// S_StopEnviroSequence
//
// Unconditionally stops the environmental sequence engine. Called from
// S_StopAllSequences above.
//
static void S_StopEnviroSequence(void)
{
   // stomp on everything to stop it from running any more sequences
   EnviroSequence = NULL;     // no playing sequence
   nextEnviroSpot = NULL;     // no spot chosen
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
void S_SequenceGameLoad(void)
{
   CDLListItem<SndSeq_t> *link;

   // kill all running sequences
   // note the loop restarts from the beginning each time because S_KillSequence
   // modifies the double-linked list; it'll stop running when the last sequence
   // is deleted.
   while((link = SoundSequences))
      S_KillSequence(link->dllObject->origin);

   // reset the enviro sequence engine in a way that lets it start up again
   S_ResetEnviroSeqEngine();
}

// EOF

