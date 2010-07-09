// Emacs style mode select   -*- C -*-
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
#include "m_qstr.h"
#include "m_misc.h"
#include "metaapi.h"

// Macros

// Tunables
#define METANUMCHAINS     53
#define METANUMTYPECHAINS 31
#define METALOADFACTOR    0.667f

// These primes roughly double in size.
static const unsigned int metaPrimes[] =
{
     53,    97,   193,   389,   769,  1543,   
   3079,  6151, 12289, 24593, 49157, 98317
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

// key retrieval function for metatable hashing by key
E_KEYFUNC(metaobject_t, key)

// key and link retrieval function for metatable hashing by type
E_KEYFUNC(metaobject_t, type)
E_LINKFUNC(metaobject_t, typelinks)

//
// MetaInit
//
// Initializes a metatable.
//
void MetaInit(metatable_t *metatable)
{
   // the key hash is growable.
   // keys are case-insensitive.
   E_NCStrHashInit(&metatable->keyhash, METANUMCHAINS, 
                   E_KEYFUNCNAME(metaobject_t, key), NULL);

   // the type hash is fixed size since there are a limited number of types
   // defined in the source code.
   // types are case sensitive, because they are based on C types.
   E_StrHashInit(&metatable->typehash, METANUMTYPECHAINS,
                 E_KEYFUNCNAME(metaobject_t, type), 
                 E_LINKFUNCNAME(metaobject_t, typelinks));
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
// MetaHasKey
//
// Returns true or false if an object of the same key is in the metatable.
// No type checking is done, so it will match any object with that key.
//
boolean MetaHasKey(metatable_t *metatable, const char *key)
{
   return (E_HashObjectForKey(&metatable->keyhash, key) != NULL);
}

//
// MetaHasType
//
// Returns true or false if an object of the same type is in the metatable.
//
boolean MetaHasType(metatable_t *metatable, const char *type)
{
   return (E_HashObjectForKey(&metatable->typehash, type) != NULL);
}

//
// MetaHasKeyAndType
//
// Returns true if an object exists in the table of both the specified key
// and type, and it is the same object. This is naturally slower as it must
// search down the key hash chain for a type match.
//
boolean MetaHasKeyAndType(metatable_t *metatable, const char *key, 
                          const char *type)
{
   metaobject_t *obj = NULL;
   boolean found = false;

   while((obj = E_HashObjectIterator(&metatable->keyhash, obj, &key)))
   {
      // for each object that matches the key, test the type
      if(IsMetaKindOf(obj, type))
      {
         found = true;
         break;
      }
   }

   return found;
}

//
// MetaCountOfKey
//
// Returns the count of objects in the metatable with the given key.
//
int MetaCountOf(metatable_t *metatable, const char *key)
{
   metaobject_t *obj = NULL;
   int count = 0;

   while((obj = E_HashObjectIterator(&metatable->keyhash, obj, &key)))
      ++count;

   return count;
}

//
// MetaCountOfType
//
// Returns the count of objects in the metatable with the given type.
//
int MetaCountOfType(metatable_t *metatable, const char *type)
{
   metaobject_t *obj = NULL;
   int count = 0;

   while((obj = E_HashObjectIterator(&metatable->typehash, obj, &type)))
      ++count;

   return count;
}

//
// MetaCountOfKeyAndType
//
// As above, but satisfying both conditions at once.
//
int MetaCountOfKeyAndType(metatable_t *metatable, const char *key, 
                          const char *type)
{
   metaobject_t *obj = NULL;
   int count = 0;

   while((obj = E_HashObjectIterator(&metatable->keyhash, obj, &key)))
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
// responsibility for the memory management of metaobjects or type strings.
// Key strings are managed, however, to avoid serious problems with mutual
// references between metaobjects.
//
void MetaAddObject(metatable_t *metatable, const char *key, metaobject_t *object,
                   void *data, const char *type)
{
   ehash_t *keyhash = &metatable->keyhash;

   object->key    = strdup(key);
   object->type   = type;
   object->object = data; // generally, a pointer back to the owning structure

   // check for key table overload
   if(keyhash->loadfactor > METALOADFACTOR && 
      keyhash->numchains < metaPrimes[METANUMPRIMES - 1])
   {
      int i;

      // find a prime larger than the current number of chains
      for(i = 0; keyhash->numchains < metaPrimes[i]; ++i);
         
      E_HashRebuild(keyhash, metaPrimes[i]);
   }

   // Add the object to the key table
   E_HashAddObject(keyhash, object);

   // Add the object to the type table, which is static in size
   E_HashAddObject(&metatable->typehash, object);
}

//
// MetaRemoveObject
//
// Removes the provided object from the given metatable. The key 
// string will be freed.
//
void MetaRemoveObject(metatable_t *metatable, metaobject_t *object)
{
   E_HashRemoveObject(&metatable->keyhash,  object);
   E_HashRemoveObject(&metatable->typehash, object);

   if(object->key)
   {
      free(object->key);
      object->key = NULL;
   }
}

//
// MetaGetObject
//
// Returns the first object found in the metatable with the given key, 
// regardless of its type. Returns NULL if no such object exists.
//
metaobject_t *MetaGetObject(metatable_t *metatable, const char *key)
{
   return E_HashObjectForKey(&metatable->keyhash, &key);
}

//
// MetaGetObjectType
//
// Returns the first object found in the metatable which matches the type. 
// Returns NULL if no such object exists.
//
metaobject_t *MetaGetObjectType(metatable_t *metatable, const char *type)
{
   return E_HashObjectForKey(&metatable->typehash, &type);
}

//
// MetaGetObjectKeyAndType
//
// As above, but satisfying both conditions at once.
//
metaobject_t *MetaGetObjectKeyAndType(metatable_t *metatable, const char *key,
                                      const char *type)
{
   metaobject_t *obj = NULL;

   while((obj = E_HashObjectIterator(&metatable->keyhash, obj, &key)))
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
metaobject_t *MetaGetNextObject(metatable_t *metatable, metaobject_t *object,
                                const char *key)
{
   return E_HashObjectIterator(&metatable->keyhash, object, &key);
}

//
// MetaGetNextType
//
// Similar to above, but this returns the next object which also matches
// the specified type.
//
metaobject_t *MetaGetNextType(metatable_t *metatable, metaobject_t *object,
                              const char *type)
{
   return E_HashObjectIterator(&metatable->typehash, object, &type);
}

//
// MetaGetNextKeyAndType
//
// As above, but satisfying both conditions at once.
//
metaobject_t *MetaGetNextKeyAndType(metatable_t *metatable, metaobject_t *object,
                                    const char *key, const char *type)
{
   metaobject_t *obj = object;

   while((obj = E_HashObjectIterator(&metatable->keyhash, obj, &key)))
   {
      if(IsMetaKindOf(obj, type))
         break;
   }

   return obj;
}

//
// MetaTableIterator
//
// Iterates on all objects in the metatable, regardless of key or type.
//
metaobject_t *MetaTableIterator(metatable_t *metatable, metaobject_t *object)
{
   return E_HashTableIterator(&metatable->keyhash, object);
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
// metaIntToString
//
// toString method for metaint objects.
//
static const char *metaIntToString(void *obj)
{
   metaint_t *mi = (metaint_t *)obj;
   static char str[33];

   memset(str, 0, sizeof(str));

   M_Itoa(mi->value, str, 10);

   return str;
}

//
// MetaAddInt
//
// Add an integer to the metatable.
//
void MetaAddInt(metatable_t *metatable, const char *key, int value)
{
   static metatype_t metaIntType;
   metaint_t *newInt = calloc(1, sizeof(metaint_t));

   if(!metaIntType.isinit)
   {
      // register metaint type
      MetaRegisterTypeEx(&metaIntType, METATYPE(metaint_t), sizeof(metaint_t),
                         NULL, NULL, NULL, metaIntToString);
   }

   newInt->value = value;
   MetaAddObject(metatable, key, &newInt->parent, newInt, METATYPE(metaint_t));
}

//
// MetaGetInt
//
// Get an integer from the metatable. This routine returns the value
// rather than a pointer to a metaint_t. If an object of the requested
// name doesn't exist in the table, defvalue is returned and metaerrno 
// is set to indicate the problem.
//
// Use of this routine only returns the first such value in the table.
// This routine is meant for singleton fields.
//
int MetaGetInt(metatable_t *metatable, const char *key, int defvalue)
{
   int retval;
   metaobject_t *obj;

   metaerrno = META_ERR_NOERR;

   if(!(obj = MetaGetObjectKeyAndType(metatable, key, METATYPE(metaint_t))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      retval = defvalue;
   }
   else
      retval = ((metaint_t *)obj->object)->value;

   return retval;
}

//
// MetaSetInt
//
// If the metatable already contains a metaint of the given name, it will
// be edited to have the provided value. Otherwise, a new metaint will be
// added to the table with that value.
//
void MetaSetInt(metatable_t *metatable, const char *key, int newvalue)
{
   metaobject_t *obj;

   if(!(obj = MetaGetObjectKeyAndType(metatable, key, METATYPE(metaint_t))))
      MetaAddInt(metatable, key, newvalue);
   else
   {
      metaint_t *md = (metaint_t *)(obj->object);

      md->value = newvalue;
   }
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
int MetaRemoveInt(metatable_t *metatable, const char *key)
{
   metaobject_t *obj;
   int value;

   metaerrno = META_ERR_NOERR;

   if(!(obj = MetaGetObjectKeyAndType(metatable, key, METATYPE(metaint_t))))
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
// Double
//

//
// metaDoubleToString
//
// toString method for metadouble objects.
//
static const char *metaDoubleToString(void *obj)
{
   static char str[64];
   metadouble_t *md = (metadouble_t *)obj;

   memset(str, 0, sizeof(str));
   psnprintf(str, sizeof(str), "%+.5f", md->value);

   return str;
}

//
// MetaAddDouble
//
// Add a double-precision float to the metatable.
//
void MetaAddDouble(metatable_t *metatable, const char *key, double value)
{
   static metatype_t metaDoubleType;
   metadouble_t *newDouble = calloc(1, sizeof(metadouble_t));

   if(!metaDoubleType.isinit)
   {
      // register metadouble type
      MetaRegisterTypeEx(&metaDoubleType, 
                         METATYPE(metadouble_t), sizeof(metadouble_t),
                         NULL, NULL, NULL, metaDoubleToString);
   }

   newDouble->value = value;
   MetaAddObject(metatable, key, &newDouble->parent, newDouble, METATYPE(metadouble_t));
}

//
// MetaGetDouble
//
// Get a double from the metatable. This routine returns the value
// rather than a pointer to a metadouble_t. If an object of the requested
// name doesn't exist in the table, defvalue is returned and metaerrno is set
// to indicate the problem.
//
// Use of this routine only returns the first such value in the table.
// This routine is meant for singleton fields.
//
double MetaGetDouble(metatable_t *metatable, const char *key, double defvalue)
{
   double retval;
   metaobject_t *obj;

   metaerrno = META_ERR_NOERR;

   if(!(obj = MetaGetObjectKeyAndType(metatable, key, METATYPE(metadouble_t))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      retval = defvalue;
   }
   else
      retval = ((metadouble_t *)obj->object)->value;

   return retval;
}

//
// MetaSetDouble
//
// If the metatable already contains a metadouble of the given name, it will
// be edited to have the provided value. Otherwise, a new metadouble will be
// added to the table with that value.
//
void MetaSetDouble(metatable_t *metatable, const char *key, double newvalue)
{
   metaobject_t *obj;

   if(!(obj = MetaGetObjectKeyAndType(metatable, key, METATYPE(metadouble_t))))
      MetaAddDouble(metatable, key, newvalue);
   else
   {
      metadouble_t *md = (metadouble_t *)(obj->object);

      md->value = newvalue;
   }
}

//
// MetaRemoveDouble
//
// Removes the given field if it exists as a metadouble_t.
// Only one object will be removed. If more than one such object 
// exists, you would need to call this routine until metaerrno is
// set to META_ERR_NOSUCHOBJECT.
//
// The value of the object is returned in case it is needed.
//
double MetaRemoveDouble(metatable_t *metatable, const char *key)
{
   metaobject_t *obj;
   double value;

   metaerrno = META_ERR_NOERR;

   if(!(obj = MetaGetObjectKeyAndType(metatable, key, METATYPE(metadouble_t))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      return 0.0;
   }

   MetaRemoveObject(metatable, obj);

   value = ((metadouble_t *)obj->object)->value;

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
// If the metatable has been copied, more than one object may point to the
// string in question.
//

//
// metaStringToString
//
// toString method for metastrings
//
static const char *metaStringToString(void *obj)
{
   return ((metastring_t *)obj)->value;
}

//
// MetaAddString
//
void MetaAddString(metatable_t *metatable, const char *key, const char *value)
{
   static metatype_t metaStrType;
   metastring_t *newString = calloc(1, sizeof(metastring_t));

   if(!metaStrType.isinit)
   {
      // register metastring type
      MetaRegisterTypeEx(&metaStrType, 
                         METATYPE(metastring_t), sizeof(metastring_t), 
                         NULL, NULL, NULL, metaStringToString);
   }

   newString->value = value;

   MetaAddObject(metatable, key, &newString->parent, newString, 
                 METATYPE(metastring_t));
}

//
// MetaGetString
//
// Get a string from the metatable. This routine returns the value
// rather than a pointer to a metastring_t. If an object of the requested
// name doesn't exist in the table, defvalue is returned and metaerrno is set
// to indicate the problem.
//
// Use of this routine only returns the first such value in the table.
// This routine is meant for singleton fields.
//
const char *MetaGetString(metatable_t *metatable, const char *key, const char *defvalue)
{
   const char *retval;
   metaobject_t *obj;

   metaerrno = META_ERR_NOERR;

   if(!(obj = MetaGetObjectKeyAndType(metatable, key, METATYPE(metastring_t))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      retval = defvalue;
   }
   else
      retval = ((metastring_t *)obj->object)->value;

   return retval;
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
const char *MetaRemoveString(metatable_t *metatable, const char *key)
{
   metaobject_t *obj;
   const char *value;

   metaerrno = META_ERR_NOERR;

   if(!(obj = MetaGetObjectKeyAndType(metatable, key, METATYPE(metastring_t))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      return NULL;
   }

   MetaRemoveObject(metatable, obj);

   value = ((metastring_t *)obj->object)->value;

   free(obj->object);

   return value;
}

//=============================================================================
//
// Metatypes
//
// The metatype registry is used to support operations such as copying of
// items from one metatable to another, where additional information is needed.
// Only items with a registered metatype can be copied from one table to
// another.
//

// The metatypes registry. This is itself a metatable.
static metatable_t metaTypeRegistry;

//
// MetaAlloc
//
// Default method for allocation of an object given its metatype.
//
static void *MetaAlloc(size_t size)
{
   return calloc(1, size);
}

//
// MetaCopy
//
// Default method for copying of an object given its metatype.
// Performs a shallow copy only.
//
static void MetaCopy(void *dest, const void *src, size_t size)
{
   memcpy(dest, src, size);
}

//
// MetaObjectPtr
//
// Default method to get the metaobject_t field for an object.
// Returns the same pointer. Works for objects which have the
// metaobject_t as their first item only.
//
static metaobject_t *MetaObjectPtr(void *object)
{
   return object;
}

// data for MetaDefToString
static size_t metaobjlen;

//
// MetaDefToString
//
// Default method for string display of a metaobject_t via a registered
// metatype.
//
static const char *MetaDefToString(void *object)
{
   static qstring_t qstr;
   byte *data = (byte *)object;
   size_t bytestoprint = metaobjlen;

   if(!qstr.buffer)
      QStrInitCreate(&qstr);
   else
      QStrClear(&qstr);

   while(bytestoprint)
   {
      int i;

      // print up to 12 bytes on each line
      for(i = 0; i < 12 && bytestoprint; ++i, --bytestoprint)
      {
         byte val = *data++;
         char bytes[4] = { 0 };

         sprintf(bytes, "%02x ", val);

         QStrCat(&qstr, bytes);
      }
      QStrPutc(&qstr, '\n');
   }

   return qstr.buffer;
}

//
// MetaRegisterType
//
// Registers a metatype, but only if that type isn't already registered.
//
void MetaRegisterType(metatype_t *type)
{
   // early check for an already-initialized metatype
   if(type->isinit)
      return;

   // init table the first time
   if(!metaTypeRegistry.keyhash.isinit)
      MetaInit(&metaTypeRegistry);

   // set default methods if any are NULL

   if(!type->alloc)
      type->alloc = MetaAlloc;

   if(!type->copy)
      type->copy = MetaCopy;

   if(!type->objptr)
      type->objptr = MetaObjectPtr;

   if(!type->toString)
      type->toString = MetaDefToString;

   // sanity check: we do not allow multiple metatypes of the same name
   if(MetaGetObject(&metaTypeRegistry, type->name))
      I_Error("MetaRegisterType: type %s is non-singleton\n", type->name);

   MetaAddObject(&metaTypeRegistry, type->name, &type->parent, type, 
                 METATYPE(metatype_t));

   type->isinit = true;
}

//
// MetaRegisterTypeEx
//
// Registers a metatype as above, but takes the information to put into the
// metatype structure as parameters.
//
void MetaRegisterTypeEx(metatype_t *type, const char *typeName, size_t typeSize,
                        MetaAllocFn_t alloc, MetaCopyFn_t copy, 
                        MetaObjPtrFn_t objptr, MetaToStrFn_t tostr)
{
   // early check for already-initialized metatype
   if(type->isinit)
      return;

   type->name     = typeName;
   type->size     = typeSize;
   type->alloc    = alloc;
   type->copy     = copy;
   type->objptr   = objptr;
   type->toString = tostr;

   MetaRegisterType(type);
}

//
// MetaCopyTable
//
// Adds copies of all objects with a registered metatype in the source table to
// the destination table.
//
void MetaCopyTable(metatable_t *desttable, metatable_t *srctable)
{
   metaobject_t *srcobj = NULL;

   // iterate on the source table
   while((srcobj = MetaTableIterator(srctable, srcobj)))
   {
      metatype_t *type;

      // see if the object has a registered metatype
      if((type = (metatype_t *)MetaGetObject(&metaTypeRegistry, srcobj->type)))
      {
         // create the new object
         void *destobj         = type->alloc(type->size);
         metaobject_t *newmeta = type->objptr(destobj);

         // copy from the old object
         type->copy(destobj, srcobj->object, type->size);

         // clear metaobject for safety
         memset(newmeta, 0, sizeof(metaobject_t));

         // add the new object to the destination table
         MetaAddObject(desttable, srcobj->key, newmeta, destobj, type->name);
      }
   }
}

//
// MetaToString
//
// Call this function to convert a metaobject with a registered metatype into
// a printable string.
//
const char *MetaToString(metaobject_t *obj)
{
   const char *ret;
   metatype_t *type;

   // see if the object has a registered metatype
   if((type = (metatype_t *)MetaGetObject(&metaTypeRegistry, obj->type)))
   {
      metaobjlen = type->size; // needed by default method
      ret = type->toString(obj->object);
   }
   else
      ret = "(unregistered object type)";

   return ret;
}

// EOF

