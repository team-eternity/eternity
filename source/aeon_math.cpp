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

#include "aeon_common.h"
#include "aeon_math.h"
#include "aeon_system.h"
#include "c_io.h"
#include "m_fixed.h"
#include "m_random.h"
#include "m_vector.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "tables.h"

namespace Aeon
{
   //=============================================================================
   //
   // Aeon Math Class
   //

   class Math
   {
   public:
      static fixed_t Sin(const angle_t val)
      {
         return fixed_t(finesine[val >> ANGLETOFINESHIFT]);
      }

      static fixed_t Cos(const angle_t val)
      {
         return fixed_t(finecosine[val >> ANGLETOFINESHIFT]);
      }

      static fixed_t Tan(const angle_t val)
      {
         return fixed_t(finetangent[val >> ANGLETOFINESHIFT]);
      }

      static fixed_t Atan2(const fixed_t y, const fixed_t x)
      {
         return fixed_t(P_PointToAngle(0, 0, x, y));
      }

      static fixed_t Fabs(const fixed_t val)
      {
         return fixed_t(D_abs(val));
      }

      static fixed_t Sqrt(const fixed_t val)
      {
         return fixed_t(M_DoubleToFixed(sqrt(M_FixedToDouble(val))));
      }

      static int Abs(const int val)
      {
         return abs(val);
      }

      static fixed_t Ceil(const fixed_t val)
      {
         if(val & (FRACUNIT - 1))
            return fixed_t((val & ~(FRACUNIT - 1)) + FRACUNIT);
         else
            return val;
      }

      static fixed_t Floor(const fixed_t val)
      {
         return fixed_t(val & ~(FRACUNIT - 1));
      }
   };

   class Rand
   {
   public:
      static unsigned int UInt()             { return P_RandomEx(pr_aeon);              }
      static uint8_t      Byte()             { return P_Random(pr_aeon);                }
      static unsigned int Max(const int max) { return P_RangeRandomEx(pr_aeon, 0, max); }
      static fixed_t      Fixed()            { return fixed_t(P_RandomEx(pr_aeon)); }
      static int RangeUInt(const int min, const int max)
      {
         return P_RangeRandomEx(pr_aeon, min, max);
      }
      static int RangeByte(const uint8_t min, const uint8_t max)
      {
         return P_RangeRandom(pr_aeon, min, max);
      }
      static fixed_t RangeFixed(const fixed_t min, const fixed_t max)
      {
         if(min >= 0)
            return fixed_t(P_RangeRandomEx(pr_aeon, min, max));
         return fixed_t(P_RangeRandomEx(pr_aeon, min - min, max - min) + min);
      }

      static int SubInt(const unsigned int max) { return P_SubRandomEx(pr_aeon, max); }
      /*static fixed_t SubFixed(const fixed_t max)
      {
         return fixed_t(P_SubRandomEx(pr_script, max));
      }*/
   };

   static const aeonfuncreg_t mathFuncs[]
   {
      { "fixed_t Sin(const angle_t val)",                  WRAP_FN(Math::Sin)   },
      { "fixed_t Cos(const angle_t val)",                  WRAP_FN(Math::Cos)   },
      { "fixed_t Tan(const angle_t val)",                  WRAP_FN(Math::Tan)   },
      { "fixed_t Atan2(const fixed_t y, const fixed_t x)", WRAP_FN(Math::Atan2) },
      { "fixed_t Fabs(const fixed_t val)",                 WRAP_FN(Math::Fabs)  },
      { "fixed_t Sqrt(const fixed_t val)",                 WRAP_FN(Math::Sqrt)  },
      { "int     Abs(const int)",                          WRAP_FN(Math::Abs)   },
      { "fixed_t Ceil(const fixed_t val)",                 WRAP_FN(Math::Ceil)  },
      { "fixed_t Floor(const fixed_t val)",                WRAP_FN(Math::Floor) },
   };

   static const aeonfuncreg_t randFuncs[]
   {
      { "uint    UInt()",                                           WRAP_FN(Rand::UInt)       },
      { "uint8   Byte()",                                           WRAP_FN(Rand::Byte)       },
      { "uint    Max(const uint max)",                              WRAP_FN(Rand::UInt)       },
      { "fixed_t Fixed()",                                          WRAP_FN(Rand::Fixed)      },
      { "uint    RangeUInt(const uint min, const uint max)",        WRAP_FN(Rand::RangeUInt)  },
      { "uint8   RangeByte(const uint8 min, const uint8 max)",      WRAP_FN(Rand::RangeByte)  },
      { "fixed_t RangeFixed(const fixed_t min, const fixed_t max)", WRAP_FN(Rand::RangeFixed) },
      { "int     SubUInt()",                                        WRAP_FN(Rand::SubInt)     },

   };

   void ScriptObjMath::Init()
   {
      asIScriptEngine *const e = ScriptManager::Engine();

      e->SetDefaultNamespace("Math");
      for(const aeonfuncreg_t &fn : mathFuncs)
         e->RegisterGlobalFunction(fn.declaration, fn.funcPointer, asCALL_GENERIC);
      e->SetDefaultNamespace("Math::Rand");
      for(const aeonfuncreg_t &fn : randFuncs)
         e->RegisterGlobalFunction(fn.declaration, fn.funcPointer, asCALL_GENERIC);
      e->SetDefaultNamespace("");
   }

   //=============================================================================
   //
   // Aeon fixed_t
   //

   fixed_t FixedAddFixed(const fixed_t *lval, const fixed_t rval) { return *lval + rval;              }
   fixed_t FixedAddInt  (const fixed_t *lval, const int rval)     { return *lval + (rval * FRACUNIT); }
   fixed_t FixedSubFixed(const fixed_t *lval, const fixed_t rval) { return *lval - rval;              }
   fixed_t FixedSubInt  (const fixed_t *lval, const int rval)     { return *lval - (rval * FRACUNIT); }
   fixed_t FixedMulFixed(const fixed_t *lval, const fixed_t rval) { return FixedMul(*lval, rval);     }
   fixed_t FixedMulInt  (const fixed_t *lval, const int rval)     { return *lval * rval;              }
   fixed_t FixedDivFixed(const fixed_t *lval, const fixed_t rval) { return FixedDiv(*lval, rval);     }
   fixed_t FixedDivInt  (const fixed_t *lval, const int rval)     { return *lval / rval;              }
   fixed_t FixedLshInt  (const fixed_t *lval, const int rval)     { return *lval << rval;             }

   fixed_t &FixedAssignAddFixed(fixed_t *lval, const fixed_t rval)
   {
      *lval += rval;
      return *lval;
   }
   fixed_t &FixedAssignAddInt(fixed_t *lval, const int rval)
   {
      *lval += rval * FRACUNIT;
      return *lval;
   }
   fixed_t &FixedAssignSubFixed(fixed_t *lval, const fixed_t rval)
   {
      *lval -= rval;
      return *lval;
   }
   fixed_t &FixedAssignSubInt(fixed_t *lval, const int rval)
   {
      *lval -= rval * FRACUNIT;
      return *lval;
   }
   fixed_t &FixedAssignMulFixed(fixed_t *lval, const fixed_t rval)
   {
      *lval = FixedMul(*lval, rval);
      return *lval;
   }
   fixed_t &FixedAssignMulInt(fixed_t *lval, const int rval)
   {
      *lval *= rval;
      return *lval;
   }
   fixed_t &FixedAssignDivFixed(fixed_t *lval, const fixed_t rval)
   {
      *lval = FixedDiv(*lval, rval);
      return *lval;
   }
   fixed_t &FixedAssignDivInt(fixed_t *lval, const int rval)
   {
      *lval /= rval;
      return *lval;
   }

   double FixedToDouble(const fixed_t *value) { return M_FixedToDouble(*value); }

   void ScriptObjFixed::Construct(fixed_t *thisFixed)
   {
      *thisFixed = 0;
   }

   void ScriptObjFixed::ConstructFromOther(const fixed_t other, fixed_t *thisFixed)
   {
      *thisFixed = other;
   }

   void ScriptObjFixed::ConstructFromDouble(const double other, fixed_t *thisFixed)
   {
      *thisFixed = M_DoubleToFixed(other);
   }

   void ScriptObjFixed::ConstructFromInt(const int other, fixed_t *thisFixed)
   {
      *thisFixed = other * FRACUNIT;
   }

   void ScriptObjFixed::ConstructFromPair(const int16_t integer, const double frac, fixed_t *thisFixed)
   {
      *thisFixed = static_cast<int32_t>(integer) * FRACUNIT;
      if(frac < 1.0 && frac >= 0.0)
         *thisFixed |= M_DoubleToFixed(frac);
   }

   fixed_t ScriptObjFixed::ConstructFromBits(const int bits)
   {
      return bits;
   }

   static void asPrint(const fixed_t f)
   {
      C_Printf("%.11f\n", M_FixedToDouble(f));
   }

   static const aeonfuncreg_t fixedFuncs[] =
   {
      { "fixed_t opAdd(const fixed_t val) const",  WRAP_OBJ_FIRST(FixedAddFixed)       },
      { "fixed_t opAdd(const int val) const",      WRAP_OBJ_FIRST(FixedAddInt),        },
      { "fixed_t opSub(const fixed_t val) const",  WRAP_OBJ_FIRST(FixedSubFixed)       },
      { "fixed_t opSub(const int val) const",      WRAP_OBJ_FIRST(FixedSubInt),        },
      { "fixed_t opMul(const fixed_t val) const",  WRAP_OBJ_FIRST(FixedMulFixed)       },
      { "fixed_t opMul(const int val) const",      WRAP_OBJ_FIRST(FixedMulInt)         },
      { "fixed_t opDiv(const fixed_t val) const",  WRAP_OBJ_FIRST(FixedDivFixed)       },
      { "fixed_t opDiv(const int val) const",      WRAP_OBJ_FIRST(FixedDivInt)         },
      { "fixed_t opShl(const int val) const",      WRAP_OBJ_FIRST(FixedLshInt)         },
      { "fixed_t &opAddAssign(const fixed_t val)", WRAP_OBJ_FIRST(FixedAssignAddFixed) },
      { "fixed_t &opAddAssign(const int val)",     WRAP_OBJ_FIRST(FixedAssignAddInt)   },
      { "fixed_t &opSubAssign(const fixed_t val)", WRAP_OBJ_FIRST(FixedAssignSubFixed) },
      { "fixed_t &opSubAssign(const int val)",     WRAP_OBJ_FIRST(FixedAssignSubInt)   },
      { "fixed_t &opMulAssign(const fixed_t val)", WRAP_OBJ_FIRST(FixedAssignMulFixed) },
      { "fixed_t &opMulAssign(const int val)",     WRAP_OBJ_FIRST(FixedAssignMulInt)   },
      { "fixed_t &opDivAssign(const fixed_t val)", WRAP_OBJ_FIRST(FixedAssignDivFixed) },
      { "fixed_t &opDivAssign(const int val)",     WRAP_OBJ_FIRST(FixedAssignDivInt)   },
      { "double  opImplConv() const",              WRAP_OBJ_FIRST(FixedToDouble)       },
   };

   void ScriptObjFixed::Init()
   {
      asIScriptEngine *const e = ScriptManager::Engine();

      e->RegisterObjectType("fixed_t", sizeof(fixed_t),
                            asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CA | asOBJ_APP_CLASS_ALLINTS);

      e->RegisterObjectBehaviour(
         "fixed_t", asBEHAVE_CONSTRUCT, "void f()",
         WRAP_OBJ_LAST(Construct), asCALL_GENERIC
      );
      e->RegisterObjectBehaviour(
         "fixed_t", asBEHAVE_CONSTRUCT, "void f(const fixed_t)",
         WRAP_OBJ_LAST(ConstructFromOther), asCALL_GENERIC
      );
      e->RegisterObjectBehaviour(
         "fixed_t", asBEHAVE_CONSTRUCT, "void f(const double)",
         WRAP_OBJ_LAST(ConstructFromDouble), asCALL_GENERIC
      );
      e->RegisterObjectBehaviour(
         "fixed_t", asBEHAVE_CONSTRUCT, "void f(const int)",
         WRAP_OBJ_LAST(ConstructFromInt), asCALL_GENERIC
      );
      e->RegisterObjectBehaviour(
         "fixed_t", asBEHAVE_CONSTRUCT, "void f(const int16, const double)",
         WRAP_OBJ_LAST(ConstructFromPair), asCALL_GENERIC
      );

      e->SetDefaultNamespace("fixed_t");
      e->RegisterGlobalFunction(
         "fixed_t FromBits(const int val)", WRAP_FN(ConstructFromBits), asCALL_GENERIC
      );
      e->SetDefaultNamespace("");

      for(const aeonfuncreg_t &fn : fixedFuncs)
         e->RegisterObjectMethod("fixed_t", fn.declaration, fn.funcPointer, asCALL_GENERIC);

      e->RegisterGlobalFunction(
         "void print(fixed_t)", WRAP_FN_PR(asPrint, (const fixed_t), void), asCALL_GENERIC
      );
   }

   //=============================================================================
   //
   // Aeon angle_t
   //

   //
   // Clamps a fixed_t to be within the range representable by an angle_t
   // then converts it to an angle_t
   //
   static inline angle_t fixedToAngleClamped(const fixed_t val)
   {
      return FixedToAngle(val % (360 << FRACBITS));
   }

   //
   // If the integer value supplied is a multiple of 45 then use ANG45,
   // due to its perfect accuracy (compared to ANGLE_1 which isn't exacly ANG45/45)
   //
   static inline angle_t intToAngle(const int val)
   {
      if(val % 45)
         return val * ANGLE_1;
      else
         return (val / 45) * ANG45;
   }

   angle_t AngleAddAngle(const angle_t *lval, const angle_t rval) { return *lval + rval;                 }
   angle_t AngleAddFixed(const angle_t *lval, const fixed_t rval) { return *lval + (FixedToAngle(rval)); }
   angle_t AngleAddInt  (const angle_t *lval, const int rval)     { return *lval + intToAngle(rval);     }
   angle_t AngleSubAngle(const angle_t *lval, const angle_t rval) { return *lval - rval;                 }
   angle_t AngleSubFixed(const angle_t *lval, const fixed_t rval) { return *lval - (FixedToAngle(rval)); }
   angle_t AngleSubInt  (const angle_t *lval, const int rval)     { return *lval - intToAngle(rval);     }

   angle_t AngleMulFixed(const angle_t *lval, const fixed_t rval)
   {
      return fixedToAngleClamped(FixedMul(AngleToFixed(*lval), rval));
   }
   angle_t AngleMulInt(const angle_t *lval, const int rval)       { return *lval * rval; }
   angle_t AngleDivAngle(const angle_t *lval, const angle_t rval) { return *lval / rval; }
   angle_t AngleDivFixed(const angle_t *lval, const fixed_t rval)
   {
      return fixedToAngleClamped(FixedDiv(AngleToFixed(*lval), rval));
   }
   angle_t AngleDivInt(const angle_t *lval, const int rval) { return *lval / rval; }

   angle_t &AngleAssignAddAngle(angle_t *lval, const angle_t rval)
   {
      *lval += rval;
      return *lval;
   }
   angle_t &AngleAssignAddFixed(angle_t *lval, const fixed_t rval)
   {
      *lval += fixedToAngleClamped(rval);
      return *lval;
   }
   angle_t &AngleAssignAddInt(angle_t *lval, const int rval)
   {
      *lval += intToAngle(rval);
      return *lval;
   }
   angle_t &AngleAssignSubAngle(angle_t *lval, const angle_t rval)
   {
      *lval -= rval;
      return *lval;
   }
   angle_t &AngleAssignSubFixed(angle_t *lval, const fixed_t rval)
   {
      *lval -= fixedToAngleClamped(rval);
      return *lval;
   }
   angle_t &AngleAssignSubInt(angle_t *lval, const int rval)
   {
      *lval -= intToAngle(rval);
      return *lval;
   }

   angle_t &AngleAssignMulFixed(angle_t *lval, const fixed_t rval)
   {
      *lval = fixedToAngleClamped(FixedMul(AngleToFixed(*lval), rval));
      return *lval;
   }
   angle_t &AngleAssignMulInt(angle_t *lval, const int rval)
   {
      *lval *= rval;
      return *lval;
   }
   angle_t &AngleAssignDivAngle(angle_t *lval, const angle_t rval)
   {
      *lval /= rval;
      return *lval;
   }
   angle_t &AngleAssignDivFixed(angle_t *lval, const fixed_t rval)
   {
      *lval = fixedToAngleClamped(FixedDiv(AngleToFixed(*lval), rval));
      return *lval;
   }
   angle_t &AngleAssignDivInt(angle_t *lval, const int rval)
   {
      *lval /= rval;
      return *lval;
   }

   fixed_t AeonAngleToFixed(fixed_t *thisAngle) { return AngleToFixed(*thisAngle); }

   void ScriptObjAngle::Construct(angle_t *thisFixed)
   {
      *thisFixed = 0;
   }

   void ScriptObjAngle::ConstructFromOther(const angle_t other, angle_t *thisAngle)
   {
      *thisAngle = other;
   }

   void ScriptObjAngle::ConstructFromDouble(const double other, angle_t *thisAngle)
   {
      *thisAngle = fixedToAngleClamped(M_DoubleToFixed(other));
   }

   void ScriptObjAngle::ConstructFromInt(const int other, angle_t *thisAngle)
   {
      *thisAngle = intToAngle(other);
   }

   angle_t ScriptObjAngle::ConstructFromBits(angle_t bits)
   {
      return bits;
   }

   static void asPrint(const angle_t angle)
   {
      C_Printf("%f\n", M_FixedToDouble(AngleToFixed(angle)));
   }

   static const aeonfuncreg_t angleFuncs[] =
   {
      { "angle_t opAdd(const angle_t val) const",  WRAP_OBJ_FIRST(AngleAddAngle)       },
      { "angle_t opAdd(const fixed_t val) const",  WRAP_OBJ_FIRST(AngleAddFixed)       },
      { "angle_t opAdd(const int val) const",      WRAP_OBJ_FIRST(AngleAddInt)         },
      { "angle_t opSub(const angle_t val) const",  WRAP_OBJ_FIRST(AngleSubAngle)       },
      { "angle_t opSub(const fixed_t val) const",  WRAP_OBJ_FIRST(AngleSubFixed)       },
      { "angle_t opSub(const int val) const",      WRAP_OBJ_FIRST(AngleSubInt)         },
      { "angle_t opMul(const fixed_t val) const",  WRAP_OBJ_FIRST(AngleMulFixed)       },
      { "angle_t opMul(const int val) const",      WRAP_OBJ_FIRST(AngleMulInt)         },
      { "angle_t opDiv(const angle_t val) const",  WRAP_OBJ_FIRST(AngleDivAngle)       },
      { "angle_t opDiv(const fixed_t val) const",  WRAP_OBJ_FIRST(AngleDivFixed)       },
      { "angle_t opDiv(const int val) const",      WRAP_OBJ_FIRST(AngleDivInt)         },
      { "angle_t &opAddAssign(const angle_t val)", WRAP_OBJ_FIRST(AngleAssignAddAngle) },
      { "angle_t &opAddAssign(const fixed_t val)", WRAP_OBJ_FIRST(AngleAssignAddFixed) },
      { "angle_t &opAddAssign(const int val)",     WRAP_OBJ_FIRST(AngleAssignAddInt)   },
      { "angle_t &opSubAssign(const angle_t val)", WRAP_OBJ_FIRST(AngleAssignSubAngle) },
      { "angle_t &opSubAssign(const fixed_t val)", WRAP_OBJ_FIRST(AngleAssignSubFixed) },
      { "angle_t &opSubAssign(const int val)",     WRAP_OBJ_FIRST(AngleAssignSubInt)   },
      { "angle_t &opMulAssign(const fixed_t val)", WRAP_OBJ_FIRST(AngleAssignMulFixed) },
      { "angle_t &opMulAssign(const int val)",     WRAP_OBJ_FIRST(AngleAssignMulInt)   },
      { "angle_t &opDivAssign(const angle_t val)", WRAP_OBJ_FIRST(AngleAssignDivAngle) },
      { "angle_t &opDivAssign(const fixed_t val)", WRAP_OBJ_FIRST(AngleAssignDivFixed) },
      { "angle_t &opDivAssign(const int val)",     WRAP_OBJ_FIRST(AngleAssignDivInt)   },
      { "fixed_t opImplConv() const",              WRAP_OBJ_FIRST(AeonAngleToFixed)    },
   };

   void ScriptObjAngle::Init()
   {
      asIScriptEngine *const e = ScriptManager::Engine();

      e->RegisterObjectType(
         "angle_t", sizeof(angle_t),
         asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CA | asOBJ_APP_CLASS_ALLINTS
      );

      e->RegisterObjectBehaviour(
         "angle_t", asBEHAVE_CONSTRUCT, "void f()",
         WRAP_OBJ_LAST(Construct), asCALL_GENERIC
      );
      e->RegisterObjectBehaviour(
         "angle_t", asBEHAVE_CONSTRUCT, "void f(const angle_t)",
         WRAP_OBJ_LAST(ConstructFromOther), asCALL_GENERIC
      );
      e->RegisterObjectBehaviour(
         "angle_t", asBEHAVE_CONSTRUCT, "void f(const double)",
         WRAP_OBJ_LAST(ConstructFromDouble), asCALL_GENERIC
      );
      e->RegisterObjectBehaviour(
         "angle_t", asBEHAVE_CONSTRUCT, "void f(const int)",
         WRAP_OBJ_LAST(ConstructFromInt), asCALL_GENERIC
      );

      // AEON_TODO: Figure if there's a nicer way to do this
      e->SetDefaultNamespace("angle_t");
      e->RegisterGlobalFunction("angle_t FromBits(const uint val)", WRAP_FN(ConstructFromBits), asCALL_GENERIC);
      e->SetDefaultNamespace("");

      for(const aeonfuncreg_t &fn : angleFuncs)
         e->RegisterObjectMethod("angle_t", fn.declaration, fn.funcPointer, asCALL_GENERIC);

      e->RegisterGlobalFunction(
         "void print(angle_t)", WRAP_FN_PR(asPrint, (const angle_t), void), asCALL_GENERIC
      );
   }

   //=============================================================================
   //
   // Aeon Vector Class
   //

   class Vector
   {
   public:
      v3fixed_t value;

      Vector() : value() { }

      Vector(fixed_t x, fixed_t y, fixed_t z) : value({x, y, z}) { }

   };

   void ScriptObjVector::Construct(Vector *thisVector)
   {
      *thisVector = Vector();
   }

   void ScriptObjVector::ConstructFromOther(const Vector &other, Vector *thisVector)
   {
      *thisVector = Vector(other);
   }

   void ScriptObjVector::ConstructFromFixed(const fixed_t x, const fixed_t y,
                                            const fixed_t z, Vector *thisVector)
   {
      *thisVector = Vector(x, y, z);
   }

   static void asPrint(Vector f)
   {
      C_Printf("x: %.11f, y: %.11f, z: %.11f\n", M_FixedToDouble(f.value.x),
                                                 M_FixedToDouble(f.value.y),
                                                 M_FixedToDouble(f.value.z));
   }

   void ScriptObjVector::Init()
   {
      asIScriptEngine *const e = ScriptManager::Engine();

      e->RegisterObjectType("vector_t", sizeof(Vector),
                            asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CA | asOBJ_APP_CLASS_ALLINTS);

      e->RegisterObjectBehaviour("vector_t", asBEHAVE_CONSTRUCT, "void f()",
                                 WRAP_OBJ_LAST(Construct), asCALL_GENERIC);
      e->RegisterObjectBehaviour("vector_t", asBEHAVE_CONSTRUCT, "void f(const vector_t &in)",
                                 WRAP_OBJ_LAST(ConstructFromOther), asCALL_GENERIC);
      e->RegisterObjectBehaviour("vector_t", asBEHAVE_CONSTRUCT,
                                 "void f(const fixed_t, const fixed_t, const fixed_t)",
                                 WRAP_OBJ_LAST(ConstructFromFixed), asCALL_GENERIC);

      e->RegisterObjectProperty("vector_t", "fixed_t x", asOFFSET(Vector, value.x));
      e->RegisterObjectProperty("vector_t", "fixed_t y", asOFFSET(Vector, value.y));
      e->RegisterObjectProperty("vector_t", "fixed_t z", asOFFSET(Vector, value.z));

      e->RegisterGlobalFunction("void print(vector_t)", WRAP_FN_PR(asPrint, (Vector), void),
                                 asCALL_GENERIC);
   }
}

// EOF

