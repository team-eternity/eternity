// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
//
// EDF Reverb Module
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_REVERBS_H__
#define E_REVERBS_H__

#include "m_dllist.h"

// EDF reverb data structure
struct ereverb_t
{
   DLListItem<ereverb_t> links; // hash by id
   int id;                      // id number

   double roomsize;
   double dampening;
   double wetscale;
   double dryscale;
   double width;
   int    predelay;
   bool   equalized;
   double eqlowfreq;
   double eqhighfreq;
   double eqlowgain;
   double eqmidgain;
   double eqhighgain;
};

ereverb_t *E_ReverbForID(int id1, int id2);

#ifdef NEED_EDF_DEFINITIONS

// EDF section name and options array
#define EDF_SEC_REVERB "reverb"
extern cfg_opt_t edf_reverb_opts[];

// Main processing function
void E_ProcessReverbs(cfg_t *cfg);

#endif

#endif

// EOF

