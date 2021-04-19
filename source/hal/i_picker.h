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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Startup IWAD picker.
//
//-----------------------------------------------------------------------------

#ifndef I_PICKER_H__
#define I_PICKER_H__

// picker iwad enumeration
enum
{
   IWAD_DOOMSW,
   IWAD_DOOMREG,
   IWAD_DOOMU,
   IWAD_DOOM2,
   IWAD_BFGDOOM2,
   IWAD_TNT,
   IWAD_PLUT,
   IWAD_HACX,
   IWAD_HTICSW,
   IWAD_HTICREG,
   IWAD_HTICSOSR,
   IWAD_FREEDOOM,
   IWAD_FREEDOOMU,
   IWAD_FREEDM,
   IWAD_REKKR,
   NUMPICKIWADS
};

int I_Pick_DoPicker(bool haveIWADs[], int startchoice);

#endif

// EOF

