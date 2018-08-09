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

#include "aeon_common.h"
#include "aeon_fixed.h"
#include "aeon_system.h"
#include "c_io.h"
#include "m_fixed.h"

AeonFixed AeonFixed::operator + (const AeonFixed &in) { return value + in.value;          }
AeonFixed AeonFixed::operator + (const int val)     { return value + (val << FRACBITS); }
AeonFixed AeonFixed::operator * (const AeonFixed &in) { return FixedMul(value, in.value); }
AeonFixed AeonFixed::operator * (const int val)     { return value * val;               }
AeonFixed AeonFixed::operator / (const AeonFixed &in) { return FixedDiv(value, in.value); }
AeonFixed AeonFixed::operator / (const int val)     { return value / val;               }

AeonFixed &AeonFixed::operator += (const AeonFixed &in)
{
   value += in.value;
   return *this;
}
AeonFixed &AeonFixed::operator += (const int val)
{
   value += val << FRACBITS;
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
   thisFixed->value = other << FRACBITS;
}

void AeonScriptObjFixed::ConstructFromPair(int16_t integer, double frac, AeonFixed *thisFixed)
{
   thisFixed->value = static_cast<int32_t>(integer) << FRACBITS;
   if(frac < 1.0 && frac >= 0.0)
      thisFixed->value |= M_DoubleToFixed(frac);
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
   { "fixed opAdd(const fixed &in)",        FIXEDBINOP(+, const AeonFixed &)         },
   { "fixed opAdd(const int val)",          FIXEDBINOP(+, const int),                },
   { "fixed opMul(const fixed &in)",        FIXEDBINOP(*, const AeonFixed &)         },
   { "fixed opMul(const int val)",          FIXEDBINOP(*, const int)                 },
   { "fixed opDiv(const fixed &in)",        FIXEDBINOP(/, const AeonFixed &)         },
   { "fixed opDiv(const int val)",          FIXEDBINOP(/, const int)                 },
   { "fixed &opAddAssign(const fixed &in)", FIXEDASSIGNOP(+=, const AeonFixed &)     },
   { "fixed &opAddAssign(const int val)",   FIXEDASSIGNOP(+=, const int)             },
   { "fixed &opMulAssign(const fixed &in)", FIXEDASSIGNOP(*=, const AeonFixed &)     },
   { "fixed &opMulAssign(const int val)",   FIXEDASSIGNOP(*=, const int)             },
   { "fixed &opDivAssign(const fixed &in)", FIXEDASSIGNOP(/=, const AeonFixed &)     },
   { "fixed &opDivAssign(const int val)",   FIXEDASSIGNOP(/=, const int)             },
   { "double opImplConv() const",           WRAP_MFN(AeonFixed, operator double)     },
};

void AeonScriptObjFixed::Init()
{
   asIScriptEngine *e = AeonScriptManager::Engine();

   e->RegisterObjectType("fixed", sizeof(AeonFixed),
                         asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CA | asOBJ_APP_CLASS_ALLINTS);

   e->RegisterObjectBehaviour("fixed", asBEHAVE_CONSTRUCT, "void f()",
                              WRAP_OBJ_LAST(Construct), asCALL_GENERIC);
   e->RegisterObjectBehaviour("fixed", asBEHAVE_CONSTRUCT, "void f(const fixed &in)",
                              WRAP_OBJ_LAST(ConstructFromOther), asCALL_GENERIC);
   e->RegisterObjectBehaviour("fixed", asBEHAVE_CONSTRUCT, "void f(const double)",
                              WRAP_OBJ_LAST(ConstructFromDouble), asCALL_GENERIC);
   e->RegisterObjectBehaviour("fixed", asBEHAVE_CONSTRUCT, "void f(const int)",
                              WRAP_OBJ_LAST(ConstructFromInt), asCALL_GENERIC);
   e->RegisterObjectBehaviour("fixed", asBEHAVE_CONSTRUCT, "void f(const int16, const double)",
                              WRAP_OBJ_LAST(ConstructFromPair), asCALL_GENERIC);

   for(const aeonfuncreg_t &fn : fixedFuncs)
      e->RegisterObjectMethod("fixed", fn.declaration, fn.funcPointer, asCALL_GENERIC);

   e->RegisterGlobalFunction("void print(fixed)", WRAP_FN_PR(ASPrint, (AeonFixed), void),
                             asCALL_GENERIC);
}

// EOF

