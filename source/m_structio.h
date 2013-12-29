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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Object-oriented portable structure IO
//
//-----------------------------------------------------------------------------

#ifndef M_STRUCTIO_H__
#define M_STRUCTIO_H__

#include "m_buffer.h"

//
// Abstract base class for all structure field descriptors.
// This allows all the descriptors for a class to be iterated over in order
// even though they point to different member variable types.
//
template<typename S> class MDescriptorBase
{
protected:
   MDescriptorBase<S> *next; // Next descriptor for the structure

public:
   MDescriptorBase<S>(MDescriptorBase<S> *pNext) : next(pNext)
   {
   }

   // Pure virtual. Override in each descriptor class to read that
   // particular type of field out of an InBuffer and then store the
   // value read into the structure instance. Return false if an IO
   // error occurs.
   virtual bool readField(S &structure, InBuffer &fin) = 0;

   // Get the next descriptor for the structure.
   MDescriptorBase<S> *nextField() const { return next; }
};

//
// Descriptor class for uint16_t structure fields.
//
template<typename S, uint16_t S::*field> 
class MUint16Descriptor : public MDescriptorBase<S>
{
public:
   typedef MDescriptorBase<S> Super;

   MUint16Descriptor(Super *pNext) : Super(pNext) {}

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

   MUint32Descriptor(Super *pNext) : Super(pNext) {}

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

protected:
   FieldType *firstField; // Descriptor for the first field in the structure.

public:
   // Pass the address of the first structure field's descriptor. The remaining
   // descriptors must link to each other via their constructors, in order to
   // form a linked list of field descriptors.
   MStructReader(FieldType *pFirstField) : firstField(pFirstField) {}

   // Read all fields for this structure from an InBuffer, in the order their
   // descriptors are linked together. Returns false if an IO error occurs, and
   // true otherwise.
   bool readFields(S &instance, InBuffer &fin)
   {
      FieldType *field = firstField;

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

