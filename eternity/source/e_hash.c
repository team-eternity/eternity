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
// E_HashInit
//
// Initializes a hash table.
//
void E_HashInit(ehash_t *table, unsigned int numchains,
                EHashFunc_t hfunc, ECompFunc_t cfunc, EKeyFunc_t kfunc)
{
   table->chains     = calloc(numchains, sizeof(mdllistitem_t *));
   table->numchains  = numchains;
   table->numitems   = 0;
   table->loadfactor = 0.0f;
   table->hashfunc   = hfunc;
   table->compfunc   = cfunc;
   table->keyfunc    = kfunc;
}

//
// E_HashDestroy
//
// Frees the hash table's chains.
//
void E_HashDestroy(ehash_t *table)
{
   if(table->chains)
   {
      free(table->chains);
      memset(table, 0, sizeof(ehash_t));
   }

   table->numchains  = 0;
   table->numitems   = 0;
   table->loadfactor = 0.0f;
}

//
// E_HashAddObject
//
// Put an object into the hash table
//
void E_HashAddObject(ehash_t *table, void *object)
{
   void *key = table->keyfunc(object);
   unsigned int hashcode = table->hashfunc(key) % table->numchains;

   M_DLListInsert(object, &(table->chains[hashcode]));

   table->numitems++;

   table->loadfactor = (float)table->numitems / table->numchains;
}

//
// E_HashAddObjectNC
//
// Adds an object but doesn't modify the table loadfactor.
//
void E_HashAddObjectNC(ehash_t *table, void *object)
{
   // save data
   unsigned int numitems = table->numitems;
   float loadfactor      = table->loadfactor;

   E_HashAddObject(table, object);

   // restore data
   table->numitems   = numitems;
   table->loadfactor = loadfactor;
}

//
// E_HashRemoveObject
//
// Removes an object from the hash table, provided it is in the
// hash table already.
//
void E_HashRemoveObject(ehash_t *table, void *object)
{
   M_DLListRemove(object);

   table->numitems--;
   table->loadfactor = (float)table->numitems / table->numchains;
}

//
// E_HashRemoveObjectNC
//
// Removes the object but doesn't modify the hash loadfactor.
//
void E_HashRemoveObjectNC(void *object)
{
   M_DLListRemove(object);
}

//
// E_HashObjectForKey
//
// Tries to find an object for the given key in the hash table
//
void *E_HashObjectForKey(ehash_t *table, void *key)
{
   unsigned int hashcode = table->hashfunc(key) % table->numchains;
   mdllistitem_t *chain  = table->chains[hashcode];

   while(chain && !table->compfunc(table, chain, key))
      chain = chain->next;

   return chain;
}

//
// E_HashKeyForObject
//
// Retrieves a key from an object in the hash table.
//
void *E_HashKeyForObject(ehash_t *table, void *object)
{
   return table->keyfunc(object);
}

//
// E_HashObjectIterator
//
// Looks for the next object after the current one specified having the
// same key. If passed NULL in object, it will start a new search.
// Returns NULL when the search has reached the end of the hash chain.
//
void *E_HashObjectIterator(ehash_t *table, void *object, void *key)
{
   void *ret;

   if(!object) // starting a new search?
      ret = E_HashObjectForKey(table, key);
   else
   {
      mdllistitem_t *item = (mdllistitem_t *)object;

      // start on next object in hash chain
      item = item->next; 

      // walk down the chain
      while(item && !table->compfunc(table, item, key))
         item = item->next;

      ret = item;
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

   // allocate a new chains table
   table->chains    = calloc(newnumchains, sizeof(mdllistitem_t *));
   table->numchains = newnumchains;

   // run down the old chains
   for(i = 0; i < oldnumchains; ++i)
   {
      mdllistitem_t *chain = oldchains[i];

      while(chain)
      {
         // remove from old hash
         E_HashRemoveObjectNC(chain);

         // clear out fields
         chain->next = NULL;
         chain->prev = NULL;

         // add to new hash
         E_HashAddObjectNC(table, chain);

         // restart from beginning of old chain
         chain = oldchains[i];
      }
   }

   // recalculate load factor (numitems has not changed)
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
static unsigned int E_NCStrHashFunc(void *key)
{
   return D_HashTableKey((const char *)key);
}

//
// E_NCStrCompareFunc
//
// No-case string comparison callback for string-specialized hashes.
//
static boolean E_NCStrCompareFunc(void *table, void *object, void *key)
{
   ehash_t *hash = (ehash_t *)table;
   const char *objkey = E_HashKeyForObject(hash, object);

   return !strcasecmp(objkey, (const char *)key);
}

//
// E_NCStrHashInit
//
// Creates a hash specialized to deal with structures which use a
// case-insensitive string as their key. The hash computation and comparison
// functions are provided implicitly, but the key retrieval function must be
// supplied to this function. It can be created using the E_HASHKEYFUNC macro
// defined in e_hash.h.
//
void E_NCStrHashInit(ehash_t *table, unsigned int numchains, EKeyFunc_t kfunc)
{
   E_HashInit(table, numchains, E_NCStrHashFunc, E_NCStrCompareFunc, kfunc);
}

//
// Integer Hash
//

//
// E_UintHashFunc
//
// Hash key function for unsigned integer keys.
//
static unsigned int E_UintHashFunc(void *key)
{
   return (unsigned int)((intptr_t)key);
}

//
// E_UintCompareFunc
//
// Key comparison function for unsigned integer keys.
//
static boolean E_UintCompareFunc(ehash_t *table, void *object, void *key)
{
   ehash_t *hash = (ehash_t *)table;
   unsigned int objkey = 
      (unsigned int)((intptr_t)E_HashKeyForObject(hash, object));

   return objkey == (unsigned int)((intptr_t)key);
}

//
// E_UintHashInit
//
// Creates a hash specialized to deal with structures which use an integer
// as their key.
// 
void E_UintHashInit(ehash_t *table, unsigned int numchains, EKeyFunc_t kfunc)
{
   E_HashInit(table, numchains, E_UintHashFunc, E_UintCompareFunc, kfunc);
}

// EOF

