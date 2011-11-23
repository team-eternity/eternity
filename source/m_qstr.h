// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// Reallocating string structure
//
// What this class guarantees:
// * The string will always be null-terminated
// * Indexing functions always check array bounds
// * Insertion functions always reallocate when needed
//
// Of course, using getBuffer can negate these, so avoid it
// except for passing a char * to read op functions.
//
// By James Haley
//
//-----------------------------------------------------------------------------

#ifndef M_QSTR_H__
#define M_QSTR_H__

#include "z_zone.h"

class qstring : public ZoneObject
{
private:
   char   *buffer;
   size_t  index;
   size_t  size;
   
   char   *checkBuffer();

public:
   static const size_t npos;
   static const size_t basesize;

   // Constructor / Destructor
   qstring(size_t startSize = 0, int tag = PU_STATIC) 
      : ZoneObject(), buffer(NULL), index(0), size(0)
   {
      ChangeTag(tag);
      if(startSize)
         initCreateSize(startSize);
   }
   ~qstring() { freeBuffer(); }

   // Basic Property Getters

   //
   // qstring::getBuffer
   //
   // Retrieves a pointer to the internal buffer. This pointer shouldn't be 
   // cached, and is not meant for writing into (although it is safe to do so, it
   // circumvents the encapsulation and security of this structure).
   //
   char *getBuffer() { return checkBuffer(); }

   //
   // qstring::constPtr
   //
   // Like qstring::getBuffer, but casts to const to enforce safety.
   //
   const char *constPtr() { return checkBuffer(); }

   //
   // qstring::length
   //
   // Because the validity of "index" is maintained by all insertion and editing
   // functions, we can bypass calling strlen.
   //
   size_t length() const { return index; }

   //
   // qstring::getSize
   //
   // Returns the amount of size allocated for this qstring (will be >= strlen).
   // You are allowed to index into the qstring up to size - 1, although any bytes
   // beyond the strlen will be zero.
   //
   size_t getSize() const { return size; }

   qstring    &initCreate();
   qstring    &initCreateSize(size_t size);
   qstring    &createSize(size_t size);
   qstring    &create();
   qstring    &grow(size_t len);
   qstring    &clear();
   qstring    &clearOrCreate(size_t size);
   void        freeBuffer();
   char        charAt(size_t idx);
   char       *bufferAt(size_t idx);
   qstring    &Putc(char ch);
   qstring    &Delc();
   qstring    &concat(const char *str);
   qstring    &concat(const qstring &src);
   qstring    &insert(const char *insertstr, size_t pos);
   int         strCmp(const char *str) const;
   int         strNCmp(const char *str, size_t maxcount) const;
   int         strCaseCmp(const char *str) const;
   int         strNCaseCmp(const char *str, size_t maxcount) const;
   bool        compare(const char *str) const;
   bool        compare(const qstring &other) const;
   qstring    &copy(const char *str);
   qstring    &copy(const qstring &src);
   char       *copyInto(char *dest, size_t size) const;
   qstring    &copyInto(qstring &dest) const;
   void        swapWith(qstring &str2);
   qstring    &toUpper();
   qstring    &toLower();
   size_t      replace(const char *filter, char repl);
   size_t      replaceNotOf(const char *filter, char repl);
   qstring    &normalizeSlashes();
   char       *duplicate(int tag) const;
   char       *duplicateAuto() const;
   int         toInt() const;
   long        toLong(char **endptr, int radix);
   double      toDouble(char **endptr);
   const char *strChr(char c) const;
   const char *strRChr(char c) const;
   size_t      findFirstOf(char c) const;
   size_t      findFirstNotOf(char c) const;
   const char *findSubStr(const char *substr) const;
   const char *findSubStrNoCase(const char *substr) const;
   qstring    &LStrip(char c);
   qstring    &RStrip(char c);
   qstring    &truncate(size_t pos);
   qstring    &makeQuoted();
   int         Printf(size_t maxlen, const char *fmt, ...);

   // Operators
   bool     operator == (const qstring &other) const;
   bool     operator == (const char    *other) const;
   bool     operator != (const qstring &other) const;
   bool     operator != (const char    *other) const;
   qstring &operator  = (const qstring &other);
   qstring &operator  = (const char    *other);
   qstring &operator += (const qstring &other);
   qstring &operator += (const char    *other);
   qstring &operator += (char  ch);
};

#endif

// EOF


