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
// Purpose: Aeon wrapper for fixed_t
// Authors: Max Waine
//

#ifndef AEON_FIXED_H__
#define AEON_FIXED_H__

#include <stdint.h>

#include "m_fixed.h"

class AeonFixed
{
public:
   fixed_t value;

   AeonFixed() : value(0)
   {
   }

   AeonFixed(fixed_t value) : value(value)
   {
   }

   AeonFixed operator + (const AeonFixed &in);
   AeonFixed operator + (const int val);
   AeonFixed operator - (const AeonFixed &in);
   AeonFixed operator - (const int val);
   AeonFixed operator * (const AeonFixed &in);
   AeonFixed operator * (const int val);
   AeonFixed operator / (const AeonFixed &in);
   AeonFixed operator / (const int val);

   AeonFixed &operator += (const AeonFixed &in);
   AeonFixed &operator += (const int val);
   AeonFixed &operator -= (const AeonFixed &in);
   AeonFixed &operator -= (const int val);
   AeonFixed &operator *= (const AeonFixed &in);
   AeonFixed &operator *= (const int val);
   AeonFixed &operator /= (const AeonFixed &in);
   AeonFixed &operator /= (const int val);

   operator double() const;
   operator fixed_t() const { return value; }
};

class AeonScriptObjFixed
{
public:
   static void Construct(AeonFixed *thisFixed);
   static void ConstructFromOther(const AeonFixed &other, AeonFixed *thisFixed);
   static void ConstructFromDouble(double other, AeonFixed *thisFixed);
   static void ConstructFromInt(int other, AeonFixed *thisFixed);
   static void ConstructFromPair(int16_t integer, double frac, AeonFixed *thisFixed);

   static void Init();
};

#endif

// EOF

