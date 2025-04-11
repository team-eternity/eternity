//
// Copyright (C) 2005-2014 Simon Howard
// Copyright (C) 2025 James Haley et al.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//----------------------------------------------------------------------------
//
// Purpose: Reading of MIDI files
// Authors: Simon Howard, James Haley, Roman Fomin, Max Waine
//


#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "z_zone.h"

#include "doomtype.h"
#include "m_swap.h"
#include "i_system.h"
#include "midifile.h"

#define HEADER_CHUNK_ID "MThd"
#define TRACK_CHUNK_ID  "MTrk"
#define MAX_BUFFER_SIZE 0x10000

// haleyjd 09/09/10: packing required
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

struct PACKED_PREFIX chunk_header_t
{
   byte chunk_id[4];
   unsigned int chunk_size;
} PACKED_SUFFIX;

struct PACKED_PREFIX midi_header_t
{
   chunk_header_t chunk_header;
   unsigned short format_type;
   unsigned short num_tracks;
   unsigned short time_division;
} PACKED_SUFFIX;

// haleyjd 09/09/10: packing off.
#ifdef _MSC_VER
#pragma pack(pop)
#endif

struct midi_track_t
{
   // Length in bytes
   unsigned int data_len;

   // Events in this track:
   midi_event_t *events;
   int num_events;
};

struct midi_track_iter_t
{
   midi_track_t *track;
   unsigned int position;
   unsigned int loop_point;
};

struct midi_file_t
{
   midi_header_t header;

   // All tracks in this file:
   midi_track_t *tracks;
   unsigned int num_tracks;

   // Data buffer used to store data read for SysEx or meta events:
   byte *buffer;
   unsigned int buffer_size;
};

// Check the header of a chunk:

static bool CheckChunkHeader(chunk_header_t *chunk, const char *expected_id)
{
   const bool result = (memcmp((char *)chunk->chunk_id, expected_id, 4) == 0);

   if(!result)
   {
      printf(
         "CheckChunkHeader: Expected '%s' chunk header, got '%c%c%c%c'\n",
         expected_id, chunk->chunk_id[0], chunk->chunk_id[1], chunk->chunk_id[2], chunk->chunk_id[3]
      );
   }

   return result;
}

// Read a single byte.  Returns false on error.

static bool ReadByte(byte *result, SDL_RWops *stream)
{
   int c;

   if(SDL_RWread(stream, &c, 1, 1) == 0)
   {
      printf("ReadByte: Unexpected end of file\n");
      return false;
   }
   else
   {
      *result = byte(c);

      return true;
   }
}

// Read a variable-length value.

static bool ReadVariableLength(unsigned int *result, SDL_RWops *stream)
{
   byte b = 0;

   *result = 0;

   for(int i = 0; i < 4; ++i)
   {
      if(!ReadByte(&b, stream))
      {
         printf("ReadVariableLength: Error while reading variable-length value\n");
         return false;
      }

      // Insert the bottom seven bits from this byte
      *result <<= 7;
      *result |= b & 0x7f;

      // If the top bit is not set, this is the end
      if((b & 0x80) == 0)
         return true;
   }

   printf("ReadVariableLength: Variable-length value too "
      "long: maximum of four bytes\n");
   return false;
}

//
// Read a byte sequence into the data buffer.
//
static byte *ReadByteSequence(unsigned int num_bytes, SDL_RWops *stream)
{
   byte *result;

   // Allocate a buffer. Allocate one extra byte, as malloc(0) is
   // non-portable.

   result = emalloc(byte *, num_bytes + 1);

   if(result == nullptr)
   {
      printf("ReadByteSequence: Failed to allocate buffer\n");
      return nullptr;
   }

   // Read the data:

   for(unsigned int i = 0; i < num_bytes; ++i)
   {
      if(!ReadByte(&result[i], stream))
      {
         printf("ReadByteSequence: Error while reading byte %u\n", i);
         efree(result);
         return nullptr;
      }
   }

   return result;
}

// Read a MIDI channel event.
// two_param indicates that the event type takes two parameters
// (three byte) otherwise it is single parameter (two byte)

static bool ReadChannelEvent(midi_event_t *event, byte event_type, bool two_param, SDL_RWops *stream)
{
   byte b = 0;

   // Set basics:

   event->event_type = midi_event_type_e(event_type & 0xf0);
   event->data.channel.channel = event_type & 0x0f;

   // Read parameters:

   if(!ReadByte(&b, stream))
   {
      printf("ReadChannelEvent: Error while reading channel event parameters\n");
      return false;
   }

   event->data.channel.param1 = b;

   // Second parameter:

   if(two_param)
   {
      if(!ReadByte(&b, stream))
      {
         printf("ReadChannelEvent: Error while reading channel event parameters\n");
         return false;
      }

      event->data.channel.param2 = b;
   }

   return true;
}

// Read sysex event:

static bool ReadSysExEvent(midi_event_t *event, int event_type, SDL_RWops *stream)
{
   event->event_type = midi_event_type_e(event_type);

   if(!ReadVariableLength(&event->data.sysex.length, stream))
   {
      printf("ReadSysExEvent: Failed to read length of SysEx block\n");
      return false;
   }

   // Read the byte sequence:

   event->data.sysex.data = ReadByteSequence(event->data.sysex.length, stream);

   if(event->data.sysex.data == nullptr)
   {
      printf("ReadSysExEvent: Failed while reading SysEx event\n");
      return false;
   }

   return true;
}

// Read meta event:

static bool ReadMetaEvent(midi_event_t *event, SDL_RWops *stream)
{
   byte b = 0;

   event->event_type = MIDI_EVENT_META;

   // Read meta event type:

   if(!ReadByte(&b, stream))
   {
      printf("ReadMetaEvent: Failed to read meta event type\n");
      return false;
   }

   event->data.meta.type = b;

   // Read length of meta event data:

   if(!ReadVariableLength(&event->data.meta.length, stream))
   {
      printf("ReadSysExEvent: Failed to read length of SysEx block\n");
      return false;
   }

   // Read the byte sequence:

   event->data.meta.data = ReadByteSequence(event->data.meta.length, stream);

   if(event->data.meta.data == nullptr)
   {
      printf("ReadSysExEvent: Failed while reading SysEx event\n");
      return false;
   }

   return true;
}

static bool ReadEvent(midi_event_t *event, unsigned int *last_event_type,
   SDL_RWops *stream)
{
   byte event_type = 0;

   if(!ReadVariableLength(&event->delta_time, stream))
   {
      printf("ReadEvent: Failed to read event timestamp\n");
      return false;
   }

   if(!ReadByte(&event_type, stream))
   {
      printf("ReadEvent: Failed to read event type\n");
      return false;
   }

   // All event types have their top bit set.  Therefore, if 
   // the top bit is not set, it is because we are using the "same
   // as previous event type" shortcut to save a byte.  Skip back
   // a byte so that we read this byte again.

   if((event_type & 0x80) == 0)
   {
      event_type = *last_event_type;

      if(SDL_RWseek(stream, -1, RW_SEEK_CUR) < 0)
      {
         printf("ReadEvent: Unable to seek in stream\n");
         return false;
      }
   }
   else
      *last_event_type = event_type;

   // Check event type:

   switch(event_type & 0xf0)
   {
      // Two parameter channel events:

   case MIDI_EVENT_NOTE_OFF:
   case MIDI_EVENT_NOTE_ON:
   case MIDI_EVENT_AFTERTOUCH:
   case MIDI_EVENT_CONTROLLER:
   case MIDI_EVENT_PITCH_BEND:
      return ReadChannelEvent(event, event_type, true, stream);

      // Single parameter channel events:

   case MIDI_EVENT_PROGRAM_CHANGE:
   case MIDI_EVENT_CHAN_AFTERTOUCH:
      return ReadChannelEvent(event, event_type, false, stream);

   default:
      break;
   }

   // Specific value?

   switch(event_type)
   {
   case MIDI_EVENT_SYSEX:
   case MIDI_EVENT_SYSEX_SPLIT:
      return ReadSysExEvent(event, event_type, stream);

   case MIDI_EVENT_META:
      return ReadMetaEvent(event, stream);

   default:
      break;
   }

   printf("ReadEvent: Unknown MIDI event type: 0x%x\n", event_type);
   return false;
}

// Free an event:

static void FreeEvent(midi_event_t *event)
{
   // Some event types have dynamically allocated buffers assigned
   // to them that must be freed.

   switch(event->event_type)
   {
   case MIDI_EVENT_SYSEX:
   case MIDI_EVENT_SYSEX_SPLIT:
      efree(event->data.sysex.data);
      break;

   case MIDI_EVENT_META:
      efree(event->data.meta.data);
      break;

   default:
      // Nothing to do.
      break;
   }
}

// Read and check the track chunk header

static bool ReadTrackHeader(midi_track_t *track, SDL_RWops *stream)
{
   size_t records_read;
   chunk_header_t chunk_header;

   records_read = SDL_RWread(stream, &chunk_header, sizeof(chunk_header_t), 1);

   if(records_read < 1)
      return false;

   if(!CheckChunkHeader(&chunk_header, TRACK_CHUNK_ID))
      return false;

   track->data_len = SwapBigULong(chunk_header.chunk_size);

   return true;
}

static bool ReadTrack(midi_track_t *track, SDL_RWops *stream)
{
   midi_event_t *new_events;
   midi_event_t *event;
   unsigned int  last_event_type;

   track->num_events = 0;
   track->events     = nullptr;

   // Read the header:

   if(!ReadTrackHeader(track, stream))
      return false;

   // Then the events:

   last_event_type = 0;

   for(;;)
   {
      // Resize the track slightly larger to hold another event:

      new_events    = erealloc(midi_event_t *, track->events, sizeof(midi_event_t) * (track->num_events + 1));
      track->events = new_events;

      // Read the next event:
      event = &track->events[track->num_events];
      if(!ReadEvent(event, &last_event_type, stream))
         return false;

      ++track->num_events;

      // End of track?
      if(event->event_type == MIDI_EVENT_META && event->data.meta.type == MIDI_META_END_OF_TRACK)
         break;
   }

   return true;
}

//
// Free a track
//
static void FreeTrack(midi_track_t *track)
{
   for(int i = 0; i < track->num_events; ++i)
      FreeEvent(&track->events[i]);

   efree(track->events);
}

static bool ReadAllTracks(midi_file_t *file, SDL_RWops *stream)
{
   // Allocate list of tracks
   file->tracks = estructalloc(midi_track_t, file->num_tracks);

   if(file->tracks == nullptr)
      return false;

   // Read each track
   for(unsigned int i = 0; i < file->num_tracks; ++i)
   {
      if(!ReadTrack(&file->tracks[i], stream))
         return false;
   }

   return true;
}

// Read and check the header chunk.

static bool ReadFileHeader(midi_file_t *file, SDL_RWops *stream)
{
   size_t records_read;
   unsigned int format_type;

   records_read = SDL_RWread(stream, &file->header, sizeof(midi_header_t), 1);

   if(records_read < 1)
      return false;

   if(!CheckChunkHeader(&file->header.chunk_header, HEADER_CHUNK_ID) ||
      SwapBigULong(file->header.chunk_header.chunk_size) != 6)
   {
      printf(
         "ReadFileHeader: Invalid MIDI chunk header! chunk_size=%i\n",
         SwapBigULong(file->header.chunk_header.chunk_size)
      );
      return false;
   }

   format_type      = SwapBigUShort(file->header.format_type);
   file->num_tracks = SwapBigUShort(file->header.num_tracks);

   if((format_type != 0 && format_type != 1) || file->num_tracks < 1)
   {
      printf("ReadFileHeader: Only type 0/1 MIDI files supported!\n");
      return false;
   }

   return true;
}

void MIDI_FreeFile(midi_file_t *file)
{
   if(file->tracks != nullptr)
   {
      for(unsigned int i = 0; i < file->num_tracks; ++i)
         FreeTrack(&file->tracks[i]);

      efree(file->tracks);
   }

   efree(file);
}

midi_file_t *MIDI_LoadFile(SDL_RWops *stream)
{
   midi_file_t *file;

   file = estructalloc(midi_file_t, 1);

   if(file == nullptr)
      return nullptr;

   // Open file
   if(stream == nullptr)
   {
      printf("MIDI_LoadFile: Failed to load\n");
      MIDI_FreeFile(file);
      return nullptr;
   }

   // Read MIDI file header
   if(!ReadFileHeader(file, stream))
   {
      SDL_RWclose(stream);
      MIDI_FreeFile(file);
      return nullptr;
   }

   // Read all tracks
   if(!ReadAllTracks(file, stream))
   {
      SDL_RWclose(stream);
      MIDI_FreeFile(file);
      return nullptr;
   }

   SDL_RWclose(stream);

   return file;
}

// Get the number of tracks in a MIDI file.

unsigned int MIDI_NumTracks(midi_file_t *file)
{
   return file->num_tracks;
}

// Start iterating over the events in a track.

midi_track_iter_t *MIDI_IterateTrack(midi_file_t *file, unsigned int track)
{
   midi_track_iter_t *iter;

   if(track >= file->num_tracks)
      I_Error("MIDI_IterateTrack: Track number greater than or equal to number of tracks\n");

   iter = estructalloc(midi_track_iter_t, 1);
   iter->track      = &file->tracks[track];
   iter->position   = 0;
   iter->loop_point = 0;

   return iter;
}

void MIDI_FreeIterator(midi_track_iter_t *iter)
{
   efree(iter);
}

// Get the time until the next MIDI event in a track.

unsigned int MIDI_GetDeltaTime(midi_track_iter_t *iter)
{
   if(int(iter->position) < iter->track->num_events)
   {
      midi_event_t *next_event;

      next_event = &iter->track->events[iter->position];

      return next_event->delta_time;
   }
   else
   {
      return 0;
   }
}

// Get a pointer to the next MIDI event.

int MIDI_GetNextEvent(midi_track_iter_t *iter, midi_event_t **event)
{
   if(int(iter->position) < iter->track->num_events)
   {
      *event = &iter->track->events[iter->position];
      ++iter->position;

      return 1;
   }
   else
   {
      return 0;
   }
}

unsigned int MIDI_GetFileTimeDivision(midi_file_t *file)
{
   short result = SwapBigUShort(file->header.time_division);

   // Negative time division indicates SMPTE time and must be handled
   // differently.
   if(result < 0)
      return int(-(result / 256)) * int(result & 0xFF);
   else
      return result;
}

void MIDI_RestartIterator(midi_track_iter_t *iter)
{
   iter->position   = 0;
   iter->loop_point = 0;
}

void MIDI_SetLoopPoint(midi_track_iter_t *iter)
{
   iter->loop_point = iter->position;
}

void MIDI_RestartAtLoopPoint(midi_track_iter_t *iter)
{
   iter->position = iter->loop_point;
}

// EOF

