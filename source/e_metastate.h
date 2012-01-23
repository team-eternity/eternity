// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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

//
// MetaState
//
// With DECORATE state support, it is necessary to allow storage of arbitrary
// states in mobjinfo.
//
class MetaState : public MetaObject
{
protected:
   state_t *state;      // the state

public:
   // Constructor
   MetaState(const char *key, state_t *pState) 
      : MetaObject("MetaState", key), state(pState)
   {
   }

   // Copy Constructor
   MetaState(const MetaState &other) : MetaObject(other)
   {
      this->state = other.state;
   }
   
   // Accessors
   state_t *getValue() const { return state; }
   void setValue(state_t *s) { state = s;    }

   // Clone - virtual copy constructor
   virtual MetaObject *clone() const { return new MetaState(*this); }

   // toString - virtual method for nice display of metastate properties.
   virtual const char *toString() const { return state->name; }
};

#endif

// EOF

