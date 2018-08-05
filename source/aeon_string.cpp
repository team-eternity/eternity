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
// Most of this code is based on Quasar's astest.
//
// Purpose: Aeon string functions
// Authors: James Haley, Max Waine
//

#include "aeon_common.h"
#include "aeon_fixed.h"
#include "aeon_string.h"
#include "c_io.h"
#include "i_system.h"
#include "m_fixed.h"
#include "m_qstr.h"
#include "m_utils.h"

class ASRefQString : public qstring
{
protected:
   unsigned int refcount;

public:
   ASRefQString() : qstring(), refcount(1) {}
   ASRefQString(const ASRefQString &other) : qstring(other), refcount(1) {}

   // Basic factory
   static qstring *Factory()
   {
      auto qstr = new ASRefQString();
      return qstr;
   }

   static qstring *FactoryFromOther(const qstring &other)
   {
      auto qstr = new ASRefQString(static_cast<const ASRefQString &>(other));
      return qstr;
   }

   static void AddRef(qstring *qstr)
   {
      auto asRefQStr = static_cast<ASRefQString *>(qstr);
      ++asRefQStr->refcount;
   }

   static void Release(qstring *qstr)
   {
      auto asRefQStr = static_cast<ASRefQString *>(qstr);
      if(--asRefQStr->refcount == 0)
         delete asRefQStr;
   }
};

class ASStringFactory : public asIStringFactory
{
public:
   // TODO: Cache
   const void *GetStringConstant(const char *data, asUINT length) override
   {
      auto newStr = ASRefQString::Factory();
      newStr->copy(data, length);
      return static_cast<qstring *>(newStr);
   }

   int ReleaseStringConstant(const void *str) override
   {
      ASRefQString::Release(static_cast<qstring *>(const_cast<void *>(str)));
      return 0;
   }

   int GetRawStringData(const void *str, char *data, asUINT *length) const override
   {
      auto qstr = static_cast<const qstring *>(str);
      if(data == nullptr)
         *length = qstr->length() + 1;
      else
         qstr->copyInto(data, *length);
      return 0;

   }
} static aeonStringFactory;

//
// npos wrapper
//
static unsigned int QStrGetNpos(qstring *qstr)
{
   return qstring::npos;
}

//
// Wrappers for operator []
//
static int QStrGetOpIdx(const qstring *qstr, int idx)
{
   return (*qstr)[idx];
}

static void QStrSetOpIdx(qstring *qstr, int idx, int value)
{
   (*qstr)[idx] = static_cast<char>(value);
}

//
// Print function for AngelScript
//
static void ASPrint(const qstring &qstr)
{
   C_Printf("%s\n", qstr.constPtr());
}

template<typename T>
class sizer
{
public:
   static const size_t size = sizeof(T);
};

#define QSTRMETHOD(m) asMETHOD(qstring, m)
#define QSTRXFORM(m)  asMETHODPR(qstring, m, (const qstring &),       qstring &)
#define QSTRQUERY(m)  asMETHODPR(qstring, m, (const qstring &) const, bool)
#define QSTRCOMPR(m)  asMETHODPR(qstring, m, (const qstring &) const, int)
#define QSTRMARG(m)   asMETHODPR(qstring, m, (qstring &) const,       qstring &)
#define QSTRFMINT(m)  asMETHODPR(qstring, m, (int),                   qstring &)
#define QSTRFMDBL(m)  asMETHODPR(qstring, m, (double),                qstring &)

#define XFORMSIG(name) "qstring &" name "(const qstring &in)"
#define QUERYSIG(name) "bool " name "(const qstring &in) const"
#define COMPRSIG(name) "int " name "(const qstring &in) const"
#define MARGSIG(name)  "qstring &" name "(qstring &inout) const"

static aeonfuncreg_t qstringFuncs[] =
{
   { "uint npos() const",              asFUNCTION(QStrGetNpos),    asCALL_CDECL_OBJFIRST },
   { "uint length() const",            QSTRMETHOD(length),         asCALL_THISCALL       },
   { "bool empty() const",             QSTRMETHOD(empty),          asCALL_THISCALL       },
   { "qstring &clear()",               QSTRMETHOD(clear),          asCALL_THISCALL       },
   { "char charAt(uint idx) const",    QSTRMETHOD(charAt),         asCALL_THISCALL       },
   { "uchar ucharAt(uint idx) const",  QSTRMETHOD(ucharAt),        asCALL_THISCALL       },
   { "qstring &push(char ch)",         QSTRMETHOD(Putc),           asCALL_THISCALL       },
   { "qstring &pop()",                 QSTRMETHOD(Delc),           asCALL_THISCALL       },
   { XFORMSIG("concat"),               QSTRXFORM(concat),          asCALL_THISCALL       },
   { QUERYSIG("compare"),              QSTRQUERY(compare),         asCALL_THISCALL       },
   { "uint hashCode() const",          QSTRMETHOD(hashCode),       asCALL_THISCALL       },
   { "uint hashCodeCase() const",      QSTRMETHOD(hashCodeCase),   asCALL_THISCALL       },
   { COMPRSIG("strCmp"),               QSTRCOMPR(strCmp),          asCALL_THISCALL       },
   { XFORMSIG("copy"),                 QSTRXFORM(copy),            asCALL_THISCALL       },
   { MARGSIG("copyInto"),              QSTRMARG(copyInto),         asCALL_THISCALL       },
   { "void swapWith(qstring &inout)",  QSTRMETHOD(swapWith),       asCALL_THISCALL       },
   { "qstring &toUpper()",             QSTRMETHOD(toUpper),        asCALL_THISCALL       },
   { "qstring &toLower()",             QSTRMETHOD(toLower),        asCALL_THISCALL       },
   { "int toInt() const",              QSTRMETHOD(toInt),          asCALL_THISCALL       },
   { "uint findFirstOf(int) const",    QSTRMETHOD(findFirstOf),    asCALL_THISCALL       },
   { "uint findFirstNotOf(int) const", QSTRMETHOD(findFirstNotOf), asCALL_THISCALL       },
   { "uint findLastOf(int) const",     QSTRMETHOD(findLastOf),     asCALL_THISCALL       },
   { XFORMSIG("opAssign"),             QSTRXFORM(operator =),      asCALL_THISCALL       },
   { XFORMSIG("opAddAssign"),          QSTRXFORM(operator +=),     asCALL_THISCALL       },
   { QUERYSIG("opEquals"),             QSTRQUERY(compare),         asCALL_THISCALL       },
   { COMPRSIG("opCmp"),                QSTRCOMPR(strCmp),          asCALL_THISCALL       },
   { XFORMSIG("opShl"),                QSTRXFORM(operator <<),     asCALL_THISCALL       },
   { "qstring &opShl(int)",            QSTRFMINT(operator <<),     asCALL_THISCALL       },
   { "qstring &opShl(double)",         QSTRFMDBL(operator <<),     asCALL_THISCALL       },
   { "int get_opIndex(int) const",     asFUNCTION(QStrGetOpIdx),   asCALL_CDECL_OBJFIRST },
   { "void set_opIndex(int, int)",     asFUNCTION(QStrSetOpIdx),   asCALL_CDECL_OBJFIRST },
};

//
// Register qstring as a reftype and register desired methods
//
void AeonScriptObjQString::Init(asIScriptEngine *e)
{
   // register type
   e->RegisterObjectType("qstring", 0, asOBJ_REF);

   // register behaviors
   e->RegisterObjectBehaviour("qstring", asBEHAVE_FACTORY, "qstring @f()",
                              asFUNCTION(ASRefQString::Factory), asCALL_CDECL);
   e->RegisterObjectBehaviour("qstring", asBEHAVE_FACTORY, "qstring @f(const qstring &in)",
                              asFUNCTION(ASRefQString::FactoryFromOther), asCALL_CDECL);
   e->RegisterObjectBehaviour("qstring", asBEHAVE_ADDREF, "void f()",
                              asFUNCTION(ASRefQString::AddRef), asCALL_CDECL_OBJFIRST);
   e->RegisterObjectBehaviour("qstring", asBEHAVE_RELEASE, "void f()",
                              asFUNCTION(ASRefQString::Release), asCALL_CDECL_OBJFIRST);

   // register qstring as the string factory
   // "qstring @"???
   e->RegisterStringFactory("qstring", &aeonStringFactory);

   for(const aeonfuncreg_t &fn : qstringFuncs)
      e->RegisterObjectMethod("qstring", fn.declaration, fn.funcPointer, fn.callConv);

   // register global print func
   e->RegisterGlobalFunction("void print(const qstring &in)",
                             asFUNCTIONPR(ASPrint, (const qstring &), void),
                             asCALL_CDECL);
}

// EOF

