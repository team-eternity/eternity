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
   MetaState() : Super(), state(nullptr)
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
   virtual MetaObject *clone() const override { return new MetaState(*this); }

   // toString - virtual method for nice display of metastate properties.
   virtual const char *toString() const override { return state->name; }
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
   virtual MetaObject *clone() const override { return new MetaDropItem(*this); }

   // toString - virtual method for nice display of metastate properties.
   virtual const char *toString() const override { return item.constPtr(); }
};

enum bloodaction_e : int;
enum bloodtype_e : int;

//
// MetaBloodBehavior
//
// Specify the behavior of a blood object when spawned for different actions.
//
class MetaBloodBehavior : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaBloodBehavior, MetaObject)

public:
   bloodaction_e action;
   bloodtype_e   behavior;
   
   // Default constructor
   MetaBloodBehavior() : Super(), action(), behavior()
   {
   }

   // Parameterized constructor
   MetaBloodBehavior(const char *key, bloodaction_e pAction, bloodtype_e pBehavior)
      : Super(key), action(pAction), behavior(pBehavior)
   {
   }

   // Copy constructor
   MetaBloodBehavior(const MetaBloodBehavior &other) 
      : Super(other), action(other.action), behavior(other.behavior)
   {
   }

   // Clone - virtual copy constructor
   virtual MetaObject *clone() const override { return new MetaBloodBehavior(*this); }

   // toString - virtual method for nice display of metastate properties.
   virtual const char *toString() const override
   { 
      return "MetaBloodBehavior"; // TODO
   }
};

#endif

// EOF

