// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Collection objects
//
//-----------------------------------------------------------------------------

#ifndef M_COLLECTION_H__
#define M_COLLECTION_H__

#include "z_zone.h"
#include "i_system.h"
#include "m_random.h"

//
// BaseCollection
//
// This is a base class for functionality shared by the other collection types.
// It cannot be used directly but must be inherited from.
//
template<typename T> class BaseCollection : public ZoneObject
{
protected:
   T *ptrArray;
   size_t length;
   size_t numalloc;
   size_t wrapiterator;

   BaseCollection()
      : ZoneObject(), ptrArray(NULL), length(0), numalloc(0), wrapiterator(0)
   {
   }

   BaseCollection(int zoneTag)
      : ZoneObject(), ptrArray(NULL), length(0), numalloc(0), wrapiterator(0)
   {
      ChangeTag(zoneTag);
   }

   //
   // resize
   //
   // Resizes the internal array by the amount provided
   //
   void resize(size_t amtToAdd)
   {
      size_t newnumalloc = numalloc + amtToAdd;
      if(newnumalloc > numalloc)
      {
         ptrArray = (T *)(realloc(ptrArray, newnumalloc * sizeof(T)));
         memset(ptrArray + numalloc, 0, (newnumalloc - numalloc) * sizeof(T));
         numalloc = newnumalloc;
      }
   }

   //
   // baseClear
   //
   // Frees the storage, if any is allocated, and clears all member variables
   //
   void baseClear()
   {
      if(ptrArray)
         free(ptrArray);
      ptrArray = NULL;
      length = 0;
      numalloc = 0;
      wrapiterator = 0;
   }

public:
   // getLength
   size_t getLength() const { return length; }

   // isEmpty: test if the collection is empty or not
   bool isEmpty() const { return length == 0; }
   
   //
   // wrapIterator
   //
   // Returns the next item in the collection, wrapping to the beginning
   // once the end is reached. The iteration state is stored in the
   // collection object, so you can only do one such iteration at a time.
   //
   T &wrapIterator()
   {
      if(!ptrArray || !length)
         I_Error("BaseCollection::wrapIterator: called on empty collection\n");

      T &ret = ptrArray[wrapiterator++];

      wrapiterator %= length;

      return ret;
   }
   
   //
   // at
   //
   // Get the item at a given index. Index must be in range from 0 to length-1.
   //
   T &at(size_t index)
   {
      if(!ptrArray || index >= length)
         I_Error("BaseCollection::at: array index out of bounds\n");
      return ptrArray[index];
   }

   // operator[] - Overloaded operator wrapper for at method.
   T &operator [] (size_t index) { return at(index); }
   
   //
   // getRandom
   //
   // Returns a random item from the collection.
   //
   T &getRandom(pr_class_t rng)
   {
      size_t index;

      if(!ptrArray || !length)
         I_Error("BaseCollection::getRandom: called on empty collection\n");
      
      index = (size_t)P_Random(rng) % length;

      return ptrArray[index];
   }
};

//
// PODCollection
//
// This class can store primitive/intrinsic data types or POD objects. It does
// not do any construction or destruction of the objects it stores; it is just
// a simple reallocating array with maximum efficiency.
//
template<typename T> class PODCollection : public BaseCollection<T>
{
public:
   // Basic constructor
   PODCollection() : BaseCollection<T>()
   {
   }
   
   // Parameterized constructor
   PODCollection(size_t initSize, int zoneTag = PU_STATIC) 
      : BaseCollection<T>(zoneTag)
   {
      BaseCollection<T>::resize(initSize);
   }

   // Assignment
   void assign(const PODCollection<T> &other)
   {
      size_t oldlength = length;

      if(this->ptrArray == other.ptrArray) // same object?
         return;
      
      length = other.length;
      wrapiterator = other.wrapiterator;
      
      if(length <= numalloc)
         resize(length - oldlength);

      memcpy(ptrArray, other.ptrArray, length * sizeof(T));
   }

   // Copy constructor
   PODCollection(const PODCollection<T> &other) : BaseCollection<T>()
   {
      assign(other);
   }
   
   // Destructor
   ~PODCollection() { clear(); }

   // operator = - Overloaded operator wrapper for assign method
   PODCollection<T> &operator = (const PODCollection<T> &other)
   {
      assign(other);
      return *this;
   }

   //
   // clear
   //
   // Empties the collection and frees its storage.
   //
   void clear() { baseClear(); }

   //
   // makeEmpty
   //
   // Marks the collection as empty but doesn't free the storage.
   //
   void makeEmpty()
   {
      length = wrapiterator = 0;
      memset(ptrArray, 0, numalloc * sizeof(T));
   }

   //
   // add
   //
   // Adds a new item to the end of the collection.
   //
   void add(const T &newItem)
   {
      if(length >= numalloc)
         resize(length ? length : 32); // double array size
      ptrArray[length] = newItem;
      ++length;
   }
};

//
// Collection
//
// This class can store any type of data, including objects that require 
// constructor/destructor calls and contain virtual methods.
//
template<typename T> class Collection : public BaseCollection<T>
{
public:
   // Basic constructor
   Collection() : BaseCollection<T>()
   {
   }
   
   // Parameterized constructor
   Collection(size_t initSize, int zoneTag = PU_STATIC) 
      : BaseCollection<T>(zoneTag)
   {
      BaseCollection<T>::resize(initSize);
   }
   
   // Destructor
   ~Collection() { clear(); }

   //
   // clear
   //
   // Empties the collection and frees its storage.
   //
   void clear()
   {
      if(ptrArray)
      {
         // manually call all destructors
         for(size_t i = 0; i < length; i++)
            ptrArray[i].~T();
      }
      baseClear();
   }
   
   //
   // makeEmpty
   //
   // Marks the collection as empty but doesn't free the storage.
   //
   void makeEmpty()
   {
      if(ptrArray)
      {
         // manually call all destructors
         for(size_t i = 0; i < length; i++)
            ptrArray[i].~T();

         memset(ptrArray, 0, numalloc * sizeof(T));
      }
      length = wrapiterator = 0;
   }

   //
   // add
   //
   // Adds a new item to the end of the collection.
   //
   void add(const T &newItem)
   {
      if(length >= numalloc)
         resize(length ? length : 32); // double array size
      
      // copy construct new item
      ::new (&ptrArray[length]) T(newItem);
      
      ++length;
   }
};

#endif

// EOF

