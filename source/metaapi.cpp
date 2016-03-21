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

#include "z_zone.h"
#include "i_system.h"
#include "doomtype.h"
#include "m_buffer.h"   // IOANCH
#include "m_collection.h"
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

#define METANUMPRIMES earrlen(metaPrimes)

// Globals

// metaerrno represents the last error to occur. All routines that can cause
// an error will reset this to 0 if no error occurs.
int metaerrno = 0;

//! Common function for writing string to file. Writes size, then content.
static bool commonWriteString(const char* value, OutBuffer& outbuf)
{
    size_t s = strlen(value);
    if (!outbuf.WriteUint32((uint32_t)s))
        return false;
    return outbuf.Write(value, s);
}

//! Common function for reading string from file. Reads size, then content.
//! The read string will be appended to the input parameter string, so be sure
//! to empty it first if you don't want appending.
static bool commonReadString(qstring& string, InBuffer& inbuf)
{
   uint32_t size;
   if(!inbuf.readUint32(size))
      return false;
   char buffer[64];
   buffer[0] = 0;
   size_t readnum;
   while(size)
   {
      readnum = earrlen(buffer) < size ? earrlen(buffer) : size;
      size -= readnum;
      buffer[readnum] = 0;
      if(inbuf.read(buffer, readnum) < readnum)
         return false;
      string << buffer;
   }
   return true;
}

//=============================================================================
//
// Hash Table Tuning
//

//
// MetaHashRebuild
//
// Static method for rebuilding an EHashTable when its load factor has exceeded
// the metatable load factor. Expansion is limited; once the table is too large,
// load factor will increase indefinitely. Hash chain table size is currently
// limited to approximately 384 KB, which would require almost 400K objects to
// be in the table to hit 100% load factor... I'm not really concerned :P
//
template<typename HashType>
static void MetaHashRebuild(HashType &hash)
{
   auto curNumChains = hash.getNumChains();

   // check for key table overload
   if(hash.getLoadFactor() > METALOADFACTOR && 
      curNumChains < metaPrimes[METANUMPRIMES - 1])
   {
      int i;

      // find the next larger prime
      for(i = 0; curNumChains < metaPrimes[i]; i++);

      hash.rebuild(metaPrimes[i]);
   }
}

//=============================================================================
//
// Key Interning
//
// MetaObject key strings are interned here, for space efficiency. 
//

struct metakey_t
{
   DLListItem<metakey_t> links; // hash links
   char   *key;                 // key name
   size_t  index;               // numeric index
   unsigned int unmodHC;        // unmodulated hash key
};

// Hash table of keys by their name
static EHashTable<metakey_t, ENCStringHashKey, &metakey_t::key, &metakey_t::links>
   metaKeyHash;

// Collection of all key objects
static PODCollection<metakey_t *> metaKeys;

//
// MetaKey
//
// If the given key string is already interned, the metakey_t structure that
// stores it will be returned. If not, it will be added to the collection of
// keys and hashed by name.
//
static metakey_t &MetaKey(const char *key)
{
   metakey_t *keyObj;
   unsigned int unmodHC = ENCStringHashKey::HashCode(key);

   // Do we already have this key?
   if(!(keyObj = metaKeyHash.objectForKey(key, unmodHC)))
   {
      keyObj = estructalloc(metakey_t, 1);

      // add it to the list
      metaKeys.add(keyObj);

      keyObj->key     = estrdup(key);
      keyObj->index   = metaKeys.getLength() - 1;
      keyObj->unmodHC = unmodHC;

      // check for table overload, and hash it
      MetaHashRebuild<>(metaKeyHash);
      metaKeyHash.addObject(keyObj, keyObj->unmodHC);
   }

   return *keyObj;
}

//
// MetaKeyForIndex
//
// Given a key index, get the key.
//
static metakey_t &MetaKeyForIndex(size_t index)
{
   if(index >= metaKeys.getLength())
      I_Error("MetaKeyForIndex: illegal key index requested\n");

   return *metaKeys[index];
}

//=============================================================================
//
// MetaObject Methods
//

IMPLEMENT_RTTI_TYPE(MetaObject)

//
// MetaObject Default Constructor
//
// Not recommended for use. This exists because use of DECLARE_RTTI_OBJECT
// requires it.
//
MetaObject::MetaObject()
   : Super(), links(), typelinks(), type()
{
   metakey_t &keyObj = MetaKey("default"); // TODO: GUID?

   key    = keyObj.key;
   keyIdx = keyObj.index;
}

//
// MetaObject(size_t keyIndex)
//
// Constructor for MetaObject using an interned key index
//
MetaObject::MetaObject(size_t keyIndex)
   : Super(), links(), typelinks(), type()
{
   metakey_t &keyObj = MetaKeyForIndex(keyIndex);

   key    = keyObj.key;
   keyIdx = keyObj.index;
}

//
// MetaObject(const char *pKey)
//
// Constructor for MetaObject when key is known.
//
MetaObject::MetaObject(const char *pKey) 
   : Super(), links(), typelinks(), type()
{
   metakey_t &keyObj = MetaKey(pKey);

   key    = keyObj.key;
   keyIdx = keyObj.index;
}

//
// MetaObject::toString
//
// Virtual method for conversion of metaobjects into strings. As with the prior
// C implementation, the returned string pointer is a static buffer and should
// not be cached. The default toString method creates a hex dump representation
// of the object. This should be pretty interesting in C++...
// 04/03/11: Altered to use new ZoneObject functionality so that the entire
// object, including all subclasses, are dumped properly. Really cool ;)
//
const char *MetaObject::toString() const
{
   static qstring qstr;
   size_t bytestoprint = getZoneSize();
   const byte *data    = reinterpret_cast<const byte *>(getBlockPtr());
   
   qstr.clearOrCreate(128);

   if(!bytestoprint) // Not a zone object? Can only dump the base class.
   {
      bytestoprint = sizeof(*this);
      data = reinterpret_cast<const byte *>(this); // Not evil, I swear :P
   }

   while(bytestoprint)
   {
      int i;

      // print up to 12 bytes on each line
      for(i = 0; i < 12 && bytestoprint; ++i, --bytestoprint)
      {
         byte val = *data++;
         char bytes[4] = { 0 };

         sprintf(bytes, "%02x ", val);

         qstr += bytes;
      }
      qstr += '\n';
   }

   return qstr.constPtr();
}

bool MetaObject::writeToFile(OutBuffer& outbuf) const
{
    return commonWriteString(type ? type : "", outbuf) && commonWriteString(key, outbuf);
}

bool MetaObject::readFromFile(InBuffer &inbuf)
{
   qstring keystr;
   if(!commonReadString(keystr, inbuf))
      return false;

   // Do the reading procedure
   metakey_t &keyObj = MetaKey(keystr.constPtr());
   key    = keyObj.key;
   keyIdx = keyObj.index;

   return true;
}

MetaObject* MetaObject::createFromFile(InBuffer &inbuf)
{
   qstring typestr;
   if(!commonReadString(typestr, inbuf))
      return nullptr;
   // Now find the type
   MetaObject::Type *metaType;
   MetaObject* newMo;
   
   metaType = RTTIObject::FindTypeCls<MetaObject>(typestr.constPtr());
   if(!metaType)
      return nullptr;

   // Convert MetaConstString to MetaString
   if(!strcmp(metaType->getName(), RTTI(MetaConstString)->getName()))
      newMo = new MetaString;
   else
      newMo = metaType->newObject();   // Use "delete" to free this.
   
   if(!newMo->readFromFile(inbuf))
   {
      delete newMo;
      return nullptr;
   }
   
   return newMo;
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

IMPLEMENT_RTTI_TYPE(MetaInteger)

//
// MetaInteger::toString
//
// String conversion method for MetaInteger objects.
//
const char *MetaInteger::toString() const
{
   static char str[33];

   memset(str, 0, sizeof(str));

   M_Itoa(value, str, 10);

   return str;
}

// IOANCH: writeToFile
bool MetaInteger::writeToFile(OutBuffer& outbuf) const
{
    return Super::writeToFile(outbuf) && outbuf.WriteSint32(value);
}

bool MetaInteger::readFromFile(InBuffer &inbuf)
{
   return Super::readFromFile(inbuf) && inbuf.readSint32(value);
}

//
// IOANCH: v2fixed_t
//

IMPLEMENT_RTTI_TYPE(MetaV2Fixed)

//
// MetaInteger::toString
//
// String conversion method for MetaInteger objects.
//
const char *MetaV2Fixed::toString() const
{
   static char str[64];
   
   memset(str, 0, sizeof(str));
   
   psnprintf(str, sizeof(str), "(%d %d)", value.x, value.y);
   
   return str;
}

bool MetaV2Fixed::writeToFile(OutBuffer& outbuf) const
{
    return Super::writeToFile(outbuf) && outbuf.WriteSint32(value.x) && outbuf.WriteSint32(value.y);
}

bool MetaV2Fixed::readFromFile(InBuffer &inbuf)
{
   return Super::readFromFile(inbuf) && inbuf.readSint32(value.x) && inbuf.readSint32(value.y);
}

//
// Double
//

IMPLEMENT_RTTI_TYPE(MetaDouble)

//
// MetaDouble::toString
//
// toString method for metadouble objects.
//
const char *MetaDouble::toString() const
{
   static char str[64];

   memset(str, 0, sizeof(str));

   psnprintf(str, sizeof(str), "%+.5f", this->value);

   return str;
}

bool MetaDouble::writeToFile(OutBuffer& outbuf) const
{
    return Super::writeToFile(outbuf) && outbuf.Write(&value, sizeof(value));
}

bool MetaDouble::readFromFile(InBuffer &inbuf)
{
   return Super::readFromFile(inbuf) && inbuf.read(&value, sizeof(value));
}

//
// Strings
//
// metastrings created with these APIs assume ownership of the string. 
//

IMPLEMENT_RTTI_TYPE(MetaString)

//
// MetaString::setValue
//
// Non-trivial, unlike the other MetaObjects' setValue methods.
// This one can return the current value of the MetaString in *ret if the
// pointer-to-pointer is non-NULL. If it IS NULL, the current value will
// be freed instead.
//
void MetaString::setValue(const char *s, char **ret)
{
   if(value)
   {
      if(ret)
         *ret = value;
      else
         efree(value);
   }

   value = estrdup(s);
}

bool MetaString::writeToFile(OutBuffer& outbuf) const
{
    return Super::writeToFile(outbuf) && commonWriteString(value, outbuf);
}

bool MetaString::readFromFile(InBuffer &inbuf)
{
   if(!Super::readFromFile(inbuf))
      return false;
   qstring string;
   if(!commonReadString(string, inbuf))
      return false;
   setValue(string.constPtr(), nullptr);
   return true;
}

//
// Const Strings
//
// Const strings are not owned by their metaobject; they are simply referenced.
// These are for efficient storage of string constants/literals as meta fields.
//

// All we need here is the RTTIObject proxy type instance
IMPLEMENT_RTTI_TYPE(MetaConstString)

bool MetaConstString::writeToFile(OutBuffer& outbuf) const
{
    return Super::writeToFile(outbuf) && commonWriteString(value, outbuf);
}

bool MetaConstString::readFromFile(InBuffer &inbuf)
{
   // meta const strings can't be deserialized: get metastrings instead.
   return false;
}

//
// MetaVariant
//

IMPLEMENT_RTTI_TYPE(MetaVariant)

//
// Copy Constructor
//
MetaVariant::MetaVariant(const MetaVariant &other) 
   : Super(other), cachedType(other.cachedType)
{
   switch(other.cachedType)
   {
   case VARIANT_INT:
      cachedValue.i = other.cachedValue.i;
      break;
   case VARIANT_BOOL:
      cachedValue.b = other.cachedValue.b;
      break;
   case VARIANT_FLOAT:
      cachedValue.f = other.cachedValue.f;
      break;
   case VARIANT_DOUBLE:
      cachedValue.d = other.cachedValue.d;
      break;
   default:
      cachedValue.i = 0;
      break;
   }
}

//
// Retrieve value as an integer.
//
int MetaVariant::getInt()
{
   int ret;

   if(cachedType == VARIANT_INT)
      ret = cachedValue.i;
   else
   {
      cachedType = VARIANT_INT;
      ret = cachedValue.i = atoi(value);
   }

   return ret;
}

//
// Retrieve value as a boolean.
//
bool MetaVariant::getBool()
{
   bool ret;

   if(cachedType == VARIANT_BOOL)
      ret = cachedValue.b;
   else
   {
      cachedType = VARIANT_BOOL;
      ret = cachedValue.b = !!atoi(value);
   }

   return ret;
}

//
// Retrieve value as a float.
//
float MetaVariant::getFloat()
{
   float ret;

   if(cachedType == VARIANT_FLOAT)
      ret = cachedValue.f;
   else
   {
      cachedType = VARIANT_FLOAT;
      ret = cachedValue.f = static_cast<float>(atof(value));
   }

   return ret;
}

//
// Retrieve value as a double
//
double MetaVariant::getDouble()
{
   double ret;

   if(cachedType == VARIANT_DOUBLE)
      ret = cachedValue.d;
   else
   {
      cachedType = VARIANT_DOUBLE;
      ret = cachedValue.d = atof(value);
   }

   return ret;
}

//
// Overrides MetaString::setValue. Set the new string value and then
// update the cache based on the last interpreted value of this variant.
//
void MetaVariant::setValue(const char *s, char **ret)
{
   Super::setValue(s, ret);
   
   // force reinterpretation
   varianttype_e oldType = cachedType;
   cachedType = VARIANT_NONE;

   switch(oldType)
   {
   case VARIANT_INT:
      getInt();
      break;
   case VARIANT_BOOL:
      getBool();
      break;
   case VARIANT_FLOAT:
      getFloat();
      break;
   case VARIANT_DOUBLE:
      getDouble();
      break;
   default:
      break;
   }
}

//
// End MetaObject Specializations
//
//=============================================================================

//=============================================================================
//
// MetaTable Methods - General Utilities
//

//
// MetaTablePimpl
//
// Private implementation structure for the MetaTable class. Because I am not
// about to expose the entire engine to the EHashTable template if I can help
// it.
//
// A MetaTable is just a pair of hash tables, one on keys and one on types.
//
class MetaTablePimpl : public ZoneObject
{
public:
   // the key hash is growable; keys are case-insensitive.
   EHashTable<MetaObject, ENCStringHashKey, 
              &MetaObject::key, &MetaObject::links> keyhash;

   // the type hash is fixed size since there are a limited number of types
   // defined in the source code; types are case sensitive, because they are 
   // based on C++ types.
   EHashTable<MetaObject, EStringHashKey, 
              &MetaObject::type, &MetaObject::typelinks> typehash;

   MetaTablePimpl() : ZoneObject(), keyhash(METANUMCHAINS), typehash(METANUMCHAINS) {}

   virtual ~MetaTablePimpl()
   {
      keyhash.destroy();
      typehash.destroy();
   }
};

IMPLEMENT_RTTI_TYPE(MetaTable)

//
// MetaTable Default Constructor
//
MetaTable::MetaTable() : Super()
{
   // Construct the private implementation object that holds our dual hashes
   pImpl = new MetaTablePimpl();
}

//
// MetaTable(name)
//
MetaTable::MetaTable(const char *name) : Super(name)
{
   // Construct the private implementation object that holds our dual hashes
   pImpl = new MetaTablePimpl();
}

//
// MetaTable(const MetaTable &)
//
// Copy constructor
//
MetaTable::MetaTable(const MetaTable &other) : Super(other)
{
   pImpl = new MetaTablePimpl();
   copyTableFrom(&other);
}

//
// MetaTable Destructor
//
MetaTable::~MetaTable()
{
   clearTable();

   delete pImpl;
   pImpl = NULL;
}

//
// MetaTable::clone
//
// Virtual method inherited from MetaObject. Create a copy of this table.
//
MetaObject *MetaTable::clone() const
{
   return new MetaTable(*this);
}

//
// MetaTable::getLoadFactor
//
// Returns load factor of the key hash table.
//
float MetaTable::getLoadFactor() const
{
   return pImpl->keyhash.getLoadFactor();
}
 
//
// MetaTable::getNumItems
//
// Returns the number of items in the table.
//
unsigned int MetaTable::getNumItems() const
{
   return pImpl->keyhash.getNumItems();
}

//
// MetaTable::toString
//
// Virtual method inherited from MetaObject.
//
const char *MetaTable::toString() const
{
   return key;
}

//
// MetaTable::hasKey
//
// Returns true or false if an object of the same key is in the metatable.
// No type checking is done, so it will match any object with that key.
//
bool MetaTable::hasKey(const char *key)
{
   return (pImpl->keyhash.objectForKey(key) != NULL);
}

//
// MetaTable::hasType
//
// Returns true or false if an object of the same type is in the metatable.
//
bool MetaTable::hasType(const char *type)
{
   return (pImpl->typehash.objectForKey(type) != NULL);
}

//
// MetaTable::hasKeyAndType
//
// Returns true if an object exists in the table of both the specified key
// and type, and it is the same object. This is naturally slower as it must
// search down the key hash chain for a type match.
//
bool MetaTable::hasKeyAndType(const char *key, const char *type)
{
   MetaObject *obj = NULL;
   bool found = false;

   while((obj = pImpl->keyhash.keyIterator(obj, key)))
   {
      // for each object that matches the key, test the type
      if(obj->isInstanceOf(type))
      {
         found = true;
         break;
      }
   }

   return found;
}

//
// MetaTable::countOfKey
//
// Returns the count of objects in the metatable with the given key.
//
int MetaTable::countOfKey(const char *key)
{
   MetaObject *obj = NULL;
   int count = 0;

   while((obj = pImpl->keyhash.keyIterator(obj, key)))
      ++count;

   return count;
}

//
// MetaTable::countOfType
//
// Returns the count of objects in the metatable with the given type.
//
int MetaTable::countOfType(const char *type)
{
   MetaObject *obj = NULL;
   int count = 0;

   while((obj = pImpl->typehash.keyIterator(obj, type)))
      ++count;

   return count;
}

//
// MetaTable::countOfKeyAndType
//
// As above, but satisfying both conditions at once.
//
int MetaTable::countOfKeyAndType(const char *key, const char *type)
{
   MetaObject *obj = NULL;
   int count = 0;

   while((obj = pImpl->keyhash.keyIterator(obj, key)))
   {
      if(obj->isInstanceOf(type))
         ++count;
   }

   return count;
}

//=============================================================================
//
// MetaTable Methods - General Metaobjects
//

//
// MetaTable::addObject
//
// Adds a generic metaobject to the table. The metatable does not assume
// responsibility for the memory management of metaobjects or type strings.
// Key strings are managed, however, to avoid serious problems with mutual
// references between metaobjects.
//
void MetaTable::addObject(MetaObject *object)
{
   // Check for rehash
   MetaHashRebuild<>(pImpl->keyhash);

   // Initialize type name
   object->setType();

   // Add the object to the key table.
   // haleyjd 09/17/2012: use the precomputed unmodulated hash code for the
   // MetaObject's interned key.
   pImpl->keyhash.addObject(object, MetaKeyForIndex(object->getKeyIdx()).unmodHC);

   // Add the object to the type table, which is static in size
   pImpl->typehash.addObject(object);
}

//
// Convenience overload for references
//
void MetaTable::addObject(MetaObject &object)
{
   addObject(&object);
}

//
// MetaTable::removeObject
//
// Removes the provided object from the given metatable.
//
void MetaTable::removeObject(MetaObject *object)
{
   pImpl->keyhash.removeObject(object);
   pImpl->typehash.removeObject(object);
}

//
// Convenience overload for references.
//
void MetaTable::removeObject(MetaObject &object)
{
   removeObject(&object);
}

//
// MetaTable::getObject
//
// Returns the first object found in the metatable with the given key, 
// regardless of its type. Returns NULL if no such object exists.
//
MetaObject *MetaTable::getObject(const char *key)
{
   return pImpl->keyhash.objectForKey(key);
}

//
// MetaTable::getObject
//
// Overload taking a MetaObject interned key index.
//
MetaObject *MetaTable::getObject(size_t keyIndex)
{
   metakey_t &keyObj = MetaKeyForIndex(keyIndex);
   return pImpl->keyhash.objectForKey(keyObj.key, keyObj.unmodHC);
}

//
// MetaTable::getObjectType
//
// Returns the first object found in the metatable which matches the type. 
// Returns NULL if no such object exists.
//
MetaObject *MetaTable::getObjectType(const char *type)
{
   return pImpl->typehash.objectForKey(type);
}

//
// MetaTable::getObjectType
//
// Overload taking a MetaObject::Type instance.
//
MetaObject *MetaTable::getObjectType(const MetaObject::Type &type)
{
   return pImpl->typehash.objectForKey(type.getName());
}

//
// MetaTable::getObjectKeyAndType
//
// As above, but satisfying both conditions at once.
//
MetaObject *MetaTable::getObjectKeyAndType(const char *key, 
                                           const MetaObject::Type *type) const
{
   MetaObject *obj = NULL;

   while((obj = pImpl->keyhash.keyIterator(obj, key)))
   {
      if(obj->isInstanceOf(type))
         break;
   }

   return obj;
}

//
// MetaTable::getObjectKeyAndType
//
// As above, but satisfying both conditions at once.
// Overload for type names.
//
MetaObject *MetaTable::getObjectKeyAndType(const char *key, const char *type)
{
   MetaObject::Type *rttiType = FindTypeCls<MetaObject>(type);

   return rttiType ? getObjectKeyAndType(key, rttiType) : NULL;
}

//
// MetaTable::getObjectKeyAndType
//
// Overload taking a MetaObject interned key index and RTTIObject::Type
// instance.
//
MetaObject *MetaTable::getObjectKeyAndType(size_t keyIndex, 
                                           const MetaObject::Type *type) const
{
   metakey_t  &keyObj = MetaKeyForIndex(keyIndex);
   MetaObject *obj    = NULL;

   while((obj = pImpl->keyhash.keyIterator(obj, keyObj.key, keyObj.unmodHC)))
   {
      if(obj->isInstanceOf(type))
         break;
   }
   
   return obj;
}

//
// MetaTable::getObjectKeyAndType
//
// Overload taking a MetaObject interned key index and type name.
//
MetaObject *MetaTable::getObjectKeyAndType(size_t keyIndex, const char *type)
{
   MetaObject::Type *rttiType = FindTypeCls<MetaObject>(type);

   return rttiType ? getObjectKeyAndType(keyIndex, rttiType) : NULL;
}

//
// MetaTable::getNextObject
//
// Returns the next object with the same key, or the first such object
// in the table if NULL is passed in the object pointer. Returns NULL
// when no further objects of the same key are available.
//
MetaObject *MetaTable::getNextObject(MetaObject *object, const char *key)
{
   // If no key is provided but object is valid, get the next object with the 
   // same key as the current one.
   if(object && !key)
   {
      unsigned int hc = MetaKeyForIndex(object->getKeyIdx()).unmodHC;
      key = object->getKey();

      return pImpl->keyhash.keyIterator(object, key, hc);
   }
   else
      return pImpl->keyhash.keyIterator(object, key);
}

//
// MetaTable::getNextObject
//
// Overload taking a MetaObject interned key index.
//
MetaObject *MetaTable::getNextObject(MetaObject *object, size_t keyIndex)
{
   metakey_t &keyObj = MetaKeyForIndex(keyIndex);

   return pImpl->keyhash.keyIterator(object, keyObj.key, keyObj.unmodHC);
}

//
// MetaTable::getNextType
//
// Similar to above, but this returns the next object which also matches
// the specified type.
//
MetaObject *MetaTable::getNextType(MetaObject *object, const char *type)
{
   // As above, allow using the same type as the current object
   if(object && !type)
      type = object->getClassName();

   return pImpl->typehash.keyIterator(object, type);
}

//
// MetaTable::getNextType
//
// Overload accepting a pointer to a MetaObject RTTI proxy. If object is valid,
// type is optional, but otherwise it must be valid.
//
MetaObject *MetaTable::getNextType(MetaObject *object, const MetaObject::Type *type)
{
   // Same as above
   if(object && !type)
      type = object->getDynamicType();

   // Must have a type
   if(!type)
      return NULL;

   return pImpl->typehash.keyIterator(object, type->getName());
}

//
// MetaTable::getNextKeyAndType
//
// As above, but satisfying both conditions at once.
//
MetaObject *MetaTable::getNextKeyAndType(MetaObject *object, const char *key, const char *type)
{
   MetaObject *obj = object;

   if(object)
   {
      // As above, allow NULL in either key or type to mean "same as current"
      if(!key)
         key = object->getKey();

      if(!type)
         type = object->getClassName();
   }

   while((obj = pImpl->keyhash.keyIterator(obj, key)))
   {
      if(obj->isInstanceOf(type))
         break;
   }

   return obj;
}

//
// MetaTable::getNextKeyAndType
//
// Overload taking a MetaObject interned key index.
//
MetaObject *MetaTable::getNextKeyAndType(MetaObject *object, size_t keyIdx, const char *type)
{
   MetaObject *obj    = object;
   metakey_t  &keyObj = MetaKeyForIndex(keyIdx);

   if(object)
   {
      // As above, allow NULL in type to mean "same as current"
      if(!type)
         type = object->getClassName();
   }

   while((obj = pImpl->keyhash.keyIterator(obj, keyObj.key, keyObj.unmodHC)))
   {
      if(obj->isInstanceOf(type))
         break;
   }

   return obj;
}

//
// MetaTable::getNextKeyAndType
//
// Overload taking a key string and a MetaObject RTTI proxy object.
//
MetaObject *MetaTable::getNextKeyAndType(MetaObject *object, const char *key, 
                                         const MetaObject::Type *type)
{
   MetaObject *obj = object;

   if(object)
   {
      // As above, allow NULL in either key or type to mean "same as current"
      if(!key)
         key = object->getKey();

      if(!type)
         type = object->getDynamicType();
   }

   while((obj = pImpl->keyhash.keyIterator(obj, key)))
   {
      if(obj->isInstanceOf(type))
         break;
   }

   return obj;
}

//
// MetaTable::getNextKeyAndType
//
// Overload taking a MetaKey index and a MetaObject RTTI proxy object.
//
MetaObject *MetaTable::getNextKeyAndType(MetaObject *object, size_t keyIdx, 
                                         const MetaObject::Type *type)
{
   MetaObject *obj    = object;
   metakey_t  &keyObj = MetaKeyForIndex(keyIdx);

   if(object)
   {
      // As above, allow NULL in type to mean "same as current"
      if(!type)
         type = object->getDynamicType();
   }

   while((obj = pImpl->keyhash.keyIterator(obj, keyObj.key, keyObj.unmodHC)))
   {
      if(obj->isInstanceOf(type))
         break;
   }

   return obj;
}

//
// MetaTable::tableIterator
//
// Iterates on all objects in the metatable, regardless of key or type.
// Const version.
//
const MetaObject *MetaTable::tableIterator(const MetaObject *object) const
{
   return pImpl->keyhash.tableIterator(object);
}

//
// MetaTable::tableIterator
//
// Iterates on all objects in the metatable, regardless of key or type.
// Mutable version.
//
MetaObject *MetaTable::tableIterator(MetaObject *object) const
{
   return pImpl->keyhash.tableIterator(object);
}

//
// MetaTable::addInt
//
// Add an integer to the metatable using an interned key index.
//
void MetaTable::addInt(size_t keyIndex, int value)
{
   addObject(new MetaInteger(keyIndex, value));
}

//
// MetaTable::addInt
//
// Add an integer to the metatable using a raw string key.
//
void MetaTable::addInt(const char *key, int value)
{
   addObject(new MetaInteger(key, value));
}

//
// MetaTable::getInt
//
// Get an integer from the metatable. This routine returns the value
// rather than a pointer to a metaint_t. If an object of the requested
// name doesn't exist in the table, defvalue is returned and metaerrno 
// is set to indicate the problem.
//
// Use of this routine only returns the first such value in the table.
// This routine is meant for singleton fields.
//
int  MetaTable::getInt(size_t keyIndex, int defValue) const
{
   int retval;
   MetaObject *obj;

   metaerrno = META_ERR_NOERR;

   if(!(obj = getObjectKeyAndType(keyIndex, RTTI(MetaInteger))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      retval = defValue;
   }
   else
      retval = static_cast<MetaInteger *>(obj)->value;

   return retval;
}

//
// MetaTable::getInt
//
// Overload for raw key strings.
//
int MetaTable::getInt(const char *key, int defValue)
{
   return getInt(MetaKey(key).index, defValue);
}

//
// MetaTable::setInt
//
// If the metatable already contains a metaint of the given name, it will
// be edited to have the provided value. Otherwise, a new metaint will be
// added to the table with that value.
//
void MetaTable::setInt(size_t keyIndex, int newValue)
{
   MetaObject *obj;

   if(!(obj = getObjectKeyAndType(keyIndex, RTTI(MetaInteger))))
      addInt(keyIndex, newValue);
   else
      static_cast<MetaInteger *>(obj)->value = newValue;
}

//
// MetaTable::setInt
//
// Overload for raw key strings.
//
void MetaTable::setInt(const char *key, int newValue)
{
   setInt(MetaKey(key).index, newValue);
}

//
// MetaTable::removeInt
//
// Removes the given field if it exists as a metaint_t.
// Only one object will be removed. If more than one such object 
// exists, you would need to call this routine until metaerrno is
// set to META_ERR_NOSUCHOBJECT.
//
// The value of the object is returned in case it is needed.
//
int MetaTable::removeInt(const char *key)
{
   MetaObject *obj;
   int value;

   metaerrno = META_ERR_NOERR;

   if(!(obj = getObjectKeyAndType(key, RTTI(MetaInteger))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      return 0;
   }

   removeObject(obj);

   value = static_cast<MetaInteger *>(obj)->value;

   delete obj;

   return value;
}

// IOANCH 20130815: added for 2D fixed vector
//
// MetaTable::addV2Fixed
//
void MetaTable::addV2Fixed(const char *key, v2fixed_t value)
{
   addObject(new MetaV2Fixed(key, value));
}

//
// MetaTable::getDouble
//
v2fixed_t MetaTable::getV2Fixed(const char *key, v2fixed_t defValue)
{
   v2fixed_t retval;
   MetaObject *obj;
   
   metaerrno = META_ERR_NOERR;
   
   if(!(obj = getObjectKeyAndType(key, RTTI(MetaV2Fixed))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      retval = defValue;
   }
   else
      retval = static_cast<MetaV2Fixed *>(obj)->value;
   
   return retval;
}

//
// MetaTable::setV2Fixed
//
void MetaTable::setV2Fixed(const char *key, v2fixed_t newValue)
{
   MetaObject *obj;
   
   if(!(obj = getObjectKeyAndType(key, RTTI(MetaV2Fixed))))
      addV2Fixed(key, newValue);
   else
      static_cast<MetaV2Fixed *>(obj)->value = newValue;
}

//
// MetaTable::removeV2Fixed
//
v2fixed_t MetaTable::removeV2Fixed(const char *key)
{
   MetaObject *obj;
   v2fixed_t value;
   
   metaerrno = META_ERR_NOERR;
   
   if(!(obj = getObjectKeyAndType(key, RTTI(MetaV2Fixed))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      v2fixed_t ret = {0 , 0};
      return ret;
   }
   
   removeObject(obj);
   
   value = static_cast<MetaV2Fixed *>(obj)->value;
   
   delete obj;
   
   return value;
}


//
// MetaTable::addDouble
//
// Add a double-precision float to the metatable.
//
void MetaTable::addDouble(const char *key, double value)
{
   addObject(new MetaDouble(key, value));
}

//
// MetaTable::getDouble
//
// Get a double from the metatable. This routine returns the value
// rather than a pointer to a metadouble_t. If an object of the requested
// name doesn't exist in the table, defvalue is returned and metaerrno is set
// to indicate the problem.
//
// Use of this routine only returns the first such value in the table.
// This routine is meant for singleton fields.
//
double MetaTable::getDouble(const char *key, double defValue)
{
   double retval;
   MetaObject *obj;

   metaerrno = META_ERR_NOERR;

   if(!(obj = getObjectKeyAndType(key, RTTI(MetaDouble))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      retval = defValue;
   }
   else
      retval = static_cast<MetaDouble *>(obj)->value;

   return retval;
}

//
// MetaTable::setDouble
//
// If the metatable already contains a metadouble of the given name, it will
// be edited to have the provided value. Otherwise, a new metadouble will be
// added to the table with that value.
//
void MetaTable::setDouble(const char *key, double newValue)
{
   MetaObject *obj;

   if(!(obj = getObjectKeyAndType(key, RTTI(MetaDouble))))
      addDouble(key, newValue);
   else
      static_cast<MetaDouble *>(obj)->value = newValue;
}

//
// MetaTable::removeDouble
//
// Removes the given field if it exists as a metadouble_t.
// Only one object will be removed. If more than one such object 
// exists, you would need to call this routine until metaerrno is
// set to META_ERR_NOSUCHOBJECT.
//
// The value of the object is returned in case it is needed.
//
double MetaTable::removeDouble(const char *key)
{
   MetaObject *obj;
   double value;

   metaerrno = META_ERR_NOERR;

   if(!(obj = getObjectKeyAndType(key, RTTI(MetaDouble))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      return 0.0;
   }

   removeObject(obj);

   value = static_cast<MetaDouble *>(obj)->value;

   delete obj;

   return value;
}


//
// MetaTable::addString
//
void MetaTable::addString(const char *key, const char *value)
{
   addObject(new MetaString(key, value));
}

//
// MetaTable::getString
//
// Get a string from the metatable. This routine returns the value
// rather than a pointer to a metastring_t. If an object of the requested
// name doesn't exist in the table, defvalue is returned and metaerrno is set
// to indicate the problem.
//
// Use of this routine only returns the first such value in the table.
// This routine is meant for singleton fields.
//
const char *MetaTable::getString(const char *key, const char *defValue)
{
   const char *retval;
   MetaObject *obj;

   metaerrno = META_ERR_NOERR;

   if(!(obj = getObjectKeyAndType(key, RTTI(MetaString))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      retval = defValue;
   }
   else
      retval = static_cast<MetaString *>(obj)->value;

   return retval;
}

//
// MetaTable::setString
//
// If the metatable already contains a metastring of the given name, it will
// be edited to have the provided value. Otherwise, a new metastring will be
// added to the table with that value. 
//
void MetaTable::setString(const char *key, const char *newValue)
{
   MetaObject *obj;

   if(!(obj = getObjectKeyAndType(key, RTTI(MetaString))))
      addString(key, newValue);
   else
      static_cast<MetaString *>(obj)->setValue(newValue);
}

//
// MetaTable::removeString
//
// Removes the given field if it exists as a metastring_t.
// Only one object will be removed. If more than one such object 
// exists, you would need to call this routine until metaerrno is
// set to META_ERR_NOSUCHOBJECT.
//
// When calling this routine, the value of the object is returned 
// in case it is needed, and you will need to then free it yourself 
// using free(). If the return value is not needed, call
// MetaTable::removeStringNR instead and the string value will be destroyed.
//
char *MetaTable::removeString(const char *key)
{
   MetaObject *obj;
   MetaString *str;
   char *value;

   metaerrno = META_ERR_NOERR;

   if(!(obj = getObjectKeyAndType(key, RTTI(MetaString))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      return NULL;
   }

   removeObject(obj);

   // Destroying the MetaString will destroy the value inside it too, unless we
   // get and then nullify its value manually. This is one reason why MetaTable
   // is a friend to these basic types, as it makes some simple management 
   // chores like this more efficient. Otherwise I'd have to estrdup the string 
   // and that's stupid.

   str = static_cast<MetaString *>(obj);
   
   value = str->value;
   str->value = NULL; // destructor does nothing if this is cleared first

   delete str;

   return value;
}

//
// MetaTable::removeStringNR
//
// As above, but the string value is not returned, but instead freed, to
// alleviate any need the user code might have to free string values in
// which it isn't interested.
//
void MetaTable::removeStringNR(const char *key)
{
   MetaObject *obj;

   metaerrno = META_ERR_NOERR;

   if(!(obj = getObjectKeyAndType(key, RTTI(MetaString))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      return;
   }

   removeObject(obj);

   delete obj;
}

//
// MetaTable::addConstString
//
// Add a sharable string constant/literal value to the MetaTable,
// using an interned key index.
//
void MetaTable::addConstString(size_t keyIndex, const char *value)
{
   addObject(new MetaConstString(keyIndex, value));
}

//
// MetaTable::addConstString
//
// Add a sharable string constant/literal value to the MetaTable.
//
void MetaTable::addConstString(const char *key, const char *value)
{
   addObject(new MetaConstString(key, value));
}

//
// MetaTable::getConstString
//
// Get a sharable string constant/literal value from the MetaTable. If the
// requested property does not exist as a MetaConstString, the value provided
// by the defValue parameter will be returned, and metaerrno will be set to
// META_ERR_NOSUCHOBJECT. Otherwise, the string constant value is returned and
// metaerrno is META_ERR_NOERR.
//
const char *MetaTable::getConstString(const char *key, const char *defValue)
{
   const char *retval;
   MetaObject *obj;

   metaerrno = META_ERR_NOERR;

   if(!(obj = getObjectKeyAndType(key, RTTI(MetaConstString))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      retval    = defValue;
   }
   else
      retval = static_cast<MetaConstString *>(obj)->value;

   return retval;
}

//
// MetaTable::setConstString
//
// If the table already contains a MetaConstString with the provided key, its
// value will be set to newValue. Otherwise, a new MetaConstString will be
// created with this key and value and will be added to the table.
//
void MetaTable::setConstString(size_t keyIndex, const char *newValue)
{
   MetaObject *obj;

   if(!(obj = getObjectKeyAndType(keyIndex, RTTI(MetaConstString))))
      addConstString(keyIndex, newValue);
   else
      static_cast<MetaConstString *>(obj)->setValue(newValue);
}

//
// MetaTable::setConstString
//
// Overload for raw string keys.
//
void MetaTable::setConstString(const char *key, const char *newValue)
{
   setConstString(MetaKey(key).index, newValue);
}

//
// MetaTable::removeConstString
//
// Removes a constant string from the table with the given key. If no such
// object exists, metaerrno will be META_ERR_NOSUCHOBJECT and NULL is returned.
// Otherwise, metaerrno is META_ERR_NOERR and the shared string value that 
// was in the MetaConstString instance is returned.
//
const char *MetaTable::removeConstString(const char *key)
{
   MetaObject *obj;
   MetaConstString *str;
   const char *value;

   metaerrno = META_ERR_NOERR;

   if(!(obj = getObjectKeyAndType(key, RTTI(MetaConstString))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      return NULL;
   }

   removeObject(obj);

   str = static_cast<MetaConstString *>(obj);
   value = str->value;
   delete str;

   return value;
}

//
// MetaTable::copyTableTo
//
// Adds copies of all objects in the source table to the destination table.
//
void MetaTable::copyTableTo(MetaTable *dest) const
{
   MetaObject *srcobj = NULL;

   // iterate on the source table
   while((srcobj = tableIterator(srcobj)))
   {
      // create the new object
      MetaObject *newObject = srcobj->clone();

      // add the new object to the destination table
      dest->addObject(newObject);
   }
}

//
// MetaTable::copyTableFrom
//
// Convenience method
// Adds copies of all objects in the source table to *this* table.
//
void MetaTable::copyTableFrom(const MetaTable *source)
{
   source->copyTableTo(this);
}

//
// MetaTable::clearTable
//
// Removes all objects from a metatable.
//
void MetaTable::clearTable()
{
   MetaObject *obj = NULL;

   // iterate on the source table
   while((obj = tableIterator(obj)))
   {
      removeObject(obj);
      delete obj;
      obj = NULL; // restart from the beginning
   }
}

//
// MetaTable Statics
//

//
// MetaTable::IndexForKey
//
// This will intern the passed-in key string if it has not been interned
// already (consider it pre-caching, if you will). The index of that
// interned string will be returned.
//
size_t MetaTable::IndexForKey(const char *key)
{
   return MetaKey(key).index;
}

// IOANCH: serialization
bool MetaTable::writeToFile(OutBuffer& outbuf) const
{
    if (!Super::writeToFile(outbuf) || !outbuf.WriteUint32(getNumItems()))
        return false;
    for (const MetaObject* mo = tableIterator((const MetaObject*)nullptr); mo; mo = tableIterator(mo))
    {
        if (!mo->writeToFile(outbuf))
            return false;
    }
    return true;
}

bool MetaTable::readFromFile(InBuffer &inbuf)
{
   if(!Super::readFromFile(inbuf))
      return false;
   uint32_t num;
   if(!inbuf.readUint32(num))
      return false;
   MetaObject* mo;
   for(uint32_t i = 0; i < num; ++i)
   {
      mo = MetaObject::createFromFile(inbuf);
      if(!mo)
         return false;
      addObject(mo);
   }
   return true;
}

// EOF

