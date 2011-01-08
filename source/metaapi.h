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

//#include "z_zone.h"
#include "e_hashkeys.h"

// A metatypename is just a string constant.
typedef const char *metatypename_t;

// METATYPE macro - make a string from a typename
#define METATYPE(t) #t

// enumeration for metaerrno
enum
{
   META_ERR_NOERR,        // 0 is not an error
   META_ERR_NOSUCHOBJECT,
   META_ERR_NOSUCHTYPE,
   META_NUMERRS
};

extern int metaerrno;

class metaTablePimpl;

//
// MetaObject
//
class MetaObject
{
protected:
   DLListItem<MetaObject> links;     // links by key
   DLListItem<MetaObject> typelinks; // links by type
   EStringHashKey          type;      // type hash key
   ENCStringHashKey        key;       // primary hash key
   
   metatypename_t type_name; // storage pointer for type (static string)
   char *key_name;           // storage pointer for key  (alloc'd string)

   // Protected Methods
   void setType(metatypename_t t) { type_name = t; type = type_name; }

   friend class metaTablePimpl;

public:
   // Constructors/Destructor
   MetaObject(metatypename_t pType, const char *pKey);
   MetaObject(const MetaObject &other);
   virtual ~MetaObject();

   // RTTI Methods
   boolean isKindOf(metatypename_t) const;
   metatypename_t getType() const { return type_name; }
   const char   * getKey()  const { return key_name;  }

   // Virtual Methods
   virtual MetaObject *clone() const;
   virtual const char *toString() const;   

   // Operators
   void *operator new (size_t size);
   void  operator delete (void *p);
};

// MetaObject specializations for basic types

class MetaInteger : public MetaObject
{
protected:
   int value;

public:
   MetaInteger(const char *key, int i);
   MetaInteger(const MetaInteger &other);

   // Virtual Methods
   virtual MetaObject *clone() const;
   virtual const char *toString() const; 

   // Accessors
   int getValue() const { return value; }
   void setValue(int i) { value = i;    }

   friend class MetaTable;
};

class MetaDouble : public MetaObject
{
protected:
   double value;

public:
   MetaDouble(const char *key, double d);
   MetaDouble(const MetaDouble &other);

   // Virtual Methods
   virtual MetaObject *clone() const;
   virtual const char *toString() const;

   // Accessors
   double getValue() const { return value; }
   void setValue(double d) { value = d;    }

   friend class MetaTable;
};

class MetaString : public MetaObject
{
protected:
   char *value;

public:
   MetaString(const char *key, const char *s);
   MetaString(const MetaString &other);
   virtual ~MetaString();

   // Virtual Methods
   virtual MetaObject *clone() const;
   virtual const char *toString() const;

   // Accessors
   const char *getValue() const { return value; }
   void setValue(const char *s, char **ret = NULL);

   friend class MetaTable;
};

// MetaTable

class MetaTable
{
private:
   metaTablePimpl *pImpl;

public:
   MetaTable();

   // Search functions. Frankly, it's more efficient to just use the "get" routines :P
   boolean hasKey(const char *key);
   boolean hasType(metatypename_t type);
   boolean hasKeyAndType(const char *key, metatypename_t type);

   // Count functions.
   int countOfKey(const char *key);
   int countOfType(metatypename_t type);
   int countOfKeyAndType(const char *key, metatypename_t type);

   // Add/Remove Objects
   void addObject(MetaObject *object);
   void addObject(MetaObject &object);
   void removeObject(MetaObject *object);
   void removeObject(MetaObject &object);

   // Find objects in the table:
   // * By Key
   MetaObject *getObject(const char *key);
   // * By Type
   MetaObject *getObjectType(metatypename_t type);
   // * By Key AND Type
   MetaObject *getObjectKeyAndType(const char *key, metatypename_t type);

   // Iterators
   MetaObject *getNextObject(MetaObject *object, const char *key);
   MetaObject *getNextType(MetaObject *object, metatypename_t type);
   MetaObject *getNextKeyAndType(MetaObject *object, const char *key, metatypename_t type);
   MetaObject *tableIterator(MetaObject *object);

   // Add/Get/Set Convenience Methods for Basic MetaObjects
   
   // Signed integer
   void addInt(const char *key, int value);
   int  getInt(const char *key, int defValue);
   void setInt(const char *key, int newValue);
   int  removeInt(const char *key);

   // Double floating-point
   void   addDouble(const char *key, double value);
   double getDouble(const char *key, double defValue);
   void   setDouble(const char *key, double newValue);
   double removeDouble(const char *key);

   // Managed strings
   void        addString(const char *key, const char *value);
   const char *getString(const char *key, const char *defValue);
   void        setString(const char *key, const char *newValue);
   char       *removeString(const char *key);
   void        removeStringNR(const char *key);

   // Copy routine - clones the entire MetaTable
   void copyTableTo(MetaTable *dest);
   void copyTableFrom(MetaTable *source);

   // Operators
   void *operator new (size_t size);
   void  operator delete (void *p);
};

#endif

// EOF

