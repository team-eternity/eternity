// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//
//    Routines for IWAD location, version identification, and loading.
//    Split out of d_main.cpp 08/12/12.
//
//-----------------------------------------------------------------------------

#ifndef D_IWAD_H__
#define D_IWAD_H__

// Needed for GameMode_t/GameMission_t
#include "d_gi.h" 

extern bool d_scaniwads;

extern bool freedoom;
extern bool bfgedition;

extern const char *const standard_iwads[];
extern int nstandard_iwads;

// IWAD check flags
enum IWADCheckFlags_e
{
   IWADF_NOERRORS     = 0,
   IWADF_FATALNOTOPEN = 0x00000001,
   IWADF_FATALNOTWAD  = 0x00000002
};

struct iwadcheck_t
{
   unsigned int   flags;
   bool           error;
   GameMode_t    *gmode;
   GameMission_t *gmission;
   bool          *hassec;
   bool          *freedoom;
   bool          *bfgedition;
};

void D_CheckIWAD(const char *iwadname, iwadcheck_t &version);

size_t  D_GetNumDoomWadPaths();
char   *D_GetDoomWadPath(size_t i);

void D_IdentifyVersion();
void D_MissionMetaData(const char *lump, int mission);
void D_DeferredMissionMetaData(const char *lump, int mission);
void D_DoDeferredMissionMetaData();

#endif

// EOF

