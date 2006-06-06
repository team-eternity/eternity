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

#include "z_zone.h"
#include "s_sndseq.h"
#include "e_things.h"

// Global data

SndSeq_t *SoundSequences; // head of running sndseq list

SndSeq_t *EnviroSequence; // currently playing environmental sequence

//
// S_StopSequence
//
// Stops any sound sequence being played by the given object.
//
void S_StopSequence(mobj_t *mo)
{
   SndSeq_t *curSeq = SoundSequences, *next;

   while(curSeq)
   {
      next = (SndSeq_t *)(curSeq->link.next);

      if(curSeq->origin == mo)
      {
         // if allowed, stop any other sound playing
         if(curSeq->sequence->nostopcutoff == false)
            S_StopSound(curSeq->origin);

         // if a stopsound is defined, play it
         if(curSeq->sequence->stopsound)
         {
            S_StartSfxInfo(curSeq->origin, curSeq->sequence->stopsound,
                           curSeq->volume, curSeq->attenuation, false);
         }

         // unlink and delete this object
         M_DLListRemove((mdllistitem_t *)curSeq);
         Z_Free(curSeq);
      }

      curSeq = next;
   }
}

//
// S_StartSequenceNum
//
// Starts a sound sequence by index number. The actual sequence started may be
// altered by sound sequence redirects, depending on the sequence activation
// type.
//
void S_StartSequenceNum(mobj_t *mo, int seqnum, int seqtype)
{
   ESoundSeq_t *edfSeq;
   SndSeq_t *newSeq;

   // find sequence by number, if none, return
   if(!(edfSeq = E_SequenceForNum(seqnum)))
      return;

   // check for redirection for certain activation types
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
   newSeq = Z_Malloc(sizeof(SndSeq_t), PU_LEVEL, NULL);
   memset(newSeq, 0, sizeof(SndSeq_t));

   M_DLListInsert((mdllistitem_t *)newSeq, (mdllistitem_t **)SoundSequences);

   // set up all fields
   newSeq->origin       = mo;                  // set origin
   newSeq->sequence     = edfSeq;              // set sequence pointer
   newSeq->cmdPtr       = edfSeq->commands;    // set command pointer
   newSeq->attenuation  = edfSeq->attenuation; // use starting attenuation
   newSeq->volume       = edfSeq->volume;      // use starting volume
   newSeq->delayCounter = 0;                   // no delay at start
}

//
// S_StartSequenceName
//
// Starts the named sound sequence.
//
void S_StartSequenceName(mobj_t *mo, const char *seqname)
{
   ESoundSeq_t *edfSeq;
   SndSeq_t *newSeq;

   // find sequence by name, if none, return
   if(!(edfSeq = E_SequenceForName(seqname)))
      return;

   // stop any sequence the object is already playing
   S_StopSequence(mo);

   // allocate a new SndSeq object and link it
   newSeq = Z_Malloc(sizeof(SndSeq_t), PU_LEVEL, NULL);
   memset(newSeq, 0, sizeof(SndSeq_t));

   M_DLListInsert((mdllistitem_t *)newSeq, (mdllistitem_t **)SoundSequences);

   // set up all fields
   newSeq->origin       = mo;                  // set origin
   newSeq->sequence     = edfSeq;              // set sequence pointer
   newSeq->cmdPtr       = edfSeq->commands;    // set command pointer
   newSeq->attenuation  = edfSeq->attenuation; // use starting attenuation
   newSeq->volume       = edfSeq->volume;      // use starting volume
   newSeq->delayCounter = 0;                   // no delay at start
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
                     seq->attenuation, loop);
   }
}

#define CMD_ARG1(field) ((curSeq->cmdPtr + 1)-> field )
#define CMD_ARG2(field) ((curSeq->cmdPtr + 2)-> field )

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
      break;
   case SEQ_CMD_PLAYLOOP: // play sound in a delay loop (doesn't advance)
      curSeq->currentSound = CMD_ARG1(sfx);
      curSeq->delayCounter = CMD_ARG2(data);
      S_StartSeqSound(curSeq, true);
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
         curSeq->delayCounter = M_RangeRandom(min, max);
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
   case SEQ_CMD_END:
      // sequences without a stopsound are ended here
      if(curSeq->sequence->type == SEQ_ENVIRONMENT)
         enviroSeqFinished = true;
      else if(curSeq->sequence->stopsound == NULL)
      {
         // if allowed, stop any other sound playing
         if(curSeq->sequence->nostopcutoff == false)
            S_StopSound(curSeq->origin);
         
         // unlink and delete this object
         M_DLListRemove((mdllistitem_t *)curSeq);
         Z_Free(curSeq);
      }
      break;
   default: // unknown command? try advancing
      curSeq->cmdPtr += 1; 
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
   SndSeq_t *curSeq = SoundSequences;

   while(curSeq)
   {
      SndSeq_t *next = (SndSeq_t *)(curSeq->link.next);

      S_RunSequence(curSeq);

      curSeq = next;
   } // end while

   // run the environmental sequence, if any exists
   S_RunEnviroSequence();
}

//
// S_StopAllSequences
//
// Stops all running sound sequences.
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

//
// S_SetSequenceStatus
//
// Restores a sound sequence's status using data extracted from a game save.
//
void S_SetSequenceStatus(void)
{
}

//=============================================================================
//
// Environmental Ambience Sequences
//
// haleyjd 06/06/06: At long last, I can implement Heretic's global ambience
// effects, but folded within the Hexen-like sound sequence engine.
//

static MobjCollection enviroSpots;
static int enviroTics;
static mobj_t *nextEnviroSpot;

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

   EnviroSequence    = NULL;
   enviroSeqFinished = true;
   enviroTics        = 10 * TICRATE + (M_Random() & 31);

   if(!P_CollectionIsEmpty(&enviroSpots))
      nextEnviroSpot = P_CollectionGetRandom(&enviroSpots, pr_misc);
   else
      nextEnviroSpot = NULL; // broken, but shouldn't matter
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
   static SndSeq_t seq;

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
         memset(&seq, 0, sizeof(SndSeq_t));
         EnviroSequence = NULL;
         enviroTics = 6 * TICRATE + M_Random();
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

      seq.sequence     = edfSeq;
      seq.cmdPtr       = edfSeq->commands;
      seq.currentSound = NULL;
      seq.origin       = nextEnviroSpot;
      seq.attenuation  = edfSeq->attenuation;
      seq.volume       = M_RangeRandom(32, 112);
      seq.delayCounter = 0;

      EnviroSequence    = &seq;   // now playing an enviro sequence
      enviroSeqFinished = false;  // sequence is not finished
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

// EOF

