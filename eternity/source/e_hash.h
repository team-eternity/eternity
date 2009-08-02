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

typedef unsigned int (*EHashFunc_t)(void *);                 // hash function
typedef boolean      (*ECompFunc_t)(void *, void *, void *); // comparison function
typedef void *       (*EKeyFunc_t) (void *);                 // key retrieval function

typedef struct ehash_s
{
   mdllistitem_t **chains; // hash chains
   unsigned int numchains; // number of chains
   unsigned int numitems;  // number of items currently in table
   float loadfactor;       // load = numitems / numchains
   EHashFunc_t hashfunc;   // hash computation function
   ECompFunc_t compfunc;   // object comparison function
   EKeyFunc_t  keyfunc;    // key retrieval function
   boolean isinit;         // true if hash is initialized

} ehash_t;

void  E_HashInit(ehash_t *, unsigned int, EHashFunc_t, ECompFunc_t, EKeyFunc_t);
void  E_HashAddObject(ehash_t *, void *);
void  E_HashAddObjectNC(ehash_t *, void *);
void  E_HashRemoveObject(ehash_t *, void *);
void  E_HashRemoveObjectNC(void *);
void *E_HashObjectForKey(ehash_t *, void *);
void *E_HashKeyForObject(ehash_t *, void *);
void *E_HashObjectIterator(ehash_t *, void *, void *);
void  E_HashRebuild(ehash_t *, unsigned int);

// specializations
void  E_NCStrHashInit(ehash_t *, unsigned int, EKeyFunc_t);
void  E_UintHashInit(ehash_t *, unsigned int, EKeyFunc_t);
void  E_SintHashInit(ehash_t *, unsigned int, EKeyFunc_t);

#define E_KEYFUNC(type, field) \
   static void *EHashKeyFunc_ ## type (void *object) \
   { \
      return &(((type *)object)-> field ); \
   }

#define E_KEYFUNCNAME(type) EHashKeyFunc_ ## type

#endif

// EOF

