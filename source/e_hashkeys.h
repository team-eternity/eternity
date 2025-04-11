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
// * They should expose a secondary type that is comparable with the basic key
//   type for use as the function parameter to EHashTable's objectForKey and 
//   chainForKey methods. This can be the same as the basic type, or different.
// * They should define a HashCode method returning an unsigned int. If the
//   basic and param types differ, implement overloads for both types.
// * They should define a Compare method taking one param_type and one 
//   basic_type parameter, and returning boolean value true if there is a 
//   match, and false otherwise.
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
   typedef int param_type;

   static unsigned int HashCode(int input)
   {
      return (unsigned int)input;
   }

   static bool Compare(int first, int second)
   {
      return (first == second);
   }
};

//
// For two integers
//
class EInt64HashKey
{
public:
   typedef int64_t basic_type;
   typedef int64_t param_type;

   static unsigned HashCode(int64_t input)
   {
      return static_cast<unsigned>(input ^ (input >> 32));
   }

   static bool Compare(int64_t first, int64_t second)
   {
      return first == second;
   }
};

//
// POD case sensitive string key
//
class EStringHashKey
{
public: 
   typedef const char *basic_type;
   typedef const char *param_type;

   static unsigned int HashCode(const char *input)
   {
      return D_HashTableKeyCase(input);
   }

   static bool Compare(const char *first, const char *second)
   {
      return !strcmp(first, second);
   }
};

// 
// POD case-insensitive string key
//
class ENCStringHashKey
{
public:
   typedef const char *basic_type;
   typedef const char *param_type;

   static unsigned int HashCode(const char *input)
   {
      return D_HashTableKey(input);
   }

   static bool Compare(const char *first, const char *second)
   {
      return !strcasecmp(first, second);
   }
};

#endif

// EOF

