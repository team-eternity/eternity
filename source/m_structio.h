//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Object-oriented portable structure IO.
// Authors: James Haley
//

#ifndef M_STRUCTIO_H__
#define M_STRUCTIO_H__

#include "m_buffer.h"
#include "m_collection.h"

//
// Structure field descriptor class
// Stores the type of the field and a pointer-to-member to that field.
//
template<typename S>
class MFieldDescriptor
{
public:
   enum mfdtype_e
   {
      MFD_UINT16,
      MFD_UINT32
   };

protected:
   // pointer-to-member
   union mfdfield_u
   {
      uint16_t S::*u16;
      uint32_t S::*u32;
   } fields;

   // type of field
   mfdtype_e type;

public:
   MFieldDescriptor(uint16_t S::*field) : type(MFD_UINT16)
   {
      fields.u16 = field;
   }

   MFieldDescriptor(uint32_t S::*field) : type(MFD_UINT32)
   {
      fields.u32 = field;
   }

   MFieldDescriptor(const MFieldDescriptor &other) : type(other.type)
   {
      switch(type)
      {
      case MFD_UINT16:
         fields.u16 = other.fields.u16;
         break;
      case MFD_UINT32:
         fields.u32 = other.fields.u32;
         break;
      }
   }

   MFieldDescriptor(MFieldDescriptor &&other) noexcept
      : type(other.type)
   {
      switch(type)
      {
      case MFD_UINT16:
         fields.u16 = other.fields.u16;
         break;
      case MFD_UINT32:
         fields.u32 = other.fields.u32;
         break;
      }
   }

   // Returns true if the field was successfully read from the input stream, 
   // and false otherwise.
   bool readField(S &instance, InBuffer &fin)
   {
      switch(type)
      {
      case MFD_UINT16:
         return fin.readUint16(instance.*(fields.u16));
      case MFD_UINT32:
         return fin.readUint32(instance.*(fields.u32));
      default:
         return false;
      }
   }
};

//
// Object-oriented structure reader. Use a callback during construction to add
// fields in the structure to this reader. Call the readFields method with an
// instance of the structure type to read all its fields from an InBuffer using
// alignment- and endianness-agnostic reads.
//
template<typename S>
class MStructReader
{
public:
   using fieldadd_t = void (*)(MStructReader<S> &);

protected:
   Collection<MFieldDescriptor<S>> descriptors;

public:
   MStructReader(fieldadd_t fieldAdder) : descriptors()
   {
      fieldAdder(*this);
   }

   // Add a field to the structure reader
   template<typename T>
   void addField(T S::*field)
   {
      descriptors.add(MFieldDescriptor<S>(field));
   }

   // Read all the structure's fields from the input buffer. 
   // Returns true if all reads were successful, false otherwise.
   bool readFields(S &instance, InBuffer &fin)
   {
      for(auto &obj : descriptors)
      {
         if(!obj.readField(instance, fin))
            return false;
      }

      return true;
   }
};

#endif

// EOF

