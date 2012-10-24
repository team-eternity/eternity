// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Object-oriented portable structure IO
//
//-----------------------------------------------------------------------------

#ifndef M_STRUCTIO_H__
#define M_STRUCTIO_H__

#include "m_buffer.h"

// Base class
template<typename S> class MDescriptorBase
{
protected:
   MDescriptorBase<S> *next;

public:
   virtual bool readField(S &structure, InBuffer &fin) = 0;

   MDescriptorBase<S> *nextField() const { return next; }
};

template<typename S, uint16_t S::*field> 
class MUint16Descriptor : public MDescriptorBase<S>
{
public:
   typedef MDescriptorBase<S> Super;

   MUint16Descriptor(Super *pNext)
   {
      this->next = pNext;
   }

   bool readField(S &structure, InBuffer &fin)
   {
      return fin.ReadUint16(structure.*field);
   }
};

template<typename S, uint32_t S::*field>
class MUint32Descriptor : public MDescriptorBase<S>
{
public:
   typedef MDescriptorBase<S> Super;

   MUint32Descriptor(Super *pNext)
   {
      this->next = pNext;
   }

   bool readField(S &structure, InBuffer &fin)
   {
      return fin.ReadUint32(structure.*field);
   }
};

template<typename S> class MStructReader
{
public:
   typedef MDescriptorBase<S> FieldType;

protected:
   FieldType *firstField;

public:
   MStructReader(FieldType *pFirstField) : firstField(pFirstField) {}

   bool readFields(S &instance, InBuffer &fin)
   {
      FieldType *field = firstField;

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

