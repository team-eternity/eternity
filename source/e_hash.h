// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2009 James Haley
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
//    Generic hash table implementation.
//
//-----------------------------------------------------------------------------

#ifndef E_HASH_H__
#define E_HASH_H__

#include "e_hashkeys.h"

//
// EHashTable<T, K>
//
// This class replaces the ehash_t structure to provide a generic,
// high-performance hash table which utilizes the in-structure link capability
// provided by DLListItem<T>, meaning no cache-harmful separately allocated
// links to traverse through.
//
// Provide the template the primary type of object to be stored in T, and the
// type of HashKey object being used in K (see e_hashkeys.h for some basic key 
// object types that implement the required interface).
//
template<typename T, typename K> class EHashTable
{
public:
   typedef T              item_type;    // Type of item this hashtable can hold
   typedef K              key_type;     // Type of key marshalling object
   typedef DLListItem<T> link_type;    // Type of linked list item
   typedef key_type  T::* keyptr_type;  // Type of pointer-to-member for key
   typedef link_type T::* linkptr_type; // Type of pointer-to-member for links
   
   // Type of key's basic data member
   typedef typename key_type::basic_type basic_key_type; 

protected:
   keyptr_type    hashKey;     // pointer-to-member for hash key object
   linkptr_type   linkPtr;     // pointer-to-member for linked list links
   link_type    **chains;      // hash chains
   bool           isInit;      // true if hash is initialized
   unsigned int   numChains;   // number of chains
   unsigned int   numItems;    // number of items currently in table
   float          loadFactor;  // load = numitems / numchains
   int            iteratorPos;
   
   void calcLoadFactor() { loadFactor = (float)numItems / numChains; }

public:
   //
   // Constructor
   //
   // Pass the pointer-to-member from the contained type for the key and the
   // link (must be a DLListItem<T>). The hash table uses these pointer-to-
   // members on all the objects in the hash so that it can manipulate the
   // objects without knowing anything else about their type or layout.
   //
   EHashTable(keyptr_type pKey, linkptr_type pLink) 
      : hashKey(pKey), linkPtr(pLink), chains(NULL), isInit(false), 
        numChains(0), numItems(0), loadFactor(0.0f), iteratorPos(-1)
   {
   }

   // Basic accessors
   int   isInitialized() const { return isInit;     }
   float getLoadFactor() const { return loadFactor; }
   
   unsigned int getNumItems()  const { return numItems;  }
   unsigned int getNumChains() const { return numChains; }

   // 
   // Initialize
   //
   // Initializes a hash table. This is currently done here rather than in the
   // constructor primarily because EHashTables are generally global objects,
   // and therefore they cannot safely call Z_Malloc during C++ init. I'd rather
   // this move to the constructor eventually.
   //
   void Initialize(unsigned int pNumChains)
   {
      numChains = pNumChains;
      chains    = (link_type **)(calloc(numChains, sizeof(link_type *)));
      isInit    = true;
   }

   //
   // Destroy
   //
   // Frees the hash table's chains. Again this would be better in the 
   // destructor, but it should remain here until/unless initialization is moved
   // into the constructor, so it remains balanced.
   //
   void Destroy()
   {
      if(chains)
         free(chains);

      chains      =  NULL;
      isInit      =  false;
      numChains   =  0;
      numItems    =  0;
      loadFactor  =  0.0f;
      iteratorPos = -1;
   }

   //
   // addObject
   //
   // Put an object into the hash table
   //
   void addObject(item_type &object)
   {
      if(isInit)
      {
         link_type &link = object.*linkPtr;
         link.dllData    = (object.*hashKey).hashCode();
         unsigned int hc = link.dllData % numChains;

         link.insert(&object, &chains[hc]);

         ++numItems;
         calcLoadFactor();
      }
   }

   // Convenience overload for pointers
   void addObject(item_type *object) { addObject(*object); }

   //
   // removeObject
   //
   // Removes an object from the hash table, provided it is in the
   // hash table already.
   //
   void removeObject(item_type &object)
   {
      if(isInit)
      {
         link_type &link = object.*linkPtr;
         link.remove();

         --numItems;
         calcLoadFactor();
      }
   }

   // Convenience overload for pointers
   void removeObject(item_type *object) { removeObject(*object); }

   //
   // objectForKey(key_type&)
   //
   // Tries to find an object for the given key in the hash table. This version
   // takes a reference to an object of the key type passed into the template.
   //
   item_type *objectForKey(const key_type &key) const
   {
      if(isInit)
      {
         unsigned int hc  = key.hashCode() % numChains;
         link_type *chain = chains[hc];

         while(chain && chain->dllObject->*hashKey != key)
            chain = chain->dllNext;

         return chain ? chain->dllObject : NULL;
      }
      else
         return NULL;
   }

   //
   // objectForKey(basic_key_type)
   //
   // As above, but taking an argument of the key object type's basic_type 
   // typedef. ie., an int, const char *, etc. This is useful for passing in
   // literals without having to manually construct a temporary object at
   // every point of call.
   //
   item_type *objectForKey(basic_key_type key) const
   {
      key_type tempKey;
      tempKey = key;

      return objectForKey(tempKey);
   }

   //
   // chainForKey(key_type&)
   //
   // Returns the first object on the hash chain used by the given key, or NULL
   // if that hash chain is empty. The object returned does not necessarily 
   // match the given key.
   //
   item_type *chainForKey(const key_type &key) const
   {
      if(isInit)
      {
         unsigned int hc  = key.hashCode() % numChains;
         link_type *chain = chains[hc];

         return chain ? chain->dllObject : NULL;
      }
      else
         return NULL;
   }

   //
   // chainForKey(basic_key_type)
   //
   // As above but allowing literal key input, as with objectForKey.
   //
   item_type *chainForKey(basic_key_type key) const
   {
      key_type tempKey;
      tempKey = key;

      return chainForKey(tempKey);
   }

   //
   // nextOnChain
   //
   // Returns the next object on the same hash chain, which may or may not
   // have the same key as the previous object.
   //
   item_type *nextOnChain(item_type *object) const
   {
      if(isInit)
      {
         link_type &link = object->*linkPtr;

         return link.dllNext ? link.dllNext->dllObject : NULL;
      }
      else
         return NULL;
   }

   //
   // keyForObject
   //
   // Retrieves a key from an object in the hash table.
   //
   key_type &keyForObject(T *object) const
   {
      return isInit ? object->*keyPtr : NULL;
   }

   //
   // keyIterator
   //
   // Looks for the next object after the current one specified having the
   // same key. If passed NULL in object, it will start a new search.
   // Returns NULL when the search has reached the end of the hash chain.
   //
   item_type *keyIterator(item_type *object, const key_type &key)
   {
      item_type *ret;

      if(!isInit)
         return NULL;

      if(!object) // starting a new search?
         ret = objectForKey(key);
      else
      {
         link_type *link = &(object->*linkPtr);

         // start on next object in hash chain
         link = link->dllNext;

         // walk down the chain
         while(link && link->dllObject->*hashKey != key)
            link = link->dllNext;

         ret = link ? link->dllObject : NULL;
      }

      return ret;
   }

   item_type *keyIterator(item_type *object, basic_key_type key)
   {
      key_type tempKey;
      tempKey = key;

      return keyIterator(object, tempKey);
   }

   //
   // tableIterator
   //
   // Iterates over all objects in a hash table, in chain order.
   // Pass NULL in object to start a new search. NULL is returned when the 
   // entire table has been iterated over.
   //
   item_type *tableIterator(item_type *object)
   {
      item_type *ret = NULL;

      if(!isInit)
         return NULL;

      // already searching?
      if(object)
      {
         // is there another object on the same chain?
         link_type &link = object->*linkPtr;

         if(link.dllNext)
            ret = link.dllNext->dllObject;
      }
      else
         iteratorPos = -1; // starting a new search, reset position

      // search for the next chain with an object in it
      while(!ret && ++iteratorPos < (signed int)numChains)
      {
         link_type *chain = chains[iteratorPos];

         if(chain)
            ret = chain->dllObject;
      }

      return ret;
   }

   //
   // Rebuild
   //
   // Rehashes all objects in the table, in the event that the load factor has
   // exceeded acceptable levels. Rehashing policy is determined by user code.
   //
   void Rebuild(unsigned int newNumChains)
   {
      link_type    **oldchains    = chains;    // save current chains
      unsigned int   oldNumChains = numChains;
      unsigned int   i;

      if(!isInit)
         return;

      // allocate a new chains table
      chains = (link_type **)(calloc(newNumChains, sizeof(link_type *)));
      numChains = newNumChains;

      // run down the old chains
      for(i = 0; i < oldNumChains; ++i)
      {
         link_type *chain, *prevobj = NULL;
         unsigned int hashcode;

         // restart from beginning of old chain each time
         while((chain = oldchains[i]))
         {
            // remove from old hash
            chain->remove();

            // clear out fields for safety
            chain->dllNext = NULL;
            chain->dllPrev = NULL;

            // re-add to new hash at end of list
            hashcode = chain->dllData % numChains;

            chain->insert(chain->dllObject, prevobj ? &(prevobj->dllNext) :
                                                      &(chains[hashcode]));
            prevobj = chain;
         }
      }

      // recalculate load factor
      calcLoadFactor();

      // delete old chains
      free(oldchains);
   }
};

#endif

// EOF

