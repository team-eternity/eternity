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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//    Bounding-box routines (clipping, bsp, etc)
//    
//-----------------------------------------------------------------------------

#ifndef M_BBOX_H__
#define M_BBOX_H__

#include "m_fixed.h"

// Bounding box coordinate storage.
enum
{
  BOXTOP,
  BOXBOTTOM,
  BOXLEFT,
  BOXRIGHT
};

// Bounding box functions.

void M_ClearBox(fixed_t *box);

void M_AddToBox(fixed_t *box, fixed_t x, fixed_t y);
void M_AddToBox2(fixed_t *box, fixed_t x, fixed_t y);

inline static bool M_BoxesTouch(const fixed_t bbox1[4], const fixed_t bbox2[4])
{
   return bbox1[BOXTOP] >= bbox2[BOXBOTTOM] && bbox1[BOXBOTTOM] <= bbox2[BOXTOP] &&
      bbox1[BOXLEFT] <= bbox2[BOXRIGHT] && bbox1[BOXRIGHT] >= bbox2[BOXLEFT];
}

#endif

//----------------------------------------------------------------------------
//
// $Log: m_bbox.h,v $
// Revision 1.3  1998/05/05  19:55:58  phares
// Formatting and Doc changes
//
// Revision 1.2  1998/01/26  19:27:06  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
