// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2009 James Haley
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
   NUMPICKIWADS
};

int I_Pick_DoPicker(bool haveIWADs[], int startchoice);

#endif

// EOF