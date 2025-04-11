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
//      Mission start screen wipe/melt, special effects.
//
//-----------------------------------------------------------------------------

#ifndef __F_WIPE_H__
#define __F_WIPE_H__

//
// SCREEN WIPE PACKAGE
//

void Wipe_Drawer(void);
void Wipe_Ticker(void);
void Wipe_StartScreen(void);
void Wipe_ScreenReset(void);
void Wipe_SaveEndScreen(void);
void Wipe_BlitEndScreen(void);

extern bool inwipe;
extern int wipetype;

#endif

//----------------------------------------------------------------------------
//
// $Log: f_wipe.h,v $
// Revision 1.3  1998/05/03  22:11:27  killough
// beautification
//
// Revision 1.2  1998/01/26  19:26:49  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:54  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
