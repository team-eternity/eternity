// Emacs style mode select   -*- C -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley
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
//  Generalized Double-linked List Routines
//
// haleyjd 08/05/05: This is Lee Killough's smart double-linked list
// implementation with pointer-to-pointer prev links, generalized to
// be able to work with any structure. This type of double-linked list 
// can only be traversed from head to tail, but it treats all nodes 
// uniformly even without the use of a dummy head node, and thus it is 
// very efficient. These routines are inlined for maximum speed.
//
// Just put an mdllistitem_t as the first item in a structure and you
// can then cast a pointer to that structure to mdllistitem_t * and
// pass it to these routines. You are responsible for defining the 
// pointer used as the head of the list.
//    
//-----------------------------------------------------------------------------

#ifndef M_DLLIST_H__
#define M_DLLIST_H__

#include "d_keywds.h"

// 
// CDLListItem
//
// This template class is an evolution of the original mdllistitem_t.
// However rather than using an is-a relationship, this functions best
// in a has-a relationship (which is the same role it could already
// play via use of the object member pointer).
//
// This class is intentionally a POD and will most likely remain that way
// for speed and efficiency concerns.
//
template<typename T> class CDLListItem
{
public:
   CDLListItem<T>  *dllNext;
   CDLListItem<T> **dllPrev;
   T               *dllObject; // 08/02/09: pointer back to object
   unsigned int     dllData;   // 02/07/10: arbitrary data cached at node

   inline void insert(T *parentObject, CDLListItem<T> **head)
   {
      CDLListItem<T> *next = *head;

      if((this->dllNext = next))
         next->dllPrev = &this->dllNext;
      this->dllPrev = head;
      *head = this;

      this->dllObject = parentObject; // set to object, which is generally distinct
   }

   inline void remove()
   {
      CDLListItem<T> **prev = this->dllPrev;
      CDLListItem<T>  *next = this->dllNext;

      if((*prev = next))
         next->dllPrev = prev;
   }
};

#endif

// EOF

