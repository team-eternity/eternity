// Emacs style mode select   -*- C -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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

#ifndef P_TICK_H__
#define P_TICK_H__

#include "doomtype.h"

//
// CZoneLevelItem
//
// haleyjd 11/22/10:
// This class can be inherited from to obtain the ability to be allocated on the
// zone heap with a PU_LEVEL allocation tag.
//
class CZoneLevelItem
{
public:
   void *operator new (size_t size);
   void  operator delete (void *p);
};

// Doubly linked list of actors.
class CThinker : public CZoneLevelItem
{
private:
   // Private implementation details
   void RemoveDelayed();   

protected:
   // Virtual methods (overridables)
   virtual void Think() {}

   // Methods
   void AddToThreadedList(int tclass);

public:
   // Static functions
   static void RunThinkers();

   // Methods
   void Add();
   void Remove();

   // Virtual methods (overridables)
   virtual void Update();

   // Data Members

   CThinker *prev, *next;
  
   // killough 8/29/98: we maintain thinkers in several equivalence classes,
   // according to various criteria, so as to allow quicker searches.

   CThinker *cnext, *cprev; // Next, previous thinkers in same class

   // killough 11/98: count of how many other objects reference
   // this one using pointers. Used for garbage collection.
   unsigned int references;

   // haleyjd 08/01/09: use to number thinkers instead of *prev, which angers
   // GCC with thoughts of writing pointers into integers.
   unsigned int ordinal;

   boolean removed;
};

// Called by C_Ticker, can call G_PlayerExited.
// Carries out all thinking of monsters and players.
void P_Ticker(void);

extern CThinker thinkercap;  // Both the head and tail of the thinker list

void P_InitThinkers(void);

template<typename T> void P_SetTarget(T **mop, T *targ); // killough 11/98

// killough 8/29/98: threads of thinkers, for more efficient searches
typedef enum 
{
  th_delete,  // haleyjd 11/09/06: giant bug fix
  th_misc,
  th_friends,
  th_enemies,
  NUMTHCLASS
} th_class;

extern CThinker thinkerclasscap[];

// sf: jumping-viewz-on-hyperlift bug
extern boolean reset_viewz;

#endif

//----------------------------------------------------------------------------
//
// $Log: p_tick.h,v $
// Revision 1.5  1998/05/15  00:36:22  killough
// Remove unnecessary crash hack
//
// Revision 1.4  1998/05/13  22:58:01  killough
// Restore Doom bug compatibility for demos
//
// Revision 1.3  1998/05/03  22:49:29  killough
// Add external declarations, formerly in p_local.h
//
// Revision 1.2  1998/01/26  19:27:31  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
