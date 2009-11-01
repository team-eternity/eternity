// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//
// Hexen-inspired action functions
//
// DISCLAIMER: None of this code was taken from Hexen. Any
// resemblence is purely coincidental or is the result of work from
// a common base source.
//
//-----------------------------------------------------------------------------

#ifndef P_XENEMY_H__
#define P_XENEMY_H__

// Earthquakes

typedef struct quakethinker_s
{
   degenmobj_t origin;   // serves as thinker and sound origin
   int intensity;        // richter scale (hardly realistic)
   int duration;         // how long it lasts
   fixed_t quakeRadius;  // radius of shaking effects
   fixed_t damageRadius; // radius of damage effects (if any)
} quakethinker_t;

void T_QuakeThinker(quakethinker_t *qt);
boolean P_StartQuake(int *args);

#endif

// EOF

