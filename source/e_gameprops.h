// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
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
//----------------------------------------------------------------------------
//
// EDF Gamemode Properties
//
// Allows editing of select properties from the GameModeInfo structure.
//
// James Haley
//
//----------------------------------------------------------------------------

#ifndef E_GAMEPROPS_H__
#define E_GAMEPROPS_H__

// EDF-Only Definitions
#ifdef NEED_EDF_DEFINITIONS

// Game properties section name
#define EDF_SEC_GAMEPROPS "gameproperties"

extern cfg_opt_t edf_game_opts[];

void E_ProcessGameProperties(cfg_t *cfg);

#endif

#endif

// EOF

