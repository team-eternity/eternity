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
// Purpose: Aeon maths bindings
// Authors: Max Waine
//

#ifndef AEON_MATH_H__
#define AEON_MATH_H__

#include <stdint.h>

#include "m_fixed.h"

#include "tables.h"

namespace Aeon
{
   class ScriptObjMath
   {
   public:
      static void Init();
   };

   class Fixed
   {
   public:
      fixed_t value;

      Fixed() : value(0)
      {
      }

      Fixed(fixed_t value) : value(value)
      {
      }

      Fixed operator +  (const Fixed &in);
      Fixed operator +  (const int val);
      Fixed operator -  (const Fixed &in);
      Fixed operator -  (const int val);
      Fixed operator *  (const Fixed &in);
      Fixed operator *  (const int val);
      Fixed operator /  (const Fixed &in);
      Fixed operator /  (const int val);
      Fixed operator << (const int val);

      Fixed &operator += (const Fixed &in);
      Fixed &operator += (const int val);
      Fixed &operator -= (const Fixed &in);
      Fixed &operator -= (const int val);
      Fixed &operator *= (const Fixed &in);
      Fixed &operator *= (const int val);
      Fixed &operator /= (const Fixed &in);
      Fixed &operator /= (const int val);

      operator double() const;
      operator fixed_t() const { return value; }
   };

   class Angle
   {
   public:
      angle_t value;

      Angle() : value(0)
      {
      }

      Angle(fixed_t value) : value(value)
      {
      }

      Angle operator + (const Angle &in);
      Angle operator + (const Fixed &in);
      Angle operator + (const int val);

      Angle operator - (const Angle &in);
      Angle operator - (const Fixed &in);
      Angle operator - (const int val);

      Angle operator * (const Fixed &in);
      Angle operator * (const int val);

      Angle operator / (const Angle &in);
      Angle operator / (const Fixed &in);
      Angle operator / (const int val);

      Angle &operator += (const Angle &in);
      Angle &operator += (const Fixed &in);
      Angle &operator += (const int val);

      Angle &operator -= (const Angle &in);
      Angle &operator -= (const Fixed &in);
      Angle &operator -= (const int val);

      Angle &operator *= (const Fixed &in);
      Angle &operator *= (const int val);

      Angle &operator /= (const Angle &in);
      Angle &operator /= (const Fixed &in);
      Angle &operator /= (const int val);

      operator Fixed() const;
      operator angle_t() const { return value; }
   };

   class ScriptObjFixed
   {
   public:
      static void Construct(Fixed *thisFixed);
      static void ConstructFromOther(const Fixed &other, Fixed *thisFixed);
      static void ConstructFromDouble(double other, Fixed *thisFixed);
      static void ConstructFromInt(int other, Fixed *thisFixed);
      static void ConstructFromPair(int16_t integer, double frac, Fixed *thisFixed);
      static Fixed ConstructFromBits(int bits);

      static void Init();
   };

   class ScriptObjAngle
   {
   public:
      static void Construct(Angle *thisAngle);
      static void ConstructFromOther(const Angle &other, Angle *thisAngle);
      static void ConstructFromDouble(double other, Angle *thisAngle);
      static void ConstructFromInt(int other, Angle *thisAngle);

      static void Init();
   };

   class Vector;

   class ScriptObjVector
   {
   public:
      static void Construct(Vector *thisVector);
      static void ConstructFromOther(const Vector &other, Vector *thisVector);
      static void ConstructFromFixed(fixed_t x, fixed_t y, fixed_t z, Vector *thisVector);

      static void Init();
   };
}

#endif

// EOF

