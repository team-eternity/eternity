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
// Purpose: Aeon wrapper for angle_t
// Authors: Max Waine
//

#ifndef AEON_ANGLE_H__
#define AEON_ANGLE_H__

#include "tables.h"

class AeonFixed;

class AeonAngle
{
public:
   angle_t value;

   AeonAngle() : value(0)
   {
   }

   AeonAngle(fixed_t value) : value(value)
   {
   }

   AeonAngle operator + (const AeonAngle &in);
   AeonAngle operator + (const AeonFixed &in);
   AeonAngle operator + (const int val);

   AeonAngle operator * (const AeonFixed &in);
   AeonAngle operator * (const int val);

   AeonAngle operator / (const AeonAngle &in);
   AeonAngle operator / (const AeonFixed &in);
   AeonAngle operator / (const int val);

   AeonAngle &operator += (const AeonAngle &in);
   AeonAngle &operator += (const AeonFixed &in);
   AeonAngle &operator += (const int val);

   AeonAngle &operator *= (const AeonFixed &in);
   AeonAngle &operator *= (const int val);

   AeonAngle &operator /= (const AeonAngle &in);
   AeonAngle &operator /= (const AeonFixed &in);
   AeonAngle &operator /= (const int val);

   operator AeonFixed() const;
};

class AeonScriptObjAngle
{
public:
   static void Construct(AeonAngle *thisAngle);
   static void ConstructFromOther(const AeonAngle &other, AeonAngle *thisAngle);
   static void ConstructFromDouble(double other, AeonAngle *thisAngle);
   static void ConstructFromInt(int other, AeonAngle *thisAngle);

   static void Init();
};

#endif

// EOF

