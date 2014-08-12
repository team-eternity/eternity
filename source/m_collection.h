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
//      Collection objects
//
//-----------------------------------------------------------------------------

#ifndef M_COLLECTION_H__
#define M_COLLECTION_H__

#include <utility>

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

   //
   // baseResize
   //
   // Resizes the internal array by the amount provided
   //
   void baseResize(size_t amtToAdd)
   {
      size_t newnumalloc = numalloc + amtToAdd;
      if(newnumalloc > numalloc)
      {
         ptrArray = erealloc(T *, ptrArray, newnumalloc * sizeof(T));
         memset(static_cast<void *>(ptrArray + numalloc), 0, 
                (newnumalloc - numalloc) * sizeof(T));
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
		efree(ptrArray);
      ptrArray = NULL;
      length = 0;
      numalloc = 0;
      wrapiterator = 0;
   }

   //
   // checkWrapIterator
   //
   // Certain operations may invalidate the wrap iterator.
   //
   void checkWrapIterator()
   {
      if(wrapiterator >= length)
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
   
   // Obtain the wrap iterator position
   size_t getWrapIteratorPos() const { return wrapiterator; }
   
   // Set the wrap iterator position, if the indicated position is valid
   void setWrapIteratorPos(size_t i)
   {
      if(i < length)
         wrapiterator = i;
   }


   //
   // at
   //
   // Get the item at a given index. Index must be in range from 0 to length-1.
   //
   T &at(size_t index) const
   {
      if(!ptrArray || index >= length)
         I_Error("BaseCollection::at: array index out of bounds\n");
      return ptrArray[index];
   }

   // operator[] - Overloaded operator wrapper for at method.
   T &operator [] (size_t index) const { return at(index); }
   
   //
   // getRandom
   //
   // Returns a random item from the collection.
   //
   T &getRandom(pr_class_t rngnum)
   {
      size_t index;

      if(!ptrArray || !length)
         I_Error("BaseCollection::getRandom: called on empty collection\n");
      
      index = (size_t)P_Random(rngnum) % length;

      return ptrArray[index];
   }

   // STL-compatible iterator semantics
   typedef T *iterator;

   T *begin() const { return ptrArray; }
   T *end()   const { return ptrArray + length; }
    
    // Mister jackson
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
   PODCollection(size_t initSize) : BaseCollection<T>()
   {
      this->baseResize(initSize);
   }

   // Assignment
   void assign(const PODCollection<T> &other)
   {
      size_t oldlength = this->length;

      if(this->ptrArray == other.ptrArray) // same object?
         return;
      
      this->length = other.length;
      this->wrapiterator = other.wrapiterator;
      
      if(this->length > this->numalloc)
         this->baseResize(this->length - oldlength);

      memcpy(this->ptrArray, other.ptrArray, this->length * sizeof(T));
   }

   // Copy constructor
   PODCollection(const PODCollection<T> &other)
   {
      this->assign(other);
   }
   
   // Destructor
   ~PODCollection() { clear(); }

   // operator = - Overloaded operator wrapper for assign method
   PODCollection<T> &operator = (const PODCollection<T> &other)
   {
      this->assign(other);
      return *this;
   }
   
   //
   // IOANCH 20130805: added move semantics
   //
   // Assignment
   void assign(PODCollection<T> &&other)
   {
      if(this->ptrArray == other.ptrArray) // same object?
         return;
      
      this->clear();
      this->length = other.length;
      this->wrapiterator = other.wrapiterator;
      this->ptrArray = other.ptrArray;
      this->numalloc = other.numalloc;
      
      other.ptrArray = nullptr;
		other.length = 0;
		other.wrapiterator = 0;
		other.numalloc = 0;
   }
   
   // Copy constructor
   PODCollection(PODCollection<T> &&other) : BaseCollection<T>()
   {
      this->assign(other);
   }
   // operator = - Overloaded operator wrapper for assign method
   PODCollection<T> &operator = (PODCollection<T> &&other)
   {
      this->assign(other);
      return *this;
   }

   //
   // clear
   //
   // Empties the collection and frees its storage.
   //
   void clear() { this->baseClear(); }

   //
   // makeEmpty
   //
   // Marks the collection as empty but doesn't free the storage.
   //
   template<bool KEEP = false>
   void makeEmpty()
   {
      this->length = this->wrapiterator = 0;
      if(!KEEP && this->ptrArray)
         memset(this->ptrArray, 0, this->numalloc * sizeof(T));
   }

   //
   // zero
   //
   // Zero out the storage but do not change any other properties, including
   // length.
   //
   void zero()
   {
      if(this->ptrArray)
         memset(this->ptrArray, 0, this->numalloc * sizeof(T));
   }

   //
   // add
   //
   // Adds a new item to the end of the collection.
   //
   void add(const T &newItem)
   {
      if(this->length >= this->numalloc)
         this->baseResize(this->length ? this->length : 32); // double array size
      this->ptrArray[this->length] = newItem;
      ++this->length;
   }

   //
   // addNew
   //
   // Adds a new zero-initialized item to the end of the collection.
   //
   T &addNew()
   {
      if(this->length >= this->numalloc)
         this->baseResize(this->length ? this->length : 32);
      memset(&(this->ptrArray[this->length]), 0, sizeof(T));
      
      return this->ptrArray[this->length++];
   }

   //
   // pop
   //
   // davidph 11/23/11: Returns the last item and decrements the length.
   //
   const T &pop()
   {
      if(!this->ptrArray || !this->length)
         I_Error("PODCollection::pop: array underflow\n");
      
      const T &ret = this->ptrArray[--this->length];
      this->checkWrapIterator();

      return ret;
   }
   void justPop()
   {
       if (!this->ptrArray || !this->length)
           I_Error("PODCollection::pop: array underflow\n");
       this->ptrArray[--this->length];
       this->wrapiterator = 0;
   }
   
   //
   // resize
   //
   // Change the length of the array.
   //
   void resize(size_t n)
   {
      // increasing?
      if(n > this->length)
      {
         // need to reallocate?
         if(n > this->numalloc)
            this->baseResize(n - this->numalloc);
         // ensure newly available slots are initialized
         memset(&(this->ptrArray[this->length]), 0, (n - this->length) * sizeof(T));
      }
      // set the new length, in all cases.
      this->length = n;
      this->checkWrapIterator();
   }

   //
   // reserve
   //
   // IOANCH 20132612: reserve space like with std::vector
   //
   void reserve(size_t size)
   {
	   if (size > this->numalloc)
		   this->baseResize(size - this->numalloc);
   }
   
   //
   // back
   //
   // IOANCH: return reference to last component
   //
   const T& back() const
   {
      if(!this->ptrArray || !this->length)
         I_Error("PODCollection::back: array underflow\n");
      return this->ptrArray[this->length - 1];
   }
   T& back() 
   {
       if (!this->ptrArray || !this->length)
           I_Error("PODCollection::back: array underflow\n");
       return this->ptrArray[this->length - 1];
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
protected:
   const T *prototype; // An unowned object of type T that serves as a prototype

public:
   // Basic constructor
   Collection() : BaseCollection<T>(), prototype(NULL)
   {
   }
   
   // Parameterized constructor
   Collection(size_t initSize) : BaseCollection<T>(), prototype(NULL)
   {
      this->baseResize(initSize);
   }
   
   // Assignment
   void assign(const Collection<T> &other)
   {
      if(this->ptrArray == other.ptrArray) // same object?
         return;

      this->clear();
      for(size_t i = 0; i < other.length; i++)
         this->add(other[i]);

      this->prototype = other.prototype;
   }

   // Copy constructor
   Collection(const Collection<T> &other)
   {
      assign(other);
   }

   // Destructor
   ~Collection() { this->clear(); }

   //
   // setPrototype
   //
   // Set the pointer to a prototype object of type T that can be used
   // for construction of new items in the collection without constant
   // instantiation of temporaries just for the purpose of passing into
   // methods of this class.
   // Ownership of the prototype is *not* assumed by the collection.
   //
   void setPrototype(const T *pPrototype) { prototype = pPrototype; }
   const T *getPrototype() const { return prototype; }

   //
   // clear
   //
   // Empties the collection and frees its storage.
   //
   void clear()
   {
      if(this->ptrArray)
      {
         // manually call all destructors
         for(size_t i = 0; i < this->length; i++)
            this->ptrArray[i].~T();
      }
      this->baseClear();
   }
   
   //
   // makeEmpty
   //
   // Marks the collection as empty but doesn't free the storage.
   //
   void makeEmpty()
   {
      if(this->ptrArray)
      {
         // manually call all destructors
         for(size_t i = 0; i < this->length; i++)
            this->ptrArray[i].~T();

         memset(static_cast<void *>(this->ptrArray), 0, this->numalloc * sizeof(T));
      }
      this->length = this->wrapiterator = 0;
   }

   //
   // add
   //
   // Adds a new item to the end of the collection.
   //
   void add(const T &newItem)
   {
      if(this->length >= this->numalloc)
      {
         size_t newnumalloc = this->numalloc + (this->length ? this->length : 32);

         if(newnumalloc > this->numalloc)
         {
            T *newItems = ecalloc(T *, newnumalloc, sizeof(T));
            for(size_t i = 0; i < this->length; i++)
            {
               ::new (&newItems[i]) T(std::move(this->ptrArray[i]));
               this->ptrArray[i].~T();
            }
            efree(this->ptrArray);
            this->ptrArray = newItems;
            this->numalloc = newnumalloc;
         }
      }
      
      // placement copy construct new item
      ::new (&this->ptrArray[this->length]) T(newItem);
      
      ++this->length;
   }

   //
   // add
   //
   // Adds a new item to the end of the collection, using the
   // prototype as the copy source - the prototype must be valid!
   //
   void add()
   {
      if(!prototype)
         I_Error("Collection::add: invalid prototype object\n");

      add(*prototype);
   }

   //
   // addNew
   //
   // Adds a new item to the end of the collection, using the prototype
   // as the copy source - must have a valid prototype!
   //
   T &addNew()
   {
      add();
      return this->ptrArray[this->length - 1];
   }
   
   //
   // pop
   //
   const T &pop()
   {
      if(!this->ptrArray || !this->length)
         I_Error("Collection::pop: array underflow\n");
      
      const T &ret = this->ptrArray[--this->length];
      this->checkWrapIterator();
      
      return ret;
   }
};

#endif

// EOF

