// Emacs style mode select   -*- C++ -*-
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//
//  Freeverb algorithm implementation
//  Based on original public domain implementation by Jezar at Dreampoint
//
//-----------------------------------------------------------------------------

#ifndef S_REVERB_H__
#define S_REVERB_H__

// reverb parameters
enum s_reverbparam_e
{
   S_REVERB_MODE,
   S_REVERB_ROOMSIZE,
   S_REVERB_DAMP,
   S_REVERB_WET,
   S_REVERB_DRY,
   S_REVERB_WIDTH,
   S_REVERB_PREDELAY
};

// equalization types
enum s_reverbeq_e
{
   S_REVERB_LPF,
   S_REVERB_HPF,
   S_REVERB_BPF
};

void S_SuspendReverb();
void S_ResumeReverb();
void S_ReverbSetParam(s_reverbparam_e param, double value);
void S_ProcessReverb(double *stream, int samples);
void S_ProcessReverbReplace(double *stream, int samples);

#endif

// EOF

