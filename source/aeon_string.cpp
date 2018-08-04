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

#include "angelscript.h"

#include "c_io.h"
#include "i_system.h"
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
      //gPlatform->debugMsg("%p: newborn qstring leaving factory\n", qstr);
      return qstr;
   }

   static qstring *FactoryFromOther(const qstring &other)
   {
      auto qstr = new ASRefQString(static_cast<const ASRefQString &>(other));
      //gPlatform->debugMsg("%p: newborn qstring from %s leaving factory\n", qstr, other.constPtr());
      return qstr;
   }

   static void AddRef(qstring *qstr)
   {
      auto asRefQStr = static_cast<ASRefQString *>(qstr);
      ++asRefQStr->refcount;
      //gPlatform->debugMsg("%p: refcount increased to %u\n", asRefQStr, asRefQStr->refcount);
   }

   static void Release(qstring *qstr)
   {
      auto asRefQStr = static_cast<ASRefQString *>(qstr);
      if(--asRefQStr->refcount == 0)
      {
         //gPlatform->debugMsg("%p: refcount is zero, destroying\n", asRefQStr);
         delete asRefQStr;
      }
      else
         ;//gPlatform->debugMsg("%p: refcount decreased to %u\n", asRefQStr, asRefQStr->refcount);
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
unsigned int QStrGetNpos(qstring *qstr)
{
   return qstring::npos;
}

//
// Wrappers for operator []
//
int QStrGetOpIdx(const qstring *qstr, int idx)
{
   return (*qstr)[idx];
}

void QStrSetOpIdx(qstring *qstr, int idx, int value)
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

static void ASPrint(int i)
{
   C_Printf("%d\n", i);
}

static void ASPrint(unsigned int u)
{
   C_Printf("%u\n", u);
}

static void ASPrint(float f)
{
   C_Printf("%f\n", f);
}

template<typename T>
class sizer
{
public:
   static const size_t size = sizeof(T);
};

struct aeonfuncreg_t
{
   const char *declaration;
   asSFuncPtr  funcPointer;
   asDWORD     callConv;
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

static bool Aeon_RegisterClassMethods(asIScriptEngine *engine, const char *classname,
                                      aeonfuncreg_t *funcs, size_t numfuncs)
{
   for(size_t i = 0; i < numfuncs; i++)
   {
      aeonfuncreg_t &fn = funcs[i];
      if(engine->RegisterObjectMethod(classname, fn.declaration, fn.funcPointer, fn.callConv) < 0)
         return false;
   }

   return true;
}

//
// Register qstring as a reftype and register desired methods
//
bool RegisterQString(asIScriptEngine *engine)
{
   // register type
   if(engine->RegisterObjectType("qstring", 0, asOBJ_REF) < 0)
      return false;

   // register behaviors
   if(engine->RegisterObjectBehaviour("qstring", asBEHAVE_FACTORY, "qstring @f()",
                                      asFUNCTION(ASRefQString::Factory), asCALL_CDECL) < 0)
      return false;
   if(engine->RegisterObjectBehaviour("qstring", asBEHAVE_FACTORY, "qstring @f(const qstring &in)",
                                      asFUNCTION(ASRefQString::FactoryFromOther), asCALL_CDECL) < 0)
      return false;
   if(engine->RegisterObjectBehaviour("qstring", asBEHAVE_ADDREF, "void f()",
                                      asFUNCTION(ASRefQString::AddRef), asCALL_CDECL_OBJFIRST) < 0)
      return false;
   if(engine->RegisterObjectBehaviour("qstring", asBEHAVE_RELEASE, "void f()",
                                      asFUNCTION(ASRefQString::Release), asCALL_CDECL_OBJFIRST) < 0)
      return false;

   // register qstring as the string factory
   // "qstring @"???
   if(engine->RegisterStringFactory("qstring", &aeonStringFactory) < 0)
      return false;

   // register methods
   if(!Aeon_RegisterClassMethods(engine, "qstring", qstringFuncs, earrlen(qstringFuncs)))
      return false;

   /*
   // Concatenation and insertion/deletion
   qstring &insert(const char *insertstr, size_t pos);

   // Comparisons: C and C++ style
   int  strNCmp(const char *str, size_t maxcount) const;
   int  strCaseCmp(const char *str) const;
   int  strNCaseCmp(const char *str, size_t maxcount) const;

   // Substring Replacements
   size_t replace(const char *filter, char repl);
   size_t replaceNotOf(const char *filter, char repl);

   // File Path Utilities
   qstring &normalizeSlashes();
   qstring &pathConcatenate(const char *addend);
   qstring &pathConcatenate(const qstring &other);
   qstring &addDefaultExtension(const char *ext);
   qstring &removeFileSpec();
   void     extractFileBase(qstring &dest);

   // Numeric Conversions
   long   toLong(char **endptr, int radix) const;
   double toDouble(char **endptr) const;

   // Searching/Substring Finding Routines
   const char *strChr(char c) const;
   const char *strRChr(char c) const;
   const char *findSubStr(const char *substr) const;
   const char *findSubStrNoCase(const char *substr) const;
   size_t find(const char *s, size_t pos = 0) const;

   // Stripping and Truncation
   qstring &lstrip(char c);
   qstring &rstrip(char c);
   qstring &truncate(size_t pos);
   qstring &erase(size_t pos, size_t n = npos);

   // Special Formatting
   qstring &makeQuoted();
   int      printf(size_t maxlen, const char *fmt, ...);

   // Operators
   qstring &operator += (char  ch);
   qstring &operator << (char   ch);
   */

   // register global print func
   if(engine->RegisterGlobalFunction("void print(const qstring &in)",
                                     asFUNCTIONPR(ASPrint, (const qstring &), void),
                                     asCALL_CDECL) < 0)
      return false;
   if(engine->RegisterGlobalFunction("void print(int)",
                                     asFUNCTIONPR(ASPrint, (int), void),
                                     asCALL_CDECL) < 0)
      return false;
   if(engine->RegisterGlobalFunction("void print(uint)",
                                     asFUNCTIONPR(ASPrint, (unsigned int), void),
                                     asCALL_CDECL) < 0)
      return false;
   if(engine->RegisterGlobalFunction("void print(float)",
                                     asFUNCTIONPR(ASPrint, (float), void),
                                     asCALL_CDECL) < 0)
      return false;

   return true;
}

// EOF

