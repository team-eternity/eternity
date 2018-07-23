// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2014 James Haley et al.
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Generalized sector special system
//
//-----------------------------------------------------------------------------

#ifndef EV_SECTORS_H__
#define EV_SECTORS_H__

struct sector_t;

typedef void (*EVSectorSpecialFunc)(sector_t *);

// Sector special binding
struct ev_sectorbinding_t
{
   int special;               // special number
   EVSectorSpecialFunc apply; // function which applies the special
};

bool EV_IsGenSectorSpecial(int special);

void EV_SpawnSectorSpecials();

#endif

// EOF

