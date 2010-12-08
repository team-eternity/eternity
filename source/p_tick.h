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
   virtual ~CZoneLevelItem() {}
   void *operator new (size_t size);
   void  operator delete (void *p);
};

// Doubly linked list of actors.
class CThinker : public CZoneLevelItem
{
private:
   // Private implementation details
   void RemoveDelayed(); 
   
   // Current position in list during RunThinkers
   static CThinker *CThinker::currentthinker;

protected:
   // Virtual methods (overridables)
   virtual void Think() {}

   // Methods
   void AddToThreadedList(int tclass);

   // Data Members
   boolean removed;

   // killough 11/98: count of how many other objects reference
   // this one using pointers. Used for garbage collection.
   unsigned int references;

public:
   // Static functions
   static void InitThinkers();
   static void RunThinkers();

   // Methods
   void Add();
   
   // Accessors
   boolean isRemoved() const { return removed; }

   // Reference counting
   void addReference() { ++references; }
   void delReference() { --references; }

   // Virtual methods (overridables)
   virtual void Update();
   virtual void Remove();
   
   // Enumeration 
   // For thinkers needing savegame enumeration.
   // Must be implemented by a child class. Return the next enumeration
   // value from Enumerate.
   virtual void Enumerate(unsigned int val) {}
   virtual unsigned int getEnumeration() { return 0; }

   // Data Members

   CThinker *prev, *next;
  
   // killough 8/29/98: we maintain thinkers in several equivalence classes,
   // according to various criteria, so as to allow quicker searches.

   CThinker *cnext, *cprev; // Next, previous thinkers in same class
};

//
// thinker_cast
//
// Use this dynamic_cast variant to automatically check if something is a valid
// unremoved CThinker subclass instance. This is necessary because the old 
// behavior of checking function pointer values effectively changed the type 
// that a thinker was considered to be when it was set into a deferred removal
// state.
//
template<typename T> inline T thinker_cast(CThinker *th)
{
   return (th && !th->isRemoved()) ? dynamic_cast<T>(th) : NULL;
}

// Called by C_Ticker, can call G_PlayerExited.
// Carries out all thinking of monsters and players.
void P_Ticker(void);

extern CThinker thinkercap;  // Both the head and tail of the thinker list

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

//
// Thinker Factory
//
// haleyjd 12/07/10: The save game code needs to be able to construct thinkers
// of any class without resort to a gigantic switch statement. This calls for
// a factory pattern.
//

class CThinkerType
{
protected:
   const char *name;

public:
};

class CThinkerFactory
{
protected:
public:
};


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
