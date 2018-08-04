//
// The Eternity Engine
// Copyright(C) 2018 James Haley, Max Waine, et al.
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
// Purpose: Aeon fixed_t functions
// Authors: Max Waine
//

#ifndef AEON_FIXED_H__
#define AEON_FIXED_H__

#include "m_fixed.h"

class ASFixed
{
public:
   fixed_t value;

   ASFixed() : value(0)
   {
   }

   ASFixed(fixed_t value) : value(value)
   {
   }

   ASFixed operator + (const ASFixed &other);
   ASFixed operator + (const fixed_t other);
   ASFixed &operator += (const ASFixed &in);
   ASFixed &operator += (const int val);
   ASFixed operator * (const ASFixed &other);
   ASFixed operator * (const fixed_t other);
   ASFixed &operator *= (const ASFixed &in);
   ASFixed &operator *= (const int val);
   ASFixed operator / (const ASFixed &other);
   ASFixed operator / (const fixed_t other);
   ASFixed &operator /= (const ASFixed &in);
   ASFixed &operator /= (const int val);

   operator double() { return M_FixedToDouble(value);  }
};

class ASScriptObjFixed
{
public:
   static void Construct(ASFixed *thisFixed);
   static void ConstructFromOther(const ASFixed &other, ASFixed *thisFixed);
   static void ConstructFromDouble(double other, ASFixed *thisFixed);
   static void ConstructFromInt(int other, ASFixed *thisFixed);

   static void Init(asIScriptEngine *e);
};

#endif

// EOF

