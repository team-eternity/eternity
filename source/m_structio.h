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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Object-oriented portable structure IO
//
//-----------------------------------------------------------------------------

#ifndef M_STRUCTIO_H__
#define M_STRUCTIO_H__

#include "z_zone.h"
#include "m_buffer.h"
#include "m_dllist.h"

//
// Abstract base class for all structure field descriptors.
// This allows all the descriptors for a class to be iterated over in order
// even though they point to different member variable types.
//
template<typename S> class MDescriptorBase : public ZoneObject
{
public:
   DLListItem<MDescriptorBase<S>> links;

   MDescriptorBase<S>() : ZoneObject(), links()
   {
   }

   // Pure virtual. Override in each descriptor class to read that
   // particular type of field out of an InBuffer and then store the
   // value read into the structure instance. Return false if an IO
   // error occurs.
   virtual bool readField(S &structure, InBuffer &fin) = 0;

   // Get the next descriptor for the structure.
   MDescriptorBase<S> *nextField() const 
   { 
      return links.dllNext ? links.dllNext->dllObject : nullptr;
   }
};

//
// Descriptor class for uint16_t structure fields.
//
template<typename S, uint16_t S::*field> 
class MUint16Descriptor : public MDescriptorBase<S>
{
public:
   typedef MDescriptorBase<S> Super;

   MUint16Descriptor() : Super() {}

   // Read a uint16_t from the InBuffer and assign it to the field
   // this descriptor represents.
   bool readField(S &structure, InBuffer &fin)
   {
      return fin.readUint16(structure.*field);
   }
};

//
// Descriptor class for a uint32_t structure field.
//
template<typename S, uint32_t S::*field>
class MUint32Descriptor : public MDescriptorBase<S>
{
public:
   typedef MDescriptorBase<S> Super;

   MUint32Descriptor() : Super() {}

   // Read a uint32_t from the InBuffer and assign it to the field
   // this descriptor represents.
   bool readField(S &structure, InBuffer &fin)
   {
      return fin.readUint32(structure.*field);
   }
};

template<typename S> class MStructReader
{
public:
   typedef MDescriptorBase<S> FieldType;
   typedef void (*fieldadd_t)(MStructReader<S> &);

protected:
   DLList<FieldType, &FieldType::links> descriptors; // List of field descriptors

public:
   // Pass the address of the first structure field's descriptor. The remaining
   // descriptors must link to each other via their constructors, in order to
   // form a linked list of field descriptors.
   MStructReader(fieldadd_t fieldAdder) : descriptors() 
   {
      fieldAdder(*this);
   }

   void addField(FieldType *field)
   {
      descriptors.insert(field);
   }

   // Read all fields for this structure from an InBuffer, in the order their
   // descriptors are linked together. Returns false if an IO error occurs, and
   // true otherwise.
   bool readFields(S &instance, InBuffer &fin)
   {
      if(!descriptors.head)
         return false;

      FieldType *field = descriptors.head->dllObject;

      // walk the descriptor list
      while(field)
      {
         if(!field->readField(instance, fin))
            return false;

         field = field->nextField();
      }

      return true;
   }
};

#endif

// EOF

