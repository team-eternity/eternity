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

#ifndef METAAPI_H__
#define METAAPI_H__

#include "z_zone.h"
#include "e_hash.h"

typedef const char *metatypename_t;

#define METATYPE(type) #type

// enumeration for metaerrno
enum
{
   META_ERR_NOERR,        // 0 is not an error
   META_ERR_NOSUCHOBJECT,
   META_ERR_NOSUCHTYPE,
   META_NUMERRS
};

extern int metaerrno;

// metatable

typedef struct metatable_s
{
   ehash_t keyhash;  // hash of objects by key
   ehash_t typehash; // hash of objects by type
} metatable_t;

// metaobject

typedef struct metaobject_s
{
   mdllistitem_t  links;
   mdllistitem_t  typelinks;
   metatypename_t type;
   char *key;   
   void *object;
} metaobject_t;

// metaobject specializations for basic types

typedef struct metaint_s
{
   metaobject_t parent;
   int value;
} metaint_t;

typedef struct metadouble_s
{
   metaobject_t parent;
   double value;
} metadouble_t;

typedef struct metastring_s
{
   metaobject_t parent;
   char *value;
} metastring_t;

// metatypes

typedef struct metatype_s *mtptr;

// metatype method prototypes
typedef void         *(*MetaAllocFn_t) (mtptr);
typedef void          (*MetaCopyFn_t)  (mtptr, void *, const void *);
typedef metaobject_t *(*MetaObjPtrFn_t)(mtptr, void *);
typedef const char   *(*MetaToStrFn_t) (mtptr, void *);

// interface object for metatypes
typedef struct metatype_i_s
{
   MetaAllocFn_t  alloc;    // allocation method
   MetaCopyFn_t   copy;     // copy method
   MetaObjPtrFn_t objptr;   // object pointer method (returns metaobject)
   MetaToStrFn_t  toString; // string conversion method
} metatype_i;

typedef struct metatype_s
{
   metaobject_t   parent;   // metatypes are also metaobjects
   metatypename_t name;     // name of metatype (derived from C type)
   size_t         size;     // size of type for allocation purposes
   boolean        isinit;   // if true, this type has been registered
   metatype_i     methods;  // methods for this metatype

   struct metatype_s *super; // superclass type (NULL if none)
} metatype_t;

// global functions

// Initialize a metatable
void    MetaInit(metatable_t *metatable);

// RTTI for metaobjects
boolean IsMetaKindOf(metaobject_t *object, metatypename_t type);

// Search functions. Frankly, it's more efficient to just use the MetaGet routines :P
boolean MetaHasKey(metatable_t *metatable, const char *key);
boolean MetaHasType(metatable_t *metatable, metatypename_t type);
boolean MetaHasKeyAndType(metatable_t *metatable, const char *key, metatypename_t type);

// Count functions.
int MetaCountOfKey(metatable_t *metatable, const char *key);
int MetaCountOfType(metatable_t *metatable, metatypename_t type);
int MetaCountOfKeyAndType(metatable_t *metatable, const char *key, metatypename_t type);

// Add and remove objects
void MetaAddObject(metatable_t *metatable, const char *key, metaobject_t *object, 
                   void *data, metatypename_t type);
void MetaRemoveObject(metatable_t *metatable, metaobject_t *object);

// Find objects in the table by key, by type, or by key AND type.
metaobject_t *MetaGetObject(metatable_t *metatable, const char *key);
metaobject_t *MetaGetObjectType(metatable_t *metatable, metatypename_t type);
metaobject_t *MetaGetObjectKeyAndType(metatable_t *metatable, const char *key,
                                      metatypename_t type);

// Iterator functions
metaobject_t *MetaGetNextObject(metatable_t *metatable, metaobject_t *object, 
                                const char *key);
metaobject_t *MetaGetNextType(metatable_t *metatable, metaobject_t *object, 
                              metatypename_t type);
metaobject_t *MetaGetNextKeyAndType(metatable_t *metatable, metaobject_t *object,
                                    const char *key, metatypename_t type);
metaobject_t *MetaTableIterator(metatable_t *metatable, metaobject_t *object);

//
// Metaobject specializations for basic C types
//

// Signed integer
void MetaAddInt(metatable_t *metatable, const char *key, int value);
int  MetaGetInt(metatable_t *metatable, const char *key, int defvalue);
void MetaSetInt(metatable_t *metatable, const char *key, int newvalue);
int  MetaRemoveInt(metatable_t *metatable, const char *key);

// Double floating-point
void   MetaAddDouble(metatable_t *metatable, const char *key, double value);
double MetaGetDouble(metatable_t *metatable, const char *key, double defvalue);
void   MetaSetDouble(metatable_t *metatable, const char *key, double newvalue);
double MetaRemoveDouble(metatable_t *metatable, const char *key);

// Managed strings
void        MetaAddString(metatable_t *metatable, const char *key, const char *value);
const char *MetaGetString(metatable_t *metatable, const char *key, const char *defvalue);
char       *MetaRemoveString(metatable_t *metatable, const char *key);
void        MetaRemoveStringNR(metatable_t *metatable, const char *key);

//
// Metatypes
//
// Metatypes are like the classes for metaobjects. Pass the name, size, and optional
// method overrides for a particular metaobject type into here and it will be 
// recorded for use with the below metatype methods.
//

void MetaRegisterType(metatype_t *type);
void MetaRegisterTypeEx(metatype_t *type, metatypename_t typeName, size_t typeSize,
                        metatypename_t inheritsFrom, metatype_i *mInterface);
void MetaTypeInheritFrom(metatype_t *parent, metatype_t *child);

//
// Metatype Methods
//

// MetaCopyTable will copy all the objects in the table that have a registered
// metatype (the metatype's name must match the METATYPE() of the metaobject).
// Most objects interested in supporting copying through this method will need
// to define a MetaCopyFn method if they contain any sub-allocations.
void MetaCopyTable(metatable_t *desttable, metatable_t *srctable);

// Convert a metaobject to a string representation using its MetaToStrFn method.
// Strings returned by this function point to temporary buffers and shouldn't be
// cached. Metaobjects with a registered type but no custom toString method will
// return a hex dump of the object courtesy of the default implementation. Objects
// without metatypes will only appear as "(unregistered object type)".
const char *MetaToString(metaobject_t *obj);

#endif

// EOF

