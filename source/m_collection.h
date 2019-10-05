//
// The Eternity Engine
// Copyright (C) 2016 James Haley et al.
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
// Purpose: Collection objects
// Authors: James Haley, David Hill, Ioan Chera
//

#ifndef M_COLLECTION_H__
#define M_COLLECTION_H__

#include <utility>

#include "z_zone.h"
#include "i_system.h"
#include "m_random.h"

//
// This is a base class for functionality shared by the other collection types.
// It cannot be used directly but must be inherited from.
//
template<typename T> 
class BaseCollection : public ZoneObject
{
protected:
   T *ptrArray;
   size_t length;
   size_t numalloc;
   size_t wrapiterator;

   BaseCollection()
      : ZoneObject(), ptrArray(nullptr), length(0), numalloc(0), wrapiterator(0)
   {
   }

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
   // Frees the storage, if any is allocated, and clears all member variables
   //
   void baseClear()
   {
		efree(ptrArray);
      ptrArray = nullptr;
      length = 0;
      numalloc = 0;
      wrapiterator = 0;
   }

   //
   // Certain operations may invalidate the wrap iterator.
   //
   void checkWrapIterator()
   {
      if(wrapiterator >= length)
         wrapiterator = 0;
   }

public:
   // get the length of the collection
   size_t getLength() const { return length; }

   // test if the collection is empty or not
   bool isEmpty() const { return length == 0; }
   
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
   // Get the item at a given index. Index must be in range from 0 to length-1.
   //
   T &at(size_t index) const
   {
      if(!ptrArray || index >= length)
         I_Error("BaseCollection::at: array index out of bounds\n");
      return ptrArray[index];
   }

   // Overloaded operator wrapper for at method.
   T &operator [] (size_t index) const { return at(index); }
   
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
// This class can store primitive/intrinsic data types or POD objects. It does
// not do any construction or destruction of the objects it stores; it is just
// a simple reallocating array with maximum efficiency.
//
template<typename T> 
class PODCollection : public BaseCollection<T>
{
protected:
   //
   // For move constructor and move assignment - transfer allocation from other
   // and set other to empty state.
   //
   void moveFrom(PODCollection<T> &&other) noexcept
   {
      if(this->ptrArray == other.ptrArray) // same object?
         return;

      this->clear();
      this->ptrArray     = other.ptrArray;
      this->length       = other.length;
      this->numalloc     = other.numalloc;
      this->wrapiterator = other.wrapiterator;

      other.ptrArray = nullptr;
      other.length = other.numalloc = 0;
      other.wrapiterator = 0;
   }

public:
   // Basic constructor
   PODCollection() : BaseCollection<T>()
   {
   }
   
   // Parameterized constructor
   explicit PODCollection(size_t initSize) : BaseCollection<T>()
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

   // Overloaded operator wrapper for assign method
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

   // Move constructor
   PODCollection(PODCollection<T> &&other) noexcept
   {
      moveFrom(std::move(other));
   }

   // Move assignment
   PODCollection<T> &operator = (PODCollection<T> &&other) noexcept 
   { 
      moveFrom(std::move(other)); 
      return *this;
   }
   
   // Destructor
   ~PODCollection() { clear(); }


   //
   // Empties the collection and frees its storage.
   //
   void clear() { this->baseClear(); }

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
   // Zero out the storage but do not change any other properties, including
   // length.
   //
   void zero()
   {
      if(this->ptrArray)
         memset(this->ptrArray, 0, this->numalloc * sizeof(T));
   }

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
   
   //
   // erase
   //
   // IOANCH: a method that moves the end object into destination and pops
   //
   void erase(size_t index)
   {
      this->ptrArray[index] = this->ptrArray[--this->length];
      this->wrapiterator = 0;
   }
   void erase(T* iter)
   {
      *iter = this->ptrArray[--this->length];
      this->wrapiterator = 0;
   }
   void eraseItem(const T& item)
   {
      for(size_t i = 0; i < this->length; ++i)
         if(this->at(i) == item)
         {
            this->erase(i);
            return;
         }
   }
};

//
// This class can store any type of data, including objects that require 
// constructor/destructor calls and contain virtual methods.
//
template<typename T> 
class Collection : public BaseCollection<T>
{
protected:
   const T *prototype; // An unowned object of type T that serves as a prototype

   //
   // For move constructor and move assignment - transfer allocation from other
   // and set other to empty state.
   //
   void moveFrom(Collection<T> &&other) noexcept
   {
      if(this->ptrArray == other.ptrArray) // same object?
         return;

      this->clear();
      this->ptrArray     = other.ptrArray;
      this->length       = other.length;
      this->numalloc     = other.numalloc;
      this->wrapiterator = other.wrapiterator;
      this->prototype    = other.prototype;

      other.ptrArray = nullptr;
      other.length = other.numalloc = 0;
      other.wrapiterator = 0;
   }

   //
   // When adding
   //
   void ensuresize()
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
   }

public:
   // Basic constructor
   Collection() : BaseCollection<T>(), prototype(nullptr)
   {
   }
   
   // Parameterized constructor
   explicit Collection(size_t initSize) : BaseCollection<T>(), prototype(nullptr)
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

   // Copy assignment
   Collection<T> &operator = (const Collection<T> &other) 
   { 
      assign(other); 
      return *this;
   }

   // Move constructor
   Collection(Collection<T> &&other) noexcept
   {
      moveFrom(std::move(other));
   }

   // Move assignment
   Collection<T> &operator = (Collection<T> &&other) noexcept 
   { 
      moveFrom(std::move(other)); 
      return *this;
   }

   // Destructor
   ~Collection() { this->clear(); }

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
   // Adds a new item to the end of the collection.
   //
   void add(const T &newItem)
   {
      ensuresize();

      // placement copy construct new item
      ::new (&this->ptrArray[this->length]) T(newItem);
      
      ++this->length;
   }

   void add(T &&newItem)
   {
      ensuresize();
      ::new (&this->ptrArray[this->length]) T(std::move(newItem));
      ++this->length;
   }

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

