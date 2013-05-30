// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Generalized line action system - Macros
//
//-----------------------------------------------------------------------------

#ifndef EV_MACROS_H__
#define EV_MACROS_H__

//=============================================================================
//
// Macros
//

// Call to require a valid actor
#define REQUIRE_ACTOR(actor) \
   if(!(actor))              \
      return false

// Call to require a valid line
#define REQUIRE_LINE(line) \
   if(!(line))             \
      return false

#endif

// EOF

