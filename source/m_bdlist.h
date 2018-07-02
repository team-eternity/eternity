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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//  Generalized Bidirectional Double-linked List Routines
//
// haleyjd 04/16/12: A generalized bidirectional, circular double linked list.
// As the name suggests, the primary distinction between this and DLListItem
// is that the resulting list can be traversed forward and backward. A dummy
// head node is also required.
//    
//-----------------------------------------------------------------------------

#ifndef M_BDLIST_H__
#define M_BDLIST_H__

#define BDLISTMAGIC 0xABADCAFEu

// 
// BDListItem<T>
//
// Like DLListItem, class is intentionally a POD and will most likely remain
// that way for speed and efficiency concerns.
//
// WARNING: The listHead dummy node uses its bdData field to track whether or
// not the list has been initialized. If it has not been (the prev and next
// pointers of the head must start out pointing to the list head object itself),
// then the BDLISTMAGIC constant is written there after the initialization is
// performed. Calling the static Init method yourself will clear out the list,
// but make sure you are not leaking the objects in it by doing that :)
//
template<typename T> class BDListItem
{
public:
   BDListItem<T> *bdNext;
   BDListItem<T> *bdPrev;
   T             *bdObject; // pointer back to object
   unsigned int   bdData;   // arbitrary data cached at node

   inline static void Init(BDListItem<T> &listHead)
   {
      listHead.bdPrev = listHead.bdNext = &listHead;
      listHead.bdData = BDLISTMAGIC;
   }

   inline void insert(T *parentObject, BDListItem<T> &listHead)
   {
      if(listHead.bdData != BDLISTMAGIC)
         Init(listHead);

      listHead.bdPrev->bdNext = this;
      this->bdNext = &listHead;
      this->bdPrev = listHead.bdPrev;
      listHead.bdPrev = this;

      this->bdObject = parentObject;
   }

   // WARNING: If you are iterating over the list, you MUST pass your
   // iteration pointer in *myIterator or you will have to restart the
   // iteration from the beginning of the list. Do *NOT* attempt to
   // cache next/prev values from the object around this call yourself.

   inline void remove(BDListItem<T> **myIterator = NULL)
   {
      BDListItem<T> *lnext = this->bdNext;

      if(myIterator)
         *myIterator = this->bdPrev;

      (lnext->bdPrev = this->bdPrev)->bdNext = lnext;
   }

   inline bool isDummy() const
   {
      return this->bdData == BDLISTMAGIC;
   }
};

#undef BDLISTMAGIC

#endif

// EOF

