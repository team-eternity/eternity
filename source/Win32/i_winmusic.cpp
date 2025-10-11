//
// Copyright (C) 2021-2022 Roman Fomin
// Copyright (C) 2022 ceski
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
//------------------------------------------------------------------------------
//
// Purpose: Windows native MIDI.
// Authors: Roman Fomin, ceski, Max Waine
//

#include "SDL.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "z_zone.h"

#include "c_runcmd.h"
#include "doomtype.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_winmusic.h"
#include "mmus2mid.h"
#include "m_qstr.h"
#include "midifile.h"
#include "midifallback.h"

const char *reset_type_names[] = {
    "Default", "None", "GS", "GM", "GM2", "XG",
};

int winmm_reset_type   = RESET_TYPE_DEFAULT;
int winmm_reset_delay  = 0;
int winmm_reverb_level = -1;
int winmm_chorus_level = -1;

qstring winmm_device;

VARIABLE_INT(winmm_reset_type, nullptr, RESET_TYPE_DEFAULT, RESET_TYPE_XG, reset_type_names);
VARIABLE_INT(winmm_reset_delay, nullptr, 0, 2000, nullptr);
VARIABLE_INT(winmm_reverb_level, nullptr, -1, 127, nullptr);
VARIABLE_INT(winmm_chorus_level, nullptr, -1, 127, nullptr);

CONSOLE_VARIABLE(winmm_reset_type, winmm_reset_type, 0) {}
CONSOLE_VARIABLE(winmm_reset_delay, winmm_reset_delay, 0) {}
CONSOLE_VARIABLE(winmm_reverb_level, winmm_reverb_level, 0) {}
CONSOLE_VARIABLE(winmm_chorus_level, winmm_chorus_level, 0) {}

static const byte gs_reset[] = { 0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7 };

static const byte gm_system_on[] = { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };

static const byte gm2_system_on[] = { 0xF0, 0x7E, 0x7F, 0x09, 0x03, 0xF7 };

static const byte xg_system_on[] = { 0xF0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7 };

static const byte ff_loopStart[] = { 'l', 'o', 'o', 'p', 'S', 't', 'a', 'r', 't' };
static const byte ff_loopEnd[]   = { 'l', 'o', 'o', 'p', 'E', 'n', 'd' };

static bool use_fallback;

#define DEFAULT_VOLUME 100
static int   channel_volume[MIDI_CHANNELS_PER_TRACK];
static float volume_factor = 0.0f;
static bool  update_volume = false;

static DWORD timediv;
static DWORD tempo;

static UINT      MidiDevice;
static HMIDISTRM hMidiStream;
static MIDIHDR   MidiStreamHdr;
static HANDLE    hBufferReturnEvent;
static HANDLE    hExitEvent;
static HANDLE    hPlayerThread;

// MS GS Wavetable Synth Device ID.
static int ms_gs_synth = MIDI_MAPPER;

// EMIDI device for track designation.
static int emidi_device;

static char **winmm_devices;
static int    winmm_devices_num;

// This is a reduced Windows MIDIEVENT structure for MEVT_F_SHORT
// type of events.

struct native_event_t
{
    DWORD dwDeltaTime;
    DWORD dwStreamID; // always 0
    DWORD dwEvent;
};

struct win_midi_track_t
{
    midi_track_iter_t *iter;
    unsigned int       elapsed_time;
    unsigned int       saved_elapsed_time;
    bool               end_of_track;
    bool               saved_end_of_track;
    unsigned int       emidi_device_flags;
    bool               emidi_designated;
    bool               emidi_program;
    bool               emidi_volume;
    int                emidi_loop_count;
};

struct win_midi_song_t
{
    midi_file_t      *file;
    win_midi_track_t *tracks;
    unsigned int      elapsed_time;
    unsigned int      saved_elapsed_time;
    unsigned int      num_tracks;
    bool              looping;
    bool              ff_loop;
    bool              ff_restart;
    bool              rpg_loop;
};

static win_midi_song_t song;

#define BUFFER_INITIAL_SIZE 1024

struct buffer_t
{
    byte        *data;
    unsigned int size;
    unsigned int position;
};

static buffer_t buffer;

// Maximum of 4 events in the buffer for faster volume updates.

#define STREAM_MAX_EVENTS 4

#define MAKE_EVT(a, b, c, d) ((DWORD)((a) | ((b) << 8) | ((c) << 16) | ((d) << 24)))

#define PADDED_SIZE(x) (((x) + sizeof(DWORD) - 1) & ~(sizeof(DWORD) - 1))

static bool initial_playback = false;

// Check for midiStream errors.

static void MidiError(const char *prefix, DWORD dwError)
{
    wchar_t  werror[MAXERRORLENGTH];
    MMRESULT mmr;

    mmr = midiOutGetErrorTextW(dwError, (LPWSTR)werror, MAXERRORLENGTH);
    if(mmr == MMSYSERR_NOERROR)
    {
        const int len   = WideCharToMultiByte(CP_UTF8, 0, werror, -1, NULL, 0, NULL, NULL);
        char     *error = ecalloc(char *, len, 1);
        WideCharToMultiByte(CP_UTF8, 0, werror, -1, error, len, nullptr, nullptr);

        fprintf(stderr, "%s: %s.\n", prefix, error);
        efree(error);
    }
    else
        fprintf(stderr, "%s: Unknown midiStream error.\n", prefix);
}

// midiStream callback.

static void CALLBACK MidiStreamProc(HMIDIOUT hMidi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1,
                                    DWORD_PTR dwParam2)
{
    if(uMsg == MOM_DONE)
        SetEvent(hBufferReturnEvent);
}

static void AllocateBuffer(const unsigned int size)
{
    MIDIHDR *hdr = &MidiStreamHdr;
    MMRESULT mmr;

    if(buffer.data)
    {
        mmr = midiOutUnprepareHeader((HMIDIOUT)hMidiStream, hdr, sizeof(MIDIHDR));
        if(mmr != MMSYSERR_NOERROR)
            MidiError("midiOutUnprepareHeader", mmr);
    }

    buffer.size = PADDED_SIZE(size);
    buffer.data = erealloc(byte *, buffer.data, buffer.size);

    hdr->lpData          = (LPSTR)buffer.data;
    hdr->dwBytesRecorded = 0;
    hdr->dwBufferLength  = buffer.size;
    mmr                  = midiOutPrepareHeader((HMIDIOUT)hMidiStream, hdr, sizeof(MIDIHDR));
    if(mmr != MMSYSERR_NOERROR)
        MidiError("midiOutPrepareHeader", mmr);
}

static void WriteBufferPad()
{
    unsigned int padding = PADDED_SIZE(buffer.position);
    memset(buffer.data + buffer.position, 0, padding - buffer.position);
    buffer.position = padding;
}

static void WriteBuffer(const byte *ptr, unsigned int size)
{
    if(buffer.position + size >= buffer.size)
        AllocateBuffer(size + buffer.size * 2);

    memcpy(buffer.data + buffer.position, ptr, size);
    buffer.position += size;
}

static void StreamOut()
{
    MIDIHDR *hdr = &MidiStreamHdr;
    MMRESULT mmr;

    hdr->lpData          = reinterpret_cast<LPSTR>(buffer.data);
    hdr->dwBytesRecorded = buffer.position;

    mmr = midiStreamOut(hMidiStream, hdr, sizeof(MIDIHDR));
    if(mmr != MMSYSERR_NOERROR)
        MidiError("midiStreamOut", mmr);
}

static void SendShortMsg(int time, int status, int channel, int param1, int param2)
{
    native_event_t native_event;
    native_event.dwDeltaTime = time;
    native_event.dwStreamID  = 0;
    native_event.dwEvent     = MAKE_EVT(status | channel, param1, param2, MEVT_SHORTMSG);
    WriteBuffer(reinterpret_cast<byte *>(&native_event), sizeof(native_event_t));
}

static void SendLongMsg(int time, const byte *ptr, int length)
{
    native_event_t native_event;
    native_event.dwDeltaTime = time;
    native_event.dwStreamID  = 0;
    native_event.dwEvent     = MAKE_EVT(length, 0, 0, MEVT_LONGMSG);
    WriteBuffer(reinterpret_cast<byte *>(&native_event), sizeof(native_event_t));
    WriteBuffer(ptr, length);
    WriteBufferPad();
}

static void SendNOPMsg(int time)
{
    native_event_t native_event;
    native_event.dwDeltaTime = time;
    native_event.dwStreamID  = 0;
    native_event.dwEvent     = MAKE_EVT(0, 0, 0, MEVT_NOP);
    WriteBuffer(reinterpret_cast<byte *>(&native_event), sizeof(native_event_t));
}

static void SendDelayMsg(int time_ms)
{
    // Convert ms to ticks (see "Standard MIDI Files 1.0" page 14).
    int time_ticks = int(float(time_ms) * 1000 * timediv / tempo + 0.5f);
    SendNOPMsg(time_ticks);
}

static void UpdateTempo(int time, midi_event_t *event)
{
    native_event_t native_event;

    tempo = MAKE_EVT(event->data.meta.data[2], event->data.meta.data[1], event->data.meta.data[0], 0);

    native_event.dwDeltaTime = time;
    native_event.dwStreamID  = 0;
    native_event.dwEvent     = MAKE_EVT(tempo, 0, 0, MEVT_TEMPO);
    WriteBuffer(reinterpret_cast<byte *>(&native_event), sizeof(native_event_t));
}

static void SendVolumeMsg(int time, int channel, int volume)
{
    int scaled_volume = int(volume * volume_factor + 0.5f);
    SendShortMsg(time, MIDI_EVENT_CONTROLLER, channel, MIDI_CONTROLLER_VOLUME_MSB, scaled_volume);
    channel_volume[channel] = volume;
}

static void UpdateVolume()
{
    for(int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
        SendVolumeMsg(0, i, channel_volume[i]);
}

static void ResetVolume()
{
    for(int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
        SendVolumeMsg(0, i, DEFAULT_VOLUME);
}

static void ResetReverb(int reset_type)
{
    int reverb = winmm_reverb_level;

    if(reverb == -1 && reset_type == RESET_TYPE_NONE)
    {
        // No reverb specified and no SysEx reset selected. Use GM default.
        reverb = 40;
    }

    if(reverb > -1)
    {
        for(int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_REVERB, reverb);
    }
}

static void ResetChorus(int reset_type)
{
    int chorus = winmm_chorus_level;

    if(chorus == -1 && reset_type == RESET_TYPE_NONE)
    {
        // No chorus specified and no SysEx reset selected. Use GM default.
        chorus = 0;
    }

    if(chorus > -1)
    {
        for(int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
            SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_CHORUS, chorus);
    }
}

static void ResetControllers()
{
    for(int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        // Reset commonly used controllers.
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RESET_ALL_CTRLS, 0);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_PAN, 64);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_BANK_SELECT_MSB, 0);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_BANK_SELECT_LSB, 0);
        SendShortMsg(0, MIDI_EVENT_PROGRAM_CHANGE, i, 0, 0);
    }
}

static void ResetPitchBendSensitivity()
{
    for(int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        // Set RPN MSB/LSB to pitch bend sensitivity.
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_LSB, 0);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_MSB, 0);

        // Reset pitch bend sensitivity to +/- 2 semitones and 0 cents.
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_DATA_ENTRY_MSB, 2);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_DATA_ENTRY_LSB, 0);

        // Set RPN MSB/LSB to null value after data entry.
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_LSB, 127);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RPN_MSB, 127);
    }
}

static void ResetDevice()
{
    int reset_type;

    for(int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
    {
        // Stop sound prior to reset to prevent volume spikes.
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_ALL_NOTES_OFF, 0);
        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_ALL_SOUND_OFF, 0);
    }

    if(MidiDevice == ms_gs_synth)
    {
        // MS GS Wavetable Synth lacks instrument fallback in GS mode which can
        // cause wrong or silent notes (MAYhem19.wad D_DM2TTL). It also responds
        // to XG System On when it should ignore it.
        switch(winmm_reset_type)
        {
        case RESET_TYPE_NONE: //
            reset_type = RESET_TYPE_NONE;
            break;

        case RESET_TYPE_GS: //
            reset_type = RESET_TYPE_GS;
            break;

        default: //
            reset_type = RESET_TYPE_GM;
            break;
        }
    }
    else // Unknown device
    {
        // Most devices support GS mode. Exceptions are some older hardware and
        // a few older VSTis. Some devices lack instrument fallback in GS mode.
        switch(winmm_reset_type)
        {
        case RESET_TYPE_NONE:
        case RESET_TYPE_GM:
        case RESET_TYPE_GM2:
        case RESET_TYPE_XG: //
            reset_type = winmm_reset_type;
            break;

        default: //
            reset_type = RESET_TYPE_GS;
            break;
        }
    }

    // Use instrument fallback in GS mode.
    MIDI_ResetFallback();
    use_fallback = (reset_type == RESET_TYPE_GS);

    // Assign EMIDI device for track designation.
    emidi_device = (reset_type == RESET_TYPE_GS);

    switch(reset_type)
    {
    case RESET_TYPE_NONE: ResetControllers(); break;

    case RESET_TYPE_GS: SendLongMsg(0, gs_reset, sizeof(gs_reset)); break;

    case RESET_TYPE_GM: SendLongMsg(0, gm_system_on, sizeof(gm_system_on)); break;

    case RESET_TYPE_GM2: SendLongMsg(0, gm2_system_on, sizeof(gm2_system_on)); break;

    case RESET_TYPE_XG: SendLongMsg(0, xg_system_on, sizeof(xg_system_on)); break;
    }

    if(reset_type == RESET_TYPE_NONE || MidiDevice == ms_gs_synth)
    {
        // MS GS Wavetable Synth doesn't reset pitch bend sensitivity, even
        // when sending a GM/GS reset, so do it manually.
        ResetPitchBendSensitivity();
    }

    ResetReverb(reset_type);
    ResetChorus(reset_type);

    // Reset volume (initial playback or on shutdown if no SysEx reset).
    if(initial_playback || reset_type == RESET_TYPE_NONE)
    {
        // Scale by slider on initial playback, max on shutdown.
        volume_factor = initial_playback ? volume_factor : 1.0f;
        ResetVolume();
    }

    // Send delay after reset. This is for hardware devices only (e.g. SC-55).
    if(winmm_reset_delay > 0)
        SendDelayMsg(winmm_reset_delay);
}

static bool IsPartLevel(const byte *msg, int length)
{
    if(length == 10 && msg[0] == 0x41 && // Roland
       msg[2] == 0x42 &&                 // GS
       msg[3] == 0x12 &&                 // DT1
       msg[4] == 0x40 &&                 // Address MSB
       msg[5] >= 0x10 &&                 // Address
       msg[5] <= 0x1F &&                 // Address
       msg[6] == 0x19 &&                 // Address LSB
       msg[9] == 0xF7)                   // SysEx EOX
    {
        const byte checksum = 128 - (int(msg[4]) + msg[5] + msg[6] + msg[7]) % 128;

        if(msg[8] == checksum)
        {
            // GS Part Level (aka Channel Volume)
            // 41 <dev> 42 12 40 <ch> 19 <vol> <sum> F7
            return true;
        }
    }

    return false;
}

static bool IsSysExReset(const byte *msg, int length)
{
    if(length < 5)
    {
        return false;
    }

    switch(msg[0])
    {
    case 0x41: // Roland
        switch(msg[2])
        {
        case 0x42: // GS
            switch(msg[3])
            {
            case 0x12:                               // DT1
                if(length == 10 && msg[4] == 0x00 && // Address MSB
                   msg[5] == 0x00 &&                 // Address
                   msg[6] == 0x7F &&                 // Address LSB
                   ((msg[7] == 0x00 &&               // Data     (MODE-1)
                     msg[8] == 0x01) ||              // Checksum (MODE-1)
                    (msg[7] == 0x01 &&               // Data     (MODE-2)
                     msg[8] == 0x00)))               // Checksum (MODE-2)
                {
                    // SC-88 System Mode Set
                    // 41 <dev> 42 12 00 00 7F 00 01 F7 (MODE-1)
                    // 41 <dev> 42 12 00 00 7F 01 00 F7 (MODE-2)
                    return true;
                }
                else if(length == 10 && msg[4] == 0x40 && // Address MSB
                        msg[5] == 0x00 &&                 // Address
                        msg[6] == 0x7F &&                 // Address LSB
                        msg[7] == 0x00 &&                 // Data (GS Reset)
                        msg[8] == 0x41)                   // Checksum
                {
                    // GS Reset
                    // 41 <dev> 42 12 40 00 7F 00 41 F7
                    return true;
                }
                break;
            }
            break;
        }
        break;

    case 0x43: // Yamaha
        switch(msg[2])
        {
        case 0x2B:                              // TG300
            if(length == 9 && msg[3] == 0x00 && // Start Address b20 - b14
               msg[4] == 0x00 &&                // Start Address b13 - b7
               msg[5] == 0x7F &&                // Start Address b6 - b0
               msg[6] == 0x00 &&                // Data
               msg[7] == 0x01)                  // Checksum
            {
                // TG300 All Parameter Reset
                // 43 <dev> 2B 00 00 7F 00 01 F7
                return true;
            }
            break;

        case 0x4C:                              // XG
            if(length == 8 && msg[3] == 0x00 && // Address High
               msg[4] == 0x00 &&                // Address Mid
               (msg[5] == 0x7E ||               // Address Low (System On)
                msg[5] == 0x7F) &&              // Address Low (All Parameter Reset)
               msg[6] == 0x00)                  // Data
            {
                // XG System On, XG All Parameter Reset
                // 43 <dev> 4C 00 00 7E 00 F7
                // 43 <dev> 4C 00 00 7F 00 F7
                return true;
            }
            break;
        }
        break;

    case 0x7E: // Universal Non-Real Time
        switch(msg[2])
        {
        case 0x09:                               // General Midi
            if(length == 5 && (msg[3] == 0x01 || // GM System On
                               msg[3] == 0x02 || // GM System Off
                               msg[3] == 0x03))  // GM2 System On
            {
                // GM System On/Off, GM2 System On
                // 7E <dev> 09 01 F7
                // 7E <dev> 09 02 F7
                // 7E <dev> 09 03 F7
                return true;
            }
            break;
        }
        break;
    }
    return false;
}

static void SendSysExMsg(int time, const byte *data, int length)
{
    native_event_t native_event;
    bool           is_sysex_reset;
    const byte     event_type = MIDI_EVENT_SYSEX;

    is_sysex_reset = IsSysExReset(data, length);

    if(is_sysex_reset && MidiDevice == ms_gs_synth)
    {
        // Ignore SysEx reset from MIDI file for MS GS Wavetable Synth.
        SendNOPMsg(time);
        return;
    }

    // Send the SysEx message.
    native_event.dwDeltaTime = time;
    native_event.dwStreamID  = 0;
    native_event.dwEvent     = MAKE_EVT(length + sizeof(byte), 0, 0, MEVT_LONGMSG);
    WriteBuffer(reinterpret_cast<byte *>(&native_event), sizeof(native_event_t));
    WriteBuffer(&event_type, sizeof(byte));
    WriteBuffer(data, length);
    WriteBufferPad();

    if(is_sysex_reset)
    {
        // SysEx reset also resets volume. Take the default channel volumes
        // and scale them by the user's volume slider.
        ResetVolume();

        // Disable instrument fallback and give priority to MIDI file. Fallback
        // assumes GS (SC-55 level) and the MIDI file could be GM, GM2, XG, or
        // GS (SC-88 or higher). Preserve the composer's intent.
        MIDI_ResetFallback();
        use_fallback = false;

        // Use default device for EMIDI.
        emidi_device = EMIDI_DEVICE_GENERAL_MIDI;
    }
}

static void SendProgramMsg(int time, int channel, int program, midi_fallback_t *fallback)
{
    switch((int)fallback->type)
    {
    case FALLBACK_BANK_MSB:
        SendShortMsg(time, MIDI_EVENT_CONTROLLER, channel, MIDI_CONTROLLER_BANK_SELECT_MSB, fallback->value);
        SendShortMsg(0, MIDI_EVENT_PROGRAM_CHANGE, channel, program, 0);
        break;

    case FALLBACK_DRUMS: //
        SendShortMsg(time, MIDI_EVENT_PROGRAM_CHANGE, channel, fallback->value, 0);
        break;

    default: //
        SendShortMsg(time, MIDI_EVENT_PROGRAM_CHANGE, channel, program, 0);
        break;
    }
}

static void SetLoopPoint()
{
    for(unsigned int i = 0; i < song.num_tracks; ++i)
    {
        MIDI_SetLoopPoint(song.tracks[i].iter);
        song.tracks[i].saved_end_of_track = song.tracks[i].end_of_track;
        song.tracks[i].saved_elapsed_time = song.tracks[i].elapsed_time;
    }
    song.saved_elapsed_time = song.elapsed_time;
}

static void CheckFFLoop(midi_event_t *event)
{
    if(event->data.meta.length == sizeof(ff_loopStart) &&
       !memcmp(event->data.meta.data, ff_loopStart, sizeof(ff_loopStart)))
    {
        SetLoopPoint();
        song.ff_loop = true;
    }
    else if(song.ff_loop && event->data.meta.length == sizeof(ff_loopEnd) &&
            !memcmp(event->data.meta.data, ff_loopEnd, sizeof(ff_loopEnd)))
    {
        song.ff_restart = true;
    }
}

static bool AddToBuffer(unsigned int delta_time, midi_event_t *event, win_midi_track_t *track)
{
    unsigned int    flag;
    int             count;
    midi_fallback_t fallback = { FALLBACK_NONE, 0 };

    if(use_fallback)
        MIDI_CheckFallback(event, &fallback);

    switch((int)event->event_type)
    {
    case MIDI_EVENT_SYSEX:
        if(IsPartLevel(event->data.sysex.data, event->data.sysex.length))
        {
            const byte *data = event->data.sysex.data;
            byte        channel;

            // Convert "block number" to a channel number.
            if(data[5] == 0x10) // Channel 10
                channel = 9;
            else if(data[5] < 0x1A) // Channels 1-9
                channel = (data[5] & 0x0F) - 1;
            else // Channels 11-16
                channel = data[5] & 0x0F;

            // Replace SysEx part level message with channel volume message.
            SendVolumeMsg(delta_time, channel, data[7]);
        }
        else
            SendSysExMsg(delta_time, event->data.sysex.data, event->data.sysex.length);
        return false;

    case MIDI_EVENT_META:
        switch(event->data.meta.type)
        {
        case MIDI_META_END_OF_TRACK: //
            track->end_of_track = true;
            SendNOPMsg(delta_time);
            break;

        case MIDI_META_SET_TEMPO: //
            UpdateTempo(delta_time, event);
            break;

        case MIDI_META_MARKER: //
            CheckFFLoop(event);
            SendNOPMsg(delta_time);
            break;

        default: //
            SendNOPMsg(delta_time);
            break;
        }
        return true;
    }

    if(track->emidi_designated && (emidi_device & ~track->emidi_device_flags))
    {
        // Send NOP if this device has been excluded from this track.
        SendNOPMsg(delta_time);
        return true;
    }

    switch((int)event->event_type)
    {
    case MIDI_EVENT_CONTROLLER:
        switch(event->data.channel.param1)
        {
        case MIDI_CONTROLLER_VOLUME_MSB: //
            if(track->emidi_volume)
                SendNOPMsg(delta_time);
            else
                SendVolumeMsg(delta_time, event->data.channel.channel, event->data.channel.param2);
            break;

        case MIDI_CONTROLLER_VOLUME_LSB: //
            SendNOPMsg(delta_time);
            break;

        case MIDI_CONTROLLER_BANK_SELECT_LSB: //
            if(fallback.type == FALLBACK_BANK_LSB)
            {
                SendShortMsg(delta_time, MIDI_EVENT_CONTROLLER, event->data.channel.channel,
                             MIDI_CONTROLLER_BANK_SELECT_LSB, fallback.value);
            }
            else
            {
                SendShortMsg(delta_time, MIDI_EVENT_CONTROLLER, event->data.channel.channel,
                             MIDI_CONTROLLER_BANK_SELECT_LSB, event->data.channel.param2);
            }
            break;

        case EMIDI_CONTROLLER_TRACK_DESIGNATION: //
            if(track->elapsed_time < timediv)
            {
                flag = event->data.channel.param2;

                if(flag == EMIDI_DEVICE_ALL)
                {
                    track->emidi_device_flags = UINT_MAX;
                    track->emidi_designated   = true;
                }
                else if(flag <= EMIDI_DEVICE_ULTRASOUND)
                {
                    track->emidi_device_flags |= 1 << flag;
                    track->emidi_designated    = true;
                }
            }
            SendNOPMsg(delta_time);
            break;

        case EMIDI_CONTROLLER_TRACK_EXCLUSION: //
            if(song.rpg_loop)
                SetLoopPoint();
            else if(track->elapsed_time < timediv)
            {
                flag = event->data.channel.param2;

                if(!track->emidi_designated)
                {
                    track->emidi_device_flags = UINT_MAX;
                    track->emidi_designated   = true;
                }

                if(flag <= EMIDI_DEVICE_ULTRASOUND)
                    track->emidi_device_flags &= ~(1 << flag);
            }
            SendNOPMsg(delta_time);
            break;

        case EMIDI_CONTROLLER_PROGRAM_CHANGE: //
            if(track->emidi_program || track->elapsed_time < timediv)
            {
                track->emidi_program = true;
                SendProgramMsg(delta_time, event->data.channel.channel, event->data.channel.param2, &fallback);
            }
            else
                SendNOPMsg(delta_time);
            break;

        case EMIDI_CONTROLLER_VOLUME: //
            if(track->emidi_volume || track->elapsed_time < timediv)
            {
                track->emidi_volume = true;
                SendVolumeMsg(delta_time, event->data.channel.channel, event->data.channel.param2);
            }
            else
                SendNOPMsg(delta_time);
            break;

        case EMIDI_CONTROLLER_LOOP_BEGIN: //
            count                   = event->data.channel.param2;
            count                   = (count == 0) ? (-1) : count;
            track->emidi_loop_count = count;
            MIDI_SetLoopPoint(track->iter);
            SendNOPMsg(delta_time);
            break;

        case EMIDI_CONTROLLER_LOOP_END: //
            if(event->data.channel.param2 == EMIDI_LOOP_FLAG)
            {
                if(track->emidi_loop_count != 0)
                    MIDI_RestartAtLoopPoint(track->iter);

                if(track->emidi_loop_count > 0)
                    track->emidi_loop_count--;
            }
            SendNOPMsg(delta_time);
            break;

        case EMIDI_CONTROLLER_GLOBAL_LOOP_BEGIN: //
            count = event->data.channel.param2;
            count = (count == 0) ? (-1) : count;
            for(unsigned int i = 0; i < song.num_tracks; ++i)
            {
                song.tracks[i].emidi_loop_count = count;
                MIDI_SetLoopPoint(song.tracks[i].iter);
            }
            SendNOPMsg(delta_time);
            break;

        case EMIDI_CONTROLLER_GLOBAL_LOOP_END: //
            if(event->data.channel.param2 == EMIDI_LOOP_FLAG)
            {
                for(unsigned int i = 0; i < song.num_tracks; ++i)
                {
                    if(song.tracks[i].emidi_loop_count != 0)
                        MIDI_RestartAtLoopPoint(song.tracks[i].iter);

                    if(song.tracks[i].emidi_loop_count > 0)
                        song.tracks[i].emidi_loop_count--;
                }
            }
            SendNOPMsg(delta_time);
            break;

        case MIDI_CONTROLLER_RESET_ALL_CTRLS: //
            SendShortMsg(delta_time, MIDI_EVENT_CONTROLLER, event->data.channel.channel,
                         MIDI_CONTROLLER_RESET_ALL_CTRLS, 0);

            // MS GS Wavetable Synth resets the channel volume after a
            // "reset all controllers" event, so reapply the volume
            if(MidiDevice == ms_gs_synth)
            {
                const unsigned int channel = event->data.channel.channel;
                SendVolumeMsg(0, channel, channel_volume[channel]);
            }
            break;

        default:
            SendShortMsg(delta_time, MIDI_EVENT_CONTROLLER, event->data.channel.channel, event->data.channel.param1,
                         event->data.channel.param2);
            break;
        }
        break;

    case MIDI_EVENT_NOTE_OFF:
    case MIDI_EVENT_NOTE_ON:
    case MIDI_EVENT_AFTERTOUCH:
    case MIDI_EVENT_PITCH_BEND:
        SendShortMsg(delta_time, event->event_type, //
                     event->data.channel.channel,   //
                     event->data.channel.param1,    //
                     event->data.channel.param2);
        break;

    case MIDI_EVENT_PROGRAM_CHANGE:
        if(track->emidi_program)
            SendNOPMsg(delta_time);
        else
            SendProgramMsg(delta_time, event->data.channel.channel, event->data.channel.param1, &fallback);
        break;

    case MIDI_EVENT_CHAN_AFTERTOUCH:
        SendShortMsg(delta_time, MIDI_EVENT_CHAN_AFTERTOUCH, //
                     event->data.channel.channel,            //
                     event->data.channel.param1, 0);
        break;

    default: //
        SendNOPMsg(delta_time);
        break;
    }

    return true;
}

static void RestartLoop()
{
    for(unsigned int i = 0; i < song.num_tracks; ++i)
    {
        MIDI_RestartAtLoopPoint(song.tracks[i].iter);
        song.tracks[i].end_of_track = song.tracks[i].saved_end_of_track;
        song.tracks[i].elapsed_time = song.tracks[i].saved_elapsed_time;
    }
    song.elapsed_time = song.saved_elapsed_time;
}

static void RestartTracks()
{
    for(unsigned int i = 0; i < song.num_tracks; ++i)
    {
        MIDI_RestartIterator(song.tracks[i].iter);
        song.tracks[i].elapsed_time       = 0;
        song.tracks[i].end_of_track       = false;
        song.tracks[i].emidi_device_flags = 0;
        song.tracks[i].emidi_designated   = false;
        song.tracks[i].emidi_program      = false;
        song.tracks[i].emidi_volume       = false;
        song.tracks[i].emidi_loop_count   = 0;
    }
    song.elapsed_time = 0;
}

static bool IsRPGLoop()
{
    unsigned int  num_rpg_events   = 0;
    unsigned int  num_emidi_events = 0;
    midi_event_t *event            = nullptr;

    for(unsigned int i = 0; i < song.num_tracks; ++i)
    {
        while(MIDI_GetNextEvent(song.tracks[i].iter, &event))
        {
            if(event->event_type == MIDI_EVENT_CONTROLLER)
            {
                switch(event->data.channel.param1)
                {
                case EMIDI_CONTROLLER_TRACK_EXCLUSION: //
                    num_rpg_events++;
                    break;

                case EMIDI_CONTROLLER_TRACK_DESIGNATION:
                case EMIDI_CONTROLLER_PROGRAM_CHANGE:
                case EMIDI_CONTROLLER_VOLUME:
                case EMIDI_CONTROLLER_LOOP_BEGIN:
                case EMIDI_CONTROLLER_LOOP_END:
                case EMIDI_CONTROLLER_GLOBAL_LOOP_BEGIN:
                case EMIDI_CONTROLLER_GLOBAL_LOOP_END: //
                    num_emidi_events++;
                    break;
                }
            }
        }

        MIDI_RestartIterator(song.tracks[i].iter);
    }

    return (num_rpg_events == 1 && num_emidi_events == 0);
}

static void FillBuffer()
{
    int num_events;

    buffer.position = 0;

    if(initial_playback)
    {
        ResetDevice();
        StreamOut();
        song.rpg_loop    = IsRPGLoop();
        initial_playback = false;
        return;
    }

    if(update_volume)
    {
        update_volume = false;
        UpdateVolume();
        StreamOut();
        return;
    }

    for(num_events = 0; num_events < STREAM_MAX_EVENTS;)
    {
        midi_event_t     *event    = nullptr;
        win_midi_track_t *track    = nullptr;
        unsigned int      min_time = UINT_MAX;
        unsigned int      delta_time;

        // Find next event across all tracks.
        for(unsigned int i = 0; i < song.num_tracks; ++i)
        {
            if(!song.tracks[i].end_of_track)
            {
                unsigned int time = song.tracks[i].elapsed_time + MIDI_GetDeltaTime(song.tracks[i].iter);
                if(time < min_time)
                {
                    min_time = time;
                    track    = &song.tracks[i];
                }
            }
        }

        // No more events. Restart or stop song.
        if(track == nullptr)
        {
            if(song.elapsed_time)
            {
                if(song.ff_restart || song.rpg_loop)
                {
                    song.ff_restart = false;
                    RestartLoop();
                    continue;
                }
                else if(song.looping)
                {
                    for(unsigned int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
                        SendShortMsg(0, MIDI_EVENT_CONTROLLER, i, MIDI_CONTROLLER_RESET_ALL_CTRLS, 0);

                    // MS GS Wavetable Synth resets the channel volume after a
                    // "reset all controllers" event, so reapply the volume
                    if(MidiDevice == ms_gs_synth)
                    {
                        for(unsigned int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i)
                            SendVolumeMsg(0, i, channel_volume[i]);
                    }

                    RestartTracks();
                    continue;
                }
            }
            break;
        }

        track->elapsed_time = min_time;
        delta_time          = min_time - song.elapsed_time;
        song.elapsed_time   = min_time;

        if(!MIDI_GetNextEvent(track->iter, &event))
        {
            track->end_of_track = true;
            continue;
        }

        // Restart FF loop after sending all events that share same timediv.
        if(song.ff_restart && MIDI_GetDeltaTime(track->iter) > 0)
        {
            song.ff_restart = false;
            RestartLoop();
            continue;
        }

        if(!AddToBuffer(delta_time, event, track))
        {
            StreamOut();
            return;
        }

        num_events++;
    }

    if(num_events)
        StreamOut();
}

// The Windows API documentation states: "Applications should not call any
// multimedia functions from inside the callback function, as doing so can
// cause a deadlock." We use thread to avoid possible deadlocks.

static DWORD WINAPI PlayerProc()
{
    HANDLE events[2] = { hBufferReturnEvent, hExitEvent };

    while(1)
    {
        switch(WaitForMultipleObjects(2, events, FALSE, INFINITE))
        {
        case WAIT_OBJECT_0: //
            FillBuffer();
            break;

        case WAIT_OBJECT_0 + 1: //
            return 0;
        }
    }
    return 0;
}

static void GetDevices()
{
    constexpr const char pname[] = "Microsoft GS Wavetable";

    if(winmm_devices_num)
        return;

    winmm_devices_num = midiOutGetNumDevs();

    winmm_devices = emalloc(char **, winmm_devices_num * sizeof(*winmm_devices));

    for(int i = 0; i < winmm_devices_num; ++i)
    {
        MIDIOUTCAPS caps;
        MMRESULT    mmr;

        mmr = midiOutGetDevCaps(i, &caps, sizeof(caps));
        if(mmr == MMSYSERR_NOERROR)
        {
            winmm_devices[i] = estrdup(caps.szPname);

            // is this device MS GS Synth?
            if(!strncasecmp(pname, caps.szPname, sizeof(pname) - 1))
                ms_gs_synth = i;
        }
    }
}

bool I_WIN_InitMusic(int device)
{
    MMRESULT mmr;

    MidiDevice = MIDI_MAPPER;

    if(device == DEFAULT_MIDI_DEVICE)
    {
        int i;

        GetDevices();

        for(i = 0; i < winmm_devices_num; ++i)
        {
            if(winmm_device == winmm_devices[i])
            {
                device = i;
                break;
            }
        }
        if(i == winmm_devices_num)
            device = 0;
    }

    if(winmm_devices_num)
    {
        if(device >= winmm_devices_num)
            device = 0;
        winmm_device = winmm_devices[device];
        MidiDevice   = device;
    }

    mmr = midiStreamOpen(&hMidiStream, &MidiDevice, DWORD(1), reinterpret_cast<DWORD_PTR>(MidiStreamProc),
                         reinterpret_cast<DWORD_PTR>(nullptr), CALLBACK_FUNCTION);
    if(mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamOpen", mmr);
        return false;
    }

    AllocateBuffer(BUFFER_INITIAL_SIZE);

    hBufferReturnEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    hExitEvent         = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    MIDI_InitFallback();

    printf("Windows MIDI Init: Using '%s'.\n", winmm_device.constPtr());

    return true;
}

void I_WIN_SetMusicVolume(int volume)
{
    static int last_volume = -1;

    if(last_volume == volume)
    {
        // Ignore holding key down in volume menu.
        return;
    }

    last_volume = volume;

    volume_factor = sqrtf(float(volume) / 15);

    update_volume = (song.file != nullptr);
}

void I_WIN_StopSong()
{
    if(!hPlayerThread)
    {
        return;
    }

    SetEvent(hExitEvent);
    WaitForSingleObject(hPlayerThread, INFINITE);
    CloseHandle(hPlayerThread);
    hPlayerThread = nullptr;

    if(const MMRESULT mmr = midiStreamStop(hMidiStream); mmr != MMSYSERR_NOERROR)
        MidiError("midiStreamStop", mmr);
}

void I_WIN_PlaySong(bool looping)
{
    song.looping = looping;

    hPlayerThread = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(PlayerProc), 0, 0, 0);
    SetThreadPriority(hPlayerThread, THREAD_PRIORITY_TIME_CRITICAL);

    initial_playback = true;

    SetEvent(hBufferReturnEvent);

    if(const MMRESULT mmr = midiStreamRestart(hMidiStream); mmr != MMSYSERR_NOERROR)
        MidiError("midiStreamRestart", mmr);
}

void I_WIN_PauseSong()
{
    if(const MMRESULT mmr = midiStreamPause(hMidiStream); mmr != MMSYSERR_NOERROR)
        MidiError("midiStreamPause", mmr);
}

void I_WIN_ResumeSong()
{
    if(const MMRESULT mmr = midiStreamRestart(hMidiStream); mmr != MMSYSERR_NOERROR)
        MidiError("midiStreamRestart", mmr);
}

bool I_WIN_RegisterSong(void *data, int len)
{
    MIDIPROPTIMEDIV prop_timediv;
    MIDIPROPTEMPO   prop_tempo;
    MMRESULT        mmr;

    // We verified it's a MIDI (or converted to it from MUS) before passing in.
    SDL_RWops *rw = SDL_RWFromMem(data, len);
    song.file     = MIDI_LoadFile(rw);

    if(song.file == nullptr)
    {
        fprintf(stderr, "I_WIN_RegisterSong: Failed to load MID.\n");
        return false;
    }

    prop_timediv.cbStruct  = sizeof(MIDIPROPTIMEDIV);
    prop_timediv.dwTimeDiv = MIDI_GetFileTimeDivision(song.file);
    mmr = midiStreamProperty(hMidiStream, reinterpret_cast<LPBYTE>(&prop_timediv), MIDIPROP_SET | MIDIPROP_TIMEDIV);
    if(mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamProperty", mmr);
        return false;
    }
    timediv = prop_timediv.dwTimeDiv;

    // Set initial tempo.
    prop_tempo.cbStruct = sizeof(MIDIPROPTIMEDIV);
    prop_tempo.dwTempo  = 500000; // 120 BPM
    mmr = midiStreamProperty(hMidiStream, reinterpret_cast<LPBYTE>(&prop_tempo), MIDIPROP_SET | MIDIPROP_TEMPO);
    if(mmr != MMSYSERR_NOERROR)
    {
        MidiError("midiStreamProperty", mmr);
        return false;
    }
    tempo = prop_tempo.dwTempo;

    song.num_tracks = MIDI_NumTracks(song.file);
    song.tracks     = estructalloc(win_midi_track_t, song.num_tracks);
    for(unsigned int i = 0; i < song.num_tracks; ++i)
        song.tracks[i].iter = MIDI_IterateTrack(song.file, i);

    ResetEvent(hBufferReturnEvent);
    ResetEvent(hExitEvent);

    return true;
}

void I_WIN_UnRegisterSong()
{
    if(song.tracks)
    {
        for(unsigned int i = 0; i < song.num_tracks; ++i)
        {
            MIDI_FreeIterator(song.tracks[i].iter);
            song.tracks[i].iter = nullptr;
        }
        efree(song.tracks);
        song.tracks = nullptr;
    }
    if(song.file)
    {
        MIDI_FreeFile(song.file);
        song.file = nullptr;
    }
    song.elapsed_time       = 0;
    song.saved_elapsed_time = 0;
    song.num_tracks         = 0;
    song.looping            = false;
    song.ff_loop            = false;
    song.ff_restart         = false;
    song.rpg_loop           = false;
}

void I_WIN_ShutdownMusic()
{
    MMRESULT mmr;

    if(!hMidiStream)
        return;

    I_WIN_StopSong();
    I_WIN_UnRegisterSong();

    // Reset device at shutdown.
    buffer.position = 0;
    ResetDevice();
    StreamOut();
    mmr = midiStreamRestart(hMidiStream);
    if(mmr != MMSYSERR_NOERROR)
        MidiError("midiStreamRestart", mmr);
    WaitForSingleObject(hBufferReturnEvent, INFINITE);
    mmr = midiStreamStop(hMidiStream);
    if(mmr != MMSYSERR_NOERROR)
        MidiError("midiStreamStop", mmr);

    if(buffer.data)
    {
        MidiStreamHdr.dwFlags &= ~MHDR_INQUEUE;
        mmr                    = midiOutUnprepareHeader((HMIDIOUT)hMidiStream, &MidiStreamHdr, sizeof(MIDIHDR));
        if(mmr != MMSYSERR_NOERROR)
            MidiError("midiOutUnprepareHeader", mmr);
        efree(buffer.data);
        buffer.data     = nullptr;
        buffer.size     = 0;
        buffer.position = 0;
    }

    mmr = midiStreamClose(hMidiStream);
    if(mmr != MMSYSERR_NOERROR)
        MidiError("midiStreamClose", mmr);
    hMidiStream = nullptr;

    CloseHandle(hBufferReturnEvent);
    CloseHandle(hExitEvent);
}

static int I_WIN_DeviceList(const char *devices[], int size, int *current_device)
{
    int i;

    *current_device = 0;

    GetDevices();

    for(i = 0; i < winmm_devices_num && i < size; ++i)
    {
        devices[i] = winmm_devices[i];
        if(winmm_device == winmm_devices[i])
            *current_device = i;
    }

    return i;
}

#endif // _WIN32

// EOF

