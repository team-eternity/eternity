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
//    Generic hash table implementation - Hash Key classes
//
//-----------------------------------------------------------------------------

#ifndef E_HASHKEYS_H__
#define E_HASHKEYS_H__

unsigned int D_HashTableKey(const char *str);
unsigned int D_HashTableKeyCase(const char *str);

//
// E*HashKey
//
// haleyjd 12/12/10: These POD classes replace ehashable_i with go-between 
// objects that know how to deal with the internal key of the type of object 
// being stored in an EHashTable, while not restricting a given type to only 
// being able to use EHashTable in a single way, as it would if an inheritance
// solution were used.
//
// The EHashTable class specifies the following interface which to key objects
// must adhere:
// * They should expose the type of their basic literal key field in a 
//   public typedef called basic_type.
// * They should support an operator = accepting the same type for assignment
//   purposes.
// * They should define a hashCode method returning an unsigned int.
// * They should be comparable via defining operators == and !=.
//
// Specializations are provided here for integers, C strings, and case-
// insensitive C strings.
//

//
// POD integer hash key
//
class EIntHashKey
{
public:
   typedef int basic_type;

   int hashKey;

   EIntHashKey &operator = (int key) { hashKey = key; return *this; }
   
   unsigned int hashCode() const 
   { 
      return (unsigned int)hashKey;
   }

   bool operator == (const EIntHashKey &other) const
   { 
      return (hashKey == other.hashKey);
   }
   
   bool operator != (const EIntHashKey &other) const
   {
      return (hashKey != other.hashKey);
   }

   operator int () const { return hashKey; }
};

//
// POD case sensitive string key
//
class EStringHashKey
{
public: 
   typedef const char * basic_type;

   const char *hashKey;

   EStringHashKey &operator = (const char *key) { hashKey = key; return *this; }
   
   unsigned int hashCode() const
   {
      return D_HashTableKeyCase(hashKey);
   }
   
   bool operator == (const EStringHashKey &other) const
   {
      return !strcmp(hashKey, other.hashKey);
   }
   
   bool operator != (const EStringHashKey &other) const
   {
      return (strcmp(hashKey, other.hashKey) != 0);
   }

   operator const char * () const { return hashKey; }
};

// 
// POD case-insensitive string key
//
class ENCStringHashKey
{
public:
   typedef const char * basic_type;

   ENCStringHashKey &operator = (const char *key) { hashKey = key; return *this; }

   const char *hashKey;
   
   unsigned int hashCode() const
   {
      return D_HashTableKey(hashKey);
   }
   
   bool operator == (const ENCStringHashKey &other) const
   {
      return !strcasecmp(hashKey, other.hashKey);
   }
   
   bool operator != (const ENCStringHashKey &other) const
   {
      return (strcasecmp(hashKey, other.hashKey) != 0);
   }

   operator const char * () const { return hashKey; }
};

#endif

// EOF

