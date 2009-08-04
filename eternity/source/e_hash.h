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

// hash computation function prototype
typedef unsigned int (*EHashFunc_t)(const void *);

// object comparison function prototype
typedef boolean      (*ECompFunc_t)(void *, void *, const void *);

// key retrieval function prototype
typedef const void * (*EKeyFunc_t) (void *);

// link retrieval function prototype
typedef void *       (*ELinkFunc_t)(void *);

typedef struct ehash_s
{
   mdllistitem_t **chains; // hash chains
   unsigned int numchains; // number of chains
   unsigned int numitems;  // number of items currently in table
   float loadfactor;       // load = numitems / numchains
   EHashFunc_t hashfunc;   // hash computation function
   ECompFunc_t compfunc;   // object comparison function
   EKeyFunc_t  keyfunc;    // key retrieval function
   ELinkFunc_t linkfunc;   // link-for-object function
   boolean isinit;         // true if hash is initialized

} ehash_t;

void  E_HashInit(ehash_t *, unsigned int, EHashFunc_t, ECompFunc_t, EKeyFunc_t, 
                 ELinkFunc_t);
void  E_HashAddObject(ehash_t *, void *);
void  E_HashRemoveObject(ehash_t *, void *);
void  E_HashRemoveObjectNC(ehash_t *, void *);
void *E_HashObjectForKey(ehash_t *, const void *);
void *E_HashChainForKey(ehash_t *, const void *);
void *E_HashNextOnChain(ehash_t *, void *);
const void *E_HashKeyForObject(ehash_t *, void *);
void *E_HashObjectIterator(ehash_t *, void *, const void *);
void  E_HashRebuild(ehash_t *, unsigned int);

// specializations
void  E_NCStrHashInit(ehash_t *, unsigned int, EKeyFunc_t, ELinkFunc_t);
void  E_UintHashInit(ehash_t *, unsigned int, EKeyFunc_t, ELinkFunc_t);
void  E_SintHashInit(ehash_t *, unsigned int, EKeyFunc_t, ELinkFunc_t);

// Key Function macro - autogenerates a key retrieval function

#define E_KEYFUNC(type, field) \
   static const void *EHashKeyFunc_ ## type ## field (void *object) \
   { \
      return &(((type *)object)-> field ); \
   }

#define E_KEYFUNCNAME(type, field) EHashKeyFunc_ ## type ## field

// Link Function macro - autogenerates a link retrieval function.
// This is only needed if the mdllistitem_t is not the first item
// in the structure. In that case, pass NULL as the linkfunc and a
// default function will be used.

#define E_LINKFUNC(type, field) \
   static void *EHashLinkFunc_ ## type ## field (void *object) \
   { \
      return &(((type *)object)-> field ); \
   }

#define E_LINKFUNCNAME(type, field) EHashLinkFunc_ ## type ## field

#endif

// EOF

