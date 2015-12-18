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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//  This file supports conversion of MUS format music in memory
//  to MIDI format 1 music in memory. 
//
//  The primary routine, mmus2mid, converts a block of memory in MUS format
//  to an Allegro MIDI structure. This supports playing MUS lumps in a wad
//  file with BOOM.
//
//  Another routine, Midi2MIDI, converts a block of memory in MIDI format 1 to
//  an Allegro MIDI structure. This supports playing MIDI lumps in a wad
//  file with BOOM.
//
//  For testing purposes, and to make a utility if desired, if the symbol
//  STANDALONE is defined by uncommenting the definition below, a main
//  routine is compiled that will convert a possibly wildcarded set of MUS
//  files to a similarly named set of MIDI files.
//
//  Much of the code here is thanks to S. Bacquet's source for QMUS2MID.C
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h> // haleyjd

#ifdef DJGPP // proff: I don't use allegro in windows
#include <allegro.h>
#endif // DJGPP
#include "mmus2mid.h"

//#define STANDALONE  /* uncomment this to make MMUS2MID.EXE */
#ifndef STANDALONE
#include "../z_zone.h"
#else
#define emalloc(t, n) (t)(malloc(n))
#define efree(p)      free(p)
#endif

// some macros to decode mus event bit fields

#define last(e)         ((UBYTE)((e) & 0x80))
#define event_type(e)   ((UBYTE)(((e) & 0x7F) >> 4))
#define channel(e)      ((UBYTE)((e) & 0x0F))

// event types

typedef enum
{
   RELEASE_NOTE,
   PLAY_NOTE,
   BEND_NOTE,
   SYS_EVENT,
   CNTL_CHANGE,
   UNKNOWN_EVENT1,
   SCORE_END,
   UNKNOWN_EVENT2,
} mus_event_t;

// MUS format header structure

// haleyjd 04/05/05: this structure is read directly from memory
// and so it should be packed

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(push, 1)
#endif

struct tagMUSheader
{
   char        ID[4];            // identifier "MUS"0x1A
   UWORD       ScoreLength;      // length of music portion
   UWORD       ScoreStart;       // offset of music portion
   UWORD       channels;         // count of primary channels
   UWORD       SecChannels;      // count of secondary channels
   UWORD       InstrCnt;         // number of instruments
};

typedef struct tagMUSheader MUSheader;

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(pop)
#endif

// to keep track of information in a MIDI track

typedef struct Track
{
   char  velocity;
   int   deltaT;
   UBYTE lastEvt;
   int   alloced;
} TrackInfo;

// array of info about tracks

static TrackInfo track[MIDI_TRACKS];

// initial track size allocation
static ULONG TRACKBUFFERSIZE = 1024L;   

// lookup table MUS -> MID controls 
static UBYTE MUS2MIDcontrol[15] = 
{
   0,         // Program change - not a MIDI control change
   0x00,      // Bank select               
   0x01,      // Modulation pot              
   0x07,      // Volume                  
   0x0A,      // Pan pot                 
   0x0B,      // Expression pot              
   0x5B,      // Reverb depth                
   0x5D,      // Chorus depth                
   0x40,      // Sustain pedal               
   0x43,      // Soft pedal                
   0x78,      // All sounds off              
   0x7B,      // All notes off               
   0x7E,      // Mono                    
   0x7F,      // Poly                    
   0x79       // Reset all controllers           
};

// some strings of bytes used in the midi format 

static UBYTE midikey[]   =
{0x00,0xff,0x59,0x02,0x00,0x00};        // C major
static UBYTE miditempo[] =
{0x00,0xff,0x51,0x03,0x09,0xa3,0x1a};   // uS/qnote
static UBYTE midihdr[]   =
{'M','T','h','d',0,0,0,6,0,1,0,0,0,0};  // header (length 6, format 1)
static UBYTE trackhdr[]  =
{'M','T','r','k'};                      // track header

// static routine prototypes

// proff: changed type for byte from char to unsigned char to avoid warning
static int TWriteByte(MIDI *mididata, int MIDItrack, unsigned char byte);
static int TWriteVarLen(MIDI *mididata, int MIDItrack, ULONG value);
static ULONG ReadTime(UBYTE **musptrp);
static char FirstChannelAvailable(signed char MUS2MIDchannel[]);
static UBYTE MidiEvent(MIDI *mididata,UBYTE midicode,UBYTE MIDIchannel,
                       UBYTE MIDItrack,int nocomp);

//
// TWriteByte()
//
// write one byte to the selected MIDItrack, update current position
// if track allocation exceeded, double it
// if track not allocated, initially allocate TRACKBUFFERSIZE bytes
//
// Passed pointer to Allegro MIDI structure, number of the MIDI track being
// written, and the byte to write.
//
// Returns 0 on success, MEMALLOC if a memory allocation error occurs
//
// proff: changed type for byte from char to unsigned char to avoid warning
//
static int TWriteByte(MIDI *mididata, int MIDItrack, unsigned char byte)
{
   ULONG pos;
   
   pos = mididata->track[MIDItrack].len;
   
   // proff: Added typecast to avoid warning
   if(pos >= (unsigned int)track[MIDItrack].alloced)
   {
      track[MIDItrack].alloced =        // double allocation
        track[MIDItrack].alloced?       // or set initial TRACKBUFFERSIZE
        2*track[MIDItrack].alloced :
        TRACKBUFFERSIZE;

      if(!(mididata->track[MIDItrack].data =     // attempt to reallocate
         erealloc(unsigned char *, mididata->track[MIDItrack].data, track[MIDItrack].alloced)))
         return MEMALLOC;
   }

   mididata->track[MIDItrack].data[pos] = byte;
   mididata->track[MIDItrack].len++;
   return 0;
}

//
// TWriteVarLen()
//
// write the ULONG value to tracknum-th track, in midi format, which is
// big endian, 7 bits per byte, with all bytes but the last flagged by
// bit 8 being set, allowing the length to vary.
//
// Passed the Allegro MIDI structure, the track number to write,
// and the ULONG value to encode in midi format there
//
// Returns 0 if sucessful, MEMALLOC if a memory allocation error occurs
//
static int TWriteVarLen(MIDI *mididata, int tracknum, ULONG value)
{
   ULONG buffer;
   
   buffer = value & 0x7f;

   while((value >>= 7))         // terminates because value unsigned
   {
      buffer <<= 8;               // note first value shifted in has bit 8 clear
      buffer |= 0x80;             // all succeeding values do not
      buffer += (value & 0x7f);
   }

   while(1)                     // write bytes out in opposite order
   {
      // proff: Added typecast to avoid warning
      if(TWriteByte(mididata, tracknum, (char)(buffer&0xff))) // insure buffer masked
         return MEMALLOC;
      
      if(buffer & 0x80)
         buffer >>= 8;
      else                        // terminate on the byte with bit 8 clear
         break;
   }
   return 0;
}

//
// ReadTime()
//
// Read a time value from the MUS buffer, advancing the position in it
//
// A time value is a variable length sequence of 8 bit bytes, with all
// but the last having bit 8 set.
//
// Passed a pointer to the pointer to the MUS buffer
// Returns the integer unsigned long time value there and advances the pointer
//
static ULONG ReadTime(UBYTE **musptrp)
{
   ULONG timeval = 0;
   int byte;
   
   do    // shift each byte read up in the result until a byte with bit 8 clear
   {
      byte = *(*musptrp)++;
      timeval = (timeval << 7) + (byte & 0x7F);
   }
   while(byte & 0x80);
   
   return timeval;
}

//
// FirstChannelAvailable()
//
// Return the next unassigned MIDI channel number
//
// The assignment for MUS channel 15 is not counted in the caculation, that
// being percussion and always assigned to MIDI channel 9 (base 0).
//
// Passed the array of MIDI channels assigned to MUS channels
// Returns the maximum channel number unassigned unless that is 9 in which
// case 10 is returned.
//
static char FirstChannelAvailable(signed char MUS2MIDchannel[])
{
   int i ;
   signed char max = -1 ;
   
   // find the largest MIDI channel assigned so far
   for(i = 0; i < 15; i++)
   {
      if(MUS2MIDchannel[i] > max)
         max = MUS2MIDchannel[i];
   }
      
   return (max == 8 ? 10 : max+1); // skip MIDI channel 9 (percussion)
}

//
// MidiEvent()
//
// Constructs a MIDI event code, and writes it to the current MIDI track
// unless its the same as the last event code and compression is enabled
// in which case nothing is written.
//
// Passed the Allegro MIDI structure, the midi event code, the current
// MIDI channel number, the current MIDI track number, and whether compression
// (running status) is enabled.
//
// Returns the new event code if successful, 0 if a memory allocation error
//
static UBYTE MidiEvent(MIDI *mididata,UBYTE midicode,UBYTE MIDIchannel,
        UBYTE MIDItrack,int nocomp)
{
   UBYTE newevent;
   
   newevent = midicode | MIDIchannel;
   if((newevent != track[MIDItrack].lastEvt) || nocomp)
   {
      if(TWriteByte(mididata,MIDItrack, newevent))
         return 0;                                    // indicates MEMALLOC error
      track[MIDItrack].lastEvt = newevent;
   }
   return newevent;
}

#define MAX_HEADER_SCAN 32

//
// mmuscheckformat
//
// haleyjd 11/23/12:
// Returns true if the data is a MUS.
//
bool mmuscheckformat(UBYTE *mus, int size)
{
   UBYTE *hptr = mus;
   while(hptr < mus + size - sizeof(MUSheader) && 
         hptr < mus + MAX_HEADER_SCAN &&
         strncmp((const char *)hptr, "MUS\x1a", 4))
      ++hptr;

   if(hptr < mus + size - sizeof(MUSheader) && 
      !strncmp((const char *)hptr, "MUS\x1a", 4))
      return true;

   return false;
}

//
// mmus2mid()
//
// Convert a memory buffer containing MUS data to an Allegro MIDI structure
// with specified time division and compression.
//
// Passed a pointer to the buffer containing MUS data, a pointer to the
// Allegro MIDI structure, the divisions, and a flag whether to compress.
//
// Returns 0 if successful, otherwise an error code (see mmus2mid.h).
//
int mmus2mid(UBYTE *mus, size_t size, MIDI *mididata, UWORD division, int nocomp)
{
   UWORD TrackCnt = 0;
   UBYTE evt, MUSchannel, MIDIchannel, MIDItrack=0;
   int i, event, data;
   UBYTE *musptr, *hptr;
   size_t muslen;
   static MUSheader MUSh;
   ULONG DeltaTime, TotalTime = 0;
   // proff: This arrays were too small
   //  UBYTE MIDIchan2track[16];
   //  signed char MUS2MIDchannel[16];
   UBYTE MIDIchan2track[MIDI_TRACKS];
   signed char MUS2MIDchannel[MIDI_TRACKS];

   // haleyjd 04/04/10: don't bite off more than you can chew
   if(size < sizeof(MUSheader))
      return MUSDATAMT;

   // haleyjd 04/04/10: scan forward for a MUS header. Evidently DMX was 
   // capable of doing this, and would skip over any intervening data. That, 
   // or DMX doesn't use the MUS header at all somehow.
   hptr = mus;
   while(hptr < mus + size - sizeof(MUSheader) && 
         hptr < mus + MAX_HEADER_SCAN &&
         strncmp((const char *)hptr, "MUS\x1a", 4))
      ++hptr;

   // if we found a likely header start, reset the mus pointer to that location,
   // otherwise just leave it alone and pray.
   if(hptr < mus + size - sizeof(MUSheader) && 
      !strncmp((const char *)hptr, "MUS\x1a", 4))
      mus = hptr;

   // copy the MUS header from the MUS buffer to the MUSh header structure
   
   memcpy(&MUSh, mus, sizeof(MUSheader));
   
   // check some things and set length of MUS buffer from internal data
   
   if(!(muslen = MUSh.ScoreLength + MUSh.ScoreStart))
      return MUSDATAMT;     // MUS file empty
   
   if(MUSh.channels > 15)       // MUSchannels + drum channel > 16
      return TOOMCHAN ;

   musptr = mus+MUSh.ScoreStart; // init musptr to start of score
   
   for(i = 0; i < MIDI_TRACKS; i++)   // init the track structure's tracks
   {
      MUS2MIDchannel[i] = -1;       // flag for channel not used yet
      track[i].velocity = 64;
      track[i].deltaT = 0;
      track[i].lastEvt = 0;
      efree(mididata->track[i].data);//jff 3/5/98 remove old allocations
      mididata->track[i].data=NULL;
      track[i].alloced = 0;
      mididata->track[i].len = 0;
   }

   if(!division)
      division = 70;

   // allocate the first track which is a special tempo/key track
   // note multiple tracks means midi format 1
   
   // set the divisions (ticks per quarter note)
   mididata->divisions = division;
   
   // allocate for midi tempo/key track, allow for end of track 
   if(!(mididata->track[0].data =
      erealloc(unsigned char *, mididata->track[0].data,sizeof(midikey)+sizeof(miditempo)+4)))
      return MEMALLOC;

   // key C major
   memcpy(mididata->track[0].data,midikey,sizeof(midikey));
   // tempo uS/qnote 
   memcpy(mididata->track[0].data+sizeof(midikey),miditempo,sizeof(miditempo));
   mididata->track[0].len = sizeof(midikey)+sizeof(miditempo); 

   TrackCnt++;   // music tracks start at 1
   
   // process the MUS events in the MUS buffer
   
   do 
   {
      // get a mus event, decode its type and channel fields
      event = *musptr++;
      if((evt = event_type(event)) == SCORE_END) //jff 1/23/98 use symbol
         break;  // if end of score event, leave
      MUSchannel = channel(event);

      // if this channel not initialized, do so
      if(MUS2MIDchannel[MUSchannel] == -1)
      {
         // set MIDIchannel and MIDItrack
         
         MIDIchannel = MUS2MIDchannel[MUSchannel] = 
            (MUSchannel == 15 ? 9 : FirstChannelAvailable(MUS2MIDchannel));
         // proff: Added typecast to avoid warning
         MIDItrack = MIDIchan2track[MIDIchannel] = (unsigned char)(TrackCnt++);

         if(TWriteByte(mididata, MIDItrack, 0x00)) // haleyjd 12/30/13: send all notes off 
            goto err;
         if(TWriteByte(mididata, MIDItrack, 0xB0|MIDIchannel))
            goto err;
         if(TWriteByte(mididata, MIDItrack, 0x7B))
            goto err;
         if(TWriteByte(mididata, MIDItrack, 0x00))
            goto err;        
      }
      else // channel already allocated as a track, use those values
      {
         MIDIchannel = MUS2MIDchannel[MUSchannel];
         MIDItrack   = MIDIchan2track[MIDIchannel];
      }

      if(TWriteVarLen(mididata, MIDItrack, track[MIDItrack].deltaT))
         goto err;
      track[MIDItrack].deltaT = 0;

      switch(evt)
      {
      case RELEASE_NOTE:
         if(!MidiEvent(mididata,0x90,MIDIchannel,MIDItrack,nocomp))
            goto err;
         
         data = *musptr++;
         // proff: Added typecast to avoid warning
         if(TWriteByte(mididata, MIDItrack, (unsigned char)(data & 0x7F)))
            goto err;
         if(TWriteByte(mididata, MIDItrack, 0))
            goto err;
         break;

      case PLAY_NOTE:
         if(!MidiEvent(mididata,0x90,MIDIchannel,MIDItrack,nocomp))
            goto err;
         
         data = *musptr++;
         // proff: Added typecast to avoid warning
         if(TWriteByte(mididata, MIDItrack, (unsigned char)(data & 0x7F)))
            goto err;
         if(data & 0x80)
            track[MIDItrack].velocity = (*musptr++) & 0x7f;
         if(TWriteByte(mididata, MIDItrack, track[MIDItrack].velocity))
            goto err;
         break;

      case BEND_NOTE:
         if(!MidiEvent(mididata,0xE0,MIDIchannel,MIDItrack,nocomp))
            goto err;

         data = *musptr++;
         // proff: Added typecast to avoid warning
         if(TWriteByte(mididata, MIDItrack, (unsigned char)((data & 1) << 6)))
            goto err;
         // proff: Added typecast to avoid warning
         if(TWriteByte(mididata, MIDItrack, (unsigned char)(data >> 1)))
            goto err;
         break;

      case SYS_EVENT:
         if(!MidiEvent(mididata,0xB0,MIDIchannel,MIDItrack,nocomp))
            goto err;
         
         data = *musptr++;
         if(data<10 || data>14)
            return BADSYSEVT;
         
         if(TWriteByte(mididata, MIDItrack, MUS2MIDcontrol[data]))
            goto err;
         if(data == 12)
         {
            // proff: Added typecast to avoid warning
            if(TWriteByte(mididata, MIDItrack, (unsigned char)(MUSh.channels+1)))
               goto err;
         }
         else if(TWriteByte(mididata, MIDItrack, 0))
            goto err;
         break;

      case CNTL_CHANGE:
         data = *musptr++;
         if(data>9)
            return BADCTLCHG;
         
         if(data)
         {
            if(!MidiEvent(mididata,0xB0,MIDIchannel,MIDItrack,nocomp))
               goto err;
            
            if(TWriteByte(mididata, MIDItrack, MUS2MIDcontrol[data]))
               goto err;
         }
         else
         {
            if(!MidiEvent(mididata,0xC0,MIDIchannel,MIDItrack,nocomp))
               goto err;
         }
         data = *musptr++;
         // Gez: Fix TNT.WAD's D_STALKS, based on Ben Ryves's fix in MUS2MID
         if(data & 0x80)
            data = 0x7F;
         // proff: Added typecast to avoid warning
         if(TWriteByte(mididata, MIDItrack, (unsigned char)data))
            goto err;
         break;
         
      case UNKNOWN_EVENT1:   // mus events 5 and 7
      case UNKNOWN_EVENT2:   // meaning not known
         return BADMUSCTL;
         
      case SCORE_END:
         break;
         
      default:
         return BADMUSCTL;   // exit with error
      }
      if(last(event))
      {
         DeltaTime = ReadTime(&musptr);
         TotalTime += DeltaTime ;
         for(i = 0;i < MIDI_TRACKS; i++) //jff 3/13/98 update all tracks
            track[i].deltaT += DeltaTime; //whether allocated yet or not
      }
   }
   while ((evt != SCORE_END) && (size_t)(musptr-mus) < muslen);

   if(evt!=SCORE_END)
      return MUSDATACOR;

   // Now add an end of track to each mididata track, correct allocation

   for(i = 0; i < MIDI_TRACKS; i++)
   {
      if(mididata->track[i].len)
      {
         if(TWriteByte(mididata, i, 0x00)) // midi end of track code
            goto err;
         if(TWriteByte(mididata, i, 0xFF))
            goto err;
         if(TWriteByte(mididata, i, 0x2F))
            goto err;
         if(TWriteByte(mididata, i, 0x00))
            goto err;
         // jff 1/23/98 fix failure to set data NULL, len 0 for unused tracks
         // shorten allocation to proper length (important for Allegro)
         if(!(mididata->track[i].data =
            erealloc(unsigned char *, mididata->track[i].data,mididata->track[i].len)))
            return MEMALLOC;
      }
      else
      {
         efree(mididata->track[i].data);
         mididata->track[i].data = NULL;
      }
   }
   
   return 0;
err:
   return MEMALLOC;
}

//
// ReadLength()
//
// Reads the length of a chunk in a midi buffer, advancing the pointer
// 4 bytes, bigendian
//
// Passed a pointer to the pointer to a MIDI buffer
// Returns the chunk length at the pointer position
//
size_t ReadLength(UBYTE **mid)
{
   UBYTE *midptr = *mid;
   
   size_t length = (*midptr++)<<24;
   length += (*midptr++)<<16;
   length += (*midptr++)<<8;
   length += *midptr++;
   *mid = midptr;
   return length;
}

//
// MidiToMIDI()
//
// Convert an in-memory copy of a MIDI format 0 or 1 file to 
// an Allegro MIDI structure, that is valid or has been zeroed
//
// Passed a pointer to a memory buffer with MIDI format music in it and a
// pointer to an Allegro MIDI structure. 
//
// Returns 0 if successful, BADMIDHDR if the buffer is not MIDI format
//
int MidiToMIDI(UBYTE *mid,MIDI *mididata)
{
   int i;
   int ntracks;
   
   // read the midi header
   
   if(memcmp(mid,midihdr,4))
      return BADMIDHDR;
   
   mididata->divisions = (mid[12]<<8)+mid[13];
   ntracks = (mid[10]<<8)+mid[11];
   
   if(ntracks>=MIDI_TRACKS)
      return BADMIDHDR;
   
   mid += 4;
   mid += ReadLength(&mid);            // seek past header
   
   // now read each track
   
   for(i = 0; i < ntracks; i++)
   {
      while(memcmp(mid,trackhdr,4))    // simply skip non-track data
      {
         mid += 4;
         mid += ReadLength(&mid);
      }
      mid += 4;
      mididata->track[i].len = static_cast<int>(ReadLength(&mid));  // get length, move mid past it
      
      // read a track
      mididata->track[i].data = 
         erealloc(unsigned char *, mididata->track[i].data,mididata->track[i].len);
      memcpy(mididata->track[i].data,mid,mididata->track[i].len);
      mid += mididata->track[i].len;
   }

   for(; i < MIDI_TRACKS; i++)
   {
      if(mididata->track[i].len)
      {
         efree(mididata->track[i].data);
         mididata->track[i].data = NULL;
         mididata->track[i].len = 0;
      }
   }

   return 0;
}

//#ifdef STANDALONE /* this code unused by BOOM provided for future portability */
//                  /* it also provides a MUS to MID file converter*/
// proff: I moved this down, because I need MIDItoMidi

#ifdef STANDALONE
//
// FreeTracks()
//
// Free all track allocations in the MIDI structure
//
// Passed a pointer to an Allegro MIDI structure
// Returns nothing
// 
static void FreeTracks(MIDI *mididata)
{
   int i;
   
   for(i = 0; i < MIDI_TRACKS; i++)
   {
      efree(mididata->track[i].data);
      mididata->track[i].data = NULL;
      mididata->track[i].len = 0;
   }
}
#endif

//
// TWriteLength()
//
// Write the length of a MIDI chunk to a midi buffer. The length is four
// bytes and is written byte-reversed for bigendian. The pointer to the
// midi buffer is advanced.
//
// Passed a pointer to the pointer to a midi buffer, and the length to write
// Returns nothing
//
static void TWriteLength(UBYTE **midiptr, ULONG length)
{
   // proff: Added typecast to avoid warning
   *(*midiptr)++ = (unsigned char)((length >> 24) & 0xff);
   *(*midiptr)++ = (unsigned char)((length >> 16) & 0xff);
   *(*midiptr)++ = (unsigned char)((length >>  8) & 0xff);
   *(*midiptr)++ = (unsigned char)((length      ) & 0xff);
}

//
// MIDIToMidi()
//
// This routine converts an Allegro MIDI structure to a midi 1 format file
// in memory. It is used to support memory MUS -> MIDI conversion
//
// Passed a pointer to an Allegro MIDI structure, a pointer to a pointer to
// a buffer containing midi data, and a pointer to a length return.
// Returns 0 if successful, MEMALLOC if a memory allocation error occurs
//
int MIDIToMidi(MIDI *mididata, UBYTE **mid, int *midlen)
{
   size_t total;
   int i,ntrks;
   UBYTE *midiptr;
   
   // calculate how long the mid buffer must be, and allocate
   
   total = sizeof(midihdr);
   for(i = 0, ntrks = 0; i < MIDI_TRACKS; i++)
   {
      if(mididata->track[i].len)
      {
         total += 8 + mididata->track[i].len; // Track hdr + track length
         ntrks++;
      }
   }
   if((*mid = emalloc(UBYTE *, total)) == NULL)
      return MEMALLOC;
   

   // fill in number of tracks and bigendian divisions (ticks/qnote)
   midihdr[10] = 0;
   midihdr[11] = (UBYTE)ntrks;   // set number of tracks in header
   midihdr[12] = (mididata->divisions >> 8) & 0x7f;
   midihdr[13] = (mididata->divisions     ) & 0xff;

   // write the midi header
   
   midiptr = *mid;
   memcpy(midiptr,midihdr,sizeof(midihdr));
   midiptr += sizeof(midihdr);
   
   // write the tracks
   
   for(i = 0; i < MIDI_TRACKS; i++)
   {
      if(mididata->track[i].len)
      {
         memcpy(midiptr,trackhdr,sizeof(trackhdr));    // header
         midiptr += sizeof(trackhdr);
         TWriteLength(&midiptr,mididata->track[i].len);  // track length
         // data
         memcpy(midiptr,mididata->track[i].data,mididata->track[i].len); 
         midiptr += mididata->track[i].len;
      }
   }

   // return length information
   
   *midlen = static_cast<int>(midiptr - *mid);
   
   return 0;
}

#ifdef STANDALONE /* this code unused by BOOM provided for future portability */
                  /* it also provides a MUS to MID file converter*/
// proff: I moved this down, because I need MIDItoMidi

//
// main()
//
// Main routine that will convert a globbed set of MUS files to the
// correspondingly named MID files using mmus2mid(). Only compiled
// if the STANDALONE symbol is defined.
//
// Passed the command line arguments, returns 0 if successful
//
int main(int argc,char **argv)
{
   FILE *musst,*midst;
   char musfile[FILENAME_MAX],midfile[FILENAME_MAX];
   MUSheader MUSh;
   UBYTE *mus,*mid;
   static MIDI mididata;
   int err,midlen;
   char *p,*q;
   int i;
   
   if(argc < 2)
   {
      printf("Usage: MMUS2MID musfile[.MUS]\n");
      printf("writes musfile.MID as output\n");
      printf("musfile may contain wildcards\n");
      exit(1);
   }

   for(i = 1; i < argc; i++)
   {
      strcpy(musfile,argv[i]);
      p = strrchr(musfile,'.');
      q = strrchr(musfile,'\\');
      if (p && (!q || q<p)) *p='\0';
      strcpy(midfile,musfile);
      strcat(musfile,".MUS");
      strcat(midfile,".MID");
      
      musst = fopen(musfile,"rb");
      if(musst)
      {
         fread(&MUSh,sizeof(MUSheader),1,musst);
         mus = (UBYTE *)(malloc(MUSh.ScoreLength+MUSh.ScoreStart));
         if(mus)
         {
            fseek(musst,0,SEEK_SET);
            if(!fread(mus,MUSh.ScoreLength+MUSh.ScoreStart,1,musst))
            {          
               printf("Error reading MUS file\n");
               efree(mus);
               exit(1);
            }
            fclose(musst);
         }
         else
         {
            printf("Out of memory\n");
            efree(mus);
            exit(1);
         }
         
         err = mmus2mid(mus,&mididata,89,1);
         if(err)
         {
            printf("Error converting MUS file to MIDI: %d\n",err);
            exit(1);
         }
         efree(mus);
         
         MIDIToMidi(&mididata,&mid,&midlen);

         midst = fopen(midfile,"wb");
         if(midst)
         {
            if(!fwrite(mid,midlen,1,midst))
            {
               printf("Error writing MIDI file\n");
               FreeTracks(&mididata);
               efree(mid);
               exit(1);
            }
            fclose(midst);
         }
         else
         {
            printf("Can't open MIDI file for output: %s\n", midfile);
            FreeTracks(&mididata);
            efree(mid);
            exit(1);
         }
      }
      else
      {
         printf("Can't open MUS file for input: %s\n", midfile);
         exit(1);
      }
      
      printf("MUS file %s converted to MIDI file %s\n",musfile,midfile);
      FreeTracks(&mididata);
      efree(mid);
   }

   return 0;
}

#endif
//----------------------------------------------------------------------------
//
// $Log: mmus2mid.c,v $
// Revision 1.12  1998/09/07  20:09:38  jim
// Logical output routine added
//
// Revision 1.11  1998/07/14  20:07:21  jim
// correction of minor errors
//
// Revision 1.10  1998/05/10  23:00:43  jim
// formatted/documented mmus2mid
//
// Revision 1.9  1998/03/14  17:16:19  jim
// Fixed track timing problem
//
// Revision 1.8  1998/03/05  16:59:55  jim
// Fixed minor error in mus to midi conversion
//
// Revision 1.7  1998/02/08  15:15:47  jim
// Added native midi support
//
// Revision 1.6  1998/01/26  19:23:54  phares
// First rev with no ^Ms
//
// Revision 1.5  1998/01/23  20:26:20  jim
// Fix bug causing leftover tracks to persist
//
// Revision 1.4  1998/01/21  17:41:13  rand
// Added rcsid string back that Jim took out.
//
// Revision 1.3  1998/01/21  16:56:20  jim
// Music fixed, defaults for cards added
//
// Revision 1.2  1998/01/19  23:36:15  rand
// Added rcsid and Id: strings at top of file
//
// Revision 1.1.1.1  1998/01/19  14:03:10  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
