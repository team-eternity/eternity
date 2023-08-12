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

struct ereverb_t;

void S_SuspendReverb();
void S_ResumeReverb();
void S_ReverbSetState(ereverb_t *ereverb);
void S_ProcessReverb(float *stream, const int samples, const int skip);
void S_ProcessReverbReplace(float *stream, int samples);

#endif

// EOF

