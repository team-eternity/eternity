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
// Purpose: Created by a sound utility. Kept as a sample, DOOM2 sounds.
// Authors: James Haley
//

// killough 5/3/98: reformatted

#include "z_zone.h"
#include "sounds.h"
#include "p_skin.h"

//
// Information about all the music
//

musicinfo_t S_music[] =
{
   { nullptr },
   { "e1m1",   true },
   { "e1m2",   true },
   { "e1m3",   true },
   { "e1m4",   true },
   { "e1m5",   true },
   { "e1m6",   true },
   { "e1m7",   true },
   { "e1m8",   true },
   { "e1m9",   true },
   { "e2m1",   true },
   { "e2m2",   true },
   { "e2m3",   true },
   { "e2m4",   true },
   { "e2m5",   true },
   { "e2m6",   true },
   { "e2m7",   true },
   { "e2m8",   true },
   { "e2m9",   true },
   { "e3m1",   true },
   { "e3m2",   true },
   { "e3m3",   true },
   { "e3m4",   true },
   { "e3m5",   true },
   { "e3m6",   true },
   { "e3m7",   true },
   { "e3m8",   true },
   { "e3m9",   true },
   { "inter",  true },
   { "intro",  true },
   { "bunny",  true },
   { "victor", true },
   { "introa", true },
   { "runnin", true },
   { "stalks", true },
   { "countd", true },
   { "betwee", true },
   { "doom",   true },
   { "the_da", true },
   { "shawn",  true },
   { "ddtblu", true },
   { "in_cit", true },
   { "dead",   true },
   { "stlks2", true },
   { "theda2", true },
   { "doom2",  true },
   { "ddtbl2", true },
   { "runni2", true },
   { "dead2",  true },
   { "stlks3", true },
   { "romero", true },
   { "shawn2", true },
   { "messag", true },
   { "count2", true },
   { "ddtbl3", true },
   { "ampie",  true },
   { "theda3", true },
   { "adrian", true },
   { "messg2", true },
   { "romer2", true },
   { "tense",  true },
   { "shawn3", true },
   { "openin", true },
   { "evil",   true },
   { "ultima", true },
   { "read_m", true },
   { "dm2ttl", true },
   { "dm2int", true },
};

musicinfo_t H_music[] =
{
   { nullptr },
   { "e1m1", true }, // 1
   { "e1m2", true }, // 2
   { "e1m3", true }, // 3
   { "e1m4", true }, // 4
   { "e1m5", true }, // 5
   { "e1m6", true }, // 6
   { "e1m7", true }, // 7
   { "e1m8", true }, // 8
   { "e1m9", true }, // 9
   { "e2m1", true }, // 10
   { "e2m2", true }, // 11
   { "e2m3", true }, // 12
   { "e2m4", true }, // 13
   { "e2m6", true }, // 14
   { "e2m7", true }, // 15
   { "e2m8", true }, // 16
   { "e2m9", true }, // 17
   { "e3m2", true }, // 18
   { "e3m3", true }, // 19
   { "titl", true }, // 20
   { "intr", true }, // 21
   { "cptd", true }, // 22
};

// haleyjd: a much more clever way of getting an index to the music
// above than what was used in Heretic
int H_Mus_Matrix[6][9] =
{
   {  1,  2,  3,  4,  5,  6,  7,  8,  9 }, // episode 1
   { 10, 11, 12, 13,  4, 14, 15, 16, 17 }, // episode 2
   {  1, 18, 19,  6,  3,  2,  5,  9, 14 }, // episode 3
   {  6,  2,  3,  4,  5,  1,  7,  8,  9 }, // episode 4 (sosr)
   { 10, 11, 12, 13,  4, 14, 15, 16, 17 }, // episode 5 (sosr)
   { 18, 19,  6, 20, 21, 22,  1,  1,  1 }, // hidden levels
};

//
// Information about all the sfx
//
// killough 12/98: 
// Reimplemented 'singularity' flag, adjusting many sounds below

// haleyjd 11/05/03: made dynamic with EDF
int NUMSFX = 0;

//----------------------------------------------------------------------------
//
// $Log: sounds.c,v $
// Revision 1.3  1998/05/03  22:44:25  killough
// beautification
//
// Revision 1.2  1998/01/26  19:24:54  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
