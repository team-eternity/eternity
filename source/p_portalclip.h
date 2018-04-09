// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2016 Ioan Chera et al.
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Linked portal clipping. Mostly called from functions like
//      when map has portals PIT_CheckPosition3D
//
//-----------------------------------------------------------------------------

#ifndef P_PORTALCLIP_H_
#define P_PORTALCLIP_H_

struct line_t;
struct polyobj_s;

//
// Data output by P_LineOpening, portal aware
//
enum
{
   LINECLIP_OVER3DMIDTEX = 1,    // clip thing is over this 3DMidTex
   LINECLIP_UNDER3DMIDTEX = 2,   // clip thing is under this 3DMidTex

   // basic flags
   LINECLIP_ABOVEPORTAL = 4,     // floor portal is below line, both sides
   LINECLIP_UNDERPORTAL = 8,     // ceiling portal is above line, both sides
};

bool PIT_CheckLine3D(line_t *ld, polyobj_s *po, void *context);

#endif

// EOF

