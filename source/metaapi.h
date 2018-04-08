// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
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

#include "e_rtti.h"
#include "m_dllist.h"
#include "m_vector.h"   // IOANCH

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

class MetaTablePimpl;
class InBuffer;      // IOANCH
class OutBuffer;     // IOANCH

//
// MetaObject
//
class MetaObject : public RTTIObject
{
   DECLARE_RTTI_TYPE(MetaObject, RTTIObject)

protected:
   DLListItem<MetaObject> links;     // links by key
   DLListItem<MetaObject> typelinks; // links by type
   const char *key;                  // primary hash key
   const char *type;                 // type hash key
   size_t      keyIdx;               // index of interned key

   // For efficiency, we are friends with the private implementation
   // object for MetaTable. It needs direct access to the links and
   // typelinks for use with EHashTable.
   friend class MetaTablePimpl;
   
   //! Virtual for reading from file. NOTE: type is considered as known already.
   //! Start reading from key!
   virtual bool readFromFile(InBuffer& inbuf);

public:
   // Constructors/Destructor
   MetaObject();
   explicit MetaObject(size_t keyIndex);
   explicit MetaObject(const char *pKey);
   MetaObject(const MetaObject &other)
      : Super(), links(), typelinks(), key(other.key),
        type(NULL), keyIdx(other.keyIdx)
   {
   }

   virtual ~MetaObject() {}

   //
   // MetaObject::setType
   //
   // This will set the MetaObject's internal type to its RTTI class name. This
   // is really only for use by MetaTable but calling it yourself wouldn't screw
   // anything up. It's just redundant.
   //
   void setType() { type = getClassName(); }

   const char *getKey()    const { return key;    }
   size_t      getKeyIdx() const { return keyIdx; }

   // Virtual Methods

   //
   // MetaObject::clone
   //
   // Virtual factory method for metaobjects; when invoked through the metatable,
   // a descendent class will return an object of the proper type matching itself.
   // This base class implementation doesn't really do anything, but I'm not fond
   // of pure virtuals so here it is. Don't call it from the parent implementation
   // as that is not what any of the implementations should do.
   //
   virtual MetaObject *clone()    const { return new MetaObject(*this); }
   virtual const char *toString() const;

   // IOANCH 20150514: serialization.
   virtual bool writeToFile(OutBuffer& outbuf) const;
   
   //! Factory for reading InBuffer file (deserialization). This is the one
   //! supposed to be used.
   static MetaObject* createFromFile(InBuffer& inbuf);
   
};

// MetaObject specializations for basic types

//
// MetaInteger
//
// Wrap a simple int value.
//
class MetaInteger : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaInteger, MetaObject)

protected:
   int value;
   virtual bool readFromFile(InBuffer& inbuf);

public:
   MetaInteger() : Super(), value(0) {}
   MetaInteger(size_t keyIndex, int i)
      : Super(keyIndex), value(i)
   {
   }
   MetaInteger(const char *key, int i) 
      : Super(key), value(i)
   {
   }
   MetaInteger(const MetaInteger &other)
      : Super(other), value(other.value)
   {
   }

   // Virtual Methods
   virtual MetaObject *clone()    const override { return new MetaInteger(*this); }
   virtual const char *toString() const override;

   // Accessors
   int getValue() const { return value; }
   void setValue(int i) { value = i;    }

   friend class MetaTable;

   virtual bool writeToFile(OutBuffer& outbuf) const;
};

//
// MetaV2Fixed
//
// IOANCH 20130815:
// Wrap XY map coordinates.
//
class MetaV2Fixed : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaV2Fixed, MetaObject)
   
protected:
   v2fixed_t value;
   virtual bool readFromFile(InBuffer& inbuf);
   
public:
   MetaV2Fixed() : Super()
   {
      value.x = value.y = 0;
   }
   MetaV2Fixed(size_t keyIndex, v2fixed_t i)
   : Super(keyIndex), value(i)
   {
   }
   MetaV2Fixed(const char *key, v2fixed_t i)
   : Super(key), value(i)
   {
   }
   MetaV2Fixed(const MetaV2Fixed &other)
   : Super(other), value(other.value)
   {
   }
   
   // Virtual Methods
   virtual MetaObject *clone()    const { return new MetaV2Fixed(*this); }
   virtual const char *toString() const;
   
   // Accessors
   v2fixed_t getValue() const { return value; }
   void setValue(v2fixed_t i) { value = i;    }
   
   friend class MetaTable;

   virtual bool writeToFile(OutBuffer& outbuf) const;
};

//
// MetaDouble
//
// Wrap a simple double floating-point value.
//
class MetaDouble : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaDouble, MetaObject)

protected:
   double value;
   virtual bool readFromFile(InBuffer& inbuf);

public:
   MetaDouble() : Super(), value(0.0) {}
   MetaDouble(const char *key, double d)
      : Super(key), value(d)
   {
   }
   MetaDouble(const MetaDouble &other)
      : Super(other), value(other.value)
   {
   }

   // Virtual Methods
   virtual MetaObject *clone()    const override { return new MetaDouble(*this); }
   virtual const char *toString() const override;

   // Accessors
   double getValue() const { return value; }
   void setValue(double d) { value = d;    }

   friend class MetaTable;

   virtual bool writeToFile(OutBuffer& outbuf) const;
};

//
// MetaString
//
// Wrap a dynamically allocated string value. The string is owned by the
// MetaObject.
//
class MetaString : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaString, MetaObject)

protected:
   char *value;
   virtual bool readFromFile(InBuffer& inbuf);

public:
   MetaString() : Super(), value(estrdup("")) {}
   MetaString(const char *key, const char *s)
      : Super(key), value(estrdup(s))
   {
   }
   MetaString(const MetaString &other)
      : Super(other), value(estrdup(other.value))
   {
   }
   virtual ~MetaString()
   {
      if(value)
         efree(value);
      value = NULL;
   }

   // Virtual Methods
   virtual MetaObject *clone()    const override { return new MetaString(*this); }
   virtual const char *toString() const override { return value; }

   // Accessors
   const char *getValue() const { return value; }
   virtual void setValue(const char *s, char **ret = NULL);

   friend class MetaTable;

   virtual bool writeToFile(OutBuffer& outbuf) const;
};

//
// MetaVariant
//
// Stored as a string value, this MetaObject type supports cached interpretation
// into various types.
//
class MetaVariant : public MetaString
{
   DECLARE_RTTI_TYPE(MetaVariant, MetaString)

public:
   enum varianttype_e
   {
      VARIANT_NONE,   // not yet interpreted
      VARIANT_INT,    // integer
      VARIANT_BOOL,   // boolean
      VARIANT_FLOAT,  // float
      VARIANT_DOUBLE, // double

      VARIANT_MAX
   };

protected:
   varianttype_e cachedType;

   union variantcache_u
   {
      int    i;
      bool   b;
      float  f;
      double d;
   } cachedValue;

public:
   MetaVariant() : Super(), cachedType(VARIANT_NONE) 
   {
      cachedValue.i = 0;
   }
   MetaVariant(const char *key, const char *s) 
      : Super(key, s), cachedType(VARIANT_NONE)
   {
      cachedValue.i = 0;
   }
   MetaVariant(const MetaVariant &other);

   // Virtual Methods
   virtual MetaObject *clone() const override { return new MetaVariant(*this); }
   virtual void setValue(const char *s, char **ret = NULL) override;

   // Accessors
   int    getInt();
   bool   getBool();
   float  getFloat();
   double getDouble();

   varianttype_e getCachedType() const { return cachedType;}
};

//
// MetaConstString
//
// Wrap a string constant/literal value. The string is *not* owned by the
// MetaObject and can be shared between multiple MetaTable properties.
//
class MetaConstString : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaConstString, MetaObject)

protected:
   const char *value;
   virtual bool readFromFile(InBuffer& inbuf);

public:
   MetaConstString() : Super(), value(NULL) {}
   MetaConstString(size_t keyIndex, const char *s)
      : Super(keyIndex), value(s)
   {
   }
   MetaConstString(const char *key, const char *s)
      : Super(key), value(s)
   {
   }
   MetaConstString(const MetaConstString &other)
      : Super(other), value(other.value)
   {
   }
   virtual ~MetaConstString() {}

   // Virtual Methods
   virtual MetaObject *clone()    const override { return new MetaConstString(*this); }
   virtual const char *toString() const override { return value; }

   // Accessors
   const char *getValue() const { return value; }
   void setValue(const char *s) { value = s;    }

   friend class MetaTable;

   virtual bool writeToFile(OutBuffer& outbuf) const;
};

//
// MetaTable
//
// The MetaTable is a MetaObject which may itself manage other MetaObjects in
// the form of an associative array (or hash table, in other words) by key and
// by type. It is more or less exactly equivalent in functionality to an object
// in the JavaScript language.
//
class MetaTable : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaTable, MetaObject)

private:
   // Private implementation details are in metaapi.cpp
   MetaTablePimpl *pImpl;
   virtual bool readFromFile(InBuffer& inbuf);

public:
   MetaTable();
   explicit MetaTable(const char *name);
   MetaTable(const MetaTable &other);
   virtual ~MetaTable();

   // MetaObject overrides
   virtual MetaObject *clone() const override;
   virtual const char *toString() const override;

   // EHashTable API exposures
   float        getLoadFactor() const; // returns load factor of the key hash table
   unsigned int getNumItems()   const; // returns number of items in the table

   // Search functions. Frankly, it's more efficient to just use the "get" routines :P
   bool hasKey(const char *key) const;
   bool hasType(const char *type) const;
   bool hasKeyAndType(const char *key, const char *type) const;

   // Count functions.
   int countOfKey(const char *key) const;
   int countOfType(const char *type) const;
   int countOfKeyAndType(const char *key, const char *type) const;

   // Add/Remove Objects
   void addObject(MetaObject *object);
   void addObject(MetaObject &object);
   void removeObject(MetaObject *object);
   void removeObject(MetaObject &object);

   // Find objects in the table:
   // * By Key
   MetaObject *getObject(const char *key) const;
   MetaObject *getObject(size_t keyIndex) const;
   // * By Type
   MetaObject *getObjectType(const char *type) const;
   MetaObject *getObjectType(const MetaObject::Type &type) const;
   // * By Key AND Type
   MetaObject *getObjectKeyAndType(const char *key, const MetaObject::Type *type) const;
   MetaObject *getObjectKeyAndType(const char *key, const char *type) const;
   MetaObject *getObjectKeyAndType(size_t keyIndex, const MetaObject::Type *type) const;
   MetaObject *getObjectKeyAndType(size_t keyIndex, const char *type) const;

   // Template finders
   template<typename M> M *getObjectTypeEx() const
   {
      return static_cast<M *>(getObjectType(M::StaticType));
   }

   template<typename M> M *getObjectKeyAndTypeEx(const char *key) const
   {
      return static_cast<M *>(getObjectKeyAndType(key, RTTI(M)));
   }

   template<typename M> M *getObjectKeyAndTypeEx(size_t keyIndex) const
   {
      return static_cast<M *>(getObjectKeyAndType(keyIndex, RTTI(M)));
   }

   // Iterators
   // * By Key
   MetaObject *getNextObject(MetaObject *object, const char *key) const;
   MetaObject *getNextObject(MetaObject *object, size_t keyIndex) const;
   // * By Type
   MetaObject *getNextType(MetaObject *object, const char *type) const;
   MetaObject *getNextType(MetaObject *object, const MetaObject::Type *type) const;
   // * By Key AND Type
   MetaObject *getNextKeyAndType(MetaObject *object, const char *key, const char *type) const;
   MetaObject *getNextKeyAndType(MetaObject *object, size_t keyIdx,   const char *type) const;
   MetaObject *getNextKeyAndType(MetaObject *object, const char *key, const MetaObject::Type *type) const;
   MetaObject *getNextKeyAndType(MetaObject *object, size_t keyIdx,   const MetaObject::Type *type) const;
   // * Full table iterators
   MetaObject *tableIterator(MetaObject *object) const;
   const MetaObject *tableIterator(const MetaObject *object) const;

   // Template iterators
   template<typename M> M *getNextTypeEx(M *object) const
   {
      return static_cast<M *>(getNextType(object, RTTI(M)));
   }

   template<typename M> M *getNextKeyAndTypeEx(M *object, const char *key) const
   {
      return static_cast<M *>(getNextKeyAndType(object, key, RTTI(M)));
   }

   template<typename M> M *getNextKeyAndTypeEx(M *object, size_t keyIdx) const
   {
      return static_cast<M *>(getNextKeyAndType(object, keyIdx, RTTI(M)));
   }

   // Add/Get/Set Convenience Methods for Basic MetaObjects
   
   // Signed integer
   void addInt(size_t keyIndex, int value);
   void addInt(const char *key, int value);
   int  getInt(size_t keyIndex, int defValue) const;
   int  getInt(const char *key, int defValue) const;
   void setInt(size_t keyIndex, int newValue);
   void setInt(const char *key, int newValue);
   int  removeInt(const char *key);
   
   // IOANCH: XY fixed vector
   void addV2Fixed(const char *key, v2fixed_t value);
   v2fixed_t getV2Fixed(const char *key, v2fixed_t defValue);
   void setV2Fixed(const char *key, v2fixed_t newValue);
   v2fixed_t removeV2Fixed(const char *key);

   // Double floating-point
   void   addDouble(const char *key, double value);
   double getDouble(const char *key, double defValue) const;
   void   setDouble(const char *key, double newValue);
   double removeDouble(const char *key);

   // Managed strings
   void        addString(const char *key, const char *value);
   const char *getString(size_t  keyIndex, const char *defValue) const;
   const char *getString(const char *key, const char *defValue) const;
   void        setString(const char *key, const char *newValue);
   char       *removeString(const char *key);
   void        removeStringNR(const char *key);

   // Constant shared strings
   void        addConstString(size_t keyIndex, const char *value);
   void        addConstString(const char *key, const char *value);
   const char *getConstString(const char *key, const char *defValue) const;
   void        setConstString(size_t keyIndex, const char *newValue);
   void        setConstString(const char *key, const char *newValue);
   const char *removeConstString(const char *key);

   // Nested MetaTable
   void       addMetaTable(size_t keyIndex, MetaTable *value);
   void       addMetaTable(const char *key, MetaTable *newValue);
   MetaTable *getMetaTable(size_t keyIndex, MetaTable *defValue) const;
   MetaTable *getMetaTable(const char *key, MetaTable *defValue) const;
   void       setMetaTable(size_t keyIndex, MetaTable *value);
   void       setMetaTable(const char *key, MetaTable *value);

   // Copy routine - clones the entire MetaTable
   void copyTableTo(MetaTable *dest) const;
   void copyTableFrom(const MetaTable *source);

   // Clearing
   void clearTable();

   // Statics
   static size_t IndexForKey(const char *key);

   virtual bool writeToFile(OutBuffer& outbuf) const;
};

//
// MetaKeyIndex
//
// This class can be used as a static key proxy. The first time it is used, it
// will intern the provided key in the MetaObject key interning table. From
// then on, it will return the cached index.
//
class MetaKeyIndex
{
protected:
   const char *key;
   size_t keyIndex;
   bool   haveIndex;

public:
   explicit MetaKeyIndex(const char *pKey) : key(pKey), keyIndex(0), haveIndex(false) 
   {
   }

   size_t getIndex() 
   { 
      if(!haveIndex)
      {
         keyIndex  = MetaTable::IndexForKey(key);
         haveIndex = true;
      }
      return keyIndex;
   }

   operator size_t () { return getIndex(); }
};

#endif

// EOF

