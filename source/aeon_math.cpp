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
      static Fixed Sin(const Angle val)
      {
         return Fixed(finesine[val.value >> ANGLETOFINESHIFT]);
      }

      static Fixed Cos(const Angle val)
      {
         return Fixed(finecosine[val.value >> ANGLETOFINESHIFT]);
      }

      static Fixed Tan(const Angle val)
      {
         return Fixed(finetangent[val.value >> ANGLETOFINESHIFT]);
      }

      static Fixed Atan2(const Fixed y, const Fixed x)
      {
         return Fixed(P_PointToAngle(0, 0, x, y));
      }

      static Fixed Fabs(const Fixed val)
      {
         return Fixed(D_abs(val.value));
      }

      static Fixed Sqrt(const Fixed val)
      {
         return Fixed(M_DoubleToFixed(sqrt(M_FixedToDouble(val.value))));
      }

      static int Abs(const int val)
      {
         return abs(val);
      }

      static Fixed Ceil(const Fixed val)
      {
         if(val.value & (FRACUNIT - 1))
            return Fixed((val.value & ~(FRACUNIT - 1)) + FRACUNIT);
         else
            return val;
      }

      static Fixed Floor(const Fixed val)
      {
         return Fixed(val.value & ~(FRACUNIT - 1));
      }
   };

   class Rand
   {
   public:
      static unsigned int UInt()             { return P_RandomEx(pr_script);              }
      static uint8_t      Byte()             { return P_Random(pr_script);                }
      static unsigned int Max(const int max) { return P_RangeRandomEx(pr_script, 0, max); }
      static Aeon::Fixed  Fixed()            { return Aeon::Fixed(P_RandomEx(pr_script)); }
      static Aeon::Fixed  CFixed()
      {
         // TODO: FRACUNIT or FRACUNIT-1?
         return Aeon::Fixed(P_RangeRandomEx(pr_script, 0, FRACUNIT));
      }
      static Aeon::Fixed Range(const Aeon::Fixed min, const Aeon::Fixed max)
      {
         if(min.value >= 0)
            return Aeon::Fixed(P_RangeRandomEx(pr_script, min.value, max.value));
         return Aeon::Fixed(P_RangeRandomEx(pr_script, min.value - min.value,
                                            max.value - min.value) + min.value);
      }

      /*static unsigned int SubUInt(const unsigned int max) { return P_SubRandomEx(pr_script, max); }
      static Fixed SubFixed(const Fixed max)
      {
         return Fixed(P_SubRandomEx(pr_script, max.value));
      }*/
   };

   static aeonfuncreg_t mathFuncs[]
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

   static aeonfuncreg_t randFuncs[]
   {
      { "uint   RandUInt()",                                       WRAP_FN(Rand::UInt)   },
      { "uint8  RandByte()",                                       WRAP_FN(Rand::Byte)   },
      { "uint   RandMax(const uint max)",                          WRAP_FN(Rand::UInt)   },
      { "fixed_t RandFixed()",                                     WRAP_FN(Rand::Fixed)  },
      { "fixed_t RandCFixed()",                                    WRAP_FN(Rand::CFixed) },
      { "fixed_t RandRange(const fixed_t min, const fixed_t max)", WRAP_FN(Rand::Range)  },

   };

   void ScriptObjMath::Init()
   {
      asIScriptEngine *e = ScriptManager::Engine();

      e->SetDefaultNamespace("Math");
      for(const aeonfuncreg_t &fn : mathFuncs)
         e->RegisterGlobalFunction(fn.declaration, fn.funcPointer, asCALL_GENERIC);
      for(const aeonfuncreg_t &fn : randFuncs)
         e->RegisterGlobalFunction(fn.declaration, fn.funcPointer, asCALL_GENERIC);
      e->SetDefaultNamespace("");
   }

   //=============================================================================
   //
   // Aeon Fixed-point Class
   //

   Fixed Fixed::operator +  (const Fixed &in) { return value + in.value;          }
   Fixed Fixed::operator +  (const int val)       { return value + (val * FRACUNIT);  }
   Fixed Fixed::operator -  (const Fixed &in) { return value - in.value;          }
   Fixed Fixed::operator -  (const int val)       { return value - (val * FRACUNIT);  }
   Fixed Fixed::operator *  (const Fixed &in) { return FixedMul(value, in.value); }
   Fixed Fixed::operator *  (const int val)       { return value * val;               }
   Fixed Fixed::operator /  (const Fixed &in) { return FixedDiv(value, in.value); }
   Fixed Fixed::operator /  (const int val)       { return value / val;               }
   Fixed Fixed::operator << (const int val)       { return value << val;              }

   Fixed &Fixed::operator += (const Fixed &in)
   {
      value += in.value;
      return *this;
   }
   Fixed &Fixed::operator += (const int val)
   {
      value += val * FRACUNIT;
      return *this;
   }
   Fixed &Fixed::operator -= (const Fixed &in)
   {
      value -= in.value;
      return *this;
   }
   Fixed &Fixed::operator -= (const int val)
   {
      value -= val * FRACUNIT;
      return *this;
   }
   Fixed &Fixed::operator *= (const Fixed &in)
   {
      value = FixedMul(value, in.value);
      return *this;
   }
   Fixed &Fixed::operator *= (const int val)
   {
      value *= val;
      return *this;
   }

   Fixed &Fixed::operator /= (const Fixed &in)
   {
      value = FixedDiv(value, in.value);
      return *this;
   }
   Fixed &Fixed::operator /= (const int val)
   {
      value /= val;
      return *this;
   }

   Fixed::operator double() const { return M_FixedToDouble(value); }

   void ScriptObjFixed::Construct(Fixed *thisFixed)
   {
      *thisFixed = Fixed();
   }

   void ScriptObjFixed::ConstructFromOther(const Fixed &other, Fixed *thisFixed)
   {
      *thisFixed = Fixed(other);
   }

   void ScriptObjFixed::ConstructFromDouble(const double other, Fixed *thisFixed)
   {
      thisFixed->value = M_DoubleToFixed(other);
   }

   void ScriptObjFixed::ConstructFromInt(const int other, Fixed *thisFixed)
   {
      thisFixed->value = other * FRACUNIT;
   }

   void ScriptObjFixed::ConstructFromPair(const int16_t integer, const double frac,
                                          Fixed *thisFixed)
   {
      thisFixed->value = static_cast<int32_t>(integer) * FRACUNIT;
      if(frac < 1.0 && frac >= 0.0)
         thisFixed->value |= M_DoubleToFixed(frac);
   }

   Fixed ScriptObjFixed::ConstructFromBits(int bits)
   {
      Fixed ret = Fixed();
      ret.value = bits;
      return ret;
   }

   static void ASPrint(Fixed f)
   {
      C_Printf("%f\n", M_FixedToDouble(f.value));
   }

   #define FIXEDBINOP(op, param) \
      WRAP_MFN_PR(Fixed, operator op,  (param), Fixed)

   #define FIXEDASSIGNOP(op, param) \
      WRAP_MFN_PR(Fixed, operator op,  (param), Fixed &)

   static aeonfuncreg_t fixedFuncs[] =
   {
      { "fixed_t opAdd(const fixed_t &in)",        FIXEDBINOP(+, const Fixed &)         },
      { "fixed_t opAdd(const int val)",            FIXEDBINOP(+, const int),                },
      { "fixed_t opSub(const fixed_t &in)",        FIXEDBINOP(-, const Fixed &)         },
      { "fixed_t opSub(const int val)",            FIXEDBINOP(-, const int),                },
      { "fixed_t opMul(const fixed_t &in)",        FIXEDBINOP(*, const Fixed &)         },
      { "fixed_t opMul(const int val)",            FIXEDBINOP(*, const int)                 },
      { "fixed_t opDiv(const fixed_t &in)",        FIXEDBINOP(/, const Fixed &)         },
      { "fixed_t opDiv(const int val)",            FIXEDBINOP(/, const int)                 },
      { "fixed_t opShl(const int val)",            FIXEDBINOP(<<, const int)                },
      { "fixed_t &opAddAssign(const fixed_t &in)", FIXEDASSIGNOP(+=, const Fixed &)     },
      { "fixed_t &opAddAssign(const int val)",     FIXEDASSIGNOP(+=, const int)             },
      { "fixed_t &opSubAssign(const fixed_t &in)", FIXEDASSIGNOP(-=, const Fixed &)     },
      { "fixed_t &opSubAssign(const int val)",     FIXEDASSIGNOP(-=, const int)             },
      { "fixed_t &opMulAssign(const fixed_t &in)", FIXEDASSIGNOP(*=, const Fixed &)     },
      { "fixed_t &opMulAssign(const int val)",     FIXEDASSIGNOP(*=, const int)             },
      { "fixed_t &opDivAssign(const fixed_t &in)", FIXEDASSIGNOP(/=, const Fixed &)     },
      { "fixed_t &opDivAssign(const int val)",     FIXEDASSIGNOP(/=, const int)             },
      { "double opImplConv() const",               WRAP_MFN(Fixed, operator double)     },
   };

   void ScriptObjFixed::Init()
   {
      asIScriptEngine *e = ScriptManager::Engine();

      e->RegisterObjectType("fixed_t", sizeof(Fixed),
                            asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CA | asOBJ_APP_CLASS_ALLINTS);

      e->RegisterObjectBehaviour("fixed_t", asBEHAVE_CONSTRUCT, "void f()",
                                 WRAP_OBJ_LAST(Construct), asCALL_GENERIC);
      e->RegisterObjectBehaviour("fixed_t", asBEHAVE_CONSTRUCT, "void f(const fixed_t &in)",
                                 WRAP_OBJ_LAST(ConstructFromOther), asCALL_GENERIC);
      e->RegisterObjectBehaviour("fixed_t", asBEHAVE_CONSTRUCT, "void f(const double)",
                                 WRAP_OBJ_LAST(ConstructFromDouble), asCALL_GENERIC);
      e->RegisterObjectBehaviour("fixed_t", asBEHAVE_CONSTRUCT, "void f(const int)",
                                 WRAP_OBJ_LAST(ConstructFromInt), asCALL_GENERIC);
      e->RegisterObjectBehaviour("fixed_t", asBEHAVE_CONSTRUCT, "void f(const int16, const double)",
                                 WRAP_OBJ_LAST(ConstructFromPair), asCALL_GENERIC);

      // TODO: Figure if there's a nicer way to do this
      e->SetDefaultNamespace("fixed_t");
      e->RegisterGlobalFunction("fixed_t FromBits(const int val)",
                                WRAP_FN(ConstructFromBits), asCALL_GENERIC);
      e->SetDefaultNamespace("");

      for(const aeonfuncreg_t &fn : fixedFuncs)
         e->RegisterObjectMethod("fixed_t", fn.declaration, fn.funcPointer, asCALL_GENERIC);

      e->RegisterGlobalFunction("void print(fixed_t)", WRAP_FN_PR(ASPrint, (Fixed), void),
                                asCALL_GENERIC);
   }

   //=============================================================================
   //
   // Aeon Angle Class
   //

   //
   // Clamps a fixed_t to be within the range representable by an angle_t
   // then converts it to an angle_t
   //
   static inline angle_t FixedToAngleClamped(const fixed_t val)
   {
      return FixedToAngle(val % (360 << FRACBITS));
   }

   //
   // If the integer value supplied is a multiple of 45 then use ANG45,
   // due to its perfect accuracy (compared to ANGLE_1 which isn't exacly ANG45/45)
   //
   static inline angle_t IntToAngle(const int val)
   {
      if(val % 45)
         return val * ANGLE_1;
      else
         return (val / 45) * ANG45;
   }

   Angle Angle::operator + (const Angle &in)  { return value + in.value; }
   Angle Angle::operator + (const Fixed &in)  { return value + (FixedToAngle(in.value)); }
   Angle Angle::operator + (const int val)        { return value + IntToAngle(val); }
   Angle Angle::operator - (const Angle &in)  { return value - in.value; }
   Angle Angle::operator - (const Fixed &in)  { return value - (FixedToAngle(in.value)); }
   Angle Angle::operator - (const int val)        { return value - IntToAngle(val); }

   Angle Angle::operator * (const Fixed &in)
   {
      return FixedToAngleClamped(FixedMul(AngleToFixed(value), in.value));
   }
   Angle Angle::operator * (const int val)        { return value * val; }
   Angle Angle::operator / (const Angle &in)  { return value / in.value; }
   Angle Angle::operator / (const Fixed &in)
   {
      return FixedToAngleClamped(FixedDiv(AngleToFixed(value), in.value));
   }
   Angle Angle::operator / (const int val) { return value / val; }

   Angle &Angle::operator += (const Angle &in)
   {
      value += in.value;
      return *this;
   }
   Angle &Angle::operator += (const Fixed &in)
   {
      value += FixedToAngleClamped(in.value);
      return *this;
   }
   Angle &Angle::operator += (const int val)
   {
      value += IntToAngle(val);
      return *this;
   }
   Angle &Angle::operator -= (const Angle &in)
   {
      value -= in.value;
      return *this;
   }
   Angle &Angle::operator -= (const Fixed &in)
   {
      value -= FixedToAngleClamped(in.value);
      return *this;
   }
   Angle &Angle::operator -= (const int val)
   {
      value -= IntToAngle(val);
      return *this;
   }

   Angle &Angle::operator *= (const Fixed &in)
   {
      value = FixedToAngleClamped(FixedMul(AngleToFixed(value), in.value));
      return *this;
   }
   Angle &Angle::operator *= (const int val)
   {
      value *= val;
      return *this;
   }
   Angle &Angle::operator /= (const Angle &in)
   {
      value /= in.value;
      return *this;
   }
   Angle &Angle::operator /= (const Fixed &in)
   {
      value = FixedToAngleClamped(FixedDiv(AngleToFixed(value), in.value));
      return *this;
   }
   Angle &Angle::operator /= (const int val)
   {
      value /= val;
      return *this;
   }

   Angle::operator Fixed() const { return Fixed(AngleToFixed(value)); }

   #define ANGLEBINOP(op, param) \
      WRAP_MFN_PR(Angle, operator op,  (param), Angle)

   #define ANGLEASSIGNOP(op, param) \
      WRAP_MFN_PR(Angle, operator op,  (param), Angle &)

   void ScriptObjAngle::Construct(Angle *thisFixed)
   {
      *thisFixed = Angle();
   }

   void ScriptObjAngle::ConstructFromOther(const Angle &other, Angle *thisAngle)
   {
      *thisAngle = Angle(other);
   }

   void ScriptObjAngle::ConstructFromDouble(const double other, Angle *thisAngle)
   {
      thisAngle->value = FixedToAngleClamped(M_DoubleToFixed(other));
   }

   void ScriptObjAngle::ConstructFromInt(const int other, Angle *thisAngle)
   {
      thisAngle->value = IntToAngle(other);
   }

   static void ASPrint(Angle f)
   {
      C_Printf("%f\n", M_FixedToDouble(AngleToFixed(f.value)));
   }

   static aeonfuncreg_t angleFuncs[] =
   {
      { "angle_t opAdd(const angle_t &in)",        ANGLEBINOP(+, const Angle &)         },
      { "angle_t opAdd(const fixed_t &in)",        ANGLEBINOP(+, const Fixed &)         },
      { "angle_t opAdd(const int val)",            ANGLEBINOP(+, const int),                },
      { "angle_t opSub(const angle_t &in)",        ANGLEBINOP(-, const Angle &)         },
      { "angle_t opSub(const fixed_t &in)",        ANGLEBINOP(-, const Fixed &)         },
      { "angle_t opSub(const int val)",            ANGLEBINOP(-, const int),                },
      { "angle_t opMul(const fixed_t &in)",        ANGLEBINOP(*, const Fixed &)         },
      { "angle_t opMul(const int val)",            ANGLEBINOP(*, const int)                 },
      { "angle_t opDiv(const angle_t &in)",        ANGLEBINOP(/, const Angle &)         },
      { "angle_t opDiv(const fixed_t &in)",        ANGLEBINOP(/, const Fixed &)         },
      { "angle_t opDiv(const int val)",            ANGLEBINOP(/, const int)                 },
      { "angle_t &opAddAssign(const angle_t &in)", ANGLEASSIGNOP(+=, const Angle &)     },
      { "angle_t &opAddAssign(const fixed_t &in)", ANGLEASSIGNOP(+=, const Fixed &)     },
      { "angle_t &opAddAssign(const int val)",     ANGLEASSIGNOP(+=, const int)             },
      { "angle_t &opSubAssign(const angle_t &in)", ANGLEASSIGNOP(-=, const Angle &)     },
      { "angle_t &opSubAssign(const fixed_t &in)", ANGLEASSIGNOP(-=, const Fixed &)     },
      { "angle_t &opSubAssign(const int val)",     ANGLEASSIGNOP(-=, const int)             },
      { "angle_t &opMulAssign(const angle_t &in)", ANGLEASSIGNOP(*=, const Fixed &)     },
      { "angle_t &opMulAssign(const int val)",     ANGLEASSIGNOP(*=, const int)             },
      { "angle_t &opDivAssign(const angle_t &in)", ANGLEASSIGNOP(/=, const Angle &)     },
      { "angle_t &opDivAssign(const fixed_t &in)", ANGLEASSIGNOP(/=, const Fixed &)     },
      { "angle_t &opDivAssign(const int val)",     ANGLEASSIGNOP(/=, const int)             },
      { "fixed_t opImplConv() const",              WRAP_MFN(Angle, operator Fixed)  },
   };

   void ScriptObjAngle::Init()
   {
      asIScriptEngine *e = ScriptManager::Engine();

      e->RegisterObjectType("angle_t", sizeof(Angle),
                            asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CA | asOBJ_APP_CLASS_ALLINTS);

      e->RegisterObjectBehaviour("angle_t", asBEHAVE_CONSTRUCT, "void f()",
                                 WRAP_OBJ_LAST(Construct), asCALL_GENERIC);
      e->RegisterObjectBehaviour("angle_t", asBEHAVE_CONSTRUCT, "void f(const angle_t &in)",
                                 WRAP_OBJ_LAST(ConstructFromOther), asCALL_GENERIC);
      e->RegisterObjectBehaviour("angle_t", asBEHAVE_CONSTRUCT, "void f(const double)",
                                 WRAP_OBJ_LAST(ConstructFromDouble), asCALL_GENERIC);
      e->RegisterObjectBehaviour("angle_t", asBEHAVE_CONSTRUCT, "void f(const int)",
                                 WRAP_OBJ_LAST(ConstructFromInt), asCALL_GENERIC);

      for(const aeonfuncreg_t &fn : angleFuncs)
         e->RegisterObjectMethod("angle_t", fn.declaration, fn.funcPointer, asCALL_GENERIC);

      e->RegisterGlobalFunction("void print(angle_t)", WRAP_FN_PR(ASPrint, (Angle), void),
                                asCALL_GENERIC);
   }

   //=============================================================================
   //
   // Aeon Vector Class
   //

   class Vector
   {
   public:
      vector_t value;

      Vector() : value()
      {
      }

      Vector(fixed_t x, fixed_t y, fixed_t z) : value({x, y, z})
      {
      }

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

   static void ASPrint(Vector f)
   {
      C_Printf("x: %f, y: %f, z: %f\n", M_FixedToDouble(f.value.x),
                                        M_FixedToDouble(f.value.y),
                                        M_FixedToDouble(f.value.z));
   }

   void ScriptObjVector::Init()
   {
      asIScriptEngine *e = ScriptManager::Engine();

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

      e->RegisterGlobalFunction("void print(vector_t)", WRAP_FN_PR(ASPrint, (Vector), void),
                                 asCALL_GENERIC);
   }
}

// EOF

