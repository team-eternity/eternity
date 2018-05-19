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

#ifndef P_TICK_H__
#define P_TICK_H__

#include "e_rtti.h"

class SaveArchive;
class Thinker;

//
// Thinker
//
// Doubly linked list of actors.
//
class Thinker : public RTTIObject
{
   DECLARE_RTTI_TYPE(Thinker, RTTIObject)

private:
   // Private implementation details - Methods
   void removeDelayed(); 

   // Data members
   // killough 11/98: count of how many other objects reference
   // this one using pointers. Used for garbage collection.
   mutable unsigned int references;
   
   // Statics
   // Current position in list during RunThinkers
   static Thinker *currentthinker;

protected:
   // Virtual methods (overridables)
   virtual void Think() {}

   // Methods
   void addToThreadedList(int tclass);

   // Data Members
   bool removed;

   // haleyjd 12/22/2010: for savegame enumeration
   unsigned int ordinal;

public:
   // Constructor
   Thinker() 
      : Super(), references(0), removed(false), ordinal(0), prev(NULL),
        next(NULL), cprev(NULL), cnext(NULL)
   {
   }

   // operator new, overriding ZoneObject::operator new (size_t)
   void *operator new (size_t size) { return ZoneObject::operator new(size, PU_LEVEL); }

   // Static functions
   static void InitThinkers();
   static void RunThinkers();

   // Methods
   void addThinker();
   
   // Accessors
   bool isRemoved() const { return removed; }
   
   // Reference counting
   void addReference() const { ++references; }
   void delReference() const { --references; }

   // Enumeration 
   // For thinkers needing savegame enumeration.
   void setOrdinal(unsigned int i) { ordinal = shouldSerialize() ? i : 0; }
   unsigned int getOrdinal() const { return ordinal; }

   // Virtual methods (overridables)
   virtual void updateThinker();
   virtual void remove();

   // Serialization
   // When using serialize, always call your parent implementation!
   virtual void serialize(SaveArchive &arc);
   // De-swizzling should restore pointers to other thinkers.
   virtual void deSwizzle() {}
   virtual bool shouldSerialize() const { return !removed;  }
   
   // Data Members

   Thinker *prev;
   Thinker *next;
  
   // killough 8/29/98: we maintain thinkers in several equivalence classes,
   // according to various criteria, so as to allow quicker searches.

   Thinker *cprev; // Next, previous thinkers in same class
   Thinker *cnext;
};

//
// thinker_cast
//
// Use this dynamic_cast variant to automatically check if something is a valid
// unremoved Thinker subclass instance. This is necessary because the old 
// behavior of checking function pointer values effectively changed the type 
// that a thinker was considered to be when it was set into a deferred removal
// state, and C++ doesn't support that with virtual methods OR RTTI.
//
template<typename T> inline T thinker_cast(Thinker *th)
{
   typedef typename std::remove_pointer<T>::type base_type;

   return (th && !th->isRemoved() && th->isDescendantOf(&base_type::StaticType)) ?
      static_cast<T>(th) : NULL;
}
template<typename T> inline T thinker_cast(const Thinker *th)
{
   typedef typename std::remove_pointer<T>::type base_type;

   return (th && !th->isRemoved() && th->isDescendantOf(&base_type::StaticType)) ?
   static_cast<T>(th) : NULL;
}


// Called by C_Ticker, can call G_PlayerExited.
// Carries out all thinking of monsters and players.
void P_Ticker(void);

extern Thinker thinkercap;  // Both the head and tail of the thinker list

//
// P_NextThinker
//
// davidph 06/02/12: Finds the next Thinker of the specified type.
//
template<typename T> T *P_NextThinker(T *th)
{
   Thinker *itr;

   for(itr = th ? th->next : thinkercap.next; itr != &thinkercap; itr = itr->next)
   {
      if((th = thinker_cast<T *>(itr)))
         return th;
   }

   return NULL;
}

//
// P_SetTarget
//
// killough 11/98
// This function is used to keep track of pointer references to mobj thinkers.
// In Doom, objects such as lost souls could sometimes be removed despite 
// their still being referenced. In Boom, 'target' mobj fields were tested
// during each gametic, and any objects pointed to by them would be prevented
// from being removed. But this was incomplete, and was slow (every mobj was
// checked during every gametic). Now, we keep a count of the number of
// references, and delay removal until the count is 0.
//
template<typename T> void P_SetTarget(T **mop, T *targ)
{
   if(*mop)             // If there was a target already, decrease its refcount
      (*mop)->delReference();
   if((*mop = targ))    // Set new target and if non-NULL, increase its counter
      targ->addReference();
}

template<typename T> void P_ClearTarget(T *&mop)
{
   if(mop)             // If there was a target already, decrease its refcount
      mop->delReference();
   mop = nullptr;
}

// killough 8/29/98: threads of thinkers, for more efficient searches
typedef enum 
{
  th_delete,  // haleyjd 11/09/06: giant bug fix
  th_misc,
  th_friends,
  th_enemies,
  NUMTHCLASS
} th_class;

extern Thinker thinkerclasscap[];

//
// DECLARE_THINKER_TYPE
//
#define DECLARE_THINKER_TYPE(name, inherited) DECLARE_RTTI_TYPE(name, inherited)
   
//
// IMPLEMENT_THINKER_TYPE
//
#define IMPLEMENT_THINKER_TYPE(name) IMPLEMENT_RTTI_TYPE(name)

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
