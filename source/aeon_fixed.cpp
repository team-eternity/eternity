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

#include "angelscript.h"

#include "aeon_common.h"
#include "aeon_fixed.h"
#include "c_io.h"
#include "m_fixed.h"

ASFixed ASFixed::operator + (const ASFixed &in) { return value + in.value; }
ASFixed ASFixed::operator + (const int val) { return value + (val << FRACBITS); }
ASFixed &ASFixed::operator += (const ASFixed &in)
{
   value += in.value;
   return *this;
}
ASFixed &ASFixed::operator += (const int val)
{
   value += val << FRACBITS;
   return *this;
}

ASFixed ASFixed::operator * (const ASFixed &in) { return FixedMul(value, in.value); }
ASFixed ASFixed::operator * (const int val)     { return value * val; }
ASFixed &ASFixed::operator *= (const ASFixed &in)
{
   value = FixedMul(value, in.value);
   return *this;
}
ASFixed &ASFixed::operator *= (const int val)
{
   value *= val;
   return *this;
}

ASFixed ASFixed::operator / (const ASFixed &in) { return FixedDiv(value, in.value); }
ASFixed ASFixed::operator / (const int val)     { return value / val; }
ASFixed &ASFixed::operator /= (const ASFixed &in)
{
   value = FixedDiv(value, in.value);
   return *this;
}
ASFixed &ASFixed::operator /= (const int val)
{
   value /= val;
   return *this;
}

void ASScriptObjFixed::Construct(ASFixed *thisFixed)
{
   *thisFixed = ASFixed();
}

void ASScriptObjFixed::ConstructFromOther(const ASFixed &other, ASFixed *thisFixed)
{
   *thisFixed = ASFixed(other);
}

void ASScriptObjFixed::ConstructFromDouble(double other, ASFixed *thisFixed)
{
   thisFixed->value = M_DoubleToFixed(other);
}

void ASScriptObjFixed::ConstructFromInt(int other, ASFixed *thisFixed)
{
   thisFixed->value = other << FRACBITS;
}

#define FIXEDBINOP(op, param) \
   asMETHODPR(ASFixed, operator op,  (param), ASFixed)

#define FIXEDASSIGNOP(op, param) \
   asMETHODPR(ASFixed, operator op,  (param), ASFixed &)

static aeonfuncreg_t fixedFuncs[] =
{
   { "fixed opAdd(const fixed &in)",        FIXEDBINOP(+, const ASFixed &),     asCALL_THISCALL },
   { "fixed opAdd(const int val)",          FIXEDBINOP(+, const int),           asCALL_THISCALL },
   { "fixed opMul(const fixed &in)",        FIXEDBINOP(*, const ASFixed &),     asCALL_THISCALL },
   { "fixed opMul(const int val)",          FIXEDBINOP(*, const int),           asCALL_THISCALL },
   { "fixed opDiv(const fixed &in)",        FIXEDBINOP(/, const ASFixed &),     asCALL_THISCALL },
   { "fixed opDiv(const int val)",          FIXEDBINOP(/, const int),           asCALL_THISCALL },
   { "fixed &opAddAssign(const fixed &in)", FIXEDASSIGNOP(+=, const ASFixed &), asCALL_THISCALL },
   { "fixed &opAddAssign(const int val)",   FIXEDASSIGNOP(+=, const int),       asCALL_THISCALL },
   { "fixed &opMulAssign(const fixed &in)", FIXEDASSIGNOP(*=, const ASFixed &), asCALL_THISCALL },
   { "fixed &opMulAssign(const int val)",   FIXEDASSIGNOP(*=, const int),       asCALL_THISCALL },
   { "fixed &opDivAssign(const fixed &in)", FIXEDASSIGNOP(/=, const ASFixed &), asCALL_THISCALL },
   { "fixed &opDivAssign(const int val)",   FIXEDASSIGNOP(/=, const int),       asCALL_THISCALL },
   { "double opImplConv()", asMETHODPR(ASFixed, operator double, (void), double), asCALL_THISCALL },
};

void ASScriptObjFixed::Init(asIScriptEngine *e)
{
   e->RegisterObjectType("fixed", sizeof(ASFixed),
                         asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CA | asOBJ_APP_CLASS_ALLINTS);

   e->RegisterObjectBehaviour("fixed", asBEHAVE_CONSTRUCT, "void f()",
                              asFUNCTION(Construct), asCALL_CDECL_OBJLAST);
   e->RegisterObjectBehaviour("fixed", asBEHAVE_CONSTRUCT, "void f(const fixed &in)",
                              asFUNCTION(ConstructFromOther), asCALL_CDECL_OBJLAST);
   e->RegisterObjectBehaviour("fixed", asBEHAVE_CONSTRUCT, "void f(const double)",
                              asFUNCTION(ConstructFromDouble), asCALL_CDECL_OBJLAST);
   e->RegisterObjectBehaviour("fixed", asBEHAVE_CONSTRUCT, "void f(const int)",
                              asFUNCTION(ConstructFromInt), asCALL_CDECL_OBJLAST);

   for(const aeonfuncreg_t &fn : fixedFuncs)
      e->RegisterObjectMethod("fixed", fn.declaration, fn.funcPointer, fn.callConv);
}

// EOF

