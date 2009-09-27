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

#ifndef METAAPI_H__
#define METAAPI_H__

#include "z_zone.h"
#include "e_hash.h"

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

typedef struct metatable_s
{
   ehash_t keyhash;  // hash of objects by key
   ehash_t typehash; // hash of objects by type
} metatable_t;

typedef struct metaobject_s
{
   mdllistitem_t links;
   mdllistitem_t typelinks;
   char *key;
   const char *type;
   void *object;
} metaobject_t;

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
   const char *value;
} metastring_t;

typedef void         *(*MetaAllocFn_t) (size_t);
typedef void          (*MetaCopyFn_t)  (void *, const void *, size_t);
typedef metaobject_t *(*MetaObjPtrFn_t)(void *);
typedef const char   *(*MetaToStrFn_t) (void *);

typedef struct metatype_s
{
   metaobject_t parent;
   const char *name;
   size_t size;
   MetaAllocFn_t  alloc;
   MetaCopyFn_t   copy;
   MetaObjPtrFn_t objptr;
   MetaToStrFn_t  toString;
} metatype_t;

void    MetaInit(metatable_t *metatable);
boolean IsMetaKindOf(metaobject_t *object, const char *type);

boolean MetaHasKey(metatable_t *metatable, const char *key);
boolean MetaHasType(metatable_t *metatable, const char *type);
boolean MetaHasKeyAndType(metatable_t *metatable, const char *key, const char *type);

int MetaCountOfKey(metatable_t *metatable, const char *key);
int MetaCountOfType(metatable_t *metatable, const char *type);
int MetaCountOfKeyAndType(metatable_t *metatable, const char *key, const char *type);

void MetaAddObject(metatable_t *metatable, const char *key, metaobject_t *object, 
                   void *data, const char *type);
void MetaRemoveObject(metatable_t *metatable, metaobject_t *object);

metaobject_t *MetaGetObject(metatable_t *metatable, const char *key);
metaobject_t *MetaGetObjectType(metatable_t *metatable, const char *type);
metaobject_t *MetaGetObjectKeyAndType(metatable_t *metatable, const char *key,
                                      const char *type);

metaobject_t *MetaGetNextObject(metatable_t *metatable, metaobject_t *object, 
                                const char *key);
metaobject_t *MetaGetNextType(metatable_t *metatable, metaobject_t *object, 
                              const char *type);
metaobject_t *MetaGetNextKeyAndType(metatable_t *metatable, metaobject_t *object,
                                    const char *key, const char *type);

void MetaAddInt(metatable_t *metatable, const char *key, int value);
int  MetaGetInt(metatable_t *metatable, const char *key, int defvalue);
int  MetaRemoveInt(metatable_t *metatable, const char *key);

void   MetaAddDouble(metatable_t *metatable, const char *key, double value);
double MetaGetDouble(metatable_t *metatable, const char *key, double defvalue);
double MetaRemoveDouble(metatable_t *metatable, const char *key);

void        MetaAddString(metatable_t *metatable, const char *key, const char *value);
const char *MetaGetString(metatable_t *metatable, const char *key, const char *defvalue);
const char *MetaRemoveString(metatable_t *metatable, const char *key);

void MetaRegisterType(metatype_t *type);
void MetaRegisterTypeEx(metatype_t *type, const char *typeName, size_t typeSize,
                        MetaAllocFn_t alloc, MetaCopyFn_t copy, 
                        MetaObjPtrFn_t objptr, MetaToStrFn_t tostr);
void MetaCopyTable(metatable_t *desttable, metatable_t *srctable);

const char *MetaToString(metaobject_t *obj);

metaobject_t *MetaTableIterator(metatable_t *metatable, metaobject_t *object, 
                                unsigned int *index);
#endif

// EOF

