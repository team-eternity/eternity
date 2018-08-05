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
#include "aeon_string.h"
#include "c_io.h"
#include "m_qstr.h"

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

#define QSTRMETHOD(m) WRAP_MFN(qstring, m)
#define QSTRXFORM(m)  WRAP_MFN_PR(qstring, m, (const qstring &),       qstring &)
#define QSTRQUERY(m)  WRAP_MFN_PR(qstring, m, (const qstring &) const, bool)
#define QSTRCOMPR(m)  WRAP_MFN_PR(qstring, m, (const qstring &) const, int)
#define QSTRMARG(m)   WRAP_MFN_PR(qstring, m, (qstring &) const,       qstring &)
#define QSTRFMINT(m)  WRAP_MFN_PR(qstring, m, (int),                   qstring &)
#define QSTRFMDBL(m)  WRAP_MFN_PR(qstring, m, (double),                qstring &)

#define XFORMSIG(name) "qstring &" name "(const qstring &in)"
#define QUERYSIG(name) "bool " name "(const qstring &in) const"
#define COMPRSIG(name) "int " name "(const qstring &in) const"
#define MARGSIG(name)  "qstring &" name "(qstring &inout) const"

static aeonfuncreg_t qstringFuncs[] =
{
   { "uint npos() const",              WRAP_OBJ_FIRST(QStrGetNpos)  },
   { "uint length() const",            QSTRMETHOD(length)           },
   { "bool empty() const",             QSTRMETHOD(empty)            },
   { "qstring &clear()",               QSTRMETHOD(clear)            },
   { "char charAt(uint idx) const",    QSTRMETHOD(charAt)           },
   { "uchar ucharAt(uint idx) const",  QSTRMETHOD(ucharAt)          },
   { "qstring &push(char ch)",         QSTRMETHOD(Putc)             },
   { "qstring &pop()",                 QSTRMETHOD(Delc)             },
   { XFORMSIG("concat"),               QSTRXFORM(concat)            },
   { QUERYSIG("compare"),              QSTRQUERY(compare)           },
   { "uint hashCode() const",          QSTRMETHOD(hashCode)         },
   { "uint hashCodeCase() const",      QSTRMETHOD(hashCodeCase)     },
   { COMPRSIG("strCmp"),               QSTRCOMPR(strCmp)            },
   { XFORMSIG("copy"),                 QSTRXFORM(copy)              },
   { MARGSIG("copyInto"),              QSTRMARG(copyInto)           },
   { "void swapWith(qstring &inout)",  QSTRMETHOD(swapWith)         },
   { "qstring &toUpper()",             QSTRMETHOD(toUpper)          },
   { "qstring &toLower()",             QSTRMETHOD(toLower)          },
   { "int toInt() const",              QSTRMETHOD(toInt)            },
   { "uint findFirstOf(int) const",    QSTRMETHOD(findFirstOf)      },
   { "uint findFirstNotOf(int) const", QSTRMETHOD(findFirstNotOf)   },
   { "uint findLastOf(int) const",     QSTRMETHOD(findLastOf)       },
   { XFORMSIG("opAssign"),             QSTRXFORM(operator =)        },
   { XFORMSIG("opAddAssign"),          QSTRXFORM(operator +=)       },
   { QUERYSIG("opEquals"),             QSTRQUERY(compare)           },
   { COMPRSIG("opCmp"),                QSTRCOMPR(strCmp)            },
   { XFORMSIG("opShl"),                QSTRXFORM(operator <<)       },
   { "qstring &opShl(int)",            QSTRFMINT(operator <<)       },
   { "qstring &opShl(double)",         QSTRFMDBL(operator <<)       },
   { "int get_opIndex(int) const",     WRAP_OBJ_FIRST(QStrGetOpIdx) },
   { "void set_opIndex(int, int)",     WRAP_OBJ_FIRST(QStrSetOpIdx) },
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
                              WRAP_FN(ASRefQString::Factory), asCALL_GENERIC);
   e->RegisterObjectBehaviour("qstring", asBEHAVE_FACTORY, "qstring @f(const qstring &in)",
                              WRAP_FN(ASRefQString::FactoryFromOther), asCALL_GENERIC);
   e->RegisterObjectBehaviour("qstring", asBEHAVE_ADDREF, "void f()",
                              WRAP_OBJ_FIRST(ASRefQString::AddRef), asCALL_GENERIC);
   e->RegisterObjectBehaviour("qstring", asBEHAVE_RELEASE, "void f()",
                              WRAP_OBJ_FIRST(ASRefQString::Release), asCALL_GENERIC);

   // register qstring as the string factory
   // "qstring @"???
   e->RegisterStringFactory("qstring", &aeonStringFactory);

   for(const aeonfuncreg_t &fn : qstringFuncs)
      e->RegisterObjectMethod("qstring", fn.declaration, fn.funcPointer, asCALL_GENERIC);

   // register global print func
   e->RegisterGlobalFunction("void print(const qstring &in)",
                             WRAP_FN_PR(ASPrint, (const qstring &), void),
                             asCALL_GENERIC);
}

// EOF

