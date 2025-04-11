//
// Copyright(C) 2012 David Hill, James Haley, et al.
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//    Base for classes with custom RTTI. Adapted from the original ThinkerType.
//
//-----------------------------------------------------------------------------

#include "e_rtti.h"

#include "d_dehtbl.h"
#include "i_system.h"


RTTIObject::Type RTTIObject::StaticType("RTTIObject", nullptr);

RTTIObject::Type *RTTIObject::Type::rttiTypes[NUMTYPECHAINS];

//
// RTTIObject::Type::FindType
//
RTTIObject::Type *RTTIObject::Type::FindType(const char *pName)
{
   unsigned int hashcode = D_HashTableKeyCase(pName) % NUMTYPECHAINS;
   Type *chain = rttiTypes[hashcode];

   while(chain && strcmp(chain->name, pName))
      chain = chain->next;

   return chain;
}

//
// RTTIObject::Type::addType
//
void RTTIObject::Type::addType()
{
   unsigned int hashcode;

   // Types must be singletons with unique names.
   if(FindType(name))
      I_Error("RTTIObject::Type: duplicate class registered with name '%s'\n", name);

   // Add it to the hash table; order is unimportant.
   hashcode = D_HashTableKeyCase(name) % NUMTYPECHAINS;
   this->next = rttiTypes[hashcode];
   rttiTypes[hashcode] = this;
}

// EOF

