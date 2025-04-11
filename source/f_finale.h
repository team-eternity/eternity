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
//   Related to f_finale.c, which is called at the end of a level
//    
//-----------------------------------------------------------------------------

#ifndef F_FINALE_H__
#define F_FINALE_H__

#include "info.h"

struct event_t;

// haleyjd: this stuff is now needed in e_edf.c
struct castinfo_t
{
   char       *name;
   mobjtype_t  type;   
   bool        stopattack;
   struct castsound_t { int frame; int sound; } sounds[4];
};

extern int max_castorder;
extern castinfo_t *castorder;

extern char *f_fontname;
extern char *f_titlefontname;

//
// FINALE
//

// haleyjd 05/26/06: finale types
enum
{
   FINALE_TEXT,         // text only; goes to next level unless endOfGame
   FINALE_DOOM_CREDITS, // text, then DOOM credits screen
   FINALE_DOOM_DEIMOS,  // text, then Deimos pic
   FINALE_DOOM_BUNNY,   // text, then bunny scroller
   FINALE_DOOM_MARINE,  // text, then pic of DoomGuy
   FINALE_HTIC_CREDITS, // text, then Heretic credits screen
   FINALE_HTIC_WATER,   // text, then underwater pic
   FINALE_HTIC_DEMON,   // text, then demon scroller
   FINALE_PSX_UDOOM,    // text, then return to title
   FINALE_PSX_DOOM2,    // text, scrolling, then cast call
   FINALE_END_PIC,      // custom end-pic finale, from UMAPINFO
   FINALE_NUMFINALES,

   FINALE_UNSPECIFIED   // not specified explicitly
};

// haleyjd 02/25/09
void F_Init();

// Called by main loop.
bool F_Responder(const event_t *ev);

// Called by main loop.
void F_Ticker();

// Called by main loop.
void F_Drawer();

void F_StartFinale(bool secret);

#endif

//----------------------------------------------------------------------------
//
// $Log: f_finale.h,v $
// Revision 1.3  1998/05/04  21:58:52  thldrmn
// commenting and reformatting
//
// Revision 1.2  1998/01/26  19:26:47  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:54  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
