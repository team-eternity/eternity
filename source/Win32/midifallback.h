//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
//
// Copyright (C) 2022 ceski
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
// Purpose: MIDI instrument fallback support.
// Authors: ceski
//

#ifndef MIDIFALLBACK_H
#define MIDIFALLBACK_H

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
   midi_fallback_type_e type;
   byte value;
};

void MIDI_CheckFallback(const midi_event_t *event, midi_fallback_t *fallback);
void MIDI_ResetFallback();
void MIDI_InitFallback();

#endif

// EOF

