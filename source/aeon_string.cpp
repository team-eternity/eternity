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
// Purpose: Aeon wrapper for qstring
// Authors: James Haley, Max Waine
//

#include "aeon_common.h"
#include "aeon_string.h"
#include "aeon_system.h"
#include "c_io.h"
#include "m_qstr.h"

namespace Aeon
{
   class RefQString : public qstring
   {
   protected:
      unsigned int refcount;

   public:
      RefQString() : qstring(), refcount(1) {}
      RefQString(const char *str) : qstring(str), refcount(1) {}
      RefQString(const RefQString &other) : qstring(other), refcount(1) {}

      // Basic factory
      static qstring *Factory()
      {
         auto qstr = new RefQString();
         return qstr;
      }

      static qstring *FactoryFromOther(const qstring &other)
      {
         auto qstr = new RefQString(static_cast<const RefQString &>(other));
         return qstr;
      }

      static void AddRef(qstring *qstr)
      {
         auto asRefQStr = static_cast<RefQString *>(qstr);
         ++asRefQStr->refcount;
      }

      static void Release(qstring *qstr)
      {
         auto asRefQStr = static_cast<RefQString *>(qstr);
         if(--asRefQStr->refcount == 0)
            delete asRefQStr;
      }
   };

   class StringFactory : public asIStringFactory
   {
   public:
      // TODO: Cache
      const void *GetStringConstant(const char *data, asUINT length) override
      {
         qstring *newStr = RefQString::Factory();
         newStr->copy(data, length);
         return static_cast<const void *>(newStr);
      }

      int ReleaseStringConstant(const void *str) override
      {
         RefQString::Release(static_cast<qstring *>(const_cast<void *>(str)));
         return 0;
      }

      int GetRawStringData(const void *str, char *data, asUINT *length) const override
      {
         auto qstr = static_cast<const qstring *>(str);
         if(data == nullptr)
            *length = asUINT(qstr->length() + 1);
         else
            qstr->copyInto(data, *length);
         return 0;

      }
   } static stringFactory;

   //
   // npos wrapper
   //
   static unsigned int qstrGetNpos(qstring *qstr)
   {
      return unsigned(qstring::npos);
   }

   //
   // Wrappers for operator []
   //
   static int qstrGetOpIdx(const qstring *qstr, int idx)
   {
      return (*qstr)[idx];
   }

   static void qstrSetOpIdx(qstring *qstr, int idx, int value)
   {
      (*qstr)[idx] = static_cast<char>(value);
   }

   //
   // Print function for AngelScript
   //
   static void asPrint(const qstring &qstr)
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

   #define XFORMSIG(name) "String &" name "(const String &in)"
   #define QUERYSIG(name) "bool " name "(const String &in) const"
   #define COMPRSIG(name) "int " name "(const String &in) const"
   #define MARGSIG(name)  "String &" name "(String &inout) const"

   static const aeonfuncreg_t qstringFuncs[] =
   {
      { "uint npos() const",              WRAP_OBJ_FIRST(qstrGetNpos)  },
      { "uint length() const",            QSTRMETHOD(length)           },
      { "bool empty() const",             QSTRMETHOD(empty)            },
      { "String &clear()",                QSTRMETHOD(clear)            },
      { "char charAt(uint idx) const",    QSTRMETHOD(charAt)           },
      { "uchar ucharAt(uint idx) const",  QSTRMETHOD(ucharAt)          },
      { "String &push(char ch)",          QSTRMETHOD(Putc)             },
      { "String &pop()",                  QSTRMETHOD(Delc)             },
      { XFORMSIG("concat"),               QSTRXFORM(concat)            },
      { QUERYSIG("compare"),              QSTRQUERY(compare)           },
      { "uint hashCode() const",          QSTRMETHOD(hashCode)         },
      { "uint hashCodeCase() const",      QSTRMETHOD(hashCodeCase)     },
      { COMPRSIG("strCmp"),               QSTRCOMPR(strCmp)            },
      { XFORMSIG("copy"),                 QSTRXFORM(copy)              },
      { MARGSIG("copyInto"),              QSTRMARG(copyInto)           },
      { "void swapWith(String &inout)",   QSTRMETHOD(swapWith)         },
      { "String &toUpper()",              QSTRMETHOD(toUpper)          },
      { "String &toLower()",              QSTRMETHOD(toLower)          },
      { "int toInt() const",              QSTRMETHOD(toInt)            },
      { "uint findFirstOf(int) const",    QSTRMETHOD(findFirstOf)      },
      { "uint findFirstNotOf(int) const", QSTRMETHOD(findFirstNotOf)   },
      { "uint findLastOf(int) const",     QSTRMETHOD(findLastOf)       },
      { XFORMSIG("opAssign"),             QSTRXFORM(operator =)        },
      { XFORMSIG("opAddAssign"),          QSTRXFORM(operator +=)       },
      { QUERYSIG("opEquals"),             QSTRQUERY(compare)           },
      { COMPRSIG("opCmp"),                QSTRCOMPR(strCmp)            },
      { XFORMSIG("opShl"),                QSTRXFORM(operator <<)       },
      { "String &opShl(int)",             QSTRFMINT(operator <<)       },
      { "String &opShl(double)",          QSTRFMDBL(operator <<)       },
      { "int get_opIndex(int) const",     WRAP_OBJ_FIRST(qstrGetOpIdx) },
      { "void set_opIndex(int, int)",     WRAP_OBJ_FIRST(qstrSetOpIdx) },
   };

   //
   // Register String (qstring) as a reftype and register desired methods
   //
   void ScriptObjString::Init()
   {
      asIScriptEngine *const e = ScriptManager::Engine();

      // register type
      e->RegisterObjectType("String", 0, asOBJ_REF);

      // register behaviors
      e->RegisterObjectBehaviour("String", asBEHAVE_FACTORY, "String @f()",
                                 WRAP_FN(RefQString::Factory), asCALL_GENERIC);
      e->RegisterObjectBehaviour("String", asBEHAVE_FACTORY, "String @f(const String &in)",
                                 WRAP_FN(RefQString::FactoryFromOther), asCALL_GENERIC);
      e->RegisterObjectBehaviour("String", asBEHAVE_ADDREF, "void f()",
                                 WRAP_OBJ_FIRST(RefQString::AddRef), asCALL_GENERIC);
      e->RegisterObjectBehaviour("String", asBEHAVE_RELEASE, "void f()",
                                 WRAP_OBJ_FIRST(RefQString::Release), asCALL_GENERIC);

      // register String as the string factory
      // "qstring @"???
      e->RegisterStringFactory("String", &stringFactory);

      for(const aeonfuncreg_t &fn : qstringFuncs)
         e->RegisterObjectMethod("String", fn.declaration, fn.funcPointer, asCALL_GENERIC);

      // register global print func
      e->RegisterGlobalFunction("void print(const String &in)",
                                WRAP_FN_PR(asPrint, (const qstring &), void),
                                asCALL_GENERIC);
   }

   //
   // Creates a new reference qstring, for usage elsewhere
   //
   qstring *CreateRefQString(const char *str)
   {
      return static_cast<qstring *>(new RefQString(str));
   }
}

// EOF

