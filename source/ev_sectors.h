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
// Purpose: Generalized sector special system.
// Authors: James Haley
//

#ifndef EV_SECTORS_H__
#define EV_SECTORS_H__

struct sector_t;

using EVSectorSpecialFunc = void (*)(sector_t *);

// Sector special binding
struct ev_sectorbinding_t
{
   int special;               // special number
   EVSectorSpecialFunc apply; // function which applies the special
};

void EV_SpawnSectorSpecials();

#endif

// EOF

