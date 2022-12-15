//
// Copyright(C) 2022 ceski
// Copyright(C) 2022 James Haley et al.
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
//----------------------------------------------------------------------------
//
// Purpose: MIDI instrument fallback support
// Authors: ceski
//


#include "doomtype.h"
#include "midifile.h"

enum midi_fallback_type_e
{
   FALLBACK_NONE,
   FALLBACK_BANK_MSB,
   FALLBACK_BANK_LSB,
   FALLBACK_DRUMS,
};

struct midi_fallback_t
{
   midi_fallback_type_t type;
   byte value;
};

void MIDI_CheckFallback(midi_event_t *event, midi_fallback_t *fallback);
void MIDI_ResetFallback();
void MIDI_InitFallback();
