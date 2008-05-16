// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2008 James Haley
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Dynamic segs for PolyObject re-implementation.
//
//-----------------------------------------------------------------------------

#ifndef R_DYNSEG_H__
#define R_DYNSEG_H__

#ifdef R_DYNASEGS

//
// dynaseg
//
typedef struct dynaseg_s
{
   seg_t seg; // a dynaseg is a seg, after all ;)

   struct dynaseg_s *subnext; // next dynaseg in subsector

   struct polyobj_s *polyobj;  // owner polyobject

   struct dynaseg_s *freenext;  // next dynaseg on freelist
} dynaseg_t;

#endif // R_DYNASEGS

#endif

// EOF

