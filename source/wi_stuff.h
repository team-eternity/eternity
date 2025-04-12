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
// Purpose: Intermission.
// Authors: James Haley
//

#ifndef WI_STUFF_H__
#define WI_STUFF_H__

#include "in_lude.h"

// States for the intermission

enum stateenum_t
{
   NoState = -1,
   StatCount,
   ShowNextLoc,
   IntrEnding, // haleyjd 03/16/06: fix for bug fraggle found
};

// haleyjd: DOOM intermission object
extern interfns_t DoomIntermission;

#endif

//----------------------------------------------------------------------------
//
// $Log: wi_stuff.h,v $
// Revision 1.3  1998/05/04  21:36:12  thldrmn
// commenting and reformatting
//
// Revision 1.2  1998/01/26  19:28:03  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:05  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
