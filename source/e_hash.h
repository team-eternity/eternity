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

typedef struct ehash_s ehash_t;

// hash computation function prototype
typedef unsigned int (*EHashFunc_t)(const void *);

// object comparison function prototype
typedef boolean      (*ECompFunc_t)(ehash_t *, void *, const void *);

// key retrieval function prototype
typedef const void * (*EKeyFunc_t) (void *);

// link retrieval function prototype
typedef void *       (*ELinkFunc_t)(void *);

//
// ehashable interface structure
//
// Create one of these and pass it to E_HashInit.
//
typedef struct ehashable_s
{
   EHashFunc_t hashKey; // computes hash key for object
   ECompFunc_t compare; // compares two objects
   EKeyFunc_t  getKey;  // gets key field for object
   ELinkFunc_t getLink; // gets link for object (optional)
} ehashable_i;

struct ehash_s
{
   mdllistitem_t **chains; // hash chains
   unsigned int numchains; // number of chains
   unsigned int numitems;  // number of items currently in table
   float loadfactor;       // load = numitems / numchains
   EHashFunc_t hashfunc;   // hash computation function
   ECompFunc_t compfunc;   // object comparison function
   EKeyFunc_t  keyfunc;    // key retrieval function
   ELinkFunc_t linkfunc;   // link-for-object function
   int iteratorPos;        // iterator index
   boolean isinit;         // true if hash is initialized
};

// Initialize a hash table.
void  E_HashInit(ehash_t *table, unsigned int numchains, ehashable_i *hinterface);

// Add an object to the table. The object must be an mdllistitem_t descendent.
void  E_HashAddObject(ehash_t *table, void *object);

// Remove an object from the table.
void  E_HashRemoveObject(ehash_t *table, void *object);

// Get the first object in the table for the given key. Note address of key must be
// passed, so for string specializations this becomes a pointer-to-pointer.
void *E_HashObjectForKey(ehash_t *table, const void *key);

// Returns the beginning of the hash chain for the given key.
void *E_HashChainForKey(ehash_t *table, const void *key);

// Returns the next item on the passed item's hash chain, regardless of key
void *E_HashNextOnChain(ehash_t *table, void *object);

// Returns the given object's hash key.
const void *E_HashKeyForObject(ehash_t *table, void *object);

// Iterates over all objects in the table with the given key.
void *E_HashObjectIterator(ehash_t *table, void *object, const void *key);

// Iterates over ALL objects in the entire table. Pass NULL in object to start a
// new search.
void *E_HashTableIterator(ehash_t *table, void *object);

// Rebuilds the hash table with the new number of chains provided. All objects
// will be rehashed using the key values cached in their list node.
void  E_HashRebuild(ehash_t *table, unsigned int newnumchains);

// Specializations

// Initialize an ehash_t as a case-insensitive string hash.
void  E_NCStrHashInit(ehash_t *table, unsigned int numchains, 
                      EKeyFunc_t kfunc, ELinkFunc_t lfunc);

// Initialize an ehash_t as a case-sensitive string hash.
void  E_StrHashInit  (ehash_t *table, unsigned int numchains, 
                      EKeyFunc_t kfunc, ELinkFunc_t lfunc);

// Initialize an ehash_t as an unsigned integer hash.
void  E_UintHashInit (ehash_t *table, unsigned int numchains, 
                      EKeyFunc_t kfunc, ELinkFunc_t lfunc);

// Initialize an ehash_t as a signed integer hash.
void  E_SintHashInit (ehash_t *table, unsigned int numchains, 
                      EKeyFunc_t kfunc, ELinkFunc_t lfunc);

// Key Function macro - autogenerates a key retrieval function

#define E_KEYFUNC(type, field) \
   static const void *EHashKeyFunc_ ## type ## field (void *object) \
   { \
      return &(((type *)object)-> field ); \
   }

#define E_KEYFUNCNAME(type, field) EHashKeyFunc_ ## type ## field

// Link Function macro - autogenerates a link retrieval function.
// This is only needed if the mdllistitem_t is not the first item
// in the structure. In that case, leave the getLink method of the
// ehashable interface NULL and a default method will be used.

#define E_LINKFUNC(type, field) \
   static void *EHashLinkFunc_ ## type ## field (void *object) \
   { \
      return &(((type *)object)-> field ); \
   }

#define E_LINKFUNCNAME(type, field) EHashLinkFunc_ ## type ## field

#endif

// EOF

