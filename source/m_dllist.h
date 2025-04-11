//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
// Purpose: EDF Generalized Double-linked List Routines
// Authors: James Haley
//
// haleyjd 08/05/05: This is Lee Killough's smart double-linked list
// implementation with pointer-to-pointer prev links, generalized to
// be able to work with any structure. This type of double-linked list 
// can only be traversed from head to tail, but it treats all nodes 
// uniformly even without the use of a dummy head node, and thus it is 
// very efficient. These routines are inlined for maximum speed.
//
// You are responsible for defining the pointer used as the head of the list.
//    

#ifndef M_DLLIST_H__
#define M_DLLIST_H__

//
// This template class is an evolution of the original mdllistitem_t.
// However rather than using an is-a relationship, this functions best
// in a has-a relationship (which is the same role it could already
// play via use of the object member pointer).
//
// This class is intentionally a POD and will most likely remain that way
// for speed and efficiency concerns.
//
template<typename T> class DLListItem
{
public:
   DLListItem<T>  *dllNext;
   DLListItem<T> **dllPrev;
   T              *dllObject; // 08/02/09: pointer back to object
   unsigned int    dllData;   // 02/07/10: arbitrary data cached at node

   inline void insert(T *parentObject, DLListItem<T> **head)
   {
      DLListItem<T> *next = *head;

      if((dllNext = next))
         next->dllPrev = &dllNext;
      dllPrev = head;
      *head = this;

      dllObject = parentObject; // set to object, which is generally distinct
   }

   inline void remove()
   {
      DLListItem<T> **prev = dllPrev;
      DLListItem<T>  *next = dllNext;

      // haleyjd 05/07/13: safety #1: only if prev is non-null
      if(prev && (*prev = next))
         next->dllPrev = prev;

      // haleyjd 05/07/13: safety #2: clear links.
      dllPrev = nullptr;
      dllNext = nullptr;
   }

   // DEPRECATED
   inline    operator T * () const { return dllObject; }

   // DEPRECATED
   inline T *operator ->  () const { return dllObject; }
};

//
// Reverse the order of a list made of DLListItem instances.
//
template<typename T>
static void DLList_Reverse(DLListItem<T> **head)
{
   if(!(head && *head))
      return;

   DLListItem<T> *cur  = *head;
   DLListItem<T> *prev = nullptr;
   while(cur)
   {
      DLListItem<T> *next = cur->dllNext;

      cur->dllNext = prev;
      cur->dllPrev = next ? &next->dllNext : head;

      prev = cur;
      cur  = next;
   }

   *head = prev;
}

//
// haleyjd 05/07/13: Added a list type which makes use of DLListItem more
// regulated. Use is strictly optional. Provide the type and a member to
// pointer to the DLListItem field in the class the list will use for links.
//
template<typename T, DLListItem<T> T::* link> class DLList
{
public:
   DLListItem<T> *head;
   inline void insert(T *object) { (object->*link).insert(object, &head); }
   inline void remove(T *object) { (object->*link).remove();              }
   inline void insert(T &object) { insert(&object);                       }
   inline void remove(T &object) { remove(&object);                       }

   inline void reverse() { DLList_Reverse<>(&head); }
};

#endif

// EOF

