//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
//
// Copyright (C) 2005-2014 Simon Howard
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
//
//------------------------------------------------------------------------------
//
// Purpose: MIDI file parsing.
// Authors: Simon Howard, James Haley
//

#ifndef MIDIFILE_H
#define MIDIFILE_H

using SDL_RWops = struct SDL_RWops;

struct midi_file_t;
struct midi_track_iter_t;

#define MIDI_CHANNELS_PER_TRACK 16

enum midi_event_type_e
{
    MIDI_EVENT_NOTE_OFF        = 0x80,
    MIDI_EVENT_NOTE_ON         = 0x90,
    MIDI_EVENT_AFTERTOUCH      = 0xA0,
    MIDI_EVENT_CONTROLLER      = 0xB0,
    MIDI_EVENT_PROGRAM_CHANGE  = 0xC0,
    MIDI_EVENT_CHAN_AFTERTOUCH = 0xD0,
    MIDI_EVENT_PITCH_BEND      = 0xE0,

    MIDI_EVENT_SYSEX       = 0xF0,
    MIDI_EVENT_SYSEX_SPLIT = 0xF7,
    MIDI_EVENT_META        = 0xFF,
};

enum midi_controller_e
{
    MIDI_CONTROLLER_BANK_SELECT_MSB = 0x00,
    MIDI_CONTROLLER_MODULATION      = 0x01,
    MIDI_CONTROLLER_BREATH_CONTROL  = 0x02,
    MIDI_CONTROLLER_FOOT_CONTROL    = 0x04,
    MIDI_CONTROLLER_PORTAMENTO      = 0x05,
    MIDI_CONTROLLER_DATA_ENTRY_MSB  = 0x06,
    MIDI_CONTROLLER_VOLUME_MSB      = 0x07,
    MIDI_CONTROLLER_PAN             = 0x0A,

    MIDI_CONTROLLER_BANK_SELECT_LSB = 0x20,
    MIDI_CONTROLLER_DATA_ENTRY_LSB  = 0x26,
    MIDI_CONTROLLER_VOLUME_LSB      = 0X27,

    MIDI_CONTROLLER_REVERB = 0x5B,
    MIDI_CONTROLLER_CHORUS = 0x5D,

    MIDI_CONTROLLER_RPN_LSB = 0x64,
    MIDI_CONTROLLER_RPN_MSB = 0x65,

    MIDI_CONTROLLER_ALL_SOUND_OFF   = 0x78,
    MIDI_CONTROLLER_RESET_ALL_CTRLS = 0x79,
    MIDI_CONTROLLER_ALL_NOTES_OFF   = 0x7B,
};

enum midi_meta_event_type_e
{
    MIDI_META_SEQUENCE_NUMBER = 0x00,

    MIDI_META_TEXT       = 0x01,
    MIDI_META_COPYRIGHT  = 0x02,
    MIDI_META_TRACK_NAME = 0x03,
    MIDI_META_INSTR_NAME = 0x04,
    MIDI_META_LYRICS     = 0x05,
    MIDI_META_MARKER     = 0x06,
    MIDI_META_CUE_POINT  = 0x07,

    MIDI_META_CHANNEL_PREFIX = 0x20,
    MIDI_META_END_OF_TRACK   = 0x2F,

    MIDI_META_SET_TEMPO          = 0x51,
    MIDI_META_SMPTE_OFFSET       = 0x54,
    MIDI_META_TIME_SIGNATURE     = 0x58,
    MIDI_META_KEY_SIGNATURE      = 0x59,
    MIDI_META_SEQUENCER_SPECIFIC = 0x7F,
};

#define EMIDI_LOOP_FLAG 0x7F

enum emidi_device_t
{
    EMIDI_DEVICE_GENERAL_MIDI  = 0x00,
    EMIDI_DEVICE_SOUND_CANVAS  = 0x01,
    EMIDI_DEVICE_AWE32         = 0x02,
    EMIDI_DEVICE_WAVE_BLASTER  = 0x03,
    EMIDI_DEVICE_SOUND_BLASTER = 0x04,
    EMIDI_DEVICE_PRO_AUDIO     = 0x05,
    EMIDI_DEVICE_SOUND_MAN_16  = 0x06,
    EMIDI_DEVICE_ADLIB         = 0x07,
    EMIDI_DEVICE_SOUNDSCAPE    = 0x08,
    EMIDI_DEVICE_ULTRASOUND    = 0x09,
    EMIDI_DEVICE_ALL           = 0x7F,
};

enum emidi_controller_e
{
    EMIDI_CONTROLLER_TRACK_DESIGNATION = 0x6E,
    EMIDI_CONTROLLER_TRACK_EXCLUSION   = 0x6F,
    EMIDI_CONTROLLER_PROGRAM_CHANGE    = 0x70,
    EMIDI_CONTROLLER_VOLUME            = 0x71,
    EMIDI_CONTROLLER_LOOP_BEGIN        = 0x74,
    EMIDI_CONTROLLER_LOOP_END          = 0x75,
    EMIDI_CONTROLLER_GLOBAL_LOOP_BEGIN = 0x76,
    EMIDI_CONTROLLER_GLOBAL_LOOP_END   = 0x77,
};

struct midi_meta_event_data_t
{
    // Meta event type:

    unsigned int type;

    // Length:

    unsigned int length;

    // Meta event data:

    byte *data;
};

struct midi_sysex_event_data_t
{
    // Length:

    unsigned int length;

    // Event data:

    byte *data;
};

struct midi_channel_event_data_t
{
    // The channel number to which this applies:

    unsigned int channel;

    // Extra parameters:

    unsigned int param1;
    unsigned int param2;
};

struct midi_event_t
{
    // Time between the previous event and this event.
    unsigned int delta_time;

    // Type of event:
    midi_event_type_e event_type;

    union
    {
        midi_channel_event_data_t channel;
        midi_meta_event_data_t    meta;
        midi_sysex_event_data_t   sysex;
    } data;
};

// Load a MIDI file.

midi_file_t *MIDI_LoadFile(SDL_RWops *stream);

// Free a MIDI file.

void MIDI_FreeFile(midi_file_t *file);

// Get the time division value from the MIDI header.

unsigned int MIDI_GetFileTimeDivision(midi_file_t *file);

// Get the number of tracks in a MIDI file.

unsigned int MIDI_NumTracks(midi_file_t *file);

// Start iterating over the events in a track.

midi_track_iter_t *MIDI_IterateTrack(midi_file_t *file, unsigned int track_num);

// Free an iterator.

void MIDI_FreeIterator(midi_track_iter_t *iter);

// Get the time until the next MIDI event in a track.

unsigned int MIDI_GetDeltaTime(midi_track_iter_t *iter);

// Get a pointer to the next MIDI event.

int MIDI_GetNextEvent(midi_track_iter_t *iter, midi_event_t **event);

// Reset an iterator to the beginning of a track.

void MIDI_RestartIterator(midi_track_iter_t *iter);

// Set loop point to current position.

void MIDI_SetLoopPoint(midi_track_iter_t *iter);

// Set position to saved loop point.

void MIDI_RestartAtLoopPoint(midi_track_iter_t *iter);

#endif /* #ifndef MIDIFILE_H */

