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

typedef struct metaobject_s
{
   mdllistitem_t links;
   const char *key;
   const char *type;
   void *object;
} metaobject_t;

typedef struct metaint_s
{
   metaobject_t parent;
   int value;
} metaint_t;

typedef struct metastring_s
{
   metaobject_t parent;
   const char *value;
} metastring_t;

typedef struct metatype_s
{
   metaobject_t parent;
   const char *name;
   size_t size;
   void *(*alloc)(size_t);
   void  (*copy)(void *, const void *, size_t);
   metaobject_t *(*objptr)(void *);
} metatype_t;

void    MetaInit(ehash_t *metatable);
boolean IsMetaKindOf(metaobject_t *object, const char *type);
boolean MetaHas(ehash_t *metatable, const char *key);
boolean MetaHasType(ehash_t *metatable, const char *key, const char *type);
int     MetaCountOf(ehash_t *metatable, const char *key);
int     MetaCountOfType(ehash_t *metatable, const char *key, const char *type);

void MetaAddObject(ehash_t *metatable, const char *key, metaobject_t *object, 
                   void *data, const char *type);
void MetaRemoveObject(ehash_t *metatable, metaobject_t *object);

metaobject_t *MetaGetObject(ehash_t *metatable, const char *key);
metaobject_t *MetaGetObjectType(ehash_t *metatable, const char *key, 
                                const char *type);
metaobject_t *MetaGetNextObject(ehash_t *metatable, metaobject_t *object, 
                                const char *key);
metaobject_t *MetaGetNextType(ehash_t *metatable, metaobject_t *object, 
                              const char *key, const char *type);

void MetaAddInt(ehash_t *metatable, const char *key, int value);
int  MetaGetInt(ehash_t *metatable, const char *key);
int  MetaRemoveInt(ehash_t *metatable, const char *key);

void        MetaAddString(ehash_t *metatable, const char *key, const char *value);
const char *MetaGetString(ehash_t *metatable, const char *key);
const char *MetaRemoveString(ehash_t *metatable, const char *key);

#endif

// EOF

