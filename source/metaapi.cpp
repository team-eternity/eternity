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
#include "i_system.h"
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
   : Super(), links(), typelinks(), type(), key()
{
   key = key_name = estrdup("default"); // TODO: GUID?
}

//
// MetaObject(const char *pKey)
//
// Constructor for MetaObject when type and/or key are known.
//
MetaObject::MetaObject(const char *pKey) 
   : Super(), links(), typelinks(), type(), key()
{
   key = key_name = estrdup(pKey); // key_name is managed by the metaobject
}

//
// MetaObject(MetaObject &)
//
// Copy constructor
//
MetaObject::MetaObject(const MetaObject &other)
   : Super(), links(), typelinks(), type(), key()
{
   if(key_name && key_name != other.key_name)
      efree(key_name);
   key_name = estrdup(other.key_name);

   key = key_name;
}

//
// ~MetaObject
//
// Virtual destructor for metaobjects.
//
MetaObject::~MetaObject()
{
   // key_name is managed by the metaobject
   if(key_name)
      efree(key_name);

   key_name  = NULL;
}

//
// MetaObject::clone
//
// Virtual factory method for metaobjects; when invoked through the metatable,
// a descendent class will return an object of the proper type matching itself.
// This base class implementation doesn't really do anything, but I'm not fond
// of pure virtuals so here it is. Don't call it from the parent implementation
// as that is not what any of the implementations should do.
//
MetaObject *MetaObject::clone() const
{
   return new MetaObject(*this);
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

//
// MetaObject::setType
//
// This will set the MetaObject's internal type to its class name. This is
// really only for use by MetaTable but calling it yourself wouldn't screw
// anything up. It's just redundant.
//
void MetaObject::setType()
{
   type = getClassName();
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
// MetaInteger(char *, int)
//
// Key/Value Constructor
//
MetaInteger::MetaInteger(const char *key, int i)
   : MetaObject(key), value(i)
{
}

//
// MetaInteger(MetaInteger &)
//
// Copy Constructor
//
MetaInteger::MetaInteger(const MetaInteger &other) : MetaObject(other)
{
   this->value = other.value;
}

//
// MetaInteger::clone
//
// Virtual factory method to create a new MetaInteger with the same properties
// as the original.
//
MetaObject *MetaInteger::clone() const
{
   return new MetaInteger(*this);
}

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

//
// Double
//

IMPLEMENT_RTTI_TYPE(MetaDouble)

//
// MetaDouble(char *, double)
//
MetaDouble::MetaDouble(const char *key, double d) 
   : MetaObject(key), value(d)
{
}

//
// MetaDouble(const MetaDouble &)
//
// Copy Constructor
//
MetaDouble::MetaDouble(const MetaDouble &other) : MetaObject(other)
{
   this->value = other.value;
}

//
// MetaDouble::clone
//
// Virtual factory method to create a copy of a MetaDouble.
//
MetaObject *MetaDouble::clone() const
{
   return new MetaDouble(*this);
}

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

//
// Strings
//
// metastrings created with these APIs assume ownership of the string. 
//

IMPLEMENT_RTTI_TYPE(MetaString)

//
// MetaString Default Constructor
//
MetaString::MetaString() : MetaObject()
{
   this->value = estrdup("");
}

//
// MetaString(char *, const char *)
//
// Key/Value Constructor
//
MetaString::MetaString(const char *key, const char *s) : MetaObject(key)
{
   this->value = estrdup(s);
}

//
// MetaString(const MetaString &)
//
// Copy Constructor
//
MetaString::MetaString(const MetaString &other) : MetaObject(other)
{
   this->value = estrdup(other.value);
}

//
// ~MetaString
//
// Virtual destructor. The string value will be freed if it is valid.
//
MetaString::~MetaString()
{
   if(value)
      efree(value);

   value = NULL;
}

//
// MetaString::clone
//
// Virtual factory method to create a copy of a MetaString.
//
MetaObject *MetaString::clone() const
{
   return new MetaString(*this);
}

//
// MetaString::toString
//
// toString method for metastrings
//
const char *MetaString::toString() const
{
   return value; // simplest of them all!
}

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

//
// End MetaObject Specializations
//
//=============================================================================

//=============================================================================
//
// MetaTable Methods - General Utilities
//

//
// metaTablePimpl
//
// Private implementation structure for the MetaTable class. Because I am not
// about to expose the entire engine to the EHashTable template if I can help
// it.
//
// A metatable is just a pair of hash tables, one on keys and one on types.
//
class metaTablePimpl : public ZoneObject
{
public:
   // the key hash is growable; keys are case-insensitive.
   EHashTable<MetaObject, ENCStringHashKey, 
              &MetaObject::key, &MetaObject::links> keyhash;

   // the type hash is fixed size since there are a limited number of types
   // defined in the source code; types are case sensitive, because they are 
   // based on C types.
   EHashTable<MetaObject, EStringHashKey, 
              &MetaObject::type, &MetaObject::typelinks> typehash;

   metaTablePimpl() : ZoneObject(), keyhash(METANUMCHAINS), typehash(METANUMCHAINS) {}

   virtual ~metaTablePimpl()
   {
      keyhash.destroy();
      typehash.destroy();
   }
};

IMPLEMENT_RTTI_TYPE(MetaTable)

//
// MetaTable Default Constructor
//
MetaTable::MetaTable() : MetaObject()
{
   // Construct the private implementation object that holds our dual hashes
   pImpl = new metaTablePimpl();
}

//
// MetaTable(name)
//
MetaTable::MetaTable(const char *name) : MetaObject(name)
{
   // Construct the private implementation object that holds our dual hashes
   pImpl = new metaTablePimpl();
}

//
// MetaTable(const MetaTable &)
//
// Copy constructor
//
MetaTable::MetaTable(const MetaTable &other) : MetaObject(other.key_name)
{
   pImpl = new metaTablePimpl();
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
// MetaTable::toString
//
// Virtual method inherited from MetaObject.
//
const char *MetaTable::toString() const
{
   return key_name;
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
   unsigned int curNumChains = pImpl->keyhash.getNumChains();

   // check for key table overload
   if(pImpl->keyhash.getLoadFactor() > METALOADFACTOR && 
      curNumChains < metaPrimes[METANUMPRIMES - 1])
   {
      int i;

      // find a prime larger than the current number of chains
      for(i = 0; curNumChains < metaPrimes[i]; ++i);

      pImpl->keyhash.rebuild(metaPrimes[i]);
   }

   // Initialize type name
   object->setType();

   // Add the object to the key table
   pImpl->keyhash.addObject(object);

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
// MetaGetObjectType
//
// Returns the first object found in the metatable which matches the type. 
// Returns NULL if no such object exists.
//
MetaObject *MetaTable::getObjectType(const char *type)
{
   return pImpl->typehash.objectForKey(type);
}

//
// MetaTable::getObjectKeyAndType(const char *, MetaObject::Type *)
//
// As above, but searching for an object which satisfies both the key and type
// requirements simultaneously.
//
MetaObject *MetaTable::getObjectKeyAndType(const char *key, MetaObject::Type *rt)
{
   MetaObject *obj = NULL;

   while((obj = pImpl->keyhash.keyIterator(obj, key)))
   {
      if(obj->isInstanceOf(rt))
         break;
   }

   return obj;
}

//
// MetaTable::getObjectKeyAndType
//
// Overload accepting a class name.
//
MetaObject *MetaTable::getObjectKeyAndType(const char *key, const char *type)
{
   MetaObject::Type *rttiType;

   if(!(rttiType = MetaObject::Type::FindType<MetaObject::Type>(type)))
      I_Error("MetaTable::getObjectKeyAndType: %s is not a MetaObject!\n", type);

   return getObjectKeyAndType(key, rttiType);
}

//
// MetaTable::getNextObject
//
// Returns the next object with the same key, or the first such object
// in the table if NULL is passed in the object pointer. Returns NULL
// when no further objects of the same key are available.
//
// This is just a wrapper around E_HashObjectIterator, but you should
// call this anyway if you want to pretend you don't know how the
// metatable is implemented.
//
MetaObject *MetaTable::getNextObject(MetaObject *object, const char *key)
{
   // If no key is provided but object is valid, get the next object with the 
   // same key as the current one.
   if(object && !key)
      key = object->getKey();

   return pImpl->keyhash.keyIterator(object, key);
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
// MetaTable::getNextKeyAndType
//
// As above, but satisfying both conditions at once.
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
#ifdef RANGECHECK
   else if(!(key && type))
      I_Error("MetaTable::getNextKeyAndType: illegal arguments\n");
#endif

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
// Overload accepting a class name.
//
MetaObject *MetaTable::getNextKeyAndType(MetaObject *object, const char *key, 
                                         const char *type)
{
   MetaObject::Type *rttiType = NULL;

   if(type && !(rttiType = MetaObject::Type::FindType<MetaObject::Type>(type)))
      I_Error("MetaTable::getNextKeyAndType: %s is not a MetaObject!\n", type);

   return getNextKeyAndType(object, key, rttiType);
}

//
// MetaTable::tableIterator
//
// Iterates on all objects in the metatable, regardless of key or type.
//
MetaObject *MetaTable::tableIterator(MetaObject *object) const
{
   return pImpl->keyhash.tableIterator(object);
}

//
// MetaTable::addInt
//
// Add an integer to the metatable.
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
int MetaTable::getInt(const char *key, int defValue)
{
   int retval;
   MetaObject *obj;

   metaerrno = META_ERR_NOERR;

   if(!(obj = getObjectKeyAndType(key, RTTI(MetaInteger))))
   {
      metaerrno = META_ERR_NOSUCHOBJECT;
      retval = defValue;
   }
   else
      retval = static_cast<MetaInteger *>(obj)->value;

   return retval;
}

//
// MetaTable::setInt
//
// If the metatable already contains a metaint of the given name, it will
// be edited to have the provided value. Otherwise, a new metaint will be
// added to the table with that value.
//
void MetaTable::setInt(const char *key, int newValue)
{
   MetaObject *obj;

   if(!(obj = getObjectKeyAndType(key, RTTI(MetaInteger))))
      addInt(key, newValue);
   else
      static_cast<MetaInteger *>(obj)->value = newValue;
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
   while((obj = tableIterator(NULL)))
   {
      removeObject(obj);

      delete obj;
   }
}

// EOF

