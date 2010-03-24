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

#include "z_zone.h"
#include "doomtype.h"
#include "d_io.h"
#include "m_dllist.h"
#include "e_hash.h"

//
// Default LinkForObject Function
//
// Under the default means of functioning, only one mdllistitem_t can be in the
// object, and it must be the first item defined in the object. This means that
// the link and object pointers are aliases.
//
static void *E_LinkForObject(void *object)
{
   return object;
}

//
// E_HashInit
//
// Initializes a hash table.
//
void E_HashInit(ehash_t *table, unsigned int numchains, ehashable_i *hinterface)
{
   table->chains      = calloc(numchains, sizeof(mdllistitem_t *));
   table->numchains   = numchains;
   table->numitems    = 0;
   table->loadfactor  = 0.0f;
   table->hashfunc    = hinterface->hashKey;
   table->compfunc    = hinterface->compare;
   table->keyfunc     = hinterface->getKey;
   table->linkfunc    = hinterface->getLink ? hinterface->getLink 
                                            : E_LinkForObject;
   table->iteratorPos = -1;
   table->isinit      = true;
}

//
// E_HashDestroy
//
// Frees the hash table's chains.
//
void E_HashDestroy(ehash_t *table)
{
   if(table->chains)
      free(table->chains);

   memset(table, 0, sizeof(ehash_t));
}

//
// E_HashAddObject
//
// Put an object into the hash table
//
void E_HashAddObject(ehash_t *table, void *object)
{
   if(table->isinit)
   {
      unsigned int hashcode;
      const void *key     = table->keyfunc(object);
      mdllistitem_t *link = table->linkfunc(object);
      link->data          = table->hashfunc(key);   // cache hash value in node
      hashcode            = link->data % table->numchains;

      M_DLListInsertWithPtr(link, object, &(table->chains[hashcode]));

      table->numitems++;

      table->loadfactor = (float)table->numitems / table->numchains;
   }
}

//
// E_HashRemoveObject
//
// Removes an object from the hash table, provided it is in the
// hash table already.
//
void E_HashRemoveObject(ehash_t *table, void *object)
{
   if(table->isinit)
   {
      M_DLListRemove(table->linkfunc(object));
      
      table->numitems--;
      table->loadfactor = (float)table->numitems / table->numchains;
   }
}

//
// E_HashObjectForKey
//
// Tries to find an object for the given key in the hash table
//
void *E_HashObjectForKey(ehash_t *table, const void *key)
{
   if(table->isinit)
   {
      unsigned int hashcode = table->hashfunc(key) % table->numchains;
      mdllistitem_t *chain  = table->chains[hashcode];
      
      while(chain && !table->compfunc(table, chain->object, key))
         chain = chain->next;
      
      return chain ? chain->object : NULL;
   }
   else
      return NULL;
}

//
// E_HashChainForKey
//
// Returns the first object on the same chain as that used by the given
// key. The object returned (if any) does not necessarily have the same
// key, of course.
//
void *E_HashChainForKey(ehash_t *table, const void *key)
{
   if(table->isinit)
   {
      unsigned int hashcode = table->hashfunc(key) % table->numchains;
      mdllistitem_t *chain = table->chains[hashcode];

      return chain ? chain->object : NULL;
   }
   else
      return NULL;
}

//
// E_HashNextOnChain
//
// Returns the next object on the same hash chain, which may or may not
// have the same key as the previous object.
//
void *E_HashNextOnChain(ehash_t *table, void *object)
{
   if(table->isinit)
   {
      mdllistitem_t *link = table->linkfunc(object);

      return link->next ? link->next->object : NULL;
   }
   else
      return NULL;
}

//
// E_HashKeyForObject
//
// Retrieves a key from an object in the hash table.
//
const void *E_HashKeyForObject(ehash_t *table, void *object)
{
   return table->isinit ? table->keyfunc(object) : NULL;
}

//
// E_HashObjectIterator
//
// Looks for the next object after the current one specified having the
// same key. If passed NULL in object, it will start a new search.
// Returns NULL when the search has reached the end of the hash chain.
//
void *E_HashObjectIterator(ehash_t *table, void *object, const void *key)
{
   void *ret;

   if(!table->isinit)
      return NULL;

   if(!object) // starting a new search?
      ret = E_HashObjectForKey(table, key);
   else
   {
      mdllistitem_t *item = table->linkfunc(object);

      // start on next object in hash chain
      item = item->next; 

      // walk down the chain
      while(item && !table->compfunc(table, item->object, key))
         item = item->next;

      ret = item ? item->object : NULL;
   }

   return ret;
}

//
// E_HashTableIterator
//
// Iterates over all objects in a hash table, in chain order.
// Pass NULL in object and -1 in *index to start a new search.
// NULL is returned when the entire table has been iterated over.
//
void *E_HashTableIterator(ehash_t *table, void *object)
{
   void *ret = NULL;

   if(!table->isinit)
      return NULL;

   // already searching?
   if(object)
   {
      // is there another object on the same chain?
      mdllistitem_t *item = table->linkfunc(object);

      if(item->next)
         ret = item->next->object;
   }
   else
      table->iteratorPos = -1; // starting a new search, reset position

   // search for the next chain with an object in it
   while(!ret && ++table->iteratorPos < (signed int)(table->numchains))
   {
      mdllistitem_t *chain = table->chains[table->iteratorPos];
      
      if(chain)
         ret = chain->object;
   }

   return ret;
}

//
// E_HashRebuild
//
// Rehashes all objects in the table, in the event that the load factor has
// exceeded acceptable levels.
//
void E_HashRebuild(ehash_t *table, unsigned int newnumchains)
{
   mdllistitem_t **oldchains = table->chains;    // save current chains
   unsigned int oldnumchains = table->numchains;
   unsigned int i;

   if(!table->isinit)
      return;

   // allocate a new chains table
   table->chains    = calloc(newnumchains, sizeof(mdllistitem_t *));
   table->numchains = newnumchains;

   // run down the old chains
   for(i = 0; i < oldnumchains; ++i)
   {
      mdllistitem_t *chain, *prevobj = NULL;
      unsigned int hashcode;

      // restart from beginning of old chain each time
      while((chain = oldchains[i]))
      {
         // remove from old hash
         M_DLListRemove(chain);

         // clear out fields for safety
         chain->next = NULL;
         chain->prev = NULL;

         // re-add to new hash at end of list
         hashcode = chain->data % table->numchains;
         M_DLListInsertWithPtr(chain, chain->object, 
                               prevobj ? &prevobj->next : 
                                         &(table->chains[hashcode]));
         prevobj = chain;
      }
   }

   // recalculate load factor
   table->loadfactor = (float)table->numitems / table->numchains;

   // delete old chains
   free(oldchains);
}

//=============================================================================
//
// Specializations
//

//
// Case-insensitive String Hash
//

#include "d_dehtbl.h"

//
// E_NCStrHashFunc
//
// No-case string hash key computation function for string-specialized hashes.
// 
static unsigned int E_NCStrHashFunc(const void *key)
{
   return D_HashTableKey(*(const char **)key);
}

//
// E_NCStrCompareFunc
//
// No-case string comparison callback for string-specialized hashes.
//
static boolean E_NCStrCompareFunc(ehash_t *table, void *object, const void *key)
{
   const char *objkey = *(const char **)E_HashKeyForObject(table, object);

   return !strcasecmp(objkey, *(const char **)key);
}

//
// E_NCStrHashInit
//
// Creates a hash specialized to deal with structures which use a
// case-insensitive string as their key. The hash computation and comparison
// functions are provided implicitly, but the key retrieval function must be
// supplied to this function. It can be created using the E_KEYFUNC macro
// defined in e_hash.h.
//
void E_NCStrHashInit(ehash_t *table, unsigned int numchains, EKeyFunc_t kfunc,
                     ELinkFunc_t lfunc)
{
   ehashable_i ncstrhashi;

   ncstrhashi.hashKey = E_NCStrHashFunc;
   ncstrhashi.compare = E_NCStrCompareFunc;
   ncstrhashi.getKey  = kfunc;
   ncstrhashi.getLink = lfunc;

   E_HashInit(table, numchains, &ncstrhashi);
}

//
// Case-sensitive String Hash
//

//
// E_StrHashFunc
//
// string hash key computation function for string-specialized hashes.
// 
static unsigned int E_StrHashFunc(const void *key)
{
   return D_HashTableKeyCase(*(const char **)key);
}


//
// E_StrCompareFunc
//
// string comparison callback for string-specialized hashes.
//
static boolean E_StrCompareFunc(ehash_t *table, void *object, const void *key)
{
   const char *objkey = *(const char **)E_HashKeyForObject(table, object);

   return !strcmp(objkey, *(const char **)key);
}

//
// E_StrHashInit
//
// Creates a hash specialized to deal with structures which use a
// case-sensitive string as their key. The hash computation and comparison
// functions are provided implicitly, but the key retrieval function must be
// supplied to this function. It can be created using the E_KEYFUNC macro
// defined in e_hash.h.
//
void E_StrHashInit(ehash_t *table, unsigned int numchains, EKeyFunc_t kfunc,
                   ELinkFunc_t lfunc)
{
   ehashable_i estrhashi;

   estrhashi.hashKey = E_StrHashFunc;
   estrhashi.compare = E_StrCompareFunc;
   estrhashi.getKey  = kfunc;
   estrhashi.getLink = lfunc;

   E_HashInit(table, numchains, &estrhashi);
}


//
// Unsigned Integer Hash
//

//
// E_UintHashFunc
//
// Hash key function for unsigned integer keys.
//
static unsigned int E_UintHashFunc(const void *key)
{
   return *(const unsigned int *)key;
}

//
// E_UintCompareFunc
//
// Key comparison function for unsigned integer keys.
//
static boolean E_UintCompareFunc(ehash_t *table, void *object, const void *key)
{
   unsigned int objkey = *(unsigned int *)E_HashKeyForObject(table, object);

   return objkey == *(const unsigned int *)key;
}

//
// E_UintHashInit
//
// Creates a hash specialized to deal with structures which use an integer
// as their key.
// 
void E_UintHashInit(ehash_t *table, unsigned int numchains, EKeyFunc_t kfunc,
                    ELinkFunc_t lfunc)
{
   ehashable_i uinthashi;

   uinthashi.hashKey = E_UintHashFunc;
   uinthashi.compare = E_UintCompareFunc;
   uinthashi.getKey  = kfunc;
   uinthashi.getLink = lfunc;

   E_HashInit(table, numchains, &uinthashi);
}

//
// Signed Integer Hash
//

//
// E_SintHashFunc
//
// Hash key function for signed integer keys.
//
static unsigned int E_SintHashFunc(const void *key)
{
   return (unsigned int)(*(const int *)key);
}

//
// E_SintCompareFunc
//
// Key comparison function for signed integer keys.
//
static boolean E_SintCompareFunc(ehash_t *table, void *object, const void *key)
{
   int objkey = *(int *)E_HashKeyForObject(table, object);

   return objkey == *(const int *)key;
}

//
// E_SintHashInit
//
// Creates a hash specialized to deal with structures which use an integer
// as their key.
// 
void E_SintHashInit(ehash_t *table, unsigned int numchains, EKeyFunc_t kfunc,
                    ELinkFunc_t lfunc)
{
   ehashable_i sinthashi;

   sinthashi.hashKey = E_SintHashFunc;
   sinthashi.compare = E_SintCompareFunc;
   sinthashi.getKey  = kfunc;
   sinthashi.getLink = lfunc;

   E_HashInit(table, numchains, &sinthashi);
}


// EOF

