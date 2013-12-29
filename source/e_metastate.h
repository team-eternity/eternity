// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
//
// Metastates
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_METASTATE_H__
#define E_METASTATE_H__

#include "info.h"
#include "metaapi.h"
#include "m_qstr.h"

//
// MetaState
//
// With DECORATE state support, it is necessary to allow storage of arbitrary
// states in mobjinfo.
//
class MetaState : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaState, MetaObject)

public:
   state_t *state;      // the state

   // Default constructor
   MetaState() : Super(), state(NULL)
   {
   }

   // Parameterized constructor
   MetaState(const char *key, state_t *pState) : Super(key), state(pState)
   {
   }

   // Copy constructor
   MetaState(const MetaState &other) 
      : Super(other), state(other.state)
   {
   }

   // Clone - virtual copy constructor
   virtual MetaObject *clone() const { return new MetaState(*this); }

   // toString - virtual method for nice display of metastate properties.
   virtual const char *toString() const { return state->name; }
};

//
// MetaDropItem
//
// Allow storage of multiple drop item types in mobjinfo
//
class MetaDropItem : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaDropItem, MetaObject)

public:
   qstring item;
   int     chance;
   int     amount;
   bool    toss;

   // Default constructor
   MetaDropItem() : Super(), item(), chance(255), amount(0), toss(false)
   {
   }

   // Parameterized constructor
   MetaDropItem(const char *key, const char *p_item, int p_chance, 
                int p_amount, bool p_toss)
      : Super(key), item(p_item), chance(p_chance), amount(p_amount), 
        toss(p_toss)
   {
   }

   // Copy constructor
   MetaDropItem(const MetaDropItem &other) 
      : Super(other), item(other.item), chance(other.chance), 
        amount(other.amount), toss(other.toss)
   {
   }

   // Clone - virtual copy constructor
   virtual MetaObject *clone() const { return new MetaDropItem(*this); }

   // toString - virtual method for nice display of metastate properties.
   virtual const char *toString() const { return item.constPtr(); }
};

#endif

// EOF

