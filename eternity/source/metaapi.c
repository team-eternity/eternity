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
//
//   Metatables for storage of multiple types of objects in an associative
//   array.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomtype.h"
#include "m_dllist.h"
#include "e_hash.h"
#include "metaapi.h"

// Macros

// Tunables
#define METANUMCHAINS  53
#define METALOADFACTOR 0.667f

// These primes roughly double in size.
static const unsigned int metaPrimes[] =
{
     53,    97,   193,   389,   769,   1543,   3079,
   6151, 12289, 24593, 49157, 98317, 393241, 786433
};

#define METANUMPRIMES (sizeof(metaPrimes) / sizeof(unsigned int))

// Globals

// metaerrno represents the last error to occur. All routines that can cause
// an error will reset this to 0 if no error occurs.
int metaerrno = 0;

//=============================================================================
//
// General Utilities
//

// key retrieval function for metatable hashing
E_KEYFUNC(metaobject_t, key)

//
// MetaInit
//
// Initializes an ehash_t into a metatable.
//
void MetaInit(ehash_t *metatable)
{
   E_NCStrHashInit(metatable, METANUMCHAINS, E_KEYFUNCNAME(metaobject_t, key),
                   NULL);
}

//
// IsMetaKindOf
//
// Provides RTTI for metaobjects. Pass types to this function using the
// METATYPE macro.
//
boolean IsMetaKindOf(metaobject_t *object, const char *type)
{
   return !strcmp(object->type, type);
}

//
// MetaHas
//
// Returns true or false if an object of the same key is in the metatable.
// No type checking is done, so it will match any object with that key.
//
boolean MetaHas(ehash_t *metatable, const char *key)
{
   return (E_HashObjectForKey(metatable, key) != NULL);
}

//
// MetaHasType
//
// As above, but returns true only if there is a match for both key and
// type in the table. Because this has to search down the hash chain until
// it finds an object of the proper type, it is potentially slower than the
// routine above.
//
boolean MetaHasType(ehash_t *metatable, const char *key, const char *type)
{
   metaobject_t *obj = NULL;
   boolean objfound = false;

   while((obj = E_HashObjectIterator(metatable, obj, &key)))
   {
      if(IsMetaKindOf(obj, type))
      {
         objfound = true;
         break;
      }
   }

   return objfound;
}

//
// MetaCountOf
//
// Returns the count of objects in the metatable with the given key.
//
int MetaCountOf(ehash_t *metatable, const char *key)
{
   metaobject_t *obj = NULL;
   int count = 0;

   while((obj = E_HashObjectIterator(metatable, obj, &key)))
      ++count;

   return count;
}

//
// MetaCountOfType
//
// Returns the count of objects in the metatable with the given key and
// of the specified type only.
//
int MetaCountOfType(ehash_t *metatable, const char *key, const char *type)
{
   metaobject_t *obj = NULL;
   int count = 0;

   while((obj = E_HashObjectIterator(metatable, obj, &key)))
   {
      if(IsMetaKindOf(obj, type))
         ++count;
   }

   return count;
}

//=============================================================================
//
// General Metaobjects
//
// By putting this structure at the top of another structure, or allocating it
// separately, it is possible to include any type of data in the metatable.
//

//
// MetaAddObject
//
// Adds a generic metaobject to the table. The metatable does not assume
// responsibility for the memory management of metaobjects, key strings, or
// type strings.
//
void MetaAddObject(ehash_t *metatable, const char *key, metaobject_t *object,
                   void *data, const char *type)
{
   object->key    = key;
   object->type   = type;
   object->object = data; // generally, a pointer back to the owning structure

   // check for table overload
   if(metatable->loadfactor > METALOADFACTOR)
   {
      int newnumchains = 0;

      if(metatable->numchains < metaPrimes[METANUMPRIMES - 1])
      {
         int i;

         // find a prime larger than the current number of chains
         for(i = 0; metatable->numchains < metaPrimes[i]; ++i);
         
         newnumchains = metaPrimes[i];
      }
      else
         newnumchains = metatable->numchains * 2; // too big, just double it.

      E_HashRebuild(metatable, newnumchains);
   }

   E_HashAddObject(metatable, object);
}

//
// MetaRemoveObject
//
// Removes the provided object from the given metatable.
//
void MetaRemoveObject(ehash_t *metatable, metaobject_t *object)
{
   E_HashRemoveObject(metatable, object);
}

//
// MetaGetObject
//
// Returns the first object found in the metatable with the given key, 
// regardless of its type. Returns NULL if no such object exists.
//
metaobject_t *MetaGetObject(ehash_t *metatable, const char *key)
{
   return E_HashObjectForKey(metatable, &key);
}

//
// MetaGetObjectType
//
// Returns the first object found in the metatable which matches both the
// key and type. Returns NULL if no such object exists.
//
metaobject_t *MetaGetObjectType(ehash_t *metatable, const char *key,
                                const char *type)
{
   metaobject_t *obj = NULL;

   while((obj = E_HashObjectIterator(metatable, obj, &key)))
   {
      if(IsMetaKindOf(obj, type))
         break;
   }

   return obj;
}

//
// MetaGetNextObject
//
// Returns the next object with the same key, or the first such object
// in the table if NULL is passed in the object pointer. Returns NULL
// when no further objects of the same key are available.
//
// This is just a wrapper around E_HashObjectIterator, but you should
// call this anyway if you want to pretend you don't know how the
// metatable is implemented.
//
metaobject_t *MetaGetNextObject(ehash_t *metatable, metaobject_t *object,
                                const char *key)
{
   return E_HashObjectIterator(metatable, object, &key);
}

//
// MetaGetNextType
//
// Similar to above, but this returns the next object which also matches
// the specified type.
//
metaobject_t *MetaGetNextType(ehash_t *metatable, metaobject_t *object,
                              const char *key, const char *type)
{
   metaobject_t *obj = object;

   while((obj = E_HashObjectIterator(metatable, obj, &key)))
   {
      if(IsMetaKindOf(obj, type))
         break;
   }

   return obj;
}

//=============================================================================
//
// Metaobject Specializations
//
// We provide specialized metaobjects for basic types here.
// Ownership of metaobjects for basic types *is* assumed by the routines here.
// Adding or removing a metaobject of these types via their specific interfaces
// below will allocate or free the objects holding them.
//

//
// Integer
//

//
// MetaAddInt
//
// Add an integer to the metatable.
//
void MetaAddInt(ehash_t *metatable, const char *key, int value)
{
   metaint_t *newInt = calloc(1, sizeof(metaint_t));

   newInt->value = value;
   MetaAddObject(metatable, key, &newInt->parent, newInt, METATYPE(metaint_t));
}

//
// MetaGetInt
//
// Get an integer from the metatable. This routine returns the value
// rather than a pointer to a metaint_t. If an object of the requested
// name doesn't exist in the table, 0 is returned and metaerrno is set
// to indicate the problem.
//
// Use of this routine only returns the first such value in the table.
// This routine is meant for singleton fields.
//
int MetaGetInt(ehash_t *metatable, const char *key)
{
   metaobject_t *obj;

   metaerrno = 0;

   if(!(obj = MetaGetObjectType(metatable, key, METATYPE(metaint_t))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      return 0;
   }

   return ((metaint_t *)obj->object)->value;
}

//
// MetaRemoveInt
//
// Removes the given field if it exists as a metaint_t.
// Only one object will be removed. If more than one such object 
// exists, you would need to call this routine until metaerrno is
// set to META_ERR_NOSUCHOBJECT.
//
// The value of the object is returned in case it is needed.
//
int MetaRemoveInt(ehash_t *metatable, const char *key)
{
   metaobject_t *obj;
   int value;

   metaerrno = 0;

   if(!(obj = MetaGetObjectType(metatable, key, METATYPE(metaint_t))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      return 0;
   }

   MetaRemoveObject(metatable, obj);

   value = ((metaint_t *)obj->object)->value;

   free(obj->object);

   return value;
}

//
// Strings
//
// metastrings created with these APIs do NOT assume ownership of the
// string. If the string is dynamically allocated, you are responsible for
// releasing its storage after destroying the metastring.
//

//
// MetaAddString
//
void MetaAddString(ehash_t *metatable, const char *key, const char *value)
{
   metastring_t *newString = calloc(1, sizeof(metastring_t));

   newString->value = value;

   MetaAddObject(metatable, key, &newString->parent, newString, 
                 METATYPE(metastring_t));
}

//
// MetaGetString
//
// Get a string from the metatable. This routine returns the value
// rather than a pointer to a metastring_t. If an object of the requested
// name doesn't exist in the table, NULL is returned and metaerrno is set
// to indicate the problem.
//
// Use of this routine only returns the first such value in the table.
// This routine is meant for singleton fields.
//
const char *MetaGetString(ehash_t *metatable, const char *key)
{
   metaobject_t *obj;

   metaerrno = 0;

   if(!(obj = MetaGetObjectType(metatable, key, METATYPE(metastring_t))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      return NULL;
   }

   return ((metastring_t *)obj->object)->value;
}

//
// MetaRemoveString
//
// Removes the given field if it exists as a metastring_t.
// Only one object will be removed. If more than one such object 
// exists, you would need to call this routine until metaerrno is
// set to META_ERR_NOSUCHOBJECT.
//
// The value of the object is returned in case it is needed, and
// if the string is dynamically allocated, you will need to then
// free it yourself using the proper routine.
//
const char *MetaRemoveString(ehash_t *metatable, const char *key)
{
   metaobject_t *obj;
   const char *value;

   metaerrno = 0;

   if(!(obj = MetaGetObjectType(metatable, key, METATYPE(metastring_t))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      return NULL;
   }

   MetaRemoveObject(metatable, obj);

   value = ((metastring_t *)obj->object)->value;

   free(obj->object);

   return value;
}

// EOF

