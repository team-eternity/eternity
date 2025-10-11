//-----------------------------------------------------------------------------
//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: MUS to MIDI.
// Authors: James Haley, Ioan Chera
//

#ifndef MMUS2MID_H__
#define MMUS2MID_H__

// error codes

enum error_code_t
{
    MUSDATACOR, // MUS data corrupt
    TOOMCHAN,   // Too many channels
    MEMALLOC,   // Memory allocation error
    MUSDATAMT,  // MUS file empty
    BADMUSCTL,  // MUS event 5 or 7 found
    BADSYSEVT,  // MUS system event not in 10-14 range
    BADCTLCHG,  // MUS control change larger than 9
    TRACKOVF,   // MIDI track exceeds allocation
    BADMIDHDR,  // bad midi header detected
};

// some names for integers of various sizes, all unsigned
using UBYTE = uint8_t;  // a one-byte int
using UWORD = uint16_t; // a two-byte int
#ifndef BASETYPES
// proff: changed from unsigned int to unsigned long to avoid warning
using ULONG = uint32_t; // a four-byte int (assumes int 4 bytes)
#endif

#ifndef DJGPP // proff: This is from allegro.h
static constexpr int MIDI_TRACKS = 32;

struct MIDI /* a midi file */
{
    int divisions; /* number of ticks per quarter note */
    struct
    {
        unsigned char *data; /* MIDI message stream */
        size_t         len;  /* length of the track data */
    } track[MIDI_TRACKS];
};
#endif // DJGPP

bool mmuscheckformat(UBYTE *mus, int size);
int  mmus2mid(UBYTE *mus, size_t size, MIDI *mid, UWORD division, int nocomp);
int  MIDIToMidi(const MIDI *mididata, UBYTE **mid, int *midlen);
int  MidiToMIDI(UBYTE *mid, MIDI *mididata);
void FreeMIDIData(MIDI *mididata);

#endif

//----------------------------------------------------------------------------
//
// $Log: mmus2mid.h,v $
// Revision 1.6  1998/05/10  23:00:48  jim
// formatted/documented mmus2mid
//
// Revision 1.5  1998/02/08  15:15:44  jim
// Added native midi support
//
// Revision 1.4  1998/01/26  19:27:17  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/21  16:56:22  jim
// Music fixed, defaults for cards added
//
// Revision 1.2  1998/01/19  23:40:33  rand
// Added Id: string at top of file
//
// Revision 1.1.1.1  1998/01/19  14:03:10  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
