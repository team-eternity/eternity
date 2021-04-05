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

#include "doomtype.h"
#include "m_fixed.h"
#include "m_vector.h"
#include "tables.h"

namespace Aeon
{
   class ScriptObjMath
   {
   public:
      static void Init();
   };

   class ScriptObjFixed
   {
   public:
      static void    Construct(fixed_t *thisFixed);
      static void    ConstructFromOther(const fixed_t other, fixed_t *thisFixed);
      static void    ConstructFromDouble(double other, fixed_t *thisFixed);
      static void    ConstructFromInt(int other, fixed_t *thisFixed);
      static void    ConstructFromPair(int16_t integer, double frac, fixed_t *thisFixed);
      static fixed_t ConstructFromBits(int bits);

      static void Init();
   };

   class ScriptObjAngle
   {
   public:
      static void    Construct(angle_t *thisAngle);
      static void    ConstructFromOther(const angle_t other, angle_t *thisAngle);
      static void    ConstructFromDouble(double other, angle_t *thisAngle);
      static void    ConstructFromInt(int other, angle_t *thisAngle);
      static angle_t ConstructFromBits(angle_t bits);

      static void Init();
   };

   class ScriptObjVector2
   {
   public:
      static void Construct(v2fixed_t *thisVector);
      static void ConstructFromOther(const v2fixed_t other, v2fixed_t *thisVector);
      static void ConstructFromFixed(fixed_t x, fixed_t y, fixed_t z, v2fixed_t *thisVector);

      static void Init();

   };

   class ScriptObjVector3
   {
   public:
      static void Construct(v3fixed_t *thisVector);
      static void ConstructFromOther(const v3fixed_t other, v3fixed_t *thisVector);
      static void ConstructFromFixed(fixed_t x, fixed_t y, fixed_t z, v3fixed_t *thisVector);

      static void Init();
   };
}

#endif

// EOF

