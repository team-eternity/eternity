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

//=============================================================================
//
// Aeon Math Class
//

class AeonMath
{
public:
   static AeonFixed Sin(const AeonAngle val)
   {
      return AeonFixed(finesine[val.value >> ANGLETOFINESHIFT]);
   }

   static AeonFixed Cos(const AeonAngle val)
   {
      return AeonFixed(finecosine[val.value >> ANGLETOFINESHIFT]);
   }

   static AeonFixed Tan(const AeonAngle val)
   {
      return AeonFixed(finetangent[val.value >> ANGLETOFINESHIFT]);
   }

   static AeonFixed Atan2(const AeonFixed y, const AeonFixed x)
   {
      return AeonFixed(P_PointToAngle(0, 0, x, y));
   }

   static AeonFixed Fabs(const AeonFixed val)
   {
      return AeonFixed(D_abs(val.value));
   }

   static AeonFixed Sqrt(const AeonFixed val)
   {
      return AeonFixed(M_DoubleToFixed(sqrt(M_FixedToDouble(val.value))));
   }

   static int Abs(const int val)
   {
      return abs(val);
   }

   static AeonFixed Ceil(AeonFixed val)
   {
      if(val.value & (FRACUNIT - 1))
         return AeonFixed((val.value & ~(FRACUNIT - 1)) + FRACUNIT);
      else
         return val;
   }

   static AeonFixed Floor(AeonFixed val)
   {
      return AeonFixed(val.value & ~(FRACUNIT - 1));
   }
};

class AeonRand
{
public:
   static unsigned int UInt()             { return P_RandomEx(pr_script); }
   static uint8_t      Byte()             { return P_Random(pr_script); }
   static unsigned int Max(const int max) { return P_RangeRandomEx(pr_script, 0, max); }
   static AeonFixed    Fixed()            { return AeonFixed(P_RandomEx(pr_script)); }
   static AeonFixed    CFixed()
   {
      // TODO: FRACUNIT or FRACUNIT-1?
      return AeonFixed(P_RangeRandomEx(pr_script, 0, FRACUNIT));
   }
   static AeonFixed Range(const AeonFixed min, const AeonFixed max)
   {
      if(min.value >= 0)
         return AeonFixed(P_RangeRandomEx(pr_script, min.value, max.value));
      return AeonFixed(P_RangeRandomEx(pr_script, min.value - min.value,
                                       max.value - min.value) + min.value);
   }

   /*static unsigned int SubUInt(const unsigned int max) { return P_SubRandomEx(pr_script, max); }
   static AeonFixed SubFixed(const AeonFixed max)
   {
      return AeonFixed(P_SubRandomEx(pr_script, max.value));
   }*/
};

static aeonfuncreg_t mathFuncs[]
{
   { "fixed_t Sin(const angle_t val)",                  WRAP_FN(AeonMath::Sin)   },
   { "fixed_t Cos(const angle_t val)",                  WRAP_FN(AeonMath::Cos)   },
   { "fixed_t Tan(const angle_t val)",                  WRAP_FN(AeonMath::Tan)   },
   { "fixed_t Atan2(const fixed_t y, const fixed_t x)", WRAP_FN(AeonMath::Atan2) },
   { "fixed_t Fabs(const fixed_t val)",                 WRAP_FN(AeonMath::Fabs)  },
   { "fixed_t Sqrt(const fixed_t val)",                 WRAP_FN(AeonMath::Sqrt)  },
   { "int Abs(const int)",                              WRAP_FN(AeonMath::Abs)   },
   { "fixed_t Ceil(const fixed_t val)",                 WRAP_FN(AeonMath::Ceil)  },
   { "fixed_t Floor(const fixed_t val)",                WRAP_FN(AeonMath::Floor) },
};

static aeonfuncreg_t randFuncs[]
{
   { "uint   RandUInt()",                                       WRAP_FN(AeonRand::UInt)   },
   { "uint8  RandByte()",                                       WRAP_FN(AeonRand::Byte)   },
   { "uint   RandMax(const uint max)",                          WRAP_FN(AeonRand::UInt)   },
   { "fixed_t RandFixed()",                                     WRAP_FN(AeonRand::Fixed)  },
   { "fixed_t RandCFixed()",                                    WRAP_FN(AeonRand::CFixed) },
   { "fixed_t RandRange(const fixed_t min, const fixed_t max)", WRAP_FN(AeonRand::Range)  },

};

void AeonScriptObjMath::Init()
{
   asIScriptEngine *e = AeonScriptManager::Engine();

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

AeonFixed AeonFixed::operator +  (const AeonFixed &in) { return value + in.value;          }
AeonFixed AeonFixed::operator +  (const int val)       { return value + (val * FRACUNIT);  }
AeonFixed AeonFixed::operator -  (const AeonFixed &in) { return value - in.value;          }
AeonFixed AeonFixed::operator -  (const int val)       { return value - (val * FRACUNIT);  }
AeonFixed AeonFixed::operator *  (const AeonFixed &in) { return FixedMul(value, in.value); }
AeonFixed AeonFixed::operator *  (const int val)       { return value * val;               }
AeonFixed AeonFixed::operator /  (const AeonFixed &in) { return FixedDiv(value, in.value); }
AeonFixed AeonFixed::operator /  (const int val)       { return value / val;               }
AeonFixed AeonFixed::operator << (const int val)       { return value << val;              }

AeonFixed &AeonFixed::operator += (const AeonFixed &in)
{
   value += in.value;
   return *this;
}
AeonFixed &AeonFixed::operator += (const int val)
{
   value += val * FRACUNIT;
   return *this;
}
AeonFixed &AeonFixed::operator -= (const AeonFixed &in)
{
   value -= in.value;
   return *this;
}
AeonFixed &AeonFixed::operator -= (const int val)
{
   value -= val * FRACUNIT;
   return *this;
}
AeonFixed &AeonFixed::operator *= (const AeonFixed &in)
{
   value = FixedMul(value, in.value);
   return *this;
}
AeonFixed &AeonFixed::operator *= (const int val)
{
   value *= val;
   return *this;
}

AeonFixed &AeonFixed::operator /= (const AeonFixed &in)
{
   value = FixedDiv(value, in.value);
   return *this;
}
AeonFixed &AeonFixed::operator /= (const int val)
{
   value /= val;
   return *this;
}

AeonFixed::operator double() const { return M_FixedToDouble(value); }

void AeonScriptObjFixed::Construct(AeonFixed *thisFixed)
{
   *thisFixed = AeonFixed();
}

void AeonScriptObjFixed::ConstructFromOther(const AeonFixed &other, AeonFixed *thisFixed)
{
   *thisFixed = AeonFixed(other);
}

void AeonScriptObjFixed::ConstructFromDouble(double other, AeonFixed *thisFixed)
{
   thisFixed->value = M_DoubleToFixed(other);
}

void AeonScriptObjFixed::ConstructFromInt(int other, AeonFixed *thisFixed)
{
   thisFixed->value = other * FRACUNIT;
}

void AeonScriptObjFixed::ConstructFromPair(int16_t integer, double frac, AeonFixed *thisFixed)
{
   thisFixed->value = static_cast<int32_t>(integer) * FRACUNIT;
   if(frac < 1.0 && frac >= 0.0)
      thisFixed->value |= M_DoubleToFixed(frac);
}

AeonFixed AeonScriptObjFixed::ConstructFromBits(int bits)
{
   AeonFixed ret = AeonFixed();
   ret.value = bits;
   return ret;
}

static void ASPrint(AeonFixed f)
{
   C_Printf("%f\n", M_FixedToDouble(f.value));
}

#define FIXEDBINOP(op, param) \
   WRAP_MFN_PR(AeonFixed, operator op,  (param), AeonFixed)

#define FIXEDASSIGNOP(op, param) \
   WRAP_MFN_PR(AeonFixed, operator op,  (param), AeonFixed &)

static aeonfuncreg_t fixedFuncs[] =
{
   { "fixed_t opAdd(const fixed_t &in)",        FIXEDBINOP(+, const AeonFixed &)         },
   { "fixed_t opAdd(const int val)",            FIXEDBINOP(+, const int),                },
   { "fixed_t opSub(const fixed_t &in)",        FIXEDBINOP(-, const AeonFixed &)         },
   { "fixed_t opSub(const int val)",            FIXEDBINOP(-, const int),                },
   { "fixed_t opMul(const fixed_t &in)",        FIXEDBINOP(*, const AeonFixed &)         },
   { "fixed_t opMul(const int val)",            FIXEDBINOP(*, const int)                 },
   { "fixed_t opDiv(const fixed_t &in)",        FIXEDBINOP(/, const AeonFixed &)         },
   { "fixed_t opDiv(const int val)",            FIXEDBINOP(/, const int)                 },
   { "fixed_t opShl(const int val)",            FIXEDBINOP(<<, const int)                },
   { "fixed_t &opAddAssign(const fixed_t &in)", FIXEDASSIGNOP(+=, const AeonFixed &)     },
   { "fixed_t &opAddAssign(const int val)",     FIXEDASSIGNOP(+=, const int)             },
   { "fixed_t &opSubAssign(const fixed_t &in)", FIXEDASSIGNOP(-=, const AeonFixed &)     },
   { "fixed_t &opSubAssign(const int val)",     FIXEDASSIGNOP(-=, const int)             },
   { "fixed_t &opMulAssign(const fixed_t &in)", FIXEDASSIGNOP(*=, const AeonFixed &)     },
   { "fixed_t &opMulAssign(const int val)",     FIXEDASSIGNOP(*=, const int)             },
   { "fixed_t &opDivAssign(const fixed_t &in)", FIXEDASSIGNOP(/=, const AeonFixed &)     },
   { "fixed_t &opDivAssign(const int val)",     FIXEDASSIGNOP(/=, const int)             },
   { "double opImplConv() const",               WRAP_MFN(AeonFixed, operator double)     },
};

void AeonScriptObjFixed::Init()
{
   asIScriptEngine *e = AeonScriptManager::Engine();

   e->RegisterObjectType("fixed_t", sizeof(AeonFixed),
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

   e->RegisterGlobalFunction("void print(fixed_t)", WRAP_FN_PR(ASPrint, (AeonFixed), void),
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

AeonAngle AeonAngle::operator + (const AeonAngle &in)  { return value + in.value; }
AeonAngle AeonAngle::operator + (const AeonFixed &in)  { return value + (FixedToAngle(in.value)); }
AeonAngle AeonAngle::operator + (const int val)        { return value + IntToAngle(val); }
AeonAngle AeonAngle::operator - (const AeonAngle &in)  { return value - in.value; }
AeonAngle AeonAngle::operator - (const AeonFixed &in)  { return value - (FixedToAngle(in.value)); }
AeonAngle AeonAngle::operator - (const int val)        { return value - IntToAngle(val); }

AeonAngle AeonAngle::operator * (const AeonFixed &in)
{
   return FixedToAngleClamped(FixedMul(AngleToFixed(value), in.value));
}
AeonAngle AeonAngle::operator * (const int val)        { return value * val; }
AeonAngle AeonAngle::operator / (const AeonAngle &in)  { return value / in.value; }
AeonAngle AeonAngle::operator / (const AeonFixed &in)
{
   return FixedToAngleClamped(FixedDiv(AngleToFixed(value), in.value));
}
AeonAngle AeonAngle::operator / (const int val) { return value / val; }

AeonAngle &AeonAngle::operator += (const AeonAngle &in)
{
   value += in.value;
   return *this;
}
AeonAngle &AeonAngle::operator += (const AeonFixed &in)
{
   value += FixedToAngleClamped(in.value);
   return *this;
}
AeonAngle &AeonAngle::operator += (const int val)
{
   value += IntToAngle(val);
   return *this;
}
AeonAngle &AeonAngle::operator -= (const AeonAngle &in)
{
   value -= in.value;
   return *this;
}
AeonAngle &AeonAngle::operator -= (const AeonFixed &in)
{
   value -= FixedToAngleClamped(in.value);
   return *this;
}
AeonAngle &AeonAngle::operator -= (const int val)
{
   value -= IntToAngle(val);
   return *this;
}

AeonAngle &AeonAngle::operator *= (const AeonFixed &in)
{
   value = FixedToAngleClamped(FixedMul(AngleToFixed(value), in.value));
   return *this;
}
AeonAngle &AeonAngle::operator *= (const int val)
{
   value *= val;
   return *this;
}
AeonAngle &AeonAngle::operator /= (const AeonAngle &in)
{
   value /= in.value;
   return *this;
}
AeonAngle &AeonAngle::operator /= (const AeonFixed &in)
{
   value = FixedToAngleClamped(FixedDiv(AngleToFixed(value), in.value));
   return *this;
}
AeonAngle &AeonAngle::operator /= (const int val)
{
   value /= val;
   return *this;
}

AeonAngle::operator AeonFixed() const { return AeonFixed(AngleToFixed(value)); }

#define ANGLEBINOP(op, param) \
   WRAP_MFN_PR(AeonAngle, operator op,  (param), AeonAngle)

#define ANGLEASSIGNOP(op, param) \
   WRAP_MFN_PR(AeonAngle, operator op,  (param), AeonAngle &)

void AeonScriptObjAngle::Construct(AeonAngle *thisFixed)
{
   *thisFixed = AeonAngle();
}

void AeonScriptObjAngle::ConstructFromOther(const AeonAngle &other, AeonAngle *thisAngle)
{
   *thisAngle = AeonAngle(other);
}

void AeonScriptObjAngle::ConstructFromDouble(double other, AeonAngle *thisAngle)
{
   thisAngle->value = FixedToAngleClamped(M_DoubleToFixed(other));
}

void AeonScriptObjAngle::ConstructFromInt(int other, AeonAngle *thisAngle)
{
   thisAngle->value = IntToAngle(other);
}

static void ASPrint(AeonAngle f)
{
   C_Printf("%f\n", M_FixedToDouble(AngleToFixed(f.value)));
}

static aeonfuncreg_t angleFuncs[] =
{
   { "angle_t opAdd(const angle_t &in)",        ANGLEBINOP(+, const AeonAngle &)         },
   { "angle_t opAdd(const fixed_t &in)",        ANGLEBINOP(+, const AeonFixed &)         },
   { "angle_t opAdd(const int val)",            ANGLEBINOP(+, const int),                },
   { "angle_t opSub(const angle_t &in)",        ANGLEBINOP(-, const AeonAngle &)         },
   { "angle_t opSub(const fixed_t &in)",        ANGLEBINOP(-, const AeonFixed &)         },
   { "angle_t opSub(const int val)",            ANGLEBINOP(-, const int),                },
   { "angle_t opMul(const fixed_t &in)",        ANGLEBINOP(*, const AeonFixed &)         },
   { "angle_t opMul(const int val)",            ANGLEBINOP(*, const int)                 },
   { "angle_t opDiv(const angle_t &in)",        ANGLEBINOP(/, const AeonAngle &)         },
   { "angle_t opDiv(const fixed_t &in)",        ANGLEBINOP(/, const AeonFixed &)         },
   { "angle_t opDiv(const int val)",            ANGLEBINOP(/, const int)                 },
   { "angle_t &opAddAssign(const angle_t &in)", ANGLEASSIGNOP(+=, const AeonAngle &)     },
   { "angle_t &opAddAssign(const fixed_t &in)", ANGLEASSIGNOP(+=, const AeonFixed &)     },
   { "angle_t &opAddAssign(const int val)",     ANGLEASSIGNOP(+=, const int)             },
   { "angle_t &opSubAssign(const angle_t &in)", ANGLEASSIGNOP(-=, const AeonAngle &)     },
   { "angle_t &opSubAssign(const fixed_t &in)", ANGLEASSIGNOP(-=, const AeonFixed &)     },
   { "angle_t &opSubAssign(const int val)",     ANGLEASSIGNOP(-=, const int)             },
   { "angle_t &opMulAssign(const angle_t &in)", ANGLEASSIGNOP(*=, const AeonFixed &)     },
   { "angle_t &opMulAssign(const int val)",     ANGLEASSIGNOP(*=, const int)             },
   { "angle_t &opDivAssign(const angle_t &in)", ANGLEASSIGNOP(/=, const AeonAngle &)     },
   { "angle_t &opDivAssign(const fixed_t &in)", ANGLEASSIGNOP(/=, const AeonFixed &)     },
   { "angle_t &opDivAssign(const int val)",     ANGLEASSIGNOP(/=, const int)             },
   { "fixed_t opImplConv() const",              WRAP_MFN(AeonAngle, operator AeonFixed)  },
};

void AeonScriptObjAngle::Init()
{
   asIScriptEngine *e = AeonScriptManager::Engine();

   e->RegisterObjectType("angle_t", sizeof(AeonAngle),
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

   e->RegisterGlobalFunction("void print(angle_t)", WRAP_FN_PR(ASPrint, (AeonAngle), void),
                             asCALL_GENERIC);
}

//=============================================================================
//
// Aeon Vector Class
//

class AeonVector
{
public:
   vector_t value;

   AeonVector() : value()
   {
   }

   AeonVector(fixed_t x, fixed_t y, fixed_t z) : value({x, y, z})
   {
   }

};

void AeonScriptObjVector::Construct(AeonVector *thisVector)
{
   *thisVector = AeonVector();
}

void AeonScriptObjVector::ConstructFromOther(const AeonVector &other, AeonVector *thisVector)
{
   *thisVector = AeonVector(other);
}

void AeonScriptObjVector::ConstructFromFixed(fixed_t x, fixed_t y, fixed_t z,
                                             AeonVector *thisVector)
{
   *thisVector = AeonVector(x, y, z);
}

static void ASPrint(AeonVector f)
{
   C_Printf("x: %f, y: %f, z: %f\n", M_FixedToDouble(f.value.x),
                                     M_FixedToDouble(f.value.y),
                                     M_FixedToDouble(f.value.z));
}

void AeonScriptObjVector::Init()
{
   asIScriptEngine *e = AeonScriptManager::Engine();

   e->RegisterObjectType("eVector", sizeof(AeonAngle),
                         asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CA | asOBJ_APP_CLASS_ALLINTS);

   e->RegisterObjectBehaviour("eVector", asBEHAVE_CONSTRUCT, "void f()",
                              WRAP_OBJ_LAST(Construct), asCALL_GENERIC);
   e->RegisterObjectBehaviour("eVector", asBEHAVE_CONSTRUCT, "void f(const eVector &in)",
                              WRAP_OBJ_LAST(ConstructFromOther), asCALL_GENERIC);
   e->RegisterObjectBehaviour("eVector", asBEHAVE_CONSTRUCT,
                              "void f(const fixed_t, const fixed_t, const fixed_t)",
                              WRAP_OBJ_LAST(ConstructFromFixed), asCALL_GENERIC);

   e->RegisterObjectProperty("eVector", "fixed_t x", asOFFSET(AeonVector, value.x));
   e->RegisterObjectProperty("eVector", "fixed_t y", asOFFSET(AeonVector, value.y));
   e->RegisterObjectProperty("eVector", "fixed_t z", asOFFSET(AeonVector, value.z));

   e->RegisterGlobalFunction("void print(eVector)", WRAP_FN_PR(ASPrint, (AeonVector), void),
                              asCALL_GENERIC);
}

// EOF

