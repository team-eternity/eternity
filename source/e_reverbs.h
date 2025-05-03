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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: EDF Reverb Module
// Authors: James Haley
//

#ifndef E_REVERBS_H__
#define E_REVERBS_H__

#include "m_dllist.h"

// reverb flags
enum
{
    REVERB_ENABLED   = 0x00000001, // represents an actual effect
    REVERB_EQUALIZED = 0x00000002  // equalizer is enabled.
};

// EDF reverb data structure
struct ereverb_t
{
    DLListItem<ereverb_t> links; // hash by id
    int                   id;    // id number

    double       roomsize;
    double       dampening;
    double       wetscale;
    double       dryscale;
    double       width;
    int          predelay;
    double       eqlowfreq;
    double       eqhighfreq;
    double       eqlowgain;
    double       eqmidgain;
    double       eqhighgain;
    unsigned int flags;
};

ereverb_t *E_GetDefaultReverb();            // returns the built-in default reverb
ereverb_t *E_ReverbForID(int id1, int id2); // for two separate IDs
ereverb_t *E_ReverbForID(int id);           // for combined ID

#ifdef NEED_EDF_DEFINITIONS

// EDF section name and options array
static constexpr const char EDF_SEC_REVERB[] = "reverb";
extern cfg_opt_t            edf_reverb_opts[];

// Main processing function
void E_ProcessReverbs(cfg_t *cfg);

#endif

#endif

// EOF

