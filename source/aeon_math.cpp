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
#include "tables.h"

//=============================================================================
//
// Aeon fixed-point class
//

AeonFixed AeonFixed::operator + (const AeonFixed &in) { return value + in.value;          }
AeonFixed AeonFixed::operator + (const int val)       { return value + (val * FRACUNIT);  }
AeonFixed AeonFixed::operator - (const AeonFixed &in) { return value - in.value;          }
AeonFixed AeonFixed::operator - (const int val)       { return value - (val * FRACUNIT);  }
AeonFixed AeonFixed::operator * (const AeonFixed &in) { return FixedMul(value, in.value); }
AeonFixed AeonFixed::operator * (const int val)       { return value * val;               }
AeonFixed AeonFixed::operator / (const AeonFixed &in) { return FixedDiv(value, in.value); }
AeonFixed AeonFixed::operator / (const int val)       { return value / val;               }

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
   { "eFixed opAdd(const eFixed &in)",        FIXEDBINOP(+, const AeonFixed &)         },
   { "eFixed opAdd(const int val)",           FIXEDBINOP(+, const int),                },
   { "eFixed opSub(const eFixed &in)",        FIXEDBINOP(-, const AeonFixed &)         },
   { "eFixed opSub(const int val)",           FIXEDBINOP(-, const int),                },
   { "eFixed opMul(const eFixed &in)",        FIXEDBINOP(*, const AeonFixed &)         },
   { "eFixed opMul(const int val)",           FIXEDBINOP(*, const int)                 },
   { "eFixed opDiv(const eFixed &in)",        FIXEDBINOP(/, const AeonFixed &)         },
   { "eFixed opDiv(const int val)",           FIXEDBINOP(/, const int)                 },
   { "eFixed &opAddAssign(const eFixed &in)", FIXEDASSIGNOP(+=, const AeonFixed &)     },
   { "eFixed &opAddAssign(const int val)",    FIXEDASSIGNOP(+=, const int)             },
   { "eFixed &opSubAssign(const eFixed &in)", FIXEDASSIGNOP(-=, const AeonFixed &)     },
   { "eFixed &opSubAssign(const int val)",    FIXEDASSIGNOP(-=, const int)             },
   { "eFixed &opMulAssign(const eFixed &in)", FIXEDASSIGNOP(*=, const AeonFixed &)     },
   { "eFixed &opMulAssign(const int val)",    FIXEDASSIGNOP(*=, const int)             },
   { "eFixed &opDivAssign(const eFixed &in)", FIXEDASSIGNOP(/=, const AeonFixed &)     },
   { "eFixed &opDivAssign(const int val)",    FIXEDASSIGNOP(/=, const int)             },
   { "double opImplConv() const",             WRAP_MFN(AeonFixed, operator double)     },
};

void AeonScriptObjFixed::Init()
{
   asIScriptEngine *e = AeonScriptManager::Engine();

   e->RegisterObjectType("eFixed", sizeof(AeonFixed),
                         asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CA | asOBJ_APP_CLASS_ALLINTS);

   e->RegisterObjectBehaviour("eFixed", asBEHAVE_CONSTRUCT, "void f()",
                              WRAP_OBJ_LAST(Construct), asCALL_GENERIC);
   e->RegisterObjectBehaviour("eFixed", asBEHAVE_CONSTRUCT, "void f(const eFixed &in)",
                              WRAP_OBJ_LAST(ConstructFromOther), asCALL_GENERIC);
   e->RegisterObjectBehaviour("eFixed", asBEHAVE_CONSTRUCT, "void f(const double)",
                              WRAP_OBJ_LAST(ConstructFromDouble), asCALL_GENERIC);
   e->RegisterObjectBehaviour("eFixed", asBEHAVE_CONSTRUCT, "void f(const int)",
                              WRAP_OBJ_LAST(ConstructFromInt), asCALL_GENERIC);
   e->RegisterObjectBehaviour("eFixed", asBEHAVE_CONSTRUCT, "void f(const int16, const double)",
                              WRAP_OBJ_LAST(ConstructFromPair), asCALL_GENERIC);

   e->SetDefaultNamespace("eFixed");
   e->RegisterGlobalFunction("eFixed FromBits(const int val)",
                             WRAP_FN(ConstructFromBits), asCALL_GENERIC);
   e->SetDefaultNamespace("");

   for(const aeonfuncreg_t &fn : fixedFuncs)
      e->RegisterObjectMethod("eFixed", fn.declaration, fn.funcPointer, asCALL_GENERIC);

   e->RegisterGlobalFunction("void print(eFixed)", WRAP_FN_PR(ASPrint, (AeonFixed), void),
                             asCALL_GENERIC);
}

//=============================================================================
//
// Aeon angle class
//

//
// Clamps a fixed_t to be within the range representable by an angle_t
// then converts it to an angle_t
//
static inline angle_t FixedToAngleClamped(const fixed_t val)
{
   return FixedToAngle(val % (360 << FRACBITS));
}

AeonAngle AeonAngle::operator + (const AeonAngle &in)  { return value + in.value; }
AeonAngle AeonAngle::operator + (const AeonFixed &in)  { return value + (FixedToAngle(in.value)); }
AeonAngle AeonAngle::operator + (const int val)        { return value + (val * ANGLE_1); }
AeonAngle AeonAngle::operator - (const AeonAngle &in)  { return value - in.value; }
AeonAngle AeonAngle::operator - (const AeonFixed &in)  { return value - (FixedToAngle(in.value)); }
AeonAngle AeonAngle::operator - (const int val)        { return value - (val * ANGLE_1); }

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
   value += val * ANGLE_1;
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
   value -= val * ANGLE_1;
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
   thisAngle->value = other * ANGLE_1;
}

static void ASPrint(AeonAngle f)
{
   C_Printf("%f\n", M_FixedToDouble(AngleToFixed(f.value)));
}

static aeonfuncreg_t angleFuncs[] =
{
   { "eAngle opAdd(const eAngle &in)",         ANGLEBINOP(+, const AeonAngle &)         },
   { "eAngle opAdd(const eFixed &in)",         ANGLEBINOP(+, const AeonFixed &)         },
   { "eAngle opAdd(const int val)",            ANGLEBINOP(+, const int),                },
   { "eAngle opSub(const eAngle &in)",         ANGLEBINOP(-, const AeonAngle &)         },
   { "eAngle opSub(const eFixed &in)",         ANGLEBINOP(-, const AeonFixed &)         },
   { "eAngle opSub(const int val)",            ANGLEBINOP(-, const int),                },
   { "eAngle opMul(const eFixed &in)",         ANGLEBINOP(*, const AeonFixed &)         },
   { "eAngle opMul(const int val)",            ANGLEBINOP(*, const int)                 },
   { "eAngle opDiv(const eAngle &in)",         ANGLEBINOP(/, const AeonAngle &)         },
   { "eAngle opDiv(const eFixed &in)",         ANGLEBINOP(/, const AeonFixed &)         },
   { "eAngle opDiv(const int val)",            ANGLEBINOP(/, const int)                 },
   { "eAngle &opAddAssign(const eAngle &in)",  ANGLEASSIGNOP(+=, const AeonAngle &)     },
   { "eAngle &opAddAssign(const eFixed &in)",  ANGLEASSIGNOP(+=, const AeonFixed &)     },
   { "eAngle &opAddAssign(const int val)",     ANGLEASSIGNOP(+=, const int)             },
   { "eAngle &opSubAssign(const eAngle &in)",  ANGLEASSIGNOP(-=, const AeonAngle &)     },
   { "eAngle &opSubAssign(const eFixed &in)",  ANGLEASSIGNOP(-=, const AeonFixed &)     },
   { "eAngle &opSubAssign(const int val)",     ANGLEASSIGNOP(-=, const int)             },
   { "eAngle &opMulAssign(const eAngle &in)",  ANGLEASSIGNOP(*=, const AeonFixed &)     },
   { "eAngle &opMulAssign(const int val)",     ANGLEASSIGNOP(*=, const int)             },
   { "eAngle &opDivAssign(const eAngle &in)",  ANGLEASSIGNOP(/=, const AeonAngle &)     },
   { "eAngle &opDivAssign(const eFixed &in)",  ANGLEASSIGNOP(/=, const AeonFixed &)     },
   { "eAngle &opDivAssign(const int val)",     ANGLEASSIGNOP(/=, const int)             },
   { "eFixed opImplConv() const",              WRAP_MFN(AeonAngle, operator AeonFixed)  },
};

void AeonScriptObjAngle::Init()
{
   asIScriptEngine *e = AeonScriptManager::Engine();

   e->RegisterObjectType("eAngle", sizeof(AeonAngle),
                         asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CA | asOBJ_APP_CLASS_ALLINTS);

   e->RegisterObjectBehaviour("eAngle", asBEHAVE_CONSTRUCT, "void f()",
                              WRAP_OBJ_LAST(Construct), asCALL_GENERIC);
   e->RegisterObjectBehaviour("eAngle", asBEHAVE_CONSTRUCT, "void f(const eAngle &in)",
                              WRAP_OBJ_LAST(ConstructFromOther), asCALL_GENERIC);
   e->RegisterObjectBehaviour("eAngle", asBEHAVE_CONSTRUCT, "void f(const double)",
                              WRAP_OBJ_LAST(ConstructFromDouble), asCALL_GENERIC);
   e->RegisterObjectBehaviour("eAngle", asBEHAVE_CONSTRUCT, "void f(const int)",
                              WRAP_OBJ_LAST(ConstructFromInt), asCALL_GENERIC);

   for(const aeonfuncreg_t &fn : angleFuncs)
      e->RegisterObjectMethod("eAngle", fn.declaration, fn.funcPointer, asCALL_GENERIC);

   e->RegisterGlobalFunction("void print(eAngle)", WRAP_FN_PR(ASPrint, (AeonAngle), void),
                             asCALL_GENERIC);
}

struct vector_t
{
   fixed_t x;
   fixed_t y;
   fixed_t z;
};

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
                              "void f(const eFixed, const eFixed, const eFixed)",
                              WRAP_OBJ_LAST(ConstructFromFixed), asCALL_GENERIC);

   e->RegisterObjectProperty("eVector", "eFixed x", asOFFSET(AeonVector, value.x));
   e->RegisterObjectProperty("eVector", "eFixed y", asOFFSET(AeonVector, value.y));
   e->RegisterObjectProperty("eVector", "eFixed z", asOFFSET(AeonVector, value.z));

   //for(const aeonfuncreg_t &fn : angleFuncs)
   //   e->RegisterObjectMethod("eVector", fn.declaration, fn.funcPointer, asCALL_GENERIC);

   //e->RegisterGlobalFunction("void print(eVector)", WRAP_FN_PR(ASPrint, (AeonVector), void),
   //                          asCALL_GENERIC);
}

// EOF

